#include "inventory.hpp"

#include "assets.hpp"
#include "game.hpp"
#include "lang.hpp"

#include <algorithm>
#include <cmath>

namespace Inventory {

bool tryAdd(Player &p, ItemKind kind, int amount) {
  if (kind == ITEM_NONE || amount <= 0)
    return false;

  // Primero apila en un slot con el mismo tipo
  for (int i = 0; i < C::INV_SLOTS; ++i) {
    if (p.inventory[i].kind == kind) {
      p.inventory[i].count += amount;
      return true;
    }
  }
  // Luego busca uno vacio
  for (int i = 0; i < C::INV_SLOTS; ++i) {
    if (p.inventory[i].kind == ITEM_NONE) {
      p.inventory[i].kind = kind;
      p.inventory[i].count = amount;
      return true;
    }
  }
  return false;
}

bool removeOne(Player &p, int slot) {
  if (slot < 0 || slot >= C::INV_SLOTS)
    return false;
  InvItem &it = p.inventory[slot];
  if (it.kind == ITEM_NONE || it.count <= 0)
    return false;
  it.count--;
  if (it.count <= 0) {
    it.kind = ITEM_NONE;
    it.count = 0;
  }
  return true;
}

bool dropSelected(Player &p, const Maze &maze,
                  std::vector<WorldPickup> &world) {
  if (p.selectedSlot < 0 || p.selectedSlot >= C::INV_SLOTS)
    return false;
  InvItem &it = p.inventory[p.selectedSlot];
  if (it.kind == ITEM_NONE || it.count <= 0)
    return false;

  Vector3 fwd = {sinf(p.yaw), 0.0f, -cosf(p.yaw)};
  Vector3 drop = {p.pos.x + fwd.x * C::DROP_DIST, 0.35f,
                  p.pos.z + fwd.z * C::DROP_DIST};
  // Si cae dentro de una pared, prueba mas cerca
  if (maze.circleHitsWall(drop.x, drop.z, 0.2f)) {
    drop.x = p.pos.x + fwd.x * 0.4f;
    drop.z = p.pos.z + fwd.z * 0.4f;
  }

  WorldPickup wp;
  wp.kind = it.kind;
  wp.pos = drop;
  wp.bobT = 0.0f;
  world.push_back(wp);

  removeOne(p, p.selectedSlot);
  return true;
}

int nearestPickup(const Vector3 &pos, const std::vector<WorldPickup> &world,
                  float maxDist) {
  int best = -1;
  float bestD = maxDist;
  for (size_t i = 0; i < world.size(); ++i) {
    float d = Vector3Distance(world[i].pos, pos);
    if (d < bestD) {
      bestD = d;
      best = (int)i;
    }
  }
  return best;
}

bool collectPickup(Player &p, std::vector<WorldPickup> &world, int idx) {
  if (idx < 0 || idx >= (int)world.size())
    return false;
  WorldPickup &wp = world[idx];
  if (!tryAdd(p, wp.kind, 1))
    return false;
  world.erase(world.begin() + idx);
  return true;
}

bool useSelected(Player &p, const Maze &maze,
                 std::vector<Projectile> &projectiles, Assets assets) {
  if (p.throwCooldown > 0.0f)
    return false;
  if (p.selectedSlot < 0 || p.selectedSlot >= C::INV_SLOTS)
    return false;

  InvItem &it = p.inventory[p.selectedSlot];
  if (it.kind == ITEM_NONE || it.count <= 0)
    return false;

  if (it.kind == ITEM_MONSTER) {
    p.stamina = 1.0f;
    p.exhausted = false;
    p.staminaDelay = 0.0f;

    removeOne(p, p.selectedSlot);
    p.throwCooldown = 4.0f;

    if (assets.sfxDrink.frameCount > 0)
      PlaySound(assets.sfxDrink);

    return true;
  } else if (it.kind == ITEM_WALL) {
    Vector3 fwd = {sinf(p.yaw) * cosf(p.pitch), sinf(p.pitch),
                   -cosf(p.yaw) * cosf(p.pitch)};

    Projectile pr;
    pr.pos = {p.pos.x + fwd.x * 0.4f, C::EYE_H - 0.10f, p.pos.z + fwd.z * 0.4f};

    const float TENT_SPEED = 10.0f;
    pr.vel = {fwd.x * TENT_SPEED, fwd.y * TENT_SPEED + 1.5f,
              fwd.z * TENT_SPEED};
    pr.lifetime = 1.5f;
    pr.alive = true;
    pr.kind = ITEM_WALL;
    projectiles.push_back(pr);

    removeOne(p, p.selectedSlot);
    p.throwCooldown = 1.0f;
    p.attackFlash = 0.10f;

    if (assets.sfxThrow.frameCount > 0)
      PlaySound(assets.sfxThrow);

    return true;
  } else {
    Vector3 fwd = {sinf(p.yaw) * cosf(p.pitch), sinf(p.pitch),
                   -cosf(p.yaw) * cosf(p.pitch)};

    Projectile pr;
    pr.pos = {p.pos.x + fwd.x * 0.4f, C::EYE_H - 0.10f, p.pos.z + fwd.z * 0.4f};

    switch (it.kind) {
    case ITEM_ROCK:
      pr.vel = {fwd.x * C::ROCK_THROW_SPEED, fwd.y * C::ROCK_THROW_SPEED + 2.0f,
                fwd.z * C::ROCK_THROW_SPEED};
      break;

    case ITEM_BOX:
      pr.vel = {fwd.x * C::BOX_THROW_SPEED, fwd.y * C::BOX_THROW_SPEED + 2.0f,
                fwd.z * C::BOX_THROW_SPEED};
      break;

    case ITEM_LIME:
      pr.vel = {fwd.x * C::LIME_THROW_SPEED, fwd.y * C::LIME_THROW_SPEED + 2.0f,
                fwd.z * C::LIME_THROW_SPEED};
      break;

    case ITEM_BICIMAD:
      pr.vel = {fwd.x * C::BICIMAD_THROW_SPEED,
                fwd.y * C::BICIMAD_THROW_SPEED + 2.0f,
                fwd.z * C::BICIMAD_THROW_SPEED};
      break;
    default:
      pr.vel = {fwd.x * C::ROCK_THROW_SPEED, fwd.y * C::ROCK_THROW_SPEED + 2.0f,
                fwd.z * C::ROCK_THROW_SPEED};
      break;
    }

    pr.lifetime = 3.0f;
    pr.alive = true;
    pr.kind = it.kind;
    projectiles.push_back(pr);

    removeOne(p, p.selectedSlot);

    switch (it.kind) {
    case ITEM_ROCK:
      p.throwCooldown = C::ROCK_THROW_CD;
      break;

    case ITEM_BOX:
      p.throwCooldown = C::BOX_THROW_CD;
      break;

    case ITEM_LIME:
      p.throwCooldown = C::LIME_THROW_CD;
      break;

    case ITEM_BICIMAD:
      p.throwCooldown = C::BICIMAD_THROW_CD;
      break;
    default:
      p.throwCooldown = C::ROCK_THROW_CD;
      break;
    }

    p.attackFlash = 0.10f;
    (void)maze;

    if (assets.sfxThrow.frameCount > 0)
      PlaySound(assets.sfxThrow);

    return true;
  }

  return false;
}

void updateProjectiles(std::vector<Projectile> &projs, Maze &maze,
                       std::vector<Enemy> &enemies, float dt) {
  const int SUBSTEPS = 4;
  for (auto &pr : projs) {
    if (!pr.alive)
      continue;

    pr.vel.y -= 12.0f * dt;
    float subDt = dt / (float)SUBSTEPS;

    for (int i = 0; i < SUBSTEPS && pr.alive; ++i) {
      float px = pr.pos.x;
      float pz = pr.pos.z;

      pr.pos.x += pr.vel.x * subDt;
      pr.pos.y += pr.vel.y * subDt;
      pr.pos.z += pr.vel.z * subDt;

      bool hitWall = maze.circleHitsWall(pr.pos.x, pr.pos.z, 0.1f);
      bool hitFloor = pr.pos.y < 0.05f;
      bool hitCeil = pr.pos.y > C::WALL_H;

      if (hitWall || hitFloor || hitCeil) {
        if (pr.kind == ITEM_WALL) {
          // Colocar la tienda en la celda previa (no dentro de la pared)
          int cx = (int)floorf(px / C::CELL);
          int cy = (int)floorf(pz / C::CELL);
          if (!maze.wallAt(cx, cy) && !maze.hasTent(cx, cy))
            maze.placeTent(cx, cy);
        }
        pr.alive = false;
        break;
      }

      // La tienda en vuelo no interactúa con enemigos
      if (pr.kind == ITEM_WALL)
        continue;

      for (auto &e : enemies) {
        float dx = e.pos.x - pr.pos.x;
        float dz = e.pos.z - pr.pos.z;
        float dy = 1.35f - pr.pos.y;
        if (dx * dx + dz * dz < 0.5f * 0.5f && fabsf(dy) < 1.2f) {
          switch (pr.kind) {
          case ITEM_ROCK:
            e.stunT = C::ROCK_STUN;
            e.rageT = C::ROCK_RAGE_T;
            e.rageMult = C::ROCK_RAGE_MULT;
            break;
          case ITEM_BOX:
            e.stunT = C::BOX_STUN;
            e.rageT = C::BOX_RAGE_T;
            e.rageMult = C::BOX_RAGE_MULT;
            break;
          case ITEM_LIME:
            e.stunT = C::LIME_STUN;
            e.rageT = C::LIME_RAGE_T;
            e.rageMult = C::LIME_RAGE_MULT;
            break;
          case ITEM_BICIMAD:
            e.stunT = C::BICIMAD_STUN;
            e.rageT = C::BICIMAD_RAGE_T;
            e.rageMult = C::BICIMAD_RAGE_MULT;
            break;
          default:
            e.stunT = C::ROCK_STUN;
            e.rageT = C::ROCK_RAGE_T;
            e.rageMult = C::ROCK_RAGE_MULT;
            break;
          }
          e.hitFlash = 0.30f;
          pr.alive = false;
          break;
        }
      }
    }

    // Si expira en el aire, plantar la tienda donde caiga
    pr.lifetime -= dt;
    if (pr.lifetime <= 0.0f && pr.alive) {
      if (pr.kind == ITEM_WALL) {
        int cx = (int)floorf(pr.pos.x / C::CELL); // <-- pr.pos.x aquí
        int cy = (int)floorf(pr.pos.z / C::CELL); // <-- pr.pos.z aquí
        if (!maze.wallAt(cx, cy) && !maze.hasTent(cx, cy))
          maze.placeTent(cx, cy);
      }
      pr.alive = false;
    }
  }

  projs.erase(std::remove_if(projs.begin(), projs.end(),
                             [](const Projectile &p) { return !p.alive; }),
              projs.end());
}

void updateEnemyStatus(std::vector<Enemy> &enemies, float dt) {
  for (auto &e : enemies) {
    if (e.stunT > 0.0f)
      e.stunT -= dt;
    if (e.hitFlash > 0.0f)
      e.hitFlash -= dt;
    if (e.rageT > 0.0f) {
      e.rageT -= dt;
      if (e.rageT <= 0.0f) {
        e.rageT = 0.0f;
        e.rageMult = 1.0f;
      }
    }
  }
}

Color itemColor(ItemKind kind) {
  switch (kind) {
  case ITEM_ROCK:
    return (Color){130, 130, 145, 255};
  case ITEM_BOX:
    return (Color){0, 100, 0, 255};
  case ITEM_LIME:
    return (Color){0, 255, 0, 255};
  case ITEM_BICIMAD:
    return (Color){100, 100, 255, 255};
  default:
    return (Color){200, 200, 200, 255};
  }
}

const char *itemName(ItemKind kind) {
  switch (kind) {
  case ITEM_ROCK:
    return L("item.rock", "piedra");
  case ITEM_BOX:
    return L("item.box", "caja");
  case ITEM_LIME:
    return "lime";
  case ITEM_BICIMAD:
    return "bicimad";
  case ITEM_MONSTER:
    return "monster";
  case ITEM_WALL:
    return L("item.wall", "muro");
  case ITEM_KEY:
    return L("item.key", "llave");
  default:
    return L("item", "item");
  }
}

} // namespace Inventory

void Game::spawnItemsFromPositions(const std::vector<Vector3> &positions,
                                   std::mt19937 &rng) {
  struct Weighted {
    ItemKind kind;
    int weight;
  };
  const Weighted pool[] = {{ITEM_ROCK, 30},    {ITEM_BOX, 20},
                           {ITEM_MONSTER, 20}, {ITEM_LIME, 15},
                           {ITEM_BICIMAD, 10}, {ITEM_WALL, 5}};
  const int POOL_SIZE = sizeof(pool) / sizeof(pool[0]);
  int totalWeight = 0;
  for (int i = 0; i < POOL_SIZE; ++i)
    totalWeight += pool[i].weight;

  auto rollKind = [&]() -> ItemKind {
    int r = (int)(rng() % totalWeight);
    int acc = 0;
    for (int i = 0; i < POOL_SIZE; ++i) {
      acc += pool[i].weight;
      if (r < acc)
        return pool[i].kind;
    }
    return ITEM_ROCK;
  };

  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;
    WorldPickup wp;
    wp.kind = rollKind();
    wp.pos = {p.x, p.y + 0.35f, p.z}; // ← usar p.y (altura del suelo)
    wp.bobT = (float)(rng() % 100) / 100.0f * 6.28f;
    worldItems.push_back(wp);
  }
}

void Game::spawnKeysFromPositions(const std::vector<Vector3> &positions) {
  for (const auto &p : positions) {
    if (maze.wallAtWorld(p.x, p.z))
      continue;
    WorldPickup wp;
    wp.kind = ITEM_KEY;
    wp.pos = {p.x, p.y + 0.55f, p.z}; // ← usar p.y
    wp.bobT = 0.0f;
    worldItems.push_back(wp);
  }
}
