#pragma once
#include "common.h"
#include "maze.h"
#include <vector>

namespace Inventory {

// --- Slots ---
// Devuelve true si metio el item (o lo apilo). False si esta lleno.
bool tryAdd(Player& p, ItemKind kind, int amount = 1);

// Quita 1 del slot. Si queda vacio, lo limpia.
bool removeOne(Player& p, int slot);

// Dropea el item del slot seleccionado delante del jugador.
// Devuelve true si dropeo algo.
bool dropSelected(Player& p, const Maze& maze,
                  std::vector<WorldPickup>& world);

// --- Pickups ---
// Devuelve el indice del pickup mas cercano en rango (o -1).
int nearestPickup(const Vector3& pos,
                  const std::vector<WorldPickup>& world,
                  float maxDist);

// Recoge el pickup `idx` y lo mete en el inventario.
bool collectPickup(Player& p, std::vector<WorldPickup>& world, int idx);

// --- Uso ---
// Lanza el item seleccionado (si es lanzable).
bool useSelected(Player& p, const Maze& maze,
                 std::vector<Projectile>& projectiles);

// --- Proyectiles ---
void updateProjectiles(std::vector<Projectile>& projs,
                       const Maze& maze,
                       std::vector<Enemy>& enemies,
                       float dt);

void updateEnemyStatus(std::vector<Enemy>& enemies, float dt);

// --- Render helpers ---
Color itemColor(ItemKind kind);
const char* itemName(ItemKind kind);

}  // namespace Inventory
