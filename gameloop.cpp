#include "tou.h"
#include <dinput.h>
#include <stdio.h>

// Globals
LPDIRECTINPUTDEVICE lpDI_Device = NULL;
char DAT_004877be = 0;

// Internal Logic Globals
int DAT_004877e4 = 0;
int DAT_004877e8 = 0;
int DAT_004877b4 = 0;
int DAT_004877b8 = 0;

// Input Update (00462560)
void Input_Update(void) {
  if (lpDI_Device == NULL)
    return;
  DIDEVICEOBJECTDATA didod[16];
  DWORD dwElements = 16;
  HRESULT hr = lpDI_Device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), didod,
                                          &dwElements, 0);
  if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
    lpDI_Device->Acquire();
    return;
  }
  if (FAILED(hr))
    return;
  for (DWORD i = 0; i < dwElements; i++) {
    switch (didod[i].dwOfs) {
    case DIMOFS_X:
      if (DAT_004877e4 == 1)
        DAT_004877e8 += didod[i].dwData * 128;
      else
        DAT_004877b4 += didod[i].dwData * 0x800;
      break;
    case DIMOFS_Y:
      if (DAT_004877e4 == 0)
        DAT_004877b8 += didod[i].dwData * 0x800;
      break;
    case DIMOFS_BUTTON0:
      if (!(didod[i].dwData & 0x80))
        DAT_004877be ^= 1;
      break;
    case DIMOFS_BUTTON1:
      if (!(didod[i].dwData & 0x80))
        DAT_004877be ^= 2;
      break;
    }
  }
}

// State Manager (00461260)
void Game_State_Manager(void) {
  switch (g_GameState) {
  case 3: // Initialization from 004612e4
    LOG("[INFO] Entering Initialization State (3)\n");
    // Assembly: Load_Background_To_Buffer(1), Play_Music(), Intro_Init()
    Load_Background_To_Buffer(1);
    Play_Music();
    Intro_Sequence(); // Start intro timer etc.
    g_GameState = 0x97;
    break;

  case 0x97: // Intro Sequence State
    Intro_Sequence();
    break;

  case 2: // Main Menu State
    Stop_All_Sounds();
    Free_Game_Resources();
    Handle_Menu_State();
    break;

  case 0x98: // Post-Intro Init
    LOG("[INFO] Entering Post-Intro State (0x98)\n");
    Init_New_Game();
    break;

  case 0xFE: // Exit
    Stop_All_Sounds();
    Cleanup_Sound();
    Release_DirectDraw_Surfaces();
    PostMessageA(hWnd_Main, WM_QUIT, 0, 0);
    g_GameState = 0xFF;
    break;

  default:
    break;
  }
}

// Game Loop Render (00461710)
void Game_Update_Render(void) {
  if (g_GameState == 0) {
    // Gameplay logic...
  }
}
