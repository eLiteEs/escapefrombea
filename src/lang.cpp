#include "lang.h"
#include "paths.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string> g_map;
    std::string g_current = "es";
}

namespace Lang {

void load(const char* code) {
    g_map.clear();
    g_current = code ? code : "es";
    std::string path = Paths::translation((g_current + ".txt").c_str());
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        for (char* p = line; *p; ++p) if (*p == '\r' || *p == '\n') { *p = 0; break; }
        if (line[0] == '#' || line[0] == 0) continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        std::string k = line;
        std::string v = eq + 1;
        std::string unesc;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size() && v[i+1] == 'n') { unesc += '\n'; ++i; }
            else unesc += v[i];
        }
        g_map[k] = unesc;
    }
    fclose(f);
}

void reload() { load(g_current.c_str()); }

const char* currentLang() { return g_current.c_str(); }

bool hasKey(const char* key) {
    return key && g_map.find(key) != g_map.end();
}

const char* get(const char* key, const char* fallback) {
    if (!key) return fallback ? fallback : "";
    auto it = g_map.find(key);
    if (it == g_map.end()) return fallback ? fallback : key;
    return it->second.c_str();
}

}  // namespace Lang
   //
