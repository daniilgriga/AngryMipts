#include "ui/game_scene.hpp"

#include "data/logger.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace angry
{
namespace
{

constexpr float kCameraWidth = 1280.f;
constexpr float kCameraHeight = 720.f;
constexpr float kWorldAspect = kCameraWidth / kCameraHeight;
constexpr float kImpactFlashDecay = 3.0f;

constexpr auto kPostFxFragmentShader = R"GLSL(
uniform sampler2D texture;
uniform float uTime;
uniform float uVignette;
uniform float uFlash;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 color = texture2D(texture, uv);

    // Slight filmic curve and contrast to make sprites look less flat.
    color.rgb = pow(color.rgb, vec3(0.95));
    color.rgb = (color.rgb - vec3(0.5)) * 1.08 + vec3(0.5);

    float dist = distance(uv, vec2(0.5, 0.5));
    float vignette = smoothstep(0.38, 0.86, dist);
    color.rgb *= 1.0 - uVignette * vignette;

    float atmosphere = 0.016 * sin(uTime * 1.4 + uv.y * 11.0);
    color.rgb += vec3(atmosphere);

    float flashShape = max(0.0, 0.62 - dist);
    color.rgb += vec3(uFlash * flashShape);

    gl_FragColor = color;
}
)GLSL";

void apply_letterbox ( sf::View& view, sf::Vector2u window_size )
{
    if ( window_size.x == 0 || window_size.y == 0 )
        return;

    const float window_aspect =
        static_cast<float> ( window_size.x ) / static_cast<float> ( window_size.y );

    if ( window_aspect > kWorldAspect )
    {
        const float width = kWorldAspect / window_aspect;
        const float left = ( 1.f - width ) * 0.5f;
        view.setViewport ( sf::FloatRect ( {left, 0.f}, {width, 1.f} ) );
    }
    else if ( window_aspect < kWorldAspect )
    {
        const float height = window_aspect / kWorldAspect;
        const float top = ( 1.f - height ) * 0.5f;
        view.setViewport ( sf::FloatRect ( {0.f, top}, {1.f, height} ) );
    }
    else
    {
        view.setViewport ( sf::FloatRect ( {0.f, 0.f}, {1.f, 1.f} ) );
    }
}

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

sf::Color material_particle_color ( Material mat )
{
    switch ( mat )
    {
    case Material::Wood:
        return sf::Color ( 200, 140, 70 );
    case Material::Stone:
        return sf::Color ( 170, 170, 170 );
    case Material::Glass:
        return sf::Color ( 180, 230, 255 );
    case Material::Ice:
        return sf::Color ( 210, 240, 255 );
    default:
        return sf::Color ( 200, 200, 200 );
    }
}

sf::Color projectile_trail_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Heavy:
        return sf::Color ( 186, 145, 240, 170 );
    case ProjectileType::Splitter:
        return sf::Color ( 148, 220, 255, 170 );
    case ProjectileType::Standard:
    default:
        return sf::Color ( 255, 165, 136, 165 );
    }
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

    snap.slingshot.basePx = {200.f, 550.f};
    snap.slingshot.pullOffsetPx = {0.f, 0.f};
    snap.slingshot.maxPullPx = 120.f;
    snap.slingshot.canShoot = true;
    snap.slingshot.nextProjectile = ProjectileType::Standard;

    EntityId id = 1;

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {640.f, 700.f}, 0.f, {1280.f, 40.f}, 0.f,
                              Material::Stone, ProjectileType::Standard, 1.f, true} );

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {800.f, 580.f}, 0.f, {20.f, 100.f}, 0.f,
                              Material::Wood, ProjectileType::Standard, 1.f, true} );

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {900.f, 580.f}, 0.f, {20.f, 100.f}, 0.f,
                              Material::Wood, ProjectileType::Standard, 1.f, true} );

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {850.f, 520.f}, 0.f, {140.f, 20.f}, 0.f,
                              Material::Wood, ProjectileType::Standard, 0.8f, true} );

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Block,
                              {850.f, 500.f}, 0.f, {60.f, 20.f}, 0.f,
                              Material::Glass, ProjectileType::Standard, 1.f, true} );

    snap.objects.push_back ( {id++, ObjectSnapshot::Kind::Target,
                              {850.f, 560.f}, 0.f, {0.f, 0.f}, 15.f,
                              Material::Wood, ProjectileType::Standard, 1.f, true} );

    return snap;
}

GameScene::GameScene ( const sf::Font& font )
    : snapshot_ ( make_mock_snapshot() )
    , font_ ( font )
    , hud_text_ ( font_, "", 20 )
    , game_view_ ( sf::FloatRect ( {0.f, 0.f}, {kCameraWidth, kCameraHeight} ) )
{
    hud_text_.setFillColor ( sf::Color::White );
    hud_text_.setPosition ( {20.f, 20.f} );

    if ( sf::Shader::isAvailable() )
    {
        post_shader_ready_ = post_shader_.loadFromMemory (
            kPostFxFragmentShader, sf::Shader::Type::Fragment );
        if ( post_shader_ready_ )
        {
            post_shader_.setUniform ( "texture", sf::Shader::CurrentTexture );
        }
        else
        {
            Logger::error ( "GameScene: failed to load post-processing shader" );
        }
    }
    else
    {
        Logger::info ( "GameScene: shaders are unavailable, using fallback rendering path" );
    }
}

void GameScene::load_level ( int level_id, const std::string& scores_path )
{
    level_id_ = level_id;
    scores_path_ = scores_path;
    pending_scene_ = SceneId::None;
    end_delay_ = 0.f;

    try
    {
        const std::string path = resolveProjectPath (
            "levels/level_0" + std::to_string ( level_id ) + ".json" );
        const LevelData level = level_loader_.load ( path );
        current_meta_ = level.meta;
        physics_.registerLevel ( level );
        physics_.loadLevel ( level );
        snapshot_ = physics_.getSnapshot();
        frame_clock_.restart();
        Logger::info ( "GameScene: loaded level {}", level_id );
    }
    catch ( const std::exception& e )
    {
        Logger::error ( "GameScene: failed to load level {}: {}", level_id, e.what() );
    }
}

void GameScene::retry()
{
    load_level ( level_id_, scores_path_ );
}

void GameScene::finish_level()
{
    const bool won = ( snapshot_.status == LevelStatus::Win );
    const int score = snapshot_.score;

    int stars = 0;
    if ( won && current_meta_.id > 0 )
    {
        if ( score >= current_meta_.star3Threshold )
            stars = 3;
        else if ( score >= current_meta_.star2Threshold )
            stars = 2;
        else if ( score >= current_meta_.star1Threshold )
            stars = 1;
    }

    last_result_ = { won, score, stars };

    if ( !scores_path_.empty() && level_id_ > 0 )
    {
        try
        {
            score_saver_.saveScore ( scores_path_, level_id_, score, stars );
        }
        catch ( const std::exception& e )
        {
            Logger::error ( "GameScene: failed to save score: {}", e.what() );
        }
    }

    pending_scene_ = SceneId::Result;
}

void GameScene::process_events()
{
    auto events = physics_.drainEvents();
    for ( const auto& ev : events )
    {
        std::visit (
            [this] ( const auto& e )
            {
                using T = std::decay_t<decltype ( e )>;

                if constexpr ( std::is_same_v<T, CollisionEvent> )
                {
                    sf::Vector2f pos ( e.contactPointPx.x, e.contactPointPx.y );
                    int count = std::clamp ( static_cast<int> ( e.impulse * 2.f ), 3, 15 );
                    float speed = std::clamp ( e.impulse * 30.f, 40.f, 200.f );
                    particles_.emit ( pos, count,
                                      sf::Color ( 255, 220, 150 ), speed, 0.4f, 3.f );

                    const float impulse = std::clamp ( e.impulse, 0.f, 30.f );
                    if ( impulse > 1.2f )
                    {
                        shake_time_ =
                            std::max ( shake_time_, std::clamp ( 0.06f + impulse * 0.012f,
                                                                 0.06f, 0.24f ) );
                        shake_strength_ =
                            std::max ( shake_strength_, std::clamp ( impulse * 0.55f, 2.f, 14.f ) );
                        impact_flash_ =
                            std::max ( impact_flash_, std::clamp ( impulse * 0.03f, 0.04f, 0.26f ) );
                    }
                }
                else if constexpr ( std::is_same_v<T, DestroyedEvent> )
                {
                    sf::Vector2f pos ( e.positionPx.x, e.positionPx.y );
                    sf::Color color = material_particle_color ( e.material );
                    particles_.emit ( pos, 20, color, 150.f, 0.6f, 4.f );
                    impact_flash_ = std::max ( impact_flash_, 0.08f );
                }
            },
            ev );
    }
}

SceneId GameScene::handle_input ( const sf::Event& event )
{
    if ( pending_scene_ != SceneId::None )
    {
        SceneId next = pending_scene_;
        pending_scene_ = SceneId::None;
        return next;
    }

    if ( const auto* key = event.getIf<sf::Event::KeyPressed>() )
    {
        if ( key->code == sf::Keyboard::Key::Backspace )
            return SceneId::Menu;

        if ( key->code == sf::Keyboard::Key::Space )
            command_queue_.push ( ActivateAbilityCmd{INVALID_ID} );
    }

    if ( snapshot_.status == LevelStatus::Running && window_ptr_ )
    {
        apply_letterbox ( game_view_, window_ptr_->getSize() );
        auto cmd = slingshot_.handle_input ( event, snapshot_.slingshot, *window_ptr_,
                                             game_view_ );
        if ( cmd.has_value() )
            command_queue_.push ( *cmd );
    }

    return SceneId::None;
}

void GameScene::update()
{
    const float dt = std::clamp ( frame_clock_.restart().asSeconds(), 0.0f, 1.0f / 30.0f );

    particles_.update ( dt );

    if ( snapshot_.status != LevelStatus::Running )
    {
        end_delay_ += dt;
        if ( end_delay_ >= 1.5f && pending_scene_ == SceneId::None )
            finish_level();
        return;
    }

    physics_.processCommands ( command_queue_ );
    physics_.step ( dt );
    snapshot_ = physics_.getSnapshot();
    process_events();

    for ( const auto& obj : snapshot_.objects )
    {
        if ( obj.isActive && obj.kind == ObjectSnapshot::Kind::Projectile )
        {
            particles_.emit ( {obj.positionPx.x, obj.positionPx.y}, 2,
                              projectile_trail_color ( obj.projectileType ),
                              38.f, 0.20f, 3.5f );
            break;
        }
    }

    if ( shake_time_ > 0.f )
    {
        shake_time_ = std::max ( 0.f, shake_time_ - dt );
        shake_strength_ = std::max ( 0.f, shake_strength_ - dt * 30.f );
    }

    impact_flash_ = std::max ( 0.f, impact_flash_ - dt * kImpactFlashDecay );

    hud_text_.setString ( "Score: " + std::to_string ( snapshot_.score )
                          + "   [Space] Ability   [Backspace] Menu" );
}

void GameScene::render ( sf::RenderWindow& window )
{
    window_ptr_ = &window;
    apply_letterbox ( game_view_, window.getSize() );

    if ( world_pass_.getSize() != window.getSize() )
    {
        if ( !world_pass_.resize ( window.getSize() ) )
        {
            Logger::error ( "GameScene: failed to resize world render target" );
        }
        world_pass_.setSmooth ( true );
    }

    // World rendering in game coordinates
    sf::View world_view = game_view_;
    if ( shake_time_ > 0.f && shake_strength_ > 0.f )
    {
        world_view.move ( {shake_dist_ ( rng_ ) * shake_strength_,
                           shake_dist_ ( rng_ ) * shake_strength_} );
    }

    world_pass_.clear ( sf::Color ( 6, 8, 14 ) );
    world_pass_.setView ( world_view );
    renderer_.draw_snapshot ( world_pass_, snapshot_ );
    slingshot_.render ( world_pass_, snapshot_.slingshot );
    particles_.render ( world_pass_ );
    world_pass_.display();

    window.setView ( window.getDefaultView() );
    sf::Sprite world_sprite ( world_pass_.getTexture() );

    if ( post_shader_ready_ )
    {
        post_shader_.setUniform ( "uTime", visual_clock_.getElapsedTime().asSeconds() );
        post_shader_.setUniform ( "uVignette", 0.34f );
        post_shader_.setUniform ( "uFlash", impact_flash_ );
        sf::RenderStates states;
        states.shader = &post_shader_;
        window.draw ( world_sprite, states );
    }
    else
    {
        window.draw ( world_sprite );
    }

    // HUD in screen coordinates
    renderer_.draw_hud ( window, snapshot_, hud_text_ );
}

}  // namespace angry
