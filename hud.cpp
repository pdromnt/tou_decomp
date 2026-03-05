/* ===== hud.cpp - HUD rendering (health bar, weapon icon, minimap, text) ===== */
/* All functions are part of FUN_00407720 (Render_Game_World / viewport renderer).
 * They draw onto the RGB565 framebuffer using viewport-relative coordinates
 * from DAT_004806ec/e8/d8/e4. */

#include "tou.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* float constant used in fog sky ratio check (1.0f) */
static float _DAT_004753e8_fog = 1.0f;

/* ===== FUN_0040aca0 - Pickup/powerup text display (0040ACA0) ===== */
/* Shows pickup name near bottom of viewport when player collects an item.
 * player_data[0xCA] selects the string via switch. */
void FUN_0040aca0(int param_1, int param_2, int param_3)
{
    char local_20[32];
    const char *text = NULL;

    switch (*(unsigned char *)(DAT_00487810 + 0xCA + param_3 * 0x598)) {
    case 0:    text = "Full energy"; break;
    case 1:    text = "Booby trap"; break;
    case 2:    text = "Death Ring"; break;
    case 3:    text = "4 Miniships"; break;
    case 4:    text = "6 Insects"; break;
    case 5:    text = "Weapon loaded"; break;
    case 6:    text = "Faster special gun"; break;
    case 7:    text = "Better basic gun"; break;
    case 0x14: text = "Small medikit"; break;
    case 0x15: text = "Large medikit"; break;
    case 0xC8: text = "Hurry up!"; break;
    default:   return;  /* Unknown pickup type, don't draw */
    }

    strcpy(local_20, text);

    /* Y offset: viewport bottom - 0x12, with extra -0xD if viewport width <= 255 */
    int y_off = DAT_004806e4 - ((DAT_004806d8 > 0xFF) ? 0x12 : (0x12 + 0x0D));
    int dest = param_1 + ((y_off + DAT_004806e8) * param_2 + DAT_004806ec) * 2 + 8;
    Draw_Text_To_Buffer(local_20, 1, 5, (unsigned short *)dest, param_2, 0, DAT_004806d8 - 0x0C, 0);
}

/* ===== FUN_004094f0 - Team status text (004094F0) ===== */
/* Shows team victory/death message near bottom of viewport.
 * DAT_004892a4 == 0xFF: all dead, == player team: your team wins, else: team N wins. */
void FUN_004094f0(int param_1, int param_2, int param_3)
{
    char local_34[52];

    if ((unsigned char)DAT_004892a4 == 0xFF) {
        strcpy(local_34, "All teams are dead");
    }
    else if (((unsigned char)DAT_004892a4 & 0xFF) - 1 == param_3) {
        strcpy(local_34, "Your team is the only team alive!");
    }
    else {
        FUN_004644af(local_34, (const unsigned char *)"Team %d is the only team alive.",
                     (int)((unsigned char)DAT_004892a4 & 0xFF));
    }

    int y_off;
    if (DAT_004806d8 < 0x100) {
        y_off = 0x46;
    } else {
        y_off = (DAT_004806d8 > 399) ? 0x23 : (0x23 + 0x0A);
    }

    int dest = param_1 + ((DAT_004806e4 - y_off + DAT_004806e8) * param_2 + DAT_004806ec) * 2 + 8;
    Draw_Text_To_Buffer(local_34, 1, 5, (unsigned short *)dest, param_2, 0, DAT_004806d8 - 0x12, 0);
}

/* ===== FUN_00409280 - Timer display (00409280) ===== */
/* Shows round countdown timer as "M:SS" in top-right of viewport.
 * Multiplayer mode (DAT_0048764a != 0) shows 3 stat fields instead. */
void FUN_00409280(int param_1, int param_2)
{
    char local_24[12];
    char local_18[12];
    char local_c[12];

    int base_dest = param_1 + ((DAT_004806e8 + 0x23) * param_2 + DAT_004806ec + DAT_004806d8) * 2;

    if (DAT_0048764a != 0) {
        /* Multiplayer: 3 separate stat fields */
        FUN_004644af(local_24, (const unsigned char *)"%d:", DAT_004892a8);
        FUN_004644af(local_18, (const unsigned char *)":%s%d", "", DAT_004892ac);
        FUN_004644af(local_c, (const unsigned char *)":%s%d", "", 0);
        Draw_Text_To_Buffer(local_24, 1, 9, (unsigned short *)(base_dest - 0xDC),
                            param_2, 0, DAT_004806d8 - 0x12, 0);
        Draw_Text_To_Buffer(local_18, 1, 9, (unsigned short *)(base_dest - 0xA0),
                            param_2, 0, DAT_004806d8 - 0x12, 0);
        Draw_Text_To_Buffer(local_c, 1, 9, (unsigned short *)(base_dest - 100),
                            param_2, 0, DAT_004806d8 - 0x12, 0);
        return;
    }

    /* Single player: format as "M:SS" */
    int val = DAT_004892a8 - 1;
    int seconds = (val % 0x3F) * 100 / 0x3F;

    FUN_004644af(local_24, (const unsigned char *)"%d:", val / 0x3F);

    /* Pad with leading zero if < 10 */
    if (seconds < 10) {
        strcat(local_24, "0");
    }

    FUN_004644af(local_18, (const unsigned char *)"%d", seconds);
    strcat(local_24, local_18);

    Draw_Text_To_Buffer(local_24, 1, 9, (unsigned short *)(base_dest - 0xA0),
                        param_2, 0, DAT_004806d8 - 0x12, 0);
}

/* ===== FUN_00408f90 - Minimap dot helper (00408F90) ===== */
/* Draws a 3x3 cross pattern using LUT-blended colors for minimap player dots.
 * param_1 low byte = palette index into DAT_004876a4/a8 arrays.
 * Pattern: [edge, fill, edge] / [fill, fill, fill] / [edge, fill, edge] */
void FUN_00408f90(unsigned int param_1, unsigned short *param_2, int param_3)
{
    unsigned int idx = param_1 & 0xFF;
    if (idx + 1 >= 100) return;  /* Bounds check on DAT_004876a4 array */

    unsigned short *edge_lut = (unsigned short *)DAT_004876a4[idx];
    unsigned short *fill_lut = (unsigned short *)DAT_004876a4[idx + 1];
    unsigned short *remap = (unsigned short *)DAT_00489230;
    if (!edge_lut || !fill_lut || !remap) return;

    /* Row 0: edge, fill, edge */
    param_2[0] = edge_lut[(unsigned int)remap[param_2[0]]];
    param_2[1] = fill_lut[(unsigned int)remap[param_2[1]]];
    param_2[2] = edge_lut[(unsigned int)remap[param_2[2]]];

    /* Row 1: fill, fill, fill */
    unsigned short *row1 = param_2 + param_3;
    row1[0] = fill_lut[(unsigned int)remap[row1[0]]];
    row1[1] = fill_lut[(unsigned int)remap[row1[1]]];
    row1[2] = fill_lut[(unsigned int)remap[row1[2]]];

    /* Row 2: edge, fill, edge */
    unsigned short *row2 = row1 + param_3;
    row2[0] = edge_lut[(unsigned int)remap[row2[0]]];
    row2[1] = fill_lut[(unsigned int)remap[row2[1]]];
    row2[2] = edge_lut[(unsigned int)remap[row2[2]]];
}

/* ===== FUN_004090e0 - Minimap/radar (004090E0) ===== */
/* Draws a 65x65 semi-transparent radar overlay at viewport top-left.
 * Shows grid lines every 8 pixels and player positions as 3x3 dots.
 * Friend/foe distinguished by palette (same team = 0x0C, different = 0x10). */
void FUN_004090e0(int param_1, int param_2, unsigned int param_3)
{
    if (0x0C >= 100) return;  /* Sanity */
    unsigned short *blend_lut = (unsigned short *)DAT_004876a4[0x0C];
    unsigned short *remap = (unsigned short *)DAT_00489230;
    if (!blend_lut || !remap) return;

    /* Draw horizontal grid lines (9 lines, 65 pixels each, every 8 rows) */
    int y_off = 0;
    do {
        unsigned short *p = (unsigned short *)
            (param_1 + ((DAT_004806e4 - 6 - y_off + DAT_004806e8) * param_2 + DAT_004806ec) * 2 + 0x0C);
        int count = 0x41;
        do {
            *p = blend_lut[(unsigned int)remap[*p]];
            p++;
            count--;
        } while (count != 0);
        y_off += 8;
    } while (y_off < 0x48);

    /* Draw vertical grid lines (9 lines, 65 pixels each, every 8 columns) */
    int x_off = 0;
    do {
        unsigned short *p = (unsigned short *)
            (param_1 + ((DAT_004806e8 + DAT_004806e4 - 0x46) * param_2 + x_off + DAT_004806ec) * 2 + 0x0C);
        int count = 0x41;
        do {
            *p = blend_lut[(unsigned int)remap[*p]];
            p += param_2;
            count--;
        } while (count != 0);
        x_off += 8;
    } while (x_off < 0x48);

    /* Draw player dots */
    int player_base = DAT_00487810;
    int poff = 0;
    for (int i = 0; i < DAT_00489240; i++) {
        /* Skip dead players */
        if (*(char *)(player_base + 0x24 + poff) == '\0') {
            /* Compute relative position (22-bit shift = divide by ~4M, giving tile-scale offset) */
            int ref_off = param_3 * 0x598;
            int dx = (*(int *)(player_base + poff) - *(int *)(player_base + ref_off)) >> 0x16;
            int dy = (*(int *)(player_base + 4 + poff) - *(int *)(player_base + ref_off + 4)) >> 0x16;

            /* Palette: 0x0C for same team, 0x10 for different team */
            unsigned int palette = 0x0C;
            if (*(char *)(player_base + 0x2C + poff) != *(char *)(player_base + ref_off + 0x2C)) {
                palette = 0x10;
            }

            /* Only draw if within radar range (-0x21 to +0x20) */
            if (dx > -0x21 && dx < 0x21 && dy > -0x21 && dy < 0x21) {
                unsigned short *dot_pos = (unsigned short *)
                    (param_1 + 0x4A +
                     ((DAT_004806e8 + DAT_004806e4 - 0x27 + dy) * param_2 + DAT_004806ec + dx) * 2);
                FUN_00408f90(palette, dot_pos, param_2);
                player_base = DAT_00487810;  /* Reload after potential side effects */
            }
        }
        poff += 0x598;
    }
}

/* ===== FUN_0040b860 - Health bar (0040B860) ===== */
/* Draws a vertical health bar on the right side of the viewport.
 * Bar height proportional to health, color changes based on health percentage:
 *   >40% = green (LUT 0x0C), >20% = yellow (LUT 0x0E), <=20% = red (LUT 0x10).
 * Uses LUT-blended pixel drawing: reads existing pixel, maps through
 * DAT_00489230 (brightness remap), then looks up colored value in palette LUT. */
void FUN_0040b860(int param_1, int param_2, int param_3)
{
    int health = *(int *)(DAT_00487810 + 0x20 + param_3 * 0x598);
    if (health <= 0) return;

    /* Max bar height = viewport height - 50 (0x32) */
    int max_bar = DAT_004806e4 - 50;
    if (max_bar <= 0) return;

    /* Compute bar height: ratio of current health to max health, scaled to pixels.
     * max_health from ship stats table: DAT_0048780c + player * 0x40 + 0x28 */
    int max_health = *(int *)((int)DAT_0048780c + param_3 * 0x40 + 0x28);
    if (max_health <= 0) max_health = 1;
    int bar_h = (int)((float)max_bar * (float)health / (float)max_health);
    if (health > 0 && bar_h < 1) bar_h = 1;
    if (bar_h > max_bar) bar_h = max_bar;
    if (bar_h <= 0) return;

    /* Color threshold based on health ratio */
    int lut_idx;
    float ratio = (float)health / (float)max_health;
    if (ratio > 0.4f) {
        lut_idx = 0x0C;  /* Healthy: green */
    } else if (ratio > 0.2f) {
        lut_idx = 0x0E;  /* Medium: yellow */
    } else {
        lut_idx = 0x10;  /* Critical: red */
    }

    /* Validate LUT pointers */
    if (lut_idx + 1 >= 100) return;
    unsigned short *edge_lut = (unsigned short *)DAT_004876a4[lut_idx];
    unsigned short *fill_lut = (unsigned short *)DAT_004876a4[lut_idx + 1];
    unsigned short *remap = (unsigned short *)DAT_00489230;
    if (!edge_lut || !fill_lut || !remap) return;

    /* Bottom cap: 4 pixels wide at base position */
    {
        int base_x = DAT_004806d8 + DAT_004806ec;
        int base_y = DAT_004806e8 + DAT_004806e4 - 9;
        unsigned short *p = (unsigned short *)(param_1 + (base_y * param_2 + base_x) * 2 - 0x0C);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = edge_lut[(unsigned int)remap[p[1]]];
        p[2] = edge_lut[(unsigned int)remap[p[2]]];
        p[3] = edge_lut[(unsigned int)remap[p[3]]];
    }

    /* Bar body: 6 pixels wide [edge, fill, fill, fill, fill, edge] growing upward */
    for (int i = 0; i < bar_h; i++) {
        int row_y = DAT_004806e8 + DAT_004806e4 - 10 - i;
        int row_x = DAT_004806d8 + DAT_004806ec;
        unsigned short *p = (unsigned short *)(param_1 + (row_y * param_2 + row_x) * 2 - 0x0E);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = fill_lut[(unsigned int)remap[p[1]]];
        p[2] = fill_lut[(unsigned int)remap[p[2]]];
        p[3] = fill_lut[(unsigned int)remap[p[3]]];
        p[4] = fill_lut[(unsigned int)remap[p[4]]];
        p[5] = edge_lut[(unsigned int)remap[p[5]]];
    }

    /* Top cap: 4 pixels wide */
    {
        int cap_y = DAT_004806e8 + DAT_004806e4 - 10 - bar_h;
        int cap_x = DAT_004806d8 + DAT_004806ec;
        unsigned short *p = (unsigned short *)(param_1 + (cap_y * param_2 + cap_x) * 2 - 0x0C);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = edge_lut[(unsigned int)remap[p[1]]];
        p[2] = edge_lut[(unsigned int)remap[p[2]]];
        p[3] = edge_lut[(unsigned int)remap[p[3]]];
    }
}

/* ===== FUN_0040b580 - Shield/energy bar (0040B580) ===== */
/* Same structure as health bar but fixed color, positioned 7 pixels further left.
 * Uses DAT_004876a4[1] (edge) and DAT_004876a4[3] (fill) LUTs.
 * Only drawn when shield system is enabled (DAT_00483742 != 0 in caller).
 * Bar height = player[+0x98] / DAT_00483830 * (viewport_height - 50). */
void FUN_0040b580(int param_1, int param_2, int param_3)
{
    int shield_val = *(int *)(DAT_00487810 + param_3 * 0x598 + 0x98);
    if (DAT_00483830 == 0) return;

    int bar_h = (int)((double)shield_val / (double)DAT_00483830 * (double)(DAT_004806e4 - 50));
    if (bar_h <= 0) return;

    /* Clamp to viewport bounds */
    int max_bar = DAT_004806e4 - 18;
    if (max_bar <= 0) return;
    if (bar_h > max_bar) bar_h = max_bar;

    /* Validate LUT pointers */
    unsigned short *edge_lut = (unsigned short *)DAT_004876a4[1];
    unsigned short *fill_lut = (unsigned short *)DAT_004876a4[3];
    unsigned short *remap = (unsigned short *)DAT_00489230;
    if (!edge_lut || !fill_lut || !remap) return;

    /* Bottom cap: 4 pixels wide */
    {
        int base_x = DAT_004806d8 + DAT_004806ec;
        int base_y = DAT_004806e8 + DAT_004806e4 - 9;
        unsigned short *p = (unsigned short *)(param_1 + (base_y * param_2 + base_x) * 2 - 0x1a);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = edge_lut[(unsigned int)remap[p[1]]];
        p[2] = edge_lut[(unsigned int)remap[p[2]]];
        p[3] = edge_lut[(unsigned int)remap[p[3]]];
    }

    /* Bar body: 6 pixels wide [edge, fill, fill, fill, fill, edge] growing upward */
    for (int i = 0; i < bar_h; i++) {
        int row_y = DAT_004806e8 + DAT_004806e4 - 10 - i;
        int row_x = DAT_004806d8 + DAT_004806ec;
        unsigned short *p = (unsigned short *)(param_1 + (row_y * param_2 + row_x) * 2 - 0x1c);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = fill_lut[(unsigned int)remap[p[1]]];
        p[2] = fill_lut[(unsigned int)remap[p[2]]];
        p[3] = fill_lut[(unsigned int)remap[p[3]]];
        p[4] = fill_lut[(unsigned int)remap[p[4]]];
        p[5] = edge_lut[(unsigned int)remap[p[5]]];
    }

    /* Top cap: 4 pixels wide */
    {
        int cap_y = DAT_004806e8 + DAT_004806e4 - 10 - bar_h;
        int cap_x = DAT_004806d8 + DAT_004806ec;
        unsigned short *p = (unsigned short *)(param_1 + (cap_y * param_2 + cap_x) * 2 - 0x1a);
        p[0] = edge_lut[(unsigned int)remap[p[0]]];
        p[1] = edge_lut[(unsigned int)remap[p[1]]];
        p[2] = edge_lut[(unsigned int)remap[p[2]]];
        p[3] = edge_lut[(unsigned int)remap[p[3]]];
    }
}

/* ===== FUN_0040aaf0 - Weapon icon sprite (0040AAF0) ===== */
/* Draws a weapon icon centered at (param_3, param_4).
 * Two sprites overlaid: background frame + weapon-specific icon.
 * param_5 = weapon type index (into entity type table for icon sprite).
 * param_6: 0=selected (sprite 0xC8), 1=normal (0xD4), 2=empty (0xEF), else=0xEE */
void FUN_0040aaf0(int param_1, int param_2, int param_3, int param_4,
                  int param_5, char param_6)
{
    if (!DAT_00487ab4 || !DAT_00489234 || !DAT_00489e8c || !DAT_00489e88) return;

    /* Select background frame sprite */
    int frame_spr;
    if (param_6 == 0) {
        frame_spr = 200;  /* 0xC8 = selected */
    } else if (param_6 == 1) {
        frame_spr = 0xD4;  /* normal */
    } else {
        frame_spr = (param_6 != 2) ? 0xEE : 0xEF;  /* 2=empty, else=other */
    }

    unsigned short *spr_pixels = (unsigned short *)DAT_00487ab4;

    /* Draw background frame sprite */
    {
        int spr_off = ((int *)DAT_00489234)[frame_spr];
        int spr_w = (int)((unsigned char *)DAT_00489e8c)[frame_spr];
        int spr_h = (int)((unsigned char *)DAT_00489e88)[frame_spr];

        if (spr_w > 0 && spr_h > 0) {
            int dst_x = param_3 - (spr_w >> 1);
            int dst_y = param_4 - (spr_h >> 1);
            /* Bounds check: skip if entirely off-screen */
            if (dst_x + spr_w > 0 && dst_x < 640 && dst_y + spr_h > 0 && dst_y < 480) {
                unsigned short *dst = (unsigned short *)
                    (param_1 + (dst_y * param_2 + dst_x) * 2);
                int skip = param_2 - spr_w;
                int src_idx = spr_off;

                for (int y = 0; y < spr_h; y++) {
                    for (int x = 0; x < spr_w; x++) {
                        unsigned short pixel = spr_pixels[src_idx];
                        if (pixel != 0) {
                            *dst = pixel;
                        }
                        src_idx++;
                        dst++;
                    }
                    dst += skip;
                }
            }
        }
    }

    /* Draw weapon-specific icon sprite (from entity type table) */
    if (DAT_00487abc) {
        int weapon_spr = *(int *)((char *)DAT_00487abc + 0x144 + param_5 * 0x218);
        if (weapon_spr <= 0) return;

        int spr_off = ((int *)DAT_00489234)[weapon_spr];
        int spr_w = (int)((unsigned char *)DAT_00489e8c)[weapon_spr];
        int spr_h = (int)((unsigned char *)DAT_00489e88)[weapon_spr];

        if (spr_w > 0 && spr_h > 0) {
            int dst_x = param_3 - (spr_w >> 1);
            int dst_y = param_4 - (spr_h >> 1);
            if (dst_x + spr_w > 0 && dst_x < 640 && dst_y + spr_h > 0 && dst_y < 480) {
                unsigned short *dst = (unsigned short *)
                    (param_1 + (dst_y * param_2 + dst_x) * 2);
                int skip = param_2 - spr_w;
                int src_idx = spr_off;

                for (int y = 0; y < spr_h; y++) {
                    for (int x = 0; x < spr_w; x++) {
                        unsigned short pixel = spr_pixels[src_idx];
                        if (pixel != 0) {
                            *dst = pixel;
                        }
                        src_idx++;
                        dst++;
                    }
                    dst += skip;
                }
            }
        }
    }
}

/* ===== FUN_0040a710 - Ammo dots (0040A710) ===== */
/* Draws orbital dots around the weapon icon using sin/cos lookup table.
 * param_3, param_4 = center position.
 * param_5 = loaded ammo count (bright dots 0x6739).
 * param_6 = total capacity (remaining = dim dots 0x2108).
 * Each dot is a 3x4 bordered pixel block. */
void FUN_0040a710(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
    if (param_6 <= 0) return;
    if (!DAT_00487ab0) return;

    int *sincos = (int *)DAT_00487ab0;
    int lut_pos = 0x755;  /* Starting position in sin/cos table */

    /* Draw loaded ammo dots (bright) */
    for (int i = 0; i < param_5; i++) {
        lut_pos -= 0x80;
        if (lut_pos < 0) break;  /* Safety: don't read negative indices */

        int dx = sincos[lut_pos] * 0x12 >> 0x13;
        int dy = sincos[lut_pos + 0x200] * 0x12 >> 0x13;

        int px = dx + param_3;
        int py = dy + param_4;
        /* Bounds check: dot is 3 wide, 3 tall, with p[-1] access */
        if (px < 2 || px >= 638 || py < 0 || py + 2 >= 480) continue;

        unsigned short *p = (unsigned short *)
            (param_1 + (py * param_2 + px) * 2);

        /* Row 0: [black, black] */
        p[0] = 0;
        p[1] = 0;
        /* Row 1: [black, bright, bright, black] */
        p += param_2;
        p[-1] = 0;
        p[0] = 0xCE59; /* gray: X1R5G5B5 0x6739 → RGB565 */
        p[1] = 0xCE59;
        p[2] = 0;
        /* Row 2: [black, black] */
        p += param_2;
        p[0] = 0;
        p[1] = 0;
    }

    /* Draw remaining capacity dots (dim) */
    int remaining = param_6 - param_5;
    for (int i = 0; i < remaining; i++) {
        lut_pos -= 0x80;
        if (lut_pos < 0) break;

        int dx = sincos[lut_pos] * 0x12 >> 0x13;
        int dy = sincos[lut_pos + 0x200] * 0x12 >> 0x13;

        int px = dx + param_3;
        int py = dy + param_4;
        if (px < 2 || px >= 638 || py < 0 || py + 2 >= 480) continue;

        unsigned short *p = (unsigned short *)
            (param_1 + (py * param_2 + px) * 2);

        p[0] = 0;
        p[1] = 0;
        p += param_2;
        p[-1] = 0;
        p[0] = 0x2108;
        p[1] = 0x2108;
        p[2] = 0;
        p += param_2;
        p[0] = 0;
        p[1] = 0;
    }
}

/* ===== FUN_0040a9e0 - Weapon grid (0040A9E0) ===== */
/* Displays 3-row x 6-column weapon selection grid centered on viewport.
 * Each cell shows a weapon icon via FUN_0040aaf0.
 * Current weapon highlighted (param_6=0 = selected). */
void FUN_0040a9e0(int param_1, int param_2, int param_3)
{
    int poff = param_3 * 0x598;
    int cur_slot = (int)*(char *)(DAT_00487810 + 0x34 + poff);
    int total_slots = *(int *)(DAT_00487810 + 0x38 + poff);

    if (total_slots <= 0) return;

    /* Compute starting slot: current slot's row - 2 rows */
    int start_row = cur_slot / 6 - 2;
    int slot = start_row * 6;

    /* Wrap negative indices */
    if (slot < 0) {
        int rows = total_slots / 6;
        if (rows <= 0) rows = 1;
        slot += rows * 6;
        if (slot < 0) slot += rows * 6;
        if (slot < 0) slot = 0;
    }

    /* Draw 3 rows x up to 6 columns */
    int y_pos = -0x48;
    do {
        if (total_slots > 5 || y_pos == 0 || total_slots > 5) {
            int x_col = 0;
            do {
                if (slot >= 0 && slot < total_slots) {
                    unsigned char weapon_type = *(unsigned char *)(DAT_00487810 + poff + 0x3C + slot);
                    char selected = (slot == cur_slot) ? 0 : 1;

                    FUN_0040aaf0(param_1, param_2,
                                 DAT_004806d8 / 2 + x_col - 0x5A + DAT_004806ec,
                                 DAT_004806e4 / 2 + y_pos + DAT_004806e8,
                                 (int)weapon_type, selected);
                }

                slot++;
                if (slot >= total_slots) {
                    slot = 0;
                }
                x_col += 0x24;
            } while (x_col < 0xD8);
        }
        y_pos += 0x24;
    } while (y_pos <= 0x6B);
}

/* ===== FUN_004095e0 - Fog of War raycasting + rendering (004095E0) ===== */
/* Per-player visibility system. Casts rays from player to viewport edges
 * in 4 directions, accumulating darkness from wall hits. Then applies the
 * visibility map to the framebuffer using one of 3 rendering modes.
 *
 * param_1 = framebuffer base address
 * param_2 = stride (pixels per row)
 * param_3 = player index (0-3) */
void FUN_004095e0(unsigned int param_1, int param_2, int param_3)
{
    unsigned int step = (unsigned int)(unsigned char)DAT_0048372e;
    int ent_off = DAT_004877f8[param_3] * 0x598;
    int player_x = (*(int *)(ent_off + DAT_00487810) >> 0x12) - DAT_004806dc;
    int player_y = (*(int *)(ent_off + 4 + DAT_00487810) >> 0x12) - DAT_004806e0;
    int vp_w = DAT_004806d8;
    int vp_h = DAT_004806e4;
    int stride_ext = vp_w + 0x20;
    unsigned char *vis_buf = (unsigned char *)DAT_00489eac[param_3];

    /* Scale factor for distance: 0.9f normally, 2.0f for mode 2 */
    float scale = 0.9f;
    int threshold = 0x200;
    if (DAT_0048372d == '\x02') {
        scale = 2.0f;
        threshold = 0xF3C;
    }

    /* Skip raycasting if player is in certain states */
    char state = *(char *)(ent_off + 0x4a0 + DAT_00487810);
    if (state != '\0' && state != (char)-1)
        goto apply_rendering;

    if (!vis_buf)
        goto apply_rendering;

    /* ---- Quadrant 1 & 2: vertical rays (column by column) ---- */
    {
        int col = 0;
        int col_fx = 0;
        int col_step_fx = step * 0x40000;

        while (col <= vp_w) {
            /* Quadrant 1: UP from player (rows 0..player_y-1) */
            if (player_y != 0) {
                int px_fx = player_x << 0x12;
                int slope = (col_fx - px_fx) / player_y;
                int slope2 = (col_fx - px_fx + col_step_fx) / player_y;
                int slope12 = slope >> 0xC;
                int dist = (int)(sqrtf((float)(slope12 * slope12 + 0x1000)) * scale);

                /* Optional wobble */
                if (DAT_00483730 != '\0') {
                    int angle = FUN_004257e0(0, 0, slope12, -0x40);
                    int *sinlut = (int *)DAT_00487ab0;
                    unsigned int idx;

                    idx = (DAT_00487788[2] + angle * 5) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w1 = sinlut[idx] / 120000;

                    idx = (DAT_00487788[3] + angle * 0xC) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w2 = sinlut[idx] / 150000;

                    idx = (DAT_00487788[0] + angle * 0xE) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w3 = sinlut[idx] / 150000;

                    idx = (DAT_00487788[1] + angle * 9) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w4 = sinlut[idx] / 160000;

                    dist = dist + w1 + w2 + w3 + w4;
                }

                int darkness = 0;
                int hit_wall = 0;
                int accum = 0;
                int scan_y = (player_y - 1) * stride_ext;
                unsigned char vis_val = 0;
                int ray_fx = px_fx;

                for (int row = 0; row < player_y; row++) {
                    ray_fx += slope;
                    int ray_fx2_next = px_fx + (row + 1) * slope;
                    (void)ray_fx2_next;

                    if ((row & 2) != 0) {
                        int ty = (*(int *)(ent_off + 4 + DAT_00487810) >> 0x12) - row;
                        int tx = (*(int *)(ent_off + DAT_00487810) + ray_fx) >> 0x12;
                        int tile_idx = (ty << ((unsigned char)DAT_00487a18 & 0x1f)) + tx - player_x;
                        unsigned char tile = *(unsigned char *)(tile_idx + (int)DAT_0048782c);
                        accum += dist;

                        if (accum < 0x201 || *(char *)(tile * 0x20 + (int)DAT_00487928) != '\0') {
                            if (hit_wall) darkness += dist;
                        } else {
                            if (*(char *)(tile * 0x20 + 4 + (int)DAT_00487928) != '\0') {
                                if (accum > threshold) darkness += dist >> 1;
                                if (hit_wall) darkness += dist;
                            } else {
                                hit_wall = 1;
                                darkness += dist;
                            }
                        }

                        int level = (darkness >> 6) - 10;
                        if (level < 0) vis_val = 0;
                        else if (level >= 0x11) vis_val = 0x11;
                        else vis_val = (unsigned char)level;
                    }

                    /* Fill visibility row */
                    int fill_start = (ray_fx >> 0x12) + scan_y;
                    scan_y -= stride_ext;
                    int px_fx2 = px_fx + (row + 1) * slope2;
                    int fill_count = ((px_fx2 - ray_fx) >> 0x12) + 1;
                    for (int f = 0; f < fill_count; f++) {
                        vis_buf[fill_start + f] = vis_val;
                    }
                }
            }

            /* Quadrant 2: DOWN from player (rows player_y..vp_h-1) */
            {
                int below = vp_h - player_y;
                if (below != 0) {
                    int px_fx = player_x << 0x12;
                    int slope = (col_fx - (player_x << 0x12)) / below;
                    int slope2 = (col_fx - (player_x << 0x12) + col_step_fx) / below;
                    int slope12 = slope >> 0xC;
                    int dist = (int)(sqrtf((float)(slope12 * slope12 + 0x1000)) * scale);

                    if (DAT_00483730 != '\0') {
                        int angle = FUN_004257e0(0, 0, slope12, 0x40);
                        int *sinlut = (int *)DAT_00487ab0;
                        unsigned int idx;

                        idx = (DAT_00487788[2] + angle * 5) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w1 = sinlut[idx] / 120000;

                        idx = (DAT_00487788[3] + angle * 0xC) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w2 = sinlut[idx] / 150000;

                        idx = (DAT_00487788[0] + angle * 0xE) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w3 = sinlut[idx] / 150000;

                        idx = (DAT_00487788[1] + angle * 9) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w4 = sinlut[idx] / 160000;

                        dist = dist + w1 + w2 + w3 + w4;
                    }

                    int scan_y = stride_ext * player_y;
                    unsigned char vis_val = 0;
                    int darkness = 0;
                    int hit_wall = 0;
                    int accum = 0;
                    int ray_fx = px_fx;
                    int ray2_fx = px_fx;

                    for (int row = 0; row < below; row++) {
                        ray_fx += slope;
                        ray2_fx += slope2;

                        if ((row & 2) != 0) {
                            int ty = (*(int *)(ent_off + 4 + DAT_00487810) >> 0x12) + row;
                            int tx = (*(int *)(ent_off + DAT_00487810) + ray2_fx) >> 0x12;
                            int tile_idx = (ty << ((unsigned char)DAT_00487a18 & 0x1f)) + tx - player_x;
                            unsigned char tile = *(unsigned char *)(tile_idx + (int)DAT_0048782c);
                            accum += dist;

                            if (accum < 0x201 || *(char *)(tile * 0x20 + (int)DAT_00487928) != '\0') {
                                if (hit_wall) darkness += dist;
                            } else {
                                if (*(char *)(tile * 0x20 + 4 + (int)DAT_00487928) != '\0') {
                                    if (accum > threshold) darkness += dist >> 1;
                                    if (hit_wall) darkness += dist;
                                } else {
                                    hit_wall = 1;
                                    darkness += dist;
                                }
                            }

                            int level = (darkness >> 6) - 10;
                            if (level < 0) vis_val = 0;
                            else if (level >= 0x11) vis_val = 0x11;
                            else vis_val = (unsigned char)level;
                        }

                        int fill_start = (ray_fx >> 0x12) + scan_y;
                        scan_y += stride_ext;
                        int fill_count = ((ray2_fx - ray_fx) >> 0x12) + 1;
                        for (int f = 0; f < fill_count; f++) {
                            vis_buf[fill_start + f] = vis_val;
                        }
                    }
                }
            }

            col += step;
            col_fx += col_step_fx;
        }
    }

    /* ---- Quadrant 3 & 4: horizontal rays (row by row) ---- */
    {
        int row = 0;
        int row_fx = 0;
        int row_step_fx = step * 0x40000;

        while (row < vp_h) {
            /* Quadrant 3: LEFT from player (cols 0..player_x-1) */
            if (player_x != 0) {
                int py_fx = player_y << 0x12;
                int slope = (row_fx - py_fx) / player_x;
                int slope2 = (row_fx - py_fx + row_step_fx) / player_x;
                int slope12 = slope >> 0xC;
                int dist = (int)(sqrtf((float)(slope12 * slope12 + 0x1000)) * scale);

                if (DAT_00483730 != '\0') {
                    int angle = FUN_004257e0(0, 0, -0x40, slope12);
                    int *sinlut = (int *)DAT_00487ab0;
                    unsigned int idx;

                    idx = (DAT_00487788[3] + angle * 0xC) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w1 = sinlut[idx] / 150000;

                    idx = (DAT_00487788[0] + angle * 0xE) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w2 = sinlut[idx] / 150000;

                    idx = (DAT_00487788[1] + angle * 9) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w3 = sinlut[idx] / 160000;

                    idx = (DAT_00487788[2] + angle * 5) & 0x800007ff;
                    if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                    int w4 = sinlut[idx] / 120000;

                    dist = dist + w1 + w2 + w3 + w4;
                }

                unsigned char vis_val = 0;
                int darkness = 0;
                int hit_wall = 0;
                int accum = 0;
                int ray_fx = py_fx;
                int ray2_fx = py_fx;

                for (int c = 0; c < player_x; c++) {
                    ray2_fx += slope;
                    ray_fx += slope2;

                    if ((c & 2) != 0) {
                        int ty = (*(int *)(ent_off + 4 + DAT_00487810) + ray_fx) >> 0x12;
                        int dy = ty - player_y;
                        if (dy < DAT_004879f4 && dy > 0) {
                            int tx = (*(int *)(ent_off + DAT_00487810) >> 0x12) - c;
                            int tile_idx = (dy << ((unsigned char)DAT_00487a18 & 0x1f)) + tx;
                            unsigned char tile = *(unsigned char *)(tile_idx + (int)DAT_0048782c);
                            (void)tile; /* tile used for wall check */
                        }
                        accum += dist;

                        if (accum < 0x201 || *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c +
                            ((((*(int *)(ent_off + 4 + DAT_00487810) + ray_fx) >> 0x12) - player_y) <<
                            ((unsigned char)DAT_00487a18 & 0x1f)) +
                            ((*(int *)(ent_off + DAT_00487810) >> 0x12) - c)) * 0x20 + (int)DAT_00487928) != '\0') {
                            if (hit_wall) darkness += dist;
                        } else {
                            char *ent = (char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c +
                                ((((*(int *)(ent_off + 4 + DAT_00487810) + ray_fx) >> 0x12) - player_y) <<
                                ((unsigned char)DAT_00487a18 & 0x1f)) +
                                ((*(int *)(ent_off + DAT_00487810) >> 0x12) - c)) * 0x20 + (int)DAT_00487928);
                            if (ent[4] != '\0') {
                                if (accum > threshold) darkness += dist >> 1;
                                if (hit_wall) darkness += dist;
                            } else {
                                hit_wall = 1;
                                darkness += dist;
                            }
                        }

                        int level = (darkness >> 6) - 10;
                        if (level < 0) vis_val = 0;
                        else if (level >= 0x11) vis_val = 0x11;
                        else vis_val = (unsigned char)level;
                    }

                    /* Fill visibility column */
                    int fill_pos = ((ray2_fx >> 0x12) * stride_ext) - c - 1 + player_x;
                    int fill_count = ((ray_fx - ray2_fx) >> 0x12) + 1;
                    for (int f = 0; f < fill_count; f++) {
                        vis_buf[fill_pos + (f * stride_ext)] = vis_val;
                    }
                }
            }

            /* Quadrant 4: RIGHT from player (cols player_x..vp_w-1) */
            {
                int right = vp_w - player_x;
                if (right != 0) {
                    int py_fx = player_y << 0x12;
                    int slope = (row_fx - py_fx) / right;
                    int slope2 = (row_fx - py_fx + row_step_fx) / right;
                    int slope12 = slope >> 0xC;
                    int dist = (int)(sqrtf((float)(slope12 * slope12 + 0x1000)) * scale);

                    if (DAT_00483730 != '\0') {
                        int angle = FUN_004257e0(0, 0, 0x40, slope12);
                        int *sinlut = (int *)DAT_00487ab0;
                        unsigned int idx;

                        idx = (DAT_00487788[3] + angle * 0xC) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w1 = sinlut[idx] / 150000;

                        idx = (DAT_00487788[0] + angle * 0xE) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w2 = sinlut[idx] / 150000;

                        idx = (DAT_00487788[1] + angle * 9) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w3 = sinlut[idx] / 160000;

                        idx = (DAT_00487788[2] + angle * 5) & 0x800007ff;
                        if ((int)idx < 0) idx = ((idx - 1) | 0xfffff800) + 1;
                        int w4 = sinlut[idx] / 120000;

                        dist = dist + w1 + w2 + w3 + w4;
                    }

                    unsigned char vis_val = 0;
                    int darkness = 0;
                    int hit_wall = 0;
                    int accum = 0;
                    int ray_fx = py_fx;
                    int ray2_fx = py_fx;

                    for (int c = 0; c < right; c++) {
                        ray_fx += slope;
                        ray2_fx += slope2;

                        if ((c & 2) != 0) {
                            int ty = (*(int *)(ent_off + 4 + DAT_00487810) + ray_fx) >> 0x12;
                            int dy = ty - player_y;
                            if (dy < DAT_004879f4 && dy > 0) {
                                int tx = (*(int *)(ent_off + DAT_00487810) >> 0x12) + c;
                                int tile_idx = (dy << ((unsigned char)DAT_00487a18 & 0x1f)) + tx;
                                unsigned char tile = *(unsigned char *)(tile_idx + (int)DAT_0048782c);
                                (void)tile;
                            }
                            accum += dist;

                            unsigned char tile_val = *(unsigned char *)((int)DAT_0048782c +
                                ((((*(int *)(ent_off + 4 + DAT_00487810) + ray_fx) >> 0x12) - player_y) <<
                                ((unsigned char)DAT_00487a18 & 0x1f)) +
                                ((*(int *)(ent_off + DAT_00487810) >> 0x12) + c));

                            if (accum < 0x201 || *(char *)(tile_val * 0x20 + (int)DAT_00487928) != '\0') {
                                if (hit_wall) darkness += dist;
                            } else {
                                if (*(char *)(tile_val * 0x20 + 4 + (int)DAT_00487928) != '\0') {
                                    if (accum > threshold) darkness += dist >> 1;
                                    if (hit_wall) darkness += dist;
                                } else {
                                    hit_wall = 1;
                                    darkness += dist;
                                }
                            }

                            int level = (darkness >> 6) - 10;
                            if (level < 0) vis_val = 0;
                            else if (level >= 0x11) vis_val = 0x11;
                            else vis_val = (unsigned char)level;
                        }

                        /* Fill visibility column */
                        int fill_pos = ((ray_fx >> 0x12) * stride_ext) + c + player_x;
                        int fill_count = ((ray2_fx - ray_fx) >> 0x12) + 1;
                        for (int f = 0; f < fill_count; f++) {
                            vis_buf[fill_pos + (f * stride_ext)] = vis_val;
                        }
                    }
                }
            }

            row += step;
            row_fx += row_step_fx;
        }
    }

apply_rendering:
    /* ---- Apply visibility map to framebuffer ---- */
    if (DAT_0048372d == '\x01') {
        /* Mode 1: Full fog with tile/sky substitution */
        int tilemap_row = (DAT_004806e0 << ((unsigned char)DAT_00487a18 & 0x1f)) + DAT_004806dc;
        int tilemap_row_skip = DAT_00487a00 - vp_w;
        int sky_disabled = (DAT_00483960 == '\0') ? 0 : 1;
        int sky_off = 0;

        if (!sky_disabled) {
            float ratio_x = (float)(DAT_004879f0 - vp_w) / (float)(DAT_00487a0c - vp_w);
            float ratio_y = (float)(DAT_004879f4 - vp_h) / (float)(DAT_00487a10 - vp_h);
            if (ratio_x < _DAT_004753e8_fog || ratio_y < _DAT_004753e8_fog) {
                sky_disabled = 1;
            } else {
                float r = (ratio_x < ratio_y) ? ratio_x : ratio_y;
                int sky_y = (int)((float)DAT_004806e0 / r);
                int sky_x = (int)((float)DAT_004806dc / r);
                sky_off = sky_y * DAT_00487a0c + sky_x;
            }
        }

        int vis_idx = 0;
        unsigned short *dst = (unsigned short *)(param_1 + (DAT_004806e8 * param_2 + DAT_004806ec) * 2);

        for (int y = 0; y < vp_h; y++) {
            for (int x = 0; x < vp_w; x++) {
                unsigned char vis = vis_buf[vis_idx];
                if (vis == 0x11) {
                    /* Fully dark: show tile or sky color through LUT[40] */
                    unsigned short tile_px = ((unsigned short *)DAT_00481f50)[tilemap_row];
                    unsigned short color;
                    unsigned short remapped;
                    if (tile_px != 0) {
                        color = tile_px;
                    } else if (!sky_disabled) {
                        color = ((unsigned short *)DAT_00489ea0)[sky_off];
                    } else {
                        color = *(unsigned short *)&DAT_00483820;
                        goto store_mode1;
                    }
                    /* Apply remap then darkness LUT[40] */
                    remapped = ((unsigned short *)DAT_00489230)[color];
                    color = ((unsigned short *)DAT_004876a4[40])[remapped];
                store_mode1:
                    *dst = color;
                } else if (vis != 0) {
                    /* Partial darkness: darken existing pixel */
                    unsigned short remapped = ((unsigned short *)DAT_00489230)[*dst];
                    *dst = ((unsigned short *)DAT_004876a4[32 + (vis >> 1)])[remapped];
                }
                tilemap_row++;
                dst++;
                vis_idx++;
                sky_off++;
            }
            vis_idx += 0x20;  /* stride_ext - vp_w = 0x20 */
            sky_off += (DAT_00487a0c - vp_w);
            tilemap_row += tilemap_row_skip;
            dst += (param_2 - vp_w);
        }
    } else if (DAT_0048372d == '\x02') {
        /* Mode 2: Simplified fog with tile transparency check */
        int tilemap_row = (DAT_004806e0 << ((unsigned char)DAT_00487a18 & 0x1f)) + DAT_004806dc;
        int tilemap_row_skip = DAT_00487a00 - vp_w;
        int sky_disabled = (DAT_00483960 == '\0') ? 0 : 1;
        int sky_off = 0;

        if (!sky_disabled) {
            float ratio_x = (float)(DAT_004879f0 - vp_w) / (float)(DAT_00487a0c - vp_w);
            float ratio_y = (float)(DAT_004879f4 - vp_h) / (float)(DAT_00487a10 - vp_h);
            if (ratio_x < _DAT_004753e8_fog || ratio_y < _DAT_004753e8_fog) {
                sky_disabled = 1;
            } else {
                float r = (ratio_x < ratio_y) ? ratio_x : ratio_y;
                int sky_y = (int)((float)DAT_004806e0 / r);
                int sky_x = (int)((float)DAT_004806dc / r);
                sky_off = sky_y * DAT_00487a0c + sky_x;
            }
        }

        int vis_idx = 0;
        unsigned short *dst = (unsigned short *)(param_1 + (DAT_004806e8 * param_2 + DAT_004806ec) * 2);

        for (int y = 0; y < vp_h; y++) {
            for (int x = 0; x < vp_w; x++) {
                if (vis_buf[vis_idx] == 0x11) {
                    unsigned short tile_px = ((unsigned short *)DAT_00481f50)[tilemap_row];
                    unsigned short color;
                    if (tile_px != 0) {
                        /* Check if tile entity has transparency flag */
                        unsigned char tile_type = *(unsigned char *)((int)DAT_0048782c + tilemap_row);
                        if (*(char *)(tile_type * 0x20 + 4 + (int)DAT_00487928) != '\0') {
                            /* Apply remap then LUT[39] */
                            unsigned short remapped = ((unsigned short *)DAT_00489230)[tile_px];
                            color = ((unsigned short *)DAT_004876a4[39])[remapped];
                        } else {
                            color = tile_px;
                        }
                    } else if (!sky_disabled) {
                        unsigned short sky_px = ((unsigned short *)DAT_00489ea0)[sky_off];
                        unsigned short remapped = ((unsigned short *)DAT_00489230)[sky_px];
                        color = ((unsigned short *)DAT_004876a4[39])[remapped];
                    } else {
                        color = *(unsigned short *)&DAT_00483820;
                    }
                    *dst = color;
                }
                tilemap_row++;
                dst++;
                vis_idx++;
                sky_off++;
            }
            vis_idx += 0x20;
            sky_off += (DAT_00487a0c - vp_w);
            tilemap_row += tilemap_row_skip;
            dst += (param_2 - vp_w);
        }
    } else {
        /* Mode 0/default: Simple LUT darkening */
        int vis_idx = 0;
        unsigned short *dst = (unsigned short *)(param_1 + (DAT_004806e8 * param_2 + DAT_004806ec) * 2);

        for (int y = 0; y < vp_h; y++) {
            for (int x = 0; x < vp_w; x++) {
                unsigned char vis = vis_buf[vis_idx];
                if (vis != 0) {
                    if (vis >= 0x11) {
                        *dst = *(unsigned short *)&DAT_00483820;
                    } else {
                        unsigned short remapped = ((unsigned short *)DAT_00489230)[*dst];
                        *dst = ((unsigned short *)DAT_004876a4[31 + vis])[remapped];
                    }
                }
                dst++;
                vis_idx++;
            }
            vis_idx += 0x20;
            dst += (param_2 - vp_w);
        }
    }
}
