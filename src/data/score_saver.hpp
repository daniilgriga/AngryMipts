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

class ScoreSaver
{
public:
    std::vector<LevelScore> loadScores( const std::string& filepath ) const;
    void saveScore( const std::string& filepath, int levelId, int score, int stars ) const;
};

}  // namespace angry
