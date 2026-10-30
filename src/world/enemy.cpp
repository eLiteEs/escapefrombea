#include "enemy.hpp"

#include "debug.hpp"
#include "game.hpp"
#include "util_format.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <raymath.h>
#include <vector>

bool lineOfSight(const Maze &maze, Vector3 a, Vector3 b) {
  if (maze.blockedAtWorld(a.x, a.z))
    return false;

  float dx = b.x - a.x, dz = b.z - a.z;
  float dist = sqrtf(dx * dx + dz * dz);
  int steps = (int)(dist / (C::CELL * 0.25f)) + 1;

  float startY = a.y + C::EYE_H;
  float endY = b.y + C::EYE_H;

  for (int i = 1; i <= steps; ++i) {
    float t = (float)i / (float)steps;
    float px = a.x + dx * t;
    float pz = a.z + dz * t;

    if (maze.blockedAtWorld(px, pz))
      return false;

    // Altura del rayo en este punto (interpolada)
    float rayY = startY + (endY - startY) * t;

    // Si en este punto hay un riser más alto que el rayo, bloquea.
    if (maze.groundZAtWorld(px, pz) > rayY)
      return false;
  }
  return true;
}

// Definida en player.cpp
float riserDepth(const Maze &maze, float x, float z, float r, float refZ);

static void moveEntity(const Maze &maze, float &x, float &z, float dx, float dz,
                       float r) {
  // Cada eje recalcula suelo y rampa de origen con la posición ACTUAL
  // (antes se usaba la del inicio también para el eje Z).
  auto step = [&](float ax, float az) {
    float nx = x + ax, nz = z + az;
    float curZ = maze.groundZAtWorld(x, z);
    bool srcIsRamp =
        (maze.stairAt((int)floorf(x / C::CELL), (int)floorf(z / C::CELL)) != 0);

    if (maze.circleHitsWall(nx, nz, r))
      return;

    bool dstIsRamp = (maze.stairAt((int)floorf(nx / C::CELL),
                                   (int)floorf(nz / C::CELL)) != 0);
    float dstZ = maze.groundZAtWorld(nx, nz);
    // En escaleras se sube un peldaño entero (STAIR_STEP_H), que puede ser
    // mayor que STEP_MAX; fuera de ellas solo STEP_MAX.
    float maxUp = C::STEP_MAX;
    if (srcIsRamp || dstIsRamp)
      maxUp = fmaxf(C::STEP_MAX, C::STAIR_STEP_H) + 0.01f;
    if (dstZ > curZ + maxUp)
      return;

    if (!(srcIsRamp || dstIsRamp)) {
      // Bloquear solo si el movimiento profundiza el solape con un bloque
      // más alto; así el enemigo que baja de una plataforma puede alejarse.
      float newD = riserDepth(maze, nx, nz, r, curZ);
      if (newD > 0.0f && newD > riserDepth(maze, x, z, r, curZ) + 1e-4f)
        return;
    }
    x = nx;
    z = nz;
  };
  step(dx, 0.0f);
  step(0.0f, dz);
}

// RNG compartido (antes se creaba un mt19937 nuevo en cada giro)
static std::mt19937 &enemyRng() {
  static std::mt19937 rng(std::random_device{}());
  return rng;
}

// Distancia de captura: horizontal + tolerancia vertical, para que saltar
// no te haga inatrapable.
static bool inCatchRange(const Enemy &e, const Player &p) {
  float dx = e.pos.x - p.pos.x, dz = e.pos.z - p.pos.z;
  if (dx * dx + dz * dz >= C::CATCH_R * C::CATCH_R)
    return false;
  return fabsf(e.pos.y - p.pos.y) < C::EYE_H;
}

static int closestVisibleTarget(const Enemy &self, const Maze &maze,
                                Player *players, int n) {
  int best = -1;
  float bestDist = C::SEE_DIST;
  for (int p = 0; p < n; ++p) {
    if (players[p].caught || players[p].escaped || players[p].hidden)
      continue;
    float dx = players[p].pos.x - self.pos.x;
    float dz = players[p].pos.z - self.pos.z;
    float d = sqrtf(dx * dx + dz * dz);
    if (d < bestDist && lineOfSight(maze, self.pos, players[p].pos)) {
      bestDist = d;
      best = p;
    }
  }
  return best;
}

static void separateEnemies(std::vector<Enemy> &enemies, const Maze &maze,
                            size_t i, float dt) {
  Vector3 push = {0, 0, 0};
  for (size_t j = 0; j < enemies.size(); ++j) {
    if (i == j)
      continue;
    if (!enemies[j].alive || enemies[j].dyingT > 0.0f)
      continue;

    float dx = enemies[i].pos.x - enemies[j].pos.x;
    float dz = enemies[i].pos.z - enemies[j].pos.z;
    float d2 = dx * dx + dz * dz;
    float minD = C::E_RADIUS * 2.2f;
    if (d2 <= 0.0001f) {
      // Apilados en el mismo punto: separar de forma determinista
      float sgn = (i < j) ? 1.0f : -1.0f;
      push.x += sgn * minD * 0.5f;
    } else if (d2 < minD * minD) {
      float d = sqrtf(d2);
      push.x += (dx / d) * (minD - d) * 0.5f;
      push.z += (dz / d) * (minD - d) * 0.5f;
    }
  }

  float curZ = maze.groundZAtWorld(enemies[i].pos.x, enemies[i].pos.z);

  float nx = enemies[i].pos.x + push.x * dt * 6.0f;
  float nz = enemies[i].pos.z + push.z * dt * 6.0f;

  // Solo mover si no cae en celda más alta (evita escalar por empuje)
  if (maze.groundZAtWorld(nx, nz) <= curZ + C::STEP_MAX) {
    if (!maze.circleHitsWall(nx, enemies[i].pos.z, C::E_RADIUS))
      enemies[i].pos.x = nx;
    if (!maze.circleHitsWall(enemies[i].pos.x, nz, C::E_RADIUS))
      enemies[i].pos.z = nz;
  }
}

void updateEnemies(std::vector<Enemy> &enemies, const Maze &maze,
                   Player *players, int numPlayers, float dt, float seeDist,
                   float memoryT) {
  (void)seeDist;
  for (size_t i = 0; i < enemies.size(); ++i) {
    Enemy &e = enemies[i];
    if (!e.alive || e.dyingT > 0.0f)
      continue;
    e.prevPos = e.pos;
    e.repathT -= dt;

    float newY = maze.groundZAtWorld(e.pos.x, e.pos.z);
    if (newY <= e.pos.y + fmaxf(C::STEP_MAX, C::STAIR_STEP_H) + 0.01f) {
      e.pos.y = newY;
    }

    // Stun: no hace nada
    if (e.stunT > 0.0f) {
      separateEnemies(enemies, maze, i, dt);
      if (!Debug::godMode) {
        for (int p = 0; p < numPlayers; ++p) {
          if (players[p].caught || players[p].escaped)
            continue;
          if (players[p].hidden)
            continue;
          if (inCatchRange(e, players[p])) {
            players[p].caught = true;
            players[p].caughtBy = (int)i;
          }
        }
      }
      continue;
    }

    int target = closestVisibleTarget(e, maze, players, numPlayers);

    // Rage: multiplicador de velocidad + siempre te tiene localizado
    float effectiveSpeed = e.speed * e.rageMult;
    if (e.rageT > 0.0f) {
      int nearest = -1;
      float best = 1e9f;
      for (int p = 0; p < numPlayers; ++p) {
        if (players[p].caught || players[p].escaped)
          continue;
        if (players[p].hidden)
          continue;
        float d = Vector3Distance(players[p].pos, e.pos);
        if (d < best) {
          best = d;
          nearest = p;
        }
      }
      if (nearest >= 0) {
        e.lastSeen = players[nearest].pos;
        e.memoryT = memoryT;
      }
    }

    if (target >= 0) {
      e.lastSeen = players[target].pos;
      e.memoryT = memoryT;
      float dx = players[target].pos.x - e.pos.x;
      float dz = players[target].pos.z - e.pos.z;
      float d = sqrtf(dx * dx + dz * dz);
      if (d > 0.01f) {
        moveEntity(maze, e.pos.x, e.pos.z, (dx / d) * effectiveSpeed * dt,
                   (dz / d) * effectiveSpeed * dt, C::E_RADIUS);
      }
    } else if (e.memoryT > 0.0f) {
      e.memoryT -= dt;
      float dx = e.lastSeen.x - e.pos.x;
      float dz = e.lastSeen.z - e.pos.z;
      float d = sqrtf(dx * dx + dz * dz);
      float spd = effectiveSpeed;
      if (e.repathT <= 0.0f) {
        spd *= 0.9f;
        e.repathT = C::REPATH_CD;
      } else {
        spd *= 0.75f;
      }
      if (d < 0.3f) {
        e.memoryT = 0.0f; // ya llegó: dejar de buscar aquí
      } else {
        moveEntity(maze, e.pos.x, e.pos.z, (dx / d) * spd * dt,
                   (dz / d) * spd * dt, C::E_RADIUS);
      }
    } else {
      e.turnT -= dt;
      float nx = e.pos.x + e.dir.x * effectiveSpeed * dt;
      float nz = e.pos.z + e.dir.y * effectiveSpeed * dt;

      if (e.turnT <= 0 || maze.circleHitsBlocked(nx, nz, C::E_RADIUS)) {
        std::mt19937 &rng = enemyRng();
        bool found = false;
        for (int tries = 0; tries < 8; ++tries) {
          int d = rng() % 4;
          Vector2 nd = {(float)((d == 0) - (d == 1)),
                        (float)((d == 2) - (d == 3))};
          if (!maze.circleHitsBlocked(e.pos.x + nd.x * 0.6f,
                                      e.pos.z + nd.y * 0.6f, C::E_RADIUS)) {
            e.dir = nd;
            found = true;
            break;
          }
        }
        if (!found) // callejón: dar media vuelta en vez de seguir contra la
                    // pared
          e.dir = {-e.dir.x, -e.dir.y};
        e.turnT = 1.0f + (rng() % 100) / 50.0f;
      }
      moveEntity(maze, e.pos.x, e.pos.z, e.dir.x * effectiveSpeed * 0.55f * dt,
                 e.dir.y * effectiveSpeed * 0.55f * dt, C::E_RADIUS);
    }

    float moved2 = (e.pos.x - e.prevPos.x) * (e.pos.x - e.prevPos.x) +
                   (e.pos.z - e.prevPos.z) * (e.pos.z - e.prevPos.z);
    if (moved2 < 0.0001f) {
      e.stuckT += dt;
      if (e.stuckT > C::UNSTICK_T) {
        int d = enemyRng()() % 4;
        e.memoryT = 0.0f; // si perseguía una posición inalcanzable, abandonar
        e.dir = {(float)((d == 0) - (d == 1)), (float)((d == 2) - (d == 3))};
        e.turnT = 0.8f;
        e.stuckT = 0.0f;
      }
    } else {
      e.stuckT = 0.0f;
    }

    separateEnemies(enemies, maze, i, dt);
    e.pos.y = maze.groundZAtWorld(e.pos.x, e.pos.z);

    if (!Debug::godMode) {
      for (int p = 0; p < numPlayers; ++p) {
        if (players[p].caught || players[p].escaped || players[p].hidden)
          continue;
        if (inCatchRange(e, players[p])) {
          players[p].caught = true;
          players[p].caughtBy = (int)i;
        }
      }
    }
  }
}

void Game::spawnEnemiesFromPositions(const std::vector<Vector3> &positions,
                                     int lvl, std::mt19937 &rng) {
  float dm = Util::diffMultFor(currentDifficulty);
  float spd = C::BASE_ESPD * (1.0f + 0.15f * (float)(lvl - 1)) * dm;

  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;

    Enemy e;
    e.pos = p;
    e.dir = {0, 0};
    e.speed = spd * (0.9f + 0.2f * (float)(rng() % 100) / 100.0f);
    e.turnT = 0.0f;
    e.prevPos = p;
    e.hp = e.maxHp;
    e.shield = e.maxShield;
    e.alive = true;

    int r = (int)(rng() % 100);
    if (lvl >= 5 && r > 10)
      e.kind = ENEMY_HYPERGAMY;
    else if (lvl >= 4 && r < 30)
      e.kind = ENEMY_ALMEIDA;
    else if (lvl >= 3 && r < 30)
      e.kind = ENEMY_CV;
    else if (lvl >= 3 && r < 40)
      e.kind = ENEMY_YE;
    else if (lvl >= 2 && r < 65)
      e.kind = ENEMY_MARISA;
    else if (lvl >= 2 && r < 85)
      e.kind = ENEMY_ALT;
    else
      e.kind = ENEMY_BEA;

    e.kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
    enemies.push_back(e);
  }
}

void Game::drawEnemyHealthBars(const Player &pl, int vw, int vh) {
  if (!cfg.useRenderer3D)
    return;

  // Cámara equivalente a la del renderer 3D
  Camera3D cam = {};
  cam.position = {pl.pos.x, pl.pos.y + C::EYE_H, pl.pos.z};
  float cp = cosf(pl.pitch);
  float sp = sinf(pl.pitch);
  cam.target = {cam.position.x + sinf(pl.yaw) * cp, cam.position.y + sp,
                cam.position.z - cosf(pl.yaw) * cp};
  cam.up = {0, 1, 0};
  cam.fovy = (float)cfg.fov;
  cam.projection = CAMERA_PERSPECTIVE;

  Vector3 fwd = {cam.target.x - cam.position.x, cam.target.y - cam.position.y,
                 cam.target.z - cam.position.z};
  float fwdLen = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
  if (fwdLen > 0.0001f) {
    fwd.x /= fwdLen;
    fwd.y /= fwdLen;
    fwd.z /= fwdLen;
  }

  float maxDist = renderDist() * 0.6f;

  Vector3 eye = {pl.pos.x, pl.pos.y + C::EYE_H, pl.pos.z};

  for (auto &e : enemies) {
    if (!e.alive || e.dyingT > 0.0f)
      continue;

    float dist = Vector3Distance(pl.pos, e.pos);
    if (dist > maxDist)
      continue;

    // 1) Delante de la cámara
    Vector3 toEnemy = {e.pos.x - cam.position.x, e.pos.y - cam.position.y,
                       e.pos.z - cam.position.z};
    float dot = toEnemy.x * fwd.x + toEnemy.y * fwd.y + toEnemy.z * fwd.z;
    if (dot <= 0.1f)
      continue;

    // 2) LOS doble (jugador→enemigo Y enemigo→jugador)
    // lineOfSight ya suma EYE_H a cada punto: se le pasan posiciones de pies
    // (con el pecho del enemigo expresado como offset relativo a EYE_H).
    Vector3 chest = {e.pos.x, e.pos.y + 1.35f, e.pos.z};
    Vector3 chestFeet = {chest.x, chest.y - C::EYE_H, chest.z};
    if (!lineOfSight(maze, pl.pos, chestFeet))
      continue;
    if (!lineOfSight(maze, chestFeet, pl.pos))
      continue;

    // 3) Risers en medio: si el suelo sube por encima de los ojos,
    //    hay una pared de escalón tapando la vista.
    {
      float ddx = chest.x - eye.x;
      float ddz = chest.z - eye.z;
      float len = sqrtf(ddx * ddx + ddz * ddz);
      if (len > 0.01f) {
        bool riserBlocks = false;
        float step = C::CELL * 0.25f;
        for (float t = step; t < len; t += step) {
          float px = eye.x + ddx * (t / len);
          float pz = eye.z + ddz * (t / len);
          float gz = maze.groundZAtWorld(px, pz);
          // Si el suelo en ese punto está más alto que la cabeza del jugador,
          // hay un riser bloqueando.
          if (gz > pl.pos.y + C::EYE_H) {
            riserBlocks = true;
            break;
          }
        }
        if (riserBlocks)
          continue;
      }
    }

    // Proyección de la barra
    Vector3 barWorld = {e.pos.x, e.pos.y + 2.9f, e.pos.z};
    Vector2 screen = GetWorldToScreen(barWorld, cam);

    if (screen.x < -50 || screen.x > vw + 50 || screen.y < -20 ||
        screen.y > vh + 20)
      continue;

    float scale = std::clamp(1.0f - dist / maxDist, 0.4f, 1.0f);
    float barW = 70.0f * scale;
    float barH = 7.0f * scale;
    float bx = screen.x - barW * 0.5f;
    float by = screen.y - barH;

    DrawRectangle((int)bx - 1, (int)by - 1, (int)barW + 2, (int)barH + 2,
                  (Color){0, 0, 0, 200});
    DrawRectangle((int)bx, (int)by, (int)barW, (int)barH,
                  (Color){40, 40, 60, 220});

    float hpFrac = (e.maxHp > 0.0f) ? (e.hp / e.maxHp) : 0.0f;
    Color hpCol = (hpFrac > 0.5f)    ? (Color){80, 220, 80, 255}
                  : (hpFrac > 0.25f) ? (Color){240, 160, 40, 255}
                                     : (Color){230, 60, 60, 255};
    DrawRectangle((int)bx, (int)by, (int)(barW * hpFrac), (int)barH, hpCol);

    float shFrac = (e.maxShield > 0.0f) ? (e.shield / e.maxShield) : 0.0f;
    if (shFrac > 0.0f) {
      float shH = std::max(2.0f, 3.0f * scale);
      DrawRectangle((int)bx, (int)(by - shH - 1), (int)(barW * shFrac),
                    (int)shH, (Color){80, 180, 255, 255});
    }

    DrawRectangleLines((int)bx, (int)by, (int)barW, (int)barH,
                       (Color){220, 220, 220, 200});
  }
}
