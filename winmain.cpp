/*
 * winmain.cpp - WinMain, WndProc, Handle_Init_Error
 * Addresses: WinMain=00462300, WndProc=00461F60, Handle_Init_Error=00462010
 *
 * Matched to TOU15b.exe decompilation via Ghidra.
 */
#include "tou.h"
#ifdef TOU_HAS_SDL
#include "platform.h"
#endif
#include <stdio.h>

/* ===== Globals defined in this module ===== */
HWND           hWnd_Main   = NULL;   /* 00489EDC */
int            g_bIsActive = 0;      /* 00489EC4 */
GameState      g_GameState = GAME_STATE_GAMEPLAY; /* 004877A0 */
DWORD          g_TimerStart = 0;     /* 004892B0 */
int            g_TimerAux   = 0;     /* 004892B4 */
static int     g_QuitRequested = 0;
static int     g_RuntimeShutdown = 0;

void GameState_Transition(GameState next_state)
{
    LOG("[STATE] main %u -> %u\n", (unsigned)g_GameState, (unsigned)next_state);
    g_GameState = next_state;
}

static void Set_Focus_Audio_Muted(int muted)
{
    if (!g_SoundEnabled)
        return;
    Audio_SetMuted(muted);
}

static void Release_Legacy_Input(void)
{
#ifndef TOU_HAS_SDL
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
#endif
}

static void Shutdown_Runtime(void)
{
    if (g_RuntimeShutdown)
        return;
    g_RuntimeShutdown = 1;
    Release_Legacy_Input();
    RenderBackend_Shutdown();
    Cleanup_Sound();
}

static void Handle_App_Focus(int active)
{
    g_bIsActive = active ? 1 : 0;
    if (g_bIsActive) {
#ifndef TOU_HAS_SDL
        HRESULT mouse_hr = DI_OK;
        HRESULT keyboard_hr = DI_OK;
        if (lpDI_Mouse != NULL) mouse_hr = lpDI_Mouse->Acquire();
        if (lpDI_Keyboard != NULL) keyboard_hr = lpDI_Keyboard->Acquire();
        LOG("[FOCUS] activate state=%u page=%u mouse=0x%08lX keyboard=0x%08lX\n",
            (unsigned int)g_GameState, (unsigned int)DAT_004877a4,
            (unsigned long)mouse_hr, (unsigned long)keyboard_hr);
#else
        LOG("[FOCUS] activate state=%u page=%u\n",
            (unsigned int)g_GameState, (unsigned int)DAT_004877a4);
#endif
        Set_Focus_Audio_Muted(0);
    } else {
#ifndef TOU_HAS_SDL
        if (GetCapture() == hWnd_Main) ReleaseCapture();
#endif
        if (g_GameState == GAME_STATE_GAMEPLAY && g_SubState == GAMEPLAY_ACTIVE) {
            g_SubState = GAMEPLAY_PAUSED;
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

static void Handle_Mouse_Motion(int client_x, int client_y)
{
    if (!g_bIsActive || g_InputMode != 0)
        return;
    int mouse_x;
    int mouse_y;
    Client_To_Game_Coordinates(client_x, client_y, &mouse_x, &mouse_y);
    int moved = (mouse_x != (g_MouseDeltaX >> 18) ||
                 mouse_y != (g_MouseDeltaY >> 18));
    g_MouseDeltaX = mouse_x << 18;
    g_MouseDeltaY = mouse_y << 18;
#ifndef TOU_HAS_SDL
    if (moved && DAT_004877a4 == 0x13 && GetCapture() == hWnd_Main) {
        LOG("[SCOREBOARD CURSOR] first motion=(%d,%d), releasing capture\n",
            mouse_x, mouse_y);
        ReleaseCapture();
    }
#else
    (void)moved;
#endif
}

void Request_App_Quit(void)
{
#ifdef TOU_HAS_SDL
    g_QuitRequested = 1;
#else
    PostMessageA(hWnd_Main, WM_DESTROY, 0, 0);
#endif
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
        Shutdown_Runtime();
        PostQuitMessage(0);
    }
    else if (uMsg == WM_ACTIVATEAPP) {              /* 0x1C */
        Handle_App_Focus(wParam != 0);
    }
    else if (uMsg == WM_MOUSEMOVE) {                /* 0x0200 */
        /* COMPAT: Menu cursor movement must not depend on DirectInput.
         * Match active-window client coordinates directly.
         * Slider drags deliberately freeze the anchor and accumulate X motion
         * elsewhere, so leave that mode alone. */
        Handle_Mouse_Motion((int)(short)LOWORD(lParam),
                            (int)(short)HIWORD(lParam));
    }
    else if (uMsg == WM_SETFOCUS) {                 /* 0x0007 */
        /* A real focus restoration is the correct point to reacquire the
         * foreground DirectInput devices used by gameplay/keyboard input. */
        Handle_App_Focus(1);
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
#ifdef TOU_HAS_SDL
    Platform_DestroyWindow();
#else
    DestroyWindow(hWnd);
#endif
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
    MSG msg = {};

    /* Check for --logging flag */
    if (lpCmdLine && strstr(lpCmdLine, "--logging")) {
        g_LogEnabled = 1;
    }
    if (lpCmdLine && strstr(lpCmdLine, "--directdraw")) {
        RenderBackend_SelectByName("directdraw");
    }
    if (lpCmdLine && strstr(lpCmdLine, "--sdl")) {
        RenderBackend_SelectByName("sdl");
    }

    /* 1. Early init - before anything else */
    Early_Init_Vars();

    /* COMPAT: Request 1ms timer resolution for accurate Sleep() and timeGetTime().
     * Original used DDraw exclusive fullscreen with vsync-locked flip chain (60Hz).
     * In windowed mode we use Sleep()-based frame limiting, which requires 1ms
     * resolution to hit 60fps targets. Without this, Windows default ~15.6ms
     * granularity causes Sleep(16) to actually sleep ~31ms → ~30fps. */
    timeBeginPeriod(1);

    /* 2. Create the platform window. The CMake/SDL build owns the window and
     * event queue; the transitional Makefile retains the recovered Win32 path. */
#ifdef TOU_HAS_SDL
    if (!Platform_CreateWindow(STR_TITLE, 640, 480)) {
        MessageBoxA(NULL, STR_ERR_RENDER_INIT, STR_TITLE, MB_ICONERROR);
        timeEndPeriod(1);
        return 0;
    }
    hWnd = (HWND)Platform_GetNativeWindowHandle();
    hWnd_Main = hWnd;
    if (hWnd != NULL) {
        HICON icon = LoadIconA(hInstance, (LPCSTR)0x7F00);
        SendMessageA(hWnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessageA(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
#else
    /* 2a. Register legacy window class */
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
#endif

    if (hWnd == NULL) {
#ifdef TOU_HAS_SDL
        Platform_DestroyWindow();
#endif
        timeEndPeriod(1);
        return 0;
    }

#ifndef TOU_HAS_SDL
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /* COMPAT: Hide system cursor for windowed mode.
     * Original ran exclusive fullscreen where DDraw auto-hid the cursor.
     * WndProc also sets SetCursor(NULL) on WM_SETCURSOR. */
    ShowCursor(FALSE);
#endif

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
#ifdef TOU_HAS_SDL
        Platform_DestroyWindow();
#endif
        timeEndPeriod(1);
        return 0;
    }
    Apply_Display_Settings();

    /* 5. Initialize the selected presentation backend. */
    LOG("[INIT] Initializing %s renderer...\n", RenderBackend_Name());
    iVar1 = RenderBackend_Initialize(hWnd);
    if (iVar1 == 0) {
        RenderBackend_Shutdown();
        MessageBoxA(hWnd, STR_ERR_RENDER_INIT, STR_TITLE, MB_ICONERROR);
#ifdef TOU_HAS_SDL
        Platform_DestroyWindow();
#else
        DestroyWindow(hWnd);
#endif
        timeEndPeriod(1);
        return 0;
    }

#ifdef TOU_HAS_SDL
    Platform_ShowWindow();
#endif

    /* 6. Initialize legacy input only for the DirectDraw comparison build. */
#ifndef TOU_HAS_SDL
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
#else
    {
#endif
MAIN_LOOP:
        LOG("[INIT] Entering MAIN_LOOP\n");
        g_MouseButtons = 0;

        /* Game_Update_Render (state 0) is the full gameplay loop (Phase 6).
         * Until it's decompiled, start at intro init instead. */
        GameState_Transition(GAME_STATE_INTRO_INIT);

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
#ifdef TOU_HAS_SDL
                PlatformEvent event;
                while (Platform_PollEvent(&event)) {
                    if (event.type == PLATFORM_EVENT_QUIT) {
                        Save_Options_Config();
                        Request_App_Quit();
                    } else if (event.type == PLATFORM_EVENT_FOCUS_GAINED) {
                        Handle_App_Focus(1);
                    } else if (event.type == PLATFORM_EVENT_FOCUS_LOST) {
                        Handle_App_Focus(0);
                    } else if (event.type == PLATFORM_EVENT_MOUSE_MOTION) {
                        Handle_Mouse_Motion(event.x, event.y);
                    }
                    if (g_QuitRequested || timeGetTime() >= msg_deadline)
                        break;
                }
                if (g_QuitRequested)
                    break;
#else
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
#endif
            }

            /* 2. Run one game tick */
            if (g_bIsActive == 0) {
                /* WM_ACTIVATEAPP performs the one-shot pause/mute work. Keep
                 * simulation and menu input frozen until Windows reactivates us. */
                Sleep(1);
            } else {
                /* App is active - run game */
#ifdef TOU_HAS_SDL
                Input_UpdateKeyboardState();
#endif
                /* COMPAT: Sync cursor from Windows position for windowed mode.
                 * Must run for ALL game states (menu can be at g_GameState 0 or 1).
                 * Overrides DirectInput relative deltas with absolute Win32 coords. */
                {
#ifdef TOU_HAS_SDL
                    int client_x;
                    int client_y;
                    if (Platform_GetMousePosition(&client_x, &client_y)) {
                        int game_x, game_y;
                        Client_To_Game_Coordinates(client_x, client_y, &game_x, &game_y);
                        g_MouseDeltaX = game_x << 18;
                        g_MouseDeltaY = game_y << 18;
                    }
#else
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
#endif
                }
                /* Original binary calls Input_Update() here for g_GameState==1
                 * to poll DirectInput mouse (accumulates DAT_004877e8 for slider drag).
                 * COMPAT: In windowed mode, all mouse input (position, buttons, slider
                 * delta) is handled via Win32 APIs in FUN_00425fe0's COMPAT block,
                 * making Input_Update redundant. Calling it would double-accumulate
                 * DAT_004877e8 since both DirectInput and Win32 cursor delta report
                 * the same physical mouse movement. */
                g_ProcessInput = 1;
                if (g_GameState == GAME_STATE_GAMEPLAY) {
                    Game_Update_Render();
                } else {
                    Game_State_Manager();
                }
            }
#ifdef TOU_HAS_SDL
            if (g_QuitRequested)
                break;
#endif
        }

#ifdef TOU_HAS_SDL
        Shutdown_Runtime();
        Platform_DestroyWindow();
#endif
        timeEndPeriod(1);
        return (int)msg.wParam;
    }

    timeEndPeriod(1);
    return 0;
}
