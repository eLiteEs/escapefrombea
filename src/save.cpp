#include "save.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WEB_BUILD 1
#else
#  ifdef _WIN32
#    include <direct.h>
#    define MKDIR_SAVES() _mkdir("saves")
#  else
#    include <sys/stat.h>
#    define MKDIR_SAVES() mkdir("saves", 0755)
#  endif
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  include <string>

static bool ls_save(const char* key, const std::string& data) {
    EM_ASM({
        try {
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        } catch(e) { console.error('localStorage setItem failed', e); }
    }, key, data.c_str());
    return true;
}

static std::string ls_load(const char* key) {
    char buf[8192] = { 0 };
    EM_ASM({
        var v = localStorage.getItem(UTF8ToString($0));
        if (v) stringToUTF8(v, $1, $2);
    }, key, buf, (int)sizeof(buf));
    return std::string(buf);
}

static void ls_remove(const char* key) {
    EM_ASM({
        try { localStorage.removeItem(UTF8ToString($0)); } catch(e) {}
    }, key);
}
#endif

bool SaveManager::saveSlot(int i, const SaveSlot& data) {
    if (i < 0 || i >= NUM_SLOTS) return false;

#if defined(__EMSCRIPTEN__)
    char buf[512];
    snprintf(buf, sizeof(buf),
        "level=%d\nmaxLevel=%d\nmode=%d\ntwoPlayers=%d\nplaytime=%.2f\ntimestamp=%lld\n",
        data.level, data.maxLevel, data.mode, data.twoPlayers ? 1 : 0,
        (double)data.playtime, (long long)std::time(nullptr));
    char key[32];
    snprintf(key, sizeof(key), "efb_slot%d", i + 1);
    ls_save(key, buf);
#else
    MKDIR_SAVES();
    FILE* f = fopen(slotPath(i).c_str(), "w");
    if (!f) return false;
    fprintf(f, "level=%d\n",       data.level);
    fprintf(f, "maxLevel=%d\n",    data.maxLevel);
    fprintf(f, "mode=%d\n",        data.mode);
    fprintf(f, "twoPlayers=%d\n",  data.twoPlayers ? 1 : 0);
    fprintf(f, "playtime=%.2f\n",  (double)data.playtime);
    fprintf(f, "timestamp=%lld\n", (long long)std::time(nullptr));
    fclose(f);
#endif

    slots_[i] = data;
    slots_[i].used = true;
    slots_[i].timestamp = std::time(nullptr);
    return true;
}

bool SaveManager::eraseSlot(int i) {
    if (i < 0 || i >= NUM_SLOTS) return false;
#if defined(__EMSCRIPTEN__)
    char key[32];
    snprintf(key, sizeof(key), "efb_slot%d", i + 1);
    ls_remove(key);
#else
    remove(slotPath(i).c_str());
#endif
    slots_[i] = SaveSlot{};
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
        if (content.empty()) continue;
        // Parsea igual que con fichero
        const char* p = content.c_str();
        char line[256];
        while (sscanf(p, "%255[^\n]\n", line) == 1) {
            // Avanza p al siguiente \n
            const char* nl = strchr(p, '\n');
            if (!nl) break;
            p = nl + 1;
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char* key2 = line;
            const char* val = eq + 1;
            if      (!strcmp(key2, "level"))     slots_[i].level     = atoi(val);
            else if (!strcmp(key2, "maxLevel"))  slots_[i].maxLevel  = atoi(val);
            else if (!strcmp(key2, "mode"))      slots_[i].mode      = atoi(val);
            else if (!strcmp(key2, "twoPlayers"))slots_[i].twoPlayers= atoi(val) != 0;
            else if (!strcmp(key2, "playtime"))  slots_[i].playtime  = (float)atof(val);
            else if (!strcmp(key2, "timestamp")) slots_[i].timestamp = (std::time_t)atoll(val);
        }
        slots_[i].used = true;
#else
        MKDIR_SAVES();
        FILE* f = fopen(slotPath(i).c_str(), "r");
        if (!f) continue;
        // ... el parser actual con fgets ...
        fclose(f);
        slots_[i].used = true;
#endif
    }
}

bool SaveManager::loadSlot(int i, SaveSlot& out) const {
    if (i < 0 || i >= NUM_SLOTS) return false;
    if (!slots_[i].used) return false;
    out = slots_[i];
    return true;
}

int SaveManager::firstFreeSlot() const {
    for (int i = 0; i < NUM_SLOTS; ++i)
        if (!slots_[i].used) return i;
    return -1;
}

bool SaveManager::anyUsed() const {
    for (int i = 0; i < NUM_SLOTS; ++i)
        if (slots_[i].used) return true;
    return false;
}
