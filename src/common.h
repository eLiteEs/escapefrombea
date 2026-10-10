#pragma once
#include "raylib.h"
#include "raymath.h"

namespace C {
// Mundo
constexpr float CELL = 4.0f;
constexpr float WALL_H = 3.6f;
constexpr float EYE_H = 1.7f;

// Jugador
constexpr float P_SPEED = 6.5f;
constexpr float P_SPRINT = 1.55f;
constexpr float P_RADIUS = 0.45f;
constexpr float STEP_FAST = 0.30f;
constexpr float STEP_SLOW = 0.45f;
constexpr float MOUSE_SENS_BASE = 0.0025f;

// Sprint / stamina
constexpr float STAM_MIN_START = 0.15f;
constexpr float STAM_REGEN_DELAY = 0.80f;
constexpr float STAM_DRAIN_DEF = 0.35f;
constexpr float STAM_REGEN_DEF = 0.20f;

// Enemigos
constexpr float E_RADIUS = 0.40f;
constexpr float BASE_ESPD = 2.4f;
constexpr float CATCH_R = 0.85f;
constexpr float SEE_DIST = 26.0f;
constexpr float MEMORY_T = 4.0f;
constexpr float REPATH_CD = 0.35f;
constexpr float UNSTICK_T = 1.2f;

// Vista
constexpr int FOV = 72;
constexpr float PITCH_MAX = 1.4f;
constexpr int FOG_RADIUS = 4;

// Modo infinito
constexpr int ENDLESS_MAX_SIZE = 31;

// Persistencia
#if defined(__EMSCRIPTEN__)
constexpr const char *CONFIG_PATH = "/saves/config.cfg";
#else
constexpr const char *CONFIG_PATH = "config.cfg";
#endif

constexpr float RENDER_DIST[3] = {40.0f, 60.0f, 100.0f};
constexpr int FPS_OPTIONS[6] = {0, 30, 60, 120, 144, 240};
constexpr int FPS_COUNT = 6;

// Version
constexpr const char *GAME_VERSION = "beta 1.5.1";
constexpr const char *GAME_TITLE = "Escape From Marisa 3";

// Jumpscare
constexpr float JUMPSCARE_DURATION = 1.3f; // duración total
constexpr float JUMPSCARE_ZOOM_IN = 0.15f; // primer 15% es zoom rápido

// Inventario e items
constexpr int INV_SLOTS = 5;
constexpr float PICKUP_RANGE = 3.0f;
constexpr float INTERACT_RANGE = 2.0f;

constexpr float ROCK_THROW_SPEED = 24.0f;
constexpr float ROCK_THROW_CD = 0.45f;
constexpr float ROCK_STUN = 1.8f;
constexpr float ROCK_RAGE_T = 6.0f;
constexpr float ROCK_RAGE_MULT = 1.4f;

constexpr float BOX_THROW_SPEED = 20.0f;
constexpr float BOX_THROW_CD = 0.45f;
constexpr float BOX_STUN = 2.0f;
constexpr float BOX_RAGE_T = 7.0f;
constexpr float BOX_RAGE_MULT = 1.4f;

constexpr float LIME_THROW_SPEED = 16.0f;
constexpr float LIME_THROW_CD = 1.0f;
constexpr float LIME_STUN = 2.0f;
constexpr float LIME_RAGE_T = 8.0f;
constexpr float LIME_RAGE_MULT = 1.8f;

constexpr float BICIMAD_THROW_SPEED = 12.0f;
constexpr float BICIMAD_THROW_CD = 2.0f;
constexpr float BICIMAD_STUN = 3.0f;
constexpr float BICIMAD_RAGE_T = 10.0f;
constexpr float BICIMAD_RAGE_MULT = 2.0f;

constexpr float ROCK_PICKUP_BOB = 0.18f;
constexpr float STAM_WATER_MAX = 2.0f;
constexpr float WATER_COOLDOWN = 0.5f;
constexpr float DROP_DIST = 0.9f;
} // namespace C

enum AppState {
  ST_MENU,
  ST_MODE_SELECT,
  ST_PLAYER_SELECT,
  ST_SLOT_SELECT,
  ST_CONFIG,
  ST_CONFIG_VIDEO,
  ST_CONFIG_AUDIO,
  ST_CONFIG_GAMEPLAY,
  ST_CONFIG_CONTROLS,
  ST_CONFIG_ACCESS,
  ST_CINEMATIC,
  ST_JUMPSCARE,
  ST_CREDITS,
  ST_PAUSED,
  ST_PLAYING,
  ST_LEVEL_CLEAR,
  ST_GAME_OVER
};

enum SlotPurpose { SLOT_NEW = 0, SLOT_LOAD = 1 };
enum GameMode { MODE_STORY = 0, MODE_ENDLESS = 1 };
enum EnemyKind {
  ENEMY_BEA = 0,
  ENEMY_MARISA,
  ENEMY_ALT,
  ENEMY_CV,
  ENEMY_YE,
  ENEMY_ALMEIDA,
  ENEMY_HYPERGAMY,
  ENEMY_KIND_COUNT
};
enum Diff { DIFF_NORMAL = 0, DIFF_EXPERT, DIFF_NIGHTMARE, DIFF_COUNT };

enum ItemKind {
  ITEM_NONE = 0,
  ITEM_ROCK,
  ITEM_BOX,
  ITEM_LIME,
  ITEM_BICIMAD,
  ITEM_KIND_COUNT
};

struct InvItem {
  ItemKind kind = ITEM_NONE;
  int count = 0;
};

struct WorldPickup {
  ItemKind kind = ITEM_NONE;
  Vector3 pos = {0, 0, 0};
  float bobT = 0.0f;
};

struct Projectile {
  Vector3 pos = {0, 0, 0};
  Vector3 vel = {0, 0, 0};
  float lifetime = 0.0f;
  bool alive = true;
  int ownerId = 0;
  ItemKind kind = ITEM_ROCK;
};

struct Player {
  Vector3 pos = {0, 0, 0};
  float yaw = 0.0f;
  float pitch = 0.0f;
  bool caught = false;
  bool escaped = false;
  float footstepT = 0.0f;
  int gamepadId = -1;
  bool sprinting = false;
  float stamina = 1.0f;
  float staminaDelay = 0.0f;
  bool exhausted = false;
  float escapeTime = 0.0f;
  int caughtBy = -1;

  // Inventario
  InvItem inventory[C::INV_SLOTS];
  int selectedSlot = 0;
  float throwCooldown = 0.0f;
  float attackFlash = 0.0f;

  // Esconderse
  bool hidden = false;
  Vector3 hiddenExitPos = {0, 0, 0};
  float hiddenExitYaw = 0.0f;
  int hiddenCellX = -1;
  int hiddenCellY = -1;
};

struct Enemy {
  Vector3 pos = {0, 0, 0};
  Vector2 dir = {0, 0};
  float speed = 0.0f;
  float turnT = 0.0f;
  int kind = ENEMY_BEA;
  Vector3 lastSeen = {0, 0, 0};
  float memoryT = 0.0f;
  float repathT = 0.0f;
  float stuckT = 0.0f;
  Vector3 prevPos = {0, 0, 0};

  // Estados de proyectiles
  float stunT = 0.0f;
  float rageT = 0.0f;
  float rageMult = 1.0f;
  float hitFlash = 0.0f;
};

enum ColorblindMode {
  CB_NONE = 0,
  CB_DEUTERANOPIA,
  CB_PROTANOPIA,
  CB_TRITANOPIA,
  CB_COUNT
};
