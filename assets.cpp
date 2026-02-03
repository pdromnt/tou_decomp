#include "tou.h"
#include <stdio.h>
#include <string.h>

// STB Image - single header Image loader (replaces IJL)
// #define STBI_ONLY_JPEG // Disabled to allow TGA loading
#include "stb_image.h"

// Global Variables
int DAT_00481d08 = 0; // Width
int DAT_00481cfc = 0; // Height
int DAT_00481d04 = 0; // Channels
int DAT_00481d00 = 0; // Buffer Size

// Font Globals
FontChar Font_Char_Table[1024];
unsigned char *Font_Pixel_Data = NULL;
int Font_Pixel_Offset = 0;

void Parse_Font_Data(int font_idx, unsigned char *img_data, int w, int h) {
  // Scan image for characters (ASCII 33-127 usually, start at 33 '!')
  // Assumes characters are separated by empty (black) vertical columns.

  int current_char = 33; // Start at '!'
  bool in_char = false;
  int start_x = 0;

  // Fill first 33 chars as empty/dummy
  for (int i = 0; i < 33; i++) {
    Font_Char_Table[font_idx * 256 + i].pixel_offset = 0;
    Font_Char_Table[font_idx * 256 + i].width = 0;
    Font_Char_Table[font_idx * 256 + i].height = h;
  }

  for (int x = 0; x < w; x++) {
    // Check if column is empty
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
      // End of character
      in_char = false;
      int char_w = x - start_x;

      // Store Metadata
      int table_idx = font_idx * 256 + current_char;
      if (table_idx >= 1024)
        break;

      Font_Char_Table[table_idx].width = char_w;
      Font_Char_Table[table_idx].height = h;
      Font_Char_Table[table_idx].pixel_offset = Font_Pixel_Offset;

      // Copy Pixels to Global Buffer (Pack them)
      // Store Luminance (Grayscale).
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
      if (current_char > 255)
        break;
    }
  }

  // Set Space Width (Approx 5 pixels?)
  Font_Char_Table[font_idx * 256 + 32].width = 5;
  Font_Char_Table[font_idx * 256 + 32].height = h;

  LOG("[INFO] Parsed Font %d: Found %d chars.\n", font_idx, current_char - 33);
}

void Load_Fonts(void) {
  if (Font_Pixel_Data)
    return; // Already loaded

  // Allocate Pixel Data (Guestimate size: 1MB)
  Font_Pixel_Data = (unsigned char *)malloc(1024 * 1024); // 1MB should suffice
  Font_Pixel_Offset = 0;

  const char *font_files[] = {
      "data/f_tiny5d.tga", // 0
      "data/f_mini.tga",   // 1
      "data/f_med.tga",    // 2
      "data/f_large.tga"   // 3
  };

  memset(Font_Char_Table, 0, sizeof(Font_Char_Table));

  for (int f = 0; f < 4; f++) {
    int w, h, c;
    unsigned char *data = stbi_load(font_files[f], &w, &h, &c, 3);
    if (!data) {
      LOG("[ERROR] Failed to load font: %s (Reason: %s)\n", font_files[f],
          stbi_failure_reason());
      continue;
    }

    LOG("[INFO] Loaded Font %d: %s (%dx%d)\n", f, font_files[f], w, h);

    // Scan and Pack
    Parse_Font_Data(f, data, w, h);

    stbi_image_free(data);
  }
}

// Implementation of 0042d710
int Load_Background_To_Buffer(char index) {
  // Filenames from 0x47dabc
  const char *filenames[] = {
      "data//menu3d.jpg", // index 0
      "data//sanim2.jpg", // index 1
      "data//splay.jpg"   // index 2
  };

  const char *filename;
  if (index >= 0 && index < 3) {
    filename = filenames[(int)index];
  } else if (index == 11) {
    // explode.gfx is a custom binary format, not JPEG.
    // Stb_image cannot load it. We skip it for now to avoid errors.
    LOG("[WARNING] explode.gfx (Index 11) format not supported yet.\n");
    return 0;
  } else {
    return 0;
  }

  if (index == DAT_0048769c)
    return 1;

  LOG("[INFO] Loading Background: %s\n", filename);

  // Load JPEG using stb_image
  int width, height, channels;
  unsigned char *img_data = stbi_load(filename, &width, &height, &channels, 3);

  if (!img_data) {
    LOG("[ERROR] stbi_load failed for %s: %s\n", filename,
        stbi_failure_reason());
    return 0;
  }

  LOG("[DEBUG] Loaded %s: %dx%d, %d channels\n", filename, width, height,
      channels);

  DAT_00481d08 = width;
  DAT_00481cfc = height;
  DAT_00481d04 = 3;
  DAT_00481d00 = width * height * 3;

  // Convert RGB to BGR 565 (BBBBB GGGGGG RRRRR)
  unsigned char *src = img_data;
  unsigned short *dst = DAT_004877c0;

  if (dst) {
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; i++) {
      unsigned char r = src[i * 3 + 0];
      unsigned char g = src[i * 3 + 1];
      unsigned char b = src[i * 3 + 2];
      // RGB565: RRRRR(11-15) GGGGGG(5-10) BBBBB(0-4)
      unsigned short res =
          ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
      dst[i] = res;
    }
  }

  stbi_image_free(img_data);
  DAT_0048769c = index;
  LOG("[DEBUG] Background Load Success.\n");
  return 1;
}

// Load_JPEG_Asset (00420c90) - generic loader
void *Load_JPEG_Asset(const char *filename, int *out_width, int *out_height) {
  int width, height, channels;
  unsigned char *img_data = stbi_load(filename, &width, &height, &channels, 3);

  if (!img_data) {
    LOG("[ERROR] Load_JPEG_Asset failed for %s: %s\n", filename,
        stbi_failure_reason());
    return NULL;
  }

  if (out_width)
    *out_width = width;
  if (out_height)
    *out_height = height;

  return img_data;
}
