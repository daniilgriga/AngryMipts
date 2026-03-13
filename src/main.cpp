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

FrameLimitMode nextFrameLimitMode( FrameLimitMode mode )
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

unsigned int frameLimitValue( FrameLimitMode mode )
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

const char* frameLimitLabel( FrameLimitMode mode )
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

std::string resolveProjectPath( const std::filesystem::path& relativePath )
{
    if ( std::filesystem::exists( relativePath ) )
    {
        return relativePath.string();
    }

#ifdef ANGRY_MIPTS_SOURCE_DIR
    const std::filesystem::path fromSourceDir =
        std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / relativePath;
    if ( std::filesystem::exists( fromSourceDir ) )
    {
        return fromSourceDir.string();
    }
#endif

    return relativePath.string();
}

}  // namespace

// ─── Web (Emscripten) main loop ─────────────────────────────────────────────
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
    for ( auto& ev : app.window.pollEvents() )
    {
        // Check for close
        if ( std::holds_alternative<platform::ClosedEvent>( ev ) )
        {
            app.window.close();
            return;
        }
        app.scenes.handle_input( ev );
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

    const std::string fontPath = "/assets/fonts/liberation_sans.ttf";
    const std::string levelsPath = "/levels";
    const std::string scoresPath;   // disabled on web (no persistent local file storage yet)

    app.window.create( 1280, 720, "AngryMipts" );
    app.window.setFramerateLimit( 60 );

    if ( !app.font.openFromFile( fontPath ) )
    {
        std::cerr << "Failed to load font" << std::endl;
        return 1;
    }

    app.accounts.loadSession();

    auto level_select = std::make_unique<angry::LevelSelectScene>( app.font, &app.accounts );
    level_select->load_data( levelsPath, scoresPath );

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

#else  // ─── Native (SFML) main loop ─────────────────────────────────────────

int main()
{
    const std::string fontPath = resolveProjectPath( "assets/fonts/liberation_sans.ttf" );
    const std::string windowTitle = "AngryMipts";

    sf::Font font;
    if ( !font.openFromFile ( fontPath ) )
    {
        std::cerr << "Failed to load font from: " << fontPath << std::endl;
        return 1;
    }

    sf::RenderWindow window;
    bool isFullscreen = false;
    sf::Vector2u windowedSize {1280u, 720u};
    FrameLimitMode frameLimitMode = FrameLimitMode::Fps60;

    auto applyFrameLimit = [&] ()
    {
        window.setVerticalSyncEnabled ( false );
        window.setFramerateLimit ( frameLimitValue ( frameLimitMode ) );
    };

    auto recreateWindow = [&] ( bool fullscreen )
    {
        if ( fullscreen )
        {
            window.create ( sf::VideoMode::getDesktopMode(), windowTitle, sf::State::Fullscreen );
        }
        else
        {
            window.create ( sf::VideoMode ( windowedSize ), windowTitle,
                            sf::Style::Default, sf::State::Windowed );
        }

        applyFrameLimit();
        isFullscreen = fullscreen;
    };

    recreateWindow ( true );

    const std::string sessionPath = resolveProjectPath ( "session.json" );
    angry::AccountService accounts ( sessionPath );
    accounts.loadSession();

    auto level_select = std::make_unique<angry::LevelSelectScene> ( font, &accounts );
    level_select->load_data ( resolveProjectPath ( "levels" ),
                              resolveProjectPath ( "scores.json" ) );

    angry::SceneManager scenes;
    scenes.add_scene ( angry::SceneId::Login,
                       std::make_unique<angry::LoginScene> ( font, accounts ) );
    scenes.add_scene ( angry::SceneId::Menu, std::make_unique<angry::MenuScene> ( font, accounts ) );
    scenes.add_scene ( angry::SceneId::LevelSelect, std::move ( level_select ) );
    scenes.add_scene ( angry::SceneId::Game, std::make_unique<angry::GameScene> ( font, &accounts ) );
    scenes.add_scene ( angry::SceneId::Result, std::make_unique<angry::ResultScene> ( font ) );
    scenes.switch_to (
        accounts.isLoggedIn() ? angry::SceneId::Menu : angry::SceneId::Login );

    while ( window.isOpen() )
    {
        while ( const auto event = window.pollEvent() )
        {
            if ( event->is<sf::Event::Closed>() )
                window.close();

            if ( const auto* resized = event->getIf<sf::Event::Resized>() )
            {
                if ( !isFullscreen )
                    windowedSize = resized->size;
            }

            if ( event->is<sf::Event::FocusGained>() )
            {
                if ( auto* game = scenes.get_scene<angry::GameScene> ( angry::SceneId::Game ) )
                    game->notify_window_recreated();
            }

            if ( const auto* key = event->getIf<sf::Event::KeyPressed>() )
            {
                const bool toggleFullscreen =
                    ( key->code == sf::Keyboard::Key::F11 )
                    || ( key->code == sf::Keyboard::Key::Enter && key->alt );
                if ( toggleFullscreen )
                {
                    if ( !isFullscreen )
                    {
                        windowedSize = window.getSize();
                    }
                    recreateWindow ( !isFullscreen );
                    if ( auto* game = scenes.get_scene<angry::GameScene> ( angry::SceneId::Game ) )
                        game->notify_window_recreated();
                    continue;
                }

                if ( key->code == sf::Keyboard::Key::F10 )
                {
                    frameLimitMode = nextFrameLimitMode ( frameLimitMode );
                    applyFrameLimit();
                    std::cout << "Frame limit mode: " << frameLimitLabel ( frameLimitMode )
                              << std::endl;
                    continue;
                }

                if ( key->code == sf::Keyboard::Key::Escape )
                {
                    if ( isFullscreen )
                    {
                        recreateWindow ( false );
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
