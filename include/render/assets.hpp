#pragma once
#include "common.hpp"

struct Assets {
  Texture2D enemyTex[ENEMY_KIND_COUNT] = {}; // Enemy textures

  // Font
  Font font = {};

  // Exit portal texture
  Texture2D portalTex = {};
  bool hasPortalTex = false;

  // Fountain texture
  Texture2D fountainTex = {};
  bool hasFountainTex = false;

  // Crosshair texture
  Texture2D crosshair = {};
  bool hasCrosshair = false;

  // Item textures
  Texture2D itemTex[ITEM_KIND_COUNT] = {};
  bool hasItemTex[ITEM_KIND_COUNT] = {};

  // UI custom assets
  Texture2D uiSkull      = {};
  Texture2D uiArrowLeft  = {};
  Texture2D uiArrowRight = {};
  Texture2D uiHealth  = {};
  Texture2D uiArmor = {};
  bool hasUiSkull      = false;
  bool hasUiArrowLeft  = false;
  bool hasUiArrowRight = false;
  bool hasUiHealth     = false;
  bool hasUiArmor      = false;

  // Level wall decorations textures
  Texture2D wallTile = {};
  Texture2D wallLocker = {};
  Texture2D wallBrick = {};
  Texture2D floorTile = {};
  bool hasWallTile = false;
  bool hasWallLocker = false;
  bool hasWallBrick = false;
  bool hasFloorTile = false;

  // Level decorations textures
  Texture2D stairTex = {};
  Texture2D signTex = {};
  Texture2D npcTex = {};
  bool hasStairTex = false;
  bool hasSignTex = false;
  bool hasNpcTex = false;

  // Music
  Music musicMenu = {};
  Music musicLevel = {};

  // Sounds
  Sound sfxCaught = {};
  Sound sfxFootstep = {};
  Sound sfxEscape = {};
  Sound sfxThrow = {};
  Sound sfxPickup = {};
  Sound sfxHeartbeat = {};
  Sound sfxDrink = {};

  void applyVolumes(float master, float music, float sfx);

  void load();
  void unload();

  void loadEnemyTextures();
  void loadWallTextures();
  void loadItemTextures();
  void loadUiTextures();
  void loadMusic();
  void loadSounds();
  void loadFonts();
};
