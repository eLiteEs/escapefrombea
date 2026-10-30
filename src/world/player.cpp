#include "player.hpp"
#include "debug.hpp"
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers de colisión (compartidos por modo Quake y modo clásico)
// ---------------------------------------------------------------------------

// Profundidad máxima con la que el cuerpo (cuadrado de lado 2r) solapa celdas
// cuyo suelo está por encima de refZ + STEP_MAX. 0 = sin solape.
// La profundidad de una esquina es la distancia mínima a salir por el borde
// de la celda por el lado en el que está el centro. Se usa para permitir
// moverse mientras NO se profundice más: así quien cae junto a una pared
// o baja de una plataforma puede alejarse en vez de quedarse trabado.
// (Declarada también en enemy.cpp; conviene moverla a un header.)
// Altura máxima que se puede subir de golpe: un escalón normal (STEP_MAX) o un
// peldaño de escalera (STAIR_STEP_H), el mayor de los dos.
static inline float maxStepUp() {
  return C::STAIR_STEP_H > C::STEP_MAX ? C::STAIR_STEP_H : C::STEP_MAX;
}

float riserDepth(const Maze &maze, float x, float z, float r, float refZ) {
  float worst = 0.0f;
  const float ox[4] = {-r, r, -r, r};
  const float oz[4] = {-r, -r, r, r};
  for (int k = 0; k < 4; ++k) {
    float px = x + ox[k], pz = z + oz[k];
    int cx = (int)floorf(px / C::CELL);
    int cy = (int)floorf(pz / C::CELL);
    if (maze.wallAt(cx, cy))
      continue; // las paredes ya las trata circleHitsWall
    float thr = refZ + C::STEP_MAX;
    if (maze.stairAt(cx, cy) != 0)
      thr += r * (C::HEIGHT_STEP / C::CELL) * 2.0f +
             (maxStepUp() - C::STEP_MAX); // pendiente + peldaño
    if (maze.groundZAtWorld(px, pz) <= thr)
      continue;

    float minX = cx * C::CELL, maxX = minX + C::CELL;
    float minZ = cy * C::CELL, maxZ = minZ + C::CELL;
    float dX = (x < minX) ? (px - minX) : (x >= maxX) ? (maxX - px) : C::CELL;
    float dZ = (z < minZ) ? (pz - minZ) : (z >= maxZ) ? (maxZ - pz) : C::CELL;
    float depth = fmaxf(fminf(dX, dZ), 1e-4f);
    if (depth > worst)
      worst = depth;
  }
  return worst;
}

// Pendiente (altura/unidad) de la rampa de la celda (cx,cy); 0 si no hay.
static float rampSlope(const Maze &maze, int cx, int cy) {
  int nx = cx, ny = cy;
  switch (maze.stairAt(cx, cy)) {
  case 1:
    ny = cy - 1;
    break;
  case 2:
    nx = cx + 1;
    break;
  case 3:
    ny = cy + 1;
    break;
  case 4:
    nx = cx - 1;
    break;
  default:
    return 0.0f;
  }
  int dh = maze.heightAt(nx, ny) - maze.heightAt(cx, cy);
  if (dh <= 0)
    return 0.0f;
  return (float)dh * C::HEIGHT_STEP / C::CELL;
}

// ¿Puede el jugador pasar de (fromX,fromZ) a (nx,nz)?
// refZ = altura de los pies de referencia (suelo si está en tierra, pos.y si
// está en el aire). Así se puede subir un escalón saltando y las rampas se
// recorren siempre respecto a la altura real del jugador.
static bool canWalk(const Maze &maze, float fromX, float fromZ, float nx,
                    float nz, float refZ) {
  // 1) Paredes reales: siempre bloquean
  if (maze.circleHitsWall(nx, nz, C::P_RADIUS))
    return false;

  int ncx = (int)floorf(nx / C::CELL);
  int ncy = (int)floorf(nz / C::CELL);
  int ccx = (int)floorf(fromX / C::CELL);
  int ccy = (int)floorf(fromZ / C::CELL);

  bool dstIsRamp = (maze.stairAt(ncx, ncy) != 0);
  bool srcIsRamp = (maze.stairAt(ccx, ccy) != 0);

  float dstGroundZ = maze.groundZAtWorld(nx, nz);

  // En rampas la altura sube con la distancia recorrida (la pendiente es
  // ~0.9 por unidad), así que el límite es STEP_MAX + pendiente * distancia.
  // Antes solo se miraba STEP_MAX y, a pocos FPS o corriendo, no se podía
  // subir sin saltar.
  // En escaleras se puede subir un peldaño entero (STAIR_STEP_H) aunque sea
  // mayor que STEP_MAX; fuera de ellas, solo STEP_MAX.
  float allowed = C::STEP_MAX;
  if (srcIsRamp || dstIsRamp)
    allowed = maxStepUp() + 0.01f;

  // Salto demasiado alto (lateral alto de la rampa, pared de escalón...)
  if (dstGroundZ > refZ + allowed)
    return false;

  // Rampas: no hay "riser" frontal, pero el cuerpo NO puede solapar el lateral
  // de la rampa (el triángulo lateral es una pared). Comparamos el suelo bajo
  // cada esquina con el suelo bajo el centro: a lo largo de la pendiente la
  // diferencia es pequeña y continua; en un lateral hay un salto brusco.
  if (srcIsRamp || dstIsRamp) {
    int rcx = dstIsRamp ? ncx : ccx;
    int rcy = dstIsRamp ? ncy : ccy;
    float slope = rampSlope(maze, rcx, rcy);
    const float r = C::P_RADIUS;
    float tol = maxStepUp() + r * 1.4143f * slope * 1.15f + 0.05f;
    const float ox[4] = {-r, r, -r, r};
    const float oz[4] = {-r, -r, r, r};
    for (int k = 0; k < 4; ++k) {
      float gz = maze.groundZAtWorld(nx + ox[k], nz + oz[k]);
      if (fabsf(gz - dstGroundZ) > tol)
        return false;
    }
    return true;
  }

  // Fuera de rampas: el cuerpo (no solo el centro) no puede solapar un
  // escalón/bloque más alto que los pies. Esta comprobación se hace SIEMPRE:
  // antes se saltaba si el jugador estaba en el aire (refZ > suelo), y así,
  // al saltar contra un bloque, el centro llegaba hasta su cara y medio
  // cuerpo quedaba dentro de la pared.
  const float rr = C::P_RADIUS * 1.4f;
  float newDepth = riserDepth(maze, nx, nz, rr, refZ);
  if (newDepth > 0.0f) {
    // Solo se bloquea si el movimiento mete MÁS el cuerpo en el bloque;
    // alejarse o deslizar junto a él siempre se permite.
    float curDepth = riserDepth(maze, fromX, fromZ, rr, refZ);
    if (newDepth > curDepth + 1e-4f)
      return false;
  }
  return true;
}

// Si el jugador acaba dentro de un muro, lo coloca en el centro de la celda
// libre MÁS CERCANA (por distancia real). Devuelve true si lo ha movido.
static bool pushOutOfWall(Player &pl, const Maze &maze) {
  if (!maze.circleHitsWall(pl.pos.x, pl.pos.z, C::P_RADIUS * 0.5f))
    return false;

  int cx = (int)floorf(pl.pos.x / C::CELL);
  int cy = (int)floorf(pl.pos.z / C::CELL);
  float bestD2 = 1e30f;
  Vector3 best = {0, 0, 0};
  bool found = false;

  for (int oy = -4; oy <= 4; ++oy) {
    for (int ox = -4; ox <= 4; ++ox) {
      if (maze.wallAt(cx + ox, cy + oy))
        continue;
      Vector3 c = maze.cellCenter(cx + ox, cy + oy);
      float ddx = c.x - pl.pos.x, ddz = c.z - pl.pos.z;
      float d2 = ddx * ddx + ddz * ddz;
      if (d2 < bestD2) {
        bestD2 = d2;
        best = c;
        found = true;
      }
    }
  }
  if (!found)
    return false;

  pl.pos.x = best.x;
  pl.pos.z = best.z;
  pl.velocity = {0, 0, 0};
  pl.groundZ = maze.groundZAtWorld(pl.pos.x, pl.pos.z);
  pl.pos.y = pl.groundZ;
  pl.onGround = true;
  return true;
}

// Si tras moverse el cuerpo solapa una pared (no debería ocurrir salvo en
// noClip), volver a la última posición válida en vez de quedarse dentro.
static void revertIfInsideWall(Player &pl, const Maze &maze,
                               const Vector3 &safe) {
  if (Debug::noClip)
    return;
  if (maze.circleHitsWall(pl.pos.x, pl.pos.z, C::P_RADIUS)) {
    pl.pos.x = safe.x;
    pl.pos.z = safe.z;
    pl.velocity.x = 0.0f;
    pl.velocity.z = 0.0f;
  }
}

bool updatePlayer(Player &pl, const KeyBindings &kb, float dt,
                  bool allowMouseLook, const Config &cfg, const Maze &maze) {
  if (pl.caught || pl.escaped)
    return false;
  if (pl.hidden)
    return false;

  // Un tirón de frames no debe atravesar paredes ni dar saltos enormes
  if (dt > 1.0f / 30.0f)
    dt = 1.0f / 30.0f;

  bool hasPad = pl.gamepadId >= 0 && IsGamepadAvailable(pl.gamepadId);

  // ---------- Mirar ----------
  if (allowMouseLook) {
    Vector2 md = GetMouseDelta();
    pl.yaw += md.x * C::MOUSE_SENS_BASE * cfg.mouseSens;
    float sign = cfg.invertY ? 1.0f : -1.0f;
    pl.pitch += md.y * C::MOUSE_SENS_BASE * cfg.mouseSens * sign;
  }
  if (hasPad) {
    float lx = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_RIGHT_X);
    float ly = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_RIGHT_Y);
    const float dz = 0.18f;
    if (fabsf(lx) < dz)
      lx = 0.0f;
    if (fabsf(ly) < dz)
      ly = 0.0f;
    pl.yaw += lx * 2.6f * dt * cfg.stickSens;
    float sign = cfg.invertY ? 1.0f : -1.0f;
    pl.pitch += ly * 1.9f * dt * cfg.stickSens * sign;
  }
  pl.pitch = pl.pitch > C::PITCH_MAX    ? C::PITCH_MAX
             : pl.pitch < -C::PITCH_MAX ? -C::PITCH_MAX
                                        : pl.pitch;

  // ---------- Wish direction (teclado + mando) ----------
  float fx = sinf(pl.yaw), fz = -cosf(pl.yaw);
  float rx = cosf(pl.yaw), rz = sinf(pl.yaw);
  float mx = 0, mz = 0;
  if (IsKeyDown(kb.up)) {
    mx += fx;
    mz += fz;
  }
  if (IsKeyDown(kb.down)) {
    mx -= fx;
    mz -= fz;
  }
  if (IsKeyDown(kb.right)) {
    mx += rx;
    mz += rz;
  }
  if (IsKeyDown(kb.left)) {
    mx -= rx;
    mz -= rz;
  }
  if (hasPad) {
    float mvX = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_LEFT_X);
    float mvY = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_LEFT_Y);
    const float dz = 0.18f;
    if (fabsf(mvX) < dz)
      mvX = 0.0f;
    if (fabsf(mvY) < dz)
      mvY = 0.0f;
    mx += rx * mvX - fx * mvY;
    mz += rz * mvX - fz * mvY;
  }
  float wishLen = sqrtf(mx * mx + mz * mz);
  Vector3 wishDir = {0, 0, 0};
  if (wishLen > 0.01f) {
    wishDir.x = mx / wishLen;
    wishDir.z = mz / wishLen;
  }
  bool moving = (wishLen > 0.01f);

  // ---------- Sprint / stamina ----------
  bool wantSprint =
      IsKeyDown(KEY_LEFT_SHIFT) ||
      (hasPad &&
       IsGamepadButtonDown(pl.gamepadId, GAMEPAD_BUTTON_LEFT_TRIGGER_2));
  bool canSprint = !pl.exhausted && pl.stamina > 0.0f && moving;
  pl.sprinting = wantSprint && canSprint;

  if (pl.sprinting && !Debug::infiniteStamina) {
    pl.stamina -= cfg.sprintDrain * dt;
    if (pl.stamina <= 0.0f) {
      pl.stamina = 0.0f;
      pl.exhausted = true;
    }
    pl.staminaDelay = C::STAM_REGEN_DELAY;
  } else if (Debug::infiniteStamina) {
    pl.stamina = 1.0f;
    pl.exhausted = false;
  } else {
    if (pl.staminaDelay > 0.0f)
      pl.staminaDelay -= dt;
    else {
      pl.stamina += cfg.sprintRegen * dt;
      if (pl.stamina > 1.0f)
        pl.stamina = 1.0f;
    }
    if (pl.exhausted && pl.stamina >= C::STAM_MIN_START)
      pl.exhausted = false;
  }

  // ---------- Movimiento ----------
  float hSpeed = 0.0f;
  const Vector3 safePos = pl.pos; // última posición válida (anti-atasco)

  if (cfg.quakeMovement) {
    float wishSpeed = C::Q_WALKSPEED * (pl.sprinting ? C::Q_SPRINT_MULT : 1.0f);

    if (pl.onGround) {
      float speed =
          sqrtf(pl.velocity.x * pl.velocity.x + pl.velocity.z * pl.velocity.z);
      if (speed > 0.01f) {
        float drop = speed * C::Q_FRICTION * dt;
        float newSpeed = (speed - drop > 0.0f) ? (speed - drop) : 0.0f;
        float scale = newSpeed / speed;
        pl.velocity.x *= scale;
        pl.velocity.z *= scale;
      }
    }

    float accel = pl.onGround ? C::Q_ACCEL : C::Q_AIRACCEL;
    float currentSpeed = pl.velocity.x * wishDir.x + pl.velocity.z * wishDir.z;
    float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed > 0.0f) {
      float accelSpeed = accel * wishSpeed * dt;
      if (accelSpeed > addSpeed)
        accelSpeed = addSpeed;
      pl.velocity.x += accelSpeed * wishDir.x;
      pl.velocity.z += accelSpeed * wishDir.z;
    }

    if (!pl.onGround) {
      pl.velocity.y -= C::Q_GRAVITY * dt;
    }

    bool jumpPressed =
        IsKeyDown(KEY_SPACE) ||
        (hasPad &&
         IsGamepadButtonDown(pl.gamepadId, GAMEPAD_BUTTON_LEFT_FACE_UP));

    if (jumpPressed && pl.onGround) {
      if (!pl.jumpHeld || cfg.autoBhop) {
        pl.velocity.y = C::Q_JUMPSPEED;
        pl.onGround = false;
      }
    }
    pl.jumpHeld = jumpPressed;

    // Integración XZ con step-up / rampas
    float dx = pl.velocity.x * dt;
    float dz = pl.velocity.z * dt;
    if (Debug::noClip) {
      pl.pos.x += dx;
      pl.pos.z += dz;
    } else {
      // Un eje cada vez (deslizamiento contra paredes). La altura de
      // referencia se recalcula tras mover el primer eje.
      auto tryAxis = [&](float ax, float az, float &vel) {
        float refZ =
            pl.onGround ? maze.groundZAtWorld(pl.pos.x, pl.pos.z) : pl.pos.y;
        if (canWalk(maze, pl.pos.x, pl.pos.z, pl.pos.x + ax, pl.pos.z + az,
                    refZ)) {
          pl.pos.x += ax;
          pl.pos.z += az;
        } else {
          vel = 0.0f;
        }
      };
      tryAxis(dx, 0.0f, pl.velocity.x);
      tryAxis(0.0f, dz, pl.velocity.z);
    }

    revertIfInsideWall(pl, maze, safePos);

    // Y: suelo actual + física vertical
    pl.groundZ = maze.groundZAtWorld(pl.pos.x, pl.pos.z);

    // FIX RAMPAS: si estaba en tierra y no está saltando, "pegarse" al suelo
    // al bajar por una rampa o escalón pequeño. Sin esto el jugador pasaba
    // a estar en el aire en cada bajada (sin fricción, sin poder saltar,
    // cayendo a trompicones).
    bool wasGrounded = pl.onGround && pl.velocity.y <= 0.0f;
    bool onStairs = maze.stairAtWorld(pl.pos.x, pl.pos.z) != 0 ||
                    maze.stairAtWorld(safePos.x, safePos.z) != 0;
    float snapMax = (onStairs ? maxStepUp() : C::STEP_MAX) + 0.05f;
    if (wasGrounded && pl.pos.y > pl.groundZ &&
        pl.pos.y - pl.groundZ <= snapMax) {
      pl.pos.y = pl.groundZ;
    }

    pl.pos.y += pl.velocity.y * dt;

    if (pl.pos.y <= pl.groundZ) {
      pl.pos.y = pl.groundZ;
      pl.velocity.y = 0.0f;
      pl.onGround = true;
    } else {
      pl.onGround = false;
    }

    hSpeed =
        sqrtf(pl.velocity.x * pl.velocity.x + pl.velocity.z * pl.velocity.z);

    pushOutOfWall(pl, maze);
  } else {
    // ---------- MODO CLÁSICO ----------
    // Sin física vertical, pero pos.y siempre anclado al suelo actual.
    if (!moving) {
      pl.groundZ = maze.groundZAtWorld(pl.pos.x, pl.pos.z);
      pl.pos.y = pl.groundZ;
      pl.velocity = {0, 0, 0};
      pl.onGround = true;
      return false;
    }

    float startX = pl.pos.x, startZ = pl.pos.z;
    float speed = C::P_SPEED * (pl.sprinting ? C::P_SPRINT : 1.0f);
    float dx = mx / wishLen * speed * dt;
    float dz = mz / wishLen * speed * dt;

    if (Debug::noClip) {
      pl.pos.x += dx;
      pl.pos.z += dz;
    } else {
      auto tryAxis = [&](float ax, float az) {
        float refZ = maze.groundZAtWorld(pl.pos.x, pl.pos.z);
        if (canWalk(maze, pl.pos.x, pl.pos.z, pl.pos.x + ax, pl.pos.z + az,
                    refZ)) {
          pl.pos.x += ax;
          pl.pos.z += az;
        }
      };
      tryAxis(dx, 0.0f);
      tryAxis(0.0f, dz);
    }

    revertIfInsideWall(pl, maze, safePos);

    pl.groundZ = maze.groundZAtWorld(pl.pos.x, pl.pos.z);
    pl.pos.y = pl.groundZ;
    pl.velocity = {0, 0, 0};
    pl.onGround = true;

    pushOutOfWall(pl, maze);

    // Velocidad real (horizontal): no suenan pasos si estás pegado a una pared
    float mdx = pl.pos.x - startX, mdz = pl.pos.z - startZ;
    hSpeed = (dt > 0.0f) ? sqrtf(mdx * mdx + mdz * mdz) / dt : 0.0f;
  }

  return hSpeed > 0.5f;
}
