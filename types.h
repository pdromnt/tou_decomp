#ifndef TOU_TYPES_H
#define TOU_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

/* ===== Structures ===== */

enum RuntimePoolCapacity {
    GAMEPLAY_PLAYER_CAPACITY = 64,
    PLAYER_STORAGE_CAPACITY = 80,
    ENTITY_ACTIVE_CAPACITY = 2500,
    ENTITY_STORAGE_CAPACITY = 2600,
    TROOPER_CAPACITY = 400,
    PROJECTILE_CAPACITY = 200,
    PARTICLE_CAPACITY = 2000,
    DEBRIS_ITEM_CAPACITY = 100
};

static_assert(GAMEPLAY_PLAYER_CAPACITY <= PLAYER_STORAGE_CAPACITY,
              "Selectable players must fit in the original player storage");

/*
 * Original 0x80-byte entity record.
 *
 * Evidence:
 * - Init_Memory_Pools stores a 0x51400-byte allocation in g_EntityPool
 *   (original global DAT_004892e8) at
 *   0x004203b5: 2600 physical records.
 * - FUN_00413720 indexes records with `SHL index, 7` at 0x00413a65 and writes
 *   the stable fields below through 0x00413c51.
 * - Weapon callbacks reuse several auxiliary fields with type-specific
 *   meanings. Those fields intentionally keep neutral names.
 *
 * Runtime code uses this layout directly where field width is verified.
 * Packed-width helpers remain only for original accesses that cross fields.
 */
typedef struct Entity {
    int32_t position_x;             /* 0x00 */
    int32_t previous_x;             /* 0x04 */
    int32_t position_y;             /* 0x08 */
    int32_t previous_y;             /* 0x0C */
    int32_t motion_x_10;            /* 0x10: role varies by entity type */
    int32_t motion_y_14;            /* 0x14: role varies by entity type */
    int32_t velocity_x;             /* 0x18 */
    int32_t velocity_y;             /* 0x1C */
    uint8_t state_20;               /* 0x20: flags/state/radius by type */
    uint8_t type;                   /* 0x21 */
    uint8_t owner;                  /* 0x22: player/team, 0xFF when absent */
    uint8_t unknown_23;             /* 0x23 */
    int16_t variant_24;             /* 0x24 */
    uint8_t auxiliary_26;           /* 0x26: cooldown/color/lifetime/guard */
    uint8_t unknown_27;             /* 0x27 */
    int32_t health_or_damage_28;    /* 0x28 */
    int32_t scratch_2c;             /* 0x2C */
    int32_t scratch_30;             /* 0x30 */
    uint32_t callback_address;      /* 0x34: original x86 function address */
    int32_t gravity_or_motion_38;   /* 0x38 */
    int32_t counter_3c;             /* 0x3C */
    uint8_t subtype;                /* 0x40 */
    uint8_t unknown_41[3];          /* 0x41 */
    int32_t damage_44;              /* 0x44 */
    int32_t scratch_48;             /* 0x48 */
    uint32_t palette_value;         /* 0x4C */
    int32_t scratch_50;             /* 0x50 */
    uint8_t animation_frame;        /* 0x54 */
    uint8_t unknown_55[3];          /* 0x55 */
    int32_t scratch_58;             /* 0x58: type-specific damage/force value */
    uint8_t timer_5c;               /* 0x5C */
    uint8_t unknown_5d[3];          /* 0x5D */
    int32_t scratch_60;             /* 0x60 */
    uint8_t scratch_64;             /* 0x64 */
    uint8_t scratch_65;             /* 0x65 */
    uint8_t unknown_66[0x1A];       /* 0x66..0x7F */
} Entity;

static_assert(sizeof(Entity) == 0x80, "Entity record must retain its original stride");
static_assert(std::is_trivially_copyable<Entity>::value,
              "Entities must remain safe for original whole-record copies");
static_assert(offsetof(Entity, position_x) == 0x00, "Entity::position_x offset");
static_assert(offsetof(Entity, position_y) == 0x08, "Entity::position_y offset");
static_assert(offsetof(Entity, velocity_x) == 0x18, "Entity::velocity_x offset");
static_assert(offsetof(Entity, state_20) == 0x20, "Entity::state_20 offset");
static_assert(offsetof(Entity, type) == 0x21, "Entity::type offset");
static_assert(offsetof(Entity, variant_24) == 0x24, "Entity::variant_24 offset");
static_assert(offsetof(Entity, health_or_damage_28) == 0x28, "Entity::health offset");
static_assert(offsetof(Entity, callback_address) == 0x34, "Entity::callback offset");
static_assert(offsetof(Entity, subtype) == 0x40, "Entity::subtype offset");
static_assert(offsetof(Entity, damage_44) == 0x44, "Entity::damage offset");
static_assert(offsetof(Entity, palette_value) == 0x4C, "Entity::palette offset");
static_assert(offsetof(Entity, animation_frame) == 0x54, "Entity::animation offset");
static_assert(offsetof(Entity, scratch_58) == 0x58, "Entity::scratch_58 offset");
static_assert(offsetof(Entity, timer_5c) == 0x5C, "Entity::timer offset");
static_assert(offsetof(Entity, scratch_60) == 0x60, "Entity::scratch_60 offset");
static_assert(offsetof(Entity, scratch_64) == 0x64, "Entity::scratch_64 offset");

/* Original 0x40-byte deployed projectile/building record. Several fields are
 * deliberately neutral because turrets, bases, and other deployed objects
 * reuse them with different meanings. */
typedef struct ProjectileRecord {
    int32_t position_x;              /* 0x00 */
    int32_t position_y;              /* 0x04 */
    int32_t visual_or_heading_08;    /* 0x08 */
    int32_t aim_angle_0c;            /* 0x0C */
    int32_t health_or_state_10;      /* 0x10 */
    int32_t limit_or_reload_14;      /* 0x14 */
    int32_t target_distance_18;      /* 0x18 */
    uint8_t type;                    /* 0x1C */
    uint8_t team;                    /* 0x1D */
    uint8_t palette_or_flags_1e;     /* 0x1E */
    uint8_t scratch_1f;
    uint8_t state_20;                /* 0x20 */
    uint8_t target_kind_21;          /* 0x21 */
    uint8_t sweep_counter_22;        /* 0x22 */
    uint8_t animation_counter_23;    /* 0x23 */
    uint8_t unknown_24[0x1c];
} ProjectileRecord;

static_assert(sizeof(ProjectileRecord) == 0x40, "Projectile record must retain its original stride");
static_assert(std::is_trivially_copyable<ProjectileRecord>::value,
              "Projectile records must remain safe for original whole-record copies");
static_assert(offsetof(ProjectileRecord, position_x) == 0x00, "ProjectileRecord::position_x offset");
static_assert(offsetof(ProjectileRecord, aim_angle_0c) == 0x0c, "ProjectileRecord::aim offset");
static_assert(offsetof(ProjectileRecord, health_or_state_10) == 0x10, "ProjectileRecord::health offset");
static_assert(offsetof(ProjectileRecord, type) == 0x1c, "ProjectileRecord::type offset");
static_assert(offsetof(ProjectileRecord, state_20) == 0x20, "ProjectileRecord::state offset");
static_assert(offsetof(ProjectileRecord, animation_counter_23) == 0x23, "ProjectileRecord::animation offset");

/* Original 0x40-byte infantry/car record. Movement, animation, and aiming
 * share this storage; neutral names preserve that reuse until each subtype is
 * independently documented. */
typedef struct TrooperRecord {
    int32_t position_x;              /* 0x00 */
    int32_t previous_x;              /* 0x04 */
    int32_t position_y;              /* 0x08 */
    int32_t previous_y;              /* 0x0C */
    int32_t velocity_x;              /* 0x10 */
    int32_t velocity_y;              /* 0x14 */
    int32_t movement_flags;          /* 0x18 */
    uint8_t team;                    /* 0x1C */
    int8_t patrol_direction;         /* 0x1D */
    uint8_t unknown_1e[2];
    int32_t movement_speed;          /* 0x20 */
    uint8_t animation_state_24;      /* 0x24 */
    uint8_t kind_25;                 /* 0x25 */
    uint8_t unknown_26[2];
    int32_t health_28;               /* 0x28 */
    uint8_t palette_2c;              /* 0x2C */
    uint8_t movement_mode_2d;        /* 0x2D */
    uint8_t unknown_2e[2];
    int32_t aim_angle_30;            /* 0x30 */
    int32_t aim_rate_34;             /* 0x34 */
    uint8_t fire_cooldown_38;        /* 0x38 */
    uint8_t bounce_cooldown_39;      /* 0x39 */
    uint8_t unknown_3a[6];
} TrooperRecord;

static_assert(sizeof(TrooperRecord) == 0x40, "Trooper record must retain its original stride");
static_assert(std::is_trivially_copyable<TrooperRecord>::value,
              "Trooper records must remain safe for original whole-record copies");
static_assert(offsetof(TrooperRecord, position_y) == 0x08, "TrooperRecord::position_y offset");
static_assert(offsetof(TrooperRecord, team) == 0x1c, "TrooperRecord::team offset");
static_assert(offsetof(TrooperRecord, health_28) == 0x28, "TrooperRecord::health offset");
static_assert(offsetof(TrooperRecord, palette_2c) == 0x2c, "TrooperRecord::palette offset");
static_assert(offsetof(TrooperRecord, aim_angle_30) == 0x30, "TrooperRecord::aim offset");
static_assert(offsetof(TrooperRecord, fire_cooldown_38) == 0x38, "TrooperRecord::cooldown offset");

/* Animated explosion/fire particle pool (original global DAT_00481f34). */
typedef struct ParticleRecord {
    int32_t position_x;              /* 0x00 */
    int32_t position_y;              /* 0x04 */
    int32_t velocity_x;              /* 0x08 */
    int32_t velocity_y;              /* 0x0C */
    uint8_t sprite_index;            /* 0x10 */
    uint8_t frame_number;            /* 0x11 */
    uint8_t frame_timer;             /* 0x12 */
    uint8_t flags_13;                /* 0x13 */
    uint8_t owner_or_flags_14;       /* 0x14 */
    uint8_t color_index;             /* 0x15 */
    uint8_t unknown_16[0x0a];
} ParticleRecord;

static_assert(sizeof(ParticleRecord) == 0x20, "Particle record must retain its original stride");
static_assert(std::is_trivially_copyable<ParticleRecord>::value,
              "Particle records must remain safe for original whole-record copies");
static_assert(offsetof(ParticleRecord, velocity_x) == 0x08, "ParticleRecord::velocity offset");
static_assert(offsetof(ParticleRecord, sprite_index) == 0x10, "ParticleRecord::sprite offset");
static_assert(offsetof(ParticleRecord, frame_timer) == 0x12, "ParticleRecord::timer offset");
static_assert(offsetof(ParticleRecord, owner_or_flags_14) == 0x14, "ParticleRecord::owner offset");
static_assert(offsetof(ParticleRecord, color_index) == 0x15, "ParticleRecord::color offset");

/* Menu/gameplay debris and pickup pool (original global DAT_00487830). */
typedef struct DebrisItemRecord {
    int32_t position_x;              /* 0x00 */
    int32_t position_y;              /* 0x04 */
    uint8_t sprite_frame;            /* 0x08 */
    uint8_t motion_variant;          /* 0x09 */
    uint8_t type;                    /* 0x0A */
    uint8_t scratch_0b;
    int32_t lifetime_0c;             /* 0x0C */
    uint8_t unknown_10[0x10];
} DebrisItemRecord;

static_assert(sizeof(DebrisItemRecord) == 0x20, "Debris item record must retain its original stride");
static_assert(std::is_trivially_copyable<DebrisItemRecord>::value,
              "Debris records must remain safe for original whole-record copies");
static_assert(offsetof(DebrisItemRecord, sprite_frame) == 0x08, "DebrisItemRecord::frame offset");
static_assert(offsetof(DebrisItemRecord, type) == 0x0a, "DebrisItemRecord::type offset");
static_assert(offsetof(DebrisItemRecord, lifetime_0c) == 0x0c, "DebrisItemRecord::lifetime offset");

/*
 * Original 0x598-byte player/ship runtime record.
 *
 * Only fields whose width and offset are independently established are named.
 * The remaining ranges stay opaque until their owning routines are lifted.
 */
typedef struct PlayerData {
    int32_t position_x;              /* 0x000 */
    int32_t position_y;              /* 0x004 */
    int32_t previous_x;              /* 0x008 */
    int32_t previous_y;              /* 0x00C */
    int32_t velocity_x;              /* 0x010 */
    int32_t velocity_y;              /* 0x014 */
    int32_t heading;                 /* 0x018 */
    uint8_t exhaust_counter;         /* 0x01C */
    uint8_t unknown_01d[3];
    int32_t health;                  /* 0x020 */
    uint8_t state_24;                /* 0x024 */
    uint8_t scratch_25;              /* 0x025 */
    uint8_t scratch_26;              /* 0x026 */
    uint8_t scratch_27;              /* 0x027 */
    int32_t lives;                   /* 0x028 */
    uint8_t team;                    /* 0x02C */
    uint8_t unknown_02d[3];
    int32_t timer_30;                /* 0x030 */
    uint8_t weapon_type;             /* 0x034 */
    uint8_t weapon_mark;             /* 0x035 */
    uint8_t unknown_036[2];
    int32_t highest_weapon_slot;     /* 0x038: maximum valid index */
    uint8_t weapon_slots[0x50];      /* 0x03C..0x08B */
    uint8_t primary_weapon_level;    /* 0x08C */
    uint8_t secondary_weapon_level;  /* 0x08D */
    uint8_t unknown_08e[2];
    int32_t timer_90;                /* 0x090 */
    int32_t timer_94;                /* 0x094 */
    int32_t shield_value;            /* 0x098 */
    uint8_t primary_fire_interval;   /* 0x09C */
    uint8_t scratch_09d;
    uint8_t weapon_select_active;    /* 0x09E */
    uint8_t scratch_09f;
    uint8_t timer_a0;                /* 0x0A0 */
    uint8_t flag_a1;                 /* 0x0A1 */
    uint8_t timer_a2;                /* 0x0A2 */
    uint8_t flag_a3;                 /* 0x0A3 */
    int32_t timer_a4;                /* 0x0A4 */
    int32_t timer_a8;                /* 0x0A8 */
    uint8_t key_scan_codes[7];       /* 0x0AC..0x0B2 */
    uint8_t unknown_0b3;
    uint32_t scratch_b4;             /* 0x0B4 */
    uint32_t buttons;                /* 0x0B8 */
    uint32_t previous_buttons;       /* 0x0BC */
    uint32_t scratch_c0;             /* 0x0C0 */
    uint8_t timer_c4;                /* 0x0C4 */
    uint8_t timer_c5;                /* 0x0C5 */
    uint8_t stun_timer;              /* 0x0C6 */
    uint8_t scratch_c7;              /* 0x0C7 */
    uint8_t timer_c8;                /* 0x0C8 */
    uint8_t hud_banner_timer;        /* 0x0C9 */
    uint8_t hud_banner_id;           /* 0x0CA */
    uint8_t timer_cb;                /* 0x0CB */
    uint8_t timer_cc;                /* 0x0CC */
    uint8_t unknown_0cd[3];
    int32_t timer_d0;                /* 0x0D0 */
    int32_t boost_timer;             /* 0x0D4 */
    int32_t timer_d8;                /* 0x0D8 */
    uint8_t timer_dc;                /* 0x0DC */
    uint8_t ai_level;                /* 0x0DD: zero for human control */
    uint8_t unknown_0de[0x386];      /* 0x0DE..0x463 */
    int32_t counter_464;
    int32_t counter_468;
    int32_t counter_46c;
    int32_t counter_470;
    int32_t counter_474;
    int32_t counter_478;
    uint8_t active_47c;
    uint8_t scratch_47d;
    uint8_t timer_47e;
    uint8_t scratch_47f;
    uint8_t human_controlled;        /* 0x480 */
    uint8_t unknown_481[3];
    int32_t viewport_width;          /* 0x484 */
    int32_t viewport_height;         /* 0x488 */
    int32_t viewport_x;              /* 0x48C */
    int32_t viewport_y;              /* 0x490 */
    int32_t frag_count;             /* 0x494: signed frags (team/self kills subtract) */
    int32_t death_count;            /* 0x498 */
    uint8_t scratch_49c;
    uint8_t scratch_49d;
    uint8_t timer_49e;
    uint8_t scratch_49f;
    uint8_t visibility_state;        /* 0x4A0 */
    uint8_t last_attacker;           /* 0x4A1 */
    uint8_t timer_4a2;
    uint8_t timer_4a3;
    uint8_t unknown_4a4[4];
    int32_t sound_timer;             /* 0x4A8 */
    int32_t sound_channel;           /* 0x4AC */
    int32_t sound_id;                /* 0x4B0 */
    uint8_t unknown_4b4[0xE4];
} PlayerData;

static_assert(sizeof(PlayerData) == 0x598, "Player record must retain its original stride");
static_assert(std::is_trivially_copyable<PlayerData>::value,
              "Player records must remain safe for original whole-record copies");
static_assert(offsetof(PlayerData, position_x) == 0x00, "PlayerData::position_x offset");
static_assert(offsetof(PlayerData, velocity_x) == 0x10, "PlayerData::velocity_x offset");
static_assert(offsetof(PlayerData, health) == 0x20, "PlayerData::health offset");
static_assert(offsetof(PlayerData, team) == 0x2C, "PlayerData::team offset");
static_assert(offsetof(PlayerData, weapon_type) == 0x34, "PlayerData::weapon_type offset");
static_assert(offsetof(PlayerData, weapon_slots) == 0x3C, "PlayerData::weapon_slots offset");
static_assert(offsetof(PlayerData, primary_weapon_level) == 0x8C, "PlayerData::primary_weapon_level offset");
static_assert(offsetof(PlayerData, shield_value) == 0x98, "PlayerData::shield_value offset");
static_assert(offsetof(PlayerData, scratch_c0) == 0xC0, "PlayerData::scratch_c0 offset");
static_assert(offsetof(PlayerData, buttons) == 0xB8, "PlayerData::buttons offset");
static_assert(offsetof(PlayerData, ai_level) == 0xDD, "PlayerData::ai_level offset");
static_assert(offsetof(PlayerData, viewport_width) == 0x484, "PlayerData::viewport_width offset");
static_assert(offsetof(PlayerData, viewport_x) == 0x48C, "PlayerData::viewport_x offset");
static_assert(offsetof(PlayerData, frag_count) == 0x494, "PlayerData::frag_count offset");
static_assert(offsetof(PlayerData, death_count) == 0x498, "PlayerData::death_count offset");
static_assert(offsetof(PlayerData, sound_timer) == 0x4A8, "PlayerData::sound_timer offset");

typedef struct {
    unsigned int handle;    /* FSOUND_SAMPLE* cast to uint */
    unsigned char volume;
    unsigned char padding[3];
} SoundEntry;

typedef struct {
    int pixel_offset;
    int width;
    int height;
    int unknown;
} FontChar;

typedef struct {
    int x;              /* 0x00 */
    int y;              /* 0x04 */
    int width;          /* 0x08 */
    int height;         /* 0x0C */
    int type;           /* 0x10: 0=text, 1=sprite */
    int color_style;    /* 0x14: color/style for rendering */
    char font_idx;      /* 0x18 */
    char _pad1[3];
    int string_idx;     /* 0x1C: index into g_MenuStrings */
    int hover_state;    /* 0x20: hover/scroll animation */
    unsigned char flag1;       /* 0x24 */
    unsigned char clickable;   /* 0x25 */
    unsigned char nav_target;  /* 0x26: target page (0xFF=none) */
    unsigned char _pad2;
    int linked_item;    /* 0x28 */
    unsigned char render_mode; /* 0x2C */
    unsigned char _pad3[3];
    int extra_data;     /* 0x30 */
} MenuItem; /* 0x34 = 52 bytes, max 350 items in g_GameViewData */

#endif /* TOU_TYPES_H */
