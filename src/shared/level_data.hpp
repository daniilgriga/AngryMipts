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
    bool isStatic = false;         // completely immovable in physics
    bool isIndestructible = false; // ignores damage and never breaks
    float angleDeg;
    Material material;
    float hp;
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
    int star1Threshold;
    int star2Threshold;
    int star3Threshold;
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
