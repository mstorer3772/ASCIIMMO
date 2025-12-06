#include "shared/http_server.hpp"
#include <iostream>

namespace asciimmo
{
namespace http
{

namespace beast = boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

// Plain HTTP Session
class Server::Session : public std::enable_shared_from_this<Server::Session>
    {
    public:
        Session(tcp::socket socket, Server* server)
            : socket_(std::move(socket))
            , server_(server)
            {}

        void run()
            {
            do_read();
            }

    private:
        void do_read()
            {
            auto self = shared_from_this();
            beast::http::async_read(socket_, buffer_, req_,
                [self](beast::error_code ec, std::size_t)
                {
                if (!ec)
                    {
                    self->handle_request();
                    }
                });
            }

        void handle_request()
            {
            Response res{ beast::http::status::not_found, req_.version() };
            res.set(beast::http::field::server, "ASCIIMMO");
            res.set(beast::http::field::content_type, "application/json");
            res.keep_alive(req_.keep_alive());

            server_->handle_request(req_, res);

            do_write();
            }

        void do_write()
            {
            auto self = shared_from_this();
            beast::http::async_write(socket_, res_,
                [self](beast::error_code ec, std::size_t)
                {
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                });
            }

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        Request req_;
        Response res_;
        Server* server_;
    };

// HTTPS Session
class Server::SSLSession : public std::enable_shared_from_this<Server::SSLSession>
    {
    public:
        SSLSession(tcp::socket socket, ssl::context& ctx, Server* server)
            : stream_(std::move(socket), ctx)
            , server_(server)
            {}

        void run()
            {
            auto self = shared_from_this();
            stream_.async_handshake(ssl::stream_base::server,
                [self](beast::error_code ec)
                {
                if (!ec)
                    {
                    self->do_read();
                    }
                });
            }

    private:
        void do_read()
            {
            auto self = shared_from_this();
            beast::http::async_read(stream_, buffer_, req_,
                [self](beast::error_code ec, std::size_t)
                {
                if (!ec)
                    {
                    self->handle_request();
                    }
                });
            }

        void handle_request()
            {
            Response res{ beast::http::status::not_found, req_.version() };
            res.set(beast::http::field::server, "ASCIIMMO");
            res.set(beast::http::field::content_type, "application/json");
            res.keep_alive(req_.keep_alive());

            server_->handle_request(req_, res);

            do_write();
            }

        void do_write()
            {
            auto self = shared_from_this();
            beast::http::async_write(stream_, res_,
                [self](beast::error_code ec, std::size_t)
                {
                if (!ec)
                    {
                    self->stream_.async_shutdown(
                        [self](beast::error_code)
                        {
                        // Shutdown complete
                        });
                    }
                });
            }

        beast::ssl_stream<tcp::socket> stream_;
        beast::flat_buffer buffer_;
        Request req_;
        Response res_;
        Server* server_;
    };

Server::Server(net::io_context& ioc, unsigned short port)
    : ioc_(ioc)
    , acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    , running_(false)
    , use_ssl_(false)
    {}

Server::Server(net::io_context& ioc, unsigned short port,
    const std::string& cert_file, const std::string& key_file)
    : ioc_(ioc)
    , acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    , running_(false)
    , use_ssl_(true)
    , ssl_ctx_(std::make_unique<ssl::context>(ssl::context::tls_server))
    {

    ssl_ctx_->set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::no_tlsv1 |
        ssl::context::no_tlsv1_1 |
        ssl::context::single_dh_use);

    ssl_ctx_->use_certificate_chain_file(cert_file);
    ssl_ctx_->use_private_key_file(key_file, ssl::context::pem);
    }

void Server::get(const std::string& pattern, Handler handler)
    {
    routes_.push_back({ beast::http::verb::get, std::regex(pattern), handler });
    }

void Server::post(const std::string& pattern, Handler handler)
    {
    routes_.push_back({ beast::http::verb::post, std::regex(pattern), handler });
    }

void Server::put(const std::string& pattern, Handler handler)
    {
    routes_.push_back({ beast::http::verb::put, std::regex(pattern), handler });
    }

void Server::del(const std::string& pattern, Handler handler)
    {
    routes_.push_back({ beast::http::verb::delete_, std::regex(pattern), handler });
    }

void Server::run()
    {
    running_ = true;
    do_accept();
    }

void Server::stop()
    {
    running_ = false;
    acceptor_.close();
    }

void Server::do_accept()
    {
    if (!running_) return;

    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket)
        {
        if (!ec)
            {
            if (use_ssl_)
                {
                std::make_shared<SSLSession>(std::move(socket), *ssl_ctx_, this)->run();
                }
            else
                {
                std::make_shared<Session>(std::move(socket), this)->run();
                }
            }
        do_accept();
        });
    }

void Server::handle_request(const Request& req, Response& res)
    {
    std::string target(req.target());

    // Add CORS headers to all responses
    res.set(beast::http::field::access_control_allow_origin, "*");
    // No delete.  Just get, post, put, options
    res.set(beast::http::field::access_control_allow_methods, "GET, POST, PUT, OPTIONS");
    res.set(beast::http::field::access_control_allow_headers, "Content-Type, Authorization");
    res.set(beast::http::field::access_control_max_age, "86400"); // 24 * 60 * 60 seconds

    // Handle OPTIONS preflight requests
    if (req.method() == beast::http::verb::options)
        {
        std::cout << "Handled OPTIONS request for " << target << std::endl;
        res.result(beast::http::status::no_content);
        res.prepare_payload();
        return;
        }

    for (const auto& route : routes_)
        {
        if (route.method == req.method())
            {
            std::smatch matches;
            if (std::regex_match(target, matches, route.pattern))
                {
                route.handler(req, res, matches);
                return;
                }
            }
        }

    // Not found
    res.result(beast::http::status::not_found);
    res.body() = R"({"error":"not found"})";
    res.prepare_payload();
    }

} // namespace http
} // namespace asciimmo
