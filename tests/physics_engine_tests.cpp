#include "physics/physics_engine.hpp"
#include "shared/command.hpp"
#include "shared/event.hpp"
#include "shared/level_data.hpp"
#include "shared/thread_safe_queue.hpp"
#include "shared/types.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{

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

LevelData makeLevel(
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

void runCommandsAndStep( PhysicsEngine& engine, const std::vector<Command>& commands )
{
    ThreadSafeQueue<Command> queue;
    for ( const Command& cmd : commands )
    {
        queue.push( cmd );
    }

    engine.processCommands( queue );
    engine.step( 1.0f / 60.0f );
}

int countAliveTargets( const angry::WorldSnapshot& snapshot )
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

bool hasAbilityEventFor(
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

}  // namespace

TEST( PhysicsEngineStatus, WinOnlyWhenNoAliveTargets )
{
    PhysicsEngine engine;

    const LevelData noTargetsLevel = makeLevel(
        101,
        {},
        {},
        {},
        200,
        300 );
    engine.loadLevel( noTargetsLevel );
    runCommandsAndStep( engine, {} );
    auto snapshot = engine.getSnapshot();
    EXPECT_EQ( countAliveTargets( snapshot ), 0 );
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
    const LevelData targetAliveLevel = makeLevel(
        102,
        {ProjectileType::Bomber},
        {woodBlock},
        {aliveTarget},
        50,
        100 );

    engine.loadLevel( targetAliveLevel );
    runCommandsAndStep(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::INVALID_ID},
        } );

    snapshot = engine.getSnapshot();
    EXPECT_GT( snapshot.score, 0 );  // score threshold may be reached via block destruction
    EXPECT_GT( countAliveTargets( snapshot ), 0 );
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

    const LevelData twoStarsLevel = makeLevel(
        201,
        {ProjectileType::Bomber},
        {woodBlock},
        {target},
        150,
        200 );
    engine.loadLevel( twoStarsLevel );
    runCommandsAndStep(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::INVALID_ID},
        } );
    auto snapshot = engine.getSnapshot();
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
    const LevelData threeStarsLevel = makeLevel(
        202,
        {ProjectileType::Bomber},
        {stoneBlock},
        {target},
        150,
        200 );
    engine.loadLevel( threeStarsLevel );
    runCommandsAndStep(
        engine,
        {
            LaunchCmd{Vec2{0.0f, 0.0f}},
            ActivateAbilityCmd{angry::INVALID_ID},
        } );
    snapshot = engine.getSnapshot();
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
        const LevelData level = makeLevel(
            levelId++,
            {ProjectileType::Bomber},
            {block},
            {},
            100,
            200 );

        engine.loadLevel( level );
        runCommandsAndStep(
            engine,
            {
                LaunchCmd{Vec2{0.0f, 0.0f}},
                ActivateAbilityCmd{angry::INVALID_ID},
            } );

        const auto snapshot = engine.getSnapshot();
        EXPECT_EQ( snapshot.score, c.expectedScore );
    }
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
        const LevelData level = makeLevel(
            levelId++,
            {projectileType},
            {},
            {farTarget},
            200,
            300 );

        engine.loadLevel( level );
        runCommandsAndStep(
            engine,
            {
                LaunchCmd{Vec2{-20.0f, -20.0f}},
                ActivateAbilityCmd{angry::INVALID_ID},
            } );

        const std::vector<angry::Event> events = engine.drainEvents();
        EXPECT_TRUE( hasAbilityEventFor( events, projectileType ) )
            << "missing AbilityActivatedEvent for projectileType="
            << static_cast<int>( projectileType );
    }
}

