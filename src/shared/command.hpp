// ============================================================
// command.hpp — Commands sent to physics simulation.
// Part of: angry::shared
//
// Defines command payloads exchanged with physics runtime:
//   * Launch, restart, and level-loading requests
//   * Pause toggles for simulation state control
//   * Ability activation for current projectile
//   * Unified Command variant for queue-based dispatch
// ============================================================

#pragma once
#include <variant>

#include "types.hpp"

namespace angry
{

struct LaunchCmd
{
    Vec2 pullVectorPx;
};

struct RestartCmd
{
    int levelId;
};

struct LoadLevelCmd
{
    int levelId;
};

struct PauseCmd
{
    bool paused;
};

struct ActivateAbilityCmd
{
    EntityId projectileId;
};

using Command = std::variant<LaunchCmd, RestartCmd, LoadLevelCmd, PauseCmd, ActivateAbilityCmd>;

}  // namespace angry
