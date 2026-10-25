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

// =========================================================================
//  Cinemáticas
// =========================================================================

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
//  Speedrun menu
// =========================================================================

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

// =========================================================================
//  Interacción
// =========================================================================

// =========================================================================
//  HUD items
// =========================================================================

// =========================================================================
//  Niveles custom
// =========================================================================

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

// =========================================================================
//  Level details / intro
// =========================================================================

// =========================================================================
//  Speedrun helpers
// =========================================================================

// Interpola la posición del ghost en el nivel `lvlIdx` al tiempo `t`.
// Devuelve false si no hay datos válidos.
