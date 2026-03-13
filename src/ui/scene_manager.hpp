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

    template <typename T>
    T* get_scene ( SceneId id )
    {
        auto it = scenes_.find ( id );
        if ( it == scenes_.end() )
            return nullptr;
        return dynamic_cast<T*> ( it->second.get() );
    }

    void handle_input ( const platform::Event& event );
    void update();
    void render ( platform::Window& window );
};

}  // namespace angry
