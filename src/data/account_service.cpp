// ============================================================
// account_service.cpp — Account facade implementation.
// Part of: angry::data
//
// Implements account-related integration workflows:
//   * Connects auth API with local session persistence
//   * Enforces login requirements for score submission
//   * Provides leaderboard retrieval passthrough methods
//   * Keeps UI-facing API compact and backend-agnostic
// ============================================================

#include "data/account_service.hpp"

#include "data/logger.hpp"

namespace angry
{

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

AccountService::AccountService( std::string sessionFilepath, std::string baseUrl )
    : sessionManager_( std::move( sessionFilepath ) ),
      authClient_( baseUrl ),
      onlineScoreClient_( std::move( baseUrl ) )
{
}

// #=# Session / Auth API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void AccountService::load_session()
{
    sessionManager_.load_session();
}

bool AccountService::is_logged_in() const
{
    return sessionManager_.is_logged_in();
}

const std::string& AccountService::username() const
{
    return sessionManager_.username();
}

const std::string& AccountService::token() const
{
    return sessionManager_.token();
}

AuthResult AccountService::register_and_login(
    const std::string& username,
    const std::string& password )
{
    const AuthResult registerResult = authClient_.register_user( username, password );
    if ( !registerResult.success )
    {
        return registerResult;
    }

    return login( username, password );
}

AuthResult AccountService::login(
    const std::string& username,
    const std::string& password )
{
    AuthResult result = authClient_.login_user( username, password );
    if ( !result.success )
    {
        // Contract: failed login must not save token.
        return result;
    }

    sessionManager_.set_session( result.token, result.username );
    sessionManager_.save_session();
    return result;
}

void AccountService::logout()
{
    sessionManager_.clear_session();
}

// #=# Leaderboard API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

bool AccountService::submit_score_if_logged_in( int levelId, int score, int stars )
{
    if ( !sessionManager_.is_logged_in() )
    {
        Logger::info( "User is not logged in, skipping online score submission" );
        return false;
    }

    return onlineScoreClient_.submit_score_with_token(
        sessionManager_.token(), levelId, score, stars );
}

LeaderboardFetchResult AccountService::fetch_leaderboard_with_status( int levelId )
{
    return onlineScoreClient_.fetch_leaderboard_with_status( levelId );
}

std::vector<LeaderboardEntry> AccountService::fetch_leaderboard( int levelId )
{
    return onlineScoreClient_.fetch_leaderboard( levelId );
}

}  // namespace angry
