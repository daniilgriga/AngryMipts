#include "physics_engine.hpp"

#include "physics_units.hpp"
#include "shared/world_config.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <utility>

namespace angry
{
namespace
{

constexpr float kDegreesPerRadian = 57.2957795f;
constexpr float kRadiansPerDegree = 0.0174532925f;
constexpr float kSlingshotForkYOffsetPx = 60.0f;
constexpr float kProjectileSleepLinearSpeedMps = 0.6f;
constexpr float kProjectileSleepAngularSpeedRad = 1.2f;
constexpr int kProjectileSettledFramesNeeded = 15;
constexpr float kProjectileSettledRemoveDelaySec = 1.5f;
constexpr float kDamageMinSpeedMps = 1.0f;
constexpr float kCollisionEventMinSpeedMps = 0.7f;
constexpr float kDamageScale = 16.0f;
constexpr float kBlockVsBlockDamageMultiplier = 0.15f;
// Stage 0 baseline tuning for projectile impact carry-through patch.
constexpr float kBreakCarryMinSpeedMps = 2.2f;
constexpr float kBreakCarryRetainTangential = 0.80f;
constexpr float kBreakCarryRetainNormal = 0.35f;
constexpr float kBreakCarryImpulseBoostMps = 0.8f;
constexpr float kPostBreakDampingGraceSec = 0.12f;
constexpr float kBreakCarryMaxSpeedMps = 28.0f;
constexpr float kFloorImpactDamageMultiplier = 0.75f;
constexpr float kFloorContactMinNormalY = 0.75f;
constexpr float kFloorContactBandTopPx = 30.0f;
constexpr float kFloorContactBandBottomPx = 60.0f;
constexpr float kGroundTopYpx = world::kGroundTopYpx;
constexpr float kBubblerCaptureRadiusPx = 140.0f;
constexpr float kBubblerBubbleDurationSec = 1.10f;
constexpr float kBubblerLiftAccelMps2 = 18.0f;
constexpr float kBubblerLiftLinearDamping = 1.1f;
constexpr float kBubblerLiftAngularDamping = 2.0f;
constexpr float kBubblerBurstDownImpulseMps = 1.8f;
constexpr float kBubblerMaxUpwardSpeedMps = 3.8f;
constexpr float kBoomerangDelaySec = 0.132f;
constexpr float kBoomerangReturnDurationSec = 1.21f;
constexpr float kBoomerangMinReturnDistancePx = 132.0f;
constexpr float kBoomerangMainAccelMps2 = 28.6f;
constexpr float kBoomerangCurveAccelMps2 = 8.8f;
constexpr float kBoomerangMaxReturnSpeedMps = 12.1f;
constexpr float kBoomerangReturnLinearDamping = 0.605f;
constexpr float kBoomerangReturnGravityScale = 0.902f;
constexpr float kBoomerangReturnSpinRad = 17.6f;

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

inline std::uint64_t bodyIdKey(b2BodyId id)
{
    return (static_cast<std::uint64_t>(id.world0) << 48)
        ^ (static_cast<std::uint64_t>(id.revision) << 32)
        ^ static_cast<std::uint64_t>(id.index1);
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

inline float floorHitMaxDamageFraction(Material material)
{
    switch (material)
    {
        case Material::Wood:
            return 0.35f;
        case Material::Stone:
            return 0.22f;
        case Material::Glass:
            return 1.0f;
        case Material::Ice:
            return 1.0f;
    }

    return 1.0f;
}

inline bool isDestructibleKind(ObjectSnapshot::Kind kind)
{
    return kind == ObjectSnapshot::Kind::Block || kind == ObjectSnapshot::Kind::Target;
}

inline float projectileDamageMultiplier(ProjectileType projectileType)
{
    if (projectileType == ProjectileType::Heavy)
    {
        return 1.35f;
    }
    if (projectileType == ProjectileType::Boomerang)
    {
        return 0.77f;
    }

    return 1.0f;
}

inline int blockDestroyedScore(Material material)
{
    switch (material)
    {
        case Material::Glass:
            return 20;
        case Material::Wood:
            return 50;
        case Material::Stone:
            return 100;
        case Material::Ice:
            return 30;
    }

    return 20;
}

struct ProjectileImpactOutcome
{
    bool willBreak = false;
    float blockDamage = 0.0f;
    bool hasVelocityCorrection = false;
    b2Vec2 correctedProjectileVelocity = b2Vec2{0.0f, 0.0f};
};

inline ProjectileImpactOutcome resolveProjectileImpactOutcome(
    float baseDamage,
    float blockHp,
    Material blockMaterial,
    ProjectileType projectileType,
    b2Vec2 projectileVelocity,
    b2Vec2 projectileToBlockNormal)
{
    ProjectileImpactOutcome outcome;
    outcome.blockDamage = baseDamage
        * projectileDamageMultiplier(projectileType)
        * materialDamageMultiplier(blockMaterial);
    if (!std::isfinite(outcome.blockDamage) || outcome.blockDamage < 0.0f)
    {
        outcome.blockDamage = 0.0f;
    }
    outcome.willBreak = outcome.blockDamage >= std::max(0.0f, blockHp);

    if (outcome.willBreak)
    {
        const float speed = std::sqrt(
            projectileVelocity.x * projectileVelocity.x
            + projectileVelocity.y * projectileVelocity.y);
        if (std::isfinite(speed) && speed > 0.0001f)
        {
            // Build safe contact normal and decompose velocity.
            float nx = projectileToBlockNormal.x;
            float ny = projectileToBlockNormal.y;
            const float nLen = std::sqrt(nx * nx + ny * ny);
            if (nLen > 0.0001f)
            {
                nx /= nLen;
                ny /= nLen;
            }
            else
            {
                nx = projectileVelocity.x / speed;
                ny = projectileVelocity.y / speed;
            }

            const float vn = projectileVelocity.x * nx + projectileVelocity.y * ny;
            const b2Vec2 vNormal = b2Vec2{nx * vn, ny * vn};
            const b2Vec2 vTangential = b2Vec2{
                projectileVelocity.x - vNormal.x,
                projectileVelocity.y - vNormal.y};

            b2Vec2 correctedVelocity = b2Vec2{
                vTangential.x * kBreakCarryRetainTangential + vNormal.x * kBreakCarryRetainNormal,
                vTangential.y * kBreakCarryRetainTangential + vNormal.y * kBreakCarryRetainNormal};

            float correctedSpeed = std::sqrt(
                correctedVelocity.x * correctedVelocity.x
                + correctedVelocity.y * correctedVelocity.y);
            const b2Vec2 velocityDir = b2Vec2{
                projectileVelocity.x / speed,
                projectileVelocity.y / speed};

            if (correctedSpeed < kBreakCarryMinSpeedMps)
            {
                const float add = kBreakCarryMinSpeedMps - correctedSpeed;
                correctedVelocity.x += velocityDir.x * add;
                correctedVelocity.y += velocityDir.y * add;
                correctedSpeed = kBreakCarryMinSpeedMps;
            }

            correctedVelocity.x += velocityDir.x * kBreakCarryImpulseBoostMps;
            correctedVelocity.y += velocityDir.y * kBreakCarryImpulseBoostMps;

            correctedSpeed = std::sqrt(
                correctedVelocity.x * correctedVelocity.x
                + correctedVelocity.y * correctedVelocity.y);
            if (correctedSpeed > kBreakCarryMaxSpeedMps)
            {
                const float scale = kBreakCarryMaxSpeedMps / correctedSpeed;
                correctedVelocity.x *= scale;
                correctedVelocity.y *= scale;
            }

            if (std::isfinite(correctedVelocity.x) && std::isfinite(correctedVelocity.y))
            {
                outcome.hasVelocityCorrection = true;
                outcome.correctedProjectileVelocity = correctedVelocity;
            }
        }
    }

    return outcome;
}

inline float computeBottomOffsetPx(const BlockData& block)
{
    if (block.shape == BlockShape::Circle || block.radiusPx > 0.0f)
    {
        return block.radiusPx;
    }

    if (block.shape == BlockShape::Triangle && block.triangleLocalVerticesPx.size() == 3)
    {
        const float angleRad = block.angleDeg * kRadiansPerDegree;
        const float c = std::cos(angleRad);
        const float s = std::sin(angleRad);

        float maxY = -std::numeric_limits<float>::infinity();
        for (const Vec2& vertex : block.triangleLocalVerticesPx)
        {
            const float rotatedY = vertex.x * s + vertex.y * c;
            maxY = std::max(maxY, rotatedY);
        }
        return std::isfinite(maxY) ? maxY : (block.sizePx.y * 0.5f);
    }

    return block.sizePx.y * 0.5f;
}

struct StarThresholds
{
    int one = 1;
    int two = 2;
    int three = 3;
};

inline int sumTargetScore(const LevelData& level)
{
    int scoreSum = 0;
    for (const TargetData& target : level.targets)
    {
        scoreSum += std::max(0, target.scoreValue);
    }
    return scoreSum;
}

inline StarThresholds buildStarThresholds(const LevelData& level)
{
    StarThresholds thresholds;
    thresholds.one = std::max(1, sumTargetScore(level));
    thresholds.two = std::max(level.meta.star2Threshold, thresholds.one + 1);
    thresholds.three = std::max(level.meta.star3Threshold, thresholds.two + 1);
    return thresholds;
}

inline int calculateStarsForResult(
    const LevelData& level,
    const ScoreSystem& scoreSystem,
    LevelStatus status)
{
    const StarThresholds thresholds = buildStarThresholds(level);
    int stars = scoreSystem.starsFor(thresholds.one, thresholds.two, thresholds.three);

    if (status == LevelStatus::Win)
    {
        // A completed level must grant at least one star.
        stars = std::max(stars, 1);
    }

    return std::clamp(stars, 0, 3);
}

inline bool isBodyOnSurface(b2BodyId bodyId)
{
    if (b2Body_GetContactCapacity(bodyId) <= 0)
    {
        return false;
    }

    // Avoid per-frame heap allocations in hot physics loop.
    std::array<b2ContactData, 8> contacts{};
    const int contactCount = b2Body_GetContactData(
        bodyId,
        contacts.data(),
        static_cast<int>(contacts.size()));
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

    float supportBottomPx = kGroundTopYpx;
    for (const BlockData& block : level.blocks)
    {
        const float bottomPx = block.positionPx.y + computeBottomOffsetPx(block);
        supportBottomPx = std::max(supportBottomPx, bottomPx);
    }

    supportBottomPx_ = supportBottomPx;
    groundTopYpx_ = kGroundTopYpx;
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

    for (const Command& cmd : pendingCommands_)
    {
        applyCommand(cmd);
    }
    pendingCommands_.clear();

    if (!levelLoaded_ || B2_IS_NULL(worldId_))
    {
        snapshot_.physicsStepMs = 0.0f;
        return;
    }

    if (paused_ || snapshot_.status != LevelStatus::Running)
    {
        snapshot_.physicsStepMs = 0.0f;
        return;
    }

    const float clampedDt = clampValue(dt, 0.0f, 1.0f / 30.0f);

    for (BodyBinding& binding : bodies_)
    {
        if (binding.kind == ObjectSnapshot::Kind::Projectile && binding.postBreakDampingGraceSec > 0.0f)
        {
            binding.postBreakDampingGraceSec = std::max(
                0.0f,
                binding.postBreakDampingGraceSec - clampedDt);
        }

        if (!binding.isBubbled)
        {
            continue;
        }
        if (B2_IS_NULL(binding.bodyId) || !b2Body_IsValid(binding.bodyId))
        {
            binding.isBubbled = false;
            binding.bubbleTimeSec = 0.0f;
            continue;
        }
        if (binding.kind != ObjectSnapshot::Kind::Block
            && binding.kind != ObjectSnapshot::Kind::Target)
        {
            binding.isBubbled = false;
            binding.bubbleTimeSec = 0.0f;
            continue;
        }
        if (binding.isStatic || b2Body_GetType(binding.bodyId) != b2_dynamicBody)
        {
            binding.isBubbled = false;
            binding.bubbleTimeSec = 0.0f;
            continue;
        }

        const float mass = std::max(0.01f, b2Body_GetMass(binding.bodyId));
        b2Body_ApplyForceToCenter(
            binding.bodyId,
            b2Vec2{0.0f, -mass * kBubblerLiftAccelMps2},
            true);
        b2Vec2 bubbleVel = b2Body_GetLinearVelocity(binding.bodyId);
        if (bubbleVel.y < -kBubblerMaxUpwardSpeedMps)
        {
            bubbleVel.y = -kBubblerMaxUpwardSpeedMps;
            b2Body_SetLinearVelocity(binding.bodyId, bubbleVel);
        }
        b2Body_SetAwake(binding.bodyId, true);

        binding.bubbleTimeSec -= clampedDt;
        if (binding.bubbleTimeSec <= 0.0f)
        {
            binding.isBubbled = false;
            binding.bubbleTimeSec = 0.0f;
            b2Body_SetGravityScale(binding.bodyId, 1.0f);
            b2Body_SetLinearDamping(binding.bodyId, 0.0f);
            b2Body_SetAngularDamping(binding.bodyId, 0.0f);
            b2Body_ApplyLinearImpulseToCenter(
                binding.bodyId,
                b2Vec2{0.0f, mass * kBubblerBurstDownImpulseMps},
                true);
        }
    }

    for (BodyBinding& binding : bodies_)
    {
        if (binding.kind != ObjectSnapshot::Kind::Projectile
            || binding.projectileType != ProjectileType::Boomerang)
        {
            continue;
        }
        if (B2_IS_NULL(binding.bodyId) || !b2Body_IsValid(binding.bodyId))
        {
            binding.boomerangReturnRequested = false;
            binding.boomerangReturning = false;
            binding.boomerangTimeSinceLaunchSec = 0.0f;
            binding.boomerangReturnTimeSec = 0.0f;
            continue;
        }

        binding.boomerangTimeSinceLaunchSec += clampedDt;

        if (binding.boomerangReturnRequested && !binding.boomerangReturning)
        {
            const b2Vec2 worldPos = b2Body_GetPosition(binding.bodyId);
            const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
            const Vec2 toStartPx = {
                binding.boomerangStartPx.x - posPx.x,
                binding.boomerangStartPx.y - posPx.y,
            };
            const float distToStartPx =
                std::sqrt(toStartPx.x * toStartPx.x + toStartPx.y * toStartPx.y);

            if (binding.boomerangTimeSinceLaunchSec >= kBoomerangDelaySec
                && distToStartPx >= kBoomerangMinReturnDistancePx)
            {
                binding.boomerangReturning = true;
                binding.boomerangReturnTimeSec = 0.0f;
                b2Body_SetGravityScale(binding.bodyId, kBoomerangReturnGravityScale);
                b2Body_SetLinearDamping(binding.bodyId, kBoomerangReturnLinearDamping);
                b2Body_SetAngularDamping(binding.bodyId, 0.0f);
                b2Body_SetAwake(binding.bodyId, true);

                const float cross =
                    binding.boomerangLaunchDir.x * toStartPx.y
                    - binding.boomerangLaunchDir.y * toStartPx.x;
                binding.boomerangCurveSign = cross >= 0.0f ? 1.0f : -1.0f;
            }
        }

        if (!binding.boomerangReturning)
        {
            continue;
        }

        binding.boomerangReturnTimeSec += clampedDt;

        const b2Vec2 worldPos = b2Body_GetPosition(binding.bodyId);
        const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
        Vec2 toTargetPx = {
            binding.boomerangStartPx.x - posPx.x,
            binding.boomerangStartPx.y - posPx.y,
        };
        float lenPx = std::sqrt(toTargetPx.x * toTargetPx.x + toTargetPx.y * toTargetPx.y);
        if (lenPx < 0.001f)
        {
            lenPx = 0.001f;
            toTargetPx = {-1.0f, 0.0f};
        }

        const Vec2 dirToTarget = {
            toTargetPx.x / lenPx,
            toTargetPx.y / lenPx,
        };
        const Vec2 perpDir = {
            -dirToTarget.y * binding.boomerangCurveSign,
            dirToTarget.x * binding.boomerangCurveSign,
        };

        const float mass = std::max(0.01f, b2Body_GetMass(binding.bodyId));
        const b2Vec2 force = {
            mass * (dirToTarget.x * kBoomerangMainAccelMps2 + perpDir.x * kBoomerangCurveAccelMps2),
            mass * (dirToTarget.y * kBoomerangMainAccelMps2 + perpDir.y * kBoomerangCurveAccelMps2),
        };
        b2Body_ApplyForceToCenter(binding.bodyId, force, true);

        b2Vec2 velocity = b2Body_GetLinearVelocity(binding.bodyId);
        const float speedMps = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        if (speedMps > kBoomerangMaxReturnSpeedMps)
        {
            const float scale = kBoomerangMaxReturnSpeedMps / speedMps;
            velocity.x *= scale;
            velocity.y *= scale;
            b2Body_SetLinearVelocity(binding.bodyId, velocity);
        }

        b2Body_SetAngularVelocity(
            binding.bodyId,
            binding.boomerangCurveSign * kBoomerangReturnSpinRad);

        if (binding.boomerangReturnTimeSec >= kBoomerangReturnDurationSec)
        {
            binding.boomerangReturning = false;
            binding.boomerangReturnRequested = false;
            b2Body_SetGravityScale(binding.bodyId, 1.0f);
            b2Body_SetLinearDamping(binding.bodyId, 0.0f);
        }
    }

    b2World_Step(worldId_, clampedDt, 4);

    // Apply damage from contact hit events and remove destroyed bodies.
    const b2ContactEvents contactEvents = b2World_GetContactEvents(worldId_);
    std::unordered_map<std::uint64_t, BodyBinding*> bindingByBodyId;
    bindingByBodyId.reserve(bodies_.size());
    for (BodyBinding& binding : bodies_)
    {
        if (B2_IS_NON_NULL(binding.bodyId))
        {
            bindingByBodyId[bodyIdKey(binding.bodyId)] = &binding;
        }
    }

    std::unordered_map<EntityId, float> pendingDamageById;
    if (contactEvents.hitCount > 0)
    {
        pendingDamageById.reserve(static_cast<std::size_t>(contactEvents.hitCount));
    }
    struct PendingProjectileVelocityCorrection
    {
        b2BodyId projectileBodyId = b2_nullBodyId;
        b2Vec2 correctedVelocity = b2Vec2{0.0f, 0.0f};
        bool applyDampingGrace = false;
    };
    std::unordered_map<std::uint64_t, PendingProjectileVelocityCorrection> pendingProjectileCorrectionsByBody;
    if (contactEvents.hitCount > 0)
    {
        pendingProjectileCorrectionsByBody.reserve(static_cast<std::size_t>(contactEvents.hitCount));
    }
    for (int i = 0; i < contactEvents.hitCount; ++i)
    {
        const b2ContactHitEvent& hit = contactEvents.hitEvents[static_cast<size_t>(i)];
        const b2BodyId bodyA = b2Shape_GetBody(hit.shapeIdA);
        const b2BodyId bodyB = b2Shape_GetBody(hit.shapeIdB);
        BodyBinding* bindingA = nullptr;
        BodyBinding* bindingB = nullptr;

        const auto itA = bindingByBodyId.find(bodyIdKey(bodyA));
        if (itA != bindingByBodyId.end())
        {
            bindingA = itA->second;
        }
        const auto itB = bindingByBodyId.find(bodyIdKey(bodyB));
        if (itB != bindingByBodyId.end())
        {
            bindingB = itB->second;
        }

        const bool onlyAKnown = bindingA != nullptr && bindingB == nullptr;
        const bool onlyBKnown = bindingA == nullptr && bindingB != nullptr;
        if (onlyAKnown || onlyBKnown)
        {
            BodyBinding* dynamicBinding = onlyAKnown ? bindingA : bindingB;
            if (dynamicBinding == nullptr
                || !isDestructibleKind(dynamicBinding->kind)
                || !dynamicBinding->isDestructible
                || dynamicBinding->isStatic)
            {
                continue;
            }

            // Ground/wall/ceiling are static world geometry and have no binding.
            // Apply damage here only for impacts near the floor top.
            const Vec2 contactPointPx = worldToPx({hit.point.x, hit.point.y});
            const bool isNearFloor =
                contactPointPx.y >= (groundTopYpx_ - kFloorContactBandTopPx)
                && contactPointPx.y <= (groundTopYpx_ + kFloorContactBandBottomPx);
            const bool hasFloorLikeNormal = std::abs(hit.normal.y) >= kFloorContactMinNormalY;
            if (!isNearFloor || !hasFloorLikeNormal)
            {
                continue;
            }

            const float effectiveSpeed = std::max(0.0f, hit.approachSpeed - kDamageMinSpeedMps);
            if (effectiveSpeed <= 0.0f)
            {
                continue;
            }

            const float damage =
                effectiveSpeed * kDamageScale * kFloorImpactDamageMultiplier;
            const float scaledDamage = damage * materialDamageMultiplier(dynamicBinding->material);
            const float cappedDamage = std::min(
                scaledDamage,
                std::max(0.0f, dynamicBinding->hp) * floorHitMaxDamageFraction(dynamicBinding->material));
            if (cappedDamage <= 0.0f)
            {
                continue;
            }
            pendingDamageById[dynamicBinding->id] += cappedDamage;
            continue;
        }

        if (bindingA == nullptr || bindingB == nullptr)
        {
            continue;
        }

        if (hit.approachSpeed >= kCollisionEventMinSpeedMps)
        {
            events_.push_back(CollisionEvent{
                bindingA->id,
                bindingB->id,
                hit.approachSpeed,
                worldToPx({hit.point.x, hit.point.y})});
        }

        const float effectiveSpeed = std::max(0.0f, hit.approachSpeed - kDamageMinSpeedMps);
        if (effectiveSpeed <= 0.0f)
        {
            continue;
        }

        const float baseDamage = effectiveSpeed * kDamageScale;
        const bool aIsProjectile = bindingA->kind == ObjectSnapshot::Kind::Projectile;
        const bool bIsProjectile = bindingB->kind == ObjectSnapshot::Kind::Projectile;
        const bool aIsDestructible = isDestructibleKind(bindingA->kind) && bindingA->isDestructible;
        const bool bIsDestructible = isDestructibleKind(bindingB->kind) && bindingB->isDestructible;

        if (aIsProjectile && bIsDestructible)
        {
            const b2Vec2 projectileVelocity = b2Body_IsValid(bindingA->bodyId)
                ? b2Body_GetLinearVelocity(bindingA->bodyId)
                : b2Vec2{0.0f, 0.0f};
            const ProjectileImpactOutcome outcome = resolveProjectileImpactOutcome(
                baseDamage,
                bindingB->hp,
                bindingB->material,
                bindingA->projectileType,
                projectileVelocity,
                hit.normal);
            pendingDamageById[bindingB->id] += outcome.blockDamage;
            if (outcome.willBreak && outcome.hasVelocityCorrection)
            {
                const std::uint64_t key = bodyIdKey(bindingA->bodyId);
                PendingProjectileVelocityCorrection candidate{
                    bindingA->bodyId,
                    outcome.correctedProjectileVelocity,
                    true};
                const auto [it, inserted] = pendingProjectileCorrectionsByBody.emplace(key, candidate);
                if (!inserted)
                {
                    const float prevSpeed2 =
                        it->second.correctedVelocity.x * it->second.correctedVelocity.x
                        + it->second.correctedVelocity.y * it->second.correctedVelocity.y;
                    const float newSpeed2 =
                        candidate.correctedVelocity.x * candidate.correctedVelocity.x
                        + candidate.correctedVelocity.y * candidate.correctedVelocity.y;
                    if (newSpeed2 > prevSpeed2)
                    {
                        it->second = candidate;
                    }
                    else
                    {
                        it->second.applyDampingGrace = it->second.applyDampingGrace || candidate.applyDampingGrace;
                    }
                }
            }
            continue;
        }
        if (bIsProjectile && aIsDestructible)
        {
            const b2Vec2 projectileVelocity = b2Body_IsValid(bindingB->bodyId)
                ? b2Body_GetLinearVelocity(bindingB->bodyId)
                : b2Vec2{0.0f, 0.0f};
            const ProjectileImpactOutcome outcome = resolveProjectileImpactOutcome(
                baseDamage,
                bindingA->hp,
                bindingA->material,
                bindingB->projectileType,
                projectileVelocity,
                b2Vec2{-hit.normal.x, -hit.normal.y});
            pendingDamageById[bindingA->id] += outcome.blockDamage;
            if (outcome.willBreak && outcome.hasVelocityCorrection)
            {
                const std::uint64_t key = bodyIdKey(bindingB->bodyId);
                PendingProjectileVelocityCorrection candidate{
                    bindingB->bodyId,
                    outcome.correctedProjectileVelocity,
                    true};
                const auto [it, inserted] = pendingProjectileCorrectionsByBody.emplace(key, candidate);
                if (!inserted)
                {
                    const float prevSpeed2 =
                        it->second.correctedVelocity.x * it->second.correctedVelocity.x
                        + it->second.correctedVelocity.y * it->second.correctedVelocity.y;
                    const float newSpeed2 =
                        candidate.correctedVelocity.x * candidate.correctedVelocity.x
                        + candidate.correctedVelocity.y * candidate.correctedVelocity.y;
                    if (newSpeed2 > prevSpeed2)
                    {
                        it->second = candidate;
                    }
                    else
                    {
                        it->second.applyDampingGrace = it->second.applyDampingGrace || candidate.applyDampingGrace;
                    }
                }
            }
            continue;
        }
        if (aIsProjectile && bIsProjectile)
        {
            continue;
        }
        if (aIsDestructible && bIsDestructible)
        {
            const float structuralDamage = baseDamage * kBlockVsBlockDamageMultiplier;
            pendingDamageById[bindingA->id] += structuralDamage * materialDamageMultiplier(bindingA->material);
            pendingDamageById[bindingB->id] += structuralDamage * materialDamageMultiplier(bindingB->material);
        }
    }

    for (std::size_t i = 0; i < bodies_.size();)
    {
        BodyBinding& binding = bodies_[i];
        if (binding.kind != ObjectSnapshot::Kind::Block && binding.kind != ObjectSnapshot::Kind::Target)
        {
            ++i;
            continue;
        }
        if (!binding.isDestructible)
        {
            ++i;
            continue;
        }

        const auto damageIt = pendingDamageById.find(binding.id);
        if (damageIt == pendingDamageById.end())
        {
            ++i;
            continue;
        }

        binding.hp -= damageIt->second;
        if (binding.hp > 0.0f)
        {
            ++i;
            continue;
        }

        const bool isTarget = binding.kind == ObjectSnapshot::Kind::Target;
        const bool isBlock = binding.kind == ObjectSnapshot::Kind::Block;
        const int scoreAwarded = isTarget
            ? binding.scoreValue
            : (isBlock ? blockDestroyedScore(binding.material) : 0);
        const Vec2 eventPositionPx = binding.bodyId.index1 != 0
            ? worldToPx({b2Body_GetPosition(binding.bodyId).x, b2Body_GetPosition(binding.bodyId).y})
            : binding.lastPositionPx;

        events_.push_back(DestroyedEvent{
            binding.id,
            eventPositionPx,
            binding.material});

        if (scoreAwarded > 0)
        {
            scoreSystem_.add(scoreAwarded);
            snapshot_.score = scoreSystem_.score();
            events_.push_back(ScoreChangedEvent{snapshot_.score});

            if (isTarget)
            {
                events_.push_back(TargetHitEvent{binding.id, scoreAwarded});
            }
        }

        if (binding.bodyId.index1 != 0)
        {
            destroyBody(binding.bodyId);
        }

        if (i + 1 < bodies_.size())
        {
            bodies_[i] = std::move(bodies_.back());
        }
        bodies_.pop_back();
    }

    for (const auto& item : pendingProjectileCorrectionsByBody)
    {
        const PendingProjectileVelocityCorrection& correction = item.second;
        if (B2_IS_NON_NULL(correction.projectileBodyId) && b2Body_IsValid(correction.projectileBodyId))
        {
            b2Vec2 correctedVelocity = correction.correctedVelocity;
            const float correctedSpeed = std::sqrt(
                correctedVelocity.x * correctedVelocity.x
                + correctedVelocity.y * correctedVelocity.y);
            if (!std::isfinite(correctedSpeed))
            {
                continue;
            }
            if (correctedSpeed > kBreakCarryMaxSpeedMps && correctedSpeed > 0.0001f)
            {
                const float scale = kBreakCarryMaxSpeedMps / correctedSpeed;
                correctedVelocity.x *= scale;
                correctedVelocity.y *= scale;
            }
            b2Body_SetLinearVelocity(correction.projectileBodyId, correctedVelocity);

            if (correction.applyDampingGrace)
            {
                BodyBinding* projectile = findBinding(correction.projectileBodyId);
                if (projectile != nullptr && projectile->kind == ObjectSnapshot::Kind::Projectile)
                {
                    projectile->postBreakDampingGraceSec = std::max(
                        projectile->postBreakDampingGraceSec,
                        kPostBreakDampingGraceSec);
                }
            }
        }
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
        if (binding.kind != ObjectSnapshot::Kind::Projectile && binding.isStatic)
        {
            continue;
        }
        if (!b2Body_IsAwake(binding.bodyId))
        {
            continue;
        }

        const b2Vec2 linearVel = b2Body_GetLinearVelocity(binding.bodyId);
        if (std::abs(linearVel.y) > 1.2f)
        {
            continue;
        }

        if (!isBodyOnSurface(binding.bodyId))
        {
            continue;
        }

        if (binding.kind == ObjectSnapshot::Kind::Projectile)
        {
            if (binding.postBreakDampingGraceSec > 0.0f)
            {
                applySurfaceDamping(binding.bodyId, 0.992f, 0.992f);
            }
            else
            {
                applySurfaceDamping(binding.bodyId, 0.97f, 0.97f);
            }
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

    if (!settledSecondaryProjectileIds.empty())
    {
        const std::unordered_set<EntityId> settledIds(
            settledSecondaryProjectileIds.begin(), settledSecondaryProjectileIds.end());
        for (std::size_t i = 0; i < bodies_.size();)
        {
            if (settledIds.find(bodies_[i].id) == settledIds.end())
            {
                ++i;
                continue;
            }

            if (B2_IS_NON_NULL(bodies_[i].bodyId) && b2Body_IsValid(bodies_[i].bodyId))
            {
                destroyBody(bodies_[i].bodyId);
            }

            if (i + 1 < bodies_.size())
            {
                bodies_[i] = std::move(bodies_.back());
            }
            bodies_.pop_back();
        }
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
        const int stars = calculateStarsForResult(
            currentLevel_, scoreSystem_, snapshot_.status);

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
                        constexpr float kDasherBoostMultiplier = 1.86f;
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
                    const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});
                    projectile->boomerangReturnRequested = true;
                    projectile->boomerangReturning = false;
                    projectile->boomerangReturnTimeSec = 0.0f;
                    activeProjectileAbilityUsed_ = true;
                    events_.push_back(AbilityActivatedEvent{
                        projectile->id,
                        activeProjectileType_,
                        posPx});
                }
                else if (activeProjectileType_ == ProjectileType::Bubbler)
                {
                    const b2Vec2 worldPos = b2Body_GetPosition(activeProjectileBodyId_);
                    const Vec2 centerPx = worldToPx({worldPos.x, worldPos.y});
                    const float captureRadiusWorld = kBubblerCaptureRadiusPx / PIXELS_PER_METER;
                    bool capturedAny = false;

                    for (BodyBinding& candidate : bodies_)
                    {
                        if (B2_IS_NULL(candidate.bodyId) || !b2Body_IsValid(candidate.bodyId))
                        {
                            continue;
                        }
                        if (bodyIdEquals(candidate.bodyId, activeProjectileBodyId_))
                        {
                            continue;
                        }
                        if (candidate.kind != ObjectSnapshot::Kind::Block
                            && candidate.kind != ObjectSnapshot::Kind::Target)
                        {
                            continue;
                        }
                        if (candidate.isStatic || b2Body_GetType(candidate.bodyId) != b2_dynamicBody)
                        {
                            continue;
                        }

                        const b2Vec2 candidatePos = b2Body_GetPosition(candidate.bodyId);
                        const float dx = candidatePos.x - worldPos.x;
                        const float dy = candidatePos.y - worldPos.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance > captureRadiusWorld)
                        {
                            continue;
                        }

                        candidate.isBubbled = true;
                        candidate.bubbleTimeSec = kBubblerBubbleDurationSec;
                        b2Body_SetGravityScale(candidate.bodyId, 0.0f);
                        b2Body_SetLinearDamping(candidate.bodyId, kBubblerLiftLinearDamping);
                        b2Body_SetAngularDamping(candidate.bodyId, kBubblerLiftAngularDamping);

                        const float mass = std::max(0.01f, b2Body_GetMass(candidate.bodyId));
                        b2Body_ApplyLinearImpulseToCenter(
                            candidate.bodyId,
                            b2Vec2{0.0f, -mass * 1.2f},
                            true);
                        capturedAny = true;
                    }

                    activeProjectileAbilityUsed_ = true;
                    events_.push_back(AbilityActivatedEvent{
                        projectile->id,
                        activeProjectileType_,
                        centerPx});

                    // Keep gameplay deterministic: ability is consumed even if no body was captured.
                    (void)capturedAny;
                }
                else if (activeProjectileType_ == ProjectileType::Inflater)
                {
                    constexpr float kInflatedRadiusPx = 40.0f;
                    constexpr float kInflatedDensity = 1.5f;
                    constexpr float kInflateSpeedDamp = 0.90f;
                    constexpr float kInflatePullRadiusPx = 170.0f;
                    constexpr float kInflatePullImpulse = 0.72f;
                    constexpr float kInflateMinPullFactor = 0.20f;

                    const EntityId projectileId = projectile->id;
                    const b2BodyId oldBodyId = activeProjectileBodyId_;
                    const b2Vec2 worldPos = b2Body_GetPosition(oldBodyId);
                    const b2Vec2 worldVel = b2Body_GetLinearVelocity(oldBodyId);
                    const float angularVel = b2Body_GetAngularVelocity(oldBodyId);
                    const Vec2 posPx = worldToPx({worldPos.x, worldPos.y});

                    b2BodyDef bodyDef = b2DefaultBodyDef();
                    bodyDef.type = b2_dynamicBody;
                    bodyDef.isBullet = true;
                    bodyDef.linearDamping = 0.0f;
                    bodyDef.angularDamping = 0.0f;
                    bodyDef.position = worldPos;

                    const b2BodyId inflatedBodyId = b2CreateBody(worldId_, &bodyDef);
                    if (B2_IS_NULL(inflatedBodyId))
                    {
                        return;
                    }

                    b2ShapeDef shapeDef = b2DefaultShapeDef();
                    shapeDef.density = kInflatedDensity;
                    shapeDef.friction = 0.55f;
                    shapeDef.restitution = 0.0f;
                    shapeDef.enableHitEvents = true;

                    b2Circle circle = {};
                    circle.center = b2Vec2{0.0f, 0.0f};
                    circle.radius = kInflatedRadiusPx / PIXELS_PER_METER;
                    b2CreateCircleShape(inflatedBodyId, &shapeDef, &circle);

                    b2Body_SetLinearVelocity(
                        inflatedBodyId,
                        b2Vec2{
                            worldVel.x * kInflateSpeedDamp,
                            worldVel.y * kInflateSpeedDamp});
                    b2Body_SetAngularVelocity(inflatedBodyId, angularVel * 0.7f);

                    const float pullRadiusWorld = kInflatePullRadiusPx / PIXELS_PER_METER;
                    for (BodyBinding& candidate : bodies_)
                    {
                        if (B2_IS_NULL(candidate.bodyId) || !b2Body_IsValid(candidate.bodyId))
                        {
                            continue;
                        }
                        if (bodyIdEquals(candidate.bodyId, oldBodyId)
                            || bodyIdEquals(candidate.bodyId, inflatedBodyId))
                        {
                            continue;
                        }
                        if (candidate.kind != ObjectSnapshot::Kind::Block
                            && candidate.kind != ObjectSnapshot::Kind::Target)
                        {
                            continue;
                        }
                        if (candidate.isStatic || b2Body_GetType(candidate.bodyId) != b2_dynamicBody)
                        {
                            continue;
                        }

                        const b2Vec2 candidatePos = b2Body_GetPosition(candidate.bodyId);
                        const float dx = worldPos.x - candidatePos.x;
                        const float dy = worldPos.y - candidatePos.y;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance > pullRadiusWorld || distance < 0.0001f)
                        {
                            continue;
                        }

                        const float falloff = clampValue(
                            1.0f - (distance / pullRadiusWorld),
                            kInflateMinPullFactor,
                            1.0f);
                        const float mass = std::max(0.01f, b2Body_GetMass(candidate.bodyId));
                        const float invDistance = 1.0f / distance;
                        b2Body_ApplyLinearImpulseToCenter(
                            candidate.bodyId,
                            b2Vec2{
                                dx * invDistance * kInflatePullImpulse * falloff * mass,
                                dy * invDistance * kInflatePullImpulse * falloff * mass},
                            true);
                    }

                    if (B2_IS_NON_NULL(oldBodyId) && b2Body_IsValid(oldBodyId))
                    {
                        destroyBody(oldBodyId);
                    }
                    const auto oldIt = std::find_if(
                        bodies_.begin(),
                        bodies_.end(),
                        [oldBodyId](const BodyBinding& b)
                        {
                            return B2_IS_NON_NULL(b.bodyId) && bodyIdEquals(b.bodyId, oldBodyId);
                        });
                    if (oldIt != bodies_.end())
                    {
                        bodies_.erase(oldIt);
                    }

                    BodyBinding inflatedBinding;
                    inflatedBinding.id = projectileId;
                    inflatedBinding.kind = ObjectSnapshot::Kind::Projectile;
                    inflatedBinding.bodyId = inflatedBodyId;
                    inflatedBinding.sizePx = {kInflatedRadiusPx * 2.0f, kInflatedRadiusPx * 2.0f};
                    inflatedBinding.radiusPx = kInflatedRadiusPx;
                    inflatedBinding.material = Material::Stone;
                    inflatedBinding.projectileType = ProjectileType::Inflater;
                    inflatedBinding.hp = 1.0f;
                    inflatedBinding.maxHp = 1.0f;
                    inflatedBinding.lastPositionPx = posPx;
                    inflatedBinding.lastAngleDeg = 0.0f;
                    bodies_.push_back(inflatedBinding);

                    activeProjectileBodyId_ = inflatedBodyId;
                    activeProjectileType_ = ProjectileType::Inflater;
                    activeProjectileAbilityUsed_ = true;
                    events_.push_back(AbilityActivatedEvent{
                        projectileId,
                        activeProjectileType_,
                        posPx});
                }
                else if (activeProjectileType_ == ProjectileType::Bomber)
                {
                    constexpr float kExplosionRadiusPx = 124.2f;
                    constexpr float kExplosionMaxDamage = 60.0f;
                    constexpr float kExplosionImpulse = 6.5f;
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

                        if (candidate.isStatic && candidate.kind != ObjectSnapshot::Kind::Projectile)
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

                        if ((candidate.kind == ObjectSnapshot::Kind::Block
                            || candidate.kind == ObjectSnapshot::Kind::Target)
                            && candidate.isDestructible)
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
                    for (std::size_t i = 0; i < bodies_.size(); ++i)
                    {
                        if (bodies_[i].id != bomberId)
                        {
                            continue;
                        }

                        if (i + 1 < bodies_.size())
                        {
                            bodies_[i] = std::move(bodies_.back());
                        }
                        bodies_.pop_back();
                        break;
                    }

                    const std::unordered_set<EntityId> destroyedExplosionIds(
                        destroyedByExplosionIds.begin(), destroyedByExplosionIds.end());
                    for (std::size_t i = 0; i < bodies_.size();)
                    {
                        if (destroyedExplosionIds.find(bodies_[i].id)
                            == destroyedExplosionIds.end())
                        {
                            ++i;
                            continue;
                        }

                        BodyBinding& victim = bodies_[i];
                        const bool isTarget = victim.kind == ObjectSnapshot::Kind::Target;
                        const bool isBlock = victim.kind == ObjectSnapshot::Kind::Block;
                        const int scoreAwarded = isTarget
                            ? victim.scoreValue
                            : (isBlock ? blockDestroyedScore(victim.material) : 0);
                        const Vec2 eventPositionPx = victim.bodyId.index1 != 0
                            ? worldToPx({b2Body_GetPosition(victim.bodyId).x, b2Body_GetPosition(victim.bodyId).y})
                            : victim.lastPositionPx;

                        events_.push_back(DestroyedEvent{
                            victim.id,
                            eventPositionPx,
                            victim.material});

                        if (scoreAwarded > 0)
                        {
                            scoreSystem_.add(scoreAwarded);
                            snapshot_.score = scoreSystem_.score();
                            events_.push_back(ScoreChangedEvent{snapshot_.score});

                            if (isTarget)
                            {
                                events_.push_back(TargetHitEvent{victim.id, scoreAwarded});
                            }
                        }

                        if (victim.bodyId.index1 != 0)
                        {
                            destroyBody(victim.bodyId);
                        }

                        if (i + 1 < bodies_.size())
                        {
                            bodies_[i] = std::move(bodies_.back());
                        }
                        bodies_.pop_back();
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

    const float halfWidthM =
        (world::kWidthPx + world::kBoundaryExtraWidthPx) / PIXELS_PER_METER;
    const float halfHeightM = world::kBoundaryThicknessPx / PIXELS_PER_METER;
    const float centerYpx = topYpx + (halfHeightM * PIXELS_PER_METER);
    const b2Vec2 centerM =
        b2Vec2{(world::kWidthPx * 0.5f) / PIXELS_PER_METER, centerYpx / PIXELS_PER_METER};
    const b2Polygon groundPolygon = b2MakeOffsetBox(halfWidthM, halfHeightM, centerM, 0.0f);

    b2CreatePolygonShape(groundBodyId, &shapeDef, &groundPolygon);

    // World bounds so projectile collides with screen borders instead of flying away.
    const float wallHalfWidthM = world::kBoundaryThicknessPx / PIXELS_PER_METER;
    const float wallHalfHeightM = world::kBoundaryHalfHeightPx / PIXELS_PER_METER;
    const float leftWallCenterXPx = -world::kBoundaryThicknessPx * 0.5f;
    const float rightWallCenterXPx = world::kWidthPx + world::kBoundaryThicknessPx * 0.5f;
    const float wallCenterYPx = world::kHeightPx * 0.5f;

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

    const float ceilingHalfHeightM = world::kBoundaryThicknessPx / PIXELS_PER_METER;
    const float ceilingCenterYPx = -world::kBoundaryThicknessPx;
    const b2Polygon ceiling = b2MakeOffsetBox(
        halfWidthM,
        ceilingHalfHeightM,
        b2Vec2{(world::kWidthPx * 0.5f) / PIXELS_PER_METER,
               ceilingCenterYPx / PIXELS_PER_METER},
        0.0f);
    b2CreatePolygonShape(groundBodyId, &shapeDef, &ceiling);
}

void PhysicsEngine::createBlockBody(const BlockData& block)
{
    if (B2_IS_NULL(worldId_))
    {
        return;
    }

    const bool isCircle = block.shape == BlockShape::Circle || block.radiusPx > 0.0f;
    const bool isTriangle = block.shape == BlockShape::Triangle;

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = block.isStatic ? b2_staticBody : b2_dynamicBody;
    Vec2 adjustedPositionPx = block.positionPx;
    adjustedPositionPx.y += levelYOffsetPx_;

    const float bottomOffsetPx = computeBottomOffsetPx(block);
    const float originalBottomPx = block.positionPx.y + bottomOffsetPx;

    // Force support blocks ("legs") to start exactly on the floor level.
    if (std::abs(originalBottomPx - supportBottomPx_) < 0.5f)
    {
        adjustedPositionPx.y = groundTopYpx_ - bottomOffsetPx;
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

    if (isCircle)
    {
        b2Circle circle = {};
        circle.center = b2Vec2{0.0f, 0.0f};
        circle.radius = block.radiusPx / PIXELS_PER_METER;
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
    }
    else if (isTriangle)
    {
        bool createdFromVertices = false;
        if (block.triangleLocalVerticesPx.size() == 3)
        {
            b2Vec2 vertices[3] = {};
            for (std::size_t i = 0; i < 3; ++i)
            {
                vertices[i] = b2Vec2{
                    block.triangleLocalVerticesPx[i].x / PIXELS_PER_METER,
                    block.triangleLocalVerticesPx[i].y / PIXELS_PER_METER};
            }

            const b2Hull hull = b2ComputeHull(vertices, 3);
            if (hull.count == 3 && b2ValidateHull(&hull))
            {
                const b2Polygon triangle = b2MakePolygon(&hull, 0.0f);
                b2CreatePolygonShape(bodyId, &shapeDef, &triangle);
                createdFromVertices = true;
            }
        }

        if (!createdFromVertices)
        {
            const float halfWidthM = (block.sizePx.x * 0.5f) / PIXELS_PER_METER;
            const float halfHeightM = (block.sizePx.y * 0.5f) / PIXELS_PER_METER;
            const b2Vec2 vertices[3] = {
                b2Vec2{-halfWidthM, halfHeightM},
                b2Vec2{halfWidthM, halfHeightM},
                b2Vec2{0.0f, -halfHeightM},
            };

            const b2Hull hull = b2ComputeHull(vertices, 3);
            if (hull.count == 3 && b2ValidateHull(&hull))
            {
                const b2Polygon triangle = b2MakePolygon(&hull, 0.0f);
                b2CreatePolygonShape(bodyId, &shapeDef, &triangle);
            }
            else
            {
                const b2Polygon fallbackBox = b2MakeBox(halfWidthM, halfHeightM);
                b2CreatePolygonShape(bodyId, &shapeDef, &fallbackBox);
            }
        }
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
    binding.shape = block.shape;
    binding.triangleLocalVerticesPx = block.triangleLocalVerticesPx;
    binding.hp = std::max(1.0f, block.hp);
    binding.maxHp = binding.hp;
    binding.isStatic = block.isStatic;
    binding.isDestructible = !block.isIndestructible && !block.isStatic;
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
    if (type == ProjectileType::Heavy)
    {
        radiusPx = 15.0f;
        density = 2.2f;
    }
    else if (type == ProjectileType::Dasher)
    {
        radiusPx = 14.0f;
        density = 1.15f;
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
    shapeDef.restitution = 0.08f;
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
    if (type == ProjectileType::Boomerang)
    {
        binding.boomerangStartPx = spawnPx;
        const float speedPx = std::sqrt(
            launchVelocityPx.x * launchVelocityPx.x + launchVelocityPx.y * launchVelocityPx.y);
        if (speedPx > 0.001f)
        {
            binding.boomerangLaunchDir = {
                launchVelocityPx.x / speedPx,
                launchVelocityPx.y / speedPx,
            };
        }
        else
        {
            binding.boomerangLaunchDir = {1.0f, 0.0f};
        }
        binding.boomerangTimeSinceLaunchSec = 0.0f;
        binding.boomerangReturnTimeSec = 0.0f;
        binding.boomerangCurveSign = 1.0f;
        binding.boomerangReturnRequested = false;
        binding.boomerangReturning = false;
    }
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
        // Invariant: level can be won only when all targets are destroyed.
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
        object.shape = binding.shape;
        object.triangleLocalVerticesPx = binding.triangleLocalVerticesPx;
        object.isStatic = binding.isStatic;
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
