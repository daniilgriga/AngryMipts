#include "score_system.hpp"

namespace angry
{

void ScoreSystem::reset()
{
    score_ = 0;
}

void ScoreSystem::add(int amount)
{
    if (amount > 0)
    {
        score_ += amount;
    }
}

int ScoreSystem::score() const
{
    return score_;
}

int ScoreSystem::starsFor(int star1Threshold, int star2Threshold, int star3Threshold) const
{
    if (score_ >= star3Threshold)
    {
        return 3;
    }
    if (score_ >= star2Threshold)
    {
        return 2;
    }
    if (score_ >= star1Threshold)
    {
        return 1;
    }
    return 0;
}

}  // namespace angry
