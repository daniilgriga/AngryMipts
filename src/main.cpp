#include "ui/game_scene.hpp"
#include "ui/menu_scene.hpp"
#include "ui/scene_manager.hpp"

#include <SFML/Graphics.hpp>

#include <iostream>

int main()
{
    sf::Font font;
    if ( !font.openFromFile ( "assets/fonts/liberation_sans.ttf" ) )
    {
        std::cerr << "Failed to load font" << std::endl;
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
