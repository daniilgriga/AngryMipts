#pragma once
#include "platform/platform.hpp"
#include "render/texture_manager.hpp"
#include "shared/world_snapshot.hpp"

namespace angry
{

class Renderer
{
public:
    void draw_snapshot ( platform::RenderTarget& target, const WorldSnapshot& snapshot );
    void draw_hud ( platform::RenderTarget& target, const WorldSnapshot& snapshot,
                    platform::Text& score_text );
    const platform::Texture& projectile_texture ( ProjectileType type );

private:
    TextureManager textures_;

    void draw_object ( platform::RenderTarget& target, const ObjectSnapshot& obj );
    void draw_slingshot ( platform::RenderTarget& target, const SlingshotState& sling );
    void draw_background ( platform::RenderTarget& target );
    void draw_damage_overlay ( platform::RenderTarget& target, const ObjectSnapshot& obj );

    platform::Color material_color ( Material mat );
    platform::Color kind_color ( ObjectSnapshot::Kind kind );
    platform::Color projectile_color ( ProjectileType type );
    platform::Color projectile_outline ( ProjectileType type );
    platform::Color tint_by_hp ( platform::Color base, float hp );
};

}  // namespace angry
