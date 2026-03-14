// ============================================================
// account_service.hpp — Auth/session/leaderboard facade API.
// Part of: angry::data
//
// Declares high-level account workflows for game scenes:
//   * Register+login and plain login orchestration
//   * Session bootstrap, token access, and logout
//   * Auth-gated score submission to online backend
//   * Leaderboard retrieval with status/result wrappers
// ============================================================

#pragma once

#include "data/auth_client.hpp"
#include "data/session_manager.hpp"
#include "data/OnlineScoreClient.hpp"

#include <string>
#include <vector>

namespace angry
{

// Composes auth client, session persistence, and online score
// client into one scene-friendly account service.
class AccountService
{
public:
    explicit AccountService(
        std::string sessionFilepath = "session.json",
        std::string baseUrl = "" );

    void load_session();
    bool is_logged_in() const;
    const std::string& username() const;
    const std::string& token() const;

    AuthResult register_and_login(
        const std::string& username,
        const std::string& password );
    AuthResult login(
        const std::string& username,
        const std::string& password );

    void logout();

    bool submit_score_if_logged_in( int levelId, int score, int stars );
    LeaderboardFetchResult fetch_leaderboard_with_status( int levelId );
    std::vector<LeaderboardEntry> fetch_leaderboard( int levelId );

private:
    SessionManager sessionManager_;
    AuthClient authClient_;
    OnlineScoreClient onlineScoreClient_;
};

}  // namespace angry
