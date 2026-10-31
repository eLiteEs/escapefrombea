#pragma once

#include "raylib.h"

#include <cstdint>

namespace C {
// Mundo
constexpr float CELL = 4.0f;
constexpr float WALL_H = 3.6f;
constexpr float EYE_H = 1.7f;
constexpr int CHUNK_SIZE = 8; // celdas por lado del chunk

// Jugador
constexpr float P_SPEED = 6.5f;
constexpr float P_SPRINT = 1.55f;
constexpr float P_RADIUS = 0.55f;
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
constexpr const char *GAME_VERSION = "release 1.0.0r";
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

constexpr float ENDLESS_MULT[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f};
constexpr int ENDLESS_MULT_COUNT = 6;

constexpr int RC_RESOLUTIONS[][2] = {
    {160, 100}, // 0: muy retro, coste mínimo
    {240, 150}, // 1: retro
    {320, 200}, // 2: default (Wolfenstein 3D)
    {400, 250}, // 3: nítido
    {480, 300}, // 4: alto
    {640, 400}, // 5: máxima calidad
};
constexpr int RC_RES_COUNT = 6;
constexpr int RC_RES_DEFAULT = 2;

constexpr float Q_WALKSPEED = 6.5f; // velocidad objetivo caminando
constexpr float Q_SPRINT_MULT =
    1.55f;                         // multiplicador sprint (equivale a P_SPRINT)
constexpr float Q_ACCEL = 14.0f;   // aceleración en suelo
constexpr float Q_AIRACCEL = 2.5f; // aceleración en aire (air control)
constexpr float Q_FRICTION = 5.0f; // fricción en suelo
constexpr float Q_GRAVITY = 22.0f; // gravedad
constexpr float Q_JUMPSPEED = 6.5f; // velocidad vertical inicial del salto

// Speedrun ghost
constexpr float GHOST_SAMPLE_HZ = 10.0f; // 10 muestras/segundo
constexpr float GHOST_SAMPLE_DT = 1.0f / GHOST_SAMPLE_HZ;

// Alturas de mapa
constexpr int MAX_HEIGHT = 4;
constexpr float HEIGHT_STEP = WALL_H;
constexpr float STEP_MAX = 0.4f; // máxima altura subible caminando

// Escaleras: altura de cada "sub-step" visual
constexpr float STAIR_STEP_H = 0.40f;
// Rango extra para interactuar con carteles / NPCs
constexpr float SIGN_INTERACT_RANGE = 2.5f;

// Vida y escudo
constexpr float PLAYER_MAX_HP = 50.0f;
constexpr float PLAYER_MAX_SHIELD = 50.0f;
constexpr float ENEMY_MAX_HP = 35.0f;
constexpr float ENEMY_MAX_SHIELD = 20.0f;

// Cuánto curan los pickups
constexpr float BIFRUTAS_RESTORE = 30.0f;
constexpr float ENSAIMADA_RESTORE = 40.0f;

// Daño por proyectil (según el item lanzado)
constexpr float DMG_ROCK = 15.0f;
constexpr float DMG_BOX = 20.0f;
constexpr float DMG_LIME = 25.0f;
constexpr float DMG_BICIMAD = 35.0f;

// Melee — látigo
constexpr float WHIP_RANGE = 6.0f;
constexpr float WHIP_HALF_ARC = 0.60f; // rad; ~34° de semiángulo (68° total)
constexpr float WHIP_DAMAGE = 40.0f;
constexpr float WHIP_COOLDOWN = 1.6f;
constexpr float WHIP_STAMINA_COST = 0.25f;
constexpr int WHIP_USES = 10;

// Melee — tijeras
constexpr float SCISSORS_RANGE = 3.6f;
constexpr float SCISSORS_HALF_ARC = 0.70f;
constexpr float SCISSORS_DAMAGE = 12.0f;
constexpr float SCISSORS_COOLDOWN = 0.4f;
constexpr float SCISSORS_SPEED_MULT = 1.15f;
constexpr int SCISSORS_USES = 10;

// Melee — animación
constexpr float WHIP_SWING_DUR = 0.45f;
constexpr float WHIP_EXTENDED_TIME = 0.30f; // ventana con textura extendida
constexpr float SCISSORS_SWING_DUR = 0.22f;
constexpr float SCISSORS_SWING_OFF = 0.55f; // 55% del CD → empieza a volver

} // namespace C

enum AppState {
  ST_MENU,
  ST_SAVE_LIST,
  ST_SAVE_CREATE,
  ST_CONFIG,
  ST_CONFIG_VIDEO,
  ST_CONFIG_AUDIO,
  ST_CONFIG_GAMEPLAY,
  ST_CONFIG_CONTROLS,
  ST_CONFIG_ACCESS,
  ST_CONFIG_SPEEDRUN,
  ST_SPEEDRUN,
  ST_CINEMATIC,
  ST_LEVEL_INTRO,
  ST_JUMPSCARE,
  ST_CREDITS,
  ST_PAUSED,
  ST_PLAYING,
  ST_LEVEL_CLEAR,
  ST_GAME_OVER,
  ST_DIALOG
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
  PLAYER,
  ENEMY_KIND_COUNT
};
enum Diff { DIFF_NORMAL = 0, DIFF_EXPERT, DIFF_NIGHTMARE, DIFF_COUNT };

enum ItemKind {
  ITEM_NONE = 0,
  ITEM_ROCK,
  ITEM_BOX,
  ITEM_LIME,
  ITEM_BICIMAD,
  ITEM_MONSTER,
  ITEM_WALL,
  ITEM_KEY,
  ITEM_BIFRUTAS,
  ITEM_ENSAIMADA,
  ITEM_WHIP,
  ITEM_SCISSORS,
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
  bool hasKey = false;

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

  Vector3 velocity = {0, 0, 0};
  bool onGround = true;
  bool jumpHeld = false;
  float groundZ = 0.0f; // altura del suelo bajo el jugador (rampas en Fase 5)

  float hp = C::PLAYER_MAX_HP;
  float maxHp = C::PLAYER_MAX_HP;
  float shield = C::PLAYER_MAX_SHIELD;
  float maxShield = C::PLAYER_MAX_SHIELD;
  float damageFlashT = 0.0f;
  float throwCooldownMax = 0.0f;

  // Viewmodel melee (progreso 0..1 durante la animación, 0 = en reposo)
  float meleeAnimT = 0.0f;   // tiempo restante; 0 = sin animación
  float meleeAnimDur = 0.0f; // duración total (para normalizar)
  int meleeAnimKind = 0;     // ItemKind que disparó la anim
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

  float hp = C::ENEMY_MAX_HP;
  float maxHp = C::ENEMY_MAX_HP;
  float shield = C::ENEMY_MAX_SHIELD;
  float maxShield = C::ENEMY_MAX_SHIELD;
  bool alive = true;
  float dyingT = 0.0f;
};

struct EndlessRunConfig {
  uint32_t seed = 0; // 0 = aún sin asignar (se aleatoriza al empezar)
  int enemyMult = 2; // índice en C::ENDLESS_MULT
  int itemMult = 2;
};

enum ColorblindMode {
  CB_NONE = 0,
  CB_DEUTERANOPIA,
  CB_PROTANOPIA,
  CB_TRITANOPIA,
  CB_COUNT
};
