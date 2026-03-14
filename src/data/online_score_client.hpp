// ============================================================
// online_score_client.hpp — Online score/leaderboard client API.
// Part of: angry::data
//
// Declares backend integration surface for leaderboard flows:
//   * Score submission (legacy + JWT token variants)
//   * Leaderboard fetch with explicit status reporting
//   * Simplified entries-only fetch helper
//   * Base URL configuration for environments
// ============================================================

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace angry
{

struct LeaderboardEntry
{
    std::string playerName;
    int score = 0;
    int stars = 0;
};

enum class LeaderboardFetchStatus
{
    Ok,
    Empty,
    Unavailable,
    InvalidResponse,
};

struct LeaderboardFetchResult
{
    LeaderboardFetchStatus status = LeaderboardFetchStatus::Unavailable;
    std::vector<LeaderboardEntry> entries;
};

// Performs score and leaderboard HTTP requests and exposes
// compact result models for account/UI services.
class OnlineScoreClient
{
public:
    explicit OnlineScoreClient(std::string base_url = "");

    // Legacy API kept for transitional compatibility.
    // For JWT backend use submit_score_with_token(...).
    bool submit_score(const std::string& player_name, int level_id, int score, int stars) const;
    bool submit_score_with_token(
        const std::string& token, int level_id, int score, int stars ) const;
    LeaderboardFetchResult fetch_leaderboard_with_status(int level_id) const;
    std::vector<LeaderboardEntry> fetch_leaderboard(int level_id) const;
    void submit_score_with_token_async(
        const std::string& token,
        int level_id,
        int score,
        int stars,
        std::function<void(bool)> on_done ) const;
    void fetch_leaderboard_with_status_async(
        int level_id,
        std::function<void(LeaderboardFetchResult)> on_done ) const;

private:
    std::string base_url_;
};

}  // namespace angry
