#include "assets.h"
#include "paths.h"
#include <cstdio>

static bool fileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static Texture2D loadTileable(const char* path, bool& hasFlag) {
    Texture2D t = LoadTexture(path);
    if (t.id == 0) { hasFlag = false; return t; }
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    hasFlag = true;
    return t;
}

static Texture2D loadTextureOrProcedural(const char* path, Color bodyColor) {
    Texture2D t = LoadTexture(path);
    if (t.id != 0) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); return t; }

    Image img = GenImageColor(128, 128, BLANK);
    ImageDrawCircle(&img, 64, 66, 56, bodyColor);
    Color glow = bodyColor; glow.a = 120;
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

static Sound loadSoundSmart(const char* name) {
    std::string np = Paths::sound(name);
    if (fileExists(np)) return LoadSound(np.c_str());
    std::string lp = Paths::legacyAudio(name);
    if (fileExists(lp)) return LoadSound(lp.c_str());
    return LoadSound(np.c_str());
}

static Music loadMusicSmart(const char* name) {
    std::string np = Paths::music(name);
    if (fileExists(np)) return LoadMusicStream(np.c_str());
    std::string lp = Paths::legacyAudio(name);
    if (fileExists(lp)) return LoadMusicStream(lp.c_str());
    return LoadMusicStream(np.c_str());
}

void Assets::load() {
    std::string p;

    p = Paths::sprite("enemy_bea.png");
    enemyTex[ENEMY_BEA]    = loadTextureOrProcedural(p.c_str(), (Color){ 210, 45, 60, 255 });
    p = Paths::sprite("enemy_marisa.png");
    enemyTex[ENEMY_MARISA] = loadTextureOrProcedural(p.c_str(), (Color){ 60, 90, 210, 255 });
    p = Paths::sprite("enemy_alt.png");
    enemyTex[ENEMY_ALT]    = loadTextureOrProcedural(p.c_str(), (Color){ 160, 40, 200, 255 });
    p = Paths::sprite("enemy_cv.png");
    enemyTex[ENEMY_CV]    = loadTextureOrProcedural(p.c_str(), (Color){ 160, 40, 200, 255 });
    p = Paths::sprite("enemy_ye.png");
    enemyTex[ENEMY_YE]    = loadTextureOrProcedural(p.c_str(), (Color){ 160, 40, 200, 255 });

    p = Paths::sprite("exit_portal.png");
    portalTex    = LoadTexture(p.c_str());
    hasPortalTex = portalTex.id != 0;
    if (hasPortalTex) SetTextureFilter(portalTex, TEXTURE_FILTER_BILINEAR);

    p = Paths::sprite("crosshair.png");
    crosshair = loadTextureOrProcedural(p.c_str(), (Color){ 160, 40, 200, 255 });

    wallTile   = loadTileable(Paths::sprite("wall_tile.png").c_str(),   hasWallTile);
    wallLocker = loadTileable(Paths::sprite("wall_locker.png").c_str(), hasWallLocker);
    wallBrick  = loadTileable(Paths::sprite("wall_brick.png").c_str(),  hasWallBrick);
    floorTile  = loadTileable(Paths::sprite("floor_tile.png").c_str(),  hasFloorTile);

    {
        struct { ItemKind kind; const char* file; } defs[] = {
            { ITEM_ROCK,    "rock.png"    },
            { ITEM_BOX,     "box.png"     },
            { ITEM_LIME,    "lime.png"    },
            { ITEM_BICIMAD, "bicimad.png" },
        };
        for (auto& d : defs) {
            std::string path = Paths::sprite(d.file);
            itemTex[d.kind] = LoadTexture(path.c_str());
            hasItemTex[d.kind] = (itemTex[d.kind].id != 0);
            if (hasItemTex[d.kind])
                SetTextureFilter(itemTex[d.kind], TEXTURE_FILTER_BILINEAR);
        }
    }

    musicMenu  = loadMusicSmart("music_menu.ogg");
    musicLevel = loadMusicSmart("music_level.ogg");

    sfxCaught    = loadSoundSmart("sfx_caught.ogg");
    sfxEscape    = loadSoundSmart("sfx_escape.ogg");
    sfxFootstep  = loadSoundSmart("sfx_footstep.ogg");
    sfxHeartbeat = loadSoundSmart("sfx_heartbeat.ogg");
}

void Assets::unload() {
    for (int i = 0; i < ENEMY_KIND_COUNT; ++i)
        if (enemyTex[i].id != 0) UnloadTexture(enemyTex[i]);
    if (hasPortalTex && portalTex.id != 0) UnloadTexture(portalTex);

    if(hasCrosshair && crosshair.id != 0) UnloadTexture(crosshair);

    if (hasWallTile   && wallTile.id   != 0) UnloadTexture(wallTile);
    if (hasWallLocker && wallLocker.id != 0) UnloadTexture(wallLocker);
    if (hasWallBrick  && wallBrick.id  != 0) UnloadTexture(wallBrick);
    if (hasFloorTile  && floorTile.id  != 0) UnloadTexture(floorTile);

    for (int i = 0; i < ITEM_KIND_COUNT; ++i)
        if (hasItemTex[i] && itemTex[i].id != 0)
            UnloadTexture(itemTex[i]);

    if (musicMenu.stream.buffer  != nullptr) UnloadMusicStream(musicMenu);
    if (musicLevel.stream.buffer != nullptr) UnloadMusicStream(musicLevel);
    if (sfxCaught.frameCount    > 0) UnloadSound(sfxCaught);
    if (sfxEscape.frameCount    > 0) UnloadSound(sfxEscape);
    if (sfxFootstep.frameCount  > 0) UnloadSound(sfxFootstep);
    if (sfxHeartbeat.frameCount > 0) UnloadSound(sfxHeartbeat);
}

void Assets::applyVolumes(float master, float music, float sfx) {
    float m = master * music;
    float s = master * sfx;
    if (musicMenu.stream.buffer  != nullptr) SetMusicVolume(musicMenu,  m);
    if (musicLevel.stream.buffer != nullptr) SetMusicVolume(musicLevel, m);
    if (sfxCaught.frameCount    > 0) SetSoundVolume(sfxCaught,    s);
    if (sfxEscape.frameCount    > 0) SetSoundVolume(sfxEscape,    s);
    if (sfxFootstep.frameCount  > 0) SetSoundVolume(sfxFootstep,  s);
    if (sfxHeartbeat.frameCount > 0) SetSoundVolume(sfxHeartbeat, s);
}

