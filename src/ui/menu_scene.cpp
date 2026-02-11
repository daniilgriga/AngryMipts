#include "ui/menu_scene.hpp"

namespace angry
{

MenuScene::MenuScene ( const sf::Font& font )
    : font_( font )
    , title_( font_, "AngryMipts", 64 )
    , prompt_( font_, "Press Enter to start", 24 )
{
    title_.setFillColor ( sf::Color::White );
    prompt_.setFillColor ( sf::Color ( 200, 200, 200 ) );
}

SceneId MenuScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Enter )
            return SceneId::Game;
    }
    return SceneId::None;
}

void MenuScene::update()
{
}

void MenuScene::render( sf::RenderWindow& window )
{
    auto window_size = sf::Vector2f ( window.getSize() );

    auto title_bounds = title_.getLocalBounds();
    title_.setOrigin ( {title_bounds.position.x + title_bounds.size.x / 2.f,
                        title_bounds.position.y + title_bounds.size.y / 2.f} );
    title_.setPosition ( {window_size.x / 2.f, window_size.y / 3.f} );

    auto prompt_bounds = prompt_.getLocalBounds();
    prompt_.setOrigin ( {prompt_bounds.position.x + prompt_bounds.size.x / 2.f,
                         prompt_bounds.position.y + prompt_bounds.size.y / 2.f} );
    prompt_.setPosition ( {window_size.x / 2.f, window_size.y / 2.f} );

    window.draw ( title_ );
    window.draw ( prompt_ );
}

}  // namespace angry
