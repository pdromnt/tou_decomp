#include "tou.h"
#include <malloc.h>
#include <stdio.h>

// IJL Globals or Imports
// Stubs for IJL (Intel JPEG Library) 1.5
// Defined in ijl_mock.h usually, but we need implementation for linker
// Note: JPEG_CORE_PROPERTIES is defined in ijl_mock.h included via tou.h

extern "C" {
int __stdcall ijlInit(JPEG_CORE_PROPERTIES *jcprops) { return 0; } // 0 = IJL_OK
int __stdcall ijlFree(JPEG_CORE_PROPERTIES *jcprops) { return 0; }
int __stdcall ijlRead(JPEG_CORE_PROPERTIES *jcprops, int ioType) { return 0; }
}

// Global Variables
// Assuming we link ijl15.lib or ijl10.lib

// Global Data Pointers (set by Load_JPEG_Asset or caller?)
// In FUN_00421540: DAT_00481cfc (Height), DAT_00481d08 (Width)
// These seem to be updated by Load_JPEG_Asset via stack pointers or globals?
// But decompilation suggested Load_JPEG_Asset returns a pointer.
// And FUN_00421540 updates DAT_00481d08 AFTER the call.
// So Load_JPEG_Asset might just return the pixel data and size info via other
// means? Actually, decompilation of Load_JPEG_Asset: DAT_00481d08 =
// in_stack_0000002c; (Width from struct) DAT_00481cfc = in_stack_00000030;
// (Height from struct) So it updates global Width/Height variables directly!

// Globals used by Asset Loader
int DAT_00481d08 = 0; // Width
int DAT_00481cfc = 0; // Height
int DAT_00481d04 = 0; // Channels?
int DAT_00481d00 = 0; // Size?
// int DAT_004892a0; // Extern in tou.h

// Implementation
unsigned char *Load_JPEG_Asset(int data_size, void *data_ptr, int mode) {
  JPEG_CORE_PROPERTIES jcprops;
  int res;
  unsigned char *buffer;

  // 1. Init IJL
  res = ijlInit(&jcprops);
  if (res != 0) {
    ijlFree(&jcprops);
    return NULL;
  }

  // 2. Setup Read Params
  jcprops.JPGBytes = (unsigned int)data_ptr;
  jcprops.JPGSizeBytes = data_size;
  jcprops.UseExternal = 0; // Or flags?

  // 3. Read Header
  res = ijlRead(&jcprops, IJL_JBUFF_READPARAMS);
  if (res != 0) {
    ijlFree(&jcprops);
    return NULL;
  }

  // 4. Update Globals
  DAT_00481d08 = jcprops.JPGWidth;
  DAT_00481cfc = jcprops.JPGHeight;
  DAT_00481d04 = 3; // Default to 3 channels?

  // 5. Calculate Size and Allocate
  // Logic from decompilation:
  // ... FUN_00425820(2, iVar1) ... alignment?
  // DAT_00481d00 = Width * Height * 3;

  int channels = 3;
  if (mode == 1)
    channels = 4;
  // mode == 3 -> channels = 3?

  DAT_00481d00 = jcprops.JPGWidth * jcprops.JPGHeight * channels;

  buffer = (unsigned char *)malloc(DAT_00481d00);
  // DAT_004892a0 += DAT_00481d00; // Track memory

  if (buffer == NULL) {
    ijlFree(&jcprops);
    return NULL;
  }

  // 6. Setup DIB Params
  jcprops.DIBBytes = buffer;
  jcprops.DIBWidth = jcprops.JPGWidth;
  jcprops.DIBHeight = jcprops.JPGHeight;
  jcprops.DIBChannels = channels;
  // jcprops.DIBColor = IJL_BGR; // Standard Windows

  // 7. Read Image
  res = ijlRead(&jcprops, IJL_JBUFF_READWHOLEIMAGE);
  if (res != 0) {
    free(buffer);
    ijlFree(&jcprops);
    return NULL;
  }

  // 8. Cleanup
  ijlFree(&jcprops);

  // Return pointer to RGB data
  // Decompilation returns: ~-(uint)(iVar1 != 0) & (uint)pvVar2;
  // Means if success, return pointer.
  return buffer;
}

// Sprite Tables Definition
int Sprite_Offsets[20000];           // DAT_00489234
unsigned char Sprite_Widths[20000];  // DAT_00489e8c
unsigned char Sprite_Heights[20000]; // DAT_00489e88
unsigned short
    Sprite_Data[0x200000]; // DAT_00487ab4 - 4MB Buffer for sprite data

// Font Tables Definition
int Font_Char_Table[0x2000];             // DAT_00483c84
unsigned char Font_Pixel_Data[0x100000]; // DAT_00487878
