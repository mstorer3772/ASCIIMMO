#pragma once

#include "shared/logger.hpp"
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace asciimmo
{
namespace auth
{

struct TokenInfo
    {
    std::chrono::steady_clock::time_point expires_at;
    bool isAdmin = false;

    bool is_valid() const
        {
        return std::chrono::steady_clock::now() < expires_at;
        }
    };

class TokenCache
    {
    public:
        // Add or update a token with 15 minute expiration
        void add_token(uint64_t token, int expirationMinutes = 15, bool isAdmin = false)
            {
            std::lock_guard<std::mutex> lock(mtx_);
            auto expires = std::chrono::steady_clock::now() + std::chrono::minutes(expirationMinutes);
            cache_[token] = TokenInfo{ expires, isAdmin };
            }

        // Check if token is valid and return user data
        bool validate_token(uint64_t token)
            {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = cache_.find(token);

#ifdef NDEBUG
            // Release build: enforce token validation
            return (it != cache_.end() && it->second.is_valid());
#else
            // Debug build: always return true, but log when token is invalid
            if (it == cache_.end())
                {
                logger_.info("Token validation bypassed (debug mode): token not found - " + std::to_string(token));
                return true;
                }
            if (!it->second.is_valid())
                {
                logger_.info("Token validation bypassed (debug mode): token expired - " + std::to_string(token));
                return true;
                }
            return true;
#endif
            }

        bool validate_admin(uint64_t token)
            {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = cache_.find(token);
            if (it == cache_.end() || !it->second.is_valid())
                {
                return false;
                }
            return it->second.isAdmin;
            }

        // Remove expired tokens (periodic cleanup)
        void cleanup_expired()
            {
            std::lock_guard<std::mutex> lock(mtx_);
            auto now = std::chrono::steady_clock::now();
            for (auto it = cache_.begin(); it != cache_.end(); )
                {
                if (now >= it->second.expires_at)
                    {
                    it = cache_.erase(it);
                    }
                else
                    {
                    ++it;
                    }
                }
            }

    private:
        std::unordered_map<uint64_t, TokenInfo> cache_;
        std::mutex mtx_;
        log::Logger logger_{ "TokenCache" };
    };

} // namespace auth
} // namespace asciimmo
