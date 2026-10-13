#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

#include "game.h"

#if defined(_WIN32) && !defined(NDEBUG)
#define WIN32_LEAN_AND_MEAN // excluye winsock2, shellapi, etc.
#define NOGDI               // excluye wingdi.h (Rectangle, etc.)
#define NOUSER              // excluye winuser.h (CloseWindow, ShowCursor, etc.)
#include <windows.h>
#undef near
#undef far
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__EMSCRIPTEN__)
static void web_frame() {
  if (g_game)
    g_game->tick();
}
#endif

int main() {
#if defined(_WIN32) && !defined(NDEBUG)
  FreeConsole();
#endif

#if defined(NDEBUG) || !defined(_DEBUG)
  // Release: solo warnings y errores
  SetTraceLogLevel(LOG_WARNING);
#else
  // Debug: info normal
  SetTraceLogLevel(LOG_INFO);
#endif

  Game game;
  game.init();

#if defined(__EMSCRIPTEN__)
  emscripten_set_main_loop(web_frame, 0, 1);
  return 0;
#else
  game.run();
  game.shutdown();
  return 0;
#endif
}
