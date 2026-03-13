#pragma once

// ============================================================
// platform.hpp — single include for all platform abstractions.
// On native builds:  thin wrappers / aliases over SFML types.
// On Emscripten/web: Raylib-based implementations.
// ============================================================

#ifdef __EMSCRIPTEN__
#  include "platform_raylib.hpp"
#else
#  include "platform_sfml.hpp"
#endif
