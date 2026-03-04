/*
 * graphics.cpp - DirectDraw surfaces, rendering pipeline
 * Addresses: Init_DirectDraw=004610E0, Render_Frame=0045D800,
 *            Release_DirectDraw_Surfaces=004610A0, Restore_Surfaces=00461070
 *
 * COMPAT HACK: Windowed mode + RGB565->RGB32 conversion for modern Windows.
 * Original binary uses exclusive fullscreen 16bpp which doesn't work on
 * Windows 10/11 (broken colors, mode not supported).
 */
#include "tou.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Globals defined in this module ===== */
int                 DAT_00489238    = 640;   /* Screen/viewport width */
int                 DAT_0048923c    = 480;   /* Screen/viewport height */

LPDIRECTDRAW        lpDD            = NULL;  /* 00489EC8 */
LPDIRECTDRAWSURFACE lpDDS_Primary   = NULL;  /* 00489ED8 */
LPDIRECTDRAWSURFACE lpDDS_Back      = NULL;  /* 00489ECC */
LPDIRECTDRAWSURFACE lpDDS_Offscreen = NULL;  /* 00489ED0 */
LPDIRECTDRAWSURFACE DAT_00481d44    = NULL;  /* 00481D44 - offscreen surface 640x480 */

/* ===== Init_DirectDraw (004610E0) ===== */
/*
 * Original: SetDisplayMode(w,h,16) + primary flip chain + offscreen 16bpp
 * Compat:   Windowed mode, no mode change, primary + clipper + offscreen 32bpp
 */
int Init_DirectDraw(int width, int height)
{
    HRESULT hr;
    DDSURFACEDESC ddsd;

    if (lpDD == NULL) {
        LOG("[GFX] Init_DirectDraw: lpDD is NULL!\n");
        return 0;
    }

    LOG("[GFX] Init_DirectDraw(%d, %d) - windowed mode\n", width, height);

    /* COMPAT: No SetDisplayMode - stay in desktop resolution/depth */

    /* Create primary surface (no flip chain in windowed mode) */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    hr = lpDD->CreateSurface(&ddsd, &lpDDS_Primary, NULL);
    if (hr != DD_OK) {
        LOG("[GFX] CreateSurface(Primary) failed: 0x%08X\n", hr);
        return 0;
    }
    LOG("[GFX] Primary surface created OK\n");

    /* Create and attach clipper for windowed rendering */
    LPDIRECTDRAWCLIPPER lpClipper = NULL;
    hr = lpDD->CreateClipper(0, &lpClipper, NULL);
    if (hr != DD_OK)
        return 0;

    lpClipper->SetHWnd(0, hWnd_Main);
    lpDDS_Primary->SetClipper(lpClipper);
    lpClipper->Release();

    /* No back buffer in windowed mode */
    lpDDS_Back = NULL;

    /* Create offscreen surface in system memory (matches desktop format) */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth        = width;
    ddsd.dwHeight       = height;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

    hr = lpDD->CreateSurface(&ddsd, &lpDDS_Offscreen, NULL);
    if (hr != DD_OK) {
        LOG("[GFX] CreateSurface(Offscreen) failed: 0x%08X\n", hr);
        return 0;
    }
    LOG("[GFX] Offscreen surface created OK\n");

    g_SurfaceReady = 2;
    LOG("[GFX] Init_DirectDraw success\n");
    return 1;
}

/* ===== Release_DirectDraw_Surfaces (004610A0) ===== */
void Release_DirectDraw_Surfaces(void)
{
    if (lpDD != NULL) {
        if (lpDDS_Primary != NULL) {
            lpDDS_Primary->Release();
            lpDDS_Primary = NULL;
        }
        if (lpDDS_Offscreen != NULL) {
            lpDDS_Offscreen->Release();
            lpDDS_Offscreen = NULL;
        }
    }
}

/* ===== Restore_Surfaces (00461070) ===== */
void Restore_Surfaces(void)
{
    if (lpDDS_Primary)   lpDDS_Primary->Restore();
    if (lpDDS_Back)      lpDDS_Back->Restore();
    if (lpDDS_Offscreen) lpDDS_Offscreen->Restore();
    g_SurfaceReady = 2;
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
    int poff = param_3 * 0x598;
    unsigned int timer = (unsigned int)*(unsigned char *)(poff + 0x4A3 + DAT_00487810);

    /* Screen position: entity world pos (fixed-point >> 18) minus camera offset */
    int screen_y = (*(int *)(poff + 4 + DAT_00487810) >> 0x12) - DAT_004806e0;
    int screen_x = (*(int *)(poff + DAT_00487810) >> 0x12) - DAT_004806dc;

    /* 4 crosses at different radii, all quadratic in timer */
    unsigned int r1 = (timer * timer) / 0xc;
    FUN_00408a60(param_1, param_2, (int)(r1 + 0x12), screen_x, screen_y);
    FUN_00408a60(param_1, param_2, (int)(r1 + 0x0e), screen_x, screen_y);

    timer = (unsigned int)*(unsigned char *)(poff + 0x4A3 + DAT_00487810);
    FUN_00408a60(param_1, param_2, (int)((timer * timer) >> 3) + 0x12, screen_x, screen_y);

    timer = (unsigned int)*(unsigned char *)(poff + 0x4A3 + DAT_00487810);
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
static void Render_Game_World(unsigned short *buffer, int stride)
{
    if (!DAT_00481f50 || DAT_004879f0 == 0 || DAT_004879f4 == 0)
        return;


    unsigned short *src = (unsigned short *)DAT_00481f50;
    int shift = DAT_00487a18 & 0x1F;

    /* Viewport dimensions: full screen (single player) */
    int vp_w = 640;
    int vp_h = 480;

    /* Clamp to available map area (minus 7-pixel borders each side) */
    int avail_w = (int)DAT_004879f0 - 14;
    int avail_h = (int)DAT_004879f4 - 14;
    if (vp_w > avail_w) vp_w = avail_w;
    if (vp_h > avail_h) vp_h = avail_h;

    /* Camera: center viewport on player position (FUN_00407720 logic).
     * Player position is in fixed-point 14.18 format (>> 18 = pixels).
     * Falls back to map center if no active player. */
    int vp_left, vp_top;
    if (DAT_00487808 > 0 && DAT_00487810 != 0) {
        int pidx = DAT_004877f8[0];
        int poff = pidx * 0x598;
        int player_x = *(int *)(DAT_00487810 + poff);       /* +0x00: X position */
        int player_y = *(int *)(DAT_00487810 + poff + 4);   /* +0x04: Y position */

        /* Read per-player viewport dimensions (if set) */
        int pvp_w = *(int *)(DAT_00487810 + poff + 0x484);
        int pvp_h = *(int *)(DAT_00487810 + poff + 0x488);
        if (pvp_w > 0 && pvp_h > 0) {
            vp_w = pvp_w;
            vp_h = pvp_h;
            if (vp_w > avail_w) vp_w = avail_w;
            if (vp_h > avail_h) vp_h = avail_h;
        }

        /* Center viewport on player (>> 18 converts fixed-point to pixels) */
        vp_left = (player_x >> 18) - vp_w / 2;
        vp_top  = (player_y >> 18) - vp_h / 2;
    } else {
        /* Fallback: center on map */
        vp_left = ((int)DAT_004879f0 - vp_w) / 2;
        vp_top  = ((int)DAT_004879f4 - vp_h) / 2;
    }

    /* Clamp to 7-pixel border (matches original clamping logic) */
    if (vp_left < 7) vp_left = 7;
    if (vp_top  < 7) vp_top  = 7;
    if (vp_left + vp_w > (int)DAT_004879f0 - 7)
        vp_left = (int)DAT_004879f0 - 7 - vp_w;
    if (vp_top + vp_h > (int)DAT_004879f4 - 7)
        vp_top = (int)DAT_004879f4 - 7 - vp_h;

    /* Screen shake (if player +0xC4 flag set) */
    if (DAT_00487808 > 0 && DAT_00487810 != 0) {
        int poff = DAT_004877f8[0] * 0x598;
        if (*(char *)(DAT_00487810 + poff + 0xC4) != 0) {
            vp_left += (rand() % 6) - 3;
            vp_top  += (rand() % 6) - 3;
            /* Re-clamp after shake */
            if (vp_left < 7) vp_left = 7;
            if (vp_top  < 7) vp_top  = 7;
            if (vp_left + vp_w > (int)DAT_004879f0 - 7)
                vp_left = (int)DAT_004879f0 - 7 - vp_w;
            if (vp_top + vp_h > (int)DAT_004879f4 - 7)
                vp_top = (int)DAT_004879f4 - 7 - vp_h;
        }
    }

    /* Force even viewport width */
    if (vp_w & 1) vp_w--;

    /* Set viewport globals (used by entity/particle renderers) */
    DAT_004806d8 = vp_w;           /* viewport width */
    DAT_004806e4 = vp_h;           /* viewport height */
    DAT_004806dc = vp_left;        /* viewport left (map coords) */
    DAT_004806e0 = vp_top;         /* viewport top (map coords) */
    DAT_004806d0 = vp_left + vp_w; /* viewport right */
    DAT_004806d4 = vp_top + vp_h;  /* viewport bottom */
    DAT_004806ec = 0;              /* screen X offset (0 for single player) */
    DAT_004806e8 = 0;              /* screen Y offset (0 for single player) */

    /* Sky pattern fill — tiles a sprite across the entire buffer as background.
     * In the original (FUN_00407720), this is a one-shot triggered by DAT_00489298
     * because the DDraw surface persists between frames. Our decomp uses a fresh
     * scratch buffer each frame, so we fill every frame.
     *
     * Sky type (g_ConfigBlob[0x1803] = byte 3 of DAT_00483758):
     *   0 → sprite 0x40, 1 → sprite 0x45, 2 → sprite 0x46 (default)
     *   3 → solid color 0x446, ≥4 → black */
    {
        unsigned char sky_type = g_ConfigBlob[0x1803];
        /* DIAG: Log sky fill state once */
        if (sky_type < 3 && DAT_00487ab4 && DAT_00489234 && DAT_00489e8c && DAT_00489e88) {
            /* Tile a sprite across the buffer */
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
                        if (src_x >= src_y + spr_w) {
                            src_x = src_y;  /* wrap X */
                        }
                    }
                    src_y += spr_w;
                    if (src_y >= src_y_end) {
                        src_y = spr_base;  /* wrap Y */
                    }
                }
            } else {
                memset(buffer, 0, 640 * 480 * 2);
            }
        } else if (sky_type == 3) {
            /* Type 3: solid black (original: -(ushort)(type!=3) & 0x446 = 0) */
            memset(buffer, 0, 640 * 480 * 2);
        } else {
            /* Type >= 4: solid dark blue (original: 0xFFFF & 0x446 = 0x0446) */
            unsigned short *d = buffer;
            for (int i = 0; i < 640 * 480; i++) {
                *d++ = 0x0446;
            }
        }
    }

    /* Blit level background from stride-aligned source to screen buffer.
     * Zero pixels (0x0000) in the level background represent empty/sky areas.
     *
     * Two modes (matching original FUN_00407720 viewport blit):
     *   DAT_00483960 != 0 → FUN_0040c0a0 path: per-level sky from .SWP file
     *     composites sky image behind transparent tile pixels
     *   DAT_00483960 == 0 → direct blit, sky comes from tiled sprite fill above */
    if (DAT_00483960 != 0 && DAT_00489ea0 != NULL) {
        /* Per-level sky compositing (FUN_0040c0a0):
         * Uses parallax scrolling — sky scrolls slower than the map.
         * Formula: sky_pos = viewport_pos * (sky_size - vp_size) / (map_size - vp_size)
         * If sky is bigger than map in either axis, do simple rect copy instead. */
        unsigned short *sky = (unsigned short *)DAT_00489ea0;
        int sky_w = DAT_00487a0c;
        int sky_h = DAT_00487a10;

        float ratio_x = (sky_w > vp_w) ? (float)(DAT_004879f0 - vp_w) / (float)(sky_w - vp_w) : 0.0f;
        float ratio_y = (sky_h > vp_h) ? (float)(DAT_004879f4 - vp_h) / (float)(sky_h - vp_h) : 0.0f;

        if (ratio_x < 1.0f || ratio_y < 1.0f) {
            /* Sky bigger than map: simple rect copy from sky buffer (no parallax needed) */
            int tile_src = (vp_top << ((unsigned char)DAT_00487a18 & 0x1F)) + vp_left;
            unsigned short *sky_src = (unsigned short *)((int)DAT_00481f50 + tile_src * 2);
            unsigned short *dst = buffer + (DAT_004806e8 * stride + DAT_004806ec);
            int sky_stride = DAT_00487a00;
            for (int y = 0; y < vp_h; y++) {
                memcpy(dst, sky_src, vp_w * 2);
                dst += stride;
                sky_src += sky_stride;
            }
        } else {
            /* Parallax compositing: compute sky start offset from viewport position */
            int sky_x_start = (int)((float)vp_left / ratio_x);
            int sky_y_start = (int)((float)vp_top / ratio_y);
            int sky_offset = sky_y_start * sky_w + sky_x_start;
            int tile_stride = DAT_00487a00;

            unsigned short *tile_row = src + (vp_top << ((unsigned char)DAT_00487a18 & 0x1F)) + vp_left;
            unsigned short *dst_row = buffer + (DAT_004806e8 * stride + DAT_004806ec);
            int sky_row_offset = sky_offset;

            for (int y = 0; y < vp_h; y++) {
                int sky_col = sky_row_offset;
                for (int x = 0; x < vp_w; x++) {
                    unsigned short tile_px = tile_row[x];
                    if (tile_px == 0) {
                        dst_row[x] = sky[sky_col];
                    } else {
                        dst_row[x] = tile_px;
                    }
                    sky_col++;
                }
                dst_row += stride;
                tile_row += tile_stride;
                sky_row_offset += sky_w;
            }
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
    FUN_0040dbd0((int)buffer, stride);       /* Static entities (turrets) */
    FUN_0040dce0((int)buffer, stride);       /* Dynamic entities (troopers) */
    FUN_0040bb60((unsigned int)buffer, stride); /* Main entities (items/ships) */
    FUN_0040a870((int)buffer, stride);       /* Projectiles */
    FUN_0040d6c0((int)buffer, stride);       /* Explosions */
    FUN_0040d810((int)buffer, stride);       /* Debris/particles */
    FUN_0040caf0((int)buffer, stride);       /* Player/ship */
    FUN_0040d930((int)buffer, stride);       /* Misc effects (glow/smoke) */
    FUN_0040d360((int)buffer, stride);       /* Edge tiles/detail */
    FUN_0040d100((int)buffer, stride);       /* Darkness/fog overlay */

    /* Spawn shield overlay — draw contracting cross effect for spawning players */
    if (DAT_00487810 != 0) {
        for (int p = 0; p < DAT_00489240; p++) {
            int poff = p * 0x598;
            unsigned char timer = *(unsigned char *)(DAT_00487810 + poff + 0x4A3);
            if (timer != 0 && timer < 0x2F &&
                *(char *)(DAT_00487810 + poff + 0x24) == '\0') {
                FUN_00408ea0((int)buffer, stride, p);
            }
        }
    }

    /* ---- Pause / overlay states (end of FUN_00407720) ---- */
    if (g_SubState != 0) {
        if (g_SubState == 1) {
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
        else if (g_SubState == 2) {
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
    }
}

/* ===== Render_Frame (0045D800) ===== */
/*
 * Original pipeline:
 *   1. Lock offscreen surface (16bpp)
 *   2. Copy Software_Buffer frame to surface (RGB565 -> RGB565)
 *   3. Call FUN_004076d0 to draw particles/entities on surface (RGB565)
 *   4. Unlock + Blt/Flip
 *
 * Compat pipeline:
 *   1. Copy Software_Buffer frame to scratch buffer (RGB565)
 *   2. Call FUN_004076d0 to draw particles on scratch buffer (RGB565)
 *   3. Lock offscreen surface (32bpp)
 *   4. Convert scratch buffer RGB565 -> ARGB8888 to surface
 *   5. Unlock + Blt to primary (windowed)
 */
void Render_Frame(void)
{
    HRESULT hr;
    DDSURFACEDESC ddsd;

    if (lpDDS_Offscreen == NULL || lpDDS_Primary == NULL)
        return;

    /* Allocate scratch buffer on first use */
    if (g_ScratchBuffer == NULL) {
        g_ScratchBuffer = (unsigned short *)malloc(640 * 480 * 2);
        if (!g_ScratchBuffer) return;
    }

    /* 1. Draw background into scratch buffer.
     *    Gameplay state (g_GameState==0) with level data: blit level background.
     *    Menu/intro: copy Software_Buffer (JPEG background). */
    if (g_GameState == 0 && DAT_00481f50 != NULL) {
        /* Game world: blit visible viewport from level background */
        Render_Game_World(g_ScratchBuffer, 640);
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
    if (g_GameState != 0) {
        Render_Game_View_To(g_ScratchBuffer);
        FUN_004076d0((int)g_ScratchBuffer, 640);
    }

    /* 3. Lock the offscreen surface */
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    do {
        hr = lpDDS_Offscreen->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            Restore_Surfaces();
            return;
        }
    } while (hr == DDERR_WASSTILLDRAWING);

    if (hr != DD_OK)
        return;

    /* 4. Convert scratch buffer (RGB565) to surface (32bpp ARGB) */
    int pitchBytes = ddsd.lPitch;

    for (int y = 0; y < 480; y++) {
        unsigned int *dst = (unsigned int *)((char *)ddsd.lpSurface + y * pitchBytes);
        for (int x = 0; x < 640; x++) {
            unsigned short pixel = g_ScratchBuffer[y * 640 + x];
            /* RGB565 -> ARGB8888 */
            unsigned char r = (pixel >> 11) & 0x1F;
            unsigned char g = (pixel >> 5)  & 0x3F;
            unsigned char b = pixel & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            dst[x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    /* 5. Unlock */
    lpDDS_Offscreen->Unlock(NULL);

    /* 6. Blt offscreen to primary (windowed - need screen coordinates) */
    RECT rcSrc = {0, 0, 640, 480};
    POINT pt = {0, 0};
    ClientToScreen(hWnd_Main, &pt);
    RECT rcDest = {pt.x, pt.y, pt.x + 640, pt.y + 480};

    do {
        hr = lpDDS_Primary->Blt(&rcDest, lpDDS_Offscreen, &rcSrc, DDBLT_WAIT, NULL);
        if (hr == DDERR_SURFACELOST) {
            Restore_Surfaces();
            return;
        }
    } while (hr == DDERR_WASSTILLDRAWING);
}

/* ===== Render_Game_View_To - Draw menu items onto a target buffer ===== */
/* Called every frame by Render_Frame on the scratch buffer.
 * Original pipeline: Lock DDraw surface → copy Software_Buffer (clean bg) →
 *   draw menu items fresh onto surface → unlock → Blt.
 * COMPAT: Scratch buffer replaces the DDraw surface. */
void Render_Game_View_To(unsigned short *frame)
{
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
            case 0x00: case 0x0C: case 0x20: case 0x21: case 0x22:
            case 0x23: case 0x24: case 0x25: case 0x2B: case 0x2C:
            case 0x2D: case 0x2E: case 0x2F:
                if (g_MenuStrings && item->string_idx < 350 &&
                    g_MenuStrings[item->string_idx])
                    str = g_MenuStrings[item->string_idx];
                break;

            /* Enum: string_idx + config_value */
            case 0x01: case 0x04: case 0x0F: case 0x10: case 0x11:
            case 0x12: case 0x17: case 0x1E: case 0x1F: case 0x27:
            case 0x30: case 0x33: case 0x26: case 0x1C: case 0x1D: {
                int val = cfgPtr ? (int)*cfgPtr : 0;
                int idx = item->string_idx + val;
                if (g_MenuStrings && idx >= 0 && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
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
                break;
            }

            /* One-based enum: string_idx + config_value - 1 */
            case 0x03: {
                int val = cfgPtr ? (int)*cfgPtr : 1;
                int idx = item->string_idx + val - 1;
                if (g_MenuStrings && idx >= 0 && idx < 350 && g_MenuStrings[idx])
                    str = g_MenuStrings[idx];
                break;
            }

            /* Numeric: sprintf the config value (with scaling) */
            case 0x05: case 0x09: case 0x0E: case 0x13: case 0x14:
            case 0x15: case 0x16: case 0x18: case 0x34: {
                int val = cfgPtr ? (int)*cfgPtr : 0;
                switch (item->render_mode) {
                case 0x09: val = (val > 0) ? val - 1 : 0; break;
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

            /* Color swatch: no text, draw colored rect */
            case 0x1B:
                /* TODO: draw color swatch rectangle */
                break;

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
