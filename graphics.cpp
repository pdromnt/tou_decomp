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

    /* 1. Copy current frame from Software_Buffer to scratch buffer.
     *    Software_Buffer holds the CLEAN background (JPEG).
     *    All overlays (menu items, particles) are drawn onto scratch each frame. */
    int frameOffset = (g_FrameIndex & 0xFF) * (640 * 480);
    unsigned short *src = Software_Buffer + frameOffset;
    memcpy(g_ScratchBuffer, src, 640 * 480 * 2);

    /* 2. Draw menu items onto scratch buffer (fresh each frame for hover updates) */
    Render_Game_View_To(g_ScratchBuffer);

    /* 3. Draw particles/entities onto scratch buffer (RGB565) */
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
            const char *str = NULL;

            switch (item->render_mode) {
            case 0x00: case 0x0C: case 0x20: case 0x21: case 0x22:
            case 0x23: case 0x24: case 0x25: case 0x2B: case 0x2C:
            case 0x2D: case 0x2E: case 0x2F: case 0x29:
                if (g_MenuStrings && g_MenuStrings[item->string_idx])
                    str = g_MenuStrings[item->string_idx];
                break;

            default:
                if (g_MenuStrings && g_MenuStrings[item->string_idx])
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
/* Each palette is a 256-entry RGB565 ramp from black to the target color.
 * Font pixel luminosity (0-255) indexes into the ramp for smooth anti-aliased text.
 * 14 palettes allocated at runtime in original (0x004878f0).
 * Built here at first use for simplicity. */
static unsigned short Font_Palettes[6][256];
static int Font_Palettes_Built = 0;

static void Build_Font_Palettes(void)
{
    /* Target colors (R8, G8, B8) for each color_idx.
     * These are the brightest color at luma=255; font pixel luma indexes the ramp. */
    static const unsigned char targets[][3] = {
        { 255, 255, 140 },  /* 0: golden yellow (headings, copyright) */
        { 255, 255, 255 },  /* 1: white         (info text) */
        {   0, 220, 235 },  /* 2: cyan          (clickable items) */
        { 200, 200, 200 },  /* 3: light gray    */
        { 255, 255,   0 },  /* 4: yellow        */
        { 255, 128,   0 },  /* 5: orange        */
    };

    for (int c = 0; c < 6; c++) {
        Font_Palettes[c][0] = 0;
        for (int i = 1; i < 256; i++) {
            int r = (targets[c][0] * i) / 255;
            int g = (targets[c][1] * i) / 255;
            int b = (targets[c][2] * i) / 255;
            Font_Palettes[c][i] = (unsigned short)(((r & 0xF8) << 8) |
                                                    ((g & 0xFC) << 3) |
                                                    ((b & 0xF8) >> 3));
        }
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

    /* Select base palette (clamp to valid range) */
    if (color_idx < 0 || color_idx >= 6)
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
