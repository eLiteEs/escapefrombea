#include "gameplay_dialog.hpp"

#include "game.hpp"

#include "ui.hpp"
#include "lang.hpp"

void Game::openDialog(const std::string &title, const std::string &text,
                      bool isSign) {
  dialogTitle = title;
  dialogText = text;
  dialogHasSignTex = isSign;
  state = ST_DIALOG;
  EnableCursor();
}

void Game::closeDialog() {
  dialogTitle.clear();
  dialogText.clear();
  DisableCursor();
  state = ST_PLAYING;
}

void Game::drawDialog() {
  int W = GetScreenWidth(), H = GetScreenHeight();

  DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 170});

  int marginX = std::max(40, W / 8);
  int panelH = std::max(160, H / 3);
  int panelX = marginX;
  int panelY = H - panelH - 40;
  int panelW = W - marginX * 2;

  DrawRectangle(panelX, panelY, panelW, panelH, (Color){15, 18, 28, 235});
  DrawRectangleLines(panelX, panelY, panelW, panelH,
                     dialogHasSignTex ? (Color){200, 160, 80, 255}
                                      : (Color){120, 180, 240, 255});

  UI::T(dialogTitle.c_str(), panelX + 24, panelY + 16, 24,
        dialogHasSignTex ? (Color){255, 220, 140, 255}
                         : (Color){160, 210, 255, 255});
  DrawRectangle(panelX + 24, panelY + 50, panelW - 48, 1,
                (Color){90, 100, 130, 160});

  int textX = panelX + 24;
  int textY = panelY + 64;
  int maxW = panelW - 48;
  int lineH = 22;

  std::vector<std::string> wrapped;
  {
    std::string cur, word;
    auto pushLine = [&]() {
      wrapped.push_back(cur);
      cur.clear();
    };
    auto flushWord = [&]() {
      if (word.empty())
        return;
      std::string test = cur.empty() ? word : cur + " " + word;
      if (UI::M(test.c_str(), 16) > maxW && !cur.empty()) {
        pushLine();
        cur = word;
      } else {
        cur = test;
      }
      word.clear();
    };
    for (size_t i = 0; i < dialogText.size(); ++i) {
      char c = dialogText[i];
      if (c == ' ') {
        flushWord();
      } else if (c == '\n') {
        flushWord();
        pushLine();
      } else {
        word += c;
      }
    }
    flushWord();
    if (!cur.empty())
      pushLine();
  }

  int maxVisible = (panelH - 80) / lineH;
  int shown = std::min((int)wrapped.size(), maxVisible);
  for (int i = 0; i < shown; ++i)
    UI::T(wrapped[i].c_str(), textX, textY + i * lineH, 16, RAYWHITE);
  if ((int)wrapped.size() > shown)
    UI::T("...", textX, textY + shown * lineH, 16, GRAY);

  const char *hint = L("dialog.close", "[E / RMB / ENTER] cerrar");
  int hs = 14;
  int hw = UI::M(hint, hs);
  UI::T(hint, panelX + (panelW - hw) / 2, panelY + panelH - 26, hs, GRAY);
}
