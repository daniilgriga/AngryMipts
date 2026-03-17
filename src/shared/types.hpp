// ============================================================
// types.hpp — Core shared scalar and enum types.
// Part of: angry::shared
//
// Declares low-level types used across all game modules:
//   * Entity identifiers and invalid-id sentinel
//   * Pixel-space 2D vector primitive
//   * Material, projectile, and level-status enums
//   * Global px-to-meter conversion constant
// ============================================================

#pragma once
#include <cstdint>

namespace angry
{

using EntityId = uint32_t;
inline constexpr EntityId kInvalidId = 0;
inline constexpr EntityId INVALID_ID = kInvalidId;  // backward-compatible alias

struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

// All coordinates in shared are in pixels (px).
// Physics (A) converts px <-> Box2D meters internally.
inline constexpr float kPixelsPerMeter = 50.f;
inline constexpr float PIXELS_PER_METER = kPixelsPerMeter;  // backward-compatible alias

enum class Material : uint8_t
{
    Wood,
    Stone,
    Glass,
    Ice,
};

enum class ProjectileType : uint8_t
{
    Standard,
    Heavy,
    Splitter,
    Dasher,
    Bomber,
    Dropper,
    Boomerang,
    Bubbler,
    Inflater,
};

enum class LevelStatus : uint8_t
{
    Running,
    Win,
    Lose,
};

}  // namespace angry
