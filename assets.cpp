#include "tou.h"
#include <stdio.h>
#include <string.h>

// STB Image - single header JPEG loader (replaces IJL)
#define STBI_ONLY_JPEG
#include "stb_image.h"

// Global Variables
int DAT_00481d08 = 0; // Width
int DAT_00481cfc = 0; // Height
int DAT_00481d04 = 0; // Channels
int DAT_00481d00 = 0; // Buffer Size

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
