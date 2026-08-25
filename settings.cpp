#include "settings.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include "gamestate.h"

using nlohmann::json;

namespace {

const int SETTINGS_SCHEMA_VERSION = 1;
const char *const SETTINGS_PATH = "settings.json";
const char *const SETTINGS_TEMP_PATH = "settings.json.tmp";
std::string g_Language = "en";
std::string g_RecentLanHost = "127.0.0.1";
int g_RecentLanPort = 27015;
int g_RecentLanTeam = 1;

int ClampInt(long long value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return static_cast<int>(value);
}

void ReadInt(const json &object, const char *key, int &target,
             int minimum, int maximum)
{
    json::const_iterator found = object.find(key);
    if (found == object.end()) return;
    if (!found->is_number_integer() && !found->is_number_unsigned()) {
        LOG("[CFG] Ignoring non-integer setting: %s\n", key);
        return;
    }
    try {
        target = ClampInt(found->get<long long>(), minimum, maximum);
    } catch (const json::exception &) {
        LOG("[CFG] Ignoring out-of-range setting: %s\n", key);
    }
}

template <size_t Size>
void ReadIntArray(const json &object, const char *key,
                  std::array<int, Size> &target, int minimum, int maximum)
{
    json::const_iterator found = object.find(key);
    if (found == object.end()) return;
    if (!found->is_array()) {
        LOG("[CFG] Ignoring non-array setting: %s\n", key);
        return;
    }
    const size_t count = std::min(Size, found->size());
    for (size_t i = 0; i < count; ++i) {
        const json &entry = (*found)[i];
        if (!entry.is_number_integer() && !entry.is_number_unsigned()) continue;
        try {
            target[i] = ClampInt(entry.get<long long>(), minimum, maximum);
        } catch (const json::exception &) {
            LOG("[CFG] Ignoring out-of-range %s[%u]\n", key,
                static_cast<unsigned int>(i));
        }
    }
}

template <size_t Size>
json WriteIntArray(const std::array<int, Size> &values)
{
    json result = json::array();
    for (size_t i = 0; i < Size; ++i) result.push_back(values[i]);
    return result;
}

void ReadLanguage(const json &root, std::string &language)
{
    json::const_iterator found = root.find("language");
    if (found == root.end() || !found->is_string()) return;
    const std::string candidate = found->get<std::string>();
    if (candidate == "en" || candidate == "es" ||
        candidate == "pt-BR" || candidate == "fi") {
        language = candidate;
    } else {
        LOG("[CFG] Unknown language '%s'; retaining %s\n",
            candidate.c_str(), language.c_str());
    }
}

void ReadString(const json &object, const char *key, std::string &target,
                size_t maximumLength)
{
    json::const_iterator found = object.find(key);
    if (found == object.end() || !found->is_string()) return;
    const std::string value = found->get<std::string>();
    if (!value.empty() && value.size() <= maximumLength)
        target = value;
}

json SettingsToJson(const UserSettings &settings)
{
    json root;
    root["schemaVersion"] = SETTINGS_SCHEMA_VERSION;
    root["language"] = settings.language;

    root["levels"] = {
        {"amount", settings.levels.amount},
        {"flags", WriteIntArray(settings.levels.flags)},
        {"order", WriteIntArray(settings.levels.order)}
    };

    json playerEntries = json::array();
    for (size_t i = 0; i < settings.players.size(); ++i) {
        const PlayerSettings &player = settings.players[i];
        playerEntries.push_back({
            {"difficulty", player.difficulty},
            {"enabled", player.enabled},
            {"team", player.team},
            {"ship", player.ship},
            {"color", player.color},
            {"weapons", WriteIntArray(player.weapons)}
        });
    }
    root["players"] = {
        {"count", settings.playerCount},
        {"humanCount", settings.humanPlayerCount},
        {"entries", playerEntries},
        {"startingWeapons", WriteIntArray(settings.startingWeapons)}
    };

    root["audio"] = {
        {"musicEnabled", settings.audio.musicEnabled},
        {"effectsEnabled", settings.audio.effectsEnabled},
        {"musicVolume", settings.audio.musicVolume},
        {"effectsVolume", settings.audio.effectsVolume}
    };
    root["display"] = {
        {"resolutionIndex", settings.display.resolutionIndex},
        {"mode", settings.display.mode == 0 ? "windowed" : "fullscreen"},
        {"viewportSize", settings.display.viewportSize},
        {"detail", settings.display.detail}
    };
    root["visuals"] = {
        {"ambientEmitters", settings.visuals.ambientEmitters},
        {"ambientParticles", settings.visuals.ambientParticles},
        {"fogMode", settings.visuals.fogMode},
        {"fogRayResolution", settings.visuals.fogRayResolution},
        {"fogDetail", settings.visuals.fogDetail},
        {"fogWobble", settings.visuals.fogWobble},
        {"skyColorMode", settings.visuals.skyColorMode},
        {"colorOption", settings.visuals.colorOption}
    };
    root["events"] = {
        {"civilians", settings.events.civilians},
        {"bombing", settings.events.bombing},
        {"randomTurrets", settings.events.randomTurrets},
        {"randomTroopers", settings.events.randomTroopers}
    };
    root["rules"] = {
        {"gameType", settings.rules.gameType},
        {"teamCount", settings.rules.teamCount},
        {"teamMode", settings.rules.teamMode},
        {"gameMode", settings.rules.gameMode},
        {"gameModePreset", settings.rules.gameModePreset},
        {"initialLives", settings.rules.initialLives},
        {"sharedLives", settings.rules.sharedLives},
        {"teamRules", settings.rules.teamRules},
        {"friendlyFire", settings.rules.friendlyFire},
        {"activationGuard", settings.rules.activationGuard},
        {"difficultySecondary", settings.rules.difficultySecondary},
        {"roundTime", settings.rules.roundTime},
        {"difficultyDetail", settings.rules.difficultyDetail},
        {"shieldBar", settings.rules.shieldBar},
        {"radar", settings.rules.radar},
        {"respawnDelay", settings.rules.respawnDelay},
        {"detonationMode", settings.rules.detonationMode},
        {"tickRate", settings.rules.tickRate},
        {"weaponAutoRelease", settings.rules.weaponAutoRelease}
    };
    root["advanced"] = {
        {"statScaling", WriteIntArray(settings.advanced.statScaling)},
        {"speedScaling", WriteIntArray(settings.advanced.speedScaling)},
        {"miscScaling", WriteIntArray(settings.advanced.miscScaling)},
        {"entityFlags", WriteIntArray(settings.advanced.entityFlags)},
        {"skySettings", WriteIntArray(settings.advanced.skySettings)}
    };
    root["weapons"] = {
        {"enabled", WriteIntArray(settings.enabledWeapons)}
    };

    json controlPlayers = json::array();
    for (size_t i = 0; i < settings.controls.players.size(); ++i)
        controlPlayers.push_back(WriteIntArray(settings.controls.players[i]));
    root["controls"] = {
        {"pause", settings.controls.pause},
        {"camera", settings.controls.camera},
        {"menu", WriteIntArray(settings.controls.menu)},
        {"players", controlPlayers}
    };
    root["network"] = {
        {"recentHost", settings.network.recentHost},
        {"recentPort", settings.network.recentPort},
        {"recentTeam", settings.network.recentTeam}
    };
    return root;
}

void OverlayJson(UserSettings &settings, const json &root)
{
    ReadLanguage(root, settings.language);

    json::const_iterator section = root.find("levels");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "amount", settings.levels.amount, 1, 255);
        ReadIntArray(*section, "flags", settings.levels.flags, 0, 255);
        ReadIntArray(*section, "order", settings.levels.order, -1, 299);
    }

    section = root.find("players");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "count", settings.playerCount, 1,
                GAME_CONFIG_PLAYER_CAPACITY);
        ReadInt(*section, "humanCount", settings.humanPlayerCount, 0, 4);
        ReadIntArray(*section, "startingWeapons", settings.startingWeapons, 0, 255);
        json::const_iterator entries = section->find("entries");
        if (entries != section->end() && entries->is_array()) {
            const size_t count = std::min(settings.players.size(), entries->size());
            for (size_t i = 0; i < count; ++i) {
                const json &entry = (*entries)[i];
                if (!entry.is_object()) continue;
                PlayerSettings &player = settings.players[i];
                ReadInt(entry, "difficulty", player.difficulty, 0, 255);
                ReadInt(entry, "enabled", player.enabled, 0, 1);
                ReadInt(entry, "team", player.team, 0, 255);
                ReadInt(entry, "ship", player.ship, 0, 8);
                ReadInt(entry, "color", player.color, 0, 255);
                ReadIntArray(entry, "weapons", player.weapons, 0, 1);
            }
        }
    }

    section = root.find("audio");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "musicEnabled", settings.audio.musicEnabled, 0, 1);
        ReadInt(*section, "effectsEnabled", settings.audio.effectsEnabled, 0, 1);
        ReadInt(*section, "musicVolume", settings.audio.musicVolume, 0, 100);
        ReadInt(*section, "effectsVolume", settings.audio.effectsVolume, 0, 100);
    }

    section = root.find("display");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "resolutionIndex", settings.display.resolutionIndex, 0, 255);
        ReadInt(*section, "viewportSize", settings.display.viewportSize, 0, 2);
        ReadInt(*section, "detail", settings.display.detail, 0, 255);
        json::const_iterator mode = section->find("mode");
        if (mode != section->end() && mode->is_string()) {
            const std::string value = mode->get<std::string>();
            if (value == "windowed") settings.display.mode = 0;
            else if (value == "fullscreen") settings.display.mode = 1;
        }
    }

    section = root.find("visuals");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "ambientEmitters", settings.visuals.ambientEmitters, 0, 1);
        ReadInt(*section, "ambientParticles", settings.visuals.ambientParticles, -128, 127);
        ReadInt(*section, "fogMode", settings.visuals.fogMode, -128, 127);
        ReadInt(*section, "fogRayResolution", settings.visuals.fogRayResolution, -128, 127);
        ReadInt(*section, "fogDetail", settings.visuals.fogDetail, -128, 127);
        ReadInt(*section, "fogWobble", settings.visuals.fogWobble, -128, 127);
        ReadInt(*section, "skyColorMode", settings.visuals.skyColorMode, -128, 127);
        ReadInt(*section, "colorOption", settings.visuals.colorOption, -128, 127);
    }

    section = root.find("events");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "civilians", settings.events.civilians, -128, 127);
        ReadInt(*section, "bombing", settings.events.bombing, -128, 127);
        ReadInt(*section, "randomTurrets", settings.events.randomTurrets, -128, 127);
        ReadInt(*section, "randomTroopers", settings.events.randomTroopers, -128, 127);
    }

    section = root.find("rules");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "gameType", settings.rules.gameType, -128, 127);
        ReadInt(*section, "teamCount", settings.rules.teamCount, -128, 127);
        ReadInt(*section, "teamMode", settings.rules.teamMode, -128, 127);
        ReadInt(*section, "gameMode", settings.rules.gameMode, -128, 127);
        ReadInt(*section, "gameModePreset", settings.rules.gameModePreset, -128, 127);
        ReadInt(*section, "initialLives", settings.rules.initialLives, 0, 255);
        ReadInt(*section, "sharedLives", settings.rules.sharedLives, -128, 127);
        ReadInt(*section, "teamRules", settings.rules.teamRules, -128, 127);
        ReadInt(*section, "friendlyFire", settings.rules.friendlyFire, -128, 127);
        ReadInt(*section, "activationGuard", settings.rules.activationGuard, -128, 127);
        ReadInt(*section, "difficultySecondary", settings.rules.difficultySecondary, -128, 127);
        ReadInt(*section, "roundTime", settings.rules.roundTime, -128, 127);
        ReadInt(*section, "difficultyDetail", settings.rules.difficultyDetail, -128, 127);
        ReadInt(*section, "shieldBar", settings.rules.shieldBar, -128, 127);
        ReadInt(*section, "radar", settings.rules.radar, -128, 127);
        ReadInt(*section, "respawnDelay", settings.rules.respawnDelay, -128, 127);
        ReadInt(*section, "detonationMode", settings.rules.detonationMode, -128, 127);
        ReadInt(*section, "tickRate", settings.rules.tickRate, 0, 255);
        ReadInt(*section, "weaponAutoRelease", settings.rules.weaponAutoRelease, -128, 127);
    }

    section = root.find("advanced");
    if (section != root.end() && section->is_object()) {
        ReadIntArray(*section, "statScaling", settings.advanced.statScaling, 0, 255);
        ReadIntArray(*section, "speedScaling", settings.advanced.speedScaling, 0, 255);
        ReadIntArray(*section, "miscScaling", settings.advanced.miscScaling, 0, 255);
        ReadIntArray(*section, "entityFlags", settings.advanced.entityFlags, 0, 255);
        ReadIntArray(*section, "skySettings", settings.advanced.skySettings, 0, 255);
    }

    section = root.find("weapons");
    if (section != root.end() && section->is_object())
        ReadIntArray(*section, "enabled", settings.enabledWeapons, 0, 1);

    section = root.find("controls");
    if (section != root.end() && section->is_object()) {
        ReadInt(*section, "pause", settings.controls.pause, 0, 255);
        ReadInt(*section, "camera", settings.controls.camera, 0, 255);
        ReadIntArray(*section, "menu", settings.controls.menu, 0, 255);
        json::const_iterator players = section->find("players");
        if (players != section->end() && players->is_array()) {
            const size_t count = std::min(settings.controls.players.size(), players->size());
            for (size_t i = 0; i < count; ++i) {
                json wrapper;
                wrapper["keys"] = (*players)[i];
                ReadIntArray(wrapper, "keys", settings.controls.players[i], 0, 255);
            }
        }
    }

    section = root.find("network");
    if (section != root.end() && section->is_object()) {
        ReadString(*section, "recentHost", settings.network.recentHost, 63);
        ReadInt(*section, "recentPort", settings.network.recentPort, 1, 65535);
        ReadInt(*section, "recentTeam", settings.network.recentTeam, 1, 2);
    }
}

} // namespace

UserSettings Settings_CaptureCurrent(void)
{
    UserSettings settings = {};
    settings.schemaVersion = SETTINGS_SCHEMA_VERSION;
    settings.language = g_Language;
    settings.levels.amount = g_GameConfig.values.active_level_count;
    for (size_t i = 0; i < settings.levels.flags.size(); ++i)
        settings.levels.flags[i] = g_GameConfig.values.level_flags[i];
    for (size_t i = 0; i < settings.levels.order.size(); ++i)
        settings.levels.order[i] = g_GameConfig.values.level_order[i];

    settings.playerCount = g_GameConfig.values.player_count;
    settings.humanPlayerCount = g_GameConfig.values.human_player_count;
    for (size_t i = 0; i < settings.players.size(); ++i) {
        PlayerSettings &player = settings.players[i];
        player.difficulty = g_GameConfig.values.player_difficulty[i];
        player.enabled = g_GameConfig.values.player_enabled[i];
        player.team = g_GameConfig.values.player_team[i];
        player.ship = g_GameConfig.values.player_ship[i];
        player.color = g_GameConfig.values.player_color[i];
        for (size_t j = 0; j < player.weapons.size(); ++j)
            player.weapons[j] = g_GameConfig.values.player_weapon_enabled[i][j];
    }
    for (size_t i = 0; i < settings.startingWeapons.size(); ++i)
        settings.startingWeapons[i] = g_GameConfig.values.starting_weapon[i];

    settings.audio = {
        g_GameConfig.values.music_enabled, g_GameConfig.values.sound_enabled,
        g_GameConfig.values.music_volume, g_GameConfig.values.sound_volume
    };
    settings.display = {
        g_GameConfig.values.resolution_index, g_WindowMode,
        g_GameConfig.values.display_reserved, g_GameConfig.values.display_detail
    };
    settings.visuals = {
        g_GameConfig.values.ambient_emitters, g_GameConfig.values.ambient_particles,
        g_GameConfig.values.fog_mode, g_GameConfig.values.fog_ray_resolution,
        g_GameConfig.values.fog_detail, g_GameConfig.values.fog_wobble,
        g_GameConfig.values.sky_color_mode, g_GameConfig.values.saved_color_option
    };
    settings.events = {
        g_GameConfig.values.civilians, g_GameConfig.values.bombing,
        g_GameConfig.values.random_turrets_at_start,
        g_GameConfig.values.random_troopers_at_start
    };
    settings.rules = {
        g_GameConfig.values.game_type, g_GameConfig.values.team_count,
        g_GameConfig.values.team_mode, g_GameConfig.values.game_mode,
        g_GameConfig.values.game_mode_preset, g_GameConfig.values.initial_lives,
        g_GameConfig.values.shared_lives, g_GameConfig.values.team_rules,
        g_GameConfig.values.friendly_fire, g_GameConfig.values.activation_guard,
        g_GameConfig.values.difficulty_secondary, g_GameConfig.values.round_time,
        g_GameConfig.values.difficulty_detail, g_GameConfig.values.shield_bar,
        g_GameConfig.values.radar, g_GameConfig.values.respawn_delay,
        g_GameConfig.values.detonation_mode, g_GameConfig.values.tick_rate,
        g_GameConfig.values.weapon_auto_release
    };

    for (size_t i = 0; i < 4; ++i) {
        settings.advanced.statScaling[i] = g_GameConfig.values.stat_scaling_bytes[i];
        settings.advanced.speedScaling[i] = g_GameConfig.values.speed_scaling_bytes[i];
        settings.advanced.miscScaling[i] = g_GameConfig.values.misc_scaling_bytes[i];
        settings.advanced.entityFlags[i] = g_GameConfig.values.entity_flags[i];
        settings.advanced.skySettings[i] = g_GameConfig.values.sky_settings_bytes[i];
    }
    for (size_t i = 0; i < settings.enabledWeapons.size(); ++i)
        settings.enabledWeapons[i] = g_GameConfig.values.global_weapon_enabled[i];

    settings.controls.pause = g_GameConfig.values.pause_key;
    settings.controls.camera = g_GameConfig.values.camera_key;
    for (size_t i = 0; i < settings.controls.menu.size(); ++i)
        settings.controls.menu[i] = g_GameConfig.values.menu_keys[i];
    for (size_t i = 0; i < settings.controls.players.size(); ++i)
        for (size_t j = 0; j < settings.controls.players[i].size(); ++j)
            settings.controls.players[i][j] = g_GameConfig.values.player_keys[i][j];
    settings.network = {g_RecentLanHost, g_RecentLanPort, g_RecentLanTeam};
    return settings;
}

void Settings_Apply(const UserSettings &settings)
{
    g_Language = settings.language;
    g_GameConfig.values.active_level_count = static_cast<uint8_t>(settings.levels.amount);
    for (size_t i = 0; i < settings.levels.flags.size(); ++i)
        g_GameConfig.values.level_flags[i] = static_cast<uint8_t>(settings.levels.flags[i]);
    for (size_t i = 0; i < settings.levels.order.size(); ++i)
        g_GameConfig.values.level_order[i] = settings.levels.order[i];

    g_GameConfig.values.player_count = static_cast<uint8_t>(settings.playerCount);
    g_GameConfig.values.human_player_count = static_cast<uint8_t>(settings.humanPlayerCount);
    for (size_t i = 0; i < settings.players.size(); ++i) {
        const PlayerSettings &player = settings.players[i];
        g_GameConfig.values.player_difficulty[i] = static_cast<uint8_t>(player.difficulty);
        g_GameConfig.values.player_enabled[i] = static_cast<uint8_t>(player.enabled);
        g_GameConfig.values.player_team[i] = static_cast<uint8_t>(player.team);
        g_GameConfig.values.player_ship[i] = static_cast<uint8_t>(player.ship);
        g_GameConfig.values.player_color[i] = static_cast<uint8_t>(player.color);
        for (size_t j = 0; j < player.weapons.size(); ++j)
            g_GameConfig.values.player_weapon_enabled[i][j] =
                static_cast<uint8_t>(player.weapons[j]);
    }
    for (size_t i = 0; i < settings.startingWeapons.size(); ++i)
        g_GameConfig.values.starting_weapon[i] =
            static_cast<uint8_t>(settings.startingWeapons[i]);

    g_GameConfig.values.music_enabled = static_cast<uint8_t>(settings.audio.musicEnabled);
    g_GameConfig.values.sound_enabled = static_cast<uint8_t>(settings.audio.effectsEnabled);
    g_GameConfig.values.music_volume = static_cast<uint8_t>(settings.audio.musicVolume);
    g_GameConfig.values.sound_volume = static_cast<uint8_t>(settings.audio.effectsVolume);
    g_GameConfig.values.resolution_index = static_cast<uint8_t>(settings.display.resolutionIndex);
    g_WindowMode = static_cast<unsigned char>(settings.display.mode);
    g_GameConfig.values.display_reserved = static_cast<uint8_t>(settings.display.viewportSize);
    g_GameConfig.values.display_detail = static_cast<uint8_t>(settings.display.detail);

#define APPLY_SIGNED(field, value) g_GameConfig.values.field = static_cast<int8_t>(value)
    g_GameConfig.values.ambient_emitters = static_cast<uint8_t>(settings.visuals.ambientEmitters);
    APPLY_SIGNED(ambient_particles, settings.visuals.ambientParticles);
    APPLY_SIGNED(fog_mode, settings.visuals.fogMode);
    APPLY_SIGNED(fog_ray_resolution, settings.visuals.fogRayResolution);
    APPLY_SIGNED(fog_detail, settings.visuals.fogDetail);
    APPLY_SIGNED(fog_wobble, settings.visuals.fogWobble);
    APPLY_SIGNED(sky_color_mode, settings.visuals.skyColorMode);
    APPLY_SIGNED(saved_color_option, settings.visuals.colorOption);
    APPLY_SIGNED(civilians, settings.events.civilians);
    APPLY_SIGNED(bombing, settings.events.bombing);
    APPLY_SIGNED(random_turrets_at_start, settings.events.randomTurrets);
    APPLY_SIGNED(random_troopers_at_start, settings.events.randomTroopers);
    APPLY_SIGNED(game_type, settings.rules.gameType);
    APPLY_SIGNED(team_count, settings.rules.teamCount);
    APPLY_SIGNED(team_mode, settings.rules.teamMode);
    APPLY_SIGNED(game_mode, settings.rules.gameMode);
    APPLY_SIGNED(game_mode_preset, settings.rules.gameModePreset);
    g_GameConfig.values.initial_lives = static_cast<uint8_t>(settings.rules.initialLives);
    APPLY_SIGNED(shared_lives, settings.rules.sharedLives);
    APPLY_SIGNED(team_rules, settings.rules.teamRules);
    APPLY_SIGNED(friendly_fire, settings.rules.friendlyFire);
    APPLY_SIGNED(activation_guard, settings.rules.activationGuard);
    APPLY_SIGNED(difficulty_secondary, settings.rules.difficultySecondary);
    APPLY_SIGNED(round_time, settings.rules.roundTime);
    APPLY_SIGNED(difficulty_detail, settings.rules.difficultyDetail);
    APPLY_SIGNED(shield_bar, settings.rules.shieldBar);
    APPLY_SIGNED(radar, settings.rules.radar);
    APPLY_SIGNED(respawn_delay, settings.rules.respawnDelay);
    APPLY_SIGNED(detonation_mode, settings.rules.detonationMode);
    g_GameConfig.values.tick_rate = static_cast<uint8_t>(settings.rules.tickRate);
    APPLY_SIGNED(weapon_auto_release, settings.rules.weaponAutoRelease);
#undef APPLY_SIGNED

    for (size_t i = 0; i < 4; ++i) {
        g_GameConfig.values.stat_scaling_bytes[i] = static_cast<uint8_t>(settings.advanced.statScaling[i]);
        g_GameConfig.values.speed_scaling_bytes[i] = static_cast<uint8_t>(settings.advanced.speedScaling[i]);
        g_GameConfig.values.misc_scaling_bytes[i] = static_cast<uint8_t>(settings.advanced.miscScaling[i]);
        g_GameConfig.values.entity_flags[i] = static_cast<uint8_t>(settings.advanced.entityFlags[i]);
        g_GameConfig.values.sky_settings_bytes[i] = static_cast<uint8_t>(settings.advanced.skySettings[i]);
    }
    for (size_t i = 0; i < settings.enabledWeapons.size(); ++i)
        g_GameConfig.values.global_weapon_enabled[i] =
            static_cast<uint8_t>(settings.enabledWeapons[i]);

    g_GameConfig.values.pause_key = static_cast<uint8_t>(settings.controls.pause);
    g_GameConfig.values.camera_key = static_cast<uint8_t>(settings.controls.camera);
    for (size_t i = 0; i < settings.controls.menu.size(); ++i)
        g_GameConfig.values.menu_keys[i] = static_cast<uint8_t>(settings.controls.menu[i]);
    for (size_t i = 0; i < settings.controls.players.size(); ++i)
        for (size_t j = 0; j < settings.controls.players[i].size(); ++j)
            g_GameConfig.values.player_keys[i][j] =
                static_cast<uint8_t>(settings.controls.players[i][j]);
    g_RecentLanHost = settings.network.recentHost;
    g_RecentLanPort = settings.network.recentPort;
    g_RecentLanTeam = settings.network.recentTeam;
}

SettingsLoadResult Settings_LoadJson(void)
{
    std::ifstream input(SETTINGS_PATH, std::ios::binary);
    if (!input.is_open()) return SETTINGS_LOAD_MISSING;
    try {
        json root;
        input >> root;
        if (!root.is_object()) throw std::runtime_error("root is not an object");
        if (!root.contains("schemaVersion") ||
            (!root["schemaVersion"].is_number_integer() &&
             !root["schemaVersion"].is_number_unsigned())) {
            throw std::runtime_error("schemaVersion is missing or invalid");
        }
        int schemaVersion = 0;
        ReadInt(root, "schemaVersion", schemaVersion, 1,
                std::numeric_limits<int>::max());
        if (schemaVersion > SETTINGS_SCHEMA_VERSION) {
            LOG("[CFG] settings.json schema %d is newer than supported schema %d; "
                "known fields will be loaded\n", schemaVersion, SETTINGS_SCHEMA_VERSION);
        }
        UserSettings settings = Settings_CaptureCurrent();
        OverlayJson(settings, root);
        settings.schemaVersion = SETTINGS_SCHEMA_VERSION;
        Settings_Apply(settings);
        LOG("[CFG] Loaded settings.json (schema %d)\n", schemaVersion);
        return SETTINGS_LOAD_OK;
    } catch (const std::exception &error) {
        LOG("[CFG] Ignoring invalid settings.json: %s\n", error.what());
        return SETTINGS_LOAD_INVALID;
    }
}

bool Settings_SaveJson(void)
{
    try {
        const json root = SettingsToJson(Settings_CaptureCurrent());
        std::ofstream output(SETTINGS_TEMP_PATH,
                             std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            LOG("[CFG] Could not create %s\n", SETTINGS_TEMP_PATH);
            return false;
        }
        output << root.dump(2) << '\n';
        output.flush();
        const bool writeOk = output.good();
        output.close();
        if (!writeOk) {
            SDL_RemovePath(SETTINGS_TEMP_PATH);
            LOG("[CFG] Failed while writing %s\n", SETTINGS_TEMP_PATH);
            return false;
        }
        if (!SDL_RenamePath(SETTINGS_TEMP_PATH, SETTINGS_PATH)) {
            LOG("[CFG] Could not replace %s: %s\n", SETTINGS_PATH, SDL_GetError());
            SDL_RemovePath(SETTINGS_TEMP_PATH);
            return false;
        }
        return true;
    } catch (const std::exception &error) {
        SDL_RemovePath(SETTINGS_TEMP_PATH);
        LOG("[CFG] Could not save settings.json: %s\n", error.what());
        return false;
    }
}

const char *Settings_GetLanguage(void)
{
    return g_Language.c_str();
}

bool Settings_SetLanguage(const char *language)
{
    if (!language) return false;
    const std::string candidate(language);
    if (candidate != "en" && candidate != "es" &&
        candidate != "pt-BR" && candidate != "fi")
        return false;
    g_Language = candidate;
    return true;
}

const char *Settings_GetRecentLanHost(void)
{
    return g_RecentLanHost.c_str();
}

int Settings_GetRecentLanPort(void)
{
    return g_RecentLanPort;
}

int Settings_GetRecentLanTeam(void)
{
    return g_RecentLanTeam;
}

bool Settings_RememberLanEndpoint(const char *host, int port, int team)
{
    if (host == NULL || host[0] == 0 || strlen(host) > 63 ||
        port < 1 || port > 65535 || team < 1 || team > 2)
        return false;
    g_RecentLanHost = host;
    g_RecentLanPort = port;
    g_RecentLanTeam = team;
    return Settings_SaveJson();
}

void Settings_ResetAuxiliaryDefaults(void)
{
    g_Language = "en";
    g_RecentLanHost = "127.0.0.1";
    g_RecentLanPort = 27015;
    g_RecentLanTeam = 1;
}
