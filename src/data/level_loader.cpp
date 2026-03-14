// ============================================================
// level_loader.cpp — Level JSON loading implementation.
// Part of: angry::data
//
// Implements robust level-file parsing and validation:
//   * Converts JSON fields to typed LevelData structures
//   * Validates geometry, ids, thresholds, and world bounds
//   * Supports triangle vertices with winding normalization
//   * Loads single levels and directory-wide level metadata
// ============================================================

#include "data/level_loader.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "data/logger.hpp"

namespace angry
{

// #=# Local Helpers & Parsers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

using Json = nlohmann::json;

constexpr float kWorldWidthPx = 1920.0f;
constexpr float kWorldHeightPx = 1080.0f;
constexpr float kTriangleMinTwiceAreaPx2 = 1e-3f;

const Json& requireField( const Json& object, const std::string& key, const std::string& context )
{
    if ( !object.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    const auto iterator = object.find( key );
    if ( iterator == object.end() )
    {
        throw std::runtime_error( context + ": missing required field '" + key + "'" );
    }

    return *iterator;
}

float requireFiniteFloat( const Json& value, const std::string& context )
{
    if ( !value.is_number() )
    {
        throw std::runtime_error( context + ": expected number" );
    }

    const double number = value.get<double>();
    if ( !std::isfinite( number ) )
    {
        throw std::runtime_error( context + ": expected finite number" );
    }

    if ( number < -std::numeric_limits<float>::max() || number > std::numeric_limits<float>::max() )
    {
        throw std::runtime_error( context + ": number does not fit float range" );
    }

    return static_cast<float>( number );
}

int requireInt( const Json& value, const std::string& context )
{
    if ( !value.is_number() )
    {
        throw std::runtime_error( context + ": expected number" );
    }

    const double number = value.get<double>();
    if ( !std::isfinite( number ) )
    {
        throw std::runtime_error( context + ": expected finite number" );
    }

    const double rounded = std::round( number );
    if ( std::abs( rounded - number ) > 1e-6 )
    {
        throw std::runtime_error( context + ": expected integer" );
    }

    if ( rounded < std::numeric_limits<int>::min() || rounded > std::numeric_limits<int>::max() )
    {
        throw std::runtime_error( context + ": integer out of range" );
    }

    return static_cast<int>( rounded );
}

std::string requireString( const Json& value, const std::string& context )
{
    if ( !value.is_string() )
    {
        throw std::runtime_error( context + ": expected string" );
    }

    return value.get<std::string>();
}

bool requireBool( const Json& value, const std::string& context )
{
    if ( !value.is_boolean() )
    {
        throw std::runtime_error( context + ": expected boolean" );
    }

    return value.get<bool>();
}

Vec2 parseVec2( const Json& value, const std::string& context )
{
    if ( !value.is_array() || value.size() != 2 )
    {
        throw std::runtime_error( context + ": expected array with exactly 2 numbers" );
    }

    return {
        requireFiniteFloat( value.at( 0 ), context + "[0]" ),
        requireFiniteFloat( value.at( 1 ), context + "[1]" ),
    };
}

float triangleTwiceArea( const std::vector<Vec2>& vertices )
{
    if ( vertices.size() != 3 )
    {
        return 0.0f;
    }

    float twiceArea = 0.0f;
    for ( std::size_t i = 0; i < vertices.size(); ++i )
    {
        const Vec2& current = vertices[i];
        const Vec2& next = vertices[( i + 1u ) % vertices.size()];
        twiceArea += current.x * next.y - next.x * current.y;
    }

    return twiceArea;
}

std::vector<Vec2> parseTriangleVertices( const Json& value, const std::string& context )
{
    if ( !value.is_array() )
    {
        throw std::runtime_error( context + ": expected array of 3 points" );
    }
    if ( value.size() != 3 )
    {
        throw std::runtime_error( context + ": expected exactly 3 points" );
    }

    std::vector<Vec2> vertices;
    vertices.reserve( 3 );
    for ( std::size_t i = 0; i < value.size(); ++i )
    {
        vertices.push_back( parseVec2( value.at( i ), context + "[" + std::to_string( i ) + "]" ) );
    }

    const float twiceArea = triangleTwiceArea( vertices );
    if ( std::abs( twiceArea ) <= kTriangleMinTwiceAreaPx2 )
    {
        throw std::runtime_error( context + ": degenerate triangle (area is zero)" );
    }

    // Normalize winding for downstream consumers.
    if ( twiceArea < 0.0f )
    {
        std::reverse( vertices.begin(), vertices.end() );
    }

    return vertices;
}

Vec2 triangleBoundsSize( const std::vector<Vec2>& vertices )
{
    float minX = vertices.front().x;
    float maxX = vertices.front().x;
    float minY = vertices.front().y;
    float maxY = vertices.front().y;

    for ( const Vec2& point : vertices )
    {
        minX = std::min( minX, point.x );
        maxX = std::max( maxX, point.x );
        minY = std::min( minY, point.y );
        maxY = std::max( maxY, point.y );
    }

    return { maxX - minX, maxY - minY };
}

void validateInsideWorld( const Vec2& pointPx, const std::string& context )
{
    if ( pointPx.x <= 0.0f || pointPx.y <= 0.0f )
    {
        throw std::runtime_error( context + ": coordinates must be > 0" );
    }

    if ( pointPx.x > kWorldWidthPx || pointPx.y > kWorldHeightPx )
    {
        throw std::runtime_error( context +
                                  ": coordinates are outside the game field (1920x1080)" );
    }
}

Material parseMaterial( const std::string& value, const std::string& context )
{
    if ( value == "Wood" )
    {
        return Material::Wood;
    }
    if ( value == "Stone" )
    {
        return Material::Stone;
    }
    if ( value == "Glass" )
    {
        return Material::Glass;
    }
    if ( value == "Ice" )
    {
        return Material::Ice;
    }

    throw std::runtime_error( context + ": unknown material '" + value + "'" );
}

ProjectileType parseProjectileType( const std::string& value, const std::string& context )
{
    if ( value == "Striker" || value == "Standard" )
    {
        return ProjectileType::Standard;
    }
    if ( value == "Heavy" )
    {
        return ProjectileType::Heavy;
    }
    if ( value == "Splitter" )
    {
        return ProjectileType::Splitter;
    }
    if ( value == "Dasher" )
    {
        return ProjectileType::Dasher;
    }
    if ( value == "Bomber" )
    {
        return ProjectileType::Bomber;
    }
    if ( value == "Dropper" )
    {
        return ProjectileType::Dropper;
    }
    if ( value == "Boomerang" )
    {
        return ProjectileType::Boomerang;
    }
    if ( value == "Bubbler" )
    {
        return ProjectileType::Bubbler;
    }
    if ( value == "Inflater" )
    {
        return ProjectileType::Inflater;
    }

    throw std::runtime_error( context + ": unknown projectile type '" + value + "'" );
}

LevelMeta parseMeta( const Json& value )
{
    const std::string context = "meta";
    if ( !value.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    LevelMeta meta;
    meta.id = requireInt( requireField( value, "id", context ), "meta.id" );
    if ( meta.id <= 0 )
    {
        throw std::runtime_error( "meta.id: expected value > 0" );
    }

    meta.name = requireString( requireField( value, "name", context ), "meta.name" );
    if ( meta.name.empty() )
    {
        throw std::runtime_error( "meta.name: expected non-empty string" );
    }

    meta.totalShots = requireInt( requireField( value, "totalShots", context ), "meta.totalShots" );
    if ( meta.totalShots <= 0 )
    {
        throw std::runtime_error( "meta.totalShots: expected value > 0" );
    }

    const Json& thresholds = requireField( value, "starThresholds", context );
    if ( !thresholds.is_array() || thresholds.size() != 3 )
    {
        throw std::runtime_error( "meta.starThresholds: expected exactly 3 values" );
    }

    meta.star1Threshold = requireInt( thresholds.at( 0 ), "meta.starThresholds[0]" );
    meta.star2Threshold = requireInt( thresholds.at( 1 ), "meta.starThresholds[1]" );
    meta.star3Threshold = requireInt( thresholds.at( 2 ), "meta.starThresholds[2]" );

    if ( !( meta.star1Threshold < meta.star2Threshold &&
            meta.star2Threshold < meta.star3Threshold ) )
    {
        throw std::runtime_error( "meta.starThresholds: values must be strictly increasing" );
    }

    return meta;
}

SlingshotData parseSlingshot( const Json& value )
{
    const std::string context = "slingshot";
    if ( !value.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    SlingshotData slingshot;
    slingshot.positionPx =
        parseVec2( requireField( value, "position", context ), "slingshot.position" );
    validateInsideWorld( slingshot.positionPx, "slingshot.position" );

    slingshot.maxPullPx =
        requireFiniteFloat( requireField( value, "maxPull", context ), "slingshot.maxPull" );
    if ( slingshot.maxPullPx <= 0.0f )
    {
        throw std::runtime_error( "slingshot.maxPull: expected value > 0" );
    }

    return slingshot;
}

ProjectileData parseProjectile( const Json& value, std::size_t index )
{
    const std::string context = "projectiles[" + std::to_string( index ) + "]";
    if ( !value.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    ProjectileData projectile;
    const std::string type =
        requireString( requireField( value, "type", context ), context + ".type" );
    projectile.type = parseProjectileType( type, context + ".type" );
    return projectile;
}

BlockData parseBlock( const Json& value, std::size_t index )
{
    const std::string context = "blocks[" + std::to_string( index ) + "]";
    if ( !value.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    BlockData block;
    block.positionPx =
        parseVec2( requireField( value, "position", context ), context + ".position" );
    validateInsideWorld( block.positionPx, context + ".position" );

    block.angleDeg =
        requireFiniteFloat( requireField( value, "angle", context ), context + ".angle" );

    const std::string materialName =
        requireString( requireField( value, "material", context ), context + ".material" );
    block.material = parseMaterial( materialName, context + ".material" );

    block.hp = requireFiniteFloat( requireField( value, "hp", context ), context + ".hp" );
    if ( block.hp <= 0.0f )
    {
        throw std::runtime_error( context + ".hp: expected value > 0" );
    }

    if ( const auto it = value.find( "static" ); it != value.end() )
    {
        block.isStatic = requireBool( *it, context + ".static" );
    }
    else if ( const auto it = value.find( "isStatic" ); it != value.end() )
    {
        block.isStatic = requireBool( *it, context + ".isStatic" );
    }

    if ( const auto it = value.find( "indestructible" ); it != value.end() )
    {
        block.isIndestructible = requireBool( *it, context + ".indestructible" );
    }
    else if ( const auto it = value.find( "isIndestructible" ); it != value.end() )
    {
        block.isIndestructible = requireBool( *it, context + ".isIndestructible" );
    }

    // Nondestructible blocks in this project are treated as fully static obstacles.
    if ( block.isIndestructible )
    {
        block.isStatic = true;
    }
    if ( block.isStatic )
    {
        block.isIndestructible = true;
    }

    const std::string shape =
        requireString( requireField( value, "shape", context ), context + ".shape" );
    if ( shape == "rect" )
    {
        block.sizePx = parseVec2( requireField( value, "size", context ), context + ".size" );
        if ( block.sizePx.x <= 0.0f || block.sizePx.y <= 0.0f )
        {
            throw std::runtime_error( context + ".size: width and height must be > 0" );
        }
        block.radiusPx = 0.0f;
        block.shape = BlockShape::Rect;
    }
    else if ( shape == "circle" )
    {
        block.radiusPx =
            requireFiniteFloat( requireField( value, "radius", context ), context + ".radius" );
        if ( block.radiusPx <= 0.0f )
        {
            throw std::runtime_error( context + ".radius: expected value > 0" );
        }
        block.sizePx = { 0.0f, 0.0f };
        block.shape = BlockShape::Circle;
    }
    else if ( shape == "triangle" )
    {
        block.shape = BlockShape::Triangle;
        block.radiusPx = 0.0f;

        if ( const auto verticesIt = value.find( "vertices" ); verticesIt != value.end() )
        {
            block.triangleLocalVerticesPx =
                parseTriangleVertices( *verticesIt, context + ".vertices" );
        }

        if ( const auto sizeIt = value.find( "size" ); sizeIt != value.end() )
        {
            block.sizePx = parseVec2( *sizeIt, context + ".size" );
        }
        else if ( !block.triangleLocalVerticesPx.empty() )
        {
            block.sizePx = triangleBoundsSize( block.triangleLocalVerticesPx );
        }
        else
        {
            throw std::runtime_error( context + ": triangle requires 'vertices' or legacy 'size'" );
        }

        if ( block.sizePx.x <= 0.0f || block.sizePx.y <= 0.0f )
        {
            throw std::runtime_error( context + ".size: width and height must be > 0" );
        }
    }
    else
    {
        throw std::runtime_error( context + ".shape: expected 'rect', 'circle' or 'triangle'" );
    }

    return block;
}

TargetData parseTarget( const Json& value, std::size_t index )
{
    const std::string context = "targets[" + std::to_string( index ) + "]";
    if ( !value.is_object() )
    {
        throw std::runtime_error( context + ": expected object" );
    }

    TargetData target;
    target.positionPx =
        parseVec2( requireField( value, "position", context ), context + ".position" );
    validateInsideWorld( target.positionPx, context + ".position" );

    target.radiusPx =
        requireFiniteFloat( requireField( value, "radius", context ), context + ".radius" );
    if ( target.radiusPx <= 0.0f )
    {
        throw std::runtime_error( context + ".radius: expected value > 0" );
    }

    target.hp = requireFiniteFloat( requireField( value, "hp", context ), context + ".hp" );
    if ( target.hp <= 0.0f )
    {
        throw std::runtime_error( context + ".hp: expected value > 0" );
    }

    target.scoreValue = requireInt( requireField( value, "score", context ), context + ".score" );
    if ( target.scoreValue <= 0 )
    {
        throw std::runtime_error( context + ".score: expected value > 0" );
    }

    return target;
}

Json loadJsonFromFile( const std::filesystem::path& filepath )
{
    std::ifstream input( filepath );
    if ( !input.is_open() )
    {
        throw std::runtime_error( "Failed to open level file: " + filepath.string() );
    }

    try
    {
        Json root;
        input >> root;
        return root;
    }
    catch ( const Json::exception& error )
    {
        throw std::runtime_error( "Failed to parse JSON in '" + filepath.string() +
                                  "': " + error.what() );
    }
}

}  // namespace

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

LevelData LevelLoader::load( const std::string& filepath ) const
{
    try
    {
        const std::filesystem::path path( filepath );
        if ( !std::filesystem::exists( path ) )
        {
            throw std::runtime_error( "Level file does not exist: " + path.string() );
        }

        const Json root = loadJsonFromFile( path );
        if ( !root.is_object() )
        {
            throw std::runtime_error( "root: expected object" );
        }

        LevelData data;
        data.meta = parseMeta( requireField( root, "meta", "root" ) );
        data.slingshot = parseSlingshot( requireField( root, "slingshot", "root" ) );

        const Json& projectiles = requireField( root, "projectiles", "root" );
        if ( !projectiles.is_array() )
        {
            throw std::runtime_error( "root.projectiles: expected array" );
        }

        data.projectiles.reserve( projectiles.size() );
        for ( std::size_t i = 0; i < projectiles.size(); ++i )
        {
            data.projectiles.push_back( parseProjectile( projectiles.at( i ), i ) );
        }

        const Json& blocks = requireField( root, "blocks", "root" );
        if ( !blocks.is_array() )
        {
            throw std::runtime_error( "root.blocks: expected array" );
        }

        data.blocks.reserve( blocks.size() );
        for ( std::size_t i = 0; i < blocks.size(); ++i )
        {
            data.blocks.push_back( parseBlock( blocks.at( i ), i ) );
        }

        const Json& targets = requireField( root, "targets", "root" );
        if ( !targets.is_array() )
        {
            throw std::runtime_error( "root.targets: expected array" );
        }

        data.targets.reserve( targets.size() );
        for ( std::size_t i = 0; i < targets.size(); ++i )
        {
            data.targets.push_back( parseTarget( targets.at( i ), i ) );
        }

        if ( data.meta.totalShots != static_cast<int>( data.projectiles.size() ) )
        {
            throw std::runtime_error( "meta.totalShots must match projectiles array size" );
        }

        Logger::info( "Level loaded: {} ({})", data.meta.id, data.meta.name );
        return data;
    }
    catch ( const std::exception& error )
    {
        Logger::error( "Failed to load level from {}: {}", filepath, error.what() );
        throw;
    }
}

std::vector<LevelMeta> LevelLoader::loadAllMeta( const std::string& levelsDir ) const
{
    try
    {
        const std::filesystem::path directory( levelsDir );
        if ( !std::filesystem::exists( directory ) || !std::filesystem::is_directory( directory ) )
        {
            throw std::runtime_error( "Levels directory is invalid: " + directory.string() );
        }

        std::vector<LevelMeta> allMeta;
        for ( const auto& entry : std::filesystem::directory_iterator( directory ) )
        {
            if ( !entry.is_regular_file() )
            {
                continue;
            }
            if ( entry.path().extension() != ".json" )
            {
                continue;
            }

            try
            {
                const LevelData level = load( entry.path().string() );
                allMeta.push_back( level.meta );
            }
            catch ( const std::exception& error )
            {
                Logger::error( "Skipping invalid level '{}': {}", entry.path().string(),
                               error.what() );
            }
        }

        std::sort( allMeta.begin(), allMeta.end(),
                   []( const LevelMeta& lhs, const LevelMeta& rhs ) { return lhs.id < rhs.id; } );

        for ( std::size_t i = 1; i < allMeta.size(); ++i )
        {
            if ( allMeta[i - 1].id == allMeta[i].id )
            {
                throw std::runtime_error( "Duplicate level id detected: " +
                                          std::to_string( allMeta[i].id ) );
            }
        }

        Logger::info( "Loaded {} level meta entries from {}", allMeta.size(), levelsDir );
        return allMeta;
    }
    catch ( const std::exception& error )
    {
        Logger::error( "Failed to load levels meta from {}: {}", levelsDir, error.what() );
        throw;
    }
}

}  // namespace angry
