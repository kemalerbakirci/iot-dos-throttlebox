#include "throttlebox/throttlebox.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace throttlebox {

ThrottleBox::ThrottleBox(const Config& config)
    : config_(config), serverSocket_(-1), running_(false) {
    
    rateLimiter_ = std::make_unique<RateLimiter>(config_.getGlobalLimits());
    metrics_ = std::make_unique<Metrics>();
    
    // Start metrics server if configured
    metrics_->startHttpServer(9090);
}

ThrottleBox::~ThrottleBox() {
    stop();
}

void ThrottleBox::runProxy() {
    running_ = true;
    
    // Create server socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        throw std::runtime_error("Failed to create server socket");
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.getProxySettings().listenPort);
    
    if (config_.getProxySettings().listenAddress == "0.0.0.0") {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, config_.getProxySettings().listenAddress.c_str(), &address.sin_addr);
    }
    
    if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(serverSocket_);
        throw std::runtime_error("Failed to bind to port " + 
                                std::to_string(config_.getProxySettings().listenPort));
    }
    
    // Listen for connections
    if (listen(serverSocket_, 10) < 0) {
        close(serverSocket_);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    std::cout << "ThrottleBox listening on " 
              << config_.getProxySettings().listenAddress << ":" 
              << config_.getProxySettings().listenPort << std::endl;
    
    // Accept client connections
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(serverSocket_ + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            if (running_) {
                std::cerr << "Select error in main loop" << std::endl;
            }
            break;
        }
        
        if (activity > 0 && FD_ISSET(serverSocket_, &readfds)) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket >= 0) {
                metrics_->incrementCounter("total_connections");
                
                // Handle client in separate thread
                std::thread clientThread(&ThrottleBox::handleClient, this, clientSocket);
                clientThread.detach(); // Let it run independently
            }
        }
        
        // Periodic cleanup
        static auto lastCleanup = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastCleanup > std::chrono::minutes(5)) {
            rateLimiter_->cleanupExpired();
            lastCleanup = now;
        }
    }
    
    // Clean up
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
}

void ThrottleBox::stop() {
    running_ = false;
    
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
}

void ThrottleBox::handleClient(int clientSocket) {
    ClientInfo clientInfo;
    
    try {
        // Extract client information from connection
        if (!extractClientInfo(clientSocket, clientInfo)) {
            std::cerr << "Failed to extract client info" << std::endl;
            close(clientSocket);
            return;
        }
        
        std::cout << "New client: " << clientInfo.ip << " (ID: " << clientInfo.clientId << ")" << std::endl;
        
        // Connect to broker
        int brokerSocket = connectToBroker();
        if (brokerSocket < 0) {
            std::cerr << "Failed to connect to broker" << std::endl;
            close(clientSocket);
            return;
        }
        
        // Forward traffic between client and broker
        forwardTraffic(clientSocket, brokerSocket, clientInfo);
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling client " << clientInfo.ip << ": " << e.what() << std::endl;
    }
    
    close(clientSocket);
    metrics_->incrementCounter("client_disconnects");
}

bool ThrottleBox::extractClientInfo(int socket, ClientInfo& info) {
    // Get client IP address
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(socket, (struct sockaddr*)&addr, &len) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        info.ip = ip;
    } else {
        info.ip = "unknown";
    }
    
    // Read MQTT CONNECT packet to extract ClientID
    // This is a simplified implementation - a real one would need proper MQTT parsing
    char buffer[1024];
    ssize_t bytesRead = recv(socket, buffer, sizeof(buffer), MSG_PEEK);
    
    if (bytesRead < 10) {
        return false; // Not enough data for MQTT CONNECT
    }
    
    // Very basic MQTT CONNECT parsing
    // Real implementation should use a proper MQTT library
    if (buffer[0] == 0x10) { // CONNECT packet type
        // Skip fixed header and protocol name
        int pos = 10; // Simplified - real parsing would be more complex
        
        if (pos + 2 < bytesRead) {
            uint16_t clientIdLen = (buffer[pos] << 8) | buffer[pos + 1];
            pos += 2;
            
            if (pos + clientIdLen < bytesRead) {
                info.clientId = std::string(buffer + pos, clientIdLen);
            }
        }
    }
    
    if (info.clientId.empty()) {
        info.clientId = "anonymous_" + info.ip;
    }
    
    return true;
}

void ThrottleBox::forwardTraffic(int clientSocket, int brokerSocket, const ClientInfo& info) {
    fd_set readfds;
    char buffer[4096];
    
    while (running_) {
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);
        FD_SET(brokerSocket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int maxfd = std::max(clientSocket, brokerSocket);
        int activity = select(maxfd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity <= 0) {
            continue;
        }
        
        // Data from client to broker
        if (FD_ISSET(clientSocket, &readfds)) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                break; // Client disconnected
            }
            
            // Check rate limit
            if (!rateLimiter_->allow(info.ip, info.clientId)) {
                metrics_->incrementCounter("blocked_messages");
                std::cout << "Rate limit exceeded for " << info.clientId 
                          << " (" << info.ip << "), dropping message" << std::endl;
                continue; // Drop the message
            }
            
            metrics_->incrementCounter("allowed_messages");
            
            // Forward to broker
            ssize_t bytesSent = send(brokerSocket, buffer, bytesRead, 0);
            if (bytesSent != bytesRead) {
                break; // Broker connection failed
            }
        }
        
        // Data from broker to client
        if (FD_ISSET(brokerSocket, &readfds)) {
            ssize_t bytesRead = recv(brokerSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                break; // Broker disconnected
            }
            
            // Forward to client
            ssize_t bytesSent = send(clientSocket, buffer, bytesRead, 0);
            if (bytesSent != bytesRead) {
                break; // Client connection failed
            }
        }
    }
    
    close(brokerSocket);
}

int ThrottleBox::connectToBroker() {
    int brokerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (brokerSocket < 0) {
        return -1;
    }
    
    struct sockaddr_in brokerAddr;
    brokerAddr.sin_family = AF_INET;
    brokerAddr.sin_port = htons(config_.getProxySettings().brokerPort);
    
    if (inet_pton(AF_INET, config_.getProxySettings().brokerHost.c_str(), &brokerAddr.sin_addr) <= 0) {
        close(brokerSocket);
        return -1;
    }
    
    if (connect(brokerSocket, (struct sockaddr*)&brokerAddr, sizeof(brokerAddr)) < 0) {
        close(brokerSocket);
        return -1;
    }
    
    return brokerSocket;
}

} // namespace throttlebox