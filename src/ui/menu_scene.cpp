#include "ui/menu_scene.hpp"

namespace angry
{

MenuScene::MenuScene ( const sf::Font& font )
    : font_( font )
    , title_( font_, "AngryMipts", 64 )
    , prompt_( font_, "Press Enter to start", 24 )
{
    title_.setFillColor ( sf::Color::White );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
}

SceneId MenuScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Enter )
            return SceneId::LevelSelect;
    }
    return SceneId::None;
}

void MenuScene::update()
{
}

void MenuScene::render( sf::RenderWindow& window )
{
    auto window_size = sf::Vector2f ( window.getSize() );

    sf::Vertex background[] = {
        {{0.f, 0.f}, sf::Color ( 24, 42, 78 )},
        {{window_size.x, 0.f}, sf::Color ( 24, 42, 78 )},
        {{window_size.x, window_size.y}, sf::Color ( 58, 118, 122 )},
        {{0.f, window_size.y}, sf::Color ( 58, 118, 122 )},
    };
    window.draw ( background, 4, sf::PrimitiveType::TriangleFan );

    sf::CircleShape glow ( 180.f );
    glow.setOrigin ( {180.f, 180.f} );
    glow.setPosition ( {window_size.x * 0.82f, window_size.y * 0.18f} );
    glow.setFillColor ( sf::Color ( 255, 245, 200, 45 ) );
    window.draw ( glow );

    sf::RectangleShape panel ( {window_size.x * 0.58f, window_size.y * 0.42f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {window_size.x * 0.5f, window_size.y * 0.42f} );
    panel.setFillColor ( sf::Color ( 10, 16, 32, 125 ) );
    panel.setOutlineThickness ( 2.f );
    panel.setOutlineColor ( sf::Color ( 205, 230, 255, 110 ) );
    window.draw ( panel );

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
