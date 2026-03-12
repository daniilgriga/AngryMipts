#include "physics_thread.hpp"

#include <algorithm>
#include <optional>

namespace angry
{
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

PhysicsThread::~PhysicsThread()
{
    stop();
}

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

bool PhysicsThread::isRunning() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void PhysicsThread::registerLevel(const LevelData& level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    engine_.registerLevel(level);
}

void PhysicsThread::loadLevel(const LevelData& level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    engine_.registerLevel(level);

    if (running_)
    {
        commandQueue_.push(LoadLevelCmd{level.meta.id});
    }
    else
    {
        engine_.loadLevel(level);
        publishSnapshotLocked();
    }
}

void PhysicsThread::loadLevelById(int levelId)
{
    commandQueue_.push(LoadLevelCmd{levelId});
}

void PhysicsThread::restartLevel(int levelId)
{
    commandQueue_.push(RestartCmd{levelId});
}

void PhysicsThread::setPaused(bool paused)
{
    commandQueue_.push(PauseCmd{paused});
}

void PhysicsThread::pushCommand(const Command& cmd)
{
    commandQueue_.push(cmd);
}

void PhysicsThread::tickSingleThread(float dt)
{
    if (isRunning())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    engine_.processCommands(commandQueue_);
    engine_.step(dt);
    publishSnapshotLocked();

    std::vector<Event> events = engine_.drainEvents();
    for (const Event& event : events)
    {
        eventQueue_.push(event);
    }
}

WorldSnapshot PhysicsThread::readSnapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    const int front = frontSnapshotIndex_.load(std::memory_order_acquire);
    return snapshots_[static_cast<size_t>(front)];
}

std::vector<Event> PhysicsThread::drainEvents()
{
    std::vector<Event> events;
    while (const std::optional<Event> event = eventQueue_.try_pop())
    {
        events.push_back(*event);
    }
    return events;
}

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
                engine_.processCommands(commandQueue_);
                engine_.step(kFixedDtSec);
                publishSnapshotLocked();

                std::vector<Event> events = engine_.drainEvents();
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

void PhysicsThread::publishSnapshotLocked()
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    const int front = frontSnapshotIndex_.load(std::memory_order_relaxed);
    const int back = 1 - front;
    snapshots_[static_cast<size_t>(back)] = engine_.getSnapshot();
    frontSnapshotIndex_.store(back, std::memory_order_release);
}

}  // namespace angry
