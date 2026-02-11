#pragma once
#include "shared/world_snapshot.hpp"

#include <SFML/Graphics.hpp>

namespace angry
{

class Renderer
{
public:
    void draw_snapshot ( sf::RenderWindow& window, const WorldSnapshot& snapshot );

private:
    void draw_object ( sf::RenderWindow& window, const ObjectSnapshot& obj );
    void draw_slingshot ( sf::RenderWindow& window, const SlingshotState& sling );

    sf::Color material_color ( Material mat );
    sf::Color kind_color ( ObjectSnapshot::Kind kind );
};

}  // namespace angry
