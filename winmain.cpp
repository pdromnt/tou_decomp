/*
 * winmain.cpp - WinMain, WndProc, Handle_Init_Error
 * Addresses: WinMain=00462300, WndProc=00461F60, Handle_Init_Error=00462010
 *
 * Matched to TOU15b.exe decompilation via Ghidra.
 */
#include "tou.h"
#include <stdio.h>

/* ===== Globals defined in this module ===== */
HWND           hWnd_Main   = NULL;   /* 00489EDC */
int            g_bIsActive = 0;      /* 00489EC4 */
unsigned char  g_GameState = 0;      /* 004877A0 */
DWORD          g_TimerStart = 0;     /* 004892B0 */
int            g_TimerAux   = 0;     /* 004892B4 */

static void Set_Focus_Audio_Muted(int muted)
{
    if (!g_SoundEnabled)
        return;

    int sfx_volume = muted ? 0 : ((int)DAT_00483720[1] * 0xFF) / 100;
    int music_volume = muted ? 0 : ((int)DAT_00483720[0] * 0xFF) / 100;
    FSOUND_SetSFXMasterVolume(sfx_volume);
    if (g_MusicStream != NULL && g_MusicChannel >= 0)
        FSOUND_SetPaused(g_MusicChannel, muted ? 1 : (DAT_004877a4 == 0x97));
    if (g_MusicModule != NULL)
        FMUSIC_SetMasterVolume(g_MusicModule, music_volume);
}

/* ===== WndProc (00461F60) ===== */
extern "C" LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CLOSE) {                         /* 0x10 */
        /* COMPAT: The original had no window close button, so its only normal
         * exit path saved options first.  The windowed decomp does have one;
         * treating Alt+F4 / the title-bar X as a normal exit prevents menu
         * changes from silently disappearing. */
        Save_Options_Config();
        DestroyWindow(hWnd);
        return 0;
    }
    else if (uMsg == WM_DESTROY) {                  /* 0x02 */
        /* COMPAT: Release DirectInput devices before DDraw.
         * Original relied on OS cleanup at process exit, but modern
         * Windows can leave COM objects alive between rapid relaunches. */
        if (lpDI_Mouse != NULL) {
            lpDI_Mouse->Unacquire();
            lpDI_Mouse->Release();
            lpDI_Mouse = NULL;
        }
        if (lpDI_Keyboard != NULL) {
            lpDI_Keyboard->Unacquire();
            lpDI_Keyboard->Release();
            lpDI_Keyboard = NULL;
        }
        if (lpDI != NULL) {
            lpDI->Release();
            lpDI = NULL;
        }
        if (hMouseEvent != NULL) {
            CloseHandle(hMouseEvent);
            hMouseEvent = NULL;
        }

        RenderBackend_Shutdown();

        /* Clean up FMOD */
        Cleanup_Sound();

        PostQuitMessage(0);
    }
    else if (uMsg == WM_ACTIVATEAPP) {              /* 0x1C */
        g_bIsActive = (wParam != 0) ? 1 : 0;
        if (g_bIsActive) {
            HRESULT mouse_hr = DI_OK;
            HRESULT keyboard_hr = DI_OK;
            if (lpDI_Mouse != NULL) mouse_hr = lpDI_Mouse->Acquire();
            if (lpDI_Keyboard != NULL) keyboard_hr = lpDI_Keyboard->Acquire();
            Set_Focus_Audio_Muted(0);
            LOG("[FOCUS] activate state=%u page=%u mouse=0x%08lX keyboard=0x%08lX\n",
                (unsigned int)g_GameState, (unsigned int)DAT_004877a4,
                (unsigned long)mouse_hr, (unsigned long)keyboard_hr);
        } else {
            if (GetCapture() == hWnd_Main)
                ReleaseCapture();
            if (g_GameState == 0 && g_SubState == 0) {
                g_SubState = 1;
                g_NeedsRedraw = 1;
                g_SurfaceReady = 2;
                g_TimerStart = timeGetTime();
                g_TimerAux = 0;
                g_FrameTimer = timeGetTime();
            }
            Set_Focus_Audio_Muted(1);
            LOG("[FOCUS] deactivate state=%u page=%u substate=%u\n",
                (unsigned int)g_GameState, (unsigned int)DAT_004877a4,
                (unsigned int)g_SubState);
        }
    }
    else if (uMsg == WM_MOUSEMOVE) {                /* 0x0200 */
        /* COMPAT: Menu cursor movement must not depend on DirectInput.
         * Match active-window client coordinates directly.
         * Slider drags deliberately freeze the anchor and accumulate X motion
         * elsewhere, so leave that mode alone. */
        if (g_bIsActive && g_InputMode == 0) {
            int mouse_x;
            int mouse_y;
            Client_To_Game_Coordinates((int)(short)LOWORD(lParam),
                                       (int)(short)HIWORD(lParam),
                                       &mouse_x, &mouse_y);
            int moved = (mouse_x != (g_MouseDeltaX >> 18) ||
                         mouse_y != (g_MouseDeltaY >> 18));
            g_MouseDeltaX = mouse_x << 18;
            g_MouseDeltaY = mouse_y << 18;
            /* COMPAT: The post-match DDraw transition leaves Windows waiting
             * for a click before normal hover delivery resumes.  A temporary
             * capture replaces that click; release it on the first genuine
             * motion so menus keep normal inside-window behavior afterward. */
            if (moved && DAT_004877a4 == 0x13 && GetCapture() == hWnd_Main) {
                LOG("[SCOREBOARD CURSOR] first motion=(%d,%d), releasing capture\n",
                    mouse_x, mouse_y);
                ReleaseCapture();
            }
        }
    }
    else if (uMsg == WM_SETFOCUS) {                 /* 0x0007 */
        /* A real focus restoration is the correct point to reacquire the
         * foreground DirectInput devices used by gameplay/keyboard input. */
        g_bIsActive = 1;
        if (lpDI_Mouse != NULL) lpDI_Mouse->Acquire();
        if (lpDI_Keyboard != NULL) lpDI_Keyboard->Acquire();
        Set_Focus_Audio_Muted(0);
    }
    else if (uMsg == WM_SETCURSOR) {                /* 0x20 */
        /* Hide cursor unconditionally — matches original binary exactly.
         * Original relied on DDraw exclusive fullscreen; we add ShowCursor(FALSE)
         * in WinMain for windowed-mode compat. */
        SetCursor(NULL);
        return TRUE;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

/* ===== Handle_Init_Error (00462010) ===== */
int Handle_Init_Error(HWND hWnd, unsigned char errorCode)
{
    const char *lpText;

    RenderBackend_Shutdown();

    switch (errorCode) {
    case 0:
        lpText = STR_ERR_DDRAW_INSTALL;
        break;
    case 1:
        lpText = STR_ERR_DDRAW_MODE;
        break;
    case 2:
        lpText = STR_ERR_DDRAW_MEMORY;
        break;
    case 3:
        lpText = STR_ERR_DINPUT;
        break;
    default:
        lpText = STR_ERR_UNKNOWN;
        break;
    }

    MessageBoxA(hWnd, lpText, STR_TITLE, MB_ICONERROR);
    DestroyWindow(hWnd);
    return 0;
}

/* ===== WinMain (00462300) ===== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    HWND hWnd;
    int iVar1;
    BOOL bVar;
    unsigned char uVar3;
    WNDCLASSA wc;
    MSG msg;

    /* Check for --logging flag */
    if (lpCmdLine && strstr(lpCmdLine, "--logging")) {
        g_LogEnabled = 1;
    }

    /* 1. Early init - before anything else */
    Early_Init_Vars();

    /* COMPAT: Request 1ms timer resolution for accurate Sleep() and timeGetTime().
     * Original used DDraw exclusive fullscreen with vsync-locked flip chain (60Hz).
     * In windowed mode we use Sleep()-based frame limiting, which requires 1ms
     * resolution to hit 60fps targets. Without this, Windows default ~15.6ms
     * granularity causes Sleep(16) to actually sleep ~31ms → ~30fps. */
    timeBeginPeriod(1);

    /* 2. Register window class */
    wc.style         = CS_HREDRAW | CS_VREDRAW;             /* 3 */
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconA(hInstance, (LPCSTR)0x7F00); /* IDI_APPLICATION from resources */
    wc.hCursor       = NULL;   /* COMPAT: NULL prevents Windows from restoring IDC_ARROW
                                * in windowed mode. Original used IDC_ARROW but ran
                                * DDSCL_EXCLUSIVE which auto-hid the system cursor. */
    wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH); /* 4 */
    wc.lpszMenuName  = STR_CLASSNAME;                        /* "TOU" */
    wc.lpszClassName = STR_CLASSNAME;                        /* "TOU" */
    RegisterClassA(&wc);

    /* 3. Create window
     * Original: WS_POPUP 0,0,0,0 (DDraw exclusive fullscreen takes over)
     * COMPAT:   Windowed with title bar, 640x480 client area */
    {
        DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT rc = { 0, 0, 640, 480 };
        AdjustWindowRect(&rc, dwStyle, FALSE);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        int posX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        int posY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
        hWnd = CreateWindowExA(
            0,                      /* dwExStyle */
            STR_CLASSNAME,          /* "TOU" */
            STR_TITLE,              /* "Window Title" */
            dwStyle,
            posX, posY, winW, winH, /* Centered, client area = 640x480 */
            NULL,                   /* parent */
            NULL,                   /* menu */
            hInstance,
            NULL                    /* lpParam */
        );
    }
    hWnd_Main = hWnd;

    if (hWnd == NULL) {
        timeEndPeriod(1);
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* COMPAT: Hide system cursor for windowed mode.
     * Original ran exclusive fullscreen where DDraw auto-hid the cursor.
     * WndProc also sets SetCursor(NULL) on WM_SETCURSOR. */
    ShowCursor(FALSE);

    /* 4. System Init Check (returns 1 on success) */
    LOG("[INIT] System_Init_Check...\n");
    iVar1 = System_Init_Check();
    LOG("[INIT] System_Init_Check returned %d\n", iVar1);
    if (iVar1 != 1) {
        if (iVar1 == 0) {
            /* File not found / general init failure */
            MessageBoxA(hWnd_Main, STR_ERR_INIT_FILENOTFOUND, STR_TITLE, MB_ICONERROR);
        } else {
            /* No levels or GG themes */
            MessageBoxA(hWnd_Main, STR_ERR_INIT_NOLEVELS, STR_TITLE, MB_ICONERROR);
        }
        timeEndPeriod(1);
        return 0;
    }
    Apply_Display_Settings();

    /* 5. Initialize the selected presentation backend. */
    LOG("[INIT] Initializing %s renderer...\n", RenderBackend_Name());
    iVar1 = RenderBackend_Initialize(hWnd);
    if (iVar1 == 0) {
        RenderBackend_Shutdown();
        MessageBoxA(hWnd, STR_ERR_DDRAW_INSTALL, STR_TITLE, MB_ICONERROR);
        DestroyWindow(hWnd);
        timeEndPeriod(1);
        return 0;
    }

    /* 6. Init DirectInput */
    LOG("[INIT] Init_DirectInput...\n");
    iVar1 = Init_DirectInput();
    LOG("[INIT] Init_DirectInput returned %d\n", iVar1);
    if (iVar1 != 0) {
        goto MAIN_LOOP;
    }
    uVar3 = 3;

    /* Error path - Handle_Init_Error always returns 0, so we exit */
    iVar1 = Handle_Init_Error(hWnd, uVar3);
    if (iVar1 != 0) {
        /* Dead code in original binary - Handle_Init_Error always returns 0 */
MAIN_LOOP:
        LOG("[INIT] Entering MAIN_LOOP\n");
        g_MouseButtons = 0;

        /* Game_Update_Render (state 0) is the full gameplay loop (Phase 6).
         * Until it's decompiled, start at intro init instead. */
        g_GameState = 0x96;

        /* COMPAT: Time-limited message pump.
         * Original uses PeekMessage(PM_NOREMOVE) + GetMessage, processing
         * ONE message per iteration.  In exclusive fullscreen mode this is
         * fine because very few window messages arrive.  In windowed mode,
         * mouse movement continuously generates WM_MOUSEMOVE + WM_SETCURSOR
         * messages — the drain loop never finishes and game logic starves,
         * making the cursor appear frozen.
         *
         * Fix: process messages for at most 4ms per iteration, then always
         * run game logic.  This guarantees ~60fps even under message flood,
         * matching the original's effective behavior in fullscreen. */
        while (1) {
            /* 1. Process pending messages (time-limited) */
            {
                DWORD msg_deadline = timeGetTime() + 4;
                while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        timeEndPeriod(1);
                        return (int)msg.wParam;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                    if (timeGetTime() >= msg_deadline)
                        break;  /* Bail — game logic needs to run */
                }
            }

            /* 2. Run one game tick */
            if (g_bIsActive == 0) {
                /* WM_ACTIVATEAPP performs the one-shot pause/mute work. Keep
                 * simulation and menu input frozen until Windows reactivates us. */
                Sleep(1);
            } else {
                /* App is active - run game */
                /* COMPAT: Sync cursor from Windows position for windowed mode.
                 * Must run for ALL game states (menu can be at g_GameState 0 or 1).
                 * Overrides DirectInput relative deltas with absolute Win32 coords. */
                {
                    POINT pt;
                    RECT client;
                    GetCursorPos(&pt);
                    ScreenToClient(hWnd_Main, &pt);
                    GetClientRect(hWnd_Main, &client);
                    if (PtInRect(&client, pt)) {
                        int game_x, game_y;
                        Client_To_Game_Coordinates(pt.x, pt.y, &game_x, &game_y);
                        g_MouseDeltaX = game_x << 18;
                        g_MouseDeltaY = game_y << 18;
                    }
                }
                /* Original binary calls Input_Update() here for g_GameState==1
                 * to poll DirectInput mouse (accumulates DAT_004877e8 for slider drag).
                 * COMPAT: In windowed mode, all mouse input (position, buttons, slider
                 * delta) is handled via Win32 APIs in FUN_00425fe0's COMPAT block,
                 * making Input_Update redundant. Calling it would double-accumulate
                 * DAT_004877e8 since both DirectInput and Win32 cursor delta report
                 * the same physical mouse movement. */
                g_ProcessInput = 1;
                if (g_GameState == 0x00) {
                    Game_Update_Render();
                } else {
                    Game_State_Manager();
                }
            }
        }

        timeEndPeriod(1);
        return (int)msg.wParam;
    }

    timeEndPeriod(1);
    return 0;
}
