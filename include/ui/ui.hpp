#pragma once

#include "common.hpp"

#include <string>

namespace UI {
    extern float scale;
    extern int colorblind;
    extern Font font;

    // -------------------------------------------------------------------------
    //  Paleta. Un solo acento (ámbar cálido) usado con moderación.
    // -------------------------------------------------------------------------
    constexpr Color BG_APP     = {  8,  8, 12, 255 };
    constexpr Color BG_APP_DK  = {  4,  4,  6, 255 };
    constexpr Color BG_PANEL   = { 18, 18, 24, 255 };
    constexpr Color BG_HOVER   = { 26, 26, 34, 255 };
    constexpr Color BG_FOCUS   = { 34, 34, 44, 255 };
    constexpr Color BG_CARD    = { 22, 22, 30, 255 };
    constexpr Color BORDER     = { 38, 38, 48, 255 };
    constexpr Color ACCENT     = {220, 170, 90, 255 };
    constexpr Color ACCENT_DIM = {220, 170, 90, 140 };
    constexpr Color ACCENT_SOFT= {220, 170, 90,  70 };
    constexpr Color TEXT_PRI   = {230, 230, 235, 255 };
    constexpr Color TEXT_SEC   = {130, 130, 145, 255 };
    constexpr Color TEXT_MUT   = { 80,  80,  95, 255 };

    void T(const char *t, int x, int y, int size, Color c);
    int  M(const char *t, int size);

    inline int S(int base) { return (int)(base * scale); }

    Color PlayerColor(int idx);
    Color ExitColor();
    Color DangerColor();
    Color OkColor();

    // Icono de calavera (usado por uiButton al estar enfocado)
    void drawSkull(float cx, float cy, float size, Color color);

    extern Font      font;
    extern Texture2D skullTex;
    extern Texture2D arrowLeftTex;
    extern Texture2D arrowRightTex;
    extern Texture2D healthTex;
    extern Texture2D armorTex;
    extern bool      hasSkullTex;
    extern bool      hasArrowLeftTex;
    extern bool      hasArrowRightTex;
    extern bool      hasHealthTex;
    extern bool      hasArmorTex;
} // namespace UI

enum UIAction { UI_NONE, UI_LEFT, UI_RIGHT, UI_CONFIRM };

// -------------------------------------------------------------------------
//  Botón de menú
// -------------------------------------------------------------------------
bool uiButton(int idx, int count, int yOffset, const char *label,
              int *navIndex, bool *confirmPressed);

// -------------------------------------------------------------------------
//  Fila de opción (con valor que se cambia con < >)
// -------------------------------------------------------------------------
UIAction uiOptionEx(int idx, int count, int yOffset,
                    const char *name, const char *value,
                    int *navIndex, bool *confirmPressed,
                    int leftRightInput = 0);

// -------------------------------------------------------------------------
//  Slider (float). Con click en la barra pone el valor; con izq/der ajusta
//  por step. `suffix` se anexa al valor mostrado ("%", "x", etc.).
// -------------------------------------------------------------------------
UIAction uiSlider(int idx, int count, int yOffset,
                  const char *label, float *value,
                  float minV, float maxV, float step,
                  int *navIndex, bool *confirmPressed,
                  int leftRightInput = 0,
                  const char *suffix = "");

// -------------------------------------------------------------------------
//  Input de texto. Solo procesa el teclado si `active == true`.
//  Mantiene cursor + parpadeo en `state`. Retorna true si el texto cambió.
// -------------------------------------------------------------------------
struct TextInputState {
    int   cursorPos = 0;
    float blinkT    = 0.0f;
};

bool uiTextInput(int idx, int count, int yOffset,
                 const char *label, std::string &text,
                 TextInputState &state, int maxLen,
                 int *navIndex, bool active);

// -------------------------------------------------------------------------
//  Fondo y helpers de pantalla
// -------------------------------------------------------------------------
void drawBackground2D();
void drawCenterPanel(const char *title, const char *subtitle,
                     const char *footer);
void drawConfigHeader(const char *title);
