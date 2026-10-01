#include "save.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR_SAVES() _mkdir("saves")
#else
#  include <sys/stat.h>
#  define MKDIR_SAVES() mkdir("saves", 0755)
#endif

std::string SaveManager::slotPath(int i) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "saves/slot%d.cfg", i + 1);
    return buf;
}

void SaveManager::refresh() {
    MKDIR_SAVES();
    for (int i = 0; i < NUM_SLOTS; ++i) {
        slots_[i] = SaveSlot{};
        FILE* f = fopen(slotPath(i).c_str(), "r");
        if (!f) continue;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            for (char* p = line; *p; ++p)
                if (*p == '\r' || *p == '\n') { *p = 0; break; }
            char* eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            const char* key = line;
            const char* val = eq + 1;
            if      (!strcmp(key, "level"))     slots_[i].level     = atoi(val);
            else if (!strcmp(key, "maxLevel"))  slots_[i].maxLevel  = atoi(val);
            else if (!strcmp(key, "mode"))      slots_[i].mode      = atoi(val);
            else if (!strcmp(key, "twoPlayers"))slots_[i].twoPlayers= atoi(val) != 0;
            else if (!strcmp(key, "playtime"))  slots_[i].playtime  = (float)atof(val);
            else if (!strcmp(key, "timestamp")) slots_[i].timestamp = (std::time_t)atoll(val);
        }
        fclose(f);
        slots_[i].used = true;
    }
}

bool SaveManager::saveSlot(int i, const SaveSlot& data) {
    if (i < 0 || i >= NUM_SLOTS) return false;
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

    slots_[i] = data;
    slots_[i].used = true;
    slots_[i].timestamp = std::time(nullptr);
    return true;
}

bool SaveManager::loadSlot(int i, SaveSlot& out) const {
    if (i < 0 || i >= NUM_SLOTS) return false;
    if (!slots_[i].used) return false;
    out = slots_[i];
    return true;
}

bool SaveManager::eraseSlot(int i) {
    if (i < 0 || i >= NUM_SLOTS) return false;
    remove(slotPath(i).c_str());
    slots_[i] = SaveSlot{};
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
