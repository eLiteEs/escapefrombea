#include "save.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#ifdef _WIN32
#include <direct.h>
#define MKDIR_SAVES() _mkdir("saves")
#else
#include <sys/stat.h>
#define MKDIR_SAVES() mkdir("saves", 0755)
#endif
#endif

// =========================================================================
//  IO helpers
//  Nativo: ficheros en disco.
//  Web:    localStorage (clave derivada del path).
// =========================================================================
static std::string keyFromPath(const std::string &p) {
  std::string k = p;
  for (auto &c : k)
    if (c == '/' || c == '\\')
      c = '_';
  return "efb_" + k;
}

static bool writeFile(const std::string &path, const std::string &content) {
#if defined(__EMSCRIPTEN__)
  std::string k = keyFromPath(path);
  EM_ASM(
      {
        try {
          localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        } catch (e) {
        }
      },
      k.c_str(), content.c_str());
  return true;
#else
  MKDIR_SAVES();
  FILE *f = fopen(path.c_str(), "w");
  if (!f)
    return false;
  fwrite(content.data(), 1, content.size(), f);
  fclose(f);
  return true;
#endif
}

static std::string readFile(const std::string &path) {
#if defined(__EMSCRIPTEN__)
  std::string k = keyFromPath(path);
  char buf[32768] = {0};
  EM_ASM(
      {
        var v = localStorage.getItem(UTF8ToString($0));
        if (v)
          stringToUTF8(v, $1, $2);
      },
      k.c_str(), buf, (int)sizeof(buf));
  return std::string(buf);
#else
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return "";
  std::string out;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    out.append(buf, n);
  fclose(f);
  return out;
#endif
}

static bool deleteFile(const std::string &path) {
#if defined(__EMSCRIPTEN__)
  std::string k = keyFromPath(path);
  EM_ASM(
      {
        try {
          localStorage.removeItem(UTF8ToString($0));
        } catch (e) {
        }
      },
      k.c_str());
  return true;
#else
  return remove(path.c_str()) == 0;
#endif
}

// =========================================================================
//  Parser de SaveEntry
// =========================================================================
static void parseEntryKey(SaveEntry &e, const char *key, const char *val) {
  if (!strcmp(key, "id"))
    e.id = val;
  else if (!strcmp(key, "name"))
    e.name = val;
  else if (!strcmp(key, "mode"))
    e.mode = atoi(val);
  else if (!strcmp(key, "difficulty"))
    e.difficulty = atoi(val);
  else if (!strcmp(key, "speedrun"))
    e.speedrun = atoi(val) != 0;
  else if (!strcmp(key, "twoPlayers"))
    e.twoPlayers = atoi(val) != 0;
  else if (!strcmp(key, "level"))
    e.level = atoi(val);
  else if (!strcmp(key, "maxLevel"))
    e.maxLevel = atoi(val);
  else if (!strcmp(key, "playtime"))
    e.playtime = (float)atof(val);
  else if (!strcmp(key, "timestamp"))
    e.timestamp = (std::time_t)atoll(val);
  else if (!strcmp(key, "created"))
    e.created = (std::time_t)atoll(val);
  else if (!strcmp(key, "runSeed"))
    e.runSeed = (uint32_t)strtoul(val, nullptr, 10);
  else if (!strcmp(key, "enemyMult"))
    e.enemyMult = atoi(val);
  else if (!strcmp(key, "itemMult"))
    e.itemMult = atoi(val);
  else if (!strcmp(key, "speedrunSplits")) {
    e.speedrunSplits.clear();
    if (val[0] == 0)
      return;
    const char *p = val;
    while (*p) {
      char *end;
      float v = strtof(p, &end);
      if (end == p)
        break;
      e.speedrunSplits.push_back(v);
      p = end;
      if (*p == ',')
        ++p;
    }
  } else {
    // Claves de inventario: pN_slotM_kind, pN_slotM_count, pN_sel
    int p = 0, s = 0, consumed = 0;
    int klen = (int)strlen(key);

    // El %n + comparación con klen evita que "p1_slot0_kind" matchee "p%d_sel".
    if (sscanf(key, "p%d_slot%d_kind%n", &p, &s, &consumed) == 2 &&
        consumed == klen && p >= 1 && p <= 2 && s >= 0 && s < C::INV_SLOTS) {
      e.inventory[p - 1][s].kind = atoi(val);
      return;
    }
    if (sscanf(key, "p%d_slot%d_count%n", &p, &s, &consumed) == 2 &&
        consumed == klen && p >= 1 && p <= 2 && s >= 0 && s < C::INV_SLOTS) {
      e.inventory[p - 1][s].count = atoi(val);
      return;
    }
    if (sscanf(key, "p%d_sel%n", &p, &consumed) == 1 && consumed == klen &&
        p >= 1 && p <= 2) {
      e.selectedSlot[p - 1] = atoi(val);
      return;
    }
  }
}

static void parseEntryContent(SaveEntry &e, const std::string &content) {
  size_t i = 0;
  while (i < content.size()) {
    size_t nl = content.find('\n', i);
    if (nl == std::string::npos)
      nl = content.size();
    std::string line = content.substr(i, nl - i);
    i = nl + 1;

    while (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;

    size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    parseEntryKey(e, key.c_str(), val.c_str());
  }
}

static std::string serializeEntry(const SaveEntry &e) {
  std::string s;
  char buf[512];

  auto line = [&](const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s += buf;
    s += '\n';
  };

  line("id=%s", e.id.c_str());
  line("name=%s", e.name.c_str());
  line("mode=%d", e.mode);
  line("difficulty=%d", e.difficulty);
  line("speedrun=%d", e.speedrun ? 1 : 0);
  line("twoPlayers=%d", e.twoPlayers ? 1 : 0);
  line("level=%d", e.level);
  line("maxLevel=%d", e.maxLevel);
  line("playtime=%.3f", (double)e.playtime);
  line("timestamp=%lld", (long long)e.timestamp);
  line("created=%lld", (long long)e.created);
  line("runSeed=%u", e.runSeed);
  line("enemyMult=%d", e.enemyMult);
  line("itemMult=%d", e.itemMult);

  for (int p = 0; p < 2; ++p) {
    line("p%d_sel=%d", p + 1, e.selectedSlot[p]);
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      line("p%d_slot%d_kind=%d", p + 1, i, e.inventory[p][i].kind);
      line("p%d_slot%d_count=%d", p + 1, i, e.inventory[p][i].count);
    }
  }

  s += "speedrunSplits=";
  for (size_t i = 0; i < e.speedrunSplits.size(); ++i) {
    if (i)
      s += ',';
    snprintf(buf, sizeof(buf), "%.3f", (double)e.speedrunSplits[i]);
    s += buf;
  }
  s += '\n';

  return s;
}

// =========================================================================
//  Paths
// =========================================================================
std::string SaveManager::savePath(const std::string &id) const {
#if defined(__EMSCRIPTEN__)
  return "/saves/" + id + ".sav";
#else
  return "saves/" + id + ".sav";
#endif
}

std::string SaveManager::indexPath() const {
#if defined(__EMSCRIPTEN__)
  return "/saves/index.cfg";
#else
  return "saves/index.cfg";
#endif
}

std::string SaveManager::newId() const {
  // Timestamp + sufijo aleatorio → colisión imposible incluso creando
  // varios saves en el mismo segundo.
  char buf[64];
  uint32_t r = (uint32_t)(rand() & 0xFFFF);
  snprintf(buf, sizeof(buf), "save_%lld_%04X", (long long)std::time(nullptr),
           r);
  return buf;
}

// =========================================================================
//  Index (lista de ids)
// =========================================================================
void SaveManager::loadIndex(std::vector<std::string> &out) const {
  out.clear();
  std::string content = readFile(indexPath());
  size_t i = 0;
  while (i < content.size()) {
    size_t nl = content.find('\n', i);
    if (nl == std::string::npos)
      nl = content.size();
    std::string line = content.substr(i, nl - i);
    i = nl + 1;
    while (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    out.push_back(line);
  }
}

void SaveManager::saveIndex(const std::vector<std::string> &ids) const {
  std::string s = "# saves index\n";
  for (auto &id : ids) {
    s += id;
    s += '\n';
  }
  writeFile(indexPath(), s);
}

// =========================================================================
//  Refresh
// =========================================================================
void SaveManager::refresh() {
  entries_.clear();

  std::vector<std::string> ids;
  loadIndex(ids);

  for (auto &id : ids) {
    std::string content = readFile(savePath(id));
    if (content.empty())
      continue;

    SaveEntry e;
    e.id = id;
    parseEntryContent(e, content);
    entries_.push_back(std::move(e));
  }

  // Orden: más recientes primero
  std::sort(entries_.begin(), entries_.end(),
            [](const SaveEntry &a, const SaveEntry &b) {
              return a.timestamp > b.timestamp;
            });

  loadSpeedruns();
}

// =========================================================================
//  CRUD
// =========================================================================
std::string SaveManager::create(const SaveEntry &proto) {
  SaveEntry e = proto;
  if (e.id.empty())
    e.id = newId();
  e.created = std::time(nullptr);
  e.timestamp = e.created;

  if (!writeFile(savePath(e.id), serializeEntry(e)))
    return "";

  std::vector<std::string> ids;
  loadIndex(ids);
  if (std::find(ids.begin(), ids.end(), e.id) == ids.end())
    ids.push_back(e.id);
  saveIndex(ids);

  entries_.insert(entries_.begin(), std::move(e));
  return entries_.front().id;
}

std::string SaveManager::duplicate(const std::string &id) {
  const SaveEntry *src = find(id);
  if (!src)
    return "";

  SaveEntry e = *src;
  e.id = newId();
  e.name = src->name + " (copy)";
  e.created = std::time(nullptr);
  e.timestamp = e.created;

  if (!writeFile(savePath(e.id), serializeEntry(e)))
    return "";

  std::vector<std::string> ids;
  loadIndex(ids);
  ids.push_back(e.id);
  saveIndex(ids);

  entries_.insert(entries_.begin(), std::move(e));
  return entries_.front().id;
}

std::string SaveManager::recreate(const std::string &id) {
  SaveEntry *src = find(id);
  if (!src)
    return "";

  src->level = 1;
  src->maxLevel = 1;
  src->playtime = 0.0f;
  src->timestamp = std::time(nullptr);
  src->created = src->timestamp;
  src->speedrunSplits.clear();

  for (int p = 0; p < 2; ++p) {
    src->selectedSlot[p] = 0;
    for (int i = 0; i < C::INV_SLOTS; ++i) {
      src->inventory[p][i].kind = 0;
      src->inventory[p][i].count = 0;
    }
  }

  writeFile(savePath(src->id), serializeEntry(*src));
  return src->id;
}

bool SaveManager::remove(const std::string &id) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&](const SaveEntry &e) { return e.id == id; });
  if (it == entries_.end())
    return false;

  deleteFile(savePath(id));

  std::vector<std::string> ids;
  loadIndex(ids);
  ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
  saveIndex(ids);

  entries_.erase(it);
  return true;
}

bool SaveManager::save(const SaveEntry &e) {
  SaveEntry *cur = find(e.id);
  if (cur)
    *cur = e;
  else
    entries_.push_back(e);

  return writeFile(savePath(e.id), serializeEntry(e));
}

const SaveEntry *SaveManager::find(const std::string &id) const {
  for (auto &e : entries_)
    if (e.id == id)
      return &e;
  return nullptr;
}

SaveEntry *SaveManager::find(const std::string &id) {
  for (auto &e : entries_)
    if (e.id == id)
      return &e;
  return nullptr;
}

// =========================================================================
//  Speedruns (historial global)
// =========================================================================
static std::string speedrunsPath() {
#if defined(__EMSCRIPTEN__)
  return "/saves/speedruns.cfg";
#else
  return "saves/speedruns.cfg";
#endif
}

void SaveManager::loadSpeedruns() {
  speedruns_.clear();
  std::string content = readFile(speedrunsPath());
  if (content.empty())
    return;

  size_t i = 0;
  while (i < content.size()) {
    size_t nl = content.find('\n', i);
    if (nl == std::string::npos)
      nl = content.size();
    std::string line = content.substr(i, nl - i);
    i = nl + 1;
    while (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;

    SpeedrunEntry e;
    // strtok necesita mutable, así que copiamos a un buffer.
    std::vector<char> tmp(line.begin(), line.end());
    tmp.push_back('\0');

    char *tok = strtok(tmp.data(), ",");
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
}

void SaveManager::saveSpeedruns() {
  std::string s = "# speedruns.cfg\n";
  char buf[512];
  for (auto &e : speedruns_) {
    snprintf(buf, sizeof(buf), "%u,%d,%lld,%.3f", e.seed, e.mode,
             (long long)e.date, (double)e.total);
    s += buf;
    for (float sp : e.splits) {
      snprintf(buf, sizeof(buf), ",%.3f", (double)sp);
      s += buf;
    }
    s += '\n';
  }
  writeFile(speedrunsPath(), s);
}

void SaveManager::addSpeedrun(const SpeedrunEntry &e) {
  speedruns_.push_back(e);
  saveSpeedruns();
}

void SaveManager::bestSplitsFor(uint32_t seed, int mode,
                                std::vector<float> &out) const {
  out.clear();
  const float INF = 1e9f;

  for (auto &e : speedruns_) {
    if (e.seed != seed || e.mode != mode)
      continue;
    for (size_t i = 0; i < e.splits.size(); ++i) {
      if (i >= out.size())
        out.resize(i + 1, INF);
      if (e.splits[i] > 0.0f)
        out[i] = std::min(out[i], e.splits[i]);
    }
  }

  for (auto &v : out)
    if (v >= INF)
      v = 0.0f;
}
