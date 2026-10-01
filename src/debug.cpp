#include "debug.h"
#include <cstdio>
#include <cmath>

namespace Debug {
    bool  showOverlay     = false;
    bool  showBounds      = false;
    bool  showVision      = false;
    bool  showPaths       = false;
    bool  noClip          = false;
    bool  godMode         = false;
    bool  infiniteStamina = false;
    float timeScale       = 1.0f;

    const char* version() { return C::GAME_VERSION; }

    const char* compilerName() {
        static char buf[64] = { 0 };
        if (buf[0]) return buf;
    #if defined(__clang__)
        snprintf(buf, sizeof(buf), "clang %d.%d.%d",
                 __clang_major__, __clang_minor__, __clang_patchlevel__);
    #elif defined(__GNUC__)
        snprintf(buf, sizeof(buf), "gcc %d.%d.%d",
                 __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    #elif defined(_MSC_VER)
        snprintf(buf, sizeof(buf), "msvc %d", _MSC_VER);
    #else
        snprintf(buf, sizeof(buf), "unknown");
    #endif
        return buf;
    }

    const char* buildString() {
        static char buf[64] = { 0 };
        if (buf[0]) return buf;
        snprintf(buf, sizeof(buf), "%s %s", __DATE__, __TIME__);
        return buf;
    }

    void cycleTimeScale() {
        static const float scales[] = { 1.0f, 0.5f, 0.25f, 0.1f, 0.05f };
        static const int N = 5;
        int cur = 0;
        for (int i = 0; i < N; ++i)
            if (fabsf(timeScale - scales[i]) < 0.001f) { cur = i; break; }
        cur = (cur + 1) % N;
        timeScale = scales[cur];
    }

    void resetCheats() {
        noClip          = false;
        godMode         = false;
        infiniteStamina = false;
        timeScale       = 1.0f;
    }
}

