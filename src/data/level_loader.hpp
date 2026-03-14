// ============================================================
// level_loader.hpp — Level JSON loading interface.
// Part of: angry::data
//
// Declares level-file deserialization entry points:
//   * Loads full LevelData from one JSON file
//   * Loads LevelMeta list from levels directory
//   * Validates schema and value ranges in implementation
//   * Returns strongly typed shared level structures
// ============================================================

#pragma once
#include <string>
#include <vector>

#include "shared/level_data.hpp"

namespace angry
{

// Loads and validates level definitions from JSON files into
// strongly typed runtime data structures.
class LevelLoader
{
public:
    LevelData load( const std::string& filepath ) const;
    std::vector<LevelMeta> loadAllMeta( const std::string& levelsDir ) const;
};

}  // namespace angry
