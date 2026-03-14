// ============================================================
// physics_engine_tests.cpp — PhysicsEngine behavior tests.
// Part of: angry::tests
//
// Verifies core simulation contract invariants:
//   * Win/lose status and star computation rules
//   * Score assignment and event emission behavior
//   * Projectile/block impact and carry-through behavior
//   * Triangle snapshot and ability-activation coverage
// ============================================================

#include "physics/physics_engine.hpp"
#include "shared/command.hpp"
#include "shared/event.hpp"
#include "shared/level_data.hpp"
#include "shared/thread_safe_queue.hpp"
#include "shared/types.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace
{

// #=# Test Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

using angry::AbilityActivatedEvent;
using angry::ActivateAbilityCmd;
using angry::BlockData;
using angry::Command;
using angry::LaunchCmd;
using angry::LevelData;
using angry::LevelMeta;
using angry::LevelStatus;
using angry::Material;
using angry::ObjectSnapshot;
using angry::PhysicsEngine;
using angry::ProjectileData;
using angry::ProjectileType;
using angry::TargetData;
using angry::ThreadSafeQueue;
using angry::Vec2;

LevelData make_level(
    int id,
    std::vector<ProjectileType> projectiles,
    std::vector<BlockData> blocks,
    std::vector<TargetData> targets,
    int star2Threshold,
    int star3Threshold )
{
    LevelData level;
    level.meta = LevelMeta{
        id,
        "physics_test_" + std::to_string( id ),
        static_cast<int>( projectiles.size() ),
        0,
        star2Threshold,
        star3Threshold,
    };
    level.slingshot = angry::SlingshotData{
        Vec2{300.0f, 500.0f},
        140.0f,
    };
    level.blocks = std::move( blocks );
    level.targets = std::move( targets );
    for ( ProjectileType type : projectiles )
    {
        level.projectiles.push_back( ProjectileData{type} );
    }
    return level;
}

void run_commands_and_step( PhysicsEngine& engine, const std::vector<Command>& commands )
{
    ThreadSafeQueue<Command> queue;
    for ( const Command& cmd : commands )
    {
        queue.push( cmd );
    }

    engine.process_commands( queue );
    engine.step( 1.0f / 60.0f );
}

int count_alive_targets( const angry::WorldSnapshot& snapshot )
{
    int alive = 0;
    for ( const auto& object : snapshot.objects )
    {
        if ( object.kind == ObjectSnapshot::Kind::Target && object.isActive )
        {
            ++alive;
        }
    }
    return alive;
}

bool has_ability_event_for(
    const std::vector<angry::Event>& events,
    ProjectileType type )
{
    return std::any_of(
        events.begin(),
        events.end(),
        [type]( const angry::Event& e )
        {
            const auto* ability = std::get_if<AbilityActivatedEvent>( &e );
            return ability != nullptr && ability->projectileType == type;
        } );
}

const ObjectSnapshot* find_first_active_projectile( const angry::WorldSnapshot& snapshot )
{
    auto it = std::find_if(
        snapshot.objects.begin(),
        snapshot.objects.end(),
        []( const ObjectSnapshot& object )
        {
            return object.kind == ObjectSnapshot::Kind::Projectile && object.isActive;
        } );
    if ( it == snapshot.objects.end() )
    {
        return nullptr;
    }
    return &(*it);
}

}  // namespace

// #=# Test Cases #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

TEST( PhysicsEngineStatus, WinOnlyWhenNoAliveTargets )
{
    PhysicsEngine engine;

    const LevelData noTargetsLevel = make_level(
        101,
        {},
        {},
        {},
        200,
        300 );
    engine.load_level( noTargetsLevel );
    run_commands_and_step( engine, {} );
    auto snapshot = engine.get_snapshot();
    EXPECT_EQ( count_alive_targets( snapshot ), 0 );
    EXPECT_EQ( snapshot.status, LevelStatus::Win );

    const BlockData woodBlock{
        Vec2{300.0f, 440.0f},
        Vec2{30.0f, 30.0f},
        0.0f,
        angry::BlockShape::Rect,
        false,
        false,
        0.0f,
        Material::Wood,
        1.0f };
    const TargetData aliveTarget{
        Vec2{1000.0f, 450.0f},
        12.0f,
        999.0f,
        10 };
    const LevelData targetAliveLevel = make_level(
        102,
        {ProjectileType::Bomber},
        {woodBlock},
        {aliveTarget},
        50,
        100 );

    engine.load_level( targetAliveLevel );
    run_commands_and_step(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::kInvalidId},
        } );

    snapshot = engine.get_snapshot();
    EXPECT_GT( snapshot.score, 0 );  // score threshold may be reached via block destruction
    EXPECT_GT( count_alive_targets( snapshot ), 0 );
    EXPECT_EQ( snapshot.status, LevelStatus::Lose );
}

TEST( PhysicsEngineStars, StarsComputedFromScoreThresholds )
{
    PhysicsEngine engine;

    const TargetData target{
        Vec2{300.0f, 440.0f},
        12.0f,
        1.0f,
        100 };
    const BlockData woodBlock{
        Vec2{320.0f, 440.0f},
        Vec2{30.0f, 30.0f},
        0.0f,
        angry::BlockShape::Rect,
        false,
        false,
        0.0f,
        Material::Wood,
        1.0f };

    const LevelData twoStarsLevel = make_level(
        201,
        {ProjectileType::Bomber},
        {woodBlock},
        {target},
        150,
        200 );
    engine.load_level( twoStarsLevel );
    run_commands_and_step(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::kInvalidId},
        } );
    auto snapshot = engine.get_snapshot();
    EXPECT_EQ( snapshot.status, LevelStatus::Win );
    EXPECT_EQ( snapshot.score, 150 );
    EXPECT_EQ( snapshot.stars, 2 );

    const BlockData stoneBlock{
        Vec2{320.0f, 440.0f},
        Vec2{30.0f, 30.0f},
        0.0f,
        angry::BlockShape::Rect,
        false,
        false,
        0.0f,
        Material::Stone,
        1.0f };
    const LevelData threeStarsLevel = make_level(
        202,
        {ProjectileType::Bomber},
        {stoneBlock},
        {target},
        150,
        200 );
    engine.load_level( threeStarsLevel );
    run_commands_and_step(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::kInvalidId},
        } );
    snapshot = engine.get_snapshot();
    EXPECT_EQ( snapshot.status, LevelStatus::Win );
    EXPECT_EQ( snapshot.score, 200 );
    EXPECT_EQ( snapshot.stars, 3 );
}

TEST( PhysicsEngineScore, BlockScoreByMaterial )
{
    struct Case
    {
        Material material;
        int expectedScore;
    };

    const std::vector<Case> cases = {
        {Material::Wood, 50},
        {Material::Stone, 100},
        {Material::Glass, 20},
        {Material::Ice, 30},
    };

    PhysicsEngine engine;
    int levelId = 300;

    for ( const Case& c : cases )
    {
        const BlockData block{
            Vec2{300.0f, 440.0f},
            Vec2{30.0f, 30.0f},
            0.0f,
            angry::BlockShape::Rect,
            false,
            false,
            0.0f,
            c.material,
            1.0f };
        const LevelData level = make_level(
            levelId++,
            {ProjectileType::Bomber},
            {block},
            {},
            100,
            200 );

        engine.load_level( level );
        run_commands_and_step(
            engine,
            {
                LaunchCmd{Vec2{0.0f, 0.0f}},
                ActivateAbilityCmd{angry::kInvalidId},
            } );

        const auto snapshot = engine.get_snapshot();
        EXPECT_EQ( snapshot.score, c.expectedScore );
    }
}

TEST( PhysicsEngineTriangles, SnapshotKeepsAsymmetricTriangleVertices )
{
    PhysicsEngine engine;

    BlockData triangle{
        Vec2{520.0f, 440.0f},
        Vec2{70.0f, 70.0f},
        0.0f,
        angry::BlockShape::Triangle,
        false,
        false,
        18.0f,
        Material::Stone,
        60.0f };
    triangle.triangleLocalVerticesPx = {
        Vec2{-23.0f, -35.0f},
        Vec2{47.0f, 35.0f},
        Vec2{-23.0f, 35.0f},
    };

    const TargetData farTarget{
        Vec2{1100.0f, 450.0f},
        12.0f,
        999.0f,
        100 };

    const LevelData level = make_level(
        500,
        {ProjectileType::Standard},
        {triangle},
        {farTarget},
        200,
        300 );

    engine.load_level( level );

    // Step a few frames to ensure triangle body is stable in simulation.
    for ( int i = 0; i < 10; ++i )
    {
        run_commands_and_step( engine, {} );
    }

    const auto snapshot = engine.get_snapshot();
    auto blockIt = std::find_if(
        snapshot.objects.begin(),
        snapshot.objects.end(),
        []( const ObjectSnapshot& object )
        {
            return object.kind == ObjectSnapshot::Kind::Block;
        } );

    ASSERT_NE( blockIt, snapshot.objects.end() );
    EXPECT_EQ( blockIt->shape, angry::BlockShape::Triangle );
    ASSERT_EQ( blockIt->triangleLocalVerticesPx.size(), 3u );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[0].x, -23.0f );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[0].y, -35.0f );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[1].x, 47.0f );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[1].y, 35.0f );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[2].x, -23.0f );
    EXPECT_FLOAT_EQ( blockIt->triangleLocalVerticesPx[2].y, 35.0f );
}

TEST( PhysicsEngineEvents, AbilityActivatedEventForAllAbilityProjectiles )
{
    const std::vector<ProjectileType> abilityProjectiles = {
        ProjectileType::Dasher,
        ProjectileType::Splitter,
        ProjectileType::Bomber,
        ProjectileType::Dropper,
        ProjectileType::Boomerang,
        ProjectileType::Bubbler,
        ProjectileType::Inflater,
    };

    PhysicsEngine engine;
    int levelId = 400;

    for ( ProjectileType projectileType : abilityProjectiles )
    {
        const TargetData farTarget{
            Vec2{1100.0f, 450.0f},
            12.0f,
            999.0f,
            100 };
        const LevelData level = make_level(
            levelId++,
            {projectileType},
            {},
            {farTarget},
            200,
            300 );

        engine.load_level( level );
        run_commands_and_step(
            engine,
            {
                LaunchCmd{Vec2{-20.0f, -20.0f}},
                ActivateAbilityCmd{angry::kInvalidId},
            } );

        const std::vector<angry::Event> events = engine.drain_events();
        EXPECT_TRUE( has_ability_event_for( events, projectileType ) )
            << "missing AbilityActivatedEvent for projectileType="
            << static_cast<int>( projectileType );
    }
}

TEST( PhysicsEngineImpact, ProjectileBreaksBlockKeepsMotion )
{
    PhysicsEngine engine;

    const float blockX = 520.0f;
    const BlockData fragileWood{
        Vec2{blockX, 480.0f},
        Vec2{60.0f, 120.0f},
        0.0f,
        angry::BlockShape::Rect,
        false,
        false,
        0.0f,
        Material::Wood,
        1.0f };
    const TargetData farTarget{
        Vec2{1100.0f, 450.0f},
        12.0f,
        999.0f,
        100 };
    const LevelData level = make_level(
        601,
        {ProjectileType::Standard},
        {fragileWood},
        {farTarget},
        300,
        500 );

    engine.load_level( level );
    run_commands_and_step(
        engine,
        { LaunchCmd{Vec2{140.0f, 0.0f}} } );

    bool blockDestroyed = false;
    bool seenProjectileAfterBreak = false;
    bool hasPrevAfterBreak = false;
    Vec2 prevAfterBreakPos{};
    float traveledAfterBreak = 0.0f;
    for ( int i = 0; i < 120; ++i )
    {
        run_commands_and_step( engine, {} );
        const auto snapshot = engine.get_snapshot();
        if ( snapshot.score >= 50 )
        {
            blockDestroyed = true;
            if ( const ObjectSnapshot* projectile = find_first_active_projectile( snapshot ) )
            {
                seenProjectileAfterBreak = true;
                if ( hasPrevAfterBreak )
                {
                    const float dx = projectile->positionPx.x - prevAfterBreakPos.x;
                    const float dy = projectile->positionPx.y - prevAfterBreakPos.y;
                    traveledAfterBreak += std::sqrt( dx * dx + dy * dy );
                }
                prevAfterBreakPos = projectile->positionPx;
                hasPrevAfterBreak = true;
            }
        }
    }

    EXPECT_TRUE( blockDestroyed );
    EXPECT_TRUE( seenProjectileAfterBreak );
    EXPECT_GT( traveledAfterBreak, 25.0f );
}

TEST( PhysicsEngineImpact, ProjectileHitsIndestructibleStops )
{
    PhysicsEngine engine;

    const float blockX = 520.0f;
    const BlockData indestructible{
        Vec2{blockX, 500.0f},
        Vec2{28.0f, 240.0f},
        0.0f,
        angry::BlockShape::Rect,
        true,
        true,
        0.0f,
        Material::Stone,
        999.0f };
    const TargetData farTarget{
        Vec2{1100.0f, 450.0f},
        12.0f,
        999.0f,
        100 };
    const LevelData level = make_level(
        602,
        {ProjectileType::Standard},
        {indestructible},
        {farTarget},
        300,
        500 );

    engine.load_level( level );
    run_commands_and_step(
        engine,
        { LaunchCmd{Vec2{140.0f, 0.0f}} } );

    float maxProjectileX = -1.0f;
    for ( int i = 0; i < 120; ++i )
    {
        run_commands_and_step( engine, {} );
        const auto snapshot = engine.get_snapshot();
        if ( const ObjectSnapshot* projectile = find_first_active_projectile( snapshot ) )
        {
            maxProjectileX = std::max( maxProjectileX, projectile->positionPx.x );
        }
    }

    EXPECT_EQ( engine.get_snapshot().score, 0 );
    EXPECT_GT( maxProjectileX, 0.0f );
    EXPECT_LT( maxProjectileX, blockX + 25.0f );
}

TEST( PhysicsEngineImpact, NoEnergyExplosion )
{
    PhysicsEngine engine;

    std::vector<BlockData> fragileBlocks;
    for ( int i = 0; i < 4; ++i )
    {
        fragileBlocks.push_back( BlockData{
            Vec2{500.0f + static_cast<float>( i ) * 70.0f, 440.0f},
            Vec2{30.0f, 30.0f},
            0.0f,
            angry::BlockShape::Rect,
            false,
            false,
            0.0f,
            Material::Wood,
            1.0f } );
    }

    const TargetData farTarget{
        Vec2{1180.0f, 450.0f},
        12.0f,
        999.0f,
        100 };
    const LevelData level = make_level(
        603,
        {ProjectileType::Heavy},
        fragileBlocks,
        {farTarget},
        400,
        600 );

    engine.load_level( level );
    run_commands_and_step(
        engine,
        { LaunchCmd{Vec2{150.0f, 0.0f}} } );

    bool hasPrevPos = false;
    Vec2 prevPos{};
    float maxEstimatedSpeedMps = 0.0f;

    for ( int i = 0; i < 180; ++i )
    {
        run_commands_and_step( engine, {} );
        const auto snapshot = engine.get_snapshot();
        const ObjectSnapshot* projectile = find_first_active_projectile( snapshot );
        if ( projectile == nullptr )
        {
            continue;
        }

        if ( hasPrevPos )
        {
            const float dx = projectile->positionPx.x - prevPos.x;
            const float dy = projectile->positionPx.y - prevPos.y;
            const float speedPxPerSec = std::sqrt( dx * dx + dy * dy ) * 60.0f;
            const float speedMps = speedPxPerSec / angry::kPixelsPerMeter;
            maxEstimatedSpeedMps = std::max( maxEstimatedSpeedMps, speedMps );
        }
        prevPos = projectile->positionPx;
        hasPrevPos = true;
    }

    EXPECT_GT( engine.get_snapshot().score, 0 );
    EXPECT_LT( maxEstimatedSpeedMps, 30.0f );
}
