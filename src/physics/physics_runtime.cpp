#include "physics_runtime.hpp"

namespace angry
{

PhysicsRuntime::PhysicsRuntime(PhysicsMode mode)
    : mode_(mode)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threadedEngine_.start();
    }
}

PhysicsRuntime::~PhysicsRuntime()
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threadedEngine_.stop();
    }
}

void PhysicsRuntime::registerLevel(const LevelData& level)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threadedEngine_.registerLevel(level);
        return;
    }

    singleEngine_.registerLevel(level);
}

void PhysicsRuntime::loadLevel(const LevelData& level)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threadedEngine_.loadLevel(level);
        return;
    }

    singleEngine_.loadLevel(level);
}

void PhysicsRuntime::processCommands(ThreadSafeQueue<Command>& cmdQueue)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        while (const std::optional<Command> cmd = cmdQueue.try_pop())
        {
            threadedEngine_.pushCommand(*cmd);
        }
        return;
    }

    singleEngine_.processCommands(cmdQueue);
}

void PhysicsRuntime::step(float dt)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        // Worker thread owns simulation updates in threaded mode.
        (void)dt;
        return;
    }

    singleEngine_.step(dt);
}

WorldSnapshot PhysicsRuntime::getSnapshot() const
{
    if (mode_ == PhysicsMode::Threaded)
    {
        return threadedEngine_.readSnapshot();
    }

    return singleEngine_.getSnapshot();
}

std::vector<Event> PhysicsRuntime::drainEvents()
{
    if (mode_ == PhysicsMode::Threaded)
    {
        return threadedEngine_.drainEvents();
    }

    return singleEngine_.drainEvents();
}

PhysicsMode PhysicsRuntime::mode() const
{
    return mode_;
}

}  // namespace angry
