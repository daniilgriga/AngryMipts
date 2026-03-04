#pragma once

#include "../core/score_system.hpp"
#include "../shared/command.hpp"
#include "../shared/event.hpp"
#include "../shared/level_data.hpp"
#include "../shared/thread_safe_queue.hpp"
#include "../shared/world_snapshot.hpp"

#include <box2d/id.h>

#include <unordered_map>
#include <vector>

namespace angry
{

class PhysicsEngine
{
public:
    ~PhysicsEngine();

    void registerLevel(const LevelData& level);
    void loadLevel(const LevelData& level);
    void step(float dt);
    void processCommands(ThreadSafeQueue<Command>& cmdQueue);

    WorldSnapshot getSnapshot() const;
    std::vector<Event> drainEvents();

private:
    struct BodyBinding
    {
        EntityId id = INVALID_ID;
        ObjectSnapshot::Kind kind = ObjectSnapshot::Kind::Block;
        b2BodyId bodyId = b2_nullBodyId;
        Vec2 sizePx{};
        float radiusPx = 0.0f;
        Material material = Material::Wood;
        float hp = 1.0f;
        float maxHp = 1.0f;
        int scoreValue = 0;
        Vec2 lastPositionPx{};
        float lastAngleDeg = 0.0f;
        int settledFrames = 0;
        float settledTimeSec = 0.0f;
    };

    void applyCommand(const Command& cmd);
    void createGround(float topYpx);
    void createBlockBody(const BlockData& block);
    void createTargetBody(const TargetData& target);
    b2BodyId createProjectileBody(ProjectileType type, const Vec2& spawnPx, const Vec2& launchVelocityPx);
    void destroyBody(b2BodyId bodyId);
    void updateLevelStatus();
    void refreshSnapshot();
    void tryPrepareNextProjectile();
    Vec2 computeLaunchVelocityPx(const Vec2& pullVectorPx) const;
    BodyBinding* findBinding(b2BodyId bodyId);

    EntityId nextId_ = 1;
    WorldSnapshot snapshot_{};
    b2WorldId worldId_ = b2_nullWorldId;
    std::vector<BodyBinding> bodies_;
    std::vector<Event> events_;
    std::vector<Command> pendingCommands_;
    ScoreSystem scoreSystem_;
    std::unordered_map<int, LevelData> levelRegistry_;

    LevelData currentLevel_{};
    bool levelLoaded_ = false;
    bool paused_ = false;
    float levelYOffsetPx_ = 0.0f;
    float supportBottomPx_ = 0.0f;
    float groundTopYpx_ = 700.0f;

    int nextProjectileIndex_ = 0;
    b2BodyId activeProjectileBodyId_ = b2_nullBodyId;
    int activeProjectileSettledFrames_ = 0;
    float activeProjectileSettledTimeSec_ = 0.0f;
    ProjectileType activeProjectileType_ = ProjectileType::Standard;
    bool activeProjectileAbilityUsed_ = false;
};

}  // namespace angry
