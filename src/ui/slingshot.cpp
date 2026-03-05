#include "ui/slingshot.hpp"

#include <cmath>

namespace angry
{

std::optional<Command> Slingshot::handle_input ( const sf::Event& event,
                                                  const SlingshotState& sling,
                                                  const sf::RenderWindow& window,
                                                  const sf::View& world_view )
{
    sf::Vector2f base ( sling.basePx.x, sling.basePx.y - 60.f );

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

    return std::nullopt;
}

void Slingshot::render ( sf::RenderTarget& target, const SlingshotState& sling )
{
    if ( !dragging_ )
        return;

    sf::Vector2f base ( sling.basePx.x, sling.basePx.y - 60.f );

    sf::Vector2f left_prong ( base.x - 8.f, base.y - 15.f );
    sf::Vector2f right_prong ( base.x + 8.f, base.y - 15.f );

    sf::Color band_color ( 90, 50, 20 );

    sf::Vertex band_left[] = {
        {left_prong, band_color},
        {drag_current_, band_color},
    };
    target.draw ( band_left, 2, sf::PrimitiveType::Lines );

    sf::Vertex band_right[] = {
        {right_prong, band_color},
        {drag_current_, band_color},
    };
    target.draw ( band_right, 2, sf::PrimitiveType::Lines );

    sf::CircleShape ball ( 8.f );
    ball.setOrigin ( {8.f, 8.f} );
    ball.setPosition ( drag_current_ );
    ball.setFillColor ( sf::Color ( 50, 50, 50 ) );
    target.draw ( ball );

    // Trajectory preview
    sf::Vector2f pull = base - drag_current_;
    sf::Vector2f launch_vel = pull * 4.5f;
    auto points = calc_trajectory ( launch_vel, base, 60 );

    for ( const auto& pt : points )
    {
        sf::CircleShape dot ( 2.f );
        dot.setOrigin ( {2.f, 2.f} );
        dot.setPosition ( pt );
        dot.setFillColor ( sf::Color ( 255, 255, 255, 120 ) );
        target.draw ( dot );
    }
}

std::vector<sf::Vector2f> Slingshot::calc_trajectory ( sf::Vector2f launch_vel,
                                                        sf::Vector2f start,
                                                        int num_points )
{
    const float gravity = PIXELS_PER_METER * 9.81f;
    const float dt = 0.03f;

    std::vector<sf::Vector2f> points;
    sf::Vector2f pos = start;
    sf::Vector2f vel = launch_vel;

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
