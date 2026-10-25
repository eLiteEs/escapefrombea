#pragma once

struct Game;
struct Player;

void renderPlayerView(Game& g, const Player& pl);
void drawGameplayView(Game& g, const Player& pl, int vw, int vh, int playerNum);
void drawSplitScreen(Game& g);
void drawCinematic(Game& g);
