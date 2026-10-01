#pragma once
#include <string>
#include <ctime>

struct SaveSlot {
    bool   used = false;
    int    level = 1;
    int    maxLevel = 1;
    int    mode = 0;            // GameMode
    bool   twoPlayers = false;
    float  playtime = 0.0f;
    std::time_t timestamp = 0;
};

class SaveManager {
public:
    static constexpr int NUM_SLOTS = 3;

    void refresh();
    const SaveSlot& slot(int i) const { return slots_[i]; }

    bool saveSlot(int i, const SaveSlot& data);
    bool loadSlot(int i, SaveSlot& out) const;
    bool eraseSlot(int i);
    int  firstFreeSlot() const;
    bool anyUsed() const;

private:
    SaveSlot slots_[NUM_SLOTS];
    std::string slotPath(int i) const;
};
