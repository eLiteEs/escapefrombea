#pragma once

#include "common.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

// Content of save that is readed or written to the save file
struct SaveEntry {
  std::string id; // "save_<ts>_<rand>"
  std::string name = "New game";

  int level = 1;
  int maxLevel = 1;
  int mode = 0;
  int difficulty = 0;
  bool speedrun = false;
  bool twoPlayers = false;

  float playtime = 0.0f;
  std::time_t timestamp = 0;
  std::time_t created = 0;

  uint32_t runSeed = 0;
  int enemyMult = 2;
  int itemMult = 2;

  std::vector<float> speedrunSplits;

  struct InvSlotSaved {
    int kind = 0;
    int count = 0;
  };
  InvSlotSaved inventory[2][C::INV_SLOTS] = {};
  int selectedSlot[2] = {0, 0};
};

struct SpeedrunEntry {
  uint32_t seed = 0;
  int mode = 0;
  std::time_t date = 0;
  float total = 0.0f;
  std::vector<float> splits;
};

// Specific ghost at a time
struct GhostSample {
  int16_t x = 0;    // posición X en centímetros
  int16_t z = 0;    // posición Z en centímetros
  uint16_t yaw = 0; // ángulo normalizado: 0..65535 → 0..2π
};

// Specific ghost at a level
struct GhostLevel {
  float duration = 0.0f; // How long did the ghost take to complete a level
  std::vector<GhostSample> samples;
};

// Specific ghost to a whole run
struct GhostRun {
  uint32_t seed = 0;
  int mode = 0;
  std::vector<GhostLevel> levels;
};

class SaveManager {
public:
  void refresh(); // Reload saves
  const std::vector<SaveEntry> &entries() const { return entries_; } // Get all saves
  std::string create(const SaveEntry &proto); // Create a save
  std::string duplicate(const std::string &id); // Duplicate a save
  std::string recreate(const std::string &id); // Delete user data from a save
  bool remove(const std::string &id); // Delete a save
  bool save(const SaveEntry &e); // Save a save
  const SaveEntry *find(const std::string &id) const; // Search by id and read save file as a const
  SaveEntry *find(const std::string &id); // Search by id and read save file

  // Speedruns
  void loadSpeedruns();
  void saveSpeedruns();
  void addSpeedrun(const SpeedrunEntry &e);
  bool removeSpeedrunAt(int index);
  const std::vector<SpeedrunEntry> &speedruns() const { return speedruns_; }
  void bestSplitsFor(uint32_t seed, int mode, std::vector<float> &out) const;

  // Ghosts
  bool saveGhost(uint32_t seed, int mode, const GhostRun &g);
  bool loadGhost(uint32_t seed, int mode, GhostRun &out) const;
  bool deleteGhost(uint32_t seed, int mode);
  bool hasGhost(uint32_t seed, int mode) const;

private:
  std::vector<SaveEntry> entries_;
  std::vector<SpeedrunEntry> speedruns_;

  std::string savePath(const std::string &id) const;
  std::string indexPath() const;
  std::string newId() const;

  void loadIndex(std::vector<std::string> &out) const;
  void saveIndex(const std::vector<std::string> &ids) const;
};
