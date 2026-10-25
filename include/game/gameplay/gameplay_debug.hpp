#pragma once

#include "common.hpp"

struct Game;

void handleDebugHotkeys(Game& g);
void drawDebugOverlay(Game& g);
void drawDebugWorld3D(Game& g, const Player& pl);
void debugSpawnEnemy(Game& g);
void debugKillAll(Game& g);
void debugTeleportToExit(Game& g);
void debugRevealMap(Game& g);
