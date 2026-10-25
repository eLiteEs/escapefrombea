#include "menu_pause.hpp"

#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"

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
