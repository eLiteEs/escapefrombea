#include "game.h"

#include "common.h"
#include "debug.h"
#include "enemy.h"
#include "lang.h"
#include "paths.h"
#include "player.h"
#include "rlgl.h"
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <random>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

// =========================================================================
//  Helpers estáticos
// =========================================================================
static void formatSplitTime(float sec, char *out, size_t sz) {
  if (sec < 0)
    sec = 0;
  int ms = (int)(sec * 1000.0f + 0.5f);
  int totalS = ms / 1000;
  ms %= 1000;
  int m = totalS / 60, s = totalS % 60;
  if (m > 0)
    snprintf(out, sz, "%d:%02d.%03d", m, s, ms);
  else
    snprintf(out, sz, "%02d.%03d", s, ms);
}

static void formatDelta(float sec, char *out, size_t sz) {
  const char *sign = (sec >= 0) ? "+" : "-";
  float a = fabsf(sec);
  int ms = (int)(a * 1000.0f + 0.5f);
  int totalS = ms / 1000;
  ms %= 1000;
  int m = totalS / 60, s = totalS % 60;
  if (m > 0)
    snprintf(out, sz, "%s%d:%02d.%03d", sign, m, s, ms);
  else
    snprintf(out, sz, "%s%02d.%03d", sign, s, ms);
}

static float diffMultFor(int diff) {
  switch (diff) {
  case DIFF_EXPERT:
    return 1.3f;
  case DIFF_NIGHTMARE:
    return 1.7f;
  default:
    return 1.0f;
  }
}

static std::string resolveStoryPath(const char *fname) {
  auto exists = [](const std::string &p) {
    FILE *f = fopen(p.c_str(), "rb");
    if (!f)
      return false;
    fclose(f);
    return true;
  };
  std::string p = Paths::storyIn(Lang::currentLang(), fname);
  if (exists(p))
    return p;
  if (strcmp(Lang::currentLang(), "es") != 0) {
    p = Paths::storyIn("es", fname);
    if (exists(p))
      return p;
  }
  return "";
}

static std::string resolveLevelDetailsPath(int lvl) {
  char fname[64];
  snprintf(fname, sizeof(fname), "level_%02d_details.dgn", lvl);
  auto exists = [](const std::string &p) {
    FILE *f = fopen(p.c_str(), "rb");
    if (!f)
      return false;
    fclose(f);
    return true;
  };
  std::string p = Paths::levelDetailsIn(Lang::currentLang(), fname);
  if (exists(p))
    return p;
  if (strcmp(Lang::currentLang(), "es") != 0) {
    p = Paths::levelDetailsIn("es", fname);
    if (exists(p))
      return p;
  }
  return "";
}

static const char *yesNo(bool b) {
  return b ? L("common.yes", "Sí") : L("common.no", "No");
}

static const char *pctStr(float v) {
  static char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100 + 0.5f));
  return buf;
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

static void drawDifficultyStar(float cx, float cy, float radius, Color color,
                               bool filled) {
  const int points = 10;
  Vector2 verts[10];
  for (int i = 0; i < points; ++i) {
    float angle = -PI / 2.0f + i * (2.0f * PI / points);
    float r = (i % 2 == 0) ? radius : radius * 0.42f;
    verts[i] = {cx + cosf(angle) * r, cy + sinf(angle) * r};
  }
  if (filled) {
    Vector2 c = {cx, cy};
    for (int i = 0; i < points; ++i) {
      int j = (i + 1) % points;
      DrawTriangle(c, verts[i], verts[j], color);
      DrawTriangle(c, verts[j], verts[i], color);
    }
  } else {
    for (int i = 0; i < points; ++i) {
      int j = (i + 1) % points;
      DrawLineEx(verts[i], verts[j], 2.0f, color);
    }
  }
}

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

  auto drawSplash = [&](float progress, const char *stage) {
    BeginDrawing();
    ClearBackground(WHITE);
    int W = GetScreenWidth();
    int H = GetScreenHeight();
    float barArea = 110.0f;
    float imgAreaH = (float)H - barArea;
    if (imgAreaH < 10.0f)
      imgAreaH = (float)H * 0.7f;

    if (splash.id != 0) {
      float tw = 600;
      float th = 450;
      float tx = ((float)W - tw) / 2.0f;
      float ty = (imgAreaH - th) / 2.0f;
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
    float barY = (float)H - barArea + 30.0f;

    DrawRectangle((int)barX - 2, (int)barY - 2, (int)barW + 4, (int)barH + 4,
                  (Color){0, 0, 0, 200});
    DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH,
                  (Color){30, 32, 45, 255});

    float p = std::clamp(progress, 0.0f, 1.0f);
    DrawRectangle((int)barX, (int)barY, (int)(barW * p), (int)barH,
                  (Color){80, 200, 120, 255});
    DrawRectangleLines((int)barX, (int)barY, (int)barW, (int)barH, RAYWHITE);

    if (stage && stage[0]) {
      int fs = 18;
      UI::T(stage, W / 2 - UI::M(stage, fs) / 2, (int)barY - fs - 6, fs, BLACK);
    }
    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(p * 100.0f));
    int fs2 = 16;
    UI::T(pctBuf, W / 2 - UI::M(pctBuf, fs2) / 2, (int)barY + (int)barH + 6,
          fs2, BLACK);
    EndDrawing();
  };

  drawSplash(0.0f, L("load.starting", "Iniciando..."));

  InitAudioDevice();
  assets.applyVolumes(cfg.masterVol, cfg.musicVol, cfg.sfxVol);

  auto stepLabel = [&](int i) -> const char * {
    switch (i) {
    case 0:
      return L("load.textures", "Cargando enemigos...");
    case 1:
      return L("load.walls", "Cargando paredes...");
    case 2:
      return L("load.items", "Cargando items...");
    case 3:
      return L("load.music", "Cargando música...");
    case 4:
      return L("load.sounds", "Cargando sonidos...");
    case 5:
      return L("load.world", "Preparando mundo...");
    case 6:
      return L("load.fonts", "Preparando fuentes...");
    }
    return "";
  };
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
    drawSplash((float)i / (float)N, stepLabel(i));
    doStep(i);
    drawSplash((float)(i + 1) / (float)N, stepLabel(i));
  }

  float waitStart = (float)GetTime();
  while ((float)GetTime() - waitStart < 0.20f) {
    drawSplash(1.0f, L("load.done", "Listo"));
  }

  if (splash.id != 0)
    UnloadTexture(splash);

  // Inicializar raycaster con la resolución configurada
  int rcIdx = std::clamp(cfg.raycastRes, 0, C::RC_RES_COUNT - 1);
  rc.init(C::RC_RESOLUTIONS[rcIdx][0], C::RC_RESOLUTIONS[rcIdx][1]);
  rc.loadStaticTextures();

  // Aplicar FPS, VSync, resolución de ventana, RTs, etc.
  applyVideoSettings();

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
  p.hidden = false;
  p.hiddenExitPos = spawn;
  p.hiddenExitYaw = 0.0f;
  p.hiddenCellX = -1;
  p.hiddenCellY = -1;
  p.throwCooldown = 0.0f;
  p.attackFlash = 0.0f;
  p.hasKey = false;
}

// =========================================================================
//  Cinemáticas
// =========================================================================
bool Game::hasStory(const char *name) const {
  return !resolveStoryPath(name).empty();
}

void Game::playCinematic(const char *name, const char *nextAction) {
  std::string full = resolveStoryPath(name);
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
        bool hasDetails = !resolveLevelDetailsPath(level).empty();
        bool hasMap = existsAt(Paths::level(mapName));

        bool nextLevelExists = hasIntro || hasOutro || hasDetails || hasMap;
        if (!nextLevelExists) {
          if (sr.active)
            speedrunFinishRun();
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
//  Utilidades HUD
// =========================================================================
float Game::nearestEnemyDist(Vector3 p) const {
  float best = 1e9f;
  for (auto &e : enemies)
    best = std::min(best, Vector3Distance(p, e.pos));
  return best;
}

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

  for (auto &w : worldItems) {
    if (w.kind != ITEM_KEY)
      continue;
    int kx = ox + (int)((w.pos.x / C::CELL) * scale);
    int kz = oy + (int)((w.pos.z / C::CELL) * scale);
    DrawRectangle(kx - 3, kz - 3, 6, 6, GOLD);
    DrawRectangleLines(kx - 3, kz - 3, 6, 6, (Color){120, 80, 20, 255});
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

void Game::drawSpeedrunOverlay(int vw, int vh) {
  if (sr.pbFlashTimer > 0.0f)
    sr.pbFlashTimer -= GetFrameTime();

  const int pad = 12;
  const int boxW = 320;
  const int boxH = 130;
  const int bx = vw - boxW - pad;
  const int by = pad;

  DrawRectangle(bx, by, boxW, boxH, (Color){0, 0, 0, 170});
  DrawRectangleLines(bx, by, boxW, boxH,
                     sr.pbFlashTimer > 0.0f ? GOLD
                                            : (Color){90, 100, 130, 200});

  int tx = bx + 12;
  int ty = by + 8;
  char hdr[64];
  snprintf(hdr, sizeof(hdr), "Nivel %d", level);
  UI::T(hdr, tx, ty, 18, RAYWHITE);
  ty += 24;

  float cur = totalPlaytime - sr.lastLevelStart;
  char curT[32];
  formatSplitTime(cur, curT, sizeof(curT));
  UI::T(curT, tx, ty, 26, RAYWHITE);
  ty += 30;

  int idx = (int)sr.splits.size();
  if (idx < (int)sr.bestSplits.size() && sr.bestSplits[idx] > 0.0f) {
    float delta = cur - sr.bestSplits[idx];
    char dT[32];
    formatDelta(delta, dT, sizeof(dT));
    Color dc = (delta <= 0.0f) ? (Color){100, 240, 100, 255}
                               : (Color){240, 100, 100, 255};
    UI::T(dT, tx, ty, 22, dc);
  } else {
    UI::T("-:--.---", tx, ty, 22, GRAY);
  }
  ty += 26;

  float elapsed = 0.0f;
  for (float s : sr.splits)
    elapsed += s;
  elapsed += cur;

  float remainingBest = 0.0f;
  for (int i = idx + 1; i < (int)sr.bestSplits.size(); ++i)
    remainingBest += sr.bestSplits[i];

  if (elapsed > 0.0f) {
    float predicted = elapsed + remainingBest;
    char pT[32];
    formatSplitTime(predicted, pT, sizeof(pT));
    char line[64];
    snprintf(line, sizeof(line), "Pred: %s", pT);
    UI::T(line, tx, ty, 16, (Color){200, 200, 120, 255});
  }

  if (sr.pbFlashTimer > 0.0f) {
    float pulse = 0.5f + 0.5f * sinf(sr.pbFlashTimer * 12.0f);
    unsigned char a = (unsigned char)(pulse * 255);
    const char *pb = "PB!";
    int fs = 22;
    UI::T(pb, bx + boxW - UI::M(pb, fs) - 12, by + 8, fs,
          (Color){255, 220, 60, a});
  }
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

static const char *rcResLabel(int idx) {
  static char buf[16];
  snprintf(buf, sizeof(buf), "%dx%d", C::RC_RESOLUTIONS[idx][0],
           C::RC_RESOLUTIONS[idx][1]);
  return buf;
}

// =========================================================================
//  Config: hub y submenús
// =========================================================================
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

void Game::drawConfigVideo() {
  drawConfigHeader("VIDEO");
  int items = 8;
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
      cfg.fpsLimit = 2;
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
  {
    UIAction a = uiOptionEx(
        6, items, 30, L("video.raycastres", "Resolución raycaster"),
        rcResLabel(cfg.raycastRes), &navIndex, nullptr, navIndex == 3 ? lr : 0);
    if (a == UI_LEFT)
      cfg.raycastRes = std::max(0, cfg.raycastRes - 1);
    if (a == UI_RIGHT)
      cfg.raycastRes = std::min(C::RC_RES_COUNT - 1, cfg.raycastRes + 1);
    if (a == UI_CONFIRM)
      cfg.raycastRes = C::RC_RES_DEFAULT;
    if (a != UI_NONE) {
      applyVideoSettings();
      save();
    }
  }
  if (uiOptionEx(7, items, 30, L("common.back", "Volver"), "", &navIndex,
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
  int items = 8;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  auto save = [&]() { cfg.save(); };

  if (uiOptionEx(0, items, 40, L("gameplay.inverty", "Invertir eje Y"),
                 yesNo(cfg.invertY), &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.invertY = !cfg.invertY;
    save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.mouseSens);
    UIAction a = uiOptionEx(1, items, 40,
                            L("gameplay.mousesens", "Sensibilidad del ratón"),
                            buf, &navIndex, nullptr, navIndex == 1 ? lr : 0);
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
    UIAction a = uiOptionEx(2, items, 40,
                            L("gameplay.padsens", "Sensibilidad del mando"),
                            buf, &navIndex, nullptr, navIndex == 2 ? lr : 0);
    if (a == UI_LEFT)
      cfg.stickSens = std::max(0.10f, cfg.stickSens - 0.05f);
    if (a == UI_RIGHT)
      cfg.stickSens = std::min(5.00f, cfg.stickSens + 0.05f);
    if (a == UI_CONFIRM)
      cfg.stickSens = 1.0f;
    if (a != UI_NONE)
      save();
  }
  if (uiOptionEx(3, items, 40, L("gameplay.minimap", "Minimapa"),
                 yesNo(cfg.showMinimap), &navIndex, nullptr,
                 navIndex == 3 ? lr : 0) != UI_NONE) {
    cfg.showMinimap = !cfg.showMinimap;
    save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.sprintDrain);
    UIAction a =
        uiOptionEx(4, items, 40, L("gameplay.sprintdrain", "Consumo de sprint"),
                   buf, &navIndex, nullptr, navIndex == 4 ? lr : 0);
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
    UIAction a = uiOptionEx(5, items, 40,
                            L("gameplay.sprintregen", "Regeneración de sprint"),
                            buf, &navIndex, nullptr, navIndex == 5 ? lr : 0);
    if (a == UI_LEFT)
      cfg.sprintRegen = std::max(0.05f, cfg.sprintRegen - 0.05f);
    if (a == UI_RIGHT)
      cfg.sprintRegen = std::min(0.60f, cfg.sprintRegen + 0.05f);
    if (a == UI_CONFIRM)
      cfg.sprintRegen = C::STAM_REGEN_DEF;
    if (a != UI_NONE)
      save();
  }
  if (uiOptionEx(6, items, 40, L("gameplay.cinematics", "Cinemáticas"),
                 yesNo(cfg.playCinematics), &navIndex, nullptr,
                 navIndex == 6 ? lr : 0) != UI_NONE) {
    cfg.playCinematics = !cfg.playCinematics;
    save();
  }
  if (uiOptionEx(7, items, 40, L("common.back", "Volver"), "", &navIndex,
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
      Vector2 md = GetMouseDelta();
      bool mouseMoved = (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f);
      if (hover && mouseMoved)
        saveListCursor = idx;

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
        loadGame(e.id);
        return;
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
    createBuffer.runSeed =
        (uint32_t)time(nullptr) ^ (uint32_t)(rand() * 2654435761u);
    if (createBuffer.runSeed == 0)
      createBuffer.runSeed = 1;
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

  // --- Nombre ---
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

    int k = GetCharPressed();
    while (k > 0) {
      if (selected && k >= 32 && k < 127 && createBuffer.name.size() < 40)
        createBuffer.name += (char)k;
      k = GetCharPressed();
    }
    if (selected && IsKeyPressed(KEY_BACKSPACE) && !createBuffer.name.empty())
      createBuffer.name.pop_back();
    if (selected && fmodf((float)GetTime(), 1.0f) < 0.5f)
      UI::T("_", tx + tw, y + 14, 24, GOLD);
  }

  // --- Modo ---
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

  // --- Dificultad ---
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

  // --- Jugadores ---
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

  // --- Speedrun ---
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

  // --- Sección infinito ---
  if (endless) {
    // Seed
    {
      int x = GetScreenWidth() / 2 - 340;
      int y = rowY(seedRow);
      Rectangle r = {(float)x, (float)y, 680, 52};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      Vector2 md = GetMouseDelta();
      if (hover && (fabsf(md.x) > 0.5f || fabsf(md.y) > 0.5f))
        navIndex = seedRow;
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        navIndex = seedRow;
      bool selected = (navIndex == seedRow);
      Color bg = (hover || selected) ? (Color){55, 62, 90, 255}
                                     : (Color){35, 40, 58, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2,
                           selected ? SKYBLUE : (Color){80, 90, 125, 255});
      UI::T(L("endless.seed", "Seed"), x + 22, y + 14, 24, RAYWHITE);

      char buf[32];
      snprintf(buf, sizeof(buf), "%u", createBuffer.runSeed);
      int vw = UI::M(buf, 24);
      int vx = x + 680 - vw - 22;
      UI::T(buf, vx, y + 14, 24, GOLD);

      int k = GetCharPressed();
      while (k > 0) {
        if (selected && k >= '0' && k <= '9') {
          uint64_t v =
              (uint64_t)createBuffer.runSeed * 10ull + (uint64_t)(k - '0');
          if (v <= 0xFFFFFFFFull)
            createBuffer.runSeed = (uint32_t)v;
        }
        k = GetCharPressed();
      }
      if (selected && IsKeyPressed(KEY_BACKSPACE))
        createBuffer.runSeed /= 10;
      if (selected && fmodf((float)GetTime(), 1.0f) < 0.5f)
        UI::T("_", vx + vw, y + 14, 24, GOLD);
    }

    // Aleatorizar
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
        createBuffer.runSeed = s;
        if (selected)
          confirm = false;
      }
    }

    // Multiplicadores
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

  // --- Botones create/cancel ---
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
    if (createBuffer.mode == MODE_ENDLESS && createBuffer.runSeed == 0) {
      createBuffer.runSeed =
          (uint32_t)time(nullptr) ^ (uint32_t)(rand() * 2654435761u);
      if (createBuffer.runSeed == 0)
        createBuffer.runSeed = 1;
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

  const auto &runs = saves.speedruns();

  if (runs.empty()) {
    const char *line1 = L("speedrun.empty1", "Sin runs registrados todavía.");
    const char *line2 =
        L("speedrun.empty2", "Termina una partida con speedrun activado.");
    int fs = 22;
    UI::T(line1, W / 2 - UI::M(line1, fs) / 2, H / 2 - 20, fs, LIGHTGRAY);
    UI::T(line2, W / 2 - UI::M(line2, fs) / 2, H / 2 + 20, fs, GRAY);
  } else {
    // Claves únicas (seed, mode)
    std::vector<std::pair<uint32_t, int>> keys;
    for (auto &e : runs) {
      bool found = false;
      for (auto &k : keys)
        if (k.first == e.seed && k.second == e.mode) {
          found = true;
          break;
        }
      if (!found)
        keys.push_back({e.seed, e.mode});
    }

    if (navMenu < 0)
      navMenu = 0;
    if (navMenu >= (int)keys.size())
      navMenu = (int)keys.size() - 1;

    int lr = gamepadLeftRight();
    if (IsKeyPressed(KEY_LEFT))
      lr = -1;
    if (IsKeyPressed(KEY_RIGHT))
      lr = +1;
    if (IsKeyPressed(KEY_ESCAPE)) {
      state = ST_MENU;
      navMenu = 0;
    }
    if (lr < 0)
      navMenu = (navMenu - 1 + (int)keys.size()) % (int)keys.size();
    if (lr > 0)
      navMenu = (navMenu + 1) % (int)keys.size();

    uint32_t selSeed = keys[navMenu].first;
    int selMode = keys[navMenu].second;

    char keyBuf[128];
    snprintf(keyBuf, sizeof(keyBuf), "Seed %u  |  %s", selSeed,
             selMode == MODE_STORY ? L("speedrun.mode.story", "Historia")
                                   : L("speedrun.mode.endless", "Infinito"));
    char keyFull[160];
    snprintf(keyFull, sizeof(keyFull), "< %s >", keyBuf);
    UI::T(keyFull, W / 2 - UI::M(keyFull, 22) / 2, 110, 22, GOLD);

    const SpeedrunEntry *best = nullptr;
    const SpeedrunEntry *last = nullptr;
    std::time_t lastRunDate = 0;
    for (auto &e : runs) {
      if (e.seed != selSeed || e.mode != selMode)
        continue;
      if (!best || e.total < best->total)
        best = &e;
      if (e.date > lastRunDate) {
        last = &e;
        lastRunDate = e.date;
      }
    }

    std::vector<float> bestSplits;
    saves.bestSplitsFor(selSeed, selMode, bestSplits);

    int rowY = 170;
    int rowH = 26;
    int colSplitX = W / 2 - 220;
    int colBestX = W / 2;
    int colLastX = W / 2 + 180;

    UI::T(L("speedrun.split", "Split"), colSplitX, rowY, 18, GRAY);
    UI::T(L("speedrun.best", "Mejor"), colBestX, rowY, 18, GRAY);
    UI::T(L("speedrun.last", "Última"), colLastX, rowY, 18, GRAY);
    rowY += rowH;

    int numRows = (int)bestSplits.size();
    if (last && (int)last->splits.size() > numRows)
      numRows = (int)last->splits.size();

    for (int i = 0; i < numRows && rowY < H - 140; ++i) {
      char name[32];
      snprintf(name, sizeof(name), "Nivel %d", i + 1);
      UI::T(name, colSplitX, rowY, 16, RAYWHITE);

      char tbuf[32];
      if (i < (int)bestSplits.size() && bestSplits[i] > 0.0f) {
        formatSplitTime(bestSplits[i], tbuf, sizeof(tbuf));
        UI::T(tbuf, colBestX, rowY, 16, GOLD);
      } else {
        UI::T("--:--.---", colBestX, rowY, 16, GRAY);
      }
      if (last && i < (int)last->splits.size()) {
        formatSplitTime(last->splits[i], tbuf, sizeof(tbuf));
        Color c = RAYWHITE;
        if (i < (int)bestSplits.size() && bestSplits[i] > 0.0f) {
          c = (last->splits[i] <= bestSplits[i] + 0.001f)
                  ? (Color){100, 240, 100, 255}
                  : (Color){220, 180, 100, 255};
        }
        UI::T(tbuf, colLastX, rowY, 16, c);
      }
      rowY += rowH;
    }

    rowY += 10;
    if (best) {
      char tb[32];
      formatSplitTime(best->total, tb, sizeof(tb));
      char line[64];
      snprintf(line, sizeof(line), "%s: %s",
               L("speedrun.besttotal", "Mejor total"), tb);
      UI::T(line, colSplitX, rowY, 18, GOLD);
      rowY += 24;
    }
    if (last) {
      char tb[32];
      formatSplitTime(last->total, tb, sizeof(tb));
      char line[104];
      snprintf(line, sizeof(line), "%s: %s   (%s)",
               L("speedrun.lasttotal", "Último total"), tb,
               formatDate(last->date));
      UI::T(line, colSplitX, rowY, 18, RAYWHITE);
    }
  }

  const char *hint =
      L("speedrun.hint", "Izq/Der cambia de categoría. ESC para volver.");
  UI::T(hint, W / 2 - UI::M(hint, 18) / 2, H - 40, 18, GRAY);
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
void Game::handleDebugHotkeys() {
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
    for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
      if (players[i].caught || players[i].escaped)
        continue;
      DrawCircle3D({players[i].pos.x, 0.06f, players[i].pos.z}, C::CATCH_R,
                   {1, 0, 0}, 90.0f, (Color){255, 0, 0, 80});
    }
  }

  if (Debug::showVision) {
    for (auto &e : enemies) {
      Vector3 eye = {e.pos.x, 1.4f, e.pos.z};
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
        Vector3 tgt = {players[target].pos.x, 1.4f, players[target].pos.z};
        DrawLine3D(eye, tgt, (Color){0, 255, 0, 200});
      } else if (e.memoryT > 0.0f) {
        Vector3 mem = {e.lastSeen.x, 1.4f, e.lastSeen.z};
        DrawLine3D(eye, mem, (Color){255, 220, 0, 180});
      } else {
        Vector3 fwd = {eye.x + e.dir.x * 3.0f, eye.y, eye.z + e.dir.y * 3.0f};
        DrawLine3D(eye, fwd, (Color){200, 200, 200, 150});
      }
    }
  }

  if (Debug::showPaths) {
    for (auto &e : enemies) {
      if (e.memoryT <= 0.0f)
        continue;
      DrawLine3D({e.pos.x, 0.2f, e.pos.z}, {e.lastSeen.x, 0.2f, e.lastSeen.z},
                 (Color){255, 100, 255, 180});
    }
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
  (void)vw;
  (void)vh;
  if (!Debug::showOverlay)
    return;

  const int pad = 10;
  const int lineH = 16;
  const int font = 14;
  const int width = 460;

  int lines = 0;
  lines += 3;
  lines += 3;
  lines += 6;
  if (twoPlayers)
    lines += 6;
  lines += 4;
  lines += 2 + std::min<int>((int)enemies.size(), 6);
  if (enemies.size() > 6)
    lines += 1;
  lines += 1 + 8;

  int height = pad * 2 + lines * lineH;

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
  const Color offC = (Color){130, 130, 130, 255};
  const Color warnC = (Color){255, 200, 80, 255};

  char buf[256];

  line(titleC, TextFormat("%s  %s", C::GAME_TITLE, Debug::version()));
  snprintf(buf, sizeof(buf), "Build: %s  |  %s", Debug::buildString(),
           Debug::compilerName());
  line(labelC, buf);
  line(labelC, "F3: toggle overlay   |   Numpad: cheats (ver abajo)");

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
  line(valC, "[F10] Reveal   [F11] Teleport   [NUM.] TimeScale");
  line(valC, "[NUM0] Spawn   [NUM1] Kill all   [NUM2] Restart");
  line(valC, "[NUM3] Next lvl   [NUM4] Prev lvl");
}

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
int Game::findInteractable(const Player &pl, int &outCx, int &outCy,
                           bool &isLocker) const {
  Vector3 fwd = {sinf(pl.yaw), 0.0f, -cosf(pl.yaw)};
  Vector3 probe = {pl.pos.x + fwd.x * C::INTERACT_RANGE, 0.0f,
                   pl.pos.z + fwd.z * C::INTERACT_RANGE};
  int cx = (int)floorf(probe.x / C::CELL);
  int cy = (int)floorf(probe.z / C::CELL);
  outCx = cx;
  outCy = cy;

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

  int idx = Inventory::nearestPickup(pl.pos, worldItems, C::PICKUP_RANGE);
  if (idx >= 0) {
    ItemKind kind = worldItems[idx].kind;

    if (kind == ITEM_KEY) {
      worldItems.erase(worldItems.begin() + idx);
      players[0].hasKey = true;
      players[1].hasKey = true;

      if (assets.sfxPickup.frameCount > 0) {
        SetSoundPitch(assets.sfxPickup, 1.4f);
        PlaySound(assets.sfxPickup);
        SetSoundPitch(assets.sfxPickup, 1.0f);
      }
      if (p == 0)
        showToast(L("toast.key.acquired", "¡Llave conseguida!"));
      return;
    }

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

  int cx, cy;
  bool isLocker;
  int what = findInteractable(pl, cx, cy, isLocker);
  if (what == 0)
    return;

  if (what == 1) {
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

  if (IsKeyPressed(cfg.binds[p].drop)) {
    if (Inventory::dropSelected(pl, maze, worldItems)) {
      if (assets.sfxPickup.frameCount > 0) {
        SetSoundPitch(assets.sfxPickup, 0.7f);
        PlaySound(assets.sfxPickup);
        SetSoundPitch(assets.sfxPickup, 1.0f);
      }
      if (p == 0)
        showToast(L("toast.item.dropped", "Item dropeado"));
    }
  }

  bool usePressed = (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
                    IsKeyPressed(KEY_E);
  if (usePressed) {
    if (Inventory::useSelected(pl, maze, projectiles, assets)) {
      // sonido se maneja dentro
    }
  }
}

// =========================================================================
//  HUD items
// =========================================================================
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
  float dm = diffMultFor(currentDifficulty);
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
    wp.pos = {p.x, 0.35f, p.z};
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
    wp.pos = {p.x, 0.55f, p.z};
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

  if (usedCustom)
    exitPos = extras.exit;
  else
    exitPos = maze.cellCenter(maze.w - 2, maze.h - 2);

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

    float dm = diffMultFor(currentDifficulty);
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

  if (sr.active)
    sr.lastLevelStart = totalPlaytime;

  maze.markExplored(players[0].pos.x, players[0].pos.z, C::FOG_RADIUS);
  if (twoPlayers)
    maze.markExplored(players[1].pos.x, players[1].pos.z, C::FOG_RADIUS);
}

// =========================================================================
//  Level details / intro
// =========================================================================
bool Game::loadLevelDetails(int lvl) {
  levelDetails = {};

  std::string path = resolveLevelDetailsPath(lvl);
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
      drawDifficultyStar(cx, sy, starR, (Color){255, 200, 70, a}, true);
    else
      drawDifficultyStar(cx, sy, starR, (Color){70, 70, 85, a}, false);
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
  saves.addSpeedrun(e);

  sr.active = false;
}

void Game::drawHandItem2D(const Player &pl, int vw, int vh) {
  if (pl.caught || pl.escaped || pl.hidden)
    return;
  int slot = pl.selectedSlot;
  if (slot < 0 || slot >= C::INV_SLOTS)
    return;

  const InvItem &it = pl.inventory[slot];
  if (it.kind == ITEM_NONE || it.count <= 0)
    return;

  int k = (int)it.kind;
  if (k <= ITEM_NONE || k >= ITEM_KIND_COUNT)
    return;
  if (!assets.hasItemTex[k])
    return;

  // Bob al caminar
  float bob = sinf((float)GetTime() * 4.0f) * 6.0f;
  if (pl.footstepT > 0.0f)
    bob += sinf((C::STEP_SLOW - pl.footstepT) * 14.0f) * 12.0f;

  float baseSize = vw < 900 ? 90.0f : 120.0f;

  float size = baseSize;
  switch (it.kind) {
  case ITEM_ROCK:
    size = baseSize * 0.6f;
    break;
  case ITEM_BOX:
    size = baseSize * 1.0f;
    break;
  case ITEM_LIME:
    size = baseSize * 1.1f;
    break;
  case ITEM_BICIMAD:
    size = baseSize * 1.3f;
    break;
  case ITEM_KEY:
    size = baseSize * 0.9f;
    break;
  default:
    size = baseSize;
    break;
  }

  float x = vw * 0.80f;
  float y = vh - size * 0.55f + bob;

  Texture2D tex = assets.itemTex[k];
  DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height},
                 {x, y - 200, size, size}, {0, 0}, -10.0f, WHITE);
}

void Game::renderPlayerView(const Player &pl) {
  bool levelHasKey = false;
  for (auto &w : worldItems)
    if (w.kind == ITEM_KEY) {
      levelHasKey = true;
      break;
    }
  bool locked = levelHasKey && !(players[0].hasKey || players[1].hasKey);

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
      s.pos = {e.pos.x, 0.0f, e.pos.z};
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
      s.pos = {w.pos.x, w.pos.y + bob, w.pos.z};
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
        s.pos = {center.x, 0.0f, center.z};
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
        s.pos = {center.x, 0.0f, center.z};
        s.size = {1.5f, 1.5f};
        s.tex = assets.fountainTex;
        s.tint = {255, 255, 255, 255};
        s.crossedQuads = true;
        sprites.push_back(s);
      }
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
    sPortal.pos = {exitPos.x, 0.0f, exitPos.z};
    sPortal.size = {width, height};
    sPortal.tex = portalTex;
    sPortal.tint = portalTint;
    sprites.push_back(sPortal);

    // Renderizar con Camera3D
    Vector3 eye = {pl.pos.x, pl.pos.y + C::EYE_H, pl.pos.z};
    r3d.render(maze, eye, pl.yaw, pl.pitch, (float)cfg.fov, renderDist(),
               sprites, true);

    if (cfg.useRenderer3D) {
      static int logTick = 0;
      if (++logTick % 60 == 0) {
        TraceLog(LOG_INFO, "[R3D-DIAG] drawn=%d calls=%d visSize=%zu",
                 r3d.lastDrawnChunks(), r3d.lastDrawCalls(),
                 r3d.visibility().size());
      }
    }
  } else {
    // Fallback al raycaster clásico (intacto)
    rc.render(pl, maze, enemies, projectiles, worldItems, exitPos, locked,
              renderDist(), (float)cfg.fov);
  }
}
