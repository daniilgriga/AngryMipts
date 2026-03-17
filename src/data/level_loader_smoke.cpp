// ============================================================
// level_loader_smoke.cpp — Data-layer smoke executable.
// Part of: angry::data
//
// Implements a lightweight runtime sanity check that:
//   * Loads selected level JSON files via LevelLoader
//   * Verifies presence of triangle vertices on test level
//   * Exercises ScoreSaver best-value update behavior
//   * Returns non-zero on any failed expectation
// ============================================================

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>

#include "data/level_loader.hpp"
#include "data/score_saver.hpp"

namespace
{

// Throws when a smoke condition fails to keep main flow linear.
void expect( bool condition, const std::string& message )
{
    if ( !condition )
    {
        throw std::runtime_error( message );
    }
}

std::string two_digit( int value )
{
    std::ostringstream out;
    out << std::setw( 2 ) << std::setfill( '0' ) << value;
    return out.str();
}

std::filesystem::path resolve_level_path( const std::filesystem::path& levels_dir,
                                          int level_id )
{
    const std::array<std::string, 3> candidates = {
        "level_0" + std::to_string( level_id ) + ".json",
        "level_" + two_digit( level_id ) + ".json",
        "level_" + std::to_string( level_id ) + ".json",
    };

    for ( const std::string& candidate : candidates )
    {
        const std::filesystem::path path = levels_dir / candidate;
        if ( std::filesystem::exists( path ) )
            return path;
    }

    throw std::runtime_error( "Failed to resolve level file for id=" +
                              std::to_string( level_id ) );
}

}  // namespace

// #=# Smoke Entry Point #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

int main()
{
    try
    {
        angry::LevelLoader loader;
        const std::filesystem::path levelsDir =
            std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / "levels";

        const std::vector<angry::LevelMeta> allMeta = loader.loadAllMeta( levelsDir.string() );
        std::cout << "Loaded meta count: " << allMeta.size() << "\n";
        expect( !allMeta.empty(), "no levels discovered in levels directory" );

        bool has_triangle_vertices = false;
        std::size_t printed_levels = 0;
        for ( const angry::LevelMeta& meta : allMeta )
        {
            const std::filesystem::path levelPath = resolve_level_path( levelsDir, meta.id );
            const angry::LevelData level = loader.load( levelPath.string() );

            if ( printed_levels < 3 )
            {
                std::cout << "Loaded level id=" << level.meta.id << " name='"
                          << level.meta.name << "'\n";
                ++printed_levels;
            }

            for ( const angry::BlockData& block : level.blocks )
            {
                if ( block.shape == angry::BlockShape::Triangle &&
                     !block.triangleLocalVerticesPx.empty() )
                {
                    has_triangle_vertices = true;
                    break;
                }
            }
        }
        expect( has_triangle_vertices,
                "at least one level must include triangle vertices" );

        angry::ScoreSaver scoreSaver;
        const std::filesystem::path smokeScoresPath =
            std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / "assets" / "scores_smoke.json";
        std::filesystem::remove( smokeScoresPath );

        scoreSaver.saveScore( smokeScoresPath.string(), 1, 1000, 1 );
        scoreSaver.saveScore( smokeScoresPath.string(), 1, 900, 1 );
        scoreSaver.saveScore( smokeScoresPath.string(), 1, 1100, 1 );
        scoreSaver.saveScore( smokeScoresPath.string(), 1, 1050, 2 );
        scoreSaver.saveScore( smokeScoresPath.string(), 2, 500, 1 );

        const std::vector<angry::LevelScore> scores =
            scoreSaver.loadScores( smokeScoresPath.string() );
        expect( scores.size() == 2, "scores size must be 2" );
        expect( scores[0].levelId == 1, "first score entry levelId must be 1" );
        expect( scores[0].bestScore == 1100, "best score for level 1 must stay 1100" );
        expect( scores[0].bestStars == 2, "best stars for level 1 must be 2" );
        expect( scores[1].levelId == 2, "second score entry levelId must be 2" );

        std::filesystem::remove( smokeScoresPath );
        std::cout << "ScoreSaver smoke passed\n";
        return 0;
    }
    catch ( const std::exception& error )
    {
        std::cerr << "Data smoke failed: " << error.what() << "\n";
        return 1;
    }
}
