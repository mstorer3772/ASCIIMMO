#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class AuthDatabaseTest : public ::testing::Test
    {
    protected:
        void SetUp() override
            {
            config_ = Config::from_env();
            try
                {
                pool_ = std::make_unique<ConnectionPool>(config_);
                }
                catch (const std::exception& e)
                    {
                    GTEST_SKIP() << "Database not available: " << e.what();
                    }
            }

        void TearDown() override
            {
            // Cleanup test data
            if (pool_)
                {
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                txn.exec("DELETE FROM users WHERE username LIKE 'test_user_%'");
                txn.commit();
                }
            pool_.reset();
            }

        Config config_;
        std::unique_ptr<ConnectionPool> pool_;
    };

TEST_F(AuthDatabaseTest, CreateUser)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    std::string username = "test_user_create";
    int64_t password_hash = 123456789012345;
    int64_t salt = 987654321098765;

    auto result = txn.exec(
        "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ username, password_hash, salt }
    );

    ASSERT_EQ(result.size(), 1);
    int user_id = result[0][0].as<int>();
    EXPECT_GT(user_id, 0);

    txn.commit();
    }

TEST_F(AuthDatabaseTest, FindUserByUsername)
    {
    auto conn = pool_->acquire();

    int64_t password_hash = 111222333444555;
    int64_t salt = 555444333222111;

    // Insert test user
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3)",
        pqxx::params{ "test_user_find", password_hash, salt }
    );
    txn.commit();
    }

    // Find user
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT id, username, password_hash FROM users WHERE username = $1",
        pqxx::params{ "test_user_find" }
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["username"].as<std::string>(), "test_user_find");
    EXPECT_EQ(result[0]["password_hash"].as<int64_t>(), password_hash);
    }
    }

TEST_F(AuthDatabaseTest, DeleteUser)
    {
    auto conn = pool_->acquire();

    // Insert test user
    int user_id;
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ "test_user_delete", int64_t(777888999000111), int64_t(111000999888777) }
    );
    user_id = result[0][0].as<int>();
    txn.commit();
    }

    // Delete user
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM users WHERE id = $1",
        pqxx::params{ user_id }
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }

    // Verify deletion
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM users WHERE id = $1",
        pqxx::params{ user_id }
    );
    EXPECT_EQ(result[0][0].as<int>(), 0);
    }
    }

TEST_F(AuthDatabaseTest, UniqueUsernameConstraint)
    {
    auto conn = pool_->acquire();

    std::string username = "test_user_unique";

    // Insert first user
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3)",
        pqxx::params{ username, int64_t(123456789), int64_t(987654321) }
    );
    txn.commit();
    }

    // Try to insert duplicate username
    EXPECT_THROW({
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3)",
            pqxx::params{username, int64_t(111111111), int64_t(222222222)}
        );
        txn.commit();
        }, pqxx::unique_violation);
    }

TEST_F(AuthDatabaseTest, UpdateLastLogin)
    {
    auto conn = pool_->acquire();

    // Insert test user
    int user_id;
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ "test_user_login", int64_t(333444555666), int64_t(666555444333) }
    );
    user_id = result[0][0].as<int>();
    txn.commit();
    }

    // Update last_login
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = $1",
        pqxx::params{ user_id }
    );
    txn.commit();
    }

    // Verify update
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT last_login FROM users WHERE id = $1",
        pqxx::params{ user_id }
    );
    EXPECT_FALSE(result[0][0].is_null()) << "last_login should be set";
    }
    }
