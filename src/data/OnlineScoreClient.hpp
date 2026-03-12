#pragma once

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

class OnlineScoreClient
{
public:
    explicit OnlineScoreClient(std::string baseUrl = "");

    // Legacy API kept for transitional compatibility.
    // For JWT backend use submitScoreWithToken(...).
    bool submitScore(const std::string& playerName, int levelId, int score, int stars);
    bool submitScoreWithToken(const std::string& token, int levelId, int score, int stars);
    LeaderboardFetchResult fetchLeaderboardWithStatus(int levelId);
    std::vector<LeaderboardEntry> fetchLeaderboard(int levelId);

private:
    std::string baseUrl_;
};

}  // namespace angry
