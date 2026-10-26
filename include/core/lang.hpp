#pragma once

struct Game;

namespace Lang {
    void load(const char* code);
    void reload();
    const char* currentLang();
    const char* get(const char* key, const char* fallback = nullptr);
    bool hasKey(const char* key);

    void loadExtra(const char* path);
    void unloadExtra();
}

#define L(key, fallback) Lang::get(key, fallback)

