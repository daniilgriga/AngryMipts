// ============================================================
// platform.hpp — Unified platform abstraction include.
// Part of: angry::platform
//
// Selects active platform backend by build target:
//   * Includes SFML aliases on native builds
//   * Includes Raylib wrapper on Emscripten builds
//   * Keeps upper layers backend-agnostic
// ============================================================

#pragma once

#ifdef __EMSCRIPTEN__
#  include "platform_raylib.hpp"
#else
#  include "platform_sfml.hpp"
#endif
