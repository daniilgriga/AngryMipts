#include "physics_engine.hpp"

#include "physics_units.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <type_traits>
#include <utility>

namespace angry
{
namespace
{

constexpr float kDegreesPerRadian = 57.2957795f;
constexpr float kSlingshotForkYOffsetPx = 60.0f;

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

}  // namespace

PhysicsEngine::~PhysicsEngine()
{
    if (B2_IS_NON_NULL(worldId_))
    {
        b2DestroyWorld(worldId_);
        worldId_ = b2_nullWorldId;
    }
}

void PhysicsEngine::loadLevel(const LevelData& level)
{
    if (B2_IS_NON_NULL(worldId_))
    {
        b2DestroyWorld(worldId_);
        worldId_ = b2_nullWorldId;
    }

    currentLevel_ = level;
    levelLoaded_ = true;
    paused_ = false;

    nextId_ = 1;
    nextProjectileIndex_ = 0;
    activeProjectileBodyId_ = b2_nullBodyId;
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

    snapshot_.score = 0;
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

    if (B2_IS_NON_NULL(activeProjectileBodyId_))
    {
        const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
        const Vec2 projectilePosPx = worldToPx({worldPos.x, worldPos.y});

        if (isOutOfBoundsPx(projectilePosPx))
        {
            destroyBody(activeProjectileBodyId_);
            activeProjectileBodyId_ = b2_nullBodyId;
            tryPrepareNextProjectile();
        }
    }

    updateLevelStatus();
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
                if (concrete.levelId == currentLevel_.meta.id)
                {
                    loadLevel(currentLevel_);
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
                // v1 MVP: projectile abilities are not implemented yet.
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
    shapeDef.material.friction = 0.9f;
    shapeDef.material.restitution = 0.05f;

    const float halfWidthM = 1400.0f / PIXELS_PER_METER;
    const float halfHeightM = 20.0f / PIXELS_PER_METER;
    const float centerYpx = topYpx + (halfHeightM * PIXELS_PER_METER);
    const b2Vec2 centerM = b2Vec2{640.0f / PIXELS_PER_METER, centerYpx / PIXELS_PER_METER};
    const b2Polygon groundPolygon = b2MakeOffsetBox(halfWidthM, halfHeightM, centerM, b2MakeRot(0.0f));

    b2CreatePolygonShape(groundBodyId, &shapeDef, &groundPolygon);
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
    shapeDef.material.friction = 0.7f;
    shapeDef.material.restitution = 0.08f;

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
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.15f;

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
    if (type == ProjectileType::Heavy)
    {
        radiusPx = 14.0f;
        density = 1.7f;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.isBullet = true;
    const WorldVec2 spawnWorld = pxToWorld(spawnPx);
    bodyDef.position = b2Vec2{spawnWorld.x, spawnWorld.y};

    const b2BodyId bodyId = b2CreateBody(worldId_, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = density;
    shapeDef.material.friction = 0.4f;
    shapeDef.material.restitution = 0.2f;

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

    if (snapshot_.shotsRemaining == 0 && B2_IS_NULL(activeProjectileBodyId_))
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
}

void PhysicsEngine::tryPrepareNextProjectile()
{
    snapshot_.slingshot.canShoot = nextProjectileIndex_ < static_cast<int>(currentLevel_.projectiles.size());
    if (snapshot_.slingshot.canShoot)
    {
        snapshot_.slingshot.nextProjectile = currentLevel_.projectiles[nextProjectileIndex_].type;
        events_.push_back(ProjectileReadyEvent{snapshot_.slingshot.nextProjectile, snapshot_.shotsRemaining});
    }
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
