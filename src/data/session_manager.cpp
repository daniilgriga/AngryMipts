// ============================================================
// session_manager.cpp — Persistent auth session implementation.
// Part of: angry::data
//
// Implements session file IO and validation logic:
//   * Reads/writes session JSON with token/username fields
//   * Validates schema and gracefully handles corrupt data
//   * Clears persisted state on explicit logout
//   * Emits diagnostic logs for load/save/clear operations
// ============================================================

#include "data/session_manager.hpp"

#include "data/logger.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace angry
{

// #=# Local Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

using Json = nlohmann::json;

}  // namespace

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

SessionManager::SessionManager( std::string filepath )
    : filepath_( std::move( filepath ) )
{
}

// #=# Session Persistence API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void SessionManager::load_session()
{
    token_.clear();
    username_.clear();

    if ( filepath_.empty() )
    {
        Logger::info( "Session load skipped: empty filepath" );
        return;
    }

    const std::filesystem::path path( filepath_ );
    if ( !std::filesystem::exists( path ) )
    {
        Logger::info( "Session loaded: empty (file not found: {})", filepath_ );
        return;
    }

    try
    {
        std::ifstream input( path );
        if ( !input.is_open() )
        {
            Logger::error( "Session load failed: cannot open {}", filepath_ );
            return;
        }

        Json root;
        input >> root;
        if ( !root.is_object() )
        {
            Logger::error( "Session load failed: invalid JSON root in {}", filepath_ );
            return;
        }

        if ( !root.contains( "token" ) || !root.contains( "username" )
             || !root["token"].is_string() || !root["username"].is_string() )
        {
            Logger::error( "Session load failed: missing token/username in {}", filepath_ );
            return;
        }

        token_ = root["token"].get<std::string>();
        username_ = root["username"].get<std::string>();

        if ( token_.empty() || username_.empty() )
        {
            token_.clear();
            username_.clear();
            Logger::info( "Session loaded: empty" );
            return;
        }

        Logger::info( "Session loaded: user={}", username_ );
    }
    catch ( const std::exception& e )
    {
        token_.clear();
        username_.clear();
        Logger::error( "Session load failed for {}: {}", filepath_, e.what() );
    }
}

void SessionManager::save_session() const
{
    if ( filepath_.empty() )
    {
        Logger::info( "Session save skipped: empty filepath" );
        return;
    }

    try
    {
        const std::filesystem::path path( filepath_ );
        const std::filesystem::path parent = path.parent_path();
        if ( !parent.empty() )
        {
            std::filesystem::create_directories( parent );
        }

        std::ofstream output( path );
        if ( !output.is_open() )
        {
            Logger::error( "Session save failed: cannot open {}", filepath_ );
            return;
        }

        const Json root = {
            {"token", token_},
            {"username", username_},
        };

        output << root.dump( 2 ) << '\n';
        if ( !output.good() )
        {
            Logger::error( "Session save failed: write error {}", filepath_ );
            return;
        }

        Logger::info( "Session saved: user={}", username_ );
    }
    catch ( const std::exception& e )
    {
        Logger::error( "Session save failed for {}: {}", filepath_, e.what() );
    }
}

void SessionManager::clear_session()
{
    token_.clear();
    username_.clear();

    if ( filepath_.empty() )
    {
        Logger::info( "Session clear skipped: empty filepath" );
        return;
    }

    try
    {
        const std::filesystem::path path( filepath_ );
        if ( std::filesystem::exists( path ) )
        {
            std::filesystem::remove( path );
        }
        Logger::info( "Session cleared" );
    }
    catch ( const std::exception& e )
    {
        Logger::error( "Session clear failed for {}: {}", filepath_, e.what() );
    }
}

// #=# Accessors / Mutators #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

bool SessionManager::is_logged_in() const
{
    return !token_.empty() && !username_.empty();
}

const std::string& SessionManager::token() const
{
    return token_;
}

const std::string& SessionManager::username() const
{
    return username_;
}

void SessionManager::set_session( std::string token, std::string username )
{
    token_ = std::move( token );
    username_ = std::move( username );
}

}  // namespace angry
