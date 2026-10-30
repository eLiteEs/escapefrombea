#include "gameplay_hud.hpp"

#include "common.hpp"
#include "game.hpp"
#include "inventory.hpp"
#include "lang.hpp"
#include "ui.hpp"
#include "util_format.hpp"

#include <raylib.h>
#include <raymath.h>
#include <algorithm>

void Game::drawStaminaBar(const Player &pl, int vw, int vh) {
  int barW = std::max(120, vw / 3);
  int barH = 14;
  int bx = vw / 2 - barW / 2;
  int by = vh - 60;

  DrawRectangle(bx - 2, by - 2, barW + 4, barH + 4, (Color){0, 0, 0, 160});
  DrawRectangle(bx, by, barW, barH, (Color){40, 40, 55, 255});

  float maxStam = C::STAM_WATER_MAX;
  float clamped = std::min(pl.stamina, maxStam);
  float clampedBase = std::min(clamped, 1.0f);        // 0..1
  float clampedOver = std::max(0.0f, clamped - 1.0f); // 0..1

  int halfW = barW / 2;

  int baseW = (int)(halfW * clampedBase);
  int overW = (int)(halfW * clampedOver);

  Color cBase;
  if (pl.exhausted)
    cBase = RED;
  else if (pl.stamina < 0.3f)
    cBase = ORANGE;
  else
    cBase = GREEN;

  Color cOver = (Color){80, 180, 255, 255}; // azul

  if (baseW > 0)
    DrawRectangle(bx, by, baseW * 2, barH, cBase);

  if (overW > 0)
    DrawRectangle(bx, by, overW * 2, barH, cOver);

  DrawRectangleLines(bx, by, barW, barH, RAYWHITE);

  const char *label = pl.exhausted ? L("hud.stamina.exhausted", "Sin aliento")
  : L("hud.stamina", "Energía");
  int lw = UI::M(label, 14);
  Color lc = pl.exhausted ? RED : (pl.stamina > 1.0f ? cOver : LIGHTGRAY);
  UI::T(label, vw / 2 - lw / 2, by - 18, 14, lc);
}

void Game::drawHealthAndShieldBars(const Player &pl, int vw, int vh) {
  // --- Layout escalado con la UI ---
  const int iconSize   = UI::S(20);
  const int iconX      = UI::S(10);
  const int healthBarY = vh - UI::S(50);
  const int shieldBarY = vh - UI::S(25);
  const int barH       = UI::S(20);
  const int barX       = UI::S(10);
  const int barW       = std::max(UI::S(90), vw / 4);

  // --- Valores clamp (ver nota abajo: shield usa pl.shield, no pl.hp) ---
  float healthClamped = std::min(pl.hp,     C::PLAYER_MAX_HP);
  float shieldClamped = std::min(pl.shield, C::PLAYER_MAX_SHIELD);

  float healthFrac = (C::PLAYER_MAX_HP > 0.0f)
  ? healthClamped / C::PLAYER_MAX_HP : 0.0f;
  float shieldFrac = (C::PLAYER_MAX_SHIELD > 0.0f)
  ? shieldClamped / C::PLAYER_MAX_SHIELD : 0.0f;

  Color healthColor = (healthFrac < 0.3f) ? ORANGE : GREEN;
  Color shieldColor = {80, 180, 255, 255};

  // --- Barra de vida ---
  DrawRectangle(barX - 2, healthBarY - 2, barW + 4, barH + 4,
                (Color){0, 0, 0, 160});
  DrawRectangle(barX, healthBarY, barW, barH, (Color){40, 40, 55, 255});
  DrawRectangle(barX, healthBarY, (int)(barW * healthFrac), barH, healthColor);
  DrawRectangleLines(barX, healthBarY, barW, barH, RAYWHITE);

  // --- Barra de escudo ---
  DrawRectangle(barX - 2, shieldBarY - 2, barW + 4, barH + 4,
                (Color){0, 0, 0, 160});
  DrawRectangle(barX, shieldBarY, barW, barH, (Color){40, 40, 55, 255});
  DrawRectangle(barX, shieldBarY, (int)(barW * shieldFrac), barH, shieldColor);
  DrawRectangleLines(barX, shieldBarY, barW, barH, RAYWHITE);

  // --- Iconos ---
  if (UI::hasHealthTex && UI::healthTex.id != 0) {
    DrawTexturePro(
      UI::healthTex,
      {0, 0, (float)UI::healthTex.width, (float)UI::healthTex.height},
                   {(float)iconX, (float)healthBarY, (float)iconSize, (float)iconSize},
                   {0, 0}, 0.0f, WHITE);
  }
  if (UI::hasArmorTex && UI::armorTex.id != 0) {
    DrawTexturePro(
      UI::armorTex,
      {0, 0, (float)UI::armorTex.width, (float)UI::armorTex.height},
                   {(float)iconX, (float)shieldBarY, (float)iconSize, (float)iconSize},
                   {0, 0}, 0.0f, WHITE);
  }
}

float Game::nearestEnemyDist(Vector3 p) const {
  float best = 1e9f;
  for (auto &e : enemies)
    best = std::min(best, Vector3Distance(p, e.pos));
  return best;
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
  case ITEM_BIFRUTAS:
    size = baseSize * 0.7f;
    break;
  case ITEM_ENSAIMADA:
    size = baseSize * 0.8f;
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

void Game::drawCooldownBar(const Player& pl, int vw, int vh) {
  if (pl.throwCooldown <= 0.0f)    return;
  if (pl.throwCooldownMax <= 0.0f) return;

  float frac = std::clamp(pl.throwCooldown / pl.throwCooldownMax, 0.0f, 1.0f);

  int barW = std::max(6, vw / 260);
  int barH = std::max(48, vh / 12);
  int gap  = std::max(18, vw / 90);
  int bx = vw / 2 + gap;
  int by = vh / 2 - barH / 2;

  DrawRectangle(bx - 2, by - 2, barW + 4, barH + 4, (Color){ 0, 0, 0, 160 });
  DrawRectangle(bx, by, barW, barH, (Color){ 40, 40, 55, 200 });

  int fillH = (int)(barH * frac);
  int fillY = by + (barH - fillH);

  Color c;
  if (frac > 0.5f)       c = (Color){ 210,  60,  60, 255 };
  else if (frac > 0.2f)  c = (Color){ 240, 150,  50, 255 };
  else                   c = (Color){ 240, 220,  60, 255 };

  if (fillH > 0)
    DrawRectangle(bx, fillY, barW, fillH, c);

  DrawRectangleLines(bx, by, barW, barH, (Color){ 220, 220, 220, 180 });
  DrawRectangle(bx - 1, by + barH / 2, barW + 2, 1,
                (Color){ 220, 220, 220, 80 });
}

void Game::drawSpeedrunOverlay(int vw, int vh) {
  if (!sr.active)
    return;
  if (sr.pbFlashTimer > 0.0f)
    sr.pbFlashTimer -= GetFrameTime();

  // --- Tamaño de la caja ---
  int boxW, boxH;
  switch (cfg.srOverlaySize) {
  case 0:
    boxW = 220;
    boxH = 86;
    break; // Pequeño
  case 1:
    boxW = 320;
    boxH = 150;
    break; // Medio
  default:
    boxW = 420;
    boxH = 280;
    break; // Grande
  }

  // --- Posición según anchor (3x3) + offsets ---
  int col = cfg.srOverlayAnchor % 3;
  int row = cfg.srOverlayAnchor / 3;
  const int pad = 12;
  int px = (col == 0) ? pad : (col == 1) ? (vw - boxW) / 2 : (vw - boxW - pad);
  int py = (row == 0) ? pad : (row == 1) ? (vh - boxH) / 2 : (vh - boxH - pad);
  px += cfg.srOverlayOffsetX;
  py += cfg.srOverlayOffsetY;
  if (px < 0)
    px = 0;
  if (py < 0)
    py = 0;
  if (px + boxW > vw)
    px = vw - boxW;
  if (py + boxH > vh)
    py = vh - boxH;

  // --- Fondo ---
  DrawRectangle(px, py, boxW, boxH, (Color){12, 12, 18, 210});
  DrawRectangleLines(px, py, boxW, boxH, (Color){90, 100, 130, 200});

  // --- Datos de la run ---
  int idx = (int)sr.splits.size(); // índice del nivel actual
  float cur = totalPlaytime - sr.lastLevelStart;
  float curLevelPB =
      (idx < (int)sr.bestSplits.size()) ? sr.bestSplits[idx] : 0.0f;

  float totalCompleted = 0.0f;
  for (float s : sr.splits)
    totalCompleted += s;
  float total = totalCompleted + cur;

  float bpt = 0.0f; // Best Possible Time: suma de todos los mejores splits
  for (float s : sr.bestSplits)
    bpt += s;

  float remainingBest = 0.0f;
  for (int i = idx + 1; i < (int)sr.bestSplits.size(); ++i)
    remainingBest += sr.bestSplits[i];
  float predicted = total + remainingBest;

  // --- Colores ---
  const Color colGood = {100, 240, 100, 255};
  const Color colBad = {240, 100, 100, 255};
  const Color colGold = {255, 220, 60, 255};
  const Color colGray = {130, 130, 140, 255};
  const Color colPred = {200, 200, 120, 255};
  const Color colCur = {255, 255, 255, 255};

  const int padIn = 10;
  int tx = px + padIn;
  int ty = py + 8;

  // ==========================================================
  // Header: "Nv X" + "PB XX.XXX"
  // ==========================================================
  {
    char buf[32];
    snprintf(buf, sizeof(buf), "Nv %d", level);
    UI::T(buf, tx, ty, 16, colCur);

    if (curLevelPB > 0.0f) {
      char pbbuf[32];
      Util::formatSplitTime(curLevelPB, pbbuf, sizeof(pbbuf));
      char full[48];
      snprintf(full, sizeof(full), "PB %s", pbbuf);
      int rw = UI::M(full, 11);
      UI::T(full, px + boxW - rw - padIn, ty + 3, 11, colGray);
    }
    ty += 22;
  }

  // ==========================================================
  // Tiempo actual (grande, dorado)
  // ==========================================================
  {
    char buf[32];
    Util::formatSplitTime(cur, buf, sizeof(buf));
    int fs = (cfg.srOverlaySize == 0) ? 22 : 28;
    UI::T(buf, tx, ty, fs, colGold);
    ty += fs + 4;
  }

  // ==========================================================
  // Delta vs PB del nivel actual
  // ==========================================================
  if (cfg.srShowDelta) {
    if (curLevelPB > 0.0f) {
      float delta = cur - curLevelPB;
      char dbuf[32];
      Util::formatDelta(delta, dbuf, sizeof(dbuf));
      Color dcol = (delta <= 0.0f) ? colGood : colBad;
      UI::T(dbuf, tx, ty, 16, dcol);
    } else {
      UI::T("-:--.---", tx, ty, 16, colGray);
    }
    ty += 20;
  }

  // ==========================================================
  // Separador (solo medium y large)
  // ==========================================================
  if (cfg.srOverlaySize >= 1) {
    int sepY = ty + 4;
    DrawRectangle(px + padIn, sepY, boxW - padIn * 2, 1,
                  (Color){80, 90, 120, 160});
    ty = sepY + 8;
  }

  // ==========================================================
  // Splits anteriores + actual (solo large)
  // ==========================================================
  if (cfg.srOverlaySize >= 2 && cfg.srPrevSplitsCount > 0) {
    // Cuántas filas caben visualmente (máximo 5, aunque config permita 10)
    int maxRows = std::min(cfg.srPrevSplitsCount, 5);
    int start = std::max(0, (int)sr.splits.size() - maxRows);

    for (int i = start; i < (int)sr.splits.size(); ++i) {
      char line[64], tbuf[16], dbuf[16];
      Util::formatSplitTime(sr.splits[i], tbuf, sizeof(tbuf));

      bool hasDelta =
          (i < (int)sr.bestSplits.size() && sr.bestSplits[i] > 0.0f);
      Color c = colGray;
      if (hasDelta) {
        float delta = sr.splits[i] - sr.bestSplits[i];
        Util::formatDelta(delta, dbuf, sizeof(dbuf));
        c = (delta <= 0.0f) ? colGood : colBad;
      } else {
        snprintf(dbuf, sizeof(dbuf), "--");
      }

      snprintf(line, sizeof(line), "Nv%-2d  %-9s  %s", i + 1, tbuf, dbuf);
      UI::T(line, tx, ty, 12, c);
      ty += 15;
    }

    // Fila del nivel actual (resaltada en dorado si no hay PB de nivel)
    {
      char line[64], tbuf[16], dbuf[16];
      Util::formatSplitTime(cur, tbuf, sizeof(tbuf));

      Color c = colGold;
      if (curLevelPB > 0.0f) {
        float delta = cur - curLevelPB;
        Util::formatDelta(delta, dbuf, sizeof(dbuf));
        c = (delta <= 0.0f) ? colGood : colBad;
        snprintf(line, sizeof(line), "Nv%-2d  %-9s  %s", idx + 1, tbuf, dbuf);
      } else {
        snprintf(line, sizeof(line), "Nv%-2d  %-9s  --", idx + 1, tbuf);
      }
      UI::T(line, tx, ty, 12, c);
      ty += 15;
    }

    // Separador antes del footer
    int sepY = ty + 2;
    DrawRectangle(px + padIn, sepY, boxW - padIn * 2, 1,
                  (Color){80, 90, 120, 160});
    ty = sepY + 6;
  }

  // ==========================================================
  // Footer: Total / BPT / Pred
  // ==========================================================
  if (cfg.srShowTotal) {
    char tbuf[32], line[64];
    Util::formatSplitTime(total, tbuf, sizeof(tbuf));
    snprintf(line, sizeof(line), "Total: %s", tbuf);
    UI::T(line, tx, ty, 12, colGold);
    ty += 16;
  }

  if (bpt > 0.0f) {
    char tbuf[32], line[64];
    Util::formatSplitTime(bpt, tbuf, sizeof(tbuf));
    snprintf(line, sizeof(line), "BPT:   %s", tbuf);
    UI::T(line, tx, ty, 12, colGray);
    ty += 16;
  }

  if (cfg.srShowPredicted && remainingBest > 0.0f) {
    char tbuf[32], line[64];
    Util::formatSplitTime(predicted, tbuf, sizeof(tbuf));
    snprintf(line, sizeof(line), "Pred:  %s", tbuf);
    UI::T(line, tx, ty, 12, colPred);
    ty += 16;
  }

  // ==========================================================
  // Flash "PB!"
  // ==========================================================
  if (cfg.srShowPbFlash && sr.pbFlashTimer > 0.0f) {
    float pulse = 0.5f + 0.5f * sinf(sr.pbFlashTimer * 12.0f);
    unsigned char a = (unsigned char)(pulse * 255);
    const char *pb = "PB!";
    int fs = 20;
    int pw = UI::M(pb, fs);
    UI::T(pb, px + boxW - pw - padIn, py + 8, fs, (Color){255, 220, 60, a});
  }
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

void Game::showToast(const char *msg) {
  snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
  toastTimer = 1.8f;
}
