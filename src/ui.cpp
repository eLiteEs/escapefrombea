#include "ui.h"
#include <cstdio>

namespace UI {
    float scale = 1.0f;
    int   colorblind = CB_NONE;

    Color PlayerColor(int idx) {
        if (colorblind == CB_NONE)     return idx == 0 ? GOLD : SKYBLUE;
        if (colorblind == CB_DEUTERANOPIA) return idx == 0 ? (Color){255,160,0,255}  : (Color){0,140,255,255};
        if (colorblind == CB_PROTANOPIA)   return idx == 0 ? (Color){255,220,0,255}  : (Color){60,60,255,255};
        /* CB_TRITANOPIA */                 return idx == 0 ? (Color){255,80,180,255} : (Color){80,255,220,255};
    }
    Color ExitColor()   { return colorblind == CB_NONE ? GREEN : (Color){80,200,255,255}; }
    Color DangerColor() { return colorblind == CB_NONE ? RED   : (Color){255,120,0,255}; }
    Color OkColor()     { return colorblind == CB_NONE ? GREEN : (Color){80,200,255,255}; }
}

bool uiButton(int idx, int count, int yOffset, const char* label,
              int* navIndex, bool* confirmPressed)
{
    int w = 460, h = 60, gap = 22;
    int totalH = count * (h + gap) - gap;
    int x = GetScreenWidth()/2 - w/2;
    int y = GetScreenHeight()/2 - totalH/2 + yOffset + idx * (h + gap);

    Rectangle r = { (float)x, (float)y, (float)w, (float)h };
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool selected = navIndex && *navIndex == idx;
    bool kb = selected && confirmPressed && *confirmPressed;
    if (kb) *confirmPressed = false;

    Color bg = (hover || selected) ? (Color){ 70, 80, 120, 255 }
                                   : (Color){ 40, 45, 65, 255 };
    Color border = (hover || selected) ? SKYBLUE : (Color){ 90, 100, 140, 255 };
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, border);

    int fs = 26;
    int tw = UI::M(label, fs);
    if (tw > w - 20) { fs = 20; tw = UI::M(label, fs); }
    UI::T(label, x + (w - tw)/2, y + (h - fs)/2, fs,
             (hover || selected) ? WHITE : RAYWHITE);
    return click || kb;
}

UIAction uiOptionEx(int idx, int count, int yOffset,
                    const char* name, const char* value,
                    int* navIndex, bool* confirmPressed,
                    int leftRightInput)
{
    int w = 680, h = 58, gap = 18;
    int totalH = count * (h + gap) - gap;
    int x = GetScreenWidth()/2 - w/2;
    int y = GetScreenHeight()/2 - totalH/2 + yOffset + idx * (h + gap);

    Rectangle r = { (float)x, (float)y, (float)w, (float)h };
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, r);
    bool selected = navIndex && *navIndex == idx;

    UIAction result = UI_NONE;

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        result = (mp.x < r.x + r.width * 0.5f) ? UI_LEFT : UI_RIGHT;
    }
    if (selected) {
        if (confirmPressed && *confirmPressed) {
            *confirmPressed = false;
            if (result == UI_NONE) result = UI_CONFIRM;
        }
        if (leftRightInput < 0 && result == UI_NONE) result = UI_LEFT;
        if (leftRightInput > 0 && result == UI_NONE) result = UI_RIGHT;
    }

    Color bg = (hover || selected) ? (Color){ 55, 62, 90, 255 }
                                   : (Color){ 35, 40, 58, 255 };
    Color border = (hover || selected) ? SKYBLUE : (Color){ 80, 90, 125, 255 };
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, border);

    UI::T(name, x + 22, y + (h - 24)/2, 24, RAYWHITE);
    if (value && value[0] != '\0') {
        char buf[128];
        snprintf(buf, sizeof(buf), "< %s >", value);
        int vw = UI::M(buf, 24);
        UI::T(buf, x + w - vw - 22, y + (h - 24)/2, 24, GOLD);
    }
    return result;
}

void drawBackground2D() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    for (int y = 0; y < H; y += 4) {
        unsigned char r = (unsigned char)(12 + 22 * y / H);
        unsigned char g = (unsigned char)(10 + 12 * y / H);
        unsigned char b = (unsigned char)(22 + 34 * y / H);
        DrawRectangle(0, y, W, 4, (Color){ r, g, b, 255 });
    }
}

void drawCenterPanel(const char* title, const char* subtitle, const char* footer) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, (Color){ 0, 0, 0, 180 });
    int tw = UI::M(title, 64);
    UI::T(title, W/2 - tw/2, H/2 - 80, 64, RAYWHITE);
    int sw = UI::M(subtitle, 24);
    UI::T(subtitle, W/2 - sw/2, H/2 + 10, 24, LIGHTGRAY);
    int fw = UI::M(footer, 20);
    UI::T(footer, W/2 - fw/2, H/2 + 70, 20, GOLD);
}

void drawConfigHeader(const char* title) {
    int W = GetScreenWidth();
    UI::T(title, W/2 - UI::M(title, 48)/2, 60, 48, RAYWHITE);
    const char* back = "ESC: volver";
    UI::T(back, W - UI::M(back, 18) - 20, 20, 18, GRAY);
}

