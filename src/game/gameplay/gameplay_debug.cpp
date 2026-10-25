#include "gameplay_debug.hpp"

#include "enemy.hpp"
#include "game.hpp"

void Game::handleDebugHotkeys() {
    if (state == ST_PLAYING || state == ST_PAUSED) {
        if (IsKeyPressed(KEY_F3))
            Debug::showOverlay = !Debug::showOverlay;
        if (IsKeyPressed(KEY_F4))
            Debug::showBounds = !Debug::showBounds;
        if (IsKeyPressed(KEY_F5))
            Debug::showVision = !Debug::showVision;
        if (IsKeyPressed(KEY_F6))
            Debug::showPaths = !Debug::showPaths;
        if (IsKeyPressed(KEY_F7))
            Debug::godMode = !Debug::godMode;
        if (IsKeyPressed(KEY_F8))
            Debug::infiniteStamina = !Debug::infiniteStamina;
        if (IsKeyPressed(KEY_F9))
            Debug::noClip = !Debug::noClip;

        if (state == ST_PLAYING) {
            if (IsKeyPressed(KEY_F10))
                debugRevealMap();
            if (IsKeyPressed(KEY_F11))
                debugTeleportToExit();
            if (IsKeyPressed(KEY_KP_DECIMAL))
                Debug::cycleTimeScale();
            if (IsKeyPressed(KEY_KP_0))
                debugSpawnEnemy();
            if (IsKeyPressed(KEY_KP_1))
                debugKillAll();
            if (IsKeyPressed(KEY_KP_2)) {
                startLevel(level);
                assignGamepads();
                DisableCursor();
                showToast("Nivel reiniciado");
            }
            if (IsKeyPressed(KEY_KP_3)) {
                ++level;
                startLevel(level);
                assignGamepads();
                DisableCursor();
                showToast(TextFormat("Nivel %d", level));
            }
            if (IsKeyPressed(KEY_KP_4) && level > 1) {
                --level;
                startLevel(level);
                assignGamepads();
                DisableCursor();
                showToast(TextFormat("Nivel %d", level));
            }
        }
    }
}


void Game::debugSpawnEnemy() {
    if (enemies.size() >= 20) {
        showToast("Limite de enemigos (20)");
        return;
    }
    Enemy e;
    e.pos = players[0].pos;
    e.pos.x += 1.5f;
    e.pos.z += 1.5f;
    if (maze.wallAtWorld(e.pos.x, e.pos.z)) {
        e.pos.x = players[0].pos.x - 1.5f;
        e.pos.z = players[0].pos.z - 1.5f;
    }
    e.dir = {1, 0};
    e.speed = C::BASE_ESPD;
    e.kind = (int)(enemies.size() % ENEMY_KIND_COUNT);
    e.prevPos = e.pos;
    e.memoryT = 0.0f;
    e.repathT = 0.0f;
    e.stuckT = 0.0f;
    e.stunT = 0.0f;
    e.rageT = 0.0f;
    e.rageMult = 1.0f;
    e.hitFlash = 0.0f;
    enemies.push_back(e);
    showToast("Enemigo spawneado");
}

void Game::debugKillAll() {
    int n = (int)enemies.size();
    enemies.clear();
    showToast(TextFormat("Eliminados %d enemigos", n));
}

void Game::debugTeleportToExit() {
    if (players[0].caught || players[0].escaped)
        return;
    players[0].pos = exitPos;
    showToast("Teletransportado a la salida");
}

void Game::debugRevealMap() {
    for (int y = 0; y < maze.h; ++y)
        for (int x = 0; x < maze.w; ++x)
            maze.explored[y][x] = true;
    showToast("Mapa revelado");
}

void Game::drawDebugWorld3D(const Player &pl) {
    (void)pl;
    if (!Debug::showOverlay)
        return;

    if (Debug::showBounds) {
        for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
            if (players[i].caught || players[i].escaped)
                continue;
            DrawCircle3D({players[i].pos.x, 0.05f, players[i].pos.z}, C::P_RADIUS,
                         {1, 0, 0}, 90.0f, (Color){255, 220, 80, 220});
        }
        for (auto &e : enemies) {
            Color c = (e.rageT > 0.0f)   ? RED
            : (e.stunT > 0.0f) ? GRAY
            : (Color){80, 180, 255, 220};
            DrawCircle3D({e.pos.x, 0.05f, e.pos.z}, C::E_RADIUS, {1, 0, 0}, 90.0f, c);
        }
        for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
            if (players[i].caught || players[i].escaped)
                continue;
            DrawCircle3D({players[i].pos.x, 0.06f, players[i].pos.z}, C::CATCH_R,
                         {1, 0, 0}, 90.0f, (Color){255, 0, 0, 80});
        }
    }

    if (Debug::showVision) {
        for (auto &e : enemies) {
            Vector3 eye = {e.pos.x, 1.4f, e.pos.z};
            int target = -1;
            for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
                if (players[i].caught || players[i].escaped || players[i].hidden)
                    continue;
                if (Vector3Distance(e.pos, players[i].pos) < C::SEE_DIST &&
                    lineOfSight(maze, e.pos, players[i].pos)) {
                    target = i;
                break;
                    }
            }
            if (target >= 0) {
                Vector3 tgt = {players[target].pos.x, 1.4f, players[target].pos.z};
                DrawLine3D(eye, tgt, (Color){0, 255, 0, 200});
            } else if (e.memoryT > 0.0f) {
                Vector3 mem = {e.lastSeen.x, 1.4f, e.lastSeen.z};
                DrawLine3D(eye, mem, (Color){255, 220, 0, 180});
            } else {
                Vector3 fwd = {eye.x + e.dir.x * 3.0f, eye.y, eye.z + e.dir.y * 3.0f};
                DrawLine3D(eye, fwd, (Color){200, 200, 200, 150});
            }
        }
    }

    if (Debug::showPaths) {
        for (auto &e : enemies) {
            if (e.memoryT <= 0.0f)
                continue;
            DrawLine3D({e.pos.x, 0.2f, e.pos.z}, {e.lastSeen.x, 0.2f, e.lastSeen.z},
                       (Color){255, 100, 255, 180});
        }
        for (auto &e : enemies) {
            float s = 0.3f;
            DrawLine3D({e.pos.x - s, 0.1f, e.pos.z}, {e.pos.x + s, 0.1f, e.pos.z},
                       MAGENTA);
            DrawLine3D({e.pos.x, 0.1f, e.pos.z - s}, {e.pos.x, 0.1f, e.pos.z + s},
                       MAGENTA);
        }
    }
}

void Game::drawDebugOverlay(int vw, int vh, int playerNum) {
    (void)playerNum;
    (void)vw;
    (void)vh;
    if (!Debug::showOverlay)
        return;

    const int pad = 10;
    const int lineH = 16;
    const int font = 14;
    const int width = 460;

    int lines = 0;
    lines += 3;
    lines += 3;
    lines += 6;
    if (twoPlayers)
        lines += 6;
    lines += 4;
    lines += 2 + std::min<int>((int)enemies.size(), 6);
    if (enemies.size() > 6)
        lines += 1;
    lines += 1 + 8;

    int height = pad * 2 + lines * lineH;

    DrawRectangle(12, 12, width, height, (Color){0, 0, 0, 210});
    DrawRectangleLines(12, 12, width, height, (Color){80, 255, 100, 220});

    int tx = 12 + pad;
    int ty = 12 + pad;

    auto line = [&](Color c, const char *txt) {
        DrawText(txt, tx, ty, font, c);
        ty += lineH;
    };

    const Color titleC = (Color){100, 255, 120, 255};
    const Color labelC = (Color){180, 180, 180, 255};
    const Color valC = (Color){240, 240, 240, 255};
    const Color offC = (Color){130, 130, 130, 255};
    const Color warnC = (Color){255, 200, 80, 255};

    char buf[256];

    line(titleC, TextFormat("%s  %s", C::GAME_TITLE, Debug::version()));
    snprintf(buf, sizeof(buf), "Build: %s  |  %s", Debug::buildString(),
             Debug::compilerName());
    line(labelC, buf);
    line(labelC, "F3: toggle overlay   |   Numpad: cheats (ver abajo)");

    snprintf(buf, sizeof(buf), "FPS: %d   Frame: %.2f ms   TimeScale: %.2fx",
             GetFPS(), GetFrameTime() * 1000.0f, Debug::timeScale);
    line(valC, buf);
    snprintf(buf, sizeof(buf), "Estado: %d   Modo: %s   Jugadores: %d",
             (int)state, (mode == MODE_STORY ? "Historia" : "Infinito"),
             twoPlayers ? 2 : 1);
    line(valC, buf);
    snprintf(buf, sizeof(buf), "Nivel: %d   Tiempo: %.2fs   Playtime: %.1fs",
             level, levelTime, totalPlaytime);
    line(valC, buf);

    for (int i = 0; i < (twoPlayers ? 2 : 1); ++i) {
        const Player &p = players[i];
        Vector2 cell = maze.worldToCell(p.pos.x, p.pos.z);

        Color hc =
        (i == 0) ? (Color){255, 200, 80, 255} : (Color){120, 200, 255, 255};
        snprintf(buf, sizeof(buf), "--- JUGADOR %d ---", i + 1);
        line(hc, buf);
        snprintf(buf, sizeof(buf), "Pos: (%.2f, %.2f)  Cell: (%d, %d)", p.pos.x,
                 p.pos.z, (int)cell.x, (int)cell.y);
        line(valC, buf);
        snprintf(buf, sizeof(buf), "Yaw: %.1f  Pitch: %.1f  Stamina: %.0f%%",
                 p.yaw * 180.0f / PI, p.pitch * 180.0f / PI, p.stamina * 100.0f);
        line(valC, buf);

        const InvItem &sel = p.inventory[p.selectedSlot];
        const char *iname = (sel.kind == ITEM_NONE || sel.count <= 0)
        ? "-"
        : Inventory::itemName(sel.kind);
        snprintf(buf, sizeof(buf), "Slot %d: %s x%d  Sprint: %s  Cd: %.2f",
                 p.selectedSlot + 1, iname, sel.count, p.sprinting ? "SI" : "no",
                 p.throwCooldown);
        line(valC, buf);

        snprintf(buf, sizeof(buf), "Flags: %s%s%s", p.caught ? "[CAUGHT] " : "",
                 p.escaped ? "[ESCAPED] " : "", p.exhausted ? "[EXHAUSTED]" : "");
        line(p.caught ? warnC : valC, buf);
    }

    line(titleC, "--- MUNDO ---");
    snprintf(buf, sizeof(buf), "Maze: %dx%d  Seed: 0x%08X", maze.w, maze.h,
             currentSeed);
    line(valC, buf);
    snprintf(buf, sizeof(buf),
             "Salida: (%.1f, %.1f)  Enemigos: %d  Proyectiles: %d", exitPos.x,
             exitPos.z, (int)enemies.size(), (int)projectiles.size());
    line(valC, buf);
    snprintf(buf, sizeof(buf), "Render dist: %.0fm   GUI scale: %.2fx",
             renderDist(), cfg.guiScale);
    line(valC, buf);

    snprintf(buf, sizeof(buf), "--- ENEMIGOS (%d) ---", (int)enemies.size());
    line(titleC, buf);
    int shown = std::min<int>((int)enemies.size(), 6);
    for (int i = 0; i < shown; ++i) {
        const Enemy &e = enemies[i];
        const char *kind = "BEA";
        if (e.kind == ENEMY_MARISA)
            kind = "MAR";
        else if (e.kind == ENEMY_ALT)
            kind = "ALT";

        float d0 = Vector3Distance(e.pos, players[0].pos);
        const char *st = "wander";
        if (e.stunT > 0)
            st = "STUN";
        else if (e.rageT > 0)
            st = "RAGE";
        else if (e.memoryT > 0)
            st = "chase";

        snprintf(buf, sizeof(buf),
                 "#%d %s  d=%.1fm  spd=%.2f  %s  mem=%.1f  rage=%.1f", i, kind, d0,
                 e.speed * e.rageMult, st, e.memoryT, e.rageT);
        line(e.rageT > 0 ? warnC : valC, buf);
    }
    if ((int)enemies.size() > 6) {
        snprintf(buf, sizeof(buf), "... y %d mas", (int)enemies.size() - 6);
        line(offC, buf);
    }

    line(titleC, "--- CHEATS / VISUALIZACION ---");
    auto toggleStr = [](bool b) { return b ? "ON " : "off"; };
    snprintf(buf, sizeof(buf), "[F4] Bounds:%s  [F5] Vision:%s  [F6] Paths:%s",
             toggleStr(Debug::showBounds), toggleStr(Debug::showVision),
             toggleStr(Debug::showPaths));
    line(valC, buf);
    snprintf(buf, sizeof(buf), "[F7] GodMode:%s  [F8] InfStam:%s  [F9] NoClip:%s",
             toggleStr(Debug::godMode), toggleStr(Debug::infiniteStamina),
             toggleStr(Debug::noClip));
    line(valC, buf);
    line(valC, "[F10] Reveal   [F11] Teleport   [NUM.] TimeScale");
    line(valC, "[NUM0] Spawn   [NUM1] Kill all   [NUM2] Restart");
    line(valC, "[NUM3] Next lvl   [NUM4] Prev lvl");
}
