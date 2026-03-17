// ============================================================
// slingshot.cpp — Slingshot interaction implementation.
// Part of: angry::ui
//
// Implements drag-to-launch mechanics and rendering:
//   * Converts pointer drag into launch pull vector
//   * Clamps pull by slingshot maximum distance
//   * Draws bands, projectile, and trajectory preview
//   * Emits LaunchCmd on release when shooting is allowed
// ============================================================

#include "ui/slingshot.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#endif

#include <cmath>

namespace angry
{

// #=# Input Handling #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

std::optional<Command> Slingshot::handle_input ( const platform::Event& event,
                                                  const SlingshotState& sling,
                                                  const platform::Window& window,
                                                  const platform::View& world_view )
{
#ifndef __EMSCRIPTEN__
    platform::Vec2f base ( sling.basePx.x, sling.basePx.y - 60.f );

    if ( const auto* press = event.getIf<sf::Event::MouseButtonPressed>() )
    {
        if ( press->button == sf::Mouse::Button::Left && sling.canShoot )
        {
            sf::Vector2f mouse = window.mapPixelToCoords (
                {press->position.x, press->position.y}, world_view );
            float dist = std::hypot ( mouse.x - base.x, mouse.y - base.y );

            if ( dist < grab_radius_ )
            {
                dragging_ = true;
                drag_start_ = base;
                drag_current_ = mouse;
            }
        }
    }

    if ( const auto* move = event.getIf<sf::Event::MouseMoved>() )
    {
        if ( dragging_ )
        {
            sf::Vector2f mouse = window.mapPixelToCoords (
                {move->position.x, move->position.y}, world_view );

            sf::Vector2f offset = mouse - drag_start_;
            float len = std::hypot ( offset.x, offset.y );

            if ( len > sling.maxPullPx )
            {
                offset = offset / len * sling.maxPullPx;
            }

            drag_current_ = drag_start_ + offset;
        }
    }

    if ( const auto* release = event.getIf<sf::Event::MouseButtonReleased>() )
    {
        if ( release->button == sf::Mouse::Button::Left && dragging_ )
        {
            dragging_ = false;

            sf::Vector2f pull = drag_start_ - drag_current_;

            if ( std::hypot ( pull.x, pull.y ) > 5.f )
            {
                return LaunchCmd{{pull.x, pull.y}};
            }
        }
    }

#else  // __EMSCRIPTEN__ — Raylib event variant

    platform::Vec2f base { sling.basePx.x, sling.basePx.y - 60.f };

    if ( const auto* btn = std::get_if<platform::MouseBtnEvent>( &event ) )
    {
        if ( btn->button != 0 )
        {
            return std::nullopt;
        }

        const platform::Vec2f mouse = window.mapPixelToCoords (
            { static_cast<int> ( btn->x ), static_cast<int> ( btn->y ) },
            world_view );

        if ( btn->pressed && sling.canShoot )
        {
            float dist = std::hypot ( mouse.x - base.x, mouse.y - base.y );
            if ( dist < grab_radius_ )
            {
                dragging_ = true;
                drag_start_ = base;
                drag_current_ = mouse;
            }
            return std::nullopt;
        }

        if ( !btn->pressed && dragging_ )
        {
            dragging_ = false;
            platform::Vec2f pull { drag_start_.x - drag_current_.x,
                                   drag_start_.y - drag_current_.y };
            if ( std::hypot ( pull.x, pull.y ) > 5.f )
            {
                return LaunchCmd{{pull.x, pull.y}};
            }
            return std::nullopt;
        }
    }

    if ( const auto* move = std::get_if<platform::MouseMoveEvent>( &event ) )
    {
        if ( dragging_ )
        {
            const platform::Vec2f mouse = window.mapPixelToCoords (
                { static_cast<int> ( move->x ), static_cast<int> ( move->y ) },
                world_view );
            platform::Vec2f offset { mouse.x - drag_start_.x, mouse.y - drag_start_.y };
            float len = std::hypot ( offset.x, offset.y );
            if ( len > sling.maxPullPx )
            {
                offset.x = offset.x / len * sling.maxPullPx;
                offset.y = offset.y / len * sling.maxPullPx;
            }
            drag_current_ = { drag_start_.x + offset.x, drag_start_.y + offset.y };
        }
    }

#endif

    return std::nullopt;
}

// #=# Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void Slingshot::render ( platform::RenderTarget& target, const SlingshotState& sling,
                         const platform::Texture& projectile_tex )
{
    if ( !sling.canShoot )
        return;

    platform::Vec2f base { sling.basePx.x, sling.basePx.y - 60.f };
    platform::Vec2f left_prong  { base.x - 8.f, base.y - 15.f };
    platform::Vec2f right_prong { base.x + 8.f, base.y - 15.f };

    const platform::Vec2f ball_pos = dragging_ ? drag_current_ : base;
    const float           ball_r   = 14.f;

    platform::Color band_color ( 90, 50, 20 );

#ifndef __EMSCRIPTEN__

    if ( dragging_ )
    {
        sf::Vertex band_left[]  = { {left_prong,  band_color}, {ball_pos, band_color} };
        sf::Vertex band_right[] = { {right_prong, band_color}, {ball_pos, band_color} };
        target.draw ( band_left,  2, sf::PrimitiveType::Lines );
        target.draw ( band_right, 2, sf::PrimitiveType::Lines );
    }
    else
    {
        sf::Vector2f sag ( 0.f, 6.f );
        sf::Vertex band_left[]  = { {left_prong,       band_color},
                                    {left_prong  + sag, band_color},
                                    {ball_pos,          band_color} };
        sf::Vertex band_right[] = { {right_prong,      band_color},
                                    {right_prong + sag, band_color},
                                    {ball_pos,          band_color} };
        target.draw ( band_left,  3, sf::PrimitiveType::LineStrip );
        target.draw ( band_right, 3, sf::PrimitiveType::LineStrip );
    }

    const auto  ts    = projectile_tex.getSize();
    const float scale = ball_r * 2.f / static_cast<float> ( std::max ( ts.x, ts.y ) );
    sf::Sprite  bird ( projectile_tex );
    bird.setOrigin ( { static_cast<float> ( ts.x ) * 0.5f,
                       static_cast<float> ( ts.y ) * 0.5f } );
    bird.setPosition ( ball_pos );
    bird.setScale ( { scale, scale } );
    target.draw ( bird );

    if ( dragging_ )
    {
        sf::Vector2f pull       = base - drag_current_;
        sf::Vector2f launch_vel = pull * 4.5f;
        auto         points     = calc_trajectory ( launch_vel, base, 60 );

        for ( const auto& pt : points )
        {
            sf::CircleShape dot ( 2.f );
            dot.setOrigin ( {2.f, 2.f} );
            dot.setPosition ( pt );
            dot.setFillColor ( sf::Color ( 255, 255, 255, 120 ) );
            target.draw ( dot );
        }
    }

#else  // __EMSCRIPTEN__ — use platform::draw so world_to_screen is applied

    // Bands — drawn via platform Vertex/Lines so the View transform is respected
    auto draw_band = [&]( platform::Vec2f a, platform::Vec2f b )
    {
        platform::Vertex verts[2] = { {a, band_color}, {b, band_color} };
        target.draw( verts, 2, sf::PrimitiveType::Lines );
    };

    if ( dragging_ )
    {
        draw_band( left_prong,  ball_pos );
        draw_band( right_prong, ball_pos );
    }
    else
    {
        platform::Vec2f sag { 0.f, 6.f };
        platform::Vertex bl[3] = { {left_prong,          band_color},
                                   {left_prong  + sag,   band_color},
                                   {ball_pos,             band_color} };
        platform::Vertex br[3] = { {right_prong,         band_color},
                                   {right_prong + sag,   band_color},
                                   {ball_pos,             band_color} };
        target.draw( bl, 3, sf::PrimitiveType::LineStrip );
        target.draw( br, 3, sf::PrimitiveType::LineStrip );
    }

    // Bird — Sprite so world_to_screen handles position
    {
        const auto  ts    = projectile_tex.getSize();
        const float sc    = ball_r * 2.f / static_cast<float>( std::max( ts.x, ts.y ) );
        platform::Sprite bird( projectile_tex );
        bird.setOrigin( { static_cast<float>( ts.x ) * 0.5f,
                          static_cast<float>( ts.y ) * 0.5f } );
        bird.setPosition( ball_pos );
        bird.setScale( { sc, sc } );
        target.draw( bird );
    }

    if ( dragging_ )
    {
        platform::Vec2f pull       { base.x - drag_current_.x, base.y - drag_current_.y };
        platform::Vec2f launch_vel { pull.x * 4.5f, pull.y * 4.5f };
        auto            points     = calc_trajectory( launch_vel, base, 60 );
        for ( const auto& pt : points )
        {
            platform::CircleShape dot( 2.f );
            dot.setOrigin( { 2.f, 2.f } );
            dot.setPosition( pt );
            dot.setFillColor( platform::Color( 255, 255, 255, 120 ) );
            target.draw( dot );
        }
    }

#endif
}

// #=# Trajectory Prediction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

std::vector<platform::Vec2f> Slingshot::calc_trajectory ( platform::Vec2f launch_vel,
                                                           platform::Vec2f start,
                                                           int num_points )
{
    const float gravity = PIXELS_PER_METER * 9.81f;
    const float dt = 0.03f;

    std::vector<platform::Vec2f> points;
    platform::Vec2f pos = start;
    platform::Vec2f vel = launch_vel;

    for ( int i = 0; i < num_points; ++i )
    {
        points.push_back ( pos );
        vel.y += gravity * dt;
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
    }

    return points;
}

}  // namespace angry
