#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace asciimmo {
namespace auth {

struct TokenInfo {
    std::string user_data;
    std::chrono::steady_clock::time_point expires_at;
    
    bool is_valid() const {
        return std::chrono::steady_clock::now() < expires_at;
    }
};

class TokenCache {
public:
    // Add or update a token with 15 minute expiration
    void add_token(const std::string& token, const std::string& user_data) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto expires = std::chrono::steady_clock::now() + std::chrono::minutes(15);
        cache_[token] = TokenInfo{user_data, expires};
    }
    
    // Check if token is valid and return user data
    bool validate_token(const std::string& token, std::string& user_data) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = cache_.find(token);
        if (it != cache_.end() && it->second.is_valid()) {
            user_data = it->second.user_data;
            return true;
        }
        return false;
    }
    
    // Remove expired tokens (periodic cleanup)
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (now >= it->second.expires_at) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
private:
    std::unordered_map<std::string, TokenInfo> cache_;
    std::mutex mtx_;
};

} // namespace auth
} // namespace asciimmo
