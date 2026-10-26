#pragma once

#include "assets.hpp"
#include "attract.hpp"
#include "cinematic.hpp"
#include "common.hpp"
#include "config.hpp"
#include "credits.hpp"
#include "maze.hpp"
#include "raycaster.hpp"
#include "raylib.h"
#include "renderer3d.hpp"
#include "save.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct LevelDetails {
  bool loaded = false;
  std::string name;
  std::string subtitle;
  int difficulty = 1;
};

struct SpeedrunState {
  bool active = false;
  uint32_t seed = 0;
  int mode = 0;
  std::vector<float> splits;
  std::vector<float> bestSplits;
  float lastLevelStart = 0.0f;
  float lastSplitDelta = 0.0f;
  bool justRecordedPB = false;
  float pbFlashTimer = 0.0f;
};

struct Game {
  Config cfg;
  Maze maze;
  AttractMode attract;
  Assets assets;
  SaveManager saves;
  Raycaster rc;
  Renderer3D r3d;

  Player players[2];
  std::vector<Enemy> enemies;
  Vector3 exitPos = {0, 0, 0};
  std::vector<Projectile> projectiles;
  std::vector<WorldPickup> worldItems;
  float fountainCooldown[2] = {0.0f, 0.0f};

  int level = 1;
  float levelTime = 0.0f;
  float totalPlaytime = 0.0f;
  bool twoPlayers = false;
  bool shouldQuit = false;
  GameMode mode = MODE_STORY;
  AppState state = ST_MENU;

  // Cinemáticas
  Cinematic cinematic;
  std::string nextAfterCinematic;

  // Pantalla de intro de nivel
  LevelDetails levelDetails;
  float introTimer = 0.0f;

  // Flujo de menús
  int navMenu = 0;
  AppState configReturn = ST_MENU;

  // Partida actual (sistema dinámico)
  std::string currentSaveId;
  int currentDifficulty = DIFF_NORMAL;
  uint32_t runSeed = 0;

  // Speedrun
  SpeedrunState sr;

  // Lista de saves + formulario de creación
  SaveEntry createBuffer;
  int saveListCursor = 0;
  int saveListScroll = 0;
  bool saveDeleteConfirm = false;
  std::string saveDeletePendingId;

  // Toast
  char toastMsg[128] = {0};
  float toastTimer = 0.0f;

  // Remapeo de controles
  int remapAction = -1;
  int remapPlayer = 0;

  // Navegación de configuración
  int navIndex = 0;
  int navIndexCfg = 0;

  // Split-screen render targets
  RenderTexture2D rtLeft = {};
  RenderTexture2D rtRight = {};
  RenderTexture2D rt3d = {}; // Render target para el renderer 3D
  int rtW = 0, rtH = 0;

  uint32_t currentSeed = 0; // Current save seed

  // Audio
  bool musicMenuPlaying = false;
  bool musicLevelPlaying = false;
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

  // Sistema de saves
  void startNewGame(const SaveEntry &proto);
  void loadGame(const std::string &saveId);
  void saveCurrentGame();
  void autoSaveProgress();
  void showToast(const char *msg);

  // Update / Draw
  void update(float dt);
  void draw();

  // Game screen
  void drawGameplayView(const Player &pl, int vw, int vh, int playerNum);
  void drawSplitScreen();

  // HUD
  void drawInventory(const Player &pl, int vw, int vh);
  void drawHandItem2D(const Player &pl, int vw, int vh);
  void drawMinimap(const Player &pl, int vw);
  void drawStaminaBar(const Player &pl, int vw, int vh);
  void drawCinematic();
  void drawToast();

  // Menus
  void drawMenus();
  void drawSpeedrunMenu();
  void drawPauseMenu();
  void drawMenuLevelClear();
  void drawMenuGameOver();
  void drawMenuMain();

  void drawSaveList();
  void drawSaveCreate();

  // Configs
  void drawConfigHub();
  void drawConfigVideo();
  void drawConfigAudio();
  void drawConfigGameplay();
  void drawConfigControls();
  void drawConfigAccess();
  void drawConfigSpeedrun();

  // Cinemáticas y transiciones de nivel
  void playCinematic(const char *name, const char *nextAction);
  bool hasStory(const char *name) const;
  bool loadLevelDetails(int lvl);
  void requestEnterLevel(int lvl);
  void enterLevel(int lvl);
  void drawLevelIntro();

  // Speedruns
  void speedrunStartRun(uint32_t seed, int mode);
  void speedrunRecordSplit();
  void speedrunFinishRun();
  void drawSpeedrunOverlay(int vw, int vh);

  int srMenuRunCursor = 0;
  int srMenuRunScroll = 0;
  bool srMenuDeleteConfirm = false;
  int srMenuDeleteIndex = -1;

  GhostRun srGhostRec;
  GhostRun srGhostPlayback;
  bool srGhostPlaybackValid = false;
  float srGhostSampleTimer = 0.0f;

  // Niveles custom
  bool tryLoadCustomLevel(int lvl, LevelExtras &extras);
  void spawnEnemiesFromPositions(const std::vector<Vector3> &positions, int lvl,
                                 std::mt19937 &rng);
  void spawnItemsFromPositions(const std::vector<Vector3> &positions,
                               std::mt19937 &rng);
  void spawnKeysFromPositions(const std::vector<Vector3> &positions);

  // Helpers
  float nearestEnemyDist(Vector3 p) const;
  void handleGamepadMenuNav(int count);
  int gamepadLeftRight();
  bool consumeConfirm();
  bool consumeCancel();
  Color playerColor(int idx) const;

  // Vídeo / accesibilidad
  void applyVideoSettings();
  float renderDist() const;

  // Debug
  void handleDebugHotkeys();
  void drawDebugOverlay(int vw, int vh, int playerNum);
  void drawDebugWorld3D(const Player &pl);

  // Cheats
  void debugSpawnEnemy();
  void debugKillAll();
  void debugTeleportToExit();
  void debugRevealMap();

  // Jumpscare
  void startJumpscare(int enemyIdx);
  void drawJumpscare();
  float jumpscareTimer = 0.0f;
  int jumpscareEnemy = -1;

  // Credits
  CreditsScreen credits;

  // Items
  void spawnWorldItems(int lvl, int size, std::mt19937 &rng);
  void updateWorldItems(float dt);

  // Interactions
  void handleInteraction(int playerIdx);
  void handleItemInput(int playerIdx);
  int findInteractable(const Player &pl, int &outCx, int &outCy,
                       bool &isLocker) const;
  void renderPlayerView(const Player &pl);

  // Buffer de texto para editar la seed en la creación de partida
  char seedEditBuf[16] = {0};

  // Handle double clicking on save list
  int saveListLastClickIdx = -1;
  double saveListLastClickTime = 0.0;

  // NPCs, Signs, Dialogs
  std::vector<LevelExtras::SignData> currentSigns;
  std::vector<LevelExtras::NpcData> currentNpcs;

  std::string dialogTitle;
  std::string dialogText;
  bool dialogHasSignTex = false;

  void openDialog(const std::string &title, const std::string &text,
                  bool isSign);
  void closeDialog();
  void drawDialog();
};
