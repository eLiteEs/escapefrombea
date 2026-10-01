#pragma once
#include "common.h"
#include "maze.h"
#include <vector>

namespace Weapon {

bool tryAttack(Player& p, const Maze& maze,
               std::vector<Projectile>& projectiles,
               std::vector<Enemy>& enemies);

void updateProjectiles(std::vector<Projectile>& projectiles,
                       const Maze& maze,
                       std::vector<Enemy>& enemies,
                       float dt);

void updateEnemyStatus(std::vector<Enemy>& enemies, float dt);

void applyHit(Enemy& e, WeaponKind weapon);

}  // namespace Weapon
