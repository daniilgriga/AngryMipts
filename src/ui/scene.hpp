#pragma once
#include "platform/platform.hpp"

namespace angry
{

enum class SceneId
{
    Login,
    Menu,
    LevelSelect,
    Game,
    Result,
    None,
};

class Scene
{
public:
    virtual ~Scene() = default;

    virtual SceneId handle_input ( const platform::Event& event ) = 0;
    virtual void update() = 0;
    virtual void render ( platform::Window& window ) = 0;
};

}  // namespace angry
