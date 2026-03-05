#include "ui/game_scene.hpp"

#include "data/logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
constexpr float kStrongImpactThreshold = 8.0f;
constexpr float kStrongImpactMax = 22.0f;

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
    int collision_vfx_budget = 18;
    for ( const auto& ev : events )
    {
        std::visit (
            [this, &collision_vfx_budget] ( const auto& e )
            {
                using T = std::decay_t<decltype ( e )>;

                if constexpr ( std::is_same_v<T, CollisionEvent> )
                {
                    if ( collision_vfx_budget <= 0 )
                        return;
                    --collision_vfx_budget;

                    const float impulse = std::clamp ( e.impulse, 0.f, 30.f );
                    if ( impulse < 1.8f )
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
                    }
                    else if ( e.projectileType == ProjectileType::Dropper )
                    {
                        particles_.emit_ring ( pos + sf::Vector2f ( 0.f, 14.f ), 12, glow,
                                               116.f, 0.26f, 3.6f );
                        particles_.emit ( pos + sf::Vector2f ( 0.f, 16.f ), 12, core,
                                          106.f, 0.42f, 3.9f );
                    }
                    else if ( e.projectileType == ProjectileType::Boomerang )
                    {
                        particles_.emit_ring ( pos, 14, glow, 164.f, 0.24f, 3.9f );
                        particles_.emit_shards ( pos, 10, core, 188.f, 0.36f, 3.1f, 860.f );
                    }
                    else if ( e.projectileType == ProjectileType::Dasher )
                    {
                        particles_.emit_ring ( pos, 10, glow, 208.f, 0.18f, 3.0f );
                        particles_.emit ( pos, 12, core, 194.f, 0.22f, 3.4f );
                    }

                    shake_time_ = std::max ( shake_time_, cfg.shakeTime );
                    shake_strength_ = std::max ( shake_strength_, cfg.shakeStrength );
                    impact_flash_ = std::max ( impact_flash_, cfg.flashBoost );
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

    if ( snapshot_.status != LevelStatus::Running )
    {
        particles_.update ( dt );
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

        if ( bloom_ready_ )
        {
            const bool bloom_ok = bloom_extract_pass_.resize ( window.getSize() )
                                  && bloom_ping_pass_.resize ( window.getSize() )
                                  && bloom_pong_pass_.resize ( window.getSize() );
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

    if ( bloom_ready_ )
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

    if ( bloom_ready_ )
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
}

}  // namespace angry
