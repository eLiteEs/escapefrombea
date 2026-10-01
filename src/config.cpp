#include "config.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

static void clampSelf(Config& c) {
    c.difficulty = std::clamp(c.difficulty, 0, DIFF_COUNT - 1);
    c.mouseSens  = std::clamp(c.mouseSens,  0.1f, 5.0f);
    c.stickSens  = std::clamp(c.stickSens,  0.1f, 5.0f);
    c.fov        = std::clamp(c.fov, 55, 100);
    c.masterVol  = std::clamp(c.masterVol, 0.0f, 1.0f);
    c.musicVol   = std::clamp(c.musicVol,  0.0f, 1.0f);
    c.sfxVol     = std::clamp(c.sfxVol,    0.0f, 1.0f);
    c.sprintDrain= std::clamp(c.sprintDrain, 0.10f, 0.80f);
    c.sprintRegen= std::clamp(c.sprintRegen, 0.05f, 0.60f);
    c.maxLevel        = std::max(1, c.maxLevel);
    c.maxEndlessLevel = std::max(1, c.maxEndlessLevel);
    c.windowW = std::clamp(c.windowW, 640, 7680);
    c.windowH = std::clamp(c.windowH, 480, 4320);
    c.fpsLimit = std::clamp(c.fpsLimit, 0, C::FPS_COUNT - 1);
    c.renderDistance = std::clamp(c.renderDistance, 0, 2);
    c.guiScale = std::clamp(c.guiScale, 0.75f, 2.0f);
    c.colorblindMode = std::clamp(c.colorblindMode, 0, CB_COUNT - 1);
    if (c.language.empty()) c.language = "es";
}

float Config::diffMult() const {
    switch (difficulty) {
        case DIFF_EXPERT:    return 1.3f;
        case DIFF_NIGHTMARE: return 1.7f;
        default:             return 1.0f;
    }
}

const char* Config::diffName() const {
    switch (difficulty) {
        case DIFF_EXPERT:    return "EXPERTO";
        case DIFF_NIGHTMARE: return "PESADILLA";
        default:             return "NORMAL";
    }
}

const char* Config::keyName(int key) {
    switch (key) {
        case KEY_W: return "W";
        case KEY_A: return "A";
        case KEY_S: return "S";
        case KEY_D: return "D";
        case KEY_UP: return "ARRIBA";
        case KEY_DOWN: return "ABAJO";
        case KEY_LEFT: return "IZQUIERDA";
        case KEY_RIGHT: return "DERECHA";
        case KEY_ESCAPE: return "ESC";
        case KEY_I: return "I"; case KEY_J: return "J";
        case KEY_K: return "K"; case KEY_L: return "L";
        case KEY_SPACE: return "ESPACIO";
        case KEY_LEFT_SHIFT: return "SHIFT IZQ";
        case KEY_LEFT_CONTROL: return "CTRL IZQ";
        case KEY_ENTER: return "ENTER";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "TECLA %d", key);
            return buf;
        }
    }
}

void Config::save() const {
    FILE* f = fopen(C::CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "fullscreen=%d\n", fullscreen ? 1 : 0);
    fprintf(f, "vsync=%d\n",      vsync ? 1 : 0);
    fprintf(f, "fov=%d\n",        fov);
    fprintf(f, "showFps=%d\n",    showFps ? 1 : 0);
    fprintf(f, "masterVol=%.3f\n", (double)masterVol);
    fprintf(f, "musicVol=%.3f\n",  (double)musicVol);
    fprintf(f, "sfxVol=%.3f\n",    (double)sfxVol);
    fprintf(f, "difficulty=%d\n", difficulty);
    fprintf(f, "invertY=%d\n",    invertY ? 1 : 0);
    fprintf(f, "mouseSens=%.3f\n", (double)mouseSens);
    fprintf(f, "stickSens=%.3f\n", (double)stickSens);
    fprintf(f, "showMinimap=%d\n", showMinimap ? 1 : 0);
    fprintf(f, "headBob=%d\n",     headBob ? 1 : 0);
    fprintf(f, "sprintDrain=%.3f\n", (double)sprintDrain);
    fprintf(f, "sprintRegen=%.3f\n", (double)sprintRegen);
    fprintf(f, "playCinematics=%d\n", playCinematics ? 1 : 0);
    fprintf(f, "language=%s\n",   language.c_str());
    fprintf(f, "swapGamepads=%d\n", swapGamepads ? 1 : 0);
    fprintf(f, "maxLevel=%d\n",   maxLevel);
    fprintf(f, "maxEndlessLevel=%d\n", maxEndlessLevel);
    for (int p = 0; p < 2; ++p) {
        fprintf(f, "p%d_up=%d\n",    p+1, binds[p].up);
        fprintf(f, "p%d_down=%d\n",  p+1, binds[p].down);
        fprintf(f, "p%d_left=%d\n",  p+1, binds[p].left);
        fprintf(f, "p%d_right=%d\n", p+1, binds[p].right);
    }
    fprintf(f, "windowW=%d\n",       windowW);
    fprintf(f, "windowH=%d\n",       windowH);
    fprintf(f, "fpsLimit=%d\n",      fpsLimit);
    fprintf(f, "renderDistance=%d\n",renderDistance);
    fprintf(f, "guiScale=%.3f\n",    (double)guiScale);
    fprintf(f, "colorblindMode=%d\n",colorblindMode);
    fprintf(f, "highContrast=%d\n",  highContrast ? 1 : 0);
    fprintf(f, "reduceMotion=%d\n",  reduceMotion ? 1 : 0);
    fprintf(f, "bigSubtitles=%d\n",  bigSubtitles ? 1 : 0);
    fprintf(f, "holdToSprint=%d\n",  holdToSprint ? 1 : 0);
    fclose(f);
}

void Config::load() {
    FILE* f = fopen(C::CONFIG_PATH, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        for (char* p = line; *p; ++p) if (*p == '\r' || *p == '\n') { *p = 0; break; }
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* val = eq + 1;

        auto asInt   = [](const char* v) { return (int)atol(v); };
        auto asFloat = [](const char* v) { return (float)atof(v); };

        if      (!strcmp(key, "fullscreen"))   fullscreen   = asInt(val) != 0;
        else if (!strcmp(key, "vsync"))        vsync        = asInt(val) != 0;
        else if (!strcmp(key, "fov"))          fov          = asInt(val);
        else if (!strcmp(key, "showFps"))      showFps      = asInt(val) != 0;
        else if (!strcmp(key, "masterVol"))    masterVol    = asFloat(val);
        else if (!strcmp(key, "musicVol"))     musicVol     = asFloat(val);
        else if (!strcmp(key, "sfxVol"))       sfxVol       = asFloat(val);
        else if (!strcmp(key, "difficulty"))   difficulty   = asInt(val);
        else if (!strcmp(key, "invertY"))      invertY      = asInt(val) != 0;
        else if (!strcmp(key, "mouseSens"))    mouseSens    = asFloat(val);
        else if (!strcmp(key, "stickSens"))    stickSens    = asFloat(val);
        else if (!strcmp(key, "showMinimap"))  showMinimap  = asInt(val) != 0;
        else if (!strcmp(key, "headBob"))      headBob      = asInt(val) != 0;
        else if (!strcmp(key, "sprintDrain"))  sprintDrain  = asFloat(val);
        else if (!strcmp(key, "sprintRegen"))  sprintRegen  = asFloat(val);
        else if (!strcmp(key, "playCinematics")) playCinematics = asInt(val) != 0;
        else if (!strcmp(key, "language"))     language     = val;
        else if (!strcmp(key, "swapGamepads")) swapGamepads = asInt(val) != 0;
        else if (!strcmp(key, "maxLevel"))     maxLevel     = asInt(val);
        else if (!strcmp(key, "maxEndlessLevel")) maxEndlessLevel = asInt(val);
        else if (!strcmp(key, "windowW"))        windowW        = asInt(val);
        else if (!strcmp(key, "windowH"))        windowH        = asInt(val);
        else if (!strcmp(key, "fpsLimit"))       fpsLimit       = asInt(val);
        else if (!strcmp(key, "renderDistance")) renderDistance = asInt(val);
        else if (!strcmp(key, "guiScale"))       guiScale       = asFloat(val);
        else if (!strcmp(key, "colorblindMode")) colorblindMode = asInt(val);
        else if (!strcmp(key, "highContrast"))   highContrast   = asInt(val) != 0;
        else if (!strcmp(key, "reduceMotion"))   reduceMotion   = asInt(val) != 0;
        else if (!strcmp(key, "bigSubtitles"))   bigSubtitles   = asInt(val) != 0;
        else if (!strcmp(key, "holdToSprint"))   holdToSprint   = asInt(val) != 0;
	else {
            for (int p = 0; p < 2; ++p) {
                char buf[16];
                snprintf(buf, sizeof(buf), "p%d_up", p+1);    if (!strcmp(key, buf)) binds[p].up    = asInt(val);
                snprintf(buf, sizeof(buf), "p%d_down", p+1);  if (!strcmp(key, buf)) binds[p].down  = asInt(val);
                snprintf(buf, sizeof(buf), "p%d_left", p+1);  if (!strcmp(key, buf)) binds[p].left  = asInt(val);
                snprintf(buf, sizeof(buf), "p%d_right", p+1); if (!strcmp(key, buf)) binds[p].right = asInt(val);
            }
        }
    }
    fclose(f);
    clampSelf(*this);
}

