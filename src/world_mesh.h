#pragma once
#include "raylib.h"
#include "maze.h"
#include "assets.h"

struct WorldMeshes {
    Model floorModel      = {};
    Model wallModelTile   = {};
    Model wallModelLocker = {};
    Model wallModelBrick  = {};

    bool built = false;

    void build(const Maze& maze, const Assets& assets);
    void unload();
    void draw(bool textured) const;

private:
    static Model makeWallModel(float width, float height, Texture2D tex, bool hasTex);
    static Model makeFloorModel(float w, float h, Texture2D tex, bool hasTex);
};
