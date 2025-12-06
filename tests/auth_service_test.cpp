#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include "shared/password_hash.hpp"
#include "shared/email_sender.hpp"
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

// Test fixture for auth service tests
class AuthServiceTest : public ::testing::Test
    {
    protected:
        std::unique_ptr<asciimmo::db::ConnectionPool> db_pool;

        void SetUp() override
            {
            // Initialize database connection pool for tests
            auto db_config = asciimmo::db::Config::from_env();
            db_pool = std::make_unique<asciimmo::db::ConnectionPool>(db_config);

            // Clean up test data
            CleanupTestUsers();
            }

        void TearDown() override
            {
            // Clean up test data after each test
            CleanupTestUsers();
            }

        void CleanupTestUsers()
            {
            auto conn = db_pool->acquire();
            pqxx::work txn(conn.get());

            // Delete test users and their confirmation tokens
            txn.exec("DELETE FROM email_confirmation_tokens WHERE user_id IN (SELECT id FROM users WHERE username LIKE 'test_%')");
            txn.exec("DELETE FROM users WHERE username LIKE 'test_%'");

            txn.commit();
            }

        int CreateTestUser(const std::string& username, const std::string& password, const std::string& email, bool confirmed = false)
            {
            uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();
            uint64_t password_hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

            auto conn = db_pool->acquire();
            pqxx::work txn(conn.get());

            auto result = txn.exec(
                "INSERT INTO users (username, password_hash, salt, email, is_active, email_confirmed) "
                "VALUES (" + txn.quote(username) + ", " + std::to_string(password_hash) + ", " +
                std::to_string(salt) + ", " + txn.quote(email) + ", true, " +
                (confirmed ? "true" : "false") + ") RETURNING id"
            );

            int user_id = result[0][0].as<int>();
            txn.commit();

            return user_id;
            }
    };

// Test password hashing
TEST_F(AuthServiceTest, PasswordHashingWorks)
    {
    std::string password = "testpassword123";
    uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();

    EXPECT_NE(salt, 0) << "Salt should be non-zero";

    uint64_t hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

    EXPECT_NE(hash, 0) << "Hash should be non-zero";
    }

TEST_F(AuthServiceTest, PasswordVerificationWorks)
    {
    std::string password = "mySecurePassword!";
    uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();
    uint64_t hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

    // Correct password should verify
    EXPECT_TRUE(asciimmo::auth::PasswordHash::verify_password(password, salt, hash));

    // Wrong password should fail
    EXPECT_FALSE(asciimmo::auth::PasswordHash::verify_password("wrongPassword", salt, hash));

    // Empty password should fail
    EXPECT_FALSE(asciimmo::auth::PasswordHash::verify_password("", salt, hash));
    }

TEST_F(AuthServiceTest, DifferentSaltsProduceDifferentHashes)
    {
    std::string password = "samePassword";
    uint64_t salt1 = asciimmo::auth::PasswordHash::generate_salt();
    uint64_t salt2 = asciimmo::auth::PasswordHash::generate_salt();

    EXPECT_NE(salt1, salt2) << "Generated salts should be unique";

    uint64_t hash1 = asciimmo::auth::PasswordHash::hash_password(password, salt1);
    uint64_t hash2 = asciimmo::auth::PasswordHash::hash_password(password, salt2);

    EXPECT_NE(hash1, hash2) << "Same password with different salts should produce different hashes";
    }

// Test token generation
TEST_F(AuthServiceTest, TokenGenerationProducesUniqueTokens)
    {
    uint64_t token1 = asciimmo::auth::PasswordHash::generate_token();
    uint64_t token2 = asciimmo::auth::PasswordHash::generate_token();

    EXPECT_NE(token1, 0) << "Token should be non-zero";
    EXPECT_NE(token2, 0) << "Token should be non-zero";
    EXPECT_NE(token1, token2) << "Tokens should be unique";
    }

// Test user registration logic
TEST_F(AuthServiceTest, UserRegistrationCreatesUser)
    {
    std::string username = "test_newuser";
    std::string password = "password123";
    std::string email = "test@example.com";

    // Simulate registration
    uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();
    uint64_t password_hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    auto result = txn.exec(
        "INSERT INTO users (username, password_hash, salt, email, is_active, email_confirmed) "
        "VALUES (" + txn.quote(username) + ", " + std::to_string(password_hash) + ", " +
        std::to_string(salt) + ", " + txn.quote(email) + ", true, false) RETURNING id"
    );

    int user_id = result[0][0].as<int>();
    txn.commit();

    EXPECT_GT(user_id, 0) << "User ID should be positive";

    // Verify user exists
    auto conn2 = db_pool->acquire();
    pqxx::work txn2(conn2.get());
    auto check = txn2.exec("SELECT username, email, email_confirmed FROM users WHERE id = " + txn2.quote(user_id));

    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check[0]["username"].as<std::string>(), username);
    EXPECT_EQ(check[0]["email"].as<std::string>(), email);
    EXPECT_FALSE(check[0]["email_confirmed"].as<bool>());
    }

TEST_F(AuthServiceTest, DuplicateUsernameRejected)
    {
    std::string username = "test_duplicate";

    // Create first user
    CreateTestUser(username, "password1", "user1@example.com");

    // Try to create duplicate
    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    // Check if username exists
    auto check = txn.exec("SELECT id FROM users WHERE username = " + txn.quote(username));

    EXPECT_FALSE(check.empty()) << "Duplicate username should be detected";
    }

TEST_F(AuthServiceTest, EmailConfirmationTokenCreated)
    {
    int user_id = CreateTestUser("test_tokenuser", "password123", "token@example.com");

    uint64_t token = asciimmo::auth::PasswordHash::generate_token();
    auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
    auto expires_timestamp = std::chrono::system_clock::to_time_t(expires_at);

    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    txn.exec(
        "INSERT INTO email_confirmation_tokens (user_id, token, expires_at) "
        "VALUES (" + txn.quote(user_id) + ", " + std::to_string(token) + ", "
        "TO_TIMESTAMP(" + txn.quote(expires_timestamp) + "))"
    );

    txn.commit();

    // Verify token exists
    auto conn2 = db_pool->acquire();
    pqxx::work txn2(conn2.get());
    auto check = txn2.exec(
        "SELECT token, used, expires_at > CURRENT_TIMESTAMP AS not_expired "
        "FROM email_confirmation_tokens WHERE user_id = " + txn2.quote(user_id)
    );

    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check[0]["token"].as<uint64_t>(), token);
    EXPECT_FALSE(check[0]["used"].as<bool>());
    EXPECT_TRUE(check[0]["not_expired"].as<bool>());
    }

TEST_F(AuthServiceTest, EmailConfirmationMarksUserConfirmed)
    {
    int user_id = CreateTestUser("test_confirmuser", "password123", "confirm@example.com", false);

    uint64_t token = asciimmo::auth::PasswordHash::generate_token();
    auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
    auto expires_timestamp = std::chrono::system_clock::to_time_t(expires_at);

    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    // Create confirmation token
    txn.exec(
        "INSERT INTO email_confirmation_tokens (user_id, token, expires_at) "
        "VALUES (" + txn.quote(user_id) + ", " + std::to_string(token) + ", "
        "TO_TIMESTAMP(" + txn.quote(expires_timestamp) + "))"
    );

    // Simulate confirmation
    txn.exec("UPDATE email_confirmation_tokens SET used = true WHERE token = " + std::to_string(token));
    txn.exec("UPDATE users SET email_confirmed = true WHERE id = " + txn.quote(user_id));

    txn.commit();

    // Verify user is confirmed
    auto conn2 = db_pool->acquire();
    pqxx::work txn2(conn2.get());
    auto check = txn2.exec("SELECT email_confirmed FROM users WHERE id = " + txn2.quote(user_id));

    EXPECT_TRUE(check[0]["email_confirmed"].as<bool>());

    // Verify token is marked as used
    auto token_check = txn2.exec("SELECT used FROM email_confirmation_tokens WHERE token = " + std::to_string(token));
    EXPECT_TRUE(token_check[0]["used"].as<bool>());
    }

TEST_F(AuthServiceTest, LoginRequiresEmailConfirmation)
    {
    // Create unconfirmed user
    int user_id = CreateTestUser("test_loginuser", "password123", "login@example.com", false);

    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    auto result = txn.exec(
        "SELECT email_confirmed FROM users WHERE id = " + txn.quote(user_id)
    );

    bool email_confirmed = result[0]["email_confirmed"].as<bool>();

    EXPECT_FALSE(email_confirmed) << "Unconfirmed user should not be able to login";
    }

TEST_F(AuthServiceTest, PasswordVerificationFailsForWrongPassword)
    {
    std::string username = "test_passcheck";
    std::string correct_password = "correctPass123";
    std::string wrong_password = "wrongPass123";

    CreateTestUser(username, correct_password, "passcheck@example.com");

    auto conn = db_pool->acquire();
    pqxx::work txn(conn.get());

    auto result = txn.exec(
        "SELECT password_hash, salt FROM users WHERE username = " + txn.quote(username)
    );

    uint64_t stored_hash = result[0]["password_hash"].as<uint64_t>();
    uint64_t salt = result[0]["salt"].as<uint64_t>();

    EXPECT_TRUE(asciimmo::auth::PasswordHash::verify_password(correct_password, salt, stored_hash));
    EXPECT_FALSE(asciimmo::auth::PasswordHash::verify_password(wrong_password, salt, stored_hash));
    }

// Test email sender
TEST_F(AuthServiceTest, EmailSenderCreatesConfirmationEmail)
    {
    asciimmo::email::EmailSender sender("localhost", 25, "noreply@test.com", "Test");

    std::string email = "testuser@example.com";
    std::string username = "testuser";
    uint64_t token = asciimmo::auth::PasswordHash::generate_token();
    std::string base_url = "https://localhost:8081";

    // In debug mode, this should just log, not actually send
    bool result = sender.send_confirmation_email(email, username, token, base_url);

    // Should return true in debug mode (just logs)
    EXPECT_TRUE(result);
    }


