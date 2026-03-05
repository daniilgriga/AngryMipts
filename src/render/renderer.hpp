#pragma once
#include "render/texture_manager.hpp"
#include "shared/world_snapshot.hpp"

#include <SFML/Graphics.hpp>

namespace angry
{

class Renderer
{
public:
    void draw_snapshot ( sf::RenderTarget& target, const WorldSnapshot& snapshot );
    void draw_hud ( sf::RenderTarget& target, const WorldSnapshot& snapshot,
                    sf::Text& score_text );

private:
    TextureManager textures_;

    void draw_object ( sf::RenderTarget& target, const ObjectSnapshot& obj );
    void draw_slingshot ( sf::RenderTarget& target, const SlingshotState& sling );
    void draw_background ( sf::RenderTarget& target );

    sf::Color material_color ( Material mat );
    sf::Color kind_color ( ObjectSnapshot::Kind kind );
    sf::Color projectile_color ( ProjectileType type );
    sf::Color projectile_outline ( ProjectileType type );
    sf::Color tint_by_hp ( sf::Color base, float hp );
};

}  // namespace angry
