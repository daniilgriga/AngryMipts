#pragma once

#include <string>

namespace angry
{

class SessionManager
{
public:
    explicit SessionManager( std::string filepath = "session.json" );

    void loadSession();
    void saveSession() const;
    void clearSession();

    bool isLoggedIn() const;
    const std::string& token() const;
    const std::string& username() const;

    void setSession( std::string token, std::string username );

private:
    std::string filepath_;
    std::string token_;
    std::string username_;
};

}  // namespace angry
