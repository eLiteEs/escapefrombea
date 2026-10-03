#include "enemy.h"
#include <cmath>
#include <random>
#include <vector>

bool lineOfSight(const Maze& maze, Vector3 a, Vector3 b) {
    if (maze.wallAtWorld(a.x, a.z)) return false;
    float dx = b.x - a.x, dz = b.z - a.z;
    float dist = sqrtf(dx*dx + dz*dz);
    int steps = (int)(dist / (C::CELL * 0.25f)) + 1;
    for (int i = 1; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        if (maze.wallAtWorld(a.x + dx*t, a.z + dz*t)) return false;
    }
    return true;
}

static void moveEntity(const Maze& maze, float& x, float& z, float dx, float dz, float r) {
    if (!maze.circleHitsWall(x + dx, z, r)) x += dx;
    if (!maze.circleHitsWall(x, z + dz, r)) z += dz;
}

static int closestVisibleTarget(const Enemy& self,
                                const Maze& maze, Player* players, int n)
{
    int best = -1;
    float bestDist = C::SEE_DIST;
    for (int p = 0; p < n; ++p) {
        if (players[p].caught || players[p].escaped) continue;
        if (players[p].hidden) continue; 
	float dx = players[p].pos.x - self.pos.x;
        float dz = players[p].pos.z - self.pos.z;
        float d = sqrtf(dx*dx + dz*dz);
        if (d < bestDist && lineOfSight(maze, self.pos, players[p].pos)) {
            bestDist = d; best = p;
        }
    }
    return best;
}

static void separateEnemies(std::vector<Enemy>& enemies, size_t i, float dt) {
    Vector3 push = { 0, 0, 0 };
    for (size_t j = 0; j < enemies.size(); ++j) {
        if (i == j) continue;
        float dx = enemies[i].pos.x - enemies[j].pos.x;
        float dz = enemies[i].pos.z - enemies[j].pos.z;
        float d2 = dx*dx + dz*dz;
        float minD = C::E_RADIUS * 2.2f;
        if (d2 > 0.0001f && d2 < minD*minD) {
            float d = sqrtf(d2);
            push.x += (dx/d) * (minD - d) * 0.5f;
            push.z += (dz/d) * (minD - d) * 0.5f;
        }
    }
    enemies[i].pos.x += push.x * dt * 6.0f;
    enemies[i].pos.z += push.z * dt * 6.0f;
}

void updateEnemies(std::vector<Enemy>& enemies, const Maze& maze,
                   Player* players, int numPlayers, float dt,
                   float seeDist, float memoryT)
{
    (void)seeDist;
    for (size_t i = 0; i < enemies.size(); ++i) {
        Enemy& e = enemies[i];
        e.prevPos = e.pos;
        e.repathT -= dt;

        int target = closestVisibleTarget(e, maze, players, numPlayers);

        // Stun: no hace nada
        if (e.stunT > 0.0f) {
            separateEnemies(enemies, i, dt);
            if (!Debug::godMode) {
                for (int p = 0; p < numPlayers; ++p) {
                    if (players[p].caught || players[p].escaped) continue;
                    if (players[p].hidden) continue;   // <-- NUEVO
                    if (Vector3Distance(e.pos, players[p].pos) < C::CATCH_R)
                        players[p].caught = true;
                }
            }
            continue;
        }

        // Rage: multiplicador de velocidad + siempre te tiene localizado
        float effectiveSpeed = e.speed * e.rageMult;
        if (e.rageT > 0.0f) {
            int nearest = -1;
            float best = 1e9f;
            for (int p = 0; p < numPlayers; ++p) {
                if (players[p].caught || players[p].escaped) continue;
                if (players[p].hidden) continue;   // <-- NUEVO
                float d = Vector3Distance(players[p].pos, e.pos);
                if (d < best) { best = d; nearest = p; }
            }
            if (nearest >= 0) {
                e.lastSeen = players[nearest].pos;
                e.memoryT  = memoryT;
            }
        }

        if (target >= 0) {
            e.lastSeen = players[target].pos;
            e.memoryT = memoryT;
            float dx = players[target].pos.x - e.pos.x;
            float dz = players[target].pos.z - e.pos.z;
            float d = sqrtf(dx*dx + dz*dz);
            if (d > 0.01f) {
                moveEntity(maze, e.pos.x, e.pos.z,
                           (dx/d) * effectiveSpeed * dt, (dz/d) * effectiveSpeed * dt, C::E_RADIUS);
            }
        }
        else if (e.memoryT > 0.0f) {
            e.memoryT -= dt;
            float dx = e.lastSeen.x - e.pos.x;
            float dz = e.lastSeen.z - e.pos.z;
            float d = sqrtf(dx*dx + dz*dz);
            float spd = effectiveSpeed * (e.repathT <= 0.0f ? 0.9f : 0.75f);
            e.repathT = C::REPATH_CD;
            if (d > 0.01f) {
                moveEntity(maze, e.pos.x, e.pos.z,
                           (dx/d) * spd * dt, (dz/d) * spd * dt, C::E_RADIUS);
            }
        }
        else {
            e.turnT -= dt;
            float nx = e.pos.x + e.dir.x * effectiveSpeed * dt;
            float nz = e.pos.z + e.dir.y * effectiveSpeed * dt;
            if (e.turnT <= 0 || maze.circleHitsWall(nx, nz, C::E_RADIUS)) {
                std::mt19937 rng((uint32_t)(GetTime() * 1000) ^ (uint32_t)(i * 7919));
                for (int tries = 0; tries < 8; ++tries) {
                    int d = rng() % 4;
                    Vector2 nd = { (float)((d==0) - (d==1)),
                                   (float)((d==2) - (d==3)) };
                    if (!maze.circleHitsWall(e.pos.x + nd.x*0.6f,
                                             e.pos.z + nd.y*0.6f, C::E_RADIUS)) {
                        e.dir = nd;
                        break;
                    }
                }
                e.turnT = 1.0f + (rng() % 100) / 50.0f;
            }
            moveEntity(maze, e.pos.x, e.pos.z,
                       e.dir.x * effectiveSpeed * 0.55f * dt,
                       e.dir.y * effectiveSpeed * 0.55f * dt, C::E_RADIUS);
        }

        float moved2 = (e.pos.x - e.prevPos.x)*(e.pos.x - e.prevPos.x) +
                       (e.pos.z - e.prevPos.z)*(e.pos.z - e.prevPos.z);
        if (moved2 < 0.0001f) {
            e.stuckT += dt;
            if (e.stuckT > C::UNSTICK_T) {
                std::mt19937 rng((uint32_t)(GetTime() * 1000) ^ (uint32_t)(i * 3571));
                int d = rng() % 4;
                e.dir = { (float)((d==0) - (d==1)), (float)((d==2) - (d==3)) };
                e.turnT = 0.8f;
                e.stuckT = 0.0f;
            }
        } else {
            e.stuckT = 0.0f;
        }

        separateEnemies(enemies, i, dt);

        if (!Debug::godMode) {
            for (int p = 0; p < numPlayers; ++p) {
                if (players[p].caught || players[p].escaped) continue;
                if (Vector3Distance(e.pos, players[p].pos) < C::CATCH_R) {
                    players[p].caught  = true;
                    players[p].caughtBy = (int)i;
                }
            }
        }
    }
}

