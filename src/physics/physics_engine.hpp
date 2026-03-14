// ============================================================
// physics_engine.hpp — Public interface for the Box2D physics
//                      simulation engine.
// Part of: angry::physics
//
// PhysicsEngine owns the Box2D world and all body bindings
// for a single level. It:
//   * Accepts Commands (load, launch, ability, pause, restart)
//   * Steps the Box2D world and resolves contact damage
//   * Emits Events (collision, destroy, score, level complete)
//   * Exposes an immutable WorldSnapshot each frame
// ============================================================

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

// Wraps a Box2D world for one level: creates bodies from level
// data, steps the simulation, tracks damage and scoring, and
// produces WorldSnapshot for thread-safe rendering.
class PhysicsEngine
{
public:
    // #=# Lifecycle & Simulation API #=#=#=#=#=#=#=#=#=#=#=#=#=#

    ~PhysicsEngine();

    void register_level(const LevelData& level);
    void load_level(const LevelData& level);
    void step(float dt);
    void process_commands(ThreadSafeQueue<Command>& cmdQueue);

    WorldSnapshot get_snapshot() const;
    std::vector<Event> drain_events();

private:
    // #=# Internal State Types #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

    struct BodyBinding
    {
        EntityId id = INVALID_ID;
        ObjectSnapshot::Kind kind = ObjectSnapshot::Kind::Block;
        b2BodyId bodyId = b2_nullBodyId;
        Vec2 sizePx{};
        float radiusPx = 0.0f;
        Material material = Material::Wood;
        BlockShape shape = BlockShape::Rect;
        std::vector<Vec2> triangleLocalVerticesPx;
        float hp = 1.0f;
        float maxHp = 1.0f;
        bool isStatic = false;
        bool isDestructible = true;
        bool isBubbled = false;
        float bubbleTimeSec = 0.0f;
        int scoreValue = 0;
        Vec2 lastPositionPx{};
        float lastAngleDeg = 0.0f;
        ProjectileType projectileType = ProjectileType::Standard;
        float postBreakDampingGraceSec = 0.0f;
        int settledFrames = 0;
        float settledTimeSec = 0.0f;
        Vec2 boomerangStartPx{};
        Vec2 boomerangLaunchDir{};
        float boomerangTimeSinceLaunchSec = 0.0f;
        float boomerangReturnTimeSec = 0.0f;
        float boomerangCurveSign = 1.0f;
        bool boomerangReturnRequested = false;
        bool boomerangReturning = false;
    };

    // #=# Internal Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

    void apply_command(const Command& cmd);
    void create_ground(float topYpx);
    void create_block_body(const BlockData& block);
    void create_target_body(const TargetData& target);
    b2BodyId create_projectile_body(ProjectileType type, const Vec2& spawnPx, const Vec2& launchVelocityPx);
    void destroy_body(b2BodyId bodyId);
    void update_level_status();
    void refresh_snapshot();
    void try_prepare_next_projectile();
    bool has_alive_projectiles() const;
    Vec2 compute_launch_velocity_px(const Vec2& pullVectorPx) const;
    BodyBinding* find_binding(b2BodyId bodyId);

    // #=# Internal State #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

    EntityId next_id_ = 1;
    WorldSnapshot snapshot_{};
    b2WorldId world_id_ = b2_nullWorldId;
    std::vector<BodyBinding> bodies_;
    std::vector<Event> events_;
    std::vector<Command> pending_commands_;
    ScoreSystem score_system_;
    std::unordered_map<int, LevelData> level_registry_;

    LevelData current_level_{};
    bool level_loaded_ = false;
    bool paused_ = false;
    float level_y_offset_px_ = 0.0f;
    float support_bottom_px_ = 0.0f;
    float ground_top_ypx_ = 600.0f;

    int next_projectile_index_ = 0;
    b2BodyId active_projectile_body_id_ = b2_nullBodyId;
    int active_projectile_settled_frames_ = 0;
    float active_projectile_settled_time_sec_ = 0.0f;
    ProjectileType active_projectile_type_ = ProjectileType::Standard;
    bool active_projectile_ability_used_ = false;
};

}  // namespace angry
