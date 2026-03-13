#pragma once

namespace angry
{

class ScoreSystem
{
public:
    void reset();
    void add(int amount);
    int score() const;

    int starsFor(int star1Threshold, int star2Threshold, int star3Threshold) const;

private:
    int score_ = 0;
};

}  // namespace angry
