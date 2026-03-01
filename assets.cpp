/*
 * assets.cpp - Image loading, font system, background loader
 * Addresses: Load_Background_To_Buffer=0042D710, Load_JPEG_Asset=00420C90
 */
#include "tou.h"
#include "stb_image.h"
#include <stdio.h>
#include <string.h>

/* ===== Globals defined in this module ===== */
int g_ImageWidth   = 0;    /* 00481D08 */
int g_ImageHeight  = 0;    /* 00481CFC */
int g_ImageBPP     = 0;    /* 00481D04 */
int g_ImageSize    = 0;    /* 00481D00 */

FontChar Font_Char_Table[1024];
unsigned char *Font_Pixel_Data = NULL;
static int Font_Pixel_Offset = 0;

/* Character ordering in font TGA files (from binary at 0047BE18).
 * The TGA glyphs are laid out left-to-right in this order (95 chars): */
static const unsigned char Font_Char_Map[95] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z', 0xC5, 0xC4, 0xD6,
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x','y','z', 0xE5, 0xE4, 0xF6,
    '0','1','2','3','4','5','6','7','8','9',
    '.','!','?','+','-',':',';','(',')','/','\\','$',
    '%','&','\'','<','>', 0xBD, '"','#','[',']','_',',','=','*','@'
};

/* Default space widths per font index (from binary FUN_00422a10) */
static const int Font_Space_Width[4] = { 16, 12, 14, 5 };

/* ===== Parse_Font_Data (from FUN_00422a10) ===== */
static void Parse_Font_Data(int font_idx, unsigned char *img_data, int w, int h)
{
    int glyph_index = 0;
    bool in_char = false;
    int start_x = 0;

    /* Initialize all 256 entries with zero width (unmapped) */
    for (int i = 0; i < 256; i++) {
        Font_Char_Table[font_idx * 256 + i].pixel_offset = 0;
        Font_Char_Table[font_idx * 256 + i].width = 0;
        Font_Char_Table[font_idx * 256 + i].height = h;
    }

    for (int x = 0; x < w; x++) {
        bool empty = true;
        for (int y = 0; y < h; y++) {
            int r = img_data[(y * w + x) * 3 + 0];
            int g = img_data[(y * w + x) * 3 + 1];
            int b = img_data[(y * w + x) * 3 + 2];
            if (r > 0 || g > 0 || b > 0) {
                empty = false;
                break;
            }
        }

        if (!empty && !in_char) {
            in_char = true;
            start_x = x;
        } else if (empty && in_char) {
            in_char = false;
            if (glyph_index >= 95) break;

            int char_w = x - start_x;

            /* Map glyph position to ASCII code using the character table */
            unsigned char ascii_char = Font_Char_Map[glyph_index];
            int table_idx = font_idx * 256 + ascii_char;

            Font_Char_Table[table_idx].width = char_w;
            Font_Char_Table[table_idx].height = h;
            Font_Char_Table[table_idx].pixel_offset = Font_Pixel_Offset;

            for (int cy = 0; cy < h; cy++) {
                for (int cx = 0; cx < char_w; cx++) {
                    int src_x = start_x + cx;
                    int idx = (cy * w + src_x) * 3;
                    unsigned char r = img_data[idx];
                    unsigned char g = img_data[idx + 1];
                    unsigned char b = img_data[idx + 2];
                    unsigned char lum = (r + g + b) / 3;
                    Font_Pixel_Data[Font_Pixel_Offset + (cy * char_w) + cx] = lum;
                }
            }

            Font_Pixel_Offset += (char_w * h);
            glyph_index++;
        }
    }

    /* Set space character width from per-font defaults */
    Font_Char_Table[font_idx * 256 + 32].width = Font_Space_Width[font_idx];
    Font_Char_Table[font_idx * 256 + 32].height = h;
}

/* ===== Load_Fonts (FUN_00422a10) ===== */
void Load_Fonts(void)
{
    if (Font_Pixel_Data) return;

    Font_Pixel_Data = (unsigned char *)Mem_Alloc(1024 * 1024);
    Font_Pixel_Offset = 0;

    /* Original font order: 0=large, 1=mini, 2=med, 3=tiny */
    const char *font_files[] = {
        "data/f_large.tga",     /* font 0: large (default w=16, h=20) */
        "data/f_mini.tga",      /* font 1: mini  (default w=12, h=12) */
        "data/f_med.tga",       /* font 2: med   (default w=14, h=16) */
        "data/f_tiny5d.tga"     /* font 3: tiny  (default w=5,  h=11) */
    };

    memset(Font_Char_Table, 0, sizeof(Font_Char_Table));

    int loaded = 0;
    for (int f = 0; f < 4; f++) {
        int w, h, c;
        unsigned char *data = stbi_load(font_files[f], &w, &h, &c, 3);
        if (!data) {
            LOG("[FONT] Failed to load %s\n", font_files[f]);
            continue;
        }
        LOG("[FONT] Font %d: Loaded %s (%dx%d)\n", f, font_files[f], w, h);
        Parse_Font_Data(f, data, w, h);
        stbi_image_free(data);
        loaded++;
    }
    LOG("[FONT] %d fonts loaded, %d bytes of pixel data\n", loaded, Font_Pixel_Offset);
}

/* ===== Load_Background_To_Buffer (0042D710) ===== */
int Load_Background_To_Buffer(char index)
{
    const char *filenames[] = {
        "data//menu3d.jpg",
        "data//sanim2.jpg",
        "data//splay.jpg"
    };

    const char *filename;
    if (index >= 0 && index < 3) {
        filename = filenames[(int)index];
    } else if (index == 11) {
        return 0;
    } else {
        return 0;
    }

    if (index == g_LoadedBgIndex)
        return 1;

    int width, height, channels;
    unsigned char *img_data = stbi_load(filename, &width, &height, &channels, 3);
    if (!img_data) {
        return 0;
    }

    g_ImageWidth  = width;
    g_ImageHeight = height;
    g_ImageBPP    = 3;
    g_ImageSize   = width * height * 3;

    /* Convert RGB24 to RGB565 into Software_Buffer */
    unsigned char *src = img_data;
    unsigned short *dst = Software_Buffer;

    if (dst) {
        int total_pixels = width * height;
        for (int i = 0; i < total_pixels; i++) {
            unsigned char r = src[i * 3 + 0];
            unsigned char g = src[i * 3 + 1];
            unsigned char b = src[i * 3 + 2];
            unsigned short res = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
            dst[i] = res;
        }
    }

    stbi_image_free(img_data);
    g_LoadedBgIndex = index;
    return 1;
}

/* ===== Load_JPEG_Asset (00420C90) ===== */
void *Load_JPEG_Asset(const char *filename, int *out_width, int *out_height)
{
    int width, height, channels;
    unsigned char *img_data = stbi_load(filename, &width, &height, &channels, 3);
    if (!img_data) return NULL;
    if (out_width)  *out_width  = width;
    if (out_height) *out_height = height;
    return img_data;
}

/* ===== Load_JPEG_From_Memory ===== */
/* Decodes JPEG/PNG data from a memory buffer using stb_image.
 * Returns RGB24 pixel data (caller must stbi_image_free or Mem_Free).
 * Sets out_w/out_h to image dimensions. Returns NULL on failure. */
void *Load_JPEG_From_Memory(const unsigned char *data, int len, int *out_w, int *out_h)
{
    int width, height, channels;
    unsigned char *img_data = stbi_load_from_memory(data, len, &width, &height, &channels, 3);
    if (!img_data) return NULL;
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return img_data;
}
