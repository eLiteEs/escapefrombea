#include "debug.h"
#include "player.h"
#include <cmath>

static void moveEntity(const Maze &maze, float &x, float &z, float dx, float dz,
                       float r) {
  if (Debug::noClip) {
    x += dx;
    z += dz;
    return;
  }
  if (!maze.circleHitsWall(x + dx, z, r))
    x += dx;
  if (!maze.circleHitsWall(x, z + dz, r))
    z += dz;
}

bool updatePlayer(Player &pl, const KeyBindings &kb, float dt,
                  bool allowMouseLook, const Config &cfg, const Maze &maze) {
  if (pl.caught || pl.escaped)
    return false;
  if (pl.hidden)
    return false;

  bool hasPad = pl.gamepadId >= 0 && IsGamepadAvailable(pl.gamepadId);

  // ---------- Mirar (idéntico al original) ----------
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

  // ---------- Sprint / stamina (idéntico al original) ----------
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
  if (cfg.quakeMovement) {
    // ============ MODELO QUAKE ============
    float wishSpeed = C::Q_WALKSPEED * (pl.sprinting ? C::Q_SPRINT_MULT : 1.0f);

    // Fricción (solo en suelo)
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

    // Aceleración (suelo o aire)
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

    // Gravedad
    if (!pl.onGround) {
      pl.velocity.y -= C::Q_GRAVITY * dt;
    }

    // Salto / bhop
    bool jumpPressed =
        IsKeyDown(KEY_SPACE) ||
        (hasPad &&
         IsGamepadButtonDown(pl.gamepadId, GAMEPAD_BUTTON_LEFT_FACE_UP));

    if (jumpPressed && pl.onGround) {
      // Salto normal (primer frame) o auto-bhop (mantiene pulsado)
      if (!pl.jumpHeld || cfg.autoBhop) {
        pl.velocity.y = C::Q_JUMPSPEED;
        pl.onGround = false;
      }
    }
    pl.jumpHeld = jumpPressed;

    // Integración XZ con colisiones
    float dx = pl.velocity.x * dt;
    float dz = pl.velocity.z * dt;
    if (Debug::noClip) {
      pl.pos.x += dx;
      pl.pos.z += dz;
    } else {
      if (!maze.circleHitsWall(pl.pos.x + dx, pl.pos.z, C::P_RADIUS))
        pl.pos.x += dx;
      else
        pl.velocity.x = 0.0f;
      if (!maze.circleHitsWall(pl.pos.x, pl.pos.z + dz, C::P_RADIUS))
        pl.pos.z += dz;
      else
        pl.velocity.z = 0.0f;
    }

    // Integración Y y detección de suelo
    pl.pos.y += pl.velocity.y * dt;
    if (pl.pos.y <= pl.groundZ) {
      pl.pos.y = pl.groundZ;
      pl.velocity.y = 0.0f;
      pl.onGround = true;
    }

    // Devuelve true si hay velocidad horizontal (para footsteps)
    float hSpeed =
        sqrtf(pl.velocity.x * pl.velocity.x + pl.velocity.z * pl.velocity.z);
    return hSpeed > 0.5f;
  } else {
    // ============ MODO CLÁSICO (sin cambios de comportamiento) ============
    if (!moving)
      return false;
    float speed = C::P_SPEED * (pl.sprinting ? C::P_SPRINT : 1.0f);
    moveEntity(maze, pl.pos.x, pl.pos.z, mx / wishLen * speed * dt,
               mz / wishLen * speed * dt, C::P_RADIUS);
    return true;
  }
}
