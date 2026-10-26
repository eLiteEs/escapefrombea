#include "gameplay_jumpscare.hpp"

#include "game.hpp"

#include <algorithm>

void Game::startJumpscare(int enemyIdx) {
    jumpscareEnemy = enemyIdx;
    jumpscareTimer = C::JUMPSCARE_DURATION;
    state = ST_JUMPSCARE;
    EnableCursor();
}

void Game::drawJumpscare() {
    BeginDrawing();
    ClearBackground(BLACK);

    if (jumpscareEnemy < 0 || jumpscareEnemy >= (int)enemies.size()) {
        EndDrawing();
        return;
    }

    const Enemy &e = enemies[jumpscareEnemy];
    int kind = std::clamp(e.kind, 0, ENEMY_KIND_COUNT - 1);
    Texture2D tex = assets.enemyTex[kind];

    int W = GetScreenWidth();
    int H = GetScreenHeight();

    float t = 1.0f - (jumpscareTimer / C::JUMPSCARE_DURATION);
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    float sizeScale;
    if (t < C::JUMPSCARE_ZOOM_IN) {
        float k = t / C::JUMPSCARE_ZOOM_IN;
        k = 1.0f - (1.0f - k) * (1.0f - k);
        sizeScale = 0.5f + k * 1.2f;
    } else {
        sizeScale = 1.7f;
    }

    float shake = 0.0f;
    if (t >= C::JUMPSCARE_ZOOM_IN) {
        float k = (t - C::JUMPSCARE_ZOOM_IN) / (1.0f - C::JUMPSCARE_ZOOM_IN);
        shake = (1.0f - k) * 14.0f;
    }

    float dx = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;
    float dy = ((float)GetRandomValue(-100, 100) / 100.0f) * shake;
    float drawSize = (float)H * sizeScale;
    float x = W / 2.0f + dx;
    float y = H / 2.0f + dy;

    Color tint = WHITE;
    if (t < 0.2f) {
        unsigned char k = (unsigned char)((1.0f - t / 0.2f) * 100);
        tint =
        (Color){255, (unsigned char)(255 - k), (unsigned char)(255 - k), 255};
    }

    if (tex.id != 0) {
        Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
        Rectangle dst = {x - drawSize / 2.0f, y - drawSize / 2.0f, drawSize,
            drawSize};
            DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
    } else {
        Color c = (kind == ENEMY_BEA)      ? (Color){210, 45, 60, 255}
        : (kind == ENEMY_MARISA) ? (Color){60, 90, 210, 255}
        : (Color){160, 40, 200, 255};
        DrawCircle((int)x, (int)y, drawSize * 0.4f, c);
    }

    if (t < 0.08f) {
        unsigned char a = (unsigned char)((1.0f - t / 0.08f) * 255);
        DrawRectangle(0, 0, W, H, (Color){255, 255, 255, a});
    }
    if (t > 0.85f) {
        float k = (t - 0.85f) / 0.15f;
        unsigned char a = (unsigned char)(k * 255);
        DrawRectangle(0, 0, W, H, (Color){0, 0, 0, a});
    }
    EndDrawing();
}
