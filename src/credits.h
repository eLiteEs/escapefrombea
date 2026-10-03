#pragma once
#include "raylib.h"
#include <string>
#include <vector>

struct CreditsLine {
    enum LineType {
        CL_SECTION,
        CL_SUBSECTION,
        CL_BULLET,
        CL_PARAGRAPH,
        CL_BLANK
    };
    LineType type = CL_PARAGRAPH;
    std::string text;
};

class CreditsScreen {
public:
    bool load(const char* path);
    void start();
    void update(float dt, bool advancePressed, bool backPressed);
    void draw(int W, int H);

    bool isActive()   const { return active; }
    bool isFinished() const { return finished; }
    void stop()             { active = false; finished = true; }

private:
    std::vector<CreditsLine> lines_;
    float scrollY  = 0.0f;
    float fadeIn   = 0.0f;
    bool  active   = false;
    bool  finished = false;
    float endWait  = 0.0f;

    float totalHeight(int H) const;
};
