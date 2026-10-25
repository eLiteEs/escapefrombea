#include "menu_speedrun.hpp"

#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"
#include "util_format.hpp"
#include <algorithm>

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
