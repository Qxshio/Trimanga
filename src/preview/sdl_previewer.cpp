#include "trimanga/previewer.hpp"

#include "sdl/review_internal.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace trimanga {

namespace {

void drain_destroy_events() {
  for (int frame = 0; frame < 12; ++frame) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
    }
    SDL_PumpEvents();
    SDL_Delay(1);
  }
}

}  // namespace

bool review_candidates(std::vector<Candidate>& candidates) {
  using namespace preview;

  if (candidates.empty()) {
    return false;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return false;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  SDL_Window* window =
      SDL_CreateWindow("Review Detected Scanlations", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1220, 860,
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
  bool escape_held = false;
  double escape_hold_time = 0.0;
  bool confirming = false;
  bool window_closed = false;
  double confirm_time = 0.0;
  std::vector<double> delete_slides(candidates.size(), 0.0);
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
  auto start_confirm = [&] {
    confirming = true;
    confirm_time = 0.0;
    escape_held = false;
    escape_hold_time = 0.0;
    hovered_button = ButtonId::None;
    pressed_button = ButtonId::None;
    SDL_SetWindowTitle(window, "Review Detected Scanlations - confirming selection");
  };
  auto close_preview_now = [&] {
    if (window_closed) {
      return;
    }
    window_closed = true;
    cache.clear();
    if (renderer != nullptr) {
      SDL_DestroyRenderer(renderer);
      renderer = nullptr;
    }
    if (window != nullptr) {
      SDL_DestroyWindow(window);
      window = nullptr;
    }
    drain_destroy_events();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
  };

  bool running = true;
  Uint64 last_tick = SDL_GetPerformanceCounter();
  const double tick_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (confirming) {
        continue;
      } else if (event.type == SDL_KEYDOWN) {
        if (escape_held && event.key.keysym.sym != SDLK_ESCAPE) {
          continue;
        }
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            if (event.key.repeat == 0) {
              escape_held = true;
              escape_hold_time = 0.0;
              SDL_SetWindowTitle(window, "Review Detected Scanlations - hold Esc to confirm");
            }
            break;
          case SDLK_q:
            escape_held = true;
            escape_hold_time = 1.0;
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
        if (escape_held) {
          continue;
        }
        if (event.wheel.y < 0 && selected_index + 1 < candidates.size()) {
          select(selected_index + 1);
        } else if (event.wheel.y > 0 && selected_index > 0) {
          select(selected_index - 1);
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        mouse_x = event.motion.x;
        mouse_y = event.motion.y;
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (escape_held) {
          continue;
        }
        int window_width = 0;
        int window_height = 0;
        update_logical_render_size(window, renderer, window_width, window_height);
        const Layout layout = compute_layout(window_width, window_height);
        const int button_width = std::clamp((layout.action.w - 72) / 2, 160, 220);
        const int button_gap = 18;
        const int group_width = button_width * 2 + button_gap;
        const int button_x = layout.action.x + (layout.action.w - group_width) / 2;
        const int button_y = layout.action.y + 28;
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
      } else if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) {
        escape_held = false;
        escape_hold_time = 0.0;
        update_title(window, candidates[selected_index], selected_index, candidates.size());
      }
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const double delta_seconds = std::min(0.05, static_cast<double>(now - last_tick) / tick_frequency);
    last_tick = now;
    entry_time += delta_seconds;
    if (escape_held && !confirming) {
      escape_hold_time += delta_seconds;
      if (escape_hold_time >= 1.0) {
        start_confirm();
        close_preview_now();
        return true;
      }
    }
    if (confirming) {
      confirm_time += delta_seconds;
      if (confirm_time >= 0.48) {
        close_preview_now();
        return true;
      }
    }
    focus_pulse = std::max(0.0, focus_pulse - delta_seconds * 4.8);
    badge_pulse = std::max(0.0, badge_pulse - delta_seconds * 5.2);
    press_timer = std::max(0.0, press_timer - delta_seconds);
    if (press_timer <= 0.0) {
      pressed_button = ButtonId::None;
    }
    const double easing = 1.0 - std::exp(-10.0 * delta_seconds);
    carousel_position += (static_cast<double>(selected_index) - carousel_position) * easing;
    if (std::abs(carousel_position - static_cast<double>(selected_index)) < 0.0001) {
      carousel_position = static_cast<double>(selected_index);
    }
    const double delete_easing = 1.0 - std::exp(-16.0 * delta_seconds);
    for (std::size_t index = 0; index < candidates.size(); ++index) {
      const double target = candidates[index].review_action == ReviewAction::Delete ? 1.0 : 0.0;
      delete_slides[index] += (target - delete_slides[index]) * delete_easing;
      if (std::abs(delete_slides[index] - target) < 0.002) {
        delete_slides[index] = target;
      }
    }

    int window_width = 0;
    int window_height = 0;
    update_logical_render_size(window, renderer, window_width, window_height);
    const Layout layout = compute_layout(window_width, window_height);
    const int button_width = std::clamp((layout.action.w - 72) / 2, 160, 220);
    const int button_gap = 18;
    const int group_width = button_width * 2 + button_gap;
    const int button_x = layout.action.x + (layout.action.w - group_width) / 2;
    const int button_y = layout.action.y + 28;
    const Rect delete_button{button_x, button_y, button_width, 50};
    const Rect toggle_all_button{button_x + button_width + button_gap, button_y, button_width, 50};
    hovered_button = ButtonId::None;
    if (!confirming && !escape_held && delete_button.contains(mouse_x, mouse_y)) {
      hovered_button = ButtonId::Delete;
    } else if (!confirming && !escape_held && toggle_all_button.contains(mouse_x, mouse_y)) {
      hovered_button = ButtonId::ToggleAll;
    }
    const double header_alpha = ease_out(std::min(1.0, entry_time / 0.20));
    const double content_alpha = ease_out(std::min(1.0, std::max(0.0, entry_time - 0.05) / 0.22));
    const double action_alpha = ease_out(std::min(1.0, std::max(0.0, entry_time - 0.10) / 0.22));

    render_manga_background(renderer, window_width, window_height);
    render_header(renderer, layout.header, selected_index, candidates.size(), candidates[selected_index].review_action,
                  header_alpha);

    const double gather_progress = confirming ? ease_out(std::min(1.0, confirm_time / 0.16)) : 0.0;
    const double tear_progress = confirming ? std::clamp((confirm_time - 0.12) / 0.32, 0.0, 1.0) : 0.0;
    const double hold_progress = escape_held && !confirming ? std::clamp(escape_hold_time / 1.0, 0.0, 1.0) : 0.0;
    const double shake = hold_progress * 5.0;
    const int base = static_cast<int>(std::round(carousel_position));
    const SDL_Rect content_clip{layout.content.x, layout.content.y, layout.content.w, layout.content.h};
    SDL_RenderSetClipRect(renderer, &content_clip);
    for (int offset = 3; offset >= -3; --offset) {
      const int candidate_index = base + offset;
      if (candidate_index < 0 || candidate_index >= static_cast<int>(candidates.size())) {
        continue;
      }
      if (candidate_index == static_cast<int>(selected_index)) {
        continue;
      }
      const double visual_offset = (static_cast<double>(candidate_index) - carousel_position) * (1.0 - gather_progress);
      const double delete_slide = delete_slides[static_cast<std::size_t>(candidate_index)] * (1.0 - gather_progress);
      render_page_card(renderer, cache, candidates, static_cast<std::size_t>(candidate_index),
                       visual_offset, layout.content,
                       ease_out(focus_pulse) * content_alpha, ease_out(badge_pulse) * content_alpha,
                       delete_slide, tear_progress,
                       shake * delete_slides[static_cast<std::size_t>(candidate_index)]);
    }
    const double active_offset = (static_cast<double>(selected_index) - carousel_position) * (1.0 - gather_progress);
    const double active_delete_slide = delete_slides[selected_index] * (1.0 - gather_progress);
    render_page_card(renderer, cache, candidates, selected_index, active_offset, layout.content,
                     ease_out(focus_pulse) * content_alpha, ease_out(badge_pulse) * content_alpha,
                     active_delete_slide, tear_progress, shake * delete_slides[selected_index]);
    SDL_RenderSetClipRect(renderer, nullptr);

    const Rect progress{layout.action.x + 18, layout.action.y + 12, layout.action.w - 36, 6};
    render_action_area(renderer, layout.action, delete_button, toggle_all_button, candidates[selected_index].review_action,
                       hovered_button, pressed_button, action_alpha);
    render_progress(renderer, candidates, selected_index, progress);
    render_footer(renderer, layout.footer);

    SDL_RenderPresent(renderer);
  }

  close_preview_now();
  return true;
}

}  // namespace trimanga
