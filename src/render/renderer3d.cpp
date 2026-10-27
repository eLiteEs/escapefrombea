#include "common.hpp"
#include "renderer3d.hpp"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float PI_F = 3.14159265358979323846f;
constexpr float RAD_F = PI_F / 180.0f;
constexpr int OCC_RAYS = 360;   // 1 rayo por grado: suficiente para un grid
constexpr int OCC_STEPS = 1024; // techo de seguridad del DDA

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float wrapAngle(float a) {
  while (a > PI_F)
    a -= 2.0f * PI_F;
  while (a < -PI_F)
    a += 2.0f * PI_F;
  return a;
}

Color blendColor(Color a, Color b, float t) {
  return {static_cast<unsigned char>(a.r + (b.r - a.r) * t),
          static_cast<unsigned char>(a.g + (b.g - a.g) * t),
          static_cast<unsigned char>(a.b + (b.b - a.b) * t),
          static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

// Test conservador: círculo delimitador en el plano XZ.
// No se usa pitch: nunca cullamos verticalmente, así no recortamos
// techos/suelo.
bool inFrustum(float ex, float ez, float yaw, float halfH, float cx, float cz,
               float radius) {
  const float vx = cx - ex;
  const float vz = cz - ez;
  const float d = sqrtf(vx * vx + vz * vz);

  if (d <= radius)
    return true; // muy cerca / dentro: nunca cullar

  const float angle = atan2f(vx, -vz); // forward = (sin yaw, -cos yaw)
  const float slack = asinf(clamp01(radius / d));
  return fabsf(wrapAngle(angle - yaw)) <= halfH + slack + 0.001f;
}

} // namespace

Renderer3D::~Renderer3D() { shutdown(); }

void Renderer3D::shutdown() {
  if (m_white.id != 0) {
    UnloadTexture(m_white);
    m_white = {};
  }
  m_visible.clear();
  m_visW = m_visH = 0;
}

void Renderer3D::ensureFallback() {
  if (m_white.id != 0)
    return;

  // Textura blanca 1x1: permite ver geometria aunque falten los assets.
  Image img = GenImageColor(1, 1, WHITE);
  m_white = LoadTextureFromImage(img);
  UnloadImage(img);

  if (m_white.id != 0)
    SetTextureFilter(m_white, TEXTURE_FILTER_POINT);
}

// --------------------------------------------------------------------------
// Occlusion: DDA 2D -> mapa de celdas potencialmente visibles
// --------------------------------------------------------------------------
void Renderer3D::buildVisibility(const Maze &maze, float ex, float ez,
                                 float dist) {
  const int w = maze.w;
  const int h = maze.h;
  if (w <= 0 || h <= 0)
    return;

  if (m_visW != w || m_visH != h ||
      m_visible.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) {
    m_visible.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    m_visW = w;
    m_visH = h;
  } else {
    std::fill(m_visible.begin(), m_visible.end(),
              static_cast<unsigned char>(0));
  }

  // Todo el DDA trabaja en UNIDADES DE CELDA, no de mundo.
  const float ox = ex / C::CELL;
  const float oz = ez / C::CELL;
  const float maxCells = std::max(1.0f, dist / C::CELL);

  const int originX = static_cast<int>(floorf(ox));
  const int originY = static_cast<int>(floorf(oz));

  auto mark = [&](int x, int y) {
    if (x >= 0 && y >= 0 && x < w && y < h)
      m_visible[static_cast<size_t>(y) * static_cast<size_t>(w) +
                static_cast<size_t>(x)] = 1;
  };

  mark(originX, originY);

  for (int r = 0; r < OCC_RAYS; ++r) {
    const float a = static_cast<float>(r) * (2.0f * PI_F / OCC_RAYS);
    const float dx = cosf(a);
    const float dz = sinf(a);

    int mapX = originX;
    int mapY = originY;

    const float stepX = (dx < 0.0f) ? -1 : 1;
    const float stepY = (dz < 0.0f) ? -1 : 1;

    const float deltaXX = (fabsf(dx) < 1e-6f) ? 1e30f : fabsf(1.0f / dx);
    const float deltaXY = (fabsf(dz) < 1e-6f) ? 1e30f : fabsf(1.0f / dz);

    float sideX =
        (dx < 0.0f) ? (ox - mapX) * deltaXX : (mapX + 1.0f - ox) * deltaXX;
    float sideY =
        (dz < 0.0f) ? (oz - mapY) * deltaXY : (mapY + 1.0f - oz) * deltaXY;

    for (int s = 0; s < OCC_STEPS; ++s) {
      float t;
      if (sideX < sideY) {
        t = sideX;
        sideX += deltaXX;
        mapX += stepX;
      } else {
        t = sideY;
        sideY += deltaXY;
        mapY += stepY;
      }

      if (t > maxCells)
        break;
      if (mapX < 0 || mapY < 0 || mapX >= w || mapY >= h)
        break;

      mark(mapX, mapY);

      // ADAPTADOR: si tu Maze no expone wallAt(), usa aqui tu acceso al grid.
      if (maze.wallAt(mapX, mapY))
        break; // la cara del muro SI es visible
    }
  }

  // Margen conservador de 1 celda: una cara rozada por la vision puede tener
  // el centro de su celda tras otro muro. Sin este margen habria pops en
  // esquinas.
  std::vector<unsigned char> grown(m_visible.size(), 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!m_visible[static_cast<size_t>(y) * static_cast<size_t>(w) +
                     static_cast<size_t>(x)])
        continue;

      const int y0 = std::max(0, y - 1);
      const int y1 = std::min(h - 1, y + 1);
      const int x0 = std::max(0, x - 1);
      const int x1 = std::min(w - 1, x + 1);

      for (int yy = y0; yy <= y1; ++yy)
        for (int xx = x0; xx <= x1; ++xx)
          grown[static_cast<size_t>(yy) * static_cast<size_t>(w) +
                static_cast<size_t>(xx)] = 1;
    }
  }
  m_visible.swap(grown);
}

bool Renderer3D::chunkHasVisibleCell(const Maze &maze, int cx, int cy) const {
  if (m_visible.empty())
    return true;

  const int x0 = cx * C::CHUNK_SIZE;
  const int y0 = cy * C::CHUNK_SIZE;
  const int x1 = std::min(x0 + C::CHUNK_SIZE, maze.w);
  const int y1 = std::min(y0 + C::CHUNK_SIZE, maze.h);

  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (m_visible[static_cast<size_t>(y) * static_cast<size_t>(m_visW) +
                    static_cast<size_t>(x)])
        return true;

  return false;
}

// --------------------------------------------------------------------------
// Dibujado
// --------------------------------------------------------------------------
void Renderer3D::drawTintedModel(Model &model, Color tint) {
  if (model.meshCount == 0 || model.meshes == nullptr)
    return;

  for (int i = 0; i < model.materialCount; ++i) {
    Material &mat = model.materials[i];
    if (mat.maps == nullptr)
      continue;

    MaterialMap &diffuse = mat.maps[MATERIAL_MAP_DIFFUSE];
    diffuse.color =
        WHITE; // el fog va por el tint de DrawModel, no por el material
    if (diffuse.texture.id == 0)
      diffuse.texture = m_white;
  }
  DrawModel(model, {0.0f, 0.0f, 0.0f}, 1.0f, tint);
  ++m_drawCalls;
}

void Renderer3D::drawCrossQuads(Vector3 base, Vector2 size, Texture2D tex,
                                Color tint) {
  Texture2D useTex = (tex.id != 0) ? tex : m_white;
  if (useTex.id == 0)
    return;

  float hw = size.x * 0.5f;
  float h = size.y;
  float x0 = base.x - hw;
  float x1 = base.x + hw;
  float z0 = base.z - hw;
  float z1 = base.z + hw;
  float y0 = base.y;
  float y1 = base.y + h;

  rlSetTexture(useTex.id);

  rlBegin(RL_QUADS);
  rlColor4ub(tint.r, tint.g, tint.b, tint.a);

  // --- Quad 0° cara +Z ---
  rlTexCoord2f(0.0f, 1.0f);
  rlVertex3f(x0, y0, base.z);
  rlTexCoord2f(1.0f, 1.0f);
  rlVertex3f(x1, y0, base.z);
  rlTexCoord2f(1.0f, 0.0f);
  rlVertex3f(x1, y1, base.z);
  rlTexCoord2f(0.0f, 0.0f);
  rlVertex3f(x0, y1, base.z);

  // --- Quad 0° cara -Z ---
  rlTexCoord2f(1.0f, 1.0f);
  rlVertex3f(x1, y0, base.z);
  rlTexCoord2f(0.0f, 1.0f);
  rlVertex3f(x0, y0, base.z);
  rlTexCoord2f(0.0f, 0.0f);
  rlVertex3f(x0, y1, base.z);
  rlTexCoord2f(1.0f, 0.0f);
  rlVertex3f(x1, y1, base.z);

  // --- Quad 90° cara +X ---
  rlTexCoord2f(0.0f, 1.0f);
  rlVertex3f(base.x, y0, z1);
  rlTexCoord2f(1.0f, 1.0f);
  rlVertex3f(base.x, y0, z0);
  rlTexCoord2f(1.0f, 0.0f);
  rlVertex3f(base.x, y1, z0);
  rlTexCoord2f(0.0f, 0.0f);
  rlVertex3f(base.x, y1, z1);

  // --- Quad 90° cara -X ---
  rlTexCoord2f(1.0f, 1.0f);
  rlVertex3f(base.x, y0, z0);
  rlTexCoord2f(0.0f, 1.0f);
  rlVertex3f(base.x, y0, z1);
  rlTexCoord2f(0.0f, 0.0f);
  rlVertex3f(base.x, y1, z1);
  rlTexCoord2f(1.0f, 0.0f);
  rlVertex3f(base.x, y1, z0);

  rlEnd();
  rlSetTexture(0);

  ++m_drawCalls;
}

void Renderer3D::render(Maze &maze, Vector3 eye, float yaw, float pitch,
                        float fovVerticalDeg, float renderDist,
                        const std::vector<WorldSprite3D> &sprites,
                        bool occlusion) {
  ensureFallback();

  m_drawnChunks = 0;
  m_drawCalls = 0;

  if (maze.w <= 0 || maze.h <= 0 || maze.chunksW <= 0 || maze.chunksH <= 0) {
    return;
  }
  if (maze.chunks.size() <
      static_cast<size_t>(maze.chunksW) * static_cast<size_t>(maze.chunksH)) {
    return;
  }

  if (renderDist < C::CELL)
    renderDist = C::CELL;

  Camera3D cam = {};
  cam.position = eye;
  const float cp = cosf(pitch);
  const float sp = sinf(pitch);
  cam.target = {eye.x + sinf(yaw) * cp, eye.y + sp, eye.z - cosf(yaw) * cp};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = std::max(1.0f, std::min(179.0f, fovVerticalDeg));
  cam.projection = CAMERA_PERSPECTIVE;

  BeginMode3D(cam);

  const int rw = std::max(1, GetScreenWidth());
  const int rh = std::max(1, GetScreenHeight());
  const float aspect =
      std::max(0.1f, static_cast<float>(rw) / static_cast<float>(rh));
  const float halfH = atanf(tanf(cam.fovy * RAD_F * 0.5f) * aspect);

  const Color fogColor = {12, 12, 18, 255};
  const Color wallBase = {255, 255, 255, 255}; // BLANCO PURO para diagnóstico
  const Color floorBase = {200, 200, 220, 255};
  const float fogStart = renderDist * 0.35f;
  const float fogSpan = std::max(0.001f, renderDist - fogStart);

  if (occlusion)
    buildVisibility(maze, eye.x, eye.z, renderDist);

  const float chunkWorld = C::CHUNK_SIZE * C::CELL;
  const float boundRadius = chunkWorld * 0.70710678f;

  int totalChunks = 0;
  int emptyModels = 0;

  for (int cy = 0; cy < maze.chunksH; ++cy) {
    for (int cx = 0; cx < maze.chunksW; ++cx) {
      Maze::Chunk &ch = maze.chunks[static_cast<size_t>(cy) *
                                        static_cast<size_t>(maze.chunksW) +
                                    static_cast<size_t>(cx)];
      if (!ch.built)
        continue;
      totalChunks++;

      const float x0 = static_cast<float>(cx * C::CHUNK_SIZE) * C::CELL;
      const float z0 = static_cast<float>(cy * C::CHUNK_SIZE) * C::CELL;
      const float x1 =
          static_cast<float>(std::min((cx + 1) * C::CHUNK_SIZE, maze.w)) *
          C::CELL;
      const float z1 =
          static_cast<float>(std::min((cy + 1) * C::CHUNK_SIZE, maze.h)) *
          C::CELL;

      const float nx = std::max(x0, std::min(eye.x, x1));
      const float nz = std::max(z0, std::min(eye.z, z1));
      const float dx = eye.x - nx;
      const float dz = eye.z - nz;
      const float dNear = sqrtf(dx * dx + dz * dz);

      if (dNear > renderDist)
        continue;

      if (!inFrustum(eye.x, eye.z, yaw, halfH, (x0 + x1) * 0.5f,
                     (z0 + z1) * 0.5f, boundRadius))
        continue;

      if (occlusion && !chunkHasVisibleCell(maze, cx, cy))
        continue;

      const bool hasTile =
          (ch.wallsTile.meshCount > 0 && ch.wallsTile.meshes != nullptr);
      const bool hasLocker =
          (ch.wallsLocker.meshCount > 0 && ch.wallsLocker.meshes != nullptr);
      const bool hasBrick =
          (ch.wallsBrick.meshCount > 0 && ch.wallsBrick.meshes != nullptr);
      const bool hasFloor =
          (ch.floor.meshCount > 0 && ch.floor.meshes != nullptr);

      if (!hasTile && !hasLocker && !hasBrick && !hasFloor) {
        emptyModels++;
        continue;
      }

      const float f = clamp01((dNear - fogStart) / fogSpan);
      const Color tintWalls = blendColor(wallBase, fogColor, f);
      const Color tintFloor = blendColor(floorBase, fogColor, f);

      if (hasTile)
        drawTintedModel(ch.wallsTile, tintWalls);
      if (hasLocker)
        drawTintedModel(ch.wallsLocker, tintWalls);
      if (hasBrick)
        drawTintedModel(ch.wallsBrick, tintWalls);
      if (hasFloor)
        drawTintedModel(ch.floor, tintFloor);

      ++m_drawnChunks;
    }
  }

  for (const auto &s : sprites) {
    if (s.crossedQuads) {
      drawCrossQuads(s.pos, s.size, s.tex, s.tint);
      continue;
    }
    Texture2D tex = (s.tex.id != 0) ? s.tex : m_white;
    if (tex.id == 0) continue;

    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    if (s.flipY) {
      src.y      = (float)tex.height;
      src.height = -(float)tex.height;
    }

    const Vector3 center = { s.pos.x, s.pos.y + s.size.y * 0.5f, s.pos.z };
    DrawBillboardRec(cam, tex, src, center, s.size, s.tint);
    ++m_drawCalls;
  }

  EndMode3D();
}
