#pragma once
#include "common.h"

struct Assets {
    Texture2D enemyTex[ENEMY_KIND_COUNT] = {};
    Texture2D portalTex = {};
    bool hasPortalTex   = false;

    // NUEVO
    Texture2D wallTile   = {};
    Texture2D wallLocker = {};
    Texture2D wallBrick  = {};
    Texture2D floorTile  = {};
    Texture2D crosshair = {};
    bool hasWallTile     = false;
    bool hasWallLocker   = false;
    bool hasWallBrick    = false;
    bool hasFloorTile    = false;
    bool hasCrosshair    = false;

    Texture2D itemTex[ITEM_KIND_COUNT] = {};
    bool hasItemTex[ITEM_KIND_COUNT]   = {};

    Music musicMenu  = {};
    Music musicLevel = {};
    Sound sfxCaught  = {};
    Sound sfxEscape  = {};
    Sound sfxFootstep = {};
    Sound sfxHeartbeat = {};

    void load();
    void unload();
    void applyVolumes(float master, float music, float sfx);
};

