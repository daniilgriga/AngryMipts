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

float bounce_in ( float t )
{
    if ( t <= 0.f ) return 0.f;
    if ( t >= 1.f ) return 1.f;
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
    , prompt_ ( font_, "[Enter] Retry   [Backspace] Menu", 20 )
    , lb_title_ ( font_, "Leaderboard", 22 )
    , lb_empty_ ( font_, "", 18 )
{
    title_.setFillColor ( sf::Color::White );
    score_text_.setFillColor ( sf::Color::White );
    status_note_.setFillColor ( sf::Color ( 230, 245, 255 ) );
    lb_title_.setFillColor ( sf::Color ( 224, 240, 255 ) );
    lb_empty_.setFillColor ( sf::Color ( 196, 214, 232 ) );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
}

void ResultScene::set_result ( const LevelResult& result )
{
    result_    = result;
    lb_scroll_ = 0.f;
    star_clock_.restart();

    title_.setString ( result_.win ? "Level Complete!" : "Level Failed" );
    title_.setFillColor ( result_.win ? sf::Color ( 50, 200, 50 ) : sf::Color ( 200, 50, 50 ) );

    score_text_.setString ( "Score: " + std::to_string ( result_.score ) );

    if ( result_.win )
    {
        if ( result_.logged_in && result_.fetch_status == LeaderboardFetchStatus::Ok )
        {
            status_note_.setString ( "Result saved to leaderboard." );
            status_note_.setFillColor ( sf::Color ( 100, 220, 140 ) );
        }
        else if ( !result_.logged_in )
        {
            status_note_.setString ( "Login required for online result submission." );
            status_note_.setFillColor ( sf::Color ( 210, 160, 40 ) );
        }
        else
        {
            status_note_.setString ( "Leaderboard unavailable." );
            status_note_.setFillColor ( sf::Color ( 230, 130, 60 ) );
        }
    }
    else
    {
        status_note_.setString ( "Level not completed: stars are not saved." );
        status_note_.setFillColor ( sf::Color ( 255, 200, 210 ) );
    }

    if ( result_.fetch_status == LeaderboardFetchStatus::Empty )
        lb_empty_.setString ( "No scores yet" );
    else if ( result_.fetch_status != LeaderboardFetchStatus::Ok )
        lb_empty_.setString ( "Leaderboard unavailable" );
    else
        lb_empty_.setString ( "" );
}

SceneId ResultScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Enter )
            return SceneId::Game;
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;
        if ( key->code == sf::Keyboard::Key::Up )
            lb_scroll_ = std::max ( 0.f, lb_scroll_ - 36.f );
        if ( key->code == sf::Keyboard::Key::Down )
            lb_scroll_ += 36.f;
    }
    if ( const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>() )
        lb_scroll_ = std::max ( 0.f, lb_scroll_ - wheel->delta * 36.f );
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

    // Animated background glow
    sf::CircleShape glow ( 260.f );
    glow.setOrigin ( {260.f, 260.f} );
    glow.setPosition ( {ws.x * 0.25f + std::sin ( t * 0.3f ) * 30.f, ws.y * 0.5f} );
    glow.setFillColor ( result_.win ? sf::Color ( 130, 200, 255, 18 )
                                    : sf::Color ( 255, 140, 160, 16 ) );
    window.draw ( glow );

    // ── Layout constants ───────────────────────────────────────────────────
    const float gap       = ws.x * 0.02f;
    const float left_w    = ws.x * 0.46f;
    const float right_w   = ws.x * 0.44f;
    const float panel_h   = ws.y * 0.82f;
    const float panel_top = ws.y * 0.09f;

    const float left_cx  = ws.x * 0.02f + left_w * 0.5f;
    const float right_x  = ws.x * 0.02f + left_w + gap;
    const float right_cx = right_x + right_w * 0.5f;

    // ── Left panel ─────────────────────────────────────────────────────────
    {
        sf::RectangleShape shadow ( {left_w, panel_h} );
        shadow.setPosition ( {ws.x * 0.02f + 6.f, panel_top + 8.f} );
        shadow.setFillColor ( sf::Color ( 4, 8, 16, 100 ) );
        window.draw ( shadow );

        sf::RectangleShape panel ( {left_w, panel_h} );
        panel.setPosition ( {ws.x * 0.02f, panel_top} );
        panel.setFillColor ( sf::Color ( 10, 15, 30, 148 ) );
        panel.setOutlineThickness ( 2.f );
        panel.setOutlineColor ( result_.win ? sf::Color ( 210, 236, 255, 130 )
                                            : sf::Color ( 255, 195, 210, 124 ) );
        window.draw ( panel );

        sf::RectangleShape accent ( {left_w - 8.f, 5.f} );
        accent.setPosition ( {ws.x * 0.02f + 4.f, panel_top + 6.f} );
        accent.setFillColor ( result_.win ? sf::Color ( 132, 232, 186, 155 )
                                          : sf::Color ( 255, 148, 168, 148 ) );
        window.draw ( accent );
    }

    auto left_center = [&] ( sf::Text& text, float y )
    {
        auto b = text.getLocalBounds();
        text.setOrigin ( {b.position.x + b.size.x / 2.f,
                          b.position.y + b.size.y / 2.f} );
        text.setPosition ( {left_cx, y} );
    };

    // Title
    title_.setStyle ( sf::Text::Bold );
    title_.setOutlineThickness ( 2.f );
    title_.setOutlineColor ( sf::Color ( 10, 16, 28, 160 ) );
    left_center ( title_, panel_top + panel_h * 0.14f + std::sin ( t * 1.4f ) * 2.f );
    window.draw ( title_ );

    // Stars — bounce-in
    {
        const float star_y      = panel_top + panel_h * 0.36f;
        const float star_r      = ws.y * 0.055f;
        const float spacing     = star_r * 2.8f;
        const float total_w     = spacing * 2.f;
        const float anim_t      = star_clock_.getElapsedTime().asSeconds();

        for ( int i = 0; i < 3; ++i )
        {
            const float delay   = static_cast<float> ( i ) * 0.22f;
            const float local_t = std::clamp ( ( anim_t - delay ) / 0.45f, 0.f, 1.f );
            const float scale   = bounce_in ( local_t );
            const float cx      = left_cx - total_w * 0.5f + static_cast<float> ( i ) * spacing;
            const bool  filled  = ( i < result_.stars );
            const float pop_y   = ( 1.f - scale ) * star_r * 0.6f;

            sf::Transform tr;
            tr.translate ( { cx, star_y + pop_y } );
            tr.scale ( { scale, scale } );
            sf::RenderStates rs;
            rs.transform = tr;

            const int   pts     = 10;
            const float outer_r = star_r;
            const float inner_r = star_r * 0.42f;
            const float aoff    = -3.14159f / 2.f;

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

                sf::CircleShape gl ( star_r );
                gl.setOrigin ( {star_r, star_r} );
                gl.setFillColor ( sf::Color ( 255, 230, 80,
                                              static_cast<uint8_t> ( 30.f + pulse * 25.f ) ) );
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

    // Score + status
    score_text_.setFillColor ( sf::Color ( 236, 246, 255 ) );
    left_center ( score_text_,  panel_top + panel_h * 0.56f );
    left_center ( status_note_, panel_top + panel_h * 0.65f );
    window.draw ( score_text_ );
    window.draw ( status_note_ );

    // Prompt
    prompt_.setFillColor ( sf::Color ( 230, 245, 255,
                                       static_cast<uint8_t> ( 182.f + 70.f * pulse ) ) );
    left_center ( prompt_, panel_top + panel_h * 0.88f );
    window.draw ( prompt_ );

    // ── Right panel — Leaderboard ──────────────────────────────────────────
    {
        sf::RectangleShape shadow ( {right_w, panel_h} );
        shadow.setPosition ( {right_x + 6.f, panel_top + 8.f} );
        shadow.setFillColor ( sf::Color ( 4, 8, 16, 100 ) );
        window.draw ( shadow );

        sf::RectangleShape panel ( {right_w, panel_h} );
        panel.setPosition ( {right_x, panel_top} );
        panel.setFillColor ( sf::Color ( 8, 14, 26, 148 ) );
        panel.setOutlineThickness ( 2.f );
        panel.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
        window.draw ( panel );

        sf::RectangleShape accent ( {right_w - 8.f, 5.f} );
        accent.setPosition ( {right_x + 4.f, panel_top + 6.f} );
        accent.setFillColor ( sf::Color ( 80, 160, 255, 145 ) );
        window.draw ( accent );
    }

    // Leaderboard title (fixed, not scrolled)
    {
        auto b = lb_title_.getLocalBounds();
        lb_title_.setOrigin ( {b.position.x + b.size.x / 2.f,
                               b.position.y + b.size.y / 2.f} );
        lb_title_.setPosition ( {right_cx, panel_top + 28.f} );
        window.draw ( lb_title_ );
    }

    // Scrollable list area
    const float list_top    = panel_top + 58.f;
    const float list_bottom = panel_top + panel_h - 14.f;
    const float list_h      = list_bottom - list_top;
    const float row_step    = 36.f;

    if ( result_.fetch_status != LeaderboardFetchStatus::Ok )
    {
        // Status message
        auto b = lb_empty_.getLocalBounds();
        lb_empty_.setOrigin ( {b.position.x + b.size.x / 2.f,
                               b.position.y + b.size.y / 2.f} );
        lb_empty_.setPosition ( {right_cx, list_top + list_h * 0.4f} );
        window.draw ( lb_empty_ );
    }
    else if ( result_.leaderboard.empty() )
    {
        lb_empty_.setString ( "No scores yet" );
        auto b = lb_empty_.getLocalBounds();
        lb_empty_.setOrigin ( {b.position.x + b.size.x / 2.f,
                               b.position.y + b.size.y / 2.f} );
        lb_empty_.setPosition ( {right_cx, list_top + list_h * 0.4f} );
        window.draw ( lb_empty_ );
    }
    else
    {
        const float total_h  = static_cast<float> ( result_.leaderboard.size() ) * row_step;
        const float max_sc   = std::max ( 0.f, total_h - list_h );
        lb_scroll_ = std::clamp ( lb_scroll_, 0.f, max_sc );

        // Set clip view
        sf::View clip ( sf::FloatRect ( {0.f, list_top}, {ws.x, list_h} ) );
        clip.setViewport ( sf::FloatRect (
            {0.f, list_top / ws.y},
            {1.f, list_h  / ws.y} ) );
        window.setView ( clip );

        for ( std::size_t i = 0; i < result_.leaderboard.size(); ++i )
        {
            const float y = list_top + static_cast<float> ( i ) * row_step
                            + row_step * 0.5f - lb_scroll_;

            if ( y + row_step < list_top || y - row_step > list_bottom )
                continue;

            const LeaderboardEntry& entry = result_.leaderboard[i];
            const std::string name = entry.playerName.empty() ? "Player" : entry.playerName;

            // Highlight top-3
            if ( i < 3 )
            {
                static const sf::Color rank_colors[3] = {
                    sf::Color ( 255, 210, 40, 55 ),
                    sf::Color ( 210, 215, 220, 38 ),
                    sf::Color ( 200, 140, 80, 38 ),
                };
                sf::RectangleShape row_bg ( {right_w - 16.f, row_step - 4.f} );
                row_bg.setOrigin ( {( right_w - 16.f ) * 0.5f, ( row_step - 4.f ) * 0.5f} );
                row_bg.setPosition ( {right_cx, y} );
                row_bg.setFillColor ( rank_colors[i] );
                window.draw ( row_bg );
            }

            const std::string label =
                std::to_string ( static_cast<int> ( i + 1u ) ) + ". "
                + name
                + "   " + std::to_string ( entry.score ) + " pts"
                + "   " + std::string ( static_cast<std::size_t> ( std::clamp ( entry.stars, 0, 3 ) ), '*' );

            sf::Text row ( font_, label, 17 );
            // Тinting: gold for #1, silver for #2, bronze for #3, dim for rest
            if      ( i == 0 ) row.setFillColor ( sf::Color ( 255, 220, 60 ) );
            else if ( i == 1 ) row.setFillColor ( sf::Color ( 210, 218, 228 ) );
            else if ( i == 2 ) row.setFillColor ( sf::Color ( 210, 155, 95 ) );
            else               row.setFillColor ( sf::Color ( 200, 218, 238 ) );

            auto b = row.getLocalBounds();
            row.setOrigin ( {b.position.x + b.size.x / 2.f,
                             b.position.y + b.size.y / 2.f} );
            row.setPosition ( {right_cx, y} );
            window.draw ( row );
        }

        window.setView ( window.getDefaultView() );

        // Scroll hint — show only when list overflows
        if ( total_h > list_h )
        {
            sf::Text hint ( font_, "[Scroll] to navigate", 14 );
            hint.setFillColor ( sf::Color ( 160, 190, 220, 160 ) );
            auto b = hint.getLocalBounds();
            hint.setOrigin ( {b.position.x + b.size.x / 2.f, b.position.y} );
            hint.setPosition ( {right_cx, list_bottom + 2.f} );
            window.draw ( hint );
        }
    }

    window.setView ( window.getDefaultView() );
}

}  // namespace angry
