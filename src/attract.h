#pragma once
#include "common.h"
#include "maze.h"
#include "assets.h"
#include "world_mesh.h"
#include <random>

class AttractMode {
public:
    void init(int size, uint32_t seed, const Assets& assets);
    void update(float dt);
    void draw(int W, int H);
    void unload();

private:
    Maze        maze_;
    WorldMeshes world_;
    Vector3     pos_       = { 0, 0, 0 };
    float       yaw_       = 0.0f;
    float       targetYaw_ = 0.0f;
    float       dirTimer_  = 0.0f;
    float       pickCd_    = 0.0f;
    float       bobT_      = 0.0f;
    std::mt19937 rng_;

    void pickNewDirection();
};
