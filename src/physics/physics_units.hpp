// ============================================================
// physics_units.hpp — Pixel/World unit conversion helpers.
// Part of: angry::physics
//
// Defines lightweight conversion API used by physics code:
//   * Represents Box2D-space coordinates via WorldVec2
//   * Converts screen pixels to Box2D world meters
//   * Converts Box2D world meters back to screen pixels
//   * Centralizes scaling through PIXELS_PER_METER constant
// ============================================================

#pragma once

#include "../shared/types.hpp"

namespace angry
{

// Box2D-facing vector type in world meters. Explicitly separate
// from pixel-space Vec2 to avoid mixing units in calculations.
struct WorldVec2
{
    float x = 0.0f;
    float y = 0.0f;
};

// Converts pixel coordinates into Box2D meters so simulation
// parameters stay physically consistent across resolutions.
inline WorldVec2 pxToWorld(Vec2 px)
{
    return {px.x / PIXELS_PER_METER, px.y / PIXELS_PER_METER};
}

// Converts Box2D world meters back into pixels for snapshot/UI.
inline Vec2 worldToPx(WorldVec2 world)
{
    return {world.x * PIXELS_PER_METER, world.y * PIXELS_PER_METER};
}

}  // namespace angry
