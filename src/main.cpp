#include "game.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static Game* g_game = nullptr;

#if defined(__EMSCRIPTEN__)
static void web_frame() {
    if (g_game) g_game->tick();
}
#endif

int main() {
#if defined(NDEBUG) || !defined(_DEBUG)
    // Release: solo warnings y errores
    SetTraceLogLevel(LOG_WARNING);
#else
    // Debug: info normal
    SetTraceLogLevel(LOG_INFO);
#endif
    
    Game game;
    g_game = &game;
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
