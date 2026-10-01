#pragma once
#include "raylib.h"
#include "common.h"

namespace Debug {
    // Overlay
    extern bool showOverlay;

    // Visualización 3D
    extern bool showBounds;
    extern bool showVision;
    extern bool showPaths;

    // Cheats
    extern bool  noClip;
    extern bool  godMode;
    extern bool  infiniteStamina;
    extern float timeScale;

    // Info
    const char* version();
    const char* buildString();
    const char* compilerName();

    // Cicla time scale: 1.0 -> 0.5 -> 0.25 -> 0.1 -> 0.05 -> 1.0
    void cycleTimeScale();

    // Reset de cheats al empezar partida
    void resetCheats();
}

