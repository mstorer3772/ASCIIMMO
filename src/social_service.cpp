#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "httplib.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P]\n";
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

int main(int argc, char** argv) {
    int port = 8083;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    httplib::Server svr;

    // GET /chat/global?limit=N - retrieve recent global chat messages
    svr.Get("/chat/global", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 50;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch(...) {}
        }
        std::lock_guard<std::mutex> lock(data_mtx);
        std::string json = "{\"messages\":[";
        int start = std::max(0, (int)global_chat.size() - limit);
        for (size_t i = start; i < global_chat.size(); ++i) {
            if (i > start) json += ",";
            json += "{\"from\":\"" + global_chat[i].from + "\",\"message\":\"" + global_chat[i].message + "\",\"timestamp\":" + std::to_string(global_chat[i].timestamp) + "}";
        }
        json += "]}";
        res.set_content(json, "application/json");
    });

    // POST /chat/global - send a message (expects {"from":"...", "message":"..."})
    svr.Post("/chat/global", [](const httplib::Request& req, httplib::Response& res) {
        // TODO: parse JSON properly; stub: extract from body
        std::lock_guard<std::mutex> lock(data_mtx);
        ChatMessage msg;
        msg.from = "user"; // stub
        msg.message = req.body;
        msg.timestamp = std::time(nullptr);
        global_chat.push_back(msg);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // GET /friends/:user - get friend list for user
    svr.Get(R"(/friends/(\w+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string user = req.matches[1];
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = friends.find(user);
        std::string json = "{\"user\":\"" + user + "\",\"friends\":[";
        if (it != friends.end()) {
            bool first = true;
            for (const auto& f : it->second) {
                if (!first) json += ",";
                json += "\"" + f + "\"";
                first = false;
            }
        }
        json += "]}";
        res.set_content(json, "application/json");
    });

    // POST /friends/:user/add - add friend (expects {"friend":"..."})
    svr.Post(R"(/friends/(\w+)/add)", [](const httplib::Request& req, httplib::Response& res) {
        std::string user = req.matches[1];
        std::string friend_name = "friend"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        friends[user].insert(friend_name);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // POST /party/create - create a party (expects {"leader":"..."})
    svr.Post("/party/create", [](const httplib::Request& req, httplib::Response& res) {
        std::string leader = "leader"; // TODO: parse JSON
        std::string party_id = "party-" + std::to_string(std::hash<std::string>{}(leader + std::to_string(std::time(nullptr))));
        std::lock_guard<std::mutex> lock(data_mtx);
        Party p;
        p.leader = leader;
        p.members.insert(leader);
        parties[party_id] = p;
        res.set_content("{\"status\":\"ok\",\"party_id\":\"" + party_id + "\"}", "application/json");
    });

    // POST /party/:id/join - join a party (expects {"user":"..."})
    svr.Post(R"(/party/(\w+-\w+)/join)", [](const httplib::Request& req, httplib::Response& res) {
        std::string party_id = req.matches[1];
        std::string user = "user"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = parties.find(party_id);
        if (it != parties.end()) {
            it->second.members.insert(user);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"party not found\"}", "application/json");
        }
    });

    // GET /party/:id - get party info
    svr.Get(R"(/party/(\w+-\w+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string party_id = req.matches[1];
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = parties.find(party_id);
        if (it != parties.end()) {
            std::string json = "{\"party_id\":\"" + party_id + "\",\"leader\":\"" + it->second.leader + "\",\"members\":[";
            bool first = true;
            for (const auto& m : it->second.members) {
                if (!first) json += ",";
                json += "\"" + m + "\"";
                first = false;
            }
            json += "]}";
            res.set_content(json, "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"party not found\"}", "application/json");
        }
    });

    // POST /guild/create - create a guild (expects {"name":"...", "leader":"..."})
    svr.Post("/guild/create", [](const httplib::Request& req, httplib::Response& res) {
        std::string name = "guild"; // TODO: parse JSON
        std::string leader = "leader";
        std::string guild_id = "guild-" + name;
        std::lock_guard<std::mutex> lock(data_mtx);
        Guild g;
        g.name = name;
        g.leader = leader;
        g.members.insert(leader);
        guilds[guild_id] = g;
        res.set_content("{\"status\":\"ok\",\"guild_id\":\"" + guild_id + "\"}", "application/json");
    });

    // POST /guild/:id/join - join a guild (expects {"user":"..."})
    svr.Post(R"(/guild/([\w-]+)/join)", [](const httplib::Request& req, httplib::Response& res) {
        std::string guild_id = req.matches[1];
        std::string user = "user"; // TODO: parse JSON
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = guilds.find(guild_id);
        if (it != guilds.end()) {
            it->second.members.insert(user);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"guild not found\"}", "application/json");
        }
    });

    // GET /guild/:id - get guild info
    svr.Get(R"(/guild/([\w-]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string guild_id = req.matches[1];
        std::lock_guard<std::mutex> lock(data_mtx);
        auto it = guilds.find(guild_id);
        if (it != guilds.end()) {
            std::string json = "{\"guild_id\":\"" + guild_id + "\",\"name\":\"" + it->second.name + "\",\"leader\":\"" + it->second.leader + "\",\"members\":[";
            bool first = true;
            for (const auto& m : it->second.members) {
                if (!first) json += ",";
                json += "\"" + m + "\"";
                first = false;
            }
            json += "]}";
            res.set_content(json, "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"guild not found\"}", "application/json");
        }
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"service\":\"social\"}", "application/json");
    });

    std::cout << "[social-service] listening on port " << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
