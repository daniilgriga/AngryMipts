#pragma once
#include "data/account_service.hpp"
#include "scene.hpp"

namespace angry
{

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
