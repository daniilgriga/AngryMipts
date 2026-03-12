#include "ui/result_scene.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>

namespace angry
{
namespace
{

void draw_vertical_gradient ( sf::RenderWindow& window,
                              sf::Color top, sf::Color bottom )
{
    const sf::Vector2f ws ( window.getSize() );
    sf::Vertex background[] = {
        {{0.f, 0.f}, top},
        {{ws.x, 0.f}, top},
        {{ws.x, ws.y}, bottom},
        {{0.f, ws.y}, bottom},
    };
    window.draw ( background, 4, sf::PrimitiveType::TriangleFan );
}

// Bounce-in easing: overshoots slightly then settles at 1.0
float bounce_in ( float t )
{
    if ( t <= 0.f ) return 0.f;
    if ( t >= 1.f ) return 1.f;
    // elastic overshoot: scale * sin curve
    const float s = 1.70158f;
    t -= 1.f;
    return t * t * ( ( s + 1.f ) * t + s ) + 1.f;
}

}  // namespace

ResultScene::ResultScene ( const sf::Font& font )
    : font_ ( font )
    , title_ ( font_, "", 48 )
    , score_text_ ( font_, "", 28 )
    , status_note_ ( font_, "", 18 )
    , leaderboard_title_ ( font_, "Top leaderboard", 22 )
    , leaderboard_empty_ ( font_, "", 18 )
    , prompt_ ( font_, "[Enter] Retry   [Backspace] Menu", 20 )
{
    title_.setFillColor ( sf::Color::White );
    score_text_.setFillColor ( sf::Color::White );
    status_note_.setFillColor ( sf::Color ( 230, 245, 255 ) );
    leaderboard_title_.setFillColor ( sf::Color ( 224, 240, 255 ) );
    leaderboard_empty_.setFillColor ( sf::Color ( 196, 214, 232 ) );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
}

void ResultScene::set_result ( const LevelResult& result )
{
    result_ = result;
    star_clock_.restart();

    title_.setString ( result_.win ? "Level Complete!" : "Level Failed" );
    title_.setFillColor ( result_.win ? sf::Color ( 50, 200, 50 ) : sf::Color ( 200, 50, 50 ) );

    score_text_.setString ( "Score: " + std::to_string ( result_.score ) );
    if ( result_.win )
    {
        status_note_.setString ( result_.leaderboard.empty()
            ? "Result saved locally. Leaderboard unavailable."
            : "Result saved to leaderboard." );
    }
    else
    {
        status_note_.setString ( "Level not completed: stars are not saved." );
    }
    leaderboard_empty_.setString ( result_.win
        ? "Leaderboard unavailable"
        : "No data" );
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
    static sf::Clock anim_clock;
    const float t     = anim_clock.getElapsedTime().asSeconds();
    const float pulse = 0.5f + 0.5f * std::sin ( t * 2.8f );
    auto ws = sf::Vector2f ( window.getSize() );

    if ( result_.win )
        draw_vertical_gradient ( window, sf::Color ( 18, 42, 64 ), sf::Color ( 42, 96, 118 ) );
    else
        draw_vertical_gradient ( window, sf::Color ( 34, 22, 34 ), sf::Color ( 88, 38, 58 ) );

    sf::CircleShape glow ( 240.f );
    glow.setOrigin ( {240.f, 240.f} );
    glow.setPosition ( {ws.x * 0.78f + std::sin ( t * 0.35f ) * 32.f, ws.y * 0.20f} );
    glow.setFillColor ( result_.win ? sf::Color ( 255, 236, 156, 42 )
                                    : sf::Color ( 255, 162, 176, 36 ) );
    window.draw ( glow );

    sf::RectangleShape panel ( {ws.x * 0.52f, ws.y * 0.70f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {ws.x * 0.5f, ws.y * 0.47f} );
    panel.setFillColor ( sf::Color ( 10, 15, 30, 145 ) );
    panel.setOutlineThickness ( 2.5f );
    panel.setOutlineColor ( result_.win ? sf::Color ( 210, 236, 255, 138 )
                                        : sf::Color ( 255, 195, 210, 132 ) );
    window.draw ( panel );

    sf::RectangleShape panel_accent ( {panel.getSize().x - 8.f, 6.f} );
    panel_accent.setOrigin ( {panel_accent.getSize().x * 0.5f, 0.f} );
    panel_accent.setPosition ( {panel.getPosition().x,
                                 panel.getPosition().y - panel.getSize().y * 0.5f + 7.f} );
    panel_accent.setFillColor ( result_.win ? sf::Color ( 132, 232, 186, 160 )
                                            : sf::Color ( 255, 148, 168, 152 ) );
    window.draw ( panel_accent );

    auto center_text = [&] ( sf::Text& text, float y )
    {
        auto bounds = text.getLocalBounds();
        text.setOrigin ( {bounds.position.x + bounds.size.x / 2.f,
                          bounds.position.y + bounds.size.y / 2.f} );
        text.setPosition ( {ws.x / 2.f, y} );
    };

    title_.setStyle ( sf::Text::Bold );
    title_.setOutlineThickness ( 2.f );
    title_.setOutlineColor ( sf::Color ( 10, 16, 28, 160 ) );
    center_text ( title_, ws.y * 0.25f + std::sin ( t * 1.4f ) * 2.f );

    // Animated stars — staggered bounce-in, 0.18s apart
    {
        const float star_y      = ws.y * 0.42f;
        const float star_r      = ws.y * 0.055f;
        const float spacing     = star_r * 2.8f;
        const float total_w     = spacing * 2.f;
        const float star_anim_t = star_clock_.getElapsedTime().asSeconds();

        for ( int i = 0; i < 3; ++i )
        {
            const float delay    = static_cast<float> ( i ) * 0.22f;
            const float local_t  = std::clamp ( ( star_anim_t - delay ) / 0.45f, 0.f, 1.f );
            const float scale    = bounce_in ( local_t );
            const float cx       = ws.x * 0.5f - total_w * 0.5f + static_cast<float> ( i ) * spacing;
            const bool  filled   = ( i < result_.stars );

            // Slight upward pop during bounce
            const float pop_y    = ( 1.f - scale ) * star_r * 0.6f;

            // Draw scaled by transforming origin
            sf::Transform tr;
            tr.translate ( { cx, star_y + pop_y } );
            tr.scale ( { scale, scale } );

            // Temporarily draw via transformed render states
            sf::RenderStates rs;
            rs.transform = tr;

            const int   pts      = 10;
            const float outer_r  = star_r;
            const float inner_r  = star_r * 0.42f;
            const float aoff     = -3.14159f / 2.f;

            sf::ConvexShape star ( pts );
            for ( int p = 0; p < pts; ++p )
            {
                const float angle = aoff + static_cast<float> ( p ) * 3.14159f / 5.f;
                const float r     = ( p % 2 == 0 ) ? outer_r : inner_r;
                star.setPoint ( p, { std::cos ( angle ) * r, std::sin ( angle ) * r } );
            }

            if ( filled )
            {
                const uint8_t br = static_cast<uint8_t> ( 220.f + pulse * 35.f );
                star.setFillColor ( result_.win
                    ? sf::Color ( br, static_cast<uint8_t> ( br * 0.89f ), 60, 255 )
                    : sf::Color ( br, static_cast<uint8_t> ( br * 0.78f ), 80, 255 ) );
                star.setOutlineThickness ( 1.5f / scale );
                star.setOutlineColor ( sf::Color ( 255, 200, 20, 180 ) );

                // Glow behind
                sf::CircleShape gl ( star_r * 1.0f );
                gl.setOrigin ( {star_r * 1.0f, star_r * 1.0f} );
                const uint8_t ga = static_cast<uint8_t> ( 30.f + pulse * 25.f );
                gl.setFillColor ( sf::Color ( 255, 230, 80, ga ) );
                window.draw ( gl, rs );
            }
            else
            {
                star.setFillColor ( sf::Color ( 50, 60, 85, 160 ) );
                star.setOutlineThickness ( 1.5f / std::max ( scale, 0.01f ) );
                star.setOutlineColor ( sf::Color ( 90, 110, 140, 100 ) );
            }

            window.draw ( star, rs );
        }
    }

    score_text_.setFillColor ( sf::Color ( 236, 246, 255 ) );
    status_note_.setFillColor ( result_.win
        ? sf::Color ( 170, 235, 196 )
        : sf::Color ( 255, 200, 210 ) );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255,
                                       static_cast<uint8_t> ( 182.f + 70.f * pulse ) ) );

    center_text ( score_text_, ws.y * 0.57f );
    center_text ( status_note_, ws.y * 0.63f );
    center_text ( leaderboard_title_, ws.y * 0.70f );
    center_text ( prompt_, ws.y * 0.84f );

    window.draw ( title_ );
    window.draw ( score_text_ );
    window.draw ( status_note_ );
    window.draw ( leaderboard_title_ );

    if ( result_.leaderboard.empty() )
    {
        center_text ( leaderboard_empty_, ws.y * 0.76f );
        window.draw ( leaderboard_empty_ );
    }
    else
    {
        const std::size_t max_rows = std::min<std::size_t> ( 5u, result_.leaderboard.size() );
        for ( std::size_t i = 0; i < max_rows; ++i )
        {
            const LeaderboardEntry& entry = result_.leaderboard[i];
            const std::string name = entry.playerName.empty() ? "Player" : entry.playerName;
            const std::string row_text =
                std::to_string ( static_cast<int> ( i + 1u ) ) + ". " + name
                + "   score: " + std::to_string ( entry.score )
                + "   stars: " + std::to_string ( std::clamp ( entry.stars, 0, 3 ) );

            sf::Text row ( font_, row_text, 18 );
            row.setFillColor ( sf::Color ( 224, 240, 255 ) );
            center_text ( row, ws.y * ( 0.75f + static_cast<float> ( i ) * 0.042f ) );
            window.draw ( row );
        }
    }

    window.draw ( prompt_ );
}

}  // namespace angry
