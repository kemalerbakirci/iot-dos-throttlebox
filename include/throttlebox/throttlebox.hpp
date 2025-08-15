#pragma once

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include "rate_limiter.hpp"
#include "config.hpp"
#include "metrics.hpp"

namespace throttlebox {

class ThrottleBox {
public:
    ThrottleBox(const Config& config);
    ~ThrottleBox();

    // Start the proxy server
    void runProxy();
    
    // Stop the proxy server
    void stop();

private:
    // Handle individual client connection
    void handleClient(int clientSocket);
    
    // Extract client info from MQTT CONNECT packet
    struct ClientInfo {
        std::string ip;
        std::string clientId;
    };
    
    bool extractClientInfo(int socket, ClientInfo& info);
    
    // Forward traffic between client and broker
    void forwardTraffic(int clientSocket, int brokerSocket, const ClientInfo& info);
    
    // Connect to the real MQTT broker
    int connectToBroker();

private:
    std::unique_ptr<RateLimiter> rateLimiter_;
    std::unique_ptr<Metrics> metrics_;
    Config config_;
    
    int serverSocket_;
    std::atomic<bool> running_;
    std::vector<std::thread> clientThreads_;
};

} // namespace throttlebox