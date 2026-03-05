#pragma once
#include "shared/command.hpp"
#include "shared/world_snapshot.hpp"

#include <SFML/Graphics.hpp>

#include <optional>
#include <vector>

namespace angry
{

class Slingshot
{
private:
    bool dragging_ = false;
    sf::Vector2f drag_start_;
    sf::Vector2f drag_current_;

    float grab_radius_ = 40.f;

    std::vector<sf::Vector2f> calc_trajectory ( sf::Vector2f launch_vel,
                                                sf::Vector2f start,
                                                int num_points );

public:
    // Returns LaunchCmd if projectile was released
    std::optional<Command> handle_input ( const sf::Event& event,
                                          const SlingshotState& sling,
                                          const sf::RenderWindow& window,
                                          const sf::View& world_view );

    void render ( sf::RenderTarget& target, const SlingshotState& sling );
};

}  // namespace angry
