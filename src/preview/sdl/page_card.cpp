#include "review_internal.hpp"

#include <algorithm>
#include <cmath>

namespace trimanga::preview {

void render_torn_edges(SDL_Renderer* renderer, const Rect& left, const Rect& right, double progress) {
  SDL_Color edge_color{224, 77, 105, static_cast<Uint8>(120 + 90 * progress)};
  SDL_Color paper_edge{255, 252, 246, static_cast<Uint8>(180 + 60 * progress)};
  const int step = 18;
  for (int y = left.y; y < left.y + left.h; y += step) {
    const int jag = ((y / step) % 2 == 0 ? 7 : -5);
    line(renderer, left.x + left.w - 1, y, left.x + left.w + jag, std::min(y + step, left.y + left.h), paper_edge);
    line(renderer, left.x + left.w + 2, y, left.x + left.w + jag + 2, std::min(y + step, left.y + left.h), edge_color);
  }
  for (int y = right.y; y < right.y + right.h; y += step) {
    const int jag = ((y / step) % 2 == 0 ? -7 : 5);
    line(renderer, right.x, y, right.x + jag, std::min(y + step, right.y + right.h), paper_edge);
    line(renderer, right.x - 2, y, right.x + jag - 2, std::min(y + step, right.y + right.h), edge_color);
  }
}

void render_page_card(SDL_Renderer* renderer, TextureCache& cache, std::vector<Candidate>& candidates,
                      std::size_t index, double offset, const Rect& content, double focus_pulse,
                      double badge_pulse, double delete_slide, double tear_progress, double shake) {
  const double distance = std::min(1.0, std::abs(offset));
  const double focus = smoothstep(1.0 - distance);
  const bool selected = focus > 0.72;
  const double delete_depth = ease_out(delete_slide);
  const double scale =
      (0.62 + 0.38 * focus + 0.018 * focus * focus_pulse) * (1.0 - 0.13 * delete_depth);
  TextureEntry* texture = cache.peek(index);
  const double aspect =
      (texture != nullptr && texture->width > 0 && texture->height > 0)
          ? static_cast<double>(texture->width) / static_cast<double>(texture->height)
          : 0.70;
  const int max_height = std::max(220, content.h - 22);
  const int max_width = std::max(210, static_cast<int>(content.w * (0.34 + 0.22 * focus)));
  int card_height = static_cast<int>(max_height * scale);
  int card_width = static_cast<int>(card_height * aspect);
  if (card_width > max_width) {
    card_width = max_width;
    card_height = static_cast<int>(card_width / std::max(0.2, aspect));
  }
  card_height = std::clamp(card_height, 220, max_height);
  card_width = std::clamp(card_width, 170, max_width);
  const int shake_x = static_cast<int>(std::sin(shake * 92.0 + index * 1.7) * shake);
  const int shake_y = static_cast<int>(std::cos(shake * 79.0 + index * 2.1) * shake * 0.55);
  const int base_top = content.y + (content.h - card_height) / 2 + static_cast<int>(distance * 18);
  const int delete_drop = static_cast<int>(std::round(delete_depth * (18.0 + 18.0 * focus)));
  const int center_x = content.x + content.w / 2 + static_cast<int>(offset * std::min(390, content.w / 3)) + shake_x;
  const int top = base_top + delete_drop + shake_y;
  const Rect card{center_x - card_width / 2, top, card_width, card_height};

  const double tear = ease_out(tear_progress);
  if (tear > 0.0) {
    const double fall = ease_in(std::clamp((tear_progress - 0.16) / 0.72, 0.0, 1.0));
    const int split = static_cast<int>(10 + tear * (content.w * 0.58));
    const int fall_left = static_cast<int>(fall * (content.h * 1.65));
    const int fall_right = static_cast<int>(fall * (content.h * 1.82));
    const int tilt = static_cast<int>(tear * 10);
    const Rect left_card{card.x - split, card.y + fall_left - tilt, card.w / 2, card.h};
    const Rect right_card{card.x + card.w / 2 + split, card.y + fall_right + tilt, card.w - card.w / 2, card.h};
    const bool left_visible = left_card.x + left_card.w >= 0 && left_card.y < content.y + content.h + 80;
    const bool right_visible = right_card.x <= content.x + content.w && right_card.y < content.y + content.h + 80;
    if (!left_visible && !right_visible) {
      return;
    }

    if (left_visible) {
      fill(renderer, Rect{left_card.x + 8, left_card.y + 10, left_card.w, left_card.h}, SDL_Color{116, 62, 88, 42});
      fill(renderer, left_card, kPaper);
    }
    if (right_visible) {
      fill(renderer, Rect{right_card.x + 8, right_card.y + 10, right_card.w, right_card.h}, SDL_Color{116, 62, 88, 42});
      fill(renderer, right_card, kPaper);
    }

    const Rect image_bounds{card.x + 12, card.y + 12, card.w - 24, card.h - 24};
    if (texture != nullptr && texture->texture != nullptr) {
      const Rect destination = fit_image(texture->width, texture->height, image_bounds);
      const int left_clip = std::clamp(card.x + card.w / 2 - destination.x, 0, destination.w);
      const int right_clip = destination.w - left_clip;
      if (left_clip > 0 && left_visible) {
        SDL_Rect source{0, 0, static_cast<int>((static_cast<double>(left_clip) / destination.w) * texture->width),
                        texture->height};
        SDL_Rect dest{left_card.x + std::max(0, destination.x - card.x), left_card.y + destination.y - card.y,
                      left_clip, destination.h};
        SDL_SetTextureAlphaMod(texture->texture, static_cast<Uint8>(255 * (1.0 - fall * 0.35)));
        SDL_RenderCopy(renderer, texture->texture, &source, &dest);
      }
      if (right_clip > 0 && right_visible) {
        const int source_x = static_cast<int>((static_cast<double>(left_clip) / destination.w) * texture->width);
        SDL_Rect source{source_x, 0, texture->width - source_x, texture->height};
        SDL_Rect dest{right_card.x + std::max(0, destination.x + left_clip - (card.x + card.w / 2)),
                      right_card.y + destination.y - card.y, right_clip, destination.h};
        SDL_SetTextureAlphaMod(texture->texture, static_cast<Uint8>(255 * (1.0 - fall * 0.35)));
        SDL_RenderCopy(renderer, texture->texture, &source, &dest);
      }
      SDL_SetTextureAlphaMod(texture->texture, selected ? 255 : 165);
    }
    if (left_visible) {
      outline(renderer, left_card, SDL_Color{230, 190, 207, 255});
    }
    if (right_visible) {
      outline(renderer, right_card, SDL_Color{230, 190, 207, 255});
    }
    if (left_visible || right_visible) {
      render_torn_edges(renderer, left_card, right_card, tear);
    }
    return;
  }

  SDL_Color backing = mix(SDL_Color{255, 239, 246, static_cast<Uint8>(230 - distance * 70)}, kPaper, focus);
  if (delete_depth > 0.0) {
    backing = mix(backing, SDL_Color{245, 219, 230, backing.a}, 0.28 * delete_depth);
  }
  const int shadow_offset =
      static_cast<int>(std::round(8.0 + 4.0 * focus + 4.0 * focus * focus_pulse + 12.0 * delete_depth));
  const int shadow_spread = static_cast<int>(std::round(delete_depth * (12.0 + 10.0 * focus)));
  const Uint8 shadow_alpha =
      static_cast<Uint8>(std::clamp(44.0 + 26.0 * focus + 54.0 * delete_depth, 0.0, 180.0));
  fill(renderer,
       Rect{card.x + shadow_offset - shadow_spread / 2, card.y + shadow_offset - shadow_spread / 2,
            card.w + shadow_spread, card.h + shadow_spread},
       SDL_Color{116, 62, 88, shadow_alpha});
  fill(renderer, card, backing);
  outline(renderer, card, selected ? action_color(candidates[index].review_action) : SDL_Color{230, 190, 207, 255});

  const Rect image_bounds{card.x + 12, card.y + 12, card.w - 24, card.h - 24};
  if (texture != nullptr && texture->texture != nullptr) {
    const Rect destination = fit_image(texture->width, texture->height, image_bounds);
    SDL_Rect sdl_destination{destination.x, destination.y, destination.w, destination.h};
    SDL_SetTextureAlphaMod(texture->texture, static_cast<Uint8>(165 + 90 * focus));
    SDL_RenderCopy(renderer, texture->texture, nullptr, &sdl_destination);
  } else {
    draw_centered_text(renderer, image_bounds, "LOADING", kMuted, selected ? 3 : 2);
  }

  render_action_badge(renderer, card, candidates[index].review_action, selected, selected ? badge_pulse : 0.0);
}

}  // namespace trimanga::preview
