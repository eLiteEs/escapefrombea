#include "credits.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {
    const Color COL_TITLE     = { 255, 255, 255, 255 };
    const Color COL_SUBTITLE  = { 255, 210, 100, 255 };
    const Color COL_BULLET    = { 200, 200, 200, 255 };
    const Color COL_TEXT      = { 160, 160, 160, 255 };
    const Color COL_HINT      = { 120, 120, 120, 255 };
    const Color COL_BG        = { 0, 0, 0, 255 };
}

bool CreditsScreen::load(const char* path) {
    lines_.clear();
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char raw[1024];
    while (fgets(raw, sizeof(raw), f)) {
        std::string line = raw;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        if (line.empty()) {
            CreditsLine cl;
            cl.type = CreditsLine::CL_BLANK;
            lines_.push_back(cl);
            continue;
        }

        CreditsLine cl;
        if (line[0] == '#') {
            cl.type = CreditsLine::CL_SECTION;
            cl.text = line.substr(1);
            while (!cl.text.empty() && cl.text.front() == ' ') cl.text.erase(cl.text.begin());
        } else if (line[0] == '-') {
            cl.type = CreditsLine::CL_SUBSECTION;
            cl.text = line.substr(1);
            while (!cl.text.empty() && cl.text.front() == ' ') cl.text.erase(cl.text.begin());
        } else if (line[0] == '*') {
            cl.type = CreditsLine::CL_BULLET;
            cl.text = line.substr(1);
            while (!cl.text.empty() && cl.text.front() == ' ') cl.text.erase(cl.text.begin());
        } else {
            cl.type = CreditsLine::CL_PARAGRAPH;
            cl.text = line;
        }
        lines_.push_back(cl);
    }
    fclose(f);
    return !lines_.empty();
}

void CreditsScreen::start() {
    scrollY  = 0.0f;
    fadeIn   = 0.0f;
    endWait  = 0.0f;
    active   = true;
    finished = false;
}

float CreditsScreen::totalHeight(int H) const {
    // Calcula la altura total que ocupan todas las lineas
    int fsize = std::max(20, H / 40);
    float y = 0.0f;
    for (const auto& l : lines_) {
        switch (l.type) {
            case CreditsLine::CL_SECTION:    y += fsize * 1.8f; break;
            case CreditsLine::CL_SUBSECTION: y += fsize * 1.3f; break;
            case CreditsLine::CL_BULLET:     y += fsize * 1.0f; break;
            case CreditsLine::CL_PARAGRAPH:       y += fsize * 1.0f; break;
            case CreditsLine::CL_BLANK:      y += fsize * 0.7f; break;
        }
    }
    return y + H;   // margen extra al final
}

void CreditsScreen::update(float dt, bool advancePressed, bool backPressed) {
    if (!active) return;

    if (fadeIn < 1.0f) {
        fadeIn += dt * 1.5f;
        if (fadeIn > 1.0f) fadeIn = 1.0f;
    }

    int H = GetScreenHeight();
    float speed = 40.0f;   // pixeles por segundo
    float maxScroll = totalHeight(H) - (float)H;

    if (scrollY < maxScroll) {
        scrollY += speed * dt;
        if (scrollY > maxScroll) scrollY = maxScroll;
    } else {
        endWait += dt;
    }

    // Avance manual
    if (advancePressed) {
        if (scrollY < maxScroll - 100.0f) {
            scrollY = std::min(scrollY + 300.0f, maxScroll);
        } else {
            scrollY = maxScroll;
            endWait = 3.0f;   // fuerza fin
        }
    }

    // Backspace/ESC/click para salir
    if (backPressed || endWait > 3.0f) {
        active = false;
        finished = true;
    }
}

void CreditsScreen::draw(int W, int H) {
    if (!active) return;

    unsigned char alpha = (unsigned char)(fadeIn * 255.0f);

    // Fondo
    DrawRectangle(0, 0, W, H, (Color){ COL_BG.r, COL_BG.g, COL_BG.b, alpha });

    // Layout escalado por GUI scale
    int fsize = std::max(20, H / 40);

    float y = (float)H - scrollY;
    float cx = (float)W * 0.5f;

    for (const auto& l : lines_) {
        switch (l.type) {
            case CreditsLine::CL_SECTION: {
                int size = fsize * 2;
                int tw = MeasureText(l.text.c_str(), size);
                Color c = COL_TITLE; c.a = alpha;
                DrawText(l.text.c_str(), (int)(cx - tw / 2), (int)y, size, c);
                y += fsize * 1.8f;
                break;
            }
            case CreditsLine::CL_SUBSECTION: {
                int size = (int)(fsize * 1.2f);
                int tw = MeasureText(l.text.c_str(), size);
                Color c = COL_SUBTITLE; c.a = alpha;
                DrawText(l.text.c_str(), (int)(cx - tw / 2), (int)y, size, c);
                y += fsize * 1.3f;
                break;
            }
            case CreditsLine::CL_BULLET: {
                int size = fsize;
                int tw = MeasureText(l.text.c_str(), size);
                Color c = COL_BULLET; c.a = alpha;
                DrawText(l.text.c_str(), (int)(cx - tw / 2), (int)y, size, c);
                y += fsize * 1.0f;
                break;
            }
            case CreditsLine::CL_PARAGRAPH: {
                int size = fsize;
                int tw = MeasureText(l.text.c_str(), size);
                Color c = COL_TEXT; c.a = alpha;
                DrawText(l.text.c_str(), (int)(cx - tw / 2), (int)y, size, c);
                y += fsize * 1.0f;
                break;
            }
            case CreditsLine::CL_BLANK:
                y += fsize * 0.7f;
                break;
        }
    }

    // Degradados arriba y abajo para no cortar en seco
    DrawRectangleGradientV(0, 0, W, H / 6,
                           (Color){ 0, 0, 0, alpha },
                           (Color){ 0, 0, 0, 0 });
    DrawRectangleGradientV(0, H - H / 6, W, H / 6,
                           (Color){ 0, 0, 0, 0 },
                           (Color){ 0, 0, 0, alpha });

    // Hint "pulsa para salir" siempre al pie
    if (fadeIn > 0.8f) {
        const char* hint = "ESC / click para salir";
        int hs = fsize * 3 / 5;
        int hw = MeasureText(hint, hs);
        DrawText(hint, W / 2 - hw / 2, H - hs - 16, hs, COL_HINT);
    }
}

