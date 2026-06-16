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

constexpr SDL_Color kCanvas{13, 15, 19, 255};
constexpr SDL_Color kPanel{24, 27, 33, 255};
constexpr SDL_Color kPanelSoft{31, 35, 42, 255};
constexpr SDL_Color kStroke{67, 74, 86, 255};
constexpr SDL_Color kMuted{139, 146, 158, 255};
constexpr SDL_Color kText{235, 238, 242, 255};
constexpr SDL_Color kKeep{70, 190, 122, 255};
constexpr SDL_Color kKeepSoft{37, 92, 61, 255};
constexpr SDL_Color kDelete{229, 89, 89, 255};
constexpr SDL_Color kDeleteSoft{105, 47, 50, 255};
constexpr SDL_Color kAccent{129, 167, 255, 255};
constexpr SDL_Color kWarm{229, 178, 95, 255};

void set_color(SDL_Renderer* renderer, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill(SDL_Renderer* renderer, const Rect& rect, SDL_Color color) {
  SDL_Rect sdl{rect.x, rect.y, rect.w, rect.h};
  set_color(renderer, color);
  SDL_RenderFillRect(renderer, &sdl);
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

std::string action_name(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return "Keep";
    case ReviewAction::Delete:
      return "Delete";
    case ReviewAction::Undecided:
      return "Review";
  }
  return "Review";
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
        << "   Wheel/Arrows/HJKL=Carousel  Enter/Y=Keep  X/D=Delete  Esc=Close";
  SDL_SetWindowTitle(window, title.str().c_str());
}

SDL_Color action_color(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return kKeep;
    case ReviewAction::Delete:
      return kDelete;
    case ReviewAction::Undecided:
      return kWarm;
  }
  return kWarm;
}

void render_action_badge(SDL_Renderer* renderer, const Rect& card, ReviewAction action, bool prominent) {
  if (action == ReviewAction::Undecided) {
    return;
  }
  const int radius = prominent ? 22 : 14;
  const int cx = card.x + card.w - radius - (prominent ? 18 : 10);
  const int cy = card.y + radius + (prominent ? 18 : 10);
  fill_circle(renderer, cx, cy, radius, action == ReviewAction::Keep ? kKeep : kDelete);
  if (action == ReviewAction::Keep) {
    draw_check(renderer, cx, cy, kCanvas);
  } else {
    draw_x(renderer, cx, cy, kCanvas);
  }
}

void render_page_card(SDL_Renderer* renderer, TextureCache& cache, std::vector<Candidate>& candidates,
                      std::size_t index, double offset, int window_width, int window_height) {
  const bool selected = std::abs(offset) < 0.15;
  const double depth = std::min(1.0, std::abs(offset) / 2.0);
  const double scale = selected ? 1.0 : 0.74 - 0.08 * std::min(1.0, std::abs(offset));
  const int max_height = std::max(260, window_height - 250);
  const int card_height = static_cast<int>(max_height * scale);
  const int card_width = static_cast<int>(std::min<double>(window_width * (selected ? 0.54 : 0.34), card_height * 0.72));
  const int center_x = window_width / 2 + static_cast<int>(offset * std::min(360, window_width / 3));
  const int top = 90 + static_cast<int>(std::abs(offset) * 26);
  const Rect card{center_x - card_width / 2, top, card_width, card_height};

  SDL_Color backing = selected ? kPanelSoft : SDL_Color{22, 25, 30, static_cast<Uint8>(230 - depth * 80)};
  fill(renderer, Rect{card.x + 10, card.y + 12, card.w, card.h}, SDL_Color{4, 5, 7, 100});
  fill(renderer, card, backing);
  outline(renderer, card, selected ? action_color(candidates[index].review_action) : kStroke);

  TextureEntry& texture = cache.get(index);
  const Rect image_bounds{card.x + 14, card.y + 14, card.w - 28, card.h - 28};
  if (texture.texture != nullptr) {
    const Rect destination = fit_image(texture.width, texture.height, image_bounds);
    SDL_Rect sdl_destination{destination.x, destination.y, destination.w, destination.h};
    SDL_SetTextureAlphaMod(texture.texture, selected ? 255 : 150);
    SDL_RenderCopy(renderer, texture.texture, nullptr, &sdl_destination);
  } else {
    draw_centered_text(renderer, image_bounds, "IMAGE FAILED", kDelete, selected ? 3 : 2);
  }

  render_action_badge(renderer, card, candidates[index].review_action, selected);
}

void render_progress(SDL_Renderer* renderer, std::size_t index, std::size_t total, int window_width, int window_height) {
  if (total == 0) {
    return;
  }
  const int width = std::max(160, window_width - 360);
  const Rect track{180, window_height - 118, width, 6};
  fill(renderer, track, SDL_Color{51, 56, 66, 255});
  const int filled = static_cast<int>(track.w * (static_cast<double>(index + 1) / static_cast<double>(total)));
  fill(renderer, Rect{track.x, track.y, filled, track.h}, kAccent);
}

void render_button(SDL_Renderer* renderer, const Rect& rect, SDL_Color base, SDL_Color accent, const std::string& label,
                   bool active) {
  fill(renderer, rect, active ? accent : base);
  outline(renderer, rect, accent);
  draw_centered_text(renderer, Rect{rect.x + 42, rect.y, rect.w - 52, rect.h}, label, kText, 2);
}

}  // namespace

bool review_candidates(std::vector<Candidate>& candidates) {
  if (candidates.empty()) {
    return false;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return false;
  }

  SDL_Window* window = SDL_CreateWindow("Trimanga Review", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1220, 860,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == nullptr) {
    SDL_Quit();
    return false;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  std::size_t selected_index = 0;
  double carousel_position = 0.0;
  TextureCache cache(renderer, candidates);
  update_title(window, candidates[selected_index], selected_index, candidates.size());

  auto select = [&](std::size_t next) {
    selected_index = std::min(next, candidates.size() - 1);
    cache.keep_near(selected_index, 3);
    update_title(window, candidates[selected_index], selected_index, candidates.size());
  };
  auto mark = [&](ReviewAction action) {
    candidates[selected_index].review_action = action;
    if (selected_index + 1 < candidates.size()) {
      select(selected_index + 1);
    } else {
      update_title(window, candidates[selected_index], selected_index, candidates.size());
    }
  };

  bool running = true;
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
          case SDLK_SPACE:
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
          case SDLK_RETURN:
            mark(ReviewAction::Keep);
            break;
          case SDLK_y:
            mark(ReviewAction::Keep);
            break;
          case SDLK_d:
          case SDLK_x:
          case SDLK_DELETE:
          case SDLK_BACKSPACE:
            mark(ReviewAction::Delete);
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
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        const Rect keep_button{window_width / 2 - 250, window_height - 78, 220, 50};
        const Rect delete_button{window_width / 2 + 30, window_height - 78, 220, 50};
        const Rect previous_hit{0, 80, window_width / 3, window_height - 190};
        const Rect next_hit{window_width * 2 / 3, 80, window_width / 3, window_height - 190};
        if (keep_button.contains(event.button.x, event.button.y)) {
          mark(ReviewAction::Keep);
        } else if (delete_button.contains(event.button.x, event.button.y)) {
          mark(ReviewAction::Delete);
        } else if (previous_hit.contains(event.button.x, event.button.y) && selected_index > 0) {
          select(selected_index - 1);
        } else if (next_hit.contains(event.button.x, event.button.y) && selected_index + 1 < candidates.size()) {
          select(selected_index + 1);
        }
      }
    }

    carousel_position += (static_cast<double>(selected_index) - carousel_position) * 0.18;
    if (std::abs(carousel_position - static_cast<double>(selected_index)) < 0.001) {
      carousel_position = static_cast<double>(selected_index);
    }

    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);

    fill(renderer, Rect{0, 0, window_width, window_height}, kCanvas);
    fill(renderer, Rect{0, 0, window_width, 74}, kPanel);
    draw_text(renderer, 28, 20, "TRIMANGA REVIEW", kText, 3);
    draw_text(renderer, 28, 50, "WHEEL OR ARROWS TO SCROLL", kMuted, 2);

    const std::string count = std::to_string(selected_index + 1) + "/" + std::to_string(candidates.size());
    draw_text(renderer, window_width - text_width(count, 3) - 28, 22, count, kAccent, 3);

    const int base = static_cast<int>(std::floor(carousel_position));
    for (int offset = 3; offset >= -3; --offset) {
      const int candidate_index = base + offset;
      if (candidate_index < 0 || candidate_index >= static_cast<int>(candidates.size())) {
        continue;
      }
      render_page_card(renderer, cache, candidates, static_cast<std::size_t>(candidate_index),
                       static_cast<double>(candidate_index) - carousel_position, window_width, window_height);
    }

    fill(renderer, Rect{0, window_height - 132, window_width, 132}, kPanel);
    render_progress(renderer, selected_index, candidates.size(), window_width, window_height);

    const Rect keep_button{window_width / 2 - 250, window_height - 78, 220, 50};
    const Rect delete_button{window_width / 2 + 30, window_height - 78, 220, 50};
    render_button(renderer, keep_button, kKeepSoft, kKeep, "KEEP", candidates[selected_index].review_action == ReviewAction::Keep);
    render_button(renderer, delete_button, kDeleteSoft, kDelete, "DELETE",
                  candidates[selected_index].review_action == ReviewAction::Delete);
    fill_circle(renderer, keep_button.x + 28, keep_button.y + 25, 15, kKeep);
    draw_check(renderer, keep_button.x + 28, keep_button.y + 25, kCanvas);
    fill_circle(renderer, delete_button.x + 28, delete_button.y + 25, 15, kDelete);
    draw_x(renderer, delete_button.x + 28, delete_button.y + 25, kCanvas);

    draw_text(renderer, 28, window_height - 46, "ESC CLOSE", kMuted, 2);
    draw_text(renderer, window_width - 312, window_height - 46, "ENTER KEEP  X DELETE", kMuted, 2);

    SDL_RenderPresent(renderer);
    SDL_Delay(8);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return true;
}

}  // namespace trimanga
