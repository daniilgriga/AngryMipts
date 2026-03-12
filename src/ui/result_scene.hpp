#pragma once
#include "data/OnlineScoreClient.hpp"
#include "scene.hpp"

#include <vector>

namespace angry
{

struct LevelResult
{
    bool win = false;
    int score = 0;
    int stars = 0;
    std::vector<LeaderboardEntry> leaderboard;
};

class ResultScene : public Scene
{
private:
    sf::Font font_;
    sf::Text title_;
    sf::Text score_text_;
    sf::Text status_note_;
    sf::Text leaderboard_title_;
    sf::Text leaderboard_empty_;
    sf::Text prompt_;

    LevelResult result_;
    sf::Clock   star_clock_;  // reset on set_result, drives bounce-in animation

public:
    explicit ResultScene ( const sf::Font& font );

    void set_result ( const LevelResult& result );

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;
};

}  // namespace angry
