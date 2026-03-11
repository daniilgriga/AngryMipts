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

// Facade that keeps a stable API while allowing either single-thread or threaded physics backend.
class PhysicsRuntime
{
public:
    explicit PhysicsRuntime(PhysicsMode mode = PhysicsMode::SingleThread);
    ~PhysicsRuntime();

    void registerLevel(const LevelData& level);
    void loadLevel(const LevelData& level);
    void processCommands(ThreadSafeQueue<Command>& cmdQueue);
    void step(float dt);

    WorldSnapshot getSnapshot() const;
    std::vector<Event> drainEvents();

    PhysicsMode mode() const;

private:
    PhysicsMode mode_ = PhysicsMode::SingleThread;
    PhysicsEngine singleEngine_;
    PhysicsThread threadedEngine_;
};

}  // namespace angry
