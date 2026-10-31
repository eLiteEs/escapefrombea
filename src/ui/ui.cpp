#include "ui.hpp"
#include <cmath>
#include <cstdio>
#include <string>

namespace UI {

float scale = 1.0f;
int colorblind = CB_NONE;
Font font = {};
Texture2D skullTex = {};
Texture2D arrowLeftTex = {};
Texture2D arrowRightTex = {};
Texture2D healthTex = {};
Texture2D armorTex = {};
bool hasSkullTex = false;
bool hasArrowLeftTex = false;
bool hasArrowRightTex = false;
bool hasHealthTex = false;
bool hasArmorTex = false;

bool scrollActive = false;
int scrollOffsetY = 0;
int scrollViewportTop = 0;
int scrollViewportBottom = 0;
static int s_scrollContentH = 0;
static int s_scrollViewportH = 0;

void T(const char *t, int x, int y, int size, Color c) {
  DrawTextEx(font, t, {(float)x, (float)y}, (float)S(size), 1.0f, c);
}
int M(const char *t, int size) {
  Vector2 v = MeasureTextEx(font, t, (float)S(size), 1.0f);
  return (int)v.x;
}

Color PlayerColor(int idx) {
  if (colorblind == CB_NONE)
    return idx == 0 ? GOLD : SKYBLUE;
  if (colorblind == CB_DEUTERANOPIA)
    return idx == 0 ? (Color){255, 160, 0, 255} : (Color){0, 140, 255, 255};
  if (colorblind == CB_PROTANOPIA)
    return idx == 0 ? (Color){255, 220, 0, 255} : (Color){60, 60, 255, 255};
  return idx == 0 ? (Color){255, 80, 180, 255} : (Color){80, 255, 220, 255};
}
Color ExitColor() {
  return colorblind == CB_NONE ? GREEN : (Color){80, 200, 255, 255};
}
Color DangerColor() {
  return colorblind == CB_NONE ? RED : (Color){255, 120, 0, 255};
}
Color OkColor() {
  return colorblind == CB_NONE ? GREEN : (Color){80, 200, 255, 255};
}

// -------------------------------------------------------------------------
//  Calavera. cx/cy son píxeles de pantalla; radius es lógico y se escala.
// -------------------------------------------------------------------------
void drawSkull(float cx, float cy, float radiusLogical, Color color) {
  float r = (float)S((int)radiusLogical);

  if (hasSkullTex && skullTex.id != 0) {
    float sz = r * 2.0f;
    DrawTexturePro(
        skullTex, {0, 0, (float)skullTex.width, (float)skullTex.height},
        {cx - sz * 0.5f, cy - sz * 0.5f, sz, sz}, {0, 0}, 0.0f, color);
    return;
  }

  // Fallback procedural
  Color hole = {20, 20, 25, 255};
  DrawCircle((int)cx, (int)cy, r, color);
  DrawRectangle((int)(cx - r * 0.5f), (int)(cy + r * 0.5f), (int)(r * 1.0f),
                (int)(r * 0.5f), color);
  DrawCircle((int)(cx - r * 0.4f), (int)(cy - r * 0.1f), r * 0.28f, hole);
  DrawCircle((int)(cx + r * 0.4f), (int)(cy - r * 0.1f), r * 0.28f, hole);
  DrawTriangle({cx + r * 0.12f, cy + r * 0.55f},
               {cx - r * 0.12f, cy + r * 0.55f}, {cx, cy + r * 0.15f}, hole);
  for (int i = -1; i <= 1; ++i)
    DrawRectangle((int)(cx + i * r * 0.3f - 1), (int)(cy + r * 0.65f), 2,
                  (int)(r * 0.35f), hole);
}

} // namespace UI

// -------------------------------------------------------------------------
//  Botón de menú
// -------------------------------------------------------------------------
bool uiButton(int idx, int count, int yOffset, const char *label,
              int *navIndex, bool *confirmPressed) {
  int w = UI::S(460);
  int h = UI::S(60);
  int gap = UI::S(22);
  int totalH = count * (h + gap) - gap;
  int x = GetScreenWidth() / 2 - w / 2;

  int y;
  if (UI::scrollActive) {
    y = UI::scrollViewportTop + UI::S(yOffset) + idx * (h + gap)
    + UI::scrollOffsetY;
  } else {
    y = GetScreenHeight() / 2 - totalH / 2 + UI::S(yOffset) + idx * (h + gap);
  }

  // Si está fuera del viewport cuando hay scroll, no dibujar ni interactuar
  if (UI::scrollActive &&
    (y + h <= UI::scrollViewportTop || y >= UI::scrollViewportBottom))
    return false;

  Rectangle r = {(float)x, (float)y, (float)w, (float)h};
  Vector2 mp = GetMousePosition();
  bool hover = CheckCollisionPointRec(mp, r);
  bool click = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

  Vector2 md = GetMouseDelta();
  bool mouseMoved = (fabsf(md.x) > 0.1f || fabsf(md.y) > 0.1f);
  if (hover && mouseMoved && navIndex)
    *navIndex = idx;

  bool selected = navIndex && *navIndex == idx;
  bool kb = selected && confirmPressed && *confirmPressed;
  if (kb)
    *confirmPressed = false;

  if (selected)
    DrawRectangleRec(r, UI::BG_FOCUS);
  else if (hover)
    DrawRectangleRec(r, UI::BG_HOVER);
  else
    DrawRectangleRec(r, UI::BG_PANEL);

  if (selected)
    DrawRectangle(x + UI::S(2), y + UI::S(8), UI::S(3), h - UI::S(16),
                  UI::ACCENT);

  int fs = 22;
  int tw = UI::M(label, fs);
  if (tw > w - UI::S(96)) {
    fs = 18;
    tw = UI::M(label, fs);
  }
  Color textCol = (selected || hover) ? UI::TEXT_PRI : UI::TEXT_SEC;
  UI::T(label, x + UI::S(32), y + (h - UI::S(fs)) / 2, fs, textCol);

  if (selected) {
    float skullRadius = 12.0f;
    float skullX = (float)(x + w - UI::S(34));
    float skullY = (float)(y + h / 2);
    UI::drawSkull(skullX, skullY, skullRadius, UI::ACCENT);
  }

  return click || kb;
}

// -------------------------------------------------------------------------
//  Botón de flecha (pill + icono). r ya está en píxeles de pantalla.
// -------------------------------------------------------------------------
static void drawArrowButton(Rectangle r, bool left, bool hover, bool focused) {
  float bgAlpha = 0.0f;
  if (focused)
    bgAlpha = hover ? 0.30f : 0.15f;
  else if (hover)
    bgAlpha = 0.10f;

  if (bgAlpha > 0.0f) {
    Color pill = {UI::ACCENT.r, UI::ACCENT.g, UI::ACCENT.b,
                  (unsigned char)(bgAlpha * 255)};
    DrawRectangleRounded(r, 0.5f, 8, pill);
  }

  Color arrowColor;
  if (focused)
    arrowColor = hover ? UI::ACCENT : UI::ACCENT_DIM;
  else if (hover)
    arrowColor = UI::TEXT_SEC;
  else
    arrowColor = UI::TEXT_MUT;

  float cx = r.x + r.width * 0.5f;
  float cy = r.y + r.height * 0.5f;

  Texture2D *tex = left ? &UI::arrowLeftTex : &UI::arrowRightTex;
  bool hasTex = left ? UI::hasArrowLeftTex : UI::hasArrowRightTex;

  if (hasTex && tex->id != 0) {
    float sz = std::min(r.width, r.height) * 0.55f;
    DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height},
                   {cx - sz * 0.5f, cy - sz * 0.5f, sz, sz}, {0, 0}, 0.0f,
                   arrowColor);
  } else {
    float sz = std::min(r.width, r.height) * 0.22f;
    Vector2 v1, v2, v3;
    if (left) {
      v1 = {cx - sz * 0.6f, cy};
      v2 = {cx + sz * 0.6f, cy - sz};
      v3 = {cx + sz * 0.6f, cy + sz};
    } else {
      v1 = {cx + sz * 0.6f, cy};
      v2 = {cx - sz * 0.6f, cy - sz};
      v3 = {cx - sz * 0.6f, cy + sz};
    }
    DrawTriangle(v1, v2, v3, arrowColor);
    DrawTriangle(v1, v3, v2, arrowColor);
  }
}

// -------------------------------------------------------------------------
//  Fila de opción
// -------------------------------------------------------------------------
UIAction uiOptionEx(int idx, int count, int yOffset, const char *name,
                    const char *value, int *navIndex, bool *confirmPressed,
                    int leftRightInput) {
  int w = UI::S(680);
  int h = UI::S(58);
  int gap = UI::S(18);
  int totalH = count * (h + gap) - gap;
  int x = GetScreenWidth() / 2 - w / 2;

  int y;
  if (UI::scrollActive) {
    y = UI::scrollViewportTop + UI::S(yOffset) + idx * (h + gap)
    + UI::scrollOffsetY;
  } else {
    y = GetScreenHeight() / 2 - totalH / 2 + UI::S(yOffset) + idx * (h + gap);
  }

  if (UI::scrollActive &&
    (y + h <= UI::scrollViewportTop || y >= UI::scrollViewportBottom))
    return UI_NONE;

  Rectangle r = {(float)x, (float)y, (float)w, (float)h};
  Vector2 mp = GetMousePosition();
  bool hover = CheckCollisionPointRec(mp, r);

  Vector2 md = GetMouseDelta();
  bool mouseMoved = (fabsf(md.x) > 0.1f || fabsf(md.y) > 0.1f);
  if (hover && mouseMoved && navIndex)
    *navIndex = idx;

  bool selected = navIndex && *navIndex == idx;

  // Geometría del control group
  const float arrowSz = (float)UI::S(28);
  const float arrowGap = (float)UI::S(10);
  const float valueW = (float)UI::S(160);
  const float rightMargin = (float)UI::S(20);

  float controlW = arrowSz * 2 + arrowGap * 2 + valueW;
  float controlX = r.x + r.width - controlW - rightMargin;
  float controlY = r.y + (r.height - arrowSz) * 0.5f;

  Rectangle leftArrowRect = {controlX, controlY, arrowSz, arrowSz};
  Rectangle valueRect = {controlX + arrowSz + arrowGap, controlY, valueW,
                         arrowSz};
  Rectangle rightArrowRect = {controlX + arrowSz + arrowGap + valueW + arrowGap,
                              controlY, arrowSz, arrowSz};

  bool hasValue = (value && value[0] != '\0');
  bool hoverLeft =
      hasValue && hover && CheckCollisionPointRec(mp, leftArrowRect);
  bool hoverRight =
      hasValue && hover && CheckCollisionPointRec(mp, rightArrowRect);

  UIAction result = UI_NONE;
  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (hasValue) {
      if (CheckCollisionPointRec(mp, leftArrowRect))
        result = UI_LEFT;
      else if (CheckCollisionPointRec(mp, rightArrowRect))
        result = UI_RIGHT;
      else
        result = (mp.x < r.x + r.width * 0.5f) ? UI_LEFT : UI_RIGHT;
    } else {
      result = UI_CONFIRM;
    }
  }
  if (selected) {
    if (confirmPressed && *confirmPressed) {
      *confirmPressed = false;
      if (result == UI_NONE)
        result = UI_CONFIRM;
    }
    if (leftRightInput < 0 && result == UI_NONE)
      result = UI_LEFT;
    if (leftRightInput > 0 && result == UI_NONE)
      result = UI_RIGHT;
  }

  if (selected)
    DrawRectangleRec(r, UI::BG_FOCUS);
  else if (hover)
    DrawRectangleRec(r, UI::BG_HOVER);
  else
    DrawRectangleRec(r, UI::BG_PANEL);
  if (selected)
    DrawRectangle(x + UI::S(2), y + UI::S(8), UI::S(3), h - UI::S(16),
                  UI::ACCENT);

  int fs = 20;
  UI::T(name, x + UI::S(32), y + (h - UI::S(fs)) / 2, fs,
        (selected || hover) ? UI::TEXT_PRI : UI::TEXT_SEC);

  if (hasValue) {
    drawArrowButton(leftArrowRect, true, hoverLeft, selected);

    int vFs = 18;
    int vw = UI::M(value, vFs);
    if (vw > (int)valueW - UI::S(10)) {
      vFs = 14;
      vw = UI::M(value, vFs);
    }
    int vx = (int)(valueRect.x + (valueW - vw) * 0.5f);
    int vy = (int)(valueRect.y + (arrowSz - UI::S(vFs)) * 0.5f);
    UI::T(value, vx, vy, vFs, selected ? UI::ACCENT : UI::TEXT_PRI);

    drawArrowButton(rightArrowRect, false, hoverRight, selected);
  }

  return result;
}

// -------------------------------------------------------------------------
//  Slider
// -------------------------------------------------------------------------
UIAction uiSlider(int idx, int count, int yOffset, const char *label,
                  float *value, float minV, float maxV, float step,
                  int *navIndex, bool *confirmPressed, int leftRightInput,
                  const char *suffix) {
  int w = UI::S(680);
  int h = UI::S(58);
  int gap = UI::S(18);
  int totalH = count * (h + gap) - gap;
  int x = GetScreenWidth() / 2 - w / 2;
  int y = GetScreenHeight() / 2 - totalH / 2 + UI::S(yOffset) + idx * (h + gap);

  Rectangle r = {(float)x, (float)y, (float)w, (float)h};
  Vector2 mp = GetMousePosition();
  bool hover = CheckCollisionPointRec(mp, r);

  Vector2 md = GetMouseDelta();
  bool mouseMoved = (fabsf(md.x) > 0.1f || fabsf(md.y) > 0.1f);
  if (hover && mouseMoved && navIndex)
    *navIndex = idx;

  bool selected = navIndex && *navIndex == idx;

  const int labelW = UI::S(260);
  const int trackX0 = x + UI::S(32) + labelW;
  const int trackX1 = x + w - UI::S(96);
  const int trackY = y + h / 2;
  const int trackH = UI::S(4);
  const int thumbR = UI::S(7);

  float t = (*value - minV) / (maxV - minV);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  int fillX = trackX0 + (int)((trackX1 - trackX0) * t);

  UIAction result = UI_NONE;

  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    float tx = (mp.x - trackX0) / (float)(trackX1 - trackX0);
    if (tx < 0.0f)
      tx = 0.0f;
    if (tx > 1.0f)
      tx = 1.0f;
    *value = minV + tx * (maxV - minV);
    *value = std::round((*value - minV) / step) * step + minV;
    if (*value < minV)
      *value = minV;
    if (*value > maxV)
      *value = maxV;
    result = UI_CONFIRM;
  }
  if (selected) {
    if (leftRightInput < 0) {
      *value -= step;
      if (*value < minV)
        *value = minV;
      result = UI_LEFT;
    }
    if (leftRightInput > 0) {
      *value += step;
      if (*value > maxV)
        *value = maxV;
      result = UI_RIGHT;
    }
    if (confirmPressed && *confirmPressed) {
      *confirmPressed = false;
      if (result == UI_NONE)
        result = UI_CONFIRM;
    }
  }

  if (selected)
    DrawRectangleRec(r, UI::BG_FOCUS);
  else if (hover)
    DrawRectangleRec(r, UI::BG_HOVER);
  else
    DrawRectangleRec(r, UI::BG_PANEL);
  if (selected)
    DrawRectangle(x + UI::S(2), y + UI::S(8), UI::S(3), h - UI::S(16),
                  UI::ACCENT);

  UI::T(label, x + UI::S(32), y + (h - UI::S(20)) / 2, 20,
        (selected || hover) ? UI::TEXT_PRI : UI::TEXT_SEC);

  DrawRectangle(trackX0, trackY - trackH / 2, trackX1 - trackX0, trackH,
                UI::BORDER);
  DrawRectangle(trackX0, trackY - trackH / 2, fillX - trackX0, trackH,
                selected ? UI::ACCENT : UI::TEXT_SEC);
  DrawCircle(fillX, trackY, thumbR, selected ? UI::ACCENT : UI::TEXT_PRI);

  char vbuf[32];
  if (minV >= 0.0f && maxV <= 1.0f)
    snprintf(vbuf, sizeof(vbuf), "%d%s", (int)(*value * 100.0f + 0.5f), suffix);
  else
    snprintf(vbuf, sizeof(vbuf), "%.2f%s", (double)*value, suffix);

  int vFs = 16;
  int vw = UI::M(vbuf, vFs);
  int vx = x + w - vw - UI::S(32);
  int vy = y + (h - UI::S(vFs)) / 2;
  UI::T(vbuf, vx, vy, vFs, selected ? UI::ACCENT : UI::TEXT_PRI);

  return result;
}

// -------------------------------------------------------------------------
//  Input de texto
// -------------------------------------------------------------------------
bool uiTextInput(int idx, int count, int yOffset, const char *label,
                 std::string &text, TextInputState &state, int maxLen,
                 int *navIndex, bool active) {
  int w = UI::S(680);
  int h = UI::S(52);
  int gap = UI::S(18);
  int totalH = count * (h + gap) - gap;
  int x = GetScreenWidth() / 2 - w / 2;
  int y = GetScreenHeight() / 2 - totalH / 2 + UI::S(yOffset) + idx * (h + gap);

  Rectangle r = {(float)x, (float)y, (float)w, (float)h};
  Vector2 mp = GetMousePosition();
  bool hover = CheckCollisionPointRec(mp, r);

  Vector2 md = GetMouseDelta();
  bool mouseMoved = (fabsf(md.x) > 0.1f || fabsf(md.y) > 0.1f);
  if (hover && mouseMoved && navIndex)
    *navIndex = idx;

  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    // Click en la mitad derecha del campo pone el cursor al final
    if (mp.x > r.x + (float)UI::S(320))
      state.cursorPos = (int)text.size();
    if (navIndex)
      *navIndex = idx;
  }

  bool changed = false;

  if (active) {
    state.blinkT += GetFrameTime();
    if (state.blinkT >= 1.0f)
      state.blinkT -= 1.0f;

    int k = GetCharPressed();
    while (k > 0) {
      if (k >= 32 && k < 127 && (int)text.size() < maxLen) {
        text.insert(text.begin() + state.cursorPos, (char)k);
        state.cursorPos++;
        changed = true;
      }
      k = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && state.cursorPos > 0) {
      text.erase(text.begin() + state.cursorPos - 1);
      state.cursorPos--;
      changed = true;
    }
    if (IsKeyPressed(KEY_DELETE) && state.cursorPos < (int)text.size()) {
      text.erase(text.begin() + state.cursorPos);
      changed = true;
    }
    if (IsKeyPressed(KEY_LEFT) && state.cursorPos > 0)
      state.cursorPos--;
    if (IsKeyPressed(KEY_RIGHT) && state.cursorPos < (int)text.size())
      state.cursorPos++;
    if (IsKeyPressed(KEY_HOME))
      state.cursorPos = 0;
    if (IsKeyPressed(KEY_END))
      state.cursorPos = (int)text.size();
  } else {
    state.blinkT = 0.0f;
    state.cursorPos = (int)text.size();
  }

  if (active)
    DrawRectangleRec(r, UI::BG_FOCUS);
  else if (hover)
    DrawRectangleRec(r, UI::BG_HOVER);
  else
    DrawRectangleRec(r, UI::BG_PANEL);
  if (active)
    DrawRectangle(x + UI::S(2), y + UI::S(8), UI::S(3), h - UI::S(16),
                  UI::ACCENT);

  UI::T(label, x + UI::S(32), y + (h - UI::S(20)) / 2, 20,
        active ? UI::TEXT_PRI : UI::TEXT_SEC);

  int fieldX = x + UI::S(260);
  int fieldFs = 20;
  int fieldY = y + (h - UI::S(fieldFs)) / 2;
  Color fieldColor =
      text.empty() ? UI::TEXT_MUT : (active ? UI::ACCENT : UI::TEXT_PRI);
  UI::T(text.c_str(), fieldX, fieldY, fieldFs, fieldColor);

  if (active && state.blinkT < 0.5f) {
    std::string before = text.substr(0, state.cursorPos);
    int cw = UI::M(before.c_str(), fieldFs);
    DrawRectangle(fieldX + cw, fieldY - UI::S(2), UI::S(2),
                  UI::S(fieldFs) + UI::S(4), UI::ACCENT);
  }

  return changed;
}

// -------------------------------------------------------------------------
//  Fondo
// -------------------------------------------------------------------------
void drawBackground2D() {
  int W = GetScreenWidth(), H = GetScreenHeight();
  DrawRectangleGradientV(0, 0, W, H, UI::BG_APP, UI::BG_APP_DK);
}

// -------------------------------------------------------------------------
//  Panel central
// -------------------------------------------------------------------------
void drawCenterPanel(const char *title, const char *subtitle,
                     const char *footer) {
  int W = GetScreenWidth(), H = GetScreenHeight();
  DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 190});

  int cardW = UI::S(640);
  int cardH = UI::S(280);
  int cx = W / 2 - cardW / 2;
  int cy = H / 2 - cardH / 2;

  DrawRectangle(cx, cy, cardW, cardH, UI::BG_CARD);
  DrawRectangleLinesEx({(float)cx, (float)cy, (float)cardW, (float)cardH}, 1,
                       UI::BORDER);
  DrawRectangle(cx, cy, cardW, UI::S(2), UI::ACCENT);

  int ty = cy + UI::S(56);
  int tFs = 44;
  int tw = UI::M(title, tFs);
  UI::T(title, W / 2 - tw / 2, ty, tFs, UI::TEXT_PRI);

  int sepW = UI::S(80);
  DrawRectangle(W / 2 - sepW / 2, ty + UI::S(tFs) + UI::S(18), sepW, UI::S(1),
                UI::ACCENT_DIM);

  int sy = ty + UI::S(tFs) + UI::S(42);
  int sFs = 18;
  int sw = UI::M(subtitle, sFs);
  UI::T(subtitle, W / 2 - sw / 2, sy, sFs, UI::TEXT_SEC);

  int fy = cy + cardH - UI::S(56);
  int fFs = 15;
  int fw = UI::M(footer, fFs);
  UI::T(footer, W / 2 - fw / 2, fy, fFs, UI::ACCENT_DIM);
}

// -------------------------------------------------------------------------
//  Cabecera de configuración
// -------------------------------------------------------------------------
void drawConfigHeader(const char *title) {
  int W = GetScreenWidth();
  int fs = 40;
  int tw = UI::M(title, fs);
  int ty = UI::S(55);
  UI::T(title, W / 2 - tw / 2, ty, fs, UI::TEXT_PRI);

  int sepW = tw + UI::S(60);
  if (sepW > W - UI::S(80))
    sepW = W - UI::S(80);
  DrawRectangle(W / 2 - sepW / 2, ty + UI::S(fs) + UI::S(14), sepW, UI::S(2),
                UI::ACCENT_SOFT);

  const char *hint = "ESC";
  int hs = 13;
  UI::T(hint, W - UI::M(hint, hs) - UI::S(24), UI::S(24), hs, UI::TEXT_MUT);
}

void UI::beginScroll(int items, int itemH, int itemGap, int viewportTop,
                     int viewportBottom, int navIndex) {
  scrollActive = false;
  scrollOffsetY = 0;
  scrollViewportTop = viewportTop;
  scrollViewportBottom = viewportBottom;

  int contentH = items * (itemH + itemGap) - itemGap;
  int viewportH = viewportBottom - viewportTop;

  if (contentH <= viewportH) {
    // Cabe entero: layout centrado como siempre
    return;
  }

  UI::scrollActive = true;
  s_scrollContentH = contentH;
  s_scrollViewportH = viewportH;

  // Rueda del ratón (una fila por notch)
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    int delta = (wheel > 0.0f) - (wheel < 0.0f);
    scrollOffsetY -= delta * (itemH + itemGap);
  }

  // Auto-scroll para que navIndex quede visible
  if (navIndex >= 0 && navIndex < items) {
    int itemTop = navIndex * (itemH + itemGap);
    int itemBottom = itemTop + itemH;
    if (itemTop + scrollOffsetY < 0) {
      scrollOffsetY = -itemTop;
    } else if (itemBottom + scrollOffsetY > viewportH) {
      scrollOffsetY = viewportH - itemBottom;
    }
  }

  // Clamp final
  int maxScroll = std::max(0, contentH - viewportH);
  if (scrollOffsetY > 0)
    scrollOffsetY = 0;
  if (scrollOffsetY < -maxScroll)
    scrollOffsetY = -maxScroll;

  BeginScissorMode(0, viewportTop, GetScreenWidth(), viewportH);
}

void UI::endScroll() {
  if (scrollActive)
    EndScissorMode();
  scrollActive = false;
  scrollOffsetY = 0;
}

void UI::drawScrollbar(int x, int viewportTop, int viewportBottom, int contentH,
                       int viewportH, int scrollY) {
  if (contentH <= viewportH)
    return;

  const int trackW = UI::S(4);
  const int thumbW = UI::S(4);
  const int trackX = x;
  const int trackH = viewportBottom - viewportTop;

  DrawRectangle(trackX, viewportTop, trackW, trackH, UI::BORDER);

  float ratio = (float)viewportH / (float)contentH;
  int thumbH = std::max(UI::S(20), (int)(trackH * ratio));
  int maxScroll = contentH - viewportH;
  float t = maxScroll > 0 ? (float)(-scrollY) / (float)maxScroll : 0.0f;
  int thumbY = viewportTop + (int)((trackH - thumbH) * t);

  DrawRectangle(trackX, thumbY, thumbW, thumbH, UI::ACCENT_DIM);
}
