#pragma once
#include "raylib.h"
#include <string>
#include <vector>

struct CinematicLine {
    enum Type { NARRATION, SPEECH };
    Type type = NARRATION;
    std::string speaker;
    std::string text;
    float waitBefore = 0.0f;
    float speed      = 45.0f;
    std::string music;
};

class Cinematic {
public:
    bool load(const char* path);
    void start();
    void update(float dt, bool advancePressed);
    void draw(int W, int H) const;

    bool isActive()    const { return active; }
    bool isFinished()  const { return finished; }
    void stop()              { active = false; finished = true; }

    const std::string& pendingMusic() const { return pendingMusic_; }
    void clearPendingMusic() { pendingMusic_.clear(); }

private:
    std::vector<CinematicLine> lines_;
    size_t  idx_        = 0;
    float   typing_     = 0.0f;
    float   wait_       = 0.0f;
    float   fade_       = 0.0f;
    bool    lineReady_  = false;
    bool    waiting_    = false;
    bool    active      = false;
    bool    finished    = false;
    std::string pendingMusic_;

    void beginLine();
    void drawWrappedText(const std::string& text, int x, int y,
                         int maxW, int fontSize, Color color) const;
};

