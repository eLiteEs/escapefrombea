#include "menu_config.hpp"

#include "game.hpp"
#include "lang.hpp"
#include "ui.hpp"
#include "util_format.hpp"

static const char *rcResLabel(int idx) {
  static char buf[16];
  snprintf(buf, sizeof(buf), "%dx%d", C::RC_RESOLUTIONS[idx][0],
           C::RC_RESOLUTIONS[idx][1]);
  return buf;
}

void Game::drawConfigHub() {
  drawConfigHeader(L("menu.config", "Configuración"));
  int items = 8;
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

  if (uiButton(5, items, 30, L("config.speedrun", "Speedrun"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 5 && consumeConfirm())) {
    state = ST_CONFIG_SPEEDRUN;
    navIndex = 0;
  }

  if (uiButton(
          6, items, 30,
          TextFormat(L("config.language", "Idioma: %s"), Lang::currentLang()),
          &navIndexCfg, nullptr) ||
      (navIndexCfg == 6 && consumeConfirm())) {
    std::string cur = Lang::currentLang();
    cfg.language = (cur == "es") ? "en" : "es";
    Lang::load(cfg.language.c_str());
    reloadLevelTranslations();
    cfg.save();
  }

  if (uiButton(7, items, 30, L("common.back", "Volver"), &navIndexCfg,
               nullptr) ||
      (navIndexCfg == 6 && consumeConfirm()) || consumeCancel()) {
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
                            Util::fpsLabel(cfg.fpsLimit), &navIndex, nullptr,
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
                   Util::rdLabel(cfg.renderDistance), &navIndex, nullptr,
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
    UIAction a = uiSlider(4, items, 30, L("video.guiscale", "Tamaño de la interfaz"),
                      &cfg.guiScale, 0.75f, 2.00f, 0.05f, &navIndex, nullptr,
                      navIndex == 4 ? lr : 0, "x");
    if (a != UI_NONE) { UI::scale = cfg.guiScale; save(); }
  }
  if (uiOptionEx(5, items, 30, L("video.vsync", "VSync"),
                 Util::yesNo(cfg.vsync), &navIndex, nullptr,
                 navIndex == 5 ? lr : 0) != UI_NONE) {
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

  auto apply = [&]() {
    assets.applyVolumes(cfg.masterVol, cfg.musicVol, cfg.sfxVol);
    cfg.save();
  };

  UIAction a;

  a = uiSlider(0, items, 40, L("audio.master", "Volumen maestro"),
               &cfg.masterVol, 0.0f, 1.0f, 0.05f, &navIndex, nullptr,
               navIndex == 0 ? lr : 0, "%");
  if (a != UI_NONE) apply();

  a = uiSlider(1, items, 40, L("audio.music", "Música"),
               &cfg.musicVol, 0.0f, 1.0f, 0.05f, &navIndex, nullptr,
               navIndex == 1 ? lr : 0, "%");
  if (a != UI_NONE) apply();

  a = uiSlider(2, items, 40, L("audio.sfx", "Efectos"),
               &cfg.sfxVol, 0.0f, 1.0f, 0.05f, &navIndex, nullptr,
               navIndex == 2 ? lr : 0, "%");
  if (a != UI_NONE) apply();

  if (uiOptionEx(3, items, 40, L("common.back", "Volver"), "", &navIndex,
    nullptr, 0) != UI_NONE || IsKeyPressed(KEY_ESCAPE))
    state = ST_CONFIG;
}

void Game::drawConfigGameplay() {
  drawConfigHeader(L("config.gameplay", "Jugabilidad"));
  int items = 8;
  handleGamepadMenuNav(items);
  int lr = gamepadLeftRight();
  auto save = [&]() { cfg.save(); };

  if (uiOptionEx(0, items, 40, L("gameplay.inverty", "Invertir eje Y"),
                 Util::yesNo(cfg.invertY), &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.invertY = !cfg.invertY;
    save();
  }
  {
    UIAction a = uiSlider(1, items, 40, L("gameplay.mousesens", "Sensibilidad del ratón"),
                      &cfg.mouseSens, 0.10f, 5.00f, 0.05f, &navIndex, nullptr,
                      navIndex == 1 ? lr : 0, "");
    if (a != UI_NONE) save();
  }
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", cfg.stickSens);
    UIAction a = uiOptionEx(2, items, 40,
                            L("gameplay.padsens", "Sensibilidad del mando"),
                            buf, &navIndex, nullptr, navIndex == 2 ? lr : 0);
    if (a != UI_NONE) save();
  }
  if (uiOptionEx(3, items, 40, L("gameplay.minimap", "Minimapa"),
                 Util::yesNo(cfg.showMinimap), &navIndex, nullptr,
                 navIndex == 3 ? lr : 0) != UI_NONE) {
    cfg.showMinimap = !cfg.showMinimap;
    save();
  }
  {
    UIAction a = uiSlider(4, items, 40, L("gameplay.sprintdrain", "Consumo de sprint"),
                          &cfg.sprintDrain, 0.10f, 0.80f, 0.05f, &navIndex, nullptr,
                          navIndex == 4 ? lr : 0, "");
    if (a != UI_NONE) save();
  }
  {
    UIAction a = uiSlider(5, items, 40, L("gameplay.sprintregen", "Regeneración de sprint"),
                          &cfg.sprintRegen, 0.05f, 0.60f, 0.05f, &navIndex, nullptr,
                          navIndex == 5 ? lr : 0, "");
    if (a != UI_NONE) save();
  }
  if (uiOptionEx(6, items, 40, L("gameplay.cinematics", "Cinemáticas"),
                 Util::yesNo(cfg.playCinematics), &navIndex, nullptr,
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
                 Util::yesNo(cfg.swapGamepads), &navIndex, nullptr,
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
      (consumeCancel() && remapAction < 0))
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
                 Util::cbLabel(cfg.colorblindMode), &navIndex, nullptr,
                 navIndex == 0 ? lr : 0) != UI_NONE) {
    cfg.colorblindMode = (cfg.colorblindMode + 1) % CB_COUNT;
    UI::colorblind = cfg.colorblindMode;
    save();
  }
  if (uiOptionEx(1, items, 30, L("access.contrast", "Alto contraste"),
                 Util::yesNo(cfg.highContrast), &navIndex, nullptr,
                 navIndex == 1 ? lr : 0) != UI_NONE) {
    cfg.highContrast = !cfg.highContrast;
    save();
  }
  if (uiOptionEx(2, items, 30, L("access.motion", "Reducir movimiento"),
                 Util::yesNo(cfg.reduceMotion), &navIndex, nullptr,
                 navIndex == 2 ? lr : 0) != UI_NONE) {
    cfg.reduceMotion = !cfg.reduceMotion;
    save();
  }
  if (uiOptionEx(3, items, 30, L("access.subtitles", "Subtítulos grandes"),
                 Util::yesNo(cfg.bigSubtitles), &navIndex, nullptr,
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
