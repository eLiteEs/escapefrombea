#include "game_speedrun.hpp"

#include "game.hpp"

void Game::speedrunStartRun(uint32_t seed, int mode) {
  sr.active = true;
  sr.seed = seed;
  sr.mode = mode;
  sr.splits.clear();
  sr.bestSplits.clear();
  sr.lastLevelStart = totalPlaytime;
  sr.lastSplitDelta = 0.0f;
  sr.justRecordedPB = false;
  sr.pbFlashTimer = 0.0f;
  saves.bestSplitsFor(seed, mode, sr.bestSplits);

  // Ghost: iniciar grabación vacía y cargar playback del PB
  srGhostRec = GhostRun{};
  srGhostRec.seed = seed;
  srGhostRec.mode = mode;
  srGhostSampleTimer = 0.0f;

  srGhostPlayback = GhostRun{};
  srGhostPlaybackValid = saves.loadGhost(seed, mode, srGhostPlayback);
}

void Game::speedrunRecordSplit() {
  if (!sr.active)
    return;

  float split = totalPlaytime - sr.lastLevelStart;
  sr.lastLevelStart = totalPlaytime;
  sr.splits.push_back(split);

  int idx = (int)sr.splits.size() - 1;
  if (idx < (int)sr.bestSplits.size() && sr.bestSplits[idx] > 0.0f) {
    sr.lastSplitDelta = split - sr.bestSplits[idx];
    if (split < sr.bestSplits[idx]) {
      sr.bestSplits[idx] = split;
      sr.justRecordedPB = true;
      sr.pbFlashTimer = 2.0f;
    }
  } else {
    if (idx >= (int)sr.bestSplits.size())
      sr.bestSplits.resize(idx + 1, 0.0f);
    sr.bestSplits[idx] = split;
    sr.lastSplitDelta = 0.0f;
    sr.justRecordedPB = true;
    sr.pbFlashTimer = 2.0f;
  }
}

void Game::speedrunFinishRun() {
  if (!sr.active || sr.splits.empty()) {
    sr.active = false;
    return;
  }

  SpeedrunEntry e;
  e.seed = sr.seed;
  e.mode = sr.mode;
  e.date = std::time(nullptr);
  e.total = 0.0f;
  for (float s : sr.splits)
    e.total += s;
  e.splits = sr.splits;

  // ¿Es nuevo PB y completo? → guardar ghost
  float prevBest = 1e30f;
  size_t prevMaxLevels = 0;
  for (auto &r : saves.speedruns()) {
    if (r.seed != e.seed || r.mode != e.mode)
      continue;
    if (r.total < prevBest)
      prevBest = r.total;
    if (r.splits.size() > prevMaxLevels)
      prevMaxLevels = r.splits.size();
  }

  bool isNewPB = (e.total < prevBest - 0.001f);
  bool isComplete = (sr.splits.size() >= prevMaxLevels) || (prevMaxLevels == 0);

  saves.addSpeedrun(e);

  if (isNewPB && isComplete && !srGhostRec.levels.empty()) {
    // Cerrar la duración del último nivel grabado
    int lastLvl = level - 1;
    if (lastLvl >= 0 && lastLvl < (int)srGhostRec.levels.size()) {
      GhostLevel &gl = srGhostRec.levels[lastLvl];
      gl.duration = gl.samples.size() * C::GHOST_SAMPLE_DT;
    }
    saves.saveGhost(sr.seed, sr.mode, srGhostRec);
  }

  sr.active = false;
}
