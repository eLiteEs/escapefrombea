#pragma once
#include "common.h"

struct Assets {
    Texture2D enemyTex[ENEMY_KIND_COUNT] = {};
    Texture2D portalTex = {};
    bool hasPortalTex   = false;

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

