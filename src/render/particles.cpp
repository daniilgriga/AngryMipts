#include "render/particles.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdlib>

namespace angry
{
namespace
{

constexpr std::size_t kParticleHardCap = 1600;
constexpr int kFrameEmitCap = 260;

float rand_float ( float lo, float hi )
{
    float t = static_cast<float> ( std::rand() ) / static_cast<float> ( RAND_MAX );
    return lo + t * ( hi - lo );
}

}  // namespace

void ParticleSystem::emit ( sf::Vector2f pos, int count, sf::Color color,
                            float speed, float lifetime, float size )
{
    if ( particles_.capacity() < kParticleHardCap )
        particles_.reserve ( kParticleHardCap );

    const std::size_t available =
        kParticleHardCap > particles_.size() ? ( kParticleHardCap - particles_.size() ) : 0u;
    const int frame_budget_left = std::max ( 0, kFrameEmitCap - emitted_this_frame_ );
    count = std::min ( count, static_cast<int> ( available ) );
    count = std::min ( count, frame_budget_left );
    if ( count <= 0 )
        return;

    for ( int i = 0; i < count; ++i )
    {
        float angle = rand_float ( 0.f, 6.2832f );
        float spd = rand_float ( speed * 0.3f, speed );
        Particle p;
        p.position = pos;
        p.velocity = {std::cos ( angle ) * spd, std::sin ( angle ) * spd};
        p.color = color;
        p.lifetime = rand_float ( lifetime * 0.5f, lifetime );
        p.size = rand_float ( size * 0.5f, size );
        p.drag = 0.6f;
        particles_.push_back ( p );
    }

    emitted_this_frame_ += count;
}

void ParticleSystem::emit_ring ( sf::Vector2f pos, int count, sf::Color color,
                                 float speed, float lifetime, float size )
{
    if ( particles_.capacity() < kParticleHardCap )
        particles_.reserve ( kParticleHardCap );

    const std::size_t available =
        kParticleHardCap > particles_.size() ? ( kParticleHardCap - particles_.size() ) : 0u;
    const int frame_budget_left = std::max ( 0, kFrameEmitCap - emitted_this_frame_ );
    count = std::min ( count, static_cast<int> ( available ) );
    count = std::min ( count, frame_budget_left );
    if ( count <= 0 )
        return;

    for ( int i = 0; i < count; ++i )
    {
        float angle = 6.2832f * static_cast<float> ( i ) / static_cast<float> ( count );
        Particle p;
        p.position = pos;
        p.velocity = {std::cos ( angle ) * speed, std::sin ( angle ) * speed};
        p.color = color;
        p.lifetime = lifetime;
        p.size = size;
        p.drag = 0.25f;
        particles_.push_back ( p );
    }

    emitted_this_frame_ += count;
}

void ParticleSystem::emit_shards ( sf::Vector2f pos, int count, sf::Color color,
                                   float speed, float lifetime, float size,
                                   float angular_speed )
{
    if ( particles_.capacity() < kParticleHardCap )
        particles_.reserve ( kParticleHardCap );

    const std::size_t available =
        kParticleHardCap > particles_.size() ? ( kParticleHardCap - particles_.size() ) : 0u;
    const int frame_budget_left = std::max ( 0, kFrameEmitCap - emitted_this_frame_ );
    count = std::min ( count, static_cast<int> ( available ) );
    count = std::min ( count, frame_budget_left );
    if ( count <= 0 )
        return;

    for ( int i = 0; i < count; ++i )
    {
        float angle = rand_float ( 0.f, 6.2832f );
        float spd = rand_float ( speed * 0.35f, speed );
        Particle p;
        p.position = pos;
        p.velocity = {std::cos ( angle ) * spd, std::sin ( angle ) * spd};
        p.color = color;
        p.lifetime = rand_float ( lifetime * 0.6f, lifetime );
        p.size = rand_float ( size * 0.6f, size );
        p.shape = ParticleShape::Shard;
        p.rotationDeg = rand_float ( 0.f, 360.f );
        p.angularVelocityDeg = rand_float ( -angular_speed, angular_speed );
        p.gravityScale = 1.15f;
        p.drag = 1.2f;
        particles_.push_back ( p );
    }

    emitted_this_frame_ += count;
}

void ParticleSystem::update ( float dt )
{
    emitted_this_frame_ = 0;

    for ( auto& p : particles_ )
    {
        p.age += dt;
        const float damping = std::max ( 0.f, 1.f - p.drag * dt );
        p.velocity *= damping;
        p.velocity.y += 200.f * p.gravityScale * dt;  // gravity on particles
        p.position += p.velocity * dt;
        p.rotationDeg += p.angularVelocityDeg * dt;
    }

    // remove dead particles
    particles_.erase (
        std::remove_if ( particles_.begin(), particles_.end(),
                         [] ( const Particle& p ) { return p.age >= p.lifetime; } ),
        particles_.end() );
}

void ParticleSystem::render ( sf::RenderTarget& target )
{
    const std::size_t draw_stride = particles_.size() > 1100 ? 2u : 1u;
    for ( std::size_t i = 0; i < particles_.size(); i += draw_stride )
    {
        const auto& p = particles_[i];
        const float alpha = std::clamp ( 1.f - ( p.age / p.lifetime ), 0.f, 1.f );
        float cur_size = p.size * alpha;
        const float base_alpha = static_cast<float> ( p.color.a ) / 255.f;

        sf::Color c = p.color;

        if ( p.shape == ParticleShape::Shard )
        {
            // Keep shards readable longer than dust/sparks so material breakage is visible.
            cur_size = p.size * ( 0.72f + 0.28f * alpha );
            c.a = static_cast<uint8_t> (
                255.f * base_alpha * ( 0.25f + 0.75f * alpha ) );

            sf::RectangleShape shard ( {cur_size * 1.6f, cur_size * 0.8f} );
            shard.setOrigin ( {cur_size * 0.8f, cur_size * 0.4f} );
            shard.setPosition ( p.position );
            shard.setRotation ( sf::degrees ( p.rotationDeg ) );
            shard.setFillColor ( c );
            target.draw ( shard );
        }
        else
        {
            c.a = static_cast<uint8_t> ( 255.f * base_alpha * alpha * alpha );
            sf::CircleShape dot ( cur_size );
            dot.setOrigin ( {cur_size, cur_size} );
            dot.setPosition ( p.position );
            dot.setFillColor ( c );
            target.draw ( dot );
        }
    }
}

}  // namespace angry
