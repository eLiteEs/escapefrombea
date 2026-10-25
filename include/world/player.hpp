#pragma once
#include "common.hpp"
#include "config.hpp"
#include "maze.hpp"

bool updatePlayer(Player& pl, const KeyBindings& kb, float dt,
                  bool allowMouseLook, const Config& cfg, const Maze& maze);

