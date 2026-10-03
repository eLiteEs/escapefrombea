#pragma once
#include "raylib.h"
#include "raymath.h"

namespace C {
    // Mundo
    constexpr float CELL      = 4.0f;
    constexpr float WALL_H    = 3.6f;
    constexpr float EYE_H     = 1.7f;

    // Jugador
    constexpr float P_SPEED   = 6.5f;
    constexpr float P_SPRINT  = 1.55f;
    constexpr float P_RADIUS  = 0.45f;
    constexpr float STEP_FAST = 0.30f;
    constexpr float STEP_SLOW = 0.45f;
    constexpr float MOUSE_SENS_BASE = 0.0025f;

    // Sprint / stamina
    constexpr float STAM_MIN_START  = 0.15f;
    constexpr float STAM_REGEN_DELAY= 0.80f;
    constexpr float STAM_DRAIN_DEF  = 0.35f;
    constexpr float STAM_REGEN_DEF  = 0.20f;

    // Enemigos
    constexpr float E_RADIUS   = 0.40f;
    constexpr float BASE_ESPD  = 2.4f;
    constexpr float CATCH_R    = 0.85f;
    constexpr float SEE_DIST   = 26.0f;
    constexpr float MEMORY_T   = 4.0f;
    constexpr float REPATH_CD  = 0.35f;
    constexpr float UNSTICK_T  = 1.2f;

    // Vista
    constexpr int   FOV        = 72;
    constexpr float PITCH_MAX  = 1.4f;
    constexpr int   FOG_RADIUS = 4;

    // Modo infinito
    constexpr int   ENDLESS_MAX_SIZE = 31;

    // Persistencia
#if defined(__EMSCRIPTEN__)
    constexpr const char* CONFIG_PATH = "/saves/config.cfg";
#else
    constexpr const char* CONFIG_PATH = "config.cfg";
#endif

    constexpr float RENDER_DIST[3] = { 40.0f, 60.0f, 100.0f };
    constexpr int   FPS_OPTIONS[6] = { 0, 30, 60, 120, 144, 240 };
    constexpr int   FPS_COUNT      = 6;

    // Version
    constexpr const char* GAME_VERSION = "beta 1.2";
    constexpr const char* GAME_TITLE   = "Escape From Marisa 3";

    // Armas
    constexpr int   SLINGSHOT_AMMO_DEF = 6;
    constexpr float STUN_SLING         = 1.5f;
    constexpr float RAGE_SLING_T       = 10.0f;
    constexpr float RAGE_SLING_MULT    = 1.8f;
    constexpr float STUN_WHIP          = 0.6f;
    constexpr float RAGE_WHIP_T        = 8.0f;
    constexpr float RAGE_WHIP_MULT     = 1.5f;
    constexpr float WHIP_RANGE         = 3.0f;
    constexpr float WHIP_CONE          = 0.70f;
    constexpr float WHIP_CD            = 1.0f;
    constexpr float SLING_CD           = 0.7f;

    // Jumpscare
    constexpr float JUMPSCARE_DURATION = 1.3f;   // duración total
    constexpr float JUMPSCARE_ZOOM_IN  = 0.15f;  // primer 15% es zoom rápido
}

enum AppState {
    ST_MENU,
    ST_MODE_SELECT,
    ST_PLAYER_SELECT,
    ST_SLOT_SELECT,
    ST_CONFIG, ST_CONFIG_VIDEO, ST_CONFIG_AUDIO, ST_CONFIG_GAMEPLAY, ST_CONFIG_CONTROLS, ST_CONFIG_ACCESS,
    ST_CINEMATIC,
    ST_JUMPSCARE,
    ST_PAUSED, ST_PLAYING, ST_LEVEL_CLEAR, ST_GAME_OVER
};

enum SlotPurpose { SLOT_NEW = 0, SLOT_LOAD = 1 };
enum GameMode { MODE_STORY = 0, MODE_ENDLESS = 1 };
enum EnemyKind { ENEMY_BEA = 0, ENEMY_MARISA, ENEMY_ALT, ENEMY_CV, ENEMY_YE, ENEMY_KIND_COUNT };
enum Diff { DIFF_NORMAL = 0, DIFF_EXPERT, DIFF_NIGHTMARE, DIFF_COUNT };

enum WeaponKind {
    WEAPON_NONE = 0,
    WEAPON_SLINGSHOT,
    WEAPON_WHIP,
    WEAPON_KIND_COUNT
};


struct Player {
    Vector3 pos       = { 0, 0, 0 };
    float   yaw       = 0.0f;
    float   pitch     = 0.0f;
    bool    caught    = false;
    bool    escaped   = false;
    float   footstepT = 0.0f;
    int     gamepadId = -1;
    bool    sprinting = false;
    float   stamina       = 1.0f;
    float   staminaDelay  = 0.0f;
    bool    exhausted     = false;
    float   escapeTime    = 0.0f;
    int   caughtBy = -1;
    
    // Armas
    int   weapon         = WEAPON_NONE;
    int   ammo           = 0;
    float attackCooldown = 0.0f;
    float attackFlash    = 0.0f;
};

struct Enemy {
    Vector3 pos       = { 0, 0, 0 };
    Vector2 dir       = { 0, 0 };
    float   speed     = 0.0f;
    float   turnT     = 0.0f;
    int     kind      = ENEMY_BEA;
    Vector3 lastSeen  = { 0, 0, 0 };
    float   memoryT   = 0.0f;
    float   repathT   = 0.0f;
    float   stuckT    = 0.0f;
    Vector3 prevPos   = { 0, 0, 0 };

        // Estados de armas
    float stunT    = 0.0f;
    float rageT    = 0.0f;
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
struct Projectile {
    Vector3 pos      = { 0, 0, 0 };
    Vector3 vel      = { 0, 0, 0 };
    float   lifetime = 0.0f;
    bool    alive    = true;
    int     ownerId  = 0;
};
