#pragma once
#include <cstdint>

namespace angry
{

using EntityId = uint32_t;
constexpr EntityId INVALID_ID = 0;

struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

// All coordinates in shared are in pixels (px).
// Physics (A) converts px <-> Box2D meters internally.
constexpr float PIXELS_PER_METER = 50.f;

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
