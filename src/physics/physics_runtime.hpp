// ============================================================
// physics_runtime.hpp — Physics mode facade interface.
// Part of: angry::physics
//
// Declares a thin runtime wrapper that:
//   * Chooses single-threaded or threaded physics backend
//   * Keeps one stable API for UI/game loop code
//   * Forwards commands, stepping, snapshots, and events
//   * Hides backend-specific ownership/lifecycle details
// ============================================================

#pragma once

#include "physics_engine.hpp"
#include "physics_thread.hpp"

#include "../shared/command.hpp"
#include "../shared/event.hpp"
#include "../shared/level_data.hpp"
#include "../shared/thread_safe_queue.hpp"
#include "../shared/world_snapshot.hpp"

#include <vector>

namespace angry
{

enum class PhysicsMode
{
    SingleThread,
    Threaded,
};

// Keeps one physics-facing API while internally switching
// between direct PhysicsEngine and worker-thread backend.
class PhysicsRuntime
{
public:
    explicit PhysicsRuntime(PhysicsMode mode = PhysicsMode::SingleThread);
    ~PhysicsRuntime();

    void register_level(const LevelData& level);
    void load_level(const LevelData& level);
    void process_commands(ThreadSafeQueue<Command>& cmdQueue);
    void step(float dt);

    WorldSnapshot get_snapshot() const;
    std::vector<Event> drain_events();

    PhysicsMode mode() const;

private:
    PhysicsMode mode_ = PhysicsMode::SingleThread;
    PhysicsEngine single_engine_;
    PhysicsThread threaded_engine_;
};

}  // namespace angry
