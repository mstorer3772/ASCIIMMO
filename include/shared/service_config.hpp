#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace asciimmo::config
{

// YAML config parser using yaml-cpp library
class ServiceConfig
    {
    public:
        struct BroadcastTarget
            {
            std::string host;
            int port;
            std::string name;
            };

        static ServiceConfig& instance()
            {
            static ServiceConfig config;
            return config;
            }

        // Load config from file
        bool load(const std::string& config_file = "config/services.yaml")
            {
            try
                {
                // Try to load the file
                try
                    {
                    root_ = YAML::LoadFile(config_file);
                    }
                    catch (const YAML::BadFile&)
                        {
                        // Try alternate path
                        root_ = YAML::LoadFile("../config/services.yaml");
                        }
                    loaded_ = true;
                    return true;
                }
                catch (const YAML::Exception& e)
                    {
                    loaded_ = false;
                    return false;
                    }
            }

        // Get string value with default
        std::string get_string(const std::string& key, const std::string& default_val = "") const
            {
            if (!loaded_) return default_val;

            try
                {
                YAML::Node node = navigate_to_key(key);
                if (node && node.IsScalar())
                    {
                    return node.as<std::string>();
                    }
                }
                catch (...)
                    {
                    // Key not found or conversion failed
                    }
                return default_val;
            }

        // Get int value with default
        int get_int(const std::string& key, int default_val = 0) const
            {
            if (!loaded_) return default_val;

            try
                {
                YAML::Node node = navigate_to_key(key);
                if (node && node.IsScalar())
                    {
                    return node.as<int>();
                    }
                }
                catch (...)
                    {
                    // Key not found or conversion failed
                    }
                return default_val;
            }

        // Get unsigned long long value with default
        unsigned long long get_ulonglong(const std::string& key, unsigned long long default_val = 0) const
            {
            if (!loaded_) return default_val;

            try
                {
                YAML::Node node = navigate_to_key(key);
                if (node && node.IsScalar())
                    {
                    return node.as<unsigned long long>();
                    }
                }
                catch (...)
                    {
                    // Key not found or conversion failed
                    }
                return default_val;
            }

        // Get bool value with default
        bool get_bool(const std::string& key, bool default_val = false) const
            {
            if (!loaded_) return default_val;

            try
                {
                YAML::Node node = navigate_to_key(key);
                if (node && node.IsScalar())
                    {
                    return node.as<bool>();
                    }
                }
                catch (...)
                    {
                    // Key not found or conversion failed
                    }
                return default_val;
            }

        // Get broadcast targets for session service
        std::vector<BroadcastTarget> get_broadcast_targets() const
            {
            std::vector<BroadcastTarget> targets;

            if (!loaded_) return targets;

            try
                {
                YAML::Node broadcast_node = navigate_to_key("session_service.broadcast_targets");
                if (broadcast_node && broadcast_node.IsSequence())
                    {
                    for (const auto& target : broadcast_node)
                        {
                        BroadcastTarget bt;
                        bt.host = target["host"].as<std::string>("localhost");
                        bt.port = target["port"].as<int>(8080);
                        bt.name = target["name"].as<std::string>("unknown");
                        targets.push_back(bt);
                        }
                    }
                }
                catch (...)
                    {
                    // If parsing fails, return empty vector
                    }

                return targets;
            }

    private:
        ServiceConfig() = default;

        // Navigate to a key using dot notation (e.g., "section.subsection.key")
        YAML::Node navigate_to_key(const std::string& key) const
            {
            YAML::Node node = root_;

            size_t start = 0;
            size_t end = key.find('.');

            while (end != std::string::npos)
                {
                std::string part = key.substr(start, end - start);
                if (!node[part]) return YAML::Node();
                node = node[part];
                start = end + 1;
                end = key.find('.', start);
                }

            std::string last_part = key.substr(start);
            return node[last_part];
            }

        YAML::Node root_;
        bool loaded_ = false;
    };

} // namespace asciimmo::config
