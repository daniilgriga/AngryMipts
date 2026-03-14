// ============================================================
// texture_manager.hpp — Render texture cache and factories.
// Part of: angry::render
//
// Declares texture provisioning API used by renderer:
//   * Lazy generation/loading of texture atlas entries
//   * Material/projectile/target texture selection helpers
//   * Internal cache keyed by logical texture identifier
//   * Backend-neutral texture handle accessors
// ============================================================

#pragma once
#include "platform/platform.hpp"
#include "shared/types.hpp"

#include <string>
#include <unordered_map>

namespace angry
{

// Owns generated/loaded textures and returns stable references
// for renderer draw calls during gameplay frames.
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
