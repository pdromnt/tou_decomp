/*
 * gg_gen.cpp - GG (Ground Generator) procedural level generation
 * Addresses: FUN_004143e0, FUN_00415a60, FUN_00416ad0, FUN_00416320,
 *            FUN_00418520, FUN_00417ea0, FUN_00414b00, FUN_00414c90,
 *            FUN_00417b40, FUN_00417d90, FUN_00417700, FUN_00417850,
 *            FUN_004152e0, FUN_004153b0, FUN_004155e0, FUN_00418640,
 *            FUN_00417460, Calc_Power_Of_Two, FUN_00425820
 *
 * GG levels are procedurally generated from theme assets in ggstuff/ directories.
 * Each theme contains TGA/JPEG tile textures, an info.txt config, and decoration sprites.
 */
#include "tou.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

extern "C" void stbi_image_free(void *retval_from_stbi_load);

/* ===== GG Globals ===== */

/* Theme selection */
char          DAT_0048396e[256];    /* theme name buffer for matching */
char          DAT_004808e1;         /* simple-mode flag (param_1 == 0) */

/* Map boundaries */
int           DAT_0048071c;         /* left boundary (7) */
int           DAT_00480724;         /* top boundary (7) */
int           DAT_00480720;         /* right boundary (width - 7) */
int           DAT_00480728;         /* bottom boundary (height - 7) */

/* Tile graphics state */
int           DAT_00480898;         /* tile count */
int           DAT_004808d0;         /* error code */
int           DAT_004808d4;         /* unused counter */
int           DAT_004808d8;         /* unused counter */
int           DAT_004808dc;         /* unused counter */
char          DAT_00480884;         /* sprite start index (above water) */
int           DAT_00480888;         /* sprite count (above water) */
char          DAT_0048088c;         /* sprite start index (below water) */
int           DAT_00480890;         /* sprite count (below water) */
int           DAT_00480894;         /* main tile sprite index */
int           DAT_004808a4;         /* alt tile sprite index */
int           DAT_004808a8;         /* extra tile count */
int           DAT_004808a0;         /* deco tile count */
int           DAT_0048089c;         /* landscape tile index */
int           DAT_004808b4;         /* creature sprite start */
int           DAT_004808b8;         /* creature count */
int           DAT_004808bc;         /* decoration start */
int           DAT_004808c0;         /* decoration count */
int           DAT_004808c4;         /* pickup start */
int           DAT_004808c8;         /* pickup count */
int           DAT_004808ac;         /* treasure start */
int           DAT_004808b0;         /* treasure count */
char          DAT_004808e0;         /* TGA vertical flip flag */
int           DAT_00480840;         /* pixel write cursor */
unsigned int  DAT_0048085c;         /* texture darkness copy */

/* Work buffers (allocated in FUN_00416320, freed in FUN_004143e0) */
void         *DAT_0048072c;         /* 8MB work buffer (RGB565 tile pixels) */
void         *DAT_00481b54;         /* tile dimensions buffer (0xc00) */
void         *DAT_00480738;         /* buffer (0x800) */
void         *DAT_00480734;         /* buffer (0x3000) */
void         *DAT_00480730;         /* buffer (0x3000) */
void         *DAT_004818e4;         /* buffer (0x1000) */

/* Terrain generation state */
int           DAT_00480848;         /* half-total area */
int           DAT_00480844;         /* cleared area counter */
int           DAT_00480860;         /* darkness adjusted value */
int           DAT_0048084c;         /* tunnel style (default 2) */
int           DAT_00480850;         /* generation flag */
int           DAT_00480854;         /* lighting enable flag */
int           DAT_00480858;         /* border tunnel flag */
int           DAT_00480864;         /* texture X offset */
int           DAT_00480868;         /* texture Y offset */

/* Water vertex generation */
int           DAT_00480870;         /* noise grid width */
int           DAT_00480874;         /* noise grid height */
void         *DAT_0048086c;         /* noise grid buffer */

/* Info.txt parsed data */
char          DAT_004818e8[256];    /* maker name */
char          DAT_00481988[256];    /* email */
char          DAT_00481a28;         /* TEXATTR tile code */
char          DAT_00481a29;         /* GRASSATTR tile code */
char          DAT_00481a2a;         /* PICATTR tile code */
char          DAT_00481a2b;         /* SIGNATTR tile code */
char          DAT_00481a2c;         /* TEXATTR color */
char          DAT_00481a2d;         /* GRASSATTR color */
char          DAT_00481a2e;         /* PICATTR color */
char          DAT_00481a2f;         /* SIGNATTR color */
char          DAT_00481a30;         /* sign color */
char          DAT_00481a31;         /* sign text X */
char          DAT_00481a32;         /* sign text Y */
int           DAT_00481a34;         /* fixed width (0 = use random) */
int           DAT_00481a38;         /* fixed height (0 = use random) */
int           DAT_00481a3c;         /* water config: [0]=level, [1]=R, [2]=G, [3]=B */
char          DAT_00481a40;         /* beach style flag */
char          DAT_00481a41;         /* texture darkness (0-128) */
char          DAT_00481a42;         /* parallax darkness (0-128) */
char          DAT_00481a43;         /* sign name count */
char          DAT_00481a44[256];    /* sign name table (16 entries x 16 bytes) */
char          DAT_00481b44[16];     /* sign name type table (0=above, 1=below, 2=alongside) */

/* Info parsed flags */
char          DAT_00480708;         /* maker parsed */
char          DAT_00480709;         /* (part of DAT_00480708 int) */
char          DAT_0048070c;         /* picattr parsed */
char          DAT_0048070d;         /* (part of DAT_0048070c int) */
int           DAT_00480710;         /* fixedsize/water parsed flags */
int           DAT_00480714;         /* texdark/pardark parsed flags */
int           DAT_00480718;         /* sign parsed flag */

/* Entity spawning config */
char          DAT_004839ee;         /* entity enable flag */
char          DAT_004839ef;         /* creature density */
short         DAT_004839f0;         /* treasure/pickup config (bytes) */
DWORD         DAT_004839f4;         /* progress timer */
int           DAT_00480878;         /* creature spawn rate */
int           DAT_0048087c;         /* pickup spawn rate */
int           DAT_00480880;         /* treasure spawn rate */

/* Tile boundary lookup tables (initialized by FUN_004155e0) */
int           DAT_004808e4[1024];   /* tile left-start X */
int           DAT_00480ce4[1024];   /* tile right-end X */
int           DAT_004810e4[1024];   /* tile top-start Y */
int           DAT_004814e4[1024];   /* tile bottom-end Y */

/* Tile type/config bytes stored in DAT_00483860 area */
/* DAT_00483963-0048396c are sub-bytes within DAT_00483860 */
/* They're already defined as part of DAT_00483860[0x39c] in level.cpp */
/* We access them by offset: DAT_00483860[0x103] etc. */

/* DAT_00483bf8 is at offset 0x598 from DAT_00483860 - part of the level config */
/* We reference it through DAT_00483860 */

/* Tunnel control constants (from binary at 0x47bba8 and 0x47bbb8) */
static int g_TunnelRadiusStep[4] = { 0x2D, 0x1E, 0x0F, 0x00 };
static int g_TunnelShapeCtrl[4]  = { 0x2D, 0x1E, 0x0F, 0x00 };

/* Info.txt keyword table (17 entries) */
static const char *g_InfoKeywords[17] = {
    "MAKER", "EMAIL", "TEXATTR", "GRASSATTR", "PICATTR",
    "SIGNATTR", "SIGNCOLOR", "SIGNTEXT", "FIXEDSIZE", "WATERLEVEL",
    "WATERC", "BEACHSTYLE", "TEXDARK", "SIGNU", "SIGND",
    "PARDARK", "SIGNA"
};

/* Attribute lookup tables (per-type values for tile codes 1-7) */
static unsigned char g_AttrTileCode[7]  = { 1, 10, 5, 0x14, 4, 0x0C, 0 };
static unsigned char g_AttrTileColor[7] = { 0x0F, 0x0F, 0x0F, 0x10, 0x0F, 0x0F, 0x11 };

/* ===== Helper Functions ===== */

/* FUN_00425820: Integer power: base^exp */
int FUN_00425820(int base, int exp)
{
    int result = 1;
    while (exp > 0) {
        result *= base;
        exp--;
    }
    return result;
}

/* Calc_Power_Of_Two: Find minimum power-of-2 shift such that 2^shift >= value */
int Calc_Power_Of_Two(int value)
{
    int shift = 0;
    if (value > 1) {
        do {
            shift++;
        } while (FUN_00425820(2, shift) < value);
    }
    return shift;
}

/* ===== FUN_004152e0: Convert RGB24 tile data to RGB565 in work buffer ===== */
void FUN_004152e0(int src_rgb24, int tile_idx, int height, int width)
{
    int rec_offset = tile_idx * 0xc;

    /* Store tile dimensions in the tile info table */
    *(int *)((char *)DAT_00481b54 + rec_offset) = width;
    *(int *)((char *)DAT_00481b54 + rec_offset + 4) = height;
    *(int *)((char *)DAT_00481b54 + rec_offset + 8) = DAT_00480840;

    int row_src = 0;
    for (int y = height - 1; y >= 0; y--) {
        int actual_y = row_src;
        if (DAT_004808e0 != '\0') {
            actual_y = y;  /* flip vertically */
        }

        for (int x = 0; x < width; x++) {
            unsigned char *p = (unsigned char *)(src_rgb24 + (actual_y * width + x) * 3);
            unsigned char r = p[0];
            unsigned char g = p[1];
            unsigned char b = p[2];

            /* Convert to RGB565: RRRRR GGGGGG BBBBB */
            unsigned short rgb565 = ((b >> 3) & 0x1F) |
                                    (((g >> 2) & 0x3F) << 5) |
                                    (((r >> 3) & 0x1F) << 11);

            *(unsigned short *)((char *)DAT_0048072c + DAT_00480840 * 2) = rgb565;
            DAT_00480840++;
        }
        row_src++;
    }
}

/* ===== FUN_004153b0: Load a TGA file from ggstuff/<theme>/ ===== */
/* Returns: 1=success, 0=not found, -1=too large */
int FUN_004153b0(const char *filename, int tile_idx)
{
    char path[256];
    sprintf(path, "ggstuff//%s//%s.tga", DAT_00480740, filename);

    FILE *f = fopen(path, "rb");
    if (!f) {
        DAT_004808d0 = 2;
        return 0;
    }

    /* Read TGA header (simplified: 12-byte skip, then 4 bytes for width/height) */
    unsigned char header[18];
    if (fread(header, 1, 18, f) != 18) {
        fclose(f);
        DAT_004808d0 = 1;
        return 0;
    }

    /* TGA: bytes 12-13 = width, 14-15 = height, 16 = bpp, 17 = descriptor */
    int width  = header[12] | (header[13] << 8);
    int height = header[14] | (header[15] << 8);
    int bpp    = header[16];

    if (bpp != 24 && bpp != 32) {
        fclose(f);
        DAT_004808d0 = 1;
        return 0;
    }

    /* Check if origin is top-left (bit 5 of descriptor) */
    DAT_004808e0 = (header[17] & 0x20) ? 0 : 1;

    int pixel_count = width * height;
    if ((int)(pixel_count + DAT_00480840) >= 0x3d0901) {
        fclose(f);
        return -1;
    }

    /* Read pixel data */
    int bytes_per_pixel = bpp / 8;
    int data_size = pixel_count * bytes_per_pixel;
    unsigned char *pixels = (unsigned char *)malloc(data_size);
    if (!pixels) {
        fclose(f);
        return -1;
    }

    /* Skip any image ID */
    if (header[0] > 0) {
        fseek(f, header[0], SEEK_CUR);
    }

    /* Skip color map if present */
    if (header[1]) {
        int cm_len = (header[5] | (header[6] << 8)) * ((header[7] + 7) / 8);
        fseek(f, cm_len, SEEK_CUR);
    }

    fread(pixels, 1, data_size, f);
    fclose(f);

    /* Convert BGR(A) to RGB for FUN_004152e0 */
    unsigned char *rgb_buf = (unsigned char *)malloc(pixel_count * 3);
    if (!rgb_buf) {
        free(pixels);
        return -1;
    }

    for (int i = 0; i < pixel_count; i++) {
        rgb_buf[i * 3 + 0] = pixels[i * bytes_per_pixel + 2]; /* R */
        rgb_buf[i * 3 + 1] = pixels[i * bytes_per_pixel + 1]; /* G */
        rgb_buf[i * 3 + 2] = pixels[i * bytes_per_pixel + 0]; /* B */
    }
    free(pixels);

    FUN_004152e0((int)rgb_buf, tile_idx, height, width);
    free(rgb_buf);

    DAT_004808d0 = 0;
    return 1;
}

/* ===== FUN_00415a60: Parse info.txt for current GG theme ===== */
int FUN_00415a60(void)
{
    /* Set defaults */
    strcpy(DAT_004818e8, "Anonymous");
    strcpy(DAT_00481988, "");

    DAT_00481a28 = 1;   /* TEXATTR tile code */
    DAT_00481a29 = 1;   /* GRASSATTR tile code */
    DAT_00481a2a = 1;   /* PICATTR tile code */
    DAT_00481a2b = 10;  /* SIGNATTR tile code */
    DAT_00481a2c = 0x0F; /* TEXATTR color */
    DAT_00481a2d = 0x0F; /* GRASSATTR color */
    DAT_00481a2e = 0x0F; /* PICATTR color */
    DAT_00481a2f = 0x10; /* SIGNATTR color */
    DAT_00481a30 = 1;   /* sign color */
    DAT_00481a31 = 5;   /* sign text X */
    DAT_00481a32 = 5;   /* sign text Y */
    DAT_00481a34 = 0;   /* fixed width */
    DAT_00481a38 = 0;   /* fixed height */
    ((unsigned char *)&DAT_00481a3c)[0] = 0;    /* water level */
    ((unsigned char *)&DAT_00481a3c)[1] = 100;  /* water R */
    ((unsigned char *)&DAT_00481a3c)[2] = 100;  /* water G */
    ((unsigned char *)&DAT_00481a3c)[3] = 0xFF; /* water B */
    DAT_00481a40 = 0;   /* beach style */
    DAT_00481a41 = 0x5A; /* texture darkness 90 */
    DAT_00481a42 = 0x5A; /* parallax darkness 90 */
    DAT_00481a43 = 0;   /* sign name count */
    DAT_00480708 = 0;
    DAT_00480709 = 0;
    DAT_0048070c = 0;
    DAT_0048070d = 0;
    DAT_00480710 = 0;
    DAT_00480714 = 0x10100; /* default pardark flags */
    DAT_00480718 = 0;

    /* Build path: ggstuff//<theme>//info.txt */
    char path[256];
    sprintf(path, "ggstuff//%s//info.txt", DAT_00480740);

    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }

    int parsed_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        /* Remove newline */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        /* Skip empty lines and comments */
        if (line[0] == '#' || line[0] == '\0') continue;

        /* Check if this is a keyword line (starts with '/') */
        if (line[0] != '/') continue;

        /* Match keyword */
        int keyword_idx = -1;
        for (int i = 0; i < 17; i++) {
            const char *kw = g_InfoKeywords[i];
            int kw_len = (int)strlen(kw);
            int line_len = (int)strlen(line + 1);

            if (kw_len == line_len) {
                int match = 0;
                for (int j = 0; j < kw_len; j++) {
                    if (toupper(line[j + 1]) == kw[j]) {
                        match++;
                    }
                }
                if (match == kw_len) {
                    keyword_idx = i;
                    break;
                }
            }
        }

        if (keyword_idx == -1) continue;

        /* Read the next line as the value */
        if (!fgets(line, sizeof(line), f)) break;
        len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '\0') break;

        switch (keyword_idx) {
        case 0: /* MAKER */
            DAT_00480708 = 1;
            strcpy(DAT_004818e8, line);
            break;

        case 1: /* EMAIL */
            DAT_00480709 = 1;
            strcpy(DAT_00481988, line);
            break;

        case 2: { /* TEXATTR */
            int val = ((atoi(line) - 1) % 7);
            DAT_00481a28 = g_AttrTileCode[val];
            DAT_00481a2c = g_AttrTileColor[val];
            break;
        }

        case 3: { /* GRASSATTR */
            int val = ((atoi(line) - 1) % 7);
            DAT_00481a29 = g_AttrTileCode[val];
            DAT_00481a2d = g_AttrTileColor[val];
            break;
        }

        case 4: { /* PICATTR */
            int val = ((atoi(line) - 1) % 7);
            DAT_00481a2a = g_AttrTileCode[val];
            DAT_00481a2e = g_AttrTileColor[val];
            DAT_0048070c = 1;
            break;
        }

        case 5: { /* SIGNATTR */
            int val = ((atoi(line) - 1) % 7);
            DAT_00481a2b = g_AttrTileCode[val];
            DAT_00481a2f = g_AttrTileColor[val];
            break;
        }

        case 6: /* SIGNCOLOR */
            DAT_00481a30 = (char)atoi(line);
            break;

        case 7: { /* SIGNTEXT: X,Y */
            char *comma = strchr(line, ',');
            if (comma) {
                *comma = '\0';
                DAT_00481a31 = (char)atoi(line);
                DAT_00481a32 = (char)atoi(comma + 1);
            }
            break;
        }

        case 8: { /* FIXEDSIZE: W,H */
            char *comma = strchr(line, ',');
            if (comma) {
                *comma = '\0';
                DAT_00481a34 = atoi(line);
                DAT_00481a38 = atoi(comma + 1);
                DAT_00480710 |= 1;
            }
            break;
        }

        case 9: /* WATERLEVEL */
            ((unsigned char *)&DAT_00481a3c)[0] = (unsigned char)atoi(line);
            break;

        case 10: { /* WATERC: R,G,B */
            char *p1 = strchr(line, ',');
            if (p1) {
                *p1 = '\0';
                char *p2 = strchr(p1 + 1, ',');
                if (p2) {
                    *p2 = '\0';
                    ((unsigned char *)&DAT_00481a3c)[1] = (unsigned char)atoi(line);
                    ((unsigned char *)&DAT_00481a3c)[2] = (unsigned char)atoi(p1 + 1);
                    ((unsigned char *)&DAT_00481a3c)[3] = (unsigned char)atoi(p2 + 1);
                }
            }
            break;
        }

        case 11: /* BEACHSTYLE */
            if (toupper(line[0]) == 'Y' && toupper(line[1]) == 'E' && toupper(line[2]) == 'S') {
                DAT_00481a40 = 1;
            } else if (toupper(line[0]) == 'N' && toupper(line[1]) == 'O') {
                DAT_00481a40 = 0;
            }
            break;

        case 12: /* TEXDARK */
            DAT_00481a41 = (char)atoi(line);
            break;

        case 13: /* SIGNU (sign name, text above) */
            if (DAT_00481a43 < 16) {
                strncpy(&DAT_00481a44[(int)DAT_00481a43 * 16], line, 15);
                DAT_00481a44[(int)DAT_00481a43 * 16 + 15] = '\0';
                DAT_00481b44[(int)DAT_00481a43] = 0;
                DAT_00481a43++;
            }
            break;

        case 14: /* SIGND (sign name, text below) */
            if (DAT_00481a43 < 16) {
                strncpy(&DAT_00481a44[(int)DAT_00481a43 * 16], line, 15);
                DAT_00481a44[(int)DAT_00481a43 * 16 + 15] = '\0';
                DAT_00481b44[(int)DAT_00481a43] = 1;
                DAT_00481a43++;
            }
            break;

        case 15: /* PARDARK */
            DAT_00481a42 = (char)atoi(line);
            break;

        case 16: /* SIGNA (sign name, alongside) */
            if (DAT_00481a43 < 16) {
                strncpy(&DAT_00481a44[(int)DAT_00481a43 * 16], line, 15);
                DAT_00481a44[(int)DAT_00481a43 * 16 + 15] = '\0';
                DAT_00481b44[(int)DAT_00481a43] = 2;
                DAT_00481a43++;
            }
            break;
        }

        parsed_count++;
    }

    fclose(f);
    return (parsed_count > 0) ? 1 : 0;
}

/* ===== FUN_00416ad0: Allocate map buffers ===== */
void FUN_00416ad0(void)
{
    /* Free previously allocated level buffers (prevents leak/crash on consecutive loads) */
    if (DAT_00481f50 != NULL) {
        Mem_Free(DAT_00481f50);
        DAT_00481f50 = NULL;
        DAT_0048782c = NULL;
    }
    if (DAT_00487814 != NULL) {
        Mem_Free(DAT_00487814);
        DAT_00487814 = NULL;
    }
    if (DAT_00489ea4 != NULL) {
        Mem_Free(DAT_00489ea4);
        DAT_00489ea4 = NULL;
    }
    if (DAT_00489ea8 != NULL) {
        Mem_Free(DAT_00489ea8);
        DAT_00489ea8 = NULL;
    }
    if (DAT_00489ea0 != NULL) {
        Mem_Free(DAT_00489ea0);
        DAT_00489ea0 = NULL;
    }

    DAT_004879f8 = (DAT_004879f0 >> 4) + 2;
    DAT_004879fc = (DAT_004879f4 >> 4) + 2;
    DAT_00487a04 = DAT_004879f0 / 0x12 + 2;
    DAT_00487a08 = DAT_004879f4 / 0x12 + 2;

    unsigned int shift = Calc_Power_Of_Two(DAT_004879f0);
    DAT_00487a18 = (DAT_00487a18 & ~0xFF) | (shift & 0xFF);
    DAT_00487a00 = FUN_00425820(2, shift & 0xFF);

    int alloc_shift = Calc_Power_Of_Two(DAT_004879f0 + 0xe);
    int alloc_stride = FUN_00425820(2, alloc_shift);
    int alloc_size = alloc_stride * (DAT_004879f4 + 0xe) * 3;

    void *buf = Mem_Alloc(alloc_size);

    DAT_00489278 = 0;
    g_MemoryTracker += alloc_size;

    /* Tilemap sits after 2 * height rows of the RGB565 buffer */
    DAT_0048782c = (void *)((char *)buf + (DAT_004879f4 << (shift & 0x1F)) * 2);
    DAT_00481f50 = buf;

    /* Clear all pixels and tilemap */
    for (int y = 0; y < (int)DAT_004879f4; y++) {
        for (int x = 0; x < (int)DAT_004879f0; x++) {
            int offset = (y << (shift & 0x1F)) + x;
            *(unsigned short *)((char *)DAT_00481f50 + offset * 2) = 0;
            *((unsigned char *)DAT_0048782c + offset) = 0;
        }
    }

    /* Initialize 7-pixel border (top/bottom) */
    for (int border = 0; border < 7; border++) {
        for (int x = 0; x < (int)DAT_004879f0; x++) {
            /* Top border */
            int off_top = (border << (shift & 0x1F)) + x;
            *((unsigned char *)DAT_0048782c + off_top) = 5;
            *(unsigned short *)((char *)DAT_00481f50 + off_top * 2) = 0x60;

            /* Bottom border */
            int off_bot = ((DAT_004879f4 - border - 1) << (shift & 0x1F)) + x;
            *((unsigned char *)DAT_0048782c + off_bot) = 5;
            *(unsigned short *)((char *)DAT_00481f50 + off_bot * 2) = 0x60;
        }
    }

    /* Initialize 7-pixel border (left/right) */
    for (int y = 0; y < (int)DAT_004879f4; y++) {
        for (int x = 0; x < 7; x++) {
            /* Left border */
            int off_left = (y << (shift & 0x1F)) + x;
            *((unsigned char *)DAT_0048782c + off_left) = 5;
            *(unsigned short *)((char *)DAT_00481f50 + off_left * 2) = 0x60;

            /* Right border */
            int off_right = (y << (shift & 0x1F)) + DAT_004879f0 - 1 - x;
            *((unsigned char *)DAT_0048782c + off_right) = 5;
            *(unsigned short *)((char *)DAT_00481f50 + off_right * 2) = 0x60;
        }
    }

    /* Allocate grid buffers */
    DAT_00487814 = Mem_Alloc((DAT_004879fc + 1) * (DAT_004879f8 + 1));
    g_MemoryTracker += (DAT_004879fc + 1) * (DAT_004879f8 + 1);

    DAT_00489ea4 = Mem_Alloc((DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    g_MemoryTracker += (DAT_00487a08 + 1) * (DAT_00487a04 + 1);
    memset(DAT_00489ea4, 0, DAT_00487a08 * DAT_00487a04);

    DAT_00489ea8 = Mem_Alloc((DAT_00487a08 + 1) * (DAT_00487a04 + 1));
    g_MemoryTracker += (DAT_00487a08 + 1) * (DAT_00487a04 + 1);
    memset(DAT_00489ea8, 0, DAT_00487a08 * DAT_00487a04);

    DAT_00483960 = 0;

    /* Set tile type config bytes (within DAT_00483860) */
    DAT_00483860[0x103] = 0x28;  /* DAT_00483963 */
    DAT_00483860[0x104] = 0x3C;  /* DAT_00483964[0] */
    DAT_00483860[0x105] = 0x8C;  /* DAT_00483964[1] */

    FUN_00421310();

    DAT_00483860[0x106] = 1;    /* DAT_00483964[2] */
    DAT_00483860[0x107] = 10;   /* DAT_00483964[3] */
    DAT_00483860[0x108] = 10;   /* DAT_00483968[0] */
    DAT_00483860[0x109] = 10;   /* DAT_00483968[1] */
    DAT_00483860[0x10A] = 10;   /* DAT_00483968[2] */
    DAT_00483860[0x10B] = 0;    /* DAT_00483968[3] */
    DAT_00483860[0x10C] = 0;    /* DAT_0048396c */

    DAT_0048396d = 1;           /* generated-map flag */
    DAT_004839f4 = 0;
    DAT_004839ef = 10;
    DAT_004839f4 = timeGetTime();
    ((unsigned char *)&DAT_004839f0)[0] = 10;
    ((unsigned char *)&DAT_004839f0)[1] = 10;

    /* DAT_00483bf8[0] = 0 (within the config region) */
    DAT_00483860[0x598] = 0;
}

/* ===== FUN_00417700: Place a tile sprite at position ===== */
void FUN_00417700(int x, int y, unsigned int sprite_idx)
{
    int rec_offset = (sprite_idx & 0xFF) * 0xc;
    int tile_w = *(int *)((char *)DAT_00481b54 + rec_offset);
    int tile_h = *(int *)((char *)DAT_00481b54 + rec_offset + 4);
    unsigned int tile_data_start = *(unsigned int *)((char *)DAT_00481b54 + rec_offset + 8);
    int half_w = tile_w / 2;
    unsigned int pixel_idx = tile_data_start;
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;

    for (int row = 0; row < tile_h; row++) {
        int py = row + (y - tile_h / 2);
        int px = x - half_w;

        for (int col = 0; col < tile_w; col++) {
            if (px >= DAT_0048071c && px < DAT_00480720 &&
                py >= DAT_00480724 && py < DAT_00480728) {

                unsigned short pix = *(unsigned short *)((char *)DAT_0048072c + pixel_idx * 2);
                if (pix != 0) {
                    unsigned char *tilemap = (unsigned char *)DAT_0048782c +
                                             (py << shift) + px;
                    if (*(char *)((unsigned int)(*tilemap) * 0x20 + (int)DAT_00487928) != '\0') {
                        DAT_00480844++;
                        *tilemap = 0x51;
                        *(unsigned short *)((char *)DAT_00481f50 +
                                            ((py << shift) + px) * 2) = (unsigned short)DAT_00480860;
                    }
                }
            }
            pixel_idx++;
            px++;
        }
    }
}

/* ===== FUN_00417850: Place a tile (for cave terrain, with tile-aware collision) ===== */
void FUN_00417850(int param_1, int param_2, unsigned char param_3)
{
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;
    unsigned char cur_tile = *(unsigned char *)((int)DAT_0048782c + (param_2 << shift) + param_1);

    if (cur_tile == 0 || cur_tile > 0x7F) {
        /* Look up tile info for the main tile sprite */
        int *tile_info = (int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc);
        int tile_h = tile_info[1];
        int tile_w = tile_info[0];
        int tile_start = tile_info[2];

        /* Get the pixel at this position in the tile */
        unsigned int pix_idx = ((DAT_00480868 + param_2) % tile_h) * tile_w +
                               (param_1 + DAT_00480864) % tile_w + tile_start;
        unsigned short pix_val = *(unsigned short *)((char *)DAT_0048072c + pix_idx * 2);

        /* Calculate tile boundaries */
        int tile_x = ((param_1 + DAT_00480864) / tile_w) * tile_w - DAT_00480864;
        int tile_y = ((DAT_00480868 + param_2) / tile_h) * tile_h - DAT_00480868;

        if (DAT_00480ce4[pix_val] + tile_x < param_1) tile_x += tile_w;
        if (DAT_004814e4[pix_val] + tile_y < param_2) tile_y += tile_h;
        if (param_1 < DAT_004808e4[pix_val] + tile_x) tile_x -= tile_w;
        if (param_2 < DAT_004810e4[pix_val] + tile_y) tile_y -= tile_h;

        int start_x = DAT_004808e4[pix_val] + tile_x;
        int start_y = DAT_004810e4[pix_val] + tile_y;
        int extent_w = DAT_00480ce4[pix_val] - DAT_004808e4[pix_val];
        int extent_h = DAT_004814e4[pix_val] - DAT_004810e4[pix_val];

        for (int dy = 0; dy < extent_h; dy++) {
            int cy = start_y + dy;
            int *info = (int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc);
            int tw = info[0];
            int pixel_start = info[2];
            int row_in_tile = (DAT_00480868 + cy) % info[1];
            int col_in_tile = (DAT_00480864 + start_x) % tw;

            for (int dx = 0; dx < extent_w; dx++) {
                int cx = start_x + dx;

                if (cx >= DAT_0048071c && cx < DAT_00480720 &&
                    cy >= DAT_00480724 && cy < DAT_00480728) {
                    /* Check if this pixel matches our reference pixel */
                    unsigned short test_pix = *(unsigned short *)((char *)DAT_0048072c +
                                              (col_in_tile + row_in_tile * tw + pixel_start) * 2);
                    if (test_pix == pix_val) {
                        unsigned char *tmap = (unsigned char *)DAT_0048782c + (cy << shift) + cx;
                        unsigned char old = *tmap;
                        if (old > 0x7F) old += 0x80;  /* unwrap */

                        char *type_info = (char *)((unsigned int)param_3 * 0x20 + (int)DAT_00487928);
                        char *old_info = (char *)((unsigned int)old * 0x20 + (int)DAT_00487928);

                        if (old_info[0] != '\0') {
                            DAT_00480844++;
                        }

                        if (type_info[0x1b] == '\0') {
                            /* Non-indestructible new tile */
                            if (old_info[0x1b] != '\0') {
                                *tmap = old;  /* keep old if indestructible */
                            } else if (old_info[0] == '\0') {
                                if (old_info[4] == '\0') {
                                    *tmap = param_3;
                                } else {
                                    *tmap = 0x56;
                                }
                            } else {
                                *tmap = 0x51;
                            }
                        } else {
                            /* Indestructible new tile */
                            if (old_info[0x1b] != '\0') {
                                *tmap = old;
                            } else if (old_info[0] == '\0') {
                                if (old_info[4] != '\0') {
                                    *tmap = 0x56;
                                } else {
                                    *tmap = old;
                                }
                            } else {
                                *tmap = 0x51;
                            }
                        }

                        *(unsigned short *)((char *)DAT_00481f50 + ((cy << shift) + cx) * 2) =
                            (unsigned short)DAT_00480860;
                    }
                }

                col_in_tile++;
                if (col_in_tile >= tw) col_in_tile = 0;
            }
        }
    }
}

/* ===== FUN_00417d90: Path validation (raycast) ===== */
int FUN_00417d90(int x, int y, unsigned int angle, int steps)
{
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;
    int *sincos = (int *)DAT_00487ab0;
    unsigned int idx = angle & 0x7FF;
    unsigned int cos_idx = (idx - 0x400) & 0x7FF;

    /* Start from the midpoint of the ray */
    int half_sin = (sincos[idx] * steps) / 2;
    int start_x = ((half_sin + (half_sin >> 31 & 0x7FFFF)) >> 19) + x;

    int half_cos = (sincos[0x600 + ((angle & 0x7FF) - 0x400)] * steps) / 2;
    int start_y = ((half_cos + (half_cos >> 31 & 0x7FFFF)) >> 19) + y;

    int step_x = sincos[cos_idx] * 0x1E;
    int step_y = sincos[0x200 + cos_idx] * 0x1E;

    int px = start_x;
    int py = start_y;

    for (int i = 0; i < steps; i += 0x1E) {
        if (px < DAT_0048071c || px >= DAT_00480720 ||
            py < DAT_00480724 || py >= DAT_00480728) {
            return 0;
        }

        unsigned char tile = *(unsigned char *)((int)DAT_0048782c + (py << shift) + px);
        if (*(char *)((unsigned int)tile * 0x20 + (int)DAT_00487928) == '\0') {
            return 0;
        }

        px += ((step_x + (step_x >> 31 & 0x7FFFF)) >> 19);
        py += ((step_y + (step_y >> 31 & 0x7FFFF)) >> 19);
    }

    return 1;
}

/* ===== FUN_00417b40: Tunnel carving (noise-based variable radius) ===== */
void FUN_00417b40(int cx, int cy, int radius_mult)
{
    /* Generate random noise parameters (8 octaves) */
    int freq[8], phase[8], amp[8];
    for (int i = 0; i < 8; i++) {
        freq[i]  = (rand() & 7) + 1;
        phase[i] = rand() & 0x7FF;
        amp[i]   = (rand() & 0x7F) + 0x40;
    }

    int *sincos = (int *)DAT_00487ab0;
    int shape_val = g_TunnelShapeCtrl[DAT_0048084c];

    for (int scale = 1; scale <= radius_mult * 0x1E; scale += g_TunnelRadiusStep[DAT_0048084c]) {
        int step = (shape_val << 11) / (scale * 6);
        if (step == 0) step = 1;

        for (int angle = 0; angle < 0x800; angle += step) {
            /* Sum 8 noise octaves to compute variable radius */
            int r = scale * 8;
            for (int oct = 0; oct < 8; oct++) {
                int noise = sincos[(freq[oct] * angle + phase[oct]) & 0x7FF] * amp[oct] * scale;
                noise = (noise + (noise >> 31 & 0x7FFFF)) >> 19;
                r += ((noise + (noise >> 31 & 0x1F)) >> 5);
            }

            if (r < scale) r = scale;
            if (r > scale * 0x40) r = scale * 0x40;

            /* Calculate position from center using sin/cos LUT */
            int dx = sincos[angle] * r;
            dx = (dx + (dx >> 31 & 0x7FFFF)) >> 19;
            int px = ((dx + (dx >> 31 & 0xF)) >> 4) + cx;

            int dy = sincos[0x200 + angle] * r;
            dy = (dy + (dy >> 31 & 0x7FFFF)) >> 19;
            int py = ((dy + (dy >> 31 & 0xF)) >> 4) + cy;

            if (DAT_00480898 < 2) {
                /* Simple mode: use sprite tiles */
                int sprite_idx;
                char base;
                if (cy < py && DAT_00480890 != 0) {
                    sprite_idx = rand() % DAT_00480890;
                    base = DAT_0048088c;
                } else {
                    sprite_idx = rand() % DAT_00480888;
                    base = DAT_00480884;
                }
                FUN_00417700(px, py, (unsigned int)(unsigned char)((char)sprite_idx + base));
            } else {
                /* Complex mode: use tile-aware placement */
                unsigned char shift_val = (unsigned char)DAT_00487a18 & 0x1F;
                if (px >= DAT_0048071c && px < DAT_00480720 &&
                    py >= DAT_00480724 && py < DAT_00480728) {
                    unsigned char tile = *(unsigned char *)((int)DAT_0048782c + (py << shift_val) + px);
                    if (*(char *)((unsigned int)tile * 0x20 + (int)DAT_00487928) != '\0') {
                        FUN_00417850(px, py, 0x51);
                    }
                }
            }
        }
    }
}

/* ===== FUN_00418520: Simple terrain generation (beach/sine-wave) ===== */
void FUN_00418520(void)
{
    unsigned int r1 = rand();
    unsigned int r2 = rand();
    unsigned int r3 = rand();

    if (DAT_0048071c >= DAT_00480720) return;

    unsigned int phase1 = (r2 & 0x7FF) + DAT_0048071c * 4;
    unsigned int phase2 = (r1 & 0x7FF) + DAT_0048071c * 2;
    unsigned int phase3 = (r3 & 0x7FF) + DAT_0048071c * 3;

    int *sincos = (int *)DAT_00487ab0;

    for (int x = DAT_0048071c; x < DAT_00480720; x += 0x28) {
        /* Sum three sine waves to get terrain height */
        int height = DAT_00480728 +
                     (sincos[phase2 & 0x7FF] >> 0x0D) +
                     (sincos[phase1 & 0x7FF] >> 0x0E) +
                     (sincos[phase3 & 0x7FF] >> 0x0E);

        if (height < 0xFA) height = 0xFA;

        if (height < DAT_00480728) {
            /* Carve tunnels from the surface down */
            for (int y = height; y < DAT_00480728; y += 0x28) {
                FUN_00417b40(x, y, 2);
            }
        }

        phase3 += 0x78;
        phase1 += 0xA0;
        phase2 += 0x50;
    }
}

/* ===== FUN_00417ea0: Standard terrain generation (cave/random-walk) ===== */
void FUN_00417ea0(void)
{
    int pos_x = 0, pos_y = 0;
    int walk_count = 0;
    unsigned int dir_bias = rand() & 1;
    int total_area = DAT_004879f0 + DAT_004879f4 * 2;

    /* Phase 1: Carve border tunnels (if DAT_00480858 > 0) */
    if (DAT_00480858 > 0) {
        int y = DAT_00480724;
        while (y < DAT_00480728) {
            FUN_00417b40(DAT_0048071c, y, 2);
            FUN_00417b40(DAT_00480720, y, 2);
            y += 0x1E;
        }

        int x = DAT_0048071c;
        while (x < DAT_00480720) {
            FUN_00417b40(x, DAT_00480728 - 7, 2);
            if (DAT_00480858 == 2) {
                FUN_00417b40(x, DAT_00480724, 2);
            }
            x += 0x1E;
        }
    }

    /* Backtrack stack */
    int stack_x[100], stack_y[100];
    int stack_depth = 0;

    int iteration = 0;

    while (iteration < 500) {
        walk_count++;

        if (walk_count > (int)(total_area / 0x140 + dir_bias)) {
            /* Find valid random position */
            int attempts = 0;
            do {
                pos_x = rand() % DAT_004879f0;
                pos_y = rand() % DAT_004879f4;
                attempts++;

                /* Verify area is solid (all tiles passable) */
                int valid = 1;
                for (int dy = 0; dy < 200 && valid; dy += 0x12) {
                    for (int dx = 0; dx < 200 && valid; dx += 0x12) {
                        int tx = dx - 100 + pos_x;
                        int ty = pos_y - 100 + dy;
                        if (tx < DAT_0048071c || tx >= DAT_00480720 ||
                            ty < DAT_00480724 || ty >= DAT_00480728) {
                            valid = 0;
                        } else {
                            unsigned char tile = *(unsigned char *)((int)DAT_0048782c +
                                                (ty << ((unsigned char)DAT_00487a18 & 0x1F)) + tx);
                            if (*(char *)((unsigned int)tile * 0x20 + (int)DAT_00487928) == '\0') {
                                valid = 0;
                            }
                        }
                    }
                }
                if (valid) break;
            } while (attempts < 100);

            if (attempts == 100) return;
        } else {
            /* Pick random edge start */
            unsigned int edge = rand() & 3;
            while (edge == 0 && DAT_00480858 == 1) {
                edge = rand() & 3;
            }

            switch (edge) {
            case 0:
                pos_x = rand() % DAT_004879f0;
                pos_y = DAT_00480724;
                break;
            case 1:
                pos_x = rand() % DAT_004879f0;
                pos_y = DAT_00480728;
                break;
            case 2:
                pos_x = DAT_00480720;
                pos_y = rand() % DAT_004879f4;
                break;
            case 3:
                pos_x = DAT_0048071c;
                pos_y = rand() % DAT_004879f4;
                break;
            }
        }

        /* Random walk */
        stack_depth = 0;
        int walk_steps = rand() % 0x29 + 0x32;
        int steps_done = 0;

        if (walk_steps <= 0) continue;

        FUN_00417b40(pos_x, pos_y, 2);

        while (1) {
            int retries = 0;

            while (retries < 0x1A) {
                /* Random backtrack check */
                unsigned int bt = rand() & 3;
                if (bt == 0 && stack_depth > 0) goto backtrack;

                unsigned int angle = rand() & 0x7FF;
                int *sincos = (int *)DAT_00487ab0;

                int dx = sincos[angle] * 4;
                dx = (dx + (dx >> 31 & 0xFFF)) >> 12;
                int new_x = ((dx + (dx >> 31 & 3)) >> 2) + pos_x;

                int dy = sincos[0x200 + angle] * 4;
                dy = (dy + (dy >> 31 & 0xFFF)) >> 12;
                int new_y = ((dy + (dy >> 31 & 3)) >> 2) + pos_y;

                /* Bounds and passability check */
                if (new_x >= DAT_0048071c && new_x <= DAT_00480720 &&
                    new_y >= DAT_00480724 && new_y <= DAT_00480728) {

                    /* Check area around target is solid */
                    int area_ok = 1;
                    for (int cy = 0; cy < 0x78 && area_ok; cy += 0x12) {
                        for (int cx = 0; cx < 0x78 && area_ok; cx += 0x12) {
                            int tx = cx - 0x3C + new_x;
                            int ty = new_y - 0x3C + cy;
                            if (tx < DAT_0048071c || tx >= DAT_00480720 ||
                                ty < DAT_00480724 || ty >= DAT_00480728) {
                                area_ok = 0;
                            } else {
                                unsigned char tile = *(unsigned char *)((int)DAT_0048782c +
                                    (ty << ((unsigned char)DAT_00487a18 & 0x1F)) + tx);
                                if (*(char *)((unsigned int)tile * 0x20 + (int)DAT_00487928) == '\0') {
                                    area_ok = 0;
                                }
                            }
                        }
                    }

                    if (area_ok) {
                        /* Validate path with raycast */
                        if (FUN_00417d90(new_x, new_y, angle + 0x200, 0x15E)) {
                            int sv = sincos[angle];
                            int cv = sincos[0x200 + angle];

                            pos_x += ((sv + (sv >> 31 & 0x3FF)) >> 10) / 0xC;
                            pos_y += ((cv + (cv >> 31 & 0x3FF)) >> 10) / 0xC;
                            FUN_00417b40(pos_x, pos_y, 2);

                            sv = sincos[angle];
                            cv = sincos[0x200 + angle];
                            pos_x += ((sv + (sv >> 31 & 0x3FF)) >> 10) / 0xC;
                            pos_y += ((cv + (cv >> 31 & 0x3FF)) >> 10) / 0xC;

                            stack_x[stack_depth] = pos_x;
                            stack_y[stack_depth] = pos_y;
                            stack_depth++;

                            if (DAT_00480848 < DAT_00480844) return;

                            steps_done++;
                            if (steps_done < walk_steps) {
                                FUN_00417b40(pos_x, pos_y, 2);
                            }
                            goto next_step;
                        }
                    }
                }

                retries++;
            }

backtrack:
            stack_depth--;
            if (stack_depth < 0) break;
            pos_x = stack_x[stack_depth];
            pos_y = stack_y[stack_depth];
            continue;

next_step:
            if (steps_done >= walk_steps) break;
        }

        iteration++;
    }
}

/* ===== FUN_004155e0: Tile boundary calculator ===== */
/* Analyzes the second TGA tile (t2) to find unique color regions and their extents */
void FUN_004155e0(void)
{
    int *tile_info = (int *)((char *)DAT_00481b54 + (DAT_00480894 * 3 + 3) * 4);
    int tile_w = tile_info[0];
    int tile_h = tile_info[1];
    int tile_start = tile_info[2];

    int unique_count = 0;
    int palette[256];
    int tile_data_start = *(int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc + 0x14);

    for (int y = tile_h - 1; y >= 0; y--) {
        int src_y = (DAT_004808e0 != '\0') ? (tile_h - 1 - y) : y;

        for (int x = 0; x < tile_w; x++) {
            int src_idx = src_y * tile_w + x;
            unsigned char *pixel = (unsigned char *)((int)DAT_00481cf8 + src_idx * 3);

            /* Pack RGB into single int for comparison */
            int color = ((int)pixel[0] << 16) | ((int)pixel[1] << 8) | pixel[2];

            int found = -1;
            for (int i = 0; i < unique_count; i++) {
                if (palette[i] == color) {
                    found = i;
                    break;
                }
            }

            if (found == -1) {
                palette[unique_count] = color;
                *(short *)((char *)DAT_0048072c + tile_data_start * 2) = (short)unique_count;
                if (unique_count < 0xFE) unique_count++;
            } else {
                *(short *)((char *)DAT_0048072c + tile_data_start * 2) = (short)found;
            }

            tile_data_start++;
        }
    }

    /* Initialize boundary tables */
    for (int i = 0; i < unique_count; i++) {
        DAT_004808e4[i] = 0;
        DAT_00480ce4[i] = -10000;  /* sentinel */
        DAT_004810e4[i] = 0;
        DAT_004814e4[i] = 0;
    }

    /* Calculate tile boundaries for each unique color */
    int idx = *(int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc + 0x14);

    for (int row = 0; row < tile_h; row++) {
        for (int col = 0; col < tile_w; col++) {
            unsigned char color_idx = *(unsigned char *)((char *)DAT_0048072c + idx * 2);

            if (DAT_00480ce4[color_idx] == -10000) {
                /* Find right extent */
                int extent = 1;
                while (extent < tile_w) {
                    int check_col = (extent + col) % tile_w;
                    int found_in_row = 0;
                    for (int r = 0; r < tile_h; r++) {
                        unsigned short *p = (unsigned short *)((char *)DAT_0048072c +
                            (check_col + tile_data_start + r * tile_w) * 2);
                        if (*p == color_idx) { found_in_row = 1; break; }
                    }
                    if (!found_in_row) break;
                    extent++;
                }
                DAT_00480ce4[color_idx] = extent + col;

                /* Find left extent */
                extent = 1;
                int left_col = col;
                while (extent < tile_w) {
                    left_col--;
                    if (left_col < 0) left_col += tile_w;
                    int found_in_row = 0;
                    for (int r = 0; r < tile_h; r++) {
                        unsigned short *p = (unsigned short *)((char *)DAT_0048072c +
                            (left_col + tile_data_start + r * tile_w) * 2);
                        if (*p == color_idx) { found_in_row = 1; break; }
                    }
                    if (!found_in_row) break;
                    extent++;
                }
                DAT_004808e4[color_idx] = col - extent + 1;

                /* Find bottom extent */
                extent = 1;
                while (extent < tile_h) {
                    int check_row = (extent + row) % tile_h;
                    int found_in_col = 0;
                    for (int c = 0; c < tile_w; c++) {
                        unsigned short *p = (unsigned short *)((char *)DAT_0048072c +
                            (c + tile_data_start + check_row * tile_w) * 2);
                        if (*p == color_idx) { found_in_col = 1; break; }
                    }
                    if (!found_in_col) break;
                    extent++;
                }
                DAT_004814e4[color_idx] = extent + row;

                /* Find top extent */
                extent = 1;
                int top_row = row;
                while (extent < tile_h) {
                    top_row--;
                    if (top_row < 0) top_row += tile_h;
                    int found_in_col = 0;
                    for (int c = 0; c < tile_w; c++) {
                        unsigned short *p = (unsigned short *)((char *)DAT_0048072c +
                            (c + tile_data_start + top_row * tile_w) * 2);
                        if (*p == color_idx) { found_in_col = 1; break; }
                    }
                    if (!found_in_col) break;
                    extent++;
                }
                DAT_004810e4[color_idx] = row - extent + 1;
            }

            idx++;
        }
    }
}

/* ===== FUN_00416320: GG tile graphics loader ===== */
int FUN_00416320(void)
{
    /* Allocate work buffers */
    DAT_0048072c = Mem_Alloc(8000000);
    g_MemoryTracker += 8000000;
    DAT_00481b54 = Mem_Alloc(0xc00);
    g_MemoryTracker += 0xc00;
    DAT_00480738 = Mem_Alloc(0x800);
    g_MemoryTracker += 0x800;
    DAT_00480734 = Mem_Alloc(0x3000);
    g_MemoryTracker += 0x3000;
    DAT_00480730 = Mem_Alloc(0x3000);
    g_MemoryTracker += 0x3000;
    DAT_004818e4 = Mem_Alloc(0x1000);
    g_MemoryTracker += 0x1000;

    DAT_00480840 = 0;
    DAT_004808d4 = 0;
    DAT_004808d8 = 0;
    DAT_004808dc = 0;
    DAT_00480884 = 0;
    DAT_00480888 = 0;

    /* Load "s1, s2, ... s255" sprite TGAs */
    int tile_slot = 0;
    char name[64];
    for (int i = 0; i < 255; i++) {
        sprintf(name, "s%d", i + 1);
        int next_slot = tile_slot + 1;
        int result = FUN_004153b0(name, tile_slot);
        if (result == 0) break;
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
        DAT_00480888++;
        tile_slot = next_slot;
    }

    if (DAT_00480888 == 0) return 0;

    /* Load "sd1, sd2, ..." sub-decoration sprites */
    DAT_00480890 = 0;
    DAT_0048088c = (char)tile_slot;
    for (int i = 0; i < 255; i++) {
        int prev_slot = tile_slot;
        sprintf(name, "sd%d", i + 1);
        tile_slot = prev_slot + 1;
        int result = FUN_004153b0(name, prev_slot);
        if (result == 0) break;
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
        DAT_00480890++;
    }

    /* Load main tile JPEG (t1.jpg) */
    DAT_00480898 = 0;
    char path[256];
    sprintf(path, "ggstuff//%s//t1.jpg", DAT_00480740);

    DAT_00480894 = tile_slot;
    void *jpeg_data = Load_JPEG_Asset(path, &g_ImageWidth, &g_ImageHeight);
    if (!jpeg_data) {
        DAT_004808d0 = 3;
        return 0;
    }

    DAT_004808e0 = 0;
    if ((int)(g_ImageHeight * g_ImageWidth + DAT_00480840) >= 0x3d0901) {
        stbi_image_free(jpeg_data);
        DAT_004808d0 = 4;
        return 0;
    }

    FUN_004152e0((int)jpeg_data, tile_slot, g_ImageHeight, g_ImageWidth);
    stbi_image_free(jpeg_data);
    DAT_00480898++;
    DAT_004808d0 = 0;

    /* Try loading t2.tga */
    int t2_slot = tile_slot + 2;
    int result = FUN_004153b0("t2", tile_slot + 1);
    if (result == -1) {
        DAT_004808d0 = 4;
        return 0;
    }
    if (result == 1) {
        DAT_00480898++;
        FUN_004155e0();
        t2_slot = tile_slot + 3;

        /* Try loading t3.tga */
        result = FUN_004153b0("t3", tile_slot + 2);
        if (result == 1) {
            DAT_00480898++;
        }
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
    }

    /* Set random texture offsets */
    int *main_tile = (int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc);
    DAT_00480864 = rand() % main_tile[0];
    DAT_00480868 = rand() % main_tile[1];

    /* Load extra tile JPEGs (px1.jpg, ex1.jpg) */
    DAT_004808a8 = 0;
    sprintf(path, "ggstuff//%s//px1.jpg", DAT_00480740);
    DAT_004808a4 = t2_slot;
    jpeg_data = Load_JPEG_Asset(path, &g_ImageWidth, &g_ImageHeight);
    if (jpeg_data) {
        DAT_004808e0 = 0;
        if ((int)(g_ImageHeight * g_ImageWidth + DAT_00480840) >= 4000000) {
            stbi_image_free(jpeg_data);
            DAT_004808d0 = 4;
            return 0;
        }
        FUN_004152e0((int)jpeg_data, t2_slot, g_ImageHeight, g_ImageWidth);
        stbi_image_free(jpeg_data);
        DAT_004808a8++;
        DAT_004808d0 = 0;
    } else {
        DAT_004808d0 = 3;
    }

    /* Load landscape tile (ex1.jpg) */
    DAT_004808a0 = 0;
    sprintf(path, "ggstuff//%s//ex1.jpg", DAT_00480740);
    DAT_0048089c = t2_slot + 1;
    jpeg_data = Load_JPEG_Asset(path, &g_ImageWidth, &g_ImageHeight);
    if (jpeg_data) {
        DAT_004808e0 = 0;
        if ((int)(g_ImageHeight * g_ImageWidth + DAT_00480840) >= 4000000) {
            stbi_image_free(jpeg_data);
            DAT_004808d0 = 4;
            return 0;
        }
        FUN_004152e0((int)jpeg_data, t2_slot + 1, g_ImageHeight, g_ImageWidth);
        stbi_image_free(jpeg_data);
        DAT_004808a0++;
        DAT_004808d0 = 0;
    } else {
        DAT_004808d0 = 3;
    }

    /* Load decoration TGA (l1.tga) */
    DAT_004808c0 = 0;
    DAT_004808bc = t2_slot + 2;
    result = FUN_004153b0("l1", t2_slot + 2);
    if (result == 1) {
        DAT_004808c0++;
    }
    if (result == -1) {
        DAT_004808d0 = 4;
        return 0;
    }

    /* Load creature sprites (d1, d2, ...) */
    DAT_004808b8 = 0;
    int creature_slot = t2_slot + 3;
    DAT_004808b4 = creature_slot;
    for (int i = 0; i < 255; i++) {
        sprintf(name, "d%d", i + 1);
        int prev = creature_slot;
        creature_slot = prev + 1;
        result = FUN_004153b0(name, prev);
        if (result == 0) break;
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
        DAT_004808b8++;
    }

    /* Load pickup sprites (x1, x2, ...) */
    DAT_004808c8 = 0;
    DAT_004808c4 = creature_slot;
    for (int i = 0; i < 255; i++) {
        sprintf(name, "x%d", i + 1);
        int prev = creature_slot;
        creature_slot = prev + 1;
        result = FUN_004153b0(name, prev);
        if (result == 0) break;
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
        DAT_004808c8++;
    }

    /* Load treasure sprites (p1, p2, ...) */
    DAT_004808b0 = 0;
    DAT_004808ac = creature_slot;
    for (int i = 0; i < 255; i++) {
        sprintf(name, "p%d", i + 1);
        result = FUN_004153b0(name, creature_slot);
        if (result == 0) break;
        if (result == -1) {
            DAT_004808d0 = 4;
            return 0;
        }
        DAT_004808b0++;
        creature_slot++;
    }

    /* Update tile type config for terrain rendering */
    DAT_00483860[0x105] = (unsigned char)200; /* DAT_00483964[1] */
    DAT_00483860[0x103] = 0x32;               /* DAT_00483963 */
    DAT_00483860[0x104] = 0x32;               /* DAT_00483964[0] */
    FUN_00421310();

    return 1;
}

/* ===== FUN_00414b00: Tile post-processing (remap P-X codes) ===== */
void FUN_00414b00(void)
{
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;

    for (int y = DAT_00480724; y < DAT_00480728; y++) {
        for (int x = DAT_0048071c; x < DAT_00480720; x++) {
            char *tile = (char *)((int)DAT_0048782c + (y << shift) + x);
            char val = *tile;
            char replacement;

            switch (val) {
            case 'P': replacement = DAT_00481a29; break; /* GRASSATTR */
            case 'Q': replacement = DAT_00481a28; break; /* TEXATTR */
            case 'S': replacement = DAT_00481a2b; break; /* SIGNATTR */
            case 'R': replacement = DAT_00481a2a; break; /* PICATTR */
            case 'U': replacement = DAT_00481a2d; break; /* GRASSATTR color */
            case 'V': replacement = DAT_00481a2c; break; /* TEXATTR color */
            case 'X': replacement = DAT_00481a2f; break; /* SIGNATTR color */
            case 'W': replacement = DAT_00481a2e; break; /* PICATTR color */
            default: replacement = val; break;
            }

            *tile = replacement;
        }
    }
}

/* ===== FUN_00418640: Water vertex grid cell processing ===== */
void FUN_00418640(int grid_x, int grid_y)
{
    int grid_idx = DAT_00480870 * grid_y + grid_x;
    char *noise = (char *)DAT_0048086c;

    int corner_tl = (int)noise[grid_idx];
    int corner_tr = (int)noise[grid_idx + 1];
    int corner_bl = (int)noise[grid_idx + DAT_00480870];
    int corner_br = (int)noise[grid_idx + DAT_00480870 + 1];

    /* Bilinear interpolation constants (fixed-point 18-bit) */
    int left_step = (corner_bl - corner_tl) << 18;
    int right_step = (corner_br - corner_tr) << 18;
    int left_val = corner_tl << 18;
    int right_val = corner_tr << 18;

    int py = grid_y * 16;
    int cell_w = 16;
    if (grid_x * 16 + 16 >= DAT_00487a0c) {
        cell_w = DAT_00487a0c - grid_x * 16 - 1;
    }

    int pixel_idx = DAT_00487a0c * py + grid_x * 16;

    for (int row = 0; row < 16 && py < DAT_00487a10; row++) {
        int interp = left_val;
        int horiz_step = ((right_val - left_val) + ((right_val - left_val) >> 31 & 0xF)) >> 4;

        for (int col = 0; col < cell_w; col++) {
            int brightness = interp >> 18;
            if (brightness < 0) brightness = 0;

            unsigned short src = *(unsigned short *)((char *)DAT_00489ea0 + pixel_idx * 2);

            /* Extract RGB565 components to 8-bit, apply brightness */
            int cr = ((src >> 11) & 0x1F) << 3;   /* R: 5-bit to 8-bit */
            int cg = ((src >> 5) & 0x3F) << 2;    /* G: 6-bit to 8-bit */
            int cb = (src & 0x1F) << 3;            /* B: 5-bit to 8-bit */

            int rv = cr * brightness;
            int gv = cg * brightness;
            int bv = cb * brightness;

            /* Repack to RGB565 (>> 10 scales 8-bit*brightness to 5-bit,
             * >> 9 scales to 6-bit for green channel) */
            int r5 = (rv >> 10) & 0x1F;
            int g6 = (gv >> 9) & 0x3F;
            int b5 = (bv >> 10) & 0x1F;
            unsigned short result = (unsigned short)((r5 << 11) | (g6 << 5) | b5);

            *(unsigned short *)((char *)DAT_00489ea0 + pixel_idx * 2) = result;

            pixel_idx++;
            interp += horiz_step;
        }

        left_val += ((left_step + (left_step >> 31 & 0xF)) >> 4);
        right_val += ((right_step + (right_step >> 31 & 0xF)) >> 4);
        pixel_idx += (DAT_00487a0c - cell_w);
        py++;
    }
}

/* ===== FUN_00417460: Assign tile colors from texture ===== */
void FUN_00417460(void)
{
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;

    for (int y = DAT_00480724; y < DAT_00480728; y++) {
        int *tile_info = (int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc);
        int tile_w = tile_info[0];
        int tile_h = tile_info[1];
        int tile_start = tile_info[2];

        int tex_row = (DAT_00480868 + y) % tile_h;
        int tex_col = (DAT_00480864 + DAT_0048071c) % tile_w;

        for (int x = DAT_0048071c; x < DAT_00480720; x++) {
            int map_offset = (y << shift) + x;
            unsigned char tile_val = *(unsigned char *)((int)DAT_0048782c + map_offset);

            /* Unwrap high-bit tiles */
            if (tile_val > 0x7F) tile_val += 0x80;

            char *type = (char *)((unsigned int)tile_val * 0x20 + (int)DAT_00487928);

            if (type[4] != '\0') {
                /* Water tile */
                *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) = DAT_0048384c;
            } else if (type[0] == '\0') {
                /* Empty/air tile - sample texture */
                unsigned short color;

                if (type[0x1a] == '\0') {
                    /* Normal texture */
                    color = *(unsigned short *)((char *)DAT_0048072c +
                             (tex_col + tex_row * tile_w + tile_start) * 2);
                } else {
                    /* Landscape texture */
                    int *land_info = (int *)((char *)DAT_00481b54 + DAT_0048089c * 0xc);
                    int lw = land_info[0];
                    int lh = land_info[1];
                    int ls = land_info[2];
                    color = *(unsigned short *)((char *)DAT_0048072c +
                             ((x % lw) + (y % lh) * lw + ls) * 2);
                }

                if (DAT_0048085c == 0) {
                    *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) = color;
                } else {
                    /* Apply darkness/bump mapping (RGB565) */
                    unsigned int cur = (unsigned int)*(unsigned short *)((char *)DAT_00481f50 + map_offset * 2);
                    int cr = ((color >> 11) & 0x1F) << 3;  /* R: 5-bit to 8-bit */
                    int cg = ((color >> 5) & 0x3F) << 2;   /* G: 6-bit to 8-bit */
                    int cb = (color & 0x1F) << 3;           /* B: 5-bit to 8-bit */

                    int rv = cr * cur;
                    int gv = cg * cur;
                    int bv = cb * cur;

                    /* Repack to RGB565 */
                    int r5 = (rv >> 10) & 0x1F;
                    int g6 = (gv >> 9) & 0x3F;
                    int b5 = (bv >> 10) & 0x1F;

                    unsigned short result = (unsigned short)((r5 << 11) | (g6 << 5) | b5);
                    *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) = result;

                    /* Ensure non-zero pixel */
                    if (*(short *)((char *)DAT_00481f50 + map_offset * 2) == 0) {
                        *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) = 0;
                    }
                }
            } else {
                /* Solid tile */
                if (type[0x1a] == '\0') {
                    *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) = 0;
                } else {
                    /* Random fire-like color (RGB565) */
                    int rr = rand() % 0x1E;
                    int rb = rand() % 0x1E + 0x40;
                    *(unsigned short *)((char *)DAT_00481f50 + map_offset * 2) =
                        (unsigned short)(((rr * 8 + 0x200) & 0xFFC0) + 0x8000 + (rb >> 3));
                }
            }

            tex_col++;
            if (tex_col >= *(int *)((char *)DAT_00481b54 + DAT_00480894 * 0xc)) {
                tex_col = 0;
            }
        }
    }
}

/* ===== FUN_00414c90: Background texture sampling and water vertex generation ===== */
void FUN_00414c90(void)
{
    if (DAT_00483724[3] != '\x01') return;

    /* Calculate swap dimensions from map size */
    DAT_00487a0c = (int)((float)DAT_004879f0);
    DAT_00487a10 = (int)((float)DAT_004879f4);

    /* Allocate swap buffer */
    DAT_00489ea0 = Mem_Alloc(DAT_00487a10 * DAT_00487a0c * 2);
    g_MemoryTracker += DAT_00487a10 * DAT_00487a0c * 2;

    /* Sample texture into swap buffer */
    int write_idx = 0;

    if (DAT_004808a8 == 0) {
        /* Use main tile texture */
        int tile_idx = DAT_00480894;
        int *tile_info = (int *)((char *)DAT_00481b54 + tile_idx * 0xc);
        int tw = tile_info[0];
        int th = tile_info[1];
        int ts = tile_info[2];

        for (int y = 0; y < DAT_00487a10; y++) {
            int tex_y = (DAT_00480868 + y * 2) % th;
            int tex_x = DAT_00480864 % tw;

            for (int x = 0; x < DAT_00487a0c; x++) {
                int src_idx = tex_x + tex_y * tw + ts;
                *(unsigned short *)((char *)DAT_00489ea0 + write_idx * 2) =
                    *(unsigned short *)((char *)DAT_0048072c + src_idx * 2);
                write_idx++;

                tex_x += 2;
                if (tex_x >= tw) tex_x -= tw;
            }
        }
    } else {
        /* Use alt tile texture (px1) */
        int tile_idx = DAT_004808a4;
        int *tile_info = (int *)((char *)DAT_00481b54 + tile_idx * 0xc);
        int tw = tile_info[0];
        int th = tile_info[1];
        int ts = tile_info[2];

        for (int y = 0; y < DAT_00487a10; y++) {
            int tex_x = 0;
            for (int x = 0; x < DAT_00487a0c; x++) {
                int src_idx = tex_x + (y % th) * tw + ts;
                *(unsigned short *)((char *)DAT_00489ea0 + write_idx * 2) =
                    *(unsigned short *)((char *)DAT_0048072c + src_idx * 2);
                write_idx++;

                tex_x++;
                if (tex_x >= tw) tex_x -= tw;
            }
        }
    }

    if (DAT_004808a8 != 0) {
        DAT_00483960 = 1;
        return;
    }

    /* Generate noise grid for water vertices */
    DAT_00480870 = (DAT_00487a0c + (DAT_00487a0c >> 31 & 0xF)) >> 4;
    DAT_00480874 = (DAT_00487a10 + (DAT_00487a10 >> 31 & 0xF)) >> 4;

    DAT_0048086c = Mem_Alloc((DAT_00480874 + 2) * (DAT_00480870 + 2));
    g_MemoryTracker += (DAT_00480874 + 2) * (DAT_00480870 + 2);

    /* Fill noise grid with random values */
    for (int y = 0; y < DAT_00480874; y++) {
        for (int x = 0; x < DAT_00480870; x++) {
            int val = (rand() & 0x3F) - 8;
            *(char *)((char *)DAT_0048086c + x + DAT_00480870 * y) = (char)val;
        }
    }

    /* Smooth noise grid */
    for (int y = 1; y < DAT_00480874 - 1; y++) {
        for (int x = 1; x < DAT_00480870 - 1; x++) {
            char *center = (char *)((char *)DAT_0048086c + x + DAT_00480870 * y);
            int avg = (int)*(char *)((char *)DAT_0048086c + x + DAT_00480870 * (y - 1)) +
                      (int)*(char *)((char *)DAT_0048086c + x + DAT_00480870 * (y + 1)) +
                      (int)center[-1] + (int)center[1];
            *center = (char)((avg + (avg >> 31 & 3)) >> 2);
        }
    }

    /* Apply noise to water vertices */
    for (int y = 0; y < DAT_00480874; y++) {
        for (int x = 0; x < DAT_00480870; x++) {
            FUN_00418640(x, y);
        }
    }

    Mem_Free(DAT_0048086c);
    DAT_00483960 = 1;
}

/* ===== Stub functions (Phase E: entity spawning, effects) ===== */

void FUN_00414bb0(int water_level) { /* Water plane fill - stub */ }
void FUN_00417140(void)   { /* Lighting/shadow pass - stub */ }
void FUN_00418a60(void)   { /* Decoration placement - stub */ }
void FUN_00418430(void)   { /* Terrain layer pass 1 - stub */ }
void FUN_00418420(void)   { /* Terrain layer pass 2 - stub */ }
void FUN_00419d80(void)   { /* Portal placement - stub */ }
void FUN_00419f60(void)   { /* Creature spawning (regular) - stub */ }
void FUN_00419c90(void)   { /* Creature spawning (timed) - stub */ }
void FUN_00419fb0(void)   { /* Pickup spawning (regular) - stub */ }
void FUN_00419d00(void)   { /* Pickup spawning (timed) - stub */ }
void FUN_00419860(void)   { /* Treasure spawning - stub */ }

/* ===== FUN_004143e0: Main GG generator ===== */
/* Returns 1 on success, 0 on failure */
int FUN_004143e0(int width, int height)
{
    if (DAT_00486484 == 0) {
        sprintf(DAT_00489d7c, "GG Error : No GG themes!");
        return 0;
    }

    DAT_004808e1 = (width == 0) ? 1 : 0;

    /* Search for theme matching DAT_0048396e */
    int theme_idx = -1;
    for (int i = 0; i < DAT_00486484; i++) {
        if (_stricmp((const char *)DAT_00486488[i], DAT_0048396e) == 0) {
            theme_idx = i;
            break;
        }
    }

    if (theme_idx < 0) {
        /* No match - pick random theme */
        theme_idx = rand() % DAT_00486484;
    }

    /* Copy selected theme name to DAT_00480740 */
    strcpy(DAT_00480740, (const char *)DAT_00486488[theme_idx]);

    /* Parse theme info.txt */
    FUN_00415a60();
    DAT_0048085c = (unsigned int)(unsigned char)DAT_00481a41;

    /* Set up map dimensions (unless simple mode) */
    if (DAT_004808e1 == '\0') {
        if (DAT_00481a34 != 0) width = DAT_00481a34;
        if (DAT_00481a38 != 0) height = DAT_00481a38;

        DAT_004879f0 = width + 0xe;
        DAT_004879f4 = height + 0xe;

        if (DAT_004879f0 > 0x1e78) DAT_004879f0 = 0x1e78;
        if (DAT_004879f4 > 0x1e78) DAT_004879f4 = 0x1e78;

        FUN_00416ad0();
    }

    /* Set map boundaries */
    DAT_0048071c = 7;
    DAT_00480724 = 7;
    DAT_00480720 = DAT_004879f0 - 7;
    DAT_00480728 = DAT_004879f4 - 7;

    /* Calculate area and generation parameters */
    DAT_00480848 = (DAT_004879f4 * DAT_004879f0) / 2;
    DAT_00480844 = 0;
    DAT_00480860 = 0x80 - (int)DAT_0048085c / 2;
    DAT_0048084c = 2;
    DAT_00480850 = 1;
    DAT_00480854 = 1;
    DAT_00480858 = 1;

    /* Load tile graphics */
    int result = FUN_00416320();
    if (result == 0) {
        if (DAT_004808d0 == 3)
            sprintf(DAT_00489d7c, "GG Error : Wrong type JPG or not found in \"%s\"", DAT_00480740);
        if (DAT_004808d0 == 1)
            sprintf(DAT_00489d7c, "GG Error : Wrong type TGA in \"%s\"", DAT_00480740);
        if (DAT_004808d0 == 2)
            sprintf(DAT_00489d7c, "GG Error : Could not find or access important TGA in \"%s\"", DAT_00480740);
        if (DAT_004808d0 == 4)
            sprintf(DAT_00489d7c, "GG Error : Graphics take too much memory in \"%s\"", DAT_00480740);
        return 0;
    }

    /* Generate terrain */
    if (DAT_004808e1 == '\0') {
        /* Full generation mode */
        if (DAT_00481a40 == '\0') {
            FUN_00417ea0();  /* Standard cave generation */
        } else {
            FUN_00418520();  /* Beach-style generation */
        }
    } else {
        /* Simple mode: minimal generation */
        if (DAT_00480898 > 1 && DAT_004839ee != '\0') {
            FUN_00418430();
        }
        FUN_00419d80();
    }

    /* Extra terrain layers */
    if ((DAT_004808e1 == '\0' || (DAT_00480898 > 2 && DAT_004839ee != '\0')) &&
        DAT_00480898 > 2) {
        FUN_00418420();
    }

    /* Water fill */
    if ((char)(DAT_00481a3c & 0xFF) != '\0') {
        FUN_00414bb0(DAT_00481a3c & 0xFF);
    }

    /* Lighting pass */
    if (DAT_00480854 != 0) {
        FUN_00417140();
    }

    /* Assign tile colors from textures */
    FUN_00417460();

    /* Decoration placement */
    if (DAT_004808c0 != 0) {
        FUN_00418a60();
    }

    /* Entity spawning: creatures */
    if (DAT_004808b8 != 0) {
        if (DAT_004839ef == 0) {
            DAT_00480878 = 4;
            FUN_00419f60();
        } else {
            DAT_00480878 = (unsigned int)(unsigned char)DAT_004839ef * 0x3C;
            FUN_00419c90();
        }
    }

    /* Entity spawning: pickups */
    if (DAT_004808c8 != 0) {
        unsigned char pickup_density = (unsigned char)(DAT_004839f0 >> 8);
        if (pickup_density == 0) {
            DAT_0048087c = 4;
            FUN_00419fb0();
        } else {
            DAT_0048087c = (unsigned int)pickup_density * 0x28;
            FUN_00419d00();
        }
    }

    /* Entity spawning: treasures */
    if (DAT_004808b0 != 0 && (char)DAT_004839f0 != '\0') {
        DAT_00480880 = (int)(0x5000 / (long long)(int)(DAT_004839f0 & 0xFF));
        FUN_00419860();
    }

    /* Post-processing: flip high-bit tiles back */
    unsigned char shift = (unsigned char)DAT_00487a18 & 0x1F;
    for (int y = DAT_00480724; y < DAT_00480728; y++) {
        for (int x = DAT_0048071c; x < DAT_00480720; x++) {
            unsigned char *tile = (unsigned char *)((int)DAT_0048782c + (y << shift) + x);
            if (*tile > 0x7F) {
                *tile += 0x80;
            }
        }
    }

    /* Tile code remapping */
    FUN_00414b00();

    /* Background texture and water vertex generation */
    FUN_00414c90();

    /* Free temporary work buffers */
    Mem_Free(DAT_0048072c);
    Mem_Free(DAT_00481b54);
    Mem_Free(DAT_00480738);
    Mem_Free(DAT_00480734);
    Mem_Free(DAT_00480730);
    Mem_Free(DAT_004818e4);

    return 1;
}
