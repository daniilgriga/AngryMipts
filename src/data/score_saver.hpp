// ============================================================
// score_saver.hpp — Best-score persistence interface.
// Part of: angry::data
//
// Declares score storage API for local progression:
//   * Loads best score/star per level from JSON file
//   * Saves score updates with best-value semantics
//   * Keeps model small via LevelScore records
//   * Validation and file IO are handled in implementation
// ============================================================

#pragma once
#include <string>
#include <vector>

namespace angry
{

struct LevelScore
{
    int levelId;
    int bestScore;
    int bestStars;  // 0..3
};

// Persists best results per level in local JSON storage and
// merges new submissions using "best score / best stars" policy.
class ScoreSaver
{
public:
    std::vector<LevelScore> loadScores( const std::string& filepath ) const;
    void saveScore( const std::string& filepath, int levelId, int score, int stars ) const;
};

}  // namespace angry
