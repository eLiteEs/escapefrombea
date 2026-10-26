#pragma once

#include <raylib.h>

struct Game;
struct Player;

void drawStaminaBar(Game& g, const Player& pl, int vw, int vh);
void drawMinimap(Game& g, const Player& pl, int vw);
void drawInventory(Game& g, const Player& pl, int vw, int vh);
void drawHandItem2D(Game& g, const Player& pl, int vw, int vh);
void drawSpeedrunOverlay(Game& g, int vw, int vh);
float nearestEnemyDist(const Game& g, Vector3 p);
void drawToast();
void showToast(const char *msg);
