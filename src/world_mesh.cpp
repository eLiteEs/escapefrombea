#include "world_mesh.h"
#include <cstring>

// Crea un modelo de cubo con la textura asignada.
// El cubo tiene dimensiones reales (width, height, width) para que las UVs
// [0,1] de cada cara no se estiren mas de lo necesario.
Model WorldMeshes::makeWallModel(float width, float height,
                                  Texture2D tex, bool hasTex)
{
    Mesh mesh = GenMeshCube(width, height, width);

    // Ajuste fino: las caras laterales tienen V vertical.
    // Si height != width, la textura se estiraria. Compensamos multiplicando
    // las V por (height / width). Asi la textura se ve cuadrada en las caras
    // laterales, que son las que se ven.
    // (Las caras superior/inferior no las mira nadie.)
    float aspect = height / width;
    for (int i = 0; i < mesh.vertexCount; ++i) {
        mesh.texcoords[i * 2 + 1] *= aspect;
    }

    Model m = LoadModelFromMesh(mesh);
    if (hasTex && tex.id != 0) {
        m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    }
    return m;
}

Model WorldMeshes::makeFloorModel(float w, float h,
                                   Texture2D tex, bool hasTex)
{
    // Plano horizontal. GenMeshPlane hace un plano en XZ con UVs [0,1].
    Mesh mesh = GenMeshPlane(w, h, 1, 1);

    // Repetir la textura: 1 repeticion cada 2 metros.
    for (int i = 0; i < mesh.vertexCount; ++i) {
        mesh.texcoords[i * 2 + 0] *= (w / 2.0f);
        mesh.texcoords[i * 2 + 1] *= (h / 2.0f);
    }

    Model m = LoadModelFromMesh(mesh);
    if (hasTex && tex.id != 0) {
        m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    }
    return m;
}

void WorldMeshes::build(const Maze& maze, const Assets& assets) {
    unload();

    float mw = maze.w * C::CELL;
    float mh = maze.h * C::CELL;

    // Suelo: un solo plano grande
    floorModel = makeFloorModel(mw, mh, assets.floorTile, assets.hasFloorTile);

    // Paredes: 3 modelos, uno por variante. Todos del mismo tamano.
    wallModelTile   = makeWallModel(C::CELL, C::WALL_H, assets.wallTile,   assets.hasWallTile);
    wallModelLocker = makeWallModel(C::CELL, C::WALL_H, assets.wallLocker, assets.hasWallLocker);
    wallModelBrick  = makeWallModel(C::CELL, C::WALL_H, assets.wallBrick,  assets.hasWallBrick);

    built = true;
}

void WorldMeshes::unload() {
    auto freeModel = [](Model& m) {
        if (m.meshCount > 0) {
            UnloadModel(m);
            m = {};
        }
    };
    freeModel(floorModel);
    freeModel(wallModelTile);
    freeModel(wallModelLocker);
    freeModel(wallModelBrick);
    built = false;
}

void WorldMeshes::draw(bool /*textured*/) const {
    if (!built) return;

    // Suelo
    if (floorModel.meshCount > 0) {
        DrawModel(floorModel, { 0, 0, 0 }, 1.0f, WHITE);
    }
}
