#pragma once
#include "common.h"
#include "debug.h"
#include "maze.h"
#include <vector>

void updateEnemies(std::vector<Enemy>& enemies, const Maze& maze,
                   Player* players, int numPlayers, float dt,
                   float seeDist, float memoryT);

bool lineOfSight(const Maze& maze, Vector3 a, Vector3 b);

