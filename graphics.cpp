#include "tou.h"
#include <stdio.h>
#include <stdlib.h>

// Globals - DirectDraw v1
LPDIRECTDRAW lpDD = NULL;                   // DAT_00489ec8
LPDIRECTDRAWSURFACE lpDDS_Primary = NULL;   // DAT_00489ed8
LPDIRECTDRAWSURFACE lpDDS_Back = NULL;      // DAT_00489ecc (Attached)
LPDIRECTDRAWSURFACE lpDDS_Offscreen = NULL; // DAT_00489ed0 (Staging)

// Init_DirectDraw (004610e0)
int Init_DirectDraw(int width, int height) {
  HRESULT hr;

  if (lpDD == NULL) {
    LOG("[ERROR] Init_DirectDraw: lpDD is NULL\n");
    return 0;
  }

  // 1. Set Display Mode
  LOG("[INFO] Setting Display Mode to %dx%dx16...\n", width, height);
  hr = lpDD->SetDisplayMode(width, height, 16);
  if (FAILED(hr)) {
    LOG("[WARNING] SetDisplayMode failed: 0x%08X\n", (unsigned int)hr);
    // Continue anyway, might be in a windowed cooperative level
  }

  // 2. Create Primary Surface
  DDSURFACEDESC ddsd;
  memset(&ddsd, 0, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);

  // Try to create Primary with backbuffer (Exclusive/Fullscreen)
  ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
  ddsd.dwBackBufferCount = 1;

  LOG("[INFO] Creating Primary Surface...\n");
  hr = lpDD->CreateSurface(&ddsd, &lpDDS_Primary, NULL);
  if (FAILED(hr)) {
    LOG("[INFO] Complex Primary Surface failed: 0x%08X. Trying Simple...\n",
        (unsigned int)hr);

    // Fallback: Simple Primary Surface for windowed mode
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hr = lpDD->CreateSurface(&ddsd, &lpDDS_Primary, NULL);

    if (FAILED(hr)) {
      LOG("[ERROR] Primary Surface creation failed completely: 0x%08X\n",
          (unsigned int)hr);
      return 0;
    }

    // In windowed mode, we create our own manual back buffer
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth = width;
    ddsd.dwHeight = height;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    hr = lpDD->CreateSurface(&ddsd, &lpDDS_Back, NULL);
  } else {
    // Fullscreen: Get Attached Back Buffer
    DDSCAPS ddscaps;
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    hr = lpDDS_Primary->GetAttachedSurface(&ddscaps, &lpDDS_Back);
    if (FAILED(hr)) {
      LOG("[ERROR] GetAttachedSurface failed: 0x%08X\n", (unsigned int)hr);
      return 0;
    }
  }

  // 3. Create Offscreen Staging Surface (System Memory)
  memset(&ddsd, 0, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
  ddsd.dwWidth = width;
  ddsd.dwHeight = height;
  ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

  LOG("[INFO] Creating Staging Surface...\n");
  hr = lpDD->CreateSurface(&ddsd, &lpDDS_Offscreen, NULL);
  if (FAILED(hr)) {
    LOG("[ERROR] Staging Surface creation failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  DAT_00489298 = 2; // Signal ready
  LOG("[INFO] DirectDraw Initialized Successfully.\n");
  return 1;
}

// Restore_Surfaces (Called when surface is lost)
void Restore_Surfaces(void) {
  if (lpDDS_Primary)
    lpDDS_Primary->Restore();
  if (lpDDS_Back)
    lpDDS_Back->Restore();
  if (lpDDS_Offscreen)
    lpDDS_Offscreen->Restore();
}

// Render_Frame (0045d800)
void Render_Frame(void) {
  if (!lpDDS_Offscreen || !lpDDS_Back || !lpDDS_Primary)
    return;

  HRESULT hr;
  DDSURFACEDESC ddsd;
  memset(&ddsd, 0, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);

  // 1. Lock Offscreen Staging Surface
  hr = lpDDS_Offscreen->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
  if (hr == DDERR_SURFACELOST) {
    Restore_Surfaces();
    return;
  }
  if (FAILED(hr))
    return;

  // 2. Copy Software_Buffer to Offscreen Surface
  int frame_idx = DAT_004877c8 & 0xFF;
  if (frame_idx > 2)
    frame_idx = 0;

  unsigned short *src = DAT_004877c0 + (frame_idx * 640 * 480);
  unsigned short *dst = (unsigned short *)ddsd.lpSurface;
  int pitchPixels = ddsd.lPitch / 2;

  for (int y = 0; y < 480; y++) {
    memcpy(dst + y * pitchPixels, src + y * 640, 640 * 2);
  }

  lpDDS_Offscreen->Unlock(NULL);

  // 3. Blit Offscreen to Back Buffer (using BltFast like the game)
  hr = lpDDS_Back->BltFast(0, 0, lpDDS_Offscreen, NULL, DDBLTFAST_WAIT);
  if (FAILED(hr)) {
    if (hr == DDERR_SURFACELOST) {
      Restore_Surfaces();
    } else {
      static int err_count = 0;
      if (err_count < 10) {
        LOG("[ERROR] BltFast failed: 0x%08X\n", (unsigned int)hr);
        err_count++;
      }
    }
    return;
  }

  // 4. Flip or Blit to Primary
  DDSCAPS caps;
  lpDDS_Back->GetCaps(&caps);
  if (caps.dwCaps & DDSCAPS_BACKBUFFER) {
    hr = lpDDS_Primary->Flip(NULL, DDFLIP_WAIT);
    if (FAILED(hr) && hr != DDERR_SURFACELOST) {
      static int flip_err = 0;
      if (flip_err < 10) {
        LOG("[ERROR] Flip failed: 0x%08X\n", (unsigned int)hr);
        flip_err++;
      }
    }
  } else {
    // Windowed mode Blit
    hr = lpDDS_Primary->Blt(NULL, lpDDS_Back, NULL, DDBLT_WAIT, NULL);
    if (FAILED(hr) && hr != DDERR_SURFACELOST) {
      static int blt_err = 0;
      if (blt_err < 10) {
        LOG("[ERROR] Blt to Primary failed: 0x%08X\n", (unsigned int)hr);
        blt_err++;
      }
    }
  }

  if (hr == DDERR_SURFACELOST) {
    Restore_Surfaces();
  }
}

// Release_DirectDraw_Surfaces
void Release_DirectDraw_Surfaces(void) {
  if (lpDDS_Offscreen) {
    lpDDS_Offscreen->Release();
    lpDDS_Offscreen = NULL;
  }
  if (lpDDS_Back) {
    lpDDS_Back->Release();
    lpDDS_Back = NULL;
  }
  if (lpDDS_Primary) {
    lpDDS_Primary->Release();
    lpDDS_Primary = NULL;
  }
}
