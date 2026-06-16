#include "trimanga/previewer.hpp"

#include "trimanga/image_loader.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>

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

constexpr SDL_Color kBackground{18, 20, 24, 255};
constexpr SDL_Color kPanel{28, 31, 36, 255};
constexpr SDL_Color kMuted{138, 145, 154, 255};
constexpr SDL_Color kText{232, 235, 239, 255};
constexpr SDL_Color kKeep{57, 178, 110, 255};
constexpr SDL_Color kDelete{221, 85, 85, 255};
constexpr SDL_Color kUndecided{88, 95, 105, 255};

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

std::string action_name(ReviewAction action) {
  switch (action) {
    case ReviewAction::Keep:
      return "Keep";
    case ReviewAction::Delete:
      return "Delete";
    case ReviewAction::Undecided:
      return "Undecided";
  }
  return "Undecided";
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
    return nullptr;
  }
  SDL_UpdateTexture(texture, nullptr, image.pixels.data(), image.width * static_cast<int>(sizeof(std::uint32_t)));
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  width = image.width;
  height = image.height;
  return texture;
}

Rect fit_image(int image_width, int image_height, int window_width, int window_height) {
  const Rect bounds{24, 72, std::max(1, window_width - 48), std::max(1, window_height - 162)};
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
        << " - " << action_name(candidate.review_action) << "   K=Keep  D=Delete  Arrows/Wheel=Navigate  Esc=Close";
  SDL_SetWindowTitle(window, title.str().c_str());
}

}  // namespace

bool review_candidates(std::vector<Candidate>& candidates) {
  if (candidates.empty()) {
    return false;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return false;
  }

  SDL_Window* window = SDL_CreateWindow("Trimanga Review", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1120, 820,
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

  std::size_t index = 0;
  SDL_Texture* texture = nullptr;
  int texture_width = 0;
  int texture_height = 0;
  auto reload = [&] {
    if (texture != nullptr) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
    texture = load_texture(renderer, candidates[index].page.image_path, texture_width, texture_height);
    update_title(window, candidates[index], index, candidates.size());
  };
  auto move_to = [&](std::size_t next) {
    index = std::min(next, candidates.size() - 1);
    reload();
  };
  auto advance = [&] {
    if (index + 1 < candidates.size()) {
      move_to(index + 1);
    } else {
      update_title(window, candidates[index], index, candidates.size());
    }
  };
  auto mark = [&](ReviewAction action) {
    candidates[index].review_action = action;
    advance();
  };
  reload();

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
          case SDLK_n:
          case SDLK_SPACE:
            if (index + 1 < candidates.size()) {
              move_to(index + 1);
            }
            break;
          case SDLK_LEFT:
          case SDLK_UP:
          case SDLK_p:
            if (index > 0) {
              move_to(index - 1);
            }
            break;
          case SDLK_k:
            mark(ReviewAction::Keep);
            break;
          case SDLK_d:
          case SDLK_DELETE:
          case SDLK_BACKSPACE:
            mark(ReviewAction::Delete);
            break;
          default:
            break;
        }
      } else if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y < 0 && index + 1 < candidates.size()) {
          move_to(index + 1);
        } else if (event.wheel.y > 0 && index > 0) {
          move_to(index - 1);
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        const int button_width = std::max(160, (window_width - 88) / 2);
        const Rect keep_button{24, window_height - 70, button_width, 46};
        const Rect delete_button{window_width - button_width - 24, window_height - 70, button_width, 46};
        if (keep_button.contains(event.button.x, event.button.y)) {
          mark(ReviewAction::Keep);
        } else if (delete_button.contains(event.button.x, event.button.y)) {
          mark(ReviewAction::Delete);
        }
      }
    }

    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    set_color(renderer, kBackground);
    SDL_RenderClear(renderer);

    fill(renderer, Rect{0, 0, window_width, 54}, kPanel);
    draw_text(renderer, 24, 18,
              std::to_string(index + 1) + "/" + std::to_string(candidates.size()) + " " +
                  action_name(candidates[index].review_action),
              kText, 2);
    draw_text(renderer, std::max(24, window_width - 280), 20, "ESC CLOSE", kMuted, 2);

    const Rect image_rect = fit_image(texture_width, texture_height, window_width, window_height);
    if (texture != nullptr) {
      SDL_Rect destination{image_rect.x, image_rect.y, image_rect.w, image_rect.h};
      SDL_RenderCopy(renderer, texture, nullptr, &destination);
      outline(renderer, image_rect, kUndecided);
    } else {
      draw_text(renderer, 24, window_height / 2, "IMAGE COULD NOT LOAD", kDelete, 3);
    }

    fill(renderer, Rect{0, window_height - 90, window_width, 90}, kPanel);
    const int button_width = std::max(160, (window_width - 88) / 2);
    const Rect keep_button{24, window_height - 70, button_width, 46};
    const Rect delete_button{window_width - button_width - 24, window_height - 70, button_width, 46};
    fill(renderer, keep_button, candidates[index].review_action == ReviewAction::Keep ? kKeep : SDL_Color{38, 92, 62, 255});
    fill(renderer, delete_button,
         candidates[index].review_action == ReviewAction::Delete ? kDelete : SDL_Color{105, 48, 48, 255});
    outline(renderer, keep_button, kKeep);
    outline(renderer, delete_button, kDelete);
    draw_text(renderer, keep_button.x + 24, keep_button.y + 16, "KEEP", kText, 2);
    draw_text(renderer, delete_button.x + 24, delete_button.y + 16, "DELETE", kText, 2);

    SDL_RenderPresent(renderer);
    SDL_Delay(8);
  }

  if (texture != nullptr) {
    SDL_DestroyTexture(texture);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return true;
}

}  // namespace trimanga
