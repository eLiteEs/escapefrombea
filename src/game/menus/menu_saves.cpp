#include "menu_saves.hpp"

#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"
#include "util_format.hpp"

#include <cstring>

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

      const char *dateStr = Util::formatDate(e.timestamp);
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
