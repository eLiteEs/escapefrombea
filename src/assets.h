#pragma once
#include "common.h"

struct Assets {
  Texture2D enemyTex[ENEMY_KIND_COUNT] = {};
  Texture2D portalTex = {};
  bool hasPortalTex = false;

  Texture2D crosshair = {};
  bool hasCrosshair = false;

  Texture2D itemTex[ITEM_KIND_COUNT] = {};
  bool hasItemTex[ITEM_KIND_COUNT] = {};

  Music musicMenu = {};
  Music musicLevel = {};
  Sound sfxCaught = {};
  Sound sfxFootstep = {};
  Sound sfxEscape = {};
  Sound sfxThrow = {};
  Sound sfxPickup = {};
  Sound sfxHeartbeat = {};
  Sound sfxDrink = {};

  void unload();
  void applyVolumes(float master, float music, float sfx);

  void load(); // wrapper que llama a todas las etapas

  void loadEnemyTextures();
  void loadWallTextures();
  void loadItemTextures();
  void loadMusic();
  void loadSounds();
};
