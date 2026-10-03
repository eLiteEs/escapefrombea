#pragma once
#include "common.h"
#include <vector>
#include <cstdint>

class Maze {
public:
    int w = 0, h = 0;
    std::vector<std::vector<int>>  g;
    std::vector<std::vector<bool>> explored;
    
    std::vector<std::vector<unsigned char>> wallVariant;
    std::vector<std::vector<bool>>          fountains;

    bool hasFountain(int cx, int cy) const {
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) return false;
        if ((int)fountains.size() != h) return false;
        return fountains[cy][cx];
    }

    void generate(int mw, int mh, uint32_t seed);

    bool wallAt(int cx, int cy) const;
    bool wallAtWorld(float x, float z) const;
    bool circleHitsWall(float x, float z, float r) const;
    Vector3 cellCenter(int cx, int cy) const;
    Vector2 worldToCell(float x, float z) const;

    void markExplored(float wx, float wz, int radius);
    void markExploredCell(int cx, int cy, int radius);

    bool findPath(Vector2 fromCell, Vector2 toCell,
                  std::vector<Vector2>& out) const;
};

