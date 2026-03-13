#include "data/account_service.hpp"

#include "data/logger.hpp"

namespace angry
{

AccountService::AccountService( std::string sessionFilepath, std::string baseUrl )
    : sessionManager_( std::move( sessionFilepath ) ),
      authClient_( baseUrl ),
      onlineScoreClient_( std::move( baseUrl ) )
{
}

void AccountService::loadSession()
{
    sessionManager_.loadSession();
}

bool AccountService::isLoggedIn() const
{
    return sessionManager_.isLoggedIn();
}

const std::string& AccountService::username() const
{
    return sessionManager_.username();
}

const std::string& AccountService::token() const
{
    return sessionManager_.token();
}

AuthResult AccountService::registerAndLogin(
    const std::string& username,
    const std::string& password )
{
    const AuthResult registerResult = authClient_.registerUser( username, password );
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
    AuthResult result = authClient_.loginUser( username, password );
    if ( !result.success )
    {
        // Contract: failed login must not save token.
        return result;
    }

    sessionManager_.setSession( result.token, result.username );
    sessionManager_.saveSession();
    return result;
}

void AccountService::logout()
{
    sessionManager_.clearSession();
}

bool AccountService::submitScoreIfLoggedIn( int levelId, int score, int stars )
{
    if ( !sessionManager_.isLoggedIn() )
    {
        Logger::info( "User is not logged in, skipping online score submission" );
        return false;
    }

    return onlineScoreClient_.submitScoreWithToken(
        sessionManager_.token(), levelId, score, stars );
}

LeaderboardFetchResult AccountService::fetchLeaderboardWithStatus( int levelId )
{
    return onlineScoreClient_.fetchLeaderboardWithStatus( levelId );
}

std::vector<LeaderboardEntry> AccountService::fetchLeaderboard( int levelId )
{
    return onlineScoreClient_.fetchLeaderboard( levelId );
}

}  // namespace angry
