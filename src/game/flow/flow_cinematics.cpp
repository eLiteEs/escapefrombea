#include "flow_cinematics.hpp"

#include "debug.hpp"
#include "util_format.hpp"

#include <cstring>
#include <cstdio>
#include <algorithm>

bool Game::hasStory(const char *name) const {
  return !Util::resolveStoryPath(name).empty();
}

void Game::playCinematic(const char *name, const char *nextAction) {
  std::string full = Util::resolveStoryPath(name);
  if (full.empty() || !cinematic.load(full.c_str())) {
    if (!strcmp(nextAction, "playing")) {
      startLevel(level);
      assignGamepads();
      DisableCursor();
      state = ST_PLAYING;
      if (assets.musicLevel.stream.buffer != nullptr && !musicLevelPlaying) {
        PlayMusicStream(assets.musicLevel);
        musicLevelPlaying = true;
      }
    } else {
      toMenu();
    }
    return;
  }
  nextAfterCinematic = nextAction;
  cinematic.start();
  state = ST_CINEMATIC;
  EnableCursor();
  Debug::resetCheats();
}

bool Game::loadLevelDetails(int lvl) {
  levelDetails = {};

  std::string path = Util::resolveLevelDetailsPath(lvl);
  if (path.empty())
    return false;

  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;

  levelDetails.loaded = true;
  levelDetails.difficulty = 1;

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    for (char *p = line; *p; ++p)
      if (*p == '\r' || *p == '\n') {
        *p = 0;
        break;
      }
    if (line[0] == '#' || line[0] == 0)
      continue;

    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = 0;
    const char *key = line;
    const char *val = eq + 1;

    if (!strcmp(key, "name"))
      levelDetails.name = val;
    else if (!strcmp(key, "subtitle"))
      levelDetails.subtitle = val;
    else if (!strcmp(key, "difficulty"))
      levelDetails.difficulty = atoi(val);
  }
  fclose(f);

  levelDetails.difficulty = std::clamp(levelDetails.difficulty, 1, 5);
  return true;
}
