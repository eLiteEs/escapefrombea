#pragma once

#include <random>

struct Game;
struct LevelExtras;
struct LevelDetails;

void startLevel(Game& g, int lvl);
void enterLevel(Game& g, int lvl);
void requestEnterLevel(Game& g, int lvl);
bool tryLoadCustomLevel(Game& g, int lvl, LevelExtras& extras);
void spawnWorldItems(Game& g, int lvl, int size, std::mt19937& rng);
void updateWorldItems(Game& g, float dt);
void drawLevelIntro(Game& g);
