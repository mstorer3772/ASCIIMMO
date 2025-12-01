#include "worldgen.hpp"
#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/token_cache.hpp"
#include <iostream>
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P] [--cert FILE] [--key FILE] [--default-seed N] [--default-width W] [--default-height H]\n";
}

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

int main(int argc, char** argv) {
    int port = 8080;
    std::string cert_file = "certs/server.crt";
    std::string key_file = "certs/server.key";
    unsigned long long default_seed = 12345;
    int default_width = 80;
    int default_height = 24;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "--cert" && i + 1 < argc) {
            cert_file = argv[++i];
        } else if (a == "--key" && i + 1 < argc) {
            key_file = argv[++i];
        } else if (a == "--default-seed" && i + 1 < argc) {
            default_seed = std::stoull(argv[++i]);
        } else if (a == "--default-width" && i + 1 < argc) {
            default_width = std::stoi(argv[++i]);
        } else if (a == "--default-height" && i + 1 < argc) {
            default_height = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    asciimmo::log::Logger logger("world-service");
    asciimmo::auth::TokenCache token_cache;
    
    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

    logger.info("Starting world-service on port " + std::to_string(port));

    svr.get("/world", [default_seed, default_width, default_height](
        const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        unsigned long long seed = default_seed;
        int width = default_width;
        int height = default_height;
        
        std::string target(req.target());
        try {
            auto seed_str = get_param(target, "seed");
            auto width_str = get_param(target, "width");
            auto height_str = get_param(target, "height");
            
            if (!seed_str.empty()) seed = std::stoull(seed_str);
            if (!width_str.empty()) width = std::stoi(width_str);
            if (!height_str.empty()) height = std::stoi(height_str);
        } catch (...) {
            // ignore parse errors and fall back to defaults
        }

        asciimmo::WorldGen gen(seed, width, height);
        std::string map = gen.generate();
        res.result(boost::beast::http::status::ok);
        res.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
        res.body() = map;
        res.prepare_payload();
    });

    svr.get("/health", [](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","service":"world"})";
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
