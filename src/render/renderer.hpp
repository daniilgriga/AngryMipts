// ============================================================
// renderer.hpp — World and HUD rendering facade.
// Part of: angry::render
//
// Declares high-level frame rendering entry points:
//   * Draws world snapshot objects and slingshot
//   * Draws HUD state (score, shots, projectile queue)
//   * Provides projectile textures for UI integrations
//   * Uses TextureManager for material/projectile assets
// ============================================================

#pragma once
#include "platform/platform.hpp"
#include "render/texture_manager.hpp"
#include "shared/world_snapshot.hpp"

namespace angry
{

// Renders immutable WorldSnapshot content into target-specific
// draw calls and textures used by UI/game scenes.
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
