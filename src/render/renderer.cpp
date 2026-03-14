// ============================================================
// renderer.cpp — World/HUD rendering implementation.
// Part of: angry::render
//
// Implements translation from WorldSnapshot to draw commands:
//   * Background, slingshot, objects, and overlays
//   * Material/projectile tinting and health-based effects
//   * HUD rendering for score and projectile state
//   * Geometry helpers for rotated and shaped objects
// ============================================================

#include "render/renderer.hpp"
#include "shared/world_config.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#endif

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace angry
{
namespace
{

constexpr float kWorldW = world::kWidthPx;
constexpr float kWorldH = world::kHeightPx;
constexpr float kGroundY = world::kGroundTopYpx;
constexpr float kPi = 3.14159265358979323846f;

platform::Vec2f rotate_local ( platform::Vec2f v, float angle_deg )
{
    const float rad = angle_deg * kPi / 180.f;
    const float cs = std::cos ( rad );
    const float sn = std::sin ( rad );
    return {v.x * cs - v.y * sn, v.x * sn + v.y * cs};
}

float hash01 ( uint32_t value )
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return static_cast<float> ( value & 0x00ffffffu )
           / static_cast<float> ( 0x01000000u );
}

std::string projectile_label ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return "Dasher";
    case ProjectileType::Bomber:
        return "Bomber";
    case ProjectileType::Dropper:
        return "Dropper";
    case ProjectileType::Boomerang:
        return "Boomerang";
    case ProjectileType::Bubbler:
        return "Bubbler";
    case ProjectileType::Inflater:
        return "Inflater";
    case ProjectileType::Heavy:
        return "Heavy";
    case ProjectileType::Splitter:
        return "Splitter";
    case ProjectileType::Standard:
    default:
        return "Striker";
    }
}

}  // namespace

void draw_hill ( platform::RenderTarget& target, float cx, float base_y,
                 float width, float height, platform::Color color );
void draw_cloud ( platform::RenderTarget& target, float x, float y, float scale );

// #=# HUD Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void Renderer::draw_hud ( platform::RenderTarget& target, const WorldSnapshot& snapshot,
                          platform::Text& score_text )
{
    static platform::Clock hud_clock;
    static platform::Clock hud_frame_clock;
    static std::vector<ProjectileType> last_queue;
    static float queue_slide = 0.f;

    const float t = hud_clock.getElapsedTime().asSeconds();
    const float dt = std::clamp ( hud_frame_clock.restart().asSeconds(), 0.f, 0.1f );
    const float pulse = 0.5f + 0.5f * std::sin ( t * 4.2f );
    const platform::Vec2f ws ( target.getSize() );

    const int total = std::max ( 0, snapshot.totalShots );
    const int remaining = std::clamp ( snapshot.shotsRemaining, 0, total );
    const auto& queue = snapshot.projectileQueue;
    const std::string current_name =
        queue.empty() ? "--" : projectile_label ( queue.front() );
    const std::string next_name =
        queue.size() > 1 ? projectile_label ( queue[1] ) : "--";

    const std::string charge_line = snapshot.slingshot.canShoot
                                        ? "Loaded  " + current_name + "    Next  " + next_name
                                        : "Current  " + current_name + "    Next  " + next_name;

    if ( last_queue != queue )
    {
        queue_slide = 1.f;
        last_queue = queue;
    }
    queue_slide = std::max ( 0.f, queue_slide - dt * 7.5f );
    const float slide_px = 16.f * queue_slide * queue_slide;

    // Top status card
    const platform::Vec2f card_size ( 420.f, 100.f );
    const platform::Vec2f card_pos ( 18.f, 16.f );

    platform::RectShape card ( card_size );
    card.setPosition ( card_pos );
    card.setFillColor ( platform::Color ( 10, 18, 34, 166 ) );
    card.setOutlineThickness ( 2.2f );
    card.setOutlineColor ( platform::Color ( 170, 220, 255, 118 ) );
    target.draw ( card );

    platform::RectShape card_accent ( {card_size.x - 8.f, 5.f} );
    card_accent.setPosition ( card_pos + platform::Vec2f ( 4.f, 4.f ) );
    card_accent.setFillColor ( platform::Color ( 132, 204, 255, 140 ) );
    target.draw ( card_accent );

    score_text.setString ( "Score  " + std::to_string ( snapshot.score ) );
    score_text.setCharacterSize ( 32 );
    score_text.setStyle ( sf::Text::Bold );
    score_text.setFillColor ( platform::Color ( 245, 250, 255 ) );
    score_text.setPosition ( card_pos + platform::Vec2f ( 18.f, 14.f ) );
    target.draw ( score_text );

    platform::Text status_text = score_text;
    status_text.setCharacterSize ( 16 );
    status_text.setStyle ( sf::Text::Regular );
    status_text.setFillColor ( platform::Color ( 216, 236, 255, 220 ) );
    status_text.setString (
        "Shots  " + std::to_string ( remaining ) + "/" + std::to_string ( total )
        + "    " + charge_line );
    status_text.setPosition ( card_pos + platform::Vec2f ( 20.f, 58.f ) );
    target.draw ( status_text );

    platform::Text controls_text = status_text;
    controls_text.setCharacterSize ( 14 );
    controls_text.setFillColor ( platform::Color ( 190, 220, 244,
                                             static_cast<uint8_t> ( 156.f + pulse * 90.f ) ) );
    controls_text.setString ( "[Space] Ability   [Backspace] Menu" );
    controls_text.setPosition ( card_pos + platform::Vec2f ( 20.f, 78.f ) );
    target.draw ( controls_text );

    // Top-right mini-sprite ammo queue
    const float radius = 12.f;
    const float spacing = 34.f;
    const float rail_w =
        42.f + static_cast<float> ( std::max ( 1, total ) - 1 ) * spacing + radius * 2.f;
    const float rail_h = 52.f;
    const platform::Vec2f rail_pos ( ws.x - rail_w - 18.f, 16.f );

    platform::RectShape rail ( {rail_w, rail_h} );
    rail.setPosition ( rail_pos );
    rail.setFillColor ( platform::Color ( 10, 16, 30, 160 ) );
    rail.setOutlineThickness ( 2.f );
    rail.setOutlineColor ( platform::Color ( 160, 208, 240, 110 ) );
    target.draw ( rail );

    platform::RectShape rail_accent ( {rail_w - 6.f, 4.f} );
    rail_accent.setPosition ( rail_pos + platform::Vec2f ( 3.f, 3.f ) );
    rail_accent.setFillColor ( platform::Color ( 132, 194, 245, 115 ) );
    target.draw ( rail_accent );

    const float base_x = rail_pos.x + 20.f + radius;
    const float base_y = rail_pos.y + rail_h * 0.5f;
    const int queue_count = static_cast<int> ( queue.size() );
    const int consumed = std::max ( 0, total - queue_count );

    for ( int i = 0; i < total; ++i )
    {
        const float slot_x = base_x + i * spacing;
        const int queue_idx = i - consumed;
        const bool has_projectile = ( queue_idx >= 0 && queue_idx < queue_count );
        const bool front_projectile = has_projectile && queue_idx == 0;

        platform::CircleShape slot ( radius + 5.f );
        slot.setOrigin ( {radius + 5.f, radius + 5.f} );
        slot.setPosition ( {slot_x, base_y} );
        slot.setFillColor ( platform::Color ( 255, 255, 255, 18 ) );
        target.draw ( slot );

        if ( has_projectile )
        {
            const ProjectileType type = queue[static_cast<size_t> ( queue_idx )];
            const platform::Texture& tex = textures_.projectile ( type );
            platform::Sprite icon ( tex );
            const platform::Vec2u tex_size = tex.getSize();
            const float diameter = front_projectile ? radius * 2.f + 3.f : radius * 2.f;
            icon.setOrigin ( {static_cast<float> ( tex_size.x ) * 0.5f,
                              static_cast<float> ( tex_size.y ) * 0.5f} );
            icon.setScale ( {diameter / static_cast<float> ( tex_size.x ),
                             diameter / static_cast<float> ( tex_size.y )} );
            icon.setPosition ( {slot_x + slide_px, base_y} );
            icon.setColor ( front_projectile ? platform::Color::White
                                             : platform::Color ( 245, 245, 245, 208 ) );
            target.draw ( icon );

            if ( front_projectile )
            {
                platform::CircleShape ring ( radius + 1.5f );
                ring.setOrigin ( {radius + 1.5f, radius + 1.5f} );
                ring.setPosition ( {slot_x + slide_px, base_y} );
                ring.setFillColor ( platform::Color::Transparent );
                ring.setOutlineThickness ( 1.8f );
                ring.setOutlineColor ( projectile_outline ( type ) );
                target.draw ( ring );
            }
            continue;
        }

        platform::CircleShape spent ( radius );
        spent.setOrigin ( {radius, radius} );
        spent.setPosition ( {slot_x, base_y} );
        spent.setFillColor ( platform::Color::Transparent );
        spent.setOutlineColor ( platform::Color ( 124, 132, 148, 130 ) );
        spent.setOutlineThickness ( 2.f );
        target.draw ( spent );
    }
}

// #=# World Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void Renderer::draw_snapshot ( platform::RenderTarget& target, const WorldSnapshot& snapshot )
{
    draw_background ( target );
    draw_slingshot ( target, snapshot.slingshot );

    for ( const auto& obj : snapshot.objects )
    {
        if ( obj.isActive )
            draw_object ( target, obj );
    }
}

void Renderer::draw_background ( platform::RenderTarget& target )
{
    // Sky — two solid rects in world coordinates (Window::draw(RectShape) applies
    // world_to_screen internally, so we use world units here).
    const float sky_mid = kGroundY * 0.55f;

    platform::RectShape sky_top ( { kWorldW, sky_mid } );
    sky_top.setPosition ( { 0.f, 0.f } );
    sky_top.setFillColor ( platform::Color ( 70, 130, 200 ) );
    target.draw ( sky_top );

    platform::RectShape sky_bot ( { kWorldW, kGroundY - sky_mid } );
    sky_bot.setPosition ( { 0.f, sky_mid } );
    sky_bot.setFillColor ( platform::Color ( 148, 200, 235 ) );
    target.draw ( sky_bot );

    // Distant hills (dark green silhouettes)
    draw_hill ( target, 300.f, kGroundY, 400.f, 120.f, platform::Color ( 52, 110, 48, 255 ) );
    draw_hill ( target, 800.f, kGroundY, 500.f, 90.f, platform::Color ( 42, 96, 40, 255 ) );
    draw_hill ( target, 1400.f, kGroundY, 450.f, 110.f, platform::Color ( 48, 104, 44, 255 ) );

    // Clouds
    static platform::Clock cloud_clock;
    const float t = cloud_clock.getElapsedTime().asSeconds();
    draw_cloud ( target, std::fmod ( 250.f + t * 5.f, kWorldW + 320.f ) - 160.f, 120.f, 1.2f );
    draw_cloud ( target, std::fmod ( 700.f + t * 6.2f, kWorldW + 340.f ) - 170.f, 80.f, 0.8f );
    draw_cloud ( target, std::fmod ( 1200.f + t * 4.1f, kWorldW + 300.f ) - 150.f, 150.f, 1.0f );
    draw_cloud ( target, std::fmod ( 1600.f + t * 7.4f, kWorldW + 360.f ) - 180.f, 100.f, 0.6f );

    // --- Ground ---
    // Earth body: layered gradient (grass → topsoil → dark soil)
    {
        const platform::Color c_grass  ( 88,  168,  62 );
        const platform::Color c_top    ( 112, 78,   44 );
        const platform::Color c_soil   ( 72,  50,   28 );
        const platform::Color c_deep   ( 48,  34,   18 );

        // Use RectShape for solid fills — reliable on all WebGL backends.
        // Gradient layers: approximate with two rects (top colour / bottom colour).
        auto draw_rect_strip = [&]( float y0, float y1,
                                    platform::Color top_c, platform::Color bot_c )
        {
            platform::RectShape r ( { kWorldW, ( y1 - y0 ) * 0.5f } );
            r.setPosition ( { 0.f, y0 } );
            r.setFillColor ( top_c );
            target.draw ( r );
            r.setPosition ( { 0.f, y0 + ( y1 - y0 ) * 0.5f } );
            r.setFillColor ( bot_c );
            target.draw ( r );
        };

        draw_rect_strip ( kGroundY,        kGroundY + 28.f,  c_grass, c_top  );
        draw_rect_strip ( kGroundY + 28.f, kGroundY + 120.f, c_top,   c_soil );
        draw_rect_strip ( kGroundY + 120.f, kWorldH,          c_soil,  c_deep );
    }

    // Wavy grass edge: a thin strip with sinusoidal top profile
    {
        const int   segs     = 96;
        const float seg_w    = kWorldW / static_cast<float> ( segs );
        const float wave_amp = 4.f;
        const float strip_h  = 10.f;
        const platform::Color c_bright ( 128, 200, 72 );
        const platform::Color c_mid    ( 98,  175,  58 );

        platform::VertexArray wave ( sf::PrimitiveType::TriangleStrip,
                               static_cast<std::size_t> ( ( segs + 1 ) * 2 ) );
        for ( int i = 0; i <= segs; ++i )
        {
            const float x   = static_cast<float> ( i ) * seg_w;
            const float top = kGroundY - wave_amp
                              * ( 0.5f + 0.5f * std::sin ( x * 0.031f )
                                       + 0.25f * std::sin ( x * 0.073f + 1.1f ) );
            wave[static_cast<std::size_t> ( i * 2 )]     = { {x, top},           c_bright };
            wave[static_cast<std::size_t> ( i * 2 + 1 )] = { {x, top + strip_h}, c_mid    };
        }
        target.draw ( wave );
    }

    // Grass blades: short vertical strokes procedurally scattered
    {
        const int   blade_count = 180;
        const float base_y      = kGroundY;
        platform::VertexArray blades ( sf::PrimitiveType::Lines,
                                 static_cast<std::size_t> ( blade_count * 2 ) );
        for ( int i = 0; i < blade_count; ++i )
        {
            // deterministic pseudo-random position & height from index
            const uint32_t h1 = static_cast<uint32_t> ( i ) * 2654435761u;
            const uint32_t h2 = h1 ^ ( h1 >> 16u );
            const float x     = static_cast<float> ( h1 & 0xFFFFu ) / 65535.f * kWorldW;
            const float lean  = ( static_cast<float> ( h2 & 0xFFu ) / 255.f - 0.5f ) * 6.f;
            const float ht    = 8.f + static_cast<float> ( h2 >> 24u ) / 255.f * 10.f;

            const float wave_offset = -( 0.5f + 0.5f * std::sin ( x * 0.031f )
                                                + 0.25f * std::sin ( x * 0.073f + 1.1f ) ) * 4.f;

            const uint8_t g = static_cast<uint8_t> ( 160 + ( h2 & 0x1Fu ) );
            const platform::Color c_base ( 60,  g,  40, 200 );
            const platform::Color c_tip  ( 110, static_cast<uint8_t>(g+30u > 255u ? 255u : g+30u), 60, 140 );

            blades[static_cast<std::size_t> ( i * 2 )]     = { {x,        base_y + wave_offset},         c_base };
            blades[static_cast<std::size_t> ( i * 2 + 1 )] = { {x + lean, base_y + wave_offset - ht},    c_tip  };
        }
        target.draw ( blades );
    }

    // Soil texture: horizontal scan lines suggesting layered earth
    {
        const int lines = 12;
        for ( int i = 0; i < lines; ++i )
        {
            const float y      = kGroundY + 30.f + static_cast<float> ( i ) * 8.f;
            const uint8_t a    = static_cast<uint8_t> ( 18 + i * 2 );
            const platform::Color lc ( 255, 220, 150, a );
            platform::Vertex ln[] = { {{0.f, y}, lc}, {{kWorldW, y}, lc} };
            target.draw ( ln, 2, sf::PrimitiveType::Lines );
        }
    }
}

void draw_hill ( platform::RenderTarget& target, float cx, float base_y,
                 float width, float height, platform::Color color )
{
    // TriangleStrip: pairs of (top_contour, base_y) vertices marching left→right.
    // This avoids TriangleFan winding issues on WebGL.
    const int segments = 20;
    platform::VertexArray hill ( sf::PrimitiveType::TriangleStrip,
                                 static_cast<std::size_t>( ( segments + 1 ) * 2 ) );

    for ( int i = 0; i <= segments; ++i )
    {
        const float t = static_cast<float>( i ) / static_cast<float>( segments );
        const float x = cx - width * 0.5f + t * width;
        const float y_top = base_y - height * std::sin ( t * 3.14159f );
        hill[static_cast<std::size_t>( i * 2 )]     = { { x, y_top  }, color };
        hill[static_cast<std::size_t>( i * 2 + 1 )] = { { x, base_y }, color };
    }

    target.draw ( hill );
}

void draw_cloud ( platform::RenderTarget& target, float x, float y, float scale )
{
    platform::Color cloud_color ( 255, 255, 255, 60 );

    auto blob = [&] ( float ox, float oy, float r )
    {
        platform::CircleShape c ( r * scale );
        c.setOrigin ( {r * scale, r * scale} );
        c.setPosition ( {x + ox * scale, y + oy * scale} );
        c.setFillColor ( cloud_color );
        target.draw ( c );
    };

    blob ( 0.f, 0.f, 40.f );
    blob ( 50.f, -10.f, 50.f );
    blob ( 100.f, 5.f, 35.f );
    blob ( -40.f, 5.f, 30.f );
    blob ( 30.f, -25.f, 30.f );
}

// Darkens a color for static/indestructible blocks to signal they are immovable.
static platform::Color static_tint ( platform::Color c )
{
    return platform::Color (
        static_cast<uint8_t> ( c.r * 0.55f ),
        static_cast<uint8_t> ( c.g * 0.55f ),
        static_cast<uint8_t> ( c.b * 0.55f ),
        c.a );
}

// #=# Object Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

void Renderer::draw_object ( platform::RenderTarget& target, const ObjectSnapshot& obj )
{
    const bool is_block = ( obj.kind == ObjectSnapshot::Kind::Block );
    const bool uses_hp = is_block || ( obj.kind == ObjectSnapshot::Kind::Target );
    const bool is_triangle = is_block && ( obj.shape == BlockShape::Triangle );

    float draw_w = obj.sizePx.x;
    float draw_h = obj.sizePx.y;
    if ( obj.radiusPx > 0.f )
    {
        draw_w = obj.radiusPx * 2.f;
        draw_h = obj.radiusPx * 2.f;
    }
    if ( draw_w <= 0.f || draw_h <= 0.f )
        return;

    // --- Triangle blocks ---
    if ( is_triangle )
    {
        const platform::Texture& tex = textures_.block ( obj.material );
        const platform::Vec2u ts = tex.getSize();

        platform::ConvexShape tri ( 3 );

        if ( obj.triangleLocalVerticesPx.size() == 3 )
        {
            // Use exact geometry from physics (via snapshot)
            tri.setPoint ( 0, {obj.triangleLocalVerticesPx[0].x, obj.triangleLocalVerticesPx[0].y} );
            tri.setPoint ( 1, {obj.triangleLocalVerticesPx[1].x, obj.triangleLocalVerticesPx[1].y} );
            tri.setPoint ( 2, {obj.triangleLocalVerticesPx[2].x, obj.triangleLocalVerticesPx[2].y} );
        }
        else
        {
            // Legacy fallback: isosceles triangle from bounding box
            const float hw = obj.sizePx.x * 0.5f;
            const float hh = obj.sizePx.y * 0.5f;
            tri.setPoint ( 0, {-hw,  hh} );
            tri.setPoint ( 1, { hw,  hh} );
            tri.setPoint ( 2, {  0, -hh} );
        }

        tri.setOrigin ( {0.f, 0.f} );
        tri.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        tri.setRotation ( sf::degrees ( obj.angleDeg ) );
        tri.setTexture ( &tex );
        tri.setTextureRect ( sf::IntRect ( {0, 0},
            {static_cast<int> ( ts.x ), static_cast<int> ( ts.y )} ) );

        platform::Color tint = obj.isStatic ? static_tint ( platform::Color::White )
                                      : tint_by_hp ( platform::Color::White, obj.hpNormalized );
        tri.setFillColor ( tint );

        if ( obj.isStatic )
        {
            tri.setOutlineThickness ( 2.f );
            tri.setOutlineColor ( platform::Color ( 20, 20, 20, 180 ) );
        }

        target.draw ( tri );
        if ( !obj.isStatic )
            draw_damage_overlay ( target, obj );
        return;
    }

    // --- Textured blocks, targets, projectiles ---
    const platform::Texture* texture = nullptr;
    if ( is_block )
        texture = &textures_.block ( obj.material );
    else if ( obj.kind == ObjectSnapshot::Kind::Target )
        texture = &textures_.target();
    else if ( obj.kind == ObjectSnapshot::Kind::Projectile )
        texture = &textures_.projectile ( obj.projectileType );

    if ( texture )
    {
        platform::Sprite sprite ( *texture );
        const auto tex_size = texture->getSize();
        const platform::Vec2f tex_size_f ( static_cast<float> ( tex_size.x ),
                                        static_cast<float> ( tex_size.y ) );

        sprite.setOrigin ( {tex_size_f.x * 0.5f, tex_size_f.y * 0.5f} );
        sprite.setScale ( {draw_w / tex_size_f.x, draw_h / tex_size_f.y} );
        sprite.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        sprite.setRotation ( sf::degrees ( obj.angleDeg ) );

        platform::Color tint = platform::Color::White;
        if ( is_block && obj.isStatic )
            tint = static_tint ( platform::Color::White );
        else if ( uses_hp )
            tint = tint_by_hp ( platform::Color::White, obj.hpNormalized );
        sprite.setColor ( tint );

        target.draw ( sprite );

        if ( is_block )
        {
            if ( obj.isStatic )
            {
                // Dark outline to visually mark static/indestructible platforms
                const float ow = draw_w + 4.f;
                const float oh = draw_h + 4.f;
                platform::RectShape outline ( {ow, oh} );
                outline.setOrigin ( {ow * 0.5f, oh * 0.5f} );
                outline.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
                outline.setRotation ( sf::degrees ( obj.angleDeg ) );
                outline.setFillColor ( platform::Color::Transparent );
                outline.setOutlineThickness ( 2.f );
                outline.setOutlineColor ( platform::Color ( 20, 20, 20, 160 ) );
                target.draw ( outline );
            }
            else
            {
                draw_damage_overlay ( target, obj );
            }
        }

        if ( obj.kind == ObjectSnapshot::Kind::Projectile
             && obj.projectileType == ProjectileType::Bomber
             && obj.radiusPx > 0.f )
        {
            static platform::Clock bomber_idle_clock;
            const float t = bomber_idle_clock.getElapsedTime().asSeconds();
            const float pulse = 0.5f + 0.5f * std::sin ( t * 7.0f + obj.positionPx.x * 0.012f );

            const float aura_r = obj.radiusPx * ( 0.70f + pulse * 0.12f );
            platform::CircleShape aura ( aura_r );
            aura.setOrigin ( {aura_r, aura_r} );
            aura.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
            aura.setFillColor ( platform::Color ( 255, 146, 70,
                                            static_cast<uint8_t> ( 24.f + pulse * 32.f ) ) );
            target.draw ( aura );

            platform::CircleShape heat_ring ( obj.radiusPx * 0.64f );
            heat_ring.setOrigin ( {obj.radiusPx * 0.64f, obj.radiusPx * 0.64f} );
            heat_ring.setPosition ( {obj.positionPx.x, obj.positionPx.y + obj.radiusPx * 0.08f} );
            heat_ring.setFillColor ( platform::Color::Transparent );
            heat_ring.setOutlineThickness ( std::max ( 1.4f, obj.radiusPx * 0.08f ) );
            heat_ring.setOutlineColor (
                platform::Color ( 255, 192, 122, static_cast<uint8_t> ( 98.f + pulse * 70.f ) ) );
            target.draw ( heat_ring );

            const platform::Vec2f fuse_local ( obj.radiusPx * 0.62f, -obj.radiusPx * 0.78f );
            const platform::Vec2f fuse_offset = rotate_local ( fuse_local, obj.angleDeg );
            const platform::Vec2f fuse_tip ( obj.positionPx.x + fuse_offset.x,
                                          obj.positionPx.y + fuse_offset.y );

            const float spark_glow_r = std::max ( 3.5f, obj.radiusPx * ( 0.17f + pulse * 0.04f ) );
            platform::CircleShape spark_glow ( spark_glow_r );
            spark_glow.setOrigin ( {spark_glow_r, spark_glow_r} );
            spark_glow.setPosition ( fuse_tip );
            spark_glow.setFillColor (
                platform::Color ( 255, 180, 98, static_cast<uint8_t> ( 120.f + pulse * 80.f ) ) );
            target.draw ( spark_glow );

            const float spark_core_r = std::max ( 1.8f, obj.radiusPx * 0.08f );
            platform::CircleShape spark_core ( spark_core_r );
            spark_core.setOrigin ( {spark_core_r, spark_core_r} );
            spark_core.setPosition ( fuse_tip );
            spark_core.setFillColor ( platform::Color ( 255, 240, 174, 228 ) );
            target.draw ( spark_core );
        }
        return;
    }

    // --- Fallback: primitive shapes (no texture available) ---
    if ( obj.radiusPx > 0.f )
    {
        platform::CircleShape shape ( obj.radiusPx );
        shape.setOrigin ( {obj.radiusPx, obj.radiusPx} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );

        platform::Color color;
        if ( obj.kind == ObjectSnapshot::Kind::Projectile )
        {
            color = projectile_color ( obj.projectileType );
            shape.setOutlineColor ( projectile_outline ( obj.projectileType ) );
            shape.setOutlineThickness ( 2.f );
        }
        else
            color = is_block ? material_color ( obj.material ) : kind_color ( obj.kind );

        if ( is_block && obj.isStatic )
            color = static_tint ( color );
        else if ( uses_hp )
            color = tint_by_hp ( color, obj.hpNormalized );
        shape.setFillColor ( color );

        target.draw ( shape );
        if ( is_block && !obj.isStatic )
            draw_damage_overlay ( target, obj );
    }
    else
    {
        platform::RectShape shape ( {obj.sizePx.x, obj.sizePx.y} );
        shape.setOrigin ( {obj.sizePx.x / 2.f, obj.sizePx.y / 2.f} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );

        platform::Color color;
        if ( obj.kind == ObjectSnapshot::Kind::Target )
            color = kind_color ( obj.kind );
        else if ( obj.kind == ObjectSnapshot::Kind::Projectile )
        {
            color = projectile_color ( obj.projectileType );
            shape.setOutlineColor ( projectile_outline ( obj.projectileType ) );
            shape.setOutlineThickness ( 2.f );
        }
        else
            color = material_color ( obj.material );

        if ( is_block && obj.isStatic )
            color = static_tint ( color );
        else if ( uses_hp )
            color = tint_by_hp ( color, obj.hpNormalized );
        shape.setFillColor ( color );

        target.draw ( shape );
        if ( is_block && !obj.isStatic )
            draw_damage_overlay ( target, obj );
    }
}

const platform::Texture& Renderer::projectile_texture ( ProjectileType type )
{
    return textures_.projectile ( type );
}

// #=# Slingshot Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void Renderer::draw_slingshot ( platform::RenderTarget& target, const SlingshotState& sling )
{
    const platform::Texture& wood = textures_.slingshot_wood();
    const auto tex_size = wood.getSize();

    auto draw_piece = [&] ( platform::Vec2f position, platform::Vec2f size_px )
    {
        platform::Sprite piece ( wood );
        piece.setOrigin ( {static_cast<float> ( tex_size.x ) * 0.5f,
                           static_cast<float> ( tex_size.y )} );
        piece.setPosition ( position );
        piece.setScale ( {size_px.x / static_cast<float> ( tex_size.x ),
                          size_px.y / static_cast<float> ( tex_size.y )} );
        target.draw ( piece );
    };

    draw_piece ( {sling.basePx.x, sling.basePx.y}, {14.f, 62.f} );
    draw_piece ( {sling.basePx.x - 10.f, sling.basePx.y - 54.f}, {8.f, 26.f} );
    draw_piece ( {sling.basePx.x + 10.f, sling.basePx.y - 54.f}, {8.f, 26.f} );
}

// #=# Damage Overlay #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

void Renderer::draw_damage_overlay ( platform::RenderTarget& target, const ObjectSnapshot& obj )
{
    if ( obj.kind != ObjectSnapshot::Kind::Block )
        return;

    const float hp = std::clamp ( obj.hpNormalized, 0.f, 1.f );
    const float damage = 1.f - hp;
    if ( damage < 0.22f )
        return;

    const float w = obj.radiusPx > 0.f ? obj.radiusPx * 2.f : obj.sizePx.x;
    const float h = obj.radiusPx > 0.f ? obj.radiusPx * 2.f : obj.sizePx.y;
    if ( w <= 1.f || h <= 1.f )
        return;

    auto crack_color = [this, damage] ( Material mat )
    {
        const uint8_t alpha = static_cast<uint8_t> (
            std::clamp ( 62.f + damage * 154.f, 0.f, 220.f ) );
        switch ( mat )
        {
        case Material::Wood:
            return platform::Color ( 66, 40, 24, alpha );
        case Material::Stone:
            return platform::Color ( 92, 96, 106, alpha );
        case Material::Glass:
            return platform::Color ( 202, 244, 255, alpha );
        case Material::Ice:
            return platform::Color ( 228, 248, 255, alpha );
        default:
            return platform::Color ( 38, 38, 38, alpha );
        }
    };

    const platform::Color c = crack_color ( obj.material );
    const int crack_count = damage > 0.72f ? 7 : ( damage > 0.46f ? 5 : 3 );
    const float half_w = w * 0.5f;
    const float half_h = h * 0.5f;
    const float max_r = std::min ( half_w, half_h ) * 0.94f;

    auto clamp_local = [half_w, half_h, max_r, &obj] ( platform::Vec2f p )
    {
        if ( obj.radiusPx > 0.f )
        {
            const float len = std::sqrt ( p.x * p.x + p.y * p.y );
            if ( len > max_r && len > 0.0001f )
                p *= max_r / len;
            return p;
        }

        p.x = std::clamp ( p.x, -half_w * 0.94f, half_w * 0.94f );
        p.y = std::clamp ( p.y, -half_h * 0.94f, half_h * 0.94f );
        return p;
    };

    auto to_world = [&obj] ( platform::Vec2f local )
    {
        const platform::Vec2f r = rotate_local ( local, obj.angleDeg );
        return platform::Vec2f ( obj.positionPx.x + r.x, obj.positionPx.y + r.y );
    };

    platform::VertexArray crack_lines ( sf::PrimitiveType::Lines );
    crack_lines.resize ( static_cast<std::size_t> ( crack_count * 6 ) );

    std::size_t v = 0;
    for ( int i = 0; i < crack_count; ++i )
    {
        const uint32_t seed = static_cast<uint32_t> ( obj.id ) * 1103515245u
                              + static_cast<uint32_t> ( i ) * 12345u;
        const float a0 = hash01 ( seed + 11u );
        const float a1 = hash01 ( seed + 23u );
        const float a2 = hash01 ( seed + 37u );
        const float a3 = hash01 ( seed + 53u );

        const float theta = a0 * 2.f * kPi;
        const platform::Vec2f dir ( std::cos ( theta ), std::sin ( theta ) );
        const platform::Vec2f normal ( -dir.y, dir.x );

        platform::Vec2f center (
            ( a1 - 0.5f ) * w * 0.50f,
            ( a2 - 0.5f ) * h * 0.50f );
        center = clamp_local ( center );

        const float len = ( std::min ( w, h ) * ( 0.30f + a3 * 0.33f ) )
                          * ( 0.85f + damage * 0.38f );
        const float wobble = len * ( 0.10f + a2 * 0.08f );

        platform::Vec2f p0 = clamp_local ( center - dir * ( len * 0.52f ) );
        platform::Vec2f p1 = clamp_local ( center - dir * ( len * 0.16f ) + normal * wobble );
        platform::Vec2f p2 = clamp_local ( center + dir * ( len * 0.14f ) - normal * wobble * 0.82f );
        platform::Vec2f p3 = clamp_local ( center + dir * ( len * 0.52f ) );

        const platform::Vec2f w0 = to_world ( p0 );
        const platform::Vec2f w1 = to_world ( p1 );
        const platform::Vec2f w2 = to_world ( p2 );
        const platform::Vec2f w3 = to_world ( p3 );

        crack_lines[v].position = w0;
        crack_lines[v++].color = c;
        crack_lines[v].position = w1;
        crack_lines[v++].color = c;
        crack_lines[v].position = w1;
        crack_lines[v++].color = c;
        crack_lines[v].position = w2;
        crack_lines[v++].color = c;
        crack_lines[v].position = w2;
        crack_lines[v++].color = c;
        crack_lines[v].position = w3;
        crack_lines[v++].color = c;
    }

    target.draw ( crack_lines );
}

// #=# Color Palettes #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

platform::Color Renderer::material_color ( Material mat )
{
    switch ( mat )
    {
    case Material::Wood:
        return platform::Color ( 180, 120, 60 );
    case Material::Stone:
        return platform::Color ( 150, 150, 150 );
    case Material::Glass:
        return platform::Color ( 170, 220, 240, 180 );
    case Material::Ice:
        return platform::Color ( 200, 230, 255, 200 );
    default:
        return platform::Color::White;
    }
}

platform::Color Renderer::kind_color ( ObjectSnapshot::Kind kind )
{
    switch ( kind )
    {
    case ObjectSnapshot::Kind::Target:
        return platform::Color ( 220, 50, 50 );
    case ObjectSnapshot::Kind::Projectile:
        return platform::Color ( 50, 50, 50 );
    case ObjectSnapshot::Kind::Debris:
        return platform::Color ( 120, 120, 120 );
    default:
        return platform::Color::White;
    }
}

platform::Color Renderer::projectile_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return platform::Color ( 246, 164, 74 );
    case ProjectileType::Bomber:
        return platform::Color ( 86, 90, 104 );
    case ProjectileType::Dropper:
        return platform::Color ( 88, 188, 152 );
    case ProjectileType::Boomerang:
        return platform::Color ( 156, 196, 82 );
    case ProjectileType::Bubbler:
        return platform::Color ( 92, 194, 236 );
    case ProjectileType::Inflater:
        return platform::Color ( 234, 120, 174 );
    case ProjectileType::Heavy:
        return platform::Color ( 80, 40, 120 );
    case ProjectileType::Splitter:
        return platform::Color ( 50, 130, 200 );
    default:
        return platform::Color ( 200, 60, 60 );
    }
}

platform::Color Renderer::projectile_outline ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return platform::Color ( 255, 212, 142 );
    case ProjectileType::Bomber:
        return platform::Color ( 255, 180, 102 );
    case ProjectileType::Dropper:
        return platform::Color ( 180, 246, 218 );
    case ProjectileType::Boomerang:
        return platform::Color ( 226, 248, 170 );
    case ProjectileType::Bubbler:
        return platform::Color ( 196, 242, 255 );
    case ProjectileType::Inflater:
        return platform::Color ( 255, 212, 230 );
    case ProjectileType::Heavy:
        return platform::Color ( 160, 100, 220 );
    case ProjectileType::Splitter:
        return platform::Color ( 100, 190, 255 );
    default:
        return platform::Color ( 255, 120, 120 );
    }
}

platform::Color Renderer::tint_by_hp ( platform::Color base, float hp )
{
    const float t = std::clamp ( hp, 0.f, 1.f );
    return platform::Color (
        static_cast<uint8_t> ( base.r * t + 80.f * ( 1.f - t ) ),
        static_cast<uint8_t> ( base.g * t ),
        static_cast<uint8_t> ( base.b * t ),
        base.a
    );
}

}  // namespace angry
