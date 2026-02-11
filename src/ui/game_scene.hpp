#pragma once
#include "render/renderer.hpp"
#include "scene.hpp"
#include "shared/world_snapshot.hpp"

namespace angry
{

class GameScene : public Scene
{
private:
    Renderer renderer_;
    WorldSnapshot snapshot_;
    sf::Font font_;
    sf::Text hud_text_;

    static WorldSnapshot make_mock_snapshot();

public:
    explicit GameScene ( const sf::Font& font );

    SceneId handle_input ( const sf::Event& event ) override;
    void update() override;
    void render ( sf::RenderWindow& window ) override;
};

}  // namespace angry
