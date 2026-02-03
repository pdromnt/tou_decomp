#include "tou.h"

// Globals
// Input
LPDIRECTINPUTDEVICE lpDI_Device = NULL; // DAT_00489ec0
// int DAT_00489ec4 = 0; // g_bIsActive in tou.h
// Input Active?

// Game State
int DAT_004877e4 = 0;
int DAT_004877e8 = 0;
int DAT_004877b4 = 0;
int DAT_004877b8 = 0;
// BYTE g_GameState; // Already in tou.h

// Input Update
// Input Update (00462560) - Handles Mouse (Buffered)
void Input_Update(void) {
  if (lpDI_Device == NULL)
    return; // DAT_00489ec0 = Mouse

  DIDEVICEOBJECTDATA didod[16];
  DWORD dwElements = 16;
  HRESULT hr;

  hr = lpDI_Device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), didod,
                                  &dwElements, 0);
  if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
    lpDI_Device->Acquire();
    return;
  }
  if (FAILED(hr))
    return;

  for (DWORD i = 0; i < dwElements; i++) {
    switch (didod[i].dwOfs) {
    case DIMOFS_X: // 0
      if (DAT_004877e4 == 1)
        DAT_004877e8 += didod[i].dwData * 128; // Tuning
      else
        DAT_004877b4 += didod[i].dwData * 0x800; // Tuning
      break;
    case DIMOFS_Y: // 4
      if (DAT_004877e4 == 0)
        DAT_004877b8 += didod[i].dwData * 0x800;
      break;
    case DIMOFS_BUTTON0: // 0xc
      // Simple toggle logic based on decomp
      if (!(didod[i].dwData & 0x80)) { // Key Up
        DAT_004877be ^= 1;
        break;
      }
    case DIMOFS_BUTTON1: // 0xd
      if (!(didod[i].dwData & 0x80)) {
        DAT_004877be ^= 2;
      }
      break;
    }
  }
}

// State Manager
// Game State Manager (00461260)
// Manages game state transitions
void Game_State_Manager(void) {
  switch (g_GameState) {
  case 1:
    // Unknown/Idle?
    // goto case 1? (Spin wait?)
    break;
  case 2:
    // Menu / Transition
    // DAT_00489299 = 0; // Reset something
    Stop_All_Sounds();
    Free_Game_Resources(); // 0040ffc0
    Handle_Menu_State();   // 004611d0
    break;

  case 3:
    // Init DirectDraw / Intro Start
    // Check if already init?
    if (Init_DirectDraw(640, 480)) {
      // Success
      // DAT_004877b1 = 1; // Flag?
      g_GameState = 0x97; // Transition to Intro

      // Init other things?
      // FUN_0042d710(1);
      // FUN_0040e130();
      // FUN_0045d7d0();
    } else {
      // Fail
      Handle_Init_Error(hWnd_Main, 0); // DD Fail
    }
    break;

  case 0x97: // 151
    // Intro Sequence
    Intro_Sequence();
    break;

  case 0x98: // 152
    // Init New Game / Level?
    // DAT_004877a4 = 0x98; // Substate
    Init_New_Game();
    // g_GameState = 0xfe; // Temporary test exit
    break;

  case 0xFE: // 254
    // Breakdown / Exit
    Cleanup_Sound();
    PostMessageA(hWnd_Main, WM_DESTROY, 0, 0); // Quit
    g_GameState = 0xFF;                        // Done
    break;

  case 0xFF:
    // Exit
    break;

  default:
    break;
  }
}

// Main Loop
void Game_Update_Render(void) {
  // timeGetTime checks
  // Input polling
  // Rendering calls
}
