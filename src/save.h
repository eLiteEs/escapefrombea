#pragma once
#include "common.h"
#include <ctime>
#include <string>

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

private:
  SaveSlot slots_[NUM_SLOTS];
  std::string slotPath(int i) const;
};
