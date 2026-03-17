// ============================================================
// view_utils.hpp — UI view and viewport helper declarations.
// Part of: angry::ui
//
// Declares reusable helpers for camera/view setup:
//   * Applies letterbox viewport for fixed world aspect ratio
//   * Keeps gameplay area proportions across window sizes
//   * Shared by scenes that render world-space content
// ============================================================

#pragma once

#include "platform/platform.hpp"

namespace angry
{

// Applies a centered letterbox viewport for the configured gameplay aspect ratio.
void apply_letterbox_view ( platform::View& view, platform::Vec2u window_size );

}  // namespace angry
