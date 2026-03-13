#pragma once
#include "platform/platform.hpp"
#include "shared/types.hpp"

#include <string>
#include <unordered_map>

namespace angry
{

class TextureManager
{
private:
    bool generated_ = false;
    std::unordered_map<std::string, platform::Texture> textures_;

    void generate_all();
    const platform::Texture& get ( const std::string& key );

public:
    const platform::Texture& block ( Material material );
    const platform::Texture& projectile ( ProjectileType type );
    const platform::Texture& target();
    const platform::Texture& slingshot_wood();
};

}  // namespace angry
