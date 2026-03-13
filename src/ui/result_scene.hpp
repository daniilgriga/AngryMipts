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
    platform::Font font_;
    platform::Text title_;
    platform::Text score_text_;
    platform::Text status_note_;
    platform::Text prompt_;
    platform::Text lb_title_;
    platform::Text lb_empty_;

    LevelResult     result_;
    platform::Clock star_clock_;
    float           lb_scroll_ = 0.f;   // pixels scrolled in leaderboard panel

    platform::FloatRect rect_btn_retry_;
    platform::FloatRect rect_btn_menu_;

public:
    explicit ResultScene ( const platform::Font& font );

    void set_result ( const LevelResult& result );

    SceneId handle_input ( const platform::Event& event ) override;
    void update() override;
    void render ( platform::Window& window ) override;
};

}  // namespace angry
