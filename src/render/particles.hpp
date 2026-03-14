// ============================================================
// particles.hpp — Runtime particle simulation declarations.
// Part of: angry::render
//
// Declares lightweight particle rendering primitives:
//   * Particle shape and per-particle runtime state
//   * Emitter API for bursts, rings, and directional sprays
//   * Frame update and draw entry points
//   * Internal cap/budget enforcement for stable performance
// ============================================================

#pragma once
#include "platform/platform.hpp"
#include "shared/types.hpp"

#include <vector>

namespace angry
{

enum class ParticleShape : uint8_t
{
    Circle,
    Shard,
};

struct Particle
{
    platform::Vec2f position;
    platform::Vec2f velocity;
    platform::Color color;
    float lifetime;
    float age = 0.f;
    float size;
    ParticleShape shape = ParticleShape::Circle;
    float rotationDeg = 0.f;
    float angularVelocityDeg = 0.f;
    float gravityScale = 1.f;
    float drag = 0.f;
};

// Manages short-lived visual particles with capped emission and
// deterministic per-frame update/draw integration.
class ParticleSystem
{
private:
    std::vector<Particle> particles_;
    int emitted_this_frame_ = 0;

public:
    void emit ( platform::Vec2f pos, int count, platform::Color color, float speed,
                float lifetime, float size );
    void emit_ring ( platform::Vec2f pos, int count, platform::Color color, float speed,
                     float lifetime, float size );
    void emit_shards ( platform::Vec2f pos, int count, platform::Color color, float speed,
                       float lifetime, float size, float angular_speed );
    void update ( float dt );
    void render ( platform::RenderTarget& target );
    std::size_t size() const { return particles_.size(); }
    bool empty() const { return particles_.empty(); }
};

}  // namespace angry
