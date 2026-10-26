#include "lang.hpp"

#include "game.hpp"
#include "paths.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::unordered_map<std::string, std::string> g_map;
std::string g_current = "es";
std::vector<std::string> g_extraKeys;
} // namespace

namespace Lang {

void load(const char *code) {
  g_map.clear();
  g_current = code ? code : "es";
  std::string path = Paths::translation((g_current + ".txt").c_str());
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return;

  char line[1024];
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
    std::string k = line;
    std::string v = eq + 1;
    std::string unesc;
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] == '\\' && i + 1 < v.size() && v[i + 1] == 'n') {
        unesc += '\n';
        ++i;
      } else
        unesc += v[i];
    }
    g_map[k] = unesc;
  }
  fclose(f);
}

void reload() { load(g_current.c_str()); }

const char *currentLang() { return g_current.c_str(); }

bool hasKey(const char *key) { return key && g_map.find(key) != g_map.end(); }

const char *get(const char *key, const char *fallback) {
  if (!key)
    return fallback ? fallback : "";
  auto it = g_map.find(key);
  if (it == g_map.end())
    return fallback ? fallback : key;
  return it->second.c_str();
}

void loadExtra(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[2048];
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

    std::string k = line;
    std::string v = eq + 1;

    std::string unesc;
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] == '\\' && i + 1 < v.size() && v[i + 1] == 'n') {
        unesc += '\n';
        ++i;
      } else {
        unesc += v[i];
      }
    }

    // Si es la primera vez que aparece en este loadExtra, recordar
    // la key para poder quitarla en unloadExtra.
    if (g_map.find(k) == g_map.end())
      g_extraKeys.push_back(k);

    g_map[k] = unesc;
  }
  fclose(f);
}

void unloadExtra() {
  for (const auto &k : g_extraKeys)
    g_map.erase(k);
  g_extraKeys.clear();
}

} // namespace Lang

void Game::loadLevelTranslations(int lvl) {
  Lang::unloadExtra();
  currentTranslationsLevel = lvl;

  // Primero intentar español (fallback)
  if (strcmp(Lang::currentLang(), "es") != 0) {
    std::string esPath = Paths::levelTranslation("es", lvl);
    FILE *f = fopen(esPath.c_str(), "rb");
    if (f) {
      fclose(f);
      Lang::loadExtra(esPath.c_str());
    }
  }

  // Luego el idioma actual (sobrescribe las claves que coincidan)
  std::string curPath = Paths::levelTranslation(Lang::currentLang(), lvl);
  FILE *f2 = fopen(curPath.c_str(), "rb");
  if (f2) {
    fclose(f2);
    Lang::loadExtra(curPath.c_str());
  }
}

void Game::reloadLevelTranslations() {
  if (currentTranslationsLevel >= 0)
    loadLevelTranslations(currentTranslationsLevel);
}

std::string Game::resolveDialogText(const std::string &raw) const {
  static const std::string prefix = "@key:";
  if (raw.size() > prefix.size() &&
      raw.compare(0, prefix.size(), prefix) == 0) {

    std::string key = raw.substr(prefix.size());
    const char *val = Lang::get(key.c_str(), nullptr);
    // Lang::get devuelve la propia key si no la encuentra
    if (val == nullptr || strcmp(val, key.c_str()) == 0) {
      char buf[300];
      snprintf(buf, sizeof(buf), "[missing key: %s]", key.c_str());
      return buf;
    }
    return val;
  }
  return raw;
}
