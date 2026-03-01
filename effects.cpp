/*
 * effects.cpp - Color LUTs, explosion sprites, particle rendering
 * Addresses: FUN_0045a060=0045A060, FUN_0045b2a0=0045B2A0,
 *            FUN_00422fc0=00422FC0, FUN_0040d100=0040D100,
 *            FUN_004076d0=004076D0, FUN_004257e0=004257E0
 */
#include "tou.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ===== Viewport globals (set by FUN_004076d0) ===== */
int DAT_004806dc = 0;  /* viewport left */
int DAT_004806d0 = 0;  /* viewport right */
int DAT_004806e0 = 0;  /* viewport top */
int DAT_004806d4 = 0;  /* viewport bottom */
int DAT_004806d8 = 0;  /* viewport width */
int DAT_004806e4 = 0;  /* viewport height */
int DAT_004806e8 = 0;  /* camera/scroll Y offset */
int DAT_004806ec = 0;  /* camera/scroll X offset */

/* ===== Entity rendering counts (all zero until entity spawning is implemented) ===== */
int DAT_00489274 = 0;     /* static entity count (turrets) */
int DAT_0048924c = 0;     /* dynamic entity/trooper count */
int DAT_00489260 = 0;     /* projectile count */
int DAT_0048926c = 0;     /* explosion count */
int DAT_00489264 = 0;     /* misc effect count */
int DAT_00489268 = 0;     /* debris/particle count */
int DAT_00487808 = 0;     /* active player viewport count */
unsigned short DAT_0048384e = 0;  /* laser pixel color A */
unsigned short DAT_00483850 = 0;  /* laser pixel color B */

/* ===== FUN_004257e0 - Angle calculation (atan2 to table index) ===== */
/* Returns 11-bit angle index (0-2047) into sin/cos table.
 * Original uses x87 FPATAN(ST1=dx, ST0=dy) which is atan2(dx, dy),
 * then multiplies by 1024.0 * (1/PI) to convert radians to table index. */
int FUN_004257e0(int cx, int cy, int px, int py)
{
    double dx = (double)(px - cx);
    double dy = (double)(py - cy);
    /* FPATAN(ST1=dx, ST0=dy) = atan2(dx, dy) - angle from Y axis */
    double angle = atan2(dx, dy);
    int idx = (int)(angle * 1024.0 / 3.14159265358979323846);
    return idx & 0x7FF;
}

/* ===== Helper: clamp int to [0, 255] ===== */
static inline int clamp255(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/* ===== Helper: pack 8-bit RGB to RGB565 ===== */
static inline unsigned short pack_rgb565(int r, int g, int b)
{
    return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* ===== Helper: fill one 4096-entry LUT from a per-pixel transform ===== */
typedef void (*lut_transform_fn)(int src_r, int src_g, int src_b,
                                  int *out_r, int *out_g, int *out_b, int param);

static void fill_lut(unsigned short *lut, lut_transform_fn fn, int param)
{
    for (int r4 = 0; r4 < 16; r4++) {
        int r8 = (r4 * 255) / 15;
        for (int g4 = 0; g4 < 16; g4++) {
            int g8 = (g4 * 255) / 15;
            for (int b4 = 0; b4 < 16; b4++) {
                int b8 = (b4 * 255) / 15;
                int or8, og8, ob8;
                fn(r8, g8, b8, &or8, &og8, &ob8, param);
                int idx = b4 + (g4 + r4 * 16) * 16;
                lut[idx] = pack_rgb565(clamp255(or8), clamp255(og8), clamp255(ob8));
            }
        }
    }
}

/* ---- Per-table transform functions ---- */

/* [0..3] Fog-to-white: (ch*4+255)/5 applied N times */
static void xform_fog_white(int r, int g, int b, int *or_, int *og, int *ob, int n)
{
    for (int i = 0; i < n; i++) {
        r = (r * 4 + 255) / 5;
        g = (g * 4 + 255) / 5;
        b = (b * 4 + 255) / 5;
    }
    *or_ = r; *og = g; *ob = b;
}

/* [4..11] Brightness dimming: ch = ch * level / 11 */
static void xform_brightness(int r, int g, int b, int *or_, int *og, int *ob, int level)
{
    *or_ = (r * level) / 11;
    *og = (g * level) / 11;
    *ob = (b * level) / 11;
}

/* [12..13] Cold/green shift: R-=0x30*n, G+=0x30*n, B-=0x30*n */
static void xform_cold(int r, int g, int b, int *or_, int *og, int *ob, int n)
{
    *or_ = r - 0x30 * n;
    *og = g + 0x30 * n;
    *ob = b - 0x30 * n;
}

/* [14..15] Warm/yellow shift: R+=0x30*n, G+=0x30*n, B-=0x30*n */
static void xform_warm(int r, int g, int b, int *or_, int *og, int *ob, int n)
{
    *or_ = r + 0x30 * n;
    *og = g + 0x30 * n;
    *ob = b - 0x30 * n;
}

/* [16..17] Red shift: R+=0x30*n, G-=0x30*n, B-=0x30*n */
static void xform_red(int r, int g, int b, int *or_, int *og, int *ob, int n)
{
    *or_ = r + 0x30 * n;
    *og = g - 0x30 * n;
    *ob = b - 0x30 * n;
}

/* [18..21] Fog-to-gray: ch + ((0x50 - ch) * level) / 7 */
static void xform_fog_gray(int r, int g, int b, int *or_, int *og, int *ob, int level)
{
    *or_ = r + ((0x50 - r) * level) / 7;
    *og = g + ((0x50 - g) * level) / 7;
    *ob = b + ((0x50 - b) * level) / 7;
}

/* [22] Shadow: subtractive darkening */
static void xform_shadow(int r, int g, int b, int *or_, int *og, int *ob, int /*unused*/)
{
    *or_ = 0x19 - r;
    *og = 0x1E - g;
    *ob = 0x23 - b;
}

/* [23] Contrast pinch: pull R toward 0x8C, G/B toward 0xDC with +-0x28 */
static void xform_contrast(int r, int g, int b, int *or_, int *og, int *ob, int /*unused*/)
{
    /* R: pull toward 0x8C */
    if (r > 0x8C) { r -= 0x28; if (r < 0x8C) r = 0x8C; }
    else          { r += 0x28; if (r > 0x8C) r = 0x8C; }
    /* G: pull toward 0xDC */
    if (g > 0xDC) { g -= 0x28; if (g < 0xDC) g = 0xDC; }
    else          { g += 0x28; if (g > 0xDC) g = 0xDC; }
    /* B: pull toward 0xDC */
    if (b > 0xDC) { b -= 0x28; if (b < 0xDC) b = 0xDC; }
    else          { b += 0x28; if (b > 0xDC) b = 0xDC; }
    *or_ = r; *og = g; *ob = b;
}

/* [24..27] Highlight: R pulled to 0xA0 by +-0xC, G/B reduced by 0x10*n */
static void xform_highlight(int r, int g, int b, int *or_, int *og, int *ob, int n)
{
    int og_v = g - 0x10 * n;
    int ob_v = b - 0x10 * n;
    for (int i = 0; i < n; i++) {
        if (r < 0xA0) r += 0x0C;
        else          r -= 0x0C;
    }
    *or_ = r; *og = og_v; *ob = ob_v;
}

/* ===== FUN_0045a060 - Color LUT generation (0045A060) ===== */
/*
 * Generates the master color remap table (DAT_00489230) and all color
 * transformation LUTs (DAT_004876a4[0..47+]). Each LUT is 4096 entries
 * of RGB565 pixels, indexed by a 12-bit R4:G4:B4 value.
 *
 * Table layout in DAT_004876a4[]:
 *   [0..3]   Fog-to-white (4 tables)
 *   [4..11]  Brightness dimming (8 tables, level 10 down to 3)
 *   [12..13] Cold/green shift (2 tables)
 *   [14..15] Warm/yellow shift (2 tables)
 *   [16..17] Red shift (2 tables)
 *   [18..21] Fog-to-gray (4 tables)
 *   [22]     Shadow (subtractive)
 *   [23]     Contrast pinch
 *   [24..27] Highlight (4 tables)
 *   [32..47] Blend-to-background (16 tables, via FUN_0045adc0)
 */
void FUN_0045a060(void)
{
    /* ---- Section 1: RGB565 -> 12-bit remap table (DAT_00489230) ---- */
    /* Maps every possible 16-bit pixel to a 12-bit index (R4:G4:B4).
     * Original iterates 32x32x32 (5-bit per channel); we fill all 64K. */
    unsigned short *remap = (unsigned short *)DAT_00489230;
    for (int pixel = 0; pixel < 65536; pixel++) {
        int r5 = (pixel >> 11) & 0x1F;
        int g6 = (pixel >> 5)  & 0x3F;
        int b5 = pixel & 0x1F;
        int r4 = (r5 * 15) / 31;
        int g4 = (g6 * 15) / 63;
        int b4 = (b5 * 15) / 31;
        remap[pixel] = (unsigned short)(b4 + (g4 + r4 * 16) * 16);
    }

    /* ---- Section 2: Fog-to-white [0..3] ---- */
    for (int i = 0; i < 4; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[i];
        if (lut) fill_lut(lut, xform_fog_white, i + 1);
    }

    /* ---- Section 3: Brightness dimming [4..11] ---- */
    /* 8 tables: level counts down from 10 to 3 */
    {
        int level = 10;
        for (int i = 4; i < 12; i++, level--) {
            unsigned short *lut = (unsigned short *)DAT_004876a4[i];
            if (lut) fill_lut(lut, xform_brightness, level);
        }
    }

    /* ---- Section 4: Cold/green shift [12..13] ---- */
    for (int i = 0; i < 2; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[12 + i];
        if (lut) fill_lut(lut, xform_cold, i + 1);
    }

    /* ---- Section 5: Warm/yellow shift [14..15] ---- */
    for (int i = 0; i < 2; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[14 + i];
        if (lut) fill_lut(lut, xform_warm, i + 1);
    }

    /* ---- Section 6: Red shift [16..17] ---- */
    for (int i = 0; i < 2; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[16 + i];
        if (lut) fill_lut(lut, xform_red, i + 1);
    }

    /* ---- Section 7: Fog-to-gray [18..21] ---- */
    for (int i = 0; i < 4; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[18 + i];
        if (lut) fill_lut(lut, xform_fog_gray, i + 1);
    }

    /* ---- Section 8: Shadow [22] ---- */
    {
        unsigned short *lut = (unsigned short *)DAT_004876a4[22];
        if (lut) fill_lut(lut, xform_shadow, 0);
    }

    /* ---- Section 9: Contrast pinch [23] ---- */
    {
        unsigned short *lut = (unsigned short *)DAT_004876a4[23];
        if (lut) fill_lut(lut, xform_contrast, 0);
    }

    /* ---- Section 10: Highlight [24..27] ---- */
    for (int i = 0; i < 4; i++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[24 + i];
        if (lut) fill_lut(lut, xform_highlight, i + 1);
    }

    /* ---- Section 11: Blend-to-background [32..47] via FUN_0045adc0 ---- */
    FUN_0045adc0();
}

/* ===== FUN_0045adc0 - Blend-to-background LUT generation (0045ADC0) ===== */
/*
 * Generates 16 tables at DAT_004876a4[32..47] that linearly blend
 * each source pixel toward a target background color (DAT_00483820).
 * Step 1/17 through 16/17.
 */
void FUN_0045adc0(void)
{
    /* Decode target color from DAT_00483820 (RGB565) */
    unsigned short bg = DAT_00483820;
    int tgt_r = ((bg >> 11) & 0x1F) << 3;
    int tgt_g = ((bg >> 5)  & 0x3F) << 2;
    int tgt_b = (bg & 0x1F) << 3;

    for (int step = 1; step <= 16; step++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[32 + step - 1];
        if (!lut) continue;

        for (int r4 = 0; r4 < 16; r4++) {
            int r8 = (r4 * 255) / 15;
            for (int g4 = 0; g4 < 16; g4++) {
                int g8 = (g4 * 255) / 15;
                for (int b4 = 0; b4 < 16; b4++) {
                    int b8 = (b4 * 255) / 15;
                    int or8 = r8 + ((tgt_r - r8) * step) / 17;
                    int og8 = g8 + ((tgt_g - g8) * step) / 17;
                    int ob8 = b8 + ((tgt_b - b8) * step) / 17;
                    int idx = b4 + (g4 + r4 * 16) * 16;
                    lut[idx] = pack_rgb565(clamp255(or8), clamp255(og8), clamp255(ob8));
                }
            }
        }
    }
}

/* ===== FUN_0045b2a0 - Blend LUT generation (0045B2A0) ===== */
/*
 * Generates DAT_0048792c blend tables for explosion particle rendering.
 * 3 color groups x 16 blend levels = 48 tables.
 * Each table: 4096 entries mapping 12-bit source brightness -> RGB565 blended color.
 *
 * Group 0: Fire (red -> orange -> yellow -> white)
 * Group 1: Warm fire variant
 * Group 2: Cooler tones
 */
void FUN_0045b2a0(void)
{
    /* Color targets per group per level: [R, G, B]
     * Extracted from local_bb in Ghidra decompilation */
    static const unsigned char fire_colors[3][15][3] = {
        /* Group 0: Fire - red to white */
        {
            {0xFF,0x00,0x00}, {0xFF,0x00,0x00}, {0xFF,0x00,0x00}, {0xFF,0x00,0x00},
            {0xFF,0x10,0x00}, {0xFF,0x20,0x00}, {0xFF,0x30,0x00}, {0xFF,0x50,0x00},
            {0xFF,0x82,0x00}, {0xFF,0xB4,0x20}, {0xFF,0xE6,0x40}, {0xFF,0xFF,0x60},
            {0xFF,0xFF,0x80}, {0xFF,0xFF,0xA0}, {0xFF,0xFF,0xC8},
        },
        /* Group 1: Warm fire variant */
        {
            {0xFF,0x00,0x00}, {0xFF,0x00,0x00}, {0xFF,0x00,0x00}, {0xFF,0x00,0x00},
            {0xFF,0x1E,0x00}, {0xFF,0x3C,0x00}, {0xFF,0x64,0x00}, {0xFF,0x96,0x00},
            {0xFF,0xBE,0x00}, {0xFF,0xDC,0x28}, {0xFF,0xFF,0x32}, {0xFF,0xFF,0x64},
            {0xFF,0xFF,0x96}, {0xFF,0xFF,0xC8}, {0xFF,0xFF,0xFF},
        },
        /* Group 2: Blue/ice (R,G low, B=0xFF dominant) */
        {
            {0x28,0x28,0xFF}, {0x28,0x28,0xFF}, {0x28,0x28,0xFF}, {0x28,0x28,0xFF},
            {0x3C,0x3C,0xFF}, {0x50,0x50,0xFF}, {0x64,0x64,0xFF}, {0x64,0x64,0xFF},
            {0x78,0x78,0xFF}, {0x8C,0x8C,0xFF}, {0xA0,0xA0,0xFF}, {0xB4,0xB4,0xFF},
            {0xC8,0xC8,0xFF}, {0xDC,0xDC,0xFF}, {0xFF,0xFF,0xFF},
        },
    };

    /* Blend strength per group per level (from local_104 in Ghidra) */
    static const unsigned char blend_str[3][15] = {
        { 2, 5, 8,10,11,13,15,18,24,30,35,39,43,47,52},
        { 5, 8,10,12,15,18,24,30,35,42,47,52,57,63,63},
        { 4, 7, 9,11,13,17,22,27,32,37,42,47,52,57,60},
    };

    for (int group = 0; group < 3; group++) {
        for (int level = 0; level < 15; level++) {
            int lut_idx = group * 16 + level;
            if (lut_idx >= 48) break;

            unsigned short *lut = (unsigned short *)DAT_0048792c[lut_idx];
            if (!lut) continue;

            int tgt_r = fire_colors[group][level][0];
            int tgt_g = fire_colors[group][level][1];
            int tgt_b = fire_colors[group][level][2];
            int alpha = blend_str[group][level];

            /* Fill 4096 entries: 12-bit source (R4:G4:B4) -> RGB565 blend */
            for (int src_idx = 0; src_idx < 4096; src_idx++) {
                int sb = src_idx & 0xF;
                int sg = (src_idx >> 4) & 0xF;
                int sr = (src_idx >> 8) & 0xF;

                /* Scale 4-bit to 8-bit */
                int r = (sr * 255) / 15;
                int g = (sg * 255) / 15;
                int b = (sb * 255) / 15;

                /* Alpha blend: result = src + (target - src) * alpha / 63 */
                r = r + ((tgt_r - r) * alpha) / 63;
                g = g + ((tgt_g - g) * alpha) / 63;
                b = b + ((tgt_b - b) * alpha) / 63;

                if (r < 0) r = 0; if (r > 255) r = 255;
                if (g < 0) g = 0; if (g > 255) g = 255;
                if (b < 0) b = 0; if (b > 255) b = 255;

                /* Convert to RGB565 */
                lut[src_idx] = (unsigned short)(
                    ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                );
            }
        }
    }
}

/* ===== FUN_00422fc0 - Load explode.gfx (00422FC0) ===== */
/*
 * Loads explosion sprite atlas from "data\explode.gfx".
 * Format: 20 entries, each with 3-byte header [width, height, num_frames]
 * followed by width*height*num_frames bytes of pixel data.
 * Pixel transform: 0 -> 0xFF (transparent), else value - 1.
 *
 * Descriptors stored in DAT_00481f20 (8 bytes per entry, 20 entries = 160 bytes):
 *   +0: int pixel_offset  (into DAT_0048787c)
 *   +4: byte width
 *   +5: byte height
 *   +6: byte num_frames
 *   +7: padding
 *
 * Pixel data stored in DAT_0048787c (430000 bytes allocated).
 */
int FUN_00422fc0(void)
{
    FILE *f;
    unsigned char header[3];
    int pixel_offset = 0;

    f = fopen("data\\explode.gfx", "rb");
    if (!f) {
        LOG("[FX] Failed to open data\\explode.gfx\n");
        return 0;
    }

    for (int i = 0; i < 20; i++) {
        int desc_base = i * 8;

        /* Read 3-byte header */
        if (fread(header, 1, 3, f) != 3) {
            fclose(f);
            return 0;
        }

        /* Store descriptor */
        unsigned char *desc = (unsigned char *)DAT_00481f20 + desc_base;
        *(int *)desc = pixel_offset;         /* +0: pixel_offset */
        desc[4] = header[0];                 /* +4: width */
        desc[5] = header[1];                 /* +5: height */
        desc[6] = header[2];                 /* +6: num_frames */

        /* Read pixel data */
        int frame_size = (int)header[0] * (int)header[1];
        int total_pixels = frame_size * (int)header[2];

        unsigned char *pixels = (unsigned char *)DAT_0048787c + pixel_offset;
        if (fread(pixels, 1, total_pixels, f) != (size_t)total_pixels) {
            fclose(f);
            return 0;
        }

        /* Transform pixels: 0 -> 0xFF (transparent), else value - 1 */
        for (int j = 0; j < total_pixels; j++) {
            if (pixels[j] == 0) {
                pixels[j] = 0xFF;
            } else {
                pixels[j] = pixels[j] - 1;
            }
        }

        pixel_offset += total_pixels;
    }

    fclose(f);
    LOG("[FX] Loaded explode.gfx: %d bytes of pixel data\n", pixel_offset);
    return 1;
}

/* ===== FUN_0040d100 - Particle renderer (0040D100) ===== */
/*
 * Renders explosion particles onto the given RGB565 buffer.
 * Iterates all active particles (DAT_00489250 count, DAT_00481f34 array).
 * Uses table-based alpha blending via DAT_0048792c and DAT_00489230.
 *
 * Particle structure (32 bytes at DAT_00481f34 + i*0x20):
 *   +0x00: int x         (18-bit fixed point, >> 18 for pixels)
 *   +0x04: int y         (18-bit fixed point)
 *   +0x08: int dx        (velocity x)
 *   +0x0C: int dy        (velocity y)
 *   +0x10: byte sprite_index  (which explode.gfx entry)
 *   +0x11: byte frame_number  (current animation frame)
 *   +0x12: byte frame_timer
 *   +0x13: byte unused
 *   +0x14: byte unused
 *   +0x15: byte color_index   (blend LUT group)
 */
void FUN_0040d100(int buffer, int stride)
{
    if (DAT_00489250 <= 0) return;

    int i = 0;
    int offset = 0;

    while (i < DAT_00489250) {
        unsigned char *part = (unsigned char *)DAT_00481f34 + offset;
        int *pi = (int *)part;

        unsigned char sprite_idx = part[0x10];
        unsigned char *desc = (unsigned char *)DAT_00481f20 + (int)sprite_idx * 8;

        int spr_w = (int)desc[4];
        int spr_h = (int)desc[5];
        int half_w = spr_w >> 1;
        int half_h = spr_h >> 1;

        /* Get pixel position from fixed-point */
        int px = pi[0] >> 18;
        int py = pi[1] >> 18;

        /* Viewport culling */
        if ((px + half_w) <= DAT_004806dc || (px - half_w) >= DAT_004806d0) {
            i++; offset += 0x20; continue;
        }
        if ((py + half_h) <= DAT_004806e0 || (py - half_h) >= DAT_004806d4) {
            i++; offset += 0x20; continue;
        }
        if (sprite_idx == 100) {
            i++; offset += 0x20; continue;
        }

        /* Position relative to viewport */
        int rx = (px - half_w) - DAT_004806dc;
        int ry = (py - half_h) - DAT_004806e0;

        /* Compute source pixel offset into explode pixel data */
        int spr_pixel_base = *(int *)desc;  /* pixel_offset from descriptor */
        int frame_num = part[0x11];
        int src_offset = spr_pixel_base + frame_num * spr_h * spr_w;

        /* Destination pointer */
        unsigned short *dst = (unsigned short *)
            (buffer + ((DAT_004806e8 + ry) * stride + DAT_004806ec + rx) * 2);

        int draw_w = spr_w;
        int draw_h = spr_h;
        int src_skip = 0;
        int dst_skip = stride - spr_w;

        /* Clip top */
        if (ry < 0) {
            src_offset += spr_w * (-ry);
            dst += (-ry) * stride;
            draw_h += ry;
        } else if ((int)(spr_h + ry) > DAT_004806e4) {
            draw_h = DAT_004806e4 - ry;
        }

        /* Clip left */
        if (rx < 0) {
            src_offset += (-rx);
            src_skip = -rx;
            dst_skip = (stride - spr_w) - rx;
            draw_w += rx;
            dst += (-rx);
        } else if ((int)(spr_w + rx) > DAT_004806d8) {
            src_skip = (spr_w - DAT_004806d8) + rx;
            dst_skip = (src_skip - spr_w) + stride;
            draw_w = spr_w - src_skip;
        } else {
            dst_skip = stride - spr_w;
            src_skip = 0;
        }

        unsigned char color_idx = part[0x15];

        /* Render pixels with alpha blending */
        if (draw_h > 0) {
            for (int row = 0; row < draw_h; row++) {
                if (draw_w > 0) {
                    for (int col = 0; col < draw_w; col++) {
                        unsigned char px_val = *((unsigned char *)DAT_0048787c + src_offset);
                        if (px_val != 0xFF) {
                            /* Two-stage LUT blend:
                             * 1. Look up blend color table from [color_idx * 16 + pixel_value]
                             * 2. Look up source brightness from background pixel
                             * 3. Use brightness as index into blend color table */
                            int lut_idx = (int)color_idx * 16 + (int)px_val;
                            if (lut_idx < 48) {
                                unsigned short *blend_table =
                                    (unsigned short *)DAT_0048792c[lut_idx];
                                if (blend_table) {
                                    unsigned short bg_pixel = *dst;
                                    unsigned short remap_idx =
                                        ((unsigned short *)DAT_00489230)[bg_pixel];
                                    *dst = blend_table[remap_idx];
                                }
                            }
                        }
                        dst++;
                        src_offset++;
                    }
                }
                src_offset += src_skip;
                dst += dst_skip;
            }
        }

        i++;
        offset += 0x20;
    }
}

/* ===== FUN_0040c280 - General entity sprite renderer (0040C280) ===== */
/*
 * Renders a single sprite frame onto the RGB565 buffer.
 * param_1: sprite/frame index into DAT_00489234
 * param_2: X position relative to viewport
 * param_3: Y position relative to viewport
 * param_4: color palette index (0 = no palette)
 * param_5: buffer pointer
 * param_6: buffer stride (pixels per row)
 * param_7: blend mode (0 = no blend)
 *
 * Pixel data source: DAT_00487ab4 (16-bit sprite pixels, index via DAT_00489234)
 * Dimensions: DAT_00489e8c[sprite] = width, DAT_00489e88[sprite] = height
 */
void FUN_0040c280(int param_1, int param_2, int param_3, unsigned char param_4,
                  int param_5, int param_6, unsigned char param_7)
{
    int pixel_base = ((int *)DAT_00489234)[param_1];
    int spr_w = (int)((unsigned char *)DAT_00489e8c)[param_1];
    int spr_h = (int)((unsigned char *)DAT_00489e88)[param_1];

    if (spr_w <= 0 || spr_h <= 0) return;

    short *dst = (short *)(param_5
        + ((DAT_004806e8 + param_3) * param_6 + DAT_004806ec + param_2) * 2);
    int src_idx = pixel_base;
    int draw_w = spr_w;
    int draw_h = spr_h;
    int src_skip = 0;
    int dst_skip;

    /* Clip top */
    if ((int)param_3 < 0) {
        src_idx += spr_w * (-(int)param_3);
        dst += (-(int)param_3) * param_6;
        draw_h = spr_h + (int)param_3;
    } else if ((int)(param_3 + spr_h) > DAT_004806e4) {
        draw_h = DAT_004806e4 - (int)param_3;
    }

    /* Clip left */
    if (param_2 < 0) {
        src_idx += (-param_2);
        dst += (-param_2);
        src_skip = -param_2;
        draw_w = spr_w + param_2;
        dst_skip = param_6 - spr_w - param_2;
    } else if ((int)(param_2 + spr_w) > DAT_004806d8) {
        src_skip = (spr_w + param_2) - DAT_004806d8;
        draw_w = spr_w - src_skip;
        dst_skip = param_6 - draw_w;
    } else {
        dst_skip = param_6 - spr_w;
        src_skip = 0;
    }

    if (draw_w <= 0 || draw_h <= 0) return;

    /* Simple non-blended, non-palette mode (param_4==0, param_7==0) - used by intro entities */
    if (param_7 == 0 && param_4 == 0) {
        for (int row = 0; row < draw_h; row++) {
            for (int col = 0; col < draw_w; col++) {
                short pixel = ((short *)DAT_00487ab4)[src_idx];
                if (pixel != 0) {
                    *dst = pixel;
                }
                dst++;
                src_idx++;
            }
            src_idx += src_skip;
            dst += dst_skip;
        }
    }
    else if (param_7 == 0 && param_4 != 0) {
        /* Palette-remapped mode */
        for (int row = 0; row < draw_h; row++) {
            for (int col = 0; col < draw_w; col++) {
                unsigned short pixel = ((unsigned short *)DAT_00487ab4)[src_idx];
                if (pixel != 0) {
                    unsigned short remap = ((unsigned short *)DAT_00489230)[(int)pixel];
                    *dst = ((short *)DAT_004876a4[param_4])[(int)remap];
                }
                dst++;
                src_idx++;
            }
            src_idx += src_skip;
            dst += dst_skip;
        }
    }
    else {
        /* Blended mode (param_7 != 0) - simplified for now */
        int blend_idx = (param_7 < 0x1C) ? 0x1C : (int)param_7;
        if (param_4 == 0) {
            for (int row = 0; row < draw_h; row++) {
                for (int col = 0; col < draw_w; col++) {
                    unsigned short pixel = ((unsigned short *)DAT_00487ab4)[src_idx];
                    if (pixel != 0) {
                        unsigned short remap = ((unsigned short *)DAT_00489230)[(int)pixel];
                        *dst = ((short *)DAT_004876a4[blend_idx])[(int)remap];
                    }
                    dst++;
                    src_idx++;
                }
                src_idx += src_skip;
                dst += dst_skip;
            }
        }
    }
}

/* ===== FUN_0040c590 - Explosion/Ship sprite blitter (0040C590) ===== */
/*
 * Similar to FUN_0040c280 but reads from DAT_00487aac (pre-rendered rotated frames).
 * param_1: frame index (angle-based)
 * param_2: player index (selects explosion set)
 * param_3: X position relative to viewport
 * param_4: Y position relative to viewport
 * param_5: palette index (0 = no palette remap)
 * param_6: buffer pointer
 * param_7: buffer stride
 * param_8: blend mode (0 = no blend)
 */
void FUN_0040c590(int param_1, int param_2, int param_3, int param_4,
                  unsigned char param_5, int param_6, int param_7, unsigned char param_8)
{
    int base = (int)DAT_00487aac;
    int spr_h = *(int *)(base + 0x186a4 + param_2 * 0x186a8);
    int spr_w = *(int *)(base + param_2 * 0x186a8 + 100000);
    int src_idx = spr_w * spr_h * param_1;

    short *dst = (short *)(param_6 + ((DAT_004806e8 + param_4) * param_7 + DAT_004806ec + param_3) * 2);
    int draw_h, draw_w, src_skip, dst_skip;

    /* Clip top/bottom */
    if (param_4 < 0) {
        src_idx += spr_w * (-param_4);
        dst += (-param_4) * param_7;
        draw_h = spr_h + param_4;
    } else {
        draw_h = spr_h;
        if (DAT_004806e4 < spr_h + param_4)
            draw_h = DAT_004806e4 - param_4;
    }

    /* Clip left/right */
    if (param_3 < 0) {
        src_idx -= param_3;
        dst_skip = (param_7 - spr_w) - param_3;
        dst += -param_3;
        src_skip = -param_3;
        draw_w = param_3 + spr_w;
    } else if (DAT_004806d8 < spr_w + param_3) {
        src_skip = (spr_w - DAT_004806d8) + param_3;
        dst_skip = (src_skip - spr_w) + param_7;
        draw_w = spr_w - src_skip;
    } else {
        dst_skip = param_7 - spr_w;
        src_skip = 0;
        draw_w = spr_w;
    }

    if (draw_w <= 0 || draw_h <= 0) return;

    unsigned char blend = param_8;
    if (blend == 0) {
        if (param_5 == 0) {
            /* Direct copy mode */
            for (int row = 0; row < draw_h; row++) {
                for (int col = 0; col < draw_w; col++) {
                    int pixel_offset = (src_idx + param_2 * 0xc354) * 2;
                    short pixel = *(short *)(base + pixel_offset);
                    if (pixel != 0) *dst = pixel;
                    dst++;
                    src_idx++;
                }
                src_idx += src_skip;
                dst += dst_skip;
            }
        } else {
            /* Palette-remap mode */
            for (int row = 0; row < draw_h; row++) {
                for (int col = 0; col < draw_w; col++) {
                    int pixel_offset = (src_idx + param_2 * 0xc354) * 2;
                    unsigned short pixel = *(unsigned short *)(base + pixel_offset);
                    if (pixel != 0) {
                        unsigned short remap = ((unsigned short *)DAT_00489230)[pixel];
                        *dst = ((short *)DAT_004876a4[param_5])[remap];
                    }
                    dst++;
                    src_idx++;
                }
                src_idx += src_skip;
                dst += dst_skip;
            }
        }
    } else {
        /* Blended mode */
        int blend_idx = (blend < 0x1C) ? 0x1C : (int)blend;
        if (param_5 == 0) {
            /* Blend only (no palette) */
            for (int row = 0; row < draw_h; row++) {
                for (int col = 0; col < draw_w; col++) {
                    int pixel_offset = (src_idx + param_2 * 0xc354) * 2;
                    unsigned short pixel = *(unsigned short *)(base + pixel_offset);
                    if (pixel != 0) {
                        unsigned short remap = ((unsigned short *)DAT_00489230)[pixel];
                        *dst = ((short *)DAT_004876a4[blend_idx])[remap];
                    }
                    dst++;
                    src_idx++;
                }
                src_idx += src_skip;
                dst += dst_skip;
            }
        } else {
            /* Palette + blend (double LUT lookup) */
            for (int row = 0; row < draw_h; row++) {
                for (int col = 0; col < draw_w; col++) {
                    int pixel_offset = (src_idx + param_2 * 0xc354) * 2;
                    unsigned short pixel = *(unsigned short *)(base + pixel_offset);
                    if (pixel != 0) {
                        unsigned short remap1 = ((unsigned short *)DAT_00489230)[pixel];
                        unsigned short dark = ((unsigned short *)DAT_004876a4[blend_idx])[remap1];
                        unsigned short remap2 = ((unsigned short *)DAT_00489230)[dark];
                        *dst = ((short *)DAT_004876a4[param_5])[remap2];
                    }
                    dst++;
                    src_idx++;
                }
                src_idx += src_skip;
                dst += dst_skip;
            }
        }
    }
}

/* ===== FUN_0040c940 - Shield/Bubble renderer (0040C940) ===== */
/*
 * Renders a translucent shield bubble around an entity.
 * Uses sprite 0x1A3 as a grayscale alpha mask.
 * param_1: X position (18-bit fixed point)
 * param_2: Y position (18-bit fixed point)
 * param_3: buffer pointer
 * param_4: buffer stride
 * param_5: intensity (subtracted from alpha for fade-in effect)
 */
void FUN_0040c940(unsigned int param_1, unsigned int param_2, unsigned int param_3,
                  int param_4, int param_5)
{
    unsigned int spr_w = (unsigned int)((unsigned char *)DAT_00489e8c)[0x1a3];
    int screen_x = ((int)param_1 >> 0x12) - (int)(spr_w >> 1) - DAT_004806dc;
    unsigned int spr_h = (unsigned int)((unsigned char *)DAT_00489e88)[0x1a3];
    int screen_y = ((int)param_2 >> 0x12) - (int)(spr_h >> 1) - DAT_004806e0;
    int src_idx = ((int *)DAT_00489234)[0x1a3];
    unsigned short *dst = (unsigned short *)(param_3 + ((DAT_004806e8 + screen_y) * param_4 + DAT_004806ec + screen_x) * 2);
    unsigned int draw_h, draw_w;
    int src_skip, dst_stride;

    /* Clip top/bottom */
    if (screen_y < 0) {
        src_idx += spr_w * (-screen_y);
        dst += (-screen_y) * param_4;
        draw_h = spr_h + screen_y;
    } else {
        draw_h = spr_h;
        if (DAT_004806e4 < (int)(spr_h + screen_y))
            draw_h = DAT_004806e4 - screen_y;
    }

    /* Clip left/right */
    if (screen_x < 0) {
        dst_stride = (param_4 - spr_w) - screen_x;
        src_idx -= screen_x;
        dst += -screen_x;
        src_skip = -screen_x;
        draw_w = spr_w + screen_x;
    } else if (DAT_004806d8 < (int)(spr_w + screen_x)) {
        src_skip = (spr_w - DAT_004806d8) + screen_x;
        dst_stride = (src_skip - spr_w) + param_4;
        draw_w = spr_w - src_skip;
    } else {
        src_skip = 0;
        dst_stride = param_4 - spr_w;
        draw_w = spr_w;
    }

    if ((int)draw_h <= 0 || (int)draw_w <= 0) return;

    for (unsigned int row = 0; row < draw_h; row++) {
        for (unsigned int col = 0; col < draw_w; col++) {
            int alpha = (int)(((unsigned char *)DAT_00489e94)[src_idx] >> 4) - param_5;
            if (alpha >= 0x11) alpha = 0x10;
            if (alpha > 0) {
                unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                *dst = ((unsigned short *)DAT_0048792c[31 + alpha])[remap];
            }
            dst++;
            src_idx++;
        }
        src_idx += src_skip;
        dst += dst_stride;
    }
}

/* ===== FUN_0040dbd0 - Static entity renderer (0040DBD0) ===== */
/*
 * Renders static entities like turrets/decorations.
 * Array: DAT_00489e98, stride 0x10, count DAT_00489274.
 */
void FUN_0040dbd0(int param_1, unsigned int param_2)
{
    int offset = 0;
    int base = (int)DAT_00489e98;

    for (int i = 0; i < DAT_00489274; i++) {
        int px = *(int *)(offset + base) >> 0x12;
        if (DAT_004806dc < px + 0xe && px - 0xe < DAT_004806d0) {
            int py = *(int *)(offset + 4 + base) >> 0x12;
            if (DAT_004806e0 < py + 0xe && py - 0xe < DAT_004806d4) {
                int angle = *(int *)(offset + 8 + base);
                int sprite = ((int)(angle + (angle >> 0x1f & 0x7f)) >> 7) + 0xf3c;
                int sw = (int)((unsigned char *)DAT_00489e8c)[sprite];
                int sh = (int)((unsigned char *)DAT_00489e88)[sprite];

                /* Darkness from tilemap tile type */
                unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                FUN_0040c280(sprite, (px - (sw >> 1)) - DAT_004806dc,
                             (py - (sh >> 1)) - DAT_004806e0, 0, param_1, param_2, darkness);
                base = (int)DAT_00489e98;
            }
        }
        offset += 0x10;
    }
}

/* ===== FUN_0040dce0 - Dynamic entity/trooper renderer (0040DCE0) ===== */
/*
 * Renders troopers (foot soldiers). Two layers: optional helmet + body.
 * Array: DAT_00487884, stride 0x40, count DAT_0048924c.
 */
void FUN_0040dce0(int param_1, unsigned int param_2)
{
    int offset = 0;
    int vp_left = DAT_004806dc;
    int base = (int)DAT_00487884;

    for (int i = 0; i < DAT_0048924c; i++) {
        int px = *(int *)(offset + base) >> 0x12;
        if (vp_left < px + 6 && px - 6 < DAT_004806d0) {
            int py = *(int *)(offset + 8 + base) >> 0x12;
            if (DAT_004806e0 < py + 6 && py - 6 < DAT_004806d4) {
                /* Darkness from tilemap */
                unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                /* Helmet layer (if flag bit 0 is set) */
                if ((*(unsigned char *)(offset + 0x18 + base) & 1) == 1) {
                    int hw = (int)((unsigned char *)DAT_00489e8c)[0x11];
                    int hh = (int)((unsigned char *)DAT_00489e88)[0x11];
                    FUN_0040c280(0x11, (px - (hw >> 1)) - DAT_004806dc,
                                 ((py - (hh >> 1)) - DAT_004806e0) - 5,
                                 *(unsigned char *)(offset + 0x2c + base),
                                 param_1, param_2, darkness);
                    vp_left = DAT_004806dc;
                    base = (int)DAT_00487884;
                }

                /* Body sprite */
                unsigned char variant = *(unsigned char *)(offset + 0x1c + base);
                unsigned int team = (variant < 5) ? (unsigned int)variant : 3;

                int sprite;
                if (*(char *)(offset + 0x25 + base) == '\x01') {
                    /* Walking animation */
                    int angle = *(int *)(offset + 0x30 + base);
                    sprite = ((int)(angle + (angle >> 0x1f & 0x3f)) >> 6) + (team + 100) * 100;
                    if (*(char *)(offset + 0x1d + base) == '\x01')
                        sprite += 500;
                    int bw = (int)((unsigned char *)DAT_00489e8c)[sprite];
                    int bh = (int)((unsigned char *)DAT_00489e88)[sprite];
                    int sx = (px - (bw >> 1)) - vp_left;
                    int sy = ((py - (bh >> 1)) - (*(unsigned char *)(offset + 0x24 + base) & 1)) - DAT_004806e0 - 3;
                    FUN_0040c280(sprite, sx, sy, *(unsigned char *)(offset + 0x2c + base),
                                 param_1, param_2, darkness);
                } else {
                    /* Standing/shooting animation */
                    sprite = (unsigned int)(2 < *(unsigned char *)(offset + 0x24 + base)) + (team + 0xb4) * 100;
                    int bw = (int)((unsigned char *)DAT_00489e8c)[sprite];
                    int bh = (int)((unsigned char *)DAT_00489e88)[sprite];
                    int sx = (px - (bw >> 1)) - vp_left;
                    int sy = ((py - (bh >> 1)) - DAT_004806e0) - 2;
                    FUN_0040c280(sprite, sx, sy, *(unsigned char *)(offset + 0x2c + base),
                                 param_1, param_2, darkness);
                }
                vp_left = DAT_004806dc;
                base = (int)DAT_00487884;
            }
        }
        offset += 0x40;
    }
}

/* ===== FUN_0040bb60 - Main entity renderer (0040BB60) ===== */
/*
 * Renders main gameplay entities (items, weapons, ships, effects).
 * Array: DAT_004892e8, stride 0x80, count DAT_00489248.
 * Most complex renderer - handles pixel dots, sprites, shields, angle animations.
 */
void FUN_0040bb60(unsigned int param_1, unsigned int param_2)
{
    if (DAT_00489248 <= 0) return;

    int ent_base = (int)DAT_004892e8;

    for (int i = 0; i < DAT_00489248; i++) {
        int ent_off = i * 0x80;
        unsigned int sprite_idx = *(unsigned int *)(ent_base + 0x4c + ent_off);

        if (sprite_idx < 30000) {
            /* Full sprite rendering */
            unsigned char anim_type = *(unsigned char *)(ent_base + 0x21 + ent_off);
            int px = *(int *)(ent_base + ent_off) >> 0x12;

            if (anim_type == 0x65) {
                /* 2x2 pixel cross pattern */
                if (DAT_004806dc <= px && px + 2 < DAT_004806d0) {
                    int py = *(int *)(ent_base + 8 + ent_off) >> 0x12;
                    if (DAT_004806e0 <= py && py + 2 < DAT_004806d4 && sprite_idx != 130000) {
                        unsigned short *dst = (unsigned short *)(param_1 + 2 +
                            (((py - DAT_004806e0 + DAT_004806e8) * param_2 - DAT_004806dc + px + DAT_004806ec) * 2));
                        *dst = DAT_0048384e;
                        *(dst + (param_2 - 1)) = DAT_0048384e;
                        *(dst + param_2 * 1 + 1) = DAT_00483850;
                        *(dst + param_2 * 1 + param_2) = DAT_00483850;
                        ent_base = (int)DAT_004892e8;
                    }
                }
            } else if (DAT_004806dc < px + 7 && px - 7 < DAT_004806d0) {
                int py_raw = *(int *)(ent_base + 8 + ent_off) >> 0x12;
                if (DAT_004806e0 < py_raw + 7 && py_raw - 7 < DAT_004806d4) {
                    /* Determine animation variant */
                    unsigned char anim_data = *(unsigned char *)(
                        (int)DAT_00487abc + (unsigned int)*(unsigned char *)(ent_base + 0x40 + ent_off) +
                        0x124 + (unsigned int)anim_type * 0x218);

                    if (anim_data < 200) {
                        sprite_idx += *(int *)(ent_base + 0x48 + ent_off);
                    } else if (anim_data == 200) {
                        /* 16-direction angle-based */
                        int angle = FUN_004257e0(0, 0, *(int *)(ent_base + 0x18 + ent_off),
                                                 *(int *)(ent_base + 0x1c + ent_off));
                        sprite_idx += (unsigned int)((angle + 0x40) >> 7) & 0xf;
                        ent_base = (int)DAT_004892e8;
                    } else if (anim_data == 0xc9) {
                        /* 32-direction angle-based */
                        int angle = FUN_004257e0(0, 0, *(int *)(ent_base + 0x18 + ent_off),
                                                 *(int *)(ent_base + 0x1c + ent_off));
                        sprite_idx += (unsigned int)((angle * 2 + 0x40) >> 7) & 0x1f;
                        ent_base = (int)DAT_004892e8;
                    } else if (anim_data == 0xca) {
                        /* 16-dir from velocity angle */
                        int angle = FUN_004257e0(0, 0, *(int *)(ent_base + 0x18 + ent_off),
                                                 *(int *)(ent_base + 0x1c + ent_off));
                        sprite_idx += (unsigned int)((angle * 2 + 0x40) >> 7) & 0xf;
                        ent_base = (int)DAT_004892e8;
                    } else if (anim_data == 0xcb) {
                        sprite_idx += (unsigned int)((*(int *)(ent_base + 0x3c + ent_off) + 0x40) >> 7) & 0xf;
                    } else if (anim_data == 0xcc) {
                        int r = rand();
                        ent_base = (int)DAT_004892e8;
                        if (r % 0xf == 0) {
                            int r2 = rand();
                            sprite_idx += 1 + r2 % 5;
                            ent_base = (int)DAT_004892e8;
                        }
                    } else if (anim_data == 0xcd) {
                        sprite_idx += (unsigned int)((*(int *)(ent_base + 0x3c + ent_off) + 0x20) >> 6) & 0x1f;
                    } else if (anim_data == 0xce) {
                        sprite_idx += (unsigned int)((*(int *)(ent_base + 0x3c + ent_off) * 2 + 0x40) >> 7) & 0xf;
                    } else if (anim_data == 0xcf) {
                        sprite_idx += (unsigned int)(((*(int *)(ent_base + 0x3c + ent_off)) << 2) >> 8) & 7;
                    }

                    /* Render: shield bubble or sprite */
                    if (sprite_idx == 0x36) {
                        int shield_age = *(int *)(ent_base + 0x28 + ent_off);
                        int intensity = (shield_age < 0x20) ? (0x10 - shield_age / 2) : 0;
                        FUN_0040c940(*(unsigned int *)(ent_base + ent_off),
                                     *(unsigned int *)(ent_base + 8 + ent_off),
                                     param_1, param_2, intensity);
                        ent_base = (int)DAT_004892e8;
                    } else {
                        int py = *(int *)(ent_base + 8 + ent_off) >> 0x12;
                        int px2 = *(int *)(ent_base + ent_off) >> 0x12;
                        int sw = (int)((unsigned char *)DAT_00489e8c)[sprite_idx];
                        int sh = (int)((unsigned char *)DAT_00489e88)[sprite_idx];

                        unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px2];
                        unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                        FUN_0040c280(sprite_idx, (px2 - (sw >> 1)) - DAT_004806dc,
                                     (py - (sh >> 1)) - DAT_004806e0,
                                     *(unsigned char *)(ent_base + 0x24 + ent_off),
                                     param_1, param_2, darkness);
                        ent_base = (int)DAT_004892e8;
                    }
                }
            }
        } else {
            /* Pixel dot rendering (sprite_idx >= 30000) */
            int px = *(int *)(ent_base + ent_off) >> 0x12;
            if (DAT_004806dc <= px && px + 1 < DAT_004806d0) {
                int py = *(int *)(ent_base + 8 + ent_off) >> 0x12;
                if (DAT_004806e0 <= py && py + 1 < DAT_004806d4 && sprite_idx != 130000) {
                    unsigned short *dst = (unsigned short *)(param_1 +
                        (((py - DAT_004806e0 + DAT_004806e8) * param_2 - DAT_004806dc + px + DAT_004806ec) * 2));

                    unsigned short color = (unsigned short)(*(short *)(ent_base + 0x4c + ent_off) + (short)0x8ad0);

                    /* Check darkness */
                    unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                    if (((char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4] != '\0') {
                        unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)color];
                        color = ((unsigned short *)DAT_004876a4[28])[remap]; /* DAT_00487714 = palette[28] */
                    }

                    /* Shape patterns based on entry[0x24] */
                    unsigned short shape = *(unsigned short *)(ent_base + 0x24 + ent_off);
                    switch (shape) {
                    case 0:
                        *dst = color;
                        break;
                    case 1:
                        *dst = color;
                        *(dst + param_2) = color;
                        *(dst + param_2 + 1) = color;
                        break;
                    case 2:
                        *dst = color;
                        *(dst + param_2) = color;
                        break;
                    case 3:
                        *dst = color;
                        *(dst + param_2 + 1) = color;
                        break;
                    case 4:
                        *dst = color;
                        *(dst + 1) = color;
                        break;
                    case 5:
                        *(dst + 1) = color;
                        *(dst + param_2) = color;
                        break;
                    case 6:
                        *dst = 0x6739;
                        *(dst + 1) = color;
                        *(dst + param_2) = color;
                        *(dst + param_2 + 1) = color;
                        break;
                    }
                    ent_base = (int)DAT_004892e8;
                }
            }
        }
    }
}

/* ===== FUN_0040a870 - Projectile renderer (0040A870) ===== */
/*
 * Renders projectiles (bullets, missiles, etc).
 * Array: DAT_00481f28, stride 0x40, count DAT_00489260.
 */
void FUN_0040a870(int param_1, unsigned int param_2)
{
    int offset = 0;

    for (int i = 0; i < DAT_00489260; i++) {
        int px = *(int *)(offset + (int)DAT_00481f28) >> 0x12;
        if (DAT_004806dc < px + 0xc && px - 0xc < DAT_004806d0) {
            int py = *(int *)(offset + 4 + (int)DAT_00481f28) >> 0x12;
            if (DAT_004806e0 < py + 0xc && py - 0xc < DAT_004806d4) {
                unsigned char proj_type = *(unsigned char *)(offset + 0x1c + (int)DAT_00481f28);
                int angle_raw = *(int *)(offset + 8 + (int)DAT_00481f28);

                unsigned int angle_idx;
                if (proj_type == 7) {
                    angle_idx = angle_raw << 6;
                } else {
                    angle_idx = (angle_raw + 0x20) & 0x7ff;
                }

                int sprite = *(int *)((unsigned int)proj_type * 0x20 + 0xc + (int)DAT_00487818) +
                             (unsigned int)*(unsigned char *)(offset + 0x1d + (int)DAT_00481f28) * 100 +
                             ((int)(angle_idx + (angle_idx >> 0x1f & 0x3f)) >> 6);

                if (*(char *)(offset + 0x1c + (int)DAT_00481f28) == '\x05' &&
                    *(int *)(offset + 0x14 + (int)DAT_00481f28) > 2) {
                    if (*(char *)(offset + 0x20 + (int)DAT_00481f28) == '\x01')
                        sprite += 500;
                    else
                        sprite += 1000;
                }

                int sw = (int)((unsigned char *)DAT_00489e8c)[sprite];
                int sh = (int)((unsigned char *)DAT_00489e88)[sprite];

                unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                FUN_0040c280(sprite, (px - (sw >> 1)) - DAT_004806dc,
                             (py - (sh >> 1)) - DAT_004806e0,
                             *(unsigned char *)(offset + 0x1e + (int)DAT_00481f28),
                             param_1, param_2, darkness);
            }
        }
        offset += 0x40;
    }
}

/* ===== FUN_0040d6c0 - Explosion renderer (0040D6C0) ===== */
/*
 * Renders explosions using the pre-rendered rotation frames.
 * Array: DAT_00487a9c, stride 0x20, count DAT_0048926c.
 * Uses FUN_0040c590 (explosion blitter).
 */
void FUN_0040d6c0(int param_1, int param_2)
{
    int offset = 0;

    for (int i = 0; i < DAT_0048926c; i++) {
        int px = *(int *)(offset + (int)DAT_00487a9c) >> 0x12;
        if (DAT_004806dc < px + 0x14 && px - 0x14 < DAT_004806d0) {
            int py = *(int *)(offset + 4 + (int)DAT_00487a9c) >> 0x12;
            if (DAT_004806e0 < py + 0x14 && py - 0x14 < DAT_004806d4) {
                unsigned int player = (unsigned int)*(unsigned char *)(offset + 0x18 + (int)DAT_00487a9c);
                int base = (int)DAT_00487aac + player * 0x186a8;
                int ew = *(int *)(base + 100000);
                int eh = *(int *)(base + 0x186a4);

                int angle_raw = *(int *)(offset + 0x10 + (int)DAT_00487a9c);
                int frame = (int)(((angle_raw + 0x20) & 0x7ff) * 0x20) >> 0xb;

                unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                FUN_0040c590(frame, player,
                             (px - ew / 2) - DAT_004806dc,
                             (py - eh / 2) - DAT_004806e0,
                             0, param_1, param_2, darkness);
            }
        }
        offset += 0x20;
    }
}

/* ===== FUN_0040d810 - Debris/particle renderer (0040D810) ===== */
/*
 * Renders debris particles (small sprites).
 * Array: DAT_00487830, stride 0x20, count DAT_00489268.
 */
void FUN_0040d810(int param_1, unsigned int param_2)
{
    int offset = 0;
    int base = (int)DAT_00487830;

    for (int i = 0; i < DAT_00489268; i++) {
        int px = *(int *)(offset + base) >> 0x12;
        if (DAT_004806dc < px + 0x14 && px - 0x14 < DAT_004806d0) {
            int py = *(int *)(offset + 4 + base) >> 0x12;
            if (DAT_004806e0 < py + 0x14 && py - 0x14 < DAT_004806d4) {
                int sprite = ((int *)g_EntityConfig)[(unsigned int)*(unsigned char *)(offset + 10 + base) * 2] +
                             (unsigned int)*(unsigned char *)(offset + 8 + base);
                int sw = (int)((unsigned char *)DAT_00489e8c)[sprite];
                int sh = (int)((unsigned char *)DAT_00489e88)[sprite];

                unsigned char tile = ((unsigned char *)DAT_0048782c)[(py << ((unsigned char)DAT_00487a18 & 0x1f)) + px];
                unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                FUN_0040c280(sprite, (px - (sw >> 1)) - DAT_004806dc,
                             (py - (sh >> 1)) - DAT_004806e0, 0, param_1, param_2, darkness);
                base = (int)DAT_00487830;
            }
        }
        offset += 0x20;
    }
}

/* ===== FUN_0040caf0 - Player/ship renderer (0040CAF0) ===== */
/*
 * Renders player ships and their visual effects (sparks, shields, exhaust).
 * Array: DAT_00487810, stride 0x598, count DAT_00489240.
 * Two passes: (1) ship sprite, (2) overlay effects.
 */
void FUN_0040caf0(int param_1, unsigned int param_2)
{
    int player_base = DAT_00487810;

    /* Pass 1: Ship sprites via FUN_0040c590 */
    int offset = 0;
    int explosion_offset = 0;
    for (int i = 0; i < DAT_00489240; i++) {
        if (*(char *)(player_base + 0x24 + offset) == '\0') {
            int px = *(int *)(player_base + offset) >> 0x12;
            if (DAT_004806dc < px + 0xe && px - 0xe < DAT_004806d0) {
                int py = *(int *)(player_base + 4 + offset) >> 0x12;
                if (DAT_004806e0 < py + 0xe && py - 0xe < DAT_004806d4) {
                    /* Calculate blend from damage */
                    unsigned char blend = 0;
                    char damage = *(char *)(player_base + 0x49f + offset);
                    if (damage > 0) {
                        int b = damage * 4;
                        blend = (char)((int)(b + (b >> 0x1f & 0x3f)) >> 6) + 0x30;
                    }

                    int base = (int)DAT_00487aac + explosion_offset;
                    int ew = *(int *)(base + 100000);
                    int eh = *(int *)(base + 0x186a4);

                    int angle = *(int *)(player_base + 0x18 + offset);
                    int frame = (int)(((angle + 0x20) & 0x7ff) * 0x20) >> 0xb;

                    FUN_0040c590(frame, i,
                                 (px - ew / 2) - DAT_004806dc,
                                 (py - eh / 2) - DAT_004806e0,
                                 *(unsigned char *)(player_base + 0xa3 + offset),
                                 param_1, param_2, blend);
                    player_base = DAT_00487810;
                }
            }
        }
        offset += 0x598;
        explosion_offset += 0x186a8;
    }

    /* Pass 2: Ship overlay effects */
    offset = 0;
    for (int i = 0; i < DAT_00489240; i++) {
        if (*(char *)(offset + 0x24 + player_base) == '\0') {
            int px = *(int *)(offset + player_base) >> 0x12;
            if (DAT_004806dc < px + 10 && px - 10 < DAT_004806d0) {
                int py = *(int *)(offset + 4 + player_base) >> 0x12;
                if (DAT_004806e0 < py + 10 && py - 10 < DAT_004806d4) {
                    /* Damage sparks (random sprite 0x29-0x2B) */
                    if (*(int *)(offset + 0xa4 + player_base) != 0 &&
                        *(char *)(*(char *)(offset + 0x34 + player_base) + offset + 0x3c + player_base) == '\r') {
                        int r = rand();
                        player_base = DAT_00487810;
                        if (r % 10 == 0) {
                            int r2 = rand();
                            int spr = r2 % 3 + 0x29;
                            int spx = *(int *)(offset + DAT_00487810) >> 0x12;
                            int spy = *(int *)(offset + 4 + DAT_00487810) >> 0x12;
                            int sw = (int)((unsigned char *)DAT_00489e8c)[spr];
                            int sh = (int)((unsigned char *)DAT_00489e88)[spr];

                            unsigned char tile = ((unsigned char *)DAT_0048782c)[(spy << ((unsigned char)DAT_00487a18 & 0x1f)) + spx];
                            unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                            FUN_0040c280(spr, (spx - (sw >> 1)) - DAT_004806dc,
                                         (spy - (sh >> 1)) - DAT_004806e0, 0,
                                         param_1, param_2, darkness);
                            player_base = DAT_00487810;
                        }
                    }

                    /* Shield glow effect (sprite 0x198+) */
                    unsigned int shield_state = *(unsigned int *)(offset + 0xa4 + player_base);
                    if (shield_state != 0 &&
                        *(char *)(*(char *)(offset + 0x34 + player_base) + offset + 0x3c + player_base) == '\f') {
                        int spy = *(int *)(offset + 4 + player_base) >> 0x12;
                        int spr = 0x51c - (shield_state >> 1) % 9;
                        int spx = *(int *)(offset + player_base) >> 0x12;
                        int sw = (int)((unsigned char *)DAT_00489e8c)[spr];
                        int sh = (int)((unsigned char *)DAT_00489e88)[spr];

                        unsigned char tile = ((unsigned char *)DAT_0048782c)[(spy << ((unsigned char)DAT_00487a18 & 0x1f)) + spx];
                        unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                        FUN_0040c280(spr, (spx - (sw >> 1)) - DAT_004806dc,
                                     (spy - (sh >> 1)) - DAT_004806e0, 0,
                                     param_1, param_2, darkness);
                        player_base = DAT_00487810;
                    }

                    /* Weapon charge glow (sprite 0x198 + charge level) */
                    if (*(char *)(offset + 0xc6 + player_base) != '\0') {
                        int charge_spr = *(unsigned char *)(offset + 199 + player_base) + 0x198;
                        unsigned int cw = (unsigned int)((unsigned char *)DAT_00489e8c)[charge_spr];
                        unsigned int ch = (unsigned int)((unsigned char *)DAT_00489e88)[charge_spr];
                        int cx = ((*(int *)(offset + player_base) >> 0x12) - (int)(cw >> 1)) - DAT_004806dc;
                        int cy = ((*(int *)(offset + 4 + player_base) >> 0x12) - (int)(ch >> 1)) - DAT_004806e0;
                        int src_idx = ((int *)DAT_00489234)[charge_spr];

                        unsigned short *dst = (unsigned short *)(param_1 +
                            ((DAT_004806e8 + cy) * param_2 + DAT_004806ec + cx) * 2);
                        int draw_h = ch, draw_w = cw;
                        int src_skip = 0, dst_skip;

                        /* Clip top */
                        if (cy < 0) {
                            src_idx += cw * (-cy);
                            dst += (-cy) * param_2;
                            draw_h = ch + cy;
                        } else if (DAT_004806e4 < (int)(ch + cy)) {
                            draw_h = DAT_004806e4 - cy;
                        }
                        /* Clip left */
                        if (cx < 0) {
                            src_idx -= cx;
                            dst_skip = (param_2 - cw) - cx;
                            dst += -cx;
                            src_skip = -cx;
                            draw_w = cx + cw;
                        } else if (DAT_004806d8 < (int)(cw + cx)) {
                            src_skip = (cw - DAT_004806d8) + cx;
                            dst_skip = (src_skip - cw) + param_2;
                            draw_w = cw - src_skip;
                        } else {
                            dst_skip = param_2 - cw;
                            src_skip = 0;
                        }

                        if (draw_w > 0 && draw_h > 0) {
                            for (int row = 0; row < draw_h; row++) {
                                for (int col = 0; col < draw_w; col++) {
                                    int intensity = (int)(((unsigned char *)DAT_00489e94)[src_idx] + 0x10) >> 5;
                                    while (intensity > 0) {
                                        unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                                        *dst = ((unsigned short *)DAT_004876a4[22])[remap]; /* DAT_004876fc = palette[22] */
                                        intensity--;
                                        player_base = DAT_00487810;
                                    }
                                    dst++;
                                    src_idx++;
                                }
                                src_idx += src_skip;
                                dst += dst_skip;
                            }
                        }
                    }

                    /* Jet exhaust smoke (sprite 0xDAB-) */
                    shield_state = *(unsigned int *)(offset + 0xa4 + player_base);
                    if (shield_state != 0 &&
                        *(char *)(*(char *)(offset + 0x34 + player_base) + offset + 0x3c + player_base) == '\n') {
                        int spy = *(int *)(offset + 4 + player_base) >> 0x12;
                        int spr = 0xdb5 - (shield_state >> 2) % 10;
                        int spx = *(int *)(offset + player_base) >> 0x12;
                        int sw = (int)((unsigned char *)DAT_00489e8c)[spr];
                        int sh = (int)((unsigned char *)DAT_00489e88)[spr];

                        unsigned char tile = ((unsigned char *)DAT_0048782c)[(spy << ((unsigned char)DAT_00487a18 & 0x1f)) + spx];
                        unsigned char darkness = ((unsigned char *)DAT_00487928)[(unsigned int)tile * 0x20 + 4];

                        FUN_0040c280(spr, (spx - (sw >> 1)) - DAT_004806dc,
                                     (spy - (sh >> 1)) - DAT_004806e0, 0,
                                     param_1, param_2, darkness);
                        player_base = DAT_00487810;
                    }
                }
            }
        }
        offset += 0x598;
    }
}

/* ===== FUN_0040d930 - Misc effect renderer (0040D930) ===== */
/*
 * Renders misc glow/smoke effects with inline grayscale blitter.
 * Array: DAT_00487780, stride 0x20, count DAT_00489264.
 */
void FUN_0040d930(int param_1, unsigned int param_2)
{
    if (DAT_00489264 <= 0) return;

    int base = (int)DAT_00487780;
    int widths = (int)DAT_00489e8c;

    int entry_off = 0;
    for (int i = 0; i < DAT_00489264; i++) {
        int saved_off = entry_off;
        int px = *(int *)(entry_off + base) >> 0x12;
        if (DAT_004806dc < px + 0x14 && px - 0x14 < DAT_004806d0) {
            int py = *(int *)(entry_off + 4 + base) >> 0x12;
            if (DAT_004806e0 < py + 0x14 && py - 0x14 < DAT_004806d4) {
                /* Glow sprite 0x193 - inline grayscale blitter */
                unsigned int spr_w = (unsigned int)*(unsigned char *)(widths + 0x193);
                int screen_x = (px - (int)(spr_w >> 1)) - DAT_004806dc;
                int screen_y = (py - (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x193)) / 2 - DAT_004806e0;
                screen_y = (py - (int)((unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x193) >> 1)) - DAT_004806e0;
                int src_idx = ((int *)DAT_00489234)[0x193];
                unsigned short *dst = (unsigned short *)(param_1 +
                    ((DAT_004806e8 + screen_y) * param_2 + DAT_004806ec + screen_x) * 2);
                unsigned int rows_remaining = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x193);

                if (rows_remaining != 0) {
                    int row_count = 0;
                    do {
                        int col = 0;
                        int col_x = screen_x;
                        if ((int)spr_w > 0) {
                            do {
                                unsigned char gray = ((unsigned char *)DAT_00489e94)[src_idx];
                                int intensity;
                                if (gray == 0) {
                                    intensity = 0;
                                } else {
                                    intensity = (int)((*(int *)((int)DAT_00487ab0 + *(int *)(saved_off + 0x10 + base) * 4) >> 0xc)
                                                + 0x6e + (unsigned int)gray) >> 5;
                                    if (intensity > 0xe) intensity = 0xe;
                                }
                                if (col_x >= 0 && col_x < DAT_004806d8 &&
                                    screen_y >= 0 && screen_y < DAT_004806e4 && intensity > 0) {
                                    unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                                    *dst = ((unsigned short *)DAT_0048792c[32 + intensity])[remap];
                                    base = (int)DAT_00487780;
                                    widths = (int)DAT_00489e8c;
                                }
                                src_idx++;
                                dst++;
                                col++;
                                col_x++;
                            } while (col < (int)(unsigned int)*(unsigned char *)(widths + 0x193));
                        }
                        spr_w = (unsigned int)*(unsigned char *)(widths + 0x193);
                        row_count++;
                        dst += (param_2 - spr_w);
                        screen_y++;
                    } while (row_count < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x193));
                }

                /* Additional large glow sprite */
                int glow_val = *(int *)(saved_off + 0xc + base);
                if (glow_val > 0) {
                    int spr2 = (glow_val >> 1) + 0x1004;
                    int sw2 = (int)((unsigned char *)DAT_00489e8c)[spr2];
                    int sh2 = (int)((unsigned char *)DAT_00489e88)[spr2];
                    int px2 = *(int *)(saved_off + base) >> 0x12;
                    int py2 = *(int *)(saved_off + 4 + base) >> 0x12;
                    FUN_0040c280(spr2, (px2 - (sw2 >> 1)) - DAT_004806dc,
                                 (py2 - (sh2 >> 1)) - DAT_004806e0, 0,
                                 param_1, param_2, 0);
                    base = (int)DAT_00487780;
                    widths = (int)DAT_00489e8c;
                }
            }
        }
        entry_off = saved_off + 0x20;
    }
}

/* ===== FUN_0040d360 - Edge tile/detail renderer (0040D360) ===== */
/*
 * Renders edge/detail tiles with custom inline grayscale blitter.
 * Array: DAT_00481f2c, stride 0x20, count DAT_0048925c.
 * Three blend modes based on entry[0x15].
 */
void FUN_0040d360(int param_1, int param_2)
{
    if (DAT_0048925c <= 0) return;

    unsigned int entry_off = 0;
    for (int i = 0; i < DAT_0048925c; i++) {
        unsigned int saved_off = entry_off;
        int px = *(int *)(entry_off + (int)DAT_00481f2c) >> 0x12;
        if (DAT_004806dc < px + 0x14 && px - 0x14 < DAT_004806d0) {
            int py = *(int *)(entry_off + 4 + (int)DAT_00481f2c) >> 0x12;
            if (DAT_004806e0 < py + 0x14 && py - 0x14 < DAT_004806d4) {
                /* Look up edge sprite from type table */
                int spr_base = *(int *)((int)DAT_00487ab8 + (unsigned int)*(unsigned char *)(entry_off + 0x10 + (int)DAT_00481f2c) * 8);
                unsigned char w_byte = *(unsigned char *)((int)DAT_00489e8c + 500 + spr_base);
                unsigned int spr_w = (unsigned int)w_byte;
                int sprite = spr_base + 500;
                int screen_x = (px - (int)(w_byte >> 1)) - DAT_004806dc;
                unsigned char h_byte = *(unsigned char *)((int)DAT_00489e88 + sprite);
                unsigned int spr_h = (unsigned int)h_byte;
                int screen_y = (py - (int)(h_byte >> 1)) - DAT_004806e0;
                int src_idx = ((int *)DAT_00489234)[sprite];
                unsigned short *dst = (unsigned short *)(param_1 +
                    ((DAT_004806e8 + screen_y) * param_2 + DAT_004806ec + screen_x) * 2);
                int src_skip = 0;
                int dst_skip;

                /* Clip top */
                if (screen_y < 0) {
                    src_idx -= spr_w * screen_y;
                    dst += -(screen_y * param_2);
                    spr_h = spr_h + screen_y;
                } else if (DAT_004806e4 < (int)(spr_h + screen_y)) {
                    spr_h = DAT_004806e4 - screen_y;
                }

                /* Clip left */
                int left_clip;
                if (screen_x < 0) {
                    src_idx -= screen_x;
                    dst += -screen_x;
                    left_clip = -screen_x;
                    dst_skip = (param_2 - spr_w) - screen_x;
                    spr_w = screen_x + spr_w;
                } else {
                    if (DAT_004806d8 < (int)(spr_w + screen_x)) {
                        left_clip = (spr_w - DAT_004806d8) + screen_x;
                        dst_skip = left_clip - spr_w;
                        spr_w = spr_w - left_clip;
                    } else {
                        dst_skip = -(int)spr_w;
                        left_clip = 0;
                    }
                    dst_skip += param_2;
                }

                char blend_mode = *(char *)(saved_off + 0x15 + (int)DAT_00481f2c);
                unsigned char threshold = *(unsigned char *)(saved_off + 0x11 + (int)DAT_00481f2c);

                if (blend_mode == '\x02') {
                    /* Blend mode 2: palette[23+n] */
                    if ((int)spr_h > 0) {
                        for (unsigned int row = 0; row < spr_h; row++) {
                            for (unsigned int col = 0; col < spr_w; col++) {
                                char intensity = (char)((int)(((unsigned char *)DAT_00489e94)[src_idx] + 0x28) >> 6) - (threshold >> 1);
                                if (intensity > 0) {
                                    unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                                    *dst = ((unsigned short *)DAT_004876a4[23 + intensity])[remap];
                                }
                                dst++;
                                src_idx++;
                            }
                            src_idx += left_clip;
                            dst += dst_skip;
                        }
                    }
                } else if (blend_mode == '\x01') {
                    /* Blend mode 1: palette[4+n] */
                    if ((int)spr_h > 0) {
                        for (unsigned int row = 0; row < spr_h; row++) {
                            for (unsigned int col = 0; col < spr_w; col++) {
                                char intensity = (((unsigned char *)DAT_00489e94)[src_idx] >> 5) - (char)threshold;
                                if (intensity > 0) {
                                    unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                                    *dst = ((unsigned short *)DAT_004876a4[4 + intensity])[remap];
                                }
                                dst++;
                                src_idx++;
                            }
                            src_idx += left_clip;
                            dst += dst_skip;
                        }
                    }
                } else {
                    /* Default blend mode: palette[17+n] via DAT_004876e8 */
                    if ((int)spr_h > 0) {
                        for (unsigned int row = 0; row < spr_h; row++) {
                            for (unsigned int col = 0; col < spr_w; col++) {
                                char intensity = (char)((int)(((unsigned char *)DAT_00489e94)[src_idx] + 0x28) >> 6) - (threshold >> 1);
                                if (intensity > 0) {
                                    unsigned short remap = ((unsigned short *)DAT_00489230)[(unsigned int)*dst];
                                    *dst = ((unsigned short *)DAT_004876a4[17 + intensity])[remap];
                                }
                                dst++;
                                src_idx++;
                            }
                            src_idx += left_clip;
                            dst += dst_skip;
                        }
                    }
                }
            }
        }
        entry_off = saved_off + 0x20;
    }
}

/* ===== FUN_004075f0 - Entity renderer (004075F0) ===== */
/*
 * Iterates active entities (DAT_00489248 count, DAT_004892e8 array)
 * and calls FUN_0040c280 to render each entity's current sprite frame.
 * Entity struct is 0x80 bytes, position at +0/+8 (fixed point >> 18).
 * Sprite index: entity[0x48] + entity[0x4C].
 */
void FUN_004075f0(int buffer, int stride)
{
    if (DAT_00489248 <= 0) return;

    unsigned char *ent_base = (unsigned char *)DAT_004892e8;
    int count = DAT_00489248;

    for (int i = 0; i < count; i++) {
        int *ent = (int *)(ent_base + i * 0x80);

        /* Get pixel position from 18-bit fixed point */
        int px = ent[0] >> 0x12;
        int py = ent[2] >> 0x12;  /* entity[0x08] = ent[2] */

        /* Viewport bounds check with 7-pixel margin */
        if (px + 7 <= DAT_004806dc) continue;
        if (px - 7 >= DAT_004806d0) continue;
        if (py + 7 <= DAT_004806e0) continue;
        if (py - 7 >= DAT_004806d4) continue;

        /* Compute sprite index: animation base + current frame offset */
        int sprite_idx = ent[0x4C / 4] + ent[0x48 / 4];

        /* Get sprite dimensions for centering */
        int sw = (int)((unsigned char *)DAT_00489e8c)[sprite_idx];
        int sh = (int)((unsigned char *)DAT_00489e88)[sprite_idx];

        /* Draw centered at position, no palette, no blend */
        FUN_0040c280(sprite_idx,
                     (px - (sw >> 1)) - DAT_004806dc,
                     (py - (sh >> 1)) - DAT_004806e0,
                     0, buffer, stride, 0);
    }
}

/* ===== FUN_004076d0 - Viewport setup + render effects (004076D0) ===== */
void FUN_004076d0(int buffer, int stride)
{
    DAT_004806dc = 0;
    DAT_004806e0 = 0;
    DAT_004806d0 = 640;   /* 0x280 */
    DAT_004806d4 = 480;   /* 0x1E0 */
    DAT_004806d8 = 640;
    DAT_004806e4 = 480;
    DAT_004806e8 = 0;
    DAT_004806ec = 0;

    FUN_004075f0(buffer, stride);
    FUN_0040d100(buffer, stride);
}
