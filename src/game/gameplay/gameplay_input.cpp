#include "gameplay_input.hpp"

#include "game.hpp"
#include "inventory.hpp"
#include "lang.hpp"

void Game::handleInteraction(int p) {
  Player &pl = players[p];
  if (pl.caught || pl.escaped)
    return;

  bool interactPressed = IsKeyPressed(cfg.binds[p].interact) ||
                         (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));

  // Salir de taquilla (si estamos escondidos)
  if (pl.hidden) {
    if (interactPressed) {
      pl.hidden = false;
      pl.pos = pl.hiddenExitPos;
      pl.yaw = pl.hiddenExitYaw;
      pl.hiddenCellX = -1;
      pl.hiddenCellY = -1;
      if (p == 0)
        showToast(L("toast.locker.leave", "Saliste de la taquilla"));
    }
    return;
  }

  if (fountainCooldown[p] > 0.0f)
    return;
  if (!interactPressed)
    return;

  // Distancia 2D (ignoramos la Y para que items elevados y carteles
  // compitan en igualdad de condiciones).
  auto distXZ = [](Vector3 a, Vector3 b) -> float {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
  };

  enum IntType {
    INT_NONE,
    INT_PICKUP,
    INT_SIGN,
    INT_NPC,
    INT_LOCKER,
    INT_FOUNTAIN
  };
  int bestType = INT_NONE;
  int bestIdx = -1;
  float bestDist = 1e9f;

  // ----- Pickups -----
  int pk = Inventory::nearestPickup(pl.pos, worldItems, C::PICKUP_RANGE);
  if (pk >= 0) {
    float d = distXZ(pl.pos, worldItems[pk].pos);
    if (d < bestDist) {
      bestDist = d;
      bestType = INT_PICKUP;
      bestIdx = pk;
    }
  }

  // ----- Carteles -----
  for (size_t i = 0; i < currentSigns.size(); ++i) {
    float d = distXZ(pl.pos, currentSigns[i].pos);
    if (d < C::SIGN_INTERACT_RANGE && d < bestDist) {
      bestDist = d;
      bestType = INT_SIGN;
      bestIdx = (int)i;
    }
  }

  // ----- NPCs -----
  for (size_t i = 0; i < currentNpcs.size(); ++i) {
    float d = distXZ(pl.pos, currentNpcs[i].pos);
    if (d < C::SIGN_INTERACT_RANGE && d < bestDist) {
      bestDist = d;
      bestType = INT_NPC;
      bestIdx = (int)i;
    }
  }

  // ----- Taquilla / fuente -----
  int interactCx = -1, interactCy = -1;
  {
    bool isLocker;
    int what = findInteractable(pl, interactCx, interactCy, isLocker);
    if (what == 1 || what == 2) {
      Vector3 target = maze.cellCenter(interactCx, interactCy);
      float d = distXZ(pl.pos, target);
      if (d < C::INTERACT_RANGE && d < bestDist) {
        bestDist = d;
        bestType = (what == 1) ? INT_LOCKER : INT_FOUNTAIN;
      }
    }
  }

  // ================================================================
  //  Ejecutar el ganador
  // ================================================================
  if (bestType == INT_PICKUP) {
    ItemKind kind = worldItems[bestIdx].kind;

    if (kind == ITEM_KEY) {
      worldItems.erase(worldItems.begin() + bestIdx);
      players[0].hasKey = true;
      players[1].hasKey = true;
      if (assets.sfxPickup.frameCount > 0) {
        SetSoundPitch(assets.sfxPickup, 1.4f);
        PlaySound(assets.sfxPickup);
        SetSoundPitch(assets.sfxPickup, 1.0f);
      }
      if (p == 0)
        showToast(L("toast.key.acquired", "¡Llave conseguida!"));
      return;
    }

    if (Inventory::collectPickup(pl, worldItems, bestIdx)) {
      if (assets.sfxPickup.frameCount > 0)
        PlaySound(assets.sfxPickup);
      if (p == 0)
        showToast(TextFormat("%s", Inventory::itemName(kind)));
    } else {
      if (p == 0)
        showToast(L("toast.inventory.full", "Inventario lleno"));
    }
    return;
  }

  if (bestType == INT_SIGN) {
    openDialog(L("dialog.sign", "Cartel"), currentSigns[bestIdx].text, true);
    return;
  }

  if (bestType == INT_NPC) {
    const auto &n = currentNpcs[bestIdx];
    std::string title = n.name.empty() ? L("dialog.npc", "NPC") : n.name;
    openDialog(title, n.text, false);
    return;
  }

  if (bestType == INT_LOCKER) {
    pl.hidden = true;
    pl.hiddenExitPos = pl.pos;
    pl.hiddenExitYaw = pl.yaw;
    pl.hiddenCellX = interactCx;
    pl.hiddenCellY = interactCy;
    pl.yaw += PI;
    while (pl.yaw > PI)
      pl.yaw -= 2.0f * PI;
    while (pl.yaw < -PI)
      pl.yaw += 2.0f * PI;
    pl.sprinting = false;
    if (p == 0)
      showToast(
          TextFormat(L("toast.locker.hidden", "Escondido. [%s/RMB] salir"),
                     Config::keyName(cfg.binds[p].interact)));
    return;
  }

  if (bestType == INT_FOUNTAIN) {
    if (pl.stamina < C::STAM_WATER_MAX - 0.01f) {
      pl.stamina = C::STAM_WATER_MAX;
      pl.exhausted = false;
      pl.staminaDelay = 0.0f;
      fountainCooldown[p] = C::WATER_COOLDOWN;
      if (p == 0)
        showToast(L("toast.energy.full", "Energia al maximo!"));
    } else {
      if (p == 0)
        showToast(L("toast.energy.alreadyfull", "Ya tienes la energia llena"));
      fountainCooldown[p] = 0.3f;
    }
    return;
  }
}

void Game::handleItemInput(int p) {
  Player &pl = players[p];
  if (pl.caught || pl.escaped)
    return;

  if (p == 0) {
    for (int i = 0; i < C::INV_SLOTS; ++i)
      if (IsKeyPressed(KEY_ONE + i))
        pl.selectedSlot = i;

    float wheel = GetMouseWheelMove();
    if (wheel > 0.0f)
      pl.selectedSlot = (pl.selectedSlot - 1 + C::INV_SLOTS) % C::INV_SLOTS;
    else if (wheel < 0.0f)
      pl.selectedSlot = (pl.selectedSlot + 1) % C::INV_SLOTS;
  }

  if (IsKeyPressed(cfg.binds[p].drop)) {
    if (Inventory::dropSelected(pl, maze, worldItems)) {
      if (assets.sfxPickup.frameCount > 0) {
        SetSoundPitch(assets.sfxPickup, 0.7f);
        PlaySound(assets.sfxPickup);
        SetSoundPitch(assets.sfxPickup, 1.0f);
      }
      if (p == 0)
        showToast(L("toast.item.dropped", "Item dropeado"));
    }
  }

  bool usePressed = (p == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
                    IsKeyPressed(KEY_E);
  if (usePressed) {
    if (Inventory::useSelected(pl, maze, projectiles, assets)) {}
  }
}

int Game::findInteractable(const Player &pl, int &outCx, int &outCy,
                           bool &isLocker) const {
  Vector3 fwd = {sinf(pl.yaw), 0.0f, -cosf(pl.yaw)};
  Vector3 probe = {pl.pos.x + fwd.x * C::INTERACT_RANGE, 0.0f,
                   pl.pos.z + fwd.z * C::INTERACT_RANGE};
  int cx = (int)floorf(probe.x / C::CELL);
  int cy = (int)floorf(probe.z / C::CELL);
  outCx = cx;
  outCy = cy;

  if (maze.wallAt(cx, cy)) {
    int variant = 0;
    if (cy < (int)maze.wallVariant.size() &&
        cx < (int)maze.wallVariant[cy].size())
      variant = maze.wallVariant[cy][cx];
    if (variant == 1) {
      isLocker = true;
      return 1;
    }
    return 0;
  }
  if (maze.hasFountain(cx, cy)) {
    isLocker = false;
    return 2;
  }
  return 0;
}
