#pragma once

#include <string>

struct Game;

void openDialog(Game& g, const std::string& title, const std::string& text, bool isSign);
void closeDialog(Game& g);
void drawDialog(Game& g);
