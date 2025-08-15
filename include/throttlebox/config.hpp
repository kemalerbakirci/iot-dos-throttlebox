#pragma once

#include <string>
#include <unordered_map>
#include "rate_limiter.hpp"

namespace throttlebox {

class Config {
public:
    struct ProxySettings {
        std::string listenAddress = "0.0.0.0";
        int listenPort = 1883;
        std::string brokerHost = "localhost";
        int brokerPort = 1884;
    };

    Config() = default;
    ~Config() = default;

    // Load configuration from file (YAML or JSON)
    bool loadFromFile(const std::string& path);
    
    // Get global rate limiting policy
    const RateLimitPolicy& getGlobalLimits() const { return globalPolicy_; }
    
    // Get client-specific policy (falls back to global if not found)
    RateLimitPolicy getClientPolicy(const std::string& clientId) const;
    
    // Get proxy settings
    const ProxySettings& getProxySettings() const { return proxySettings_; }
    
    // Check if configuration is valid
    bool isValid() const { return valid_; }
    
    // Get last error message
    const std::string& getLastError() const { return lastError_; }

private:
    bool loadFromYaml(const std::string& path);
    bool loadFromJson(const std::string& path);
    bool validateConfig();
    
    RateLimitPolicy globalPolicy_;
    std::unordered_map<std::string, RateLimitPolicy> clientPolicies_;
    ProxySettings proxySettings_;
    
    bool valid_ = false;
    std::string lastError_;
};

} // namespace throttlebox