#pragma once
#include "common.h"
#include "config.h"
#include "maze.h"

bool updatePlayer(Player& pl, const KeyBindings& kb, float dt,
                  bool allowMouseLook, const Config& cfg, const Maze& maze);

