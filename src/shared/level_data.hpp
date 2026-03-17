// ============================================================
// level_data.hpp — Serializable level content structures.
// Part of: angry::shared
//
// Declares immutable gameplay data loaded from level files:
//   * Block/target/projectile record layouts
//   * Slingshot and star-threshold metadata
//   * Shape-specific parameters (circle/triangle/rect)
//   * Top-level LevelData aggregate passed to systems
// ============================================================

#pragma once
#include <string>
#include <vector>

#include "types.hpp"

namespace angry
{

enum class BlockShape : uint8_t
{
    Rect,
    Circle,
    Triangle,
};

struct BlockData
{
    Vec2 positionPx;
    Vec2 sizePx;
    float radiusPx;  // >0 only for circle blocks
    BlockShape shape = BlockShape::Rect;
    bool isStatic = false;          // completely immovable in physics
    bool isIndestructible = false;  // ignores damage and never breaks
    float angleDeg;
    Material material;
    float hp;
    std::vector<Vec2> triangleLocalVerticesPx;  // local vertices for triangle blocks (relative to center)
};

struct TargetData
{
    Vec2 positionPx;
    float radiusPx;
    float hp;
    int scoreValue;
};

struct ProjectileData
{
    ProjectileType type;
};

struct SlingshotData
{
    Vec2 positionPx;
    float maxPullPx;
};

struct LevelMeta
{
    int id;
    std::string name;
    int totalShots;
    int star_1_threshold;
    int star_2_threshold;
    int star_3_threshold;
};

struct LevelData
{
    LevelMeta meta;
    SlingshotData slingshot;
    std::vector<BlockData> blocks;
    std::vector<TargetData> targets;
    std::vector<ProjectileData> projectiles;
};

}  // namespace angry
