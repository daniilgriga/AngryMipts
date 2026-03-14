// ============================================================
// menu_scene.hpp — Main menu scene interface.
// Part of: angry::ui
//
// Declares entry menu scene used before level selection:
//   * Renders title and primary navigation prompt
//   * Shows account badge/login status shortcut
//   * Handles click/keyboard transition events
//   * Keeps simple static UI state and hit regions
// ============================================================

#pragma once
#include "data/account_service.hpp"
#include "scene.hpp"

namespace angry
{

// Displays top-level menu actions and forwards navigation
// choices to SceneManager.
class MenuScene : public Scene
{
private:
    AccountService&    accounts_;
    platform::Font     font_;
    platform::Text     title_;
    platform::Text     prompt_;
    platform::Text     badge_text_;
    platform::Text     badge_btn_;

    platform::FloatRect rect_prompt_;
    platform::FloatRect rect_badge_btn_;

public:
    MenuScene ( const platform::Font& font, AccountService& accounts );

    SceneId handle_input ( const platform::Event& event ) override;
    void update() override;
    void render ( platform::Window& window ) override;
};

}  // namespace angry
