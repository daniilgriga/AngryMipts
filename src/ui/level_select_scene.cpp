// ============================================================
// level_select_scene.cpp — Level selection scene implementation.
// Part of: angry::ui
//
// Implements interactive level-browser behavior:
//   * Loads and displays level metadata and local best scores
//   * Requests leaderboard previews asynchronously
//   * Handles keyboard/mouse selection and scrolling
//   * Produces selected level id for scene transitions
// ============================================================

#include "ui/level_select_scene.hpp"

#include "data/logger.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Graphics.hpp>
#endif

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <thread>

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
#else
void draw_vertical_gradient ( platform::Color top, platform::Color bottom, int W, int H )
{
    DrawRectangleGradientV( 0, 0, W, H, top.to_rl(), bottom.to_rl() );
}
#endif

}  // namespace

LevelSelectScene::LevelSelectScene ( const platform::Font& font, AccountService* accounts )
    : accounts_ ( accounts )
    , font_ ( font )
#ifndef __EMSCRIPTEN__
    , title_ ( font_, "Select Level", 28 )
    , prompt_ ( font_, "[Enter] Play   [Backspace] Menu   [Scroll] Navigate", 18 )
    , badge_text_ ( font_, "", 18 )
    , badge_btn_ ( font_, "", 16 )
#endif
{
#ifndef __EMSCRIPTEN__
    title_.setFillColor ( sf::Color::White );
    prompt_.setFillColor ( sf::Color ( 230, 245, 255 ) );
#else
    title_.font_  = &font_; title_.string_  = "Select Level"; title_.char_size_ = 28;
    prompt_.font_ = &font_; prompt_.string_ = "[Enter] Play   [Backspace] Menu"; prompt_.char_size_ = 18;
    badge_text_.font_ = &font_;
    badge_btn_.font_  = &font_;
#endif
}

void LevelSelectScene::fetch_preview ( int level_id )
{
    if ( preview_requested_id_ == level_id )
        return;
    preview_requested_id_ = level_id;
    preview_scroll_       = 0.f;

    {
        std::lock_guard<std::mutex> lock ( preview_->mutex );
        preview_->fetched_level_id = -1;
        preview_->fetch_status     = LeaderboardFetchStatus::Unavailable;
        preview_->entries.clear();
    }

#ifndef __EMSCRIPTEN__
    const std::shared_ptr<PreviewState> state = preview_;
    std::thread ( [state, level_id]()
    {
        OnlineScoreClient client;
        LeaderboardFetchResult r = client.fetch_leaderboard_with_status ( level_id );

        std::lock_guard<std::mutex> lock ( state->mutex );
        state->fetched_level_id = level_id;
        state->fetch_status     = r.status;
        state->entries          = std::move ( r.entries );
    } ).detach();
#else
    // On web: fetch synchronously (no worker threads in wasm build).
    LeaderboardFetchResult r;
    try
    {
        OnlineScoreClient client;
        r = client.fetch_leaderboard_with_status ( level_id );
    }
    catch ( const std::exception& e )
    {
        Logger::error ( "LevelSelectScene: failed to fetch leaderboard preview: {}", e.what() );
        r = {};
    }

    std::lock_guard<std::mutex> lock ( preview_->mutex );
    preview_->fetched_level_id = level_id;
    preview_->fetch_status     = r.status;
    preview_->entries          = std::move ( r.entries );
#endif
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

    if ( !scores_path_.empty() )
    {
        try
        {
            ScoreSaver saver;
            scores_ = saver.loadScores ( scores_path_ );
        }
        catch ( const std::exception& )
        {
            scores_.clear();
        }
    }
    else
    {
        scores_.clear();
    }

    selected_ = 0;
    rebuild_texts();

    if ( !levels_.empty() )
        fetch_preview ( levels_[0].id );
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

#ifndef __EMSCRIPTEN__
        sf::Text text ( font_, label, 24 );
        text.setFillColor ( i == selected_ ? sf::Color ( 255, 220, 50 )
                                           : sf::Color::White );
        level_texts_.push_back ( std::move ( text ) );
#else
        platform::Text text;
        text.font_ = &font_;
        text.string_ = label;
        text.char_size_ = 24;
        text.fill_color_ = i == selected_ ? platform::Color(255,220,50) : platform::Color(255,255,255);
        level_texts_.push_back( std::move(text) );
#endif
    }
}

SceneId LevelSelectScene::handle_input ( const platform::Event& event )
{
#ifndef __EMSCRIPTEN__
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;

        if ( key->code == sf::Keyboard::Key::Up )
        {
            if ( selected_ > 0 ) { --selected_; rebuild_texts(); fetch_preview(levels_[selected_].id); }
        }
        if ( key->code == sf::Keyboard::Key::Down )
        {
            if ( selected_ < static_cast<int>( levels_.size() ) - 1 ) { ++selected_; rebuild_texts(); fetch_preview(levels_[selected_].id); }
        }
        if ( key->code == sf::Keyboard::Key::Enter && !levels_.empty() )
        {
            selected_level_id_ = levels_[selected_].id;
            return SceneId::Game;
        }
        if ( key->code == sf::Keyboard::Key::L && accounts_ )
        {
            if ( accounts_->is_logged_in() ) accounts_->logout();
            else return SceneId::Login;
        }
        if ( key->code == sf::Keyboard::Key::PageUp )
            preview_scroll_ = std::max(0.f, preview_scroll_ - 72.f);
        if ( key->code == sf::Keyboard::Key::PageDown )
            preview_scroll_ += 72.f;
    }

    if ( const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>() )
    {
        const sf::Vector2f pos ( static_cast<float>(wheel->position.x),
                                  static_cast<float>(wheel->position.y) );
        if ( rect_right_panel_.contains(pos) )
            preview_scroll_ = std::max(0.f, preview_scroll_ - wheel->delta * 36.f);
        else
        {
            scroll_offset_ -= wheel->delta * 52.f;
            if ( scroll_offset_ < 0.f ) scroll_offset_ = 0.f;
        }
    }

    if ( const auto* click = event.getIf<sf::Event::MouseButtonPressed>() )
    {
        if ( click->button == sf::Mouse::Button::Left )
        {
            const sf::Vector2f pos ( static_cast<float>(click->position.x),
                                     static_cast<float>(click->position.y) );
            if ( rect_badge_.contains(pos) && accounts_ )
            {
                if ( accounts_->is_logged_in() ) accounts_->logout();
                else return SceneId::Login;
                return SceneId::None;
            }
            for ( int i = 0; i < static_cast<int>(rects_level_items_screen_.size()); ++i )
            {
                if ( rects_level_items_screen_[i].contains(pos) )
                {
                    if ( i == selected_ && !levels_.empty() )
                    { selected_level_id_ = levels_[selected_].id; return SceneId::Game; }
                    else
                    { selected_ = i; rebuild_texts(); fetch_preview(levels_[selected_].id); }
                    return SceneId::None;
                }
            }
        }
    }

#else  // __EMSCRIPTEN__

    if ( const auto* key = std::get_if<platform::KeyEvent>( &event ) )
    {
        if ( key->key == KEY_BACKSPACE ) return SceneId::Menu;
        if ( key->key == KEY_UP && selected_ > 0 )
            { --selected_; rebuild_texts(); fetch_preview(levels_[selected_].id); }
        if ( key->key == KEY_DOWN && selected_ < (int)levels_.size()-1 )
            { ++selected_; rebuild_texts(); fetch_preview(levels_[selected_].id); }
        if ( (key->key == KEY_ENTER || key->key == KEY_KP_ENTER) && !levels_.empty() )
            { selected_level_id_ = levels_[selected_].id; return SceneId::Game; }
        if ( key->key == KEY_L && accounts_ )
        {
            if ( accounts_->is_logged_in() ) accounts_->logout();
            else return SceneId::Login;
        }
        if ( key->key == KEY_PAGE_UP )   preview_scroll_ = std::max(0.f, preview_scroll_ - 72.f);
        if ( key->key == KEY_PAGE_DOWN ) preview_scroll_ += 72.f;
    }

    if ( const auto* wheel = std::get_if<platform::MouseWheelEvent>( &event ) )
    {
        platform::Vec2f pos { wheel->x, wheel->y };
        if ( rect_right_panel_.contains(pos) )
            preview_scroll_ = std::max(0.f, preview_scroll_ - wheel->delta * 36.f);
        else
        {
            scroll_offset_ -= wheel->delta * 52.f;
            if ( scroll_offset_ < 0.f ) scroll_offset_ = 0.f;
        }
    }

    if ( const auto* click = std::get_if<platform::MouseBtnEvent>( &event ) )
    {
        if ( click->button == 0 && click->pressed )
        {
            platform::Vec2f pos { click->x, click->y };
            if ( rect_badge_.contains(pos) && accounts_ )
            {
                if ( accounts_->is_logged_in() ) accounts_->logout(); else return SceneId::Login;
                return SceneId::None;
            }
            for ( int i = 0; i < (int)rects_level_items_screen_.size(); ++i )
            {
                if ( rects_level_items_screen_[i].contains(pos) )
                {
                    if ( i == selected_ && !levels_.empty() )
                        { selected_level_id_ = levels_[selected_].id; return SceneId::Game; }
                    else
                        { selected_ = i; rebuild_texts(); fetch_preview(levels_[selected_].id); }
                    return SceneId::None;
                }
            }
        }
    }

#endif

    return SceneId::None;
}

void LevelSelectScene::update()
{
}

void LevelSelectScene::render ( platform::Window& window )
{
#ifndef __EMSCRIPTEN__

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

    const float margin    = ws.x * 0.015f;
    const float gap       = ws.x * 0.015f;
    const float left_w    = ws.x * 0.42f;
    const float right_w   = ws.x - margin * 2.f - left_w - gap;
    const float panel_top = ws.y * 0.10f;
    const float panel_h   = ws.y * 0.78f;
    const float left_cx   = margin + left_w * 0.5f;
    const float right_x   = margin + left_w + gap;
    const float right_cx  = right_x + right_w * 0.5f;

    rect_left_panel_  = sf::FloatRect ( {margin, panel_top}, {left_w, panel_h} );
    rect_right_panel_ = sf::FloatRect ( {right_x, panel_top}, {right_w, panel_h} );

    {
        sf::RectangleShape panel ( {left_w, panel_h} );
        panel.setPosition ( {margin, panel_top} );
        panel.setFillColor ( sf::Color ( 8, 16, 30, 138 ) );
        panel.setOutlineThickness ( 2.5f );
        panel.setOutlineColor ( sf::Color ( 188, 220, 244, 128 ) );
        window.draw ( panel );

        sf::RectangleShape accent ( {left_w - 10.f, 6.f} );
        accent.setPosition ( {margin + 5.f, panel_top + 8.f} );
        accent.setFillColor ( sf::Color ( 132, 200, 255, 145 ) );
        window.draw ( accent );
    }

    title_.setStyle ( sf::Text::Bold );
    auto title_bounds = title_.getLocalBounds();
    title_.setOrigin ( {title_bounds.position.x + title_bounds.size.x / 2.f,
                        title_bounds.position.y + title_bounds.size.y / 2.f} );
    title_.setPosition ( {left_cx, panel_top + 28.f} );
    window.draw ( title_ );

    const float step        = 48.f;
    const float list_top    = panel_top + 60.f;
    const float list_bottom = panel_top + panel_h - 44.f;
    const float list_h      = list_bottom - list_top;
    const float total_h     = static_cast<float> ( level_texts_.size() ) * step;
    const float max_scroll  = std::max ( 0.f, total_h - list_h );

    const float sel_y_local = static_cast<float> ( selected_ ) * step + step * 0.5f;
    if ( sel_y_local - scroll_offset_ < step )
        scroll_offset_ = std::max ( 0.f, sel_y_local - step );
    if ( sel_y_local - scroll_offset_ > list_h - step )
        scroll_offset_ = std::min ( max_scroll, sel_y_local - list_h + step );
    scroll_offset_ = std::clamp ( scroll_offset_, 0.f, max_scroll );

    const sf::FloatRect list_rect ( {0.f, list_top}, {ws.x, list_h} );
    sf::View list_view ( list_rect );
    list_view.setViewport ( sf::FloatRect (
        {0.f, list_top / ws.y},
        {1.f, list_h  / ws.y} ) );
    window.setView ( list_view );

    rects_level_items_screen_.resize ( level_texts_.size() );

    for ( int i = 0; i < static_cast<int> ( level_texts_.size() ); ++i )
    {
        const float y      = list_top + static_cast<float> ( i ) * step + step * 0.5f - scroll_offset_;
        const bool  is_sel = ( i == selected_ );

        rects_level_items_screen_[i] = sf::FloatRect ( {margin, y - step * 0.5f}, {left_w, step} );

        if ( y + step < list_top || y - step > list_bottom )
            continue;

        if ( is_sel )
        {
            sf::RectangleShape highlight ( {left_w - 20.f, 40.f} );
            highlight.setOrigin ( {( left_w - 20.f ) * 0.5f, 20.f} );
            highlight.setPosition ( {left_cx + std::sin ( t * 5.0f ) * 2.f, y} );
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
        text.setPosition ( {left_cx, y} );
        window.draw ( text );
    }

    window.setView ( window.getDefaultView() );

    prompt_.setFillColor ( sf::Color ( 232, 246, 255,
                                       static_cast<uint8_t> ( 180.f + 70.f * pulse ) ) );
    auto prompt_bounds = prompt_.getLocalBounds();
    prompt_.setOrigin ( {prompt_bounds.position.x + prompt_bounds.size.x / 2.f,
                         prompt_bounds.position.y + prompt_bounds.size.y / 2.f} );
    prompt_.setPosition ( {left_cx, panel_top + panel_h - 22.f} );
    window.draw ( prompt_ );

    {
        sf::RectangleShape panel ( {right_w, panel_h} );
        panel.setPosition ( {right_x, panel_top} );
        panel.setFillColor ( sf::Color ( 8, 14, 26, 148 ) );
        panel.setOutlineThickness ( 2.f );
        panel.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
        window.draw ( panel );

        sf::RectangleShape accent ( {right_w - 8.f, 5.f} );
        accent.setPosition ( {right_x + 4.f, panel_top + 8.f} );
        accent.setFillColor ( sf::Color ( 80, 160, 255, 145 ) );
        window.draw ( accent );
    }

    if ( !levels_.empty() )
    {
        const auto& meta  = levels_[selected_];
        const LevelScore* local_score = find_score ( meta.id );
        float cy = panel_top + 36.f;

        {
            sf::Text name_text ( font_, meta.name, 28 );
            name_text.setStyle ( sf::Text::Bold );
            name_text.setFillColor ( sf::Color ( 230, 245, 255 ) );
            auto b = name_text.getLocalBounds();
            name_text.setOrigin ( {b.position.x + b.size.x / 2.f,
                                   b.position.y + b.size.y / 2.f} );
            name_text.setPosition ( {right_cx, cy} );
            window.draw ( name_text );
            cy += 44.f;
        }

        {
            std::string local_str = "Local best: ";
            if ( local_score && local_score->bestScore > 0 )
                local_str += std::to_string(local_score->bestScore) + " pts  "
                           + std::string(static_cast<std::size_t>(local_score->bestStars), '*');
            else
                local_str += "not played";

            sf::Text local_text ( font_, local_str, 18 );
            local_text.setFillColor ( sf::Color ( 180, 220, 255 ) );
            auto b = local_text.getLocalBounds();
            local_text.setOrigin ( {b.position.x + b.size.x / 2.f,
                                    b.position.y + b.size.y / 2.f} );
            local_text.setPosition ( {right_cx, cy} );
            window.draw ( local_text );
            cy += 30.f;
        }

        {
            sf::RectangleShape div ( {right_w - 30.f, 1.f} );
            div.setPosition ( {right_x + 15.f, cy} );
            div.setFillColor ( sf::Color ( 80, 130, 200, 80 ) );
            window.draw ( div );
            cy += 10.f;
        }

        {
            sf::Text lb_hdr ( font_, "Online Leaderboard", 20 );
            lb_hdr.setStyle ( sf::Text::Bold );
            lb_hdr.setFillColor ( sf::Color ( 200, 225, 255 ) );
            auto b = lb_hdr.getLocalBounds();
            lb_hdr.setOrigin ( {b.position.x + b.size.x / 2.f,
                                b.position.y + b.size.y / 2.f} );
            lb_hdr.setPosition ( {right_cx, cy} );
            window.draw ( lb_hdr );
            cy += 32.f;
        }

        LeaderboardFetchStatus fetch_status;
        std::vector<LeaderboardEntry> entries;
        int fetched_id;
        {
            std::lock_guard<std::mutex> lock ( preview_->mutex );
            fetch_status = preview_->fetch_status;
            entries      = preview_->entries;
            fetched_id   = preview_->fetched_level_id;
        }

        const float lb_top    = cy;
        const float lb_bottom = panel_top + panel_h - 14.f;
        const float lb_h      = lb_bottom - lb_top;
        const float row_step  = 34.f;

        if ( fetched_id != meta.id )
        {
            sf::Text loading ( font_, "Loading...", 17 );
            loading.setFillColor ( sf::Color ( 160, 190, 220, 160 ) );
            auto b = loading.getLocalBounds();
            loading.setOrigin ( {b.position.x + b.size.x / 2.f,
                                 b.position.y + b.size.y / 2.f} );
            loading.setPosition ( {right_cx, lb_top + lb_h * 0.4f} );
            window.draw ( loading );
        }
        else if ( fetch_status == LeaderboardFetchStatus::Empty || entries.empty() )
        {
            sf::Text msg ( font_,
                fetch_status == LeaderboardFetchStatus::Empty ? "No scores yet"
                                                              : "Leaderboard unavailable", 17 );
            msg.setFillColor ( sf::Color ( 160, 190, 220, 180 ) );
            auto b = msg.getLocalBounds();
            msg.setOrigin ( {b.position.x + b.size.x / 2.f,
                             b.position.y + b.size.y / 2.f} );
            msg.setPosition ( {right_cx, lb_top + lb_h * 0.4f} );
            window.draw ( msg );
        }
        else
        {
            const float total_lb_h = static_cast<float>( entries.size() ) * row_step;
            preview_scroll_ = std::clamp(preview_scroll_, 0.f, std::max(0.f, total_lb_h - lb_h));

            sf::View lb_view ( sf::FloatRect ( {0.f, lb_top}, {ws.x, lb_h} ) );
            lb_view.setViewport ( sf::FloatRect(
                {0.f, lb_top / ws.y},
                {1.f, lb_h  / ws.y} ) );
            window.setView ( lb_view );

            for ( std::size_t i = 0; i < entries.size(); ++i )
            {
                const float y = lb_top + float(i)*row_step + row_step*0.5f - preview_scroll_;
                if ( y + row_step < lb_top || y - row_step > lb_bottom ) continue;

                const LeaderboardEntry& entry = entries[i];
                const std::string name = entry.playerName.empty() ? "Player" : entry.playerName;

                if ( i < 3 )
                {
                    static const sf::Color rank_colors[3] = {
                        sf::Color(255,210,40,48), sf::Color(210,215,220,34), sf::Color(200,140,80,34)
                    };
                    sf::RectangleShape row_bg ( {right_w-16.f, row_step-4.f} );
                    row_bg.setOrigin( {(right_w-16.f)*0.5f, (row_step-4.f)*0.5f} );
                    row_bg.setPosition( {right_cx, y} );
                    row_bg.setFillColor( rank_colors[i] );
                    window.draw( row_bg );
                }

                const std::string label =
                    std::to_string(int(i+1u)) + ". " + name
                    + "  " + std::to_string(entry.score) + " pts"
                    + "  " + std::string(std::size_t(std::clamp(entry.stars,0,3)),'*');

                sf::Text row ( font_, label, 16 );
                if      (i==0) row.setFillColor(sf::Color(255,220,60));
                else if (i==1) row.setFillColor(sf::Color(210,218,228));
                else if (i==2) row.setFillColor(sf::Color(210,155,95));
                else           row.setFillColor(sf::Color(200,218,238));
                auto b = row.getLocalBounds();
                row.setOrigin({b.position.x+b.size.x/2.f, b.position.y+b.size.y/2.f});
                row.setPosition({right_cx, y});
                window.draw(row);
            }

            window.setView(window.getDefaultView());

            if ( total_lb_h > lb_h )
            {
                sf::Text hint(font_,"[PgUp/PgDn] scroll leaderboard",13);
                hint.setFillColor(sf::Color(140,175,210,140));
                auto b = hint.getLocalBounds();
                hint.setOrigin({b.position.x+b.size.x/2.f, b.position.y});
                hint.setPosition({right_cx, lb_bottom+2.f});
                window.draw(hint);
            }
        }
    }

    window.setView ( window.getDefaultView() );

    const float badge_right = ws.x - 14.f;
    const float badge_top   = 10.f;
    const float pill_w      = 268.f;
    const float pill_h      = 50.f;

    sf::RectangleShape pill ( {pill_w, pill_h} );
    pill.setPosition ( {badge_right - pill_w, badge_top} );
    pill.setFillColor ( sf::Color ( 10, 18, 36, 200 ) );
    pill.setOutlineThickness ( 1.5f );
    pill.setOutlineColor ( sf::Color ( 80, 140, 220, 110 ) );
    window.draw ( pill );

    rect_badge_ = sf::FloatRect ( {badge_right - pill_w, badge_top}, {pill_w, pill_h} );

    if ( accounts_ && accounts_->is_logged_in() )
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

    badge_text_.setCharacterSize ( 17 );
    auto bt_bounds = badge_text_.getLocalBounds();
    badge_text_.setOrigin ( {bt_bounds.position.x, bt_bounds.position.y} );
    badge_text_.setPosition ( {badge_right - pill_w + 10.f, badge_top + 5.f} );
    window.draw ( badge_text_ );

    badge_btn_.setCharacterSize ( 15 );
    auto bb_bounds = badge_btn_.getLocalBounds();
    badge_btn_.setOrigin ( {bb_bounds.position.x, bb_bounds.position.y} );
    badge_btn_.setPosition ( {badge_right - pill_w + 10.f, badge_top + 27.f} );
    window.draw ( badge_btn_ );

#else  // __EMSCRIPTEN__ — Raylib render

    if ( !font_.loaded ) return;
    const platform::Vec2u ws_u = window.getSize();
    const float W = float(ws_u.x), H = float(ws_u.y);
    static platform::Clock anim_clock;
    const float t = anim_clock.getElapsedTime().asSeconds();
    const float pulse = 0.5f + 0.5f * std::sin(t*3.f);

    draw_vertical_gradient({12,28,55},{28,84,95},int(W),int(H));

    const float margin    = W * 0.015f;
    const float gap       = W * 0.015f;
    const float left_w    = W * 0.42f;
    const float right_w   = W - margin*2.f - left_w - gap;
    const float panel_top = H * 0.10f;
    const float panel_h   = H * 0.78f;
    const float left_cx   = margin + left_w*0.5f;
    const float right_x   = margin + left_w + gap;
    const float right_cx  = right_x + right_w*0.5f;

    rect_left_panel_  = { margin, panel_top, left_w, panel_h };
    rect_right_panel_ = { right_x, panel_top, right_w, panel_h };

    // Left panel
    DrawRectangle(int(margin),int(panel_top),int(left_w),int(panel_h),
                  platform::Color(8,16,30,255).to_rl());
    DrawRectangleLinesEx({margin,panel_top,left_w,panel_h},2.5f,
                         platform::Color(188,220,244,128).to_rl());

    // Title
    ::Vector2 tsz = MeasureTextEx(font_.rl,"Select Level",28.f,1.f);
    DrawTextEx(font_.rl,"Select Level",{left_cx-tsz.x*0.5f,panel_top+14.f},
               28.f,1.f,WHITE);

    // Level list
    const float step     = 48.f;
    const float list_top = panel_top + 60.f;
    const float list_bottom = panel_top + panel_h - 44.f;
    const float list_h   = list_bottom - list_top;
    const float total_h  = float(level_texts_.size()) * step;
    const float max_scroll = std::max(0.f, total_h - list_h);

    const float sel_y_local = float(selected_)*step + step*0.5f;
    if (sel_y_local-scroll_offset_ < step) scroll_offset_ = std::max(0.f,sel_y_local-step);
    if (sel_y_local-scroll_offset_ > list_h-step) scroll_offset_ = std::min(max_scroll,sel_y_local-list_h+step);
    scroll_offset_ = std::clamp(scroll_offset_,0.f,max_scroll);

    rects_level_items_screen_.resize(level_texts_.size());

    BeginScissorMode(int(margin),int(list_top),int(left_w),int(list_h));
    for ( int i = 0; i < (int)level_texts_.size(); ++i )
    {
        const float y = list_top + float(i)*step + step*0.5f - scroll_offset_;
        rects_level_items_screen_[i] = {margin, y-step*0.5f, left_w, step};
        if (y+step < list_top || y-step > list_bottom) continue;
        const bool is_sel = (i==selected_);
        if ( is_sel )
        {
            uint8_t ha = static_cast<uint8_t>(62.f+52.f*pulse);
            DrawRectangle(int(margin+5.f),int(y-20.f),int(left_w-20.f),40,
                          platform::Color(244,214,98,ha).to_rl());
        }
        platform::Color tc = is_sel ? platform::Color(255,247,220) : platform::Color(228,238,248);
        const char* lbl = level_texts_[i].string_.c_str();
        ::Vector2 lsz = MeasureTextEx(font_.rl,lbl,24.f,1.f);
        DrawTextEx(font_.rl,lbl,{left_cx-lsz.x*0.5f,y-lsz.y*0.5f},24.f,1.f,tc.to_rl());
    }
    EndScissorMode();

    // Right panel
    DrawRectangle(int(right_x),int(panel_top),int(right_w),int(panel_h),
                  platform::Color(8,14,26,255).to_rl());
    DrawRectangleLinesEx({right_x,panel_top,right_w,panel_h},2.f,
                         platform::Color(80,140,220,110).to_rl());

    if ( !levels_.empty() )
    {
        const auto& meta = levels_[selected_];
        float cy = panel_top + 28.f;
        ::Vector2 nsz = MeasureTextEx(font_.rl,meta.name.c_str(),28.f,1.f);
        DrawTextEx(font_.rl,meta.name.c_str(),{right_cx-nsz.x*0.5f,cy},28.f,1.f,
                   platform::Color(230,245,255).to_rl());
        cy += 38.f;

        const LevelScore* ls = find_score(meta.id);
        std::string loc_str = "Local best: ";
        if (ls && ls->bestScore > 0) loc_str += std::to_string(ls->bestScore)+" pts  "+std::string(ls->bestStars,'*');
        else loc_str += "not played";
        ::Vector2 lsz2 = MeasureTextEx(font_.rl,loc_str.c_str(),18.f,1.f);
        DrawTextEx(font_.rl,loc_str.c_str(),{right_cx-lsz2.x*0.5f,cy},18.f,1.f,
                   platform::Color(180,220,255).to_rl());
        cy += 30.f;

        DrawLine(int(right_x+10),int(cy),int(right_x+right_w-10),int(cy),
                 platform::Color(80,130,200,80).to_rl());
        cy += 10.f;

        DrawTextEx(font_.rl,"Online Leaderboard",{right_cx-90.f,cy},20.f,1.f,
                   platform::Color(200,225,255).to_rl());
        cy += 32.f;

        LeaderboardFetchStatus fetch_status;
        std::vector<LeaderboardEntry> entries;
        int fetched_id;
        {
            std::lock_guard<std::mutex> lock(preview_->mutex);
            fetch_status = preview_->fetch_status;
            entries      = preview_->entries;
            fetched_id   = preview_->fetched_level_id;
        }

        const float lb_top    = cy;
        const float lb_bottom = panel_top + panel_h - 14.f;
        const float lb_h      = lb_bottom - lb_top;
        const float row_step  = 32.f;

        if ( fetched_id != meta.id )
        {
            DrawTextEx(font_.rl,"Loading...",{right_cx-50.f,lb_top+lb_h*0.4f},17.f,1.f,
                       platform::Color(160,190,220,160).to_rl());
        }
        else if (entries.empty())
        {
            const char* em = (fetch_status==LeaderboardFetchStatus::Empty)?"No scores yet":"Leaderboard unavailable";
            DrawTextEx(font_.rl,em,{right_cx-80.f,lb_top+lb_h*0.4f},17.f,1.f,
                       platform::Color(160,190,220,180).to_rl());
        }
        else
        {
            preview_scroll_ = std::clamp(preview_scroll_,0.f,std::max(0.f,float(entries.size())*row_step-lb_h));
            BeginScissorMode(int(right_x),int(lb_top),int(right_w),int(lb_h));
            static const platform::Color rank_col[4] = {{255,220,60},{210,218,228},{210,155,95},{200,218,238}};
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const float y = lb_top + float(i)*row_step - preview_scroll_;
                if (y+row_step < lb_top || y > lb_bottom) continue;
                const std::string& pname = entries[i].playerName.empty() ? "Player" : entries[i].playerName;
                std::string lbl2 = std::to_string(int(i+1u))+". "+pname
                    +"  "+std::to_string(entries[i].score)+" pts"
                    +"  "+std::string(std::size_t(std::clamp(entries[i].stars,0,3)),'*');
                DrawTextEx(font_.rl,lbl2.c_str(),{right_x+8.f,y},16.f,1.f,
                           rank_col[std::min((int)i,3)].to_rl());
            }
            EndScissorMode();
        }
    }

    // Badge
    const float badge_right = W - 14.f, badge_top2 = 10.f;
    const float pill_w = 268.f, pill_h = 50.f;
    DrawRectangle(int(badge_right-pill_w),int(badge_top2),int(pill_w),int(pill_h),
                  platform::Color(10,18,36,255).to_rl());
    DrawRectangleLinesEx({badge_right-pill_w,badge_top2,pill_w,pill_h},1.5f,
                         platform::Color(80,140,220,110).to_rl());
    rect_badge_ = {badge_right-pill_w,badge_top2,pill_w,pill_h};

    bool logged = accounts_ && accounts_->is_logged_in();
    std::string badge_str = logged ? "Logged in as " + accounts_->username() : "Guest mode";
    platform::Color badge_col = logged ? platform::Color(100,220,140) : platform::Color(200,180,100);
    DrawTextEx(font_.rl,badge_str.c_str(),{badge_right-pill_w+10.f,badge_top2+6.f},17.f,1.f,badge_col.to_rl());
    std::string btn_str = logged ? "[L] Logout" : "[L] Login";
    platform::Color btn_col = logged ? platform::Color(255,120,120) : platform::Color(80,160,255);
    DrawTextEx(font_.rl,btn_str.c_str(),{badge_right-pill_w+10.f,badge_top2+28.f},15.f,1.f,btn_col.to_rl());

#endif
}

}  // namespace angry
