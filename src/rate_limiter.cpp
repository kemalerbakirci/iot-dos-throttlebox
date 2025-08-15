#include "throttlebox/rate_limiter.hpp"
#include <algorithm>
#include <iostream>

namespace throttlebox {

RateLimiter::RateLimiter(const RateLimitPolicy& defaultPolicy)
    : defaultPolicy_(defaultPolicy) {
}

bool RateLimiter::allow(const std::string& ip, const std::string& clientId) {
    // Use clientId as primary key, fallback to IP if clientId is empty
    std::string key = clientId.empty() ? ip : clientId;
    
    // Get policy for this client
    RateLimitPolicy policy = defaultPolicy_;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clientPolicies_.find(clientId);
        if (it != clientPolicies_.end()) {
            policy = it->second;
        }
    }
    
    bool allowed = checkAndUpdateBucket(key, policy);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        if (allowed) {
            allowedMessages_++;
        } else {
            blockedMessages_++;
        }
    }
    
    return allowed;
}

bool RateLimiter::checkAndUpdateBucket(const std::string& key, const RateLimitPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& bucket = buckets_[key];
    
    // Refill tokens based on time elapsed first
    refillBucket(bucket, policy);
    
    // Check if client is still blocked
    if (bucket.isBlocked && now < bucket.blockedUntil) {
        return false;
    }
    
    // Clear block status if block period has expired
    if (bucket.isBlocked && now >= bucket.blockedUntil) {
        bucket.isBlocked = false;
    }
    
    // Check if we have tokens available
    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return true;
    } else {
        // No tokens available
        if (policy.blockDurationSec > 0) {
            // Block the client if block duration is configured
            bucket.isBlocked = true;
            bucket.blockedUntil = now + std::chrono::seconds(policy.blockDurationSec);
        }
        return false;
    }
}

void RateLimiter::refillBucket(TokenBucket& bucket, const RateLimitPolicy& policy) {
    auto now = std::chrono::steady_clock::now();
    
    if (bucket.lastRefill == std::chrono::steady_clock::time_point{}) {
        // First time - initialize
        bucket.lastRefill = now;
        bucket.tokens = policy.burstSize;
        return;
    }
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - bucket.lastRefill);
    double secondsElapsed = elapsed.count() / 1000.0;
    
    // Add tokens based on rate
    double tokensToAdd = secondsElapsed * policy.maxMessagesPerSec;
    bucket.tokens = std::min(static_cast<double>(policy.burstSize), bucket.tokens + tokensToAdd);
    bucket.lastRefill = now;
}

void RateLimiter::setClientPolicy(const std::string& clientId, const RateLimitPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    clientPolicies_[clientId] = policy;
}

void RateLimiter::cleanupExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = buckets_.begin();
    
    while (it != buckets_.end()) {
        // Remove buckets that haven't been used for 1 hour
        auto timeSinceLastRefill = now - it->second.lastRefill;
        if (timeSinceLastRefill > std::chrono::hours(1)) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}

RateLimiter::Stats RateLimiter::getStats() const {
    Stats stats;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats.totalClients = buckets_.size();
        
        // Count blocked clients
        auto now = std::chrono::steady_clock::now();
        for (const auto& pair : buckets_) {
            if (pair.second.isBlocked && now < pair.second.blockedUntil) {
                stats.blockedClients++;
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats.allowedMessages = allowedMessages_;
        stats.blockedMessages = blockedMessages_;
    }
    
    return stats;
}

} // namespace throttlebox