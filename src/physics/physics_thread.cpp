// ============================================================
// physics_thread.cpp — Threaded physics worker implementation.
// Part of: angry::physics
//
// Implements worker-thread orchestration for PhysicsEngine:
//   * Starts/stops fixed-step simulation thread safely
//   * Drains command queue and executes deterministic physics ticks
//   * Publishes double-buffered snapshots for readers
//   * Collects emitted events into a thread-safe outbound queue
// ============================================================

#include "physics_thread.hpp"

#include <algorithm>
#include <optional>

namespace angry
{

// #=# Local Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

template <typename T>
void clearQueue(ThreadSafeQueue<T>& queue)
{
    while (queue.try_pop().has_value())
    {
    }
}

}  // namespace

// #=# Construction / Destruction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#

PhysicsThread::~PhysicsThread()
{
    stop();
}

// #=# Lifecycle & Command API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void PhysicsThread::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_)
    {
        return;
    }

    stopRequested_.store(false, std::memory_order_release);
    running_ = true;
    worker_ = std::thread(&PhysicsThread::workerLoop, this);
}

void PhysicsThread::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
        {
            return;
        }

        stopRequested_.store(true, std::memory_order_release);
        running_ = false;
    }

    stopCv_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }

    clearQueue(commandQueue_);
    clearQueue(eventQueue_);
}

bool PhysicsThread::is_running() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void PhysicsThread::register_level(const LevelData& level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    engine_.register_level(level);
}

void PhysicsThread::loadLevel(const LevelData& level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    engine_.register_level(level);

    if (running_)
    {
        commandQueue_.push(LoadLevelCmd{level.meta.id});
    }
    else
    {
        engine_.load_level(level);
        publishSnapshotLocked();
    }
}

void PhysicsThread::load_level_by_id(int levelId)
{
    commandQueue_.push(LoadLevelCmd{levelId});
}

void PhysicsThread::restart_level(int levelId)
{
    commandQueue_.push(RestartCmd{levelId});
}

void PhysicsThread::set_paused(bool paused)
{
    commandQueue_.push(PauseCmd{paused});
}

void PhysicsThread::push_command(const Command& cmd)
{
    commandQueue_.push(cmd);
}

// #=# Single-Thread Adapter #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void PhysicsThread::tick_single_thread(float dt)
{
    if (is_running())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    engine_.process_commands(commandQueue_);
    engine_.step(dt);
    publishSnapshotLocked();

    std::vector<Event> events = engine_.drain_events();
    for (const Event& event : events)
    {
        eventQueue_.push(event);
    }
}

// #=# Snapshot / Events API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

WorldSnapshot PhysicsThread::read_snapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    const int front = frontSnapshotIndex_.load(std::memory_order_acquire);
    return snapshots_[static_cast<size_t>(front)];
}

std::vector<Event> PhysicsThread::drain_events()
{
    std::vector<Event> events;
    while (const std::optional<Event> event = eventQueue_.try_pop())
    {
        events.push_back(*event);
    }
    return events;
}

// #=# Worker Internals #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

void PhysicsThread::workerLoop()
{
    using clock = std::chrono::steady_clock;
    const auto fixedDt = std::chrono::duration<double>(kFixedDtSec);
    auto previous = clock::now();
    std::chrono::duration<double> accumulator{0.0};

    while (!stopRequested_.load(std::memory_order_acquire))
    {
        const auto now = clock::now();
        accumulator += now - previous;
        previous = now;

        // Prevent very large catch-up after breakpoints/sleep.
        accumulator = std::min(accumulator, fixedDt * 5);

        while (accumulator >= fixedDt
            && !stopRequested_.load(std::memory_order_acquire))
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                engine_.process_commands(commandQueue_);
                engine_.step(kFixedDtSec);
                publishSnapshotLocked();

                std::vector<Event> events = engine_.drain_events();
                for (const Event& event : events)
                {
                    eventQueue_.push(event);
                }
            }

            accumulator -= fixedDt;
        }

        std::unique_lock<std::mutex> sleepLock(mutex_);
        stopCv_.wait_for(
            sleepLock,
            std::chrono::milliseconds(1),
            [this]()
            {
                return stopRequested_.load(std::memory_order_acquire);
            });
    }
}

// Publishes newly computed world state by flipping back/front
// snapshot buffers under snapshot mutex.
void PhysicsThread::publishSnapshotLocked()
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    const int front = frontSnapshotIndex_.load(std::memory_order_relaxed);
    const int back = 1 - front;
    snapshots_[static_cast<size_t>(back)] = engine_.get_snapshot();
    frontSnapshotIndex_.store(back, std::memory_order_release);
}

}  // namespace angry
