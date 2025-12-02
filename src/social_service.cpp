#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/token_cache.hpp"
#include "shared/service_config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--config FILE] [--port P] [--cert FILE] [--key FILE]\n";
    std::cerr << "  Config file defaults to config/services.yaml\n";
    std::cerr << "  Command line options override config file values\n";
}

// In-memory data structures (ephemeral; replace with persistent storage)
struct ChatMessage {
    std::string from;
    std::string message;
    long long timestamp;
};

struct Party {
    std::string leader;
    std::unordered_set<std::string> members;
};

struct Guild {
    std::string name;
    std::string leader;
    std::unordered_set<std::string> members;
};

static std::vector<ChatMessage> global_chat;
static std::unordered_map<std::string, std::unordered_set<std::string>> friends; // user -> set of friends
static std::unordered_map<std::string, Party> parties; // party_id -> Party
static std::unordered_map<std::string, Guild> guilds; // guild_id -> Guild
static std::mutex data_mtx;

// Parse query string parameters
static std::string get_param(const std::string& target, const std::string& key) {
    auto pos = target.find('?');
    if (pos == std::string::npos) return "";
    
    std::string query = target.substr(pos + 1);
    std::string search = key + "=";
    pos = query.find(search);
    if (pos == std::string::npos) return "";
    
    auto start = pos + search.length();
    auto end = query.find('&', start);
    return query.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// Validate session token
static bool validate_session_token(const std::string& target, asciimmo::auth::TokenCache& cache) {
    std::string token = get_param(target, "session_token");

    if (token.empty()) return false;

    return cache.validate_token(token);
}

int main(int argc, char** argv) {
    // Load configuration
    auto& config = asciimmo::config::ServiceConfig::instance();
    std::string config_file = "config/services.yaml";
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_file = argv[++i];
            break;
        }
    }
    
    if (!config.load(config_file)) {
        std::cerr << "Warning: Could not load config file: " << config_file << std::endl;
    }
    
    int port = config.get_int("social_service.port", 8083);
    std::string cert_file = config.get_string("global.cert_file", "certs/server.crt");
    std::string key_file = config.get_string("global.key_file", "certs/server.key");

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config") {
            ++i;
        } else if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "--cert" && i + 1 < argc) {
            cert_file = argv[++i];
        } else if (a == "--key" && i + 1 < argc) {
            key_file = argv[++i];
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    asciimmo::log::Logger logger("social-service");
    asciimmo::auth::TokenCache token_cache;
    
    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

    logger.info("Starting social-service on port " + std::to_string(port));

    // Token registration endpoint (called by session service)
    svr.post("/token/register", [&token_cache, &logger](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        // TODO: parse JSON properly; for now extract token from body
        std::string body = req.body();
        // Simple extraction (production should use proper JSON parser)
        auto token_pos = body.find("\"token\":\"");
        if (token_pos != std::string::npos) {
            auto start = token_pos + 9;
            auto end = body.find("\"", start);
            std::string token = body.substr(start, end - start);
            token_cache.add_token(token);
            logger.info("Registered token: " + token);
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok"})";
        } else {
            res.result(boost::beast::http::status::bad_request);
            res.body() = R"({"status":"error","message":"invalid request"})";
        }
        res.prepare_payload();
    });

    // GET /chat/global?session_token=xxx&limit=N - retrieve recent global chat messages
    svr.get("/chat/global", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        int limit = 50;
        auto limit_str = get_param(target, "limit");
        if (!limit_str.empty()) {
            try { limit = std::stoi(limit_str); } catch(...) {}
        }
        
        std::lock_guard<std::mutex> lock(data_mtx);
        std::string json = R"({"messages":[)";
        int start = std::max(0, (int)global_chat.size() - limit);
        for (size_t i = start; i < global_chat.size(); ++i) {
            if (i > start) json += ",";
            json += R"({"from":")" + global_chat[i].from + R"(","message":")" + global_chat[i].message + R"(","timestamp":)" + std::to_string(global_chat[i].timestamp) + "}";
        }
        json += "]}";
        res.result(boost::beast::http::status::ok);
        res.body() = json;
        res.prepare_payload();
    });

    // POST /chat/global?session_token=xxx - send a message (expects {"from":"...", "message":"..."})
    svr.post("/chat/global", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        // TODO: parse JSON properly; stub: extract from body
        std::lock_guard<std::mutex> lock(data_mtx);
        ChatMessage msg;
        msg.from = "user"; // stub
        msg.message = req.body();
        msg.timestamp = std::time(nullptr);
        global_chat.push_back(msg);
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok"})";
        res.prepare_payload();
    });

    // GET /friends/:user?session_token=xxx - get friend list for user
    svr.get(R"(/friends/(\w+))", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string user = matches[1].str();
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = friends.find(user);
        std::string json = R"({"user":")" + user + R"(","friends":[)";
        if (it != friends.end()) {
            bool first = true;
            for (const auto& f : it->second) {
                if (!first) json += ",";
                json += "\"" + f + "\"";
                first = false;
            }
        }
        json += "]}";
        res.result(boost::beast::http::status::ok);
        res.body() = json;
        res.prepare_payload();
    });

    // POST /friends/:user/add?session_token=xxx - add friend (expects {"friend":"..."})
    svr.post(R"(/friends/(\w+)/add)", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string user = matches[1].str();
        std::string friend_name = "friend"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        friends[user].insert(friend_name);
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok"})";
        res.prepare_payload();
    });

    // POST /party/create?session_token=xxx - create a party (expects {"leader":"..."})
    svr.post("/party/create", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string leader = "leader"; // TODO: parse JSON
        std::string party_id = "party-" + std::to_string(std::hash<std::string>{}(leader + std::to_string(std::time(nullptr))));
        std::lock_guard<std::mutex> lock(data_mtx);
        Party p;
        p.leader = leader;
        p.members.insert(leader);
        parties[party_id] = p;
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","party_id":")" + party_id + R"("})";
        res.prepare_payload();
    });

    // POST /party/:id/join?session_token=xxx - join a party (expects {"user":"..."})
    svr.post(R"(/party/([\w-]+)/join)", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string party_id = matches[1].str();
        std::string user = "user"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = parties.find(party_id);
        if (it != parties.end()) {
            it->second.members.insert(user);
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok"})";
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"party not found"})";
        }
        res.prepare_payload();
    });

    // GET /party/:id?session_token=xxx - get party info
    svr.get(R"(/party/([\w-]+))", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string party_id = matches[1].str();
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = parties.find(party_id);
        if (it != parties.end()) {
            std::string json = R"({"party_id":")" + party_id + R"(","leader":")" + it->second.leader + R"(","members":[)";
            bool first = true;
            for (const auto& m : it->second.members) {
                if (!first) json += ",";
                json += "\"" + m + "\"";
                first = false;
            }
            json += "]}";
            res.result(boost::beast::http::status::ok);
            res.body() = json;
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"party not found"})";
        }
        res.prepare_payload();
    });

    // POST /guild/create?session_token=xxx - create a guild (expects {"name":"...", "leader":"..."})
    svr.post("/guild/create", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string name = "guild"; // TODO: parse JSON
        std::string leader = "leader";
        std::string guild_id = "guild-" + name;
        std::lock_guard<std::mutex> lock(data_mtx);
        Guild g;
        g.name = name;
        g.leader = leader;
        g.members.insert(leader);
        guilds[guild_id] = g;
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","guild_id":")" + guild_id + R"("})";
        res.prepare_payload();
    });

    // POST /guild/:id/join?session_token=xxx - join a guild (expects {"user":"..."})
    svr.post(R"(/guild/([\w-]+)/join)", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string guild_id = matches[1].str();
        std::string user = "user"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = guilds.find(guild_id);
        if (it != guilds.end()) {
            it->second.members.insert(user);
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok"})";
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"guild not found"})";
        }
        res.prepare_payload();
    });

    // GET /guild/:id?session_token=xxx - get guild info
    svr.get(R"(/guild/([\w-]+))", [&token_cache](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string target(req.target());
        
        // Validate session token
        if (!validate_session_token(target, token_cache)) {
            res.result(boost::beast::http::status::unauthorized);
            res.body() = R"({"status":"error","message":"invalid or missing session token"})";
            res.prepare_payload();
            return;
        }
        
        std::string guild_id = matches[1].str();
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = guilds.find(guild_id);
        if (it != guilds.end()) {
            std::string json = R"({"guild_id":")" + guild_id + R"(","name":")" + it->second.name + R"(","leader":")" + it->second.leader + R"(","members":[)";
            bool first = true;
            for (const auto& m : it->second.members) {
                if (!first) json += ",";
                json += "\"" + m + "\"";
                first = false;
            }
            json += "]}";
            res.result(boost::beast::http::status::ok);
            res.body() = json;
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"guild not found"})";
        }
        res.prepare_payload();
    });

    svr.get("/health", [](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        // Health endpoint doesn't require session token
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","service":"social"})";
        res.prepare_payload();
    });

    svr.post("/shutdown", [&ioc, &logger](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        logger.info("Shutdown requested via /shutdown endpoint");
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","message":"shutting down"})";
        res.prepare_payload();
        ioc.stop();
    });

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){
        logger.info("Shutdown signal received");
        ioc.stop();
    });
    
    svr.run();
    ioc.run();
    
    logger.info("Service stopped");
    return 0;
}
