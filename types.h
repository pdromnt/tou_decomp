#ifndef TOU_TYPES_H
#define TOU_TYPES_H

/* ===== Structures ===== */

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
