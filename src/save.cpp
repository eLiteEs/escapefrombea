#include "save.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define WEB_BUILD 1
#else
#ifdef _WIN32
#include <direct.h>
#define MKDIR_SAVES() _mkdir("saves")
#else
#include <sys/stat.h>
#define MKDIR_SAVES() mkdir("saves", 0755)
#endif
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <string>

static bool ls_save(const char *key, const std::string &data) {
  EM_ASM(
      {
        try {
          localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        } catch (e) {
          console.error('localStorage setItem failed', e);
        }
      },
      key, data.c_str());
  return true;
}

static std::string ls_load(const char *key) {
  char buf[8192] = {0};
  EM_ASM(
      {
        var v = localStorage.getItem(UTF8ToString($0));
        if (v)
          stringToUTF8(v, $1, $2);
      },
      key, buf, (int)sizeof(buf));
  return std::string(buf);
}

static void ls_remove(const char *key) {
  EM_ASM(
      {
        try {
          localStorage.removeItem(UTF8ToString($0));
        } catch (e) {
        }
      },
      key);
}
#endif

static void parseSlotKey(SaveSlot &slot, const char *key, const char *val) {
  // Claves simples
  if (!strcmp(key, "level"))
    slot.level = atoi(val);
  else if (!strcmp(key, "maxLevel"))
    slot.maxLevel = atoi(val);
  else if (!strcmp(key, "mode"))
    slot.mode = atoi(val);
  else if (!strcmp(key, "twoPlayers"))
    slot.twoPlayers = atoi(val) != 0;
  else if (!strcmp(key, "playtime"))
    slot.playtime = (float)atof(val);
  else if (!strcmp(key, "timestamp"))
    slot.timestamp = (std::time_t)atoll(val);
  else if (!strcmp(key, "runSeed"))
    slot.runSeed = (uint32_t)strtoul(val, nullptr, 10);
  else if (!strcmp(key, "enemyMult"))
    slot.enemyMult = atoi(val);
  else if (!strcmp(key, "itemMult"))
    slot.itemMult = atoi(val);
  else if (!strcmp(key, "speedrunSplits")) {
    slot.speedrunSplits.clear();
    if (val[0] == 0)
      return;
    const char *p = val;
    while (*p) {
      char *end;
      float v = strtof(p, &end);
      if (end == p)
        break;
      slot.speedrunSplits.push_back(v);
      p = end;
      if (*p == ',')
        ++p;
    }
  } else {
    int p = 0, s = 0, consumed = 0;
    int klen = (int)strlen(key);

    // pN_slotM_kind
    if (sscanf(key, "p%d_slot%d_kind%n", &p, &s, &consumed) == 2 &&
        consumed == klen && p >= 1 && p <= 2 && s >= 0 && s < C::INV_SLOTS) {
      slot.inventory[p - 1][s].kind = atoi(val);
      return;
    }

    // pN_slotM_count
    if (sscanf(key, "p%d_slot%d_count%n", &p, &s, &consumed) == 2 &&
        consumed == klen && p >= 1 && p <= 2 && s >= 0 && s < C::INV_SLOTS) {
      slot.inventory[p - 1][s].count = atoi(val);
      return;
    }

    // pN_sel   (al final: es el más "laxo")
    if (sscanf(key, "p%d_sel%n", &p, &consumed) == 1 && consumed == klen &&
        p >= 1 && p <= 2) {
      slot.selectedSlot[p - 1] = atoi(val);
      return;
    }
  }
}

bool SaveManager::saveSlot(int i, const SaveSlot &data) {
  if (i < 0 || i >= NUM_SLOTS)
    return false;

#if defined(__EMSCRIPTEN__)
  char buf[4096];
  int n =
      snprintf(buf, sizeof(buf),
               "level=%d\nmaxLevel=%d\nmode=%d\ntwoPlayers=%d\nplaytime=%."
               "2f\ntimestamp=%lld\n",
               data.level, data.maxLevel, data.mode, data.twoPlayers ? 1 : 0,
               (double)data.playtime, (long long)std::time(nullptr));

  char splitsBuf[512] = {0};
  for (size_t i = 0; i < data.speedrunSplits.size(); ++i) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%s%.3f", i ? "," : "",
             (double)data.speedrunSplits[i]);
    strncat(splitsBuf, tmp, sizeof(splitsBuf) - strlen(splitsBuf) - 1);
  }
  n += snprintf(buf + n, sizeof(buf) - n,
                "runSeed=%u\nenemyMult=%d\nitemMult=%d\nspeedrunSplits=%s\n",
                data.runSeed, data.enemyMult, data.itemMult, splitsBuf);

  for (int p = 0; p < 2; ++p) {
    n += snprintf(buf + n, sizeof(buf) - n, "p%d_sel=%d\n", p + 1,
                  data.selectedSlot[p]);
    for (int s = 0; s < C::INV_SLOTS; ++s) {
      n += snprintf(buf + n, sizeof(buf) - n, "p%d_slot%d_kind=%d\n", p + 1, s,
                    data.inventory[p][s].kind);
      n += snprintf(buf + n, sizeof(buf) - n, "p%d_slot%d_count=%d\n", p + 1, s,
                    data.inventory[p][s].count);
    }
  }

  char key[32];
  snprintf(key, sizeof(key), "efb_slot%d", i + 1);
  ls_save(key, buf);
#else
  MKDIR_SAVES();
  FILE *f = fopen(slotPath(i).c_str(), "w");
  if (!f)
    return false;

  fprintf(f, "level=%d\n", data.level);
  fprintf(f, "maxLevel=%d\n", data.maxLevel);
  fprintf(f, "mode=%d\n", data.mode);
  fprintf(f, "twoPlayers=%d\n", data.twoPlayers ? 1 : 0);
  fprintf(f, "playtime=%.2f\n", (double)data.playtime);
  fprintf(f, "timestamp=%lld\n", (long long)std::time(nullptr));
  fprintf(f, "runSeed=%u\n", data.runSeed);
  fprintf(f, "enemyMult=%d\n", data.enemyMult);
  fprintf(f, "itemMult=%d\n", data.itemMult);

  fprintf(f, "speedrunSplits=");
  for (size_t i = 0; i < data.speedrunSplits.size(); ++i)
    fprintf(f, "%s%.3f", i ? "," : "", (double)data.speedrunSplits[i]);
  fprintf(f, "\n");

  for (int p = 0; p < 2; ++p) {
    fprintf(f, "p%d_sel=%d\n", p + 1, data.selectedSlot[p]);
    for (int s = 0; s < C::INV_SLOTS; ++s) {
      fprintf(f, "p%d_slot%d_kind=%d\n", p + 1, s, data.inventory[p][s].kind);
      fprintf(f, "p%d_slot%d_count=%d\n", p + 1, s, data.inventory[p][s].count);
    }
  }
  fclose(f);
#endif

  slots_[i] = data;
  slots_[i].used = true;
  slots_[i].timestamp = std::time(nullptr);
  return true;
}

std::string SaveManager::slotPath(int i) const {
  char buf[80];
#if defined(__EMSCRIPTEN__)
  snprintf(buf, sizeof(buf), "/saves/slot%d.cfg", i + 1);
#else
  snprintf(buf, sizeof(buf), "saves/slot%d.cfg", i + 1);
#endif
  return buf;
}

void SaveManager::refresh() {
  for (int i = 0; i < NUM_SLOTS; ++i) {
    slots_[i] = SaveSlot{};

#if defined(__EMSCRIPTEN__)
    char key[32];
    snprintf(key, sizeof(key), "efb_slot%d", i + 1);
    std::string content = ls_load(key);
    if (content.empty())
      continue;

    const char *p = content.c_str();
    char line[256];
    while (sscanf(p, "%255[^\n]\n", line) == 1) {
      const char *nl = strchr(p, '\n');
      if (!nl)
        break;
      p = nl + 1;
      char *eq = strchr(line, '=');
      if (!eq)
        continue;
      *eq = 0;
      parseSlotKey(slots_[i], line, eq + 1);
    }
    slots_[i].used = true;
#else
    MKDIR_SAVES();
    FILE *f = fopen(slotPath(i).c_str(), "r");
    if (!f)
      continue;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      for (char *p = line; *p; ++p)
        if (*p == '\r' || *p == '\n') {
          *p = 0;
          break;
        }
      char *eq = strchr(line, '=');
      if (!eq)
        continue;
      *eq = 0;
      parseSlotKey(slots_[i], line, eq + 1);
    }
    fclose(f);
    slots_[i].used = true;
#endif
  }

  loadSpeedruns();
}

bool SaveManager::loadSlot(int i, SaveSlot &out) const {
  if (i < 0 || i >= NUM_SLOTS)
    return false;
  if (!slots_[i].used)
    return false;
  out = slots_[i];
  return true;
}

int SaveManager::firstFreeSlot() const {
  for (int i = 0; i < NUM_SLOTS; ++i)
    if (!slots_[i].used)
      return i;
  return -1;
}

bool SaveManager::anyUsed() const {
  for (int i = 0; i < NUM_SLOTS; ++i)
    if (slots_[i].used)
      return true;
  return false;
}

static std::string speedrunsPath() {
#if defined(__EMSCRIPTEN__)
  return "/saves/speedruns.cfg";
#else
  MKDIR_SAVES();
  return "saves/speedruns.cfg";
#endif
}

void SaveManager::loadSpeedruns() {
  speedruns_.clear();

#if defined(__EMSCRIPTEN__)
  std::string content = ls_load("efb_speedruns");
  if (content.empty())
    return;
  const char *p = content.c_str();
  char line[1024];
  while (sscanf(p, "%1023[^\n]\n", line) == 1) {
    const char *nl = strchr(p, '\n');
    if (!nl)
      break;
    p = nl + 1;

    if (line[0] == '#' || line[0] == 0)
      continue;
    // Formato: seed,mode,date,total,s1,s2,s3,...
    SpeedrunEntry e;
    char *tok = strtok(line, ",");
    if (!tok)
      continue;
    e.seed = (uint32_t)strtoul(tok, nullptr, 10);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.mode = atoi(tok);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.date = (std::time_t)atoll(tok);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.total = strtof(tok, nullptr);
    while ((tok = strtok(nullptr, ",")) != nullptr)
      e.splits.push_back(strtof(tok, nullptr));
    speedruns_.push_back(std::move(e));
  }
#else
  FILE *f = fopen(speedrunsPath().c_str(), "r");
  if (!f)
    return;
  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    for (char *q = line; *q; ++q)
      if (*q == '\r' || *q == '\n') {
        *q = 0;
        break;
      }
    if (line[0] == '#' || line[0] == 0)
      continue;

    SpeedrunEntry e;
    char *tok = strtok(line, ",");
    if (!tok)
      continue;
    e.seed = (uint32_t)strtoul(tok, nullptr, 10);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.mode = atoi(tok);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.date = (std::time_t)atoll(tok);
    tok = strtok(nullptr, ",");
    if (!tok)
      continue;
    e.total = strtof(tok, nullptr);
    while ((tok = strtok(nullptr, ",")) != nullptr)
      e.splits.push_back(strtof(tok, nullptr));
    speedruns_.push_back(std::move(e));
  }
  fclose(f);
#endif
}

void SaveManager::saveSpeedruns() {
#if defined(__EMSCRIPTEN__)
  std::string content = "# speedruns.cfg\n";
  for (auto &e : speedruns_) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "%u,%d,%lld,%.3f", e.seed, e.mode,
                     (long long)e.date, (double)e.total);
    content += buf;
    for (float s : e.splits) {
      snprintf(buf, sizeof(buf), ",%.3f", (double)s);
      content += buf;
    }
    content += "\n";
  }
  ls_save("efb_speedruns", content);
#else
  FILE *f = fopen(speedrunsPath().c_str(), "w");
  if (!f)
    return;
  fprintf(f, "# speedruns.cfg\n");
  for (auto &e : speedruns_) {
    fprintf(f, "%u,%d,%lld,%.3f", e.seed, e.mode, (long long)e.date,
            (double)e.total);
    for (float s : e.splits)
      fprintf(f, ",%.3f", (double)s);
    fprintf(f, "\n");
  }
  fclose(f);
#endif
}

void SaveManager::addSpeedrun(const SpeedrunEntry &e) {
  speedruns_.push_back(e);
  saveSpeedruns();
}

void SaveManager::bestSplitsFor(uint32_t seed, int mode,
                                std::vector<float> &outBest) const {
  outBest.clear();
  const float INF = 1e9f;
  for (auto &e : speedruns_) {
    if (e.seed != seed || e.mode != mode)
      continue;
    for (size_t i = 0; i < e.splits.size(); ++i) {
      if (i >= outBest.size())
        outBest.resize(i + 1, INF);
      if (e.splits[i] > 0.0f)
        outBest[i] = std::min(outBest[i], e.splits[i]);
    }
  }
  for (auto &v : outBest)
    if (v >= INF)
      v = 0.0f;
}
