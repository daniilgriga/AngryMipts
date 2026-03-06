#pragma once
#include "shared/types.hpp"

#include <SFML/Graphics.hpp>

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
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color color;
    float lifetime;
    float age = 0.f;
    float size;
    ParticleShape shape = ParticleShape::Circle;
    float rotationDeg = 0.f;
    float angularVelocityDeg = 0.f;
    float gravityScale = 1.f;
    float drag = 0.f;
};

class ParticleSystem
{
private:
    std::vector<Particle> particles_;
    int emitted_this_frame_ = 0;

public:
    void emit ( sf::Vector2f pos, int count, sf::Color color, float speed,
                float lifetime, float size );
    void emit_ring ( sf::Vector2f pos, int count, sf::Color color, float speed,
                     float lifetime, float size );
    void emit_shards ( sf::Vector2f pos, int count, sf::Color color, float speed,
                       float lifetime, float size, float angular_speed );
    void update ( float dt );
    void render ( sf::RenderTarget& target );
    std::size_t size() const { return particles_.size(); }
    bool empty() const { return particles_.empty(); }
};

}  // namespace angry
