#include "assets.hpp"
#include "paths.hpp"
#include <cstdio>

static bool fileExists(const std::string &p) {
  FILE *f = fopen(p.c_str(), "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}

static Texture2D loadTileable(const char *path, bool &hasFlag) {
  Texture2D t = LoadTexture(path);
  if (t.id == 0) {
    hasFlag = false;
    return t;
  }
  SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
  hasFlag = true;
  return t;
}

static Texture2D loadTextureOrProcedural(const char *path, Color bodyColor) {
  Texture2D t = LoadTexture(path);
  if (t.id != 0) {
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
  }

  Image img = GenImageColor(128, 128, BLANK);
  ImageDrawCircle(&img, 64, 66, 56, bodyColor);
  Color glow = bodyColor;
  glow.a = 120;
  ImageDrawCircle(&img, 40, 34, 10, glow);
  ImageDrawCircle(&img, 44, 50, 13, WHITE);
  ImageDrawCircle(&img, 84, 50, 13, WHITE);
  ImageDrawCircle(&img, 47, 53, 6, BLACK);
  ImageDrawCircle(&img, 87, 53, 6, BLACK);
  ImageDrawRectangle(&img, 46, 86, 36, 10, BLACK);
  t = LoadTextureFromImage(img);
  SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
  UnloadImage(img);
  return t;
}

static Sound loadSoundSmart(const char *name) {
  std::string np = Paths::sound(name);
  if (fileExists(np))
    return LoadSound(np.c_str());
  std::string lp = Paths::legacyAudio(name);
  if (fileExists(lp))
    return LoadSound(lp.c_str());
  return LoadSound(np.c_str());
}

static Music loadMusicSmart(const char *name) {
  std::string np = Paths::music(name);
  if (fileExists(np))
    return LoadMusicStream(np.c_str());
  std::string lp = Paths::legacyAudio(name);
  if (fileExists(lp))
    return LoadMusicStream(lp.c_str());
  return LoadMusicStream(np.c_str());
}

void Assets::loadUiTextures() {
  auto loadIfPresent = [](const char* name, Texture2D& tex, bool& has) {
    std::string p = Paths::sprite(name);
    tex = LoadTexture(p.c_str());
    has = (tex.id != 0);
    if (has) SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
  };
    loadIfPresent("ui_skull.png",       uiSkull,      hasUiSkull);
    loadIfPresent("ui_arrow_left.png",  uiArrowLeft,  hasUiArrowLeft);
    loadIfPresent("ui_arrow_right.png", uiArrowRight, hasUiArrowRight);
}

void Assets::loadEnemyTextures() {
  std::string p;

  p = Paths::sprite("enemy_bea.png");
  enemyTex[ENEMY_BEA] =
      loadTextureOrProcedural(p.c_str(), (Color){210, 45, 60, 255});
  p = Paths::sprite("enemy_marisa.png");
  enemyTex[ENEMY_MARISA] =
      loadTextureOrProcedural(p.c_str(), (Color){60, 90, 210, 255});
  p = Paths::sprite("enemy_alt.png");
  enemyTex[ENEMY_ALT] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  p = Paths::sprite("enemy_cv.png");
  enemyTex[ENEMY_CV] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  p = Paths::sprite("enemy_ye.png");
  enemyTex[ENEMY_YE] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  p = Paths::sprite("enemy_almeida.png");
  enemyTex[ENEMY_ALMEIDA] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  p = Paths::sprite("enemy_hypergamy.png");
  enemyTex[ENEMY_HYPERGAMY] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  p = Paths::sprite("player.png");
  enemyTex[PLAYER] =
      loadTextureOrProcedural(p.c_str(), (Color){160, 160, 160, 255});

  p = Paths::sprite("exit_portal.png");
  portalTex = LoadTexture(p.c_str());
  hasPortalTex = portalTex.id != 0;
  if (hasPortalTex)
    SetTextureFilter(portalTex, TEXTURE_FILTER_BILINEAR);

  auto loadOrFallback = [](const char *name, Texture2D &out, bool &has,
                           Color base) {
    std::string p = Paths::sprite(name);
    out = LoadTexture(p.c_str());
    has = (out.id != 0);
    if (has) {
      SetTextureFilter(out, TEXTURE_FILTER_BILINEAR);
      return;
    }
    Image img = GenImageColor(32, 32, base);
    out = LoadTextureFromImage(img);
    UnloadImage(img);
    has = (out.id != 0);
  };
  loadOrFallback("stair.png", stairTex, hasStairTex, (Color){120, 90, 60, 255});
  loadOrFallback("sign.png", signTex, hasSignTex, (Color){160, 120, 70, 255});
  loadOrFallback("npc.png", npcTex, hasNpcTex, (Color){200, 200, 220, 255});

  p = Paths::sprite("crosshair.png");
  crosshair = loadTextureOrProcedural(p.c_str(), (Color){160, 40, 200, 255});
  hasCrosshair = crosshair.id != 0;
}

void Assets::loadWallTextures() {
  wallTile = loadTileable(Paths::sprite("wall_tile.png").c_str(), hasWallTile);
  wallLocker =
      loadTileable(Paths::sprite("wall_locker.png").c_str(), hasWallLocker);
  wallBrick =
      loadTileable(Paths::sprite("wall_brick.png").c_str(), hasWallBrick);
  floorTile =
      loadTileable(Paths::sprite("floor_tile.png").c_str(), hasFloorTile);
}

void Assets::loadItemTextures() {
  struct {
    ItemKind kind;
    const char *file;
  } defs[] = {{ITEM_ROCK, "item_rock.png"},
              {ITEM_BOX, "item_box.png"},
              {ITEM_LIME, "item_lime.png"},
              {ITEM_BICIMAD, "item_bicimad.png"},
              {ITEM_MONSTER, "item_monster.png"},
              {ITEM_WALL, "item_wall.png"},
              {ITEM_KEY, "item_key.png"},
              {ITEM_BIFRUTAS, "item_bifrutas.png"},
              {ITEM_ENSAIMADA, "item_ensaimada.png"}};
  for (auto &d : defs) {
    std::string path = Paths::sprite(d.file);
    itemTex[d.kind] = LoadTexture(path.c_str());
    hasItemTex[d.kind] = (itemTex[d.kind].id != 0);
    if (hasItemTex[d.kind])
      SetTextureFilter(itemTex[d.kind], TEXTURE_FILTER_BILINEAR);
  }

  {
    std::string path = Paths::sprite("fountain.png");
    fountainTex = LoadTexture(path.c_str());
    hasFountainTex = (fountainTex.id != 0);

    if (hasFountainTex) {
      SetTextureFilter(fountainTex, TEXTURE_FILTER_BILINEAR);
    } else {
      // Fallback procedural: franjas horizontales azul/gris
      Image img = GenImageColor(32, 32, BLANK);
      for (int y = 0; y < 32; ++y) {
        Color band;
        if (y < 8)
          band = {180, 230, 255, 255};
        else if (y < 16)
          band = {60, 180, 220, 255};
        else if (y < 24)
          band = {90, 95, 115, 255};
        else
          band = {50, 55, 75, 255};
        for (int x = 0; x < 32; ++x)
          ImageDrawPixel(&img, x, y, band);
      }
      fountainTex = LoadTextureFromImage(img);
      UnloadImage(img);
      hasFountainTex = (fountainTex.id != 0);
    }
  }
}

void Assets::loadMusic() {
  musicMenu = loadMusicSmart("music_menu.ogg");
  musicLevel = loadMusicSmart("music_level.ogg");
}

void Assets::loadSounds() {
  sfxEscape = loadSoundSmart("sfx_escape.ogg");
  sfxCaught = loadSoundSmart("sfx_caught.ogg");
  sfxPickup = loadSoundSmart("sfx_pickup.ogg");
  sfxThrow = loadSoundSmart("sfx_throw.ogg");
  sfxFootstep = loadSoundSmart("sfx_footstep.ogg");
  sfxHeartbeat = loadSoundSmart("sfx_heartbeat.ogg");
  sfxDrink = loadSoundSmart("sfx_drink.ogg");
}

void Assets::loadFonts() {
  // Codepoints que necesitas (ASCII + Latin-1 Supplement para castellano)
  // Puedes generar el array con LoadCodepoints desde un string
  const char *chars = " !\"#$%&'()*+,-./0123456789:;<=>?@"
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                      "abcdefghijklmnopqrstuvwxyz{|}~"
                      "¡¿ÁÉÍÓÚÜÑáéíóúüñ"
                      "«»“”‘’—–…°ªº€";

  int cpCount = 0;
  int *codepoints = LoadCodepoints(chars, &cpCount);

  font = LoadFontEx(Paths::font("font.ttf").c_str(), 64, codepoints, cpCount);
  UnloadCodepoints(codepoints);

  if (font.texture.id != 0) {
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    TraceLog(LOG_INFO, "[Font] Cargada: %d glyphs", font.glyphCount);
  } else {
    font = GetFontDefault();
  }
}

void Assets::load() {
  loadEnemyTextures();
  loadWallTextures();
  loadItemTextures();
  loadMusic();
  loadSounds();
  loadFonts();
}

void Assets::unload() {
  for (int i = 0; i < ENEMY_KIND_COUNT; ++i)
    if (enemyTex[i].id != 0)
      UnloadTexture(enemyTex[i]);

  if (hasCrosshair && crosshair.id != 0)
    UnloadTexture(crosshair);

  for (int i = 0; i < ITEM_KIND_COUNT; ++i)
    if (hasItemTex[i] && itemTex[i].id != 0)
      UnloadTexture(itemTex[i]);

  if (hasFountainTex && fountainTex.id != 0)
    UnloadTexture(fountainTex);

  if (musicMenu.stream.buffer != nullptr)
    UnloadMusicStream(musicMenu);
  if (musicLevel.stream.buffer != nullptr)
    UnloadMusicStream(musicLevel);
  if (sfxCaught.frameCount > 0)
    UnloadSound(sfxCaught);
  if (sfxEscape.frameCount > 0)
    UnloadSound(sfxEscape);
  if (sfxHeartbeat.frameCount > 0)
    UnloadSound(sfxHeartbeat);
  if (sfxFootstep.frameCount > 0)
    UnloadSound(sfxFootstep);
  if (sfxThrow.frameCount > 0)
    UnloadSound(sfxThrow);
  if (sfxPickup.frameCount > 0)
    UnloadSound(sfxPickup);

  if (hasStairTex && stairTex.id != 0)
    UnloadTexture(stairTex);
  if (hasSignTex && signTex.id != 0)
    UnloadTexture(signTex);
  if (hasNpcTex && npcTex.id != 0)
    UnloadTexture(npcTex);

  if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
    UnloadFont(font);
  }

  if (hasUiSkull      && uiSkull.id      != 0) UnloadTexture(uiSkull);
  if (hasUiArrowLeft  && uiArrowLeft.id  != 0) UnloadTexture(uiArrowLeft);
  if (hasUiArrowRight && uiArrowRight.id != 0) UnloadTexture(uiArrowRight);
}

void Assets::applyVolumes(float master, float music, float sfx) {
  float m = master * music;
  float s = master * sfx;
  if (musicMenu.stream.buffer != nullptr)
    SetMusicVolume(musicMenu, m);
  if (musicLevel.stream.buffer != nullptr)
    SetMusicVolume(musicLevel, m);
  if (sfxCaught.frameCount > 0)
    SetSoundVolume(sfxCaught, s);
  if (sfxEscape.frameCount > 0)
    SetSoundVolume(sfxEscape, s);
  if (sfxFootstep.frameCount > 0)
    SetSoundVolume(sfxFootstep, s);
  if (sfxHeartbeat.frameCount > 0)
    SetSoundVolume(sfxHeartbeat, s);
}
