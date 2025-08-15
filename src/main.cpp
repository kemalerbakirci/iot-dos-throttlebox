#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <getopt.h>
#include "throttlebox/throttlebox.hpp"
#include "throttlebox/config.hpp"

using namespace throttlebox;

std::atomic<bool> shutdown_requested{false};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    shutdown_requested = true;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "Options:\n"
              << "  -c, --config PATH    Configuration file path (YAML or JSON)\n"
              << "  -p, --port PORT      Listen port (default: 1883)\n"
              << "  -b, --broker HOST    Broker host (default: localhost)\n"
              << "  -P, --broker-port N  Broker port (default: 1884)\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --version        Show version information\n";
}

void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════╗
║               IoT DoS ThrottleBox                ║
║           MQTT Reverse Proxy & Rate Limiter     ║
╚══════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string configPath;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"port", required_argument, 0, 'p'},
        {"broker", required_argument, 0, 'b'},
        {"broker-port", required_argument, 0, 'P'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "c:p:b:P:hv", long_options, nullptr)) != -1) {
        switch (c) {
            case 'c':
                configPath = optarg;
                break;
            case 'p':
                std::cout << "Note: Port override not yet implemented. Use config file." << std::endl;
                break;
            case 'b':
                std::cout << "Note: Broker override not yet implemented. Use config file." << std::endl;
                break;
            case 'P':
                std::cout << "Note: Broker port override not yet implemented. Use config file." << std::endl;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                std::cout << "IoT DoS ThrottleBox v1.0.0" << std::endl;
                return 0;
            case '?':
                printUsage(argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    printBanner();
    
    // Load configuration
    Config config;
    if (!configPath.empty()) {
        std::cout << "Loading configuration from: " << configPath << std::endl;
        if (!config.loadFromFile(configPath)) {
            std::cerr << "Failed to load config: " << config.getLastError() << std::endl;
            return 1;
        }
    } else {
        std::cout << "Using default configuration" << std::endl;
    }
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Create and configure ThrottleBox
        ThrottleBox proxy(config);
        
        std::cout << "Starting ThrottleBox proxy..." << std::endl;
        std::cout << "Listen address: " << config.getProxySettings().listenAddress 
                  << ":" << config.getProxySettings().listenPort << std::endl;
        std::cout << "Broker address: " << config.getProxySettings().brokerHost 
                  << ":" << config.getProxySettings().brokerPort << std::endl;
        std::cout << "Rate limit: " << config.getGlobalLimits().maxMessagesPerSec 
                  << " msg/sec (burst: " << config.getGlobalLimits().burstSize << ")" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl << std::endl;
        
        // Start proxy in a separate thread
        std::thread proxyThread([&proxy]() {
            proxy.runProxy();
        });
        
        // Wait for shutdown signal
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop proxy
        std::cout << "Stopping proxy..." << std::endl;
        proxy.stop();
        
        if (proxyThread.joinable()) {
            proxyThread.join();
        }
        
        std::cout << "ThrottleBox stopped successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}