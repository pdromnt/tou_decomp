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
int           DAT_0048227c = 0;       /* player config packed */

static const char LEV_MAGIC[] = "TOU level file v1.4";
#define LEV_MAGIC_LEN 19

/* Tile remap table: maps RLE index (b0>>2) to actual tile value.
 * Entry 0x21 (0xFF) = terminator. From Ghidra Load_Image_Wrapper. */
static const unsigned char tile_remap[34] = {
    0x00, 0x01, 0x0A, 0x05, 0x02, 0x07, 0x1A, 0x04,
    0x06, 0x14, 0x0C, 0x40, 0x44, 0x41, 0x45, 0x42,
    0x46, 0x43, 0x47, 0x1B, 0x16, 0x17, 0x18, 0x19,
    0x0F, 0x10, 0x12, 0x13, 0x1C, 0x1D, 0x1E, 0x1F,
    0x81, 0xFF
};


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

    /* Copy tile type table (924 bytes at offset 0x22) */
    memcpy(DAT_00483860, file_buf + 0x22, 0x39c);

    /* Load JPEG background, entities, and tilemap */
    result = Load_Image_Data(jpeg_offset, extra_offset, entity_offset, file_buf);

    Mem_Free(file_buf);

    if (!result) {
        if (DAT_00489d7c[0] == '\0') {
            sprintf(DAT_00489d7c, "Could not load level \"%s\"", level_name);
        }
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

    return 1;
}
