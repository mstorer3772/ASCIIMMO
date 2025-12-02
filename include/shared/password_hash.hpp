#pragma once

#include <string>
#include <random>
#include <cstdint>
#include <openssl/sha.h>

namespace asciimmo
{
namespace auth
{

class PasswordHash
    {
    public:
        // Generate a random 64-bit salt (limited to signed int64 range for database compatibility)
        static uint64_t generate_salt()
            {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            // Limit to signed int64 range (0 to INT64_MAX)
            std::uniform_int_distribution<int64_t> dis(0, INT64_MAX);
            return static_cast<uint64_t>(dis(gen));
            }

        // Generate a random 64-bit token (for email confirmation, password reset, etc.)
        static uint64_t generate_token()
            {
            return generate_salt();
            }

        // Hash password with salt using SHA256, return first 64 bits as uint64_t (limited to signed range)
        static uint64_t hash_password(const std::string& password, uint64_t salt)
            {
            // Convert salt to bytes
            std::string salt_bytes;
            salt_bytes.resize(sizeof(uint64_t));
            for (size_t i = 0; i < sizeof(uint64_t); ++i)
                {
                salt_bytes[i] = static_cast<char>((salt >> (i * 8)) & 0xFF);
                }

            std::string salted = salt_bytes + password;
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(salted.c_str()), salted.length(), hash);

            // Convert first 8 bytes of hash to uint64_t, but mask to signed int64 range
            uint64_t result = 0;
            for (int i = 0; i < 8; ++i)
                {
                result |= static_cast<uint64_t>(hash[i]) << (i * 8);
                }
            // Ensure result fits in signed int64 range
            return result & INT64_MAX;
            }

        // Verify password against hash and salt
        static bool verify_password(const std::string& password, uint64_t salt, uint64_t hash)
            {
            return hash_password(password, salt) == hash;
            }
    };

} // namespace auth
} // namespace asciimmo
