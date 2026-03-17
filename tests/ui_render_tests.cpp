// ============================================================
// ui_render_tests.cpp — UI/render helper unit tests.
// Part of: angry::tests
//
// Verifies rendering-adjacent utility behavior:
//   * Letterbox viewport calculations for aspect handling
//   * Particle system frame/hard-cap constraints
//   * Particle lifetime expiration behavior
//   * Non-GPU deterministic helper expectations
// ============================================================

#include "render/particles.hpp"
#include "shared/world_config.hpp"
#include "ui/view_utils.hpp"

#include <SFML/Graphics/View.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

// #=# Test Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

bool almost_equal ( float a, float b, float eps = 1e-4f )
{
    return std::fabs ( a - b ) <= eps;
}

// #=# Test Cases #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

TEST ( LetterboxView, EqualAspectUsesFullViewport )
{
    sf::View view ( sf::FloatRect ( {0.f, 0.f}, {angry::world::kWidthPx, angry::world::kHeightPx} ) );
    angry::apply_letterbox_view ( view, {1280u, 720u} );
    const sf::FloatRect viewport = view.getViewport();
    EXPECT_TRUE ( almost_equal ( viewport.position.x, 0.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.position.y, 0.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.x, 1.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.y, 1.f ) );
}

TEST ( LetterboxView, WideWindowAddsSideBars )
{
    sf::View view ( sf::FloatRect ( {0.f, 0.f}, {angry::world::kWidthPx, angry::world::kHeightPx} ) );
    angry::apply_letterbox_view ( view, {2560u, 720u} );
    const sf::FloatRect viewport = view.getViewport();
    EXPECT_TRUE ( almost_equal ( viewport.position.x, 0.25f ) );
    EXPECT_TRUE ( almost_equal ( viewport.position.y, 0.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.x, 0.5f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.y, 1.f ) );
}

TEST ( LetterboxView, TallWindowAddsTopBottomBars )
{
    sf::View view ( sf::FloatRect ( {0.f, 0.f}, {angry::world::kWidthPx, angry::world::kHeightPx} ) );
    angry::apply_letterbox_view ( view, {720u, 1280u} );
    const sf::FloatRect viewport = view.getViewport();
    EXPECT_TRUE ( almost_equal ( viewport.position.x, 0.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.position.y, 0.341796875f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.x, 1.f ) );
    EXPECT_TRUE ( almost_equal ( viewport.size.y, 0.31640625f ) );
}

TEST ( ParticleSystem, EnforcesFrameAndHardCaps )
{
    std::srand ( 1 );
    angry::ParticleSystem particles;

    particles.emit ( {0.f, 0.f}, 10'000, sf::Color::White, 100.f, 0.8f, 2.0f );
    EXPECT_EQ ( particles.size(), 260u );

    particles.emit ( {0.f, 0.f}, 10'000, sf::Color::White, 100.f, 0.8f, 2.0f );
    EXPECT_EQ ( particles.size(), 260u );

    for ( int i = 0; i < 12; ++i )
    {
        particles.update ( 0.f );  // reset per-frame budget without aging out particles
        particles.emit ( {0.f, 0.f}, 10'000, sf::Color::White, 100.f, 0.8f, 2.0f );
    }

    EXPECT_EQ ( particles.size(), 1600u );
}

TEST ( ParticleSystem, RemovesExpiredParticles )
{
    std::srand ( 2 );
    angry::ParticleSystem particles;
    particles.emit ( {0.f, 0.f}, 180, sf::Color::White, 120.f, 0.2f, 2.0f );
    EXPECT_FALSE ( particles.empty() );

    particles.update ( 1.0f );
    EXPECT_TRUE ( particles.empty() );
}
