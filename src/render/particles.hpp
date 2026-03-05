#pragma once
#include "shared/types.hpp"

#include <SFML/Graphics.hpp>

#include <vector>

namespace angry
{

struct Particle
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color color;
    float lifetime;
    float age = 0.f;
    float size;
};

class ParticleSystem
{
private:
    std::vector<Particle> particles_;

public:
    void emit ( sf::Vector2f pos, int count, sf::Color color, float speed,
                float lifetime, float size );
    void emit_ring ( sf::Vector2f pos, int count, sf::Color color, float speed,
                     float lifetime, float size );
    void update ( float dt );
    void render ( sf::RenderTarget& target );
    bool empty() const { return particles_.empty(); }
};

}  // namespace angry
