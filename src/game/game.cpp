#include "game.hpp"

#include "common.hpp"
#include "debug.hpp"
#include "enemy.hpp"
#include "gameplay_dialog.hpp"
#include "lang.hpp"
#include "paths.hpp"
#include "player.hpp"
#include "rlgl.h"
#include "ui.hpp"
#include "util_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

// =========================================================================
//  Util
// =========================================================================
Color Game::playerColor(int idx) const { return UI::PlayerColor(idx); }

// =========================================================================
//  init / shutdown / render targets
// =========================================================================
void Game::init() {
  cfg.load();
  Lang::load(cfg.language.c_str());

  SetConfigFlags(cfg.vsync ? FLAG_VSYNC_HINT : 0);

  int winW = std::max(640, cfg.windowW);
  int winH = std::max(480, cfg.windowH);

  InitWindow(winW, winH, C::GAME_TITLE);

  int monW = GetMonitorWidth(0);
  int monH = GetMonitorHeight(0);

  if (cfg.fullscreen) {
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetWindowPosition(0, 0);
  } else {
    SetWindowPosition((monW - winW) / 2, (monH - winH) / 2);
  }

  SetTargetFPS(60);
  SetMouseCursor(MOUSE_CURSOR_ARROW);
  SetExitKey(KEY_NULL);

  Texture2D splash = LoadTexture(Paths::sprite("splash.png").c_str());
  if (splash.id != 0)
    SetTextureFilter(splash, TEXTURE_FILTER_BILINEAR);

  auto drawSplash = [&](float progress) {
    BeginDrawing();
    ClearBackground(WHITE);
    int W = GetScreenWidth();
    int H = GetScreenHeight();
    float barArea = 22.0f;
    float imgAreaH = (float)H - barArea;
    if (imgAreaH < 10.0f)
      imgAreaH = (float)H * 0.7f;

    float tw = 600;
    float th = 450;
    float tx = ((float)W - tw) / 2.0f;
    float ty = (imgAreaH - th) / 2.0f;

    if (splash.id != 0) {
      DrawTexturePro(splash, {0, 0, (float)splash.width, (float)splash.height},
                     {tx, ty, tw, th}, {0, 0}, 0.0f, WHITE);
    } else {
      const char *t = "Escape from Marisa 3";
      int fs = 42;
      UI::T(t, W / 2 - UI::M(t, fs) / 2, (int)(imgAreaH / 2), fs, RED);
    }

    float barW = (float)W * 0.65f;
    if (barW > 900.0f)
      barW = 900.0f;
    float barH = 20.0f;
    float barX = (float)W / 2.0f - barW / 2.0f;
    float barY = ty + th + barH;

    DrawRectangle((int)barX - 2, (int)barY - 2, (int)barW + 4, (int)barH + 4,
                  (Color){0, 0, 0, 200});
    DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH,
                  (Color){30, 32, 45, 255});

    float p = std::clamp(progress, 0.0f, 1.0f);
    DrawRectangle((int)barX, (int)barY, (int)(barW * p), (int)barH, BLUE);
    DrawRectangleLines((int)barX, (int)barY, (int)barW, (int)barH, RAYWHITE);

    EndDrawing();
  };

  drawSplash(0.0f);

  InitAudioDevice();
  assets.applyVolumes(cfg.masterVol, cfg.musicVol, cfg.sfxVol);

  auto doStep = [&](int i) {
    switch (i) {
    case 0:
      assets.loadEnemyTextures();
      break;
    case 1:
      assets.loadWallTextures();
      break;
    case 2:
      assets.loadItemTextures();
      break;
    case 3:
      assets.loadMusic();
      break;
    case 4:
      assets.loadSounds();
      break;
    case 5:
      saves.refresh();
      credits.load("assets/credits.txt");
      attract.init();
      break;
    case 6:
      assets.loadFonts();
      UI::font = assets.font;
      break;
    }
  };

  const int N = 7;
  for (int i = 0; i < N; ++i) {
    drawSplash((float)i / (float)N);
    doStep(i);
    drawSplash((float)(i + 1) / (float)N);
  }

  float waitStart = (float)GetTime();
  while ((float)GetTime() - waitStart < 0.20f) {
    drawSplash(1.0f);
  }

  if (splash.id != 0)
    UnloadTexture(splash);

  // Inicializar raycaster con la resolución configurada
  int rcIdx = std::clamp(cfg.raycastRes, 0, C::RC_RES_COUNT - 1);
  rc.init(C::RC_RESOLUTIONS[rcIdx][0], C::RC_RESOLUTIONS[rcIdx][1]);
  rc.loadStaticTextures();

  // Aplicar FPS, VSync, resolución de ventana, RTs, etc.
  applyVideoSettings();

  WaitTime(2);

  state = ST_MENU;
}

void Game::shutdown() {
  cfg.save();
  r3d.shutdown();
  if (rt3d.texture.id != 0)
    UnloadRenderTexture(rt3d);
  rc.unload();
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
  int w = GetScreenWidth();
  int h = GetScreenHeight();
  if (twoPlayers) {
    w /= 2;
  }
  if (rtLeft.texture.id != 0 &&
      (rtLeft.texture.width != w || rtLeft.texture.height != h)) {
    UnloadRenderTexture(rtLeft);
    rtLeft = {};
  }
  if (rtRight.texture.id != 0 &&
      (rtRight.texture.width != w || rtRight.texture.height != h)) {
    UnloadRenderTexture(rtRight);
    rtRight = {};
  }
  if (rtLeft.texture.id == 0) {
    rtLeft = LoadRenderTexture(w, h);
    SetTextureFilter(rtLeft.texture, TEXTURE_FILTER_BILINEAR);
  }
  if (rtRight.texture.id == 0) {
    rtRight = LoadRenderTexture(w, h);
    SetTextureFilter(rtRight.texture, TEXTURE_FILTER_BILINEAR);
  }

  // NUEVO: Render target para el renderer 3D
  if (rt3d.texture.id != 0 && (rt3d.texture.width != GetScreenWidth() ||
                               rt3d.texture.height != GetScreenHeight())) {
    UnloadRenderTexture(rt3d);
    rt3d = {};
  }
  if (rt3d.texture.id == 0) {
    rt3d = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureFilter(rt3d.texture, TEXTURE_FILTER_BILINEAR);
  }
}

// =========================================================================
//  Estado
// =========================================================================
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

  for (int p = 0; p < 2; ++p) {
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      players[p].inventory[i].kind = ITEM_NONE;
      players[p].inventory[i].count = 0;
    }
    players[p].selectedSlot = 0;
  }

  requestEnterLevel(1);
}

void Game::assignGamepads() {
  int p1Pad = cfg.swapGamepads ? 1 : 0;
  int p2Pad = cfg.swapGamepads ? 0 : 1;
  players[0].gamepadId = IsGamepadAvailable(p1Pad) ? p1Pad : -1;
  players[1].gamepadId = (twoPlayers && IsGamepadAvailable(p2Pad)) ? p2Pad : -1;
}

// =========================================================================
//  Helpers de spawn
// =========================================================================
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
      p.inventory[i].count = p.inventory[i].count / 2;
  }

  // Posición / orientación
  p.pos = spawn;
  p.pos.y = 0.0f; // asegurar que aterriza en el suelo
  p.yaw = 0.0f;
  p.pitch = 0.0f;

  // Estado general
  p.caught = false;
  p.escaped = false;
  p.footstepT = 0.0f;
  p.stamina = 1.0f;
  p.staminaDelay = 0.0f;
  p.exhausted = false;
  p.sprinting = false;
  p.escapeTime = 0.0f;
  p.caughtBy = -1;
  p.hasKey = false;

  // Esconderse
  p.hidden = false;
  p.hiddenExitPos = spawn;
  p.hiddenExitYaw = 0.0f;
  p.hiddenCellX = -1;
  p.hiddenCellY = -1;

  // Inventario / armas
  p.throwCooldown = 0.0f;
  p.attackFlash = 0.0f;

  // Movimiento estilo Quake — CRÍTICO: resetear para no heredar
  // la velocidad del nivel anterior.
  p.velocity = {0.0f, 0.0f, 0.0f};
  p.onGround = true;
  p.jumpHeld = false;
  p.groundZ = 0.0f;
}

// =========================================================================
//  Cinemáticas
// =========================================================================
bool Game::hasStory(const char *name) const {
  return !Util::resolveStoryPath(name).empty();
}

void Game::playCinematic(const char *name, const char *nextAction) {
  std::string full = Util::resolveStoryPath(name);
  if (full.empty() || !cinematic.load(full.c_str())) {
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

// =========================================================================
//  Input helpers
// =========================================================================
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

bool Game::consumeCancel() {
  if (IsKeyPressed(KEY_ESCAPE))
    return true;
  for (int p = 0; p < 2; ++p)
    if (players[p].gamepadId >= 0 &&
        IsGamepadButtonPressed(players[p].gamepadId,
                               GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
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

// =========================================================================
//  update
// =========================================================================
void Game::update(float dt) {
  UI::scale = cfg.guiScale;
  UI::colorblind = cfg.colorblindMode;

  if (state == ST_MENU)
    attract.update(dt);

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

    if (sr.active && numPlayers > 0) {
      srGhostSampleTimer += gdt;
      while (srGhostSampleTimer >= C::GHOST_SAMPLE_DT) {
        srGhostSampleTimer -= C::GHOST_SAMPLE_DT;
        if ((int)srGhostRec.levels.size() < level)
          srGhostRec.levels.resize(level);
        GhostLevel &gl = srGhostRec.levels[level - 1];

        GhostSample s;
        s.x = (int16_t)(players[0].pos.x * 100.0f);
        s.z = (int16_t)(players[0].pos.z * 100.0f);
        // yaw ∈ [-π, π] → [0, 65535]
        float yawNorm = (players[0].yaw + PI) / (2.0f * PI);
        while (yawNorm < 0.0f)
          yawNorm += 1.0f;
        while (yawNorm >= 1.0f)
          yawNorm -= 1.0f;
        s.yaw = (uint16_t)(yawNorm * 65535.0f);
        gl.samples.push_back(s);
      }
    }

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

    bool levelHasKey = false;
    for (auto &w : worldItems)
      if (w.kind == ITEM_KEY) {
        levelHasKey = true;
        break;
      }
    bool keySatisfied = !levelHasKey || players[0].hasKey || players[1].hasKey;

    for (int p = 0; p < numPlayers; ++p) {
      if (players[p].caught || players[p].escaped)
        continue;
      if (Vector3Distance(players[p].pos, exitPos) < 1.7f) {
        if (!keySatisfied) {
          if (p == 0 && toastTimer <= 0.0f)
            showToast(L("toast.exit.locked", "Necesitas la llave"));
          continue;
        }
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

        if (sr.active)
          speedrunFinishRun();

        int caughtPlayer = players[0].caught ? 0 : 1;
        if (!cfg.noJumpscares && players[caughtPlayer].caughtBy >= 0) {
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

        if (sr.active)
          speedrunRecordSplit();
        autoSaveProgress();
        state = ST_LEVEL_CLEAR;
      }
    } else if (pausePressed) {
      EnableCursor();
      navMenu = 0;
      state = ST_PAUSED;
    }
  } else if (state == ST_PAUSED) {
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
  } else if (state == ST_DIALOG) {
    bool close = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                 IsKeyPressed(KEY_ESCAPE) ||
                 IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                 IsKeyPressed(cfg.binds[0].interact);
    for (int p = 0; p < 2 && !close; ++p) {
      if (players[p].gamepadId >= 0 &&
          IsGamepadButtonPressed(players[p].gamepadId,
                                 GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        close = true;
    }
    if (close)
      closeDialog();
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
  } else if (state == ST_LEVEL_INTRO) {
    introTimer += dt;
    if (introTimer >= 0.8f) {
      bool advance = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                     IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      for (int p = 0; p < 2 && !advance; ++p) {
        if (players[p].gamepadId >= 0 &&
            IsGamepadButtonPressed(players[p].gamepadId,
                                   GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
          advance = true;
      }
      if (advance)
        enterLevel(level);
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
      if (mode == MODE_STORY) {
        char introName[64], outroName[64], mapName[64];
        snprintf(introName, sizeof(introName), "level_%02d_intro.txt", level);
        snprintf(outroName, sizeof(outroName), "level_%02d_outro.txt", level);
        snprintf(mapName, sizeof(mapName), "level_%02d_map.dgn", level);

        auto existsAt = [](const std::string &p) {
          FILE *fp = fopen(p.c_str(), "rb");
          if (!fp)
            return false;
          fclose(fp);
          return true;
        };

        bool hasIntro = hasStory(introName);
        bool hasOutro = hasStory(outroName);
        bool hasDetails = !Util::resolveLevelDetailsPath(level).empty();
        bool hasMap = existsAt(Paths::level(mapName));

        bool nextLevelExists = hasIntro || hasOutro || hasDetails || hasMap;
        if (!nextLevelExists) {
          if (sr.active) {
            speedrunRecordSplit();
            // Cerrar duración del nivel recién completado
            int justFinished = level - 1;
            if (justFinished >= 0 &&
                justFinished < (int)srGhostRec.levels.size()) {
              srGhostRec.levels[justFinished].duration =
                  srGhostRec.levels[justFinished].samples.size() *
                  C::GHOST_SAMPLE_DT;
            }
          }
          credits.start();
          state = ST_CREDITS;
          return;
        }
      }
      requestEnterLevel(level);
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

  if (IsKeyPressed(KEY_F2)) {
    time_t now = time(nullptr);
    tm *t = localtime(&now);
    char nombre[64];
    strftime(nombre, sizeof(nombre), "Escape-Screenshot-%Y-%m-%d_%H-%M-%S.png",
             t);
    TakeScreenshot(nombre);
  }
}

void Game::drawGameplayView(const Player &pl, int vw, int vh, int playerNum) {
  // 1) Blit del framebuffer del raycaster (ya renderizado previamente)
  if (!cfg.useRenderer3D) {
    Rectangle src = {0, 0, (float)rc.texture.width, (float)rc.texture.height};
    Rectangle dst = {0, 0, (float)vw, (float)vh};
    DrawTexturePro(rc.texture, src, dst, {0, 0}, 0.0f, WHITE);
  }

  // 2) Vignette / gradient superior
  DrawRectangle(0, 0, vw, vh, (Color){0, 0, 0, 70});
  DrawRectangleGradientV(0, 0, vw, vh / 6, (Color){0, 0, 0, 140},
                         (Color){0, 0, 0, 0});

  // 3) Minimapa
  drawMinimap(pl, vw);

  // 4) HUD superior izquierdo
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

  // 5) Stamina, inventario, ítem en mano (todo 2D)
  drawStaminaBar(pl, vw, vh);
  drawInventory(pl, vw, vh);
  drawHandItem2D(pl, vw, vh);

  // 6) Crosshair
  if (assets.crosshair.id != 0) {
    float cw = (float)assets.crosshair.width;
    float ch = (float)assets.crosshair.height;
    float cx = vw * 0.5f - cw * 0.5f;
    float cy = vh * 0.5f - ch * 0.5f;
    if (!pl.caught && !pl.escaped && !pl.hidden) {
      DrawTexturePro(
          assets.crosshair,
          {0, 0, (float)assets.crosshair.width, (float)assets.crosshair.height},
          {cx, cy, cw, ch}, {0, 0}, 0.0f, (Color){255, 255, 255, 200});
    }
  }

  // 7) Overlays de estado del jugador
  if (pl.caught) {
    DrawRectangle(0, 0, vw, vh, (Color){120, 0, 0, 90});
    const char *msg = L("hud.caught.waiting", "Atrapado - esperando...");
    UI::T(msg, vw / 2 - UI::M(msg, small) / 2, vh / 2, small, RAYWHITE);
  } else if (pl.escaped) {
    DrawRectangle(0, 0, vw, vh, (Color){0, 80, 40, 90});
    const char *msg = L("hud.escaped.waiting", "Escapaste - esperando...");
    UI::T(msg, vw / 2 - UI::M(msg, small) / 2, vh / 2, small, RAYWHITE);
  }

  // 8) Speedrun overlay
  if (sr.active)
    drawSpeedrunOverlay(vw, vh);
}

void Game::drawSplitScreen() {
  // Jugador 0
  BeginTextureMode(rtLeft);
  ClearBackground((Color){12, 12, 18, 255});
  renderPlayerView(players[0]);
  drawGameplayView(players[0], rtLeft.texture.width, rtLeft.texture.height, 0);
  EndTextureMode();

  // Jugador 1
  BeginTextureMode(rtRight);
  ClearBackground((Color){12, 12, 18, 255});
  renderPlayerView(players[1]);
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

// =========================================================================
//  Formato fecha
// =========================================================================
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

// =========================================================================
//  Toast
// =========================================================================
void Game::showToast(const char *msg) {
  snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
  toastTimer = 1.8f;
}

// =========================================================================
//  Saves: guardado y carga
// =========================================================================
void Game::saveCurrentGame() {
  if (currentSaveId.empty()) {
    showToast(L("toast.noslot", "Sin slot asignado"));
    return;
  }
  SaveEntry e = *saves.find(currentSaveId);
  e.level = level;
  e.maxLevel = std::max(e.maxLevel, level);
  e.mode = (int)mode;
  e.difficulty = currentDifficulty;
  e.twoPlayers = twoPlayers;
  e.playtime = totalPlaytime;
  e.timestamp = std::time(nullptr);
  e.speedrunSplits = sr.splits;

  int numPlayers = twoPlayers ? 2 : 1;
  for (int p = 0; p < numPlayers && p < 2; ++p) {
    e.selectedSlot[p] = players[p].selectedSlot;
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      e.inventory[p][i].kind = (int)players[p].inventory[i].kind;
      e.inventory[p][i].count = players[p].inventory[i].count;
    }
  }
  saves.save(e);
  showToast(L("toast.saved", "Partida guardada"));
}

void Game::autoSaveProgress() {
  if (currentSaveId.empty())
    return;
  SaveEntry e = *saves.find(currentSaveId);
  e.level = level;
  e.maxLevel = std::max(e.maxLevel, level);
  e.mode = (int)mode;
  e.difficulty = currentDifficulty;
  e.twoPlayers = twoPlayers;
  e.playtime = totalPlaytime;
  e.timestamp = std::time(nullptr);
  e.speedrunSplits = sr.splits;

  int numPlayers = twoPlayers ? 2 : 1;
  for (int p = 0; p < numPlayers && p < 2; ++p) {
    e.selectedSlot[p] = players[p].selectedSlot;
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      e.inventory[p][i].kind = (int)players[p].inventory[i].kind;
      e.inventory[p][i].count = players[p].inventory[i].count;
    }
  }
  saves.save(e);
}

void Game::startNewGame(const SaveEntry &proto) {
  std::string id = saves.create(proto);
  if (id.empty()) {
    showToast("Error creando la partida");
    return;
  }
  currentSaveId = id;

  const SaveEntry *e = saves.find(id);
  if (!e)
    return;

  currentDifficulty = e->difficulty;
  runSeed = e->runSeed;
  mode = (GameMode)e->mode;
  twoPlayers = e->twoPlayers;
  level = 1;
  totalPlaytime = 0.0f;

  if (e->speedrun)
    speedrunStartRun(runSeed, (int)mode);
  else
    sr.active = false;

  beginPlay(twoPlayers, mode);
}

void Game::loadGame(const std::string &saveId) {
  const SaveEntry *e = saves.find(saveId);
  if (!e)
    return;

  currentSaveId = saveId;
  currentDifficulty = e->difficulty;
  runSeed = e->runSeed;
  mode = (GameMode)e->mode;
  twoPlayers = e->twoPlayers;
  level = e->level;
  totalPlaytime = e->playtime;

  startLevel(level);
  assignGamepads();

  int numPlayers = twoPlayers ? 2 : 1;
  for (int p = 0; p < numPlayers && p < 2; ++p) {
    int sel = e->selectedSlot[p];
    if (sel < 0 || sel >= C::INV_SLOTS)
      sel = 0;
    players[p].selectedSlot = sel;
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      ItemKind k = (ItemKind)e->inventory[p][i].kind;
      int c = e->inventory[p][i].count;
      if (k <= ITEM_NONE || k >= ITEM_KIND_COUNT || c <= 0) {
        players[p].inventory[i].kind = ITEM_NONE;
        players[p].inventory[i].count = 0;
      } else {
        players[p].inventory[i].kind = k;
        players[p].inventory[i].count = c;
      }
    }
  }

  if (e->speedrun) {
    speedrunStartRun(runSeed, (int)mode);
    sr.splits = e->speedrunSplits;
    sr.lastLevelStart = totalPlaytime;
  } else {
    sr.active = false;
  }

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

// =========================================================================
//  Lista de saves + creación
//  (drawSaveList y drawSaveCreate van en la parte 2/2)
// =========================================================================

// =========================================================================
//  Pausa
// =========================================================================
void Game::drawPauseMenu() {
  int W = GetScreenWidth(), H = GetScreenHeight();
  DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 200});

  const char *t = L("pause.title", "Pausa");
  UI::T(t, W / 2 - UI::M(t, 64) / 2, (int)(H * 0.12f), 64, RAYWHITE);

  bool canSave = !currentSaveId.empty();

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
    if (sr.active)
      speedrunFinishRun();
    toMenu();
  }
  if (uiButton(4, items, 40, L("menu.quit", "Salir del juego"), &navMenu,
               nullptr) ||
      (navMenu == 4 && consumeConfirm())) {
    autoSaveProgress();
    shouldQuit = true;
  }

  char info[128];
  snprintf(info, sizeof(info), L("pause.info", "Nivel %d  -  %.1fs"), level,
           levelTime);
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

// =========================================================================
//  Menú principal + speedrun menu
// =========================================================================
void Game::drawMenus() {
  BeginDrawing();
  int W = GetScreenWidth();
  int H = GetScreenHeight();

  if (state == ST_MENU) {
    ClearBackground(BLACK);
    attract.prepareRaycaster(rc);
    attract.draw(rc, W, H);
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
    int y = (int)(H * 0.10f);
    const char *t1 = "Escape from Marisa 3:";
    UI::T(t1, W / 2 - UI::M(t1, 54) / 2, y, 54, RAYWHITE);
    y += 54 + 6;
    const char *t2 = "Escape from Bea";
    UI::T(t2, W / 2 - UI::M(t2, 84) / 2, y, 84, RED);
    y += 84 + 14;
    const char *t3 = L("menu.subtitle", "modo serio chat");
    UI::T(t3, W / 2 - UI::M(t3, 22) / 2, y, 22, LIGHTGRAY);

    int titleTop = (int)(H * 0.10f);
    int titleH = 54 + 6 + 84;
    Rectangle titleRect = {(float)(W / 2 - 400), (float)titleTop, 800.0f,
                           (float)titleH};
    bool hoveringTitle = CheckCollisionPointRec(GetMousePosition(), titleRect);
    if (hoveringTitle) {
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

    if (uiButton(0, items, 90, L("menu.single", "Un jugador"), &navIndex,
                 nullptr) ||
        (navIndex == 0 && consumeConfirm())) {
      saves.refresh();
      saveListCursor = 0;
      saveListScroll = 0;
      saveDeleteConfirm = false;
      saveListLastClickIdx = -1;
      state = ST_SAVE_LIST;
      navIndex = 0;
    }
    if (uiButton(1, items, 90, L("menu.speedrun", "Speedruns"), &navIndex,
                 nullptr) ||
        (navIndex == 1 && consumeConfirm())) {
      state = ST_SPEEDRUN;
      navMenu = 0;
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
  case ST_SAVE_LIST:
    drawSaveList();
    break;
  case ST_SAVE_CREATE:
    drawSaveCreate();
    break;
  case ST_LEVEL_INTRO:
    drawLevelIntro();
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
  case ST_CONFIG_SPEEDRUN:
    drawConfigSpeedrun();
    break;
  case ST_SPEEDRUN:
    drawSpeedrunMenu();
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

  drawToast();
  if (cfg.showFps)
    DrawFPS(GetScreenWidth() - 90, 20);
  EndDrawing();
}

// =========================================================================
//  Save list — pantalla principal de partidas
// =========================================================================
void Game::drawSaveList() {
  int W = GetScreenWidth(), H = GetScreenHeight();

  UI::T(L("saves.title", "Partidas guardadas"),
        W / 2 - UI::M(L("saves.title", "Partidas guardadas"), 42) / 2, 50, 42,
        RAYWHITE);

  const auto &list = saves.entries();

  const int rowH = 58;
  const int topY = 130;
  const int botY = H - 120;
  const int listX = 80;
  const int listW = W - 160;
  int visRows = std::max(1, (botY - topY) / rowH);

  if (list.empty()) {
    const char *msg =
        L("saves.empty", "No hay partidas. Crea una con el botón de abajo.");
    UI::T(msg, W / 2 - UI::M(msg, 22) / 2, H / 2 - 20, 22, LIGHTGRAY);
  } else {
    if (saveListCursor < 0)
      saveListCursor = 0;
    if (saveListCursor >= (int)list.size())
      saveListCursor = (int)list.size() - 1;

    if (saveListCursor < saveListScroll)
      saveListScroll = saveListCursor;
    if (saveListCursor >= saveListScroll + visRows)
      saveListScroll = saveListCursor - visRows + 1;
    if (saveListScroll < 0)
      saveListScroll = 0;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && !saveDeleteConfirm) {
      saveListScroll -= (int)wheel;
      if (saveListScroll < 0)
        saveListScroll = 0;
      int maxScroll = std::max(0, (int)list.size() - visRows);
      if (saveListScroll > maxScroll)
        saveListScroll = maxScroll;
    }

    if (IsKeyPressed(KEY_DOWN))
      saveListCursor++;
    if (IsKeyPressed(KEY_UP))
      saveListCursor--;
    if (saveListCursor < 0)
      saveListCursor = 0;
    if (saveListCursor >= (int)list.size())
      saveListCursor = (int)list.size() - 1;

    for (int r = 0; r < visRows; ++r) {
      int idx = saveListScroll + r;
      if (idx >= (int)list.size())
        break;

      const SaveEntry &e = list[idx];
      int y = topY + r * rowH;

      Rectangle row = {(float)listX, (float)y, (float)listW, (float)(rowH - 6)};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, row);

      bool selected = (idx == saveListCursor);
      bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

      Color bg =
          selected ? (Color){60, 70, 105, 230} : (Color){30, 34, 48, 200};
      Color brd = selected ? SKYBLUE : (Color){70, 80, 110, 200};
      DrawRectangleRec(row, bg);
      DrawRectangleLinesEx(row, 2, brd);

      UI::T(e.name.c_str(), listX + 20, y + 12, 22, RAYWHITE);

      char info[256];
      const char *modeStr = (e.mode == MODE_STORY)
                                ? L("mode.story", "Historia")
                                : L("mode.endless", "Infinito");
      snprintf(info, sizeof(info), "%s · Nivel %d · %s", modeStr, e.level,
               e.speedrun ? L("saves.tag.sr", "Speedrun") : "");
      UI::T(info, listX + 20, y + 34, 14, LIGHTGRAY);

      int totalSec = (int)e.playtime;
      int hh = totalSec / 3600;
      int mm = (totalSec / 60) % 60;
      char pt[32];
      if (hh > 0)
        snprintf(pt, sizeof(pt), "%dh %02dm", hh, mm);
      else
        snprintf(pt, sizeof(pt), "%dm", mm);

      int ptW = UI::M(pt, 16);
      UI::T(pt, listX + listW - ptW - 20, y + 12, 16, RAYWHITE);

      const char *dateStr = formatDate(e.timestamp);
      int dtW = UI::M(dateStr, 14);
      UI::T(dateStr, listX + listW - dtW - 20, y + 32, 14, GRAY);

      if (clicked) {
        double now = GetTime();
        bool isDoubleClick = (idx == saveListLastClickIdx &&
                              (now - saveListLastClickTime) < 0.4);

        if (isDoubleClick) {
          // Doble-click: entrar en la partida
          loadGame(e.id);
          return;
        }

        // Single-click: seleccionar
        saveListCursor = idx;
        saveListLastClickIdx = idx;
        saveListLastClickTime = now;
      }
    }
  }

  // ----- Botones inferiores -----
  auto bottomBtn = [&](int idx, int total, const char *label) -> bool {
    int bw = 180, bh = 46, gap = 16;
    int totalW = total * bw + (total - 1) * gap;
    int bx = W / 2 - totalW / 2 + idx * (bw + gap);
    int by = H - 90;
    Rectangle r = {(float)bx, (float)by, (float)bw, (float)bh};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = hover ? (Color){70, 80, 120, 255} : (Color){40, 45, 65, 255};
    Color brd = hover ? SKYBLUE : (Color){90, 100, 140, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, brd);

    int fs = 20;
    int tw = UI::M(label, fs);
    UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs,
          hover ? WHITE : RAYWHITE);
    return click;
  };

  bool hasSelection = !list.empty();
  bool createClicked = bottomBtn(0, 5, L("saves.new", "+ Nueva partida"));
  bool playClicked = hasSelection && bottomBtn(1, 5, L("saves.play", "Jugar"));
  bool dupClicked = hasSelection && bottomBtn(2, 5, L("saves.dup", "Duplicar"));
  bool recClicked = hasSelection && bottomBtn(3, 5, L("saves.rec", "Recrear"));
  bool delClicked = hasSelection && bottomBtn(4, 5, L("saves.del", "Eliminar"));

  // Modal de confirmación
  if (saveDeleteConfirm) {
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 180});

    const char *q = L("saves.delete.confirm", "¿Eliminar esta partida?");
    int qw = UI::M(q, 30);
    UI::T(q, W / 2 - qw / 2, H / 2 - 40, 30, RAYWHITE);

    auto confirmBtn = [&](int idx, const char *label) -> bool {
      int bw = 160, bh = 50, gap = 30;
      int bx = W / 2 - bw - gap / 2 + idx * (bw + gap);
      int by = H / 2 + 20;
      Rectangle r = {(float)bx, (float)by, (float)bw, (float)bh};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      Color bg = hover ? (idx == 0 ? (Color){140, 60, 60, 255}
                                   : (Color){60, 90, 140, 255})
                       : (Color){40, 45, 65, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2, RAYWHITE);
      int fs = 22;
      int tw = UI::M(label, fs);
      UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs, WHITE);
      return click;
    };

    bool yes = confirmBtn(0, L("common.yes", "Sí"));
    bool no = confirmBtn(1, L("common.no", "No"));
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N))
      no = true;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Y))
      yes = true;

    if (yes) {
      if (!saveDeletePendingId.empty()) {
        saves.remove(saveDeletePendingId);
        saveDeletePendingId.clear();
        if (saveListCursor >= (int)saves.entries().size())
          saveListCursor = std::max(0, (int)saves.entries().size() - 1);
      }
      saveDeleteConfirm = false;
    }
    if (no) {
      saveDeleteConfirm = false;
      saveDeletePendingId.clear();
    }
    return;
  }

  if (createClicked) {
    createBuffer = SaveEntry{};
    createBuffer.name = "New world";
    createBuffer.mode = MODE_STORY;
    createBuffer.difficulty = DIFF_NORMAL;
    createBuffer.speedrun = false;
    createBuffer.twoPlayers = false;
    // Buffer de seed EMPTY: el usuario puede teclear una nueva, o dejar
    // vacío para que se genere aleatoria al confirmar.
    seedEditBuf[0] = '\0';
    state = ST_SAVE_CREATE;
    navIndex = 0;
  }
  if (playClicked && hasSelection)
    loadGame(list[saveListCursor].id);
  if (dupClicked && hasSelection)
    saves.duplicate(list[saveListCursor].id);
  if (recClicked && hasSelection)
    saves.recreate(list[saveListCursor].id);
  if (delClicked && hasSelection) {
    saveDeletePendingId = list[saveListCursor].id;
    saveDeleteConfirm = true;
  }

  if (!saveDeleteConfirm && (IsKeyPressed(KEY_ESCAPE) || consumeCancel())) {
    state = ST_MENU;
    navIndex = 0;
  }
}

// =========================================================================
//  Save create — formulario de nueva partida
// =========================================================================
void Game::drawSaveCreate() {
  drawConfigHeader(L("create.title", "Nueva partida"));

  bool endless = (createBuffer.mode == MODE_ENDLESS);

  int seedRow = endless ? 5 : -1;
  int randomRow = endless ? 6 : -1;
  int enemyRow = endless ? 7 : -1;
  int itemRow = endless ? 8 : -1;
  int createRow = endless ? 9 : 5;
  int cancelRow = createRow + 1;
  int totalRows = cancelRow + 1;

  if (IsKeyPressed(KEY_DOWN))
    navIndex = (navIndex + 1) % totalRows;
  if (IsKeyPressed(KEY_UP))
    navIndex = (navIndex + totalRows - 1) % totalRows;
  handleGamepadMenuNav(totalRows);

  int lr = gamepadLeftRight();
  if (IsKeyPressed(KEY_LEFT))
    lr = -1;
  if (IsKeyPressed(KEY_RIGHT))
    lr = +1;

  bool confirm = consumeConfirm();

  auto rowY = [&](int row) {
    return GetScreenHeight() / 2 - (totalRows * 60) / 2 + row * 60;
  };

  auto drawRow = [&](int row, const char *label, const char *value,
                     bool *outSelected, bool *outHover) {
    int x = GetScreenWidth() / 2 - 340;
    int y = rowY(row);
    Rectangle r = {(float)x, (float)y, 680, 52};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    Vector2 md = GetMouseDelta();
    if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
      navIndex = row;
    bool selected = (navIndex == row);
    Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                   : (Color){35, 40, 58, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, selected ? SKYBLUE : (Color){80, 90, 125, 255});
    UI::T(label, x + 22, y + 14, 24, RAYWHITE);
    if (value && value[0]) {
      char full[160];
      snprintf(full, sizeof(full), "< %s >", value);
      int vw = UI::M(full, 24);
      UI::T(full, x + 680 - vw - 22, y + 14, 24, GOLD);
    }
    *outSelected = selected;
    *outHover = hover;
  };

  // ============================================================
  // Fila 0 — Nombre (input editable)
  // ============================================================
  {
    int x = GetScreenWidth() / 2 - 340;
    int y = rowY(0);
    Rectangle r = {(float)x, (float)y, 680, 52};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    Vector2 md = GetMouseDelta();
    if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
      navIndex = 0;
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      navIndex = 0;
    bool selected = (navIndex == 0);
    Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                   : (Color){35, 40, 58, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, selected ? SKYBLUE : (Color){80, 90, 125, 255});
    UI::T(L("create.name", "Nombre"), x + 22, y + 14, 24, RAYWHITE);

    int tw = UI::M(createBuffer.name.c_str(), 24);
    int tx = x + 680 - tw - 22;
    UI::T(createBuffer.name.c_str(), tx, y + 14, 24, GOLD);

    // Input SOLO si está seleccionado: así no roba caracteres a otros campos
    if (selected) {
      int k = GetCharPressed();
      while (k > 0) {
        if (k >= 32 && k < 127 && createBuffer.name.size() < 40)
          createBuffer.name += (char)k;
        k = GetCharPressed();
      }
      if (IsKeyPressed(KEY_BACKSPACE) && !createBuffer.name.empty())
        createBuffer.name.pop_back();
    }
    if (selected && fmodf((float)GetTime(), 1.0f) < 0.5f)
      UI::T("_", tx + tw, y + 14, 24, GOLD);
  }

  // ============================================================
  // Fila 1 — Modo
  // ============================================================
  {
    bool sel = false, hov = false;
    const char *modeStr = (createBuffer.mode == MODE_STORY)
                              ? L("mode.story", "Historia")
                              : L("mode.endless", "Infinito");
    drawRow(1, L("create.mode", "Modo"), modeStr, &sel, &hov);
    if (sel) {
      int x = GetScreenWidth() / 2 - 340;
      Vector2 mp = GetMousePosition();
      bool hitL =
          hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mp.x < x + 340;
      bool hitR =
          hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mp.x >= x + 340;
      if (confirm || lr != 0 || hitL || hitR) {
        createBuffer.mode = (createBuffer.mode + 1) % 2;
        if (confirm)
          confirm = false;
      }
    }
  }

  // ============================================================
  // Fila 2 — Dificultad
  // ============================================================
  {
    const char *diffStr;
    switch (createBuffer.difficulty) {
    case DIFF_EXPERT:
      diffStr = "EXPERTO";
      break;
    case DIFF_NIGHTMARE:
      diffStr = "PESADILLA";
      break;
    default:
      diffStr = "NORMAL";
      break;
    }
    bool sel = false, hov = false;
    drawRow(2, L("create.difficulty", "Dificultad"), diffStr, &sel, &hov);
    if (sel) {
      int x = GetScreenWidth() / 2 - 340;
      Vector2 mp = GetMousePosition();
      int delta = 0;
      if (lr < 0)
        delta = -1;
      if (lr > 0)
        delta = +1;
      if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        delta = (mp.x < x + 340) ? -1 : +1;
      createBuffer.difficulty =
          (createBuffer.difficulty + delta + DIFF_COUNT) % DIFF_COUNT;
    }
  }

  // ============================================================
  // Fila 3 — Jugadores
  // ============================================================
  {
    char buf[8];
    snprintf(buf, sizeof(buf), "%s", createBuffer.twoPlayers ? "2" : "1");
    bool sel = false, hov = false;
    drawRow(3, L("create.players", "Jugadores"), buf, &sel, &hov);
    if (sel) {
      bool hit = hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      if (confirm || hit || lr != 0) {
        createBuffer.twoPlayers = !createBuffer.twoPlayers;
        if (confirm)
          confirm = false;
      }
    }
  }

  // ============================================================
  // Fila 4 — Speedrun
  // ============================================================
  {
    const char *sr =
        createBuffer.speedrun ? L("common.yes", "Sí") : L("common.no", "No");
    bool sel = false, hov = false;
    drawRow(4, L("create.speedrun", "Modo speedrun"), sr, &sel, &hov);
    if (sel) {
      bool hit = hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      if (confirm || hit || lr != 0) {
        createBuffer.speedrun = !createBuffer.speedrun;
        if (confirm)
          confirm = false;
      }
    }
  }

  // ============================================================
  // Sección infinito — Seed, Aleatorizar, Multiplicadores
  // ============================================================
  if (endless) {
    // --- Seed (input editable con seedEditBuf) ---
    {
      int x = GetScreenWidth() / 2 - 340;
      int y = rowY(seedRow);
      Rectangle r = {(float)x, (float)y, 680, 52};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        navIndex = seedRow;
      bool selected = (navIndex == seedRow);
      Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                     : (Color){35, 40, 58, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2,
                           selected ? SKYBLUE : (Color){80, 90, 125, 255});
      UI::T(L("endless.seed", "Seed"), x + 22, y + 14, 24, RAYWHITE);

      // Texto a mostrar: la seed tecleada, o un hint si está vacía
      const char *display = seedEditBuf;
      Color dispCol = GOLD;
      if (seedEditBuf[0] == '\0') {
        display = L("endless.seed.random", "(vacío = aleatoria)");
        dispCol = GRAY;
      }
      int vw = UI::M(display, 24);
      int vx = x + 680 - vw - 22;
      UI::T(display, vx, y + 14, 24, dispCol);

      // Input SOLO si está seleccionado
      if (selected) {
        int k = GetCharPressed();
        while (k > 0) {
          if (k >= '0' && k <= '9' && strlen(seedEditBuf) < 10) {
            size_t len = strlen(seedEditBuf);
            seedEditBuf[len] = (char)k;
            seedEditBuf[len + 1] = '\0';
          }
          k = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
          size_t len = strlen(seedEditBuf);
          if (len > 0)
            seedEditBuf[len - 1] = '\0';
        }
      }
      if (selected && fmodf((float)GetTime(), 1.0f) < 0.5f)
        UI::T("_", vx + vw, y + 14, 24, GOLD);
    }

    // --- Aleatorizar seed ---
    {
      int x = GetScreenWidth() / 2 - 340;
      int y = rowY(randomRow);
      Rectangle r = {(float)x, (float)y, 680, 52};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      Vector2 md = GetMouseDelta();
      if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
        navIndex = randomRow;
      bool selected = (navIndex == randomRow);
      Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                     : (Color){35, 40, 58, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2,
                           selected ? SKYBLUE : (Color){80, 90, 125, 255});
      UI::T(L("endless.randomize", "Aleatorizar seed"), x + 22, y + 14, 24,
            RAYWHITE);

      bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      if ((selected && confirm) || clicked) {
        uint32_t s = (uint32_t)time(nullptr) ^ (uint32_t)(rand() * 2654435761u);
        if (s == 0)
          s = 1;
        snprintf(seedEditBuf, sizeof(seedEditBuf), "%u", s);
        if (selected)
          confirm = false;
      }
    }

    // --- Multiplicadores ---
    auto multRowFn = [&](int row, const char *label, int *value) {
      int x = GetScreenWidth() / 2 - 340;
      int y = rowY(row);
      Rectangle r = {(float)x, (float)y, 680, 52};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      Vector2 md = GetMouseDelta();
      if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
        navIndex = row;
      bool selected = (navIndex == row);
      Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                     : (Color){35, 40, 58, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2,
                           selected ? SKYBLUE : (Color){80, 90, 125, 255});
      UI::T(label, x + 22, y + 14, 24, RAYWHITE);

      char buf[16];
      snprintf(buf, sizeof(buf), "%gx", (double)C::ENDLESS_MULT[*value]);
      char full[32];
      snprintf(full, sizeof(full), "< %s >", buf);
      int vw = UI::M(full, 24);
      UI::T(full, x + 680 - vw - 22, y + 14, 24, GOLD);

      if (selected) {
        int delta = 0;
        if (lr < 0)
          delta = -1;
        if (lr > 0)
          delta = +1;
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
          delta = (mp.x < x + 340) ? -1 : +1;
        *value =
            (*value + delta + C::ENDLESS_MULT_COUNT) % C::ENDLESS_MULT_COUNT;
      }
    };
    multRowFn(enemyRow, L("endless.enemyMult", "Multiplicador enemigos"),
              &createBuffer.enemyMult);
    multRowFn(itemRow, L("endless.itemMult", "Multiplicador items"),
              &createBuffer.itemMult);
  }

  // ============================================================
  // Botones Crear / Volver
  // ============================================================
  auto actionBtn = [&](int row, const char *label) -> bool {
    int x = GetScreenWidth() / 2 - 340;
    int y = rowY(row);
    Rectangle r = {(float)x, (float)y, 680, 52};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    Vector2 md = GetMouseDelta();
    if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
      navIndex = row;
    bool selected = (navIndex == row);
    Color bg = (hover || selected) ? (Color){70, 100, 80, 255}
                                   : (Color){40, 45, 65, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, selected ? SKYBLUE : (Color){90, 100, 140, 255});
    int tw = UI::M(label, 24);
    UI::T(label, x + (680 - tw) / 2, y + 14, 24, RAYWHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  };

  bool createClicked =
      actionBtn(createRow, L("create.confirm", "Crear partida"));
  bool cancelClicked = actionBtn(cancelRow, L("common.back", "Volver"));

  if (createClicked || (navIndex == createRow && consumeConfirm())) {
    if (createBuffer.name.empty())
      createBuffer.name = "New world";

    if (createBuffer.mode == MODE_ENDLESS) {
      // Parsear la seed del buffer de texto. Si está vacía o es 0,
      // generar una aleatoria.
      uint32_t parsed = (uint32_t)strtoul(seedEditBuf, nullptr, 10);
      if (parsed == 0) {
        parsed = (uint32_t)time(nullptr) ^ (uint32_t)(rand() * 2654435761u);
        if (parsed == 0)
          parsed = 1;
      }
      createBuffer.runSeed = parsed;
    }

    startNewGame(createBuffer);
  } else if (cancelClicked || IsKeyPressed(KEY_ESCAPE) || consumeCancel()) {
    state = ST_SAVE_LIST;
    navIndex = 0;
  }
}

// =========================================================================
//  Speedrun menu
// =========================================================================
void Game::drawSpeedrunMenu() {
  int W = GetScreenWidth(), H = GetScreenHeight();
  drawConfigHeader(L("speedrun.title", "Speedruns"));

  const auto &allRuns = saves.speedruns();

  // =========================================================
  //  Estado vacío
  // =========================================================
  if (allRuns.empty()) {
    const char *line1 = L("speedrun.empty1", "Sin runs registrados todavía.");
    const char *line2 =
        L("speedrun.empty2", "Termina una partida con speedrun activado.");
    int fs = 22;
    UI::T(line1, W / 2 - UI::M(line1, fs) / 2, H / 2 - 20, fs, LIGHTGRAY);
    UI::T(line2, W / 2 - UI::M(line2, fs) / 2, H / 2 + 20, fs, GRAY);
    if (IsKeyPressed(KEY_ESCAPE) || consumeCancel()) {
      state = ST_MENU;
      navMenu = 0;
    }
    return;
  }

  // =========================================================
  //  Categorías únicas (seed + modo)
  // =========================================================
  std::vector<std::pair<uint32_t, int>> categories;
  for (auto &e : allRuns) {
    bool found = false;
    for (auto &c : categories)
      if (c.first == e.seed && c.second == e.mode) {
        found = true;
        break;
      }
    if (!found)
      categories.push_back({e.seed, e.mode});
  }

  if (navMenu < 0)
    navMenu = 0;
  if (navMenu >= (int)categories.size())
    navMenu = (int)categories.size() - 1;

  // Navegación de categorías (solo si NO estamos en modal de borrado)
  if (!srMenuDeleteConfirm) {
    int lr = gamepadLeftRight();
    if (IsKeyPressed(KEY_LEFT))
      lr = -1;
    if (IsKeyPressed(KEY_RIGHT))
      lr = +1;
    if (lr < 0) {
      navMenu = (navMenu - 1 + (int)categories.size()) % (int)categories.size();
      srMenuRunCursor = 0;
      srMenuRunScroll = 0;
    }
    if (lr > 0) {
      navMenu = (navMenu + 1) % (int)categories.size();
      srMenuRunCursor = 0;
      srMenuRunScroll = 0;
    }
  }

  uint32_t selSeed = categories[navMenu].first;
  int selMode = categories[navMenu].second;

  // =========================================================
  //  Lista filtrada de runs (por categoría, ordenadas por fecha desc)
  // =========================================================
  std::vector<int> filtered;
  for (int i = 0; i < (int)allRuns.size(); ++i) {
    if (allRuns[i].seed == selSeed && allRuns[i].mode == selMode)
      filtered.push_back(i);
  }
  std::sort(filtered.begin(), filtered.end(),
            [&](int a, int b) { return allRuns[a].date > allRuns[b].date; });

  // Encontrar la mejor run de esta categoría
  int bestIdx = -1;
  float bestTotal = 1e30f;
  for (int i : filtered) {
    if (allRuns[i].total < bestTotal) {
      bestTotal = allRuns[i].total;
      bestIdx = i;
    }
  }

  // Media
  float totalSum = 0.0f;
  for (int i : filtered)
    totalSum += allRuns[i].total;
  float avg = filtered.empty() ? 0.0f : totalSum / (float)filtered.size();

  // =========================================================
  //  Header: categoría
  // =========================================================
  char keyBuf[128];
  snprintf(keyBuf, sizeof(keyBuf), "Seed %u  |  %s", selSeed,
           selMode == MODE_STORY ? L("speedrun.mode.story", "Historia")
                                 : L("speedrun.mode.endless", "Infinito"));
  char keyFull[160];
  snprintf(keyFull, sizeof(keyFull), "< %s >", keyBuf);
  UI::T(keyFull, W / 2 - UI::M(keyFull, 22) / 2, 110, 22, GOLD);

  // =========================================================
  //  Layout de dos columnas
  // =========================================================
  const int colGap = 30;
  const int leftW = 480;
  const int rightW = 400;
  const int totalW = leftW + colGap + rightW;
  const int leftX = W / 2 - totalW / 2;
  const int rightX = leftX + leftW + colGap;
  int topY = 155;

  // =========================================================
  //  Columna izquierda: stats + lista de runs
  // =========================================================
  UI::T(TextFormat(L("speedrun.nruns", "%d runs"), (int)filtered.size()), leftX,
        topY, 16, LIGHTGRAY);
  topY += 22;

  if (bestIdx >= 0) {
    char tbuf[32];
    Util::formatSplitTime(bestTotal, tbuf, sizeof(tbuf));
    UI::T(TextFormat("%s: %s", L("speedrun.besttotal", "Mejor"), tbuf), leftX,
          topY, 16, GOLD);
    topY += 20;
  }

  {
    char tbuf[32];
    Util::formatSplitTime(avg, tbuf, sizeof(tbuf));
    UI::T(TextFormat("%s: %s", L("speedrun.avg", "Media"), tbuf), leftX, topY,
          16, LIGHTGRAY);
  }
  topY += 28;

  UI::T(L("speedrun.runs", "Runs"), leftX, topY, 15, LIGHTGRAY);
  topY += 22;

  const int rowH = 24;
  const int listH = H - topY - 100;
  int visRows = std::max(1, listH / rowH);

  // Clamp de cursor y scroll
  if (srMenuRunCursor < 0)
    srMenuRunCursor = 0;
  if (srMenuRunCursor >= (int)filtered.size())
    srMenuRunCursor = (int)filtered.size() - 1;
  if (srMenuRunCursor < srMenuRunScroll)
    srMenuRunScroll = srMenuRunCursor;
  if (srMenuRunCursor >= srMenuRunScroll + visRows)
    srMenuRunScroll = srMenuRunCursor - visRows + 1;
  if (srMenuRunScroll < 0)
    srMenuRunScroll = 0;

  // Navegación con teclado
  if (!srMenuDeleteConfirm) {
    if (IsKeyPressed(KEY_DOWN))
      srMenuRunCursor++;
    if (IsKeyPressed(KEY_UP))
      srMenuRunCursor--;
    if (srMenuRunCursor < 0)
      srMenuRunCursor = 0;
    if (srMenuRunCursor >= (int)filtered.size())
      srMenuRunCursor = (int)filtered.size() - 1;
  }

  // Rueda del ratón
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f && !srMenuDeleteConfirm) {
    srMenuRunScroll -= (int)wheel;
    if (srMenuRunScroll < 0)
      srMenuRunScroll = 0;
    int maxScroll = std::max(0, (int)filtered.size() - visRows);
    if (srMenuRunScroll > maxScroll)
      srMenuRunScroll = maxScroll;
  }

  // Dibujar filas
  for (int r = 0; r < visRows; ++r) {
    int idx = srMenuRunScroll + r;
    if (idx >= (int)filtered.size())
      break;

    int runOrigIdx = filtered[idx];
    const SpeedrunEntry &e = allRuns[runOrigIdx];
    int ry = topY + r * rowH;

    Rectangle row = {(float)leftX, (float)ry, (float)leftW, (float)(rowH - 2)};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, row);
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      srMenuRunCursor = idx;

    bool selected = (idx == srMenuRunCursor);
    if (selected) {
      DrawRectangleRec(row, (Color){60, 70, 105, 200});
      DrawRectangleLinesEx(row, 1, (Color){100, 140, 200, 255});
    } else if (hover) {
      DrawRectangleRec(row, (Color){40, 45, 70, 180});
    }

    bool isPB = (runOrigIdx == bestIdx);
    Color cDate = selected ? RAYWHITE : (isPB ? GOLD : LIGHTGRAY);
    Color cTime = isPB ? GOLD : RAYWHITE;

    // Fecha
    char dateStr[24];
    std::tm *tm_ = std::localtime(&e.date);
    if (tm_)
      std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M", tm_);
    else
      snprintf(dateStr, sizeof(dateStr), "---");
    UI::T(dateStr, leftX + 10, ry + 4, 13, cDate);

    // Tiempo (derecha)
    char tbuf[32];
    Util::formatSplitTime(e.total, tbuf, sizeof(tbuf));
    int tw = UI::M(tbuf, 13);
    UI::T(tbuf, leftX + leftW - tw - 10, ry + 4, 13, cTime);

    // Delta vs PB (solo si no es el PB)
    if (!isPB && bestTotal < 1e29f) {
      float delta = e.total - bestTotal;
      char dbuf[20];
      snprintf(dbuf, sizeof(dbuf), "+%.3f", delta);
      int dw = UI::M(dbuf, 11);
      UI::T(dbuf, leftX + leftW - tw - dw - 22, ry + 6, 11,
            (Color){220, 100, 100, 255});
    }

    // Estrella de PB
    if (isPB) {
      UI::T("*", leftX + 2, ry + 4, 13, GOLD);
    }
  }

  // =========================================================
  //  Columna derecha: tabla de splits de la mejor run
  // =========================================================
  if (bestIdx >= 0) {
    const SpeedrunEntry &best = allRuns[bestIdx];

    // Encontrar la última run de esta categoría (distinta del PB)
    const SpeedrunEntry *last = nullptr;
    std::time_t lastDate = 0;
    for (int i : filtered) {
      if (i == bestIdx)
        continue;
      if (allRuns[i].date > lastDate) {
        lastDate = allRuns[i].date;
        last = &allRuns[i];
      }
    }

    int ry = topY - 22;

    // Headers
    UI::T(TextFormat("%s", L("speedrun.split", "Split")), rightX, ry, 14,
          LIGHTGRAY);
    ry += 20;

    const int colNumX = rightX;
    const int colBestX = rightX + 60;
    const int colLastX = rightX + 220;

    UI::T("#", colNumX + 4, ry, 11, GRAY);
    UI::T(L("speedrun.best", "Mejor"), colBestX, ry, 11, GRAY);
    if (last)
      UI::T(L("speedrun.last", "Última"), colLastX, ry, 11, GRAY);
    ry += 18;

    // Separador
    DrawRectangle(rightX, ry, rightW, 1, (Color){80, 90, 120, 160});
    ry += 6;

    int numSplits = (int)best.splits.size();
    for (int i = 0; i < numSplits && ry < H - 100; ++i) {
      UI::T(TextFormat("Nv%d", i + 1), colNumX + 4, ry, 12, LIGHTGRAY);

      char tbuf[32];
      Util::formatSplitTime(best.splits[i], tbuf, sizeof(tbuf));
      UI::T(tbuf, colBestX, ry, 12, GOLD);

      if (last && i < (int)last->splits.size()) {
        Util::formatSplitTime(last->splits[i], tbuf, sizeof(tbuf));
        Color c = (last->splits[i] <= best.splits[i] + 0.001f)
                      ? (Color){100, 240, 100, 255}
                      : (Color){220, 180, 100, 255};
        UI::T(tbuf, colLastX, ry, 12, c);
      }
      ry += 16;
    }

    // Total de la mejor run al pie de la tabla
    ry += 6;
    DrawRectangle(rightX, ry, rightW, 1, (Color){80, 90, 120, 160});
    ry += 6;
    {
      char tbuf[32];
      Util::formatSplitTime(best.total, tbuf, sizeof(tbuf));
      UI::T(TextFormat("%s: %s", L("speedrun.besttotal", "Mejor total"), tbuf),
            colNumX + 4, ry, 13, GOLD);
    }
  }

  // =========================================================
  //  Borrado con confirmación
  // =========================================================
  if (!srMenuDeleteConfirm) {
    bool deletePressed =
        IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE);
    // También permitimos borrar con "Supr" del gamepad (Right Face Left)
    for (int p = 0; p < 2; ++p) {
      if (players[p].gamepadId >= 0 &&
          IsGamepadButtonPressed(players[p].gamepadId,
                                 GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
        deletePressed = true;
    }

    if (deletePressed && !filtered.empty() && srMenuRunCursor >= 0 &&
        srMenuRunCursor < (int)filtered.size()) {
      srMenuDeleteIndex = filtered[srMenuRunCursor];
      srMenuDeleteConfirm = true;
    }
  }

  if (srMenuDeleteConfirm) {
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 180});

    const char *q = L("speedrun.delete.confirm", "¿Borrar esta run?");
    int qw = UI::M(q, 26);
    UI::T(q, W / 2 - qw / 2, H / 2 - 40, 26, RAYWHITE);

    auto confirmBtn = [&](int idx, const char *label) -> bool {
      int bw = 160, bh = 50, gap = 30;
      int bx = W / 2 - bw - gap / 2 + idx * (bw + gap);
      int by = H / 2 + 20;
      Rectangle r = {(float)bx, (float)by, (float)bw, (float)bh};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      Color bg = hover ? (idx == 0 ? (Color){140, 60, 60, 255}
                                   : (Color){60, 90, 140, 255})
                       : (Color){40, 45, 65, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2, RAYWHITE);
      int fs = 22;
      int tw = UI::M(label, fs);
      UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs, WHITE);
      return click;
    };

    bool yes = confirmBtn(0, L("common.yes", "Sí"));
    bool no = confirmBtn(1, L("common.no", "No"));

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N))
      no = true;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Y))
      yes = true;

    if (yes) {
      if (srMenuDeleteIndex >= 0) {
        saves.removeSpeedrunAt(srMenuDeleteIndex);
        srMenuDeleteIndex = -1;
        // Ajustar cursor si apuntaba fuera de rango
        if (srMenuRunCursor >= (int)saves.speedruns().size() - 1)
          srMenuRunCursor = std::max(0, (int)saves.speedruns().size() - 2);
      }
      srMenuDeleteConfirm = false;
    }
    if (no) {
      srMenuDeleteConfirm = false;
      srMenuDeleteIndex = -1;
    }
    return;
  }

  // =========================================================
  //  Salir
  // =========================================================
  if (IsKeyPressed(KEY_ESCAPE) || consumeCancel()) {
    state = ST_MENU;
    navMenu = 0;
    srMenuRunCursor = 0;
    srMenuRunScroll = 0;
  }

  // =========================================================
  //  Hints al pie
  // =========================================================
  const char *hint =
      L("speedrun.hint2", "Arriba/Abajo: navegar runs   Izq/Der: categoría   "
                          "Supr: borrar   ESC: volver");
  UI::T(hint, W / 2 - UI::M(hint, 13) / 2, H - 28, 13, GRAY);
}

// =========================================================================
//  draw / tick / run
// =========================================================================
void Game::draw() {
  if (state == ST_PLAYING && twoPlayers) {
    drawSplitScreen();

    BeginDrawing();
    ClearBackground(BLACK);

    DrawTextureRec(
        rtLeft.texture,
        {0, 0, (float)rtLeft.texture.width, -(float)rtLeft.texture.height},
        {0, 0}, WHITE);
    DrawTextureRec(
        rtRight.texture,
        {0, 0, (float)rtRight.texture.width, -(float)rtRight.texture.height},
        {(float)rtLeft.texture.width, 0}, WHITE);

    DrawRectangle(rtLeft.texture.width - 2, 0, 4, rtH, BLACK);

    drawToast();
    drawDebugOverlay(GetScreenWidth(), GetScreenHeight(), -1);
    if (cfg.showFps && !Debug::showOverlay)
      DrawFPS(GetScreenWidth() - 90, 20);
    EndDrawing();
  } else if (state == ST_PLAYING) {
    BeginDrawing();
    ClearBackground((Color){12, 12, 18, 255});
    renderPlayerView(players[0]);
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
  } else if (state == ST_DIALOG) {
    BeginDrawing();
    if (twoPlayers) {
      drawSplitScreen();
      DrawTextureRec(
          rtLeft.texture,
          {0, 0, (float)rtLeft.texture.width, -(float)rtLeft.texture.height},
          {0, 0}, WHITE);
      DrawTextureRec(
          rtRight.texture,
          {0, 0, (float)rtRight.texture.width, -(float)rtRight.texture.height},
          {(float)rtLeft.texture.width, 0}, WHITE);
    } else {
      ClearBackground((Color){12, 12, 18, 255});
      renderPlayerView(players[0]);
      drawGameplayView(players[0], GetScreenWidth(), GetScreenHeight(), 0);
    }
    drawDialog();
    EndDrawing();
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

  // VSync a runtime
  if (cfg.vsync)
    SetWindowState(FLAG_VSYNC_HINT);
  else
    ClearWindowState(FLAG_VSYNC_HINT);

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

  // Resolución raycaster según config
  int rcIdx = std::clamp(cfg.raycastRes, 0, C::RC_RES_COUNT - 1);
  rc.resize(C::RC_RESOLUTIONS[rcIdx][0], C::RC_RESOLUTIONS[rcIdx][1]);

  UI::scale = cfg.guiScale;
  UI::colorblind = cfg.colorblindMode;

  ensureRenderTargets();
}

// =========================================================================
//  Debug
// =========================================================================

// =========================================================================
//  Jumpscare
// =========================================================================
void Game::startJumpscare(int enemyIdx) {
  jumpscareEnemy = enemyIdx;
  jumpscareTimer = C::JUMPSCARE_DURATION;
  state = ST_JUMPSCARE;
  EnableCursor();
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

  float t = 1.0f - (jumpscareTimer / C::JUMPSCARE_DURATION);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  float sizeScale;
  if (t < C::JUMPSCARE_ZOOM_IN) {
    float k = t / C::JUMPSCARE_ZOOM_IN;
    k = 1.0f - (1.0f - k) * (1.0f - k);
    sizeScale = 0.5f + k * 1.2f;
  } else {
    sizeScale = 1.7f;
  }

  float shake = 0.0f;
  if (t >= C::JUMPSCARE_ZOOM_IN) {
    float k = (t - C::JUMPSCARE_ZOOM_IN) / (1.0f - C::JUMPSCARE_ZOOM_IN);
    shake = (1.0f - k) * 14.0f;
  }

  float dx = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;
  float dy = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;
  float drawSize = (float)H * sizeScale;
  float x = W / 2.0f + dx;
  float y = H / 2.0f + dy;

  Color tint = WHITE;
  if (t < 0.2f) {
    unsigned char k = (unsigned char)((1.0f - t / 0.2f) * 100);
    tint =
        (Color){255, (unsigned char)(255 - k), (unsigned char)(255 - k), 255};
  }

  if (tex.id != 0) {
    Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
    Rectangle dst = {x - drawSize / 2.0f, y - drawSize / 2.0f, drawSize,
                     drawSize};
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
  } else {
    Color c = (kind == ENEMY_BEA)      ? (Color){210, 45, 60, 255}
              : (kind == ENEMY_MARISA) ? (Color){60, 90, 210, 255}
                                       : (Color){160, 40, 200, 255};
    DrawCircle((int)x, (int)y, drawSize * 0.4f, c);
  }

  if (t < 0.08f) {
    unsigned char a = (unsigned char)((1.0f - t / 0.08f) * 255);
    DrawRectangle(0, 0, W, H, (Color){255, 255, 255, a});
  }
  if (t > 0.85f) {
    float k = (t - 0.85f) / 0.15f;
    unsigned char a = (unsigned char)(k * 255);
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, a});
  }
  EndDrawing();
}

// =========================================================================
//  Items en el mundo
// =========================================================================
void Game::spawnWorldItems(int lvl, int size, std::mt19937 &rng) {
  int im = 2;
  if (const SaveEntry *cur = saves.find(currentSaveId))
    im = cur->itemMult;
  im = std::clamp(im, 0, C::ENDLESS_MULT_COUNT - 1);

  float mult = (mode == MODE_ENDLESS) ? C::ENDLESS_MULT[im] : 1.0f;
  int count = std::max(0, (int)((float)std::min(6 + lvl / 2, 8) * mult));
  if (count == 0)
    return;

  float minDistFromSpawn = size * C::CELL * 0.20f;

  struct Weighted {
    ItemKind kind;
    int weight;
  };
  const Weighted pool[] = {{ITEM_ROCK, 30},    {ITEM_BOX, 20},
                           {ITEM_MONSTER, 20}, {ITEM_LIME, 15},
                           {ITEM_BICIMAD, 10}, {ITEM_WALL, 5}};
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

// =========================================================================
//  Interacción
// =========================================================================

// =========================================================================
//  HUD items
// =========================================================================

// =========================================================================
//  Niveles custom
// =========================================================================
bool Game::tryLoadCustomLevel(int lvl, LevelExtras &extras) {
  if (mode != MODE_STORY)
    return false;

  char fname[64];
  snprintf(fname, sizeof(fname), "level_%02d_map.dgn", lvl);
  std::string full = Paths::level(fname);

  FILE *f = fopen(full.c_str(), "rb");
  if (!f)
    return false;
  fclose(f);
  return maze.loadFromFile(full.c_str(), extras);
}

void Game::spawnEnemiesFromPositions(const std::vector<Vector3> &positions,
                                     int lvl, std::mt19937 &rng) {
  float dm = Util::diffMultFor(currentDifficulty);
  float spd = C::BASE_ESPD * (1.0f + 0.15f * (float)(lvl - 1)) * dm;

  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;

    Enemy e;
    e.pos = p;
    e.dir = {0, 0};
    e.speed = spd * (0.9f + 0.2f * (float)(rng() % 100) / 100.0f);
    e.turnT = 0.0f;
    e.prevPos = p;

    int r = (int)(rng() % 100);
    if (lvl >= 5 && r > 10)
      e.kind = ENEMY_HYPERGAMY;
    else if (lvl >= 4 && r < 30)
      e.kind = ENEMY_ALMEIDA;
    else if (lvl >= 3 && r < 30)
      e.kind = ENEMY_CV;
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
}

void Game::spawnItemsFromPositions(const std::vector<Vector3> &positions,
                                   std::mt19937 &rng) {
  struct Weighted {
    ItemKind kind;
    int weight;
  };
  const Weighted pool[] = {{ITEM_ROCK, 30},    {ITEM_BOX, 20},
                           {ITEM_MONSTER, 20}, {ITEM_LIME, 15},
                           {ITEM_BICIMAD, 10}, {ITEM_WALL, 5}};
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

  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;
    WorldPickup wp;
    wp.kind = rollKind();
    wp.pos = {p.x, p.y + 0.35f, p.z}; // ← usar p.y (altura del suelo)
    wp.bobT = (float)(rng() % 100) / 100.0f * 6.28f;
    worldItems.push_back(wp);
  }
}

void Game::spawnKeysFromPositions(const std::vector<Vector3> &positions) {
  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;
    WorldPickup wp;
    wp.kind = ITEM_KEY;
    wp.pos = {p.x, p.y + 0.55f, p.z}; // ← usar p.y
    wp.bobT = 0.0f;
    worldItems.push_back(wp);
  }
}

// =========================================================================
//  startLevel
// =========================================================================
void Game::startLevel(int lvl) {
  bool usedCustom = false;
  LevelExtras extras;

  if (tryLoadCustomLevel(lvl, extras)) {
    usedCustom = true;
    for (Player &p : players)
      p.hasKey = false;
    currentSeed = 0;
  } else {
    int size = 11 + (lvl - 1) * 2;
    if (mode == MODE_ENDLESS && size > C::ENDLESS_MAX_SIZE)
      size = C::ENDLESS_MAX_SIZE;

    const SaveEntry *cur = saves.find(currentSaveId);
    bool srEnabled = (cur && cur->speedrun);

    uint32_t baseSeed;
    if (mode == MODE_ENDLESS && runSeed != 0) {
      baseSeed = runSeed ^ (uint32_t)(lvl * 7919);
    } else if (srEnabled && runSeed != 0) {
      baseSeed = runSeed ^ (uint32_t)(lvl * 7919);
    } else {
      baseSeed = (uint32_t)time(nullptr) ^ (uint32_t)(lvl * 7919);
    }
    currentSeed = baseSeed;
    maze.generate(size, size, baseSeed);
  }

  maze.buildChunks(assets);

  {
    int idx = std::clamp(cfg.raycastRes, 0, C::RC_RES_COUNT - 1);
    int rw = C::RC_RESOLUTIONS[idx][0];
    int rh = C::RC_RESOLUTIONS[idx][1];
    if (rc.renderW != rw || rc.renderH != rh) {
      rc.resize(rw, rh);
    }
    if (rc.texture.id != 0) {
      SetTextureFilter(rc.texture, TEXTURE_FILTER_POINT);
    }
  }

  rc.loadDynamicTextures(assets);
  levelTime = 0.0f;

  Vector3 p1Spawn, p2Spawn;
  if (usedCustom && !extras.spawns.empty()) {
    p1Spawn = extras.spawns[0];
    if (extras.spawns.size() >= 2) {
      p2Spawn = extras.spawns[1];
    } else {
      Vector2 c = maze.worldToCell(p1Spawn.x, p1Spawn.z);
      p2Spawn = findFreeSpawnNear(maze, (int)c.x, (int)c.y, p1Spawn);
    }
  } else {
    p1Spawn = maze.cellCenter(1, 1);
    p2Spawn = findFreeSpawnNear(maze, 1, 1, p1Spawn);
  }

  resetPlayerState(players[0], p1Spawn);
  if (twoPlayers)
    resetPlayerState(players[1], p2Spawn);

  if (usedCustom) {
    exitPos = extras.exit;

    currentSigns = extras.signs;
    currentNpcs = extras.npcs;
  } else {
    exitPos = maze.cellCenter(maze.w - 2, maze.h - 2);
    exitPos.y = maze.groundZAtWorld(exitPos.x, exitPos.z);

    currentSigns.clear();
    currentNpcs.clear();
  }

  enemies.clear();
  worldItems.clear();
  projectiles.clear();

  std::mt19937 rng((uint32_t)time(nullptr) ^ (uint32_t)(lvl * 7919) ^
                   0xA5A5A5A5u);

  const SaveEntry *cur = saves.find(currentSaveId);

  if (usedCustom) {
    spawnEnemiesFromPositions(extras.enemies, lvl, rng);
    spawnItemsFromPositions(extras.items, rng);
    spawnKeysFromPositions(extras.keys);
  } else {
    int count;
    if (mode == MODE_ENDLESS) {
      int base = std::min(2 + lvl / 2, 12);
      int em = (cur ? cur->enemyMult : 2);
      em = std::clamp(em, 0, C::ENDLESS_MULT_COUNT - 1);
      count = std::max(0, (int)((float)base * C::ENDLESS_MULT[em]));
    } else {
      count = lvl + 1;
    }

    float dm = Util::diffMultFor(currentDifficulty);
    float spd = C::BASE_ESPD * (1.0f + 0.15f * (float)(lvl - 1)) * dm;
    float minDist = maze.w * C::CELL * 0.35f;

    int attempts = 0;
    while ((int)enemies.size() < count && attempts < 1000) {
      ++attempts;
      int cx = 1 + (int)(rng() % (maze.w - 2));
      int cy = 1 + (int)(rng() % (maze.h - 2));
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
      if (lvl >= 5 && r > 10)
        e.kind = ENEMY_HYPERGAMY;
      else if (lvl >= 4 && r < 30)
        e.kind = ENEMY_ALMEIDA;
      else if (lvl >= 3 && r < 30)
        e.kind = ENEMY_CV;
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

    spawnWorldItems(lvl, maze.w, rng);
  }

  fountainCooldown[0] = 0.0f;
  fountainCooldown[1] = 0.0f;

  if (sr.active) {
    sr.lastLevelStart = totalPlaytime;
    // Preparar slot para el nivel actual
    if ((int)srGhostRec.levels.size() < level)
      srGhostRec.levels.resize(level);
    srGhostRec.levels[level - 1] = GhostLevel{};
    srGhostSampleTimer = 0.0f;
  }

  maze.markExplored(players[0].pos.x, players[0].pos.z, C::FOG_RADIUS);
  if (twoPlayers)
    maze.markExplored(players[1].pos.x, players[1].pos.z, C::FOG_RADIUS);
}

// =========================================================================
//  Level details / intro
// =========================================================================
bool Game::loadLevelDetails(int lvl) {
  levelDetails = {};

  std::string path = Util::resolveLevelDetailsPath(lvl);
  if (path.empty())
    return false;

  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;

  levelDetails.loaded = true;
  levelDetails.difficulty = 1;

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    for (char *p = line; *p; ++p)
      if (*p == '\r' || *p == '\n') {
        *p = 0;
        break;
      }
    if (line[0] == '#' || line[0] == 0)
      continue;

    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = 0;
    const char *key = line;
    const char *val = eq + 1;

    if (!strcmp(key, "name"))
      levelDetails.name = val;
    else if (!strcmp(key, "subtitle"))
      levelDetails.subtitle = val;
    else if (!strcmp(key, "difficulty"))
      levelDetails.difficulty = atoi(val);
  }
  fclose(f);

  levelDetails.difficulty = std::clamp(levelDetails.difficulty, 1, 5);
  return true;
}

void Game::requestEnterLevel(int lvl) {
  level = lvl;
  assignGamepads();

  if (mode == MODE_STORY && loadLevelDetails(lvl)) {
    state = ST_LEVEL_INTRO;
    introTimer = 0.0f;
    return;
  }
  enterLevel(lvl);
}

void Game::enterLevel(int lvl) {
  if (mode == MODE_STORY && cfg.playCinematics) {
    char name[64];
    snprintf(name, sizeof(name), "level_%02d_intro.txt", lvl);
    if (hasStory(name)) {
      playCinematic(name, "playing");
      return;
    }
  }
  startLevel(lvl);
  assignGamepads();
  DisableCursor();
  state = ST_PLAYING;
  if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
    PlayMusicStream(assets.musicLevel);
    musicLevelPlaying = true;
  }
}

void Game::drawLevelIntro() {
  int W = GetScreenWidth();
  int H = GetScreenHeight();

  ClearBackground((Color){8, 8, 12, 255});
  drawBackground2D();
  DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 170});

  float t = std::min(1.0f, introTimer / 0.8f);
  unsigned char a = (unsigned char)(t * 255.0f);

  char cap[64];
  snprintf(cap, sizeof(cap), L("intro.chapter", "Capítulo %d"), level);
  int capSize = 22;
  int capW = UI::M(cap, capSize);
  UI::T(cap, W / 2 - capW / 2, H / 3 - 70, capSize, (Color){200, 200, 200, a});

  int lineW = 260;
  DrawRectangle(W / 2 - lineW / 2, H / 3 - 36, lineW, 2,
                (Color){180, 140, 60, a});

  const char *title = levelDetails.name.empty()
                          ? L("intro.untitled", "Sin título")
                          : levelDetails.name.c_str();
  int titleSize = 60;
  int tw = UI::M(title, titleSize);
  if (tw > W - 80) {
    titleSize = 42;
    tw = UI::M(title, titleSize);
  }
  UI::T(title, W / 2 - tw / 2, H / 3, titleSize, (Color){255, 240, 200, a});

  if (!levelDetails.subtitle.empty()) {
    int subSize = 24;
    int sw = UI::M(levelDetails.subtitle.c_str(), subSize);
    UI::T(levelDetails.subtitle.c_str(), W / 2 - sw / 2, H / 3 + titleSize + 16,
          subSize, (Color){180, 180, 190, a});
  }

  int stars = std::clamp(levelDetails.difficulty, 1, 5);
  float starR = 16.0f;
  float starGap = 12.0f;
  float totalW = 5 * (starR * 2) + 4 * starGap;
  float sx = W / 2.0f - totalW / 2.0f + starR;
  float sy = H * 2.0f / 3.0f;
  for (int i = 0; i < 5; ++i) {
    float cx = sx + i * (starR * 2 + starGap);
    if (i < stars)
      Util::drawDifficultyStar(cx, sy, starR, (Color){255, 200, 70, a}, true);
    else
      Util::drawDifficultyStar(cx, sy, starR, (Color){70, 70, 85, a}, false);
  }

  const char *diffLbl = L("intro.difficulty", "Dificultad");
  int dlSize = 16;
  int dlW = UI::M(diffLbl, dlSize);
  UI::T(diffLbl, W / 2 - dlW / 2, (int)sy - starR - 32, dlSize,
        (Color){150, 150, 160, a});

  if (t >= 1.0f) {
    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 3.0f);
    unsigned char ha = (unsigned char)(pulse * 220);
    const char *hint = L("intro.continue", "Pulsa ENTER para continuar");
    int hs = 18;
    int hw = UI::M(hint, hs);
    UI::T(hint, W / 2 - hw / 2, H - 60, hs, (Color){200, 200, 200, ha});
  }
}

// =========================================================================
//  Speedrun helpers
// =========================================================================
void Game::speedrunStartRun(uint32_t seed, int mode) {
  sr.active = true;
  sr.seed = seed;
  sr.mode = mode;
  sr.splits.clear();
  sr.bestSplits.clear();
  sr.lastLevelStart = totalPlaytime;
  sr.lastSplitDelta = 0.0f;
  sr.justRecordedPB = false;
  sr.pbFlashTimer = 0.0f;
  saves.bestSplitsFor(seed, mode, sr.bestSplits);

  // Ghost: iniciar grabación vacía y cargar playback del PB
  srGhostRec = GhostRun{};
  srGhostRec.seed = seed;
  srGhostRec.mode = mode;
  srGhostSampleTimer = 0.0f;

  srGhostPlayback = GhostRun{};
  srGhostPlaybackValid = saves.loadGhost(seed, mode, srGhostPlayback);
}

void Game::speedrunRecordSplit() {
  if (!sr.active)
    return;

  float split = totalPlaytime - sr.lastLevelStart;
  sr.lastLevelStart = totalPlaytime;
  sr.splits.push_back(split);

  int idx = (int)sr.splits.size() - 1;
  if (idx < (int)sr.bestSplits.size() && sr.bestSplits[idx] > 0.0f) {
    sr.lastSplitDelta = split - sr.bestSplits[idx];
    if (split < sr.bestSplits[idx]) {
      sr.bestSplits[idx] = split;
      sr.justRecordedPB = true;
      sr.pbFlashTimer = 2.0f;
    }
  } else {
    if (idx >= (int)sr.bestSplits.size())
      sr.bestSplits.resize(idx + 1, 0.0f);
    sr.bestSplits[idx] = split;
    sr.lastSplitDelta = 0.0f;
    sr.justRecordedPB = true;
    sr.pbFlashTimer = 2.0f;
  }
}

void Game::speedrunFinishRun() {
  if (!sr.active || sr.splits.empty()) {
    sr.active = false;
    return;
  }

  SpeedrunEntry e;
  e.seed = sr.seed;
  e.mode = sr.mode;
  e.date = std::time(nullptr);
  e.total = 0.0f;
  for (float s : sr.splits)
    e.total += s;
  e.splits = sr.splits;

  // ¿Es nuevo PB y completo? → guardar ghost
  float prevBest = 1e30f;
  size_t prevMaxLevels = 0;
  for (auto &r : saves.speedruns()) {
    if (r.seed != e.seed || r.mode != e.mode)
      continue;
    if (r.total < prevBest)
      prevBest = r.total;
    if (r.splits.size() > prevMaxLevels)
      prevMaxLevels = r.splits.size();
  }

  bool isNewPB = (e.total < prevBest - 0.001f);
  bool isComplete = (sr.splits.size() >= prevMaxLevels) || (prevMaxLevels == 0);

  saves.addSpeedrun(e);

  if (isNewPB && isComplete && !srGhostRec.levels.empty()) {
    // Cerrar la duración del último nivel grabado
    int lastLvl = level - 1;
    if (lastLvl >= 0 && lastLvl < (int)srGhostRec.levels.size()) {
      GhostLevel &gl = srGhostRec.levels[lastLvl];
      gl.duration = gl.samples.size() * C::GHOST_SAMPLE_DT;
    }
    saves.saveGhost(sr.seed, sr.mode, srGhostRec);
  }

  sr.active = false;
}

// Interpola la posición del ghost en el nivel `lvlIdx` al tiempo `t`.
// Devuelve false si no hay datos válidos.
static bool sampleGhostAt(const GhostRun &ghost, int lvlIdx, float t,
                          Vector3 &outPos, float &outYaw) {
  if (lvlIdx < 0 || lvlIdx >= (int)ghost.levels.size())
    return false;
  const GhostLevel &gl = ghost.levels[lvlIdx];
  if (gl.samples.empty())
    return false;

  float duration = gl.duration > 0.0f
                       ? gl.duration
                       : (gl.samples.size() * C::GHOST_SAMPLE_DT);
  if (t < 0.0f)
    t = 0.0f;
  if (t >= duration) {
    // Ghost terminó: dejar en la última posición
    const GhostSample &s = gl.samples.back();
    outPos = {s.x * 0.01f, 0.0f, s.z * 0.01f};
    outYaw = (float)s.yaw / 65535.0f * (2.0f * PI) - PI;
    return true;
  }

  float u = t / duration;
  float idx = u * (float)(gl.samples.size() - 1);
  int i0 = (int)idx;
  int i1 = std::min(i0 + 1, (int)gl.samples.size() - 1);
  float f = idx - (float)i0;

  const GhostSample &a = gl.samples[i0];
  const GhostSample &b = gl.samples[i1];
  float x = (float)a.x + ((float)b.x - (float)a.x) * f;
  float z = (float)a.z + ((float)b.z - (float)a.z) * f;
  outPos = {x * 0.01f, 0.0f, z * 0.01f};

  // yaw: interpolación angular (evita saltos en el wraparound)
  float ya = (float)a.yaw / 65535.0f * (2.0f * PI) - PI;
  float yb = (float)b.yaw / 65535.0f * (2.0f * PI) - PI;
  float diff = yb - ya;
  while (diff > PI)
    diff -= 2.0f * PI;
  while (diff < -PI)
    diff += 2.0f * PI;
  outYaw = ya + diff * f;
  return true;
}

void Game::renderPlayerView(const Player &pl) {
  bool levelHasKey = false;
  for (auto &w : worldItems)
    if (w.kind == ITEM_KEY) {
      levelHasKey = true;
      break;
    }
  bool locked = levelHasKey && !(players[0].hasKey || players[1].hasKey);

  // Ghost de la mejor run
  Vector3 ghostPos = {0, 0, 0};
  float ghostYaw = 0.0f;
  bool ghostVisible = false;
  if (sr.active && srGhostPlaybackValid && cfg.srShowGhost) {
    float levelT = totalPlaytime - sr.lastLevelStart;
    ghostVisible =
        sampleGhostAt(srGhostPlayback, level - 1, levelT, ghostPos, ghostYaw);
    ghostPos.y = maze.groundZAtWorld(ghostPos.x, ghostPos.z);
  }

  if (cfg.useRenderer3D) {
    std::vector<WorldSprite3D> sprites;
    sprites.reserve(enemies.size() + worldItems.size() + projectiles.size() +
                    16);

    // 1. Enemigos
    for (auto &e : enemies) {
      int kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
      Color tint = {255, 255, 255, 255};
      if (e.hitFlash > 0.0f)
        tint = {255, 80, 80, 255};
      else if (e.rageT > 0.0f)
        tint = {255, 140, 90, 255};

      Texture2D tex = assets.enemyTex[kind];
      float aspect = (tex.id != 0 && tex.height > 0)
                         ? (float)tex.width / (float)tex.height
                         : 1.0f;
      float height = 2.6f;
      float width = height * aspect;

      WorldSprite3D s;
      s.pos = {e.pos.x, e.pos.y, e.pos.z}; // ← era {e.pos.x, 0.0f, e.pos.z}
      s.size = {width, height};
      s.tex = tex;
      s.tint = tint;
      sprites.push_back(s);
    }

    // 2. Items del mundo
    for (auto &w : worldItems) {
      int k = (int)w.kind;
      if (k < 0 || k >= ITEM_KIND_COUNT)
        continue;

      Texture2D tex = assets.itemTex[k];
      float aspect = (tex.id != 0 && tex.height > 0)
                         ? (float)tex.width / (float)tex.height
                         : 1.0f;
      float height = 0.70f;
      switch (w.kind) {
      case ITEM_ROCK:
        height = 0.35f;
        break;
      case ITEM_BOX:
        height = 0.70f;
        break;
      case ITEM_LIME:
        height = 0.85f;
        break;
      case ITEM_BICIMAD:
        height = 1.00f;
        break;
      case ITEM_KEY:
        height = 0.55f;
        break;
      default:
        height = 0.70f;
        break;
      }
      float width = height * aspect;
      float bob = sinf(w.bobT) * C::ROCK_PICKUP_BOB;

      WorldSprite3D s;
      s.pos = {w.pos.x, w.pos.y + bob, w.pos.z}; // ← w.pos.y ya incluye altura
      s.size = {width, height};
      s.tex = tex;
      s.tint = {255, 255, 255, 255};
      sprites.push_back(s);
    }

    // 3. Proyectiles
    for (auto &pr : projectiles) {
      if (!pr.alive)
        continue;
      int k = (int)pr.kind;
      if (k <= ITEM_NONE || k >= ITEM_KIND_COUNT)
        continue;

      Texture2D tex = assets.itemTex[k];
      float aspect = (tex.id != 0 && tex.height > 0)
                         ? (float)tex.width / (float)tex.height
                         : 1.0f;
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
        size = 1.25f;
        break;
      case ITEM_WALL:
        size = 0.90f;
        break;
      default:
        size = 0.25f;
        break;
      }

      WorldSprite3D s;
      s.pos = {pr.pos.x, pr.pos.y - size * 0.5f, pr.pos.z};
      s.size = {size * aspect, size};
      s.tex = tex;
      s.tint = {255, 255, 255, 255};
      sprites.push_back(s);
    }

    // 4. Tiendas (tents)
    int pcx = (int)floorf(pl.pos.x / C::CELL);
    int pcz = (int)floorf(pl.pos.z / C::CELL);
    for (int y = 0; y < maze.h; ++y) {
      for (int x = 0; x < maze.w; ++x) {
        if (!maze.hasTent(x, y))
          continue;
        if (x == pcx && y == pcz)
          continue;
        Vector3 center = maze.cellCenter(x, y);

        Texture2D tex = assets.itemTex[ITEM_WALL];
        float aspect = (tex.id != 0 && tex.height > 0)
                           ? (float)tex.width / (float)tex.height
                           : 1.0f;
        float height = 1.5f;
        float width = height * aspect;

        WorldSprite3D s;
        s.pos = {center.x, maze.groundZAtWorld(center.x, center.z), center.z};
        s.size = {width, height};
        s.tex = tex;
        s.tint = {255, 255, 255, 255};
        sprites.push_back(s);
      }
    }

    // 4.5. Fuentes (crossed quads, más natural que billboard)
    for (int y = 0; y < maze.h; ++y) {
      for (int x = 0; x < maze.w; ++x) {
        if (!maze.hasFountain(x, y))
          continue;
        Vector3 center = maze.cellCenter(x, y);

        WorldSprite3D s;
        s.pos = {center.x, maze.groundZAtWorld(center.x, center.z), center.z};
        s.size = {1.5f, 1.5f};
        s.tex = assets.fountainTex;
        s.tint = {255, 255, 255, 255};
        s.crossedQuads = true;
        sprites.push_back(s);
      }
    }

    // Carteles
    for (auto &s : currentSigns) {
      WorldSprite3D sp;
      sp.pos = {s.pos.x, s.pos.y + 0.6f, s.pos.z};
      sp.size = {1.0f, 0.8f};
      sp.tex = assets.hasSignTex ? assets.signTex : assets.itemTex[ITEM_WALL];
      sp.tint = {255, 255, 255, 255};
      sprites.push_back(sp);
    }
    // NPCs
    for (auto &n : currentNpcs) {
      WorldSprite3D sp;
      sp.pos = {n.pos.x, n.pos.y, n.pos.z};
      sp.size = {1.6f, 2.4f};
      sp.tex = assets.hasNpcTex ? assets.npcTex : assets.enemyTex[ENEMY_BEA];
      sp.tint = {255, 255, 255, 255};
      sprites.push_back(sp);
    }

    // 5. Portal de salida
    float pulse = 1.0f + 0.15f * sinf((float)GetTime() * 4.0f);
    Color portalTint =
        locked ? (Color){200, 60, 60, 255} : (Color){200, 255, 220, 255};

    Texture2D portalTex = assets.portalTex;
    float aspect = (portalTex.id != 0 && portalTex.height > 0)
                       ? (float)portalTex.width / (float)portalTex.height
                       : 1.0f;
    float height = 2.4f * pulse;
    float width = height * aspect;

    WorldSprite3D sPortal;
    sPortal.pos = {exitPos.x, exitPos.y, exitPos.z}; // ← era 0.0f
    sPortal.size = {width, height};
    sPortal.tex = portalTex;
    sPortal.tint = portalTint;
    sprites.push_back(sPortal);

    if (ghostVisible) {
      Texture2D ghostTex = assets.enemyTex[PLAYER];
      float aspect = (ghostTex.id != 0 && ghostTex.height > 0)
                         ? (float)ghostTex.width / (float)ghostTex.height
                         : 1.0f;
      WorldSprite3D s;
      s.pos = ghostPos;
      s.size = {2.6f * aspect, 2.6f};
      s.tex = ghostTex;
      s.tint = {140, 170, 255, 110};
      sprites.push_back(s);
    }

    // Renderizar con Camera3D
    Vector3 eye = {pl.pos.x, pl.pos.y + C::EYE_H, pl.pos.z};
    r3d.render(maze, eye, pl.yaw, pl.pitch, (float)cfg.fov, renderDist(),
               sprites, true);

    if (cfg.useRenderer3D) {
      static int logTick = 0;
      if (++logTick % 60 == 0) {
      }
    }
  } else {
    // Fallback al raycaster clásico (intacto)
    rc.render(pl, maze, enemies, projectiles, worldItems, exitPos, locked,
              renderDist(), (float)cfg.fov);
  }
}

// =========================================================================
//  Config: speedrun overlay
// =========================================================================
static const char *srAnchorLabel(int idx) {
  static const char *labels[9] = {
      "Arriba izq", "Arriba centro", "Arriba der",   "Medio izq", "Centro",
      "Medio der",  "Abajo izq",     "Abajo centro", "Abajo der",
  };
  return labels[std::clamp(idx, 0, 8)];
}

static const char *srSizeLabel(int idx) {
  static const char *labels[3] = {"Pequeño", "Medio", "Grande"};
  return labels[std::clamp(idx, 0, 2)];
}

void Game::drawConfigSpeedrun() {
  drawConfigHeader(L("config.speedrun", "Speedrun"));
  int W = GetScreenWidth(), H = GetScreenHeight();

  // ----------------------------------------------------------
  // Preview: dibujar mock del overlay en su posición real.
  // Se dibuja ANTES de las opciones, así los menús van encima.
  // ----------------------------------------------------------
  {
    int boxW, boxH;
    switch (cfg.srOverlaySize) {
    case 0:
      boxW = 220;
      boxH = 80;
      break;
    case 1:
      boxW = 300;
      boxH = 140;
      break;
    default:
      boxW = 400;
      boxH = 200;
      break;
    }

    int col = cfg.srOverlayAnchor % 3;
    int row = cfg.srOverlayAnchor / 3;
    const int M = 12;

    int px = (col == 0) ? M : (col == 1) ? (W - boxW) / 2 : (W - boxW - M);
    int py = (row == 0) ? M : (row == 1) ? (H - boxH) / 2 : (H - boxH - M);
    px += cfg.srOverlayOffsetX;
    py += cfg.srOverlayOffsetY;
    if (px < 0)
      px = 0;
    if (py < 0)
      py = 0;
    if (px + boxW > W)
      px = W - boxW;
    if (py + boxH > H)
      py = H - boxH;

    DrawRectangle(px, py, boxW, boxH, (Color){0, 0, 0, 140});
    DrawRectangleLines(px, py, boxW, boxH, (Color){255, 200, 60, 100});

    int tx = px + 10;
    int ty = py + 8;

    // Header
    UI::T("Nivel 3", tx, ty, 16, RAYWHITE);
    if (boxW > 260) {
      int rw = UI::M("PB: 42.123", 11);
      UI::T("PB: 42.123", px + boxW - rw - 8, ty, 11, GRAY);
    }
    ty += 20;

    // Tiempo actual
    UI::T("12.345", tx, ty, 22, GOLD);
    ty += 26;

    // Delta
    if (cfg.srShowDelta) {
      UI::T("-0.234", tx, ty, 16, (Color){100, 240, 100, 255});
      ty += 22;
    }

    // Splits previos (en tamaño grande)
    if (cfg.srPrevSplitsCount > 0 && boxH > 110) {
      int maxRows = std::min(cfg.srPrevSplitsCount, 5);
      for (int i = 0; i < maxRows && ty + 14 < py + boxH - 30; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "Nv%d  10.%03d  -0.%03d", i + 1,
                 100 + i * 47, i * 20);
        UI::T(line, tx, ty, 11, LIGHTGRAY);
        ty += 14;
      }
    }

    // Total + Pred (abajo)
    if ((cfg.srShowTotal || cfg.srShowPredicted) && boxH > 110) {
      int by = py + boxH - 20;
      if (cfg.srShowTotal) {
        UI::T("Tot: 1:12.345", tx, by, 11, GOLD);
        by += 12;
      }
      if (cfg.srShowPredicted) {
        UI::T("Pred: 1:08.000", tx, by, 11, (Color){200, 200, 120, 255});
      }
    }
  }

  // ----------------------------------------------------------
  // Opciones: 9 filas, layout compacto (46px por fila)
  // ----------------------------------------------------------
  const int items = 11;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  if (IsKeyPressed(KEY_LEFT))
    lr = -1;
  if (IsKeyPressed(KEY_RIGHT))
    lr = +1;
  bool confirm = consumeConfirm();

  const int rowH = 46;
  const int topY = H / 2 - (items * rowH) / 2 + 40;

  auto drawRow = [&](int row, const char *label,
                     const char *value) -> UIAction {
    int x = W / 2 - 340;
    int y = topY + row * rowH;
    Rectangle r = {(float)x, (float)y, 680.0f, (float)(rowH - 6)};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    Vector2 md = GetMouseDelta();
    if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
      navIndex = row;

    bool selected = (navIndex == row);
    UIAction result = UI_NONE;

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (value && value[0])
        result = (mp.x < x + 340) ? UI_LEFT : UI_RIGHT;
      else
        result = UI_CONFIRM;
    }
    if (selected) {
      if (confirm) {
        confirm = false;
        if (result == UI_NONE)
          result = UI_CONFIRM;
      }
      if (lr < 0 && result == UI_NONE)
        result = UI_LEFT;
      if (lr > 0 && result == UI_NONE)
        result = UI_RIGHT;
    }

    Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                   : (Color){35, 40, 58, 255};
    Color brd = (hover || selected) ? SKYBLUE : (Color){80, 90, 125, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, brd);
    UI::T(label, x + 20, y + (rowH - 6 - 20) / 2, 20, RAYWHITE);
    if (value && value[0]) {
      char buf[128];
      snprintf(buf, sizeof(buf), "< %s >", value);
      int vw = UI::M(buf, 20);
      UI::T(buf, x + 680 - vw - 20, y + (rowH - 6 - 20) / 2, 20, GOLD);
    }
    return result;
  };

  auto save = [&]() { cfg.save(); };

  // 0) Posición
  {
    UIAction a = drawRow(0, L("sr.anchor", "Posición"),
                         srAnchorLabel(cfg.srOverlayAnchor));
    if (a == UI_LEFT)
      cfg.srOverlayAnchor = (cfg.srOverlayAnchor + 8) % 9;
    if (a == UI_RIGHT)
      cfg.srOverlayAnchor = (cfg.srOverlayAnchor + 1) % 9;
    if (a == UI_CONFIRM)
      cfg.srOverlayAnchor = (cfg.srOverlayAnchor + 1) % 9;
    if (a != UI_NONE)
      save();
  }

  // 1) Tamaño
  {
    UIAction a =
        drawRow(1, L("sr.size", "Tamaño"), srSizeLabel(cfg.srOverlaySize));
    if (a == UI_LEFT)
      cfg.srOverlaySize = std::max(0, cfg.srOverlaySize - 1);
    if (a == UI_RIGHT)
      cfg.srOverlaySize = std::min(2, cfg.srOverlaySize + 1);
    if (a == UI_CONFIRM)
      cfg.srOverlaySize = 1;
    if (a != UI_NONE)
      save();
  }

  // 2) Ajuste X
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cfg.srOverlayOffsetX);
    UIAction a = drawRow(2, L("sr.offsetx", "Ajuste X"), buf);
    if (a == UI_LEFT)
      cfg.srOverlayOffsetX = std::max(-200, cfg.srOverlayOffsetX - 10);
    if (a == UI_RIGHT)
      cfg.srOverlayOffsetX = std::min(200, cfg.srOverlayOffsetX + 10);
    if (a == UI_CONFIRM)
      cfg.srOverlayOffsetX = 0;
    if (a != UI_NONE)
      save();
  }

  // 3) Ajuste Y
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cfg.srOverlayOffsetY);
    UIAction a = drawRow(3, L("sr.offsety", "Ajuste Y"), buf);
    if (a == UI_LEFT)
      cfg.srOverlayOffsetY = std::max(-200, cfg.srOverlayOffsetY - 10);
    if (a == UI_RIGHT)
      cfg.srOverlayOffsetY = std::min(200, cfg.srOverlayOffsetY + 10);
    if (a == UI_CONFIRM)
      cfg.srOverlayOffsetY = 0;
    if (a != UI_NONE)
      save();
  }

  // 4) Delta
  {
    UIAction a = drawRow(4, L("sr.showdelta", "Delta vs PB"),
                         Util::yesNo(cfg.srShowDelta));
    if (a != UI_NONE) {
      cfg.srShowDelta = !cfg.srShowDelta;
      save();
    }
  }

  // 5) Tiempo total
  {
    UIAction a = drawRow(5, L("sr.showtotal", "Tiempo total"),
                         Util::yesNo(cfg.srShowTotal));
    if (a != UI_NONE) {
      cfg.srShowTotal = !cfg.srShowTotal;
      save();
    }
  }

  // 6) Predicción
  {
    UIAction a = drawRow(6, L("sr.showpredicted", "Predicción"),
                         Util::yesNo(cfg.srShowPredicted));
    if (a != UI_NONE) {
      cfg.srShowPredicted = !cfg.srShowPredicted;
      save();
    }
  }

  // 7) Flash PB
  {
    UIAction a =
        drawRow(7, L("sr.showpb", "Flash PB"), Util::yesNo(cfg.srShowPbFlash));
    if (a != UI_NONE) {
      cfg.srShowPbFlash = !cfg.srShowPbFlash;
      save();
    }
  }

  // 8) Splits previos
  {
    char buf[16];
    if (cfg.srPrevSplitsCount == 0)
      snprintf(buf, sizeof(buf), "%s", L("sr.hidden", "Ocultos"));
    else
      snprintf(buf, sizeof(buf), "%d", cfg.srPrevSplitsCount);
    UIAction a = drawRow(8, L("sr.prevcount", "Splits previos"), buf);
    if (a == UI_LEFT)
      cfg.srPrevSplitsCount = std::max(0, cfg.srPrevSplitsCount - 1);
    if (a == UI_RIGHT)
      cfg.srPrevSplitsCount = std::min(10, cfg.srPrevSplitsCount + 1);
    if (a == UI_CONFIRM)
      cfg.srPrevSplitsCount = 5;
    if (a != UI_NONE)
      save();
  }

  // 9) Mostrar ghost
  {
    UIAction a = drawRow(9, L("sr.showghost", "Mostrar ghost"),
                         Util::yesNo(cfg.srShowGhost));
    if (a != UI_NONE) {
      cfg.srShowGhost = !cfg.srShowGhost;
      save();
    }
  }

  // 10) Volver
  {
    UIAction a = drawRow(10, L("common.back", "Volver"), "");
    if (a != UI_NONE || IsKeyPressed(KEY_ESCAPE) || consumeCancel())
      state = ST_CONFIG;
  }
}
