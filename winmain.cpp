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

/* ===== WndProc (00461F60) ===== */
extern "C" LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DESTROY) {                       /* 0x02 */
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

        /* Release DirectDraw (matches original WndProc) */
        if (lpDD != NULL) {
            if (lpDDS_Primary != NULL) {
                lpDDS_Primary->Release();
                lpDDS_Primary = NULL;
            }
            if (lpDDS_Offscreen != NULL) {
                lpDDS_Offscreen->Release();
                lpDDS_Offscreen = NULL;
            }
            lpDD->Release();
            lpDD = NULL;
        }

        /* Clean up FMOD */
        Cleanup_Sound();

        PostQuitMessage(0);
    }
    else if (uMsg == WM_ACTIVATEAPP) {              /* 0x1C */
        /* COMPAT: Always stay active in windowed mode.
         * Original ran DDSCL_EXCLUSIVE fullscreen where the window always
         * had foreground — WM_ACTIVATEAPP(0) never fired during normal play.
         * In windowed DDSCL_NORMAL mode, DDraw surface operations during
         * state transitions trigger spurious WM_ACTIVATEAPP(0), which sets
         * g_bIsActive=0 and freezes the game loop in the inactive pause path.
         * Since we can't distinguish spurious from real deactivation, always
         * stay active — matching the original fullscreen behavior. */
        g_bIsActive = 1;
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

    /* Release DirectDraw objects if they exist */
    if (lpDD != NULL) {
        if (lpDDS_Primary != NULL) {
            lpDDS_Primary->Release();
            lpDDS_Primary = NULL;
        }
        if (lpDDS_Offscreen != NULL) {
            lpDDS_Offscreen->Release();
            lpDDS_Offscreen = NULL;
        }
        lpDD->Release();
        lpDD = NULL;
    }

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
            STR_TITLE,              /* "TOU v0.1" */
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

    /* 5. DirectDraw Create */
    LOG("[INIT] DirectDrawCreate...\n");
    iVar1 = DirectDrawCreate(NULL, &lpDD, NULL);
    LOG("[INIT] DirectDrawCreate returned 0x%08X\n", iVar1);
    if (iVar1 != 0) {
        /* DDraw create failed - cleanup if partially created */
        if (lpDD != NULL) {
            Release_DirectDraw_Surfaces();
            lpDD->Release();
            lpDD = NULL;
        }
        MessageBoxA(hWnd, STR_ERR_DDRAW_INSTALL, STR_TITLE, MB_ICONERROR);
        DestroyWindow(hWnd);
        timeEndPeriod(1);
        return 0;
    }

    /* 6. Set Cooperative Level
     * Original: DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN
     * COMPAT:   DDSCL_NORMAL for windowed mode on modern Windows */
    iVar1 = lpDD->SetCooperativeLevel(hWnd, DDSCL_NORMAL);
    LOG("[INIT] SetCooperativeLevel returned 0x%08X\n", iVar1);
    if (iVar1 == 0) {
        /* 7. Init DirectInput */
        LOG("[INIT] Init_DirectInput...\n");
        iVar1 = Init_DirectInput();
        LOG("[INIT] Init_DirectInput returned %d\n", iVar1);
        if (iVar1 != 0) {
            /* Success - zero surface pointers, enter main loop */
            lpDDS_Primary   = NULL;
            lpDDS_Offscreen = NULL;
            lpDDS_Back      = NULL;
            goto MAIN_LOOP;
        }
        uVar3 = 3;  /* DirectInput error */
    } else {
        uVar3 = 0;  /* SetCooperativeLevel error */
    }

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
                /* Original: pause game when WM_ACTIVATEAPP(0) fires.
                 * COMPAT: WM_ACTIVATEAPP handler now always sets g_bIsActive=1,
                 * so this path is only reached if g_bIsActive is explicitly
                 * cleared elsewhere. Kept for correctness. */
                Sleep(0);
                g_SubState     = 1;
                g_NeedsRedraw  = 1;
                g_SurfaceReady = 2;
                g_TimerStart   = timeGetTime();
                g_TimerAux     = 0;
            } else {
                /* App is active - run game */
                /* COMPAT: Sync cursor from Windows position for windowed mode.
                 * Must run for ALL game states (menu can be at g_GameState 0 or 1).
                 * Overrides DirectInput relative deltas with absolute Win32 coords. */
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hWnd_Main, &pt);
                    g_MouseDeltaX = pt.x << 18;
                    g_MouseDeltaY = pt.y << 18;
                }
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
