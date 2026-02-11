#include "render/renderer.hpp"

#include <cmath>

namespace angry
{

void Renderer::draw_snapshot ( sf::RenderWindow& window, const WorldSnapshot& snapshot )
{
    draw_slingshot ( window, snapshot.slingshot );

    for ( const auto& obj : snapshot.objects )
    {
        if ( obj.isActive )
            draw_object ( window, obj );
    }
}

void Renderer::draw_object ( sf::RenderWindow& window, const ObjectSnapshot& obj )
{
    if ( obj.radiusPx > 0.f )
    {
        sf::CircleShape shape ( obj.radiusPx );
        shape.setOrigin ( {obj.radiusPx, obj.radiusPx} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );
        shape.setFillColor ( kind_color ( obj.kind ) );

        if ( obj.kind == ObjectSnapshot::Kind::Block )
            shape.setFillColor ( material_color ( obj.material ) );

        window.draw ( shape );
    }
    else
    {
        sf::RectangleShape shape ( {obj.sizePx.x, obj.sizePx.y} );
        shape.setOrigin ( {obj.sizePx.x / 2.f, obj.sizePx.y / 2.f} );
        shape.setPosition ( {obj.positionPx.x, obj.positionPx.y} );
        shape.setRotation ( sf::degrees ( obj.angleDeg ) );
        shape.setFillColor ( material_color ( obj.material ) );

        if ( obj.kind == ObjectSnapshot::Kind::Target )
            shape.setFillColor ( kind_color ( obj.kind ) );
        else if ( obj.kind == ObjectSnapshot::Kind::Projectile )
            shape.setFillColor ( kind_color ( obj.kind ) );

        window.draw ( shape );
    }
}

void Renderer::draw_slingshot ( sf::RenderWindow& window, const SlingshotState& sling )
{
    // Base post
    sf::RectangleShape post ( {10.f, 60.f} );
    post.setOrigin ( {5.f, 60.f} );
    post.setPosition ( {sling.basePx.x, sling.basePx.y} );
    post.setFillColor ( sf::Color ( 101, 67, 33 ) );
    window.draw ( post );

    // Fork (two prongs)
    sf::RectangleShape left_prong ( {6.f, 25.f} );
    left_prong.setOrigin ( {3.f, 25.f} );
    left_prong.setPosition ( {sling.basePx.x - 8.f, sling.basePx.y - 55.f} );
    left_prong.setFillColor ( sf::Color ( 101, 67, 33 ) );
    window.draw ( left_prong );

    sf::RectangleShape right_prong ( {6.f, 25.f} );
    right_prong.setOrigin ( {3.f, 25.f} );
    right_prong.setPosition ( {sling.basePx.x + 8.f, sling.basePx.y - 55.f} );
    right_prong.setFillColor ( sf::Color ( 101, 67, 33 ) );
    window.draw ( right_prong );
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

}  // namespace angry
