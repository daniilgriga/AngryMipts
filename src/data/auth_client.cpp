// ============================================================
// auth_client.cpp — Backend authentication client implementation.
// Part of: angry::data
//
// Implements register/login request flow:
//   * Resolves backend URL from explicit arg/env/default
//   * Builds JSON request payloads and parses responses
//   * Normalizes transport/server errors into AuthResult
//   * Emits concise diagnostics for auth operations
// ============================================================

#include "data/auth_client.hpp"

#include "data/logger.hpp"
#include "platform/http.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <utility>

namespace angry
{

// #=# Local Helpers & Constants #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

using Json = nlohmann::json;

constexpr int kAuthTimeoutMs = 3000;
constexpr const char* kDefaultBackendUrl = "http://84.201.138.107:8080";
constexpr const char* kBackendUrlEnvVar = "ANGRY_BACKEND_URL";

bool starts_with( const std::string& value, const char* prefix )
{
    const std::size_t prefix_len = std::char_traits<char>::length( prefix );
    return value.size() >= prefix_len && value.compare( 0, prefix_len, prefix ) == 0;
}

bool is_local_http_url( const std::string& url )
{
    return starts_with( url, "http://127.0.0.1" )
        || starts_with( url, "http://localhost" )
        || starts_with( url, "http://[::1]" );
}

bool is_insecure_non_local_url( const std::string& url )
{
    return starts_with( url, "http://" ) && !is_local_http_url( url );
}

std::string resolve_backend_url( std::string baseUrl )
{
    if ( !baseUrl.empty() )
    {
        return baseUrl;
    }

    const char* envUrl = std::getenv( kBackendUrlEnvVar );
    if ( envUrl != nullptr && envUrl[0] != '\0' )
    {
        return std::string( envUrl );
    }

    return std::string( kDefaultBackendUrl );
}

std::string extract_error_message( const platform::http::Response& response, const char* fallback )
{
    if ( response.network_error )
    {
        if ( !response.error_message.empty() )
        {
            return response.error_message;
        }
        return "server unavailable";
    }

    if ( response.body.empty() )
    {
        return std::string( fallback );
    }

    const Json root = Json::parse( response.body, nullptr, false );
    if ( root.is_object() )
    {
        if ( root.contains( "error" ) && root["error"].is_string() )
        {
            return root["error"].get<std::string>();
        }

        if ( root.contains( "message" ) && root["message"].is_string() )
        {
            return root["message"].get<std::string>();
        }
    }

    return std::string( fallback );
}

AuthResult post_auth_request(
    const std::string& endpoint,
    const std::string& baseUrl,
    const std::string& username,
    const std::string& password )
{
    if ( is_insecure_non_local_url( baseUrl ) )
    {
        Logger::info( "AuthClient: connecting to insecure (non-HTTPS) backend URL '{}'", baseUrl );
    }

    AuthResult result;
    const Json body = {
        {"username", username},
        {"password", password},
    };

    const platform::http::Response response = platform::http::post(
        baseUrl + endpoint,
        body.dump(),
        platform::http::Headers {
            {"Content-Type", "application/json"},
        },
        kAuthTimeoutMs );

    if ( response.network_error )
    {
        result.errorMessage = extract_error_message( response, "server unavailable" );
        return result;
    }

    if ( response.status_code < 200 || response.status_code >= 300 )
    {
        result.errorMessage = extract_error_message( response, "request failed" );
        return result;
    }

    result.success = true;
    return result;
}

}  // namespace

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

AuthClient::AuthClient( std::string baseUrl )
    : baseUrl_( resolve_backend_url( std::move( baseUrl ) ) )
{
}

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

AuthResult AuthClient::register_user(
    const std::string& username,
    const std::string& password ) const
{
    Logger::info( "Register request started" );

    AuthResult result = post_auth_request( "/register", baseUrl_, username, password );
    if ( !result.success )
    {
        Logger::error( "Register failed: {}", result.errorMessage );
        return result;
    }

    result.username = username;
    Logger::info( "Register successful" );
    return result;
}

AuthResult AuthClient::login_user(
    const std::string& username,
    const std::string& password ) const
{
    if ( is_insecure_non_local_url( baseUrl_ ) )
    {
        Logger::info( "AuthClient: connecting to insecure (non-HTTPS) backend URL '{}'", baseUrl_ );
    }

    Logger::info( "Login request started" );

    const Json body = {
        {"username", username},
        {"password", password},
    };

    const platform::http::Response response = platform::http::post(
        baseUrl_ + "/login",
        body.dump(),
        platform::http::Headers {
            {"Content-Type", "application/json"},
        },
        kAuthTimeoutMs );

    AuthResult result;

    if ( response.network_error )
    {
        result.errorMessage = extract_error_message( response, "server unavailable" );
        Logger::error( "Login failed: {}", result.errorMessage );
        return result;
    }

    if ( response.status_code < 200 || response.status_code >= 300 )
    {
        result.errorMessage = extract_error_message( response, "invalid username or password" );
        Logger::error( "Login failed: {}", result.errorMessage );
        return result;
    }

    const Json root = Json::parse( response.body, nullptr, false );
    if ( !root.is_object() )
    {
        result.errorMessage = "invalid login response";
        Logger::error( "Login failed: {}", result.errorMessage );
        return result;
    }

    if ( !root.contains( "token" ) || !root["token"].is_string()
         || !root.contains( "username" ) || !root["username"].is_string() )
    {
        result.errorMessage = "invalid login response";
        Logger::error( "Login failed: {}", result.errorMessage );
        return result;
    }

    result.success = true;
    result.token = root["token"].get<std::string>();
    result.username = root["username"].get<std::string>();
    if ( result.token.empty() || result.username.empty() )
    {
        result.success = false;
        result.token.clear();
        result.username.clear();
        result.errorMessage = "invalid login response";
        Logger::error( "Login failed: {}", result.errorMessage );
        return result;
    }

    Logger::info( "Login successful" );
    return result;
}

}  // namespace angry
