// ============================================================
// world_config.hpp — Canonical world-space constants.
// Part of: angry::shared
//
// Defines global gameplay-space bounds in pixels:
//   * Visible world size shared by physics and renderer
//   * Ground top position used by level placement logic
//   * Boundary dimensions for off-screen containment bodies
//   * Aspect ratio helper for camera/view calculations
// ============================================================

#pragma once

namespace angry::world
{

// Canonical gameplay space in pixels. Physics, renderer and game camera
// must use the same values to avoid scale and bounds drift.
inline constexpr float kWidthPx = 1280.0f;
inline constexpr float kHeightPx = 720.0f;
inline constexpr float kGroundTopYPx = 600.0f;
inline constexpr float kGroundTopYpx = kGroundTopYPx;  // backward-compatible alias

inline constexpr float kAspect = kWidthPx / kHeightPx;
inline constexpr float kBoundaryThicknessPx = 20.0f;
inline constexpr float kBoundaryExtraWidthPx = 120.0f;
inline constexpr float kBoundaryHalfHeightPx = 900.0f;

}  // namespace angry::world
