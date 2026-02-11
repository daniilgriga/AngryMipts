#include "ui/scene_manager.hpp"

namespace angry
{

void SceneManager::add_scene ( SceneId id, std::unique_ptr<Scene> scene )
{
    scenes_[id] = std::move ( scene );
}

void SceneManager::switch_to ( SceneId id )
{
    current_id_ = id;
}

void SceneManager::handle_input ( const sf::Event& event )
{
    if ( current_id_ == SceneId::None )
        return;

    auto next = scenes_[current_id_]->handle_input ( event );
    if ( next != SceneId::None )
        switch_to ( next );
}

void SceneManager::update()
{
    if ( current_id_ == SceneId::None )
        return;

    scenes_[current_id_]->update();
}

void SceneManager::render ( sf::RenderWindow& window )
{
    if ( current_id_ == SceneId::None )
        return;

    scenes_[current_id_]->render ( window );
}

}  // namespace angry
