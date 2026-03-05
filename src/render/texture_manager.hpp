#pragma once
#include "shared/types.hpp"

#include <SFML/Graphics.hpp>

#include <string>
#include <unordered_map>

namespace angry
{

class TextureManager
{
private:
    bool generated_ = false;
    std::unordered_map<std::string, sf::Texture> textures_;

    void generate_all();
    const sf::Texture& get ( const std::string& key );

public:
    const sf::Texture& block ( Material material );
    const sf::Texture& projectile ( ProjectileType type );
    const sf::Texture& target();
    const sf::Texture& slingshot_wood();
};

}  // namespace angry
