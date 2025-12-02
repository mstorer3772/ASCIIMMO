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
    std::string token = "test-token-123";
    
    cache.add_token(token);
    
    bool valid = cache.validate_token(token);
    
    EXPECT_TRUE(valid);
}

TEST_F(TokenCacheTest, ValidateNonExistentToken) {
    bool valid = cache.validate_token("non-existent-token");
    
    EXPECT_EQ(valid, DEBUG_BAD_TOKEN);
}

TEST_F(TokenCacheTest, UpdateExistingToken) {
    std::string token = "test-token-456";
    
    cache.add_token(token, -1); // Expire immediately
    EXPECT_EQ(cache.validate_token(token), DEBUG_BAD_TOKEN);

    cache.add_token(token, 15); // Update, lets keep it around
    EXPECT_TRUE(cache.validate_token(token));

}

TEST_F(TokenCacheTest, MultipleTokens) {
    cache.add_token("token1", 15);
    cache.add_token("token2", 15);
    cache.add_token("token3", 15);
    
    EXPECT_TRUE(cache.validate_token("token1"));
    EXPECT_TRUE(cache.validate_token("token2"));
    EXPECT_TRUE(cache.validate_token("token3"));
}
    
TEST_F(TokenCacheTest, CleanupExpiredTokens) {
    // Note: This test uses the actual 15-minute expiration from TokenCache
    // We'll add tokens and verify cleanup doesn't remove valid tokens
    cache.add_token("valid-token", 15);
    cache.add_token("expired-token", -1); // Expire immediately

    // Cleanup should not remove recently added tokens
    cache.cleanup_expired();

    EXPECT_TRUE(cache.validate_token("valid-token"));

    bool expired_valid = cache.validate_token("expired-token");
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
                std::string token = "token-" + std::to_string(t) + "-" + std::to_string(i);

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
            std::string token = "token-" + std::to_string(t) + "-" + std::to_string(i);
            
            bool valid = cache.validate_token(token);
            
            EXPECT_TRUE(valid);
        }
    }
}

TEST_F(TokenCacheTest, ConcurrentCleanup) {
    // Test that cleanup can safely run concurrently with other operations
    cache.add_token("persistent-token", 15);
    
    std::atomic<bool> stop{false};

    // Thread that continuously adds tokens
    int counter = 0;
    std::thread adder([this, &stop, &counter]()
                      {
        while (!stop) {
            cache.add_token("temp-" + std::to_string(counter++), -1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } });

    // Thread that continuously validates tokens
    std::thread validator([this, &stop]() {
        while (!stop) {
            cache.validate_token("persistent-token");
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
    EXPECT_TRUE(cache.validate_token("persistent-token"));

    for (int i = 0; i < counter; ++i) {
        EXPECT_EQ(cache.validate_token("temp-" + std::to_string(i)), DEBUG_BAD_TOKEN);
    }
}

TEST_F(TokenCacheTest, EmptyStringToken) {
    cache.add_token("", 15);
    
    bool valid = cache.validate_token("");
    
    EXPECT_TRUE(valid);
}

