#pragma once
#include "common.h"

namespace UI {
    extern float scale;       // = guiScale actual, se setea 1 vez por frame
    extern int   colorblind;

    inline int  S(int base)  { return (int)(base * scale); }
    inline void T(const char* t, int x, int y, int size, Color c) {
        DrawText(t, x, y, S(size), c);
    }
    inline int  M(const char* t, int size) { return MeasureText(t, S(size)); }

    // Color adaptado a daltonismo
    Color PlayerColor(int idx);
    Color ExitColor();
    Color DangerColor();
    Color OkColor();
}

enum UIAction { UI_NONE, UI_LEFT, UI_RIGHT, UI_CONFIRM };

bool uiButton(int idx, int count, int yOffset, const char* label,
              int* navIndex, bool* confirmPressed);

UIAction uiOptionEx(int idx, int count, int yOffset,
                    const char* name, const char* value,
                    int* navIndex, bool* confirmPressed,
                    int leftRightInput = 0);

void drawBackground2D();
void drawCenterPanel(const char* title, const char* subtitle, const char* footer);
void drawConfigHeader(const char* title);

