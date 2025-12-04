#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/service_config.hpp"
#include "shared/password_hash.hpp"
#include "shared/email_sender.hpp"
#include "db_pool.hpp"
#include "db_config.hpp"
#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>

static void print_usage(const char* prog)
    {
    std::cerr << "Usage: " << prog << " [--config FILE] [--port P] [--cert FILE] [--key FILE]\n";
    std::cerr << "  Config file defaults to config/services.yaml\n";
    std::cerr << "  Command line options override config file values\n";
    }

int main(int argc, char** argv)
    {
    // Load configuration
    auto& config = asciimmo::config::ServiceConfig::instance();
    std::string config_file = "config/services.yaml";

    for (int i = 1; i < argc; ++i)
        {
        if (std::string(argv[i]) == "--config" && i + 1 < argc)
            {
            config_file = argv[++i];
            break;
            }
        }

    if (!config.load(config_file))
        {
        std::cerr << "Warning: Could not load config file: " << config_file << std::endl;
        }

    int port = config.get_int("auth_service.port", 8081);
    std::string cert_file = config.get_string("global.cert_file", "certs/server.crt");
    std::string key_file = config.get_string("global.key_file", "certs/server.key");
    std::string base_url = config.get_string("auth_service.base_url", "https://localhost:8081");

    for (int i = 1; i < argc; ++i)
        {
        std::string a = argv[i];
        if (a == "--config")
            {
            ++i;
            }
        else if (a == "--port" && i + 1 < argc)
            {
            port = std::stoi(argv[++i]);
            }
        else if (a == "--cert" && i + 1 < argc)
            {
            cert_file = argv[++i];
            }
        else if (a == "--key" && i + 1 < argc)
            {
            key_file = argv[++i];
            }
        else if (a == "-h" || a == "--help")
            {
            print_usage(argv[0]);
            return 0;
            }
        else
            {
            print_usage(argv[0]);
            return 1;
            }
        }

    asciimmo::log::Logger logger("auth-service");

    // Initialize database connection pool
    auto db_config = asciimmo::db::Config::from_env();
    asciimmo::db::ConnectionPool db_pool(db_config);

    asciimmo::email::EmailSender email_sender(
        "localhost", 25,
        "noreply@asciimmo.com",
        "ASCIIMMO");

    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

    logger.info("Starting auth-service on port " + std::to_string(port));

    // POST /auth/register - Register new user
    svr.post("/auth/register", [&db_pool, &email_sender, &logger, &base_url](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&)
        {

        try
            {
            // Parse JSON body (simple parsing - production should use proper JSON library)
            std::string body = req.body();
            std::string username, password, email;

            // Extract username
            auto user_pos = body.find("\"username\":\"");
            if (user_pos != std::string::npos)
                {
                auto start = user_pos + 12;
                auto end = body.find("\"", start);
                username = body.substr(start, end - start);
                }

            // Extract password
            auto pass_pos = body.find("\"password\":\"");
            if (pass_pos != std::string::npos)
                {
                auto start = pass_pos + 12;
                auto end = body.find("\"", start);
                password = body.substr(start, end - start);
                }

            // Extract email
            auto email_pos = body.find("\"email\":\"");
            if (email_pos != std::string::npos)
                {
                auto start = email_pos + 9;
                auto end = body.find("\"", start);
                email = body.substr(start, end - start);
                }

            // Validate input
            if (username.empty() || password.empty() || email.empty())
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"username, password, and email are required"})";
                res.prepare_payload();
                return;
                }

            if (username.length() < 3 || username.length() > 50)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"username must be 3-50 characters"})";
                res.prepare_payload();
                return;
                }

            if (password.length() < 8)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"password must be at least 8 characters"})";
                res.prepare_payload();
                return;
                }

            // Generate salt and hash password
            uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();
            uint64_t password_hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

            // Create user in database
            auto conn = db_pool.acquire();
            pqxx::work txn(conn.get());

            // Check if username already exists
            auto check_result = txn.exec_params(
                pqxx::zview("SELECT id FROM users WHERE username = $1"),
                username
            );

            if (!check_result.empty())
                {
                res.result(boost::beast::http::status::conflict);
                res.body() = R"({"status":"error","message":"username already exists"})";
                res.prepare_payload();
                return;
                }

            // Insert user
            auto user_result = txn.exec_params(
                "INSERT INTO users (username, password_hash, salt, email, is_active, email_confirmed) "
                "VALUES ($1, $2, $3, $4, true, false) RETURNING id",
                username, static_cast<int64_t>(password_hash), static_cast<int64_t>(salt), email

            );

            int user_id = user_result[0][0].as<int>();

            // Generate confirmation token
            uint64_t confirmation_token = asciimmo::auth::PasswordHash::generate_token();
            auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
            auto expires_timestamp = std::chrono::system_clock::to_time_t(expires_at);

            txn.exec_params(
                "INSERT INTO email_confirmation_tokens (user_id, token, expires_at) "
                "VALUES ($1, $2, TO_TIMESTAMP($3))",
                user_id, static_cast<int64_t>(confirmation_token), expires_timestamp
            );

            txn.commit();

            // Send confirmation email
            bool email_sent = email_sender.send_confirmation_email(
                email, username, confirmation_token, base_url
            );

            if (email_sent)
                {
                logger.info("User registered: " + username + " (ID: " + std::to_string(user_id) + ")");
                res.result(boost::beast::http::status::created);
                res.body() = R"({"status":"ok","message":"registration successful, please check your email to confirm your account","user_id":)" + std::to_string(user_id) + "}";
                }
            else
                {
                logger.warning("User registered but email failed: " + username);
                res.result(boost::beast::http::status::created);
                res.body() = R"({"status":"ok","message":"registration successful, but email failed to send","user_id":)" + std::to_string(user_id) + "}";
                }
            res.prepare_payload();

            }
            catch (const pqxx::sql_error& e)
                {
                logger.error("Database error during registration: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"database error"})";
                res.prepare_payload();
                }
            catch (const std::exception& e)
                {
                logger.error("Error during registration: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"internal server error"})";
                res.prepare_payload();
                }
        });

    // GET /auth/confirm?token=xxx - Confirm email address
    svr.get("/auth/confirm", [&db_pool, &logger](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&)
        {

        try
            {
            // Extract token from query string
            std::string target(req.target());
            auto token_pos = target.find("token=");
            if (token_pos == std::string::npos)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"token parameter required"})";
                res.prepare_payload();
                return;
                }

            auto token_start = token_pos + 6;
            auto token_end = target.find("&", token_start);
            std::string token_str = target.substr(token_start, token_end == std::string::npos ? std::string::npos : token_end - token_start);

            uint64_t token;
            try
                {
                token = std::stoull(token_str);
                }
                catch (...)
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"invalid token format"})";
                    res.prepare_payload();
                    return;
                    }

                auto conn = db_pool.acquire();
                pqxx::work txn(conn.get());

                // Find and validate token
                auto token_result = txn.exec_params(
                    "SELECT user_id, expires_at, used FROM email_confirmation_tokens "
                    "WHERE token = $1",
                    static_cast<int64_t>(token)
                );

                if (token_result.empty())
                    {
                    res.result(boost::beast::http::status::not_found);
                    res.body() = R"({"status":"error","message":"invalid or expired token"})";
                    res.prepare_payload();
                    return;
                    }

                int user_id = token_result[0]["user_id"].as<int>();
                bool used = token_result[0]["used"].as<bool>();

                if (used)
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"token already used"})";
                    res.prepare_payload();
                    return;
                    }

                // Check expiration
                auto expires_result = txn.exec_params(
                    pqxx::zview("SELECT expires_at < CURRENT_TIMESTAMP AS expired FROM email_confirmation_tokens WHERE token = $1"),
                    static_cast<int64_t>(token)
                );

                if (expires_result[0]["expired"].as<bool>())
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"token expired"})";
                    res.prepare_payload();
                    return;
                    }

                // Mark token as used
                txn.exec_params(
                    pqxx::zview("UPDATE email_confirmation_tokens SET used = true WHERE token = $1"),
                    static_cast<int64_t>(token)
                );

                // Confirm user's email
                txn.exec_params(
                    pqxx::zview("UPDATE users SET email_confirmed = true WHERE id = $1"),
                    pqxx::params{ user_id }
                );

                txn.commit();

                logger.info("Email confirmed for user ID: " + std::to_string(user_id));

                res.result(boost::beast::http::status::ok);
                res.body() = R"({"status":"ok","message":"email confirmed successfully"})";
                res.prepare_payload();

            }
            catch (const std::exception& e)
                {
                logger.error("Error during email confirmation: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"internal server error"})";
                res.prepare_payload();
                }
        });

    // POST /auth/register - Register new user
    svr.post("/auth/register", [&db_pool, &email_sender, &logger, &base_url](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&)
        {

        try
            {
            std::string body = req.body();
            std::string username, password, email;

            // Simple JSON parsing
            auto user_pos = body.find("\"username\":\"");
            if (user_pos != std::string::npos)
                {
                auto start = user_pos + 12;
                auto end = body.find("\"", start);
                username = body.substr(start, end - start);
                }

            auto pass_pos = body.find("\"password\":\"");
            if (pass_pos != std::string::npos)
                {
                auto start = pass_pos + 12;
                auto end = body.find("\"", start);
                password = body.substr(start, end - start);
                }

            auto email_pos = body.find("\"email\":\"");
            if (email_pos != std::string::npos)
                {
                auto start = email_pos + 9;
                auto end = body.find("\"", start);
                email = body.substr(start, end - start);
                }

            if (username.empty() || password.empty() || email.empty())
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"username, password, and email are required"})";
                res.prepare_payload();
                return;
                }

            if (username.length() < 3 || username.length() > 50)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"username must be 3-50 characters"})";
                res.prepare_payload();
                return;
                }

            if (password.length() < 8)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"password must be at least 8 characters"})";
                res.prepare_payload();
                return;
                }

            uint64_t salt = asciimmo::auth::PasswordHash::generate_salt();
            uint64_t password_hash = asciimmo::auth::PasswordHash::hash_password(password, salt);

            auto conn = db_pool.acquire();
            pqxx::work txn(conn.get());

            auto check_result = txn.exec("SELECT id FROM users WHERE username = " + txn.quote(username));

            if (!check_result.empty())
                {
                res.result(boost::beast::http::status::conflict);
                res.body() = R"({"status":"error","message":"username already exists"})";
                res.prepare_payload();
                return;
                }

            auto user_result = txn.exec(
                "INSERT INTO users (username, password_hash, salt, email, is_active, email_confirmed) "
                "VALUES (" + txn.quote(username) + ", " + txn.quote(password_hash) + ", " +
                txn.quote(salt) + ", " + txn.quote(email) + ", true, false) RETURNING id"
            );

            int user_id = user_result[0][0].as<int>();

            uint64_t confirmation_token = asciimmo::auth::PasswordHash::generate_token();
            auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
            auto expires_timestamp = std::chrono::system_clock::to_time_t(expires_at);

            txn.exec(
                "INSERT INTO email_confirmation_tokens (user_id, token, expires_at) "
                "VALUES (" + txn.quote(user_id) + ", " + std::to_string(confirmation_token) + ", "
                "TO_TIMESTAMP(" + txn.quote(expires_timestamp) + "))"
            );            txn.commit();

            bool email_sent = email_sender.send_confirmation_email(
                email, username, confirmation_token, base_url
            );

            if (email_sent)
                {
                logger.info("User registered: " + username + " (ID: " + std::to_string(user_id) + ")");
                res.result(boost::beast::http::status::created);
                res.body() = R"({"status":"ok","message":"registration successful, please check your email to confirm your account","user_id":)" + std::to_string(user_id) + "}";
                }
            else
                {
                logger.warning("User registered but email failed: " + username);
                res.result(boost::beast::http::status::created);
                res.body() = R"({"status":"ok","message":"registration successful, but email failed to send","user_id":)" + std::to_string(user_id) + "}";
                }
            res.prepare_payload();

            }
            catch (const pqxx::sql_error& e)
                {
                logger.error("Database error during registration: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"database error"})";
                res.prepare_payload();
                }
            catch (const std::exception& e)
                {
                logger.error("Error during registration: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"internal server error"})";
                res.prepare_payload();
                }
        });

    // GET /auth/confirm?token=xxx
    svr.get("/auth/confirm", [&db_pool, &logger](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&)
        {

        try
            {
            std::string target(req.target());
            auto token_pos = target.find("token=");
            if (token_pos == std::string::npos)
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"token parameter required"})";
                res.prepare_payload();
                return;
                }

            auto token_start = token_pos + 6;
            auto token_end = target.find("&", token_start);
            std::string token_str = target.substr(token_start, token_end == std::string::npos ? std::string::npos : token_end - token_start);

            uint64_t token;
            try
                {
                token = std::stoull(token_str);
                }
                catch (...)
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"invalid token format"})";
                    res.prepare_payload();
                    return;
                    }

                auto conn = db_pool.acquire();
                pqxx::work txn(conn.get());

                auto token_result = txn.exec(
                    "SELECT user_id, expires_at, used FROM email_confirmation_tokens WHERE token = " + std::to_string(token)
                );            if (token_result.empty())
                    {
                    res.result(boost::beast::http::status::not_found);
                    res.body() = R"({"status":"error","message":"invalid or expired token"})";
                    res.prepare_payload();
                    return;
                    }

                int user_id = token_result[0]["user_id"].as<int>();
                bool used = token_result[0]["used"].as<bool>();

                if (used)
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"token already used"})";
                    res.prepare_payload();
                    return;
                    }

                auto expires_result = txn.exec(
                    "SELECT expires_at < CURRENT_TIMESTAMP AS expired FROM email_confirmation_tokens WHERE token = " + std::to_string(token)
                );

                if (expires_result[0]["expired"].as<bool>())
                    {
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = R"({"status":"error","message":"token expired"})";
                    res.prepare_payload();
                    return;
                    }

                txn.exec("UPDATE email_confirmation_tokens SET used = true WHERE token = " + std::to_string(token));
                txn.exec("UPDATE users SET email_confirmed = true WHERE id = " + std::to_string(user_id));

                txn.commit();

                logger.info("Email confirmed for user ID: " + std::to_string(user_id));

                res.result(boost::beast::http::status::ok);
                res.body() = R"({"status":"ok","message":"email confirmed successfully"})";
                res.prepare_payload();

            }
            catch (const std::exception& e)
                {
                logger.error("Error during email confirmation: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"internal server error"})";
                res.prepare_payload();
                }
        });

    // POST /auth/login
    svr.post("/auth/login", [&db_pool, &logger](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&)
        {

        try
            {
            std::string body = req.body();
            std::string username, password;

            auto user_pos = body.find("\"username\":\"");
            if (user_pos != std::string::npos)
                {
                auto start = user_pos + 12;
                auto end = body.find("\"", start);
                username = body.substr(start, end - start);
                }

            auto pass_pos = body.find("\"password\":\"");
            if (pass_pos != std::string::npos)
                {
                auto start = pass_pos + 12;
                auto end = body.find("\"", start);
                password = body.substr(start, end - start);
                }

            if (username.empty() || password.empty())
                {
                res.result(boost::beast::http::status::bad_request);
                res.body() = R"({"status":"error","message":"username and password required"})";
                res.prepare_payload();
                return;
                }

            auto conn = db_pool.acquire();
            pqxx::work txn(conn.get());

            auto user_result = txn.exec(
                "SELECT id, password_hash, salt, is_active, email_confirmed FROM users WHERE username = " + txn.quote(username)
            );

            if (user_result.empty())
                {
                res.result(boost::beast::http::status::unauthorized);
                res.body() = R"({"status":"error","message":"invalid username or password"})";
                res.prepare_payload();
                return;
                }

            int user_id = user_result[0]["id"].as<int>();
            uint64_t stored_hash = static_cast<uint64_t>(user_result[0]["password_hash"].as<int64_t>());
            uint64_t salt = static_cast<uint64_t>(user_result[0]["salt"].as<int64_t>());
            bool is_active = user_result[0]["is_active"].as<bool>();
            bool email_confirmed = user_result[0]["email_confirmed"].as<bool>();

            if (!asciimmo::auth::PasswordHash::verify_password(password, salt, stored_hash))
                {
                res.result(boost::beast::http::status::unauthorized);
                res.body() = R"({"status":"error","message":"invalid username or password"})";
                res.prepare_payload();
                return;
                }

            if (!is_active)
                {
                res.result(boost::beast::http::status::forbidden);
                res.body() = R"({"status":"error","message":"account is not active"})";
                res.prepare_payload();
                return;
                }

            if (!email_confirmed)
                {
                res.result(boost::beast::http::status::forbidden);
                res.body() = R"({"status":"error","message":"please confirm your email before logging in"})";
                res.prepare_payload();
                return;
                }

            txn.exec("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = " + std::to_string(user_id));
            txn.commit();

            logger.info("User logged in: " + username + " (ID: " + std::to_string(user_id) + ")");
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok","token":"stub-token-12345","user_id":)" + std::to_string(user_id) + R"(,"message":"login successful, token generation pending"})";
            res.prepare_payload();

            }
            catch (const std::exception& e)
                {
                logger.error("Error during login: " + std::string(e.what()));
                res.result(boost::beast::http::status::internal_server_error);
                res.body() = R"({"status":"error","message":"internal server error"})";
                res.prepare_payload();
                }
        });

    svr.post("/shutdown", [&ioc, &logger](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&)
        {
        logger.info("Shutdown requested via /shutdown endpoint");
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","message":"shutting down"})";
        res.prepare_payload();
        ioc.stop();
        });

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto)
        {
        logger.info("Shutdown signal received");
        ioc.stop();
        });

    svr.run();
    ioc.run();

    logger.info("Service stopped");
    return 0;
    }
