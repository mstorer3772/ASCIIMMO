#include "db_config.hpp"
#include <cstdlib>
#include <sstream>

namespace asciimmo
{
namespace db
{

std::string Config::connection_string() const
    {
    std::ostringstream oss;
    oss << "host=" << host
        << " port=" << port
        << " dbname=" << dbname
        << " user=" << user;

    if (!password.empty())
        {
        oss << " password=" << password;
        }

    oss << " connect_timeout=" << connect_timeout;

    return oss.str();
    }

Config Config::from_env()
    {
    Config cfg;

    if (const char* env = std::getenv("ASCIIMMO_DB_HOST"))
        {
        cfg.host = env;
        }
    if (const char* env = std::getenv("ASCIIMMO_DB_PORT"))
        {
        cfg.port = env;
        }
    if (const char* env = std::getenv("ASCIIMMO_DB_NAME"))
        {
        cfg.dbname = env;
        }
    if (const char* env = std::getenv("ASCIIMMO_DB_USER"))
        {
        cfg.user = env;
        }
    if (const char* env = std::getenv("ASCIIMMO_DB_PASSWORD"))
        {
        cfg.password = env;
        }
    if (const char* env = std::getenv("ASCIIMMO_DB_POOL_SIZE"))
        {
        cfg.pool_size = std::atoi(env);
        if (cfg.pool_size < 1) cfg.pool_size = 10;
        }

    return cfg;
    }

} // namespace db
} // namespace asciimmo
