#pragma once
#include "common.h"
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

// -------------------------------------------------------------------------
//  SaveEntry — un mundo/partida guardada
// -------------------------------------------------------------------------
struct SaveEntry {
  std::string id; // "save_<ts>_<rand>"
  std::string name = "New world";

  int level = 1;
  int maxLevel = 1;
  int mode = 0;       // GameMode
  int difficulty = 0; // Diff
  bool speedrun = false;
  bool twoPlayers = false;

  float playtime = 0.0f;
  std::time_t timestamp = 0; // última modificación
  std::time_t created = 0;

  // Config de la run infinita
  uint32_t runSeed = 0;
  int enemyMult = 2; // índice en C::ENDLESS_MULT
  int itemMult = 2;

  // Splits de speedrun en curso
  std::vector<float> speedrunSplits;

  // Inventario por jugador
  struct InvSlotSaved {
    int kind = 0;
    int count = 0;
  };
  InvSlotSaved inventory[2][C::INV_SLOTS] = {};
  int selectedSlot[2] = {0, 0};
};

// -------------------------------------------------------------------------
//  SpeedrunEntry — una run terminada, en el historial global
// -------------------------------------------------------------------------
struct SpeedrunEntry {
  uint32_t seed = 0;
  int mode = 0;
  std::time_t date = 0;
  float total = 0.0f;
  std::vector<float> splits;
};

// -------------------------------------------------------------------------
//  SaveManager — CRUD + historial de speedruns
// -------------------------------------------------------------------------
class SaveManager {
public:
  // Carga el índice y todos los .sav, y el historial de speedruns.
  void refresh();

  const std::vector<SaveEntry> &entries() const { return entries_; }

  // --- CRUD ---
  // Crea un save a partir de un prototipo. Devuelve el id asignado
  // (o "" si falla). El nombre puede venir vacío; se respeta tal cual.
  std::string create(const SaveEntry &proto);

  // Duplica un save existente. Nuevo id, name + " (copy)".
  // Devuelve el id nuevo o "" si no existe el original.
  std::string duplicate(const std::string &id);

  // Reinicia el progreso de un save (nivel 1, inventario vacío,
  // splits limpios). Mantiene nombre, modo, dificultad, config.
  std::string recreate(const std::string &id);

  // Borra del índice y del disco. Devuelve true si existía.
  bool remove(const std::string &id);

  // Persiste los cambios de un save. Si no existe, lo añade.
  bool save(const SaveEntry &e);

  // Búsquedas por id.
  const SaveEntry *find(const std::string &id) const;
  SaveEntry *find(const std::string &id);

  // --- Historial de speedruns (global, independiente del save) ---
  void loadSpeedruns();
  void saveSpeedruns();
  void addSpeedrun(const SpeedrunEntry &e);

  const std::vector<SpeedrunEntry> &speedruns() const { return speedruns_; }

  // Rellena out con el mejor split por índice para (seed, mode).
  // Índices sin datos quedan a 0.0f.
  void bestSplitsFor(uint32_t seed, int mode, std::vector<float> &out) const;

private:
  std::vector<SaveEntry> entries_;
  std::vector<SpeedrunEntry> speedruns_;

  std::string savePath(const std::string &id) const;
  std::string indexPath() const;
  std::string newId() const;

  void loadIndex(std::vector<std::string> &out) const;
  void saveIndex(const std::vector<std::string> &ids) const;
};
