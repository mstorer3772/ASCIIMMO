#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>

namespace asciimmo {
namespace config {

// Simple YAML-like config parser (basic key-value and sections)
class ServiceConfig {
public:
    struct BroadcastTarget {
        std::string host;
        int port;
        std::string name;
    };
    
    static ServiceConfig& instance() {
        static ServiceConfig config;
        return config;
    }
    
    // Load config from file
    bool load(const std::string& config_file = "config/services.yaml") {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            // Try alternate path
            file.open("../config/services.yaml");
            if (!file.is_open()) {
                return false;
            }
        }
        
        std::string line;
        std::string current_section;
        
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);
            
            // Check for section header (no leading spaces, ends with :)
            if (start == 0 && line.back() == ':' && line.find(':') == line.length() - 1) {
                current_section = line.substr(0, line.length() - 1);
                continue;
            }

            // Parse key: value
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);

                // Trim key and value
                key.erase(key.find_last_not_of(" \t") + 1);
                size_t val_start = value.find_first_not_of(" \t\"");
                if (val_start != std::string::npos) {
                    value = value.substr(val_start);
                    size_t val_end = value.find_last_not_of(" \t\"");
                    if (val_end != std::string::npos) {
                        value = value.substr(0, val_end + 1);
                    }
                }

                if (!current_section.empty()) {
                    data_[current_section + "." + key] = value;
                } else {
                    data_[key] = value;
                }
            }
        }

        return true;
    }

    // Get string value with default
    std::string get_string(const std::string& key, const std::string& default_val = "") const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : default_val;
    }
    
    // Get int value with default
    int get_int(const std::string& key, int default_val = 0) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return default_val;
            }
        }
        return default_val;
    }
    
    // Get unsigned long long value with default
    unsigned long long get_ulonglong(const std::string& key, unsigned long long default_val = 0) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stoull(it->second);
            } catch (...) {
                return default_val;
            }
        }
        return default_val;
    }
    
    // Get bool value with default
    bool get_bool(const std::string& key, bool default_val = false) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            std::string val = it->second;
            // Convert to lowercase for case-insensitive comparison
            for (char& c : val) c = std::tolower(c);
            return (val == "true" || val == "1" || val == "yes");
        }
        return default_val;
    }
    
    // Get broadcast targets for session service
    std::vector<BroadcastTarget> get_broadcast_targets() const {
        std::vector<BroadcastTarget> targets;
        
        // Simple parsing - in production use a real YAML library
        // For now, hardcode the known structure
        std::string host1 = get_string("session_service.broadcast_targets.- host", "localhost");
        int port1 = get_int("session_service.broadcast_targets.- port", 8080);
        
        if (!host1.empty() && port1 > 0) {
            targets.push_back({host1, port1, "world-service"});
        }
        
        // Add social service (hardcoded for now - proper YAML parser needed for arrays)
        targets.push_back({"localhost", 8083, "social-service"});
        
        return targets;
    }
    
private:
    ServiceConfig() = default;
    std::map<std::string, std::string> data_;
};

} // namespace config
} // namespace asciimmo
