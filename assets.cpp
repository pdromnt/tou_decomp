#include "ijl_mock.h"
#include "tou.h"
#include <stdio.h>
#include <string.h>

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
  } else {
    return 0;
  }

  if (index == DAT_0048769c)
    return 1;

  LOG("[INFO] Loading Background: %s\n", filename);

  // === BYPASS IJL - Use fake solid color image ===
  // IJL is crashing due to structure size mismatch with old DLL
  // TODO: Replace with stb_image or fix IJL structure

  DAT_00481d08 = 640;
  DAT_00481cfc = 480;
  DAT_00481d04 = 3;
  DAT_00481d00 = 640 * 480 * 3;

  void *buffer = Mem_Alloc(DAT_00481d00);
  if (!buffer) {
    LOG("[ERROR] Failed to allocate buffer for image\n");
    return 0;
  }

  // Fill with a gradient pattern for visual feedback
  unsigned char *pixels = (unsigned char *)buffer;
  for (int y = 0; y < 480; y++) {
    for (int x = 0; x < 640; x++) {
      int idx = (y * 640 + x) * 3;
      pixels[idx + 0] = (unsigned char)(x * 255 / 640); // R
      pixels[idx + 1] = (unsigned char)(y * 255 / 480); // G
      pixels[idx + 2] = 128;                            // B
    }
  }
  LOG("[DEBUG] Fake image loaded (gradient pattern)\n");

  // Convert to BGR 565 (BBBBB GGGGGG RRRRR)
  unsigned char *src = (unsigned char *)buffer;
  unsigned short *dst = DAT_004877c0;

  if (dst) {
    int total_pixels = DAT_00481d08 * DAT_00481cfc;
    for (int i = 0; i < total_pixels; i++) {
      unsigned char r = src[i * 3 + 0];
      unsigned char g = src[i * 3 + 1];
      unsigned char b = src[i * 3 + 2];
      unsigned short res = (r >> 3) | ((g & 0xFC) << 3) | ((b & 0xF8) << 8);
      dst[i] = res;
    }
  }

  Mem_Free(buffer);
  DAT_0048769c = index;
  LOG("[DEBUG] Background Load Success (fake).\n");
  return 1;
}

// Load_JPEG_Asset (00420c90) - generic loader
void *Load_JPEG_Asset(const char *filename, int *width, int *height) {
  // Similar to above but returns buffer
  // ...
  return NULL;
}
