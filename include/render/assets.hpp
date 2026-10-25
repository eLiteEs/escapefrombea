#pragma once
#include "common.hpp"

struct Assets {
  Texture2D enemyTex[ENEMY_KIND_COUNT] = {};
  Texture2D portalTex = {};
  bool hasPortalTex = false;

  Texture2D crosshair = {};
  bool hasCrosshair = false;

  Texture2D itemTex[ITEM_KIND_COUNT] = {};
  bool hasItemTex[ITEM_KIND_COUNT] = {};

  Texture2D wallTile = {};
  Texture2D wallLocker = {};
  Texture2D wallBrick = {};
  Texture2D floorTile = {};
  bool hasWallTile = false;
  bool hasWallLocker = false;
  bool hasWallBrick = false;
  bool hasFloorTile = false;

  Texture2D stairTex = {};
  Texture2D signTex = {};
  Texture2D npcTex = {};
  bool hasStairTex = false;
  bool hasSignTex = false;
  bool hasNpcTex = false;

  Texture2D fountainTex = {};
  bool hasFountainTex = false;

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
  void loadFonts();

  Font font = {};
};
