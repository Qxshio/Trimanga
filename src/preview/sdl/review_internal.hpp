#pragma once

#include "trimanga/previewer.hpp"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace trimanga::preview {

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

extern const SDL_Color kCanvas;
extern const SDL_Color kPaper;
extern const SDL_Color kPanel;
extern const SDL_Color kMuted;
extern const SDL_Color kText;
extern const SDL_Color kLightText;
extern const SDL_Color kDelete;
extern const SDL_Color kDeleteSoft;
extern const SDL_Color kAccent;
extern const SDL_Color kAccentSoft;
extern const SDL_Color kWarm;

void set_color(SDL_Renderer* renderer, SDL_Color color);
void fill(SDL_Renderer* renderer, const Rect& rect, SDL_Color color);
SDL_Color mix(SDL_Color from, SDL_Color to, double amount);
double ease_out(double value);
double ease_in(double value);
double smoothstep(double value);
Layout compute_layout(int window_width, int window_height);
void outline(SDL_Renderer* renderer, const Rect& rect, SDL_Color color);
void line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color);
void thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color, int thickness = 3);
void fill_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color);
void draw_check(SDL_Renderer* renderer, int cx, int cy, SDL_Color color);
void draw_x(SDL_Renderer* renderer, int cx, int cy, SDL_Color color);
void draw_spacebar(SDL_Renderer* renderer, const Rect& rect, SDL_Color color);
void draw_arrow_key(SDL_Renderer* renderer, int x, int y, bool left, SDL_Color color);
std::array<std::uint8_t, 7> glyph(char ch);
int text_width(const std::string& text, int scale);
void draw_text(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, int scale = 2);
void draw_centered_text(SDL_Renderer* renderer, const Rect& rect, const std::string& text, SDL_Color color, int scale);
void draw_keycap(SDL_Renderer* renderer, int x, int y, const std::string& label, SDL_Color color);
int keycap_width(const std::string& label);

class TextureCache {
 public:
  TextureCache(SDL_Renderer* renderer, std::vector<Candidate>& candidates);
  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;
  ~TextureCache();

  void clear();
  TextureEntry& get(std::size_t index);
  TextureEntry* peek(std::size_t index);
  void preload_near(std::size_t center, std::size_t radius);
  void keep_near(std::size_t center, std::size_t radius);

 private:
  SDL_Renderer* renderer_ = nullptr;
  std::vector<Candidate>& candidates_;
  std::vector<TextureEntry> entries_;
};

Rect fit_image(int image_width, int image_height, const Rect& bounds);
std::string action_name(ReviewAction action);
void update_title(SDL_Window* window, const Candidate& candidate, std::size_t index, std::size_t total);
SDL_Color action_color(ReviewAction action);
void render_action_badge(SDL_Renderer* renderer, const Rect& card, ReviewAction action, bool prominent, double pulse);
void render_torn_edges(SDL_Renderer* renderer, const Rect& left, const Rect& right, double progress);
void render_page_card(SDL_Renderer* renderer, TextureCache& cache, std::vector<Candidate>& candidates,
                      std::size_t index, double offset, const Rect& content, double focus_pulse,
                      double badge_pulse, double delete_slide, double tear_progress, double shake);
void render_progress(SDL_Renderer* renderer, const std::vector<Candidate>& candidates, std::size_t index,
                     const Rect& rect);
void render_button(SDL_Renderer* renderer, const Rect& rect, SDL_Color base, SDL_Color accent, const std::string& label,
                   bool active, bool hovered, bool pressed);
void render_manga_background(SDL_Renderer* renderer, int window_width, int window_height);
void render_header(SDL_Renderer* renderer, const Rect& header, std::size_t index, std::size_t total,
                   ReviewAction action, double alpha);
void render_action_area(SDL_Renderer* renderer, const Rect& action, const Rect& delete_button,
                        const Rect& toggle_all_button, ReviewAction current_action, ButtonId hovered,
                        ButtonId pressed, double alpha);
void render_footer(SDL_Renderer* renderer, const Rect& footer);
void update_logical_render_size(SDL_Window* window, SDL_Renderer* renderer, int& window_width, int& window_height);

}  // namespace trimanga::preview
