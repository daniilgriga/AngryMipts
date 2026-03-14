// ============================================================
// view_utils.cpp — UI view helper implementation.
// Part of: angry::ui
//
// Implements viewport utilities shared by UI scenes:
//   * Applies pillarbox/letterbox depending on window ratio
//   * Preserves canonical gameplay world aspect
//   * Handles backend-specific viewport API differences
// ============================================================

#include "ui/view_utils.hpp"

#include "shared/world_config.hpp"

namespace angry
{

void apply_letterbox_view ( platform::View& view, platform::Vec2u window_size )
{
    if ( window_size.x == 0 || window_size.y == 0 )
        return;

    const float window_aspect =
        static_cast<float> ( window_size.x ) / static_cast<float> ( window_size.y );

    if ( window_aspect > world::kAspect )
    {
        const float width = world::kAspect / window_aspect;
        const float left = ( 1.f - width ) * 0.5f;
#ifndef __EMSCRIPTEN__
        view.setViewport ( sf::FloatRect ( {left, 0.f}, {width, 1.f} ) );
#else
        view.setViewport ( {left, 0.f, width, 1.f} );
#endif
    }
    else if ( window_aspect < world::kAspect )
    {
        const float height = window_aspect / world::kAspect;
        const float top = ( 1.f - height ) * 0.5f;
#ifndef __EMSCRIPTEN__
        view.setViewport ( sf::FloatRect ( {0.f, top}, {1.f, height} ) );
#else
        view.setViewport ( {0.f, top, 1.f, height} );
#endif
    }
    else
    {
#ifndef __EMSCRIPTEN__
        view.setViewport ( sf::FloatRect ( {0.f, 0.f}, {1.f, 1.f} ) );
#else
        view.setViewport ( {0.f, 0.f, 1.f, 1.f} );
#endif
    }
}

}  // namespace angry
