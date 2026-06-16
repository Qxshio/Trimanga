#include "review_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace trimanga::preview {

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

double ease_in(double value) {
  value = std::clamp(value, 0.0, 1.0);
  return value * value * value;
}

double smoothstep(double value) {
  value = std::clamp(value, 0.0, 1.0);
  return value * value * (3.0 - 2.0 * value);
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

void thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color, int thickness) {
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

void draw_spacebar(SDL_Renderer* renderer, const Rect& rect, SDL_Color color) {
  outline(renderer, rect, color);
  line(renderer, rect.x + 8, rect.y + rect.h - 8, rect.x + rect.w - 8, rect.y + rect.h - 8, color);
  line(renderer, rect.x + 8, rect.y + rect.h - 8, rect.x + 8, rect.y + rect.h - 14, color);
  line(renderer, rect.x + rect.w - 8, rect.y + rect.h - 8, rect.x + rect.w - 8, rect.y + rect.h - 14, color);
}

void draw_arrow_key(SDL_Renderer* renderer, int x, int y, bool left, SDL_Color color) {
  const Rect cap{x, y, 28, 24};
  fill(renderer, cap, SDL_Color{255, 239, 246, 255});
  outline(renderer, cap, color);
  const int cx = cap.x + cap.w / 2;
  const int cy = cap.y + cap.h / 2;
  if (left) {
    thick_line(renderer, cx - 7, cy, cx + 7, cy - 7, color, 2);
    thick_line(renderer, cx - 7, cy, cx + 7, cy + 7, color, 2);
  } else {
    thick_line(renderer, cx + 7, cy, cx - 7, cy - 7, color, 2);
    thick_line(renderer, cx + 7, cy, cx - 7, cy + 7, color, 2);
  }
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

void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, int scale) {
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

int keycap_width(const std::string& label) {
  return std::max(28, text_width(label, 2) + 12);
}

}  // namespace trimanga::preview
