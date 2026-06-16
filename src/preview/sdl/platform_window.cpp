#include "review_internal.hpp"

namespace trimanga::preview {

void order_out_native_window(SDL_Window*) {}

void drain_native_window_events() {
  for (int frame = 0; frame < 4; ++frame) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
    }
    SDL_PumpEvents();
    SDL_Delay(1);
  }
}

}  // namespace trimanga::preview
