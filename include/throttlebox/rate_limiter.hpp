#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <deque>

namespace throttlebox {

struct RateLimitPolicy {
    double maxMessagesPerSec = 10.0;
    int burstSize = 20;
    int blockDurationSec = 60;
};

struct TokenBucket {
    double tokens = 0.0;
    std::chrono::steady_clock::time_point lastRefill;
    std::chrono::steady_clock::time_point blockedUntil;
    bool isBlocked = false;
};

class RateLimiter {
public:
    RateLimiter(const RateLimitPolicy& defaultPolicy);
    ~RateLimiter() = default;

    // Check if a message from this client/IP is allowed
    bool allow(const std::string& ip, const std::string& clientId);
    
    // Set custom policy for a specific client
    void setClientPolicy(const std::string& clientId, const RateLimitPolicy& policy);
    
    // Clean up expired entries to prevent memory leaks
    void cleanupExpired();
    
    // Get statistics for metrics
    struct Stats {
        size_t totalClients = 0;
        size_t blockedClients = 0;
        uint64_t allowedMessages = 0;
        uint64_t blockedMessages = 0;
    };
    
    Stats getStats() const;

private:
    bool checkAndUpdateBucket(const std::string& key, const RateLimitPolicy& policy);
    void refillBucket(TokenBucket& bucket, const RateLimitPolicy& policy);
    
    RateLimitPolicy defaultPolicy_;
    std::unordered_map<std::string, RateLimitPolicy> clientPolicies_;
    std::unordered_map<std::string, TokenBucket> buckets_;
    
    mutable std::mutex mutex_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    uint64_t allowedMessages_ = 0;
    uint64_t blockedMessages_ = 0;
};

} // namespace throttlebox