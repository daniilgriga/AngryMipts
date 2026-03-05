#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>

namespace angry
{
namespace
{

constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kGroundY = 700.f;

}  // namespace

void draw_hill ( sf::RenderTarget& target, float cx, float base_y,
                 float width, float height, sf::Color color );
void draw_cloud ( sf::RenderTarget& target, float x, float y, float scale );

void Renderer::draw_hud ( sf::RenderTarget& target, const WorldSnapshot& snapshot,
                          sf::Text& score_text )
{
    score_text.setPosition ( {20.f, 16.f} );
    target.draw ( score_text );

    const float radius = 10.f;
    const float spacing = 26.f;
    const float base_x = 20.f + radius;
    const float base_y = static_cast<float> ( target.getSize().y ) - 30.f;

    const int total = snapshot.totalShots;
    const int remaining = snapshot.shotsRemaining;

    for ( int i = 0; i < total; ++i )
    {
        sf::CircleShape icon ( radius );
        icon.setOrigin ( {radius, radius} );
        icon.setPosition ( {base_x + i * spacing, base_y} );

        if ( i < remaining )
        {
            if ( i == 0 && snapshot.slingshot.canShoot )
            {
                icon.setFillColor ( projectile_color ( snapshot.slingshot.nextProjectile ) );
                icon.setOutlineColor ( projectile_outline ( snapshot.slingshot.nextProjectile ) );
                icon.setOutlineThickness ( 2.f );
            }
            else
            {
                icon.setFillColor ( sf::Color ( 70, 65, 60 ) );
                icon.setOutlineThickness ( 0.f );
            }
        }
        else
        {
            icon.setFillColor ( sf::Color::Transparent );
            icon.setOutlineColor ( sf::Color ( 100, 100, 100, 120 ) );
            icon.setOutlineThickness ( 2.f );
        }

        target.draw ( icon );
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
