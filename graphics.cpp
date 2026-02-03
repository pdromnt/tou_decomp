#include "tou.h"
#include <ddraw.h>

// Globals
LPDIRECTDRAW7 lpDD = NULL;                 // DAT_00489ec8 (Updated to DD7)
LPDIRECTDRAWSURFACE7 lpDDS_Primary = NULL; // DAT_00489ed8
LPDIRECTDRAWSURFACE7 lpDDS_Back = NULL;    // DAT_00489ed0
// DAT_00489ecc ?

// Software Backbuffer (Simulated)
// 640x480 * 2 bytes (16-bit) = 614,400 bytes
unsigned short *Software_Buffer = NULL; // DAT_004877c0

// Init_DirectDraw (004610e0)
int Init_DirectDraw(int width, int height) {
  if (Software_Buffer == NULL) {
    Software_Buffer = (unsigned short *)malloc(width * height * 2);
  }

  // Stub for now - just returning success to allow logic flow
  // In real implementation:
  // DirectDrawCreateEx(NULL, (void**)&lpDD, IID_IDirectDraw7, NULL);
  // lpDD->SetCooperativeLevel(hWnd_Main, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
  // lpDD->SetDisplayMode(width, height, 16, 0, 0);
  // Creates Primary + BackBuffer (1)

  return 1;
}

// Release Surfaces (004610a0)
void Release_DirectDraw_Surfaces(void) {
  if (lpDDS_Back) {
    lpDDS_Back->Release();
    lpDDS_Back = NULL;
  }
  if (lpDDS_Primary) {
    lpDDS_Primary->Release();
    lpDDS_Primary = NULL;
  }
  // lpDD is released in WinMain usually, or here?
  // Original code checks DAT_00489ec8 (lpDD) but releases 9ed8/9ed0.
}

// Restore Surfaces (00461070) - Stub
int Restore_Surfaces(void) {
  if (lpDDS_Primary && lpDDS_Primary->IsLost() == DDERR_SURFACELOST) {
    return lpDDS_Primary->Restore();
  }
  return 0;
}

// Render Game View (0042f3a0)
// Implements software rendering to the buffer before Blit
extern void *DAT_004877c0; // Software Buffer Pointer (16-bit pixels?)
// Decomp suggests it processes entities and draws them.

void Render_Game_View(void) {
  if (Software_Buffer == NULL)
    return;

  // Clear Buffer (Simulated)
  // memset(Software_Buffer, 0, 640*480*2);

  // Draw Test Pattern (Gradient)
  WORD *pixel_buffer = (WORD *)Software_Buffer;

  // Simple gradient to verify rendering
  // 640x480
  /*
  for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 640; x++) {
           pixel_buffer[y * 640 + x] = (WORD)((x & 0x1F) | ((y & 0x1F) << 5));
      }
  }
  */
  // Test Draw (Sprite 0 at 0,0) - Example usage
  // Render_Game_View iterates entities and calls Draw.
  // We haven't implemented Entity system yet, so this is just a test hook.
  // Draw_Sprite_To_Buffer(0, (WORD*)Software_Buffer, 640, 0, 0);
}

// Draw Text To Buffer (0040aed0)
void Draw_Text_To_Buffer(char *text, int font_idx, int y_offset_base,
                         unsigned short *dest_buffer, int pitch,
                         int dest_pitch_pixels, int max_width, int param_8) {
  if (text == NULL)
    return;

  // Globals access
  // Font_Char_Table (int array). But decomp suggests it's structs of size 0x10.
  // 0x10 = 16 bytes = 4 ints.
  // Index = font_idx * 256 + char_code.
  // Struct: [0]: Width, [1]: Height/Offset?, [2]: Data Offset?, [3]: ?
  // Let's assume Font_Char_Table is flattened int array.
  // Each char has 4 ints.

  int current_x = 0;
  int current_y = 0;

  // Decomp logic loop
  for (int i = 0; text[i] != '\0'; i++) {
    unsigned char c = (unsigned char)text[i];

    if (c == 0x20) {   // Space
      current_x += 10; // Hardcoded or from table? Decomp used separate var.
      continue;
    }
    if (c == 0x0A) {   // Newline
      current_y += 16; // Hardcoded newline height?
      current_x = 0;
      continue;
    }

    // Lookup
    int char_idx = (font_idx * 256 + c) * 4; // 4 ints per char
    int width = Font_Char_Table[char_idx];
    int height = Font_Char_Table[char_idx + 1];
    int data_offset = Font_Char_Table[char_idx + 2];

    // Draw
    if (width > 0 && height > 0) {
      unsigned char *pixels = &Font_Pixel_Data[data_offset];
      // Render logic...
      /*
      for (int py=0; py<height; py++) {
          for (int px=0; px<width; px++) {
              unsigned char p = pixels[py*width + px];
              if (p != 0) {
                   // Draw to dest_buffer
                   // dest_buffer[(current_y + py) * pitch + (current_x + px)] =
      ColorMap[p];
              }
          }
      }
      */
      current_x += width;
    }
  }
}

// Draw Sprite To Buffer
void Draw_Sprite_To_Buffer(int sprite_idx, unsigned short *dest_buffer,
                           int pitch, int flags, int palette_idx) {
  if (sprite_idx < 0 || sprite_idx >= 20000)
    return;

  // Dimensions
  int width = (int)Sprite_Widths[sprite_idx];
  int height = (int)Sprite_Heights[sprite_idx];
  if (width == 0 || height == 0)
    return;

  // Source Data Helper
  int offset = Sprite_Offsets[sprite_idx];
  unsigned short *src = &Sprite_Data[offset];

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      unsigned short pixel = *src;
      src++;

      if (pixel != 0) {
        dest_buffer[x] = pixel;
      }
    }
    dest_buffer += pitch;
  }
}

// Render Frame (0045d800)
// Copies Software Buffer to BackBuffer, then Flips
void Render_Frame(void) {
  DDSURFACEDESC2 ddsd;
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);

  // 1. Lock BackBuffer
  // if (lpDDS_Back->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK) return;

  // 2. Copy Pixels
  // Software_Buffer -> ddsd.lpSurface
  // Handle Pitch!
  /*
  unsigned short* src = Software_Buffer;
  unsigned char* dst = (unsigned char*)ddsd.lpSurface;
  for (int y=0; y<480; y++) {
      memcpy(dst, src, 640*2);
      src += 640;
      dst += ddsd.lPitch;
  }
  */

  // 3. Unlock
  // lpDDS_Back->Unlock(NULL);

  // 4. Flip
  // lpDDS_Primary->Flip(NULL, DDFLIP_WAIT);
}

// Load Background to Buffer (0042d710)
void Load_Background_To_Buffer(char index) {
  // Uses Load_JPEG_Asset to load background image #index.
  // Then likely converts it to 565 format and stores in Software_Buffer.
  // printf("DEBUG: Load Header Logic Here\n");
}
