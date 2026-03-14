// ============================================================
// level_select_scene.hpp — Level selection scene interface.
// Part of: angry::ui
//
// Declares UI scene for choosing and previewing levels:
//   * Loads level metadata and local progress summaries
//   * Shows leaderboard previews from account service
//   * Handles keyboard/mouse navigation and scrolling
//   * Exposes selected level id for game scene launch
// ============================================================

#pragma once
#include "data/account_service.hpp"
#include "data/level_loader.hpp"
#include "data/online_score_client.hpp"
#include "data/score_saver.hpp"
#include "scene.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace angry
{

// Renders and controls interactive level list with progress and
// leaderboard preview panels.
class LevelSelectScene : public Scene
{
private:
    AccountService*    accounts_ = nullptr;
    platform::Font     font_;
    platform::Text     title_;
    platform::Text     prompt_;
    platform::Text     badge_text_;
    platform::Text     badge_btn_;

    std::vector<LevelMeta>         levels_;
    std::vector<LevelScore>        scores_;
    std::vector<platform::Text>    level_texts_;
    std::string                    scores_path_;

    int   selected_          = 0;
    int   selected_level_id_ = -1;
    float scroll_offset_     = 0.f;

    // Hit-test rects updated each render() call
    platform::FloatRect              rect_badge_;
    platform::FloatRect              rect_right_panel_;
    platform::FloatRect              rect_left_panel_;
    std::vector<platform::FloatRect> rects_level_items_screen_; // actual screen Y per item

    // Preview panel leaderboard — async fetch per selection
    struct PreviewState
    {
        std::mutex mutex;
        int        fetched_level_id = -1;
        LeaderboardFetchStatus fetch_status = LeaderboardFetchStatus::Unavailable;
        std::vector<LeaderboardEntry> entries;
    };
    std::shared_ptr<PreviewState> preview_ = std::make_shared<PreviewState>();
    int  preview_requested_id_ = -1;   // level id currently being fetched
    float preview_scroll_      = 0.f;

    void rebuild_texts();
    const LevelScore* find_score ( int level_id ) const;
    void fetch_preview ( int level_id );

public:
    explicit LevelSelectScene ( const platform::Font& font, AccountService* accounts = nullptr );

    void load_data ( const std::string& levels_dir, const std::string& scores_path );
    void reload_scores();

    int get_selected_level_id() const { return selected_level_id_; }
    const std::string& get_scores_path() const { return scores_path_; }

    SceneId handle_input ( const platform::Event& event ) override;
    void update() override;
    void render ( platform::Window& window ) override;
};

}  // namespace angry
