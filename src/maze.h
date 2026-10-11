#pragma once
#include "common.h"
#include <cstdint>
#include <vector>

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
};
