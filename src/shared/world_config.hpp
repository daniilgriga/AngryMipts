#pragma once

namespace angry::world
{

// Canonical gameplay space in pixels. Physics, renderer and game camera
// must use the same values to avoid scale and bounds drift.
inline constexpr float kWidthPx = 1280.0f;
inline constexpr float kHeightPx = 720.0f;
inline constexpr float kGroundTopYpx = 600.0f;

inline constexpr float kAspect = kWidthPx / kHeightPx;
inline constexpr float kBoundaryThicknessPx = 20.0f;
inline constexpr float kBoundaryExtraWidthPx = 120.0f;
inline constexpr float kBoundaryHalfHeightPx = 900.0f;

}  // namespace angry::world
