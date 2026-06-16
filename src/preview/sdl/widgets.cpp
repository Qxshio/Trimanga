#include "review_internal.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace trimanga::preview {

std::string action_name(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return "Kept";
    case ReviewAction::Delete:
      return "Marked for deletion";
    case ReviewAction::Undecided:
      return "Kept";
  }
  return "Kept";
}

void update_title(SDL_Window* window, const Candidate& candidate, std::size_t index, std::size_t total) {
  std::ostringstream title;
  title << "Review Detected Scanlations - " << (index + 1) << "/" << total << " - " << candidate.page.archive_name
        << " - " << action_name(candidate.review_action)
        << "   Wheel/Arrows/HJKL=Carousel  X/D=Toggle Delete  T=Toggle All  Hold Esc=Confirm";
  SDL_SetWindowTitle(window, title.str().c_str());
}

SDL_Color action_color(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return kAccent;
    case ReviewAction::Delete:
      return kDelete;
    case ReviewAction::Undecided:
      return kAccent;
  }
  return kAccent;
}

void render_action_badge(SDL_Renderer* renderer, const Rect& card, ReviewAction action, bool prominent, double pulse) {
  const int radius = (prominent ? 22 : 14) + static_cast<int>(8.0 * ease_out(pulse));
  const int cx = card.x + card.w - radius - (prominent ? 18 : 10);
  const int cy = card.y + radius + (prominent ? 18 : 10);
  const bool deleted = action == ReviewAction::Delete;
  fill_circle(renderer, cx, cy, radius, deleted ? mix(kDelete, kWarm, pulse * 0.25) : mix(kAccent, kWarm, pulse * 0.25));
  if (!deleted) {
    draw_check(renderer, cx, cy, kPaper);
  } else {
    draw_x(renderer, cx, cy, kPaper);
  }
}

void render_progress(SDL_Renderer* renderer, const std::vector<Candidate>& candidates, std::size_t index,
                     const Rect& rect) {
  const std::size_t total = candidates.size();
  if (total == 0) {
    return;
  }
  const Rect track{rect.x, rect.y, rect.w, 6};
  fill(renderer, track, SDL_Color{226, 195, 210, 140});

  const int gap = total <= static_cast<std::size_t>(track.w / 5) ? 2
                  : total <= static_cast<std::size_t>(track.w / 3) ? 1
                                                                   : 0;
  for (std::size_t page = 0; page < total; ++page) {
    const int x0 = track.x + static_cast<int>((static_cast<double>(page) / total) * track.w);
    const int x1 = track.x + static_cast<int>((static_cast<double>(page + 1) / total) * track.w);
    const int width = std::max(1, x1 - x0 - gap);
    const bool marked_delete = candidates[page].review_action == ReviewAction::Delete;
    SDL_Color color = marked_delete ? kDelete : SDL_Color{226, 195, 210, 255};
    int y = track.y;
    int height = track.h;
    if (marked_delete) {
      color = kDelete;
    } else if (page < index) {
      color = kAccent;
    } else if (page == index) {
      color = kWarm;
      y -= 2;
      height += 4;
    }
    fill(renderer, Rect{x0, y, width, height}, color);
  }
}

void render_button(SDL_Renderer* renderer, const Rect& rect, SDL_Color base, SDL_Color accent, const std::string& label,
                   bool active, bool hovered, bool pressed) {
  const int lift = hovered ? -2 : 0;
  const int press = pressed ? 3 : 0;
  const Rect animated{rect.x, rect.y + lift + press, rect.w, rect.h};
  const SDL_Color surface = active ? accent : mix(base, accent, hovered ? 0.18 : 0.0);
  fill(renderer, Rect{animated.x + 4, animated.y + 5, animated.w, animated.h}, SDL_Color{116, 62, 88, 36});
  fill(renderer, animated, surface);
  outline(renderer, animated, active || hovered ? mix(accent, kPanel, 0.20) : accent);
  draw_centered_text(renderer, Rect{animated.x + 42, animated.y, animated.w - 52, animated.h}, label,
                     active ? kLightText : kText, 2);
}

void render_manga_background(SDL_Renderer* renderer, int window_width, int window_height) {
  fill(renderer, Rect{0, 0, window_width, window_height}, kCanvas);
  for (int y = 22; y < window_height; y += 34) {
    for (int x = (y / 34) % 2 == 0 ? 16 : 33; x < window_width; x += 48) {
      fill_circle(renderer, x, y, 2, SDL_Color{236, 197, 214, 120});
    }
  }
  line(renderer, 0, 88, window_width, 18, SDL_Color{255, 222, 233, 120});
  line(renderer, 0, 128, window_width, 58, SDL_Color{216, 247, 250, 130});
  line(renderer, 0, window_height - 172, window_width, window_height - 228, SDL_Color{255, 222, 233, 120});
}

void render_header(SDL_Renderer* renderer, const Rect& header, std::size_t index, std::size_t total,
                   ReviewAction action, double alpha) {
  const SDL_Color panel = mix(kCanvas, kPanel, alpha);
  fill(renderer, header, panel);
  outline(renderer, header, SDL_Color{99, 78, 113, static_cast<Uint8>(255 * alpha)});

  draw_text(renderer, header.x + 18, header.y + 16, "REVIEW DETECTED SCANLATIONS", kLightText, 3);
  draw_text(renderer, header.x + 20, header.y + 50, "MARK PAGES FOR DELETION", SDL_Color{230, 205, 221, 255}, 2);

  const bool deleted = action == ReviewAction::Delete;
  const std::string status = deleted ? "MARKED DELETE" : "KEEP";
  const int status_width = text_width(status, 2) + 34;
  const std::string count = std::to_string(index + 1) + "/" + std::to_string(total);
  const int count_width = text_width(count, 2) + 34;
  const Rect count_badge{header.x + header.w - count_width - 22, header.y + 18, count_width, 42};
  const Rect status_badge{count_badge.x - status_width - 18, header.y + 20, status_width, 34};
  fill(renderer, status_badge, deleted ? kDelete : kAccent);
  draw_centered_text(renderer, status_badge, status, kLightText, 2);

  fill(renderer, count_badge, SDL_Color{255, 252, 246, 255});
  outline(renderer, count_badge, kAccent);
  draw_centered_text(renderer, count_badge, count, kText, 2);
}

void render_action_area(SDL_Renderer* renderer, const Rect& action, const Rect& delete_button,
                        const Rect& toggle_all_button, ReviewAction current_action, ButtonId hovered,
                        ButtonId pressed, double alpha) {
  fill(renderer, action, mix(kCanvas, kPaper, alpha));
  outline(renderer, action, SDL_Color{219, 166, 190, static_cast<Uint8>(255 * alpha)});
  const bool marked_delete = current_action == ReviewAction::Delete;

  render_button(renderer, delete_button, kDeleteSoft, kDelete, marked_delete ? "UNDO" : "DELETE", marked_delete,
                hovered == ButtonId::Delete, pressed == ButtonId::Delete);
  render_button(renderer, toggle_all_button, kAccentSoft, kAccent, "TOGGLE ALL", false,
                hovered == ButtonId::ToggleAll, pressed == ButtonId::ToggleAll);

  fill_circle(renderer, delete_button.x + 28, delete_button.y + 25, 15, kDelete);
  draw_x(renderer, delete_button.x + 28, delete_button.y + 25, kCanvas);
  draw_keycap(renderer, toggle_all_button.x + 14, toggle_all_button.y + 13, "T", kAccent);
}

void render_footer(SDL_Renderer* renderer, const Rect& footer) {
  fill(renderer, footer, SDL_Color{255, 252, 246, 218});
  outline(renderer, footer, SDL_Color{229, 190, 207, 255});
  const int y = footer.y + 10;
  const int esc_width = keycap_width("ESC");
  const int confirm_width = text_width("HOLD TO CONFIRM", 2);
  const int scroll_width = 28 + 6 + 28 + 12 + text_width("SCROLL", 2);
  const int toggle_width = 48 + 10 + keycap_width("X") + 12 + text_width("TOGGLE", 2);
  const int group_gap = 34;
  const int total_width = esc_width + 10 + confirm_width + group_gap + scroll_width + group_gap + toggle_width;
  int x = footer.x + std::max(12, (footer.w - total_width) / 2);

  draw_keycap(renderer, x, y, "ESC", kMuted);
  x += esc_width + 10;
  draw_text(renderer, x, y + 5, "HOLD TO CONFIRM", kMuted, 2);
  x += confirm_width + group_gap;
  draw_arrow_key(renderer, x, y, true, kMuted);
  x += 34;
  draw_arrow_key(renderer, x, y, false, kMuted);
  x += 40;
  draw_text(renderer, x, y + 5, "SCROLL", kMuted, 2);
  x += text_width("SCROLL", 2) + group_gap;
  draw_spacebar(renderer, Rect{x, y, 48, 24}, kMuted);
  x += 58;
  draw_keycap(renderer, x, y, "X", kMuted);
  x += keycap_width("X") + 12;
  draw_text(renderer, x, y + 5, "TOGGLE", kMuted, 2);
}

}  // namespace trimanga::preview
