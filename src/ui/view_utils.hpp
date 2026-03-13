#pragma once

#include "platform/platform.hpp"

namespace angry
{

// Applies a centered letterbox viewport for the configured gameplay aspect ratio.
void apply_letterbox_view ( platform::View& view, platform::Vec2u window_size );

}  // namespace angry
