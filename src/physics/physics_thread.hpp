// ============================================================
// physics_thread.hpp — Threaded physics worker interface.
// Part of: angry::physics
//
// Declares PhysicsThread, a worker-wrapper over PhysicsEngine:
//   * Owns lifecycle of background fixed-step simulation thread
//   * Accepts command stream and forwards it to PhysicsEngine
//   * Publishes double-buffered WorldSnapshot for lock-safe reads
//   * Buffers produced physics events for main-thread draining
// ============================================================

#pragma once

#include "physics_engine.hpp"

#include "../shared/command.hpp"
#include "../shared/event.hpp"
#include "../shared/level_data.hpp"
#include "../shared/thread_safe_queue.hpp"
#include "../shared/world_snapshot.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace angry
{

// Runs PhysicsEngine in a fixed-step worker thread and exposes
// thread-safe snapshot/event access for the main loop.
class PhysicsThread
{
public:
    PhysicsThread() = default;
    ~PhysicsThread();

    void start();
    void stop();
    bool is_running() const;

    void register_level(const LevelData& level);
    void load_level(const LevelData& level);
    void load_level_by_id(int levelId);
    void restart_level(int levelId);
    void set_paused(bool paused);
    void push_command(const Command& cmd);

    // Temporary adapter for single-thread integration while worker loop is not introduced yet.
    void tick_single_thread(float dt);

    WorldSnapshot read_snapshot() const;
    std::vector<Event> drain_events();

private:
    void worker_loop();
    void publish_snapshot_locked();

    static constexpr float kFixedDtSec = 1.0f / 60.0f;

    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    PhysicsEngine engine_;
    ThreadSafeQueue<Command> command_queue_;
    ThreadSafeQueue<Event> event_queue_;
    std::array<WorldSnapshot, 2> snapshots_{};
    mutable std::mutex snapshot_mutex_;
    std::atomic<int> front_snapshot_index_{0};
    bool running_ = false;
};

}  // namespace angry
