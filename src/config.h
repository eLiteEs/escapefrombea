#pragma once
#include "common.h"
#include <string>

struct KeyBindings { int up, down, left, right, menu; };

struct Config {
    // Vídeo
    bool  fullscreen = true;
    bool  vsync      = true;
    int   fov        = 72;
    bool  showFps    = false;

    // Audio
    float masterVol  = 1.0f;
    float musicVol   = 0.55f;
    float sfxVol     = 0.90f;

    // Gameplay
    int   difficulty   = DIFF_NORMAL;
    bool  invertY      = false;
    float mouseSens    = 1.0f;
    float stickSens    = 1.0f;
    bool  showMinimap  = true;
    bool  headBob      = true;
    float sprintDrain  = C::STAM_DRAIN_DEF;
    float sprintRegen  = C::STAM_REGEN_DEF;
    bool  playCinematics = true;

    // Idioma
    std::string language = "es";

    // Multijugador
    bool  swapGamepads = false;

    // Progreso
    int   maxLevel        = 1;
    int   maxEndlessLevel = 1;

    KeyBindings binds[2] = {
        { KEY_W,  KEY_S,    KEY_A,     KEY_D,     KEY_ESCAPE },
        { KEY_UP, KEY_DOWN, KEY_LEFT,  KEY_RIGHT, KEY_ESCAPE }
    };

    void load();
    void save() const;
    float diffMult() const;
    const char* diffName() const;
    static const char* keyName(int key);

    // Vídeo (extendido)
    int   windowW = 1280;
    int   windowH = 720;
    int   fpsLimit = 60;         // índice en C::FPS_OPTIONS
    int   renderDistance = 1;    // índice en C::RENDER_DIST
    float guiScale = 1.0f;       // 0.75 .. 2.0

    // Accesibilidad
    int   colorblindMode = CB_NONE;
    bool  highContrast   = false;
    bool  reduceMotion   = false;
    bool  bigSubtitles   = false;
    bool  holdToSprint   = true;
};

