// ============================================================
// event.hpp — Events emitted by physics and game logic.
// Part of: angry::shared
//
// Defines event payloads consumed by UI/render/audio layers:
//   * Collision and destruction notifications
//   * Score and target-hit updates
//   * Level completion and projectile readiness signals
//   * Unified Event variant for non-blocking draining
// ============================================================

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

enum class ImpactOutcome
{
    Blocked,
    BrokenCarryThrough,
    Grazed,
};

struct ImpactResolvedEvent
{
    EntityId projectileId;
    EntityId blockId;
    ImpactOutcome outcome;
    float speedBeforeMps;
    float speedAfterMps;
    Vec2 contactPointPx;
};

using Event = std::variant<CollisionEvent, DestroyedEvent, TargetHitEvent, ScoreChangedEvent,
                           LevelCompletedEvent, ProjectileReadyEvent, AbilityActivatedEvent,
                           ImpactResolvedEvent>;

}  // namespace angry
