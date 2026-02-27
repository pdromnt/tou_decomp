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

/* ===== Globals defined in this module ===== */
LPDIRECTDRAW        lpDD            = NULL;  /* 00489EC8 */
LPDIRECTDRAWSURFACE lpDDS_Primary   = NULL;  /* 00489ED8 */
LPDIRECTDRAWSURFACE lpDDS_Back      = NULL;  /* 00489ECC */
LPDIRECTDRAWSURFACE lpDDS_Offscreen = NULL;  /* 00489ED0 */

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

    /* 1. Copy current frame from Software_Buffer to scratch buffer */
    int frameOffset = (g_FrameIndex & 0xFF) * (640 * 480);
    unsigned short *src = Software_Buffer + frameOffset;
    memcpy(g_ScratchBuffer, src, 640 * 480 * 2);

    /* 2. Draw particles/entities onto scratch buffer (RGB565) */
    FUN_004076d0((int)g_ScratchBuffer, 640);

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

/* ===== Render_Game_View (0042F3A0) ===== */
/* TODO: Phase 6 */
void Render_Game_View(void) {}

/* ===== Draw_Text_To_Buffer (0040AED0) ===== */
void Draw_Text_To_Buffer(const char *str, int font_idx, int color_idx,
                         unsigned short *dest_buf, int stride, int x,
                         int max_x, int len)
{
    if (!str || !Font_Pixel_Data)
        return;

    int cur_x = 0;
    while (*str) {
        unsigned char c = (unsigned char)*str++;

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
                unsigned char p = Font_Pixel_Data[fc->pixel_offset + (fy * fc->width) + fx];
                if (p > 10) {
                    unsigned short color = 0xFFFF;
                    if (color_idx == 1) color = 0xF800;
                    dest_buf[(fy * stride) + cur_x + fx] = color;
                }
            }
        }
        cur_x += fc->width + 1;
    }
}
