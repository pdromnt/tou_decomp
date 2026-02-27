/*
 * init.cpp - System initialization, DirectInput, game config
 * Addresses: System_Init_Check=0041D480, Early_Init_Vars=0041EAD0,
 *            Init_DirectInput=004620F0, Init_Game_Config=004207C0
 */
#include "tou.h"
#define INITGUID
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ===== Globals defined in this module ===== */

/* DirectInput */
LPDIRECTINPUT        lpDI           = NULL;  /* 00489ED4 */
LPDIRECTINPUTDEVICE  lpDI_Keyboard  = NULL;  /* 00489EE4 */
LPDIRECTINPUTDEVICE  lpDI_Mouse     = NULL;  /* 00489EC0 */
HANDLE               hMouseEvent    = NULL;  /* 00489EE0 */

/* Sound config */
unsigned char DAT_00483720[8] = {0x5A,0x46,1,1, 0x40,0,0,0}; /* [0]=music vol 90%, [1]=SFX vol 70%, [2]=flag, [3]=output type 1=DSOUND */

/* Display mode struct: {flags, current_mode, desired_mode, ...} */
unsigned char DAT_00487640[4] = {0, 0xFF, 5, 0};

/* Display mode table */
int g_DisplayWidth  = 0;    /* 00489238 */
int g_DisplayHeight = 0;    /* 0048923C */
int g_NumDisplayModes = 10; /* 00483C00 */

/* Resolution tables (filled in System_Init_Check) */
static int g_ModeWidths[10];    /* 00483C04 */
static int g_ModeHeights[10];   /* 00483C44 */

/* Misc init globals */
int          g_SoundEnabled = 0;     /* 00487649 */
DWORD        g_FrameTimer   = 0;     /* 004877F4 */
unsigned char DAT_004877b1  = 0;
unsigned char DAT_004877a4  = 0;
DWORD        DAT_004892b8   = 0;

/* Config globals (004207c0 area) */
static unsigned char g_ConfigBlob[6408]; /* 00481F58 - raw config data */

/* Team color palette (RGB555 values, 4 entries) - init'd in Init_Game_Config */
unsigned short DAT_00483838[4] = {0};

/* Stub globals referenced in init */
int DAT_0048764b = 0;
int DAT_0048764a = 0;

/* ===== Stub functions (to be decompiled later) ===== */
void FUN_0041eae0(void) {}
/* FUN_0045a060 moved to effects.cpp */
/* FUN_0045b2a0 moved to effects.cpp */
void FUN_0041fc10(void) {}
void FUN_0041fe70(void) {}  /* Entity AI function pointers - not needed for intro rendering */
void FUN_0041f900(void) {}
void FUN_00422a10(void) {}
void FUN_0042d8b0(void) {}

/* ===== FUN_004236f0 - Sprite color variant generator (004236F0) ===== */
/* Creates up to 4 color-tinted copies of a type-9 sprite at indices:
 *   sprite_index+0, sprite_index+100, sprite_index+200, sprite_index+300
 * Each variant is tinted using a team color from DAT_00483838[].
 * Uses a 3x3 color matrix (per-channel LUTs with sqrt-of-squares blend).
 * Returns the new pixel write cursor (becomes DAT_00481d28). */
static int FUN_004236f0(int sprite_index, int color_param)
{
    unsigned char w = ((unsigned char *)DAT_00489e8c)[sprite_index];
    unsigned char h = ((unsigned char *)DAT_00489e88)[sprite_index];
    int total_pixels = (int)w * (int)h;
    int original_offset = ((int *)DAT_00489234)[sprite_index];

    int num_variants;
    if (color_param < 4) {
        num_variants = 1;
    } else {
        num_variants = 4;
    }

    /* Phase 1: Create pixel data copies for all variants */
    if (num_variants == 4) {
        int write_cursor = original_offset;

        for (int v = 0; v < 4; v++) {
            int src_offset = original_offset;  /* always copy from original */

            ((int *)DAT_00489234)[sprite_index + v * 100] = write_cursor;
            ((unsigned char *)DAT_00489e8c)[sprite_index + v * 100] = w;
            ((unsigned char *)DAT_00489e88)[sprite_index + v * 100] = h;

            if (total_pixels > 0) {
                unsigned short *src = (unsigned short *)DAT_00487ab4 + src_offset;
                unsigned short *dst = (unsigned short *)DAT_00487ab4 + write_cursor;
                for (int p = 0; p < total_pixels; p++) {
                    dst[p] = src[p];
                }
                write_cursor += total_pixels;
            }
        }
    }

    int pixel_cursor = ((int *)DAT_00489234)[sprite_index];

    if (num_variants == 0) {
        return pixel_cursor;
    }

    /* Phase 2: Team color transformation (DISABLED - produces wrong colors).
     *
     * The original builds 9 interpolation LUTs (3 input channels × 3 output channels)
     * and applies a sqrt-of-sum-of-squares color matrix per pixel. Analysis of the
     * Ghidra decompilation shows:
     *   - Input 0 (R channel): palette targets [pal_R?, pal_G, pal_B] → [R_out, G_out, B_out]
     *   - Input 1 (G channel): color0 targets [r0, g0, b0] → [R_out, G_out, B_out]
     *   - Input 2 (B channel): color1 targets [r1, g1, b1] → [R_out, G_out, B_out]
     *
     * Issues preventing correct implementation:
     *   1. The palette R component (param_19[-1] in Ghidra) extraction is unclear -
     *      may be uninitialized (0) or set via code the decompiler doesn't show clearly.
     *   2. The original operates in X1R5G5B5 pixel format; our compat pipeline uses RGB565.
     *      The palette values at DAT_00483838 are in X1R5G5B5 format, but the channel
     *      mapping between the two formats requires careful handling.
     *   3. The previous implementation had input channels reversed (B,G,R instead of R,G,B)
     *      and wrong palette-to-target assignments, producing grayscale output.
     *
     * For now, all 4 variants are identical pixel copies of the original sprite.
     * The intro only uses variant 3 (sprite_base+300), so entities display with
     * original sprite colors. Team color tinting will be implemented properly in Phase 4.
     *
     * TODO: Fix color transformation by:
     *   - Properly extracting all 3 palette channels from X1R5G5B5 values
     *   - Mapping input channels as R(0), G(1), B(2) with correct target ordering
     *   - Converting palette targets from X1R5G5B5 to RGB565 color space
     */

    /* Advance pixel_cursor past all variant data */
    pixel_cursor += total_pixels * num_variants;

    return pixel_cursor;
}

/* ===== FUN_00423150 - Load all3.gfx sprites (00423150) ===== */
/* Reads sprite entries: type(1) + index(2) + skip(12) + width(2) + height(2) + skip(2) + RGB24 pixels.
 * Converts RGB24 to RGB565 for types 0,1,9; grayscale for types 2,3,4.
 * Fills DAT_00487ab4 (pixels), DAT_00489234 (offsets), DAT_00489e8c (widths), DAT_00489e88 (heights). */
static int FUN_00423150(void)
{
    FILE *f = fopen("data\\all3.gfx", "rb");
    if (!f) return 0;

    int max_spr_idx = 0;
    int spr_count = 0;
    unsigned char type_byte;
    while (fread(&type_byte, 1, 1, f) == 1) {
        unsigned short sprite_index;
        fread(&sprite_index, 1, 2, f);

        /* Skip 12 bytes */
        fread((unsigned char *)DAT_00481cf8, 1, 12, f);

        unsigned short spr_w, spr_h;
        fread(&spr_w, 1, 2, f);
        fread(&spr_h, 1, 2, f);

        /* Skip 2 bytes */
        fread((unsigned char *)DAT_00481cf8, 1, 2, f);

        /* Read RGB24 pixel data into temp buffer */
        int pixel_count = (int)spr_w * (int)spr_h;
        fread((unsigned char *)DAT_00481cf8, 1, pixel_count * 3, f);

        /* Store dimensions */
        ((unsigned char *)DAT_00489e8c)[sprite_index] = (unsigned char)spr_w;
        ((unsigned char *)DAT_00489e88)[sprite_index] = (unsigned char)spr_h;
        if ((int)sprite_index > max_spr_idx) max_spr_idx = (int)sprite_index;
        spr_count++;

        switch (type_byte) {
        case 0: case 1: {
            /* BGR24 -> RGB565 conversion.
             * all3.gfx stores pixels as BGR (byte0=B, byte1=G, byte2=R).
             * Original binary encoded to X1R5G5B5 for DirectDraw surfaces.
             * We encode to RGB565 to match our compat rendering pipeline. */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d28;
            unsigned char *src = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                unsigned char b_val = src[p * 3];
                unsigned char g_val = src[p * 3 + 1];
                unsigned char r_val = src[p * 3 + 2];
                unsigned short pixel = ((r_val & 0xF8) << 8)
                    | ((g_val & 0xFC) << 3) | (b_val >> 3);
                ((unsigned short *)DAT_00487ab4)[DAT_00481d28] = pixel;
                DAT_00481d28++;
            }
            break;
        }
        case 9: {
            /* BGR24 -> RGB565 conversion (same as case 0/1) */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d28;
            unsigned char *src9 = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                unsigned char b_val = src9[p * 3];
                unsigned char g_val = src9[p * 3 + 1];
                unsigned char r_val = src9[p * 3 + 2];
                unsigned short pixel = ((r_val & 0xF8) << 8)
                    | ((g_val & 0xFC) << 3) | (b_val >> 3);
                ((unsigned short *)DAT_00487ab4)[DAT_00481d28] = pixel;
                DAT_00481d28++;
            }
            /* Generate 4 color-tinted variants at +0, +100, +200, +300 */
            DAT_00481d28 = FUN_004236f0((int)sprite_index, 0xFF);
            break;
        }
        case 2: case 3: case 4: {
            /* Grayscale: average RGB / 3 -> 8-bit */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d24;
            unsigned char *src = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                int sum = (int)src[p * 3] + (int)src[p * 3 + 1] + (int)src[p * 3 + 2];
                /* MSVC-style divide by 3: (sum * 0x55555556) >> 32 */
                ((unsigned char *)DAT_00489e94)[DAT_00481d24] = (unsigned char)(sum / 3);
                DAT_00481d24++;
            }
            break;
        }
        }
    }

    fclose(f);
    LOG("[LOAD] all3.gfx: %d RGB pixels, %d grayscale pixels, max_spr_idx=%d\n",
        DAT_00481d28, DAT_00481d24, max_spr_idx);
    return 1;
}

/* ===== FUN_004254b0 - Load loadtime.dat entity types (004254B0) ===== */
/* Reads entity type definitions into DAT_00487abc (0x218 bytes per type, 128 types).
 * Per type: 6 x 20-byte animation frame blocks, 7-byte header, 6 x 16-byte animation property blocks. */
static int FUN_004254b0(void)
{
    FILE *f = fopen("data\\loadtime.dat", "rb");
    if (!f) return 0;

    unsigned char tmp[20];
    int offset = 0;
    unsigned char *base = (unsigned char *)DAT_00487abc;

    while (offset < 0x11030) {
        /* 6 animation frame data blocks, 20 bytes each, at type+4 */
        for (int a = 0; a < 6; a++) {
            fread(base + offset + 4 + a * 0x14, 1, 0x14, f);
        }

        /* 7-byte header */
        fread(tmp, 1, 7, f);
        base[offset + 0x7C] = tmp[0];
        base[offset + 0x7D] = tmp[1];
        *(int *)(base + offset + 0x80) = (int)tmp[3] * (int)tmp[3] * 100;
        *(int *)(base + offset + 0x84) = (int)tmp[4];
        *(int *)(base + offset + 0x144) = (int)tmp[5] * 256 + (int)tmp[6];

        /* 6 animation property blocks, 16 bytes each */
        for (int a = 0; a < 6; a++) {
            fread(tmp, 1, 16, f);
            int anim_base = offset + 0xAC + a * 4;

            *(int *)(base + offset + 0x88 + a * 4) = (int)tmp[0] - 0x80;
            base[offset + 0xA0 + a] = tmp[1];
            base[offset + 0xA6 + a] = tmp[2];
            *(int *)(base + anim_base) = (int)tmp[3] * 256 + (int)tmp[4];
            *(int *)(base + anim_base + 0x18) =
                ((int)tmp[5] * 256 + (int)tmp[6]) * 0x1400;
            *(int *)(base + anim_base + 0x30) =
                ((int)tmp[7] * 256 + (int)tmp[8]) / 3;
            *(int *)(base + anim_base + 0x48) =
                (int)tmp[9] * 256 + (int)tmp[10];
            base[offset + 0x124 + a] = tmp[11];
            base[offset + 0x12A + a] = tmp[12];
            base[offset + 0x130 + a] = tmp[13];
            *(int *)(base + anim_base + 0x60) =
                (int)tmp[14] * 256 + (int)tmp[15];
        }

        offset += 0x218;
    }

    fclose(f);
    LOG("[LOAD] loadtime.dat: %d entity types loaded\n", offset / 0x218);
    return 1;
}

/* ===== FUN_00422740 - Master data loader (00422740) ===== */
int FUN_00422740(void)
{
    /* Clear sprite tables */
    DAT_00481d28 = 0;
    DAT_00481d24 = 0;
    for (int i = 0; i < 20000; i++) {
        ((int *)DAT_00489234)[i] = 0;
        ((unsigned char *)DAT_00489e8c)[i] = 0;
        ((unsigned char *)DAT_00489e88)[i] = 0;
    }

    /* Init sin/cos lookup table */
    Init_Math_Tables((int *)DAT_00487ab0, 0x800);

    /* Load sprites from all3.gfx */
    if (FUN_00423150() != 1) return 0;

    /* Load explosion sprites */
    if (FUN_00422fc0() != 1) return 0;

    /* Load entity type definitions from loadtime.dat */
    FUN_004254b0();

    /* TODO: Load palettes (pal_col, shipal_col) - not critical for intro entities */
    /* TODO: Load taulu2.tau raw data into DAT_00489e90 */
    /* TODO: FUN_00422900 - load names.dat */

    return 1;
}

/* ===== FUN_00420be0 - Entity type post-processing (00420BE0) ===== */
/* Calculates width/3 and height/3 for entity type animation sprites */
void FUN_00420be0(void)
{
    unsigned char *base = (unsigned char *)DAT_00487abc;
    int offset = 0;
    while (offset < 0x11030) {
        for (int i = 0; i < 6; i++) {
            int sprite_idx = *(int *)(base + offset + 0xF4 + i * 4);
            if (sprite_idx < 30000) {
                int w = (int)((unsigned char *)DAT_00489e8c)[sprite_idx];
                int h = (int)((unsigned char *)DAT_00489e88)[sprite_idx];
                base[offset + 0x136 + i] = (unsigned char)(w / 3);
                base[offset + 0x13C + i] = (unsigned char)(h / 3);
            } else {
                base[offset + 0x136 + i] = 0;
                base[offset + 0x13C + i] = 0;
            }
        }
        offset += 0x218;
    }
}
void FUN_0041e580(void) {}
int  FUN_00414060(void) { return 1; }
void FUN_00413f70(void) {}
void FUN_0041e4a0(void) {}
/* FUN_0045d7d0 moved to stubs.cpp */
void FUN_00425fe0(void) {}
int  FUN_0042fc40(void) { return 1; }
void FUN_0042fc10(void) {}
void FUN_0041a8c0(void) {}
void FUN_0041d740(void) {}

/* ===== Early_Init_Vars (0041EAD0) ===== */
void Early_Init_Vars(void)
{
    g_MemoryTracker = 0;
}

/* ===== Init_Game_Config (004207C0) ===== */
void Init_Game_Config(void)
{
    /* Initialize config blob with defaults */
    g_ConfigBlob[0] = 1;     /* 00481F58 */
    g_ConfigBlob[1] = 0;     /* 00481F59 */
    g_ConfigBlob[2] = 1;     /* 00481F5A */
    g_ConfigBlob[3] = 0x32;  /* 00481F5B = 50 */

    /* Clear 800 bytes starting at offset 4 */
    memset(&g_ConfigBlob[4], 0, 800);

    /* Team color palette (RGB555) - used by FUN_004236f0 sprite variant generator */
    DAT_00483838[0] = 0x1A56;  /* Orange/gold: R=176, G=144, B=48 */
    DAT_00483838[1] = 0x2ACA;  /* Green:       R=80,  G=176, B=80 */
    DAT_00483838[2] = 0x6508;  /* Blue:        R=64,  G=64,  B=200 */
    DAT_00483838[3] = 0x6739;  /* Light gray:  R=200, G=200, B=200 */

    /* TODO: Full config initialization (entity defaults, key bindings, etc.)
     * See DECOMP_PLAN.md Phase 2 for details.
     * For now, enough to pass System_Init_Check. */

    /* TODO: FUN_0042f360() - Load options.cfg overlay */
}

/* ===== Init_Math_Tables (00425780) ===== */
/* Generates sin LUT with 1.25x entries for cosine offset access */
void Init_Math_Tables(int *buffer, unsigned int count)
{
    unsigned int total = count + (count >> 2); /* count * 1.25 */
    double two_pi = 6.283185307179586;

    for (unsigned int i = 0; i < total; i++) {
        double angle = ((double)i / (double)count) * two_pi;
        buffer[i] = (int)(sin(angle) * 524288.0); /* scale = 2^19 = 0x80000 */
    }
}

/* ===== System_Init_Check (0041D480) ===== */
/* Returns: 1=success, 0=file-not-found, 2=no-levels */
int System_Init_Check(void)
{
    DWORD dwTime;
    int iVar2;

    dwTime = timeGetTime();
    srand(dwTime);  /* FUN_00464409 = srand */

    Init_Memory_Pools();
    Init_Math_Tables((int *)DAT_00487ab0, 0x800);
    Init_Game_Config();

    g_LoadedBgIndex = (char)0xFF;
    g_SoundEnabled = 1;

    /* Check sound config - byte+3: 3=NOSOUND */
    if (DAT_00483720[3] != 3) {
        iVar2 = Init_Sound_Hardware();
        if (iVar2 != 0)
            goto MainInit;
    }
    g_SoundEnabled = 0;

MainInit:
    FUN_0041eae0();  /* Entity table init */
    FUN_0045a060();  /* Color LUT generation */
    FUN_0045b2a0();  /* Blend LUT generation */

    /* Initialize physics params (80-byte heap buffer) */
    /* Byte offsets from Ghidra decompilation */
    *(int *)((char *)g_PhysicsParams + 0x08) = 0x19000;
    *(int *)((char *)g_PhysicsParams + 0x04) = 0xC0000;
    *(int *)((char *)g_PhysicsParams + 0x00) = 0;
    *(int *)((char *)g_PhysicsParams + 0x0C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x18) = 0x7800;
    *(int *)((char *)g_PhysicsParams + 0x14) = 0x80000;
    *(int *)((char *)g_PhysicsParams + 0x10) = 0x32;
    *(int *)((char *)g_PhysicsParams + 0x1C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x28) = 0;
    *(int *)((char *)g_PhysicsParams + 0x24) = 0x40000;
    *(int *)((char *)g_PhysicsParams + 0x20) = 0;
    *(int *)((char *)g_PhysicsParams + 0x2C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x38) = 0;
    *(int *)((char *)g_PhysicsParams + 0x34) = 0x40000;
    *(int *)((char *)g_PhysicsParams + 0x30) = 0;
    *(int *)((char *)g_PhysicsParams + 0x3C) = 0x2710000;

    /* Initialize entity config (40-byte heap buffer) */
    g_EntityConfig[0] = 0x708;
    *((unsigned char *)(g_EntityConfig + 1)) = 8;
    *((unsigned char *)g_EntityConfig + 5) = 4;
    g_EntityConfig[2] = 0x76C;
    *((unsigned char *)(g_EntityConfig + 3)) = 8;
    *((unsigned char *)g_EntityConfig + 0x0D) = 4;
    g_EntityConfig[4] = 0xAF0;
    *((unsigned char *)(g_EntityConfig + 5)) = 0x0C;
    *((unsigned char *)g_EntityConfig + 0x15) = 2;

    FUN_0041fc10();
    FUN_0041fe70();
    FUN_0041f900();

    /* Clear entity array: DAT_00487ac0, 100-byte steps up to 0x489230 */
    /* TODO: implement when we have the DAT_00487ac0 address mapped */

    FUN_00422a10();
    FUN_0042d8b0();

    iVar2 = FUN_00422740();
    if (iVar2 != 1) {
        return 0;
    }

    FUN_00420be0();
    FUN_0041e580();

    iVar2 = FUN_00414060();
    if (iVar2 == 0) {
        return 2;
    }

    FUN_00413f70();

    /* Resolution table */
    g_ModeWidths[0] = 320;   g_ModeHeights[0] = 200;
    g_ModeWidths[1] = 320;   g_ModeHeights[1] = 240;
    g_ModeWidths[2] = 400;   g_ModeHeights[2] = 300;
    g_ModeWidths[3] = 512;   g_ModeHeights[3] = 384;
    g_ModeWidths[4] = 400;   g_ModeHeights[4] = 480;
    g_ModeWidths[5] = 640;   g_ModeHeights[5] = 480;
    g_ModeWidths[6] = 800;   g_ModeHeights[6] = 600;
    g_ModeWidths[7] = 1024;  g_ModeHeights[7] = 768;
    g_ModeWidths[8] = 1152;  g_ModeHeights[8] = 864;
    g_ModeWidths[9] = 1280;  g_ModeHeights[9] = 1024;

    g_SubState = 0;
    g_NeedsRedraw = 0;
    g_NumDisplayModes = 10;

    /* Display mode init */
    DAT_00487640[1] = 0xFF;   /* current mode = unset */
    DAT_00487640[2] = 5;      /* desired mode = index 5 (640x480) */

    FUN_0041e4a0();

    DAT_0048764b = 0;
    DAT_0048764a = 0;

    return 1;
}

/* ===== Init_DirectInput (004620F0) ===== */
int Init_DirectInput(void)
{
    HRESULT hr;
    DIPROPDWORD dipdw;

    /* 1. Create DirectInput interface */
    hr = DirectInputCreateA(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &lpDI, NULL);
    if (FAILED(hr)) {
        return 0;
    }

    /* 2. Keyboard setup */
    hr = lpDI->CreateDevice(GUID_SysKeyboard, &lpDI_Keyboard, NULL);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->SetDataFormat(&c_dfDIKeyboard);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->SetCooperativeLevel(hWnd_Main, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->Acquire();
    if (FAILED(hr)) goto cleanup;

    /* 3. Mouse setup */
    hr = lpDI->CreateDevice(GUID_SysMouse, &lpDI_Mouse, NULL);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Mouse->SetDataFormat(&c_dfDIMouse);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Mouse->SetCooperativeLevel(hWnd_Main, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) goto cleanup;

    /* 4. Mouse event notification */
    hMouseEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (hMouseEvent == NULL) {
        goto cleanup;
    }

    hr = lpDI_Mouse->SetEventNotification(hMouseEvent);
    if (FAILED(hr)) goto cleanup;

    /* 5. Set mouse buffer size to 16 entries */
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = 16;

    hr = lpDI_Mouse->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);
    if (FAILED(hr)) goto cleanup;

    return 1;

cleanup:
    /* Release everything on failure */
    if (lpDI_Mouse != NULL) {
        lpDI_Mouse->Unacquire();
        lpDI_Mouse->Release();
        lpDI_Mouse = NULL;
    }
    if (lpDI_Keyboard != NULL) {
        lpDI_Keyboard->Unacquire();
        lpDI_Keyboard->Release();
        lpDI_Keyboard = NULL;
    }
    if (lpDI != NULL) {
        lpDI->Release();
        lpDI = NULL;
    }
    if (hMouseEvent != NULL) {
        CloseHandle(hMouseEvent);
        hMouseEvent = NULL;
    }
    return 0;
}
