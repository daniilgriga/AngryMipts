// ============================================================
// score_saver.cpp — Best-score persistence implementation.
// Part of: angry::data
//
// Implements local score file parsing and updates:
//   * Validates levelId/score/stars constraints
//   * Parses/serializes score JSON arrays
//   * Updates only when new results improve previous best
//   * Logs load/save outcomes for diagnostics
// ============================================================

#include "data/score_saver.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "data/logger.hpp"

namespace angry
{

// #=# Local Helpers & Validation #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

using Json = nlohmann::json;

void validateLevelId( int levelId, const std::string& context )
{
    if ( levelId <= 0 )
    {
        throw std::runtime_error( context + ": levelId must be > 0" );
    }
}

void validateScore( int score, const std::string& context )
{
    if ( score < 0 )
    {
        throw std::runtime_error( context + ": score must be >= 0" );
    }
}

void validateStars( int stars, const std::string& context )
{
    if ( stars < 0 || stars > 3 )
    {
        throw std::runtime_error( context + ": stars must be in range [0, 3]" );
    }
}

int requireInt( const Json& value, const std::string& context )
{
    if ( !value.is_number_integer() )
    {
        throw std::runtime_error( context + ": expected integer" );
    }
    return value.get<int>();
}

std::vector<LevelScore> parseScoresArray( const Json& arrayValue, const std::string& filepath )
{
    if ( !arrayValue.is_array() )
    {
        throw std::runtime_error( "scores: expected array in '" + filepath + "'" );
    }

    std::vector<LevelScore> scores;
    scores.reserve( arrayValue.size() );

    for ( std::size_t i = 0; i < arrayValue.size(); ++i )
    {
        const Json& item = arrayValue.at( i );
        const std::string context = "scores[" + std::to_string( i ) + "]";
        if ( !item.is_object() )
        {
            throw std::runtime_error( context + ": expected object" );
        }

        LevelScore score{};
        score.levelId = requireInt( item.at( "levelId" ), context + ".levelId" );
        score.bestScore = requireInt( item.at( "bestScore" ), context + ".bestScore" );
        score.bestStars = requireInt( item.at( "bestStars" ), context + ".bestStars" );

        validateLevelId( score.levelId, context + ".levelId" );
        validateScore( score.bestScore, context + ".bestScore" );
        validateStars( score.bestStars, context + ".bestStars" );
        scores.push_back( score );
    }

    std::sort( scores.begin(), scores.end(), []( const LevelScore& lhs, const LevelScore& rhs )
               { return lhs.levelId < rhs.levelId; } );

    for ( std::size_t i = 1; i < scores.size(); ++i )
    {
        if ( scores[i - 1].levelId == scores[i].levelId )
        {
            throw std::runtime_error( "Duplicate levelId in scores file '" + filepath +
                                      "': " + std::to_string( scores[i].levelId ) );
        }
    }

    return scores;
}

Json scoresToJson( const std::vector<LevelScore>& scores )
{
    Json array = Json::array();
    for ( const LevelScore& score : scores )
    {
        array.push_back( {
            { "levelId", score.levelId },
            { "bestScore", score.bestScore },
            { "bestStars", score.bestStars },
        } );
    }

    return Json{
        { "scores", array },
    };
}

void writeScoresToFile( const std::string& filepath, const std::vector<LevelScore>& scores )
{
    const std::filesystem::path path( filepath );
    const std::filesystem::path directory = path.parent_path();
    if ( !directory.empty() )
    {
        std::filesystem::create_directories( directory );
    }

    std::ofstream output( path );
    if ( !output.is_open() )
    {
        throw std::runtime_error( "Failed to open scores file for writing: " + filepath );
    }

    output << std::setw( 2 ) << scoresToJson( scores ) << "\n";
    if ( !output.good() )
    {
        throw std::runtime_error( "Failed to write scores file: " + filepath );
    }
}

}  // namespace

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

std::vector<LevelScore> ScoreSaver::loadScores( const std::string& filepath ) const
{
    const std::filesystem::path path( filepath );
    if ( !std::filesystem::exists( path ) )
    {
        Logger::info( "Scores file does not exist yet: {}", filepath );
        return {};
    }

    try
    {
        std::ifstream input( path );
        if ( !input.is_open() )
        {
            throw std::runtime_error( "Failed to open scores file: " + filepath );
        }

        Json root;
        input >> root;

        const Json* scoresNode = nullptr;
        if ( root.is_array() )
        {
            scoresNode = &root;
        }
        else if ( root.is_object() )
        {
            const auto iterator = root.find( "scores" );
            if ( iterator == root.end() )
            {
                Logger::info( "Scores file has no 'scores' key, treat as empty: {}", filepath );
                return {};
            }
            scoresNode = &( *iterator );
        }
        else
        {
            throw std::runtime_error( "scores root must be object or array in '" + filepath + "'" );
        }

        std::vector<LevelScore> scores = parseScoresArray( *scoresNode, filepath );
        Logger::info( "Loaded {} score entries from {}", scores.size(), filepath );
        return scores;
    }
    catch ( const nlohmann::json::exception& error )
    {
        Logger::error( "Failed to parse scores file {}: {}", filepath, error.what() );
        throw std::runtime_error( "Failed to parse scores file '" + filepath +
                                  "': " + error.what() );
    }
    catch ( const std::exception& error )
    {
        Logger::error( "Failed to load scores from {}: {}", filepath, error.what() );
        throw;
    }
}

void ScoreSaver::saveScore( const std::string& filepath, int levelId, int score, int stars ) const
{
    validateLevelId( levelId, "saveScore" );
    validateScore( score, "saveScore" );
    validateStars( stars, "saveScore" );

    try
    {
        std::vector<LevelScore> scores = loadScores( filepath );

        auto iterator =
            std::find_if( scores.begin(), scores.end(),
                          [levelId]( const LevelScore& item ) { return item.levelId == levelId; } );

        bool changed = false;
        if ( iterator == scores.end() )
        {
            scores.push_back( { levelId, score, stars } );
            changed = true;
        }
        else
        {
            if ( score > iterator->bestScore )
            {
                iterator->bestScore = score;
                changed = true;
            }
            if ( stars > iterator->bestStars )
            {
                iterator->bestStars = stars;
                changed = true;
            }
        }

        if ( !changed )
        {
            Logger::info( "Score for level {} not improved (score={}, stars={})", levelId, score,
                          stars );
            return;
        }

        std::sort( scores.begin(), scores.end(), []( const LevelScore& lhs, const LevelScore& rhs )
                   { return lhs.levelId < rhs.levelId; } );

        writeScoresToFile( filepath, scores );
        Logger::info( "Saved score for level {}: score={}, stars={}", levelId, score, stars );
    }
    catch ( const std::exception& error )
    {
        Logger::error( "Failed to save score for level {}: {}", levelId, error.what() );
        throw;
    }
}

}  // namespace angry
