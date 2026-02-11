#pragma once
#include "scene.hpp"

#include <memory>
#include <unordered_map>

namespace angry
{

class SceneManager
{
private:
    std::unordered_map<SceneId, std::unique_ptr<Scene>> scenes_;
    SceneId current_id_ = SceneId::None;

public:
    void add_scene ( SceneId id, std::unique_ptr<Scene> scene );
    void switch_to ( SceneId id );

    void handle_input ( const sf::Event& event );
    void update();
    void render ( sf::RenderWindow& window );
};

}  // namespace angry
