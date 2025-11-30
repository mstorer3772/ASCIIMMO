#pragma once
#include <string>
#include <memory>

namespace asciimmo {
namespace db {

struct Config {
    std::string host = "localhost";
    std::string port = "5432";
    std::string dbname = "asciimmo";
    std::string user = "asciimmo_user";
    std::string password = "";
    int pool_size = 10;
    int connect_timeout = 5;
    
    // Build PostgreSQL connection string
    std::string connection_string() const;
    
    // Load from environment variables
    static Config from_env();
};

} // namespace db
} // namespace asciimmo
