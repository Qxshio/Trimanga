#include "trimanga/previewer.hpp"

#include "trimanga/image_loader.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace trimanga {
namespace {

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool contains(int px, int py) const {
    return px >= x && px <= x + w && py >= y && py <= y + h;
  }
};

struct TextureEntry {
  SDL_Texture* texture = nullptr;
  int width = 0;
  int height = 0;
};

struct Layout {
  Rect header;
  Rect content;
  Rect action;
  Rect footer;
};

enum class ButtonId {
  None,
  Delete,
  ToggleAll,
};

constexpr SDL_Color kCanvas{255, 247, 250, 255};
constexpr SDL_Color kPaper{255, 252, 246, 255};
constexpr SDL_Color kPanel{48, 42, 61, 255};
constexpr SDL_Color kMuted{126, 103, 119, 255};
constexpr SDL_Color kText{54, 43, 60, 255};
constexpr SDL_Color kLightText{255, 250, 252, 255};
constexpr SDL_Color kDelete{224, 77, 105, 255};
constexpr SDL_Color kDeleteSoft{255, 222, 229, 255};
constexpr SDL_Color kAccent{62, 174, 195, 255};
constexpr SDL_Color kAccentSoft{216, 247, 250, 255};
constexpr SDL_Color kWarm{246, 177, 91, 255};

void set_color(SDL_Renderer* renderer, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill(SDL_Renderer* renderer, const Rect& rect, SDL_Color color) {
  SDL_Rect sdl{rect.x, rect.y, rect.w, rect.h};
  set_color(renderer, color);
  SDL_RenderFillRect(renderer, &sdl);
}

SDL_Color mix(SDL_Color from, SDL_Color to, double amount) {
  amount = std::clamp(amount, 0.0, 1.0);
  return SDL_Color{
      static_cast<Uint8>(from.r + (to.r - from.r) * amount),
      static_cast<Uint8>(from.g + (to.g - from.g) * amount),
      static_cast<Uint8>(from.b + (to.b - from.b) * amount),
      static_cast<Uint8>(from.a + (to.a - from.a) * amount),
  };
}

double ease_out(double value) {
  value = std::clamp(value, 0.0, 1.0);
  return 1.0 - std::pow(1.0 - value, 3.0);
}

Layout compute_layout(int window_width, int window_height) {
  constexpr int margin = 24;
  constexpr int gap = 14;
  const int header_height = std::clamp(window_height / 9, 76, 104);
  const int footer_height = 44;
  const int action_height = std::clamp(window_height / 8, 112, 132);

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

void outline(SDL_Renderer* renderer, const Rect& rect, SDL_Color color) {
  SDL_Rect sdl{rect.x, rect.y, rect.w, rect.h};
  set_color(renderer, color);
  SDL_RenderDrawRect(renderer, &sdl);
}

void line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
  set_color(renderer, color);
  SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color, int thickness = 3) {
  const int radius = std::max(1, thickness / 2);
  for (int offset = -radius; offset <= radius; ++offset) {
    line(renderer, x1 + offset, y1, x2 + offset, y2, color);
    line(renderer, x1, y1 + offset, x2, y2 + offset, color);
  }
}

void fill_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  set_color(renderer, color);
  for (int y = -radius; y <= radius; ++y) {
    const int half_width = static_cast<int>(std::sqrt(radius * radius - y * y));
    SDL_RenderDrawLine(renderer, cx - half_width, cy + y, cx + half_width, cy + y);
  }
}

void draw_check(SDL_Renderer* renderer, int cx, int cy, SDL_Color color) {
  thick_line(renderer, cx - 12, cy + 1, cx - 4, cy + 10, color, 4);
  thick_line(renderer, cx - 4, cy + 10, cx + 14, cy - 12, color, 4);
}

void draw_x(SDL_Renderer* renderer, int cx, int cy, SDL_Color color) {
  thick_line(renderer, cx - 12, cy - 12, cx + 12, cy + 12, color, 4);
  thick_line(renderer, cx + 12, cy - 12, cx - 12, cy + 12, color, 4);
}

void draw_arrow_pair(SDL_Renderer* renderer, int cx, int cy, SDL_Color color) {
  thick_line(renderer, cx - 12, cy, cx - 3, cy - 8, color, 2);
  thick_line(renderer, cx - 12, cy, cx - 3, cy + 8, color, 2);
  thick_line(renderer, cx + 12, cy, cx + 3, cy - 8, color, 2);
  thick_line(renderer, cx + 12, cy, cx + 3, cy + 8, color, 2);
}

std::array<std::uint8_t, 7> glyph(char ch) {
  switch (ch) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
    case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0F, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0F};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
}

int text_width(const std::string& text, int scale) {
  int width = 0;
  for (char ch : text) {
    width += (ch == ' ') ? 4 * scale : 6 * scale;
  }
  return width;
}

void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, int scale = 2) {
  set_color(renderer, color);
  int cursor = x;
  for (char raw : text) {
    const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
    if (ch == ' ') {
      cursor += 4 * scale;
      continue;
    }
    const auto rows = glyph(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((rows[static_cast<std::size_t>(row)] & (1 << (4 - col))) == 0) {
          continue;
        }
        SDL_Rect pixel{cursor + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    cursor += 6 * scale;
  }
}

void draw_centered_text(SDL_Renderer* renderer, const Rect& rect, const std::string& text, SDL_Color color, int scale) {
  draw_text(renderer, rect.x + (rect.w - text_width(text, scale)) / 2, rect.y + (rect.h - 7 * scale) / 2, text, color,
            scale);
}

void draw_keycap(SDL_Renderer* renderer, int x, int y, const std::string& label, SDL_Color color) {
  const int width = std::max(28, text_width(label, 2) + 12);
  const Rect cap{x, y, width, 24};
  fill(renderer, cap, SDL_Color{255, 239, 246, 255});
  outline(renderer, cap, color);
  draw_centered_text(renderer, cap, label, color, 2);
}

std::string action_name(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return "Keep";
    case ReviewAction::Delete:
      return "Delete";
    case ReviewAction::Undecided:
      return "Keep";
  }
  return "Keep";
}

SDL_Texture* load_texture(SDL_Renderer* renderer, const std::filesystem::path& path, int& width, int& height) {
  ColorImage image = load_color_image(path);
  if (!image.valid()) {
    width = 0;
    height = 0;
    return nullptr;
  }
  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) {
    width = 0;
    height = 0;
    return nullptr;
  }
  SDL_UpdateTexture(texture, nullptr, image.pixels.data(), image.width * static_cast<int>(sizeof(std::uint32_t)));
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  width = image.width;
  height = image.height;
  return texture;
}

class TextureCache {
 public:
  TextureCache(SDL_Renderer* renderer, std::vector<Candidate>& candidates)
      : renderer_(renderer), candidates_(candidates), entries_(candidates.size()) {}

  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;

  ~TextureCache() {
    for (TextureEntry& entry : entries_) {
      if (entry.texture != nullptr) {
        SDL_DestroyTexture(entry.texture);
      }
    }
  }

  TextureEntry& get(std::size_t index) {
    TextureEntry& entry = entries_[index];
    if (entry.texture == nullptr) {
      entry.texture = load_texture(renderer_, candidates_[index].page.image_path, entry.width, entry.height);
    }
    return entry;
  }

  TextureEntry* peek(std::size_t index) {
    if (index >= entries_.size() || entries_[index].texture == nullptr) {
      return nullptr;
    }
    return &entries_[index];
  }

  void preload_near(std::size_t center, std::size_t radius) {
    const std::size_t start = center > radius ? center - radius : 0;
    const std::size_t end = std::min(entries_.size(), center + radius + 1);
    for (std::size_t index = start; index < end; ++index) {
      get(index);
    }
    keep_near(center, radius + 3);
  }

  void keep_near(std::size_t center, std::size_t radius) {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const std::size_t distance = index > center ? index - center : center - index;
      if (distance <= radius || entries_[index].texture == nullptr) {
        continue;
      }
      SDL_DestroyTexture(entries_[index].texture);
      entries_[index] = {};
    }
  }

 private:
  SDL_Renderer* renderer_ = nullptr;
  std::vector<Candidate>& candidates_;
  std::vector<TextureEntry> entries_;
};

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

void update_title(SDL_Window* window, const Candidate& candidate, std::size_t index, std::size_t total) {
  std::ostringstream title;
  title << "Trimanga Review - " << (index + 1) << "/" << total << " - " << candidate.page.archive_name
        << " - " << action_name(candidate.review_action)
        << "   Wheel/Arrows/HJKL=Carousel  X/D=Toggle Delete  T=Toggle All  Esc=Confirm";
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

void render_page_card(SDL_Renderer* renderer, TextureCache& cache, std::vector<Candidate>& candidates,
                      std::size_t index, double offset, const Rect& content, double focus_pulse,
                      double badge_pulse) {
  const bool selected = std::abs(offset) < 0.15;
  const double distance = std::min(1.0, std::abs(offset));
  const double scale = (selected ? 1.0 : 0.72 - 0.10 * distance) + (selected ? 0.018 * focus_pulse : 0.0);
  TextureEntry* texture = cache.peek(index);
  const double aspect =
      (texture != nullptr && texture->width > 0 && texture->height > 0)
          ? static_cast<double>(texture->width) / static_cast<double>(texture->height)
          : 0.70;
  const int max_height = std::max(220, content.h - 22);
  const int max_width = std::max(210, static_cast<int>(content.w * (selected ? 0.56 : 0.34)));
  int card_height = static_cast<int>(max_height * scale);
  int card_width = static_cast<int>(card_height * aspect);
  if (card_width > max_width) {
    card_width = max_width;
    card_height = static_cast<int>(card_width / std::max(0.2, aspect));
  }
  card_height = std::clamp(card_height, 220, max_height);
  card_width = std::clamp(card_width, 170, max_width);
  const int center_x = content.x + content.w / 2 + static_cast<int>(offset * std::min(390, content.w / 3));
  const int top = content.y + (content.h - card_height) / 2 + static_cast<int>(distance * 18);
  const Rect card{center_x - card_width / 2, top, card_width, card_height};

  SDL_Color backing = selected ? kPaper : SDL_Color{255, 239, 246, static_cast<Uint8>(230 - distance * 70)};
  const int shadow_offset = selected ? 12 + static_cast<int>(4.0 * focus_pulse) : 8;
  fill(renderer, Rect{card.x + shadow_offset, card.y + shadow_offset, card.w, card.h},
       SDL_Color{116, 62, 88, static_cast<Uint8>(selected ? 70 : 44)});
  fill(renderer, card, backing);
  outline(renderer, card, selected ? action_color(candidates[index].review_action) : SDL_Color{230, 190, 207, 255});

  const Rect image_bounds{card.x + 12, card.y + 12, card.w - 24, card.h - 24};
  if (texture != nullptr && texture->texture != nullptr) {
    const Rect destination = fit_image(texture->width, texture->height, image_bounds);
    SDL_Rect sdl_destination{destination.x, destination.y, destination.w, destination.h};
    SDL_SetTextureAlphaMod(texture->texture, selected ? 255 : 165);
    SDL_RenderCopy(renderer, texture->texture, nullptr, &sdl_destination);
  } else {
    draw_centered_text(renderer, image_bounds, "LOADING", kMuted, selected ? 3 : 2);
  }

  render_action_badge(renderer, card, candidates[index].review_action, selected, selected ? badge_pulse : 0.0);
}

void render_progress(SDL_Renderer* renderer, std::size_t index, std::size_t total, const Rect& rect) {
  if (total == 0) {
    return;
  }
  const Rect track{rect.x, rect.y, rect.w, 6};
  fill(renderer, track, SDL_Color{226, 195, 210, 255});
  const int filled = static_cast<int>(track.w * (static_cast<double>(index + 1) / static_cast<double>(total)));
  fill(renderer, Rect{track.x, track.y, filled, track.h}, kAccent);
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

  draw_text(renderer, header.x + 18, header.y + 16, "TRIMANGA REVIEW", kLightText, 3);
  draw_text(renderer, header.x + 20, header.y + 50, "UNMARKED PAGES ARE KEPT", SDL_Color{230, 205, 221, 255}, 2);

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
  draw_text(renderer, action.x + 18, action.y + 18, marked_delete ? "CURRENT PAGE DELETE" : "CURRENT PAGE KEEP",
            marked_delete ? kDelete : kAccent, 2);
  if (delete_button.x > action.x + 430 && action.h >= 108) {
    draw_text(renderer, action.x + 18, action.y + 48, "TOGGLE ONLY PAGES YOU WANT REMOVED", kMuted, 2);
  }

  render_button(renderer, delete_button, kDeleteSoft, kDelete, marked_delete ? "UNDO" : "DELETE", marked_delete,
                hovered == ButtonId::Delete, pressed == ButtonId::Delete);
  render_button(renderer, toggle_all_button, kAccentSoft, kAccent, "TOGGLE ALL", false,
                hovered == ButtonId::ToggleAll, pressed == ButtonId::ToggleAll);

  fill_circle(renderer, delete_button.x + 28, delete_button.y + 25, 15, kDelete);
  draw_x(renderer, delete_button.x + 28, delete_button.y + 25, kCanvas);
  fill_circle(renderer, toggle_all_button.x + 28, toggle_all_button.y + 25, 15, kAccent);
  draw_arrow_pair(renderer, toggle_all_button.x + 28, toggle_all_button.y + 25, kPaper);
}

void render_footer(SDL_Renderer* renderer, const Rect& footer) {
  fill(renderer, footer, SDL_Color{255, 252, 246, 218});
  outline(renderer, footer, SDL_Color{229, 190, 207, 255});
  int x = footer.x + 14;
  const int y = footer.y + 10;
  draw_keycap(renderer, x, y, "ESC", kMuted);
  x += 48;
  draw_text(renderer, x, y + 5, "CONFIRM", kMuted, 2);
  x += 118;
  draw_arrow_pair(renderer, x + 12, y + 12, kMuted);
  x += 34;
  draw_text(renderer, x, y + 5, "SCROLL", kMuted, 2);

  int right = footer.x + footer.w - 358;
  draw_keycap(renderer, right, y, "SP", kMuted);
  right += 42;
  draw_keycap(renderer, right, y, "X", kDelete);
  right += 34;
  draw_text(renderer, right, y + 5, "TOGGLE", kMuted, 2);
  right += 98;
  draw_keycap(renderer, right, y, "T", kAccent);
  right += 34;
  draw_text(renderer, right, y + 5, "TOGGLE ALL", kMuted, 2);
}

void update_logical_render_size(SDL_Window* window, SDL_Renderer* renderer, int& window_width, int& window_height) {
  SDL_GetWindowSize(window, &window_width, &window_height);
  window_width = std::max(720, window_width);
  window_height = std::max(540, window_height);
  SDL_RenderSetLogicalSize(renderer, window_width, window_height);
}

}  // namespace

bool review_candidates(std::vector<Candidate>& candidates) {
  if (candidates.empty()) {
    return false;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  SDL_Window* window =
      SDL_CreateWindow("Trimanga Review", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1220, 860,
                       SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    SDL_Quit();
    return false;
  }
  SDL_SetWindowMinimumSize(window, 720, 540);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  std::size_t selected_index = 0;
  double carousel_position = 0.0;
  double entry_time = 0.0;
  double focus_pulse = 1.0;
  double badge_pulse = 0.0;
  int mouse_x = -1;
  int mouse_y = -1;
  ButtonId hovered_button = ButtonId::None;
  ButtonId pressed_button = ButtonId::None;
  double press_timer = 0.0;
  TextureCache cache(renderer, candidates);
  cache.preload_near(selected_index, 4);
  update_title(window, candidates[selected_index], selected_index, candidates.size());

  auto select = [&](std::size_t next) {
    selected_index = std::min(next, candidates.size() - 1);
    focus_pulse = 1.0;
    cache.preload_near(selected_index, 4);
    update_title(window, candidates[selected_index], selected_index, candidates.size());
  };
  auto toggle_delete = [&] {
    candidates[selected_index].review_action =
        candidates[selected_index].review_action == ReviewAction::Delete ? ReviewAction::Undecided : ReviewAction::Delete;
    badge_pulse = 1.0;
    update_title(window, candidates[selected_index], selected_index, candidates.size());
  };
  auto toggle_all = [&] {
    const bool all_deleted = std::all_of(candidates.begin(), candidates.end(), [](const Candidate& candidate) {
      return candidate.review_action == ReviewAction::Delete;
    });
    const ReviewAction next_action = all_deleted ? ReviewAction::Undecided : ReviewAction::Delete;
    for (Candidate& candidate : candidates) {
      candidate.review_action = next_action;
    }
    badge_pulse = 1.0;
    update_title(window, candidates[selected_index], selected_index, candidates.size());
  };

  bool running = true;
  Uint64 last_tick = SDL_GetPerformanceCounter();
  const double tick_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
          case SDLK_q:
            running = false;
            break;
          case SDLK_RIGHT:
          case SDLK_DOWN:
          case SDLK_l:
          case SDLK_j:
          case SDLK_n:
            if (selected_index + 1 < candidates.size()) {
              select(selected_index + 1);
            }
            break;
          case SDLK_LEFT:
          case SDLK_UP:
          case SDLK_h:
          case SDLK_k:
          case SDLK_p:
            if (selected_index > 0) {
              select(selected_index - 1);
            }
            break;
          case SDLK_d:
          case SDLK_x:
          case SDLK_DELETE:
          case SDLK_BACKSPACE:
          case SDLK_RETURN:
          case SDLK_SPACE:
            toggle_delete();
            break;
          case SDLK_t:
          case SDLK_a:
            toggle_all();
            break;
          default:
            break;
        }
      } else if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y < 0 && selected_index + 1 < candidates.size()) {
          select(selected_index + 1);
        } else if (event.wheel.y > 0 && selected_index > 0) {
          select(selected_index - 1);
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int window_width = 0;
        int window_height = 0;
        update_logical_render_size(window, renderer, window_width, window_height);
        const Layout layout = compute_layout(window_width, window_height);
        const int button_width = std::clamp((layout.action.w - 72) / 2, 160, 220);
        const int button_gap = 18;
        const int group_width = button_width * 2 + button_gap;
        const int button_x = layout.action.x + (layout.action.w - group_width) / 2;
        const int button_y = layout.action.y + layout.action.h - 62;
        const Rect delete_button{button_x, button_y, button_width, 50};
        const Rect toggle_all_button{button_x + button_width + button_gap, button_y, button_width, 50};
        const Rect previous_hit{layout.content.x, layout.content.y, layout.content.w / 3, layout.content.h};
        const Rect next_hit{layout.content.x + layout.content.w * 2 / 3, layout.content.y, layout.content.w / 3,
                            layout.content.h};
        mouse_x = event.button.x;
        mouse_y = event.button.y;
        if (delete_button.contains(mouse_x, mouse_y)) {
          pressed_button = ButtonId::Delete;
          press_timer = 0.12;
          toggle_delete();
        } else if (toggle_all_button.contains(mouse_x, mouse_y)) {
          pressed_button = ButtonId::ToggleAll;
          press_timer = 0.12;
          toggle_all();
        } else if (previous_hit.contains(mouse_x, mouse_y) && selected_index > 0) {
          select(selected_index - 1);
        } else if (next_hit.contains(mouse_x, mouse_y) && selected_index + 1 < candidates.size()) {
          select(selected_index + 1);
        }
      }
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const double delta_seconds = std::min(0.05, static_cast<double>(now - last_tick) / tick_frequency);
    last_tick = now;
    entry_time += delta_seconds;
    focus_pulse = std::max(0.0, focus_pulse - delta_seconds * 4.8);
    badge_pulse = std::max(0.0, badge_pulse - delta_seconds * 5.2);
    press_timer = std::max(0.0, press_timer - delta_seconds);
    if (press_timer <= 0.0) {
      pressed_button = ButtonId::None;
    }
    const double easing = 1.0 - std::exp(-18.0 * delta_seconds);
    carousel_position += (static_cast<double>(selected_index) - carousel_position) * easing;
    if (std::abs(carousel_position - static_cast<double>(selected_index)) < 0.0001) {
      carousel_position = static_cast<double>(selected_index);
    }

    int window_width = 0;
    int window_height = 0;
    update_logical_render_size(window, renderer, window_width, window_height);
    const Layout layout = compute_layout(window_width, window_height);
    const int button_width = std::clamp((layout.action.w - 72) / 2, 160, 220);
    const int button_gap = 18;
    const int group_width = button_width * 2 + button_gap;
    const int button_x = layout.action.x + (layout.action.w - group_width) / 2;
    const int button_y = layout.action.y + layout.action.h - 62;
    const Rect delete_button{button_x, button_y, button_width, 50};
    const Rect toggle_all_button{button_x + button_width + button_gap, button_y, button_width, 50};
    hovered_button = ButtonId::None;
    if (delete_button.contains(mouse_x, mouse_y)) {
      hovered_button = ButtonId::Delete;
    } else if (toggle_all_button.contains(mouse_x, mouse_y)) {
      hovered_button = ButtonId::ToggleAll;
    }
    const double header_alpha = ease_out(std::min(1.0, entry_time / 0.20));
    const double content_alpha = ease_out(std::min(1.0, std::max(0.0, entry_time - 0.05) / 0.22));
    const double action_alpha = ease_out(std::min(1.0, std::max(0.0, entry_time - 0.10) / 0.22));

    render_manga_background(renderer, window_width, window_height);
    render_header(renderer, layout.header, selected_index, candidates.size(), candidates[selected_index].review_action,
                  header_alpha);

    const int base = static_cast<int>(std::round(carousel_position));
    for (int offset = 3; offset >= -3; --offset) {
      const int candidate_index = base + offset;
      if (candidate_index < 0 || candidate_index >= static_cast<int>(candidates.size())) {
        continue;
      }
      render_page_card(renderer, cache, candidates, static_cast<std::size_t>(candidate_index),
                       static_cast<double>(candidate_index) - carousel_position, layout.content,
                       ease_out(focus_pulse) * content_alpha, ease_out(badge_pulse) * content_alpha);
    }

    const Rect progress{layout.action.x + 18, layout.action.y + layout.action.h - 16, layout.action.w - 36, 6};
    render_action_area(renderer, layout.action, delete_button, toggle_all_button, candidates[selected_index].review_action,
                       hovered_button, pressed_button, action_alpha);
    render_progress(renderer, selected_index, candidates.size(), progress);
    render_footer(renderer, layout.footer);

    SDL_RenderPresent(renderer);
    SDL_Delay(1);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return true;
}

}  // namespace trimanga
