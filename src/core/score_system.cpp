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

int ScoreSystem::stars_for(int star_1_threshold, int star_2_threshold, int star_3_threshold) const
{
    if (score_ >= star_3_threshold)
    {
        return 3;
    }
    if (score_ >= star_2_threshold)
    {
        return 2;
    }
    if (score_ >= star_1_threshold)
    {
        return 1;
    }
    return 0;
}

}  // namespace angry
