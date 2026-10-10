#include "debug.h"
#include "enemy.h"
#include "game.h"
#include "lang.h"
#include "paths.h"
#include "player.h"
#include "rlgl.h"
#include "save.h"
#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

// ---------- Util ----------
Color Game::playerColor(int idx) const { return UI::PlayerColor(idx); }

void Game::init() {
  cfg.load();
  Lang::load(cfg.language.c_str());

  // ---------- 1) Ventana pequeña, sin bordes, centrada ----------
  SetConfigFlags(cfg.vsync ? FLAG_VSYNC_HINT : 0);
  SetConfigFlags(FLAG_WINDOW_UNDECORATED);
  InitWindow(640, 400, "Cargando...");

  int monW = GetMonitorWidth(0);
  int monH = GetMonitorHeight(0);
  SetWindowPosition((monW - 640) / 2, (monH - 400) / 2);

  SetTargetFPS(60);
  SetMouseCursor(MOUSE_CURSOR_ARROW);
  SetExitKey(KEY_NULL);

  // ---------- 2) Carga y dibuja el splash ----------
  Texture2D splash = LoadTexture(Paths::sprite("splash.png").c_str());
  if (splash.id != 0)
    SetTextureFilter(splash, TEXTURE_FILTER_BILINEAR);

  BeginDrawing();
  ClearBackground(BLACK);
  if (splash.id != 0) {
    float scale = std::min(640.0f / splash.width, 400.0f / splash.height);
    float tw = splash.width * scale;
    float th = splash.height * scale;
    DrawTexturePro(splash, {0, 0, (float)splash.width, (float)splash.height},
                   {320.0f - tw / 2, 200.0f - th / 2, tw, th}, {0, 0}, 0.0f,
                   WHITE);
  } else {
    const char *t = "ESCAPE FROM BEA";
    DrawText(t, 320 - MeasureText(t, 36) / 2, 180, 36, RED);
  }
  EndDrawing();

  // ---------- 3) Carga pesada (audio + assets + resto) ----------
  InitAudioDevice();
  assets.load();
  assets.applyVolumes(cfg.masterVol, cfg.musicVol, cfg.sfxVol);

  saves.refresh();
  credits.load("assets/credits.txt");
  attract.init(15, (uint32_t)time(nullptr) + 12345u, assets);

  WaitTime(1);

  // ---------- 4) Pasar a la ventana real ----------
  if (splash.id != 0)
    UnloadTexture(splash);

  ClearWindowState(FLAG_WINDOW_UNDECORATED);

  if (cfg.fullscreen) {
    SetWindowSize(monW, monH);
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetWindowPosition(0, 0);
  } else {
    SetWindowSize(cfg.windowW, cfg.windowH);
    SetWindowPosition((monW - cfg.windowW) / 2, (monH - cfg.windowH) / 2);
  }

  // Ahora sí, con la ventana definitiva
  applyVideoSettings();
  ensureRenderTargets();

  state = ST_MENU;
}

void Game::shutdown() {
  cfg.save();
  world.unload();
  if (rtLeft.texture.id != 0)
    UnloadRenderTexture(rtLeft);
  if (rtRight.texture.id != 0)
    UnloadRenderTexture(rtRight);
  attract.unload();
  assets.unload();
  CloseAudioDevice();
  CloseWindow();
}

void Game::ensureRenderTargets() {
  int W = GetScreenWidth(), H = GetScreenHeight();
  if (W <= 1 || H <= 1)
    return;
  if (W == rtW && H == rtH && rtLeft.texture.id != 0)
    return;
  if (rtLeft.texture.id != 0)
    UnloadRenderTexture(rtLeft);
  if (rtRight.texture.id != 0)
    UnloadRenderTexture(rtRight);
  rtW = W;
  rtH = H;
  int halfW = std::max(1, rtW / 2);
  rtLeft = LoadRenderTexture(halfW, rtH);
  rtRight = LoadRenderTexture(rtW - halfW, rtH);
}

// ---------- Estado ----------
void Game::toMenu() {
  state = ST_MENU;
  navIndex = 0;
  EnableCursor();
  if (assets.musicLevel.stream.buffer != nullptr && musicLevelPlaying) {
    StopMusicStream(assets.musicLevel);
    musicLevelPlaying = false;
  }
  if (assets.musicMenu.stream.buffer != nullptr && !musicMenuPlaying) {
    PlayMusicStream(assets.musicMenu);
    musicMenuPlaying = true;
  }
}

void Game::beginPlay(bool two, GameMode m) {
  twoPlayers = two;
  mode = m;

  if (assets.musicMenu.stream.buffer != nullptr && musicMenuPlaying) {
    StopMusicStream(assets.musicMenu);
    musicMenuPlaying = false;
  }

  level = 1;

  if (mode == MODE_STORY && cfg.playCinematics) {
    char name[64];
    snprintf(name, sizeof(name), "level_%02d_intro.txt", level);
    if (hasStory(name)) {
      playCinematic(name, "playing");
      return;
    }
  }

  startLevel(level);
  assignGamepads();
  DisableCursor();
  state = ST_PLAYING;
  if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
    PlayMusicStream(assets.musicLevel);
    musicLevelPlaying = true;
  }
}

void Game::assignGamepads() {
  int p1Pad = cfg.swapGamepads ? 1 : 0;
  int p2Pad = cfg.swapGamepads ? 0 : 1;
  players[0].gamepadId = IsGamepadAvailable(p1Pad) ? p1Pad : -1;
  players[1].gamepadId = (twoPlayers && IsGamepadAvailable(p2Pad)) ? p2Pad : -1;
}

static Vector3 findFreeSpawnNear(const Maze &maze, int cx, int cy,
                                 Vector3 fallback) {
  for (int r = 1; r < std::max(maze.w, maze.h); ++r) {
    for (int y = cy - r; y <= cy + r; ++y)
      for (int x = cx - r; x <= cx + r; ++x) {
        if (maze.wallAt(x, y))
          continue;
        if (x == cx && y == cy)
          continue;
        return maze.cellCenter(x, y);
      }
  }
  return fallback;
}

static void resetPlayerState(Player &p, Vector3 spawn) {
  if (p.caught) {
    for (int i = 0; i < C::INV_SLOTS; ++i)
      p.inventory[i] = {};
  }

  p.pos = spawn;
  p.yaw = p.pitch = 0;
  p.caught = p.escaped = false;
  p.footstepT = 0;
  p.stamina = 1.0f;
  p.staminaDelay = 0.0f;
  p.exhausted = false;
  p.sprinting = false;
  p.escapeTime = 0.0f;
  p.caughtBy = -1;

  // NUEVO
  p.hidden = false;
  p.hiddenExitPos = spawn;
  p.hiddenExitYaw = 0.0f;
  p.hiddenCellX = -1;
  p.hiddenCellY = -1;
  p.selectedSlot = 0;
  p.throwCooldown = 0.0f;
  p.attackFlash = 0.0f;
}

void Game::startLevel(int lvl) {
  int size = 11 + (lvl - 1) * 2;
  if (mode == MODE_ENDLESS && size > C::ENDLESS_MAX_SIZE)
    size = C::ENDLESS_MAX_SIZE;

  uint32_t seed = (uint32_t)time(nullptr) ^ (uint32_t)(lvl * 7919);
  currentSeed = seed;
  maze.generate(size, size, seed);
  world.build(maze, assets);

  resetPlayerState(players[0], maze.cellCenter(1, 1));
  if (twoPlayers)
    resetPlayerState(players[1],
                     findFreeSpawnNear(maze, 1, 1, maze.cellCenter(1, 1)));

  exitPos = maze.cellCenter(size - 2, size - 2);
  levelTime = 0.0f;

  enemies.clear();
  int count;
  if (mode == MODE_ENDLESS)
    count = std::min(2 + lvl / 2, 12);
  else
    count = lvl + 1;

  float spd = C::BASE_ESPD * (1.0f + 0.15f * (float)(lvl - 1)) * cfg.diffMult();
  float minDist = size * C::CELL * 0.35f;

  std::mt19937 rng(seed ^ 0xA5A5A5A5u);

  int attempts = 0;
  while ((int)enemies.size() < count && attempts < 1000) {
    ++attempts;
    int cx = 1 + (int)(rng() % (size - 2));
    int cy = 1 + (int)(rng() % (size - 2));
    if (maze.wallAt(cx, cy))
      continue;
    Vector3 p = maze.cellCenter(cx, cy);
    if (Vector3Distance(p, players[0].pos) < minDist)
      continue;

    Enemy e;
    e.pos = p;
    e.dir = {0, 0};
    e.speed = spd * (0.9f + 0.2f * (float)(rng() % 100) / 100.0f);
    e.turnT = 0.0f;
    e.prevPos = p;

    int r = (int)(rng() % 100);
    if (lvl >= 4 && r < 30)
      e.kind = ENEMY_ALMEIDA;
    else if (lvl >= 3 && r < 30)
      e.kind = ENEMY_CV; // raro, solo alto nivel
    else if (lvl >= 3 && r < 40)
      e.kind = ENEMY_YE;
    else if (lvl >= 2 && r < 65)
      e.kind = ENEMY_MARISA;
    else if (lvl >= 2 && r < 85)
      e.kind = ENEMY_ALT;
    else
      e.kind = ENEMY_BEA;
    e.kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
    enemies.push_back(e);
  }

  worldItems.clear();
  projectiles.clear();
  spawnWorldItems(lvl, size, rng);
  fountainCooldown[0] = 0.0f;
  fountainCooldown[1] = 0.0f;

  maze.markExplored(players[0].pos.x, players[0].pos.z, C::FOG_RADIUS);
  if (twoPlayers)
    maze.markExplored(players[1].pos.x, players[1].pos.z, C::FOG_RADIUS);
}

// ---------- Cinemáticas ----------
bool Game::hasStory(const char *name) const {
  std::string full = Paths::story(name);
  FILE *f = fopen(full.c_str(), "r");
  if (!f)
    return false;
  fclose(f);
  return true;
}

void Game::playCinematic(const char *name, const char *nextAction) {
  std::string full = Paths::story(name);
  if (!cinematic.load(full.c_str())) {
    if (!strcmp(nextAction, "playing")) {
      startLevel(level);
      assignGamepads();
      DisableCursor();
      state = ST_PLAYING;
      if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
        PlayMusicStream(assets.musicLevel);
        musicLevelPlaying = true;
      }
    } else {
      toMenu();
    }
    return;
  }
  nextAfterCinematic = nextAction;
  cinematic.start();
  state = ST_CINEMATIC;
  EnableCursor();

  Debug::resetCheats();
}

// ---------- Update ----------
bool Game::consumeConfirm() {
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    return true;
  for (int p = 0; p < 2; ++p)
    if (players[p].gamepadId >= 0 &&
        IsGamepadButtonPressed(players[p].gamepadId,
                               GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
      return true;
  return false;
}

void Game::handleGamepadMenuNav(int count) {
  for (int p = 0; p < 2; ++p) {
    if (players[p].gamepadId < 0)
      continue;
    int id = players[p].gamepadId;
    if (IsGamepadButtonPressed(id, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
      navIndex = (navIndex + 1) % count;
    if (IsGamepadButtonPressed(id, GAMEPAD_BUTTON_LEFT_FACE_UP))
      navIndex = (navIndex - 1 + count) % count;
  }
}

int Game::gamepadLeftRight() {
  for (int p = 0; p < 2; ++p) {
    if (players[p].gamepadId < 0)
      continue;
    int id = players[p].gamepadId;
    if (IsGamepadButtonPressed(id, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
      return -1;
    if (IsGamepadButtonPressed(id, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
      return +1;
  }
  return 0;
}
void Game::update(float dt) {
  UI::scale = cfg.guiScale;
  UI::colorblind = cfg.colorblindMode;

  if (state == ST_MENU) {
    attract.update(dt);
  }

  if (assets.musicMenu.stream.buffer != nullptr)
    UpdateMusicStream(assets.musicMenu);
  if (assets.musicLevel.stream.buffer != nullptr)
    UpdateMusicStream(assets.musicLevel);

  if (toastTimer > 0.0f)
    toastTimer -= dt;

  ensureRenderTargets();
  handleDebugHotkeys();

  if (state == ST_PLAYING) {
    float gdt = dt * Debug::timeScale;

    levelTime += gdt;
    totalPlaytime += gdt;
    int numPlayers = twoPlayers ? 2 : 1;

    for (int p = 0; p < numPlayers; ++p) {
      if (players[p].caught || players[p].escaped)
        continue;
      bool allowMouse = (!twoPlayers) && (p == 0);
      bool moved =
          updatePlayer(players[p], cfg.binds[p], gdt, allowMouse, cfg, maze);
      if (moved) {
        players[p].footstepT -= gdt;
        if (players[p].footstepT <= 0.0f) {
          if (assets.sfxFootstep.frameCount > 0)
            PlaySound(assets.sfxFootstep);
          players[p].footstepT =
              players[p].sprinting ? C::STEP_FAST : C::STEP_SLOW;
        }
        maze.markExplored(players[p].pos.x, players[p].pos.z, C::FOG_RADIUS);
      }
    }

    for (int p = 0; p < numPlayers; ++p) {
      if (players[p].caught || players[p].escaped)
        continue;
      handleInteraction(p);
      handleItemInput(p);

      if (fountainCooldown[p] > 0.0f)
        fountainCooldown[p] -= gdt;
      if (players[p].throwCooldown > 0.0f)
        players[p].throwCooldown -= gdt;
      if (players[p].attackFlash > 0.0f)
        players[p].attackFlash -= gdt;
    }

    // Enemigos y proyectiles
    updateEnemies(enemies, maze, players, numPlayers, gdt, C::SEE_DIST,
                  C::MEMORY_T);
    Inventory::updateProjectiles(projectiles, maze, enemies, gdt);
    Inventory::updateEnemyStatus(enemies, gdt);
    updateWorldItems(gdt);

    heartbeatCd -= gdt;
    if (heartbeatCd <= 0.0f) {
      float minD = 1e9f;
      for (int p = 0; p < numPlayers; ++p) {
        if (players[p].caught || players[p].escaped)
          continue;
        minD = std::min(minD, nearestEnemyDist(players[p].pos));
      }
      if (minD < 9.0f && assets.sfxHeartbeat.frameCount > 0) {
        PlaySound(assets.sfxHeartbeat);
        float t = minD / 9.0f;
        heartbeatCd = 0.35f + t * 1.1f;
      } else {
        heartbeatCd = 0.4f;
      }
    }

    for (int p = 0; p < numPlayers; ++p) {
      if (players[p].caught || players[p].escaped)
        continue;
      if (Vector3Distance(players[p].pos, exitPos) < 1.7f) {
        players[p].escaped = true;
        players[p].escapeTime = levelTime;
      }
    }

    bool anyCaught = players[0].caught || (twoPlayers && players[1].caught);
    bool allDone;
    if (!twoPlayers)
      allDone = players[0].caught || players[0].escaped;
    else
      allDone = (players[0].caught || players[0].escaped) &&
                (players[1].caught || players[1].escaped);

    // ESC -> pausa
    bool pausePressed = IsKeyPressed(KEY_ESCAPE);
    for (int p = 0; p < 2 && !pausePressed; ++p) {
      if (players[p].gamepadId >= 0 &&
          IsGamepadButtonPressed(players[p].gamepadId,
                                 GAMEPAD_BUTTON_MIDDLE_RIGHT))
        pausePressed = true;
    }

    if (allDone) {
      EnableCursor();
      if (anyCaught) {
        if (assets.sfxCaught.frameCount > 0)
          PlaySound(assets.sfxCaught);
        if (mode == MODE_ENDLESS && level > cfg.maxEndlessLevel) {
          cfg.maxEndlessLevel = level;
          cfg.save();
        }
        autoSaveProgress();

        int caughtPlayer = players[0].caught ? 0 : 1;

        if (!cfg.noJumpscares && players[caughtPlayer].caughtBy >= 0) {
          // Jumpscare
          startJumpscare(players[caughtPlayer].caughtBy);
        } else {
          EnableCursor();
          state = ST_GAME_OVER;
        }
      } else {
        if (assets.sfxEscape.frameCount > 0)
          PlaySound(assets.sfxEscape);
        if (mode == MODE_STORY) {
          if (level + 1 > cfg.maxLevel) {
            cfg.maxLevel = level + 1;
            cfg.save();
          }
        } else {
          if (level > cfg.maxEndlessLevel) {
            cfg.maxEndlessLevel = level;
            cfg.save();
          }
        }
        autoSaveProgress();
        state = ST_LEVEL_CLEAR;
      }
    } else if (pausePressed) {
      EnableCursor();
      navMenu = 0;
      state = ST_PAUSED;
    }
  } else if (state == ST_PAUSED) {
    // Resumir con ESC o START
    bool resumePressed = IsKeyPressed(KEY_ESCAPE);
    for (int p = 0; p < 2 && !resumePressed; ++p) {
      if (players[p].gamepadId >= 0 &&
          IsGamepadButtonPressed(players[p].gamepadId,
                                 GAMEPAD_BUTTON_MIDDLE_RIGHT))
        resumePressed = true;
    }
    if (resumePressed) {
      DisableCursor();
      state = ST_PLAYING;
    }
  } else if (state == ST_CREDITS) {
    bool advance = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                   IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool back = IsKeyPressed(KEY_ESCAPE) || consumeCancel();
    credits.update(dt, advance, back);

    if (credits.isFinished()) {
      credits.stop();
      state = ST_MENU;
      EnableCursor();
    }
  } else if (state == ST_CINEMATIC) {
    bool advance = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                   IsKeyPressed(KEY_ESCAPE) ||
                   IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    for (int p = 0; p < 2; ++p) {
      if (players[p].gamepadId >= 0 &&
          IsGamepadButtonPressed(players[p].gamepadId,
                                 GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        advance = true;
    }
    cinematic.update(dt, advance);

    const std::string &pm = cinematic.pendingMusic();
    if (!pm.empty()) {
      if (pm == "none") {
        if (assets.musicLevel.stream.buffer != nullptr && musicLevelPlaying) {
          StopMusicStream(assets.musicLevel);
          musicLevelPlaying = false;
        }
        if (assets.musicMenu.stream.buffer != nullptr && musicMenuPlaying) {
          StopMusicStream(assets.musicMenu);
          musicMenuPlaying = false;
        }
      } else if (pm == "menu") {
        if (assets.musicMenu.stream.buffer != nullptr && !musicMenuPlaying) {
          PlayMusicStream(assets.musicMenu);
          musicMenuPlaying = true;
        }
      } else if (pm == "level") {
        if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
          PlayMusicStream(assets.musicLevel);
          musicLevelPlaying = true;
        }
      }
      cinematic.clearPendingMusic();
    }

    if (cinematic.isFinished()) {
      const std::string &next = nextAfterCinematic;
      if (next == "playing") {
        startLevel(level);
        assignGamepads();
        DisableCursor();
        state = ST_PLAYING;
        if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
          PlayMusicStream(assets.musicLevel);
          musicLevelPlaying = true;
        }
      } else if (next == "menu") {
        toMenu();
      }
    }
  } else if (state == ST_JUMPSCARE) {
    jumpscareTimer -= dt;

    if (jumpscareTimer <= 0.0f) {
      jumpscareTimer = 0.0f;
      jumpscareEnemy = -1;
      EnableCursor();
      state = ST_GAME_OVER;
    }
  } else if (state == ST_LEVEL_CLEAR) {
    if (consumeConfirm()) {
      ++level;

      if (mode == MODE_STORY && cfg.playCinematics) {
        // ¿Existe intro del siguiente nivel?
        char introName[64];
        snprintf(introName, sizeof(introName), "level_%02d_intro.txt", level);
        if (hasStory(introName)) {
          playCinematic(introName, "playing");
          return;
        }

        // Si no existe intro del siguiente nivel, ¿existe el nivel
        // propiamente? Lo definimos como "existe intro O outro del
        // nivel actual+1". Si no existe ninguno de los dos, fin de
        // la historia.
        char outroName[64];
        snprintf(outroName, sizeof(outroName), "level_%02d_outro.txt", level);
        bool nextLevelExists = hasStory(introName) || hasStory(outroName);

        if (!nextLevelExists) {
          // Fin de la historia -> creditos
          credits.start();
          state = ST_CREDITS;
          return;
        }
      }

      startLevel(level);
      assignGamepads();
      DisableCursor();
      state = ST_PLAYING;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
      if (mode == MODE_STORY && cfg.playCinematics) {
        char outroName[64];
        snprintf(outroName, sizeof(outroName), "level_%02d_outro.txt", level);
        if (hasStory(outroName)) {
          playCinematic(outroName, "menu");
          return;
        }
      }
      toMenu();
    }
  } else if (state == ST_GAME_OVER) {
    if (IsKeyPressed(KEY_R) || consumeConfirm()) {
      startLevel(level);
      assignGamepads();
      DisableCursor();
      state = ST_PLAYING;
    } else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
      toMenu();
    }
  } else if (state == ST_CONFIG_CONTROLS && remapAction >= 0) {
    int k = GetKeyPressed();
    if (k == KEY_ESCAPE) {
      remapAction = -1;
    } else if (k != 0 && k != KEY_NULL && k != KEY_LEFT_SHIFT &&
               k != KEY_RIGHT_SHIFT && k != KEY_LEFT_CONTROL &&
               k != KEY_RIGHT_CONTROL && k != KEY_LEFT_ALT &&
               k != KEY_RIGHT_ALT) {
      switch (remapAction) {
      case 0:
        cfg.binds[remapPlayer].up = k;
        break;
      case 1:
        cfg.binds[remapPlayer].down = k;
        break;
      case 2:
        cfg.binds[remapPlayer].left = k;
        break;
      case 3:
        cfg.binds[remapPlayer].right = k;
        break;
      case 4:
        cfg.binds[remapPlayer].interact = k;
        break;
      case 5:
        cfg.binds[remapPlayer].drop = k;
        break;
      }
      remapAction = -1;
      cfg.save();
    }
  }

  // Save screenshot
  if (IsKeyPressed(KEY_F2)) {
    time_t now = time(nullptr);
    tm *t = localtime(&now);

    char nombre[64];
    strftime(nombre, sizeof(nombre), "Escape-Screenshot-%Y-%m-%d_%H-%M-%S.png",
             t);

    TakeScreenshot(nombre);
  }
}

float Game::nearestEnemyDist(Vector3 p) const {
  float best = 1e9f;
  for (auto &e : enemies)
    best = std::min(best, Vector3Distance(p, e.pos));
  return best;
}

// ---------- HUD ----------
void Game::drawStaminaBar(const Player &pl, int vw, int vh) {
  int barW = std::max(120, vw / 3);
  int barH = 14;
  int bx = vw / 2 - barW / 2;
  int by = vh - 60;

  DrawRectangle(bx - 2, by - 2, barW + 4, barH + 4, (Color){0, 0, 0, 160});
  DrawRectangle(bx, by, barW, barH, (Color){40, 40, 55, 255});

  Color c = pl.exhausted ? RED : (pl.stamina < 0.3f ? ORANGE : GREEN);
  DrawRectangle(bx, by, (int)(barW * pl.stamina), barH, c);
  DrawRectangleLines(bx, by, barW, barH, RAYWHITE);

  const char *label = pl.exhausted ? L("hud.stamina.exhausted", "Sin aliento")
                                   : L("hud.stamina", "Energía");
  int lw = UI::M(label, 14);
  UI::T(label, vw / 2 - lw / 2, by - 18, 14, pl.exhausted ? RED : LIGHTGRAY);
}

void Game::drawMinimap(const Player &pl, int vw) {
  if (!cfg.showMinimap)
    return;
  int mmSize = vw < 900 ? 130 : 190;
  float scale = (float)mmSize / (float)maze.w;
  int ox = vw - mmSize - 20;
  int oy = 20;
  DrawRectangle(ox - 5, oy - 5, mmSize + 10, (int)(maze.h * scale) + 10,
                (Color){0, 0, 0, 170});

  for (int y = 0; y < maze.h; ++y)
    for (int x = 0; x < maze.w; ++x) {
      if (!maze.explored[y][x])
        continue;
      if (maze.g[y][x] == 1)
        DrawRectangle(ox + (int)(x * scale), oy + (int)(y * scale),
                      (int)scale + 1, (int)scale + 1,
                      (Color){90, 100, 130, 255});
    }
  {
    int ex = (int)((exitPos.x / C::CELL) * scale);
    int ez = (int)((exitPos.z / C::CELL) * scale);
    if (maze.explored[(int)(exitPos.z / C::CELL)][(int)(exitPos.x / C::CELL)])
      DrawRectangle(ox + ex, oy + ez, (int)scale + 1, (int)scale + 1, GREEN);
  }
  for (auto &e : enemies) {
    float d = Vector3Distance(e.pos, pl.pos);
    if (d > renderDist() * 0.25f)
      continue;
    DrawRectangle(ox + (int)((e.pos.x / C::CELL) * scale) - 2,
                  oy + (int)((e.pos.z / C::CELL) * scale) - 2, 4, 4, RED);
  }
  int px = ox + (int)((pl.pos.x / C::CELL) * scale);
  int pz = oy + (int)((pl.pos.z / C::CELL) * scale);
  DrawRectangle(px - 2, pz - 2, 5, 5, GOLD);
  Vector2 fwd = {px + sinf(pl.yaw) * 10, pz - cosf(pl.yaw) * 10};
  DrawLine(px, pz, (int)fwd.x, (int)fwd.y, GOLD);
}

void Game::drawGameplayView(const Player &pl, int vw, int vh, int playerNum) {
  Camera3D cam = {};
  cam.position = {pl.pos.x, C::EYE_H, pl.pos.z};
  Vector3 fwd = {sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch),
                 -cosf(pl.yaw) * cosf(pl.pitch)};
  cam.target = Vector3Add(cam.position, fwd);
  cam.up = {0, 1, 0};
  cam.fovy = (float)cfg.fov;
  cam.projection = CAMERA_PERSPECTIVE;

  float mw = maze.w * C::CELL, mh = maze.h * C::CELL;

  BeginMode3D(cam);
  // Suelo
  if (world.floorModel.meshCount > 0) {
    DrawModel(world.floorModel, {mw * 0.5f, 0.0f, mh * 0.5f}, 1.0f, WHITE);
  } else {
    DrawPlane({mw / 2, 0, mh / 2}, {mw, mh}, (Color){32, 32, 42, 255});
  }

  // Paredes: 1 DrawModelEx por celda
  for (int y = 0; y < maze.h; ++y)
    for (int x = 0; x < maze.w; ++x) {
      if (maze.g[y][x] != 1)
        continue;

      float wx = x * C::CELL + C::CELL * 0.5f;
      float wz = y * C::CELL + C::CELL * 0.5f;
      float dx = wx - pl.pos.x, dz = wz - pl.pos.z;
      float rd = renderDist();
      if (dx * dx + dz * dz > rd * rd)
        continue;

      // Tinte por distancia y paridad (anti-mareo)
      float dist = sqrtf(dx * dx + dz * dz);
      float distTint = 1.0f - (dist / rd) * 0.30f;
      if (distTint < 0.35f)
        distTint = 0.35f;
      float parity = ((x + y) & 1) ? 0.93f : 1.0f;
      float tint = distTint * parity;

      Color wallTint = {(unsigned char)(255 * tint),
                        (unsigned char)(255 * tint),
                        (unsigned char)(255 * tint), 255};

      // Elegir modelo segun variante
      int variant = 0;
      if (y < (int)maze.wallVariant.size() &&
          x < (int)maze.wallVariant[y].size())
        variant = maze.wallVariant[y][x];

      const Model *model = &world.wallModelTile;
      if (variant == 1 && world.wallModelLocker.meshCount > 0)
        model = &world.wallModelLocker;
      else if (variant == 2 && world.wallModelBrick.meshCount > 0)
        model = &world.wallModelBrick;

      if (model->meshCount > 0) {
        DrawModelEx(*model, {wx, C::WALL_H * 0.5f, wz}, {0, 1, 0}, 0.0f,
                    {1.0f, 1.0f, 1.0f}, wallTint);
      } else {
        // Fallback sin texturas
        DrawCube({wx, C::WALL_H * 0.5f, wz}, C::CELL, C::WALL_H, C::CELL,
                 wallTint);
      }
    }

  // Techo
  DrawCube({mw / 2, C::WALL_H + 0.05f, mh / 2}, mw, 0.1f, mh,
           (Color){18, 18, 26, 255});

  // Portal
  float pulse = 1.0f + 0.15f * sinf((float)GetTime() * 4.0f);
  DrawCube({exitPos.x, 1.2f, exitPos.z}, 1.6f * pulse, 2.4f, 1.6f * pulse,
           (Color){70, 220, 130, 255});
  DrawCubeWires({exitPos.x, 1.2f, exitPos.z}, 1.8f * pulse, 2.6f, 1.8f * pulse,
                LIME);
  if (assets.hasPortalTex)
    DrawBillboard(cam, assets.portalTex, {exitPos.x, 3.0f, exitPos.z},
                  1.6f * pulse, WHITE);

  for (auto &w : worldItems) {
    float dx = w.pos.x - pl.pos.x, dz = w.pos.z - pl.pos.z;
    if (dx * dx + dz * dz > 35.0f * 35.0f)
      continue;

    float bob = sinf(w.bobT) * C::ROCK_PICKUP_BOB;
    Vector3 dp = {w.pos.x, w.pos.y + bob, w.pos.z};

    float size = 0.55f;
    switch (w.kind) {
    case ITEM_ROCK:
      size = 0.35f;
      break;
    case ITEM_BOX:
      size = 0.70f;
      break;
    case ITEM_LIME:
      size = 0.85f;
      break;
    case ITEM_BICIMAD:
      size = 1.00f;
      break;
    default:
      break;
    }

    if (assets.hasItemTex[w.kind]) {
      DrawBillboard(cam, assets.itemTex[w.kind],
                    {dp.x, dp.y + size * 0.5f, dp.z}, size, WHITE);
    } else {
      Color c = Inventory::itemColor(w.kind);
      DrawCube(dp, size * 0.6f, size * 0.6f, size * 0.6f, c);
      DrawCubeWires(dp, size * 0.6f, size * 0.6f, size * 0.6f,
                    (Color){40, 40, 50, 255});
    }

    if (dx * dx + dz * dz < C::PICKUP_RANGE * C::PICKUP_RANGE * 1.5f) {
      float p2 = 0.35f + 0.10f * sinf((float)GetTime() * 5.0f);
      DrawCubeWires(dp, size * 0.7f + p2, size * 0.7f + p2, size * 0.7f + p2,
                    (Color){255, 240, 120, 200});
    }
  }

  for (int y = 0; y < maze.h; ++y)
    for (int x = 0; x < maze.w; ++x) {
      if (!maze.hasFountain(x, y))
        continue;
      Vector3 fp = {x * C::CELL + C::CELL * 0.5f, 0.0f,
                    y * C::CELL + C::CELL * 0.5f};
      float dx = fp.x - pl.pos.x, dz = fp.z - pl.pos.z;
      if (dx * dx + dz * dz > 40.0f * 40.0f)
        continue;

      DrawCylinder({fp.x, 0.0f, fp.z}, 0.55f, 0.65f, 0.9f, 12,
                   (Color){90, 95, 115, 255});
      DrawCylinderWires({fp.x, 0.0f, fp.z}, 0.55f, 0.65f, 0.9f, 12,
                        (Color){40, 45, 60, 255});

      float ripple = 0.03f * sinf((float)GetTime() * 3.0f + x + y);
      DrawCylinder({fp.x, 0.9f + ripple, fp.z}, 0.55f, 0.55f, 0.05f, 16,
                   (Color){60, 180, 220, 220});
      DrawCube({fp.x, 1.05f, fp.z}, 0.06f, 0.3f, 0.06f,
               (Color){120, 210, 240, 200});
    }

  for (auto &pr : projectiles) {
    float size = 0.25f;
    switch (pr.kind) {
    case ITEM_ROCK:
      size = 0.18f;
      break;
    case ITEM_BOX:
      size = 0.35f;
      break;
    case ITEM_LIME:
      size = 0.80f;
      break;
    case ITEM_BICIMAD:
      size = 1.25;
      break;
    default:
      break;
    }
    if (assets.hasItemTex[pr.kind]) {
      DrawBillboard(cam, assets.itemTex[pr.kind], pr.pos, size, WHITE);
    } else {
      Color c = Inventory::itemColor(pr.kind);
      DrawSphere(pr.pos, size * 0.5f, c);
      DrawSphereWires(pr.pos, size * 0.6f, 4, 4, (Color){220, 220, 220, 180});
    }
  }

  // Enemigos
  float bob = 0.12f * sinf((float)GetTime() * 3.0f);
  for (auto &e : enemies) {
    int kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
    Color tint = WHITE;
    if (e.hitFlash > 0.0f)
      tint = (Color){255, 80, 80, 255};
    else if (e.rageT > 0.0f)
      tint = (Color){255, 140, 90, 255};
    DrawBillboard(cam, assets.enemyTex[kind], {e.pos.x, 1.35f + bob, e.pos.z},
                  2.6f, tint);
    DrawCircle3D({e.pos.x, 0.02f, e.pos.z}, 0.7f, {1, 0, 0}, 90.0f,
                 (Color){0, 0, 0, 90});
  }

  // Otro jugador
  if (twoPlayers) {
    const Player &other = (playerNum == 0) ? players[1] : players[0];
    if (!other.escaped && !other.caught)
      DrawCube({other.pos.x, 1.9f, other.pos.z}, 0.3f, 0.3f, 0.3f,
               playerColor(1 - playerNum));
  }

  if (playerNum == 0)
    drawDebugWorld3D(pl);

  drawHandItem3D(pl, cam);
  EndMode3D();

  DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, 70});

  DrawRectangleGradientV(0, 0, vw, vh / 6, (Color){0, 0, 0, 140},
                         (Color){0, 0, 0, 0});

  drawMinimap(pl, vw);

  int fsize = vw < 900 ? 22 : 32;
  int small = fsize * 5 / 8;
  int hudX = 20, hudY = 20;
  UI::T(TextFormat(L("hud.level", "Nivel %d"), level), hudX, hudY, fsize,
        RAYWHITE);
  UI::T(TextFormat(L("hud.time", "Tiempo: %.1f s"), levelTime), hudX,
        hudY + fsize + 6, small, LIGHTGRAY);
  UI::T(TextFormat(L("hud.enemies", "Enemigos: %d"), (int)enemies.size()), hudX,
        hudY + fsize + 6 + small + 4, small, LIGHTGRAY);

  if (twoPlayers)
    UI::T(TextFormat(L("hud.player", "Jugador %d"), playerNum + 1), hudX,
          vh - small - 10, small, playerColor(playerNum));

  drawStaminaBar(pl, vw, vh);
  drawInventory(pl, vw, vh);

  if (assets.crosshair.id != 0) {
    float scale = 1.0f; // ajusta si la quieres mas grande/pequena
    float cw = (float)assets.crosshair.width * scale;
    float ch = (float)assets.crosshair.height * scale;
    float cx = vw * 0.5f - cw * 0.5f;
    float cy = vh * 0.5f - ch * 0.5f;

    // Solo lo dibujamos si el jugador no esta atrapado/escapado/hidden
    if (!pl.caught && !pl.escaped && !pl.hidden) {
      DrawTexturePro(
          assets.crosshair,
          {0, 0, (float)assets.crosshair.width, (float)assets.crosshair.height},
          {cx, cy, cw, ch}, {0, 0}, 0.0f, (Color){255, 255, 255, 200});
    }
  }

  if (pl.caught) {
    DrawRectangle(0, 0, vw, vh, (Color){120, 0, 0, 90});
    const char *msg = L("hud.caught.waiting", "Atrapado - esperando...");
    UI::T(msg, vw / 2 - UI::M(msg, small) / 2, vh / 2, small, RAYWHITE);
  } else if (pl.escaped) {
    DrawRectangle(0, 0, vw, vh, (Color){0, 80, 40, 90});
    const char *msg = L("hud.escaped.waiting", "Escapaste - esperando...");
    UI::T(msg, vw / 2 - UI::M(msg, small) / 2, vh / 2, small, RAYWHITE);
  }
}

void Game::drawSplitScreen() {
  BeginTextureMode(rtLeft);
  ClearBackground((Color){12, 12, 18, 255});
  drawGameplayView(players[0], rtLeft.texture.width, rtLeft.texture.height, 0);
  EndTextureMode();

  BeginTextureMode(rtRight);
  ClearBackground((Color){12, 12, 18, 255});
  drawGameplayView(players[1], rtRight.texture.width, rtRight.texture.height,
                   1);
  EndTextureMode();
}

void Game::drawCinematic() {
  BeginDrawing();
  ClearBackground(BLACK);
  drawBackground2D();
  cinematic.draw(GetScreenWidth(), GetScreenHeight());
  EndDrawing();
}

// ---------- Menús de config ----------
static const char *yesNo(bool b) {
  return b ? L("common.yes", "Sí") : L("common.no", "No");
}

static const char *pctStr(float v) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100 + 0.5f));
  return buf;
}

void Game::drawConfigHub() {
  drawConfigHeader(L("menu.config", "Configuración"));
  int items = 7;
  handleGamepadMenuNav(items);

  if (uiButton(0, items, 30, L("config.video", "Video"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 0 && consumeConfirm())) {
    state = ST_CONFIG_VIDEO;
    navIndex = 0;
  }
  if (uiButton(1, items, 30, L("config.audio", "Audio"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 1 && consumeConfirm())) {
    state = ST_CONFIG_AUDIO;
    navIndex = 0;
  }
  if (uiButton(2, items, 30, L("config.gameplay", "Jugabilidad"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 2 && consumeConfirm())) {
    state = ST_CONFIG_GAMEPLAY;
    navIndex = 0;
  }
  if (uiButton(3, items, 30, L("config.controls", "Controles"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 3 && consumeConfirm())) {
    state = ST_CONFIG_CONTROLS;
    navIndex = 0;
  }
  if (uiButton(4, items, 30, L("config.access", "Accesibilidad"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 4 && consumeConfirm())) {
    state = ST_CONFIG_ACCESS;
    navIndex = 0;
  }
  if (uiButton(
          5, items, 30,
          TextFormat(L("config.language", "Idioma: %s"), Lang::currentLang()),
          &navIndexCfg, nullptr) ||
      (navIndexCfg == 5 && consumeConfirm())) {
    std::string cur = Lang::currentLang();
    cfg.language = (cur == "es") ? "en" : "es";
    Lang::load(cfg.language.c_str());
    cfg.save();
  }
  if (uiButton(6, items, 30, L("common.back", "Volver"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 6 && consumeConfirm())) {
    state = configReturn;
    if (configReturn == ST_PAUSED)
      navMenu = 0;
  }
  const char *hint =
      L("config.hint", "Sube/baja con D-pad, cambia con < > o A");
  int W = GetScreenWidth();
  UI::T(hint, W / 2 - UI::M(hint, 18) / 2, GetScreenHeight() - 40, 18, GRAY);
}

static const char *fpsLabel(int idx) {
  int v = C::FPS_OPTIONS[idx];
  static char buf[16];
  if (v == 0)
    return L("video.fps.unlimited", "Sin límite");
  snprintf(buf, sizeof(buf), "%d", v);
  return buf;
}
static const char *rdLabel(int idx) {
  switch (idx) {
  case 0:
    return L("video.renderdist.near", "Cerca (40m)");
  case 1:
    return L("video.renderdist.medium", "Media (60m)");
  default:
    return L("video.renderdist.far", "Lejos (100m)");
  }
}
void Game::drawConfigVideo() {
  drawConfigHeader("VIDEO");
  int items = 7;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  auto save = [&]() { cfg.save(); };

#if !defined(__EMSCRIPTEN__)
  if (uiOptionEx(0, items, 30, "Pantalla completa",
                 cfg.fullscreen ? "SI" : "NO", &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.fullscreen = !cfg.fullscreen;
    ToggleFullscreen();
    applyVideoSettings();
    save();
  }
  if (uiOptionEx(1, items, 30, "Resolucion ventana",
                 TextFormat("%dx%d", cfg.windowW, cfg.windowH), &navIndex,
                 nullptr, navIndex == 1 ? lr : 0) != UI_NONE) {
    static const int presets[][2] = {{800, 600},  {1024, 768}, {1280, 720},
                                     {1366, 768}, {1600, 900}, {1920, 1080},
                                     {2560, 1440}};
    const int N = 7;
    int cur = 2;
    for (int i = 0; i < N; ++i)
      if (presets[i][0] == cfg.windowW && presets[i][1] == cfg.windowH)
        cur = i;
    cur = (cur + 1) % N;
    cfg.windowW = presets[cur][0];
    cfg.windowH = presets[cur][1];
    applyVideoSettings();
    save();
  }
#endif
  {
    UIAction a = uiOptionEx(2, items, 30, L("video.fps", "Límite de FPS"),
                            fpsLabel(cfg.fpsLimit), &navIndex, nullptr,
                            navIndex == 2 ? lr : 0);
    if (a == UI_LEFT)
      cfg.fpsLimit = std::max(0, cfg.fpsLimit - 1);
    if (a == UI_RIGHT)
      cfg.fpsLimit = std::min(C::FPS_COUNT - 1, cfg.fpsLimit + 1);
    if (a == UI_CONFIRM)
      cfg.fpsLimit = 2; // 60
    if (a != UI_NONE) {
      applyVideoSettings();
      save();
    }
  }
  {
    UIAction a =
        uiOptionEx(3, items, 30, L("video.renderdist", "Distancia de dibujado"),
                   rdLabel(cfg.renderDistance), &navIndex, nullptr,
                   navIndex == 3 ? lr : 0);
    if (a == UI_LEFT)
      cfg.renderDistance = std::max(0, cfg.renderDistance - 1);
    if (a == UI_RIGHT)
      cfg.renderDistance = std::min(2, cfg.renderDistance + 1);
    if (a == UI_CONFIRM)
      cfg.renderDistance = 1;
    if (a != UI_NONE)
      save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2fx", cfg.guiScale);
    UIAction a =
        uiOptionEx(4, items, 30, L("video.guiscale", "Tamaño de la interfaz"),
                   buf, &navIndex, nullptr, navIndex == 4 ? lr : 0);
    if (a == UI_LEFT)
      cfg.guiScale = std::max(0.75f, cfg.guiScale - 0.05f);
    if (a == UI_RIGHT)
      cfg.guiScale = std::min(2.00f, cfg.guiScale + 0.05f);
    if (a == UI_CONFIRM)
      cfg.guiScale = 1.0f;
    if (a != UI_NONE) {
      UI::scale = cfg.guiScale;
      save();
    }
  }
  if (uiOptionEx(5, items, 30, L("video.vsync", "VSync"), yesNo(cfg.vsync),
                 &navIndex, nullptr, navIndex == 5 ? lr : 0) != UI_NONE) {
    cfg.vsync = !cfg.vsync;
    save();
  }
  if (uiOptionEx(6, items, 30, L("common.back", "Volver"), "", &navIndex,
                 nullptr, 0) != UI_NONE ||
      IsKeyPressed(KEY_ESCAPE))
    state = ST_CONFIG;
}

void Game::drawConfigAudio() {
  drawConfigHeader(L("config.audio", "Audio"));
  int items = 4;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();

  auto applyAndSave = [&]() {
    assets.applyVolumes(cfg.masterVol, cfg.musicVol, cfg.sfxVol);
    cfg.save();
  };

  {
    UIAction a = uiOptionEx(0, items, 40, L("audio.master", "Volumen maestro"),
                            pctStr(cfg.masterVol), &navIndex, nullptr,
                            navIndex == 0 ? lr : 0);
    if (a == UI_LEFT)
      cfg.masterVol = std::max(0.0f, cfg.masterVol - 0.05f);
    if (a == UI_RIGHT)
      cfg.masterVol = std::min(1.0f, cfg.masterVol + 0.05f);
    if (a == UI_CONFIRM)
      cfg.masterVol = 1.0f;
    if (a != UI_NONE)
      applyAndSave();
  }
  {
    UIAction a = uiOptionEx(1, items, 40, L("audio.music", "Música"),
                            pctStr(cfg.musicVol), &navIndex, nullptr,
                            navIndex == 1 ? lr : 0);
    if (a == UI_LEFT)
      cfg.musicVol = std::max(0.0f, cfg.musicVol - 0.05f);
    if (a == UI_RIGHT)
      cfg.musicVol = std::min(1.0f, cfg.musicVol + 0.05f);
    if (a == UI_CONFIRM)
      cfg.musicVol = 0.55f;
    if (a != UI_NONE)
      applyAndSave();
  }
  {
    UIAction a =
        uiOptionEx(2, items, 40, L("audio.sfx", "Efectos"), pctStr(cfg.sfxVol),
                   &navIndex, nullptr, navIndex == 2 ? lr : 0);
    if (a == UI_LEFT)
      cfg.sfxVol = std::max(0.0f, cfg.sfxVol - 0.05f);
    if (a == UI_RIGHT)
      cfg.sfxVol = std::min(1.0f, cfg.sfxVol + 0.05f);
    if (a == UI_CONFIRM)
      cfg.sfxVol = 0.90f;
    if (a != UI_NONE)
      applyAndSave();
  }
  if (uiOptionEx(3, items, 40, L("common.back", "Volver"), "", &navIndex,
                 nullptr, 0) != UI_NONE ||
      IsKeyPressed(KEY_ESCAPE))
    state = ST_CONFIG;
}

void Game::drawConfigGameplay() {
  drawConfigHeader(L("config.gameplay", "Jugabilidad"));
  int items = 9;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  auto save = [&]() { cfg.save(); };

  if (uiOptionEx(0, items, 40, L("gameplay.difficulty", "Dificultad"),
                 cfg.diffName(), &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.difficulty = (cfg.difficulty + 1) % DIFF_COUNT;
    save();
  }
  if (uiOptionEx(1, items, 40, L("gameplay.inverty", "Invertir eje Y"),
                 yesNo(cfg.invertY), &navIndex, nullptr,
                 navIndex == 1 ? lr : 0) != UI_NONE) {
    cfg.invertY = !cfg.invertY;
    save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.mouseSens);
    UIAction a = uiOptionEx(2, items, 40,
                            L("gameplay.mousesens", "Sensibilidad del ratón"),
                            buf, &navIndex, nullptr, navIndex == 2 ? lr : 0);
    if (a == UI_LEFT)
      cfg.mouseSens = std::max(0.10f, cfg.mouseSens - 0.05f);
    if (a == UI_RIGHT)
      cfg.mouseSens = std::min(5.00f, cfg.mouseSens + 0.05f);
    if (a == UI_CONFIRM)
      cfg.mouseSens = 1.0f;
    if (a != UI_NONE)
      save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.stickSens);
    UIAction a = uiOptionEx(3, items, 40,
                            L("gameplay.padsens", "Sensibilidad del mando"),
                            buf, &navIndex, nullptr, navIndex == 3 ? lr : 0);
    if (a == UI_LEFT)
      cfg.stickSens = std::max(0.10f, cfg.stickSens - 0.05f);
    if (a == UI_RIGHT)
      cfg.stickSens = std::min(5.00f, cfg.stickSens + 0.05f);
    if (a == UI_CONFIRM)
      cfg.stickSens = 1.0f;
    if (a != UI_NONE)
      save();
  }
  if (uiOptionEx(4, items, 40, L("gameplay.minimap", "Minimapa"),
                 yesNo(cfg.showMinimap), &navIndex, nullptr,
                 navIndex == 4 ? lr : 0) != UI_NONE) {
    cfg.showMinimap = !cfg.showMinimap;
    save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.sprintDrain);
    UIAction a =
        uiOptionEx(5, items, 40, L("gameplay.sprintdrain", "Consumo de sprint"),
                   buf, &navIndex, nullptr, navIndex == 5 ? lr : 0);
    if (a == UI_LEFT)
      cfg.sprintDrain = std::max(0.10f, cfg.sprintDrain - 0.05f);
    if (a == UI_RIGHT)
      cfg.sprintDrain = std::min(0.80f, cfg.sprintDrain + 0.05f);
    if (a == UI_CONFIRM)
      cfg.sprintDrain = C::STAM_DRAIN_DEF;
    if (a != UI_NONE)
      save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.sprintRegen);
    UIAction a = uiOptionEx(6, items, 40,
                            L("gameplay.sprintregen", "Regeneración de sprint"),
                            buf, &navIndex, nullptr, navIndex == 6 ? lr : 0);
    if (a == UI_LEFT)
      cfg.sprintRegen = std::max(0.05f, cfg.sprintRegen - 0.05f);
    if (a == UI_RIGHT)
      cfg.sprintRegen = std::min(0.60f, cfg.sprintRegen + 0.05f);
    if (a == UI_CONFIRM)
      cfg.sprintRegen = C::STAM_REGEN_DEF;
    if (a != UI_NONE)
      save();
  }
  if (uiOptionEx(7, items, 40, L("gameplay.cinematics", "Cinemáticas"),
                 yesNo(cfg.playCinematics), &navIndex, nullptr,
                 navIndex == 7 ? lr : 0) != UI_NONE) {
    cfg.playCinematics = !cfg.playCinematics;
    save();
  }
  if (uiOptionEx(8, items, 40, L("common.back", "Volver"), "", &navIndex,
                 nullptr, 0) != UI_NONE ||
      IsKeyPressed(KEY_ESCAPE))
    state = ST_CONFIG;
}

void Game::drawConfigControls() {
  drawConfigHeader(L("controls.title", "Controles y mando"));
  int W = GetScreenWidth();

  bool pad0 = IsGamepadAvailable(0);
  bool pad1 = IsGamepadAvailable(1);
  char info[256];
  if (pad0)
    snprintf(info, sizeof(info), L("controls.pad", "Mando %d: %s"), 0,
             GetGamepadName(0));
  else
    snprintf(info, sizeof(info),
             L("controls.pad.none", "Mando %d: no detectado"), 0);
  UI::T(info, W / 2 - UI::M(info, 18) / 2, 100, 18, pad0 ? LIME : GRAY);

  if (pad1)
    snprintf(info, sizeof(info), L("controls.pad", "Mando %d: %s"), 1,
             GetGamepadName(1));
  else
    snprintf(info, sizeof(info),
             L("controls.pad.none", "Mando %d: no detectado"), 1);
  UI::T(info, W / 2 - UI::M(info, 18) / 2, 122, 18, pad1 ? LIME : GRAY);

  int items = 10;
  int row = 0;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();

  if (uiOptionEx(row, items, 40,
                 L("controls.swap", "Intercambiar mandos J1/J2"),
                 yesNo(cfg.swapGamepads), &navIndex, nullptr,
                 navIndex == row ? lr : 0) != UI_NONE) {
    cfg.swapGamepads = !cfg.swapGamepads;
    cfg.save();
    assignGamepads();
  }
  row++;

  struct RowInfo {
    int player, action;
    const char *name;
    int key;
  };
  RowInfo infos[16] = {
      {0, 0, "P1 Adelante", cfg.binds[0].up},
      {0, 1, "P1 Atras", cfg.binds[0].down},
      {0, 2, "P1 Izquierda", cfg.binds[0].left},
      {0, 3, "P1 Derecha", cfg.binds[0].right},
      {0, 4, "P1 Interactuar", cfg.binds[0].interact},
      {0, 5, "P1 Dropear", cfg.binds[0].drop},
      {1, 0, "P2 Adelante", cfg.binds[1].up},
      {1, 1, "P2 Atras", cfg.binds[1].down},
      {1, 2, "P2 Izquierda", cfg.binds[1].left},
      {1, 3, "P2 Derecha", cfg.binds[1].right},
      {1, 4, "P2 Interactuar", cfg.binds[1].interact},
      {1, 5, "P2 Dropear", cfg.binds[1].drop},
  };
  for (int i = 0; i < 16; ++i) {
    const char *lbl =
        (remapAction == infos[i].action && remapPlayer == infos[i].player)
            ? L("controls.presskey", "Pulsa una tecla...")
            : Config::keyName(infos[i].key);
    if (uiOptionEx(row, items, 40, infos[i].name, lbl, &navIndex, nullptr,
                   navIndex == row ? lr : 0) != UI_NONE) {
      remapAction = infos[i].action;
      remapPlayer = infos[i].player;
    }
    row++;
  }
  if (uiOptionEx(row, items, 40, L("common.back", "Volver"), "", &navIndex,
                 nullptr, 0) != UI_NONE ||
      (IsKeyPressed(KEY_ESCAPE) && remapAction < 0))
    state = ST_CONFIG;

  const char *hint =
      remapAction >= 0
          ? L("controls.hint.remap", "Pulsa la tecla a asignar (ESC cancela)")
          : L("controls.hint.pad", "Con mando: stick izq mover, stick der "
                                   "mirar, A confirma, LT sprint");
  UI::T(hint, W / 2 - UI::M(hint, 18) / 2, GetScreenHeight() - 36, 18, GRAY);
}

const char *Game::formatDate(std::time_t t) {
  if (t == 0)
    return "-";
  static char buf[64];
  std::tm *tm_ = std::localtime(&t);
  if (!tm_)
    return "-";
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_);
  return buf;
}

bool Game::consumeCancel() {
  if (IsKeyPressed(KEY_ESCAPE))
    return true;
  for (int p = 0; p < 2; ++p) {
    if (players[p].gamepadId >= 0 &&
        IsGamepadButtonPressed(players[p].gamepadId,
                               GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
      return true;
  }
  return false;
}

void Game::showToast(const char *msg) {
  snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
  toastTimer = 1.8f;
}

void Game::saveCurrentGame() {
  if (currentSlot < 0) {
    showToast(L("toast.noslot", "Sin slot asignado"));
    return;
  }
  SaveSlot s;
  s.level = level;
  s.maxLevel = (mode == MODE_STORY) ? cfg.maxLevel : cfg.maxEndlessLevel;
  s.mode = mode;
  s.twoPlayers = twoPlayers;
  s.playtime = totalPlaytime;
  saves.saveSlot(currentSlot, s);
  showToast(L("toast.saved", "Partida guardada"));
}

void Game::autoSaveProgress() {
  if (currentSlot < 0)
    return;
  SaveSlot s;
  s.level = level;
  s.maxLevel = (mode == MODE_STORY) ? cfg.maxLevel : cfg.maxEndlessLevel;
  s.mode = mode;
  s.twoPlayers = twoPlayers;
  s.playtime = totalPlaytime;
  saves.saveSlot(currentSlot, s);
}

void Game::startNewGame(int slot, GameMode m, bool two) {
  currentSlot = slot;
  totalPlaytime = 0.0f;

  // Guardado inicial
  SaveSlot s;
  s.level = 1;
  s.maxLevel = 1;
  s.mode = m;
  s.twoPlayers = two;
  s.playtime = 0.0f;
  saves.saveSlot(slot, s);

  beginPlay(two, m);
}

void Game::loadGameFromSlot(int slot) {
  SaveSlot s;
  if (!saves.loadSlot(slot, s))
    return;

  currentSlot = slot;
  totalPlaytime = s.playtime;
  level = s.level;
  mode = (GameMode)s.mode;
  twoPlayers = s.twoPlayers;

  // Sin cinemática al cargar
  startLevel(level);
  assignGamepads();
  DisableCursor();
  state = ST_PLAYING;

  if (assets.musicMenu.stream.buffer != nullptr && musicMenuPlaying) {
    StopMusicStream(assets.musicMenu);
    musicMenuPlaying = false;
  }
  if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
    PlayMusicStream(assets.musicLevel);
    musicLevelPlaying = true;
  }
}
void Game::drawModeSelect() {
  drawConfigHeader(L("mode.title", "Elegir modo de juego"));
  int items = 3;
  handleGamepadMenuNav(items);
  if (uiButton(0, items, 60, L("mode.story", "Modo historia"), &navMenu,
               nullptr) ||
      (navMenu == 0 && consumeConfirm())) {
    pendingMode = MODE_STORY;
    state = ST_PLAYER_SELECT;
    navMenu = 0;
  }
  if (uiButton(1, items, 60, L("mode.endless", "Modo infinito"), &navMenu,
               nullptr) ||
      (navMenu == 1 && consumeConfirm())) {
    pendingMode = MODE_ENDLESS;
    state = ST_PLAYER_SELECT;
    navMenu = 0;
  }
  if (uiButton(2, items, 60, L("common.back", "Volver"), &navMenu, nullptr) ||
      (navMenu == 2 && consumeConfirm()) || consumeCancel()) {
    state = ST_MENU;
    navMenu = 0;
  }
  const char *hint =
      L("mode.hint", "Historia: con cinemáticas y progreso. Infinito: sin fin, "
                     "con récord persistente.");
  int W = GetScreenWidth();
  UI::T(hint, W / 2 - UI::M(hint, 18) / 2, GetScreenHeight() - 40, 18, GRAY);
}

void Game::drawPlayerSelect() {
  const char *titulo =
      (pendingMode == MODE_STORY)
          ? L("players.title.story", "Modo historia - jugadores")
          : L("players.title.endless", "Modo infinito - jugadores");
  drawConfigHeader(titulo);
  int items = 3;
  handleGamepadMenuNav(items);
  if (uiButton(0, items, 60, L("players.one", "1 jugador"), &navMenu,
               nullptr) ||
      (navMenu == 0 && consumeConfirm())) {
    pendingTwoPlayers = false;
    slotPurpose = SLOT_NEW;
    state = ST_SLOT_SELECT;
    navSlot = 0;
    // refresca por si acaso
    saves.refresh();
  }
  if (uiButton(1, items, 60,
               L("menu.play2", "Dos jugadores (pantalla dividida)"), &navMenu,
               nullptr) ||
      (navMenu == 1 && consumeConfirm())) {
    pendingTwoPlayers = true;
    slotPurpose = SLOT_NEW;
    state = ST_SLOT_SELECT;
    navSlot = 0;
    saves.refresh();
  }
  if (uiButton(2, items, 60, L("common.back", "Volver"), &navMenu, nullptr) ||
      (navMenu == 2 && consumeConfirm()) || consumeCancel()) {
    state = ST_MODE_SELECT;
    navMenu = 0;
  }
}

void Game::drawSlotSelect() {
  const char *titulo = (slotPurpose == SLOT_NEW)
                           ? L("slot.title.new", "Elegir slot (se sobrescribe)")
                           : L("menu.load", "Cargar partida");
  drawConfigHeader(titulo);

  // Añadimos un boton extra para borrar con click derecho? Mejor no,
  // para mantener simple. Reservamos la navegacion con D-pad.
  int items = SaveManager::NUM_SLOTS + 1; // 3 slots + volver
  handleGamepadMenuNav(items);

  for (int i = 0; i < SaveManager::NUM_SLOTS; ++i) {
    const SaveSlot &s = saves.slot(i);
    char label[256];
    if (s.used) {
      const char *modeStr = (s.mode == MODE_STORY)
                                ? L("slot.mode.story", "Historia")
                                : L("slot.mode.endless", "Infinito");
      snprintf(label, sizeof(label),
               L("slot.used", "Slot %d  -  Nivel %d  -  %s  -  %s"), i + 1,
               s.level, modeStr, formatDate(s.timestamp));
    } else {
      snprintf(label, sizeof(label), L("slot.empty", "Slot %d  -  Vacío"),
               i + 1);
    }

    if (uiButton(i, items, 40, label, &navSlot, nullptr) ||
        (navSlot == i && consumeConfirm())) {
      if (slotPurpose == SLOT_NEW) {
        startNewGame(i, pendingMode, pendingTwoPlayers);
      } else {
        loadGameFromSlot(i);
      }
      return;
    }
  }

  if (uiButton(SaveManager::NUM_SLOTS, items, 40, L("common.back", "Volver"),
               &navSlot, nullptr) ||
      (navSlot == SaveManager::NUM_SLOTS && consumeConfirm()) ||
      consumeCancel()) {
    if (slotPurpose == SLOT_LOAD) {
      state = ST_MENU;
    } else {
      state = ST_PLAYER_SELECT;
    }
    navMenu = 0;
  }

  const char *hint =
      L("slot.hint",
        "Elige un slot. Al empezar una partida nueva se sobrescribe el slot.");
  int W = GetScreenWidth();
  UI::T(hint, W / 2 - UI::M(hint, 18) / 2, GetScreenHeight() - 40, 18, GRAY);
}

void Game::drawPauseMenu() {
  int W = GetScreenWidth(), H = GetScreenHeight();
  DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 200});

  const char *t = L("pause.title", "Pausa");
  UI::T(t, W / 2 - UI::M(t, 64) / 2, (int)(H * 0.12f), 64, RAYWHITE);

  bool canSave = (mode == MODE_STORY && currentSlot >= 0);
  // Guardar siempre permitido si tenemos slot (incluso en infinito)
  canSave = (currentSlot >= 0);

  int items = 5;
  handleGamepadMenuNav(items);

  if (uiButton(0, items, 40, L("pause.resume", "Volver al juego"), &navMenu,
               nullptr) ||
      (navMenu == 0 && consumeConfirm())) {
    DisableCursor();
    state = ST_PLAYING;
  }
  if (canSave) {
    if (uiButton(1, items, 40, L("pause.save", "Guardar partida"), &navMenu,
                 nullptr) ||
        (navMenu == 1 && consumeConfirm())) {
      saveCurrentGame();
    }
  } else {
    // Dibuja un boton deshabilitado para que no se descoloque la lista
    uiButton(1, items, 40,
             L("pause.nosave", "(sin slot - no se puede guardar)"), nullptr,
             nullptr);
  }
  if (uiButton(2, items, 40, L("menu.config", "Configuración"), &navMenu,
               nullptr) ||
      (navMenu == 2 && consumeConfirm())) {
    configReturn = ST_PAUSED;
    state = ST_CONFIG;
    navIndexCfg = 0;
  }
  if (uiButton(3, items, 40, L("pause.menu", "Volver al menú"), &navMenu,
               nullptr) ||
      (navMenu == 3 && consumeConfirm())) {
    autoSaveProgress();
    toMenu();
  }
  if (uiButton(4, items, 40, L("menu.quit", "Salir del juego"), &navMenu,
               nullptr) ||
      (navMenu == 4 && consumeConfirm())) {
    autoSaveProgress();
    shouldQuit = true;
  }

  // HUD mini abajo
  char info[128];
  snprintf(info, sizeof(info),
           L("pause.info", "Nivel %d  -  %.1fs  -  Slot %d"), level, levelTime,
           currentSlot + 1);
  UI::T(info, W / 2 - UI::M(info, 16) / 2, H - 60, 16, LIGHTGRAY);
}

void Game::drawToast() {
  if (toastTimer <= 0.0f)
    return;
  int W = GetScreenWidth();
  float a = std::min(1.0f, toastTimer / 0.4f);
  unsigned char alpha = (unsigned char)(a * 255);
  int tw = UI::M(toastMsg, 24);
  int pad = 20;
  int x = W / 2 - tw / 2 - pad;
  int y = 80;
  DrawRectangle(x, y, tw + pad * 2, 44, (Color){20, 20, 30, alpha});
  DrawRectangleLines(x, y, tw + pad * 2, 44, (Color){100, 220, 120, alpha});
  UI::T(toastMsg, W / 2 - tw / 2, y + 10, 24, (Color){200, 255, 200, alpha});
}
// ---------- Menús principales ----------
void Game::drawMenus() {
  BeginDrawing();
  int W = GetScreenWidth();
  int H = GetScreenHeight();

  if (state == ST_MENU) {
    ClearBackground(BLACK);
    attract.draw(W, H);
    // Overlay oscuro para que el texto se lea bien
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 130});
  } else {
    ClearBackground((Color){12, 12, 18, 255});
    drawBackground2D();
  }

  switch (state) {
  case ST_MENU: {
    if (assets.musicMenu.stream.buffer != nullptr && !musicMenuPlaying) {
      PlayMusicStream(assets.musicMenu);
      musicMenuPlaying = true;
    }
    int W = GetScreenWidth(), H = GetScreenHeight();
    int y = (int)(H * 0.10f);
    const char *t1 = "Escape from Marisa 3:";
    UI::T(t1, W / 2 - UI::M(t1, 54) / 2, y, 54, RAYWHITE);
    y += 54 + 6;
    const char *t2 = "Escape from Bea";
    UI::T(t2, W / 2 - UI::M(t2, 84) / 2, y, 84, RED);
    y += 84 + 14;
    const char *t3 = L("menu.subtitle", "modo serio chat");
    UI::T(t3, W / 2 - UI::M(t3, 22) / 2, y, 22, LIGHTGRAY);

    // Zona clickeable: el titulo completo (t1 + t2)
    int titleTop = (int)(H * 0.10f);
    int titleH = 54 + 6 + 84;
    Rectangle titleRect = {(float)(W / 2 - 400), (float)titleTop, 800.0f,
                           (float)titleH};
    bool hoveringTitle = CheckCollisionPointRec(GetMousePosition(), titleRect);

    if (hoveringTitle) {
      // Cursor de mano + subrayado sutil
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
      DrawRectangleLinesEx(titleRect, 1, (Color){255, 255, 255, 40});
    } else {
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    if (hoveringTitle && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      credits.start();
      state = ST_CREDITS;
    }

    int items = 4;
    handleGamepadMenuNav(items);

    if (uiButton(0, items, 90, L("menu.play", "Jugar"), &navIndex, nullptr) ||
        (navIndex == 0 && consumeConfirm())) {
      state = ST_MODE_SELECT;
      navMenu = 0;
    }
    if (uiButton(1, items, 90, L("menu.load", "Cargar partida"), &navIndex,
                 nullptr) ||
        (navIndex == 1 && consumeConfirm())) {
      slotPurpose = SLOT_LOAD;
      navSlot = 0;
      saves.refresh();
      state = ST_SLOT_SELECT;
    }
    if (uiButton(2, items, 90, L("menu.config", "Configuración"), &navIndex,
                 nullptr) ||
        (navIndex == 2 && consumeConfirm())) {
      configReturn = ST_MENU;
      state = ST_CONFIG;
      navIndexCfg = 0;
    }
    if (uiButton(3, items, 90, L("menu.quit", "Salir del juego"), &navIndex,
                 nullptr) ||
        (navIndex == 3 && consumeConfirm()))
      shouldQuit = true;

    std::string ver = std::string("Escape from Bea ") + Debug::version();
    UI::T(ver.c_str(), 5, H - 25, 20, RAYWHITE);
    break;
  }
  case ST_MODE_SELECT:
    drawModeSelect();
    break;
  case ST_PLAYER_SELECT:
    drawPlayerSelect();
    break;
  case ST_SLOT_SELECT:
    drawSlotSelect();
    break;
  case ST_CONFIG:
    drawConfigHub();
    break;
  case ST_CONFIG_VIDEO:
    drawConfigVideo();
    break;
  case ST_CONFIG_AUDIO:
    drawConfigAudio();
    break;
  case ST_CONFIG_GAMEPLAY:
    drawConfigGameplay();
    break;
  case ST_CONFIG_CONTROLS:
    drawConfigControls();
    break;
  case ST_CONFIG_ACCESS:
    drawConfigAccess();
    break;
  case ST_PAUSED:
    drawPauseMenu();
    break;
  case ST_LEVEL_CLEAR: {
    char buf[192];
    if (mode == MODE_ENDLESS) {
      snprintf(buf, sizeof(buf),
               L("level.stats.endless", "Nivel %d - Récord: %d"), level,
               cfg.maxEndlessLevel);
      drawCenterPanel(L("level.complete", "¡Nivel completado!"), buf,
                      L("level.controls", "ENTER: siguiente nivel ESC: menú"));
    } else if (twoPlayers) {
      snprintf(buf, sizeof(buf), L("level.stats.coop", "J1: %.1fs   J2: %.1fs"),
               players[0].escapeTime, players[1].escapeTime);
      drawCenterPanel(L("level.finished", "¡Nivel superado!"), buf,
                      L("level.controls", "ENTER: siguiente nivel ESC: menú"));
    } else {
      char msg[128];
      snprintf(msg, sizeof(msg), L("level.stats.single", "Nivel %d en %.1f s"),
               level, players[0].escapeTime);
      drawCenterPanel(L("level.finished", "¡Nivel superado!"), msg,
                      L("level.controls", "ENTER: siguiente nivel ESC: menú"));
    }
    break;
  }
  case ST_GAME_OVER: {
    char buf[128];
    if (mode == MODE_ENDLESS) {
      snprintf(buf, sizeof(buf),
               L("level.catched.endless", "Llegaste al nivel %d - Récord: %d"),
               level, cfg.maxEndlessLevel);
      drawCenterPanel(L("level.catched", "TE ATRAPARON"), buf,
                      L("level.retry", "R: reintentar ESC: menú"));
    } else {
      drawCenterPanel(L("level.catched", "TE ATRAPARON"),
                      L("level.catched.longtext",
                        "Bea te encontró por los pasillos del instituto..."),
                      L("level.retry", "R: reintentar ESC: menú"));
    }
    break;
  }
  default:
    break;
  }

  // El toast se dibuja al final, en cualquier estado 2D
  drawToast();
  if (cfg.showFps)
    DrawFPS(GetScreenWidth() - 90, 20);

  EndDrawing();
}

void Game::draw() {
  if (state == ST_PLAYING && twoPlayers) {
    drawSplitScreen();

    BeginDrawing();
    ClearBackground(BLACK);

    // Blit de cada mitad a la pantalla
    DrawTextureRec(
        rtLeft.texture,
        {0, 0, (float)rtLeft.texture.width, -(float)rtLeft.texture.height},
        {0, 0}, WHITE);
    DrawTextureRec(
        rtRight.texture,
        {0, 0, (float)rtRight.texture.width, -(float)rtRight.texture.height},
        {(float)rtLeft.texture.width, 0}, WHITE);

    // Separador central
    DrawRectangle(rtLeft.texture.width - 2, 0, 4, rtH, BLACK);

    // Overlays encima
    drawToast();
    drawDebugOverlay(GetScreenWidth(), GetScreenHeight(), -1);
    if (cfg.showFps && !Debug::showOverlay)
      DrawFPS(GetScreenWidth() - 90, 20);
    EndDrawing();
  } else if (state == ST_PLAYING) {
    BeginDrawing();
    ClearBackground((Color){12, 12, 18, 255});
    drawGameplayView(players[0], GetScreenWidth(), GetScreenHeight(), 0);
    drawToast();
    drawDebugOverlay(GetScreenWidth(), GetScreenHeight(), 0);
    if (cfg.showFps && !Debug::showOverlay)
      DrawFPS(GetScreenWidth() - 90, 20);
    EndDrawing();
  } else if (state == ST_CREDITS) {
    BeginDrawing();
    ClearBackground(BLACK);
    credits.draw(GetScreenWidth(), GetScreenHeight());
    EndDrawing();
  } else if (state == ST_CINEMATIC) {
    drawCinematic();
  } else if (state == ST_JUMPSCARE) {
    drawJumpscare();
  } else {
    drawMenus();
  }
}

void Game::tick() {
  float dt = GetFrameTime();
  update(dt);
  draw();
}

void Game::run() {
  while (!WindowShouldClose() && !shouldQuit) {
    tick();
  }
}

float Game::renderDist() const {
  return C::RENDER_DIST[std::clamp(cfg.renderDistance, 0, 2)];
}

void Game::applyVideoSettings() {
  int fps = C::FPS_OPTIONS[std::clamp(cfg.fpsLimit, 0, C::FPS_COUNT - 1)];
  SetTargetFPS(fps == 0 ? -1 : fps);

#if !defined(__EMSCRIPTEN__)
  if (!cfg.fullscreen) {
    int W = GetMonitorWidth(0), H = GetMonitorHeight(0);
    if (cfg.windowW > W)
      cfg.windowW = W;
    if (cfg.windowH > H)
      cfg.windowH = H;
    SetWindowSize(cfg.windowW, cfg.windowH);
  } else {
    SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
  }
#endif

  UI::scale = cfg.guiScale;
  UI::colorblind = cfg.colorblindMode;

  ensureRenderTargets();
}

static const char *cbLabel(int idx) {
  switch (idx) {
  case 0:
    return L("access.cb.none", "Ninguno");
  case 1:
    return L("access.cb.deuteranopia", "Deuteranopía");
  case 2:
    return L("access.cb.protanopia", "Protanopía");
  default:
    return L("access.cb.tritanopia", "Tritanopía");
  }
}

void Game::drawConfigAccess() {
  drawConfigHeader(L("config.access", "Accesibilidad"));
  int items = 7;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  auto save = [&]() { cfg.save(); };

  if (uiOptionEx(0, items, 30, L("access.colorblind", "Modo daltonismo"),
                 cbLabel(cfg.colorblindMode), &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.colorblindMode = (cfg.colorblindMode + 1) % CB_COUNT;
    UI::colorblind = cfg.colorblindMode;
    save();
  }
  if (uiOptionEx(1, items, 30, L("access.contrast", "Alto contraste"),
                 yesNo(cfg.highContrast), &navIndex, nullptr,
                 navIndex == 1 ? lr : 0) != UI_NONE) {
    cfg.highContrast = !cfg.highContrast;
    save();
  }
  if (uiOptionEx(2, items, 30, L("access.motion", "Reducir movimiento"),
                 yesNo(cfg.reduceMotion), &navIndex, nullptr,
                 navIndex == 2 ? lr : 0) != UI_NONE) {
    cfg.reduceMotion = !cfg.reduceMotion;
    save();
  }
  if (uiOptionEx(3, items, 30, L("access.subtitles", "Subtítulos grandes"),
                 yesNo(cfg.bigSubtitles), &navIndex, nullptr,
                 navIndex == 3 ? lr : 0) != UI_NONE) {
    cfg.bigSubtitles = !cfg.bigSubtitles;
    save();
  }
  if (uiOptionEx(4, items, 30, L("access.sprint", "Sprint (mantener/alternar)"),
                 cfg.holdToSprint ? L("access.sprint.hold", "Mantener")
                                  : L("access.sprint.toggle", "Alternar"),
                 &navIndex, nullptr, navIndex == 4 ? lr : 0) != UI_NONE) {
    cfg.holdToSprint = !cfg.holdToSprint;
    save();
  }
  if (uiOptionEx(5, items, 30, L("access.jumpscares", "Sin sustos"),
                 cfg.noJumpscares ? L("common.yes", "SI")
                                  : L("common.no", "NO"),
                 &navIndex, nullptr, navIndex == 5 ? lr : 0) != UI_NONE) {
    cfg.noJumpscares = !cfg.noJumpscares;
    save();
  }
  if (uiOptionEx(6, items, 30, L("common.back", "Volver"), "", &navIndex,
                 nullptr, 0) != UI_NONE ||
      IsKeyPressed(KEY_ESCAPE))
    state = ST_CONFIG;
}

void Game::handleDebugHotkeys() {
  // Overlay: solo en gameplay o pausa
  if (state == ST_PLAYING || state == ST_PAUSED) {
    if (IsKeyPressed(KEY_F3))
      Debug::showOverlay = !Debug::showOverlay;
    if (IsKeyPressed(KEY_F4))
      Debug::showBounds = !Debug::showBounds;
    if (IsKeyPressed(KEY_F5))
      Debug::showVision = !Debug::showVision;
    if (IsKeyPressed(KEY_F6))
      Debug::showPaths = !Debug::showPaths;
    if (IsKeyPressed(KEY_F7))
      Debug::godMode = !Debug::godMode;
    if (IsKeyPressed(KEY_F8))
      Debug::infiniteStamina = !Debug::infiniteStamina;
    if (IsKeyPressed(KEY_F9))
      Debug::noClip = !Debug::noClip;

    // Acciones (solo en playing, no en pausa)
    if (state == ST_PLAYING) {
      if (IsKeyPressed(KEY_F10))
        debugRevealMap();
      if (IsKeyPressed(KEY_F11))
        debugTeleportToExit();
      if (IsKeyPressed(KEY_KP_DECIMAL))
        Debug::cycleTimeScale();
      if (IsKeyPressed(KEY_KP_0))
        debugSpawnEnemy();
      if (IsKeyPressed(KEY_KP_1))
        debugKillAll();
      if (IsKeyPressed(KEY_KP_2)) {
        startLevel(level);
        assignGamepads();
        DisableCursor();
        showToast("Nivel reiniciado");
      }
      if (IsKeyPressed(KEY_KP_3)) {
        ++level;
        startLevel(level);
        assignGamepads();
        DisableCursor();
        showToast(TextFormat("Nivel %d", level));
      }
      if (IsKeyPressed(KEY_KP_4) && level > 1) {
        --level;
        startLevel(level);
        assignGamepads();
        DisableCursor();
        showToast(TextFormat("Nivel %d", level));
      }
    }
  }
}

void Game::debugSpawnEnemy() {
  if (enemies.size() >= 20) {
    showToast("Limite de enemigos (20)");
    return;
  }
  Enemy e;
  e.pos = players[0].pos;
  // Desplaza un poco para que no aparezca encima
  e.pos.x += 1.5f;
  e.pos.z += 1.5f;
  if (maze.wallAtWorld(e.pos.x, e.pos.z)) {
    e.pos.x = players[0].pos.x - 1.5f;
    e.pos.z = players[0].pos.z - 1.5f;
  }
  e.dir = {1, 0};
  e.speed = C::BASE_ESPD;
  e.kind = (int)(enemies.size() % ENEMY_KIND_COUNT);
  e.prevPos = e.pos;
  e.memoryT = 0.0f;
  e.repathT = 0.0f;
  e.stuckT = 0.0f;
  e.stunT = 0.0f;
  e.rageT = 0.0f;
  e.rageMult = 1.0f;
  e.hitFlash = 0.0f;
  enemies.push_back(e);
  showToast("Enemigo spawneado");
}

void Game::debugKillAll() {
  int n = (int)enemies.size();
  enemies.clear();
  showToast(TextFormat("Eliminados %d enemigos", n));
}

void Game::debugTeleportToExit() {
  if (players[0].caught || players[0].escaped)
    return;
  players[0].pos = exitPos;
  showToast("Teletransportado a la salida");
}

void Game::debugRevealMap() {
  for (int y = 0; y < maze.h; ++y)
    for (int x = 0; x < maze.w; ++x)
      maze.explored[y][x] = true;
  showToast("Mapa revelado");
}

void Game::drawDebugWorld3D(const Player &pl) {
  (void)pl;
  if (!Debug::showOverlay)
    return;

  // --- Bounds ---
  if (Debug::showBounds) {
    for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
      if (players[i].caught || players[i].escaped)
        continue;
      DrawCircle3D({players[i].pos.x, 0.05f, players[i].pos.z}, C::P_RADIUS,
                   {1, 0, 0}, 90.0f, (Color){255, 220, 80, 220});
    }
    for (auto &e : enemies) {
      Color c = (e.rageT > 0.0f)   ? RED
                : (e.stunT > 0.0f) ? GRAY
                                   : (Color){80, 180, 255, 220};
      DrawCircle3D({e.pos.x, 0.05f, e.pos.z}, C::E_RADIUS, {1, 0, 0}, 90.0f, c);
    }
    // Circulo de la distancia de captura
    for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
      if (players[i].caught || players[i].escaped)
        continue;
      DrawCircle3D({players[i].pos.x, 0.06f, players[i].pos.z}, C::CATCH_R,
                   {1, 0, 0}, 90.0f, (Color){255, 0, 0, 80});
    }
  }

  // --- Vision ---
  if (Debug::showVision) {
    for (auto &e : enemies) {
      Vector3 eye = {e.pos.x, 1.4f, e.pos.z};

      // Busca un objetivo visible
      int target = -1;
      for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
        if (players[i].caught || players[i].escaped || players[i].hidden)
          continue;
        if (Vector3Distance(e.pos, players[i].pos) < C::SEE_DIST &&
            lineOfSight(maze, e.pos, players[i].pos)) {
          target = i;
          break;
        }
      }

      if (target >= 0) {
        // Verde: tiene LOS directa
        Vector3 tgt = {players[target].pos.x, 1.4f, players[target].pos.z};
        DrawLine3D(eye, tgt, (Color){0, 255, 0, 200});
      } else if (e.memoryT > 0.0f) {
        // Amarillo: memoria (última pos conocida)
        Vector3 mem = {e.lastSeen.x, 1.4f, e.lastSeen.z};
        DrawLine3D(eye, mem, (Color){255, 220, 0, 180});
      } else {
        // Blanco: dirección de deambulación
        Vector3 fwd = {eye.x + e.dir.x * 3.0f, eye.y, eye.z + e.dir.y * 3.0f};
        DrawLine3D(eye, fwd, (Color){200, 200, 200, 150});
      }
    }
  }

  // --- Paths ---
  if (Debug::showPaths) {
    // Dibuja una línea desde cada enemigo hacia su lastSeen si tiene memoria
    for (auto &e : enemies) {
      if (e.memoryT <= 0.0f)
        continue;
      DrawLine3D({e.pos.x, 0.2f, e.pos.z}, {e.lastSeen.x, 0.2f, e.lastSeen.z},
                 (Color){255, 100, 255, 180});
    }
    // Cruces en los enemigos
    for (auto &e : enemies) {
      float s = 0.3f;
      DrawLine3D({e.pos.x - s, 0.1f, e.pos.z}, {e.pos.x + s, 0.1f, e.pos.z},
                 MAGENTA);
      DrawLine3D({e.pos.x, 0.1f, e.pos.z - s}, {e.pos.x, 0.1f, e.pos.z + s},
                 MAGENTA);
    }
  }
}

void Game::drawDebugOverlay(int vw, int vh, int playerNum) {
  (void)playerNum;
  if (!Debug::showOverlay)
    return;

  const int pad = 10;
  const int lineH = 16;
  const int font = 14;
  const int width = 460;

  // Calcula lineas segun contenido
  int lines = 0;
  lines += 3; // header
  lines += 3; // runtime
  lines += 6; // player 1
  if (twoPlayers)
    lines += 6;                                       // player 2
  lines += 4;                                         // world
  lines += 2 + std::min<int>((int)enemies.size(), 6); // enemigos (max 6)
  if (enemies.size() > 6)
    lines += 1;   // linea "y N mas..."
  lines += 1 + 8; // cheats

  int height = pad * 2 + lines * lineH;

  // Fondo
  DrawRectangle(12, 12, width, height, (Color){0, 0, 0, 210});
  DrawRectangleLines(12, 12, width, height, (Color){80, 255, 100, 220});

  int tx = 12 + pad;
  int ty = 12 + pad;

  auto line = [&](Color c, const char *txt) {
    DrawText(txt, tx, ty, font, c);
    ty += lineH;
  };

  const Color titleC = (Color){100, 255, 120, 255};
  const Color labelC = (Color){180, 180, 180, 255};
  const Color valC = (Color){240, 240, 240, 255};
  const Color okC = (Color){120, 255, 120, 255};
  const Color offC = (Color){130, 130, 130, 255};
  const Color warnC = (Color){255, 200, 80, 255};

  char buf[256];

  // --- Header ---
  line(titleC, TextFormat("%s  %s", C::GAME_TITLE, Debug::version()));
  snprintf(buf, sizeof(buf), "Build: %s  |  %s", Debug::buildString(),
           Debug::compilerName());
  line(labelC, buf);
  line(labelC, "F3: toggle overlay   |   Numpad: cheats (ver abajo)");

  // --- Runtime ---
  snprintf(buf, sizeof(buf), "FPS: %d   Frame: %.2f ms   TimeScale: %.2fx",
           GetFPS(), GetFrameTime() * 1000.0f, Debug::timeScale);
  line(valC, buf);
  snprintf(buf, sizeof(buf), "Estado: %d   Modo: %s   Jugadores: %d",
           (int)state, (mode == MODE_STORY ? "Historia" : "Infinito"),
           twoPlayers ? 2 : 1);
  line(valC, buf);
  snprintf(buf, sizeof(buf), "Nivel: %d   Tiempo: %.2fs   Playtime: %.1fs",
           level, levelTime, totalPlaytime);
  line(valC, buf);

  // --- Jugadores ---
  for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
    const Player &p = players[i];
    Vector2 cell = maze.worldToCell(p.pos.x, p.pos.z);

    Color hc =
        (i == 0) ? (Color){255, 200, 80, 255} : (Color){120, 200, 255, 255};
    snprintf(buf, sizeof(buf), "--- JUGADOR %d ---", i + 1);
    line(hc, buf);
    snprintf(buf, sizeof(buf), "Pos: (%.2f, %.2f)  Cell: (%d, %d)", p.pos.x,
             p.pos.z, (int)cell.x, (int)cell.y);
    line(valC, buf);
    snprintf(buf, sizeof(buf), "Yaw: %.1f  Pitch: %.1f  Stamina: %.0f%%",
             p.yaw * 180.0f / PI, p.pitch * 180.0f / PI, p.stamina * 100.0f);
    line(valC, buf);

    const InvItem &sel = p.inventory[p.selectedSlot];
    const char *iname = (sel.kind == ITEM_NONE || sel.count <= 0)
                            ? "-"
                            : Inventory::itemName(sel.kind);
    snprintf(buf, sizeof(buf), "Slot %d: %s x%d  Sprint: %s  Cd: %.2f",
             p.selectedSlot + 1, iname, sel.count, p.sprinting ? "SI" : "no",
             p.throwCooldown);
    line(valC, buf);

    snprintf(buf, sizeof(buf), "Flags: %s%s%s", p.caught ? "[CAUGHT] " : "",
             p.escaped ? "[ESCAPED] " : "", p.exhausted ? "[EXHAUSTED]" : "");
    line(p.caught ? warnC : valC, buf);
  }

  // --- Mundo ---
  line(titleC, "--- MUNDO ---");
  snprintf(buf, sizeof(buf), "Maze: %dx%d  Seed: 0x%08X", maze.w, maze.h,
           currentSeed);
  line(valC, buf);
  snprintf(buf, sizeof(buf),
           "Salida: (%.1f, %.1f)  Enemigos: %d  Proyectiles: %d", exitPos.x,
           exitPos.z, (int)enemies.size(), (int)projectiles.size());
  line(valC, buf);
  snprintf(buf, sizeof(buf), "Render dist: %.0fm   GUI scale: %.2fx",
           renderDist(), cfg.guiScale);
  line(valC, buf);

  // --- Enemigos ---
  snprintf(buf, sizeof(buf), "--- ENEMIGOS (%d) ---", (int)enemies.size());
  line(titleC, buf);
  int shown = std::min<int>((int)enemies.size(), 6);
  for (int i = 0; i < shown; ++i) {
    const Enemy &e = enemies[i];
    const char *kind = "BEA";
    if (e.kind == ENEMY_MARISA)
      kind = "MAR";
    else if (e.kind == ENEMY_ALT)
      kind = "ALT";

    float d0 = Vector3Distance(e.pos, players[0].pos);
    const char *st = "wander";
    if (e.stunT > 0)
      st = "STUN";
    else if (e.rageT > 0)
      st = "RAGE";
    else if (e.memoryT > 0)
      st = "chase";

    snprintf(buf, sizeof(buf),
             "#%d %s  d=%.1fm  spd=%.2f  %s  mem=%.1f  rage=%.1f", i, kind, d0,
             e.speed * e.rageMult, st, e.memoryT, e.rageT);
    line(e.rageT > 0 ? warnC : valC, buf);
  }
  if ((int)enemies.size() > 6) {
    snprintf(buf, sizeof(buf), "... y %d mas", (int)enemies.size() - 6);
    line(offC, buf);
  }

  // --- Cheats ---
  line(titleC, "--- CHEATS / VISUALIZACION ---");
  auto toggleStr = [](bool b) { return b ? "ON " : "off"; };
  snprintf(buf, sizeof(buf), "[F4] Bounds:%s  [F5] Vision:%s  [F6] Paths:%s",
           toggleStr(Debug::showBounds), toggleStr(Debug::showVision),
           toggleStr(Debug::showPaths));
  line(valC, buf);
  snprintf(buf, sizeof(buf), "[F7] GodMode:%s  [F8] InfStam:%s  [F9] NoClip:%s",
           toggleStr(Debug::godMode), toggleStr(Debug::infiniteStamina),
           toggleStr(Debug::noClip));
  line(valC, buf);
  snprintf(buf, sizeof(buf),
           "[F10] Reveal   [F11] Teleport   [NUM.] TimeScale");
  line(valC, buf);
  snprintf(buf, sizeof(buf), "[NUM0] Spawn   [NUM1] Kill all   [NUM2] Restart");
  line(valC, buf);
  snprintf(buf, sizeof(buf), "[NUM3] Next lvl   [NUM4] Prev lvl");
  line(valC, buf);
}

void Game::startJumpscare(int enemyIdx) {
  jumpscareEnemy = enemyIdx;
  jumpscareTimer = C::JUMPSCARE_DURATION;
  state = ST_JUMPSCARE;
  EnableCursor(); // libera cursor por si el usuario quiere saltar
}

void Game::drawJumpscare() {
  BeginDrawing();
  ClearBackground(BLACK);

  if (jumpscareEnemy < 0 || jumpscareEnemy >= (int)enemies.size()) {
    EndDrawing();
    return;
  }

  const Enemy &e = enemies[jumpscareEnemy];
  int kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
  Texture2D tex = assets.enemyTex[kind];

  int W = GetScreenWidth();
  int H = GetScreenHeight();

  // Progreso 0..1
  float t = 1.0f - (jumpscareTimer / C::JUMPSCARE_DURATION);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  // Fase 1: zoom brusco de 0 a JUMPSCARE_ZOOM_IN
  // Fase 2: mantener + shake + flash
  float sizeScale;
  if (t < C::JUMPSCARE_ZOOM_IN) {
    // Rápido acercamiento
    float k = t / C::JUMPSCARE_ZOOM_IN;
    // Easing out (arranca rápido, frena un poco)
    k = 1.0f - (1.0f - k) * (1.0f - k);
    sizeScale = 0.5f + k * 1.2f; // empieza al 50%, acaba al 170%
  } else {
    // Fase 2: mantiene tamaño + shake
    sizeScale = 1.7f;
  }

  // Shake solo en fase 2
  float shake = 0.0f;
  if (t >= C::JUMPSCARE_ZOOM_IN) {
    float k = (t - C::JUMPSCARE_ZOOM_IN) / (1.0f - C::JUMPSCARE_ZOOM_IN);
    shake = (1.0f - k) * 14.0f; // decae al final
  }

  float dx = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;
  float dy = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;

  // Centro + tamano
  float drawSize = (float)H * sizeScale;
  float x = W / 2.0f + dx;
  float y = H / 2.0f + dy;

  // Aplicar tinte de hit flash al principio
  Color tint = WHITE;
  if (t < 0.2f) {
    unsigned char k = (unsigned char)((1.0f - t / 0.2f) * 100);
    tint =
        (Color){255, (unsigned char)(255 - k), (unsigned char)(255 - k), 255};
  }

  // Dibujar el sprite centrado, escalado
  if (tex.id != 0) {
    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {x - drawSize / 2.0f, y - drawSize / 2.0f, drawSize,
                     drawSize};
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
  } else {
    // Fallback: circulo gigante con el color del enemigo
    Color c = (kind == ENEMY_BEA)      ? (Color){210, 45, 60, 255}
              : (kind == ENEMY_MARISA) ? (Color){60, 90, 210, 255}
                                       : (Color){160, 40, 200, 255};
    DrawCircle((int)x, (int)y, drawSize * 0.4f, c);
  }

  // Flash blanco al inicio
  if (t < 0.08f) {
    unsigned char a = (unsigned char)((1.0f - t / 0.08f) * 255);
    DrawRectangle(0, 0, W, H, (Color){255, 255, 255, a});
  }

  // Oscurecimiento al final (transición a game over)
  if (t > 0.85f) {
    float k = (t - 0.85f) / 0.15f;
    unsigned char a = (unsigned char)(k * 255);
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, a});
  }

  EndDrawing();
}

void Game::spawnWorldItems(int lvl, int size, std::mt19937 &rng) {
  int count = std::min(3 + lvl / 2, 8);

  float minDistFromSpawn = size * C::CELL * 0.20f;

  struct Weighted {
    ItemKind kind;
    int weight;
  };
  const Weighted pool[] = {
      {ITEM_ROCK, 50},
      {ITEM_BOX, 20},
      {ITEM_LIME, 20},
      {ITEM_BICIMAD, 10},
  };
  const int POOL_SIZE = sizeof(pool) / sizeof(pool[0]);

  int totalWeight = 0;
  for (int i = 0; i < POOL_SIZE; ++i)
    totalWeight += pool[i].weight;

  auto rollKind = [&]() -> ItemKind {
    int r = (int)(rng() % totalWeight);
    int acc = 0;
    for (int i = 0; i < POOL_SIZE; ++i) {
      acc += pool[i].weight;
      if (r < acc)
        return pool[i].kind;
    }
    return ITEM_ROCK;
  };

  int attempts = 0;
  while ((int)worldItems.size() < count && attempts < 800) {
    ++attempts;
    int cx = 1 + (int)(rng() % (size - 2));
    int cy = 1 + (int)(rng() % (size - 2));
    if (maze.wallAt(cx, cy))
      continue;
    if (maze.hasFountain(cx, cy))
      continue;

    Vector3 p = maze.cellCenter(cx, cy);
    if (Vector3Distance(p, players[0].pos) < minDistFromSpawn)
      continue;

    bool overlap = false;
    for (auto &w : worldItems)
      if (Vector3Distance(p, w.pos) < C::CELL * 0.4f) {
        overlap = true;
        break;
      }
    if (overlap)
      continue;

    WorldPickup wp;
    wp.kind = rollKind();
    wp.pos = {p.x, 0.35f, p.z};
    wp.bobT = (float)(rng() % 100) / 100.0f * 6.28f;
    worldItems.push_back(wp);
  }
}

void Game::updateWorldItems(float dt) {
  for (auto &w : worldItems)
    w.bobT += dt * 2.0f;
}

int Game::findInteractable(const Player &pl, int &outCx, int &outCy,
                           bool &isLocker) const {
  Vector3 fwd = {sinf(pl.yaw), 0.0f, -cosf(pl.yaw)};
  Vector3 probe = {pl.pos.x + fwd.x * C::INTERACT_RANGE, 0.0f,
                   pl.pos.z + fwd.z * C::INTERACT_RANGE};
  int cx = (int)floorf(probe.x / C::CELL);
  int cy = (int)floorf(probe.z / C::CELL);
  outCx = cx;
  outCy = cy;

  // Taquilla: pared con variante 1
  if (maze.wallAt(cx, cy)) {
    int variant = 0;
    if (cy < (int)maze.wallVariant.size() &&
        cx < (int)maze.wallVariant[cy].size())
      variant = maze.wallVariant[cy][cx];
    if (variant == 1) {
      isLocker = true;
      return 1;
    }
    return 0;
  }

  // Fuente
  if (maze.hasFountain(cx, cy)) {
    isLocker = false;
    return 2;
  }
  return 0;
}

void Game::handleInteraction(int p) {
  Player &pl = players[p];
  if (pl.caught || pl.escaped)
    return;

  bool interactPressed = IsKeyPressed(cfg.binds[p].interact) ||
                         (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));

  // ---- Salir de la taquilla ----
  if (pl.hidden) {
    if (interactPressed) {
      pl.hidden = false;
      pl.pos = pl.hiddenExitPos;
      pl.yaw = pl.hiddenExitYaw;
      pl.hiddenCellX = -1;
      pl.hiddenCellY = -1;
      if (p == 0)
        showToast(L("toast.locker.leave", "Saliste de la taquilla"));
    }
    return;
  }

  if (fountainCooldown[p] > 0.0f)
    return;
  if (!interactPressed)
    return;

  // 1) Recoger item cercano
  int idx = Inventory::nearestPickup(pl.pos, worldItems, C::PICKUP_RANGE);
  if (idx >= 0) {
    ItemKind kind = worldItems[idx].kind;
    if (Inventory::collectPickup(pl, worldItems, idx)) {
      if (assets.sfxPickup.frameCount > 0)
        PlaySound(assets.sfxPickup);
      if (p == 0)
        showToast(TextFormat("%s", Inventory::itemName(kind)));
    } else {
      if (p == 0)
        showToast(L("toast.inventory.full", "Inventario lleno"));
    }
    return;
  }

  // 2) Taquilla / fuente
  int cx, cy;
  bool isLocker;
  int what = findInteractable(pl, cx, cy, isLocker);
  if (what == 0)
    return;

  if (what == 1) {
    // Esconderse
    pl.hidden = true;
    pl.hiddenExitPos = pl.pos;
    pl.hiddenExitYaw = pl.yaw;
    pl.hiddenCellX = cx;
    pl.hiddenCellY = cy;
    pl.yaw += PI;
    while (pl.yaw > PI)
      pl.yaw -= 2.0f * PI;
    while (pl.yaw < -PI)
      pl.yaw += 2.0f * PI;
    pl.sprinting = false;
    if (p == 0)
      showToast(
          TextFormat(L("toast.locker.hidden", "Escondido. [%s/RMB] salir"),
                     Config::keyName(cfg.binds[p].interact)));
  } else if (what == 2) {
    // Beber agua
    if (pl.stamina < C::STAM_WATER_MAX - 0.01f) {
      pl.stamina = C::STAM_WATER_MAX;
      pl.exhausted = false;
      pl.staminaDelay = 0.0f;
      fountainCooldown[p] = C::WATER_COOLDOWN;
      if (p == 0)
        showToast(L("toast.energy.full", "Energia al maximo!"));
    } else {
      if (p == 0)
        showToast(L("toast.energy.alreadyfull", "Ya tienes la energia llena"));
      fountainCooldown[p] = 0.3f;
    }
  }
}

void Game::handleItemInput(int p) {
  Player &pl = players[p];
  if (pl.caught || pl.escaped)
    return;

  // Cambiar de slot
  if (p == 0) {
    for (int i = 0; i < C::INV_SLOTS; ++i)
      if (IsKeyPressed(KEY_ONE + i))
        pl.selectedSlot = i;

    float wheel = GetMouseWheelMove();
    if (wheel > 0.0f)
      pl.selectedSlot = (pl.selectedSlot - 1 + C::INV_SLOTS) % C::INV_SLOTS;
    else if (wheel < 0.0f)
      pl.selectedSlot = (pl.selectedSlot + 1) % C::INV_SLOTS;
  }

  // Dropear
  if (IsKeyPressed(cfg.binds[p].drop)) {
    if (Inventory::dropSelected(pl, maze, worldItems)) {
      if (assets.sfxPickup.frameCount > 0) {
        SetSoundPitch(assets.sfxPickup, 0.7f); // más grave
        PlaySound(assets.sfxPickup);
        SetSoundPitch(assets.sfxPickup, 1.0f);
      }
      if (p == 0)
        showToast(L("toast.item.dropped", "Item dropeado"));
    }
  }

  // Usar (lanzar) el item seleccionado
  bool usePressed = IsKeyPressed(cfg.binds[p].interact == 0 ? KEY_E : KEY_E) ||
                    (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
  // Nota: usar la tecla E explicitamente. Si quieres que sea remapeable,
  // anade un campo "use" a KeyBindings.
  usePressed = (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
               IsKeyPressed(KEY_E);

  if (usePressed) {
    if (Inventory::useSelected(pl, maze, projectiles)) {
      if (assets.sfxThrow.frameCount > 0)
        PlaySound(assets.sfxThrow);
    }
  }
}

void Game::drawInventory(const Player &pl, int vw, int vh) {
  const int slotSize = std::max(42, vw / 32);
  const int gap = 6;
  const int pad = 10;

  int totalH = C::INV_SLOTS * slotSize + (C::INV_SLOTS - 1) * gap;
  int startX = pad + 4;
  int startY = vh / 2 - totalH / 2;

  for (int i = 0; i < C::INV_SLOTS; ++i) {
    int x = startX;
    int y = startY + i * (slotSize + gap);

    Rectangle r = {(float)x, (float)y, (float)slotSize, (float)slotSize};

    bool selected = (i == pl.selectedSlot);
    Color bg = selected ? (Color){60, 70, 100, 200} : (Color){20, 22, 32, 170};
    Color border =
        selected ? (Color){255, 240, 140, 230} : (Color){90, 100, 130, 180};

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, selected ? 2.0f : 1.0f, border);

    DrawText(TextFormat("%d", i + 1), x + 4, y + 2, 12,
             (Color){180, 180, 180, 180});

    const InvItem &it = pl.inventory[i];
    if (it.kind != ITEM_NONE && it.count > 0) {
      int isz = slotSize - 16;
      int ix = x + (slotSize - isz) / 2;
      int iy = y + (slotSize - isz) / 2;

      if (assets.hasItemTex[it.kind]) {
        Texture2D tex = assets.itemTex[it.kind];
        DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height},
                       {(float)ix, (float)iy, (float)isz, (float)isz}, {0, 0},
                       0.0f, WHITE);
      } else {
        Color itemCol = Inventory::itemColor(it.kind);
        DrawRectangle(ix, iy, isz, isz, itemCol);
        DrawRectangleLines(ix, iy, isz, isz, (Color){40, 40, 50, 200});
      }

      if (it.count > 1) {
        const char *cnt = TextFormat("%d", it.count);
        int fs = 14;
        int tw = MeasureText(cnt, fs);
        DrawText(cnt, x + slotSize - tw - 4, y + slotSize - fs - 3, fs,
                 RAYWHITE);
      }
    }
  }
}

void Game::drawHandItem3D(const Player &pl, const Camera3D &cam) {
  if (pl.caught || pl.escaped || pl.hidden)
    return;
  int slot = pl.selectedSlot;
  if (slot < 0 || slot >= C::INV_SLOTS)
    return;

  const InvItem &it = pl.inventory[slot];
  if (it.kind == ITEM_NONE || it.count <= 0)
    return;

  // ---------- Ejes de la camara ----------
  Vector3 fwd = {cam.target.x - cam.position.x, cam.target.y - cam.position.y,
                 cam.target.z - cam.position.z};
  float fwdLen = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
  if (fwdLen < 0.0001f)
    return;
  fwd.x /= fwdLen;
  fwd.y /= fwdLen;
  fwd.z /= fwdLen;

  // right = fwd x up
  Vector3 right = {fwd.y * cam.up.z - fwd.z * cam.up.y,
                   fwd.z * cam.up.x - fwd.x * cam.up.z,
                   fwd.x * cam.up.y - fwd.y * cam.up.x};
  float rightLen =
      sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
  if (rightLen < 0.0001f)
    return;
  right.x /= rightLen;
  right.y /= rightLen;
  right.z /= rightLen;

  // upReal = right x fwd
  Vector3 up = {right.y * fwd.z - right.z * fwd.y,
                right.z * fwd.x - right.x * fwd.z,
                right.x * fwd.y - right.y * fwd.x};

  // ---------- Offset en espacio de camara ----------
  float dFwd = 0.55f;
  float dRight = 0.30f;
  float dDown = -0.22f;

  // Bob del caminar
  if (pl.footstepT > 0.0f) {
    float phase = (C::STEP_SLOW - pl.footstepT) * 14.0f;
    dDown += sinf(phase) * 0.035f;
    dRight += cosf(phase) * 0.015f;
  }

  Vector3 pos = {
      cam.position.x + fwd.x * dFwd + right.x * dRight + up.x * dDown,
      cam.position.y + fwd.y * dFwd + right.y * dRight + up.y * dDown,
      cam.position.z + fwd.z * dFwd + right.z * dRight + up.z * dDown};

  // ---------- Tamano por item ----------
  float size = 0.22f;
  switch (it.kind) {
  case ITEM_ROCK:
    size = 0.16f;
    break;
  case ITEM_BOX:
    size = 0.26f;
    break;
  case ITEM_LIME:
    size = 0.45f;
    break;
  case ITEM_BICIMAD:
    size = 0.60f;
    break;
  default:
    break;
  }

  // ---------- Rotacion del quad en su propio plano ----------
  // Un pequeño roll para dar inclinacion "agarrada". Alrededor de fwd.
  // Tambien un pequeño pitch alrededor de right, ligero.
  float rollRad = 18.0f * DEG2RAD;
  float pitchRad = -8.0f * DEG2RAD;

  float cr = cosf(rollRad), sr = sinf(rollRad);
  float cp = cosf(pitchRad), sp = sinf(pitchRad);

  // Ejes rotados: aplicamos roll a (right, up), luego pitch a (fwd, up)
  // 1) roll alrededor de fwd: right' = right*cr + up*sr, up' = -right*sr +
  // up*cr
  Vector3 r1 = {right.x * cr + up.x * sr, right.y * cr + up.y * sr,
                right.z * cr + up.z * sr};
  Vector3 u1 = {-right.x * sr + up.x * cr, -right.y * sr + up.y * cr,
                -right.z * sr + up.z * cr};

  // 2) pitch alrededor de r1: fwd' = fwd*cp + u1*sp, u2 = -fwd*sp + u1*cp
  Vector3 f2 = {fwd.x * cp + u1.x * sp, fwd.y * cp + u1.y * sp,
                fwd.z * cp + u1.z * sp};
  Vector3 u2 = {-fwd.x * sp + u1.x * cp, -fwd.y * sp + u1.y * cp,
                -fwd.z * sp + u1.z * cp};

  // Usamos r1 como "right", u2 como "up", f2 como "forward" del quad
  float h = size * 0.5f;

  // Los 4 vertices del quad centrado en pos
  Vector3 v0 = {pos.x + r1.x * (-h) + u2.x * (-h),
                pos.y + r1.y * (-h) + u2.y * (-h),
                pos.z + r1.z * (-h) + u2.z * (-h)};
  Vector3 v1 = {pos.x + r1.x * (h) + u2.x * (-h),
                pos.y + r1.y * (h) + u2.y * (-h),
                pos.z + r1.z * (h) + u2.z * (-h)};
  Vector3 v2 = {pos.x + r1.x * (h) + u2.x * (h),
                pos.y + r1.y * (h) + u2.y * (h),
                pos.z + r1.z * (h) + u2.z * (h)};
  Vector3 v3 = {pos.x + r1.x * (-h) + u2.x * (h),
                pos.y + r1.y * (-h) + u2.y * (h),
                pos.z + r1.z * (-h) + u2.z * (h)};

  // ---------- Dibujar por encima de todo ----------
  rlDisableDepthTest();
  rlDisableBackfaceCulling();

  if (assets.hasItemTex[it.kind]) {
    Texture2D tex = assets.itemTex[it.kind];
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    // Normal apuntando hacia la camara
    rlNormal3f(-f2.x, -f2.y, -f2.z);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
  } else {
    Color c = Inventory::itemColor(it.kind);
    // Fallback: dos quads cruzados para que tenga volumen
    rlBegin(RL_QUADS);
    rlColor4ub(c.r, c.g, c.b, 255);
    rlVertex3f(v0.x, v0.y, v0.z);
    rlVertex3f(v1.x, v1.y, v1.z);
    rlVertex3f(v2.x, v2.y, v2.z);
    rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
  }

  rlEnableBackfaceCulling();
  rlEnableDepthTest();
}
