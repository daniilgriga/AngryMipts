// ============================================================
// main.cpp — Application bootstrap and scene wiring.
// Part of: angry::app
//
// Initializes runtime and launches the game loop:
//   * Creates window/font and core services
//   * Instantiates scenes and registers transitions
//   * Runs native loop or Emscripten frame callback
//   * Handles platform-specific frame limiting behavior
// ============================================================

#include "data/account_service.hpp"
#include "platform/platform.hpp"
#include "ui/game_scene.hpp"
#include "ui/level_select_scene.hpp"
#include "ui/login_scene.hpp"
#include "ui/menu_scene.hpp"
#include "ui/result_scene.hpp"
#include "ui/scene_manager.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#else
#include <emscripten.h>
#endif

#include <filesystem>
#include <iostream>
#include <string>

namespace
{

#ifndef __EMSCRIPTEN__

enum class FrameLimitMode
{
    Fps60,
    Fps120,
    Unlimited,
};

FrameLimitMode next_frame_limit_mode( FrameLimitMode mode )
{
    switch ( mode )
    {
    case FrameLimitMode::Fps60:
        return FrameLimitMode::Fps120;
    case FrameLimitMode::Fps120:
        return FrameLimitMode::Unlimited;
    case FrameLimitMode::Unlimited:
    default:
        return FrameLimitMode::Fps60;
    }
}

unsigned int frame_limit_value( FrameLimitMode mode )
{
    switch ( mode )
    {
    case FrameLimitMode::Fps60:
        return 60u;
    case FrameLimitMode::Fps120:
        return 120u;
    case FrameLimitMode::Unlimited:
    default:
        return 0u;
    }
}

const char* frame_limit_label( FrameLimitMode mode )
{
    switch ( mode )
    {
    case FrameLimitMode::Fps60:
        return "60 FPS";
    case FrameLimitMode::Fps120:
        return "120 FPS";
    case FrameLimitMode::Unlimited:
    default:
        return "Unlimited";
    }
}

#endif  // !__EMSCRIPTEN__

std::string resolve_project_path( const std::filesystem::path& relative_path )
{
    if ( std::filesystem::exists( relative_path ) )
    {
        return relative_path.string();
    }

#ifdef ANGRY_MIPTS_SOURCE_DIR
    const std::filesystem::path from_source_dir =
        std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / relative_path;
    if ( std::filesystem::exists( from_source_dir ) )
    {
        return from_source_dir.string();
    }
#endif

    return relative_path.string();
}

}  // namespace

// #=# Web (Emscripten) Main Loop #=#=#=#=#=#=#=#=#=#=#=#=#=#=
#ifdef __EMSCRIPTEN__

struct WebApp
{
    platform::Window        window;
    platform::Font          font;
    angry::AccountService   accounts { "" };
    angry::SceneManager     scenes;
};

static WebApp* g_app = nullptr;

static void web_frame()
{
    auto& app = *g_app;

    // Poll events
    for ( auto& event : app.window.pollEvents() )
    {
        // Check for close
        if ( std::holds_alternative<platform::ClosedEvent>( event ) )
        {
            app.window.close();
            return;
        }
        app.scenes.handle_input( event );
    }

    app.scenes.update();

    app.window.clear( platform::Color( 135, 206, 235 ) );
    app.scenes.render( app.window );
    app.window.display();
}

int main()
{
    g_app = new WebApp();
    auto& app = *g_app;

    const std::string font_path = "/assets/fonts/liberation_sans.ttf";
    const std::string levels_path = "/levels";
    const std::string scores_path;   // disabled on web (no persistent local file storage yet)

    app.window.create( 1280, 720, "AngryMipts" );
    app.window.setFramerateLimit( 60 );

    if ( !app.font.openFromFile( font_path ) )
    {
        std::cerr << "Failed to load font" << std::endl;
        return 1;
    }

    app.accounts.load_session();

    auto level_select = std::make_unique<angry::LevelSelectScene>( app.font, &app.accounts );
    level_select->load_data( levels_path, scores_path );

    app.scenes.add_scene( angry::SceneId::Login,
                          std::make_unique<angry::LoginScene>( app.font, app.accounts ) );
    app.scenes.add_scene( angry::SceneId::Menu,
                          std::make_unique<angry::MenuScene>( app.font, app.accounts ) );
    app.scenes.add_scene( angry::SceneId::LevelSelect, std::move( level_select ) );
    app.scenes.add_scene( angry::SceneId::Game,
                          std::make_unique<angry::GameScene>( app.font, &app.accounts ) );
    app.scenes.add_scene( angry::SceneId::Result,
                          std::make_unique<angry::ResultScene>( app.font ) );
    app.scenes.switch_to( angry::SceneId::Login );

    emscripten_set_main_loop( web_frame, 0, 1 );

    return 0;
}

#else  // #=# Native (SFML) Main Loop #=#=#=#=#=#=#=#=#=#=#=#=#

int main()
{
    const std::string font_path = resolve_project_path( "assets/fonts/liberation_sans.ttf" );
    const std::string window_title = "AngryMipts";

    sf::Font font;
    if ( !font.openFromFile ( font_path ) )
    {
        std::cerr << "Failed to load font from: " << font_path << std::endl;
        return 1;
    }

    sf::RenderWindow window;
    bool is_fullscreen = false;
    sf::Vector2u windowed_size {1280u, 720u};
    FrameLimitMode frame_limit_mode = FrameLimitMode::Fps60;

    auto apply_frame_limit = [&] ()
    {
        window.setVerticalSyncEnabled ( false );
        window.setFramerateLimit ( frame_limit_value ( frame_limit_mode ) );
    };

    auto recreate_window = [&] ( bool fullscreen )
    {
        if ( fullscreen )
        {
            window.create ( sf::VideoMode::getDesktopMode(), window_title, sf::State::Fullscreen );
        }
        else
        {
            window.create ( sf::VideoMode ( windowed_size ), window_title,
                            sf::Style::Default, sf::State::Windowed );
        }

        apply_frame_limit();
        is_fullscreen = fullscreen;
    };

    recreate_window ( true );

    const std::string session_path = resolve_project_path ( "session.json" );
    angry::AccountService accounts ( session_path );
    accounts.load_session();

    auto level_select = std::make_unique<angry::LevelSelectScene> ( font, &accounts );
    level_select->load_data ( resolve_project_path ( "levels" ),
                              resolve_project_path ( "scores.json" ) );

    angry::SceneManager scenes;
    scenes.add_scene ( angry::SceneId::Login,
                       std::make_unique<angry::LoginScene> ( font, accounts ) );
    scenes.add_scene ( angry::SceneId::Menu, std::make_unique<angry::MenuScene> ( font, accounts ) );
    scenes.add_scene ( angry::SceneId::LevelSelect, std::move ( level_select ) );
    scenes.add_scene ( angry::SceneId::Game, std::make_unique<angry::GameScene> ( font, &accounts ) );
    scenes.add_scene ( angry::SceneId::Result, std::make_unique<angry::ResultScene> ( font ) );
    scenes.switch_to (
        accounts.is_logged_in() ? angry::SceneId::Menu : angry::SceneId::Login );

    while ( window.isOpen() )
    {
        while ( const auto event = window.pollEvent() )
        {
            if ( event->is<sf::Event::Closed>() )
                window.close();

            if ( const auto* resized = event->getIf<sf::Event::Resized>() )
            {
                if ( !is_fullscreen )
                    windowed_size = resized->size;
            }

            if ( event->is<sf::Event::FocusGained>() )
            {
                if ( auto* game = scenes.get_scene<angry::GameScene> ( angry::SceneId::Game ) )
                    game->notify_window_recreated();
            }

            if ( const auto* key = event->getIf<sf::Event::KeyPressed>() )
            {
                const bool toggle_fullscreen =
                    ( key->code == sf::Keyboard::Key::F11 )
                    || ( key->code == sf::Keyboard::Key::Enter && key->alt );
                if ( toggle_fullscreen )
                {
                    if ( !is_fullscreen )
                    {
                        windowed_size = window.getSize();
                    }
                    recreate_window ( !is_fullscreen );
                    if ( auto* game = scenes.get_scene<angry::GameScene> ( angry::SceneId::Game ) )
                        game->notify_window_recreated();
                    continue;
                }

                if ( key->code == sf::Keyboard::Key::F10 )
                {
                    frame_limit_mode = next_frame_limit_mode ( frame_limit_mode );
                    apply_frame_limit();
                    std::cout << "Frame limit mode: " << frame_limit_label ( frame_limit_mode )
                              << std::endl;
                    continue;
                }

                if ( key->code == sf::Keyboard::Key::Escape )
                {
                    if ( is_fullscreen )
                    {
                        recreate_window ( false );
                        if ( auto* game = scenes.get_scene<angry::GameScene> ( angry::SceneId::Game ) )
                            game->notify_window_recreated();
                        continue;
                    }
                    window.close();
                }
            }

            scenes.handle_input ( *event );
        }

        scenes.update();

        window.clear ( sf::Color ( 135, 206, 235 ) );
        scenes.render ( window );
        window.display();
    }

    return 0;
}

#endif  // __EMSCRIPTEN__
