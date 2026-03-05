#include "render/renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace angry
{
namespace
{

constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kGroundY = 700.f;

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
        return "Crusher";
    case ProjectileType::Splitter:
        return "Splitter";
    case ProjectileType::Standard:
    default:
        return "Striker";
    }
}

}  // namespace

void draw_hill ( sf::RenderTarget& target, float cx, float base_y,
                 float width, float height, sf::Color color );
void draw_cloud ( sf::RenderTarget& target, float x, float y, float scale );

void Renderer::draw_hud ( sf::RenderTarget& target, const WorldSnapshot& snapshot,
                          sf::Text& score_text )
{
    static sf::Clock hud_clock;
    static sf::Clock hud_frame_clock;
    static std::vector<ProjectileType> last_queue;
    static float queue_slide = 0.f;

    const float t = hud_clock.getElapsedTime().asSeconds();
    const float dt = std::clamp ( hud_frame_clock.restart().asSeconds(), 0.f, 0.1f );
    const float pulse = 0.5f + 0.5f * std::sin ( t * 4.2f );
    const sf::Vector2f ws ( target.getSize() );

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
    const sf::Vector2f card_size ( 420.f, 100.f );
    const sf::Vector2f card_pos ( 18.f, 16.f );

    sf::RectangleShape card_shadow ( card_size );
    card_shadow.setPosition ( card_pos + sf::Vector2f ( 6.f, 8.f ) );
    card_shadow.setFillColor ( sf::Color ( 8, 12, 20, 110 ) );
    target.draw ( card_shadow );

    sf::RectangleShape card ( card_size );
    card.setPosition ( card_pos );
    card.setFillColor ( sf::Color ( 10, 18, 34, 166 ) );
    card.setOutlineThickness ( 2.2f );
    card.setOutlineColor ( sf::Color ( 170, 220, 255, 118 ) );
    target.draw ( card );

    sf::RectangleShape card_accent ( {card_size.x - 8.f, 5.f} );
    card_accent.setPosition ( card_pos + sf::Vector2f ( 4.f, 4.f ) );
    card_accent.setFillColor ( sf::Color ( 132, 204, 255, 140 ) );
    target.draw ( card_accent );

    score_text.setString ( "Score  " + std::to_string ( snapshot.score ) );
    score_text.setCharacterSize ( 32 );
    score_text.setStyle ( sf::Text::Bold );
    score_text.setFillColor ( sf::Color ( 245, 250, 255 ) );
    score_text.setPosition ( card_pos + sf::Vector2f ( 18.f, 14.f ) );
    target.draw ( score_text );

    sf::Text status_text = score_text;
    status_text.setCharacterSize ( 16 );
    status_text.setStyle ( sf::Text::Regular );
    status_text.setFillColor ( sf::Color ( 216, 236, 255, 220 ) );
    status_text.setString (
        "Shots  " + std::to_string ( remaining ) + "/" + std::to_string ( total )
        + "    " + charge_line );
    status_text.setPosition ( card_pos + sf::Vector2f ( 20.f, 58.f ) );
    target.draw ( status_text );

    sf::Text controls_text = status_text;
    controls_text.setCharacterSize ( 14 );
    controls_text.setFillColor ( sf::Color ( 190, 220, 244,
                                             static_cast<uint8_t> ( 156.f + pulse * 90.f ) ) );
    controls_text.setString ( "[Space] Ability   [Backspace] Menu" );
    controls_text.setPosition ( card_pos + sf::Vector2f ( 20.f, 78.f ) );
    target.draw ( controls_text );

    // Top-right mini-sprite ammo queue
    const float radius = 12.f;
    const float spacing = 34.f;
    const float rail_w =
        42.f + static_cast<float> ( std::max ( 1, total ) - 1 ) * spacing + radius * 2.f;
    const float rail_h = 52.f;
    const sf::Vector2f rail_pos ( ws.x - rail_w - 18.f, 16.f );

    sf::RectangleShape rail_shadow ( {rail_w, rail_h} );
    rail_shadow.setPosition ( rail_pos + sf::Vector2f ( 4.f, 6.f ) );
    rail_shadow.setFillColor ( sf::Color ( 8, 12, 20, 110 ) );
    target.draw ( rail_shadow );

    sf::RectangleShape rail ( {rail_w, rail_h} );
    rail.setPosition ( rail_pos );
    rail.setFillColor ( sf::Color ( 10, 16, 30, 160 ) );
    rail.setOutlineThickness ( 2.f );
    rail.setOutlineColor ( sf::Color ( 160, 208, 240, 110 ) );
    target.draw ( rail );

    sf::RectangleShape rail_accent ( {rail_w - 6.f, 4.f} );
    rail_accent.setPosition ( rail_pos + sf::Vector2f ( 3.f, 3.f ) );
    rail_accent.setFillColor ( sf::Color ( 132, 194, 245, 115 ) );
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

        sf::CircleShape slot ( radius + 5.f );
        slot.setOrigin ( {radius + 5.f, radius + 5.f} );
        slot.setPosition ( {slot_x, base_y} );
        slot.setFillColor ( sf::Color ( 255, 255, 255, 18 ) );
        target.draw ( slot );

        if ( front_projectile )
        {
            const float glow_r = radius + 9.f + pulse * 3.f;
            sf::CircleShape glow ( glow_r );
            glow.setOrigin ( {glow_r, glow_r} );
            glow.setPosition ( {slot_x + slide_px, base_y} );
            glow.setFillColor (
                sf::Color ( 255, 235, 164, static_cast<uint8_t> ( 58.f + pulse * 62.f ) ) );
            target.draw ( glow );
        }

        if ( has_projectile )
        {
            const ProjectileType type = queue[static_cast<size_t> ( queue_idx )];
            const sf::Texture& tex = textures_.projectile ( type );
            sf::Sprite icon ( tex );
            const sf::Vector2u tex_size = tex.getSize();
            const float diameter = front_projectile ? radius * 2.f + 3.f : radius * 2.f;
            icon.setOrigin ( {static_cast<float> ( tex_size.x ) * 0.5f,
                              static_cast<float> ( tex_size.y ) * 0.5f} );
            icon.setScale ( {diameter / static_cast<float> ( tex_size.x ),
                             diameter / static_cast<float> ( tex_size.y )} );
            icon.setPosition ( {slot_x + slide_px, base_y} );
            icon.setColor ( front_projectile ? sf::Color::White
                                             : sf::Color ( 245, 245, 245, 208 ) );
            target.draw ( icon );

            if ( front_projectile )
            {
                sf::CircleShape ring ( radius + 1.5f );
                ring.setOrigin ( {radius + 1.5f, radius + 1.5f} );
                ring.setPosition ( {slot_x + slide_px, base_y} );
                ring.setFillColor ( sf::Color::Transparent );
                ring.setOutlineThickness ( 1.8f );
                ring.setOutlineColor ( projectile_outline ( type ) );
                target.draw ( ring );
            }
            continue;
        }

        sf::CircleShape spent ( radius );
        spent.setOrigin ( {radius, radius} );
        spent.setPosition ( {slot_x, base_y} );
        spent.setFillColor ( sf::Color::Transparent );
        spent.setOutlineColor ( sf::Color ( 124, 132, 148, 130 ) );
        spent.setOutlineThickness ( 2.f );
        target.draw ( spent );
    }
}

void Renderer::draw_snapshot ( sf::RenderTarget& target, const WorldSnapshot& snapshot )
{
    draw_background ( target );
    draw_slingshot ( target, snapshot.slingshot );

    for ( const auto& obj : snapshot.objects )
    {
        if ( obj.isActive )
            draw_object ( target, obj );
    }
}

void Renderer::draw_background ( sf::RenderTarget& target )
{
    // Sky gradient (top=deep blue, bottom=light blue)
    sf::Vertex sky[] = {
        {{0.f, 0.f}, sf::Color ( 70, 130, 200 )},
        {{kWorldW, 0.f}, sf::Color ( 70, 130, 200 )},
        {{kWorldW, kGroundY}, sf::Color ( 160, 210, 240 )},
        {{0.f, kGroundY}, sf::Color ( 160, 210, 240 )},
    };
    target.draw ( sky, 4, sf::PrimitiveType::TriangleFan );

    // Distant hills (dark green silhouettes)
    draw_hill ( target, 300.f, kGroundY, 400.f, 120.f, sf::Color ( 60, 100, 50, 180 ) );
    draw_hill ( target, 800.f, kGroundY, 500.f, 90.f, sf::Color ( 50, 90, 45, 160 ) );
    draw_hill ( target, 1400.f, kGroundY, 450.f, 110.f, sf::Color ( 55, 95, 48, 170 ) );

    // Clouds
    static sf::Clock cloud_clock;
    const float t = cloud_clock.getElapsedTime().asSeconds();
    draw_cloud ( target, std::fmod ( 250.f + t * 5.f, kWorldW + 320.f ) - 160.f, 120.f, 1.2f );
    draw_cloud ( target, std::fmod ( 700.f + t * 6.2f, kWorldW + 340.f ) - 170.f, 80.f, 0.8f );
    draw_cloud ( target, std::fmod ( 1200.f + t * 4.1f, kWorldW + 300.f ) - 150.f, 150.f, 1.0f );
    draw_cloud ( target, std::fmod ( 1600.f + t * 7.4f, kWorldW + 360.f ) - 180.f, 100.f, 0.6f );

    // Grass
    sf::RectangleShape grass ( {kWorldW, kWorldH - kGroundY} );
    grass.setPosition ( {0.f, kGroundY} );
    grass.setFillColor ( sf::Color ( 90, 170, 65 ) );
    target.draw ( grass );

    // Grass highlight strip
    sf::RectangleShape grass_top ( {kWorldW, 8.f} );
    grass_top.setPosition ( {0.f, kGroundY} );
    grass_top.setFillColor ( sf::Color ( 110, 190, 75 ) );
    target.draw ( grass_top );

    // Darker earth at very bottom
    sf::RectangleShape earth ( {kWorldW, 30.f} );
    earth.setPosition ( {0.f, kWorldH - 30.f} );
    earth.setFillColor ( sf::Color ( 70, 120, 45 ) );
    target.draw ( earth );
}

void draw_hill ( sf::RenderTarget& target, float cx, float base_y,
                 float width, float height, sf::Color color )
{
    const int segments = 20;
    sf::VertexArray hill ( sf::PrimitiveType::TriangleFan, segments + 2 );

    // center bottom
    hill[0] = {{cx, base_y}, color};

    for ( int i = 0; i <= segments; ++i )
    {
        float t = static_cast<float> ( i ) / static_cast<float> ( segments );
        float x = cx - width / 2.f + t * width;
        float y = base_y - height * std::sin ( t * 3.14159f );
        hill[static_cast<unsigned> ( i + 1 )] = {{x, y}, color};
    }

    target.draw ( hill );
}

void draw_cloud ( sf::RenderTarget& target, float x, float y, float scale )
{
    sf::Color cloud_color ( 255, 255, 255, 60 );

    auto blob = [&] ( float ox, float oy, float r )
    {
        sf::CircleShape c ( r * scale );
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

void Renderer::draw_object ( sf::RenderTarget& target, const ObjectSnapshot& obj )
{
    const bool uses_hp = ( obj.kind == ObjectSnapshot::Kind::Block
                           || obj.kind == ObjectSnapshot::Kind::Target );

    const sf::Texture* texture = nullptr;
    if ( obj.kind == ObjectSnapshot::Kind::Block )
    {
        texture = &textures_.block ( obj.material );
    }
    else if ( obj.kind == ObjectSnapshot::Kind::Target )
    {
        texture = &textures_.target();
    }
    else if ( obj.kind == ObjectSnapshot::Kind::Projectile )
    {
        texture = &textures_.projectile ( obj.projectileType );
    }

    if ( texture )
    {
        sf::Sprite sprite ( *texture );
        const auto tex_size = texture->getSize();
        const sf::Vector2f tex_size_f ( static_cast<float> ( tex_size.x ),
                                        static_cast<float> ( tex_size.y ) );

        float draw_w = obj.sizePx.x;
        float draw_h = obj.sizePx.y;
        if ( obj.radiusPx > 0.f )
        {
            draw_w = obj.radiusPx * 2.f;
            draw_h = obj.radiusPx * 2.f;
        }

        if ( draw_w <= 0.f || draw_h <= 0.f )
            return;

        sprite.setOrigin ( {tex_size_f.x * 0.5f, tex_size_f.y * 0.5f} );
        sprite.setScale ( {draw_w / tex_size_f.x, draw_h / tex_size_f.y} );
        sprite.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        sprite.setRotation ( sf::degrees ( obj.angleDeg ) );
        sprite.setColor ( uses_hp ? tint_by_hp ( sf::Color::White, obj.hpNormalized )
                                  : sf::Color::White );
        target.draw ( sprite );
        return;
    }

    if ( obj.radiusPx > 0.f )
    {
        sf::CircleShape shape ( obj.radiusPx );
        shape.setOrigin ( {obj.radiusPx, obj.radiusPx} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );

        sf::Color color;
        if ( obj.kind == ObjectSnapshot::Kind::Projectile )
        {
            color = projectile_color ( obj.projectileType );
            shape.setOutlineColor ( projectile_outline ( obj.projectileType ) );
            shape.setOutlineThickness ( 2.f );
        }
        else if ( obj.kind == ObjectSnapshot::Kind::Block )
        {
            color = material_color ( obj.material );
        }
        else
        {
            color = kind_color ( obj.kind );
        }

        if ( uses_hp )
            color = tint_by_hp ( color, obj.hpNormalized );
        shape.setFillColor ( color );

        target.draw ( shape );
    }
    else
    {
        sf::RectangleShape shape ( {obj.sizePx.x, obj.sizePx.y} );
        shape.setOrigin ( {obj.sizePx.x / 2.f, obj.sizePx.y / 2.f} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );

        sf::Color color;
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
        if ( uses_hp )
            color = tint_by_hp ( color, obj.hpNormalized );
        shape.setFillColor ( color );

        target.draw ( shape );
    }
}

void Renderer::draw_slingshot ( sf::RenderTarget& target, const SlingshotState& sling )
{
    const sf::Texture& wood = textures_.slingshot_wood();
    const auto tex_size = wood.getSize();

    auto draw_piece = [&] ( sf::Vector2f position, sf::Vector2f size_px )
    {
        sf::Sprite piece ( wood );
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

sf::Color Renderer::material_color ( Material mat )
{
    switch ( mat )
    {
    case Material::Wood:
        return sf::Color ( 180, 120, 60 );
    case Material::Stone:
        return sf::Color ( 150, 150, 150 );
    case Material::Glass:
        return sf::Color ( 170, 220, 240, 180 );
    case Material::Ice:
        return sf::Color ( 200, 230, 255, 200 );
    default:
        return sf::Color::White;
    }
}

sf::Color Renderer::kind_color ( ObjectSnapshot::Kind kind )
{
    switch ( kind )
    {
    case ObjectSnapshot::Kind::Target:
        return sf::Color ( 220, 50, 50 );
    case ObjectSnapshot::Kind::Projectile:
        return sf::Color ( 50, 50, 50 );
    case ObjectSnapshot::Kind::Debris:
        return sf::Color ( 120, 120, 120 );
    default:
        return sf::Color::White;
    }
}

sf::Color Renderer::projectile_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return sf::Color ( 246, 164, 74 );
    case ProjectileType::Bomber:
        return sf::Color ( 86, 90, 104 );
    case ProjectileType::Dropper:
        return sf::Color ( 88, 188, 152 );
    case ProjectileType::Boomerang:
        return sf::Color ( 156, 196, 82 );
    case ProjectileType::Bubbler:
        return sf::Color ( 92, 194, 236 );
    case ProjectileType::Inflater:
        return sf::Color ( 234, 120, 174 );
    case ProjectileType::Heavy:
        return sf::Color ( 80, 40, 120 );
    case ProjectileType::Splitter:
        return sf::Color ( 50, 130, 200 );
    default:
        return sf::Color ( 200, 60, 60 );
    }
}

sf::Color Renderer::projectile_outline ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return sf::Color ( 255, 212, 142 );
    case ProjectileType::Bomber:
        return sf::Color ( 255, 180, 102 );
    case ProjectileType::Dropper:
        return sf::Color ( 180, 246, 218 );
    case ProjectileType::Boomerang:
        return sf::Color ( 226, 248, 170 );
    case ProjectileType::Bubbler:
        return sf::Color ( 196, 242, 255 );
    case ProjectileType::Inflater:
        return sf::Color ( 255, 212, 230 );
    case ProjectileType::Heavy:
        return sf::Color ( 160, 100, 220 );
    case ProjectileType::Splitter:
        return sf::Color ( 100, 190, 255 );
    default:
        return sf::Color ( 255, 120, 120 );
    }
}

sf::Color Renderer::tint_by_hp ( sf::Color base, float hp )
{
    const float t = std::clamp ( hp, 0.f, 1.f );
    return sf::Color (
        static_cast<uint8_t> ( base.r * t + 80.f * ( 1.f - t ) ),
        static_cast<uint8_t> ( base.g * t ),
        static_cast<uint8_t> ( base.b * t ),
        base.a
    );
}

}  // namespace angry
