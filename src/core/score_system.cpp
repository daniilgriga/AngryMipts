// ============================================================
// score_system.cpp — Score and stars logic implementation.
// Part of: angry::core
//
// Implements score progression rules:
//   * Resets score between level runs
//   * Accepts only positive score increments
//   * Returns current score snapshot value
//   * Maps score to stars by threshold comparison
// ============================================================

#include "score_system.hpp"

namespace angry
{

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

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
