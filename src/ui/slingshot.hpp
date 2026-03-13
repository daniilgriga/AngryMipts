#pragma once
#include "platform/platform.hpp"
#include "shared/command.hpp"
#include "shared/world_snapshot.hpp"

#include <optional>
#include <vector>

namespace angry
{

class Slingshot
{
private:
    bool            dragging_ = false;
    platform::Vec2f drag_start_;
    platform::Vec2f drag_current_;

    float grab_radius_ = 40.f;

    std::vector<platform::Vec2f> calc_trajectory ( platform::Vec2f launch_vel,
                                                   platform::Vec2f start,
                                                   int num_points );

public:
    // Returns LaunchCmd if projectile was released
    std::optional<Command> handle_input ( const platform::Event& event,
                                          const SlingshotState& sling,
                                          const platform::Window& window,
                                          const platform::View& world_view );

    void render ( platform::RenderTarget& target, const SlingshotState& sling,
                  const platform::Texture& projectile_tex );
};

}  // namespace angry
