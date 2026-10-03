#pragma once

#include "raylib.h"
#include "debug.h"
#include "common.h"
#include "weapon.h"
#include "config.h"
#include "maze.h"
#include "assets.h"
#include "cinematic.h"
#include "world_mesh.h"
#include "save.h"
#include <vector>
#include <string>

struct Game {
    Config cfg;
    Maze   maze;
    Assets assets;
    SaveManager saves;

    Player players[2];
    std::vector<Enemy> enemies;
    Vector3 exitPos = { 0, 0, 0 };
    std::vector<Projectile> projectiles;

    int     level = 1;
    float   levelTime = 0.0f;
    float   totalPlaytime = 0.0f;
    bool    twoPlayers = false;
    bool    shouldQuit = false;
    GameMode mode = MODE_STORY;
    AppState state = ST_MENU;

    // Cinemática
    Cinematic   cinematic;
    std::string nextAfterCinematic;

    // Flujo de menús
    GameMode pendingMode = MODE_STORY;
    bool     pendingTwoPlayers = false;
    int      slotPurpose = SLOT_NEW;
    int      navMenu = 0;
    int      navSlot = 0;
    AppState configReturn = ST_MENU;

    // Partida actual
    int  currentSlot = -1;

    // Toast (mensajito de "Guardado")
    char  toastMsg[128] = { 0 };
    float toastTimer = 0.0f;

    // Remapeo
    int remapAction = -1;
    int remapPlayer = 0;

    // Navegación de configs
    int navIndex    = 0;
    int navIndexCfg = 0;

    // Split-screen RTs
    RenderTexture2D rtLeft  = {};
    RenderTexture2D rtRight = {};
    int rtW = 0, rtH = 0;

    uint32_t currentSeed = 0;   // guardado para el overlay

    // Audio
    bool  musicMenuPlaying  = false;
    bool  musicLevelPlaying = false;
    float heartbeatCd = 0.0f;

    // Ciclo de vida
    void init();
    void shutdown();
    void run();
    void tick();

    // Estado
    void toMenu();
    void beginPlay(bool two, GameMode m);
    void startLevel(int lvl);
    void assignGamepads();
    void ensureRenderTargets();

    // Save system
    void startNewGame(int slot, GameMode m, bool two);
    void loadGameFromSlot(int slot);
    void saveCurrentGame();
    void autoSaveProgress();
    void showToast(const char* msg);

    // Update / Draw
    void update(float dt);
    void draw();
    void drawMenus();
    void drawModeSelect();
    void drawPlayerSelect();
    void drawSlotSelect();
    void drawConfigHub();
    void drawConfigVideo();
    void drawConfigAudio();
    void drawConfigGameplay();
    void drawConfigControls();
    void drawGameplayView(const Player& pl, int vw, int vh, int playerNum);
    void drawMinimap(const Player& pl, int vw);
    void drawStaminaBar(const Player& pl, int vw, int vh);
    void drawSplitScreen();
    void drawCinematic();
    void drawPauseMenu();
    void drawToast();
    void drawConfigAccess();

    // Cinemáticas
    void playCinematic(const char* name, const char* nextAction);
    bool hasStory(const char* name) const;

    // Helpers
    float nearestEnemyDist(Vector3 p) const;
    void  handleGamepadMenuNav(int count);
    int   gamepadLeftRight();
    bool  consumeConfirm();
    bool  consumeCancel();
    Color playerColor(int idx) const;
    static const char* formatDate(std::time_t t);

    // Vídeo / accesibilidad
    void applyVideoSettings();
    void applyRenderDistance();
    float renderDist() const;

    void handleDebugHotkeys();
    void drawDebugOverlay(int vw, int vh, int playerNum);
    void drawDebugWorld3D(const Player& pl);

    // Cheats
    void debugSpawnEnemy();
    void debugKillAll();
    void debugTeleportToExit();
    void debugRevealMap();

    void startJumpscare(int enemyIdx);
    void drawJumpscare();
    float jumpscareTimer = 0.0f;
    int   jumpscareEnemy = -1;

    WorldMeshes world;
};

