// ============================================================
// menu_scene.cpp — Main menu scene implementation.
// Part of: angry::ui
//
// Implements top-level menu behavior:
//   * Renders title/prompt and account badge controls
//   * Handles pointer/keyboard navigation events
//   * Produces scene transitions to login/level select
//   * Keeps visual menu state and hit regions
// ============================================================

#include "ui/menu_scene.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#endif

#include <cstdint>
#include <cmath>

namespace angry
{
namespace
{

#ifndef __EMSCRIPTEN__

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

void draw_soft_glow ( sf::RenderWindow& window, sf::Vector2f pos, float radius,
                      sf::Color color )
{
    sf::CircleShape glow ( radius );
    glow.setOrigin ( {radius, radius} );
    glow.setPosition ( pos );
    glow.setFillColor ( color );
    window.draw ( glow );
}

#else  // __EMSCRIPTEN__

void draw_vertical_gradient ( platform::Window& /*window*/,
                               platform::Color top, platform::Color bottom,
                               int w, int h )
{
    DrawRectangleGradientV( 0, 0, w, h, top.to_rl(), bottom.to_rl() );
}

void draw_soft_glow ( platform::Vec2f pos, float radius, platform::Color color )
{
    DrawCircle( int(pos.x), int(pos.y), radius, color.to_rl() );
}

#endif

}  // namespace

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

MenuScene::MenuScene ( const platform::Font& font, AccountService& accounts )
    : accounts_ ( accounts )
    , font_( font )
#ifndef __EMSCRIPTEN__
    , title_( font_, "AngryMipts", 64 )
    , prompt_( font_, "Press Enter to start", 24 )
    , badge_text_( font_, "", 18 )
    , badge_btn_( font_, "", 16 )
#endif
{
#ifndef __EMSCRIPTEN__
    title_.setFillColor ( sf::Color::White );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
#else
    title_.font_ = &font_;
    title_.string_ = "AngryMipts";
    title_.char_size_ = 64;
    prompt_.font_ = &font_;
    prompt_.string_ = "Press Enter to start";
    prompt_.char_size_ = 24;
    badge_text_.font_ = &font_;
    badge_btn_.font_  = &font_;
#endif
}

// #=# Input Handling #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

SceneId MenuScene::handle_input ( const platform::Event& event )
{
#ifndef __EMSCRIPTEN__
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Enter )
            return SceneId::LevelSelect;

        if ( key->code == sf::Keyboard::Key::L )
        {
            if ( accounts_.is_logged_in() )
                accounts_.logout();
            else
                return SceneId::Login;
        }
    }

    if ( const auto* click = event.getIf<sf::Event::MouseButtonPressed>() )
    {
        if ( click->button == sf::Mouse::Button::Left )
        {
            const sf::Vector2f pos ( static_cast<float> ( click->position.x ),
                                     static_cast<float> ( click->position.y ) );
            if ( rect_prompt_.contains ( pos ) )
                return SceneId::LevelSelect;

            if ( rect_badge_btn_.contains ( pos ) )
            {
                if ( accounts_.is_logged_in() )
                    accounts_.logout();
                else
                    return SceneId::Login;
            }
        }
    }

#else  // __EMSCRIPTEN__

    if ( const auto* key = std::get_if<platform::KeyEvent>( &event ) )
    {
        // Raylib: KEY_ENTER=257, KEY_L=76
        if ( key->key == KEY_ENTER )
            return SceneId::LevelSelect;
        if ( key->key == KEY_L )
        {
            if ( accounts_.is_logged_in() )
                accounts_.logout();
            else
                return SceneId::Login;
        }
    }

    if ( const auto* click = std::get_if<platform::MouseBtnEvent>( &event ) )
    {
        if ( click->button == 0 && click->pressed )
        {
            platform::Vec2f pos { click->x, click->y };
            if ( rect_prompt_.contains( pos ) )
                return SceneId::LevelSelect;
            if ( rect_badge_btn_.contains( pos ) )
            {
                if ( accounts_.is_logged_in() )
                    accounts_.logout();
                else
                    return SceneId::Login;
            }
        }
    }

#endif

    return SceneId::None;
}

void MenuScene::update()
{
}

// #=# Rendering #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void MenuScene::render( platform::Window& window )
{
#ifndef __EMSCRIPTEN__
    static sf::Clock anim_clock;
    const float t = anim_clock.getElapsedTime().asSeconds();
    const float pulse = 0.5f + 0.5f * std::sin ( t * 2.1f );
    const auto window_size = sf::Vector2f ( window.getSize() );

    draw_vertical_gradient ( window, sf::Color ( 18, 30, 60 ), sf::Color ( 44, 112, 120 ) );

    draw_soft_glow ( window,
                     {window_size.x * 0.82f + std::sin ( t * 0.22f ) * 40.f,
                      window_size.y * 0.20f},
                     210.f, sf::Color ( 255, 240, 190, 45 ) );
    draw_soft_glow ( window,
                     {window_size.x * 0.23f + std::sin ( t * 0.35f ) * 55.f,
                      window_size.y * 0.30f + std::cos ( t * 0.43f ) * 22.f},
                     250.f, sf::Color ( 160, 210, 255, 24 ) );
    draw_soft_glow ( window,
                     {window_size.x * 0.70f + std::cos ( t * 0.31f ) * 35.f,
                      window_size.y * 0.68f},
                     320.f, sf::Color ( 110, 170, 225, 20 ) );

    sf::RectangleShape panel_shadow ( {window_size.x * 0.60f, window_size.y * 0.44f} );
    panel_shadow.setOrigin ( {panel_shadow.getSize().x * 0.5f, panel_shadow.getSize().y * 0.5f} );
    panel_shadow.setPosition ( {window_size.x * 0.5f + 8.f, window_size.y * 0.43f + 10.f} );
    panel_shadow.setFillColor ( sf::Color ( 5, 10, 18, 110 ) );
    window.draw ( panel_shadow );

    sf::RectangleShape panel ( {window_size.x * 0.60f, window_size.y * 0.44f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {window_size.x * 0.5f, window_size.y * 0.43f} );
    panel.setFillColor ( sf::Color ( 8, 15, 28, 138 ) );
    panel.setOutlineThickness ( 2.5f );
    panel.setOutlineColor ( sf::Color ( 200, 230, 255, 120 ) );
    window.draw ( panel );

    sf::RectangleShape panel_accent ( {panel.getSize().x - 8.f, 6.f} );
    panel_accent.setOrigin ( {panel_accent.getSize().x * 0.5f, 0.f} );
    panel_accent.setPosition ( {panel.getPosition().x, panel.getPosition().y - panel.getSize().y * 0.5f + 6.f} );
    panel_accent.setFillColor ( sf::Color ( 130, 195, 255, 145 ) );
    window.draw ( panel_accent );

    title_.setCharacterSize ( 74 );
    title_.setStyle ( sf::Text::Bold );
    title_.setFillColor ( sf::Color ( 242, 248, 255 ) );
    title_.setOutlineThickness ( 3.f );
    title_.setOutlineColor ( sf::Color ( 22, 34, 58, 170 ) );
    auto title_bounds = title_.getLocalBounds();
    title_.setOrigin ( {title_bounds.position.x + title_bounds.size.x / 2.f,
                        title_bounds.position.y + title_bounds.size.y / 2.f} );
    title_.setPosition (
        {window_size.x / 2.f, window_size.y * 0.33f + std::sin ( t * 1.1f ) * 3.5f} );

    const uint8_t prompt_alpha =
        static_cast<uint8_t> ( 185.f + 70.f * pulse );
    prompt_.setFillColor ( sf::Color ( 232, 244, 255, prompt_alpha ) );
    prompt_.setStyle ( sf::Text::Regular );
    auto prompt_bounds = prompt_.getLocalBounds();
    prompt_.setOrigin ( {prompt_bounds.position.x + prompt_bounds.size.x / 2.f,
                         prompt_bounds.position.y + prompt_bounds.size.y / 2.f} );
    prompt_.setPosition ( {window_size.x / 2.f, window_size.y * 0.54f} );

    window.draw ( title_ );
    window.draw ( prompt_ );

    {
        const auto pb = prompt_.getGlobalBounds();
        rect_prompt_ = sf::FloatRect ( {pb.position.x - 20.f, pb.position.y - 10.f},
                                      {pb.size.x + 40.f, pb.size.y + 20.f} );
    }

    const float badge_right  = window_size.x - 18.f;
    const float badge_top    = 14.f;
    const float pill_w = 280.f;
    const float pill_h = 54.f;
    sf::RectangleShape pill ( {pill_w, pill_h} );
    pill.setPosition ( {badge_right - pill_w, badge_top} );
    pill.setFillColor ( sf::Color ( 10, 18, 36, 200 ) );
    pill.setOutlineThickness ( 1.5f );
    pill.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
    window.draw ( pill );

    if ( accounts_.is_logged_in() )
    {
        badge_text_.setString ( "Logged in as " + accounts_.username() );
        badge_text_.setFillColor ( sf::Color ( 100, 220, 140 ) );
        badge_btn_.setString ( "[L] Logout" );
        badge_btn_.setFillColor ( sf::Color ( 255, 120, 120 ) );
    }
    else
    {
        badge_text_.setString ( "Guest mode" );
        badge_text_.setFillColor ( sf::Color ( 200, 180, 100 ) );
        badge_btn_.setString ( "[L] Login" );
        badge_btn_.setFillColor ( sf::Color ( 80, 160, 255 ) );
    }

    badge_text_.setCharacterSize ( 18 );
    auto bt_bounds = badge_text_.getLocalBounds();
    badge_text_.setOrigin ( {bt_bounds.position.x, bt_bounds.position.y} );
    badge_text_.setPosition ( {badge_right - pill_w + 12.f, badge_top + 6.f} );
    window.draw ( badge_text_ );

    badge_btn_.setCharacterSize ( 16 );
    auto bb_bounds = badge_btn_.getLocalBounds();
    badge_btn_.setOrigin ( {bb_bounds.position.x, bb_bounds.position.y} );
    badge_btn_.setPosition ( {badge_right - pill_w + 12.f, badge_top + 30.f} );
    window.draw ( badge_btn_ );

    rect_badge_btn_ = sf::FloatRect ( {badge_right - pill_w, badge_top}, {pill_w, pill_h} );

#else  // __EMSCRIPTEN__ — Raylib render

    static platform::Clock anim_clock;
    const float t = anim_clock.getElapsedTime().asSeconds();
    const float pulse = 0.5f + 0.5f * std::sin ( t * 2.1f );
    const platform::Vec2u ws = window.getSize();
    const float W = float(ws.x), H = float(ws.y);

    draw_vertical_gradient( window,
                             platform::Color(18,30,60), platform::Color(44,112,120),
                             int(W), int(H) );
    draw_soft_glow( {W*0.82f + std::sin(t*0.22f)*40.f, H*0.20f},
                    210.f, platform::Color(255,240,190,45) );
    draw_soft_glow( {W*0.23f + std::sin(t*0.35f)*55.f, H*0.30f + std::cos(t*0.43f)*22.f},
                    250.f, platform::Color(160,210,255,24) );
    draw_soft_glow( {W*0.70f + std::cos(t*0.31f)*35.f, H*0.68f},
                    320.f, platform::Color(110,170,225,20) );

    // Panel
    DrawRectangle( int(W*0.2f+8), int(H*0.21f+10), int(W*0.6f), int(H*0.44f),
                   platform::Color(5,10,18,110).to_rl() );
    DrawRectangle( int(W*0.2f), int(H*0.21f), int(W*0.6f), int(H*0.44f),
                   platform::Color(8,15,28,138).to_rl() );
    DrawRectangleLinesEx( {W*0.2f, H*0.21f, W*0.6f, H*0.44f}, 2.5f,
                          platform::Color(200,230,255,120).to_rl() );

    // Title
    if ( font_.loaded )
    {
        ::Vector2 tsz = MeasureTextEx( font_.rl, "AngryMipts", 74.f, 1.f );
        DrawTextEx( font_.rl, "AngryMipts",
                    {W/2.f - tsz.x/2.f, H*0.33f + std::sin(t*1.1f)*3.5f - tsz.y/2.f},
                    74.f, 1.f, platform::Color(242,248,255).to_rl() );

        // Prompt
        const uint8_t pa = static_cast<uint8_t>( 185.f + 70.f * pulse );
        ::Vector2 psz = MeasureTextEx( font_.rl, "Press Enter to start", 24.f, 1.f );
        float px = W/2.f - psz.x/2.f, py = H*0.54f - psz.y/2.f;
        DrawTextEx( font_.rl, "Press Enter to start",
                    {px, py}, 24.f, 1.f, ::Color{232,244,255,pa} );
        rect_prompt_ = { px-20.f, py-10.f, psz.x+40.f, psz.y+20.f };

        // Badge
        const float badge_right = W - 18.f, badge_top = 14.f;
        const float pill_w = 280.f, pill_h = 54.f;
        DrawRectangle( int(badge_right-pill_w), int(badge_top),
                       int(pill_w), int(pill_h),
                       platform::Color(10,18,36,200).to_rl() );
        DrawRectangleLinesEx( {badge_right-pill_w, badge_top, pill_w, pill_h}, 1.5f,
                              platform::Color(80,140,220,110).to_rl() );

        std::string badge_str = accounts_.is_logged_in()
            ? "Logged in as " + accounts_.username() : "Guest mode";
        ::Color badge_col = accounts_.is_logged_in()
            ? platform::Color(100,220,140).to_rl() : platform::Color(200,180,100).to_rl();
        DrawTextEx( font_.rl, badge_str.c_str(),
                    {badge_right-pill_w+12.f, badge_top+6.f}, 18.f, 1.f, badge_col );

        std::string btn_str = accounts_.is_logged_in() ? "[L] Logout" : "[L] Login";
        ::Color btn_col = accounts_.is_logged_in()
            ? platform::Color(255,120,120).to_rl() : platform::Color(80,160,255).to_rl();
        DrawTextEx( font_.rl, btn_str.c_str(),
                    {badge_right-pill_w+12.f, badge_top+30.f}, 16.f, 1.f, btn_col );

        rect_badge_btn_ = { badge_right-pill_w, badge_top, pill_w, pill_h };
    }

#endif
}

}  // namespace angry
