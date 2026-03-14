// ============================================================
// score_system.hpp — Score accumulation and star mapping API.
// Part of: angry::core
//
// Declares compact scoring state used by gameplay systems:
//   * Maintains current level score
//   * Supports score reset and positive score addition
//   * Computes stars count from configured thresholds
//   * Exposes read-only score access for snapshot/UI
// ============================================================

#pragma once

namespace angry
{

// Tracks current score and converts it to 0..3 stars based on
// per-level threshold values from level metadata.
class ScoreSystem
{
public:
    void reset();
    void add(int amount);
    int score() const;

    int stars_for(int star_1_threshold, int star_2_threshold, int star_3_threshold) const;

private:
    int score_ = 0;
};

}  // namespace angry
