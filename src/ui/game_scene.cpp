#include "ui/game_scene.hpp"

#include "data/logger.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace angry
{
namespace
{

std::string resolveProjectPath( const std::filesystem::path& relativePath )
{
    if ( std::filesystem::exists( relativePath ) )
    {
        return relativePath.string();
    }

#ifdef ANGRY_MIPTS_SOURCE_DIR
    const std::filesystem::path fromSourceDir =
        std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / relativePath;
    if ( std::filesystem::exists( fromSourceDir ) )
    {
        return fromSourceDir.string();
    }
#endif

    return relativePath.string();
}

}  // namespace

WorldSnapshot GameScene::make_mock_snapshot()
{
    WorldSnapshot snap{};
    snap.score = 0;
    snap.shotsRemaining = 3;
    snap.totalShots = 3;
    snap.status = LevelStatus::Running;
    snap.stars = 0;
    snap.physicsStepMs = 0.f;

    // Slingshot
    snap.slingshot.basePx = {200.f, 550.f};
    snap.slingshot.pullOffsetPx = {0.f, 0.f};
    snap.slingshot.maxPullPx = 120.f;
    snap.slingshot.canShoot = true;
    snap.slingshot.nextProjectile = ProjectileType::Standard;

    EntityId id = 1;

    // Ground
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {640.f, 700.f}, 0.f, {1280.f, 40.f}, 0.f,
                              Material::Stone, 1.f, true} );

    // Left pillar
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {800.f, 580.f}, 0.f, {20.f, 100.f}, 0.f,
                              Material::Wood, 1.f, true} );

    // Right pillar
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {900.f, 580.f}, 0.f, {20.f, 100.f}, 0.f,
                              Material::Wood, 1.f, true} );

    // Top beam
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {850.f, 520.f}, 0.f, {140.f, 20.f}, 0.f,
                              Material::Wood, 0.8f, true} );

    // Glass block on top
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {850.f, 500.f}, 0.f, {60.f, 20.f}, 0.f,
                              Material::Glass, 1.f, true} );

    // Target (circle)
    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Target,
                              {850.f, 560.f}, 0.f, {0.f, 0.f}, 15.f,
                              Material::Wood, 1.f, true} );

    return snap;
}

GameScene::GameScene ( const sf::Font& font )
    : snapshot_ ( make_mock_snapshot() )
    , font_ ( font )
    , hud_text_ ( font_, "", 20 )
{
    hud_text_.setFillColor ( sf::Color::White );
    hud_text_.setPosition ( {20.f, 20.f} );

    try
    {
        const std::string levelPath = resolveProjectPath( "levels/level_03.json" );
        const LevelData level = level_loader_.load ( levelPath );
        physics_.loadLevel ( level );
        snapshot_ = physics_.getSnapshot();
        Logger::info ( "Loaded level {} for GameScene from {}", level.meta.id, levelPath );
    }
    catch ( const std::exception& error )
    {
        Logger::error ( "Failed to load level_03.json: {}", error.what() );
    }
}

SceneId GameScene::handle_input ( const sf::Event& event )
{
    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;
    }

    auto cmd = slingshot_.handle_input ( event, snapshot_.slingshot );
    if ( cmd.has_value() )
    {
        command_queue_.push ( *cmd );
    }

    return SceneId::None;
}

void GameScene::update()
{
    const float dt = std::clamp ( frame_clock_.restart().asSeconds(), 0.0f, 1.0f / 30.0f );
    physics_.processCommands ( command_queue_ );
    physics_.step ( dt );
    snapshot_ = physics_.getSnapshot();

    hud_text_.setString ( "Score: " + std::to_string ( snapshot_.score )
                          + "  Shots: " + std::to_string ( snapshot_.shotsRemaining )
                          + "/" + std::to_string ( snapshot_.totalShots )
                          + "  [Backspace] Menu" );
}

void GameScene::render ( sf::RenderWindow& window )
{
    renderer_.draw_snapshot ( window, snapshot_ );
    slingshot_.render ( window, snapshot_.slingshot );
    window.draw ( hud_text_ );
}

}  // namespace angry
