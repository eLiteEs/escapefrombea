#include "flow_levels.hpp"

#include "common.hpp"
#include "game.hpp"
#include "lang.hpp"
#include "paths.hpp"
#include "ui.hpp"
#include "util_format.hpp"

#include <algorithm>
#include <raymath.h>

enum ItemCategory { CAT_WEAPONS, CAT_TOOLS, CAT_CONSUMABLES };

static ItemCategory rollItemCategory(std::mt19937 &rng) {
  // 50% armas, 15% herramientas, 35% consumibles
  int r = (int)(rng() % 100);
  if (r < 50)
    return CAT_WEAPONS;
  if (r < 65)
    return CAT_TOOLS;
  return CAT_CONSUMABLES;
}

static ItemKind rollItemKind(ItemCategory cat, std::mt19937 &rng) {
  switch (cat) {
  case CAT_WEAPONS: {
    // Rock 40, Box 25, Lime 20, Bicimad 15
    int r = (int)(rng() % 100);
    if (r < 40)
      return ITEM_ROCK;
    if (r < 65)
      return ITEM_BOX;
    if (r < 85)
      return ITEM_LIME;
    return ITEM_BICIMAD;
  }
  case CAT_TOOLS:
    return ITEM_WALL; // única herramienta actual
  case CAT_CONSUMABLES: {
    // Monster 30, Bifruta 35, Ensaimada 35
    int r = (int)(rng() % 100);
    if (r < 30)
      return ITEM_MONSTER;
    if (r < 65)
      return ITEM_BIFRUTAS;
    return ITEM_ENSAIMADA;
  }
  }
  return ITEM_ROCK;
}

static ItemKind rollItemAnyCategory(std::mt19937 &rng) {
  return rollItemKind(rollItemCategory(rng), rng);
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
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      p.inventory[i].count = p.inventory[i].count / 2;
      if (p.inventory[i].count < 1) {
        p.inventory[i].count = 0;
        p.inventory[i].kind = ITEM_NONE;
      }
    }
  }

  // Posición / orientación
  p.pos = spawn;
  p.pos.y = 0.0f; // asegurar que aterriza en el suelo
  p.yaw = 0.0f;
  p.pitch = 0.0f;

  p.hp = p.maxHp;
  p.shield = p.maxShield;
  p.damageFlashT = 0.0f;

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

void Game::spawnItemsFromPositions(const std::vector<Vector3> &positions,
                                   std::mt19937 &rng) {
  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;
    WorldPickup wp;
    wp.kind = rollItemAnyCategory(rng);
    wp.pos = {p.x, p.y + 0.35f, p.z};
    wp.bobT = (float)(rng() % 100) / 100.0f * 6.28f;
    worldItems.push_back(wp);
  }
}

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
    loadLevelTranslations(lvl);
    currentNpcs = extras.npcs;
  } else {
    exitPos = maze.cellCenter(maze.w - 2, maze.h - 2);
    exitPos.y = maze.groundZAtWorld(exitPos.x, exitPos.z);

    Lang::unloadExtra();
    currentTranslationsLevel = -1;

    currentSigns.clear();
    currentNpcs.clear();
  }

  auto faceExit = [&](Player& p) {
    float dx = exitPos.x - p.pos.x;
    float dz = exitPos.z - p.pos.z;
    if (fabsf(dx) > 0.01f || fabsf(dz) > 0.01f) {
      p.yaw = atan2f(dx, -dz);
      p.pitch = 0.0f;
    }
  };
  faceExit(players[0]);
  if (twoPlayers)
    faceExit(players[1]);

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
      e.dir = {0, 0};
      e.speed = spd * (0.9f + 0.2f * (float)(rng() % 100) / 100.0f);
      e.turnT = 0.0f;
      e.prevPos = p;
      e.pos = p;
      e.hp = e.maxHp;
      e.shield = e.maxShield;
      e.alive = true;

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
    wp.kind = rollItemAnyCategory(rng);
    wp.pos = {p.x, 0.35f, p.z};
    wp.bobT = (float)(rng() % 100) / 100.0f * 6.28f;
    worldItems.push_back(wp);
  }
}

void Game::updateWorldItems(float dt) {
  for (auto &w : worldItems)
    w.bobT += dt * 2.0f;
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
