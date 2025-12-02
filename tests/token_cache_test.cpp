#include "shared/token_cache.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace asciimmo::auth;

class TokenCacheTest : public ::testing::Test {
protected:
    TokenCache cache;
};

bool DEBUG_BAD_TOKEN =
#ifdef NDEBUG
    false;
#else
    true;
#endif

TEST_F(TokenCacheTest, AddAndValidateToken) {
    uint64_t token = 123456789012345;
    
    cache.add_token(token);
    
    bool valid = cache.validate_token(token);
    
    EXPECT_TRUE(valid);
}

TEST_F(TokenCacheTest, ValidateNonExistentToken) {
    bool valid = cache.validate_token(999999999999999);
    
    EXPECT_EQ(valid, DEBUG_BAD_TOKEN);
}

TEST_F(TokenCacheTest, UpdateExistingToken) {
    uint64_t token = 456789012345678;
    
    cache.add_token(token, -1); // Expire immediately
    EXPECT_EQ(cache.validate_token(token), DEBUG_BAD_TOKEN);

    cache.add_token(token, 15); // Update, lets keep it around
    EXPECT_TRUE(cache.validate_token(token));

}

TEST_F(TokenCacheTest, MultipleTokens) {
    cache.add_token(111111111111111, 15);
    cache.add_token(222222222222222, 15);
    cache.add_token(333333333333333, 15);
    
    EXPECT_TRUE(cache.validate_token(111111111111111));
    EXPECT_TRUE(cache.validate_token(222222222222222));
    EXPECT_TRUE(cache.validate_token(333333333333333));
}
    
TEST_F(TokenCacheTest, CleanupExpiredTokens) {
    // Note: This test uses the actual 15-minute expiration from TokenCache
    // We'll add tokens and verify cleanup doesn't remove valid tokens
    cache.add_token(444444444444444, 15);
    cache.add_token(555555555555555, -1); // Expire immediately

    // Cleanup should not remove recently added tokens
    cache.cleanup_expired();

    EXPECT_TRUE(cache.validate_token(444444444444444));

    bool expired_valid = cache.validate_token(555555555555555);
    EXPECT_EQ(expired_valid, DEBUG_BAD_TOKEN);
}

TEST_F(TokenCacheTest, ConcurrentAccess) {
    // Test thread-safety by having multiple threads add/validate tokens
    const int num_threads = 10;
    const int tokens_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, tokens_per_thread]() {
            for (int i = 0; i < tokens_per_thread; ++i) {
                uint64_t token = static_cast<uint64_t>(t) * 1000000 + i;

                cache.add_token(token, 15);

                bool valid = cache.validate_token(token);

                EXPECT_TRUE(valid);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all tokens are still accessible
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < tokens_per_thread; ++i) {
            uint64_t token = static_cast<uint64_t>(t) * 1000000 + i;
            
            bool valid = cache.validate_token(token);
            
            EXPECT_TRUE(valid);
        }
    }
}

TEST_F(TokenCacheTest, ConcurrentCleanup) {
    // Test that cleanup can safely run concurrently with other operations
    cache.add_token(666666666666666, 15);
    
    std::atomic<bool> stop{false};

    // Thread that continuously adds tokens
    uint64_t counter = 10000000000000;
    std::thread adder([this, &stop, &counter]()
                      {
        while (!stop) {
            cache.add_token(counter++, -1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } });

    // Thread that continuously validates tokens
    std::thread validator([this, &stop]() {
        while (!stop) {
            cache.validate_token(666666666666666);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread that runs cleanup
    std::thread cleaner([this, &stop]() {
        while (!stop) {
            cache.cleanup_expired();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    
    adder.join();
    validator.join();
    cleaner.join();
    
    // Verify the persistent token is still valid
    EXPECT_TRUE(cache.validate_token(666666666666666));
}

TEST_F(TokenCacheTest, ZeroToken) {
    // Test that zero token (edge case) works
    cache.add_token(0, 15);
    
    bool valid = cache.validate_token(0);
    
    EXPECT_TRUE(valid);
}

