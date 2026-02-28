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
        g_bIsActive = (int)wParam;
    }
    else if (uMsg == WM_SETCURSOR) {                /* 0x20 */
        /* Hide Windows cursor over client area - game renders its own. */
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(NULL);
            return TRUE;
        }
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
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)0x7F00);   /* IDC_ARROW */
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

        while (1) {
            /* Inner loop: check for messages with PeekMessage (PM_NOREMOVE) */
            while (1) {
                bVar = PeekMessageA(&msg, NULL, 0, 0, 0); /* PM_NOREMOVE */
                if (bVar != 0)
                    break;

                /* No messages pending - run game logic */
                if (g_bIsActive == 0) {
                    /* App is inactive - wait for messages */
                    WaitMessage();
                    g_SubState     = 1;
                    g_NeedsRedraw  = 1;
                    g_SurfaceReady = 2;
                    g_TimerStart   = timeGetTime();
                    g_TimerAux     = 0;
                } else {
                    /* App is active - run game */
                    if (g_GameState == 0x01) {
                        Input_Update();
                    }
                    g_ProcessInput = 1;
                    if (g_GameState == 0x00) {
                        Game_Update_Render();
                    } else {
                        Game_State_Manager();
                    }
                }
            }

            /* Message available - retrieve and dispatch */
            bVar = GetMessageA(&msg, NULL, 0, 0);
            if (bVar == 0)
                break; /* WM_QUIT */
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        timeEndPeriod(1);
        return (int)msg.wParam;
    }

    timeEndPeriod(1);
    return 0;
}
