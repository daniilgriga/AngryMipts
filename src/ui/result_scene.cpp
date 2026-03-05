#include "ui/result_scene.hpp"

#include <string>

namespace angry
{

ResultScene::ResultScene ( const sf::Font& font )
    : font_ ( font )
    , title_ ( font_, "", 48 )
    , score_text_ ( font_, "", 28 )
    , stars_text_ ( font_, "", 36 )
    , prompt_ ( font_, "[Enter] Retry   [Backspace] Menu", 20 )
{
    title_.setFillColor ( sf::Color::White );
    score_text_.setFillColor ( sf::Color::White );
    stars_text_.setFillColor ( sf::Color ( 255, 215, 0 ) );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
}

void ResultScene::set_result ( const LevelResult& result )
{
    result_ = result;

    title_.setString ( result_.win ? "Level Complete!" : "Level Failed" );
    title_.setFillColor ( result_.win ? sf::Color ( 50, 200, 50 ) : sf::Color ( 200, 50, 50 ) );

    score_text_.setString ( "Score: " + std::to_string ( result_.score ) );

    std::string star_str;
    for ( int i = 0; i < 3; ++i )
        star_str += ( i < result_.stars ) ? "* " : "- ";
    stars_text_.setString ( star_str );
}

SceneId ResultScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Enter )
            return SceneId::Game;
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;
    }
    return SceneId::None;
}

void ResultScene::update()
{
}

void ResultScene::render ( sf::RenderWindow& window )
{
    auto ws = sf::Vector2f ( window.getSize() );

    sf::Vertex background[] = {
        {{0.f, 0.f}, sf::Color ( 20, 28, 54 )},
        {{ws.x, 0.f}, sf::Color ( 20, 28, 54 )},
        {{ws.x, ws.y}, sf::Color ( 48, 68, 106 )},
        {{0.f, ws.y}, sf::Color ( 48, 68, 106 )},
    };
    window.draw ( background, 4, sf::PrimitiveType::TriangleFan );

    sf::RectangleShape panel ( {ws.x * 0.52f, ws.y * 0.52f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {ws.x * 0.5f, ws.y * 0.47f} );
    panel.setFillColor ( sf::Color ( 10, 15, 30, 130 ) );
    panel.setOutlineThickness ( 2.f );
    panel.setOutlineColor ( sf::Color ( 195, 220, 255, 120 ) );
    window.draw ( panel );

    auto center_text = [&] ( sf::Text& text, float y )
    {
        auto bounds = text.getLocalBounds();
        text.setOrigin ( {bounds.position.x + bounds.size.x / 2.f,
                          bounds.position.y + bounds.size.y / 2.f} );
        text.setPosition ( {ws.x / 2.f, y} );
    };

    center_text ( title_, ws.y * 0.25f );
    center_text ( stars_text_, ws.y * 0.40f );
    center_text ( score_text_, ws.y * 0.52f );
    center_text ( prompt_, ws.y * 0.70f );

    window.draw ( title_ );
    window.draw ( stars_text_ );
    window.draw ( score_text_ );
    window.draw ( prompt_ );
}

}  // namespace angry
