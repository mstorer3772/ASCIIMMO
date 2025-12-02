#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class SessionDatabaseTest : public ::testing::Test
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

                // Create test user
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                auto result = txn.exec(
                    "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
                    pqxx::params{ "test_session_user", int64_t(123456789012), int64_t(987654321098) }
                );
                test_user_id_ = result[0][0].as<int>();
                txn.commit();
            }

        void TearDown() override
            {
            if (pool_)
                {
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                txn.exec("DELETE FROM users WHERE id = $1", pqxx::params{ test_user_id_ });
                txn.commit();
                }
            pool_.reset();
            }

        Config config_;
        std::unique_ptr<ConnectionPool> pool_;
        int test_user_id_;
    };

TEST_F(SessionDatabaseTest, CreateSession)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    uint64_t token = 123456789012345;
    std::string data = R"({"user":"test","role":"player"})";

    auto result = txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES (" + std::to_string(token) + ", $1, $2::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour') "
        "RETURNING id",
        pqxx::params{ test_user_id_, data }
    );

    ASSERT_EQ(result.size(), 1);
    int session_id = result[0][0].as<int>();
    EXPECT_GT(session_id, 0);

    txn.commit();
    }

TEST_F(SessionDatabaseTest, FindSessionByToken)
    {
    auto conn = pool_->acquire();

    uint64_t token = 987654321098765;

    // Create session
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES (" + std::to_string(token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
        pqxx::params{ test_user_id_ }
    );
    txn.commit();
    }

    // Find session
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT token, user_id FROM sessions WHERE token = " + std::to_string(token)
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(static_cast<uint64_t>(result[0]["token"].as<int64_t>()), token);
    EXPECT_EQ(result[0]["user_id"].as<int>(), test_user_id_);
    }
    }

TEST_F(SessionDatabaseTest, DeleteSession)
    {
    auto conn = pool_->acquire();

    uint64_t token = 111222333444555;

    // Create session
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES (" + std::to_string(token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
        pqxx::params{ test_user_id_ }
    );
    txn.commit();
    }

    // Delete session
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM sessions WHERE token = " + std::to_string(token)
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }

    // Verify deletion
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM sessions WHERE token = " + std::to_string(token)
    );
    EXPECT_EQ(result[0][0].as<int>(), 0);
    }
    }

TEST_F(SessionDatabaseTest, SessionExpiration)
    {
    auto conn = pool_->acquire();

    uint64_t token = 999888777666555;

    // Create expired session
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES (" + std::to_string(token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP - INTERVAL '1 hour')",
        pqxx::params{ test_user_id_ }
    );
    txn.commit();
    }

    // Check if session is expired
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM sessions WHERE token = " + std::to_string(token) + " AND expires_at > CURRENT_TIMESTAMP"
    );
    EXPECT_EQ(result[0][0].as<int>(), 0) << "Expired session should not be found";
    }
    }

TEST_F(SessionDatabaseTest, CleanupExpiredSessions)
    {
    auto conn = pool_->acquire();

    // Create mix of valid and expired sessions
    {
    pqxx::work txn(conn.get());
    uint64_t expired_token = 111111111111111;
    uint64_t valid_token = 222222222222222;
    txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) VALUES "
        "(" + std::to_string(expired_token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP - INTERVAL '1 hour'), "
        "(" + std::to_string(valid_token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
        pqxx::params{ test_user_id_ }
    );
    txn.commit();
    }

    // Run cleanup
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec("SELECT cleanup_expired_sessions()");
    int deleted = result[0][0].as<int>();
    txn.commit();
    EXPECT_GE(deleted, 1) << "Should delete at least one expired session";
    }

    // Verify only valid session remains
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM sessions WHERE user_id = $1",
        pqxx::params{ test_user_id_ }
    );
    EXPECT_EQ(result[0][0].as<int>(), 1);
    }
    }

TEST_F(SessionDatabaseTest, UpdateSessionActivity)
    {
    auto conn = pool_->acquire();

    uint64_t token = 333444555666777;

    // Create session
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES (" + std::to_string(token) + ", $1, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
        pqxx::params{ test_user_id_ }
    );
    txn.commit();
    }

    // Update last activity
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "UPDATE sessions SET last_activity = CURRENT_TIMESTAMP WHERE token = " + std::to_string(token)
    );
    txn.commit();
    }

    // Verify update
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT last_activity FROM sessions WHERE token = " + std::to_string(token)
    );
    EXPECT_FALSE(result[0][0].is_null());
    }
    }
