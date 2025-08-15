#include "throttlebox/config.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

using namespace throttlebox;

void createTestYamlConfig(const std::string& filename) {
    std::ofstream file(filename);
    file << "# ThrottleBox Configuration\n";
    file << "listen_address: 0.0.0.0\n";
    file << "listen_port: 1883\n";
    file << "broker_host: localhost\n";
    file << "broker_port: 1884\n";
    file << "max_messages_per_sec: 5.0\n";
    file << "burst_size: 10\n";
    file << "block_duration_sec: 30\n";
    file.close();
}

void createTestJsonConfig(const std::string& filename) {
    std::ofstream file(filename);
    file << "{\n";
    file << "  \"listen_address\": \"127.0.0.1\",\n";
    file << "  \"listen_port\": 8883,\n";
    file << "  \"broker_host\": \"mqtt.broker.com\",\n";
    file << "  \"broker_port\": 8884,\n";
    file << "  \"max_messages_per_sec\": 15.5,\n";
    file << "  \"burst_size\": 25,\n";
    file << "  \"block_duration_sec\": 60\n";
    file << "}\n";
    file.close();
}

void testDefaultConfig() {
    std::cout << "Testing default configuration..." << std::endl;
    
    Config config;
    
    // Test default values
    auto proxySettings = config.getProxySettings();
    auto globalLimits = config.getGlobalLimits();
    
    assert(proxySettings.listenAddress == "0.0.0.0");
    assert(proxySettings.listenPort == 1883);
    assert(proxySettings.brokerHost == "localhost");
    assert(proxySettings.brokerPort == 1884);
    
    assert(globalLimits.maxMessagesPerSec == 10.0); // Default from rate_limiter.hpp
    assert(globalLimits.burstSize == 20);
    assert(globalLimits.blockDurationSec == 60);
    
    std::cout << "Default configuration test PASSED" << std::endl;
}

void testYamlConfig() {
    std::cout << "Testing YAML configuration loading..." << std::endl;
    
    std::string filename = "test_config.yaml";
    createTestYamlConfig(filename);
    
    Config config;
    bool loaded = config.loadFromFile(filename);
    
    assert(loaded && "Should successfully load YAML config");
    assert(config.isValid() && "Config should be valid");
    
    auto proxySettings = config.getProxySettings();
    auto globalLimits = config.getGlobalLimits();
    
    assert(proxySettings.listenAddress == "0.0.0.0");
    assert(proxySettings.listenPort == 1883);
    assert(proxySettings.brokerHost == "localhost");
    assert(proxySettings.brokerPort == 1884);
    
    assert(globalLimits.maxMessagesPerSec == 5.0);
    assert(globalLimits.burstSize == 10);
    assert(globalLimits.blockDurationSec == 30);
    
    // Clean up
    std::remove(filename.c_str());
    
    std::cout << "YAML configuration test PASSED" << std::endl;
}

void testJsonConfig() {
    std::cout << "Testing JSON configuration loading..." << std::endl;
    
    std::string filename = "test_config.json";
    createTestJsonConfig(filename);
    
    Config config;
    bool loaded = config.loadFromFile(filename);
    
    assert(loaded && "Should successfully load JSON config");
    assert(config.isValid() && "Config should be valid");
    
    auto proxySettings = config.getProxySettings();
    auto globalLimits = config.getGlobalLimits();
    
    assert(proxySettings.listenAddress == "127.0.0.1");
    assert(proxySettings.listenPort == 8883);
    assert(proxySettings.brokerHost == "mqtt.broker.com");
    assert(proxySettings.brokerPort == 8884);
    
    assert(globalLimits.maxMessagesPerSec == 15.5);
    assert(globalLimits.burstSize == 25);
    assert(globalLimits.blockDurationSec == 60);
    
    // Clean up
    std::remove(filename.c_str());
    
    std::cout << "JSON configuration test PASSED" << std::endl;
}

void testInvalidConfig() {
    std::cout << "Testing invalid configuration handling..." << std::endl;
    
    // Test non-existent file
    Config config1;
    bool loaded1 = config1.loadFromFile("non_existent_file.yaml");
    assert(!loaded1 && "Should fail to load non-existent file");
    assert(!config1.isValid() && "Config should be invalid");
    
    // Test invalid values
    std::string filename = "invalid_config.yaml";
    std::ofstream file(filename);
    file << "max_messages_per_sec: -5.0\n";  // Invalid negative value
    file << "burst_size: 0\n";              // Invalid zero value
    file << "listen_port: 70000\n";         // Invalid port
    file.close();
    
    Config config2;
    bool loaded2 = config2.loadFromFile(filename);
    
    // Should load but fail validation
    if (loaded2) {
        assert(!config2.isValid() && "Config with invalid values should be invalid");
    }
    
    // Clean up
    std::remove(filename.c_str());
    
    std::cout << "Invalid configuration test PASSED" << std::endl;
}

void testClientPolicyFallback() {
    std::cout << "Testing client policy fallback..." << std::endl;
    
    Config config;
    
    // Test that unknown client gets global policy
    auto clientPolicy = config.getClientPolicy("unknown_client");
    auto globalPolicy = config.getGlobalLimits();
    
    assert(clientPolicy.maxMessagesPerSec == globalPolicy.maxMessagesPerSec);
    assert(clientPolicy.burstSize == globalPolicy.burstSize);
    assert(clientPolicy.blockDurationSec == globalPolicy.blockDurationSec);
    
    std::cout << "Client policy fallback test PASSED" << std::endl;
}

int main() {
    std::cout << "Running Config tests..." << std::endl << std::endl;
    
    try {
        testDefaultConfig();
        std::cout << std::endl;
        
        testYamlConfig();
        std::cout << std::endl;
        
        testJsonConfig();
        std::cout << std::endl;
        
        testInvalidConfig();
        std::cout << std::endl;
        
        testClientPolicyFallback();
        std::cout << std::endl;
        
        std::cout << "All Config tests PASSED!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}