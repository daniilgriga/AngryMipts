#pragma once
#include "data/account_service.hpp"
#include "scene.hpp"

namespace angry
{

class MenuScene : public Scene
{
private:
    AccountService& accounts_;
    sf::Font        font_;
    sf::Text        title_;
    sf::Text        prompt_;
    sf::Text        badge_text_;
    sf::Text        badge_btn_;

public:
    MenuScene ( const sf::Font& font, AccountService& accounts );

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;
};

}  // namespace angry
