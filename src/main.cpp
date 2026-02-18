#include "ui/game_scene.hpp"
#include "ui/menu_scene.hpp"
#include "ui/scene_manager.hpp"

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{

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

    sf::Font font;
    if ( !font.openFromFile ( fontPath ) )
    {
        std::cerr << "Failed to load font from: " << fontPath << std::endl;
        return 1;
    }

    sf::RenderWindow window ( sf::VideoMode ( {1280, 720} ), "AngryMipts" );
    window.setFramerateLimit ( 60 );

    angry::SceneManager scenes;
    scenes.add_scene ( angry::SceneId::Menu, std::make_unique<angry::MenuScene> ( font ) );
    scenes.add_scene ( angry::SceneId::Game, std::make_unique<angry::GameScene> ( font ) );
    scenes.switch_to ( angry::SceneId::Menu );

    while ( window.isOpen() )
    {
        while ( const auto event = window.pollEvent() )
        {
            if ( event->is<sf::Event::Closed>() )
                window.close();

            if ( const auto* key = event->getIf<sf::Event::KeyPressed>() )
            {
                if ( key->code == sf::Keyboard::Key::Escape )
                    window.close();
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
