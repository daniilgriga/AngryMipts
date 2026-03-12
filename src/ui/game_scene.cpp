#include "ui/game_scene.hpp"

#include "data/logger.hpp"
#include "shared/world_config.hpp"
#include "ui/view_utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace angry
{
namespace
{

constexpr float kImpactFlashDecay = 3.0f;
constexpr float kStrongImpactThreshold = 8.0f;
constexpr float kStrongImpactMax = 22.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kMaxQueuedEvents = 1200u;
constexpr int kMinEventsPerFrame = 28;
constexpr int kMaxEventsPerFrame = 110;

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

constexpr auto kBloomExtractFragmentShader = R"GLSL(
uniform sampler2D texture;
uniform float uThreshold;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 color = texture2D(texture, uv);
    float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    float t = clamp((luma - uThreshold) / (1.0 - uThreshold), 0.0, 1.0);
    t = t * t;
    gl_FragColor = vec4(color.rgb * t, color.a * t);
}
)GLSL";

constexpr auto kBloomBlurFragmentShader = R"GLSL(
uniform sampler2D texture;
uniform vec2 uTexel;
uniform vec2 uDirection;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 sum = texture2D(texture, uv).rgb * 0.227027;
    sum += texture2D(texture, uv + uDirection * uTexel * 1.384615).rgb * 0.316216;
    sum += texture2D(texture, uv - uDirection * uTexel * 1.384615).rgb * 0.316216;
    sum += texture2D(texture, uv + uDirection * uTexel * 3.230769).rgb * 0.070270;
    sum += texture2D(texture, uv - uDirection * uTexel * 3.230769).rgb * 0.070270;
    gl_FragColor = vec4(sum, 1.0);
}
)GLSL";

float strong_impact_factor ( float impulse )
{
    if ( impulse < kStrongImpactThreshold )
        return 0.f;

    return std::clamp (
        ( impulse - kStrongImpactThreshold ) / ( kStrongImpactMax - kStrongImpactThreshold ),
        0.f, 1.f );
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

std::string two_digit ( int value )
{
    std::ostringstream out;
    out << std::setw ( 2 ) << std::setfill ( '0' ) << value;
    return out.str();
}

std::string resolveLevelPath ( int levelId )
{
    const std::array<std::string, 3> candidates = {
        "levels/level_0" + std::to_string ( levelId ) + ".json",
        "levels/level_" + two_digit ( levelId ) + ".json",
        "levels/level_" + std::to_string ( levelId ) + ".json",
    };

    for ( const std::string& candidate : candidates )
    {
        const std::string resolved = resolveProjectPath ( candidate );
        if ( std::filesystem::exists ( resolved ) )
            return resolved;
    }

    // Keep old error surface if none exists.
    return resolveProjectPath ( candidates.front() );
}

sf::Color projectile_trail_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Dasher:
        return sf::Color ( 255, 210, 132, 172 );
    case ProjectileType::Bomber:
        return sf::Color ( 255, 178, 102, 180 );
    case ProjectileType::Dropper:
        return sf::Color ( 166, 236, 208, 170 );
    case ProjectileType::Boomerang:
        return sf::Color ( 220, 244, 160, 168 );
    case ProjectileType::Bubbler:
        return sf::Color ( 176, 234, 255, 175 );
    case ProjectileType::Inflater:
        return sf::Color ( 255, 194, 224, 176 );
    case ProjectileType::Heavy:
        return sf::Color ( 186, 145, 240, 170 );
    case ProjectileType::Splitter:
        return sf::Color ( 148, 220, 255, 170 );
    case ProjectileType::Standard:
    default:
        return sf::Color ( 255, 165, 136, 165 );
    }
}

sf::Color ability_core_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Bomber:
        return sf::Color ( 255, 206, 132 );
    case ProjectileType::Dropper:
        return sf::Color ( 214, 255, 236 );
    case ProjectileType::Dasher:
        return sf::Color ( 255, 220, 160 );
    case ProjectileType::Boomerang:
        return sf::Color ( 236, 255, 178 );
    case ProjectileType::Bubbler:
        return sf::Color ( 214, 248, 255 );
    case ProjectileType::Inflater:
        return sf::Color ( 255, 220, 238 );
    case ProjectileType::Heavy:
        return sf::Color ( 196, 146, 255 );
    case ProjectileType::Splitter:
        return sf::Color ( 216, 246, 255 );
    case ProjectileType::Standard:
    default:
        return sf::Color ( 255, 190, 150 );
    }
}

sf::Color ability_glow_color ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Bomber:
        return sf::Color ( 255, 148, 76, 228 );
    case ProjectileType::Dropper:
        return sf::Color ( 78, 196, 162, 220 );
    case ProjectileType::Dasher:
        return sf::Color ( 248, 162, 70, 220 );
    case ProjectileType::Boomerang:
        return sf::Color ( 170, 214, 86, 220 );
    case ProjectileType::Bubbler:
        return sf::Color ( 98, 194, 240, 220 );
    case ProjectileType::Inflater:
        return sf::Color ( 232, 120, 176, 220 );
    case ProjectileType::Heavy:
        return sf::Color ( 112, 76, 196, 220 );
    case ProjectileType::Splitter:
        return sf::Color ( 126, 214, 255, 220 );
    case ProjectileType::Standard:
    default:
        return sf::Color ( 255, 170, 120, 210 );
    }
}

struct AbilityVfxProfile
{
    int ringPrimaryCount;
    float ringPrimarySpeed;
    float ringPrimaryLifetime;
    float ringPrimarySize;
    int ringSecondaryCount;
    float ringSecondarySpeed;
    float ringSecondaryLifetime;
    float ringSecondarySize;
    int burstCount;
    float burstSpeed;
    float burstLifetime;
    float burstSize;
    int shardCount;
    float shardSpeed;
    float shardLifetime;
    float shardSize;
    float shardAngularSpeed;
    float shakeTime;
    float shakeStrength;
    float flashBoost;
    bool burstUsesGlow;
};

AbilityVfxProfile ability_vfx_profile ( ProjectileType type )
{
    switch ( type )
    {
    case ProjectileType::Heavy:
        return {
            18,   // primary ring count
            128.f, 0.24f, 4.6f,
            0,    // secondary ring disabled
            0.f, 0.f, 0.f,
            24,   // burst
            165.f, 0.42f, 4.9f,
            10,   // shards
            138.f, 0.52f, 5.4f, 380.f,
            0.14f, 6.8f, 0.16f,
            true  // burst is glow, shards are core
        };
    case ProjectileType::Bomber:
        return {
            28,
            214.f, 0.26f, 5.4f,
            16,
            132.f, 0.34f, 4.3f,
            40,
            228.f, 0.46f, 5.4f,
            18,
            206.f, 0.72f, 5.8f, 760.f,
            0.16f, 7.4f, 0.22f,
            true
        };
    case ProjectileType::Dropper:
        return {
            16,
            122.f, 0.20f, 3.8f,
            10,
            88.f, 0.28f, 3.0f,
            24,
            170.f, 0.38f, 4.1f,
            14,
            192.f, 0.50f, 4.2f, 560.f,
            0.11f, 4.8f, 0.13f,
            false
        };
    case ProjectileType::Dasher:
        return {
            14,
            154.f, 0.16f, 3.2f,
            0,
            0.f, 0.f, 0.f,
            14,
            198.f, 0.24f, 3.4f,
            8,
            210.f, 0.30f, 3.1f, 520.f,
            0.08f, 4.9f, 0.10f,
            true
        };
    case ProjectileType::Boomerang:
        return {
            16,
            132.f, 0.19f, 3.6f,
            8,
            88.f, 0.28f, 3.1f,
            16,
            165.f, 0.30f, 3.7f,
            10,
            174.f, 0.42f, 3.5f, 460.f,
            0.09f, 4.5f, 0.12f,
            false
        };
    case ProjectileType::Bubbler:
        return {
            18,
            110.f, 0.24f, 3.9f,
            14,
            72.f, 0.34f, 3.3f,
            20,
            128.f, 0.36f, 3.8f,
            12,
            142.f, 0.50f, 3.6f, 420.f,
            0.09f, 4.2f, 0.12f,
            false
        };
    case ProjectileType::Inflater:
        return {
            20,
            126.f, 0.26f, 4.3f,
            10,
            84.f, 0.34f, 3.7f,
            22,
            150.f, 0.40f, 4.2f,
            10,
            160.f, 0.54f, 3.8f, 500.f,
            0.10f, 4.5f, 0.13f,
            false
        };
    case ProjectileType::Splitter:
        return {
            20,
            152.f, 0.20f, 4.1f,
            12,
            92.f, 0.30f, 3.3f,
            20,
            172.f, 0.34f, 3.9f,
            14,
            182.f, 0.40f, 3.7f, 520.f,
            0.10f, 5.2f, 0.14f,
            false  // burst is core, shards are glow
        };
    case ProjectileType::Standard:
    default:
        // Defensive fallback for future ability-enabled projectiles.
        return {
            14,
            112.f, 0.22f, 3.5f,
            0,
            0.f, 0.f, 0.f,
            16,
            138.f, 0.30f, 3.5f,
            8,
            132.f, 0.34f, 3.2f, 320.f,
            0.08f, 4.2f, 0.10f,
            false
        };
    }
}

struct MaterialVfxProfile
{
    sf::Color sparkColor;
    sf::Color dustColor;
    sf::Color shardColor;
    float impulseToCount;
    float hitSpeedScale;
    int minHitCount;
    int maxHitCount;
    int destroyBurstCount;
    int shardCount;
    float shardSpeed;
    float shardSize;
    float hitFlashBoost;
    float destroyFlashBoost;
    bool ringOnHit;
    bool ringOnDestroy;
};

const MaterialVfxProfile& vfx_profile ( Material material )
{
    static const MaterialVfxProfile wood {
        sf::Color ( 230, 170, 90 ),
        sf::Color ( 170, 120, 74, 180 ),
        sf::Color ( 190, 130, 78 ),
        1.7f,
        26.f,
        4,
        14,
        26,
        9,
        120.f,
        5.f,
        0.07f,
        0.10f,
        false,
        false,
    };

    static const MaterialVfxProfile stone {
        sf::Color ( 214, 214, 220 ),
        sf::Color ( 145, 150, 160, 210 ),
        sf::Color ( 172, 176, 186 ),
        1.3f,
        21.f,
        3,
        11,
        20,
        6,
        85.f,
        4.3f,
        0.05f,
        0.08f,
        false,
        false,
    };

    static const MaterialVfxProfile glass {
        sf::Color ( 220, 245, 255 ),
        sf::Color ( 170, 226, 255, 170 ),
        sf::Color ( 204, 240, 255 ),
        2.2f,
        32.f,
        6,
        20,
        34,
        20,
        210.f,
        4.2f,
        0.11f,
        0.16f,
        true,
        true,
    };

    static const MaterialVfxProfile ice {
        sf::Color ( 234, 250, 255 ),
        sf::Color ( 178, 225, 255, 178 ),
        sf::Color ( 214, 246, 255 ),
        2.0f,
        30.f,
        5,
        17,
        30,
        16,
        170.f,
        4.6f,
        0.09f,
        0.13f,
        true,
        true,
    };

    switch ( material )
    {
    case Material::Stone:
        return stone;
    case Material::Glass:
        return glass;
    case Material::Ice:
        return ice;
    case Material::Wood:
    default:
        return wood;
    }
}

const ObjectSnapshot* find_snapshot_object ( const WorldSnapshot& snapshot, EntityId id )
{
    for ( const auto& object : snapshot.objects )
    {
        if ( object.id == id )
            return &object;
    }
    return nullptr;
}

Material choose_impact_material ( const WorldSnapshot& snapshot, EntityId aId, EntityId bId )
{
    const ObjectSnapshot* a = find_snapshot_object ( snapshot, aId );
    const ObjectSnapshot* b = find_snapshot_object ( snapshot, bId );

    const auto prefer = [] ( const ObjectSnapshot* obj ) -> bool
    {
        if ( !obj || !obj->isActive )
            return false;
        return obj->kind == ObjectSnapshot::Kind::Block
               || obj->kind == ObjectSnapshot::Kind::Target;
    };

    if ( prefer ( a ) && prefer ( b ) )
    {
        if ( a->material == Material::Glass || b->material == Material::Glass )
            return Material::Glass;
        if ( a->material == Material::Ice || b->material == Material::Ice )
            return Material::Ice;
        if ( a->material == Material::Stone || b->material == Material::Stone )
            return Material::Stone;
        return a->material;
    }

    if ( prefer ( a ) )
        return a->material;
    if ( prefer ( b ) )
        return b->material;

    return Material::Wood;
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
    snap.projectileQueue = {
        ProjectileType::Standard,
        ProjectileType::Heavy,
        ProjectileType::Splitter,
    };

    EntityId id = 1;

    auto make_block = [] ( EntityId eid, Vec2 pos, Vec2 size, Material mat,
                           float hp = 1.f, bool is_static = false ) -> ObjectSnapshot
    {
        ObjectSnapshot o{};
        o.id = eid; o.kind = ObjectSnapshot::Kind::Block;
        o.positionPx = pos; o.sizePx = size;
        o.material = mat; o.shape = BlockShape::Rect;
        o.isStatic = is_static; o.hpNormalized = hp; o.isActive = true;
        return o;
    };

    snap.objects.push_back (
        make_block ( id++, {640.f, 700.f}, {world::kWidthPx, 40.f}, Material::Stone ) );
    snap.objects.push_back ( make_block ( id++, {800.f, 580.f}, {20.f, 100.f}, Material::Wood ) );
    snap.objects.push_back ( make_block ( id++, {900.f, 580.f}, {20.f, 100.f}, Material::Wood ) );
    snap.objects.push_back ( make_block ( id++, {850.f, 520.f}, {140.f, 20.f}, Material::Wood, 0.8f ) );
    snap.objects.push_back ( make_block ( id++, {850.f, 500.f}, {60.f, 20.f}, Material::Glass ) );

    ObjectSnapshot target{};
    target.id = id++;
    target.kind = ObjectSnapshot::Kind::Target;
    target.positionPx = {850.f, 560.f};
    target.radiusPx = 15.f;
    target.material = Material::Wood;
    target.hpNormalized = 1.f;
    target.isActive = true;
    snap.objects.push_back ( target );

    return snap;
}

GameScene::GameScene ( const sf::Font& font )
    : physics_ ( PhysicsMode::Threaded )
    , snapshot_ ( make_mock_snapshot() )
    , font_ ( font )
    , hud_text_ ( font_, "", 20 )
    , perf_text_ ( font_, "", 11 )
    , game_view_ (
          sf::FloatRect ( {0.f, 0.f}, {world::kWidthPx, world::kHeightPx} ) )
{
    hud_text_.setFillColor ( sf::Color::White );
    hud_text_.setPosition ( {20.f, 20.f} );
    perf_text_.setFillColor ( sf::Color ( 224, 240, 255, 170 ) );

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

        bloom_ready_ = bloom_extract_shader_.loadFromMemory (
                           kBloomExtractFragmentShader, sf::Shader::Type::Fragment )
                       && bloom_blur_shader_.loadFromMemory (
                           kBloomBlurFragmentShader, sf::Shader::Type::Fragment );

        if ( bloom_ready_ )
        {
            bloom_extract_shader_.setUniform ( "texture", sf::Shader::CurrentTexture );
            bloom_extract_shader_.setUniform ( "uThreshold", 0.58f );
            bloom_blur_shader_.setUniform ( "texture", sf::Shader::CurrentTexture );
        }
        else
        {
            Logger::error ( "GameScene: failed to load bloom shaders, bloom disabled" );
        }
    }
    else
    {
        Logger::info ( "GameScene: shaders are unavailable, using fallback rendering path" );
    }
}

void GameScene::notify_window_recreated()
{
    render_targets_dirty_ = true;
    world_pass_ = sf::RenderTexture();
    bloom_extract_pass_ = sf::RenderTexture();
    bloom_ping_pass_ = sf::RenderTexture();
    bloom_pong_pass_ = sf::RenderTexture();
}

void GameScene::rebuild_render_targets ( sf::Vector2u size )
{
    if ( size.x == 0 || size.y == 0 )
        return;

    world_pass_ = sf::RenderTexture();
    if ( !world_pass_.resize ( size ) )
    {
        Logger::error ( "GameScene: failed to resize world render target" );
        return;
    }
    world_pass_.setSmooth ( true );

    if ( bloom_ready_ )
    {
        bloom_extract_pass_ = sf::RenderTexture();
        bloom_ping_pass_ = sf::RenderTexture();
        bloom_pong_pass_ = sf::RenderTexture();

        const bool bloom_ok = bloom_extract_pass_.resize ( size )
                              && bloom_ping_pass_.resize ( size )
                              && bloom_pong_pass_.resize ( size );
        if ( !bloom_ok )
        {
            Logger::error ( "GameScene: failed to resize bloom render targets, bloom disabled" );
            bloom_ready_ = false;
        }
        else
        {
            bloom_extract_pass_.setSmooth ( true );
            bloom_ping_pass_.setSmooth ( true );
            bloom_pong_pass_.setSmooth ( true );
        }
    }

    render_targets_dirty_ = false;
}

void GameScene::load_level ( int level_id, const std::string& scores_path )
{
    level_id_ = level_id;
    scores_path_ = scores_path;
    pending_scene_ = SceneId::None;
    end_delay_ = 0.f;
    dropper_payload_ghosts_.clear();
    pending_events_.clear();
    smoothed_dt_sec_ = 1.0f / 60.0f;
    smoothed_fps_ = 60.0f;
    vfx_load_factor_ = 1.0f;
    render_targets_dirty_ = true;
    pending_result_token_ = 0;
    leaderboard_applied_ = true;

    try
    {
        const std::string path = resolveLevelPath ( level_id );
        const LevelData level = level_loader_.load ( path );
        physics_.registerLevel ( level );
        physics_.loadLevel ( level );
        if ( physics_.mode() == PhysicsMode::Threaded )
        {
            // Avoid blocking main thread while worker applies LoadLevelCmd.
            // Snapshot will be refreshed in update() on next ticks.
            snapshot_ = WorldSnapshot {};
            snapshot_.status = LevelStatus::Running;
        }
        else
        {
            snapshot_ = physics_.getSnapshot();
        }
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
    const int stars = std::clamp ( snapshot_.stars, 0, 3 );

    last_result_ = { won, score, stars, {} };
    leaderboard_applied_ = !won;
    pending_result_token_ = 0;

    if ( won && level_id_ > 0 )
    {
        if ( !scores_path_.empty() )
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

        const std::uint64_t token = ++leaderboard_request_token_;
        pending_result_token_ = token;
        const std::shared_ptr<LeaderboardAsyncState> async_state = leaderboard_async_state_;
        const std::string player_name = player_name_;
        const int level_id = level_id_;
        const int score_value = score;
        const int stars_value = stars;
        OnlineScoreClient client = online_score_client_;

        std::thread (
            [async_state, token, client = std::move ( client ),
             player_name = std::move ( player_name ),
             level_id, score_value, stars_value]() mutable
            {
                std::vector<LeaderboardEntry> leaderboard;

                try
                {
                    const bool submit_ok = client.submitScore (
                        player_name, level_id, score_value, stars_value );
                    if ( submit_ok )
                    {
                        leaderboard = client.fetchLeaderboard ( level_id );
                    }
                    else
                    {
                        Logger::info (
                            "GameScene: backend submit failed, keeping local result only" );
                    }
                }
                catch ( const std::exception& e )
                {
                    Logger::error ( "GameScene: failed to sync leaderboard: {}", e.what() );
                    leaderboard.clear();
                }

                std::lock_guard<std::mutex> lock ( async_state->mutex );
                if ( token >= async_state->ready_token )
                {
                    async_state->ready_token = token;
                    async_state->ready_entries = std::move ( leaderboard );
                    async_state->ready = true;
                }
            } ).detach();
    }

    pending_scene_ = SceneId::Result;
}

bool GameScene::poll_result_update()
{
    if ( leaderboard_applied_ || pending_result_token_ == 0 )
        return false;

    std::lock_guard<std::mutex> lock ( leaderboard_async_state_->mutex );
    if ( !leaderboard_async_state_->ready
         || leaderboard_async_state_->ready_token != pending_result_token_ )
    {
        return false;
    }

    if ( leaderboard_async_state_->ready_entries.empty()
         && last_result_.leaderboard.empty() )
    {
        leaderboard_applied_ = true;
        return false;
    }

    last_result_.leaderboard = leaderboard_async_state_->ready_entries;
    leaderboard_applied_ = true;
    return true;
}

void GameScene::process_events()
{
    auto fresh_events = physics_.drainEvents();
    for ( auto& ev : fresh_events )
    {
        pending_events_.push_back ( std::move ( ev ) );
    }

    if ( pending_events_.size() > kMaxQueuedEvents )
    {
        const std::size_t overflow = pending_events_.size() - kMaxQueuedEvents;
        pending_events_.erase ( pending_events_.begin(),
                                pending_events_.begin() + static_cast<std::ptrdiff_t> ( overflow ) );
    }

    const float quality = std::clamp ( vfx_load_factor_, 0.42f, 1.0f );
    const int events_budget = std::clamp (
        static_cast<int> ( std::round (
            static_cast<float> ( kMinEventsPerFrame )
            + ( static_cast<float> ( kMaxEventsPerFrame - kMinEventsPerFrame ) * quality ) ) ),
        kMinEventsPerFrame, kMaxEventsPerFrame );

    int collision_vfx_budget = std::max ( 3, static_cast<int> ( std::round ( 12.f * quality ) ) );
    int destroyed_vfx_budget = std::max ( 3, static_cast<int> ( std::round ( 8.f * quality ) ) );
    int ability_vfx_budget = std::max ( 2, static_cast<int> ( std::round ( 8.f * quality ) ) );

    int processed_events = 0;
    while ( processed_events < events_budget && !pending_events_.empty() )
    {
        Event ev = std::move ( pending_events_.front() );
        pending_events_.pop_front();
        ++processed_events;

        std::visit (
            [this, &collision_vfx_budget, &destroyed_vfx_budget, &ability_vfx_budget]
            ( const auto& e )
            {
                using T = std::decay_t<decltype ( e )>;

                if constexpr ( std::is_same_v<T, CollisionEvent> )
                {
                    if ( collision_vfx_budget <= 0 )
                        return;
                    --collision_vfx_budget;

                    const float impulse = std::clamp ( e.impulse, 0.f, 30.f );
                    if ( impulse < 2.2f )
                        return;

                    sf::Vector2f pos ( e.contactPointPx.x, e.contactPointPx.y );
                    const Material impactMaterial =
                        choose_impact_material ( snapshot_, e.aId, e.bId );
                    const MaterialVfxProfile& profile = vfx_profile ( impactMaterial );

                    const int count = std::clamp (
                        static_cast<int> ( impulse * profile.impulseToCount * 0.60f ),
                        std::max ( 2, profile.minHitCount / 2 ), profile.maxHitCount );
                    const float speed =
                        std::clamp ( impulse * profile.hitSpeedScale, 36.f, 240.f );

                    particles_.emit ( pos, count, profile.sparkColor, speed, 0.38f, 3.2f );
                    particles_.emit ( pos, std::max ( 1, count / 3 ),
                                      profile.dustColor, speed * 0.65f,
                                      0.52f, 4.8f );

                    if ( profile.ringOnHit )
                    {
                        particles_.emit_ring ( pos, 10, profile.sparkColor,
                                               std::clamp ( speed * 0.55f, 55.f, 130.f ),
                                               0.30f, 2.8f );
                    }

                    if ( impulse > 3.5f )
                    {
                        particles_.emit_shards (
                            pos, std::max ( 1, count / 3 ), profile.shardColor,
                            std::clamp ( profile.shardSpeed * ( impulse / 10.f ), 45.f, 220.f ),
                            0.48f, profile.shardSize, 320.f );
                    }

                    if ( impulse > 1.2f )
                    {
                        shake_time_ =
                            std::max ( shake_time_, std::clamp ( 0.06f + impulse * 0.012f,
                                                                 0.06f, 0.24f ) );
                        shake_strength_ =
                            std::max ( shake_strength_, std::clamp ( impulse * 0.55f, 2.f, 14.f ) );
                        impact_flash_ =
                            std::max ( impact_flash_, std::clamp (
                                impulse * profile.hitFlashBoost, 0.03f, 0.30f ) );
                    }

                    const float strongImpact = strong_impact_factor ( impulse );
                    if ( strongImpact > 0.f )
                    {
                        shake_time_ = std::max ( shake_time_, 0.069f + strongImpact * 0.014f );
                        shake_strength_ = std::max (
                            shake_strength_, std::clamp ( impulse * 0.45f, 3.f, 11.f ) );
                        impact_flash_ = std::max (
                            impact_flash_, std::clamp ( 0.05f + impulse * 0.008f, 0.05f, 0.22f ) );
                    }
                }
                else if constexpr ( std::is_same_v<T, DestroyedEvent> )
                {
                    if ( destroyed_vfx_budget <= 0 )
                    {
                        sfx_.play_destroyed ( e.material );
                        impact_flash_ = std::max ( impact_flash_, 0.06f );
                        return;
                    }
                    --destroyed_vfx_budget;

                    sf::Vector2f pos ( e.positionPx.x, e.positionPx.y );
                    const MaterialVfxProfile& profile = vfx_profile ( e.material );
                    sfx_.play_destroyed ( e.material );

                    int burstCount = profile.destroyBurstCount;
                    int dustCount = profile.destroyBurstCount / 2;
                    int shardCount = static_cast<int> (
                        std::round ( static_cast<float> ( profile.shardCount ) * 1.35f ) );
                    float shardSpeed = profile.shardSpeed;
                    float shardLifetime = 0.88f;
                    float shardSize = profile.shardSize * 1.35f;
                    float shardAngular = 420.f;
                    int ringCount = 14;
                    float ringSpeed = profile.shardSpeed * 0.62f;
                    float ringLifetime = 0.34f;
                    float ringSize = profile.shardSize * 0.92f;

                    switch ( e.material )
                    {
                    case Material::Wood:
                        shardCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.shardCount ) * 1.9f ) );
                        shardSpeed *= 1.04f;
                        shardLifetime = 0.96f;
                        shardSize *= 1.25f;
                        shardAngular = 280.f;
                        dustCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.destroyBurstCount ) * 0.70f ) );
                        break;
                    case Material::Stone:
                        shardCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.shardCount ) * 1.25f ) );
                        shardSpeed *= 0.80f;
                        shardLifetime = 1.02f;
                        shardSize *= 1.56f;
                        shardAngular = 240.f;
                        dustCount = profile.destroyBurstCount;
                        break;
                    case Material::Glass:
                        burstCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.destroyBurstCount ) * 1.15f ) );
                        dustCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.destroyBurstCount ) * 0.55f ) );
                        shardCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.shardCount ) * 2.2f ) );
                        shardSpeed *= 1.45f;
                        shardLifetime = 0.92f;
                        shardSize *= 0.92f;
                        shardAngular = 640.f;
                        ringCount = 18;
                        ringSpeed *= 1.10f;
                        ringLifetime = 0.30f;
                        ringSize *= 0.84f;
                        break;
                    case Material::Ice:
                        burstCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.destroyBurstCount ) * 0.95f ) );
                        dustCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.destroyBurstCount ) * 0.75f ) );
                        shardCount = static_cast<int> (
                            std::round ( static_cast<float> ( profile.shardCount ) * 1.8f ) );
                        shardSpeed *= 1.25f;
                        shardLifetime = 0.98f;
                        shardSize *= 1.05f;
                        shardAngular = 560.f;
                        ringCount = 16;
                        ringSpeed *= 0.92f;
                        ringLifetime = 0.36f;
                        ringSize *= 1.05f;
                        break;
                    default:
                        break;
                    }

                    burstCount = std::max ( 4, burstCount );
                    dustCount = std::max ( 2, dustCount );
                    shardCount = std::max ( 3, shardCount );

                    constexpr float kDestroyVfxScale = 0.76f;
                    burstCount = std::max ( 3, static_cast<int> (
                        std::round ( static_cast<float> ( burstCount ) * kDestroyVfxScale ) ) );
                    dustCount = std::max ( 2, static_cast<int> (
                        std::round ( static_cast<float> ( dustCount ) * kDestroyVfxScale ) ) );
                    shardCount = std::max ( 3, static_cast<int> (
                        std::round ( static_cast<float> ( shardCount ) * kDestroyVfxScale ) ) );
                    ringCount = std::max ( 8, static_cast<int> (
                        std::round ( static_cast<float> ( ringCount ) * kDestroyVfxScale ) ) );

                    particles_.emit ( pos, burstCount,
                                      profile.sparkColor, profile.shardSpeed,
                                      0.58f, profile.shardSize );
                    particles_.emit ( pos, dustCount,
                                      profile.dustColor, profile.shardSpeed * 0.55f,
                                      0.76f, profile.shardSize * 1.3f );
                    particles_.emit_shards ( pos, shardCount, profile.shardColor, shardSpeed,
                                             shardLifetime, shardSize, shardAngular );

                    if ( e.material == Material::Ice )
                    {
                        particles_.emit ( pos, 10, sf::Color ( 228, 246, 255, 175 ),
                                          profile.shardSpeed * 0.54f, 0.64f,
                                          profile.shardSize * 1.25f );
                    }

                    if ( profile.ringOnDestroy )
                    {
                        particles_.emit_ring ( pos, ringCount, profile.sparkColor,
                                               ringSpeed, ringLifetime, ringSize );
                    }

                    impact_flash_ =
                        std::max ( impact_flash_, profile.destroyFlashBoost );
                }
                else if constexpr ( std::is_same_v<T, AbilityActivatedEvent> )
                {
                    if ( ability_vfx_budget <= 0 )
                    {
                        sfx_.play_ability ( e.projectileType );
                        return;
                    }
                    --ability_vfx_budget;

                    const sf::Vector2f pos ( e.positionPx.x, e.positionPx.y );
                    const sf::Color core = ability_core_color ( e.projectileType );
                    const sf::Color glow = ability_glow_color ( e.projectileType );
                    const AbilityVfxProfile cfg = ability_vfx_profile ( e.projectileType );
                    sfx_.play_ability ( e.projectileType );

                    particles_.emit_ring ( pos, cfg.ringPrimaryCount, core,
                                           cfg.ringPrimarySpeed, cfg.ringPrimaryLifetime,
                                           cfg.ringPrimarySize );
                    if ( cfg.ringSecondaryCount > 0 )
                    {
                        particles_.emit_ring ( pos, cfg.ringSecondaryCount, glow,
                                               cfg.ringSecondarySpeed,
                                               cfg.ringSecondaryLifetime,
                                               cfg.ringSecondarySize );
                    }

                    const sf::Color burstColor = cfg.burstUsesGlow ? glow : core;
                    const sf::Color shardColor = cfg.burstUsesGlow ? core : glow;
                    particles_.emit ( pos, cfg.burstCount, burstColor, cfg.burstSpeed,
                                      cfg.burstLifetime, cfg.burstSize );
                    particles_.emit_shards ( pos, cfg.shardCount, shardColor,
                                             cfg.shardSpeed, cfg.shardLifetime,
                                             cfg.shardSize, cfg.shardAngularSpeed );

                    if ( e.projectileType == ProjectileType::Bomber )
                    {
                        particles_.emit_ring ( pos, 22, glow, 176.f, 0.34f, 6.2f );
                        particles_.emit ( pos, 16, sf::Color ( 255, 212, 166, 188 ),
                                          148.f, 0.56f, 5.2f );
                        particles_.emit ( pos, 14, sf::Color ( 255, 244, 202, 210 ),
                                          116.f, 0.20f, 4.1f );
                    }
                    else if ( e.projectileType == ProjectileType::Dropper )
                    {
                        particles_.emit_ring ( pos + sf::Vector2f ( 0.f, 14.f ), 12, glow,
                                               116.f, 0.26f, 3.6f );
                        particles_.emit ( pos + sf::Vector2f ( 0.f, 16.f ), 12, core,
                                          106.f, 0.42f, 3.9f );
                        particles_.emit_shards ( pos + sf::Vector2f ( 0.f, 22.f ),
                                                 14, sf::Color ( 202, 246, 224, 205 ),
                                                 146.f, 0.44f, 3.7f, 520.f );

                        DropperPayloadGhost ghost;
                        ghost.position = pos + sf::Vector2f ( 0.f, 20.f );
                        ghost.velocity = {0.f, 300.f};
                        ghost.lifetime = 0.62f;
                        ghost.radius = 9.f;
                        dropper_payload_ghosts_.push_back ( ghost );
                    }
                    if ( e.projectileType == ProjectileType::Boomerang )
                    {
                        particles_.emit_ring ( pos, 14, glow, 164.f, 0.24f, 3.9f );
                        particles_.emit_shards ( pos, 10, core, 188.f, 0.36f, 3.1f, 860.f );
                    }
                    else if ( e.projectileType == ProjectileType::Dasher )
                    {
                        particles_.emit_ring ( pos, 10, glow, 208.f, 0.18f, 3.0f );
                        particles_.emit ( pos, 12, core, 194.f, 0.22f, 3.4f );
                    }
                    else if ( e.projectileType == ProjectileType::Inflater )
                    {
                        // Expanding balloon ring — three concentric circles that grow outward
                        InflaterExpandRing r1; r1.position = pos; r1.maxRadius = 50.f;  r1.lifetime = 0.40f; inflater_rings_.push_back ( r1 );
                        InflaterExpandRing r2; r2.position = pos; r2.maxRadius = 85.f;  r2.lifetime = 0.55f; inflater_rings_.push_back ( r2 );
                        InflaterExpandRing r3; r3.position = pos; r3.maxRadius = 120.f; r3.lifetime = 0.70f; inflater_rings_.push_back ( r3 );
                    }
                    else if ( e.projectileType == ProjectileType::Bubbler )
                    {
                        // Spawn 8 floating soap bubbles that drift upward and pop
                        for ( int b = 0; b < 8; ++b )
                        {
                            BubbleFloat bf;
                            const float angle = static_cast<float> ( b ) * 2.f * 3.14159f / 8.f;
                            bf.position = pos + sf::Vector2f ( std::cos ( angle ) * 28.f,
                                                               std::sin ( angle ) * 18.f );
                            bf.radius   = 8.f + static_cast<float> ( b % 3 ) * 5.f;
                            bf.lifetime = 1.0f + static_cast<float> ( b % 4 ) * 0.25f;
                            bubble_floats_.push_back ( bf );
                        }

                        // Capture zone — used in render() to overlay bubbles on lifted objects
                        BubbleCaptureZone zone;
                        zone.center = pos;
                        bubble_capture_zones_.push_back ( zone );
                    }

                    shake_time_ = std::max ( shake_time_, cfg.shakeTime );
                    shake_strength_ = std::max ( shake_strength_, cfg.shakeStrength );
                    impact_flash_ = std::max ( impact_flash_, cfg.flashBoost );
                }
            },
            ev );
    }
}

SceneId GameScene::poll_pending_scene()
{
    if ( pending_scene_ != SceneId::None )
    {
        SceneId next  = pending_scene_;
        pending_scene_ = SceneId::None;
        return next;
    }
    return SceneId::None;
}

SceneId GameScene::handle_input ( const sf::Event& event )
{
    if ( event.getIf<sf::Event::Resized>() || event.getIf<sf::Event::FocusGained>() )
    {
        render_targets_dirty_ = true;
    }

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

        if ( key->code == sf::Keyboard::Key::F3 )
            show_perf_overlay_ = !show_perf_overlay_;
    }

    if ( snapshot_.status == LevelStatus::Running && window_ptr_ )
    {
        apply_letterbox_view ( game_view_, window_ptr_->getSize() );
        auto cmd = slingshot_.handle_input ( event, snapshot_.slingshot, *window_ptr_,
                                             game_view_ );
        if ( cmd.has_value() )
            command_queue_.push ( *cmd );
    }

    return SceneId::None;
}

void GameScene::update()
{
    const float raw_dt = frame_clock_.restart().asSeconds();
    const float dt = std::clamp ( raw_dt, 0.0f, 1.0f / 30.0f );

    const float perf_dt = std::clamp ( raw_dt, 1.0f / 240.0f, 0.25f );
    smoothed_dt_sec_ = smoothed_dt_sec_ * 0.90f + perf_dt * 0.10f;
    smoothed_fps_ = 1.0f / std::max ( smoothed_dt_sec_, 1.0f / 240.0f );

    const float fps_pressure = std::clamp ( ( 58.0f - smoothed_fps_ ) / 25.0f, 0.0f, 1.0f );
    const float particle_pressure = std::clamp (
        ( static_cast<float> ( particles_.size() ) - 720.0f ) / 760.0f, 0.0f, 1.0f );
    const float queue_pressure = std::clamp (
        ( static_cast<float> ( pending_events_.size() ) - 220.0f ) / 700.0f, 0.0f, 1.0f );
    const float pressure = std::max ( fps_pressure, std::max ( particle_pressure, queue_pressure ) );
    vfx_load_factor_ = 1.0f - 0.55f * pressure;

    auto update_dropper_payload_ghosts = [this, dt] ()
    {
        for ( auto& ghost : dropper_payload_ghosts_ )
        {
            ghost.age += dt;
            ghost.velocity.y += 720.f * dt;
            ghost.velocity.x *= std::max ( 0.f, 1.f - dt * 3.0f );
            ghost.position += ghost.velocity * dt;
        }

        dropper_payload_ghosts_.erase (
            std::remove_if ( dropper_payload_ghosts_.begin(), dropper_payload_ghosts_.end(),
                             [] ( const DropperPayloadGhost& ghost )
                             {
                                 return ghost.age >= ghost.lifetime
                                        || ghost.position.y > ( world::kHeightPx + 560.f );
                             } ),
            dropper_payload_ghosts_.end() );
    };

    auto update_inflater_rings = [this, dt] ()
    {
        for ( auto& r : inflater_rings_ )
            r.age += dt;
        inflater_rings_.erase (
            std::remove_if ( inflater_rings_.begin(), inflater_rings_.end(),
                             [] ( const InflaterExpandRing& r ) { return r.age >= r.lifetime; } ),
            inflater_rings_.end() );
    };

    auto update_bubble_floats = [this, dt] ()
    {
        for ( auto& b : bubble_floats_ )
        {
            b.age += dt;
            b.position.y -= ( 55.f + b.radius * 2.f ) * dt;  // float upward
            b.position.x += std::sin ( b.age * 3.5f + b.radius ) * 12.f * dt;  // sway
        }
        bubble_floats_.erase (
            std::remove_if ( bubble_floats_.begin(), bubble_floats_.end(),
                             [] ( const BubbleFloat& b ) { return b.age >= b.lifetime; } ),
            bubble_floats_.end() );

        for ( auto& z : bubble_capture_zones_ )
            z.age += dt;
        bubble_capture_zones_.erase (
            std::remove_if ( bubble_capture_zones_.begin(), bubble_capture_zones_.end(),
                             [] ( const BubbleCaptureZone& z ) { return z.age >= z.lifetime; } ),
            bubble_capture_zones_.end() );
    };

    if ( snapshot_.status != LevelStatus::Running )
    {
        particles_.update ( dt );
        update_dropper_payload_ghosts();
        update_inflater_rings();
        update_bubble_floats();
        end_delay_ += dt;
        if ( end_delay_ >= 1.5f && pending_scene_ == SceneId::None )
            finish_level();
        return;
    }

    particles_.update ( dt );

    physics_.processCommands ( command_queue_ );
    physics_.step ( dt );
    snapshot_ = physics_.getSnapshot();
    process_events();

    const bool low_vfx = vfx_load_factor_ < 0.72f;
    const int trail_emit_count = low_vfx ? 1 : 2;

    for ( const auto& obj : snapshot_.objects )
    {
        if ( obj.isActive && obj.kind == ObjectSnapshot::Kind::Projectile )
        {
            particles_.emit ( {obj.positionPx.x, obj.positionPx.y}, trail_emit_count,
                              projectile_trail_color ( obj.projectileType ),
                              38.f, 0.20f, 3.5f );

            if ( obj.projectileType == ProjectileType::Heavy && obj.radiusPx > 0.f )
            {
                // Massive shockwave aura — heavy plows through air leaving a dense purple wake
                static sf::Clock heavy_idle_clock;
                const float t     = heavy_idle_clock.getElapsedTime().asSeconds();
                const float pulse = 0.5f + 0.5f * std::sin ( t * 8.f );
                const sf::Vector2f pos { obj.positionPx.x, obj.positionPx.y };
                particles_.emit ( pos, low_vfx ? 2 : 3,
                                  sf::Color ( 148, 90, 220,
                                              static_cast<uint8_t> ( 140.f + pulse * 50.f ) ),
                                  60.f + pulse * 30.f, 0.28f, 5.2f );
                particles_.emit ( pos, low_vfx ? 1 : 2,
                                  sf::Color ( 200, 160, 255,
                                              static_cast<uint8_t> ( 90.f + pulse * 40.f ) ),
                                  40.f, 0.18f, 3.8f );
            }
            else if ( obj.projectileType == ProjectileType::Bomber && obj.radiusPx > 0.f )
            {
                static sf::Clock bomber_idle_clock;
                const float t = bomber_idle_clock.getElapsedTime().asSeconds();
                const float pulse = 0.5f + 0.5f * std::sin ( t * 14.f + obj.positionPx.x * 0.01f );
                const float fuse_angle = ( obj.angleDeg - 52.f ) * kPi / 180.f;
                const sf::Vector2f fuse_tip {
                    obj.positionPx.x + std::cos ( fuse_angle ) * obj.radiusPx * 0.88f,
                    obj.positionPx.y + std::sin ( fuse_angle ) * obj.radiusPx * 0.88f
                };

                particles_.emit ( fuse_tip, ( pulse > 0.72f && !low_vfx ) ? 2 : 1,
                                  sf::Color ( 255, 202, 132, 198 ),
                                  52.f + pulse * 26.f, 0.16f, 2.8f );
                particles_.emit ( fuse_tip, 1, sf::Color ( 255, 132, 82, 172 ),
                                  36.f + pulse * 14.f, 0.12f, 2.1f );
            }
            else if ( obj.projectileType == ProjectileType::Dropper && obj.radiusPx > 0.f )
            {
                static sf::Clock dropper_idle_clock;
                const float t = dropper_idle_clock.getElapsedTime().asSeconds();
                const float pulse = 0.5f + 0.5f * std::sin ( t * 11.f + obj.positionPx.y * 0.012f );
                const float pod_angle = ( obj.angleDeg + 90.f ) * kPi / 180.f;
                const sf::Vector2f payload_port {
                    obj.positionPx.x + std::cos ( pod_angle ) * obj.radiusPx * 0.72f,
                    obj.positionPx.y + std::sin ( pod_angle ) * obj.radiusPx * 0.72f
                };

                particles_.emit ( payload_port, 1, sf::Color ( 190, 248, 228, 188 ),
                                  48.f + pulse * 20.f, 0.14f, 2.5f );
                particles_.emit ( payload_port + sf::Vector2f ( 0.f, 5.f ), 1,
                                  sf::Color ( 98, 206, 170, 160 ),
                                  30.f + pulse * 12.f, 0.12f, 2.0f );
            }
            break;
        }
    }

    update_dropper_payload_ghosts();
    update_inflater_rings();
    update_bubble_floats();

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
    const sf::Vector2u window_size = window.getSize();
    if ( window_size.x == 0 || window_size.y == 0 )
        return;

    window_ptr_ = &window;
    apply_letterbox_view ( game_view_, window_size );

    if ( render_targets_dirty_ || world_pass_.getSize() != window_size )
    {
        rebuild_render_targets ( window_size );
    }

    if ( world_pass_.getSize().x == 0 || world_pass_.getSize().y == 0 )
    {
        return;
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
    slingshot_.render ( world_pass_, snapshot_.slingshot,
                        renderer_.projectile_texture ( snapshot_.slingshot.nextProjectile ) );

    for ( const auto& ghost : dropper_payload_ghosts_ )
    {
        const float life_t = std::clamp ( 1.f - ghost.age / ghost.lifetime, 0.f, 1.f );
        const float radius = ghost.radius * ( 0.92f + 0.16f * life_t );
        const sf::Vector2f pos = ghost.position;

        sf::CircleShape glow ( radius * 1.6f );
        glow.setOrigin ( {radius * 1.6f, radius * 1.6f} );
        glow.setPosition ( pos );
        glow.setFillColor ( sf::Color ( 118, 222, 184,
                                        static_cast<uint8_t> ( 52.f * life_t ) ) );
        world_pass_.draw ( glow );

        sf::CircleShape shell ( radius );
        shell.setOrigin ( {radius, radius} );
        shell.setPosition ( pos );
        shell.setFillColor ( sf::Color ( 96, 204, 166,
                                         static_cast<uint8_t> ( 170.f * life_t ) ) );
        shell.setOutlineThickness ( std::max ( 1.2f, radius * 0.2f ) );
        shell.setOutlineColor ( sf::Color ( 198, 252, 232,
                                            static_cast<uint8_t> ( 196.f * life_t ) ) );
        world_pass_.draw ( shell );

        sf::RectangleShape belt ( {radius * 1.25f, radius * 0.34f} );
        belt.setOrigin ( {radius * 0.625f, radius * 0.17f} );
        belt.setPosition ( pos );
        belt.setFillColor ( sf::Color ( 36, 118, 94,
                                        static_cast<uint8_t> ( 178.f * life_t ) ) );
        world_pass_.draw ( belt );

        sf::CircleShape core ( radius * 0.30f );
        core.setOrigin ( {radius * 0.30f, radius * 0.30f} );
        core.setPosition ( pos );
        core.setFillColor ( sf::Color ( 236, 255, 246,
                                        static_cast<uint8_t> ( 220.f * life_t ) ) );
        world_pass_.draw ( core );
    }

    // Inflater expanding rings
    for ( const auto& ring : inflater_rings_ )
    {
        const float t = std::clamp ( ring.age / ring.lifetime, 0.f, 1.f );
        const float ease = 1.f - ( 1.f - t ) * ( 1.f - t );  // ease-out quad
        const float r    = ring.maxRadius * ease;
        const float a    = static_cast<float> ( std::max ( 0.f, 1.f - t * 1.4f ) );

        sf::CircleShape ring_shape ( r );
        ring_shape.setOrigin ( {r, r} );
        ring_shape.setPosition ( ring.position );
        ring_shape.setFillColor ( sf::Color::Transparent );
        ring_shape.setOutlineThickness ( std::max ( 1.f, 4.f * ( 1.f - t ) ) );
        ring_shape.setOutlineColor ( sf::Color ( 255, 210, 230,
                                                  static_cast<uint8_t> ( 220.f * a ) ) );
        world_pass_.draw ( ring_shape );
    }

    // Bubbler floating soap bubbles
    for ( const auto& bub : bubble_floats_ )
    {
        const float life_t = std::clamp ( 1.f - bub.age / bub.lifetime, 0.f, 1.f );
        const float r      = bub.radius;

        // Outer glow
        sf::CircleShape glow_b ( r * 1.5f );
        glow_b.setOrigin ( {r * 1.5f, r * 1.5f} );
        glow_b.setPosition ( bub.position );
        glow_b.setFillColor ( sf::Color ( 180, 240, 255,
                                           static_cast<uint8_t> ( 35.f * life_t ) ) );
        world_pass_.draw ( glow_b );

        // Bubble shell
        sf::CircleShape shell_b ( r );
        shell_b.setOrigin ( {r, r} );
        shell_b.setPosition ( bub.position );
        shell_b.setFillColor ( sf::Color ( 210, 248, 255,
                                            static_cast<uint8_t> ( 28.f * life_t ) ) );
        shell_b.setOutlineThickness ( 1.5f );
        shell_b.setOutlineColor ( sf::Color ( 192, 238, 255,
                                               static_cast<uint8_t> ( 200.f * life_t ) ) );
        world_pass_.draw ( shell_b );

        // Rainbow highlight (small white circle at top-left)
        const float hr = r * 0.30f;
        sf::CircleShape highlight ( hr );
        highlight.setOrigin ( {hr, hr} );
        highlight.setPosition ( bub.position + sf::Vector2f ( -r * 0.35f, -r * 0.40f ) );
        highlight.setFillColor ( sf::Color ( 255, 255, 255,
                                              static_cast<uint8_t> ( 110.f * life_t ) ) );
        world_pass_.draw ( highlight );
    }

    // Bubbler: draw bubble overlay around objects caught in capture zone
    if ( !bubble_capture_zones_.empty() )
    {
        const std::size_t zone_limit = vfx_load_factor_ < 0.60f
                                           ? std::min<std::size_t> ( bubble_capture_zones_.size(), 1u )
                                           : bubble_capture_zones_.size();
        const std::size_t bubble_stride = vfx_load_factor_ < 0.68f ? 2u : 1u;

        for ( std::size_t zi = 0; zi < zone_limit; ++zi )
        {
            const auto& zone = bubble_capture_zones_[zi];
            const float life_t  = std::clamp ( 1.f - zone.age / zone.lifetime, 0.f, 1.f );
            const float pop_t   = zone.age / zone.lifetime;
            // Pulsing shimmer speed increases as bubbles near popping
            const float shimmer = 0.5f + 0.5f * std::sin ( zone.age * 6.f + pop_t * 4.f );
            const float cap_r   = zone.captureRadius;

            for ( std::size_t oi = 0; oi < snapshot_.objects.size(); oi += bubble_stride )
            {
                const auto& obj = snapshot_.objects[oi];
                if ( !obj.isActive )
                    continue;
                // Only non-static blocks/targets can be lifted
                if ( obj.isStatic )
                    continue;
                if ( obj.kind != ObjectSnapshot::Kind::Block
                     && obj.kind != ObjectSnapshot::Kind::Target )
                    continue;

                const sf::Vector2f opos ( obj.positionPx.x, obj.positionPx.y );
                const float dx = opos.x - zone.center.x;
                const float dy = opos.y - zone.center.y;
                if ( dx * dx + dy * dy > cap_r * cap_r )
                    continue;

                // Compute bubble radius to wrap the object
                const float obj_half = obj.radiusPx > 0.f
                    ? obj.radiusPx
                    : std::max ( obj.sizePx.x, obj.sizePx.y ) * 0.5f;
                const float br = obj_half * 1.35f + 4.f;

                // Outer glow
                sf::CircleShape glow_o ( br * 1.4f );
                glow_o.setOrigin ( {br * 1.4f, br * 1.4f} );
                glow_o.setPosition ( opos );
                glow_o.setFillColor ( sf::Color ( 160, 228, 255,
                    static_cast<uint8_t> ( 30.f * life_t * ( 0.7f + 0.3f * shimmer ) ) ) );
                world_pass_.draw ( glow_o );

                // Bubble shell
                sf::CircleShape shell_o ( br );
                shell_o.setOrigin ( {br, br} );
                shell_o.setPosition ( opos );
                shell_o.setFillColor ( sf::Color ( 200, 242, 255,
                    static_cast<uint8_t> ( 22.f * life_t ) ) );
                shell_o.setOutlineThickness ( 1.8f );
                shell_o.setOutlineColor ( sf::Color ( 180, 235, 255,
                    static_cast<uint8_t> ( 180.f * life_t * ( 0.6f + 0.4f * shimmer ) ) ) );
                world_pass_.draw ( shell_o );

                // Small highlight dot (top-left of bubble)
                const float hr = br * 0.22f;
                sf::CircleShape hl ( hr );
                hl.setOrigin ( {hr, hr} );
                hl.setPosition ( opos + sf::Vector2f ( -br * 0.38f, -br * 0.44f ) );
                hl.setFillColor ( sf::Color ( 255, 255, 255,
                    static_cast<uint8_t> ( 120.f * life_t ) ) );
                world_pass_.draw ( hl );
            }
        }
    }

    particles_.render ( world_pass_ );
    world_pass_.display();

    const std::size_t bloom_particle_limit = static_cast<std::size_t> (
        std::round ( 620.0f + vfx_load_factor_ * 420.0f ) );
    const bool bloom_this_frame = bloom_ready_ && particles_.size() < bloom_particle_limit;
    if ( bloom_this_frame )
    {
        const sf::Vector2u bloom_size = bloom_extract_pass_.getSize();
        const float texel_x = 1.f / static_cast<float> ( std::max ( 1u, bloom_size.x ) );
        const float texel_y = 1.f / static_cast<float> ( std::max ( 1u, bloom_size.y ) );

        bloom_extract_pass_.clear ( sf::Color::Transparent );
        sf::Sprite extract_source ( world_pass_.getTexture() );
        sf::RenderStates extract_states;
        extract_states.shader = &bloom_extract_shader_;
        bloom_extract_pass_.draw ( extract_source, extract_states );
        bloom_extract_pass_.display();

        bloom_blur_shader_.setUniform ( "uTexel", sf::Glsl::Vec2 ( texel_x, texel_y ) );

        bloom_ping_pass_.clear ( sf::Color::Transparent );
        sf::Sprite blur_h_source ( bloom_extract_pass_.getTexture() );
        bloom_blur_shader_.setUniform ( "uDirection", sf::Glsl::Vec2 ( 1.f, 0.f ) );
        sf::RenderStates blur_states;
        blur_states.shader = &bloom_blur_shader_;
        bloom_ping_pass_.draw ( blur_h_source, blur_states );
        bloom_ping_pass_.display();

        bloom_pong_pass_.clear ( sf::Color::Transparent );
        sf::Sprite blur_v_source ( bloom_ping_pass_.getTexture() );
        bloom_blur_shader_.setUniform ( "uDirection", sf::Glsl::Vec2 ( 0.f, 1.f ) );
        bloom_pong_pass_.draw ( blur_v_source, blur_states );
        bloom_pong_pass_.display();
    }

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

    if ( bloom_this_frame )
    {
        sf::Sprite bloom_sprite ( bloom_pong_pass_.getTexture() );
        const float boosted_flash = std::clamp ( impact_flash_ * 2.0f, 0.f, 0.5f );
        const uint8_t bloom_alpha =
            static_cast<uint8_t> ( 132.f + boosted_flash * 180.f );
        bloom_sprite.setColor ( sf::Color ( 255, 240, 214, bloom_alpha ) );

        sf::RenderStates bloom_states;
        bloom_states.blendMode = sf::BlendAdd;
        window.draw ( bloom_sprite, bloom_states );
    }

    // HUD in screen coordinates
    renderer_.draw_hud ( window, snapshot_, hud_text_ );

    if ( show_perf_overlay_ )
    {
        perf_text_.setString (
            std::to_string ( static_cast<int> ( std::round ( smoothed_fps_ ) ) )
            + " fps" );
        const auto bounds = perf_text_.getLocalBounds();
        perf_text_.setPosition ( {
            static_cast<float> ( window_size.x ) - bounds.size.x - 8.0f,
            6.0f
        } );
        window.draw ( perf_text_ );
    }
}

}  // namespace angry
