#pragma once
#include <vector>

#include "types.hpp"

namespace angry
{

struct ObjectSnapshot
{
    EntityId id;
    enum class Kind : uint8_t
    {
        Block,
        Target,
        Projectile,
        Debris
    } kind;
    Vec2 positionPx;
    float angleDeg;
    Vec2 sizePx;     // width x height (rectangles)
    float radiusPx;  // for circles (0 if rectangle)
    Material material;
    ProjectileType projectileType = ProjectileType::Standard;
    float hpNormalized;  // 0.0 .. 1.0
    bool isActive;
};

struct SlingshotState
{
    Vec2 basePx;
    Vec2 pullOffsetPx;
    float maxPullPx;
    bool canShoot;
    ProjectileType nextProjectile;
};

struct WorldSnapshot
{
    std::vector<ObjectSnapshot> objects;
    std::vector<ProjectileType> projectileQueue;  // Front-first queue for HUD (current/loaded + next)
    SlingshotState slingshot;

    int score;
    int shotsRemaining;
    int totalShots;
    LevelStatus status;
    int stars;  // 0..3

    float physicsStepMs;  // for debug overlay
};

}  // namespace angry
