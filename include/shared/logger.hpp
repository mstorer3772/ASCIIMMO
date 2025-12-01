#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <unistd.h>

namespace asciimmo {
namespace log {

enum class Level {
    FATAL = 0,
    ERROR = 1,
    WARNING = 2,
    INFO = 3,
    DEBUG = 4,
    VERBOSE = 5
};

inline const char* level_to_string(Level level) {
    switch (level) {
        case Level::FATAL:   return "FATAL";
        case Level::ERROR:   return "ERROR";
        case Level::WARNING: return "WARNING";
        case Level::INFO:    return "INFO";
        case Level::DEBUG:   return "DEBUG";
        case Level::VERBOSE: return "VERBOSE";
        default:             return "UNKNOWN";
    }
}

class Logger {
public:
    Logger(const std::string& service_name, Level max_level = Level::INFO)
        : service_name_(service_name)
        , max_level_(max_level)
        , pid_(getpid()) {}
    
    void set_level(Level level) {
        max_level_ = level;
    }
    
    Level get_level() const {
        return max_level_;
    }
    
    void log(Level level, const std::string& message) {
        if (level > max_level_) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << service_name_ << "]"
            << " [" << pid_ << "]"
            << " [" <<level_to_string(level) << "]"
            << " " << message;

        std::cout << oss.str() << std::endl;
    }
    
    void fatal(const std::string& message)   { log(Level::FATAL, message); }
    void error(const std::string& message)   { log(Level::ERROR, message); }
    void warning(const std::string& message) { log(Level::WARNING, message); }
    void info(const std::string& message)    { log(Level::INFO, message); }
    void debug(const std::string& message)   { log(Level::DEBUG, message); }
    void verbose(const std::string& message) { log(Level::VERBOSE, message); }
    
private:
    std::string service_name_;
    Level max_level_;
    pid_t pid_;
};

} // namespace log
} // namespace asciimmo
