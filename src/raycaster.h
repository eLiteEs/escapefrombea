#pragma once
#include "assets.h"
#include "common.h"
#include "maze.h"
#include <vector>

// -------------------------------------------------------------------------
//  RayTex — textura en RAM, formato RGBA8.
//  Acceso directo por píxel (pixels[y * w + x]).
//  Se puede originar desde fichero o copiando una Texture2D existente.
// -------------------------------------------------------------------------
struct RayTex {
  std::vector<Color> pixels;
  int w = 0, h = 0;
  bool loaded = false;

  // Carga desde fichero (PNG, JPG...) y lo formatea a RGBA8.
  bool loadFromFile(const char *path);

  // Copia desde una Texture2D ya cargada en GPU.
  // Se usa para enemigos e items, cuyos originales en Assets
  // siguen siendo necesarios como Texture2D para HUD / jumpscare.
  bool fromTexture(Texture2D tex);

  // Libera píxeles y resetea el estado.
  void unload();
};

// -------------------------------------------------------------------------
//  Raycaster — renderizado estilo Wolfenstein 3D.
//  Coste O(ancho_pantalla) por frame en paredes + O(sprites_pantalla)
//  adicionales. Sin draw calls por celda, sin matrices, sin depth buffer
//  GPU: todo el pipeline de profundidad va por zbuffer en CPU.
// -------------------------------------------------------------------------
struct Raycaster {
  // --- Resolución interna del framebuffer ---
  int renderW = 320;
  int renderH = 200;

  // --- Framebuffer y zbuffer (reusados cada frame) ---
  std::vector<Color> framebuffer; // renderW * renderH
  std::vector<float> zbuffer; // renderW (distancia perpendicular por columna)

  // --- Espejo CPU → GPU (una textura que se sube por frame) ---
  Image image = {};
  Texture2D texture = {};
  bool ready = false;

  // --- Texturas propias ---
  RayTex wallTex[3]; // 0=tile, 1=locker, 2=brick
  RayTex enemyTex[ENEMY_KIND_COUNT];
  RayTex itemTex[ITEM_KIND_COUNT];
  RayTex portalTex;

  // --- Control de carga diferida ---
  // Enemigos e items se cargan al primer startLevel, no al arrancar
  // el juego. En menú / attract no se consume RAM por ellos.
  bool dynamicLoaded = false;

  // --- Ciclo de vida ---
  void init(int rw, int rh);
  void resize(int rw, int rh);
  void unload();

  // --- Carga ---
  // Paredes y portal, directo desde assets/sprites/*.png.
  bool loadStaticTextures();

  // Enemigos e items, copiados desde Assets::enemyTex / itemTex.
  // Idempotente: si ya están cargados, no hace nada y devuelve true.
  bool loadDynamicTextures(const Assets &assets);

  // --- Render principal ---
  // Deja el framebuffer en `texture`, listo para ser dibujado con
  // DrawTexturePro en drawGameplayView.
  void render(const Player &pl, const Maze &maze,
              const std::vector<Enemy> &enemies,
              const std::vector<Projectile> &projectiles,
              const std::vector<WorldPickup> &items, const Vector3 &exitPos,
              bool exitLocked, float renderDist, float fovDeg);

private:
  // --- Sprite interno (billboard con zbuffer test) ---
  struct SpriteEntry {
    Vector3 pos;
    float height;
    float width;
    const RayTex *tex;
    Color tint;
    float dist2;
  };
  std::vector<SpriteEntry> spriteBuf; // reusable entre frames

  // Framebuffer / texture helpers
  void initFramebuffer(int rw, int rh);
  void unloadFramebuffer();

  // Añade un sprite a la lista del frame actual.
  void pushSprite(const Vector3 &pos, float height, float width,
                  const RayTex *tex, Color tint);

  // Dibuja el contenido de `spriteBuf` aplicando zbuffer + alpha blending.
  void drawSprites(const Player &pl, float dirX, float dirZ, float planeX,
                   float planeZ, int pitchOffset, int bobOffset,
                   float projScale);
};
