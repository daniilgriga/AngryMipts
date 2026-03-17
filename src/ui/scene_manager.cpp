// ============================================================
// scene_manager.cpp — Scene manager implementation.
// Part of: angry::ui
//
// Implements scene ownership and transition logic:
//   * Stores scene instances and current active scene id
//   * Forwards input/update/render to active scene
//   * Executes cross-scene handoff data wiring
//   * Applies transition side effects for gameplay flow
// ============================================================

#include "ui/scene_manager.hpp"

#include "ui/game_scene.hpp"
#include "ui/level_select_scene.hpp"
#include "ui/login_scene.hpp"
#include "ui/result_scene.hpp"

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

void SceneManager::handle_input ( const platform::Event& event )
{
    if ( current_id_ == SceneId::None )
        return;

    auto prev = current_id_;
    auto next = scenes_[current_id_]->handle_input ( event );
    if ( next != SceneId::None )
    {
        if ( next == SceneId::Result && prev == SceneId::Game )
        {
            auto* game = get_scene<GameScene> ( SceneId::Game );
            auto* result = get_scene<ResultScene> ( SceneId::Result );
            if ( game && result )
                result->set_result ( game->get_last_result() );
        }

        if ( next == SceneId::Game && prev == SceneId::LevelSelect )
        {
            auto* level_select = get_scene<LevelSelectScene> ( SceneId::LevelSelect );
            auto* game = get_scene<GameScene> ( SceneId::Game );
            if ( level_select && game )
                game->load_level ( level_select->get_selected_level_id(),
                                   level_select->get_scores_path() );
        }

        if ( next == SceneId::Game && prev == SceneId::Result )
        {
            auto* game = get_scene<GameScene> ( SceneId::Game );
            if ( game )
                game->retry();
        }

        if ( next == SceneId::LevelSelect )
        {
            auto* level_select = get_scene<LevelSelectScene> ( SceneId::LevelSelect );
            if ( level_select )
                level_select->reload_scores();
        }

        switch_to ( next );
    }
}

void SceneManager::update()
{
    if ( current_id_ == SceneId::None )
        return;

    scenes_[current_id_]->update();

    // Allow GameScene to trigger transitions without waiting for an input event.
    if ( current_id_ == SceneId::Game )
    {
        auto* game = get_scene<GameScene> ( SceneId::Game );
        if ( !game )
            return;

        const SceneId next = game->poll_pending_scene();
        if ( next == SceneId::Result )
        {
            auto* result = get_scene<ResultScene> ( SceneId::Result );
            if ( result )
                result->set_result ( game->get_last_result() );
            switch_to ( SceneId::Result );
        }
    }
    else if ( current_id_ == SceneId::Result )
    {
        auto* game = get_scene<GameScene> ( SceneId::Game );
        auto* result = get_scene<ResultScene> ( SceneId::Result );
        if ( game && result && game->poll_result_update() )
        {
            result->set_result ( game->get_last_result() );
        }
    }
}

void SceneManager::render ( platform::Window& window )
{
    if ( current_id_ == SceneId::None )
        return;

    scenes_[current_id_]->render ( window );
}

}  // namespace angry
