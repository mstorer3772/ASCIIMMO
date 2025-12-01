#pragma once

#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

namespace asciimmo {
namespace http {

class Client {
public:
    // POST request to HTTPS endpoint
    static bool post(const std::string& host, int port, const std::string& target, const std::string& body) {
        try {
            boost::asio::io_context ioc;
            boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12_client);
            
            // Allow self-signed certificates for development
            ssl_ctx.set_verify_mode(boost::asio::ssl::verify_none);
            
            boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_ctx);
            
            // Resolve and connect
            boost::asio::ip::tcp::resolver resolver(ioc);
            auto const results = resolver.resolve(host, std::to_string(port));
            boost::beast::get_lowest_layer(stream).connect(results);
            
            // Perform SSL handshake
            stream.handshake(boost::asio::ssl::stream_base::client);
            
            // Build HTTP POST request
            boost::beast::http::request<boost::beast::http::string_body> req{
                boost::beast::http::verb::post, target, 11
            };
            req.set(boost::beast::http::field::host, host);
            req.set(boost::beast::http::field::content_type, "application/json");
            req.body() = body;
            req.prepare_payload();
            
            // Send request
            boost::beast::http::write(stream, req);
            
            // Read response
            boost::beast::flat_buffer buffer;
            boost::beast::http::response<boost::beast::http::string_body> res;
            boost::beast::http::read(stream, buffer, res);
            
            // Graceful shutdown
            boost::beast::error_code ec;
            stream.shutdown(ec);
            // Ignore shutdown errors (common with self-signed certs)
            
            return res.result() == boost::beast::http::status::ok;
        } catch (const std::exception&) {
            return false;
        }
    }
};

} // namespace http
} // namespace asciimmo
