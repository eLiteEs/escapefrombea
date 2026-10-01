#include "cinematic.h"

#include "ui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace {
    const Color COL_SPEAKER   = { 255, 210, 100, 255 };
    const Color COL_TEXT      = { 240, 240, 240, 255 };
    const Color COL_NARRATION = { 200, 200, 200, 255 };
    const Color COL_BAR       = { 0, 0, 0, 235 };
}

bool Cinematic::load(const char* path) {
    lines_.clear();
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char raw[1024];
    float curSpeed = 45.0f;
    float curWait  = 0.0f;
    std::string curMusic;

    while (fgets(raw, sizeof(raw), f)) {
        std::string line = raw;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '@') {
            char cmd[32] = { 0 };
            char arg[256] = { 0 };
            sscanf(line.c_str(), "@%31s %255[^\n]", cmd, arg);
            if      (!strcmp(cmd, "music")) curMusic = arg;
            else if (!strcmp(cmd, "wait"))  curWait  = (float)atof(arg);
            else if (!strcmp(cmd, "speed")) curSpeed = (float)atof(arg);
            continue;
        }

        CinematicLine cl;
        cl.waitBefore = curWait;
        cl.speed      = curSpeed;
        cl.music      = curMusic;
        curWait  = 0.0f;
        curMusic.clear();

        size_t sep = line.find('|');
        if (sep != std::string::npos) {
            cl.type    = CinematicLine::SPEECH;
            cl.speaker = line.substr(0, sep);
            cl.text    = line.substr(sep + 1);
            while (!cl.speaker.empty() && cl.speaker.back() == ' ') cl.speaker.pop_back();
            while (!cl.text.empty()    && cl.text.front() == ' ')    cl.text.erase(cl.text.begin());
        } else {
            cl.type = CinematicLine::NARRATION;
            cl.text = line;
        }
        lines_.push_back(cl);
    }
    fclose(f);
    return !lines_.empty();
}

void Cinematic::start() {
    idx_       = 0;
    typing_    = 0.0f;
    wait_      = 0.0f;
    fade_      = 0.0f;
    lineReady_ = false;
    waiting_   = false;
    active     = true;
    finished   = false;
    pendingMusic_.clear();
    beginLine();
}

void Cinematic::beginLine() {
    if (idx_ >= lines_.size()) {
        active   = false;
        finished = true;
        return;
    }
    wait_      = lines_[idx_].waitBefore;
    waiting_   = (wait_ > 0.0f);
    typing_    = 0.0f;
    fade_      = 0.0f;
    lineReady_ = false;

    if (!lines_[idx_].music.empty())
        pendingMusic_ = lines_[idx_].music;
}

void Cinematic::update(float dt, bool advancePressed) {
    if (!active) return;
    if (idx_ >= lines_.size()) { active = false; finished = true; return; }

    const CinematicLine& cl = lines_[idx_];

    if (waiting_) {
        wait_ -= dt;
        if (wait_ <= 0.0f) waiting_ = false;
        else return;
    }

    if (fade_ < 1.0f) {
        fade_ += dt * 3.0f;
        if (fade_ > 1.0f) fade_ = 1.0f;
    }

    float totalChars = (float)cl.text.size();
    if (cl.speed <= 0.0f) {
        typing_ = totalChars;
        lineReady_ = true;
    } else {
        typing_ += dt * cl.speed;
        if (typing_ >= totalChars) {
            typing_ = totalChars;
            lineReady_ = true;
        }
    }

    if (advancePressed) {
        if (!lineReady_) {
            typing_    = totalChars;
            lineReady_ = true;
        } else {
            ++idx_;
            beginLine();
        }
    }
}

void Cinematic::drawWrappedText(const std::string& text, int x, int y,
                                int maxW, int fontSize, Color color) const
{
    int lineH = fontSize + 6;
    int cx = x;
    int cy = y;
    std::string word;
    auto flush = [&](bool last) {
        if (word.empty()) return;
        int w = UI::M(word.c_str(), fontSize);
        if (cx + w > x + maxW && cx > x) {
            cx = x;
            cy += lineH;
        }
	UI::T(word.c_str(), cx, cy, fontSize, color);
        cx += w;
        if (!last) cx += UI::M(" ", fontSize);
        word.clear();
    };
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == ' ') flush(false);
        else                word += text[i];
    }
    flush(true);
}

void Cinematic::draw(int W, int H) const {
    if (!active && !finished) return;
    if (idx_ >= lines_.size()) return;

    const CinematicLine& cl = lines_[idx_];

    int barH = std::max(60, H / 8);
    DrawRectangle(0, 0, W, barH, COL_BAR);
    DrawRectangle(0, H - barH, W, barH, COL_BAR);

    int textBoxH = barH + 40;
    DrawRectangleGradientV(0, H - textBoxH - 20, W, textBoxH + 20,
                           (Color){ 0, 0, 0, 0 }, (Color){ 0, 0, 0, 180 });

    size_t shown = (size_t)typing_;
    if (shown > cl.text.size()) shown = cl.text.size();
    std::string visible = cl.text.substr(0, shown);

    unsigned char alpha = (unsigned char)(fade_ * 255.0f);
    int margin = std::max(40, W / 10);
    int maxW   = W - margin * 2;

    if (cl.type == CinematicLine::SPEECH) {
        int nameSize = std::max(20, H / 32);
        int nameY    = H - barH - 30 - nameSize;
        Color nc = COL_SPEAKER; nc.a = alpha;
	UI::T(cl.speaker.c_str(), margin, nameY, nameSize, nc);
        int textSize = std::max(20, H / 34);
        Color tc = COL_TEXT; tc.a = alpha;
        drawWrappedText(visible, margin, H - barH - 15, maxW, textSize, tc);
    } else {
        int textSize = std::max(20, H / 34);
        Color tc = COL_NARRATION; tc.a = alpha;
        int txtW = UI::M(visible.c_str(), textSize);
        int tx   = W / 2 - txtW / 2;
        if (txtW > maxW) tx = margin;
        drawWrappedText(visible, tx, H - barH - 15, maxW, textSize, tc);
    }

    if (lineReady_ && ((int)(GetTime() * 2.0) & 1)) {
        const char* hint = "Pulsa ENTER / A para continuar";
        int hs = 16;
	UI::T(hint, W - UI::M(hint, hs) - 24, H - barH / 2 - hs / 2, hs,
                 (Color){ 200, 200, 200, 200 });
    }
}

