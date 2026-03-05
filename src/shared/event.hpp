#pragma once
#include <variant>

#include "types.hpp"

namespace angry
{

struct CollisionEvent
{
    EntityId aId;
    EntityId bId;
    float impulse;
    Vec2 contactPointPx;
};

struct DestroyedEvent
{
    EntityId id;
    Vec2 positionPx;
    Material material;
};

struct TargetHitEvent
{
    EntityId id;
    int scoreAwarded;
};

struct ScoreChangedEvent
{
    int newScore;
};

struct LevelCompletedEvent
{
    bool win;
    int finalScore;
    int stars;
};

struct ProjectileReadyEvent
{
    ProjectileType type;
    int shotsRemaining;
};

struct AbilityActivatedEvent
{
    EntityId projectileId;
    ProjectileType projectileType;
    Vec2 positionPx;
};

using Event = std::variant<CollisionEvent, DestroyedEvent, TargetHitEvent, ScoreChangedEvent,
                           LevelCompletedEvent, ProjectileReadyEvent, AbilityActivatedEvent>;

}  // namespace angry
