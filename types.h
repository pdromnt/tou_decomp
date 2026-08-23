#ifndef TOU_TYPES_H
#define TOU_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* ===== Structures ===== */

/*
 * Original 0x80-byte entity record.
 *
 * Evidence:
 * - Init_Memory_Pools stores a 0x51400-byte allocation in DAT_004892e8 at
 *   0x004203b5: 2600 physical records.
 * - FUN_00413720 indexes records with `SHL index, 7` at 0x00413a65 and writes
 *   the stable fields below through 0x00413c51.
 * - Weapon callbacks reuse several auxiliary fields with type-specific
 *   meanings. Those fields intentionally keep neutral names.
 *
 * This type documents layout only for now. Runtime code continues using the
 * verified raw-access paths until each migration can be checked separately.
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
    uint8_t unknown_55[7];          /* 0x55 */
    uint8_t timer_5c;               /* 0x5C */
    uint8_t unknown_5d[3];          /* 0x5D */
    int32_t scratch_60;             /* 0x60 */
    uint8_t scratch_64;             /* 0x64 */
    uint8_t scratch_65;             /* 0x65 */
    uint8_t unknown_66[0x1A];       /* 0x66..0x7F */
} Entity;

static_assert(sizeof(Entity) == 0x80, "Entity record must retain its original stride");
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
static_assert(offsetof(Entity, timer_5c) == 0x5C, "Entity::timer offset");
static_assert(offsetof(Entity, scratch_60) == 0x60, "Entity::scratch_60 offset");
static_assert(offsetof(Entity, scratch_64) == 0x64, "Entity::scratch_64 offset");

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
