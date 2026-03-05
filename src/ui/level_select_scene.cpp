#include "ui/level_select_scene.hpp"

#include "data/logger.hpp"

namespace angry
{

LevelSelectScene::LevelSelectScene ( const sf::Font& font )
    : font_ ( font )
    , title_ ( font_, "Select Level", 42 )
    , prompt_ ( font_, "[Enter] Play   [Backspace] Menu", 18 )
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
    }
    return SceneId::None;
}

void LevelSelectScene::update()
{
}

void LevelSelectScene::render ( sf::RenderWindow& window )
{
    auto ws = sf::Vector2f ( window.getSize() );

    sf::Vertex background[] = {
        {{0.f, 0.f}, sf::Color ( 16, 34, 62 )},
        {{ws.x, 0.f}, sf::Color ( 16, 34, 62 )},
        {{ws.x, ws.y}, sf::Color ( 36, 78, 96 )},
        {{0.f, ws.y}, sf::Color ( 36, 78, 96 )},
    };
    window.draw ( background, 4, sf::PrimitiveType::TriangleFan );

    sf::RectangleShape panel ( {ws.x * 0.64f, ws.y * 0.62f} );
    panel.setOrigin ( {panel.getSize().x * 0.5f, panel.getSize().y * 0.5f} );
    panel.setPosition ( {ws.x * 0.5f, ws.y * 0.5f} );
    panel.setFillColor ( sf::Color ( 8, 16, 28, 130 ) );
    panel.setOutlineThickness ( 2.f );
    panel.setOutlineColor ( sf::Color ( 188, 220, 244, 115 ) );
    window.draw ( panel );

    auto title_bounds = title_.getLocalBounds();
    title_.setOrigin ( {title_bounds.position.x + title_bounds.size.x / 2.f,
                        title_bounds.position.y + title_bounds.size.y / 2.f} );
    title_.setPosition ( {ws.x / 2.f, ws.y * 0.12f} );
    window.draw ( title_ );

    float start_y = ws.y * 0.28f;
    float step = 52.f;

    for ( int i = 0; i < static_cast<int> ( level_texts_.size() ); ++i )
    {
        auto& text = level_texts_[i];
        auto bounds = text.getLocalBounds();
        text.setOrigin ( {bounds.position.x + bounds.size.x / 2.f,
                          bounds.position.y + bounds.size.y / 2.f} );
        text.setPosition ( {ws.x / 2.f, start_y + i * step} );
        window.draw ( text );
    }

    auto prompt_bounds = prompt_.getLocalBounds();
    prompt_.setOrigin ( {prompt_bounds.position.x + prompt_bounds.size.x / 2.f,
                         prompt_bounds.position.y + prompt_bounds.size.y / 2.f} );
    prompt_.setPosition ( {ws.x / 2.f, ws.y * 0.88f} );
    window.draw ( prompt_ );
}

}  // namespace angry
