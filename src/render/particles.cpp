#include "render/particles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace angry
{
namespace
{

float rand_float ( float lo, float hi )
{
    float t = static_cast<float> ( std::rand() ) / static_cast<float> ( RAND_MAX );
    return lo + t * ( hi - lo );
}

}  // namespace

void ParticleSystem::emit ( sf::Vector2f pos, int count, sf::Color color,
                            float speed, float lifetime, float size )
{
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
        particles_.push_back ( p );
    }
}

void ParticleSystem::emit_ring ( sf::Vector2f pos, int count, sf::Color color,
                                 float speed, float lifetime, float size )
{
    for ( int i = 0; i < count; ++i )
    {
        float angle = 6.2832f * static_cast<float> ( i ) / static_cast<float> ( count );
        Particle p;
        p.position = pos;
        p.velocity = {std::cos ( angle ) * speed, std::sin ( angle ) * speed};
        p.color = color;
        p.lifetime = lifetime;
        p.size = size;
        particles_.push_back ( p );
    }
}

void ParticleSystem::update ( float dt )
{
    for ( auto& p : particles_ )
    {
        p.age += dt;
        p.velocity.y += 200.f * dt;  // gravity on particles
        p.position += p.velocity * dt;
    }

    // remove dead particles
    particles_.erase (
        std::remove_if ( particles_.begin(), particles_.end(),
                         [] ( const Particle& p ) { return p.age >= p.lifetime; } ),
        particles_.end() );
}

void ParticleSystem::render ( sf::RenderWindow& window )
{
    for ( const auto& p : particles_ )
    {
        float alpha = 1.f - ( p.age / p.lifetime );
        float cur_size = p.size * alpha;

        sf::CircleShape dot ( cur_size );
        dot.setOrigin ( {cur_size, cur_size} );
        dot.setPosition ( p.position );

        sf::Color c = p.color;
        c.a = static_cast<uint8_t> ( 255.f * alpha * alpha );
        dot.setFillColor ( c );

        window.draw ( dot );
    }
}

}  // namespace angry
