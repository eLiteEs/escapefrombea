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

    // ---- 1) Rueda del ratón ----
    bool wheelUsed = false;
    if (!saveDeleteConfirm) {
      float wheel = GetMouseWheelMove();
      if (wheel != 0.0f) {
        int delta = (wheel > 0.0f) - (wheel < 0.0f);
        saveListScroll -= delta;
        wheelUsed = true;
      }
    }

    // ---- 2) Navegación con teclado ----
    if (!saveDeleteConfirm) {
      if (IsKeyPressed(KEY_DOWN)) saveListCursor++;
      if (IsKeyPressed(KEY_UP))   saveListCursor--;
    }
    if (saveListCursor < 0) saveListCursor = 0;
    if (saveListCursor >= (int)list.size())
      saveListCursor = (int)list.size() - 1;

    // ---- 3) Clamp del scroll ----
    int maxScroll = std::max(0, (int)list.size() - visRows);
    if (saveListScroll < 0) saveListScroll = 0;
    if (saveListScroll > maxScroll) saveListScroll = maxScroll;

    // ---- 3b) Si se usó la rueda, mantener el cursor dentro del viewport ----
    // (así el siguiente Down/Up parte desde una fila visible, sin saltos raros)
    if (wheelUsed) {
      int lastVisible = std::min(saveListScroll + visRows - 1,
                                 (int)list.size() - 1);
      if (saveListCursor < saveListScroll)
        saveListCursor = saveListScroll;
      else if (saveListCursor > lastVisible)
        saveListCursor = lastVisible;
    }

    // ---- 4) Detección de click (independiente del draw) ----
    int clickedRow = -1;
    if (!saveDeleteConfirm && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mp = GetMousePosition();
      for (int r = 0; r < visRows; ++r) {
        int idx = saveListScroll + r;
        if (idx >= (int)list.size())
          break;
        int ry = topY + r * rowH;
        Rectangle row = {(float)listX, (float)ry, (float)listW,
                         (float)(rowH - 6)};
        if (CheckCollisionPointRec(mp, row)) {
          clickedRow = idx;
          saveListCursor = idx;
          break;
        }
      }
    }

    // ---- 5) Auto-scroll SOLO si NO se usó la rueda ----
    if (!wheelUsed) {
      if (saveListCursor < saveListScroll)
        saveListScroll = saveListCursor;
      else if (saveListCursor >= saveListScroll + visRows)
        saveListScroll = saveListCursor - visRows + 1;
      if (saveListScroll < 0) saveListScroll = 0;
      if (saveListScroll > maxScroll) saveListScroll = maxScroll;
    }

    // ---- 6) Dibujar filas (SIN detectar clicks aquí) ----
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

      // Dibujo de la fila con la nueva paleta
      if (selected)
        DrawRectangleRec(row, UI::BG_FOCUS);
      else if (hover)
        DrawRectangleRec(row, UI::BG_HOVER);
      else
        DrawRectangleRec(row, UI::BG_PANEL);

      if (selected)
        DrawRectangle(listX + 2, y + 8, 3, rowH - 22, UI::ACCENT);

      // Texto
      UI::T(e.name.c_str(), listX + 20, y + 12, 22,
            selected ? UI::TEXT_PRI : UI::TEXT_SEC);

      char info[256];
      const char *modeStr = (e.mode == MODE_STORY)
                                ? L("mode.story", "Historia")
                                : L("mode.endless", "Infinito");
      snprintf(info, sizeof(info), "%s · Nivel %d · %s", modeStr, e.level,
               e.speedrun ? L("saves.tag.sr", "Speedrun") : "");
      UI::T(info, listX + 20, y + 34, 14, UI::TEXT_MUT);

      int totalSec = (int)e.playtime;
      int hh = totalSec / 3600;
      int mm = (totalSec / 60) % 60;
      char pt[32];
      if (hh > 0)
        snprintf(pt, sizeof(pt), "%dh %02dm", hh, mm);
      else
        snprintf(pt, sizeof(pt), "%dm", mm);
      int ptW = UI::M(pt, 16);
      UI::T(pt, listX + listW - ptW - 20, y + 12, 16,
            selected ? UI::TEXT_PRI : UI::TEXT_SEC);

      const char *dateStr = Util::formatDate(e.timestamp);
      int dtW = UI::M(dateStr, 14);
      UI::T(dateStr, listX + listW - dtW - 20, y + 32, 14, UI::TEXT_MUT);


    }

    if (clickedRow >= 0) {
      double now = GetTime();
      bool isDouble = (clickedRow == saveListLastClickIdx &&
      (now - saveListLastClickTime) < 0.4);
      if (isDouble) {
        loadGame(list[clickedRow].id);
        return;
      }
      saveListLastClickIdx = clickedRow;
      saveListLastClickTime = now;
    }

    // ---- 8) Scrollbar visual ----
    UI::drawScrollbar(listX + listW + 8, topY, botY,
                      (int)list.size() * rowH, botY - topY,
                      -saveListScroll * rowH);
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

    DrawRectangleRec(r, hover ? UI::BG_HOVER : UI::BG_PANEL);
    if (hover)
      DrawRectangle(bx, by + bh - 2, bw, 2, UI::ACCENT);

    int fs = 18;
    int tw = UI::M(label, fs);
    UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs,
          hover ? UI::TEXT_PRI : UI::TEXT_SEC);
    return click;
  };

  bool hasSelection = !list.empty();
  bool createClicked = bottomBtn(0, 6, L("saves.new", "+ Nueva partida"));
  bool playClicked = hasSelection && bottomBtn(1, 6, L("saves.play", "Jugar"));
  bool dupClicked = hasSelection && bottomBtn(2, 6, L("saves.dup", "Duplicar"));
  bool recClicked = hasSelection && bottomBtn(3, 6, L("saves.rec", "Recrear"));
  bool delClicked = hasSelection && bottomBtn(4, 6, L("saves.del", "Eliminar"));
  bool backClicked = bottomBtn(5, 6, L("common.back", "Volver"));

  // Modal de confirmación
  if (saveDeleteConfirm) {
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 180});

    const char *q = L("saves.delete.confirm", "¿Eliminar esta partida?");
    int qw = UI::M(q, 26);
    UI::T(q, W / 2 - qw / 2, H / 2 - 40, 26, UI::TEXT_PRI);

    auto confirmBtn = [&](int idx, const char *label) -> bool {
      int bw = 160, bh = 50, gap = 30;
      int bx = W / 2 - bw - gap / 2 + idx * (bw + gap);
      int by = H / 2 + 20;
      Rectangle r = {(float)bx, (float)by, (float)bw, (float)bh};
      Vector2 mp = GetMousePosition();
      bool hover = CheckCollisionPointRec(mp, r);
      bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

      Color bg;
      if (idx == 0)
        bg = hover ? (Color){130, 50, 50, 255} : (Color){90, 35, 35, 255};
      else
        bg = hover ? UI::BG_HOVER : UI::BG_PANEL;

      DrawRectangleRec(r, bg);
      if (idx == 0)
        DrawRectangle(bx, by + bh - 2, bw, 2, (Color){230, 80, 80, 255});
      else
        DrawRectangle(bx, by + bh - 2, bw, 2, UI::ACCENT);

      int fs = 20;
      int tw = UI::M(label, fs);
      UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs, UI::TEXT_PRI);
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
    createSeedBuf.clear();
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
  if (backClicked) {
    state = ST_MENU;
    navMenu = 0;
  }

  if (!saveDeleteConfirm && (IsKeyPressed(KEY_ESCAPE) || consumeCancel())) {
    state = ST_MENU;
    navIndex = 0;
  }
}

void Game::drawSaveCreate() {
  int W = GetScreenWidth();
  int H = GetScreenHeight();

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

  // --- Fila 0: Nombre (input de texto) ---
  {
    bool active = (navIndex == 0);
    uiTextInput(0, totalRows, 40, L("create.name", "Nombre"), createBuffer.name,
                createNameState, 40, &navIndex, active);

    // Enter sobre el nombre → siguiente fila
    if (active && confirm) {
      confirm = false;
      navIndex = 1;
    }
  }

  // --- Fila 1: Modo ---
  {
    const char *modeStr = (createBuffer.mode == MODE_STORY)
                              ? L("mode.story", "Historia")
                              : L("mode.endless", "Infinito");
    UIAction a = uiOptionEx(1, totalRows, 40, L("create.mode", "Modo"), modeStr,
                            &navIndex, &confirm, navIndex == 1 ? lr : 0);
    if (a == UI_LEFT || a == UI_RIGHT || a == UI_CONFIRM)
      createBuffer.mode = (createBuffer.mode + 1) % 2;
  }

  // --- Fila 2: Dificultad ---
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
    UIAction a =
        uiOptionEx(2, totalRows, 40, L("create.difficulty", "Dificultad"),
                   diffStr, &navIndex, &confirm, navIndex == 2 ? lr : 0);
    if (a == UI_LEFT)
      createBuffer.difficulty =
          (createBuffer.difficulty + DIFF_COUNT - 1) % DIFF_COUNT;
    if (a == UI_RIGHT)
      createBuffer.difficulty = (createBuffer.difficulty + 1) % DIFF_COUNT;
  }

  // --- Fila 3: Jugadores ---
  {
    char buf[8];
    snprintf(buf, sizeof(buf), "%s", createBuffer.twoPlayers ? "2" : "1");
    UIAction a = uiOptionEx(3, totalRows, 40, L("create.players", "Jugadores"),
                            buf, &navIndex, &confirm, navIndex == 3 ? lr : 0);
    if (a != UI_NONE)
      createBuffer.twoPlayers = !createBuffer.twoPlayers;
  }

  // --- Fila 4: Speedrun ---
  {
    const char *sr =
        createBuffer.speedrun ? L("common.yes", "Sí") : L("common.no", "No");
    UIAction a =
        uiOptionEx(4, totalRows, 40, L("create.speedrun", "Modo speedrun"), sr,
                   &navIndex, &confirm, navIndex == 4 ? lr : 0);
    if (a != UI_NONE)
      createBuffer.speedrun = !createBuffer.speedrun;
  }

  // --- Sección infinito ---
  if (endless) {
    // Seed
    {
      bool active = (navIndex == seedRow);
      uiTextInput(seedRow, totalRows, 40, L("endless.seed", "Seed"),
                  createSeedBuf, createSeedState, 10, &navIndex, active);

      // Cuando está vacío, mostrar hint visual (sin modificar el buffer)
      if (createSeedBuf.empty() && active) {
        // hack: dibujamos un ghost text encima del campo
        int x = GetScreenWidth() / 2 - 340;
        int y = GetScreenHeight() / 2 - (totalRows * 70) / 2 + 40 +
                seedRow * 70; // nota: el layout de uiTextInput es 70
        UI::T(L("endless.seed.random", "(vacío = aleatoria)"), x + 260, y + 16,
              18, UI::TEXT_MUT);
      }
    }

    // Aleatorizar
    {
      UIAction a = uiOptionEx(randomRow, totalRows, 40,
                              L("endless.randomize", "Aleatorizar seed"), "",
                              &navIndex, &confirm, 0);
      if (a != UI_NONE) {
        uint32_t s = (uint32_t)time(nullptr) ^ (uint32_t)(rand() * 2654435761u);
        if (s == 0)
          s = 1;
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", s);
        createSeedBuf = buf;
      }
    }

    // Multiplicadores (discretos → siguen siendo uiOptionEx)
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "%gx",
               (double)C::ENDLESS_MULT[createBuffer.enemyMult]);
      UIAction a =
          uiOptionEx(enemyRow, totalRows, 40,
                     L("endless.enemyMult", "Multiplicador enemigos"), buf,
                     &navIndex, &confirm, navIndex == enemyRow ? lr : 0);
      if (a == UI_LEFT)
        createBuffer.enemyMult =
            (createBuffer.enemyMult - 1 + C::ENDLESS_MULT_COUNT) %
            C::ENDLESS_MULT_COUNT;
      if (a == UI_RIGHT)
        createBuffer.enemyMult =
            (createBuffer.enemyMult + 1) % C::ENDLESS_MULT_COUNT;
    }
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "%gx",
               (double)C::ENDLESS_MULT[createBuffer.itemMult]);
      UIAction a = uiOptionEx(
          itemRow, totalRows, 40, L("endless.itemMult", "Multiplicador items"),
          buf, &navIndex, &confirm, navIndex == itemRow ? lr : 0);
      if (a == UI_LEFT)
        createBuffer.itemMult =
            (createBuffer.itemMult - 1 + C::ENDLESS_MULT_COUNT) %
            C::ENDLESS_MULT_COUNT;
      if (a == UI_RIGHT)
        createBuffer.itemMult =
            (createBuffer.itemMult + 1) % C::ENDLESS_MULT_COUNT;
    }
  }

  // --- Botones inferiores ---
  auto actionBtn = [&](int row, const char *label, bool primary) -> bool {
    int bw = 220, bh = 46, gap = 20;
    int totalW = 2 * bw + gap;
    int bx = W / 2 - totalW / 2 + (primary ? gap + bw : 0);
    // (ojo: createRow/cancelRow, pon uno a cada lado)
    bx = W / 2 - totalW / 2 + (primary ? bw + gap : 0);
    int by = H - 100;

    Rectangle r = {(float)bx, (float)by, (float)bw, (float)bh};
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    DrawRectangleRec(r, primary ? (hover ? UI::BG_HOVER : UI::BG_PANEL)
                                : (hover ? UI::BG_HOVER : UI::BG_PANEL));
    DrawRectangle(bx, by + bh - 2, bw, 2, primary ? UI::ACCENT : UI::BORDER);

    int fs = 18;
    int tw = UI::M(label, fs);
    UI::T(label, bx + (bw - tw) / 2, by + (bh - fs) / 2, fs,
          hover || primary ? UI::TEXT_PRI : UI::TEXT_SEC);
    return click;
  };

  bool cancelClicked = actionBtn(cancelRow, L("common.back", "Volver"), false);
  bool createClicked =
      actionBtn(createRow, L("create.confirm", "Crear partida"), true);

  if (createClicked || (navIndex == createRow && consumeConfirm())) {
    if (createBuffer.name.empty())
      createBuffer.name = "New world";

    if (createBuffer.mode == MODE_ENDLESS) {
      uint32_t parsed = (uint32_t)strtoul(createSeedBuf.c_str(), nullptr, 10);
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
