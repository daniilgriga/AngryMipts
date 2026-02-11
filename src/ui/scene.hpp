#pragma once
#include <SFML/Graphics.hpp>

namespace angry
{

enum class SceneId
{
    Menu,
    LevelSelect,
    Game,
    Result,
    None,
};

class Scene
{
public:
    virtual ~Scene() = default;

    virtual SceneId handle_input ( const sf::Event& event ) = 0;
    virtual void update() = 0;
    virtual void render ( sf::RenderWindow& window ) = 0;
};

}  // namespace angry
