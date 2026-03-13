#pragma once

#include "data/auth_client.hpp"
#include "data/session_manager.hpp"
#include "data/OnlineScoreClient.hpp"

#include <string>
#include <vector>

namespace angry
{

class AccountService
{
public:
    explicit AccountService(
        std::string sessionFilepath = "session.json",
        std::string baseUrl = "" );

    void loadSession();
    bool isLoggedIn() const;
    const std::string& username() const;
    const std::string& token() const;

    AuthResult registerAndLogin(
        const std::string& username,
        const std::string& password );
    AuthResult login(
        const std::string& username,
        const std::string& password );

    void logout();

    bool submitScoreIfLoggedIn( int levelId, int score, int stars );
    LeaderboardFetchResult fetchLeaderboardWithStatus( int levelId );
    std::vector<LeaderboardEntry> fetchLeaderboard( int levelId );

private:
    SessionManager sessionManager_;
    AuthClient authClient_;
    OnlineScoreClient onlineScoreClient_;
};

}  // namespace angry
