#include "review_internal.hpp"

#include <algorithm>

namespace trimanga::preview {

Layout compute_layout(int window_width, int window_height) {
  constexpr int margin = 24;
  constexpr int gap = 14;
  const int header_height = std::clamp(window_height / 9, 76, 104);
  const int footer_height = 44;
  const int action_height = std::clamp(window_height / 10, 84, 104);

  Layout layout;
  layout.header = Rect{margin, margin, window_width - margin * 2, header_height};
  layout.footer = Rect{margin, window_height - margin - footer_height, window_width - margin * 2, footer_height};
  layout.action =
      Rect{margin, layout.footer.y - gap - action_height, window_width - margin * 2, action_height};
  layout.content = Rect{margin, layout.header.y + layout.header.h + gap, window_width - margin * 2,
                        layout.action.y - (layout.header.y + layout.header.h + gap) - gap};
  layout.content.h = std::max(240, layout.content.h);
  return layout;
}

Rect fit_image(int image_width, int image_height, const Rect& bounds) {
  if (image_width <= 0 || image_height <= 0) {
    return bounds;
  }
  const double scale =
      std::min(static_cast<double>(bounds.w) / image_width, static_cast<double>(bounds.h) / image_height);
  const int width = std::max(1, static_cast<int>(image_width * scale));
  const int height = std::max(1, static_cast<int>(image_height * scale));
  return Rect{bounds.x + (bounds.w - width) / 2, bounds.y + (bounds.h - height) / 2, width, height};
}

void update_logical_render_size(SDL_Window* window, SDL_Renderer* renderer, int& window_width, int& window_height) {
  SDL_GetWindowSize(window, &window_width, &window_height);
  window_width = std::max(720, window_width);
  window_height = std::max(540, window_height);
  SDL_RenderSetLogicalSize(renderer, window_width, window_height);
}

}  // namespace trimanga::preview
