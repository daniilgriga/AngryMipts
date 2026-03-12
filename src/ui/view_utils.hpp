#pragma once

#include <SFML/Graphics/View.hpp>

namespace angry
{

// Applies a centered letterbox viewport for the configured gameplay aspect ratio.
void apply_letterbox_view ( sf::View& view, sf::Vector2u window_size );

}  // namespace angry
