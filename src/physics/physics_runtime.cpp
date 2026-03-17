// ============================================================
// physics_runtime.cpp — Physics mode facade implementation.
// Part of: angry::physics
//
// Implements runtime dispatch between backends:
//   * Starts/stops worker thread in threaded mode
//   * Forwards level registration/loading and commands
//   * Runs direct stepping only in single-threaded mode
//   * Exposes snapshot/event reads through one interface
// ============================================================

#include "physics_runtime.hpp"

namespace angry
{

// #=# Construction / Destruction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#

PhysicsRuntime::PhysicsRuntime(PhysicsMode mode)
    : mode_(mode)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threaded_engine_.start();
    }
}

PhysicsRuntime::~PhysicsRuntime()
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threaded_engine_.stop();
    }
}

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void PhysicsRuntime::register_level(const LevelData& level)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threaded_engine_.register_level(level);
        return;
    }

    single_engine_.register_level(level);
}

void PhysicsRuntime::load_level(const LevelData& level)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        threaded_engine_.load_level(level);
        return;
    }

    single_engine_.load_level(level);
}

void PhysicsRuntime::process_commands(ThreadSafeQueue<Command>& cmdQueue)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        while (const std::optional<Command> cmd = cmdQueue.try_pop())
        {
            threaded_engine_.push_command(*cmd);
        }
        return;
    }

    single_engine_.process_commands(cmdQueue);
}

void PhysicsRuntime::step(float dt)
{
    if (mode_ == PhysicsMode::Threaded)
    {
        // Worker thread owns simulation updates in threaded mode.
        (void)dt;
        return;
    }

    single_engine_.step(dt);
}

WorldSnapshot PhysicsRuntime::get_snapshot() const
{
    if (mode_ == PhysicsMode::Threaded)
    {
        return threaded_engine_.read_snapshot();
    }

    return single_engine_.get_snapshot();
}

std::vector<Event> PhysicsRuntime::drain_events()
{
    if (mode_ == PhysicsMode::Threaded)
    {
        return threaded_engine_.drain_events();
    }

    return single_engine_.drain_events();
}

// #=# Accessors #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

PhysicsMode PhysicsRuntime::mode() const
{
    return mode_;
}

}  // namespace angry
