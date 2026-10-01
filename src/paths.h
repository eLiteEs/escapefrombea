#pragma once
#include <string>

namespace Paths {
    constexpr const char* SPRITES      = "assets/sprites/";
    constexpr const char* SOUNDS       = "assets/sounds/";
    constexpr const char* MUSIC        = "assets/music/";
    constexpr const char* STORIES      = "assets/stories/";
    constexpr const char* TRANSLATIONS = "assets/translations/";
    constexpr const char* LEGACY_AUDIO = "assets/audio/";

    inline std::string sprite(const char* n)      { return std::string(SPRITES)      + n; }
    inline std::string sound(const char* n)       { return std::string(SOUNDS)       + n; }
    inline std::string music(const char* n)       { return std::string(MUSIC)        + n; }
    inline std::string story(const char* n)       { return std::string(STORIES)      + n; }
    inline std::string translation(const char* n) { return std::string(TRANSLATIONS) + n; }
    inline std::string legacyAudio(const char* n) { return std::string(LEGACY_AUDIO) + n; }
}

