#pragma once

#include "game.hpp"

bool hasStory(const Game& g, const char* name);
void playCinematic(Game& g, const char* name, const char* nextAction);
bool loadLevelDetails(Game& g, int lvl);
