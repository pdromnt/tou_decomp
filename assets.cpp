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

/* ===== Parse_Font_Data ===== */
static void Parse_Font_Data(int font_idx, unsigned char *img_data, int w, int h)
{
    int current_char = 33;
    bool in_char = false;
    int start_x = 0;

    for (int i = 0; i < 33; i++) {
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
            int char_w = x - start_x;
            int table_idx = font_idx * 256 + current_char;
            if (table_idx >= 1024) break;

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
            current_char++;
            if (current_char > 255) break;
        }
    }

    Font_Char_Table[font_idx * 256 + 32].width = 5;
    Font_Char_Table[font_idx * 256 + 32].height = h;
}

/* ===== Load_Fonts ===== */
void Load_Fonts(void)
{
    if (Font_Pixel_Data) return;

    Font_Pixel_Data = (unsigned char *)Mem_Alloc(1024 * 1024);
    Font_Pixel_Offset = 0;

    const char *font_files[] = {
        "data/f_tiny5d.tga",
        "data/f_mini.tga",
        "data/f_med.tga",
        "data/f_large.tga"
    };

    memset(Font_Char_Table, 0, sizeof(Font_Char_Table));

    for (int f = 0; f < 4; f++) {
        int w, h, c;
        unsigned char *data = stbi_load(font_files[f], &w, &h, &c, 3);
        if (!data) continue;
        Parse_Font_Data(f, data, w, h);
        stbi_image_free(data);
    }
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
