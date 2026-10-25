#ifndef RENDERER3D_H
#define RENDERER3D_H

#include "maze.hpp"
#include "raylib.h"

#include <vector>

// Sprite del mundo. pos es la BASE del sprite (suelo), no su centro.
// tex.id == 0 -> se usa la textura blanca de fallback (color plano).
struct WorldSprite3D {
  Vector3 pos = {0.0f, 0.0f, 0.0f};
  Vector2 size = {1.0f, 1.0f}; // ancho / alto en unidades de mundo
  Texture2D tex = {};
  Color tint = {255, 255, 255, 255};
  bool crossedQuads = false;
};

// Renderer 3D por chunks. No posee los modelos: son propiedad de Maze.
class Renderer3D {
public:
  Renderer3D() = default;
  ~Renderer3D();

  Renderer3D(const Renderer3D &) = delete;
  Renderer3D &operator=(const Renderer3D &) = delete;

  // eye en unidades de mundo. yaw: 0 = -Z, creciente hacia +X (como Player).
  // pitch positivo = mirar arriba. fovVerticalDeg en GRADOS (raylib usa FOV
  // vertical). Debe llamarse dentro de BeginDrawing/EndDrawing (o de un
  // BeginTextureMode).
  void render(Maze &maze, Vector3 eye, float yaw, float pitch,
              float fovVerticalDeg, float renderDist,
              const std::vector<WorldSprite3D> &sprites, bool occlusion = true);

  // Liberar recursos propios. Llamar antes de CloseWindow().
  void shutdown();

  int lastDrawnChunks() const { return m_drawnChunks; }
  int lastDrawCalls() const { return m_drawCalls; }
  const std::vector<unsigned char> &visibility() const { return m_visible; }

private:
  void ensureFallback();
  void buildVisibility(const Maze &maze, float ex, float ez, float dist);
  bool chunkHasVisibleCell(const Maze &maze, int cx, int cy) const;

  std::vector<unsigned char> m_visible; // w*h, 1 = potencialmente visible
  int m_visW = 0;
  int m_visH = 0;
  int m_drawnChunks = 0;
  int m_drawCalls = 0;
  Texture2D m_white = {};
  void drawTintedModel(Model &model, Color tint);
  void drawCrossQuads(Vector3 base, Vector2 size, Texture2D tex, Color tint);
};

#endif // RENDERER3D_H
