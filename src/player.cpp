#include "debug.h"
#include "player.h"
#include <cmath>

static void moveEntity(const Maze& maze, float& x, float& z, float dx, float dz, float r) {
    if (Debug::noClip) {
        x += dx;
        z += dz;
        return;
    }
    if (!maze.circleHitsWall(x + dx, z, r)) x += dx;
    if (!maze.circleHitsWall(x, z + dz, r)) z += dz;
}

bool updatePlayer(Player& pl, const KeyBindings& kb, float dt,
                  bool allowMouseLook, const Config& cfg, const Maze& maze)
{
    if (pl.caught || pl.escaped) return false;
    if (pl.hidden) return false;

    bool hasPad = pl.gamepadId >= 0 && IsGamepadAvailable(pl.gamepadId);

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
        if (fabsf(lx) < dz) lx = 0.0f;
        if (fabsf(ly) < dz) ly = 0.0f;
        pl.yaw += lx * 2.6f * dt * cfg.stickSens;
        float sign = cfg.invertY ? 1.0f : -1.0f;
        pl.pitch += ly * 1.9f * dt * cfg.stickSens * sign;
    }
    pl.pitch = pl.pitch >  C::PITCH_MAX ?  C::PITCH_MAX :
               pl.pitch < -C::PITCH_MAX ? -C::PITCH_MAX : pl.pitch;

    float fx = sinf(pl.yaw), fz = -cosf(pl.yaw);
    float rx = cosf(pl.yaw), rz =  sinf(pl.yaw);

    float mx = 0, mz = 0;
    if (IsKeyDown(kb.up))    { mx += fx; mz += fz; }
    if (IsKeyDown(kb.down))  { mx -= fx; mz -= fz; }
    if (IsKeyDown(kb.right)) { mx += rx; mz += rz; }
    if (IsKeyDown(kb.left))  { mx -= rx; mz -= rz; }

    if (hasPad) {
        float mvX = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_LEFT_X);
        float mvY = GetGamepadAxisMovement(pl.gamepadId, GAMEPAD_AXIS_LEFT_Y);
        const float dz = 0.18f;
        if (fabsf(mvX) < dz) mvX = 0.0f;
        if (fabsf(mvY) < dz) mvY = 0.0f;
        mx += rx * mvX - fx * mvY;
        mz += rz * mvX - fz * mvY;
    }

    float len = sqrtf(mx*mx + mz*mz);
    bool moving = (len > 0.01f);

    bool wantSprint = IsKeyDown(KEY_LEFT_SHIFT) ||
                      (hasPad && IsGamepadButtonDown(pl.gamepadId,
                                                     GAMEPAD_BUTTON_LEFT_TRIGGER_2));
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
        if (pl.staminaDelay > 0.0f) pl.staminaDelay -= dt;
        else {
            pl.stamina += cfg.sprintRegen * dt;
            if (pl.stamina > 1.0f) pl.stamina = 1.0f;
        }
        if (pl.exhausted && pl.stamina >= C::STAM_MIN_START)
            pl.exhausted = false;
    }

    if (!moving) return false;

    float speed = C::P_SPEED * (pl.sprinting ? C::P_SPRINT : 1.0f);
    moveEntity(maze, pl.pos.x, pl.pos.z,
               mx/len * speed * dt, mz/len * speed * dt, C::P_RADIUS);
    return true;
}

