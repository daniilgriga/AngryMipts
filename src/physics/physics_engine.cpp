#include "physics_engine.hpp"

#include "physics_units.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace angry
{
namespace
{

constexpr float kDegreesPerRadian = 57.2957795f;
constexpr float kSlingshotForkYOffsetPx = 60.0f;
constexpr float kProjectileSleepLinearSpeedMps = 0.6f;
constexpr float kProjectileSleepAngularSpeedRad = 1.2f;
constexpr int kProjectileSettledFramesNeeded = 15;
constexpr float kProjectileSettledRemoveDelaySec = 1.5f;
constexpr float kDamageMinSpeedMps = 1.0f;
constexpr float kDamageScale = 16.0f;
constexpr float kBlockVsBlockDamageMultiplier = 0.15f;

inline float clampValue(float value, float minVal, float maxVal)
{
    return std::max(minVal, std::min(value, maxVal));
}

inline bool isOutOfBoundsPx(const Vec2& positionPx)
{
    return positionPx.x < -500.0f
        || positionPx.x > 4500.0f
        || positionPx.y > 3000.0f;
}

inline bool bodyIdEquals(b2BodyId a, b2BodyId b)
{
    return B2_ID_EQUALS(a, b);
}

inline Vec2 rotatePxVector(const Vec2& v, float angleRad)
{
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c,
    };
}

inline float materialDamageMultiplier(Material material)
{
    switch (material)
    {
        case Material::Wood:
            return 1.0f;
        case Material::Stone:
            return 0.65f;
        case Material::Glass:
            return 1.35f;
        case Material::Ice:
            return 1.2f;
    }

    return 1.0f;
}

inline bool isDestructibleKind(ObjectSnapshot::Kind kind)
{
    return kind == ObjectSnapshot::Kind::Block || kind == ObjectSnapshot::Kind::Target;
}

inline bool isBodyOnSurface(b2BodyId bodyId)
{
    const int contactCapacity = b2Body_GetContactCapacity(bodyId);
    if (contactCapacity <= 0)
    {
        return false;
    }

    std::vector<b2ContactData> contacts(static_cast<size_t>(contactCapacity));
    const int contactCount = b2Body_GetContactData(bodyId, contacts.data(), contactCapacity);
    for (int i = 0; i < contactCount; ++i)
    {
        if (contacts[static_cast<size_t>(i)].manifold.pointCount > 0)
        {
            const b2Vec2 n = contacts[static_cast<size_t>(i)].manifold.normal;
            if (std::abs(n.y) > 0.5f)
            {
                return true;
            }
        }
    }

    return false;
}

inline void applySurfaceDamping(b2BodyId bodyId, float linearFactor, float angularFactor)
{
    b2Vec2 linearVel = b2Body_GetLinearVelocity(bodyId);
    linearVel.x *= linearFactor;
    if (std::abs(linearVel.x) < 0.05f)
    {
        linearVel.x = 0.0f;
    }
    b2Body_SetLinearVelocity(bodyId, linearVel);

    float angularVel = b2Body_GetAngularVelocity(bodyId) * angularFactor;
    if (std::abs(angularVel) < 0.05f)
    {
        angularVel = 0.0f;
    }
    b2Body_SetAngularVelocity(bodyId, angularVel);
}

}  // namespace

PhysicsEngine::~PhysicsEngine()
{
    if (B2_IS_NON_NULL(worldId_))
    {
        b2DestroyWorld(worldId_);
        worldId_ = b2_nullWorldId;
    }
}

void PhysicsEngine::registerLevel(const LevelData& level)
{
    levelRegistry_[level.meta.id] = level;
}

void PhysicsEngine::loadLevel(const LevelData& level)
{
    if (B2_IS_NON_NULL(worldId_))
    {
        b2DestroyWorld(worldId_);
        worldId_ = b2_nullWorldId;
    }

    currentLevel_ = level;
    registerLevel(level);
    levelLoaded_ = true;
    paused_ = false;

    nextId_ = 1;
    nextProjectileIndex_ = 0;
    activeProjectileBodyId_ = b2_nullBodyId;
    activeProjectileSettledFrames_ = 0;
    activeProjectileSettledTimeSec_ = 0.0f;
    activeProjectileType_ = ProjectileType::Standard;
    activeProjectileAbilityUsed_ = false;
    levelYOffsetPx_ = 0.0f;
    supportBottomPx_ = 0.0f;
    events_.clear();
    pendingCommands_.clear();
    bodies_.clear();

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = b2Vec2{0.0f, 9.81f};
    worldId_ = b2CreateWorld(&worldDef);

    snapshot_.objects.clear();
    snapshot_.slingshot.basePx = level.slingshot.positionPx;
    snapshot_.slingshot.pullOffsetPx = {0.0f, 0.0f};
    snapshot_.slingshot.maxPullPx = level.slingshot.maxPullPx;
    snapshot_.slingshot.canShoot = !level.projectiles.empty();
    snapshot_.slingshot.nextProjectile = level.projectiles.empty()
        ? ProjectileType::Standard
        : level.projectiles.front().type;

    scoreSystem_.reset();
    snapshot_.score = scoreSystem_.score();
    snapshot_.shotsRemaining = static_cast<int>(level.projectiles.size());
    snapshot_.totalShots = level.meta.totalShots > 0
        ? level.meta.totalShots
        : static_cast<int>(level.projectiles.size());
    snapshot_.status = LevelStatus::Running;
    snapshot_.stars = 0;
    snapshot_.physicsStepMs = 0.0f;

    float supportBottomPx = 700.0f;
    for (const BlockData& block : level.blocks)
    {
        float bottomPx = block.positionPx.y;
        if (block.radiusPx > 0.0f)
        {
            bottomPx += block.radiusPx;
        }
        else
        {
            bottomPx += block.sizePx.y * 0.5f;
        }
        supportBottomPx = std::max(supportBottomPx, bottomPx);
    }

    supportBottomPx_ = supportBottomPx;
    groundTopYpx_ = 700.0f;
    levelYOffsetPx_ = groundTopYpx_ - supportBottomPx_;
    createGround(groundTopYpx_);

    for (const BlockData& block : level.blocks)
    {
        createBlockBody(block);
    }

    for (const TargetData& target : level.targets)
    {
        createTargetBody(target);
    }

    refreshSnapshot();
}

void PhysicsEngine::processCommands(ThreadSafeQueue<Command>& cmdQueue)
{
    while (const std::optional<Command> cmd = cmdQueue.try_pop())
    {
        pendingCommands_.push_back(*cmd);
    }
}

void PhysicsEngine::step(float dt)
{
    const auto start = std::chrono::steady_clock::now();
    const LevelStatus statusBeforeStep = snapshot_.status;

    if (!levelLoaded_ || B2_IS_NULL(worldId_))
    {
        snapshot_.physicsStepMs = 0.0f;
        return;
    }

    for (const Command& cmd : pendingCommands_)
    {
        applyCommand(cmd);
    }
    pendingCommands_.clear();

    if (paused_ || snapshot_.status != LevelStatus::Running)
    {
        snapshot_.physicsStepMs = 0.0f;
        return;
    }

    const float clampedDt = clampValue(dt, 0.0f, 1.0f / 30.0f);
    b2World_Step(worldId_, clampedDt, 4);

    // Apply damage from contact hit events and remove destroyed bodies.
    const b2ContactEvents contactEvents = b2World_GetContactEvents(worldId_);
    std::unordered_map<EntityId, float> pendingDamageById;
    for (int i = 0; i < contactEvents.hitCount; ++i)
    {
        const b2ContactHitEvent& hit = contactEvents.hitEvents[static_cast<size_t>(i)];
        const b2BodyId bodyA = b2Shape_GetBody(hit.shapeIdA);
        const b2BodyId bodyB = b2Shape_GetBody(hit.shapeIdB);
        BodyBinding* bindingA = findBinding(bodyA);
        BodyBinding* bindingB = findBinding(bodyB);
        if (bindingA == nullptr || bindingB == nullptr)
        {
            continue;
        }

        events_.push_back(CollisionEvent{
            bindingA->id,
            bindingB->id,
            hit.approachSpeed,
            worldToPx({hit.point.x, hit.point.y})});

        const float effectiveSpeed = std::max(0.0f, hit.approachSpeed - kDamageMinSpeedMps);
        if (effectiveSpeed <= 0.0f)
        {
            continue;
        }

        const float baseDamage = effectiveSpeed * kDamageScale;
        const bool aIsProjectile = bindingA->kind == ObjectSnapshot::Kind::Projectile;
        const bool bIsProjectile = bindingB->kind == ObjectSnapshot::Kind::Projectile;
        const bool aIsDestructible = isDestructibleKind(bindingA->kind);
        const bool bIsDestructible = isDestructibleKind(bindingB->kind);

        auto applyScaledDamage = [&pendingDamageById](const BodyBinding* binding, float damage)
        {
            const float scaledDamage = damage * materialDamageMultiplier(binding->material);
            pendingDamageById[binding->id] += scaledDamage;
        };

        if (aIsProjectile && bIsDestructible)
        {
            applyScaledDamage(bindingB, baseDamage);
            continue;
        }
        if (bIsProjectile && aIsDestructible)
        {
            applyScaledDamage(bindingA, baseDamage);
            continue;
        }
        if (aIsProjectile && bIsProjectile)
        {
            continue;
        }
        if (aIsDestructible && bIsDestructible)
        {
            const float structuralDamage = baseDamage * kBlockVsBlockDamageMultiplier;
            applyScaledDamage(bindingA, structuralDamage);
            applyScaledDamage(bindingB, structuralDamage);
        }
    }

    std::vector<EntityId> destroyedIds;
    for (BodyBinding& binding : bodies_)
    {
        if (binding.kind != ObjectSnapshot::Kind::Block && binding.kind != ObjectSnapshot::Kind::Target)
        {
            continue;
        }

        const auto damageIt = pendingDamageById.find(binding.id);
        if (damageIt == pendingDamageById.end())
        {
            continue;
        }

        binding.hp -= damageIt->second;
        if (binding.hp <= 0.0f)
        {
            destroyedIds.push_back(binding.id);
        }
    }

    for (const EntityId destroyedId : destroyedIds)
    {
        const auto it = std::find_if(
            bodies_.begin(),
            bodies_.end(),
            [destroyedId](const BodyBinding& b)
            {
                return b.id == destroyedId;
            });
        if (it == bodies_.end())
        {
            continue;
        }

        const bool isTarget = it->kind == ObjectSnapshot::Kind::Target;
        const int scoreAwarded = isTarget ? it->scoreValue : 0;
        const Vec2 eventPositionPx = it->bodyId.index1 != 0
            ? worldToPx({b2Body_GetPosition(it->bodyId).x, b2Body_GetPosition(it->bodyId).y})
            : it->lastPositionPx;

        events_.push_back(DestroyedEvent{
            it->id,
            eventPositionPx,
            it->material});

        if (isTarget)
        {
            scoreSystem_.add(scoreAwarded);
            snapshot_.score = scoreSystem_.score();
            events_.push_back(TargetHitEvent{it->id, scoreAwarded});
            events_.push_back(ScoreChangedEvent{snapshot_.score});
        }

        if (it->bodyId.index1 != 0)
        {
            if (bodyIdEquals(it->bodyId, activeProjectileBodyId_))
            {
                activeProjectileBodyId_ = b2_nullBodyId;
                activeProjectileSettledFrames_ = 0;
                activeProjectileSettledTimeSec_ = 0.0f;
                activeProjectileType_ = ProjectileType::Standard;
                activeProjectileAbilityUsed_ = false;
                tryPrepareNextProjectile();
            }
            destroyBody(it->bodyId);
        }

        bodies_.erase(it);
    }

    // Apply rolling slowdown to all dynamic gameplay bodies touching surfaces.
    for (const BodyBinding& binding : bodies_)
    {
        if (B2_IS_NULL(binding.bodyId) || !b2Body_IsValid(binding.bodyId))
        {
            continue;
        }
        if (binding.kind != ObjectSnapshot::Kind::Block
            && binding.kind != ObjectSnapshot::Kind::Target
            && binding.kind != ObjectSnapshot::Kind::Projectile)
        {
            continue;
        }
        if (!b2Body_IsAwake(binding.bodyId) || !isBodyOnSurface(binding.bodyId))
        {
            continue;
        }

        if (binding.kind == ObjectSnapshot::Kind::Projectile)
        {
            applySurfaceDamping(binding.bodyId, 0.97f, 0.97f);
        }
        else
        {
            applySurfaceDamping(binding.bodyId, 0.94f, 0.94f);
        }
    }

    // Cleanup for secondary projectiles (e.g. spawned by Splitter) that are not tracked as active.
    std::vector<EntityId> settledSecondaryProjectileIds;
    for (BodyBinding& binding : bodies_)
    {
        if (binding.kind != ObjectSnapshot::Kind::Projectile)
        {
            continue;
        }
        if (B2_IS_NULL(binding.bodyId) || !b2Body_IsValid(binding.bodyId))
        {
            continue;
        }
        if (bodyIdEquals(binding.bodyId, activeProjectileBodyId_))
        {
            continue;
        }

        const b2Vec2 worldPos = b2Body_GetPosition(binding.bodyId);
        const Vec2 projectilePosPx = worldToPx({worldPos.x, worldPos.y});
        const b2Vec2 linearVel = b2Body_GetLinearVelocity(binding.bodyId);
        const float angularVel = b2Body_GetAngularVelocity(binding.bodyId);
        const bool isAwake = b2Body_IsAwake(binding.bodyId);
        const float linearSpeed = std::sqrt(linearVel.x * linearVel.x + linearVel.y * linearVel.y);
        const bool settledNow = (!isAwake)
            || (linearSpeed < kProjectileSleepLinearSpeedMps
                && std::abs(angularVel) < kProjectileSleepAngularSpeedRad);

        if (settledNow)
        {
            binding.settledFrames += 1;
            binding.settledTimeSec += clampedDt;
        }
        else
        {
            binding.settledFrames = 0;
            binding.settledTimeSec = 0.0f;
        }

        if (isOutOfBoundsPx(projectilePosPx)
            || (binding.settledFrames >= kProjectileSettledFramesNeeded
                && binding.settledTimeSec >= kProjectileSettledRemoveDelaySec))
        {
            settledSecondaryProjectileIds.push_back(binding.id);
        }
    }

    for (const EntityId projectileId : settledSecondaryProjectileIds)
    {
        const auto it = std::find_if(
            bodies_.begin(),
            bodies_.end(),
            [projectileId](const BodyBinding& b)
            {
                return b.id == projectileId;
            });
        if (it == bodies_.end())
        {
            continue;
        }
        if (B2_IS_NON_NULL(it->bodyId) && b2Body_IsValid(it->bodyId))
        {
            destroyBody(it->bodyId);
        }
        bodies_.erase(it);
    }

    if (!settledSecondaryProjectileIds.empty()
        && B2_IS_NULL(activeProjectileBodyId_)
        && !hasAliveProjectiles())
    {
        tryPrepareNextProjectile();
    }

    if (B2_IS_NON_NULL(activeProjectileBodyId_))
    {
        if (!b2Body_IsValid(activeProjectileBodyId_))
        {
            activeProjectileBodyId_ = b2_nullBodyId;
            activeProjectileSettledFrames_ = 0;
            activeProjectileSettledTimeSec_ = 0.0f;
            activeProjectileType_ = ProjectileType::Standard;
            activeProjectileAbilityUsed_ = false;
            tryPrepareNextProjectile();
        }
        else
        {
            const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
            const Vec2 projectilePosPx = worldToPx({worldPos.x, worldPos.y});
            b2Vec2 linearVel = b2Body_GetLinearVelocity(activeProjectileBodyId_);
            float angularVel = b2Body_GetAngularVelocity(activeProjectileBodyId_);
            const bool isAwake = b2Body_IsAwake(activeProjectileBodyId_);

            linearVel = b2Body_GetLinearVelocity(activeProjectileBodyId_);
            angularVel = b2Body_GetAngularVelocity(activeProjectileBodyId_);
            const float linearSpeed = std::sqrt(linearVel.x * linearVel.x + linearVel.y * linearVel.y);
            const bool settledNow = (!isAwake)
                || (linearSpeed < kProjectileSleepLinearSpeedMps
                    && std::abs(angularVel) < kProjectileSleepAngularSpeedRad);

            if (settledNow)
            {
                activeProjectileSettledFrames_ += 1;
                activeProjectileSettledTimeSec_ += clampedDt;
            }
            else
            {
                activeProjectileSettledFrames_ = 0;
                activeProjectileSettledTimeSec_ = 0.0f;
            }

            if (isOutOfBoundsPx(projectilePosPx))
            {
                destroyBody(activeProjectileBodyId_);
                activeProjectileBodyId_ = b2_nullBodyId;
                activeProjectileSettledFrames_ = 0;
                activeProjectileSettledTimeSec_ = 0.0f;
                activeProjectileType_ = ProjectileType::Standard;
                activeProjectileAbilityUsed_ = false;
                tryPrepareNextProjectile();
            }
            else if (activeProjectileSettledFrames_ >= kProjectileSettledFramesNeeded
                && activeProjectileSettledTimeSec_ >= kProjectileSettledRemoveDelaySec)
            {
                destroyBody(activeProjectileBodyId_);
                activeProjectileBodyId_ = b2_nullBodyId;
                activeProjectileSettledFrames_ = 0;
                activeProjectileSettledTimeSec_ = 0.0f;
                activeProjectileType_ = ProjectileType::Standard;
                activeProjectileAbilityUsed_ = false;
                tryPrepareNextProjectile();
            }
        }
    }

    updateLevelStatus();
    if (statusBeforeStep != snapshot_.status
        && (snapshot_.status == LevelStatus::Win || snapshot_.status == LevelStatus::Lose))
    {
        int stars = 0;
        if (snapshot_.status == LevelStatus::Win)
        {
            stars = scoreSystem_.starsFor(
                currentLevel_.meta.star1Threshold,
                currentLevel_.meta.star2Threshold,
                currentLevel_.meta.star3Threshold);
        }

        snapshot_.stars = stars;
        events_.push_back(LevelCompletedEvent{
            snapshot_.status == LevelStatus::Win,
            snapshot_.score,
            stars});
    }
    refreshSnapshot();

    const auto end = std::chrono::steady_clock::now();
    snapshot_.physicsStepMs = std::chrono::duration<float, std::milli>(end - start).count();
}

WorldSnapshot PhysicsEngine::getSnapshot() const
{
    return snapshot_;
}

std::vector<Event> PhysicsEngine::drainEvents()
{
    std::vector<Event> out;
    out.swap(events_);
    return out;
}

void PhysicsEngine::applyCommand(const Command& cmd)
{
    std::visit(
        [this](const auto& concrete)
        {
            using T = std::decay_t<decltype(concrete)>;

            if constexpr (std::is_same_v<T, LoadLevelCmd>)
            {
                const auto it = levelRegistry_.find(concrete.levelId);
                if (it != levelRegistry_.end())
                {
                    loadLevel(it->second);
                }
            }
            else if constexpr (std::is_same_v<T, RestartCmd>)
            {
                if (concrete.levelId == currentLevel_.meta.id)
                {
                    loadLevel(currentLevel_);
                }
            }
            else if constexpr (std::is_same_v<T, PauseCmd>)
            {
                paused_ = concrete.paused;
            }
            else if constexpr (std::is_same_v<T, LaunchCmd>)
            {
                if (!levelLoaded_ || !snapshot_.slingshot.canShoot || snapshot_.shotsRemaining <= 0)
                {
                    return;
                }

                const Vec2 launchVelocityPx = computeLaunchVelocityPx(concrete.pullVectorPx);
                const Vec2 launchOriginPx = {
                    snapshot_.slingshot.basePx.x,
                    snapshot_.slingshot.basePx.y - kSlingshotForkYOffsetPx,
                };
                const b2BodyId projectileBodyId = createProjectileBody(
                    snapshot_.slingshot.nextProjectile,
                    launchOriginPx,
                    launchVelocityPx);

                if (B2_IS_NULL(projectileBodyId))
                {
                    return;
                }

                activeProjectileBodyId_ = projectileBodyId;
                activeProjectileSettledFrames_ = 0;
                activeProjectileSettledTimeSec_ = 0.0f;
                activeProjectileType_ = snapshot_.slingshot.nextProjectile;
                activeProjectileAbilityUsed_ = false;
                nextProjectileIndex_++;
                snapshot_.shotsRemaining = std::max(0, snapshot_.shotsRemaining - 1);
                snapshot_.slingshot.canShoot = false;
                snapshot_.slingshot.pullOffsetPx = {0.0f, 0.0f};

                if (nextProjectileIndex_ < static_cast<int>(currentLevel_.projectiles.size()))
                {
                    snapshot_.slingshot.nextProjectile = currentLevel_.projectiles[nextProjectileIndex_].type;
                }
            }
            else if constexpr (std::is_same_v<T, ActivateAbilityCmd>)
            {
                if (B2_IS_NULL(activeProjectileBodyId_) || !b2Body_IsValid(activeProjectileBodyId_))
                {
                    return;
                }

                BodyBinding* projectile = findBinding(activeProjectileBodyId_);
                if (projectile == nullptr)
                {
                    return;
                }

                if (concrete.projectileId != INVALID_ID && concrete.projectileId != projectile->id)
                {
                    return;
                }

                if (activeProjectileAbilityUsed_)
                {
                    return;
                }

                if (activeProjectileType_ == ProjectileType::Dasher)
                {
                    const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
                    const b2Vec2 velocity = b2Body_GetLinearVelocity(activeProjectileBodyId_);
                    const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
                    if (speed > 0.001f)
                    {
                        constexpr float kDasherBoostMultiplier = 1.35f;
                        b2Body_SetLinearVelocity(
                            activeProjectileBodyId_,
                            b2Vec2{velocity.x * kDasherBoostMultiplier, velocity.y * kDasherBoostMultiplier});
                        activeProjectileAbilityUsed_ = true;
                        events_.push_back(AbilityActivatedEvent{
                            projectile->id,
                            activeProjectileType_,
                            worldToPx({worldPos.x, worldPos.y})});
                    }
                }
                else if (activeProjectileType_ == ProjectileType::Splitter)
                {
                    const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
                    const b2Vec2 worldVel = b2Body_GetLinearVelocity(activeProjectileBodyId_);
                    const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
                    const Vec2 velPx = worldToPx({worldVel.x, worldVel.y});
                    const float speedPx = std::sqrt(velPx.x * velPx.x + velPx.y * velPx.y);

                    if (speedPx > 1.0f)
                    {
                        constexpr float kSplitAngleRad = 0.26f;  // ~15 deg
                        const Vec2 leftVelPx = rotatePxVector(velPx, -kSplitAngleRad);
                        const Vec2 rightVelPx = rotatePxVector(velPx, kSplitAngleRad);

                        // Spawn split projectiles exactly at the parent position.
                        createProjectileBody(ProjectileType::Splitter, posPx, leftVelPx);
                        createProjectileBody(ProjectileType::Splitter, posPx, rightVelPx);
                        activeProjectileAbilityUsed_ = true;
                        events_.push_back(AbilityActivatedEvent{
                            projectile->id,
                            activeProjectileType_,
                            posPx});
                    }
                }
                else if (activeProjectileType_ == ProjectileType::Dropper)
                {
                    const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
                    const b2Vec2 worldVel = b2Body_GetLinearVelocity(activeProjectileBodyId_);
                    const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
                    const Vec2 velPx = worldToPx({worldVel.x, worldVel.y});
                    const float speedPx = std::sqrt(velPx.x * velPx.x + velPx.y * velPx.y);

                    const float droppedDownSpeedPx = std::max(speedPx * 1.35f, 700.0f);
                    const Vec2 droppedVelPx = {
                        velPx.x * 0.15f,
                        droppedDownSpeedPx,
                    };
                    createProjectileBody(ProjectileType::Standard, posPx, droppedVelPx);

                    Vec2 boostedParentVelPx = velPx;
                    boostedParentVelPx.x *= 1.05f;
                    boostedParentVelPx.y -= std::max(speedPx * 0.4f, 260.0f);
                    const WorldVec2 boostedParentVelWorld = pxToWorld(boostedParentVelPx);
                    b2Body_SetLinearVelocity(
                        activeProjectileBodyId_,
                        b2Vec2{boostedParentVelWorld.x, boostedParentVelWorld.y});

                    activeProjectileAbilityUsed_ = true;
                    events_.push_back(AbilityActivatedEvent{
                        projectile->id,
                        activeProjectileType_,
                        posPx});
                }
                else if (activeProjectileType_ == ProjectileType::Boomerang)
                {
                    const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
                    const b2Vec2 worldVel = b2Body_GetLinearVelocity(activeProjectileBodyId_);
                    const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
                    const Vec2 velPx = worldToPx({worldVel.x, worldVel.y});
                    const float speedPx = std::sqrt(velPx.x * velPx.x + velPx.y * velPx.y);
                    const Vec2 aimPx = {
                        snapshot_.slingshot.basePx.x,
                        snapshot_.slingshot.basePx.y - 70.0f,
                    };
                    Vec2 toAimPx = {
                        aimPx.x - posPx.x,
                        aimPx.y - posPx.y,
                    };
                    const float toAimLen = std::sqrt(toAimPx.x * toAimPx.x + toAimPx.y * toAimPx.y);
                    if (toAimLen > 0.001f)
                    {
                        toAimPx.x /= toAimLen;
                        toAimPx.y /= toAimLen;
                    }
                    else
                    {
                        toAimPx = {-1.0f, -0.1f};
                    }

                    // Push boomerang direction lower to avoid overly horizontal return.
                    toAimPx.y += 0.35f;
                    const float biasedLen = std::sqrt(toAimPx.x * toAimPx.x + toAimPx.y * toAimPx.y);
                    if (biasedLen > 0.001f)
                    {
                        toAimPx.x /= biasedLen;
                        toAimPx.y /= biasedLen;
                    }

                    const float boomerangSpeedPx = std::max(speedPx * 1.35f, 650.0f);
                    const Vec2 redirectedVelPx = {
                        toAimPx.x * boomerangSpeedPx,
                        toAimPx.y * boomerangSpeedPx,
                    };
                    const WorldVec2 redirectedVelWorld = pxToWorld(redirectedVelPx);
                    b2Body_SetLinearVelocity(
                        activeProjectileBodyId_,
                        b2Vec2{redirectedVelWorld.x, redirectedVelWorld.y});

                    const float cross =
                        velPx.x * redirectedVelPx.y - velPx.y * redirectedVelPx.x;
                    const float spinSign = cross >= 0.0f ? 1.0f : -1.0f;
                    b2Body_SetAngularVelocity(activeProjectileBodyId_, 18.0f * spinSign);

                    activeProjectileAbilityUsed_ = true;
                    events_.push_back(AbilityActivatedEvent{
                        projectile->id,
                        activeProjectileType_,
                        posPx});
                }
                else if (activeProjectileType_ == ProjectileType::Bomber)
                {
                    constexpr float kExplosionRadiusPx = 90.0f;
                    constexpr float kExplosionMaxDamage = 60.0f;
                    constexpr float kExplosionImpulse = 5.0f;
                    constexpr float kExplosionCloseRangeLimiter = 0.85f;

                    const EntityId bomberId = projectile->id;
                    const b2BodyId bomberBodyId = activeProjectileBodyId_;
                    const b2Vec2 explosionCenterWorld = b2Body_GetPosition(bomberBodyId);
                    const Vec2 explosionCenterPx =
                        worldToPx({explosionCenterWorld.x, explosionCenterWorld.y});
                    const float explosionRadiusWorld = kExplosionRadiusPx / PIXELS_PER_METER;

                    std::vector<EntityId> destroyedByExplosionIds;
                    for (BodyBinding& candidate : bodies_)
                    {
                        if (B2_IS_NULL(candidate.bodyId) || !b2Body_IsValid(candidate.bodyId))
                        {
                            continue;
                        }
                        if (bodyIdEquals(candidate.bodyId, bomberBodyId))
                        {
                            continue;
                        }

                        const b2Vec2 candidatePos = b2Body_GetPosition(candidate.bodyId);
                        const float dx = candidatePos.x - explosionCenterWorld.x;
                        const float dy = candidatePos.y - explosionCenterWorld.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance > explosionRadiusWorld)
                        {
                            continue;
                        }

                        const float rawFalloff = 1.0f - (distance / explosionRadiusWorld);
                        const float falloff = clampValue(rawFalloff, 0.0f, kExplosionCloseRangeLimiter);
                        const float safeDistance = std::max(distance, 0.0001f);
                        const b2Vec2 dir = b2Vec2{dx / safeDistance, dy / safeDistance};
                        b2Body_ApplyLinearImpulseToCenter(
                            candidate.bodyId,
                            b2Vec2{
                                dir.x * kExplosionImpulse * falloff,
                                dir.y * kExplosionImpulse * falloff},
                            true);

                        if (candidate.kind == ObjectSnapshot::Kind::Block
                            || candidate.kind == ObjectSnapshot::Kind::Target)
                        {
                            const float damage =
                                kExplosionMaxDamage * falloff * materialDamageMultiplier(candidate.material);
                            candidate.hp -= damage;
                            if (candidate.hp <= 0.0f)
                            {
                                destroyedByExplosionIds.push_back(candidate.id);
                            }
                        }
                    }

                    if (B2_IS_NON_NULL(bomberBodyId) && b2Body_IsValid(bomberBodyId))
                    {
                        destroyBody(bomberBodyId);
                    }
                    const auto bomberIt = std::find_if(
                        bodies_.begin(),
                        bodies_.end(),
                        [bomberId](const BodyBinding& b)
                        {
                            return b.id == bomberId;
                        });
                    if (bomberIt != bodies_.end())
                    {
                        bodies_.erase(bomberIt);
                    }

                    for (const EntityId destroyedId : destroyedByExplosionIds)
                    {
                        const auto it = std::find_if(
                            bodies_.begin(),
                            bodies_.end(),
                            [destroyedId](const BodyBinding& b)
                            {
                                return b.id == destroyedId;
                            });
                        if (it == bodies_.end())
                        {
                            continue;
                        }

                        const bool isTarget = it->kind == ObjectSnapshot::Kind::Target;
                        const int scoreAwarded = isTarget ? it->scoreValue : 0;
                        const Vec2 eventPositionPx = it->bodyId.index1 != 0
                            ? worldToPx({b2Body_GetPosition(it->bodyId).x, b2Body_GetPosition(it->bodyId).y})
                            : it->lastPositionPx;

                        events_.push_back(DestroyedEvent{
                            it->id,
                            eventPositionPx,
                            it->material});

                        if (isTarget)
                        {
                            scoreSystem_.add(scoreAwarded);
                            snapshot_.score = scoreSystem_.score();
                            events_.push_back(TargetHitEvent{it->id, scoreAwarded});
                            events_.push_back(ScoreChangedEvent{snapshot_.score});
                        }

                        if (it->bodyId.index1 != 0)
                        {
                            destroyBody(it->bodyId);
                        }
                        bodies_.erase(it);
                    }

                    activeProjectileBodyId_ = b2_nullBodyId;
                    activeProjectileSettledFrames_ = 0;
                    activeProjectileSettledTimeSec_ = 0.0f;
                    activeProjectileType_ = ProjectileType::Standard;
                    activeProjectileAbilityUsed_ = false;
                    tryPrepareNextProjectile();

                    events_.push_back(AbilityActivatedEvent{
                        bomberId,
                        ProjectileType::Bomber,
                        explosionCenterPx});
                }
            }
        },
        cmd);
}

void PhysicsEngine::createGround(float topYpx)
{
    if (B2_IS_NULL(worldId_))
    {
        return;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_staticBody;
    bodyDef.position = b2Vec2{0.0f, 0.0f};

    const b2BodyId groundBodyId = b2CreateBody(worldId_, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.friction = 0.9f;
    shapeDef.restitution = 0.05f;

    const float halfWidthM = 1400.0f / PIXELS_PER_METER;
    const float halfHeightM = 20.0f / PIXELS_PER_METER;
    const float centerYpx = topYpx + (halfHeightM * PIXELS_PER_METER);
    const b2Vec2 centerM = b2Vec2{640.0f / PIXELS_PER_METER, centerYpx / PIXELS_PER_METER};
    const b2Polygon groundPolygon = b2MakeOffsetBox(halfWidthM, halfHeightM, centerM, 0.0f);

    b2CreatePolygonShape(groundBodyId, &shapeDef, &groundPolygon);

    // World bounds so projectile collides with screen borders instead of flying away.
    const float wallHalfWidthM = 20.0f / PIXELS_PER_METER;
    const float wallHalfHeightM = 900.0f / PIXELS_PER_METER;
    const float leftWallCenterXPx = -10.0f;
    const float rightWallCenterXPx = 1290.0f;
    const float wallCenterYPx = 360.0f;

    const b2Polygon leftWall = b2MakeOffsetBox(
        wallHalfWidthM,
        wallHalfHeightM,
        b2Vec2{leftWallCenterXPx / PIXELS_PER_METER, wallCenterYPx / PIXELS_PER_METER}, 0.0f);
    b2CreatePolygonShape(groundBodyId, &shapeDef, &leftWall);

    const b2Polygon rightWall = b2MakeOffsetBox(
        wallHalfWidthM,
        wallHalfHeightM,
        b2Vec2{rightWallCenterXPx / PIXELS_PER_METER, wallCenterYPx / PIXELS_PER_METER}, 0.0f);
    b2CreatePolygonShape(groundBodyId, &shapeDef, &rightWall);

    const float ceilingHalfHeightM = 20.0f / PIXELS_PER_METER;
    const float ceilingCenterYPx = -20.0f;
    const b2Polygon ceiling = b2MakeOffsetBox(
        halfWidthM,
        ceilingHalfHeightM,
        b2Vec2{640.0f / PIXELS_PER_METER, ceilingCenterYPx / PIXELS_PER_METER}, 0.0f);
    b2CreatePolygonShape(groundBodyId, &shapeDef, &ceiling);
}

void PhysicsEngine::createBlockBody(const BlockData& block)
{
    if (B2_IS_NULL(worldId_))
    {
        return;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    Vec2 adjustedPositionPx = block.positionPx;
    adjustedPositionPx.y += levelYOffsetPx_;

    const float originalBottomPx = block.radiusPx > 0.0f
        ? block.positionPx.y + block.radiusPx
        : block.positionPx.y + (block.sizePx.y * 0.5f);

    // Force support blocks ("legs") to start exactly on the floor level.
    if (std::abs(originalBottomPx - supportBottomPx_) < 0.5f)
    {
        const float halfHeightPx = block.radiusPx > 0.0f
            ? block.radiusPx
            : (block.sizePx.y * 0.5f);
        adjustedPositionPx.y = groundTopYpx_ - halfHeightPx;
    }
    const WorldVec2 blockPosWorld = pxToWorld(adjustedPositionPx);
    bodyDef.position = b2Vec2{blockPosWorld.x, blockPosWorld.y};
    bodyDef.rotation = b2MakeRot(block.angleDeg / kDegreesPerRadian);

    const b2BodyId bodyId = b2CreateBody(worldId_, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.friction = 0.7f;
    shapeDef.restitution = 0.08f;
    shapeDef.enableHitEvents = true;

    if (block.radiusPx > 0.0f)
    {
        b2Circle circle = {};
        circle.center = b2Vec2{0.0f, 0.0f};
        circle.radius = block.radiusPx / PIXELS_PER_METER;
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
    }
    else
    {
        const float halfWidthM = (block.sizePx.x * 0.5f) / PIXELS_PER_METER;
        const float halfHeightM = (block.sizePx.y * 0.5f) / PIXELS_PER_METER;
        const b2Polygon box = b2MakeBox(halfWidthM, halfHeightM);
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
    }

    BodyBinding binding;
    binding.id = nextId_++;
    binding.kind = ObjectSnapshot::Kind::Block;
    binding.bodyId = bodyId;
    binding.sizePx = block.sizePx;
    binding.radiusPx = block.radiusPx;
    binding.material = block.material;
    binding.hp = std::max(1.0f, block.hp);
    binding.maxHp = binding.hp;
    binding.lastPositionPx = adjustedPositionPx;
    binding.lastAngleDeg = block.angleDeg;
    bodies_.push_back(binding);
}

void PhysicsEngine::createTargetBody(const TargetData& target)
{
    if (B2_IS_NULL(worldId_))
    {
        return;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    Vec2 adjustedPositionPx = target.positionPx;
    adjustedPositionPx.y += levelYOffsetPx_;
    const WorldVec2 targetPosWorld = pxToWorld(adjustedPositionPx);
    bodyDef.position = b2Vec2{targetPosWorld.x, targetPosWorld.y};

    const b2BodyId bodyId = b2CreateBody(worldId_, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.friction = 0.5f;
    shapeDef.restitution = 0.15f;
    shapeDef.enableHitEvents = true;

    b2Circle circle = {};
    circle.center = b2Vec2{0.0f, 0.0f};
    circle.radius = target.radiusPx / PIXELS_PER_METER;
    b2CreateCircleShape(bodyId, &shapeDef, &circle);

    BodyBinding binding;
    binding.id = nextId_++;
    binding.kind = ObjectSnapshot::Kind::Target;
    binding.bodyId = bodyId;
    binding.sizePx = {target.radiusPx * 2.0f, target.radiusPx * 2.0f};
    binding.radiusPx = target.radiusPx;
    binding.material = Material::Wood;
    binding.hp = std::max(1.0f, target.hp);
    binding.maxHp = binding.hp;
    binding.scoreValue = target.scoreValue;
    binding.lastPositionPx = adjustedPositionPx;
    binding.lastAngleDeg = 0.0f;
    bodies_.push_back(binding);
}

b2BodyId PhysicsEngine::createProjectileBody(ProjectileType type, const Vec2& spawnPx, const Vec2& launchVelocityPx)
{
    if (B2_IS_NULL(worldId_))
    {
        return b2_nullBodyId;
    }

    float radiusPx = 12.0f;
    float density = 1.0f;
    if (type == ProjectileType::Dasher)
    {
        radiusPx = 14.0f;
        density = 1.7f;
    }
    else if (type == ProjectileType::Bomber)
    {
        radiusPx = 13.0f;
        density = 1.3f;
    }
    else if (type == ProjectileType::Dropper)
    {
        radiusPx = 13.0f;
        density = 1.15f;
    }
    else if (type == ProjectileType::Boomerang)
    {
        radiusPx = 12.0f;
        density = 0.95f;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.isBullet = true;
    bodyDef.linearDamping = 0.0f;
    bodyDef.angularDamping = 0.0f;
    const WorldVec2 spawnWorld = pxToWorld(spawnPx);
    bodyDef.position = b2Vec2{spawnWorld.x, spawnWorld.y};

    const b2BodyId bodyId = b2CreateBody(worldId_, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = density;
    shapeDef.friction = 0.35f;
    shapeDef.restitution = 0.05f;
    shapeDef.enableHitEvents = true;

    b2Circle circle = {};
    circle.center = b2Vec2{0.0f, 0.0f};
    circle.radius = radiusPx / PIXELS_PER_METER;
    b2CreateCircleShape(bodyId, &shapeDef, &circle);

    const WorldVec2 velocityWorld = pxToWorld(launchVelocityPx);
    b2Body_SetLinearVelocity(bodyId, b2Vec2{velocityWorld.x, velocityWorld.y});

    BodyBinding binding;
    binding.id = nextId_++;
    binding.kind = ObjectSnapshot::Kind::Projectile;
    binding.bodyId = bodyId;
    binding.sizePx = {radiusPx * 2.0f, radiusPx * 2.0f};
    binding.radiusPx = radiusPx;
    binding.material = Material::Stone;
    binding.projectileType = type;
    binding.hp = 1.0f;
    binding.maxHp = 1.0f;
    binding.lastPositionPx = spawnPx;
    binding.lastAngleDeg = 0.0f;
    bodies_.push_back(binding);

    return bodyId;
}

void PhysicsEngine::destroyBody(b2BodyId bodyId)
{
    if (B2_IS_NULL(worldId_) || B2_IS_NULL(bodyId))
    {
        return;
    }

    BodyBinding* binding = findBinding(bodyId);
    if (binding != nullptr)
    {
        const b2Vec2 worldPos = b2Body_GetPosition(bodyId);
        const b2Rot rot = b2Body_GetRotation(bodyId);
        binding->lastPositionPx = worldToPx({worldPos.x, worldPos.y});
        binding->lastAngleDeg = std::atan2(rot.s, rot.c) * kDegreesPerRadian;
        binding->bodyId = b2_nullBodyId;
    }

    b2DestroyBody(bodyId);
}

void PhysicsEngine::updateLevelStatus()
{
    bool hasAliveTargets = false;

    for (const BodyBinding& binding : bodies_)
    {
        if (binding.kind == ObjectSnapshot::Kind::Target && B2_IS_NON_NULL(binding.bodyId))
        {
            hasAliveTargets = true;
            break;
        }
    }

    if (!hasAliveTargets)
    {
        snapshot_.status = LevelStatus::Win;
        return;
    }

    if (snapshot_.shotsRemaining == 0 && !hasAliveProjectiles())
    {
        snapshot_.status = LevelStatus::Lose;
    }
    else
    {
        snapshot_.status = LevelStatus::Running;
    }
}

void PhysicsEngine::refreshSnapshot()
{
    snapshot_.objects.clear();
    snapshot_.objects.reserve(bodies_.size());

    for (BodyBinding& binding : bodies_)
    {
        ObjectSnapshot object{};
        object.id = binding.id;
        object.kind = binding.kind;
        object.sizePx = binding.sizePx;
        object.radiusPx = binding.radiusPx;
        object.material = binding.material;
        object.projectileType = binding.projectileType;
        object.hpNormalized = clampValue(binding.hp / std::max(1.0f, binding.maxHp), 0.0f, 1.0f);

        if (B2_IS_NON_NULL(binding.bodyId) && b2Body_IsValid(binding.bodyId))
        {
            const b2Vec2 worldPos = b2Body_GetPosition(binding.bodyId);
            const b2Rot rot = b2Body_GetRotation(binding.bodyId);
            object.positionPx = worldToPx({worldPos.x, worldPos.y});
            object.angleDeg = std::atan2(rot.s, rot.c) * kDegreesPerRadian;
            object.isActive = true;

            binding.lastPositionPx = object.positionPx;
            binding.lastAngleDeg = object.angleDeg;
        }
        else
        {
            object.positionPx = binding.lastPositionPx;
            object.angleDeg = binding.lastAngleDeg;
            object.isActive = false;
        }

        snapshot_.objects.push_back(object);
    }

    snapshot_.projectileQueue.clear();
    if (!currentLevel_.projectiles.empty())
    {
        const int totalProjectiles = static_cast<int>(currentLevel_.projectiles.size());
        const int nextIndex = std::clamp(nextProjectileIndex_, 0, totalProjectiles);

        // While projectile is flying, keep it at queue front for HUD continuity.
        if (!snapshot_.slingshot.canShoot && nextIndex > 0)
        {
            snapshot_.projectileQueue.push_back(currentLevel_.projectiles[nextIndex - 1].type);
        }

        for (int i = nextIndex; i < totalProjectiles; ++i)
        {
            snapshot_.projectileQueue.push_back(currentLevel_.projectiles[i].type);
        }
    }
}

void PhysicsEngine::tryPrepareNextProjectile()
{
    snapshot_.slingshot.canShoot =
        nextProjectileIndex_ < static_cast<int>(currentLevel_.projectiles.size())
        && !hasAliveProjectiles();
    if (snapshot_.slingshot.canShoot)
    {
        snapshot_.slingshot.nextProjectile = currentLevel_.projectiles[nextProjectileIndex_].type;
        events_.push_back(ProjectileReadyEvent{snapshot_.slingshot.nextProjectile, snapshot_.shotsRemaining});
    }
}

bool PhysicsEngine::hasAliveProjectiles() const
{
    for (const BodyBinding& binding : bodies_)
    {
        if (binding.kind != ObjectSnapshot::Kind::Projectile)
        {
            continue;
        }
        if (B2_IS_NON_NULL(binding.bodyId) && b2Body_IsValid(binding.bodyId))
        {
            return true;
        }
    }

    return false;
}

Vec2 PhysicsEngine::computeLaunchVelocityPx(const Vec2& pullVectorPx) const
{
    const float power = 4.5f;
    return {
        pullVectorPx.x * power,
        pullVectorPx.y * power,
    };
}

PhysicsEngine::BodyBinding* PhysicsEngine::findBinding(b2BodyId bodyId)
{
    for (BodyBinding& binding : bodies_)
    {
        if (B2_IS_NON_NULL(binding.bodyId) && bodyIdEquals(binding.bodyId, bodyId))
        {
            return &binding;
        }
    }
    return nullptr;
}

}  // namespace angry
