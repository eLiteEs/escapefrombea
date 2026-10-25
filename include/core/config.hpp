#pragma once
#include "common.hpp"
#include <string>

struct KeyBindings {
  int up, down, left, right, menu;
  int interact;
  int drop;
};

struct Config {
  // Vídeo
  bool fullscreen = true;
  bool vsync = true;
  int fov = 72;
  bool showFps = false;

  // Audio
  float masterVol = 1.0f;
  float musicVol = 0.55f;
  float sfxVol = 0.90f;

  // Gameplay
  bool invertY = false;
  float mouseSens = 1.0f;
  float stickSens = 1.0f;
  bool showMinimap = false;
  bool headBob = true;
  float sprintDrain = C::STAM_DRAIN_DEF;
  float sprintRegen = C::STAM_REGEN_DEF;
  bool playCinematics = true;
  bool quakeMovement = true;
  bool autoBhop = false;
  // Idioma
  std::string language = "es";

  // Multijugador
  bool swapGamepads = false;

  // Progreso
  int maxLevel = 1;
  int maxEndlessLevel = 1;

  KeyBindings binds[2] = {
      {KEY_W, KEY_S, KEY_A, KEY_D, KEY_ESCAPE, KEY_F, KEY_Q},
      {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ESCAPE, KEY_SLASH,
       KEY_RIGHT_SHIFT}};

  void load();
  void save() const;
  float diffMult() const;
  const char *diffName() const;
  static const char *keyName(int key);

  // Vídeo (extendido)
  int windowW = 1280;
  int windowH = 720;
  int fpsLimit = 60;      // índice en C::FPS_OPTIONS
  int renderDistance = 1; // índice en C::RENDER_DIST
  int raycastRes = C::RC_RES_DEFAULT;
  bool useRenderer3D = true; // true = renderer 3D, false = raycaster clásico
  float guiScale = 1.0f;     // 0.75 .. 2.0

  // Accesibilidad
  int colorblindMode = CB_NONE;
  bool highContrast = false;
  bool reduceMotion = false;
  bool bigSubtitles = false;
  bool holdToSprint = true;
  bool noJumpscares = false;

  bool srShowGhost = true;

  // Overlay de speedrun
  int srOverlayAnchor =
      1;                 // 0..8 (3x3 grid: TL, TC, TR, ML, MC, MR, BL, BC, BR)
  int srOverlaySize = 1; // 0=small, 1=medium, 2=large
  int srOverlayOffsetX = 0; // -200..200 px (ajuste fino sobre el anchor)
  int srOverlayOffsetY = 0;
  bool srShowDelta = true;      // delta vs PB
  bool srShowTotal = true;      // tiempo total
  bool srShowPredicted = true;  // predicción
  bool srShowPrevSplits = true; // lista de niveles anteriores
  int srPrevSplitsCount = 5;    // cuántos mostrar (0..10)
  bool srShowPbFlash = true;    // "PB!" cuando bates split
};
