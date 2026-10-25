#include "gameplay_render.hpp"

#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"

#include <algorithm>

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
