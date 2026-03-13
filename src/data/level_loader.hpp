#pragma once
#include <string>
#include <vector>

#include "shared/level_data.hpp"

namespace angry
{

class LevelLoader
{
public:
    LevelData load( const std::string& filepath ) const;
    std::vector<LevelMeta> loadAllMeta( const std::string& levelsDir ) const;
};

}  // namespace angry
