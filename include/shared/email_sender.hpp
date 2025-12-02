#pragma once

#include "shared/logger.hpp"
#include <string>
#include <sstream>
#include <cstdlib>

namespace asciimmo {
namespace email {

class EmailSender {
public:
    EmailSender(const std::string& smtp_server, int smtp_port, 
                const std::string& from_email, const std::string& from_name)
        : smtp_server_(smtp_server)
        , smtp_port_(smtp_port)
        , from_email_(from_email)
        , from_name_(from_name)
        , logger_("EmailSender") {}
    
    // Send email confirmation
    bool send_confirmation_email(const std::string& to_email, 
                                  const std::string& username,
                                  uint64_t confirmation_token,
                                  const std::string& base_url) {
        std::string subject = "ASCIIMMO - Confirm Your Email";
        std::string confirmation_link = base_url + "/auth/confirm?token=" + std::to_string(confirmation_token);
        
        std::stringstream body;
        body << "Hello " << username << ",\n\n"
             << "Thank you for registering with ASCIIMMO!\n\n"
             << "Please confirm your email address by clicking the link below:\n"
             << confirmation_link << "\n\n"
             << "This link will expire in 24 hours.\n\n"
             << "If you did not create this account, please ignore this email.\n\n"
             << "Best regards,\n"
             << "The ASCIIMMO Team";
        
        return send_email(to_email, subject, body.str());
    }
    
    // Send generic email
    bool send_email(const std::string& to_email, 
                    const std::string& subject,
                    const std::string& body) {
        logger_.info("Sending email to: " + to_email + " with subject: " + subject);
        
#ifdef NDEBUG
        // Production: Use external mail command or SMTP library
        std::stringstream cmd;
        cmd << "echo '" << escape_shell(body) << "' | mail -s '" 
            << escape_shell(subject) << "' " << escape_shell(to_email);
        
        int result = std::system(cmd.str().c_str());
        if (result == 0) {
            logger_.info("Email sent successfully to: " + to_email);
            return true;
        } else {
            logger_.error("Failed to send email to: " + to_email);
            return false;
        }
#else
        // Debug mode: Just log the email
        logger_.info("DEBUG MODE - Email would be sent:");
        logger_.info("  To: " + to_email);
        logger_.info("  Subject: " + subject);
        logger_.info("  Body: " + body);
        return true;
#endif
    }
    
private:
    std::string smtp_server_;
    int smtp_port_;
    std::string from_email_;
    std::string from_name_;
    log::Logger logger_;
    
    // Simple shell escape (for production, use a proper email library)
    std::string escape_shell(const std::string& input) {
        std::string escaped;
        for (char c : input) {
            if (c == '\'' || c == '"' || c == '\\' || c == '$' || c == '`') {
                escaped += '\\';
            }
            escaped += c;
        }
        return escaped;
    }
};

} // namespace email
} // namespace asciimmo
