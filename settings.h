#ifndef TOU_SETTINGS_H
#define TOU_SETTINGS_H

#include <array>
#include <string>

#include "config.h"

enum SettingsLoadResult {
    SETTINGS_LOAD_MISSING,
    SETTINGS_LOAD_OK,
    SETTINGS_LOAD_INVALID
};

struct LevelSelectionSettings {
    int amount;
    std::array<int, 3> flags;
    std::array<int, 200> order;
};

struct PlayerSettings {
    int difficulty;
    int enabled;
    int team;
    int ship;
    int color;
    std::array<int, 60> weapons;
};

struct AudioSettings {
    int musicEnabled;
    int effectsEnabled;
    int musicVolume;
    int effectsVolume;
};

struct DisplaySettings {
    int resolutionIndex;
    int mode;
    int viewportSize;
    int detail;
};

struct VisualSettings {
    int ambientEmitters;
    int ambientParticles;
    int fogMode;
    int fogRayResolution;
    int fogDetail;
    int fogWobble;
    int skyColorMode;
    int colorOption;
};

struct EventSettings {
    int civilians;
    int bombing;
    int randomTurrets;
    int randomTroopers;
};

struct RuleSettings {
    int gameType;
    int teamCount;
    int teamMode;
    int gameMode;
    int gameModePreset;
    int initialLives;
    int sharedLives;
    int teamRules;
    int friendlyFire;
    int activationGuard;
    int difficultySecondary;
    int roundTime;
    int difficultyDetail;
    int shieldBar;
    int radar;
    int respawnDelay;
    int detonationMode;
    int tickRate;
    int weaponAutoRelease;
};

struct AdvancedSettings {
    std::array<int, 4> statScaling;
    std::array<int, 4> speedScaling;
    std::array<int, 4> miscScaling;
    std::array<int, 4> entityFlags;
    std::array<int, 4> skySettings;
};

struct ControlSettings {
    int pause;
    int camera;
    std::array<int, 6> menu;
    std::array<std::array<int, 8>, 4> players;
};

struct UserSettings {
    int schemaVersion;
    std::string language;
    LevelSelectionSettings levels;
    int playerCount;
    int humanPlayerCount;
    std::array<PlayerSettings, GAME_CONFIG_PLAYER_CAPACITY> players;
    std::array<int, 80> startingWeapons;
    AudioSettings audio;
    DisplaySettings display;
    VisualSettings visuals;
    EventSettings events;
    RuleSettings rules;
    AdvancedSettings advanced;
    std::array<int, 50> enabledWeapons;
    ControlSettings controls;
};

UserSettings Settings_CaptureCurrent(void);
void Settings_Apply(const UserSettings &settings);
SettingsLoadResult Settings_LoadJson(void);
bool Settings_SaveJson(void);
const char *Settings_GetLanguage(void);

#endif
