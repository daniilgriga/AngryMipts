#include "ui/login_scene.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace angry
{
namespace
{

constexpr float kCardW    = 480.f;
constexpr float kCardH    = 420.f;
constexpr float kFieldW   = 360.f;
constexpr float kFieldH   = 46.f;
constexpr int   kMaxLen   = 24;

#ifndef __EMSCRIPTEN__

void draw_gradient ( sf::RenderWindow& window, sf::Color top, sf::Color bottom )
{
    const sf::Vector2f ws ( window.getSize() );
    sf::Vertex bg[] = {
        {{0.f, 0.f},    top},
        {{ws.x, 0.f},   top},
        {{ws.x, ws.y},  bottom},
        {{0.f, ws.y},   bottom},
    };
    window.draw ( bg, 4, sf::PrimitiveType::TriangleFan );
}

void draw_glow ( sf::RenderWindow& window, sf::Vector2f pos, float r, sf::Color c )
{
    sf::CircleShape g ( r );
    g.setOrigin ( {r, r} );
    g.setPosition ( pos );
    g.setFillColor ( c );
    window.draw ( g );
}

sf::Color status_color ( int kind )
{
    switch ( kind )
    {
    case 1: return sf::Color ( 200, 220, 255 );
    case 2: return sf::Color ( 255, 120, 120 );
    case 3: return sf::Color ( 100, 220, 140 );
    default: return sf::Color ( 180, 200, 230 );
    }
}

#else  // __EMSCRIPTEN__

void draw_gradient ( platform::Window& /*w*/, platform::Color top, platform::Color bottom,
                     int W, int H )
{
    DrawRectangleGradientV( 0, 0, W, H, top.to_rl(), bottom.to_rl() );
}

void draw_glow ( platform::Vec2f pos, float r, platform::Color c )
{
    DrawCircle( int(pos.x), int(pos.y), r, c.to_rl() );
}

platform::Color status_color ( int kind )
{
    switch ( kind )
    {
    case 1: return {200,220,255};
    case 2: return {255,120,120};
    case 3: return {100,220,140};
    default: return {180,200,230};
    }
}

#endif

}  // namespace

LoginScene::LoginScene ( const platform::Font& font, AccountService& accounts )
    : accounts_ ( accounts )
    , font_ ( font )
{
}

void LoginScene::do_login()
{
    if ( username_buf_.empty() || password_buf_.empty() )
    {
        status_msg_  = "Enter username and password";
        status_kind_ = StatusKind::Error;
        status_clock_.restart();
        return;
    }
    status_msg_  = "Logging in...";
    status_kind_ = StatusKind::Pending;
    status_clock_.restart();

    const AuthResult r = accounts_.login ( username_buf_, password_buf_ );
    if ( r.success )
    {
        status_msg_  = "Login successful";
        status_kind_ = StatusKind::Success;
    }
    else
    {
        status_msg_  = r.errorMessage.empty() ? "Invalid username or password"
                                              : r.errorMessage;
        status_kind_ = StatusKind::Error;
    }
    status_clock_.restart();
}

void LoginScene::do_register()
{
    if ( username_buf_.empty() || password_buf_.empty() )
    {
        status_msg_  = "Enter username and password";
        status_kind_ = StatusKind::Error;
        status_clock_.restart();
        return;
    }
    status_msg_  = "Creating account...";
    status_kind_ = StatusKind::Pending;
    status_clock_.restart();

    const AuthResult r = accounts_.registerAndLogin ( username_buf_, password_buf_ );
    if ( r.success )
    {
        status_msg_  = "Registration successful";
        status_kind_ = StatusKind::Success;
    }
    else
    {
        status_msg_  = r.errorMessage.empty() ? "Registration failed" : r.errorMessage;
        status_kind_ = StatusKind::Error;
    }
    status_clock_.restart();
}

SceneId LoginScene::handle_input ( const platform::Event& event )
{
#ifndef __EMSCRIPTEN__

    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Tab )
        {
            focus_ = ( focus_ == FocusField::Username )
                         ? FocusField::Password
                         : FocusField::Username;
            return SceneId::None;
        }

        if ( key->code == sf::Keyboard::Key::Backspace )
        {
            auto& buf = ( focus_ == FocusField::Username ) ? username_buf_ : password_buf_;
            if ( !buf.empty() )
                buf.pop_back();
            return SceneId::None;
        }

        if ( key->code == sf::Keyboard::Key::Enter )
        {
            if ( focus_ == FocusField::Username )
            {
                focus_ = FocusField::Password;
                return SceneId::None;
            }
            do_login();
            if ( status_kind_ == StatusKind::Success )
                return SceneId::Menu;
            return SceneId::None;
        }

        if ( key->code == sf::Keyboard::Key::F1 )
        {
            do_register();
            if ( status_kind_ == StatusKind::Success )
                return SceneId::Menu;
            return SceneId::None;
        }

        if ( key->code == sf::Keyboard::Key::F2 )
            return SceneId::Menu;
    }

    if ( const auto* click = event.getIf<sf::Event::MouseButtonPressed>() )
    {
        if ( click->button == sf::Mouse::Button::Left )
        {
            const sf::Vector2f pos ( static_cast<float> ( click->position.x ),
                                     static_cast<float> ( click->position.y ) );
            if ( rect_field_username_.contains ( pos ) )
            {
                focus_ = FocusField::Username;
                caret_clock_.restart();
                return SceneId::None;
            }
            if ( rect_field_password_.contains ( pos ) )
            {
                focus_ = FocusField::Password;
                caret_clock_.restart();
                return SceneId::None;
            }
            if ( rect_btn_login_.contains ( pos ) )
            {
                do_login();
                if ( status_kind_ == StatusKind::Success )
                    return SceneId::Menu;
                return SceneId::None;
            }
            if ( rect_btn_register_.contains ( pos ) )
            {
                do_register();
                if ( status_kind_ == StatusKind::Success )
                    return SceneId::Menu;
                return SceneId::None;
            }
            if ( rect_btn_guest_.contains ( pos ) )
                return SceneId::Menu;
        }
    }

    if ( const auto* text = event.getIf<sf::Event::TextEntered>() )
    {
        const uint32_t ch = text->unicode;
        if ( ch >= 32u && ch < 127u )
        {
            auto& buf = ( focus_ == FocusField::Username ) ? username_buf_ : password_buf_;
            if ( static_cast<int> ( buf.size() ) < kMaxLen )
                buf += static_cast<char> ( ch );
        }
    }

#else  // __EMSCRIPTEN__

    if ( const auto* key = std::get_if<platform::KeyEvent>( &event ) )
    {
        if ( key->key == KEY_TAB )
        {
            focus_ = ( focus_ == FocusField::Username ) ? FocusField::Password
                                                        : FocusField::Username;
            return SceneId::None;
        }
        if ( key->key == KEY_BACKSPACE )
        {
            auto& buf = ( focus_ == FocusField::Username ) ? username_buf_ : password_buf_;
            if ( !buf.empty() ) buf.pop_back();
            return SceneId::None;
        }
        if ( key->key == KEY_ENTER || key->key == KEY_KP_ENTER )
        {
            if ( focus_ == FocusField::Username ) { focus_ = FocusField::Password; return SceneId::None; }
            do_login();
            if ( status_kind_ == StatusKind::Success ) return SceneId::Menu;
            return SceneId::None;
        }
        if ( key->key == KEY_F1 )
        {
            do_register();
            if ( status_kind_ == StatusKind::Success ) return SceneId::Menu;
            return SceneId::None;
        }
        if ( key->key == KEY_F2 ) return SceneId::Menu;
    }

    if ( const auto* click = std::get_if<platform::MouseBtnEvent>( &event ) )
    {
        if ( click->button == 0 && click->pressed )
        {
            platform::Vec2f pos { click->x, click->y };
            if ( rect_field_username_.contains( pos ) ) { focus_ = FocusField::Username; caret_clock_.restart(); return SceneId::None; }
            if ( rect_field_password_.contains( pos ) ) { focus_ = FocusField::Password; caret_clock_.restart(); return SceneId::None; }
            if ( rect_btn_login_.contains( pos ) )    { do_login();    if ( status_kind_ == StatusKind::Success ) return SceneId::Menu; return SceneId::None; }
            if ( rect_btn_register_.contains( pos ) ) { do_register(); if ( status_kind_ == StatusKind::Success ) return SceneId::Menu; return SceneId::None; }
            if ( rect_btn_guest_.contains( pos ) ) return SceneId::Menu;
        }
    }

    if ( const auto* text = std::get_if<platform::TextEvent>( &event ) )
    {
        const uint32_t ch = text->unicode;
        if ( ch >= 32u && ch < 127u )
        {
            auto& buf = ( focus_ == FocusField::Username ) ? username_buf_ : password_buf_;
            if ( static_cast<int>( buf.size() ) < kMaxLen )
                buf += static_cast<char>( ch );
        }
    }

#endif

    return SceneId::None;
}

void LoginScene::update()
{
}

void LoginScene::render ( platform::Window& window )
{
#ifndef __EMSCRIPTEN__

    const sf::Vector2f ws ( window.getSize() );
    const float        t  = anim_clock_.getElapsedTime().asSeconds();
    const float        pulse = 0.5f + 0.5f * std::sin ( t * 2.1f );

    draw_gradient ( window, sf::Color ( 14, 22, 48 ), sf::Color ( 30, 70, 90 ) );
    draw_glow ( window,
                {ws.x * 0.20f + std::sin ( t * 0.28f ) * 50.f, ws.y * 0.25f},
                280.f, sf::Color ( 80, 140, 255, 20 ) );
    draw_glow ( window,
                {ws.x * 0.82f + std::cos ( t * 0.22f ) * 40.f, ws.y * 0.70f},
                240.f, sf::Color ( 255, 200, 80, 16 ) );

    {
        sf::Text logo ( font_, "AngryMipts", 52 );
        logo.setStyle ( sf::Text::Bold );
        logo.setFillColor ( sf::Color ( 242, 248, 255 ) );
        logo.setOutlineThickness ( 2.5f );
        logo.setOutlineColor ( sf::Color ( 20, 30, 55, 180 ) );
        const auto lb = logo.getLocalBounds();
        logo.setOrigin ( {lb.position.x, lb.position.y + lb.size.y / 2.f} );
        logo.setPosition ( {ws.x * 0.07f, ws.y * 0.28f + std::sin ( t * 1.1f ) * 2.5f} );
        window.draw ( logo );

        const char* bullets[] = {
            "Save progress locally",
            "Submit scores online",
            "Compete in leaderboards",
        };
        for ( int i = 0; i < 3; ++i )
        {
            sf::Text b ( font_, std::string ( "  " ) + bullets[i], 18 );
            b.setFillColor ( sf::Color ( 160, 200, 240,
                                         static_cast<uint8_t> ( 160 + 60 * pulse ) ) );
            const auto bb = b.getLocalBounds();
            b.setOrigin ( {bb.position.x, bb.position.y} );
            b.setPosition ( {ws.x * 0.07f,
                              ws.y * 0.42f + static_cast<float> ( i ) * 30.f} );
            window.draw ( b );
        }
    }

    const float card_cx = ws.x * 0.68f;
    const float card_cy = ws.y * 0.50f;

    {
        sf::RectangleShape shadow ( {kCardW + 8.f, kCardH + 8.f} );
        shadow.setOrigin ( {( kCardW + 8.f ) * 0.5f, ( kCardH + 8.f ) * 0.5f} );
        shadow.setPosition ( {card_cx + 6.f, card_cy + 8.f} );
        shadow.setFillColor ( sf::Color ( 4, 8, 18, 100 ) );
        window.draw ( shadow );

        sf::RectangleShape card ( {kCardW, kCardH} );
        card.setOrigin ( {kCardW * 0.5f, kCardH * 0.5f} );
        card.setPosition ( {card_cx, card_cy} );
        card.setFillColor ( sf::Color ( 10, 18, 36, 200 ) );
        card.setOutlineThickness ( 2.f );
        card.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
        window.draw ( card );

        sf::RectangleShape accent ( {kCardW - 8.f, 5.f} );
        accent.setOrigin ( {( kCardW - 8.f ) * 0.5f, 0.f} );
        accent.setPosition ( {card_cx, card_cy - kCardH * 0.5f + 5.f} );
        accent.setFillColor ( sf::Color ( 80, 160, 255, 160 ) );
        window.draw ( accent );
    }

    const float top_y = card_cy - kCardH * 0.5f;

    {
        sf::Text welcome ( font_, "Welcome", 30 );
        welcome.setStyle ( sf::Text::Bold );
        welcome.setFillColor ( sf::Color ( 236, 246, 255 ) );
        const auto wb = welcome.getLocalBounds();
        welcome.setOrigin ( {wb.position.x + wb.size.x / 2.f,
                             wb.position.y + wb.size.y / 2.f} );
        welcome.setPosition ( {card_cx, top_y + 38.f} );
        window.draw ( welcome );

        sf::Text sub ( font_, "Sign in to compete online", 15 );
        sub.setFillColor ( sf::Color ( 130, 175, 220 ) );
        const auto sb = sub.getLocalBounds();
        sub.setOrigin ( {sb.position.x + sb.size.x / 2.f,
                         sb.position.y + sb.size.y / 2.f} );
        sub.setPosition ( {card_cx, top_y + 66.f} );
        window.draw ( sub );
    }

    auto draw_field = [&] ( const std::string& label, const std::string& buf,
                            bool is_password, float cy_field, bool focused )
    {
        const float fx = card_cx - kFieldW * 0.5f;
        const float fy = cy_field - kFieldH * 0.5f;

        sf::Text lbl ( font_, label, 14 );
        lbl.setFillColor ( sf::Color ( 140, 185, 230 ) );
        lbl.setPosition ( {fx, fy - 20.f} );
        window.draw ( lbl );

        sf::RectangleShape bg ( {kFieldW, kFieldH} );
        bg.setPosition ( {fx, fy} );
        bg.setFillColor ( sf::Color ( 18, 28, 52, 200 ) );
        bg.setOutlineThickness ( focused ? 2.f : 1.f );
        bg.setOutlineColor ( focused ? sf::Color ( 80, 160, 255, 200 )
                                     : sf::Color ( 60, 90, 140, 120 ) );
        window.draw ( bg );

        const std::string display = is_password ? std::string ( buf.size(), '*' ) : buf;

        const bool caret_on = focused
            && ( static_cast<int> ( caret_clock_.getElapsedTime().asSeconds() * 2.f ) % 2 == 0 );

        const bool is_placeholder = display.empty() && !focused;
        sf::Text content ( font_, is_placeholder
                               ? ( is_password ? "Password" : "Username" )
                               : display,
                           18 );
        content.setFillColor ( is_placeholder ? sf::Color ( 80, 110, 160 )
                                              : sf::Color ( 225, 240, 255 ) );
        const auto cb = content.getLocalBounds();
        content.setOrigin ( {cb.position.x, cb.position.y + cb.size.y / 2.f} );
        content.setPosition ( {fx + 12.f, cy_field} );
        window.draw ( content );

        if ( caret_on )
        {
            const float caret_x = fx + 12.f + ( is_placeholder ? 0.f : cb.size.x );
            sf::RectangleShape caret ( {2.f, kFieldH * 0.6f} );
            caret.setOrigin ( {0.f, ( kFieldH * 0.6f ) * 0.5f} );
            caret.setPosition ( {caret_x, cy_field} );
            caret.setFillColor ( sf::Color ( 200, 230, 255, 220 ) );
            window.draw ( caret );
        }
    };

    const float field1_y = top_y + 130.f;
    const float field2_y = top_y + 215.f;
    const float field_fx = card_cx - kFieldW * 0.5f;

    rect_field_username_ = sf::FloatRect ( {field_fx, field1_y - kFieldH * 0.5f}, {kFieldW, kFieldH} );
    rect_field_password_ = sf::FloatRect ( {field_fx, field2_y - kFieldH * 0.5f}, {kFieldW, kFieldH} );

    draw_field ( "Username", username_buf_, false, field1_y, focus_ == FocusField::Username );
    draw_field ( "Password", password_buf_, true,  field2_y, focus_ == FocusField::Password );

    auto draw_button = [&] ( const std::string& label, float cx, float cy,
                              float w, float h, sf::Color fill, sf::Color text_color )
    {
        sf::RectangleShape btn ( {w, h} );
        btn.setOrigin ( {w * 0.5f, h * 0.5f} );
        btn.setPosition ( {cx, cy} );
        btn.setFillColor ( fill );
        btn.setOutlineThickness ( 1.5f );
        btn.setOutlineColor ( sf::Color ( 255, 255, 255, 40 ) );
        window.draw ( btn );

        sf::Text txt ( font_, label, 17 );
        txt.setStyle ( sf::Text::Bold );
        txt.setFillColor ( text_color );
        const auto tb = txt.getLocalBounds();
        txt.setOrigin ( {tb.position.x + tb.size.x / 2.f,
                         tb.position.y + tb.size.y / 2.f} );
        txt.setPosition ( {cx, cy} );
        window.draw ( txt );
    };

    const float btn_y   = top_y + 295.f;
    const float btn_w   = 164.f;
    const float btn_h   = 42.f;
    const float gap     = 12.f;

    const float btn_login_cx    = card_cx - btn_w * 0.5f - gap * 0.5f;
    const float btn_register_cx = card_cx + btn_w * 0.5f + gap * 0.5f;

    rect_btn_login_    = sf::FloatRect ( {btn_login_cx    - btn_w * 0.5f, btn_y - btn_h * 0.5f}, {btn_w, btn_h} );
    rect_btn_register_ = sf::FloatRect ( {btn_register_cx - btn_w * 0.5f, btn_y - btn_h * 0.5f}, {btn_w, btn_h} );
    rect_btn_guest_    = sf::FloatRect ( {card_cx - 120.f, top_y + 355.f - 12.f}, {240.f, 24.f} );

    draw_button ( "Login",    btn_login_cx,    btn_y,
                  btn_w, btn_h,
                  sf::Color ( 210, 135, 30, 230 ), sf::Color::White );
    draw_button ( "Register", btn_register_cx, btn_y,
                  btn_w, btn_h,
                  sf::Color ( 30, 100, 190, 220 ), sf::Color::White );

    {
        sf::Text guest ( font_, "Continue as Guest  (F2)", 15 );
        guest.setFillColor ( sf::Color ( 130, 175, 230,
                                          static_cast<uint8_t> ( 150 + 80 * pulse ) ) );
        const auto gb = guest.getLocalBounds();
        guest.setOrigin ( {gb.position.x + gb.size.x / 2.f,
                           gb.position.y + gb.size.y / 2.f} );
        guest.setPosition ( {card_cx, top_y + 355.f} );
        window.draw ( guest );
    }

    if ( !status_msg_.empty() )
    {
        const int kind = static_cast<int> ( status_kind_ );
        sf::Text st ( font_, status_msg_, 16 );
        st.setFillColor ( status_color ( kind ) );
        const auto sb = st.getLocalBounds();
        st.setOrigin ( {sb.position.x + sb.size.x / 2.f,
                        sb.position.y + sb.size.y / 2.f} );
        st.setPosition ( {card_cx, top_y + 393.f} );
        window.draw ( st );
    }

    {
        sf::Text hint ( font_, "[Tab] switch field   [Enter] login   [F1] register", 13 );
        hint.setFillColor ( sf::Color ( 100, 140, 190, 140 ) );
        const auto hb = hint.getLocalBounds();
        hint.setOrigin ( {hb.position.x + hb.size.x / 2.f,
                          hb.position.y + hb.size.y / 2.f} );
        hint.setPosition ( {ws.x * 0.5f, ws.y * 0.94f} );
        window.draw ( hint );
    }

#else  // __EMSCRIPTEN__ — Raylib render

    if ( !font_.loaded ) return;
    const platform::Vec2u ws = window.getSize();
    const float W = float(ws.x), H = float(ws.y);
    const float t = anim_clock_.getElapsedTime().asSeconds();
    const float pulse = 0.5f + 0.5f * std::sin( t * 2.1f );

    draw_gradient( window, {14,22,48}, {30,70,90}, int(W), int(H) );
    draw_glow( {W*0.20f + std::sin(t*0.28f)*50.f, H*0.25f}, 280.f, {80,140,255,20} );
    draw_glow( {W*0.82f + std::cos(t*0.22f)*40.f, H*0.70f}, 240.f, {255,200,80,16} );

    // Logo on left
    DrawTextEx( font_.rl, "AngryMipts",
                {W*0.07f, H*0.28f + std::sin(t*1.1f)*2.5f},
                52.f, 1.f, platform::Color(242,248,255).to_rl() );
    const char* bullets[] = {"Save progress locally","Submit scores online","Compete in leaderboards"};
    for ( int i = 0; i < 3; ++i )
    {
        uint8_t ba = static_cast<uint8_t>( 160 + 60*pulse );
        DrawTextEx( font_.rl, bullets[i],
                    {W*0.07f, H*0.42f + float(i)*30.f},
                    18.f, 1.f, ::Color{160,200,240,ba} );
    }

    const float card_cx = W * 0.68f;
    const float card_cy = H * 0.50f;
    const float top_y   = card_cy - kCardH * 0.5f;

    // Card
    DrawRectangle( int(card_cx-kCardW*0.5f+6), int(card_cy-kCardH*0.5f+8),
                   int(kCardW+8), int(kCardH+8), platform::Color(4,8,18,100).to_rl() );
    DrawRectangle( int(card_cx-kCardW*0.5f), int(card_cy-kCardH*0.5f),
                   int(kCardW), int(kCardH), platform::Color(10,18,36,200).to_rl() );
    DrawRectangleLinesEx( {card_cx-kCardW*0.5f, card_cy-kCardH*0.5f, kCardW, kCardH},
                          2.f, platform::Color(80,140,220,110).to_rl() );

    // Welcome
    DrawTextEx( font_.rl, "Welcome",
                {card_cx - MeasureTextEx(font_.rl,"Welcome",30.f,1.f).x/2.f, top_y+24.f},
                30.f, 1.f, platform::Color(236,246,255).to_rl() );
    DrawTextEx( font_.rl, "Sign in to compete online",
                {card_cx - MeasureTextEx(font_.rl,"Sign in to compete online",15.f,1.f).x/2.f, top_y+55.f},
                15.f, 1.f, platform::Color(130,175,220).to_rl() );

    // Fields
    auto rl_draw_field = [&]( const std::string& label, const std::string& buf,
                               bool is_password, float cy_field, bool focused,
                               platform::FloatRect& rect_out )
    {
        const float fx = card_cx - kFieldW*0.5f;
        const float fy = cy_field - kFieldH*0.5f;
        rect_out = { fx, fy, kFieldW, kFieldH };

        DrawTextEx( font_.rl, label.c_str(), {fx, fy-20.f},
                    14.f, 1.f, platform::Color(140,185,230).to_rl() );
        DrawRectangle( int(fx), int(fy), int(kFieldW), int(kFieldH),
                       platform::Color(18,28,52,200).to_rl() );
        DrawRectangleLinesEx( {fx, fy, kFieldW, kFieldH}, focused ? 2.f : 1.f,
                              (focused ? platform::Color(80,160,255,200)
                                       : platform::Color(60,90,140,120)).to_rl() );

        const std::string display = is_password ? std::string(buf.size(),'*') : buf;
        const bool is_placeholder = display.empty() && !focused;
        const char* show = is_placeholder ? (is_password?"Password":"Username") : display.c_str();
        ::Color tc = is_placeholder ? platform::Color(80,110,160).to_rl()
                                    : platform::Color(225,240,255).to_rl();
        DrawTextEx( font_.rl, show, {fx+12.f, fy+kFieldH*0.5f-9.f}, 18.f, 1.f, tc );

        const bool caret_on = focused
            && (static_cast<int>(caret_clock_.getElapsedTime().asSeconds()*2.f) % 2 == 0);
        if ( caret_on )
        {
            float caret_x = fx + 12.f;
            if ( !is_placeholder )
                caret_x += MeasureTextEx(font_.rl, display.c_str(), 18.f, 1.f).x;
            DrawRectangle( int(caret_x), int(fy + kFieldH*0.2f), 2, int(kFieldH*0.6f),
                           platform::Color(200,230,255,220).to_rl() );
        }
    };

    const float field1_y = top_y + 130.f;
    const float field2_y = top_y + 215.f;
    rl_draw_field( "Username", username_buf_, false, field1_y, focus_==FocusField::Username, rect_field_username_ );
    rl_draw_field( "Password", password_buf_, true,  field2_y, focus_==FocusField::Password, rect_field_password_ );

    // Buttons
    const float btn_y   = top_y + 295.f;
    const float btn_w   = 164.f, btn_h = 42.f, gap = 12.f;
    const float btn_lcx = card_cx - btn_w*0.5f - gap*0.5f;
    const float btn_rcx = card_cx + btn_w*0.5f + gap*0.5f;

    rect_btn_login_    = { btn_lcx-btn_w*0.5f, btn_y-btn_h*0.5f, btn_w, btn_h };
    rect_btn_register_ = { btn_rcx-btn_w*0.5f, btn_y-btn_h*0.5f, btn_w, btn_h };
    rect_btn_guest_    = { card_cx-120.f, top_y+343.f, 240.f, 24.f };

    auto rl_btn = [&]( const char* label, float cx, float cy, float w, float h, platform::Color fill )
    {
        DrawRectangle( int(cx-w*0.5f), int(cy-h*0.5f), int(w), int(h), fill.to_rl() );
        DrawRectangleLinesEx({cx-w*0.5f,cy-h*0.5f,w,h},1.5f,platform::Color(255,255,255,40).to_rl());
        ::Vector2 tsz = MeasureTextEx(font_.rl,label,17.f,1.f);
        DrawTextEx(font_.rl,label,{cx-tsz.x*0.5f,cy-tsz.y*0.5f},17.f,1.f,WHITE);
    };
    rl_btn("Login",    btn_lcx, btn_y, btn_w, btn_h, {210,135,30,230});
    rl_btn("Register", btn_rcx, btn_y, btn_w, btn_h, {30,100,190,220});

    // Guest
    uint8_t ga = static_cast<uint8_t>( 150 + 80*pulse );
    const char* guest_str = "Continue as Guest  (F2)";
    ::Vector2 gsz = MeasureTextEx(font_.rl,guest_str,15.f,1.f);
    DrawTextEx(font_.rl,guest_str,{card_cx-gsz.x*0.5f,top_y+347.f},15.f,1.f,::Color{130,175,230,ga});

    // Status
    if ( !status_msg_.empty() )
    {
        platform::Color sc = status_color( static_cast<int>(status_kind_) );
        ::Vector2 ssz = MeasureTextEx(font_.rl,status_msg_.c_str(),16.f,1.f);
        DrawTextEx(font_.rl,status_msg_.c_str(),{card_cx-ssz.x*0.5f,top_y+378.f},16.f,1.f,sc.to_rl());
    }

    // Hint
    const char* hint_str = "[Tab] switch field   [Enter] login   [F1] register";
    ::Vector2 hsz = MeasureTextEx(font_.rl,hint_str,13.f,1.f);
    DrawTextEx(font_.rl,hint_str,{W*0.5f-hsz.x*0.5f,H*0.94f},13.f,1.f,::Color{100,140,190,140});

#endif
}

}  // namespace angry
