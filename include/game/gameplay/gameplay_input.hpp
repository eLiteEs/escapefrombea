#pragma once

#include "common.hpp"

struct Game;

void handleInteraction(Game& g, int playerIdx);
void handleItemInput(Game& g, int playerIdx);
int findInteractable(const Game& g, const Player& pl, int& outCx, int& outCy, bool& isLocker);
