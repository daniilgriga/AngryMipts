#include "ui/level_select_scene.hpp"

#include "data/logger.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>

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

}  // namespace

LevelSelectScene::LevelSelectScene ( const sf::Font& font, AccountService* accounts )
    : accounts_ ( accounts )
    , font_ ( font )
    , title_ ( font_, "Select Level", 42 )
    , prompt_ ( font_, "[Enter] Play   [Backspace] Menu   [Scroll] Navigate", 18 )
    , badge_text_ ( font_, "", 18 )
    , badge_btn_ ( font_, "", 16 )
{
    title_.setFillColor ( sf::Color::White );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
}

void LevelSelectScene::load_data ( const std::string& levels_dir,
                                    const std::string& scores_path )
{
    scores_path_ = scores_path;

    try
    {
        LevelLoader loader;
        levels_ = loader.loadAllMeta ( levels_dir );
    }
    catch ( const std::exception& e )
    {
        Logger::error ( "LevelSelectScene: failed to load levels meta: {}", e.what() );
    }

    try
    {
        ScoreSaver saver;
        scores_ = saver.loadScores ( scores_path_ );
    }
    catch ( const std::exception& )
    {
        // Scores file may not exist yet — that is fine
        scores_.clear();
    }

    selected_ = 0;
    rebuild_texts();
}

void LevelSelectScene::reload_scores()
{
    if ( scores_path_.empty() )
        return;

    try
    {
        ScoreSaver saver;
        scores_ = saver.loadScores ( scores_path_ );
    }
    catch ( const std::exception& )
    {
        scores_.clear();
    }

    rebuild_texts();
}

const LevelScore* LevelSelectScene::find_score ( int level_id ) const
{
    for ( const auto& s : scores_ )
    {
        if ( s.levelId == level_id )
            return &s;
    }
    return nullptr;
}

void LevelSelectScene::rebuild_texts()
{
    level_texts_.clear();

    for ( int i = 0; i < static_cast<int> ( levels_.size() ); ++i )
    {
        const auto& meta = levels_[i];
        const LevelScore* score = find_score ( meta.id );

        std::string stars_str;
        int best_stars = score ? score->bestStars : 0;
        for ( int s = 0; s < 3; ++s )
            stars_str += ( s < best_stars ) ? "* " : "- ";

        std::string label = std::to_string ( meta.id ) + ". " + meta.name
                            + "   " + stars_str;
        if ( score && score->bestScore > 0 )
            label += "  " + std::to_string ( score->bestScore ) + " pts";

        sf::Text text ( font_, label, 26 );
        text.setFillColor ( i == selected_ ? sf::Color ( 255, 220, 50 )
                                           : sf::Color::White );
        level_texts_.push_back ( std::move ( text ) );
    }
}

SceneId LevelSelectScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;

        if ( key->code == sf::Keyboard::Key::Up )
        {
            if ( selected_ > 0 )
                --selected_;
            rebuild_texts();
        }

        if ( key->code == sf::Keyboard::Key::Down )
        {
            if ( selected_ < static_cast<int> ( levels_.size() ) - 1 )
                ++selected_;
            rebuild_texts();
        }

        if ( key->code == sf::Keyboard::Key::Enter && !levels_.empty() )
        {
            selected_level_id_ = levels_[selected_].id;
            return SceneId::Game;
        }

        if ( key->code == sf::Keyboard::Key::L && accounts_ )
        {
            if ( accounts_->isLoggedIn() )
                accounts_->logout();
            else
                return SceneId::Login;
        }
    }

    if ( const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>() )
    {
        scroll_offset_ -= wheel->delta * 52.f;
        if ( scroll_offset_ < 0.f )
            scroll_offset_ = 0.f;
    }

    return SceneId::None;
}

void LevelSelectScene::update()
{
}

void LevelSelectScene::render ( sf::RenderWindow& window )
{
    static sf::Clock anim_clock;
    const float t = anim_clock.getElapsedTime().asSeconds();
    auto ws = sf::Vector2f ( window.getSize() );
    const float pulse = 0.5f + 0.5f * std::sin ( t * 3.0f );

    draw_vertical_gradient ( window, sf::Color ( 12, 28, 55 ), sf::Color ( 28, 84, 95 ) );

    sf::CircleShape glow_a ( 240.f );
    glow_a.setOrigin ( {240.f, 240.f} );
    glow_a.setPosition ( {ws.x * 0.16f + std::sin ( t * 0.4f ) * 35.f, ws.y * 0.25f} );
    glow_a.setFillColor ( sf::Color ( 120, 190, 255, 22 ) );
    window.draw ( glow_a );

    sf::CircleShape glow_b ( 300.f );
    glow_b.setOrigin ( {300.f, 300.f} );
    glow_b.setPosition ( {ws.x * 0.84f + std::cos ( t * 0.33f ) * 42.f, ws.y * 0.70f} );
    glow_b.setFillColor ( sf::Color ( 255, 236, 170, 18 ) );
    window.draw ( glow_b );

    sf::RectangleShape panel ( {ws.x * 0.64f, ws.y * 0.62f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {ws.x * 0.5f, ws.y * 0.5f} );
    panel.setFillColor ( sf::Color ( 8, 16, 30, 138 ) );
    panel.setOutlineThickness ( 2.5f );
    panel.setOutlineColor ( sf::Color ( 188, 220, 244, 128 ) );
    window.draw ( panel );

    sf::RectangleShape panel_accent ( {panel.getSize().x - 10.f, 6.f} );
    panel_accent.setOrigin ( {panel_accent.getSize().x * 0.5f, 0.f} );
    panel_accent.setPosition ( {panel.getPosition().x, panel.getPosition().y - panel.getSize().y * 0.5f + 8.f} );
    panel_accent.setFillColor ( sf::Color ( 132, 200, 255, 145 ) );
    window.draw ( panel_accent );

    title_.setStyle ( sf::Text::Bold );
    auto title_bounds = title_.getLocalBounds();
    title_.setOrigin ( {title_bounds.position.x + title_bounds.size.x / 2.f,
                        title_bounds.position.y + title_bounds.size.y / 2.f} );
    title_.setPosition ( {ws.x / 2.f, ws.y * 0.12f} );
    window.draw ( title_ );

    const float step        = 52.f;
    const float list_top    = ws.y * 0.22f;
    const float list_bottom = ws.y * 0.79f;  // panel bottom is ws.y*0.81, keep margin
    const float list_h      = list_bottom - list_top;
    const float total_h     = static_cast<float> ( level_texts_.size() ) * step;
    const float max_scroll  = std::max ( 0.f, total_h - list_h );

    // Auto-follow selected item
    const float sel_y_local = static_cast<float> ( selected_ ) * step + step * 0.5f;
    if ( sel_y_local - scroll_offset_ < step )
        scroll_offset_ = std::max ( 0.f, sel_y_local - step );
    if ( sel_y_local - scroll_offset_ > list_h - step )
        scroll_offset_ = std::min ( max_scroll, sel_y_local - list_h + step );
    scroll_offset_ = std::clamp ( scroll_offset_, 0.f, max_scroll );

    // Clip list area via scissor view
    const sf::FloatRect list_rect ( {0.f, list_top}, {ws.x, list_h} );
    sf::View list_view ( list_rect );
    list_view.setViewport ( sf::FloatRect (
        {0.f, list_top / ws.y},
        {1.f, list_h  / ws.y} ) );
    window.setView ( list_view );

    for ( int i = 0; i < static_cast<int> ( level_texts_.size() ); ++i )
    {
        const float y       = list_top + static_cast<float> ( i ) * step + step * 0.5f - scroll_offset_;
        const bool  is_sel  = ( i == selected_ );

        // Skip items outside visible area
        if ( y + step < list_top || y - step > list_bottom )
            continue;

        if ( is_sel )
        {
            sf::RectangleShape highlight ( {panel.getSize().x * 0.86f, 42.f} );
            highlight.setOrigin ( {highlight.getSize().x * 0.5f, highlight.getSize().y * 0.5f} );
            highlight.setPosition ( {ws.x * 0.5f + std::sin ( t * 5.0f ) * 2.f, y} );
            highlight.setFillColor ( sf::Color ( 244, 214, 98,
                                                 static_cast<uint8_t> ( 62.f + 52.f * pulse ) ) );
            highlight.setOutlineThickness ( 1.5f );
            highlight.setOutlineColor ( sf::Color ( 255, 234, 145, 150 ) );
            window.draw ( highlight );
        }

        auto& text = level_texts_[i];
        text.setFillColor ( is_sel ? sf::Color ( 255, 247, 220 )
                                   : sf::Color ( 228, 238, 248 ) );
        auto bounds = text.getLocalBounds();
        text.setOrigin ( {bounds.position.x + bounds.size.x / 2.f,
                          bounds.position.y + bounds.size.y / 2.f} );
        text.setPosition ( {ws.x / 2.f, y} );
        window.draw ( text );
    }

    window.setView ( window.getDefaultView() );

    prompt_.setFillColor ( sf::Color ( 232, 246, 255,
                                       static_cast<uint8_t> ( 180.f + 70.f * pulse ) ) );
    auto prompt_bounds = prompt_.getLocalBounds();
    prompt_.setOrigin ( {prompt_bounds.position.x + prompt_bounds.size.x / 2.f,
                         prompt_bounds.position.y + prompt_bounds.size.y / 2.f} );
    prompt_.setPosition ( {ws.x / 2.f, ws.y * 0.88f} );
    window.draw ( prompt_ );

    // --- Account badge (top-right) ---
    const float badge_right = ws.x - 18.f;
    const float badge_top   = 14.f;

    const float pill_w = 280.f;
    const float pill_h = 54.f;
    sf::RectangleShape pill ( {pill_w, pill_h} );
    pill.setPosition ( {badge_right - pill_w, badge_top} );
    pill.setFillColor ( sf::Color ( 10, 18, 36, 200 ) );
    pill.setOutlineThickness ( 1.5f );
    pill.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
    window.draw ( pill );

    if ( accounts_ && accounts_->isLoggedIn() )
    {
        badge_text_.setString ( "Logged in as " + accounts_->username() );
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
}

}  // namespace angry
