#include "throttlebox/metrics.hpp"
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace throttlebox {

Metrics::Metrics() {
    // Initialize common counters
    counters_["total_connections"] = 0;
    counters_["allowed_messages"] = 0;
    counters_["blocked_messages"] = 0;
    counters_["client_disconnects"] = 0;
    
    // Initialize common gauges
    gauges_["active_connections"] = 0;
    gauges_["unique_clients"] = 0;
}

Metrics::~Metrics() {
    stopHttpServer();
}

void Metrics::incrementCounter(const std::string& name) {
    std::lock_guard<std::mutex> lock(counterMutex_);
    
    // Create counter if it doesn't exist
    if (counters_.find(name) == counters_.end()) {
        counters_[name] = 0;
    }
    
    counters_[name]++;
}

void Metrics::setGauge(const std::string& name, int64_t value) {
    std::lock_guard<std::mutex> lock(gaugeMutex_);
    
    // Create gauge if it doesn't exist
    if (gauges_.find(name) == gauges_.end()) {
        gauges_[name] = 0;
    }
    
    gauges_[name] = value;
}

std::string Metrics::getFormattedMetrics() const {
    std::stringstream ss;
    
    // Add header
    ss << "# HELP throttlebox_metrics ThrottleBox proxy metrics\n";
    ss << "# TYPE throttlebox_counter counter\n";
    ss << "# TYPE throttlebox_gauge gauge\n\n";
    
    // Add counters
    {
        std::lock_guard<std::mutex> lock(counterMutex_);
        for (const auto& pair : counters_) {
            ss << "throttlebox_" << pair.first << "_total " << pair.second.load() << "\n";
        }
    }
    
    ss << "\n";
    
    // Add gauges
    {
        std::lock_guard<std::mutex> lock(gaugeMutex_);
        for (const auto& pair : gauges_) {
            ss << "throttlebox_" << pair.first << " " << pair.second.load() << "\n";
        }
    }
    
    return ss.str();
}

bool Metrics::startHttpServer(int port) {
    if (httpServerRunning_) {
        return false; // Already running
    }
    
    httpPort_ = port;
    httpServerRunning_ = true;
    
    httpServerThread_ = std::thread(&Metrics::httpServerLoop, this);
    
    std::cout << "Metrics HTTP server started on port " << port << std::endl;
    return true;
}

void Metrics::stopHttpServer() {
    if (!httpServerRunning_) {
        return;
    }
    
    httpServerRunning_ = false;
    
    if (httpServerThread_.joinable()) {
        httpServerThread_.join();
    }
    
    std::cout << "Metrics HTTP server stopped" << std::endl;
}

void Metrics::httpServerLoop() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create metrics server socket" << std::endl;
        return;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(httpPort_);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind metrics server socket to port " << httpPort_ << std::endl;
        close(serverSocket);
        return;
    }
    
    if (listen(serverSocket, 3) < 0) {
        std::cerr << "Failed to listen on metrics server socket" << std::endl;
        close(serverSocket);
        return;
    }
    
    while (httpServerRunning_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            if (httpServerRunning_) {
                std::cerr << "Select error in metrics server" << std::endl;
            }
            break;
        }
        
        if (activity > 0 && FD_ISSET(serverSocket, &readfds)) {
            struct sockaddr_in clientAddress;
            socklen_t clientLen = sizeof(clientAddress);
            
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientLen);
            if (clientSocket >= 0) {
                // Read HTTP request (simple implementation)
                char buffer[1024];
                ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    
                    // Check if it's a GET request to /metrics
                    if (strstr(buffer, "GET /metrics") != nullptr) {
                        std::string metrics = getFormattedMetrics();
                        
                        std::string response = "HTTP/1.1 200 OK\r\n";
                        response += "Content-Type: text/plain\r\n";
                        response += "Content-Length: " + std::to_string(metrics.length()) + "\r\n";
                        response += "Connection: close\r\n\r\n";
                        response += metrics;
                        
                        send(clientSocket, response.c_str(), response.length(), 0);
                    } else {
                        // Return 404 for other paths
                        std::string response = "HTTP/1.1 404 Not Found\r\n";
                        response += "Content-Type: text/plain\r\n";
                        response += "Content-Length: 9\r\n";
                        response += "Connection: close\r\n\r\n";
                        response += "Not Found";
                        
                        send(clientSocket, response.c_str(), response.length(), 0);
                    }
                }
                
                close(clientSocket);
            }
        }
    }
    
    close(serverSocket);
}

} // namespace throttlebox