#ifndef TOU_CONFIG_H
#define TOU_CONFIG_H

#include <stddef.h>
#include <stdint.h>

enum { GAME_CONFIG_SIZE = 6408, GAME_CONFIG_PLAYER_CAPACITY = 64 };

#pragma pack(push, 1)
typedef struct GameConfigLayout {
    uint8_t active_level_count;             /* 0x0000 */
    uint8_t level_flags[3];                 /* 0x0001 */
    int32_t level_order[200];               /* 0x0004 */
    uint8_t player_count;                   /* 0x0324 */
    uint8_t human_player_count;             /* 0x0325 */
    uint8_t player_difficulty[64];           /* 0x0326 */
    uint8_t reserved_0366[0x10];
    uint8_t player_enabled[64];              /* 0x0376 */
    uint8_t reserved_03b6[0x10];
    uint8_t player_team[64];                 /* 0x03c6 */
    uint8_t reserved_0406[0x10];
    uint8_t player_ship[64];                 /* 0x0416 */
    uint8_t reserved_0456[0x10];
    uint8_t player_color[64];                /* 0x0466 */
    uint8_t reserved_04a6[0x10];
    uint8_t player_weapon_enabled[64][60];   /* 0x04b6 */
    uint8_t reserved_13b6[0x3c0];
    uint8_t starting_weapon[80];             /* 0x1776 */

    uint8_t music_enabled;                   /* 0x17c6 */
    uint8_t sound_enabled;                   /* 0x17c7 */
    uint8_t music_volume;                    /* 0x17c8 */
    uint8_t sound_volume;                    /* 0x17c9 */
    uint8_t sound_flags[2];                  /* 0x17ca */
    uint8_t display_flags;                   /* 0x17cc */
    uint8_t resolution_index;                /* 0x17cd */
    uint8_t display_reserved;                /* 0x17ce */
    uint8_t display_detail;                  /* 0x17cf */

    uint8_t ambient_emitters;                /* 0x17d0 */
    int8_t game_type;                        /* 0x17d1 */
    int8_t team_count;                       /* 0x17d2 */
    int8_t team_mode;                        /* 0x17d3 */
    int8_t ambient_particles;                /* 0x17d4 */
    int8_t fog_mode;                         /* 0x17d5 */
    int8_t fog_ray_resolution;               /* 0x17d6 */
    int8_t fog_detail;                       /* 0x17d7 */
    int8_t fog_wobble;                       /* 0x17d8 */
    int8_t sky_color_mode;                   /* 0x17d9 */
    int8_t saved_color_option;               /* 0x17da */
    uint8_t reserved_17db;
    int8_t critter_spawns;                   /* 0x17dc */
    int8_t team_base_placement;              /* 0x17dd */
    int8_t debris_difficulty;                /* 0x17de */
    int8_t trooper_difficulty;               /* 0x17df */
    int8_t game_mode;                        /* 0x17e0 */
    int8_t game_mode_preset;                 /* 0x17e1 */
    uint8_t initial_lives;                   /* 0x17e2; short view overlaps next byte */
    int8_t shared_lives;                     /* 0x17e3 */
    int8_t team_rules;                       /* 0x17e4 */
    int8_t friendly_fire;                    /* 0x17e5 */
    int8_t activation_guard;                 /* 0x17e6 */
    int8_t difficulty_secondary;             /* 0x17e7 */
    int8_t round_time;                       /* 0x17e8 */
    int8_t difficulty_detail;                /* 0x17e9 */
    int8_t shield_bar;                       /* 0x17ea */
    int8_t radar;                            /* 0x17eb */
    int8_t respawn_delay;                    /* 0x17ec */
    int8_t detonation_mode;                  /* 0x17ed */
    uint8_t tick_rate;                       /* 0x17ee; short view overlaps next byte */
    int8_t weapon_auto_release;              /* 0x17ef */
    union { int32_t stat_scaling; uint8_t stat_scaling_bytes[4]; }; /* 0x17f0 */
    union { int32_t speed_scaling; uint8_t speed_scaling_bytes[4]; }; /* 0x17f4 */
    union { int32_t misc_scaling; uint8_t misc_scaling_bytes[4]; }; /* 0x17f8 */
    uint8_t entity_flags[4];                 /* 0x17fc */
    union { int32_t sky_settings; uint8_t sky_settings_bytes[4]; }; /* 0x1800 */
    uint8_t global_weapon_enabled[50];       /* 0x1804 */
    uint8_t ship_taken[9];                   /* 0x1836 */
    uint8_t reserved_183f[0x23];
    uint8_t pause_key;                       /* 0x1862 */
    uint8_t camera_key;                      /* 0x1863 */
    uint8_t menu_keys[6];                    /* 0x1864 */
    uint8_t player_keys[4][8];               /* 0x186a */
    uint8_t reserved_188a[2];
    int32_t setup_toggle;                    /* 0x188c */
    int32_t setup_mode;                      /* 0x1890 */
    int32_t setup_counter;                   /* 0x1894 */
    int32_t setup_values[6];                 /* 0x1898 */
    int32_t setup_limits[6];                 /* 0x18b0 */
    uint16_t fade_color;                     /* 0x18c8 */
    uint8_t reserved_18ca[2];
    int32_t gravity_scale;                   /* 0x18cc */
    int32_t particle_gravity;                /* 0x18d0 */
    int32_t fire_rate_scale;                 /* 0x18d4 */
    int32_t starting_shield;                 /* 0x18d8 */
    uint8_t turret_density;                  /* 0x18dc */
    uint8_t troopers_enabled;                /* 0x18dd */
    int8_t runtime_team_mode;                /* 0x18de */
    uint8_t reserved_18df;
    uint16_t team_colors[4];                 /* 0x18e0 */
    uint32_t water_red;                      /* 0x18e8 */
    uint32_t water_green;                    /* 0x18ec */
    uint32_t water_blue;                     /* 0x18f0 */
    uint16_t water_color;                    /* 0x18f4 */
    uint16_t water_light_color;              /* 0x18f6 */
    uint16_t water_dark_color;               /* 0x18f8 */
    uint8_t reserved_18fa[2];
    float entity_density;                    /* 0x18fc */
    float inverse_entity_density;            /* 0x1900 */
    float weather_density;                   /* 0x1904 */
} GameConfigLayout;
#pragma pack(pop)

typedef union GameConfig {
    GameConfigLayout values;
    uint8_t bytes[GAME_CONFIG_SIZE];
} GameConfig;

static_assert(sizeof(GameConfigLayout) == GAME_CONFIG_SIZE, "GameConfig layout size");
static_assert(offsetof(GameConfigLayout, player_count) == 0x324, "player count offset");
static_assert(offsetof(GameConfigLayout, starting_weapon) == 0x1776, "starting weapon offset");
static_assert(offsetof(GameConfigLayout, music_enabled) == 0x17c6, "audio config offset");
static_assert(offsetof(GameConfigLayout, global_weapon_enabled) == 0x1804, "weapon flags offset");
static_assert(offsetof(GameConfigLayout, player_color) == 0x466, "player color offset");
static_assert(offsetof(GameConfigLayout, player_keys) == 0x186a, "player key offset");
static_assert(offsetof(GameConfigLayout, fade_color) == 0x18c8, "runtime config offset");
static_assert(offsetof(GameConfigLayout, weather_density) == 0x1904, "weather offset");

extern GameConfig g_GameConfig;
#define g_ConfigBlob (g_GameConfig.bytes)

/* Original globals were aliases into one contiguous record. Keeping these
 * compatibility names as aliases removes the old stale-copy synchronization. */
#define DAT_0048371e (g_GameConfig.values.music_enabled)
#define DAT_0048371f (g_GameConfig.values.sound_enabled)
#define DAT_00483720 (&g_GameConfig.values.music_volume)
#define DAT_00483724 (&g_GameConfig.values.display_flags)
#define DAT_00483729 (g_GameConfig.values.game_type)
#define DAT_0048372a (g_GameConfig.values.team_count)
#define DAT_0048372b (g_GameConfig.values.team_mode)
#define DAT_0048372c (g_GameConfig.values.ambient_particles)
#define DAT_0048372d (g_GameConfig.values.fog_mode)
#define DAT_0048372e (g_GameConfig.values.fog_ray_resolution)
#define DAT_0048372f (g_GameConfig.values.fog_detail)
#define DAT_00483730 (g_GameConfig.values.fog_wobble)
#define DAT_00483731 (g_GameConfig.values.sky_color_mode)
#define DAT_00483732 (g_GameConfig.values.saved_color_option)
#define DAT_00483734 (g_GameConfig.values.critter_spawns)
#define DAT_00483735 (g_GameConfig.values.team_base_placement)
#define DAT_00483736 (g_GameConfig.values.debris_difficulty)
#define DAT_00483737 (g_GameConfig.values.trooper_difficulty)
#define DAT_00483738 (g_GameConfig.values.game_mode)
#define DAT_00483739 (g_GameConfig.values.game_mode_preset)
#define DAT_0048373a (*reinterpret_cast<int16_t *>(&g_GameConfig.values.initial_lives))
#define DAT_0048373b (g_GameConfig.values.shared_lives)
#define DAT_0048373c (g_GameConfig.values.team_rules)
#define DAT_0048373d (g_GameConfig.values.friendly_fire)
#define DAT_0048373e (g_GameConfig.values.activation_guard)
#define DAT_0048373f (g_GameConfig.values.difficulty_secondary)
#define DAT_00483740 (g_GameConfig.values.round_time)
#define DAT_00483741 (g_GameConfig.values.difficulty_detail)
#define DAT_00483742 (g_GameConfig.values.shield_bar)
#define DAT_00483743 (g_GameConfig.values.radar)
#define DAT_00483744 (g_GameConfig.values.respawn_delay)
#define DAT_00483745 (g_GameConfig.values.detonation_mode)
#define DAT_00483746 (*reinterpret_cast<int16_t *>(&g_GameConfig.values.tick_rate))
#define DAT_00483747 (g_GameConfig.values.weapon_auto_release)
#define DAT_00483748 (g_GameConfig.values.stat_scaling)
#define DAT_0048374c (g_GameConfig.values.speed_scaling)
#define DAT_00483750 (g_GameConfig.values.misc_scaling)
#define DAT_00483754 (g_GameConfig.values.entity_flags)
#define DAT_00483758 (g_GameConfig.values.sky_settings)
#define DAT_0048378e (reinterpret_cast<char *>(g_GameConfig.values.ship_taken))
#define DAT_004837ba (g_GameConfig.values.pause_key)
#define DAT_004837bb (g_GameConfig.values.camera_key)
#define DAT_00483820 (g_GameConfig.values.fade_color)
#define DAT_00483824 (g_GameConfig.values.gravity_scale)
#define DAT_00483828 (g_GameConfig.values.particle_gravity)
#define DAT_0048382c (g_GameConfig.values.fire_rate_scale)
#define DAT_00483830 (g_GameConfig.values.starting_shield)
#define DAT_00483834 (g_GameConfig.values.turret_density)
#define DAT_00483835 (g_GameConfig.values.troopers_enabled)
#define DAT_00483836 (g_GameConfig.values.runtime_team_mode)
#define DAT_00483838 (g_GameConfig.values.team_colors)
#define DAT_00483840 (g_GameConfig.values.water_red)
#define DAT_00483844 (g_GameConfig.values.water_green)
#define DAT_00483848 (g_GameConfig.values.water_blue)
#define DAT_0048384c (g_GameConfig.values.water_color)
#define DAT_0048384e (g_GameConfig.values.water_light_color)
#define DAT_00483850 (g_GameConfig.values.water_dark_color)
#define DAT_00483854 (g_GameConfig.values.entity_density)
#define DAT_00483858 (g_GameConfig.values.inverse_entity_density)
#define DAT_0048385c (g_GameConfig.values.weather_density)

#endif
