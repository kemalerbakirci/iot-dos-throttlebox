#include "throttlebox/config.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

// We'll use a simple JSON parser for now - in a real implementation, 
// you'd want to use yaml-cpp or nlohmann::json
#include <sstream>

namespace throttlebox {

bool Config::loadFromFile(const std::string& path) {
    lastError_.clear();
    valid_ = false;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot open config file: " + path;
        return false;
    }
    
    // Determine file type by extension
    std::string extension = path.substr(path.find_last_of('.') + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    bool success = false;
    if (extension == "yaml" || extension == "yml") {
        success = loadFromYaml(path);
    } else if (extension == "json") {
        success = loadFromJson(path);
    } else {
        lastError_ = "Unsupported config file format. Use .yaml, .yml, or .json";
        return false;
    }
    
    if (success) {
        success = validateConfig();
    }
    
    valid_ = success;
    return success;
}

bool Config::loadFromYaml(const std::string& path) {
    // Simple YAML parser - in production use yaml-cpp
    std::ifstream file(path);
    std::string line;
    
    while (std::getline(file, line)) {
        // Remove whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // Remove whitespace from key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse configuration values
        if (key == "listen_address") {
            proxySettings_.listenAddress = value;
        } else if (key == "listen_port") {
            proxySettings_.listenPort = std::stoi(value);
        } else if (key == "broker_host") {
            proxySettings_.brokerHost = value;
        } else if (key == "broker_port") {
            proxySettings_.brokerPort = std::stoi(value);
        } else if (key == "max_messages_per_sec") {
            globalPolicy_.maxMessagesPerSec = std::stod(value);
        } else if (key == "burst_size") {
            globalPolicy_.burstSize = std::stoi(value);
        } else if (key == "block_duration_sec") {
            globalPolicy_.blockDurationSec = std::stoi(value);
        }
    }
    
    return true;
}

bool Config::loadFromJson(const std::string& path) {
    // Simple JSON parser - in production use nlohmann::json
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Very basic JSON parsing - this is just for demonstration
    // In production, use a proper JSON library
    
    auto findValue = [&content](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        
        pos = content.find_first_not_of(" \t\n", pos + 1);
        if (pos == std::string::npos) return "";
        
        size_t end;
        if (content[pos] == '"') {
            pos++;
            end = content.find('"', pos);
        } else {
            end = content.find_first_of(",}\n", pos);
        }
        
        if (end == std::string::npos) return "";
        return content.substr(pos, end - pos);
    };
    
    std::string value;
    
    value = findValue("listen_address");
    if (!value.empty()) proxySettings_.listenAddress = value;
    
    value = findValue("listen_port");
    if (!value.empty()) proxySettings_.listenPort = std::stoi(value);
    
    value = findValue("broker_host");
    if (!value.empty()) proxySettings_.brokerHost = value;
    
    value = findValue("broker_port");
    if (!value.empty()) proxySettings_.brokerPort = std::stoi(value);
    
    value = findValue("max_messages_per_sec");
    if (!value.empty()) globalPolicy_.maxMessagesPerSec = std::stod(value);
    
    value = findValue("burst_size");
    if (!value.empty()) globalPolicy_.burstSize = std::stoi(value);
    
    value = findValue("block_duration_sec");
    if (!value.empty()) globalPolicy_.blockDurationSec = std::stoi(value);
    
    return true;
}

bool Config::validateConfig() {
    if (globalPolicy_.maxMessagesPerSec <= 0) {
        lastError_ = "max_messages_per_sec must be positive";
        return false;
    }
    
    if (globalPolicy_.burstSize <= 0) {
        lastError_ = "burst_size must be positive";
        return false;
    }
    
    if (globalPolicy_.blockDurationSec < 0) {
        lastError_ = "block_duration_sec cannot be negative";
        return false;
    }
    
    if (proxySettings_.listenPort <= 0 || proxySettings_.listenPort > 65535) {
        lastError_ = "listen_port must be between 1 and 65535";
        return false;
    }
    
    if (proxySettings_.brokerPort <= 0 || proxySettings_.brokerPort > 65535) {
        lastError_ = "broker_port must be between 1 and 65535";
        return false;
    }
    
    if (proxySettings_.brokerHost.empty()) {
        lastError_ = "broker_host cannot be empty";
        return false;
    }
    
    return true;
}

RateLimitPolicy Config::getClientPolicy(const std::string& clientId) const {
    auto it = clientPolicies_.find(clientId);
    if (it != clientPolicies_.end()) {
        return it->second;
    }
    return globalPolicy_;
}

} // namespace throttlebox