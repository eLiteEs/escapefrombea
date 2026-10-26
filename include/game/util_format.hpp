#pragma once

#include "raylib.h"

#include <cstddef>
#include <string>
#include <ctime>

namespace Util {
    void formatSplitTime(float sec, char* out, size_t sz);
    void formatDelta(float sec, char* out, size_t sz);
    float diffMultFor(int diff);
    std::string resolveStoryPath(const char* fname);
    std::string resolveLevelDetailsPath(int lvl);
    const char* yesNo(bool b);
    const char* pctStr(float v);
    const char* fpsLabel(int idx);
    const char* rdLabel(int idx);
    const char* cbLabel(int idx);
    void drawDifficultyStar(float cx, float cy, float radius, Color color, bool filled);
    const char* formatDate(std::time_t t);
}
