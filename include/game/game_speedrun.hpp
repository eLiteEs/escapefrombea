#pragma once

#include <cstdint>

struct Game;
struct SpeedrunState;
struct GhostRun;

void speedrunStartRun(Game& g, uint32_t seed, int mode);
void speedrunRecordSplit(Game& g);
void speedrunFinishRun(Game& g);
