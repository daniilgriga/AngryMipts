// ============================================================
// world_snapshot.hpp — Immutable frame-state snapshot models.
// Part of: angry::shared
//
// Defines render-facing state produced by physics/runtime:
//   * Per-object transforms, shape, hp, and activity flags
//   * Slingshot state and projectile queue for HUD
//   * Score, shots, stars, and level status counters
//   * Physics step timing used by debug overlay
// ============================================================

#pragma once
#include <vector>

#include "level_data.hpp"
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
    BlockShape shape = BlockShape::Rect;  // block shape for renderer (Rect/Circle/Triangle)
    bool isStatic = false;               // static blocks rendered as solid ground elements
    float hpNormalized;  // 0.0 .. 1.0
    bool isActive;
    std::vector<Vec2> triangleLocalVerticesPx;  // local vertices for triangle blocks (relative to center)
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
