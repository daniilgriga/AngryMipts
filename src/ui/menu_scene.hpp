#pragma once
#include "scene.hpp"

namespace angry
{

class MenuScene : public Scene
{
private:
    sf::Font font_;
    sf::Text title_;
    sf::Text prompt_;

public:
    explicit MenuScene ( const sf::Font& font );

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;
};

}  // namespace angry
