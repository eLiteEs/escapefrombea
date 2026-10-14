#pragma once
#include "common.h"
#include <ctime>
#include <string>
#include <vector>

struct SpeedrunEntry {
  uint32_t seed = 0;
  int mode = 0;
  std::time_t date = 0;
  float total = 0.0f;
  std::vector<float> splits;
};

struct SaveSlot {
  bool used = false;
  int level = 1;
  int maxLevel = 1;
  int mode = 0; // GameMode
  bool twoPlayers = false;
  float playtime = 0.0f;
  std::time_t timestamp = 0;

  struct InvSlotSaved {
    int kind = 0; // ItemKind como int (ITEM_NONE = 0)
    int count = 0;
  };
  InvSlotSaved inventory[2][C::INV_SLOTS] = {};
  int selectedSlot[2] = {0, 0};

  uint32_t runSeed = 0;
  int enemyMult = 2;
  int itemMult = 2;

  std::vector<float> speedrunSplits;
};

class SaveManager {
public:
  static constexpr int NUM_SLOTS = 3;

  void refresh();
  const SaveSlot &slot(int i) const { return slots_[i]; }

  bool saveSlot(int i, const SaveSlot &data);
  bool loadSlot(int i, SaveSlot &out) const;
  bool eraseSlot(int i);
  int firstFreeSlot() const;
  bool anyUsed() const;

  void loadSpeedruns();
  void saveSpeedruns();
  void addSpeedrun(const SpeedrunEntry &e);
  const std::vector<SpeedrunEntry> &speedruns() const { return speedruns_; }
  void bestSplitsFor(uint32_t seed, int mode,
                     std::vector<float> &outBest) const;

private:
  SaveSlot slots_[NUM_SLOTS];
  std::vector<SpeedrunEntry> speedruns_;
  std::string slotPath(int i) const;
};
