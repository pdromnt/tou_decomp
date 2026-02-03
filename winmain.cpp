#include "tou.h"
#include <ddraw.h>
#include <dinput.h>
#include <stdarg.h>
#include <stdio.h>

// Globals
HWND hWnd_Main = NULL;
BYTE g_GameState = 0;
BOOL g_bIsActive = FALSE;
// LPDIRECTDRAWSURFACE and LPDIRECTDRAW should be extern in tou.h and defined in
// graphics.cpp

// External function prototypes
void Game_State_Manager(void);
void Game_Update_Render(void);
void Input_Update(void);
void Render_Frame(void);
int Init_DirectDraw(int width, int height);

// Log declaration moved to tou.h. Implementation in utils.cpp.

// Play_Music and Stop_All_Sounds moved to sound.cpp.

// WndProc (00461f60)
extern "C" LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    Release_DirectDraw_Surfaces();
    if (lpDD) {
      lpDD->Release();
      lpDD = NULL;
    }
    FSOUND_Close();
    PostQuitMessage(0);
    return 0;

  case WM_ACTIVATEAPP:
    g_bIsActive = (BOOL)wParam;
    break;

  case WM_SETCURSOR:
    SetCursor(NULL);
    return TRUE;
  }

  return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  FILE *f = fopen("debug.txt", "w");
  if (f) {
    fprintf(f, "--- Tunnels of Underworld Debug Log ---\n");
    fprintf(f, "[DEBUG] WinMain Entered.\n");
    fflush(f);
    fclose(f);
  }

  // AllocConsole();
  // freopen("CONOUT$", "w", stdout);

  // Use Log helper later

  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "TOU_CLASS";
  wc.hIcon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
  wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);

  if (!RegisterClassA(&wc))
    return 0;

  hWnd_Main = CreateWindowExA(0, "TOU_CLASS", "Tunnels of Underworld",
                              WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL,
                              hInstance, NULL);

  if (!hWnd_Main)
    return 0;

  ShowWindow(hWnd_Main, nCmdShow);
  UpdateWindow(hWnd_Main);
  g_bIsActive = TRUE; // Start active

  // 1. Sound Init
  if (!FSOUND_Init(44100, 32, 0)) {
    LOG("[ERROR] FSOUND_Init FAILED\n");
  } else {
    LOG("[INFO] Sound System Initialized.\n");
  }

  // 2. Memory Init
  Init_Memory_Pools();

  // 3. DirectDraw Init
  HRESULT hr;
  hr = DirectDrawCreate(NULL, &lpDD, NULL);
  if (FAILED(hr)) {
    LOG("[ERROR] DirectDrawCreate failed: 0x%08X\n", (unsigned int)hr);
    return 0;
  }

  // Try Fullscreen first
  hr = lpDD->SetCooperativeLevel(hWnd_Main, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
  if (FAILED(hr)) {
    LOG("[WARNING] SetCooperativeLevel Fullscreen failed: 0x%08X. Trying "
        "Normal...\n",
        (unsigned int)hr);
    lpDD->SetCooperativeLevel(hWnd_Main, DDSCL_NORMAL);
  }

  if (!Init_DirectDraw(640, 480)) {
    LOG("[WARNING] Init_DirectDraw (Fullscreen) failed. Retrying "
        "windowed...\n");
    lpDD->SetCooperativeLevel(hWnd_Main, DDSCL_NORMAL);
    if (!Init_DirectDraw(640, 480)) {
      LOG("[ERROR] Init_DirectDraw FAILED completely.\n");
      return 0;
    }
  }

  // 4. Input Init
  LOG("[DEBUG] WinMain: Calling Init_DirectInput...\n");
  Init_DirectInput();
  LOG("[DEBUG] WinMain: Calling Early_Init_Vars...\n");
  Early_Init_Vars();
  if (System_Init_Check() != 0)
    return 0;
  // Set initial state to Init (3)
  g_GameState = 3;
  // Start in Intro Sequence state

  // 5. Message Loop
  MSG msg;
  while (TRUE) {
    if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        break;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    } else {
      if (!g_bIsActive) {
        WaitMessage();
      } else {
        if (g_GameState == 0)
          Game_Update_Render();
        else
          Game_State_Manager();

        Input_Update();
        Render_Frame();
      }
    }
  }

  return msg.wParam;
}
