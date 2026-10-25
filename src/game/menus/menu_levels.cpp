#include "menu_levels.hpp"

#include "debug.hpp"
#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"

void Game::drawMenuLevelClear() {
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
}

void Game::drawMenuGameOver() {
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
}

void Game::drawMenuMain() {
  int W = GetScreenWidth();
  int H = GetScreenHeight();

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
}
