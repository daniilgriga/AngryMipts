#pragma once

#include "../shared/types.hpp"

namespace angry
{

struct WorldVec2
{
    float x = 0.0f;
    float y = 0.0f;
};

inline WorldVec2 pxToWorld(Vec2 px)
{
    return {px.x / PIXELS_PER_METER, px.y / PIXELS_PER_METER};
}

inline Vec2 worldToPx(WorldVec2 world)
{
    return {world.x * PIXELS_PER_METER, world.y * PIXELS_PER_METER};
}

}  // namespace angry
