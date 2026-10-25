#include "util_format.hpp"

#include "lang.hpp"
#include "paths.hpp"
#include "common.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace Util {

void formatSplitTime(float sec, char *out, size_t sz) {
    if (sec < 0)
        sec = 0;
    int ms = (int)(sec * 1000.0f + 0.5f);
    int totalS = ms / 1000;
    ms %= 1000;
    int m = totalS / 60, s = totalS % 60;
    if (m > 0)
        snprintf(out, sz, "%d:%02d.%03d", m, s, ms);
    else
        snprintf(out, sz, "%02d.%03d", s, ms);
}

void formatDelta(float sec, char *out, size_t sz) {
    const char *sign = (sec >= 0) ? "+" : "-";
    float a = fabsf(sec);
    int ms = (int)(a * 1000.0f + 0.5f);
    int totalS = ms / 1000;
    ms %= 1000;
    int m = totalS / 60, s = totalS % 60;
    if (m > 0)
        snprintf(out, sz, "%s%d:%02d.%03d", sign, m, s, ms);
    else
        snprintf(out, sz, "%s%02d.%03d", sign, s, ms);
}

float diffMultFor(int diff) {
    switch (diff) {
        case DIFF_EXPERT:
            return 1.3f;
        case DIFF_NIGHTMARE:
            return 1.7f;
        default:
            return 1.0f;
    }
}

std::string resolveStoryPath(const char *fname) {
    auto exists = [](const std::string &p) {
        FILE *f = fopen(p.c_str(), "rb");
        if (!f)
            return false;
        fclose(f);
        return true;
    };
    std::string p = Paths::storyIn(Lang::currentLang(), fname);
    if (exists(p))
        return p;
    if (strcmp(Lang::currentLang(), "es") != 0) {
        p = Paths::storyIn("es", fname);
        if (exists(p))
            return p;
    }
    return "";
}

std::string resolveLevelDetailsPath(int lvl) {
    char fname[64];
    snprintf(fname, sizeof(fname), "level_%02d_details.dgn", lvl);
    auto exists = [](const std::string &p) {
        FILE *f = fopen(p.c_str(), "rb");
        if (!f)
            return false;
        fclose(f);
        return true;
    };
    std::string p = Paths::levelDetailsIn(Lang::currentLang(), fname);
    if (exists(p))
        return p;
    if (strcmp(Lang::currentLang(), "es") != 0) {
        p = Paths::levelDetailsIn("es", fname);
        if (exists(p))
            return p;
    }
    return "";
}

const char *yesNo(bool b) {
    return b ? L("common.yes", "Sí") : L("common.no", "No");
}

const char *pctStr(float v) {
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)(v * 100 + 0.5f));
    return buf;
}


const char *fpsLabel(int idx) {
    int v = C::FPS_OPTIONS[idx];
    static char buf[16];
    if (v == 0)
        return L("video.fps.unlimited", "Sin límite");
    snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

const char *rdLabel(int idx) {
    switch (idx) {
        case 0:
            return L("video.renderdist.near", "Cerca (40m)");
        case 1:
            return L("video.renderdist.medium", "Media (60m)");
        default:
            return L("video.renderdist.far", "Lejos (100m)");
    }
}

const char *cbLabel(int idx) {
    switch (idx) {
        case 0:
            return L("access.cb.none", "Ninguno");
        case 1:
            return L("access.cb.deuteranopia", "Deuteranopía");
        case 2:
            return L("access.cb.protanopia", "Protanopía");
        default:
            return L("access.cb.tritanopia", "Tritanopía");
    }
}

void drawDifficultyStar(float cx, float cy, float radius, Color color,
                               bool filled) {
    const int points = 10;
    Vector2 verts[10];
    for (int i = 0; i < points; ++i) {
        float angle = -PI / 2.0f + i * (2.0f * PI / points);
        float r = (i % 2 == 0) ? radius : radius * 0.42f;
        verts[i] = {cx + cosf(angle) * r, cy + sinf(angle) * r};
    }
    if (filled) {
        Vector2 c = {cx, cy};
        for (int i = 0; i < points; ++i) {
            int j = (i + 1) % points;
            DrawTriangle(c, verts[i], verts[j], color);
            DrawTriangle(c, verts[j], verts[i], color);
        }
    } else {
        for (int i = 0; i < points; ++i) {
            int j = (i + 1) % points;
            DrawLineEx(verts[i], verts[j], 2.0f, color);
        }
    }
}

}
