#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include "emulator.hpp"
#include "types.h"

#if defined(__unix__) || defined(__SWITCH__)
#include "emulator.h"
#include "oslib/oslib.h"

#include <csignal>

#if defined(__SWITCH__)
#include "nswitch.h"
#endif

#if defined(USE_SDL)
  #include "sdl/sdl.h"

#endif

void os_SetupInput() {
#if defined(USE_SDL)
  input_sdl_init();
#endif
}

void os_TermInput() {
#if defined(USE_SDL)
  input_sdl_quit();
#endif
}

void UpdateInputState() {
  #if defined(USE_SDL)
  // input_sdl_handle();
  #endif
}

void os_DoEvents() {
}

void os_SetWindowText(const char *text) {
  #if defined(USE_SDL)
  sdl_window_set_text(text);
  #endif
}

void os_CreateWindow() {
  #if defined(USE_SDL)
  sdl_window_create();
  #endif
}

void common_linux_setup();

int main(int argc, char *argv[]) {
#if defined(__SWITCH__)
  socketInitializeDefault();
  nxlinkStdio();
  //appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
#endif

#if defined(__unix__)
  common_linux_setup();
#endif

#if USE_SDL
  if (SDL_Init(SDL_INIT_EVENTS)) {
    return 0;
  }
#endif

  flycast::start(argc, argv);

  sen::emulator.inputThread();
}

#if defined(__unix__)
void os_DebugBreak() {
  raise(SIGTRAP);
}
#endif

#endif // __unix__ || __SWITCH__
