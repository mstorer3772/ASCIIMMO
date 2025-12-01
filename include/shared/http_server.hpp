#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <regex>
#include <vector>

namespace asciimmo {
namespace http {

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// HTTP request and response types
using Request = beast::http::request<beast::http::string_body>;
using Response = beast::http::response<beast::http::string_body>;

// Route handler function type
using Handler = std::function<void(const Request&, Response&, const std::smatch&)>;

// Route pattern with regex and handler
struct Route {
    beast::http::verb method;
    std::regex pattern;
    Handler handler;
};

class Server {
public:
    Server(net::io_context& ioc, unsigned short port);
    Server(net::io_context& ioc, unsigned short port, 
           const std::string& cert_file, const std::string& key_file);
    
    // Register route handlers
    void get(const std::string& pattern, Handler handler);
    void post(const std::string& pattern, Handler handler);
    void put(const std::string& pattern, Handler handler);
    void del(const std::string& pattern, Handler handler);
    
    // Start accepting connections
    void run();
    
    // Stop the server
    void stop();
    
private:
    class Session;
    class SSLSession;
    
    void do_accept();
    void handle_request(const Request& req, Response& res);
    
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::vector<Route> routes_;
    bool running_;
    bool use_ssl_;
    std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
};

} // namespace http
} // namespace asciimmo
