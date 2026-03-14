// ============================================================
// sfx_player.cpp — Procedural SFX generation and playback.
// Part of: angry::render
//
// Implements runtime sound effects for gameplay feedback:
//   * Synthesizes chirp-like waveforms for event categories
//   * Plays ability-specific and material-specific SFX
//   * Maintains active sound list and playback lifetime
//   * Compiles out on web builds without native audio backend
// ============================================================

#include "render/sfx_player.hpp"

#ifndef __EMSCRIPTEN__
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace angry
{
namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr unsigned kSampleRate = 44100;

}  // namespace

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

SfxPlayer::SfxPlayer()
{
    dasher_ability_ = make_chirp ( 0.18f, 230.f, 95.f, 0.18f, 0.85f, 1.85f );
    splitter_ability_ = make_chirp ( 0.16f, 520.f, 810.f, 0.10f, 0.75f, 1.45f );
    boomerang_ability_ = make_chirp ( 0.20f, 340.f, 520.f, 0.20f, 0.82f, 1.65f );
    bomber_ability_ = make_chirp ( 0.22f, 140.f, 58.f, 0.52f, 0.92f, 2.25f );
    dropper_ability_ = make_chirp ( 0.19f, 380.f, 132.f, 0.24f, 0.82f, 1.75f );
    generic_ability_ = make_chirp ( 0.14f, 320.f, 470.f, 0.12f, 0.68f, 1.35f );
    wood_destroy_ = make_chirp ( 0.11f, 180.f, 110.f, 0.42f, 0.78f, 1.8f );
    stone_destroy_ = make_chirp ( 0.14f, 150.f, 70.f, 0.36f, 0.88f, 1.95f );
    glass_destroy_ = make_chirp ( 0.12f, 780.f, 420.f, 0.24f, 0.80f, 1.5f );
    ice_destroy_ = make_chirp ( 0.11f, 640.f, 340.f, 0.22f, 0.72f, 1.45f );
}

// #=# Sound Synthesis #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

sf::SoundBuffer SfxPlayer::make_chirp ( float duration_seconds, float freq_start_hz,
                                        float freq_end_hz, float noise_mix,
                                        float gain, float decay_power )
{
    const auto sample_count = static_cast<std::size_t> (
        std::max ( 1.f, duration_seconds * static_cast<float> ( kSampleRate ) ) );
    std::vector<std::int16_t> samples ( sample_count );

    std::uint32_t rng = 0x9e3779b9u;
    float phase = 0.f;

    for ( std::size_t i = 0; i < sample_count; ++i )
    {
        const float t = static_cast<float> ( i ) / static_cast<float> ( sample_count );
        const float freq = freq_start_hz + ( freq_end_hz - freq_start_hz ) * t;
        phase += 2.f * kPi * freq / static_cast<float> ( kSampleRate );

        rng = rng * 1664525u + 1013904223u;
        const float noise =
            ( static_cast<float> ( rng & 0xFFFFu ) / 32767.5f - 1.f ) * noise_mix;

        const float env = std::pow ( std::max ( 0.f, 1.f - t ), decay_power );
        const float value = std::clamp ( ( std::sin ( phase ) + noise ) * gain * env,
                                         -1.f, 1.f );
        samples[i] = static_cast<std::int16_t> ( value * 32767.f );
    }

    sf::SoundBuffer buffer;
    const std::vector<sf::SoundChannel> channels {sf::SoundChannel::Mono};
    if ( !buffer.loadFromSamples ( samples.data(),
                                   static_cast<std::uint64_t> ( samples.size() ),
                                   1, kSampleRate, channels ) )
    {
        return {};
    }
    return buffer;
}

void SfxPlayer::play ( const sf::SoundBuffer& buffer, float volume, float base_pitch )
{
    if ( buffer.getSampleCount() == 0 )
        return;

    active_sounds_.erase (
        std::remove_if ( active_sounds_.begin(), active_sounds_.end(),
                         [] ( const sf::Sound& sound )
                         {
                             return sound.getStatus() == sf::SoundSource::Status::Stopped;
                         } ),
        active_sounds_.end() );

    active_sounds_.emplace_back ( buffer );
    sf::Sound& sound = active_sounds_.back();
    sound.setVolume ( volume );
    sound.setPitch ( base_pitch + pitch_jitter_ ( rng_ ) );
    sound.play();
}

// #=# Playback #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=

void SfxPlayer::play_ability ( ProjectileType projectile_type )
{
    switch ( projectile_type )
    {
    case ProjectileType::Dasher:
        play ( dasher_ability_, 58.f, 0.96f );
        break;
    case ProjectileType::Splitter:
        play ( splitter_ability_, 56.f, 1.06f );
        break;
    case ProjectileType::Boomerang:
        play ( boomerang_ability_, 57.f, 0.98f );
        break;
    case ProjectileType::Bomber:
        play ( bomber_ability_, 64.f, 0.90f );
        break;
    case ProjectileType::Dropper:
        play ( dropper_ability_, 59.f, 0.94f );
        break;
    case ProjectileType::Heavy:
    case ProjectileType::Bubbler:
    case ProjectileType::Inflater:
        play ( generic_ability_, 50.f, 1.05f );
        break;
    case ProjectileType::Standard:
    default:
        break;
    }
}

void SfxPlayer::play_destroyed ( Material material )
{
    if ( last_destroy_sfx_clock_.getElapsedTime().asSeconds() < 0.045f )
        return;
    last_destroy_sfx_clock_.restart();

    switch ( material )
    {
    case Material::Wood:
        play ( wood_destroy_, 45.f, 0.96f );
        break;
    case Material::Stone:
        play ( stone_destroy_, 42.f, 0.90f );
        break;
    case Material::Glass:
        play ( glass_destroy_, 54.f, 1.10f );
        break;
    case Material::Ice:
        play ( ice_destroy_, 50.f, 1.04f );
        break;
    default:
        break;
    }
}

}  // namespace angry

#else  // __EMSCRIPTEN__ — audio is a no-op on web

namespace angry
{

SfxPlayer::SfxPlayer() {}
void SfxPlayer::play_ability ( ProjectileType ) {}
void SfxPlayer::play_destroyed ( Material ) {}

}  // namespace angry

#endif
