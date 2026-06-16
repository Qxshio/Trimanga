#include "review_internal.hpp"

#include <SDL_syswm.h>

#import <AppKit/AppKit.h>

namespace trimanga::preview {

void order_out_native_window(SDL_Window* window) {
  if (window == nullptr) {
    return;
  }

  @autoreleasepool {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE) {
      return;
    }
    NSWindow* native_window = info.info.cocoa.window;
    if (native_window == nil) {
      return;
    }
    [native_window orderOut:nil];
    [native_window displayIfNeeded];
  }
}

void drain_native_window_events() {
  @autoreleasepool {
    for (int frame = 0; frame < 12; ++frame) {
      SDL_Event event;
      while (SDL_PollEvent(&event) != 0) {
      }
      SDL_PumpEvents();
      while (true) {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
        if (event == nil) {
          break;
        }
        [NSApp sendEvent:event];
      }
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                               beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.001]];
    }
  }
}

}  // namespace trimanga::preview
