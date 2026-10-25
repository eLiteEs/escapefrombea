#include "attract.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

// =========================================================================
//  Mapa hardcodeado del attract (15x15).
//  '#' = muro normal   'L' = taquilla   'B' = ladrillo   '.' = suelo
//  Debe ser un grafo conexo: el "Player" camina por él con colisiones.
// =========================================================================
static const char *kAttractMap[] = {
    "###############", "#.............#", "#.###.#.#.###.#", "#.............#",
    "#.#L#.#.#B#.#.#", "#...#...#...#.#", "#.#.#.#.#.#.#.#", "#.............#",
    "#.#.#.#.#.#.#.#", "#...#...#...#.#", "#.#B#.#.#L#.#.#", "#.............#",
    "#.###.#.#.###.#", "#.............#", "###############",
};
static const int kAttractMapH =
    (int)(sizeof(kAttractMap) / sizeof(kAttractMap[0]));

// =========================================================================
//  Parser local
//  Llena Maze directamente sin pasar por generate().
//  No hay RNG, no hay fuentes, no hay variantes aleatorias.
// =========================================================================
static void parseAttractMap(Maze &maze) {
  int w = (int)strlen(kAttractMap[0]);
  int h = kAttractMapH;

  maze.w = w;
  maze.h = h;

  maze.g.assign(h, std::vector<int>(w, 1));
  maze.explored.assign(h, std::vector<bool>(w, false));
  maze.wallVariant.assign(h, std::vector<unsigned char>(w, 0));
  maze.fountains.assign(h, std::vector<bool>(w, false));
  maze.tents.assign(h, std::vector<bool>(w, false));

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      char c = kAttractMap[y][x];
      switch (c) {
      case '#':
        maze.g[y][x] = 1;
        maze.wallVariant[y][x] = 0;
        break;
      case 'L':
        maze.g[y][x] = 1;
        maze.wallVariant[y][x] = 1;
        break;
      case 'B':
        maze.g[y][x] = 1;
        maze.wallVariant[y][x] = 2;
        break;
      default:
        maze.g[y][x] = 0;
        break;
      }
    }
  }
}

// =========================================================================
//  Ciclo de vida
// =========================================================================
void AttractMode::init() {
  parseAttractMap(maze_);

  // Semilla fija: el paseo será siempre el mismo entre ejecuciones,
  // pero sigue siendo "orgánico" (no un script rígido).
  rng_.seed(0xA771AC7u);

  pos_ = maze_.cellCenter(1, 1);
  yaw_ = 0.0f;
  targetYaw_ = 0.0f;
  dirTimer_ = 2.0f;
  pickCd_ = 0.0f;
  bobT_ = 0.0f;
}

void AttractMode::unload() {
  // Nada propio que liberar: el raycaster lo gestiona Game.
}

void AttractMode::prepareRaycaster(Raycaster &rc) {
  // Resolución alta para que el menú se vea nítido, con filtro bilinear
  // para suavizar el escalado (look "CRT viejo").
  // Game::startLevel restaurará resolución y filtro para el gameplay.
  if (rc.renderW != 640 || rc.renderH != 400) {
    rc.resize(640, 400);
  }
  if (rc.texture.id != 0) {
    SetTextureFilter(rc.texture, TEXTURE_FILTER_BILINEAR);
  }
}

// =========================================================================
//  Paseo por el laberinto
// =========================================================================
void AttractMode::pickNewDirection() {
  const float checkDist = 1.5f;

  float candidates[8];
  for (int i = 0; i < 8; ++i)
    candidates[i] = yaw_ + (float)i * (PI * 0.25f);

  std::shuffle(candidates, candidates + 8, rng_);

  for (float c : candidates) {
    float nx = pos_.x + sinf(c) * checkDist;
    float nz = pos_.z - cosf(c) * checkDist;
    if (!maze_.circleHitsWall(nx, nz, C::P_RADIUS)) {
      targetYaw_ = c;
      dirTimer_ = 1.5f + (float)(rng_() % 250) / 100.0f;
      pickCd_ = 0.5f;
      return;
    }
  }

  // Sin salida: girar 180 grados
  targetYaw_ = yaw_ + PI;
  dirTimer_ = 1.0f;
  pickCd_ = 0.5f;
}

void AttractMode::update(float dt) {
  dirTimer_ -= dt;
  pickCd_ -= dt;

  // Giro suave hacia el objetivo
  float diff = targetYaw_ - yaw_;
  while (diff > PI)
    diff -= 2.0f * PI;
  while (diff < -PI)
    diff += 2.0f * PI;

  const float maxTurn = 2.5f * dt;
  if (fabsf(diff) <= maxTurn)
    yaw_ = targetYaw_;
  else
    yaw_ += (diff > 0.0f ? maxTurn : -maxTurn);

  Vector2 dir = {sinf(yaw_), -cosf(yaw_)};

  float ax = pos_.x + dir.x * 1.0f;
  float az = pos_.z + dir.y * 1.0f;
  bool blocked = maze_.circleHitsWall(ax, az, C::P_RADIUS);

  if ((dirTimer_ <= 0.0f || blocked) && pickCd_ <= 0.0f)
    pickNewDirection();

  const float speed = 2.0f;
  float mx = dir.x * speed * dt;
  float mz = dir.y * speed * dt;

  if (!maze_.circleHitsWall(pos_.x + mx, pos_.z, C::P_RADIUS))
    pos_.x += mx;
  if (!maze_.circleHitsWall(pos_.x, pos_.z + mz, C::P_RADIUS))
    pos_.z += mz;

  bobT_ += dt;
}

// =========================================================================
//  Render
// =========================================================================
void AttractMode::draw(Raycaster &rc, int W, int H) {
  // Player "fake" para el render: solo lo que el raycaster usa.
  Player fake;
  fake.pos = pos_;
  fake.yaw = yaw_;
  fake.pitch = 0.0f;

  // Portal fuera de vista: lo colocamos en la esquina inferior derecha,
  // que en el mapa hardcodeado es muro, así que nunca se ve.
  Vector3 fakeExit = maze_.cellCenter(maze_.w - 1, maze_.h - 1);
  bool locked = false;

  // Enemigos e items vacíos: el attract es solo el laberinto
  static const std::vector<Enemy> kNoEnemies;
  static const std::vector<WorldPickup> kNoItems;
  static const std::vector<Projectile> kNoProjectiles;

  rc.render(fake, maze_, kNoEnemies, kNoProjectiles, kNoItems, fakeExit, locked,
            30.0f, 72.0f);

  // Blit escalado a la ventana
  Rectangle src = {0, 0, (float)rc.texture.width, (float)rc.texture.height};
  Rectangle dst = {0, 0, (float)W, (float)H};
  DrawTexturePro(rc.texture, src, dst, {0, 0}, 0.0f, WHITE);
}
