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
