#pragma once

#include <string>

namespace angry
{

struct AuthResult
{
    bool success = false;
    std::string token;
    std::string username;
    std::string errorMessage;
};

class AuthClient
{
public:
    explicit AuthClient( std::string baseUrl = "" );

    AuthResult registerUser( const std::string& username, const std::string& password ) const;
    AuthResult loginUser( const std::string& username, const std::string& password ) const;

private:
    std::string baseUrl_;
};

}  // namespace angry
