#pragma once
#include "assets.hpp"
#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct LevelExtras {
  std::vector<Vector3> spawns;
  std::vector<Vector3> items;
  std::vector<Vector3> enemies;
  std::vector<Vector3> keys;
  bool hasExit = false;
  Vector3 exit = {0, 0, 0};

  struct SignData {
    Vector3 pos;
    std::string text;
  };
  struct NpcData {
    Vector3 pos;
    std::string name;
    std::string text;
  };
  std::vector<SignData> signs;
  std::vector<NpcData> npcs;
};

class Maze {
public:
  int w = 0, h = 0;
  std::vector<std::vector<int>> g;
  std::vector<std::vector<bool>> explored;

  std::vector<std::vector<unsigned char>> wallVariant;
  std::vector<std::vector<bool>> fountains;
  std::vector<std::vector<bool>> tents;
  std::vector<std::vector<unsigned char>> heights; // 0..MAX_HEIGHT
  std::vector<std::vector<unsigned char>> stairs;  // 0=none 1=N 2=E 3=S 4=W

  // --- Paredes (física "real": jugador y proyectiles) ---
  bool wallAt(int cx, int cy) const;
  bool wallAtWorld(float x, float z) const;
  bool circleHitsWall(float x, float z, float r) const;
  bool circleHitsRiser(float x, float z, float r, float fromZ) const; // NUEVO

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

  int heightAt(int cx, int cy) const;
  int heightAtWorld(float x, float z) const;
  unsigned char stairAt(int cx, int cy) const;
  unsigned char stairAtWorld(float x, float z) const;
  float groundZAtWorld(float x, float z) const;
  float floorZAtWorld(float x, float z) const; // sin interpolación de rampa
};
