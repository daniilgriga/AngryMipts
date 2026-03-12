#include "ui/game_scene.hpp"
#include "ui/level_select_scene.hpp"
#include "ui/menu_scene.hpp"
#include "ui/result_scene.hpp"
#include "ui/scene_manager.hpp"

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{

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

    auto level_select = std::make_unique<angry::LevelSelectScene> ( font );
    level_select->load_data ( resolveProjectPath ( "levels" ),
                              resolveProjectPath ( "scores.json" ) );

    angry::SceneManager scenes;
    scenes.add_scene ( angry::SceneId::Menu, std::make_unique<angry::MenuScene> ( font ) );
    scenes.add_scene ( angry::SceneId::LevelSelect, std::move ( level_select ) );
    scenes.add_scene ( angry::SceneId::Game, std::make_unique<angry::GameScene> ( font ) );
    scenes.add_scene ( angry::SceneId::Result, std::make_unique<angry::ResultScene> ( font ) );
    scenes.switch_to ( angry::SceneId::Menu );

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
