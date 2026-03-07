/*
 * level.cpp - Level loading, .lev file parsing, tilemap/entity setup
 * Addresses: Load_Level_File=00421060, Load_Image_Wrapper=00420720,
 *            Load_Level_Resources=0041A020
 *
 * .lev v1.4 file format:
 *   0x00  19 bytes  Magic: "TOU level file v1.4"
 *   0x13   3 bytes  Flags/padding
 *   0x16   4 bytes  int: jpeg_offset (embedded JPEG start)
 *   0x1a   4 bytes  int: extra_offset
 *   0x1e   4 bytes  int: entity_section_offset
 *   0x22  0x39c     Tile type table (924 bytes)
 *   jpeg_offset..entity_offset: Embedded JPEG background image
 *   entity_offset+0:   int entity_count
 *   entity_offset+4:   entity_count * 20 bytes entity records
 *   after entities:     RLE tilemap (terminated by 0xFF remap)
 */
#include "tou.h"
#include <stdio.h>
#include <string.h>

/* stb_image_free declaration for freeing stbi buffers */
extern "C" void stbi_image_free(void *retval_from_stbi_load);

/* ===== Globals defined in this module ===== */
unsigned int  DAT_004879f0 = 0;       /* map width (pixels + 14 border) */
unsigned int  DAT_004879f4 = 0;       /* map height (pixels + 14 border) */
int           DAT_00487a00 = 0;       /* row stride (power-of-2) */
int           DAT_00487a18 = 0;       /* shift amount (low byte only) */
int           DAT_004879f8 = 0;       /* coarse grid cols */
int           DAT_004879fc = 0;       /* coarse grid rows */
int           DAT_00487a04 = 0;       /* shadow grid cols */
int           DAT_00487a08 = 0;       /* shadow grid rows */
void         *DAT_0048782c = NULL;    /* tilemap pointer */
void         *DAT_00481f50 = NULL;    /* background RGB565 pixels */
int           DAT_00489278 = 0;       /* entity placement count */
unsigned char DAT_00483860[0x39c];    /* tile type table */
void         *DAT_00487814 = NULL;    /* coarse grid buffer */
void         *DAT_00489ea4 = NULL;    /* shadow grid 1 */
void         *DAT_00489ea8 = NULL;    /* shadow grid 2 */
void         *DAT_00489ea0 = NULL;    /* swap/heightmap */
void         *DAT_00487820 = NULL;    /* edge/boundary navigation data */
int           DAT_00487a0c = 0;       /* swap width */
int           DAT_00487a10 = 0;       /* swap height */
unsigned short DAT_0048384c = 0;      /* tile fill color */
char          DAT_0048396d = 0;       /* generated-map flag */
char          DAT_00483960 = 0;       /* swap-file enabled flag */
char         *DAT_00486938 = NULL;    /* current level name ptr */
int           DAT_0048693c = 0;       /* current level index */
char          DAT_004892e4 = 0;       /* random mirror flag */
char          DAT_004892e5 = 0;       /* difficulty flag */
char          DAT_00489d7c[256];      /* error string buffer */
void         *DAT_00487aa4 = NULL;    /* large game state buffer */
int           DAT_00489254 = 0;       /* edge count */
int           DAT_00487810 = 0;       /* player data base address */
int           DAT_00489240 = 0;       /* player count */
int           DAT_00489244 = 0;       /* active player count */
/* DAT_0048764a defined in init.cpp */
/* DAT_0048227c: In the original binary, this IS part of the config blob at
 * address 0x00481F58 + 0x324 = 0x0048227C. Our decomp defines it as a macro
 * alias into g_ConfigBlob so menu writes (via CFG_ADDR) are immediately
 * visible to game logic that reads DAT_0048227c[0] for player count. */

static const char LEV_MAGIC[] = "TOU level file v1.4";
#define LEV_MAGIC_LEN 19

/* ===== Load_SWP_Sky (based on FUN_004213f0 partial) ===== */
/* Loads a pre-computed sky image from swap\<name>.SWP.
 * Format: 4-byte width, 4-byte height, then width*height*2 RGB565 pixels.
 * The original generates these on first load and caches them. */
int Load_SWP_Sky(const char *level_name)
{
    char path[256];
    FILE *f;
    int w, h;

    sprintf(path, "swap\\%s.SWP", level_name);
    f = fopen(path, "rb");
    if (!f) {
        sprintf(path, "swap/%s.SWP", level_name);
        f = fopen(path, "rb");
    }
    if (!f) {
        LOG("[LEVEL] No SWP sky file: %s\n", path);
        return 0;
    }

    fread(&w, 4, 1, f);
    fread(&h, 4, 1, f);

    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        LOG("[LEVEL] SWP sky invalid dimensions: %dx%d\n", w, h);
        fclose(f);
        return 0;
    }

    DAT_00487a0c = w;
    DAT_00487a10 = h;

    int size = w * h * 2;
    DAT_00489ea0 = Mem_Alloc(size);
    if (!DAT_00489ea0) {
        LOG("[LEVEL] SWP sky alloc failed: %d bytes\n", size);
        fclose(f);
        return 0;
    }

    fread(DAT_00489ea0, 1, size, f);
    fclose(f);

    /* Convert RGB555 (X1R5G5B5) → RGB565 (R5G6B5).
     * SWP files store pixels in the original DirectDraw RGB555 format.
     * Our compat renderer uses RGB565, same conversion as the sprite loader. */
    {
        unsigned short *px = (unsigned short *)DAT_00489ea0;
        int count = w * h;
        for (int i = 0; i < count; i++) {
            unsigned short c = px[i];
            /* R(14-10) → bits 15-11, G(9-5) → bits 10-5 (doubled MSB), B(4-0) stays */
            px[i] = ((c & 0x7C00) << 1) | ((c & 0x03E0) << 1) | (c & 0x001F);
        }
    }

    g_MemoryTracker += size;
    LOG("[LEVEL] Loaded SWP sky: %dx%d (%d bytes), converted RGB555→RGB565\n", w, h, size);
    return 1;
}

/* Tile remap table: maps RLE index (b0>>2) to actual tile value.
 * Entry 0x21 (0xFF) = terminator. From Ghidra Load_Image_Wrapper. */
static const unsigned char tile_remap[34] = {
    0x00, 0x01, 0x0A, 0x05, 0x02, 0x07, 0x1A, 0x04,
    0x06, 0x14, 0x0C, 0x40, 0x44, 0x41, 0x45, 0x42,
    0x46, 0x43, 0x47, 0x1B, 0x16, 0x17, 0x18, 0x19,
    0x0F, 0x10, 0x12, 0x13, 0x1C, 0x1D, 0x1E, 0x1F,
    0x81, 0xFF
};


/* ===== Assign_Water_Tile_Colors — partial FUN_00417460 (00417460) ===== */
/* The original FUN_00417460 assigns colors to ALL tiles (texture sampling for
 * non-water, flat DAT_0048384c for water). We implement only the water part:
 * iterate all tiles, set DAT_00481f50 to DAT_0048384c where property[4] != 0.
 * Must be called AFTER tile map is fully set up (water fill, turrets, waves). */
void Assign_Water_Tile_Colors(void)
{
    if (!DAT_0048782c || !DAT_00481f50 || !DAT_00487928) return;

    int width = (int)DAT_004879f0;
    int height = (int)DAT_004879f4;
    int row_stride = (int)DAT_00487a00;

    for (int y = 0; y < height; y++) {
        int row_off = y * row_stride;
        for (int x = 0; x < width; x++) {
            int tile_off = row_off + x;
            unsigned char tile_type = *(unsigned char *)((int)DAT_0048782c + tile_off);
            char water_prop = *(char *)((unsigned int)tile_type * 0x20 + 4 + (int)DAT_00487928);
            if (water_prop != 0) {
                *(unsigned short *)((int)DAT_00481f50 + tile_off * 2) = DAT_0048384c;
            }
        }
    }

    LOG("[LEVEL] Water tile colors assigned: fill=0x%04X across %dx%d map\n",
        (unsigned int)DAT_0048384c, width, height);
}

/* ===== FUN_0045af70 — Build_Water_Color_LUTs (0045AF70) ===== */
/* Builds 8 color lookup tables for water tinting:
 *   DAT_004876a4[28..31]: strong blend (step 30 per iteration, 1-4 passes)
 *   DAT_004876a4[48..51]: subtle blend (step 10 per iteration, 1-4 passes)
 * Each table: 4096 entries of unsigned short, indexed by 4-bit R:G:B (from
 * the master remap table DAT_00489230). For each source pixel, converges
 * toward the water base color (DAT_00483840/44/48). */
void FUN_0045af70(void)
{
    int tbl, r4, g4, b4;

    /* First set: strong blend (step 0x1e = 30) at DAT_004876a4[28..31] */
    for (tbl = 0; tbl < 4; tbl++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[28 + tbl];
        if (!lut) continue;
        int r_chunk = 0;
        for (int r_idx = 0; r_idx < 16; r_idx++) {
            int r_init = (int)((double)r_idx * 17.0);
            for (g4 = 0; g4 < 16; g4++) {
                int g_init = (int)((double)g4 * 17.0);
                for (b4 = 0; b4 < 16; b4++) {
                    int b_init = (int)((double)b4 * 17.0);

                    int r = r_init, g = g_init, b = b_init;
                    /* Converge toward water color with (tbl+1) passes */
                    for (int n = tbl + 1; n > 0; n--) {
                        if (r > (int)DAT_00483840) {
                            r -= 0x1e;
                            if (r < (int)DAT_00483840) r = (int)DAT_00483840;
                        } else if (r < (int)DAT_00483840) {
                            r += 0x1e;
                            if (r > (int)DAT_00483840) r = (int)DAT_00483840;
                        }
                        if (g > (int)DAT_00483844) {
                            g -= 0x1e;
                            if (g < (int)DAT_00483844) g = (int)DAT_00483844;
                        } else if (g < (int)DAT_00483844) {
                            g += 0x1e;
                            if (g > (int)DAT_00483844) g = (int)DAT_00483844;
                        }
                        if (b > (int)DAT_00483848) {
                            b -= 0x1e;
                            if (b < (int)DAT_00483848) b = (int)DAT_00483848;
                        } else if (b < (int)DAT_00483848) {
                            b += 0x1e;
                            if (b > (int)DAT_00483848) b = (int)DAT_00483848;
                        }
                    }
                    if (r > 255) r = 255; else if (r < 0) r = 0;
                    if (g > 255) g = 255; else if (g < 0) g = 0;
                    if (b > 255) b = 255; else if (b < 0) b = 0;

                    int idx = (r_chunk + g4) * 16 + b4;
                    lut[idx] = (unsigned short)(
                        ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    );
                }
            }
            r_chunk += 16;
        }
    }

    /* Second set: subtle blend (step 10) at DAT_004876a4[48..51] */
    for (tbl = 0; tbl < 4; tbl++) {
        unsigned short *lut = (unsigned short *)DAT_004876a4[48 + tbl];
        if (!lut) continue;
        int r_chunk = 0;
        for (int r_idx = 0; r_idx < 16; r_idx++) {
            int r_init = (int)((double)r_idx * 17.0);
            for (g4 = 0; g4 < 16; g4++) {
                int g_init = (int)((double)g4 * 17.0);
                for (b4 = 0; b4 < 16; b4++) {
                    int b_init = (int)((double)b4 * 17.0);

                    int r = r_init, g = g_init, b = b_init;
                    for (int n = tbl + 1; n > 0; n--) {
                        if (r > (int)DAT_00483840) {
                            r -= 10;
                            if (r < (int)DAT_00483840) r = (int)DAT_00483840;
                        } else if (r < (int)DAT_00483840) {
                            r += 10;
                            if (r > (int)DAT_00483840) r = (int)DAT_00483840;
                        }
                        if (g > (int)DAT_00483844) {
                            g -= 10;
                            if (g < (int)DAT_00483844) g = (int)DAT_00483844;
                        } else if (g < (int)DAT_00483844) {
                            g += 10;
                            if (g > (int)DAT_00483844) g = (int)DAT_00483844;
                        }
                        if (b > (int)DAT_00483848) {
                            b -= 10;
                            if (b < (int)DAT_00483848) b = (int)DAT_00483848;
                        } else if (b < (int)DAT_00483848) {
                            b += 10;
                            if (b > (int)DAT_00483848) b = (int)DAT_00483848;
                        }
                    }
                    if (r > 255) r = 255; else if (r < 0) r = 0;
                    if (g > 255) g = 255; else if (g < 0) g = 0;
                    if (b > 255) b = 255; else if (b < 0) b = 0;

                    int idx = (r_chunk + g4) * 16 + b4;
                    lut[idx] = (unsigned short)(
                        ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    );
                }
            }
            r_chunk += 16;
        }
    }
}

/* ===== FUN_00421310 — Compute_Water_Colors (00421310) ===== */
/* Reads R,G,B from level config blob (DAT_00483860 offsets 0x103-0x105),
 * computes base/lighter/darker water colors, then rebuilds the water
 * color LUT tables via FUN_0045af70. Called from Load_Level_File. */
void FUN_00421310(void)
{
    /* Extract R,G,B from level config blob */
    DAT_00483840 = (unsigned int)DAT_00483860[0x103];
    DAT_00483844 = (unsigned int)DAT_00483860[0x104];
    DAT_00483848 = (unsigned int)DAT_00483860[0x105];

    /* Base water fill color (RGB565) */
    DAT_0048384c = (unsigned short)(
        ((DAT_00483840 & 0xF8) << 8) |
        ((DAT_00483844 & 0xFC) << 3) |
        (DAT_00483848 >> 3)
    );

    /* Lighter bubble color (each channel +20, clamped to 255) */
    {
        unsigned int r = DAT_00483840 + 0x14;
        unsigned int g = DAT_00483844 + 0x14;
        unsigned int b = DAT_00483848 + 0x14;
        if (r > 0xFF) r = 0xFF;
        if (g > 0xFF) g = 0xFF;
        if (b > 0xFF) b = 0xFF;
        DAT_0048384e = (unsigned short)(
            ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        );
    }

    /* Darker bubble color (each channel -20, clamped to 0) */
    {
        int r = (int)DAT_00483840 - 0x14;
        int g = (int)DAT_00483844 - 0x14;
        int b = (int)DAT_00483848 - 0x14;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        DAT_00483850 = (unsigned short)(
            ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        );
    }

    LOG("[LEVEL] Water colors: R=%d G=%d B=%d, fill=0x%04X, light=0x%04X, dark=0x%04X\n",
        DAT_00483840, DAT_00483844, DAT_00483848,
        (unsigned int)DAT_0048384c, (unsigned int)DAT_0048384e, (unsigned int)DAT_00483850);

    FUN_0045af70();
}

/* ===== Load_Level_File (00421060) ===== */
/* Opens a .lev file, verifies the header, extracts tile type table,
 * then delegates to Load_Image_Data for JPEG/entities/tilemap.
 * Returns 1 on success, 0 on failure. */
int Load_Level_File(const char *level_name)
{
    char path[256];
    FILE *f;
    long file_size;
    unsigned char *file_buf;
    int jpeg_offset, extra_offset, entity_offset;
    int result;

    /* Build path: levels\<name>.lev */
    sprintf(path, "levels\\%s.lev", level_name);
    LOG("[LEVEL] Loading level file: %s\n", path);

    f = fopen(path, "rb");
    if (!f) {
        sprintf(DAT_00489d7c, "Level \"%s\" not found!", level_name);
        LOG("[LEVEL] ERROR: %s\n", DAT_00489d7c);
        return 0;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0x22 + 0x39c) {
        sprintf(DAT_00489d7c, "Level \"%s\" is too small!", level_name);
        fclose(f);
        return 0;
    }

    /* Read entire file into memory */
    file_buf = (unsigned char *)Mem_Alloc(file_size);
    if (!file_buf) {
        sprintf(DAT_00489d7c, "Not enough memory for level \"%s\"!", level_name);
        fclose(f);
        return 0;
    }

    fread(file_buf, 1, file_size, f);
    fclose(f);

    /* Verify magic header (19 bytes) */
    if (memcmp(file_buf, LEV_MAGIC, LEV_MAGIC_LEN) != 0) {
        sprintf(DAT_00489d7c, "Level \"%s\" is wrong version or not a TOU level.", level_name);
        Mem_Free(file_buf);
        return 0;
    }

    LOG("[LEVEL] Header verified: %s (%ld bytes)\n", LEV_MAGIC, file_size);

    /* Extract section offsets from header */
    jpeg_offset   = *(int *)(file_buf + 0x16);
    extra_offset  = *(int *)(file_buf + 0x1a);
    entity_offset = *(int *)(file_buf + 0x1e);

    LOG("[LEVEL] Offsets: jpeg=0x%x, extra=0x%x, entity=0x%x\n",
        jpeg_offset, extra_offset, entity_offset);

    /* Validate offsets are within file bounds */
    if (jpeg_offset < 0x22 || entity_offset < jpeg_offset ||
        entity_offset >= file_size) {
        sprintf(DAT_00489d7c, "Level \"%s\" has invalid section offsets.", level_name);
        Mem_Free(file_buf);
        return 0;
    }

    /* Copy level config blob (924 bytes at offset 0x22).
     * In the original binary, several globals overlap this blob:
     *   DAT_00483960 = DAT_00483860[0x100] — sky/swap enabled flag
     *   DAT_0048396d = DAT_00483860[0x10d] — generated-map flag
     * Since our decomp has them as separate variables, extract after copy. */
    memcpy(DAT_00483860, file_buf + 0x22, 0x39c);
    DAT_00483960 = (char)DAT_00483860[0x100];
    DAT_0048396d = (char)DAT_00483860[0x10d];
    LOG("[LEVEL] Config blob: sky_enabled=%d, gen_map=%d\n",
        (int)(unsigned char)DAT_00483960, (int)(unsigned char)DAT_0048396d);

    /* Load JPEG background, entities, and tilemap */
    result = Load_Image_Data(jpeg_offset, extra_offset, entity_offset, file_buf);

    Mem_Free(file_buf);

    if (!result) {
        if (DAT_00489d7c[0] == '\0') {
            sprintf(DAT_00489d7c, "Could not load level \"%s\"", level_name);
        }
    }

    /* Compute per-level water colors + build LUTs.
     * FUN_00421310 reads R,G,B from level data (DAT_00483860[0x103-0x105]),
     * computes base/lighter/darker fill colors (DAT_0048384c/4e/50),
     * and rebuilds the 8 water color LUT tables via FUN_0045af70.
     * Without this, the wave renderer paints surface tiles with stale
     * colors from the config blob, creating a visible two-layer effect. */
    if (result) {
        LOG("[LEVEL] Pre-water: R=%d G=%d B=%d, fill=0x%04X (from config blob)\n",
            DAT_00483840, DAT_00483844, DAT_00483848, (unsigned int)DAT_0048384c);
        LOG("[LEVEL] Level data water bytes: [0x103]=0x%02X [0x104]=0x%02X [0x105]=0x%02X\n",
            (unsigned int)DAT_00483860[0x103],
            (unsigned int)DAT_00483860[0x104],
            (unsigned int)DAT_00483860[0x105]);
        FUN_00421310();
    }

    return result;
}


/* ===== Load_Image_Data (based on Load_Image_Wrapper @ 00420720) ===== */
/* Processes the embedded JPEG, entity placements, and RLE tilemap.
 * All data comes from the file buffer at the specified offsets.
 * Returns 1 on success, 0 on failure. */
int Load_Image_Data(int jpeg_offset, int extra_offset, int entity_offset,
                    unsigned char *file_buf)
{
    unsigned char *jpeg_data;
    int jpeg_len;
    int img_w, img_h;
    unsigned char *rgb24;
    unsigned int map_w, map_h;
    unsigned int shift, stride;
    unsigned char *combined_buf;
    unsigned short *bg_pixels;
    unsigned char *tilemap;
    int i, row, col;

    /* ---- 1. Decode embedded JPEG background ---- */
    jpeg_data = file_buf + jpeg_offset;
    jpeg_len  = entity_offset - jpeg_offset;

    LOG("[LEVEL] Decoding JPEG: offset=0x%x, size=%d bytes\n", jpeg_offset, jpeg_len);

    rgb24 = (unsigned char *)Load_JPEG_From_Memory(jpeg_data, jpeg_len, &img_w, &img_h);
    if (!rgb24) {
        sprintf(DAT_00489d7c, "Failed to decode level background JPEG");
        return 0;
    }

    LOG("[LEVEL] JPEG decoded: %dx%d pixels\n", img_w, img_h);

    /* ---- 2. Calculate map dimensions ---- */
    map_w = (unsigned int)img_w + 14;  /* +14 border (7 each side) */
    map_h = (unsigned int)img_h + 14;

    /* Find power-of-2 stride >= map_w */
    shift = 0;
    while ((1u << shift) < map_w) {
        shift++;
    }
    stride = 1u << shift;

    DAT_004879f0 = map_w;
    DAT_004879f4 = map_h;
    DAT_00487a18 = (int)shift;
    DAT_00487a00 = (int)stride;

    /* Grid sizes */
    DAT_004879f8 = ((int)map_w >> 4) + 2;   /* coarse grid cols */
    DAT_004879fc = ((int)map_h >> 4) + 2;   /* coarse grid rows */
    DAT_00487a04 = (int)map_w / 18 + 2;     /* shadow grid cols */
    DAT_00487a08 = (int)map_h / 18 + 2;     /* shadow grid rows */

    LOG("[LEVEL] Map: %ux%u, stride=%u (shift=%u)\n", map_w, map_h, stride, shift);
    LOG("[LEVEL] Grids: coarse=%dx%d, shadow=%dx%d\n",
        DAT_004879f8, DAT_004879fc, DAT_00487a04, DAT_00487a08);

    /* ---- 3. Allocate combined buffer (background + tilemap) ---- */
    /* Layout: [stride*height*2 bytes RGB565] [stride*height*1 byte tilemap] */
    combined_buf = (unsigned char *)Mem_Alloc(stride * map_h * 3);
    if (!combined_buf) {
        sprintf(DAT_00489d7c, "Not enough memory for level background");
        stbi_image_free(rgb24);
        return 0;
    }

    DAT_00481f50 = combined_buf;
    bg_pixels = (unsigned short *)combined_buf;

    /* Point tilemap to after the RGB565 data */
    DAT_0048782c = combined_buf + (stride * map_h * 2);
    tilemap = (unsigned char *)DAT_0048782c;

    /* Clear tilemap to 0 */
    memset(tilemap, 0, stride * map_h);

    /* ---- 4. Convert RGB24 → RGB565 into stride-aligned positions ---- */
    /* Place JPEG pixels at interior positions (7,7) to (map_w-8, map_h-8) */
    for (row = 0; row < img_h; row++) {
        for (col = 0; col < img_w; col++) {
            int src_idx = (row * img_w + col) * 3;
            unsigned char r = rgb24[src_idx + 0];
            unsigned char g = rgb24[src_idx + 1];
            unsigned char b = rgb24[src_idx + 2];
            unsigned short rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

            /* Place at border-offset position in stride-aligned buffer */
            int dst_row = row + 7;
            int dst_col = col + 7;
            bg_pixels[(dst_row << shift) + dst_col] = rgb565;
        }
    }

    stbi_image_free(rgb24);
    LOG("[LEVEL] Background converted to RGB565\n");

    /* ---- 5. Parse entity placements ---- */
    {
        unsigned char *ent_section = file_buf + entity_offset;
        int ent_count = *(int *)ent_section;
        unsigned char *ent_data = ent_section + 4;
        unsigned char *dst_ent = (unsigned char *)DAT_00487828;

        /* Sanity check */
        if (ent_count < 0 || ent_count > 1000) {
            LOG("[LEVEL] WARNING: entity count %d seems wrong, clamping\n", ent_count);
            if (ent_count < 0) ent_count = 0;
            if (ent_count > 1000) ent_count = 1000;
        }

        DAT_00489278 = ent_count;
        LOG("[LEVEL] Entity placements: %d\n", ent_count);

        for (i = 0; i < ent_count; i++) {
            /* Copy 20 bytes per entity */
            memcpy(dst_ent + i * 20, ent_data + i * 20, 20);

            /* Add +7 border offset to x and y positions */
            *(int *)(dst_ent + i * 20 + 0) += 7;
            *(int *)(dst_ent + i * 20 + 4) += 7;
        }

        /* ---- 6. Decode RLE tilemap ---- */
        {
            unsigned char *rle_data = ent_data + ent_count * 20;
            int tile_row = 7;
            int tile_col = 7;
            int tiles_filled = 0;

            LOG("[LEVEL] Decoding RLE tilemap (starts at file offset 0x%x)\n",
                (int)(rle_data - file_buf));

            while (1) {
                unsigned char b0 = *rle_data++;
                unsigned char b1 = *rle_data++;
                int remap_idx = b0 >> 2;

                /* Bounds check: any index >= 33 is terminator (original
                 * table entry [33] = 0xFF). The RLE stream ends with
                 * b0=0xFC (remap_idx=63) or similar out-of-range value. */
                if (remap_idx >= 33) {
                    break;
                }

                unsigned char tile_val = tile_remap[remap_idx];

                int run_len = (b0 & 3) + b1 * 4;

                for (int r = 0; r < run_len; r++) {
                    /* Write tile to tilemap at stride-aligned position */
                    tilemap[(tile_row << shift) + tile_col] = tile_val;
                    tiles_filled++;

                    tile_col++;
                    if (tile_col >= (int)(map_w - 7)) {
                        tile_col = 7;
                        tile_row++;
                    }
                }
            }

            LOG("[LEVEL] Tilemap decoded: %d tiles filled\n", tiles_filled);
        }
    }

    /* ---- 7. Set border tiles to solid (tile 5) ---- */
    /* Top and bottom 7 rows */
    for (row = 0; row < 7; row++) {
        for (col = 0; col < (int)map_w; col++) {
            tilemap[(row << shift) + col] = 5;
            tilemap[((map_h - 1 - row) << shift) + col] = 5;
        }
    }
    /* Left and right 7 columns */
    for (row = 0; row < (int)map_h; row++) {
        for (col = 0; col < 7; col++) {
            tilemap[(row << shift) + col] = 5;
            tilemap[(row << shift) + (map_w - 1 - col)] = 5;
        }
    }

    /* ---- 8. Post-process: zero background for solid tiles ---- */
    /* In original: if entity_table[tile*0x20] == 1, pixel = 0 (black) */
    if (DAT_00487928) {
        for (row = 0; row < (int)map_h; row++) {
            for (col = 0; col < (int)map_w; col++) {
                int tile_idx = tilemap[(row << shift) + col];
                unsigned char *ent_entry = (unsigned char *)DAT_00487928 + tile_idx * 0x20;
                if (*ent_entry == 0x01) {
                    /* Solid tile: black background */
                    bg_pixels[(row << shift) + col] = 0;
                }
            }
        }
    }

    /* Handle tile 0x1B → replace with 0 (from original code) */
    for (row = 0; row < (int)map_h; row++) {
        for (col = 0; col < (int)map_w; col++) {
            if (tilemap[(row << shift) + col] == 0x1B) {
                tilemap[(row << shift) + col] = 0;
            }
        }
    }

    /* ---- 9. Allocate grid buffers ---- */
    DAT_00487814 = Mem_Alloc((DAT_004879fc + 1) * (DAT_004879f8 + 1));
    if (DAT_00487814) {
        g_MemoryTracker += (DAT_004879fc + 1) * (DAT_004879f8 + 1);
    }

    DAT_00489ea4 = Mem_Alloc((DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    if (DAT_00489ea4) {
        g_MemoryTracker += (DAT_00487a08 + 1) * (DAT_00487a04 + 1);
        memset(DAT_00489ea4, 0, (DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    }

    DAT_00489ea8 = Mem_Alloc((DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    if (DAT_00489ea8) {
        g_MemoryTracker += (DAT_00487a08 + 1) * (DAT_00487a04 + 1);
        memset(DAT_00489ea8, 0, (DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    }

    LOG("[LEVEL] Grid buffers allocated (coarse=%p, shadow1=%p, shadow2=%p)\n",
        DAT_00487814, DAT_00489ea4, DAT_00489ea8);

    /* ---- 10. Initialize game config defaults (from FUN_00416ad0) ---- */
    /* These are gameplay tuning values set each time a level's grid is built.
     * Addresses 0x483963-0x48396c = g_ConfigBlob offsets 0x1A0B-0x1A14 */
    g_ConfigBlob[0x1A0B] = 0x28;  /* 40 — spawn timer? */
    g_ConfigBlob[0x1A0C] = 0x3C;  /* 60 — respawn delay? */
    g_ConfigBlob[0x1A0D] = (char)0x8C;  /* 140 — ? */
    g_ConfigBlob[0x1A0E] = 1;     /* flag */
    g_ConfigBlob[0x1A0F] = 0x0A;  /* 10 */
    g_ConfigBlob[0x1A10] = 0x0A;  /* 10 — drag factor */
    g_ConfigBlob[0x1A11] = 0x0A;  /* 10 — wall collision damage multiplier */
    g_ConfigBlob[0x1A12] = 0x0A;  /* 10 — wall collision bounce factor */
    g_ConfigBlob[0x1A13] = 0;
    g_ConfigBlob[0x1A14] = 0;

    return 1;
}
