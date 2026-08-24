/*
 * graphics.cpp - software renderer and presentation geometry
 * Address: Render_Frame=0045D800
 */
#include "tou.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFG_ADDR(a) ((int)(uintptr_t)&g_ConfigBlob[(a) - 0x481F58])

/* ===== Globals defined in this module ===== */
int                 DAT_00489238    = 640;   /* Screen/viewport width */
int                 DAT_0048923c    = 480;   /* Screen/viewport height */


/* The recovered renderer remains natively 640x480. Display settings resize
 * only its presentation viewport, keeping gameplay coordinates untouched. */
void Get_Game_Presentation_Rect(RECT *rect)
{
    RECT client = {0, 0, 640, 480};
    if (hWnd_Main != NULL)
        GetClientRect(hWnd_Main, &client);

    int client_w = client.right - client.left;
    int client_h = client.bottom - client.top;
    if (client_w <= 0 || client_h <= 0) {
        *rect = client;
        return;
    }

    int width = client_w;
    int height = (client_w * 3) / 4;
    if (height > client_h) {
        height = client_h;
        width = (client_h * 4) / 3;
    }

    rect->left = (client_w - width) / 2;
    rect->top = (client_h - height) / 2;
    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

void Client_To_Game_Coordinates(int client_x, int client_y, int *game_x, int *game_y)
{
    RECT viewport;
    Get_Game_Presentation_Rect(&viewport);
    int width = viewport.right - viewport.left;
    int height = viewport.bottom - viewport.top;
    if (width <= 0 || height <= 0) {
        *game_x = 0;
        *game_y = 0;
        return;
    }

    int x = ((client_x - viewport.left) * 640) / width;
    int y = ((client_y - viewport.top) * 480) / height;
    if (x < 0) x = 0;
    if (x > 639) x = 639;
    if (y < 0) y = 0;
    if (y > 479) y = 479;
    *game_x = x;
    *game_y = y;
}

void Apply_Display_Settings(void)
{
    if (hWnd_Main == NULL)
        return;

    unsigned int mode = DAT_00483724[1];
    if (g_NumDisplayModes <= 0 || mode >= (unsigned int)g_NumDisplayModes)
        mode = 5;

    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoA(MonitorFromWindow(hWnd_Main, MONITOR_DEFAULTTONEAREST), &monitor);

    if (g_WindowMode != 0) {
        SetWindowLongA(hWnd_Main, GWL_STYLE, WS_POPUP);
        SetWindowPos(hWnd_Main, HWND_TOP,
            monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT window_rect = {0, 0, g_ModeWidths[mode], g_ModeHeights[mode]};
        AdjustWindowRect(&window_rect, style, FALSE);
        int width = window_rect.right - window_rect.left;
        int height = window_rect.bottom - window_rect.top;
        int x = monitor.rcWork.left + ((monitor.rcWork.right - monitor.rcWork.left) - width) / 2;
        int y = monitor.rcWork.top + ((monitor.rcWork.bottom - monitor.rcWork.top) - height) / 2;

        SetWindowLongA(hWnd_Main, GWL_STYLE, style);
        SetWindowPos(hWnd_Main, HWND_NOTOPMOST, x, y, width, height,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    InvalidateRect(hWnd_Main, NULL, TRUE);
}

/* Scratch buffer for compositing particles onto RGB565 before conversion */
static unsigned short *g_ScratchBuffer = NULL;

/* ===== FUN_00408a60 — Shield cross/ring drawing primitive ===== */
/* Draws 8 line segments forming a cross shape using double LUT color transform.
 * Each pixel under the cross is remapped: pixel → DAT_00489230[pixel] → DAT_004876a4[12][remap].
 * param_1: framebuffer base, param_2: stride, param_3: arm size,
 * param_4: screen X center, param_5: screen Y center */
static void FUN_00408a60(int param_1, int param_2, int param_3, int param_4, int param_5)
{
    unsigned short *remap_lut = (unsigned short *)DAT_00489230;
    unsigned short *color_lut = (unsigned short *)DAT_004876a4[12];
    if (!remap_lut || !color_lut) return;

    int iVar5 = param_5 - param_3;
    int iVar8 = param_4 - param_3;
    int iVar1 = param_3 / 2;

    /* Segment 1: horizontal at Y=iVar5, X from iVar8 to iVar8+iVar1 */
    {
        int x0 = iVar8;      if (x0 < 0) x0 = 0;
        int x1 = iVar1 + iVar8; if (x1 > DAT_004806d8) x1 = DAT_004806d8;
        if (x0 < DAT_004806d8 && iVar5 >= 0 && iVar5 < DAT_004806e4 && x0 < x1) {
            int cnt = x1 - x0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + iVar5) * param_2 + DAT_004806ec + x0) * 2);
            do { *p = color_lut[remap_lut[*p]]; p++; } while (--cnt);
        }
    }
    /* Segment 2: horizontal at Y=(param_3+param_5), X from iVar8 to iVar8+iVar1 */
    {
        int iVar2 = param_3 + param_5;
        int x0 = iVar8;      if (x0 < 0) x0 = 0;
        int x1 = iVar1 + iVar8; if (x1 > DAT_004806d8) x1 = DAT_004806d8;
        if (x0 < DAT_004806d8 && iVar2 >= 0 && iVar2 < DAT_004806e4 && x0 < x1) {
            int cnt = x1 - x0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + iVar2) * param_2 + DAT_004806ec + x0) * 2);
            do { *p = color_lut[remap_lut[*p]]; p++; } while (--cnt);
        }
    }
    /* Segment 3: horizontal at Y=iVar5, X from (iVar1+1+param_4) */
    {
        int iVar6 = iVar1 + 1 + param_4;
        int x0 = iVar6;      if (x0 < 0) x0 = 0;
        int x1 = iVar1 + iVar6; if (x1 > DAT_004806d8) x1 = DAT_004806d8;
        if (x0 < DAT_004806d8 && iVar5 >= 0 && iVar5 < DAT_004806e4 && x0 < x1) {
            int cnt = x1 - x0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + iVar5) * param_2 + DAT_004806ec + x0) * 2);
            do { *p = color_lut[remap_lut[*p]]; p++; } while (--cnt);
        }
    }
    /* Segment 4: horizontal at Y=(param_3+param_5), X from (iVar1+1+param_4) */
    {
        int iVar2 = param_3 + param_5;
        int iVar6 = iVar1 + 1 + param_4;
        int x0 = iVar6;      if (x0 < 0) x0 = 0;
        int x1 = iVar1 + iVar6; if (x1 > DAT_004806d8) x1 = DAT_004806d8;
        if (x0 < DAT_004806d8 && iVar2 >= 0 && iVar2 < DAT_004806e4 && x0 < x1) {
            int cnt = x1 - x0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + iVar2) * param_2 + DAT_004806ec + x0) * 2);
            do { *p = color_lut[remap_lut[*p]]; p++; } while (--cnt);
        }
    }
    /* Segment 5: vertical at X=iVar8, Y from iVar5 downward */
    {
        int y0 = iVar5;      if (y0 < 0) y0 = 0;
        int y1 = iVar1 + iVar5; if (y1 > DAT_004806e4) y1 = DAT_004806e4;
        if (y0 < DAT_004806e4 && iVar8 >= 0 && iVar8 < DAT_004806d8 && y0 < y1) {
            int cnt = y1 - y0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + y0) * param_2 + DAT_004806ec + iVar8) * 2);
            do { *p = color_lut[remap_lut[*p]]; p += param_2; } while (--cnt);
        }
    }
    /* Segment 6: vertical at X=(param_3+param_4), Y from iVar5 downward */
    {
        int iVar2 = param_3 + param_4;
        int y0 = iVar5;      if (y0 < 0) y0 = 0;
        int y1 = iVar1 + iVar5; if (y1 > DAT_004806e4) y1 = DAT_004806e4;
        if (y0 < DAT_004806e4 && iVar2 >= 0 && iVar2 < DAT_004806d8 && y0 < y1) {
            int cnt = y1 - y0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + y0) * param_2 + DAT_004806ec + iVar2) * 2);
            do { *p = color_lut[remap_lut[*p]]; p += param_2; } while (--cnt);
        }
    }
    /* Segment 7: vertical at X=iVar8, Y from (iVar1+param_5) downward */
    {
        int iVar5b = iVar1 + param_5;
        int y0 = iVar5b;     if (y0 < 0) y0 = 0;
        int y1 = iVar1 + iVar5b; if (y1 > DAT_004806e4) y1 = DAT_004806e4;
        if (y0 < DAT_004806e4 && iVar8 >= 0 && iVar8 < DAT_004806d8 && y0 < y1) {
            int cnt = y1 - y0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + y0) * param_2 + DAT_004806ec + iVar8) * 2);
            do { *p = color_lut[remap_lut[*p]]; p += param_2; } while (--cnt);
        }
    }
    /* Segment 8: vertical at X=(param_3+param_4), Y from (iVar1+param_5) downward */
    {
        int iVar2 = param_3 + param_4;
        int iVar5b = iVar1 + param_5;
        int y0 = iVar5b;     if (y0 < 0) y0 = 0;
        int y1 = iVar1 + iVar5b; if (y1 > DAT_004806e4) y1 = DAT_004806e4;
        if (y0 < DAT_004806e4 && iVar2 >= 0 && iVar2 < DAT_004806d8 && y0 < y1) {
            int cnt = y1 - y0;
            unsigned short *p = (unsigned short *)(param_1 + ((DAT_004806e8 + y0) * param_2 + DAT_004806ec + iVar2) * 2);
            do { *p = color_lut[remap_lut[*p]]; p += param_2; } while (--cnt);
        }
    }
}

/* ===== FUN_00408ea0 — Spawn shield overlay effect ===== */
/* Draws 4 contracting cross shapes around a spawning entity.
 * Timer at entity+0x4A3 controls the size — starts large and shrinks to zero.
 * param_1: buffer, param_2: stride, param_3: entity index */
static void FUN_00408ea0(int param_1, int param_2, int param_3)
{
    PlayerData *player = Player_Get(param_3);
    unsigned int timer = player->timer_4a3;

    /* Screen position: entity world pos (fixed-point >> 18) minus camera offset */
    int screen_y = (player->position_y >> 0x12) - DAT_004806e0;
    int screen_x = (player->position_x >> 0x12) - DAT_004806dc;

    /* 4 crosses at different radii, all quadratic in timer */
    unsigned int r1 = (timer * timer) / 0xc;
    FUN_00408a60(param_1, param_2, (int)(r1 + 0x12), screen_x, screen_y);
    FUN_00408a60(param_1, param_2, (int)(r1 + 0x0e), screen_x, screen_y);

    timer = player->timer_4a3;
    FUN_00408a60(param_1, param_2, (int)((timer * timer) >> 3) + 0x12, screen_x, screen_y);

    timer = player->timer_4a3;
    FUN_00408a60(param_1, param_2, (int)((timer * timer) >> 4) + 0x18, screen_x, screen_y);
}

/* ===== Render_Game_World (based on FUN_00407720) ===== */
/* Renders the game world (level background) into a 640×480 RGB565 buffer.
 * Original iterates over player viewports for split-screen support;
 * since player/ship systems are stubbed, we use a single centered viewport.
 *
 * Original also calls ~10 rendering subsystems after the background blit
 * (entities, particles, shadows, HUD, etc.) — stubbed pending decompilation.
 *
 * param_1: destination RGB565 buffer (640×480)
 * param_2: buffer stride in pixels (640) */
static void Render_Game_World(Framebuffer *framebuffer)
{
    unsigned short *buffer = framebuffer->pixels;
    int stride = framebuffer->stride;
    if (!DAT_00481f50 || DAT_004879f0 == 0 || DAT_004879f4 == 0)
        return;


    unsigned short *src = (unsigned short *)DAT_00481f50;
    int shift = DAT_00487a18 & 0x1F;

    int avail_w = (int)DAT_004879f0 - 14;
    int avail_h = (int)DAT_004879f4 - 14;

    /* Number of viewports to render (1 for single player, 2+ for split-screen) */
    int num_viewports = (DAT_00487808 > 0) ? DAT_00487808 : 1;

    /* Sky pattern fill — tiles a sprite across the entire buffer ONCE as background.
     * MUST be before the viewport loop so it doesn't wipe per-viewport rendering.
     * Sky type (g_ConfigBlob[0x1803] = byte 3 of DAT_00483758):
     *   0 → sprite 0x40, 1 → sprite 0x45, 2 → sprite 0x46 (default)
     *   3 → solid color 0x446, ≥4 → black */
    {
        unsigned char sky_type = g_GameConfig.values.sky_settings_bytes[3];
        if (sky_type < 3 && DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
            int sky_sprite = (sky_type == 0) ? 0x40 : ((sky_type == 1) ? 0x45 : 0x46);
            int spr_w = (int)((unsigned char *)DAT_00489e8c)[sky_sprite];
            int spr_h = (int)((unsigned char *)DAT_00489e88)[sky_sprite];
            int spr_base = ((int *)DAT_00489234)[sky_sprite];
            unsigned short *spr_pixels = (unsigned short *)DAT_00487ab4;
            if (spr_w > 0 && spr_h > 0) {
                int src_y = spr_base;
                int src_y_end = spr_base + spr_w * spr_h;
                for (int y = 0; y < 480; y++) {
                    int src_x = src_y;
                    unsigned short *d = buffer + y * stride;
                    for (int x = 0; x < 640; x++) {
                        *d++ = spr_pixels[src_x];
                        src_x++;
                        if (src_x >= src_y + spr_w) src_x = src_y;
                    }
                    src_y += spr_w;
                    if (src_y >= src_y_end) src_y = spr_base;
                }
            } else {
                memset(buffer, 0, 640 * 480 * 2);
            }
        } else if (sky_type == 3) {
            memset(buffer, 0, 640 * 480 * 2);
        } else {
            unsigned short *d = buffer;
            for (int i = 0; i < 640 * 480; i++) *d++ = 0x0446;
        }
    }

    /* === Per-viewport rendering loop ===
     * For split-screen: renders once per active player viewport.
     * Each iteration sets viewport globals then renders all entities + HUD.
     * Entity renderers auto-clip to viewport bounds and offset to screen position. */
    for (int vp = 0; vp < num_viewports; vp++) {

    /* Compute per-player camera and viewport dimensions */
    int vp_w = 640, vp_h = 480;
    int vp_left, vp_top;
    int screen_x_off = 0, screen_y_off = 0;
    int pidx = 0;

    if (DAT_00487808 > 0 && DAT_00487810 != 0) {
        pidx = DAT_004877f8[vp];
        PlayerData *player = Player_Get(pidx);
        int player_x = player->position_x;
        int player_y = player->position_y;

        int pvp_w = player->viewport_width;
        int pvp_h = player->viewport_height;
        if (pvp_w > 0 && pvp_h > 0) {
            vp_w = pvp_w;
            vp_h = pvp_h;
        } else if (num_viewports > 1) {
            /* Fallback: split evenly if per-player dims not set */
            vp_w = 640 / num_viewports;
            vp_h = 480;
        }
        if (vp_w > avail_w) vp_w = avail_w;
        if (vp_h > avail_h) vp_h = avail_h;

        vp_left = (player_x >> 18) - vp_w / 2;
        vp_top  = (player_y >> 18) - vp_h / 2;

        /* Screen offset: each viewport placed side by side */
        screen_x_off = vp * vp_w;
        screen_y_off = 0;

        /* Screen shake */
        if (player->timer_c4 != 0) {
            vp_left += (rand() % 6) - 3;
            vp_top  += (rand() % 6) - 3;
        }
    } else {
        vp_left = ((int)DAT_004879f0 - vp_w) / 2;
        vp_top  = ((int)DAT_004879f4 - vp_h) / 2;
    }

    /* Clamp to 7-pixel border */
    if (vp_left < 7) vp_left = 7;
    if (vp_top  < 7) vp_top  = 7;
    if (vp_left + vp_w > (int)DAT_004879f0 - 7)
        vp_left = (int)DAT_004879f0 - 7 - vp_w;
    if (vp_top + vp_h > (int)DAT_004879f4 - 7)
        vp_top = (int)DAT_004879f4 - 7 - vp_h;
    if (vp_w & 1) vp_w--;

    /* Set viewport globals (all entity renderers + HUD read these) */
    DAT_004806d8 = vp_w;
    DAT_004806e4 = vp_h;
    DAT_004806dc = vp_left;
    DAT_004806e0 = vp_top;
    DAT_004806d0 = vp_left + vp_w;
    DAT_004806d4 = vp_top + vp_h;
    DAT_004806ec = screen_x_off;
    DAT_004806e8 = screen_y_off;

    /* Blit level background from stride-aligned source to screen buffer.
     * Zero pixels (0x0000) in the level background represent empty/sky areas.
     *
     * Two modes (matching original FUN_00407720 viewport blit):
     *   DAT_00483960 != 0 → FUN_0040c0a0 path: per-level sky from .SWP file
     *     composites sky image behind transparent tile pixels
     *   DAT_00483960 == 0 → direct blit, sky comes from tiled sprite fill above */
    if (DAT_00483960 != 0 && DAT_00489ea0 != NULL) {
        /* Per-level sky compositing (FUN_0040c0a0):
         * The sky image scrolls slower than the map so the backdrop feels
         * distant. Formula on each axis:
         *     sky_start = vp_pos * (sky - vp) / (map - vp)
         * so sky_start is 0 at the left/top edge of the map and (sky - vp)
         * at the right/bottom edge, exposing the full sky image as the
         * viewport traverses the map.
         *
         * Edge cases on each axis (handled independently for x and y):
         *   sky <= vp → can't scroll; sky_start stays 0 and the sky tiles
         *     (wraps with a modulo) if it's strictly smaller than the viewport.
         *   sky >  map → formula still produces valid offsets inside the
         *     sky buffer, parallax just covers a shorter visual range. */
        unsigned short *sky = (unsigned short *)DAT_00489ea0;
        int sky_w = DAT_00487a0c;
        int sky_h = DAT_00487a10;

        int sky_x_start = 0;
        int sky_y_start = 0;
        if (sky_w > vp_w && (int)DAT_004879f0 > vp_w) {
            sky_x_start = vp_left * (sky_w - vp_w) / ((int)DAT_004879f0 - vp_w);
        }
        if (sky_h > vp_h && (int)DAT_004879f4 > vp_h) {
            sky_y_start = vp_top * (sky_h - vp_h) / ((int)DAT_004879f4 - vp_h);
        }

        int tile_stride = DAT_00487a00;
        unsigned short *tile_row = src + (vp_top << ((unsigned char)DAT_00487a18 & 0x1F)) + vp_left;
        unsigned short *dst_row = buffer + (DAT_004806e8 * stride + DAT_004806ec);
        int sky_y = sky_y_start;

        for (int y = 0; y < vp_h; y++) {
            unsigned short *sky_row_ptr = sky + sky_y * sky_w;
            int sky_x = sky_x_start;
            for (int x = 0; x < vp_w; x++) {
                unsigned short tile_px = tile_row[x];
                if (tile_px == 0) {
                    dst_row[x] = sky_row_ptr[sky_x];
                } else {
                    dst_row[x] = tile_px;
                }
                sky_x++;
                if (sky_x >= sky_w) sky_x = 0;
            }
            dst_row += stride;
            tile_row += tile_stride;
            sky_y++;
            if (sky_y >= sky_h) sky_y = 0;
        }
    } else {
        /* Standard blit: copy ALL level pixels unconditionally (matching original).
         * The tiled sprite sky fill above provides the background; level data
         * overwrites it completely, including zero (black/empty) pixels. */
        for (int y = 0; y < vp_h; y++) {
            unsigned short *s = src + ((vp_top + y) << shift) + vp_left;
            unsigned short *d = buffer + (DAT_004806e8 + y) * stride + DAT_004806ec;
            memcpy(d, s, vp_w * 2);
        }
    }

    /* Entity rendering subsystems (original order from FUN_00407720) */
    FUN_0040dbd0(framebuffer);       /* Static entities (turrets) */
    FUN_0040dce0(framebuffer);       /* Dynamic entities (troopers) */
    FUN_0040bb60(framebuffer);       /* Main entities (items/ships) */
    FUN_0040a870(framebuffer);       /* Projectiles */
    FUN_0040d6c0(framebuffer);       /* Explosions */
    FUN_0040d810(framebuffer);       /* Debris/particles */
    FUN_0040caf0(framebuffer);       /* Player/ship */
    FUN_0040d930(framebuffer);       /* Misc effects (glow/smoke) */
    FUN_0040d360(framebuffer);       /* Edge tiles/detail */
    FUN_0040d100(framebuffer);       /* Particle overlay */

    /* Fog of War — raycasting visibility + darkening */
    if (DAT_0048372d != '\0' && DAT_00489eac[0] != NULL) {
        FUN_004095e0(framebuffer, 0);
    }

    /* Spawn shield overlay — draw contracting cross effect for spawning players */
    if (DAT_00487810 != 0) {
        for (int p = 0; p < DAT_00489240; p++) {
            PlayerData *player = Player_Get(p);
            unsigned char timer = player->timer_4a3;
            if (timer != 0 && timer < 0x2F &&
                player->state_24 == 0) {
                FUN_00408ea0((int)buffer, stride, p);
            }
        }
    }

    /* ---- HUD elements (per-player, only when alive) ---- */
    /* Draws inside the per-viewport loop, using the current viewport's player. */
    if (DAT_00487808 > 0 && DAT_00487810 != 0) {
        PlayerData *player = Player_Get(pidx);

        /* Only draw HUD if player is alive (status field +0xD0 == 0) */
        if (player->timer_d0 == 0) {
            /* Minimap/radar — guarded by config flag DAT_00483743 (blob 0x17EB) */
            if (DAT_00483743 != 0 && DAT_00489230 != NULL) {
                FUN_004090e0(framebuffer, (unsigned int)pidx);
            }

            /* Weapon selection grid (if player pressed weapon select key) */
            if (player->weapon_select_active == 1) {
                FUN_0040a9e0(framebuffer, pidx);
            }

            /* Health bar (if health > 0 and LUT system initialized) */
            if (player->health > 0 && DAT_00489230 != NULL) {
                FUN_0040b860(framebuffer, pidx);
            }

            /* Player/weapon name text */
            if (player->timer_c8 != 0 && DAT_00487abc != NULL) {
                int weapon_slot = (int)(int8_t)player->weapon_type;
                if (weapon_slot >= 0 && weapon_slot < 64) {
                    int weapon_type = player->weapon_slots[weapon_slot];
                    char *name_tex = (char *)DAT_00487abc + weapon_type * 0x218 + 4 +
                                     (int8_t)player->weapon_mark * 0x14;
                    int font = (DAT_004806d8 > 255) ? 2 : 1;
                    Draw_Text_To_Buffer(name_tex, font, 0,
                        buffer + (DAT_004806e8 + 3) * stride + DAT_004806ec + 4,
                        stride, 0, DAT_004806d8 - 0x26, 0x14);
                }
            }

            /* Pickup/powerup text */
            if (player->hud_banner_timer != 0) {
                FUN_0040aca0(framebuffer, pidx);
            }

            /* Weapon icon + ammo dots — only if weapon data is initialized */
            if (DAT_00487abc != NULL && DAT_00487ab4 != NULL) {
                int weapon_slot = (int)(int8_t)player->weapon_type;
                if (weapon_slot >= 0 && weapon_slot < 64) {
                    int weapon_type = player->weapon_slots[weapon_slot];
                    int icon_x = DAT_004806ec + DAT_004806d8 - 0x12;
                    int icon_y = DAT_004806e8 + 0x12;

                    /* Determine icon state: check if weapon has enough charge to show
                     * as "selected" (bright). player[+0x94] != 0 means weapon is firing/active,
                     * then check charge threshold: sub_index * weapon_data[+0xDC] * DAT_0048382c >= 0x23000 */
                    char icon_state = 1;  /* default: normal/dim */
                    if (player->timer_94 != 0) {
                        unsigned char sub_idx = player->weapon_mark;
                        int capacity = *(int *)((char *)DAT_00487abc + weapon_type * 0x218 +
                                                sub_idx * 4 + 0xDC);
                        unsigned char charge = player->primary_fire_interval;
                        int check = (capacity * charge * DAT_0048382c) & 0xFFFFF000;
                        if (check >= 0x23000) {
                            icon_state = 0;  /* selected/bright */
                        }
                    }

                    FUN_0040aaf0(framebuffer, icon_x, icon_y, weapon_type, icon_state);

                    /* Weapon Mark selector dots around weapon icon */
                    if (DAT_00487ab0 != NULL) {
                        int selected_mark = player->weapon_mark + 1;
                        int highest_mark = *(unsigned char *)((char *)DAT_00487abc +
                                           weapon_type * 0x218 + 0x7D);
                        FUN_0040a710(framebuffer, icon_x, icon_y,
                                     selected_mark, highest_mark);
                    }
                }
            }

            /* Shield/energy bar — guarded by config flag DAT_00483742 (blob 0x17EA) */
            if (DAT_00483742 != 0 && DAT_00489230 != NULL) {
                FUN_0040b580(framebuffer, pidx);
            }

            /* Frag count text */
            if (player->timer_cb != 0) {
                char frag_buf[100];
                FUN_004644af(frag_buf, (const unsigned char *)"Frags: %d",
                             player->frag_count);
                Draw_Text_To_Buffer(frag_buf, 1, 1,
                    buffer + (DAT_004806e8 + 0x32) * stride + DAT_004806ec + 4,
                    stride, 0, DAT_004806d8 - 0x0C, 0);
            }

            /* Lives display / "You are dead!" */
            if (player->timer_cc != 0) {
                char lives_buf[32];
                if (player->lives == 0) {
                    strcpy(lives_buf, "You are dead!");
                } else {
                    FUN_004644af(lives_buf, (const unsigned char *)"Lives: %d",
                                 player->lives);
                }
                Draw_Text_To_Buffer(lives_buf, 1, 5,
                    buffer + (DAT_004806e8 + 0x41) * stride + DAT_004806ec + 4,
                    stride, 0, DAT_004806d8 - 0x0C, 0);
            }
        }

        /* Team status text (outside alive-check, always if team game active) */
        if (DAT_004892a4 != 0 && DAT_0048764a == 0) {
            int team = (int)(int8_t)player->team;
            FUN_004094f0(framebuffer, team);
        }

        /* Timer display */
        if (DAT_004892a8 != 0 && (DAT_004892a8 < 0x762 || DAT_0048764a != 0)) {
            FUN_00409280(framebuffer);
        }
    }

    } /* end per-viewport loop */

    /* ---- Pause / overlay states (end of FUN_00407720) ---- */
    if (g_SubState != GAMEPLAY_ACTIVE) {
        if (g_SubState == GAMEPLAY_PAUSED) {
            /* State 1 (Pause key): text overlay with key name + version string.
             * Original: "Game Paused. Press \"[KEY]\" to continue." + "TOU v1.0" */
            char pause_msg[100];
            const char *key_name = "???";
            if (g_KeyNameTable && DAT_004837ba < 256 && g_KeyNameTable[DAT_004837ba])
                key_name = g_KeyNameTable[DAT_004837ba];
            sprintf(pause_msg, "Game Paused. Press \"%s\" to continue.", key_name);
            Draw_Text_To_Buffer(pause_msg, 3, 2,
                buffer + (DAT_0048923c - 0x1e) * stride + 8,
                stride, 0, DAT_00489238 - 0x10, 0);
            Draw_Text_To_Buffer("TOU v1.0", 3, 0,
                buffer + (DAT_0048923c - 0x0f) * stride + 8,
                stride, 0, DAT_00489238 - 0x10, 0);
        }
        else if (g_SubState == GAMEPLAY_EXIT_MENU) {
            /* State 2 (ESC menu): render sprite 0x37 centered on screen */
            if (DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
                int frame_off = ((int *)DAT_00489234)[0x37];
                int spr_h = (int)((unsigned char *)DAT_00489e88)[0x37];
                int spr_w = (int)((unsigned char *)DAT_00489e8c)[0x37];
                unsigned short *spr_px = (unsigned short *)DAT_00487ab4;

                if (spr_w > 0 && spr_h > 0) {
                    int cx = (DAT_00489238 - spr_w) / 2;
                    int cy = (DAT_0048923c - spr_h) / 2;
                    unsigned short *dst = buffer + cy * stride + cx;
                    int src_idx = frame_off;
                    for (int y = 0; y < spr_h; y++) {
                        for (int x = 0; x < spr_w; x++) {
                            unsigned short pixel = spr_px[src_idx];
                            if (pixel != 0) {
                                dst[x] = pixel;
                            }
                            src_idx++;
                        }
                        dst += stride;
                    }
                }
            }
        }
        else if ((unsigned char)DAT_0048693c < g_GameConfig.values.active_level_count) {
            /* States 3/4 (round-end stats / level preview): render sprite 0x3F panel
             * with level number, round result, and team win counts.
             * Original at end of FUN_00407720: draws for any substate not 0/1/2
             * when more rounds remain. Static text ("Level:", "Current wins:",
             * "Press Enter") is baked into the sprite artwork. */
            if (DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
                int spr_idx = 0x3F;
                int frame_off = ((int *)DAT_00489234)[spr_idx];
                int spr_h = (int)((unsigned char *)DAT_00489e88)[spr_idx];
                int spr_w = (int)((unsigned char *)DAT_00489e8c)[spr_idx];
                unsigned short *spr_px = (unsigned short *)DAT_00487ab4;

                if (spr_w > 0 && spr_h > 0) {
                    int panel_x = (DAT_00489238 - spr_w) / 2;
                    int panel_y = (DAT_0048923c - spr_h) / 2;
                    unsigned short *dst = buffer + panel_y * stride + panel_x;
                    int src_idx = frame_off;
                    for (int y = 0; y < spr_h; y++) {
                        for (int x = 0; x < spr_w; x++) {
                            unsigned short pixel = spr_px[src_idx];
                            if (pixel != 0) {
                                dst[x] = pixel;
                            }
                            src_idx++;
                        }
                        dst += stride;
                    }

                    /* Dynamic text overlaid on sprite panel */
                    char text_buf[100];

                    /* Level count: "N / M" (original format at 0x47b110) */
                    FUN_004644af(text_buf, (const unsigned char *)"%d / %d",
                                 (int)(unsigned char)DAT_0048693c,
                                 (int)g_GameConfig.values.active_level_count);
                    Draw_Text_To_Buffer(text_buf, 2, 0,
                        buffer + (panel_y + 10) * stride + panel_x + 0x73,
                        stride, 0, 0xFA, 0);

                    /* Round result text */
                    if ((char)DAT_00487640[0] == 0) {
                        strcpy(text_buf, "Level skipped");
                    } else if ((char)DAT_00487640[0] == (char)-1) {
                        strcpy(text_buf, "Draw. Everybody died");
                    } else {
                        FUN_004644af(text_buf, (const unsigned char *)"Team %d wins the round",
                                     (int)(unsigned char)DAT_00487640[0]);
                    }
                    /* Two-line display: "Team N" + "wins the round" (font 1, centered) */
                    if ((char)DAT_00487640[0] > 0 && (char)DAT_00487640[0] != (char)-1) {
                        char line1[32], line2[32];
                        FUN_004644af(line1, (const unsigned char *)"Team %d",
                                     (int)(unsigned char)DAT_00487640[0]);
                        strcpy(line2, "wins the round");
                        Draw_Text_To_Buffer(line1, 1, 1,
                            buffer + (panel_y + 0x1f) * stride + panel_x + 0x12,
                            stride, 0, spr_w - 0x24, 0);
                        Draw_Text_To_Buffer(line2, 1, 1,
                            buffer + (panel_y + 0x2b) * stride + panel_x + 0x12,
                            stride, 0, spr_w - 0x24, 0);
                    } else {
                        Draw_Text_To_Buffer(text_buf, 1, 1,
                            buffer + (panel_y + 0x1e) * stride + panel_x + 0x12,
                            stride, 0, spr_w - 0x24, 0);
                    }

                    /* Team names and win counts */
                    unsigned char *slots = (unsigned char *)&DAT_0048693c;

                    Draw_Text_To_Buffer("Team1", 1, 6,
                        buffer + (panel_y + 0x55) * stride + panel_x + 10,
                        stride, 0, 0xFA, 0);
                    FUN_004644af(text_buf, (const unsigned char *)"%d", (int)slots[1]);
                    Draw_Text_To_Buffer(text_buf, 2, 6,
                        buffer + (panel_y + 100) * stride + panel_x + 0x1E,
                        stride, 0, 0xFA, 0);

                    Draw_Text_To_Buffer("Team2", 1, 7,
                        buffer + (panel_y + 0x55) * stride + panel_x + 0x5A,
                        stride, 0, 0xFA, 0);
                    FUN_004644af(text_buf, (const unsigned char *)"%d", (int)slots[2]);
                    Draw_Text_To_Buffer(text_buf, 2, 7,
                        buffer + (panel_y + 100) * stride + panel_x + 0x6E,
                        stride, 0, 0xFA, 0);

                    Draw_Text_To_Buffer("Team3", 1, 8,
                        buffer + (panel_y + 0x55) * stride + panel_x + 0xAF,
                        stride, 0, 0xFA, 0);
                    FUN_004644af(text_buf, (const unsigned char *)"%d", (int)slots[3]);
                    Draw_Text_To_Buffer(text_buf, 2, 8,
                        buffer + (panel_y + 100) * stride + panel_x + 0xC3,
                        stride, 0, 0xFA, 0);
                }
            }
        }
    }
}

/* ===== Render_Frame (0045D800) ===== */
/*
 * The original DirectDraw lock/blit sequence is now split at a stable boundary:
 * this function composes the RGB565 software frame, then the selected backend
 * presents it. The DirectDraw backend retains the existing conversion/scaling.
 */
void Render_Frame(void)
{
    /* Allocate scratch buffer on first use */
    if (g_ScratchBuffer == NULL) {
        g_ScratchBuffer = (unsigned short *)malloc(640 * 480 * 2);
        if (!g_ScratchBuffer) return;
    }
    Framebuffer framebuffer = {g_ScratchBuffer, 640, 480, 640};

    /* 1. Draw background into scratch buffer.
     *    Gameplay state (g_GameState==0) with level data: blit level background.
     *    Menu/intro: copy Software_Buffer (JPEG background). */
    if (g_GameState == GAME_STATE_GAMEPLAY && DAT_00481f50 != NULL) {
        /* Game world: blit visible viewport from level background */
        Render_Game_World(&framebuffer);
    } else {
        /* Menu/intro: copy clean background from Software_Buffer */
        int frameOffset = (g_FrameIndex & 0xFF) * (640 * 480);
        unsigned short *src = Software_Buffer + frameOffset;
        memcpy(g_ScratchBuffer, src, 640 * 480 * 2);
    }

    /* 2. Draw menu items / particles onto scratch buffer.
     *    During gameplay (g_GameState==0), Render_Game_World already called all
     *    10 entity renderers including FUN_0040d100 (fog). Render_Game_View_To
     *    and FUN_004076d0 are menu/intro-only in the original binary —
     *    calling them during gameplay would draw stale menu text and apply
     *    fog a second time, making the screen too dark. */
    if (g_GameState != GAME_STATE_GAMEPLAY) {
        Render_Game_View_To(&framebuffer);
        FUN_004076d0(&framebuffer);
    }
    RenderBackend_Present(&framebuffer);
}

/* ===== Render_Game_View_To - Draw menu items onto a target buffer ===== */
/* Called every frame by Render_Frame on the scratch buffer.
 * The scratch framebuffer replaces the original locked DirectDraw surface. */
static int Menu_Text_Width(const char *str, int font_idx)
{
    int width = 0;
    if (!str || !Font_Char_Table)
        return 0;
    int font_base = (font_idx & 0xFF) * 256;
    while (*str)
        width += Font_Char_Table[font_base + (unsigned char)*str++].width;
    return width;
}

void Render_Game_View_To(Framebuffer *framebuffer)
{
    unsigned short *frame = framebuffer != NULL ? framebuffer->pixels : NULL;
    if (!frame || !g_GameViewData)
        return;

    MenuItem *items = (MenuItem *)g_GameViewData;

    for (int i = 0; i < DAT_004877a8; i++) {
        MenuItem *item = &items[i];

        if (item->type == 0) {
            /* ---- Text item ---- */
            /* Original: FUN_00428650 renders text based on render_mode.
             * Static labels use string_idx directly.
             * Enum values use string_idx + config_byte.
             * Numeric values use sprintf of config_byte.
             * Special modes: resolution, key binding, level name. */
            const char *str = NULL;
            char valBuf[128];
            unsigned char *cfgPtr = (unsigned char *)(uintptr_t)item->extra_data;

            switch (item->render_mode) {
            /* Static labels: use string_idx directly */
            case 0x00: case 0x0C: case 0x1A: case 0x20: case 0x21:
            case 0x22: case 0x23: case 0x24: case 0x25: case 0x2B:
            case 0x2C: case 0x2D: case 0x2E: case 0x2F:
                if (g_MenuStrings && item->string_idx < 350 &&
                    g_MenuStrings[item->string_idx])
                    str = g_MenuStrings[item->string_idx];
                break;

            /* Weapon name from loadtime.dat config table */
            case 0x06: {
                if (DAT_00487abc != NULL) {
                    static char wpn_name[21];
                    char *raw = (char *)((int)DAT_00487abc + item->string_idx * 0x218 + 4);
                    memcpy(wpn_name, raw, 20);
                    wpn_name[20] = '\0';
                    for (int ti = 19; ti >= 0 && wpn_name[ti] == ' '; ti--)
                        wpn_name[ti] = '\0';
                    str = wpn_name;
                }
                break;
            }

            /* Enum: string_idx + config_value */
            case 0x01: case 0x04: case 0x0F: case 0x10: case 0x11:
            case 0x12: case 0x17: case 0x1E: case 0x27:
            case 0x30: case 0x33: {
                int val = cfgPtr ? (int)*cfgPtr : 0;
                /* Inline slider preview for enum items during drag — clamp to valid range */
                if (g_InputMode == 1 && (unsigned char)i == DAT_004877e6) {
                    int drag_delta = DAT_004877e8 >> 10;
                    int v = val + drag_delta;
                    if (v < 0) v = 0;
                    /* Clamp to max valid enum index based on render_mode */
                    int max_val = 4; /* default */
                    if (item->render_mode == 0x33) max_val = 4;
                    else if (item->render_mode == 0x01) max_val = 1;
                    else if (item->render_mode == 0x12) max_val = 7;
                    else if (item->render_mode == 0x17) max_val = 3;
                    if (v > max_val) v = max_val;
                    val = v;
                }
                int idx = item->string_idx + val;
                if (g_MenuStrings && idx >= 0 && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
                if (str && item->render_mode != 0x1E) {
                    int text_w = Menu_Text_Width(str, item->font_idx);
                    item->x = (item->render_mode == 0x0F || item->render_mode == 0x17)
                        ? 100 : 540 - text_w;
                }
                break;
            }

            /* Weapon loadout grid: draw weapon sprite + "No"/"Yes" text.
             * item->color_style = weapon index (0-46) from FUN_00430200 param4.
             * Config value: 0 = disabled, non-0 = enabled.
             * Reads weapon sprite from config table using color_style as weapon idx.
             * The sprite index is at config table entry field for this weapon. */
            case 0x26: {
                /* Draw weapon sprite from config table */
                int wpn_idx = item->color_style;
                if (wpn_idx >= 0 && wpn_idx < 47 &&
                    DAT_00487abc && DAT_00487ab4 && DAT_00489234 &&
                    DAT_00489e8c && DAT_00489e88) {
                    /* Weapon icon sprite index at config table offset +0x144.
                     * Most weapons have indices in 200-240 range. */
                    /* 3-state circle: red=banned, blue=on, gray=off */
                    int per_player_val = cfgPtr ? (int)*cfgPtr : 0;
                    int globally_banned =
                        (g_GameConfig.values.global_weapon_enabled[wpn_idx] == 0) ? 1 : 0;
                    /* Circle background sprites — from Ghidra FUN_0040aaf0.
                     * 4 states: 0xC8=red(banned), 0xD4=blue(selected), 0xEE=normal, 0xEF=gray(nonexistent).
                     * Priority: gray > blue > red/normal. */
                    {
                        int color_state;
                        if (per_player_val != 0) color_state = 2; /* normal/enabled */
                        else color_state = 0; /* red/banned */
                        /* Check if currently selected weapon for this player */
                        unsigned char player_idx = item->flag1;
                        if (wpn_idx == (int)(unsigned char)DAT_004836ce[player_idx])
                            color_state = 1; /* blue = selected */
                        /* Check if weapon exists */
                        if (g_GameConfig.values.global_weapon_enabled[wpn_idx] == 0)
                            color_state = 3; /* gray = nonexistent/globally banned */
                        int circle_spr;
                        switch (color_state) {
                            case 0: circle_spr = 0xC8; break; /* red */
                            case 1: circle_spr = 0xD4; break; /* blue */
                            case 2: circle_spr = 0xEE; break; /* normal */
                            default: circle_spr = 0xEF; break; /* gray */
                        }
                        /* Draw circle sprite centered at item + 12, 12 */
                        int c_pb = ((int *)DAT_00489234)[circle_spr];
                        int c_w = (int)((unsigned char *)DAT_00489e8c)[circle_spr];
                        int c_h = (int)((unsigned char *)DAT_00489e88)[circle_spr];
                        if (c_w > 0 && c_h > 0) {
                            int c_x = item->x + 12 - c_w / 2;
                            int c_y = item->y + 12 - c_h / 2;
                            if (c_x >= 0 && c_y >= 0 && c_x + c_w <= 640 && c_y + c_h <= 480) {
                                unsigned short *cdst = frame + c_y * 640 + c_x;
                                unsigned short *csrc = (unsigned short *)DAT_00487ab4;
                                int cp = c_pb;
                                for (int row = 0; row < c_h; row++) {
                                    for (int col = 0; col < c_w; col++) {
                                        unsigned short pixel = csrc[cp++];
                                        if (pixel != 0) cdst[col] = pixel;
                                    }
                                    cdst += 640;
                                }
                            }
                        }
                    }
                    /* Weapon sprite from config table offset +0x144 */
                    int spr_idx = (int)*(unsigned short *)((char *)DAT_00487abc + wpn_idx * 0x218 + 0x144);
                    if (spr_idx > 0 && spr_idx < 20000) {
                        int pixel_base = ((int *)DAT_00489234)[spr_idx];
                        int spr_w = (int)((unsigned char *)DAT_00489e8c)[spr_idx];
                        int spr_h = (int)((unsigned char *)DAT_00489e88)[spr_idx];
                        if (spr_w > 0 && spr_h > 0) {
                            /* Center sprite within circle — from Ghidra: center at item+12,+12 */
                            int dx = item->x + 12 - spr_w / 2;
                            int dy = item->y + 12 - spr_h / 2;
                            if (dx >= 0 && dy >= 0 && dx + spr_w <= 640 && dy + spr_h <= 480) {
                                unsigned short *dst = frame + dy * 640 + dx;
                                unsigned short *src_pixels = (unsigned short *)DAT_00487ab4;
                                for (int row = 0; row < spr_h; row++) {
                                    for (int col = 0; col < spr_w; col++) {
                                        unsigned short pixel = src_pixels[pixel_base++];
                                        if (pixel != 0) {
                                            if (globally_banned)
                                                dst[col] = (pixel >> 3) & 0x18E3; /* very dark */
                                            else if (per_player_val > 0)
                                                dst[col] = pixel; /* bright */
                                            else
                                                dst[col] = (pixel >> 1) & 0x7BEF; /* dimmed */
                                        }
                                    }
                                    dst += 640;
                                }
                            }
                        }
                    }
                }
                /* No text overlay — sprites only for weapon grid */
                break;
            }

            /* Non-linear enum: 1→+0, 4→+1, 10→+2, 30→+3, else→+4 */
            case 0x02: {
                int val = cfgPtr ? (int)*cfgPtr : 0;
                int offset;
                if (val == 1) offset = 0;
                else if (val == 4) offset = 1;
                else if (val == 10) offset = 2;
                else if (val == 30) offset = 3;
                else offset = 4;
                int idx = item->string_idx + offset;
                if (g_MenuStrings && idx >= 0 && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            /* One-based enum: string_idx + config_value - 1 */
            case 0x03: {
                int val = cfgPtr ? (int)*cfgPtr : 1;
                int idx = item->string_idx + val - 1;
                if (g_MenuStrings && idx >= 0 && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            /* Numeric: sprintf the config value (with scaling) */
            case 0x05: case 0x09: case 0x0E: case 0x13: case 0x14:
            case 0x15: case 0x16: case 0x18: case 0x34: {
                int val = cfgPtr ? (int)(unsigned int)*cfgPtr : 0;
                /* Inline slider preview: apply drag delta with per-mode wrapping.
                 * From Ghidra FUN_00428650: each render_mode has its own range. */
                if (g_InputMode == 1 && (unsigned char)i == DAT_004877e6) {
                    switch (item->render_mode) {
                    case 0x05: /* unbounded byte add */
                        val = (unsigned char)((char)(DAT_004877e8 >> 10) + (unsigned char)val);
                        break;
                    case 0x09: { /* range 1-200 */
                        int v = val + (DAT_004877e8 >> 10);
                        v = v - 1;
                        if (v > 0) val = (v % 200) + 1;
                        else if (v < 0) val = v + (1 - (v + 1) / 200) * 200 + 1;
                        else val = 1;
                        break;
                    }
                    case 0x0E: { /* range 0-100 */
                        int v = val + (DAT_004877e8 >> 10);
                        if (v > 0) val = v % 0x65;
                        else if (v < 0) val = v + (1 - (v + 1) / 0x65) * 0x65;
                        else val = 0;
                        break;
                    }
                    case 0x13: { /* range 0-80 */
                        int v = val + (DAT_004877e8 >> 10);
                        if (v > 0) val = v % 0x51;
                        else if (v < 0) val = v + (1 - (v + 1) / 0x51) * 0x51;
                        else val = 0;
                        break;
                    }
                    case 0x14: { /* range 0-20, uses >> 0xb */
                        int v = val + (DAT_004877e8 >> 0xb);
                        if (v > 0) val = v % 0x15;
                        else if (v < 0) val = v + (1 - (v + 1) / 0x15) * 0x15;
                        else val = 0;
                        break;
                    }
                    case 0x15: { /* range 1-250 */
                        int v = val + (DAT_004877e8 >> 10);
                        v = v - 1;
                        if (v > 0) val = (v % 0xFA) + 1;
                        else if (v < 0) val = v + (1 - (v + 1) / 0xFA) * 0xFA + 1;
                        else val = 1;
                        break;
                    }
                    case 0x16: case 0x34: { /* range 0-250 */
                        int v = val + (DAT_004877e8 >> 10);
                        if (v > 0) val = v % 0xFB;
                        else if (v < 0) val = v + (1 - (v + 1) / 0xFB) * 0xFB;
                        else val = 0;
                        break;
                    }
                    case 0x18: { /* range 1-64 (player count) */
                        int v = val + (DAT_004877e8 >> 10);
                        v = v - 1;
                        if (v > 0) { v = v & 0x3F; val = v + 1; }
                        else if (v < 0) { v = v + (1 - ((v + 1) >> 6)) * 64; val = v + 1; }
                        else val = 1;
                        break;
                    }
                    }
                }
                /* Scaling multipliers per render_mode */
                switch (item->render_mode) {
                case 0x09: break; /* display config value directly */
                case 0x13: val *= 5; break;
                case 0x14: val *= 25; break;
                case 0x15: val *= 5; break;
                case 0x16: val *= 5; break;
                case 0x34: val *= 5; break;
                default: break;
                }
                if (item->render_mode == 0x34 && val > 1200) {
                    str = "INFINITE";
                } else if (item->render_mode == 0x0E || item->render_mode == 0x13 ||
                           item->render_mode == 0x14 || item->render_mode == 0x15 ||
                           item->render_mode == 0x16 || item->render_mode == 0x34) {
                    sprintf(valBuf, "%d %%", val);
                    str = valBuf;
                } else {
                    sprintf(valBuf, "%d", val);
                    str = valBuf;
                }
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            /* Resolution: format as "WxH" from mode tables */
            case 0x0D: {
                int modeIdx = cfgPtr ? (int)*cfgPtr : 0;
                if (modeIdx >= 0 && modeIdx < g_NumDisplayModes &&
                    g_ModeWidths[modeIdx] > 0) {
                    sprintf(valBuf, "%d x %d",
                            g_ModeWidths[modeIdx], g_ModeHeights[modeIdx]);
                } else {
                    sprintf(valBuf, "%d x %d", 640, 480);
                }
                str = valBuf;
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            case 0x35: {
                int mode = cfgPtr ? (int)*cfgPtr : 0;
                int idx = 0x14E + ((mode != 0) ? 1 : 0);
                if (g_MenuStrings && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            /* Key binding: show key name from scan code */
            case 0x07: {
                int scanCode = cfgPtr ? (int)*cfgPtr : 0;
                if (g_InputMode == 2 && DAT_004877e6 == (unsigned char)i) {
                    /* Waiting for key press - show ESC hint */
                    if (g_KeyNameTable && g_KeyNameTable[1])
                        str = g_KeyNameTable[1];
                } else {
                    if (g_KeyNameTable && scanCode < 256 && g_KeyNameTable[scanCode])
                        str = g_KeyNameTable[scanCode];
                }
                item->x = 540 - Menu_Text_Width(str, item->font_idx);
                break;
            }

            /* Level/map name: look up from level name array.
             * cfgPtr points to an int in the indirection table (g_ConfigBlob[4..]).
             * Dereference to get the actual level index, then look up name. */
            case 0x08: {
                int levelIdx = cfgPtr ? *(int *)cfgPtr : 0;
                if (levelIdx >= 0 && levelIdx < DAT_00485088 &&
                    DAT_00485090[levelIdx] != NULL) {
                    const char *name = (const char *)DAT_00485090[levelIdx];
                    if (DAT_00485ea0[levelIdx] == '\x02') {
                        sprintf(valBuf, "GG: %s", name);
                        str = valBuf;
                    } else {
                        str = name;
                    }
                } else {
                    sprintf(valBuf, "<%d>", levelIdx);
                    str = valBuf;
                }
                /* Center text at x=320 by calculating pixel width */
                if (str) {
                    int fontBase = (item->font_idx & 0xFF) * 256;
                    int textW = 0;
                    for (const char *p = str; *p; p++)
                        textW += Font_Char_Table[fontBase + (unsigned char)*p].width;
                    item->x = 320 - textW / 2;
                    if (item->x < 0) item->x = 0;
                }
                break;
            }

            /* Color swatch: draw 25x25 filled rectangle from ship palette */
            case 0x1B: {
                unsigned char colorVal = cfgPtr ? *cfgPtr : 0;

                /* Inline slider preview: if dragging this item, add delta */
                if (g_InputMode == 1 && (unsigned char)i == DAT_004877e6) {
                    colorVal = (unsigned char)((char)colorVal + (char)(DAT_004877e8 >> 8));
                }

                /* Original FUN_00428650 copies the 16-bit palette entry
                 * verbatim.  Converting it here shifted red and green and
                 * made the selector disagree with the actual ship color. */
                int sx = item->x;
                int sy = item->y;
                if (DAT_00481f4c && sx >= 0 && sy >= 0 && sx + 25 <= 640 && sy + 25 <= 480) {
                    unsigned short fillColor = ((unsigned short *)DAT_00481f4c)[colorVal];
                    for (int row = 0; row < 25; row++) {
                        for (int col = 0; col < 25; col++) {
                            frame[(sy + row) * 640 + sx + col] = fillColor;
                        }
                    }
                }
                break;
            }

            /* Team number: sprintf + team-colored text */
            case 0x1C: {
                int teamVal = cfgPtr ? (int)*cfgPtr : 0;
                sprintf(valBuf, "%d", teamVal);
                str = valBuf;
                /* Use team color palette (6=team0, 7=team1, 8=team2) */
                item->color_style = teamVal + 6;
                break;
            }

            /* Ship type: draw ship sprite or "Random" text */
            case 0x1D: {
                unsigned int shipType = cfgPtr ? (unsigned int)*cfgPtr : 0;

                /* If ship is banned, draw disabled sprite (0xEF) centered */
                if (shipType < 9 && DAT_0048378e[shipType] != 0 &&
                    DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
                    int dspr = 0xEF;
                    int dw = (int)((unsigned char *)DAT_00489e8c)[dspr];
                    int dh = (int)((unsigned char *)DAT_00489e88)[dspr];
                    if (dw > 0 && dh > 0) {
                        int dx = item->x - dw / 2;
                        int dy = item->y - dh / 2;
                        if (dx >= 0 && dy >= 0 && dx + dw <= 640 && dy + dh <= 480) {
                            int dpb = ((int *)DAT_00489234)[dspr];
                            unsigned short *src = (unsigned short *)DAT_00487ab4;
                            unsigned short *ddst = frame + dy * 640 + dx;
                            for (int row = 0; row < dh; row++) {
                                for (int col = 0; col < dw; col++) {
                                    unsigned short px = src[dpb++];
                                    if (px != 0) ddst[col] = px;
                                }
                                ddst += 640;
                            }
                        }
                    }
                }

                if (shipType == 9) {
                    /* "Random" text */
                    str = "Random";
                } else if (DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
                    /* Draw ship preview sprite at index 0xE5 + ship_type */
                    int sIdx = (int)shipType + 0xE5;
                    int sw2 = (int)((unsigned char *)DAT_00489e8c)[sIdx];
                    int sh2 = (int)((unsigned char *)DAT_00489e88)[sIdx];
                    if (sw2 > 0 && sh2 > 0) {
                        int sx2 = item->x - sw2 / 2;
                        int sy2 = item->y - sh2 / 2;
                        if (sx2 >= 0 && sy2 >= 0 && sx2 + sw2 <= 640 && sy2 + sh2 <= 480) {
                            int spb = ((int *)DAT_00489234)[sIdx];
                            unsigned short *src = (unsigned short *)DAT_00487ab4;
                            unsigned short *sdst = frame + sy2 * 640 + sx2;
                            for (int row = 0; row < sh2; row++) {
                                for (int col = 0; col < sw2; col++) {
                                    unsigned short px = src[spb++];
                                    if (px != 0) sdst[col] = px;
                                }
                                sdst += 640;
                            }
                        }
                    }
                }
                break;
            }

            /* Computer/CPU: stars for CPU players, "-" for humans */
            case 0x1F: {
                unsigned char cpuDiff = cfgPtr ? *cfgPtr : 0;
                unsigned int humanCount = g_GameConfig.values.human_player_count;

                /* Inline slider preview for human count */
                if (g_InputMode == 1 && g_GameViewData) {
                    MenuItem *chkIt = (MenuItem *)g_GameViewData;
                    if (chkIt[(unsigned char)DAT_004877e6].extra_data ==
                        CFG_ADDR(0x48227d)) {
                        int hDelta = DAT_004877e8 >> 0xC;
                        int hv = (int)humanCount + hDelta;
                        if (hv > 0)
                            hv = hv % 5;
                        else if (hv < 0)
                            hv = hv + (1 - (hv + 1) / 5) * 5;
                        humanCount = (unsigned int)hv;
                    }
                }

                unsigned int starCount = (unsigned int)cpuDiff + 1;
                /* Build stars string */
                int spos = 0;
                for (unsigned int s = 0; s < starCount && spos < 120; s++)
                    valBuf[spos++] = '*';
                valBuf[spos] = '\0';

                unsigned char playerIdx = item->flag1;
                if ((int)(unsigned int)playerIdx < (int)humanCount) {
                    /* Human player: show "-" */
                    valBuf[0] = '-'; valBuf[1] = '\0';
                    item->color_style = 2;  /* cyan */
                    item->font_idx = 2;
                } else {
                    /* CPU player: show stars */
                    item->font_idx = 0;
                    item->color_style = (starCount > 3) ? 0 : 3;
                }
                str = valBuf;
                break;
            }

            default:
                if (g_MenuStrings && item->string_idx < 350 &&
                    g_MenuStrings[item->string_idx])
                    str = g_MenuStrings[item->string_idx];
                break;
            }

            if (str && str[0] != '\0') {
                if (item->x >= 0 && item->x < 640 &&
                    item->y >= 0 && item->y < 480) {
                    unsigned short *dest = frame + item->y * 640 + item->x;
                    Draw_Text_To_Buffer(str,
                                        item->font_idx & 0xFF,
                                        item->color_style & 0xFF,
                                        dest, 640,
                                        item->hover_state >> 18,
                                        0, 0);
                }
            }
        }
        else if (item->type == 1) {
            /* ---- Sprite item ---- */
            /* item->color_style holds sprite index (field is overloaded) */
            int sprite_idx = item->color_style;

            if (!DAT_00487ab4 || !DAT_00489234 || !DAT_00489e8c || !DAT_00489e88)
                continue;

            int pixel_base = ((int *)DAT_00489234)[sprite_idx];
            int spr_w = (int)((unsigned char *)DAT_00489e8c)[sprite_idx];
            int spr_h = (int)((unsigned char *)DAT_00489e88)[sprite_idx];

            if (spr_w <= 0 || spr_h <= 0)
                continue;
            if (item->x < 0 || item->y < 0 ||
                item->x + spr_w > 640 || item->y + spr_h > 480)
                continue;

            unsigned short *dst = frame + item->y * 640 + item->x;
            unsigned short *src_pixels = (unsigned short *)DAT_00487ab4;

            if (sprite_idx == 0x13) {
                /* TOU logo: unconditional blit (no transparency) */
                for (int row = 0; row < spr_h; row++) {
                    for (int col = 0; col < spr_w; col++) {
                        dst[col] = src_pixels[pixel_base++];
                    }
                    dst += 640;
                }
            } else if (sprite_idx < 400) {
                /* Normal sprite: pixel 0 = transparent */
                for (int row = 0; row < spr_h; row++) {
                    for (int col = 0; col < spr_w; col++) {
                        unsigned short pixel = src_pixels[pixel_base++];
                        if (pixel != 0) {
                            dst[col] = pixel;
                        }
                    }
                    dst += 640;
                }
            } else if (DAT_00489e94) {
                /* Grayscale sprite (index >= 400): DAT_00489e94 has 1-byte grayscale pixels.
                 * gray=0 is transparent. Non-zero maps to brightness in RGB565. */
                unsigned char *gray_pixels = (unsigned char *)DAT_00489e94;
                for (int row = 0; row < spr_h; row++) {
                    for (int col = 0; col < spr_w; col++) {
                        unsigned char gray = gray_pixels[pixel_base++];
                        if (gray != 0) {
                            /* Convert grayscale to cyan-tinted RGB565 */
                            unsigned short r = (gray >> 5) & 0x07; /* low red */
                            unsigned short g = (gray >> 2) & 0x3F; /* full green */
                            unsigned short b = (gray >> 3) & 0x1F; /* full blue */
                            dst[col] = (r << 11) | (g << 5) | b;
                        }
                    }
                    dst += 640;
                }
            }
        }
    }

    /* ---- Drag popup: palette strip for color selector (0x1B), or tooltip for others ---- */
    /* From Ghidra FUN_00428650 post-loop section. */
    if (g_InputMode == 1 && g_GameViewData) {
        MenuItem *drag_items = (MenuItem *)g_GameViewData;
        int drag_idx = (int)(unsigned int)DAT_004877e6;
        if (drag_idx >= 0 && drag_idx < DAT_004877a8) {
            unsigned char drag_rm = drag_items[drag_idx].render_mode;
            if (drag_rm == 0x1B) {
                /* Color selector: draw 256-color palette strip near cursor.
                 * Original at 0x428650 post-loop: 256x16 strip with sprite 0x1B cursors. */
                unsigned char *drag_cfg = (unsigned char *)(uintptr_t)drag_items[drag_idx].extra_data;
                unsigned char colorVal = drag_cfg ? *drag_cfg : 0;
                colorVal = (unsigned char)((char)colorVal + (char)(DAT_004877e8 >> 8));

                /* DAT_004877B4/B8 are the frozen cursor position while the
                 * drag accumulator changes, exactly as in FUN_00428650. */
                int strip_x = (g_MouseDeltaX >> 18) - 0x80;
                if (strip_x < 4) strip_x = 4;
                if (strip_x + 0x104 > 640) strip_x = 640 - 0x104;
                int strip_y = g_MouseDeltaY >> 18;

                /* Draw dark border outline (259 x 17) */
                int bx0 = strip_x - 2, by0 = strip_y + 13;
                int bx1 = bx0 + 259, by1 = by0 + 17;
                if (by0 >= 0 && by1 <= 480 && bx0 >= 0 && bx1 <= 640) {
                    for (int bx = bx0; bx < bx1; bx++) { frame[by0 * 640 + bx] = 0; frame[(by1-1) * 640 + bx] = 0; }
                    for (int by = by0; by < by1; by++) { frame[by * 640 + bx0] = 0; frame[by * 640 + bx1-1] = 0; }
                }
                /* Draw filled background (257 x 19) */
                int fx0 = strip_x - 1, fy0 = strip_y + 12;
                if (fy0 >= 0 && fy0 + 19 <= 480 && fx0 >= 0 && fx0 + 257 <= 640) {
                    for (int fy = fy0; fy < fy0 + 19; fy++)
                        for (int fx = fx0; fx < fx0 + 257; fx++)
                            frame[fy * 640 + fx] = 0;
                }

                /* Draw 256-color palette strip (256 x 16) */
                unsigned short *pal_strip = (unsigned short *)DAT_00481f4c;
                if (pal_strip) {
                    int py_start = strip_y + 14;
                    if (py_start >= 0 && py_start + 16 <= 480 && strip_x >= 0 && strip_x + 256 <= 640) {
                        for (int pr = 0; pr < 16; pr++) {
                            unsigned short *row_dst = frame + (py_start + pr) * 640 + strip_x;
                            for (int pc = 0; pc < 256; pc++) {
                                row_dst[pc] = pal_strip[pc];
                            }
                        }
                    }
                }

                /* Draw cursor sprites (sprite 0x1B) above and below the strip */
                int cur_spr = 0x1B;
                int cur_pb = ((int *)DAT_00489234)[cur_spr];
                int cur_w = (int)((unsigned char *)DAT_00489e8c)[cur_spr];
                int cur_h = (int)((unsigned char *)DAT_00489e88)[cur_spr];
                if (cur_w > 0 && cur_h > 0 && DAT_00487ab4) {
                    unsigned short *cur_src = (unsigned short *)DAT_00487ab4;
                    /* Top cursor: at (strip_x + colorVal - 2, strip_y + 10) */
                    int cx1 = strip_x + (int)colorVal - cur_w / 2;
                    int cy1 = strip_y + 10;
                    if (cx1 >= 0 && cy1 >= 0 && cx1 + cur_w <= 640 && cy1 + cur_h <= 480) {
                        int cp = cur_pb;
                        for (int cr = 0; cr < cur_h; cr++) {
                            for (int cc = 0; cc < cur_w; cc++) {
                                unsigned short px = cur_src[cp++];
                                if (px != 0) frame[(cy1 + cr) * 640 + cx1 + cc] = px;
                            }
                        }
                    }
                    /* Bottom cursor: at (strip_x + colorVal - 2, strip_y + 32) */
                    int cx2 = strip_x + (int)colorVal - cur_w / 2;
                    int cy2 = strip_y + 32;
                    if (cx2 >= 0 && cy2 >= 0 && cx2 + cur_w <= 640 && cy2 + cur_h <= 480) {
                        int cp2 = cur_pb;
                        for (int cr = 0; cr < cur_h; cr++) {
                            for (int cc = 0; cc < cur_w; cc++) {
                                unsigned short px = cur_src[cp2++];
                                if (px != 0) frame[(cy2 + cr) * 640 + cx2 + cc] = px;
                            }
                        }
                    }
                }
            } else {
                /* Compact two-line drag hint at the frozen drag-start cursor. */
                int cursor_x = g_MouseDeltaX >> 18;
                int cursor_y = g_MouseDeltaY >> 18;
                const char *tip_top = "Hold &";
                const char *tip_bottom = "drag";
                int tip_top_w = Menu_Text_Width(tip_top, 2);
                int tip_bottom_w = Menu_Text_Width(tip_bottom, 2);
                int box_w = 0x61;
                int box_h = 0x26;
                int tt_x = cursor_x - box_w / 2;
                int tt_y = cursor_y + 8;
                if (tt_x < 2) tt_x = 2;
                if (tt_x + box_w > 638) tt_x = 638 - box_w;
                if (tt_y + box_h > 478) tt_y = 478 - box_h;
                /* Dark filled background */
                for (int fy = tt_y; fy < tt_y + box_h && fy < 480; fy++)
                    for (int fx = tt_x; fx < tt_x + box_w && fx < 640; fx++)
                        frame[fy * 640 + fx] = 0;
                /* Outline border */
                int ox = tt_x - 1, oy = tt_y - 1;
                int ow = box_w + 2, oh = box_h + 2;
                if (ox >= 0 && oy >= 0 && ox + ow <= 640 && oy + oh <= 480) {
                    for (int bx = ox; bx < ox + ow; bx++) {
                        frame[oy * 640 + bx] = 0x4208;
                        frame[(oy + oh - 1) * 640 + bx] = 0x4208;
                    }
                    for (int by = oy; by < oy + oh; by++) {
                        frame[by * 640 + ox] = 0x4208;
                        frame[by * 640 + ox + ow - 1] = 0x4208;
                    }
                }
                Draw_Text_To_Buffer(tip_top, 2, 0,
                    frame + (tt_y + 1) * 640 + tt_x + (box_w - tip_top_w) / 2,
                    640, 0, 100, 0);
                Draw_Text_To_Buffer(tip_bottom, 2, 0,
                    frame + (tt_y + 16) * 640 + tt_x + (box_w - tip_bottom_w) / 2,
                    640, 0, 100, 0);
            }
        }
    }

    /* ---- Draw scrollbar track + thumb for scrollable pages ---- */
    /* From Ghidra 0x4286B0: thumb = sprite 0x19F (415, grayscale).
     * Track drawn as thin line between arrows. */
    if (DAT_004877b0 != 0 && DAT_004877d8 != 0 &&
        DAT_00489e94 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
        int sb_x = DAT_004877d8;
        int arrow_h = (int)((unsigned char *)DAT_00489e88)[0x19D]; /* arrow height */
        int sb_top = DAT_004877dc + arrow_h; /* just below up arrow */
        int sb_bot = DAT_004877dc + DAT_004877e0 + 0x14; /* just above down arrow */
        int sb_h = sb_bot - sb_top;
        if (sb_x > 0 && sb_x < 630 && sb_h > 10) {
            /* Draw track — match arrow sprite width, centered on arrow x */
            int arrow_w = (int)((unsigned char *)DAT_00489e8c)[0x19D]; /* up arrow width */
            int track_x = sb_x;
            int track_w = (arrow_w > 0) ? arrow_w : 10;
            for (int ty = sb_top; ty < sb_bot; ty++)
                for (int tx = track_x; tx < track_x + track_w && tx < 640; tx++)
                    if (ty >= 0 && ty < 480)
                        frame[ty * 640 + tx] = 0x0009; /* very dark blue */
            /* Draw thumb sprite 0x19F at scroll position */
            int thumb_spr = 0x19F;
            int t_pb = ((int *)DAT_00489234)[thumb_spr];
            int t_w = (int)((unsigned char *)DAT_00489e8c)[thumb_spr];
            int t_h = (int)((unsigned char *)DAT_00489e88)[thumb_spr];
            if (t_w > 0 && t_h > 0) {
                int thumb_y = sb_top + (int)(DAT_004877d4 * (float)(sb_h - t_h));
                if (thumb_y < sb_top) thumb_y = sb_top;
                if (thumb_y + t_h > sb_bot) thumb_y = sb_bot - t_h;
                int thumb_x = sb_x + (arrow_w > 0 ? (arrow_w - t_w) / 2 : 0);
                if (thumb_x >= 0 && thumb_y >= 0 && thumb_x + t_w <= 640 && thumb_y + t_h <= 480) {
                    unsigned short *tdst = frame + thumb_y * 640 + thumb_x;
                    unsigned char *gray_px = (unsigned char *)DAT_00489e94;
                    int tp = t_pb;
                    for (int row = 0; row < t_h; row++) {
                        for (int col = 0; col < t_w; col++) {
                            unsigned char g = gray_px[tp++];
                            if (g != 0) {
                                unsigned short r = (g >> 5) & 0x07;
                                unsigned short gn = (g >> 2) & 0x3F;
                                unsigned short b = (g >> 3) & 0x1F;
                                tdst[col] = (r << 11) | (gn << 5) | b;
                            }
                        }
                        tdst += 640;
                    }
                }
            }
        }
    }

    /* ---- Draw mouse cursor sprite (0x22) on top of everything ---- */
    /* Original: FUN_00428650 draws sprite 0x22 during menu/gameplay rendering.
     * Hide during intro sequence (g_GameState 0x96/0x97) where cursor is irrelevant. */
    if (g_GameState < 0x90 &&
        DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
        int cur_sprite = 0x22;  /* cursor sprite index (decimal 34) */
        int cur_w = (int)((unsigned char *)DAT_00489e8c)[cur_sprite];
        int cur_h = (int)((unsigned char *)DAT_00489e88)[cur_sprite];

        if (cur_w > 0 && cur_h > 0) {
            int cx = (g_MouseDeltaX >> 18);
            int cy = (g_MouseDeltaY >> 18) - 9;  /* hotspot offset ~9px up */

            int pixel_base = ((int *)DAT_00489234)[cur_sprite];
            unsigned short *src_pixels = (unsigned short *)DAT_00487ab4;

            /* Draw with clipping and transparency */
            for (int row = 0; row < cur_h; row++) {
                int dy = cy + row;
                for (int col = 0; col < cur_w; col++) {
                    unsigned short pixel = src_pixels[pixel_base + row * cur_w + col];
                    int dx = cx + col;
                    if (pixel != 0 && dx >= 0 && dx < 640 && dy >= 0 && dy < 480) {
                        frame[dy * 640 + dx] = pixel;
                    }
                }
            }
        }
    }
}

/* ===== Render_Game_View (0042F3A0) - Init-time check ===== */
/* Called once during one-shot init to verify buffers are valid.
 * Actual rendering now happens in Render_Game_View_To via Render_Frame. */
int Render_Game_View(void)
{
    if (!Software_Buffer || !g_GameViewData)
        return 0;
    return 1;
}

/* ===== Font color palettes (DAT_004878f0) ===== */
/* 14 palettes (indices 0-13). Each is a 256-entry RGB565 ramp.
 * Font pixel luminosity (0-255) indexes into the ramp for anti-aliased text.
 * Original: FUN_0041e580 generates palettes 0-5 and 9-12 at startup.
 * Palettes 6-8 are dynamic (built per-frame for special styles >= 0xFA).
 * Palette 13 is unused. */
static unsigned short Font_Palettes[14][256];
static int Font_Palettes_Built = 0;

/* Helper: pack R8,G8,B8 to RGB565 */
static inline unsigned short PackRGB565(int r, int g, int b)
{
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b >> 3)));
}

static void Build_Font_Palettes(void)
{
    /* Palette 0: Golden yellow (two-phase ramp, matches FUN_0041e580)
     * First half: pure saturated yellow (no blue)
     * Second half: transitions toward white by adding blue */
    Font_Palettes[0][0] = 0;
    for (int i = 1; i < 128; i++) {
        Font_Palettes[0][i] = PackRGB565(i, i, 0);
    }
    for (int i = 128; i < 256; i++) {
        int step = i - 128;
        Font_Palettes[0][i] = PackRGB565(i, i, step * 2);
    }

    /* Palette 1: White (pure grayscale ramp) */
    Font_Palettes[1][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[1][i] = PackRGB565(i, i, i);
    }

    /* Palette 2: Cyan (R=0, G=i*0.8, B=i) */
    Font_Palettes[2][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[2][i] = PackRGB565(0, (int)(i * 0.8), i);
    }

    /* Palette 3: Dim yellow (R=i*2/3, G=i*2/3, B=i/3) */
    Font_Palettes[3][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[3][i] = PackRGB565((int)(i * 0.667), (int)(i * 0.667), i / 3);
    }

    /* Palette 4: Dark blue (R=i*0.1, G=i*0.5, B=i*0.8) */
    Font_Palettes[4][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[4][i] = PackRGB565((int)(i * 0.1), (int)(i * 0.5), (int)(i * 0.8));
    }

    /* Palette 5: Warm peach/skin (two-phase ramp)
     * First half: R=i, G=i*0.8, B=i*0.6
     * Second half: fade from warm (128,102,77) toward white */
    Font_Palettes[5][0] = 0;
    for (int i = 1; i < 128; i++) {
        Font_Palettes[5][i] = PackRGB565(i, (int)(i * 0.8), (int)(i * 0.6));
    }
    for (int i = 128; i < 256; i++) {
        double norm = (i - 128) / 128.0;
        int r = (int)(128.0 + norm * 127.0);
        int g = (int)(102.4 + norm * 153.6);
        int b = (int)(76.8 + norm * 179.2);
        Font_Palettes[5][i] = PackRGB565(r, g, b);
    }

    /* Palettes 6-8: Reserved for dynamic generation (left as black) */

    /* Palette 9: Red (R=i, G=i*0.2, B=i*0.2) */
    Font_Palettes[9][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[9][i] = PackRGB565(i, (int)(i * 0.2), (int)(i * 0.2));
    }

    /* Palette 10: Orange (R=i, G=i*0.8, B=i*0.2) */
    Font_Palettes[10][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[10][i] = PackRGB565(i, (int)(i * 0.8), (int)(i * 0.2));
    }

    /* Palette 11: Bright cyan (R=i*0.2, G=i, B=i) */
    Font_Palettes[11][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[11][i] = PackRGB565((int)(i * 0.2), i, i);
    }

    /* Palette 12: Blue (R=i*0.3, G=i*0.6, B=i*0.9) */
    Font_Palettes[12][0] = 0;
    for (int i = 1; i < 256; i++) {
        Font_Palettes[12][i] = PackRGB565((int)(i * 0.3), (int)(i * 0.6), (int)(i * 0.9));
    }

    Font_Palettes_Built = 1;
}

/* ===== Draw_Text_To_Buffer (0040AED0) ===== */
/* Params: str, font_idx, color_idx, dest_buf, stride, hover_brightness, max_width, len
 * hover_brightness (0-255): lerps palette towards white for hover highlight.
 * max_width: 0=no word-wrap, >0=wrap at that pixel width.
 * len: 0=auto strlen, >0=exact char count. */
void Draw_Text_To_Buffer(const char *str, int font_idx, int color_idx,
                         unsigned short *dest_buf, int stride, int hover,
                         int max_width, int len)
{
    if (!str || !Font_Pixel_Data)
        return;

    if (!Font_Palettes_Built)
        Build_Font_Palettes();

    /* Select base palette (clamp to valid range, 14 palettes: 0-13) */
    if (color_idx < 0 || color_idx >= 14)
        color_idx = 0;
    unsigned short *palette = Font_Palettes[color_idx];

    /* Team color palettes (6, 7, 8): dynamically build from DAT_00483838.
     * Original binary does this inline when color_idx >= 0xFA; our callers
     * pass 6/7/8 directly.  Build a 256-entry ramp each time so it always
     * reflects the current team colors (X1R5G5B5 in DAT_00483838). */
    unsigned short team_palette[256];
    if (color_idx >= 6 && color_idx <= 8) {
        int team_idx = color_idx - 6;
        unsigned short tc = DAT_00483838[team_idx];
        /* Original 0040AF2C-0040AF59 performs each shift in an 8-bit
         * register.  The truncation discards the neighboring X1R5G5B5
         * channels.  Integer promotion in the old reconstruction retained
         * those bits, saturating the ramps and making the scoreboard team
         * labels effectively black. */
        unsigned int tc_r = ((tc >> 10) & 0x1F) << 3;
        unsigned int tc_g = ((tc >> 5)  & 0x1F) << 3;
        unsigned int tc_b = ( tc        & 0x1F) << 3;

        /* First 128 entries: fade black → team color */
        int accR = 0, accG = 0, accB = 0;
        for (int j = 0; j < 128; j++) {
            int cR = (accR + ((accR >> 31) & 0x7F)) >> 7;
            int cG = (accG + ((accG >> 31) & 0x7F)) >> 7;
            int cB = (accB + ((accB >> 31) & 0x7F)) >> 7;
            if (cR > 255) cR = 255;
            if (cG > 255) cG = 255;
            if (cB > 255) cB = 255;
            team_palette[j] = (unsigned short)(((cR & 0xF8) << 8) |
                                                ((cG & 0xFC) << 3) | (cB >> 3));
            accR += tc_r;
            accG += tc_g;
            accB += tc_b;
        }

        /* Next 128 entries: fade team color → white */
        int fadeR = 0, fadeG = 0, fadeB = 0;
        for (int j = 128; j < 256; j++) {
            int cR = ((fadeR + ((fadeR >> 31) & 0x7F)) >> 7) + (int)tc_r;
            int cG = ((fadeG + ((fadeG >> 31) & 0x7F)) >> 7) + (int)tc_g;
            int cB = ((fadeB + ((fadeB >> 31) & 0x7F)) >> 7) + (int)tc_b;
            if (cR > 255) cR = 255;
            if (cG > 255) cG = 255;
            if (cB > 255) cB = 255;
            team_palette[j] = (unsigned short)(((cR & 0xF8) << 8) |
                                                ((cG & 0xFC) << 3) | (cB >> 3));
            fadeR += (255 - (int)tc_r);
            fadeG += (255 - (int)tc_g);
            fadeB += (255 - (int)tc_b);
        }
        palette = team_palette;
    }

    /* If hovering, build a brightened palette (lerp towards white) */
    unsigned short hover_palette[256];
    if (hover > 0) {
        if (hover > 255) hover = 255;
        for (int i = 0; i < 256; i++) {
            unsigned short c = palette[i];
            int r = ((c >> 11) & 0x1F) << 3;
            int g = ((c >> 5) & 0x3F) << 2;
            int b = (c & 0x1F) << 3;
            r = r + (((255 - r) * hover) >> 8);
            g = g + (((255 - g) * hover) >> 8);
            b = b + (((255 - b) * hover) >> 8);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            hover_palette[i] = (unsigned short)(((r & 0xF8) << 8) |
                                                 ((g & 0xFC) << 3) |
                                                 ((b & 0xF8) >> 3));
        }
        palette = hover_palette;
    }

    /* Calculate string length if not provided */
    int slen = len;
    if (slen == 0) {
        const char *p = str;
        while (*p++) slen++;
    }

    int cur_x = 0;
    for (int si = 0; si < slen; si++) {
        unsigned char c = (unsigned char)str[si];

        if (c == ' ') {
            cur_x += Font_Char_Table[font_idx * 256 + 32].width;
            continue;
        }

        int table_idx = font_idx * 256 + c;
        if (table_idx >= 1024) continue;

        FontChar *fc = &Font_Char_Table[table_idx];
        if (fc->width == 0) continue;

        for (int fy = 0; fy < fc->height; fy++) {
            for (int fx = 0; fx < fc->width; fx++) {
                unsigned char luma = Font_Pixel_Data[fc->pixel_offset + fy * fc->width + fx];
                if (luma != 0) {
                    dest_buf[fy * stride + cur_x + fx] = palette[luma];
                }
            }
        }
        cur_x += fc->width + 1;
    }
}
