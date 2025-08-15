#include "throttlebox/rate_limiter.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

using namespace throttlebox;

void testBasicRateLimiting() {
    std::cout << "Testing basic rate limiting..." << std::endl;
    
    RateLimitPolicy policy;
    policy.maxMessagesPerSec = 2.0; // 2 messages per second
    policy.burstSize = 3;           // Allow burst of 3
    policy.blockDurationSec = 1;    // Block for 1 second
    
    RateLimiter limiter(policy);
    
    std::string testIP = "192.168.1.100";
    std::string testClientId = "test_client";
    
    // First 3 messages should be allowed (burst)
    for (int i = 0; i < 3; i++) {
        bool allowed = limiter.allow(testIP, testClientId);
        assert(allowed && "First 3 messages should be allowed");
        std::cout << "Message " << (i+1) << ": " << (allowed ? "ALLOWED" : "BLOCKED") << std::endl;
    }
    
    // 4th message should be blocked (no tokens left)
    bool blocked = limiter.allow(testIP, testClientId);
    assert(!blocked && "4th message should be blocked");
    std::cout << "Message 4: " << (blocked ? "ALLOWED" : "BLOCKED") << std::endl;
    
    std::cout << "Basic rate limiting test PASSED" << std::endl;
}

void testTokenRefill() {
    std::cout << "Testing token refill..." << std::endl;
    
    RateLimitPolicy policy;
    policy.maxMessagesPerSec = 10.0; // 10 messages per second (fast refill for testing)
    policy.burstSize = 2;
    policy.blockDurationSec = 0;     // No blocking, just rate limiting
    
    RateLimiter limiter(policy);
    
    std::string testIP = "192.168.1.101";
    std::string testClientId = "test_client_2";
    
    // Use up all tokens
    limiter.allow(testIP, testClientId);
    limiter.allow(testIP, testClientId);
    
    // Should be blocked now
    bool blocked = limiter.allow(testIP, testClientId);
    assert(!blocked && "Should be blocked after using all tokens");
    
    // Wait for token refill (200ms should add 2 tokens at 10/sec rate)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should be allowed now (tokens refilled)
    bool allowed = limiter.allow(testIP, testClientId);
    assert(allowed && "Should be allowed after token refill");
    
    std::cout << "Token refill test PASSED" << std::endl;
}

void testMultipleClients() {
    std::cout << "Testing multiple clients independence..." << std::endl;
    
    RateLimitPolicy policy;
    policy.maxMessagesPerSec = 1.0;
    policy.burstSize = 1;
    policy.blockDurationSec = 1;
    
    RateLimiter limiter(policy);
    
    std::string client1IP = "192.168.1.102";
    std::string client1Id = "client1";
    std::string client2IP = "192.168.1.103";
    std::string client2Id = "client2";
    
    // Both clients should be allowed their first message
    bool client1_allowed = limiter.allow(client1IP, client1Id);
    bool client2_allowed = limiter.allow(client2IP, client2Id);
    
    assert(client1_allowed && "Client 1 first message should be allowed");
    assert(client2_allowed && "Client 2 first message should be allowed");
    
    // Both should be blocked on second message
    bool client1_blocked = limiter.allow(client1IP, client1Id);
    bool client2_blocked = limiter.allow(client2IP, client2Id);
    
    assert(!client1_blocked && "Client 1 second message should be blocked");
    assert(!client2_blocked && "Client 2 second message should be blocked");
    
    std::cout << "Multiple clients test PASSED" << std::endl;
}

void testStatistics() {
    std::cout << "Testing statistics..." << std::endl;
    
    RateLimitPolicy policy;
    policy.maxMessagesPerSec = 1.0;
    policy.burstSize = 1;
    policy.blockDurationSec = 1;
    
    RateLimiter limiter(policy);
    
    std::string testIP = "192.168.1.104";
    std::string testClientId = "stats_test";
    
    // Generate some traffic
    limiter.allow(testIP, testClientId); // Should be allowed
    limiter.allow(testIP, testClientId); // Should be blocked
    limiter.allow(testIP, testClientId); // Should be blocked
    
    auto stats = limiter.getStats();
    
    assert(stats.allowedMessages == 1 && "Should have 1 allowed message");
    assert(stats.blockedMessages == 2 && "Should have 2 blocked messages");
    assert(stats.totalClients == 1 && "Should have 1 total client");
    
    std::cout << "Statistics test PASSED" << std::endl;
    std::cout << "  Allowed: " << stats.allowedMessages << std::endl;
    std::cout << "  Blocked: " << stats.blockedMessages << std::endl;
    std::cout << "  Total clients: " << stats.totalClients << std::endl;
}

int main() {
    std::cout << "Running RateLimiter tests..." << std::endl << std::endl;
    
    try {
        testBasicRateLimiting();
        std::cout << std::endl;
        
        testTokenRefill();
        std::cout << std::endl;
        
        testMultipleClients();
        std::cout << std::endl;
        
        testStatistics();
        std::cout << std::endl;
        
        std::cout << "All RateLimiter tests PASSED!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}