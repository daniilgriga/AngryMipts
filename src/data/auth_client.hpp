// ============================================================
// auth_client.hpp — Backend authentication client interface.
// Part of: angry::data
//
// Declares HTTP auth operations used by account workflows:
//   * Register request and login request entry points
//   * Unified AuthResult payload for success/error handling
//   * Base URL configuration with runtime override support
//   * Minimal dependency surface for UI-facing services
// ============================================================

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

// Performs register/login HTTP calls against backend auth API
// and returns normalized AuthResult values.
class AuthClient
{
public:
    explicit AuthClient( std::string baseUrl = "" );

    AuthResult register_user( const std::string& username, const std::string& password ) const;
    AuthResult login_user( const std::string& username, const std::string& password ) const;

private:
    std::string baseUrl_;
};

}  // namespace angry
