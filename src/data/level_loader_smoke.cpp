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
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

}  // namespace

// #=# Smoke Entry Point #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

int main()
{
    try
    {
        angry::LevelLoader loader;
        const std::filesystem::path levelsDir =
            std::filesystem::path( ANGRY_MIPTS_SOURCE_DIR ) / "levels";
        const std::vector<std::string> levelFiles = {
            ( levelsDir / "level_01.json" ).string(),
            ( levelsDir / "level_02.json" ).string(),
            ( levelsDir / "level_03.json" ).string(),
            ( levelsDir / "level_020.json" ).string(),
        };

        for ( const std::string& path : levelFiles )
        {
            const angry::LevelData level = loader.load( path );
            std::cout << "Loaded level id=" << level.meta.id << " name='" << level.meta.name
                      << "'\n";

            if ( level.meta.id == 20 )
            {
                bool hasTriangleVertices = false;
                for ( const angry::BlockData& block : level.blocks )
                {
                    if ( block.shape == angry::BlockShape::Triangle &&
                         !block.triangleLocalVerticesPx.empty() )
                    {
                        hasTriangleVertices = true;
                        break;
                    }
                }
                expect( hasTriangleVertices, "level 20 must include triangle vertices" );
            }
        }

        const std::vector<angry::LevelMeta> allMeta = loader.loadAllMeta( levelsDir.string() );
        std::cout << "Loaded meta count: " << allMeta.size() << "\n";

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
