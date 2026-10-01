#include "weapon.h"
#include <cmath>
#include <algorithm>

namespace Weapon {

void applyHit(Enemy& e, WeaponKind weapon) {
    switch (weapon) {
        case WEAPON_SLINGSHOT:
            e.stunT    = C::STUN_SLING;
            e.rageT    = C::RAGE_SLING_T;
            e.rageMult = C::RAGE_SLING_MULT;
            e.hitFlash = 0.30f;
            break;
        case WEAPON_WHIP:
            e.stunT    = C::STUN_WHIP;
            e.rageT    = C::RAGE_WHIP_T;
            e.rageMult = C::RAGE_WHIP_MULT;
            e.hitFlash = 0.20f;
            break;
        default: break;
    }
}

bool tryAttack(Player& p, const Maze& maze,
               std::vector<Projectile>& projectiles,
               std::vector<Enemy>& enemies)
{
    (void)maze;
    if (p.attackCooldown > 0.0f) return false;
    if (p.weapon == WEAPON_NONE) return false;
    if (p.weapon == WEAPON_SLINGSHOT && p.ammo <= 0) return false;

    Vector3 fwd = {
        sinf(p.yaw) * cosf(p.pitch),
        sinf(p.pitch),
        -cosf(p.yaw) * cosf(p.pitch)
    };

    if (p.weapon == WEAPON_SLINGSHOT) {
        Projectile pr;
        pr.pos = { p.pos.x + fwd.x * 0.4f,
                   C::EYE_H - 0.15f,
                   p.pos.z + fwd.z * 0.4f };
        pr.vel = { fwd.x * 30.0f,
                   fwd.y * 30.0f + 1.5f,
                   fwd.z * 30.0f };
        pr.lifetime = 2.5f;
        pr.alive    = true;
        pr.ownerId  = 0;
        projectiles.push_back(pr);
        p.ammo--;
        p.attackCooldown = C::SLING_CD;
        p.attackFlash    = 0.15f;
        return true;
    }

    if (p.weapon == WEAPON_WHIP) {
        for (auto& e : enemies) {
            float dx = e.pos.x - p.pos.x;
            float dz = e.pos.z - p.pos.z;
            float d  = sqrtf(dx*dx + dz*dz);
            if (d > C::WHIP_RANGE || d < 0.01f) continue;
            float dot = (dx/d) * fwd.x + (dz/d) * fwd.z;
            if (dot < C::WHIP_CONE) continue;
            applyHit(e, WEAPON_WHIP);
        }
        p.attackCooldown = C::WHIP_CD;
        p.attackFlash    = 0.25f;
        return true;
    }

    return false;
}

void updateProjectiles(std::vector<Projectile>& projectiles,
                       const Maze& maze,
                       std::vector<Enemy>& enemies,
                       float dt)
{
    const int SUBSTEPS = 4;
    for (auto& pr : projectiles) {
        if (!pr.alive) continue;

        pr.vel.y -= 12.0f * dt;
        float subDt = dt / (float)SUBSTEPS;

        for (int i = 0; i < SUBSTEPS && pr.alive; ++i) {
            pr.pos.x += pr.vel.x * subDt;
            pr.pos.y += pr.vel.y * subDt;
            pr.pos.z += pr.vel.z * subDt;

            if (maze.circleHitsWall(pr.pos.x, pr.pos.z, 0.1f) ||
                pr.pos.y < 0.05f || pr.pos.y > C::WALL_H) {
                pr.alive = false;
                break;
            }

            for (auto& e : enemies) {
                float dx = e.pos.x - pr.pos.x;
                float dz = e.pos.z - pr.pos.z;
                float dy = 1.35f - pr.pos.y;
                if (dx*dx + dz*dz < 0.5f*0.5f && fabsf(dy) < 1.2f) {
                    applyHit(e, WEAPON_SLINGSHOT);
                    pr.alive = false;
                    break;
                }
            }
        }

        pr.lifetime -= dt;
        if (pr.lifetime <= 0.0f) pr.alive = false;
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const Projectile& p){ return !p.alive; }),
        projectiles.end());
}

void updateEnemyStatus(std::vector<Enemy>& enemies, float dt) {
    for (auto& e : enemies) {
        if (e.stunT    > 0.0f) e.stunT    -= dt;
        if (e.hitFlash > 0.0f) e.hitFlash -= dt;
        if (e.rageT    > 0.0f) {
            e.rageT -= dt;
            if (e.rageT <= 0.0f) {
                e.rageT    = 0.0f;
                e.rageMult = 1.0f;
            }
        }
    }
}

}  // namespace Weapon
