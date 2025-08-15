#pragma once

#include <string>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <thread>

namespace throttlebox {

class Metrics {
public:
    Metrics();
    ~Metrics();

    // Increment named counter
    void incrementCounter(const std::string& name);
    
    // Set gauge value
    void setGauge(const std::string& name, int64_t value);
    
    // Get formatted metrics in Prometheus text format
    std::string getFormattedMetrics() const;
    
    // Start HTTP server for /metrics endpoint (optional)
    bool startHttpServer(int port = 9090);
    
    // Stop HTTP server
    void stopHttpServer();

private:
    void httpServerLoop();
    
    mutable std::mutex counterMutex_;
    std::unordered_map<std::string, std::atomic<uint64_t>> counters_;
    
    mutable std::mutex gaugeMutex_;
    std::unordered_map<std::string, std::atomic<int64_t>> gauges_;
    
    // HTTP server for metrics
    std::atomic<bool> httpServerRunning_{false};
    std::thread httpServerThread_;
    int httpPort_ = 9090;
};

} // namespace throttlebox