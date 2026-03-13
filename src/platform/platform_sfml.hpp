#pragma once

// ============================================================
// platform_sfml.hpp — SFML backend (native builds).
// All platform types are direct SFML types — zero overhead,
// no wrapping, existing code needs no changes to compile.
// ============================================================

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

namespace platform
{

// ── Geometry ────────────────────────────────────────────────
using Vec2f     = sf::Vector2f;
using Vec2u     = sf::Vector2u;
using Vec2i     = sf::Vector2i;
using Rect      = sf::FloatRect;
using FloatRect = sf::FloatRect;
using Color     = sf::Color;
using Transform = sf::Transform;

// ── Window / rendering ──────────────────────────────────────
using Window       = sf::RenderWindow;
using RenderTarget = sf::RenderTarget;
using Event        = sf::Event;

// ── Graphics resources ──────────────────────────────────────
using Font    = sf::Font;
using Texture = sf::Texture;
using Image   = sf::Image;
using Sprite  = sf::Sprite;
using Shader  = sf::Shader;
using View    = sf::View;

// ── Text ────────────────────────────────────────────────────
using Text = sf::Text;

// ── Shapes ──────────────────────────────────────────────────
using RectShape   = sf::RectangleShape;
using CircleShape = sf::CircleShape;
using ConvexShape = sf::ConvexShape;
using VertexArray = sf::VertexArray;
using Vertex      = sf::Vertex;

// ── Audio ───────────────────────────────────────────────────
using SoundBuffer = sf::SoundBuffer;
using Sound       = sf::Sound;

// ── Time ────────────────────────────────────────────────────
using Clock = sf::Clock;
using Time  = sf::Time;

// ── Off-screen rendering ────────────────────────────────────
using RenderTexture = sf::RenderTexture;

// ── Angle helper (SFML 3) ───────────────────────────────────
inline auto degrees( float v ) { return sf::degrees( v ); }

}  // namespace platform

// ── Global convenience aliases ──────────────────────────────
// Existing code uses sf:: directly — keep compiling unchanged.
// New platform-agnostic code should use platform:: instead.
