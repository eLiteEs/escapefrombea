#pragma once
#include "assets.h"
#include "common.h"
#include <cstdint>
#include <vector>

struct LevelExtras {
  std::vector<Vector3> spawns;  // celdas 's'
  std::vector<Vector3> items;   // celdas 'i'
  std::vector<Vector3> enemies; // celdas 'e'
  std::vector<Vector3> keys;    // celdas 'k'
  bool hasExit = false;         // celdas 'f'
  Vector3 exit = {0, 0, 0};
};

class Maze {
public:
  int w = 0, h = 0;
  std::vector<std::vector<int>> g;
  std::vector<std::vector<bool>> explored;

  std::vector<std::vector<unsigned char>> wallVariant;
  std::vector<std::vector<bool>> fountains;
  std::vector<std::vector<bool>> tents;

  // --- Paredes (física "real": jugador y proyectiles) ---
  bool wallAt(int cx, int cy) const;
  bool wallAtWorld(float x, float z) const;
  bool circleHitsWall(float x, float z, float r) const;

  // --- Colisión "bloqueante" (paredes + tiendas): enemigos ---
  bool hasTent(int cx, int cy) const;
  bool hasTentWorld(float x, float z) const;
  void placeTent(int cx, int cy);
  void removeTent(int cx, int cy);

  bool blockedAt(int cx, int cy) const;
  bool blockedAtWorld(float x, float z) const;
  bool circleHitsBlocked(float x, float z, float r) const;

  // --- Fuentes ---
  bool hasFountain(int cx, int cy) const;

  // --- Utilidades ---
  void generate(int mw, int mh, uint32_t seed);
  Vector3 cellCenter(int cx, int cy) const;
  Vector2 worldToCell(float x, float z) const;

  void markExplored(float wx, float wz, int radius);
  void markExploredCell(int cx, int cy, int radius);

  bool findPath(Vector2 fromCell, Vector2 toCell,
                std::vector<Vector2> &out) const;

  bool loadFromFile(const char *path, LevelExtras &extras);

  struct Chunk {
    Model wallsTile = {};   // variant 0
    Model wallsLocker = {}; // variant 1
    Model wallsBrick = {};  // variant 2
    Model floor = {};
    bool built = false;
  };

  std::vector<Chunk> chunks;
  int chunksW = 0, chunksH = 0;
  void buildChunks(Assets &assets);
  void unloadChunks();
};
