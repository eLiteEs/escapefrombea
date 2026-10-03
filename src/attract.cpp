#include "attract.h"
#include <cmath>
#include <algorithm>

void AttractMode::init(int size, uint32_t seed, const Assets& assets) {
    rng_.seed(seed);
    maze_.generate(size, size, seed);
    world_.build(maze_, assets);

    pos_ = maze_.cellCenter(1, 1);

    // Yaw inicial aleatorio
    std::uniform_real_distribution<float> yawDist(0.0f, 2.0f * PI);
    yaw_ = targetYaw_ = yawDist(rng_);
    dirTimer_ = 2.0f;
    pickCd_   = 0.0f;
    bobT_     = 0.0f;
}

void AttractMode::unload() {
    world_.unload();
}

void AttractMode::pickNewDirection() {
    const float checkDist = 1.5f;

    // 8 candidatos a 45 grados
    float candidates[8];
    for (int i = 0; i < 8; ++i)
        candidates[i] = yaw_ + (float)i * (PI * 0.25f);

    std::shuffle(candidates, candidates + 8, rng_);

    for (float c : candidates) {
        float nx = pos_.x + sinf(c) * checkDist;
        float nz = pos_.z - cosf(c) * checkDist;
        if (!maze_.circleHitsWall(nx, nz, C::P_RADIUS)) {
            targetYaw_ = c;
            dirTimer_  = 1.5f + (float)(rng_() % 250) / 100.0f;   // 1.5 - 4.0 s
            pickCd_    = 0.5f;                                    // anti re-pick
            return;
        }
    }

    // Sin salida: girar 180 grados
    targetYaw_ = yaw_ + PI;
    dirTimer_  = 1.0f;
    pickCd_    = 0.5f;
}

void AttractMode::update(float dt) {
    dirTimer_ -= dt;
    pickCd_   -= dt;

    // Giro suave hacia el objetivo
    float diff = targetYaw_ - yaw_;
    while (diff >  PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;

    const float maxTurn = 2.5f * dt;
    if (fabsf(diff) <= maxTurn) yaw_ = targetYaw_;
    else                        yaw_ += (diff > 0.0f ? maxTurn : -maxTurn);

    // Direccion actual
    Vector2 dir = { sinf(yaw_), -cosf(yaw_) };

    // Comprobar pared delante
    float ax = pos_.x + dir.x * 1.0f;
    float az = pos_.z + dir.y * 1.0f;
    bool blocked = maze_.circleHitsWall(ax, az, C::P_RADIUS);

    if ((dirTimer_ <= 0.0f || blocked) && pickCd_ <= 0.0f)
        pickNewDirection();

    // Movimiento con colision (deslizamiento por paredes)
    const float speed = 2.0f;
    float mx = dir.x * speed * dt;
    float mz = dir.y * speed * dt;

    if (!maze_.circleHitsWall(pos_.x + mx, pos_.z, C::P_RADIUS)) pos_.x += mx;
    if (!maze_.circleHitsWall(pos_.x, pos_.z + mz, C::P_RADIUS)) pos_.z += mz;

    bobT_ += dt;
}

void AttractMode::draw(int W, int H) {
    (void)W; (void)H;

    Camera3D cam = {};
    float bob = sinf(bobT_ * 1.7f) * 0.05f;
    cam.position = { pos_.x, C::EYE_H + bob, pos_.z };
    Vector3 fwd  = { sinf(yaw_), 0.0f, -cosf(yaw_) };
    cam.target   = Vector3Add(cam.position, fwd);
    cam.up       = { 0, 1, 0 };
    cam.fovy     = (float)C::FOV;
    cam.projection = CAMERA_PERSPECTIVE;

    float mw = maze_.w * C::CELL;
    float mh = maze_.h * C::CELL;

    BeginMode3D(cam);
        // Suelo
        if (world_.floorModel.meshCount > 0)
            DrawModel(world_.floorModel, { mw * 0.5f, 0.0f, mh * 0.5f }, 1.0f, WHITE);
        else
            DrawPlane({ mw/2, 0, mh/2 }, { mw, mh }, (Color){ 32, 32, 42, 255 });

        // Paredes (misma logica que gameplay, sin portal ni enemigos)
        const float rd = 60.0f;
        for (int y = 0; y < maze_.h; ++y)
            for (int x = 0; x < maze_.w; ++x) {
                if (maze_.g[y][x] != 1) continue;

                float wx = x * C::CELL + C::CELL * 0.5f;
                float wz = y * C::CELL + C::CELL * 0.5f;
                float dx = wx - pos_.x, dz = wz - pos_.z;
                if (dx*dx + dz*dz > rd*rd) continue;

                float dist = sqrtf(dx*dx + dz*dz);
                float distTint = 1.0f - (dist / rd) * 0.30f;
                if (distTint < 0.35f) distTint = 0.35f;
                float parity = ((x + y) & 1) ? 0.93f : 1.0f;
                float tint = distTint * parity;

                Color wallTint = {
                    (unsigned char)(255 * tint),
                    (unsigned char)(255 * tint),
                    (unsigned char)(255 * tint),
                    255
                };

                int variant = 0;
                if (y < (int)maze_.wallVariant.size() &&
                    x < (int)maze_.wallVariant[y].size())
                    variant = maze_.wallVariant[y][x];

                const Model* model = &world_.wallModelTile;
                if      (variant == 1 && world_.wallModelLocker.meshCount > 0)
                    model = &world_.wallModelLocker;
                else if (variant == 2 && world_.wallModelBrick.meshCount > 0)
                    model = &world_.wallModelBrick;

                if (model->meshCount > 0)
                    DrawModelEx(*model,
                                { wx, C::WALL_H * 0.5f, wz },
                                { 0, 1, 0 }, 0.0f,
                                { 1.0f, 1.0f, 1.0f },
                                wallTint);
                else
                    DrawCube({ wx, C::WALL_H * 0.5f, wz },
                             C::CELL, C::WALL_H, C::CELL, wallTint);
            }

        // Techo
        DrawCube({ mw/2, C::WALL_H + 0.05f, mh/2 }, mw, 0.1f, mh,
                 (Color){ 18, 18, 26, 255 });
    EndMode3D();
}

