// ============================================================
// sfx_player.hpp — Procedural gameplay SFX interface.
// Part of: angry::render
//
// Declares sound-effect playback for gameplay events:
//   * Ability activation audio by projectile type
//   * Material-specific destruction sound playback
//   * Procedural chirp synthesis helpers (native builds)
//   * No-op compatible API for web/non-audio builds
// ============================================================

#pragma once

#include "shared/types.hpp"

#ifndef __EMSCRIPTEN__
#include <SFML/Audio.hpp>
#include <random>
#include <vector>
#endif

namespace angry
{

// Encapsulates short procedural SFX generation and playback for
// ability/destroy events without external asset dependency.
class SfxPlayer
{
#ifndef __EMSCRIPTEN__
private:
    sf::SoundBuffer dasher_ability_;
    sf::SoundBuffer splitter_ability_;
    sf::SoundBuffer boomerang_ability_;
    sf::SoundBuffer bomber_ability_;
    sf::SoundBuffer dropper_ability_;
    sf::SoundBuffer generic_ability_;
    sf::SoundBuffer wood_destroy_;
    sf::SoundBuffer stone_destroy_;
    sf::SoundBuffer glass_destroy_;
    sf::SoundBuffer ice_destroy_;
    std::vector<sf::Sound> active_sounds_;
    sf::Clock last_destroy_sfx_clock_;
    std::mt19937 rng_ {std::random_device {} ()};
    std::uniform_real_distribution<float> pitch_jitter_ {-0.04f, 0.04f};

    static sf::SoundBuffer make_chirp ( float duration_seconds, float freq_start_hz,
                                        float freq_end_hz, float noise_mix,
                                        float gain, float decay_power );
    void play ( const sf::SoundBuffer& buffer, float volume,
                float base_pitch = 1.f );
#endif

public:
    SfxPlayer();

    void play_ability ( ProjectileType projectile_type );
    void play_destroyed ( Material material );
};

}  // namespace angry
