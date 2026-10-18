#include "paths.h"
#include "raycaster.h"
#include <algorithm>
#include <cmath>

// =========================================================================
//  RayTex
// =========================================================================
bool RayTex::loadFromFile(const char *path) {
  unload();
  Image img = LoadImage(path);
  if (img.data == nullptr)
    return false;

  ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  w = img.width;
  h = img.height;
  Color *src = (Color *)img.data;
  pixels.assign(src, src + (size_t)w * h);
  UnloadImage(img);

  loaded = true;
  return true;
}

bool RayTex::fromTexture(Texture2D tex) {
  if (tex.id == 0)
    return false;
  unload();

  Image img = LoadImageFromTexture(tex);
  if (img.data == nullptr)
    return false;

  ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  w = img.width;
  h = img.height;
  Color *src = (Color *)img.data;
  pixels.assign(src, src + (size_t)w * h);
  UnloadImage(img);

  loaded = true;
  return true;
}

void RayTex::unload() {
  pixels.clear();
  pixels.shrink_to_fit();
  w = h = 0;
  loaded = false;
}

// =========================================================================
//  Framebuffer
// =========================================================================
void Raycaster::initFramebuffer(int rw, int rh) {
  renderW = rw;
  renderH = rh;

  framebuffer.assign((size_t)renderW * renderH, Color{0, 0, 0, 255});
  zbuffer.assign(renderW, 1e30f);
  spriteBuf.clear();
  spriteBuf.reserve(64);

  image = {};
  image.width = renderW;
  image.height = renderH;
  image.mipmaps = 1;
  image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  image.data = framebuffer.data();

  texture = LoadTextureFromImage(image);
  SetTextureFilter(texture, TEXTURE_FILTER_POINT);
  ready = true;
}

void Raycaster::unloadFramebuffer() {
  if (texture.id != 0)
    UnloadTexture(texture);
  texture = {};
  image = {};
  framebuffer.clear();
  framebuffer.shrink_to_fit();
  zbuffer.clear();
  zbuffer.shrink_to_fit();
  spriteBuf.clear();
  spriteBuf.shrink_to_fit();
  ready = false;
}

void Raycaster::init(int rw, int rh) { initFramebuffer(rw, rh); }

void Raycaster::resize(int rw, int rh) {
  if (rw == renderW && rh == renderH && ready)
    return;
  unloadFramebuffer();
  initFramebuffer(rw, rh);
}

void Raycaster::unload() {
  unloadFramebuffer();
  for (auto &t : wallTex)
    t.unload();
  for (auto &t : enemyTex)
    t.unload();
  for (auto &t : itemTex)
    t.unload();
  portalTex.unload();
  dynamicLoaded = false;
}

// =========================================================================
//  Carga de texturas
// =========================================================================
bool Raycaster::loadStaticTextures() {
  wallTex[0].loadFromFile(Paths::sprite("wall_tile.png").c_str());
  wallTex[1].loadFromFile(Paths::sprite("wall_locker.png").c_str());
  wallTex[2].loadFromFile(Paths::sprite("wall_brick.png").c_str());
  portalTex.loadFromFile(Paths::sprite("exit_portal.png").c_str());

  // Fallback del portal: 8x8 verde plano por si falta el PNG
  if (!portalTex.loaded) {
    portalTex.w = portalTex.h = 8;
    portalTex.pixels.assign(64, Color{70, 220, 130, 255});
    portalTex.loaded = true;
  }
  return true;
}

bool Raycaster::loadDynamicTextures(const Assets &assets) {
  if (dynamicLoaded)
    return true;

  for (int i = 0; i < ENEMY_KIND_COUNT; ++i)
    enemyTex[i].fromTexture(assets.enemyTex[i]);

  for (int i = 0; i < ITEM_KIND_COUNT; ++i)
    itemTex[i].fromTexture(assets.itemTex[i]);

  dynamicLoaded = true;
  return true;
}

// =========================================================================
//  Sprites
// =========================================================================
void Raycaster::pushSprite(const Vector3 &pos, float height, float width,
                           const RayTex *tex, Color tint) {
  if (!tex || !tex->loaded || tex->h == 0)
    return;

  SpriteEntry e;
  e.pos = pos;
  e.height = height;
  e.width = (width > 0.0f) ? width : height * (float)tex->w / (float)tex->h;
  e.tex = tex;
  e.tint = tint;
  e.dist2 = 0.0f;
  spriteBuf.push_back(e);
}

void Raycaster::drawSprites(const Player &pl, float dirX, float dirZ,
                            float planeX, float planeZ, int pitchOffset,
                            int bobOffset, float projScale) {
  if (spriteBuf.empty())
    return;

  const int W = renderW;
  const int H = renderH;

  // Calcular distancias al cuadrado (para ordenar)
  for (auto &s : spriteBuf) {
    float dx = s.pos.x - pl.pos.x;
    float dz = s.pos.z - pl.pos.z;
    s.dist2 = dx * dx + dz * dz;
  }

  // De lejos a cerca: así los más cercanos tapan a los más lejanos
  std::sort(spriteBuf.begin(), spriteBuf.end(),
            [](const SpriteEntry &a, const SpriteEntry &b) {
              return a.dist2 > b.dist2;
            });

  float invDet = 1.0f / (planeX * dirZ - dirX * planeZ);

  for (auto &sp : spriteBuf) {
    float sx = sp.pos.x - pl.pos.x;
    float sz = sp.pos.z - pl.pos.z;

    // Transformación al espacio de cámara
    float transformX = invDet * (dirZ * sx - dirX * sz);
    float transformZ = invDet * (-planeZ * sx + planeX * sz);

    if (transformZ <= 0.1f)
      continue;

    int spriteScreenX = (int)((W / 2.0f) * (1.0f + transformX / transformZ));

    int spriteH = (int)(sp.height * projScale / transformZ);
    int spriteW = (int)(sp.width * projScale / transformZ);
    if (spriteH <= 0 || spriteW <= 0)
      continue;

    int yBottom = H / 2 +
                  (int)((C::EYE_H - sp.pos.y) * projScale / transformZ) +
                  pitchOffset + bobOffset;
    int yTop = yBottom - spriteH;

    int x0 = spriteScreenX - spriteW / 2;
    int x1 = spriteScreenX + spriteW / 2;

    int xa = std::max(0, x0);
    int xb = std::min(W, x1);
    int ya = std::max(0, yTop);
    int yb = std::min(H, yBottom);

    if (xa >= xb || ya >= yb)
      continue;

    const int texW = sp.tex->w;
    const int texH = sp.tex->h;
    const Color *texPx = sp.tex->pixels.data();

    // Precalcular tinte
    const unsigned int tr = sp.tint.r;
    const unsigned int tg = sp.tint.g;
    const unsigned int tb = sp.tint.b;
    const bool applyTint = (tr != 255 || tg != 255 || tb != 255);

    for (int x = xa; x < xb; ++x) {
      // Test de profundidad contra las paredes
      if (transformZ >= zbuffer[x])
        continue;

      int texX = (x - x0) * texW / spriteW;
      if (texX < 0)
        texX = 0;
      if (texX >= texW)
        texX = texW - 1;

      for (int y = ya; y < yb; ++y) {
        int texY = (y - yTop) * texH / spriteH;
        if (texY < 0)
          texY = 0;
        if (texY >= texH)
          texY = texH - 1;

        Color c = texPx[(size_t)texY * texW + texX];
        if (c.a < 8)
          continue;

        // Alpha blending contra el framebuffer
        if (c.a < 255) {
          Color bg = framebuffer[(size_t)y * W + x];
          unsigned int a = c.a;
          unsigned int na = 255 - a;
          c.r = (unsigned char)((c.r * a + bg.r * na) / 255);
          c.g = (unsigned char)((c.g * a + bg.g * na) / 255);
          c.b = (unsigned char)((c.b * a + bg.b * na) / 255);
        }
        // Tinte (hit flash, rage, portal bloqueado)
        if (applyTint) {
          c.r = (unsigned char)((c.r * tr) >> 8);
          c.g = (unsigned char)((c.g * tg) >> 8);
          c.b = (unsigned char)((c.b * tb) >> 8);
        }
        c.a = 255;
        framebuffer[(size_t)y * W + x] = c;
      }
    }
  }
}

// =========================================================================
//  Render principal
// =========================================================================
void Raycaster::render(const Player &pl, const Maze &maze,
                       const std::vector<Enemy> &enemies,
                       const std::vector<Projectile> &projectiles,
                       const std::vector<WorldPickup> &items,
                       const Vector3 &exitPos, bool exitLocked,
                       float renderDist, float fovDeg) {
  if (!ready)
    return;

  const int W = renderW;
  const int H = renderH;

  // --- Cámara ---
  float fovRad = fovDeg * DEG2RAD;
  float fovScale = tanf(fovRad * 0.5f);
  float projScale = (H * 0.5f) / fovScale;

  float dirX = sinf(pl.yaw);
  float dirZ = -cosf(pl.yaw);
  float planeX = cosf(pl.yaw) * fovScale;
  float planeZ = sinf(pl.yaw) * fovScale;

  int pitchOffset = (int)(pl.pitch * projScale);
  float bob = sinf((float)GetTime() * 3.0f) * 0.06f;
  int bobOffset = (int)(bob * projScale / 3.0f);

  // --- Fondo: techo + suelo por scanlines ---
  int horizon = H / 2 + pitchOffset + bobOffset;
  if (horizon < 0)
    horizon = 0;
  if (horizon > H)
    horizon = H;

  Color colCeil = {18, 18, 26, 255};
  Color colFloor = {32, 32, 42, 255};

  for (int y = 0; y < horizon; ++y) {
    Color *row = &framebuffer[(size_t)y * W];
    for (int x = 0; x < W; ++x)
      row[x] = colCeil;
  }
  for (int y = horizon; y < H; ++y) {
    Color *row = &framebuffer[(size_t)y * W];
    for (int x = 0; x < W; ++x)
      row[x] = colFloor;
  }

  float pxGrid = pl.pos.x / C::CELL;
  float pzGrid = pl.pos.z / C::CELL;

  // --- Paredes: una columna por píxel (DDA) ---
  for (int x = 0; x < W; ++x) {
    float cameraX = 2.0f * (float)x / (float)W - 1.0f;
    float rayDirX = dirX + planeX * cameraX;
    float rayDirZ = dirZ + planeZ * cameraX;

    int mapX = (int)pxGrid;
    int mapZ = (int)pzGrid;

    float deltaX = (rayDirX == 0.0f) ? 1e30f : fabsf(1.0f / rayDirX);
    float deltaZ = (rayDirZ == 0.0f) ? 1e30f : fabsf(1.0f / rayDirZ);

    int stepX, stepZ;
    float sideX, sideZ;

    if (rayDirX < 0.0f) {
      stepX = -1;
      sideX = (pxGrid - mapX) * deltaX;
    } else {
      stepX = 1;
      sideX = (mapX + 1.0f - pxGrid) * deltaX;
    }
    if (rayDirZ < 0.0f) {
      stepZ = -1;
      sideZ = (pzGrid - mapZ) * deltaZ;
    } else {
      stepZ = 1;
      sideZ = (mapZ + 1.0f - pzGrid) * deltaZ;
    }

    int side = 0;
    bool hit = false;
    int guard = 0;
    int maxSteps = (int)(renderDist / C::CELL) + 4;

    while (!hit && guard < maxSteps) {
      ++guard;
      if (sideX < sideZ) {
        sideX += deltaX;
        mapX += stepX;
        side = 0;
      } else {
        sideZ += deltaZ;
        mapZ += stepZ;
        side = 1;
      }
      hit = maze.wallAt(mapX, mapZ);
    }

    if (!hit) {
      zbuffer[x] = 1e30f;
      continue;
    }

    float perpDist;
    if (side == 0)
      perpDist = (mapX - pxGrid + (1 - stepX) * 0.5f) / rayDirX;
    else
      perpDist = (mapZ - pzGrid + (1 - stepZ) * 0.5f) / rayDirZ;
    if (perpDist < 0.0001f)
      perpDist = 0.0001f;

    float perpDistWorld = perpDist * C::CELL;
    if (perpDistWorld > renderDist) {
      zbuffer[x] = 1e30f;
      continue;
    }
    zbuffer[x] = perpDistWorld;

    int screenYTop = H / 2 -
                     (int)((C::WALL_H - C::EYE_H) * projScale / perpDistWorld) +
                     pitchOffset + bobOffset;
    int screenYBottom = H / 2 + (int)(C::EYE_H * projScale / perpDistWorld) +
                        pitchOffset + bobOffset;

    int yStart = std::max(0, screenYTop);
    int yEnd = std::min(H, screenYBottom);
    if (yStart >= yEnd)
      continue;

    int lineHeight = screenYBottom - screenYTop;
    if (lineHeight <= 0)
      continue;

    // Textura por variante de pared
    int variant = 0;
    if (mapZ >= 0 && mapZ < (int)maze.wallVariant.size() && mapX >= 0 &&
        mapX < (int)maze.wallVariant[mapZ].size())
      variant = maze.wallVariant[mapZ][mapX];
    if (variant < 0 || variant > 2)
      variant = 0;

    const RayTex *tex = &wallTex[variant];
    if (!tex->loaded)
      tex = &wallTex[0];
    if (!tex->loaded) {
      Color flat = {100, 100, 120, 255};
      for (int y = yStart; y < yEnd; ++y)
        framebuffer[(size_t)y * W + x] = flat;
      continue;
    }

    // Coordenada X de la textura
    float wallX;
    if (side == 0)
      wallX = pzGrid + perpDist * rayDirZ;
    else
      wallX = pxGrid + perpDist * rayDirX;
    wallX -= floorf(wallX);

    int texX = (int)(wallX * tex->w);
    if ((side == 0 && rayDirX > 0.0f) || (side == 1 && rayDirZ < 0.0f))
      texX = tex->w - texX - 1;
    if (texX < 0)
      texX = 0;
    if (texX >= tex->w)
      texX = tex->w - 1;

    // Tinte por distancia (fog) + paridad anti-mareo
    float distTint = 1.0f - perpDistWorld / renderDist;
    if (distTint < 0.10f)
      distTint = 0.10f;
    float parity = ((mapX + mapZ) & 1) ? 0.93f : 1.0f;
    float tint = distTint * parity;
    unsigned int tint256 = (unsigned int)(tint * 256.0f);

    float texStep = (float)tex->h / (float)lineHeight;
    float texPos = (float)(yStart - screenYTop) * texStep;
    const Color *texPx = tex->pixels.data();

    for (int y = yStart; y < yEnd; ++y) {
      int texY = (int)texPos;
      if (texY < 0)
        texY = 0;
      if (texY >= tex->h)
        texY = tex->h - 1;
      texPos += texStep;

      Color c = texPx[(size_t)texY * tex->w + texX];
      c.r = (unsigned char)((c.r * tint256) >> 8);
      c.g = (unsigned char)((c.g * tint256) >> 8);
      c.b = (unsigned char)((c.b * tint256) >> 8);
      framebuffer[(size_t)y * W + x] = c;
    }
  }

  // --- Construir lista de sprites del frame ---
  spriteBuf.clear();

  // Enemigos
  for (auto &e : enemies) {
    int kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
    Color tint = {255, 255, 255, 255};
    if (e.hitFlash > 0.0f)
      tint = {255, 80, 80, 255};
    else if (e.rageT > 0.0f)
      tint = {255, 140, 90, 255};
    pushSprite({e.pos.x, 0.0f, e.pos.z}, 2.6f, 0.0f, &enemyTex[kind], tint);
  }

  // Items
  for (auto &w : items) {
    float h = 0.55f;
    switch (w.kind) {
    case ITEM_ROCK:
      h = 0.35f;
      break;
    case ITEM_BOX:
      h = 0.70f;
      break;
    case ITEM_LIME:
      h = 0.85f;
      break;
    case ITEM_BICIMAD:
      h = 1.00f;
      break;
    case ITEM_KEY:
      h = 0.55f;
      break;
    default:
      h = 0.70f;
      break;
    }
    int k = (int)w.kind;
    if (k < 0 || k >= ITEM_KIND_COUNT)
      continue;

    float bob = sinf(w.bobT) * C::ROCK_PICKUP_BOB;
    pushSprite({w.pos.x, w.pos.y + bob, w.pos.z}, h, 0.0f, &itemTex[k],
               Color{255, 255, 255, 255});
  }

  for (auto &pr : projectiles) {
    if (!pr.alive)
      continue;

    float size = 0.25f;
    switch (pr.kind) {
    case ITEM_ROCK:
      size = 0.18f;
      break;
    case ITEM_BOX:
      size = 0.35f;
      break;
    case ITEM_LIME:
      size = 0.80f;
      break;
    case ITEM_BICIMAD:
      size = 1.25f;
      break;
    case ITEM_WALL:
      size = 0.90f;
      break;
    default:
      size = 0.25f;
      break;
    }
    int k = (int)pr.kind;
    if (k <= ITEM_NONE || k >= ITEM_KIND_COUNT)
      continue;

    // Centrar el sprite en la posición del proyectil
    pushSprite({pr.pos.x, pr.pos.y - size * 0.5f, pr.pos.z}, size, size,
               &itemTex[k], Color{255, 255, 255, 255});
  }

  {
    int pcx = (int)floorf(pl.pos.x / C::CELL);
    int pcz = (int)floorf(pl.pos.z / C::CELL);
    for (int y = 0; y < maze.h; ++y) {
      for (int x = 0; x < maze.w; ++x) {
        if (!maze.hasTent(x, y))
          continue;
        // Si el jugador está dentro de la tienda, saltar el sprite
        // (si no, el billboard taparía toda la pantalla)
        if (x == pcx && y == pcz)
          continue;

        Vector3 center = maze.cellCenter(x, y);
        pushSprite({center.x, 0.0f, center.z}, 1.5f, 3.0f, &itemTex[ITEM_WALL],
                   Color{255, 255, 255, 255});
      }
    }
  }

  // Portal
  {
    float pulse = 1.0f + 0.15f * sinf((float)GetTime() * 4.0f);
    Color portalTint =
        exitLocked ? (Color){200, 60, 60, 255} : (Color){200, 255, 220, 255};
    pushSprite({exitPos.x, 0.0f, exitPos.z}, 2.4f * pulse, 0.0f, &portalTex,
               portalTint);
  }

  // Dibujar todos los sprites con zbuffer
  drawSprites(pl, dirX, dirZ, planeX, planeZ, pitchOffset, bobOffset,
              projScale);

  // Subir el framebuffer a la GPU (una sola llamada por frame)
  UpdateTexture(texture, framebuffer.data());
}
