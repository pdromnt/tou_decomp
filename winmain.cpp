#include "tou.h"
#include <stdio.h>

// Strings (Hardcoded for now)
const char *s_TOU_v0_1 = "Tunnels of Underworld v0.1";
const char *s_ClassName =
    "TOU_Class"; // Need to check actual string at 0047eb10
const char *s_InitFailed_Reason =
    "Init Failed. Possible reason: DirectX not installed?";
const char *s_InitFailed_NoMem = "Init Failed. You don't have enough memory.";
const char *s_DDInitFailed =
    "DirectDraw Init FAILED. Install DirectX 6.0 or higher.";

// Globals
HWND hWnd_Main = NULL;  // DAT_00489edc
HINSTANCE hInst = NULL; // DAT_00489c60

// DirectDraw/Input Globals moved to graphics.cpp/init.cpp
// LPDIRECTDRAW lpDD = NULL;
// LPDIRECTDRAWSURFACE lpDDS_Primary = NULL;
// LPDIRECTDRAWSURFACE lpDDS_Back = NULL;
// ...

BOOL g_bIsActive = FALSE; // DAT_00489ec4
BYTE g_GameState = 0;     // DAT_004877a0

int DAT_00489296 = 0;
int DAT_00489297 = 0;
int DAT_00489298 = 0;
DWORD DAT_004892b0 = 0;
int DAT_004892b4 = 0;
char DAT_004877be = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_DESTROY) { // 0x2
    if (lpDD != NULL) {
      if (lpDDS_Primary != NULL) {
        lpDDS_Primary->Release();
        lpDDS_Primary = NULL;
      }
      if (lpDDS_Back != NULL) {
        lpDDS_Back->Release();
        lpDDS_Back = NULL;
      }
      lpDD->Release();
      lpDD = NULL;
    }
    PostQuitMessage(0);
  } else if (uMsg == WM_ACTIVATEAPP) { // 0x1c
    g_bIsActive = (BOOL)wParam;
  } else if (uMsg == WM_SETCURSOR) { // 0x20
    SetCursor(NULL);
    return 1;
  }
  return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                              LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASSA wc;
  MSG msg;

  Early_Init_Vars(); // Previously FUN_0041ead0

  wc.style =
      CS_DBLCLKS | CS_HREDRAW |
      CS_VREDRAW; // 3 = HREDRAW | VREDRAW? No, 3 is HREDRAW(2) | VREDRAW(1).
  // Wait, 3 usually implies horizontal and vertical redraw.
  // Standard style is often CS_HREDRAW | CS_VREDRAW.
  // CS_HREDRAW = 0x0002, CS_VREDRAW = 0x0001. So 3 is correct.

  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon =
      LoadIconA(hInstance, (LPCSTR)101); // 0x7f00?? Need to check resource ID
  wc.hCursor = LoadCursorA(NULL, (LPCSTR)101);            // 0x7f00??
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // 4 = BLACK_BRUSH
  wc.lpszMenuName = s_ClassName; // Likely shared string
  wc.lpszClassName = s_ClassName;

  RegisterClassA(&wc);

  // Create Window
  hWnd_Main = CreateWindowEx(
      WS_EX_TOPMOST, s_ClassName, s_TOU_v0_1,
      WS_POPUP | WS_VISIBLE, // Changed from WS_POPUP to ensure visibility logic
      0, 0, 640, 480, NULL, NULL, hInstance, NULL);

  if (!hWnd_Main) {
    MessageBox(NULL, "CreateWindow Failed", "Error", MB_OK);
    return 0;
  }

  // Debug Console
  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);
  printf("DEBUG: Window Created. Handle: %p\n", hWnd_Main);

  ShowWindow(hWnd_Main, nCmdShow);
  UpdateWindow(hWnd_Main);

  // Init Systems
  printf("DEBUG: Calling Early_Init_Vars...\n");
  Early_Init_Vars();

  printf("DEBUG: Calling System_Init_Check...\n");
  if (!System_Init_Check()) {
    printf("DEBUG: System_Init_Check Failed.\n");
    return 0;
  }

  printf("DEBUG: Init Success. Entering Game...\n");

  // DirectDraw Init
  // Using wrapper which uses DD7
  if (!Init_DirectDraw(640, 480)) {
    MessageBoxA(hWnd_Main, s_DDInitFailed, s_TOU_v0_1, MB_ICONERROR);
    DestroyWindow(hWnd_Main);
    return 0;
  }

  if (Init_DirectInput() != 1) {
    // Log warning?
    // goto ExitLoop;
    // Original code jumps to loop even on input fail (returning 1 is success)
    // Our wrapper returns 1 on success.
  }

  // Message Loop
ExitLoop:
  DAT_004877be = 0;
  while (TRUE) {
    if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        break;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    } else {
      if (!g_bIsActive) {
        WaitMessage();
        // Pause logic
      } else {
        // Game Logic
        if (g_GameState == 0) {
          Game_Update_Render();
        } else {
          Game_State_Manager();
        }
        // Input
        Input_Update();
      }
    }
  } // End While(TRUE)

  return msg.wParam;
} // End if(hWnd_Main)
