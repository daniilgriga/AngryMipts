#pragma once
#include "data/account_service.hpp"
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
    bool logged_in = false;
    LeaderboardFetchStatus fetch_status = LeaderboardFetchStatus::Unavailable;
    std::vector<LeaderboardEntry> leaderboard;
};

class ResultScene : public Scene
{
private:
    sf::Font font_;
    sf::Text title_;
    sf::Text score_text_;
    sf::Text status_note_;
    sf::Text prompt_;
    sf::Text lb_title_;
    sf::Text lb_empty_;

    LevelResult result_;
    sf::Clock   star_clock_;
    float       lb_scroll_ = 0.f;   // pixels scrolled in leaderboard panel

public:
    explicit ResultScene ( const sf::Font& font );

    void set_result ( const LevelResult& result );

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;
};

}  // namespace angry
