/*
 * gameloop.cpp - Game state machine, input update, menu state
 * Addresses: Game_State_Manager=00461260, Input_Update=00462560,
 *            Handle_Menu_State=004611D0, Game_Update_Render=00461710
 */
#include "tou.h"
#include <dinput.h>
#include <stdio.h>

/* ===== Globals defined in this module ===== */
char          g_MouseButtons = 0;     /* 004877BE */
unsigned char g_ProcessInput = 0;     /* 00489295 */
unsigned char g_SubState     = 0;     /* 00489296 */
unsigned char g_NeedsRedraw  = 0;     /* 00489297 */
unsigned char g_SurfaceReady = 0;     /* 00489298 */
unsigned char g_SubState2    = 0;     /* 00489299 */

int  g_MouseDeltaX = 0;              /* 004877B4 */
int  g_MouseDeltaY = 0;              /* 004877B8 */
char g_InputMode   = 0;              /* 004877E4 */
int  DAT_004877e8  = 0;              /* alt X accumulator */

/* ===== Input_Update (00462560) - Mouse polling via DirectInput ===== */
void Input_Update(void)
{
    DIDEVICEOBJECTDATA didod;
    DWORD dwElements;
    HRESULT hr;

    if (lpDI_Mouse == NULL)
        return;

    while (1) {
        dwElements = 1;
        hr = lpDI_Mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), &didod, &dwElements, 0);

        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            if (lpDI_Mouse != NULL) {
                if (g_bIsActive != 0) {
                    lpDI_Mouse->Acquire();
                } else {
                    lpDI_Mouse->Unacquire();
                }
            }
            return;
        }

        if (FAILED(hr))
            return;

        if (dwElements == 0)
            break;

        switch (didod.dwOfs) {
        case DIMOFS_X:                          /* 0x00 */
            if (g_InputMode == 1) {
                DAT_004877e8 += didod.dwData * 0x80;
            } else if (g_InputMode == 0) {
                g_MouseDeltaX += didod.dwData * 0x80000;
            }
            break;

        case DIMOFS_Y:                          /* 0x04 */
            if (g_InputMode == 0) {
                g_MouseDeltaY += didod.dwData * 0x80000;
            }
            break;

        case DIMOFS_BUTTON0:                    /* 0x0C */
            if ((didod.dwData & 0x80) == 0) {
                /* Button released */
                if (g_MouseButtons & 1) {
                    g_MouseButtons ^= 1;
                }
            } else {
                /* Button pressed */
                g_MouseButtons |= 1;
            }
            break;

        case DIMOFS_BUTTON1:                    /* 0x0D */
            if ((didod.dwData & 0x80) == 0) {
                if (g_MouseButtons & 2) {
                    g_MouseButtons ^= 2;
                }
            } else {
                g_MouseButtons |= 2;
            }
            break;
        }
    }
}

/* ===== Game_State_Manager (00461260) ===== */
void Game_State_Manager(void)
{
    int iVar2;

    switch (g_GameState & 0xFF) {
    case 0x01: /* Gameplay rendering */
        FUN_00425fe0();
        return;

    case 0x02: /* Return to menu */
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        Handle_Menu_State();
        return;

    case 0x03: /* Init gameplay - DirectDraw setup */
        if (DAT_00487640[1] != 5) {
            DAT_00487640[1] = 5;
            g_DisplayWidth  = 640; /* Mode 5 = 640x480 */
            g_DisplayHeight = 480;

            /* Release existing surfaces */
            if (lpDD != NULL) {
                if (lpDDS_Primary != NULL) {
                    lpDDS_Primary->Release();
                    lpDDS_Primary = NULL;
                }
                if (lpDDS_Offscreen != NULL) {
                    lpDDS_Offscreen->Release();
                    lpDDS_Offscreen = NULL;
                }
            }

            iVar2 = Init_DirectDraw(g_DisplayWidth, g_DisplayHeight);
            if (iVar2 == 0) {
                /* DDraw init failed - fatal */
                if (lpDD != NULL) {
                    Release_DirectDraw_Surfaces();
                    lpDD->Release();
                    lpDD = NULL;
                }
                MessageBoxA(hWnd_Main, STR_ERR_DDRAW_MODE, STR_TITLE, MB_ICONERROR);
                DestroyWindow(hWnd_Main);
                g_GameState = 0xFE;
                return;
            }
        }
        DAT_004877b1 = 1;
        g_FrameTimer = timeGetTime();

        /* Check if coming from intro or returning */
        if (DAT_004877a4 == 0x98) {
            DAT_004877a4 = 0;
        } else {
            FUN_0040e130();
            if (DAT_004877a4 == 0x98) {
                DAT_004877a4 = 0;
            }
        }

        /* Create game view surface */
        if (FUN_0042fc40()) {
            g_GameState = 0x01; /* Enter gameplay render */
        } else {
            g_GameState = 0xFE; /* Error -> shutdown */
        }
        FUN_00425fe0();
        return;

    case 0x04: /* Quick restart */
        FUN_0042fc10();
        FUN_0041a8c0();
        Handle_Menu_State();
        return;

    case 0x05: /* Game over - return to menu with level reload */
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        FUN_0041d740();
        g_GameState = 3;
        return;

    case 0x06: /* Error restart */
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        FUN_0041d740();
        DAT_004877a4 = 0;
        g_GameState = 3;
        return;

    case 0x96: /* Intro init */
        if (DAT_00487640[1] != 5) {
            DAT_00487640[1] = 5;
            g_DisplayHeight = 480;
            g_DisplayWidth  = 640;

            if (lpDD != NULL) {
                if (lpDDS_Primary != NULL) {
                    lpDDS_Primary->Release();
                    lpDDS_Primary = NULL;
                }
                if (lpDDS_Offscreen != NULL) {
                    lpDDS_Offscreen->Release();
                    lpDDS_Offscreen = NULL;
                }
            }

            iVar2 = Init_DirectDraw(g_DisplayWidth, g_DisplayHeight);
            if (iVar2 == 0) {
                if (lpDD != NULL) {
                    Release_DirectDraw_Surfaces();
                    lpDD->Release();
                    lpDD = NULL;
                }
                MessageBoxA(hWnd_Main, STR_ERR_DDRAW_MODE, STR_TITLE, MB_ICONERROR);
                DestroyWindow(hWnd_Main);
                g_GameState = 0xFE;
                return;
            }
        }
        DAT_004877b1 = 1;
        g_FrameTimer  = timeGetTime();
        DAT_004892b8  = timeGetTime();
        g_IntroSplashIndex = 0;
        g_GameState   = 0x97;
        DAT_004877a4  = 0x97;
        Load_Background_To_Buffer(1);
        FUN_0040e130();
        FUN_0045d7d0();
        /* Fall through to case 0x97 */

    case 0x97: /* Intro running */
        Intro_Sequence();
        return;

    case 0x98: /* New game */
        DAT_004877a4 = 0x98;
        g_GameState = 3;
        iVar2 = Init_New_Game();
        if (iVar2 != 1) {
            g_GameState = 0xFE;
        }
        return;

    case 0xFE: /* Shutdown */
        Cleanup_Sound();
        PostMessageA(hWnd_Main, WM_DESTROY, 0, 0);
        g_GameState = 0xFF;
        return;

    default:
        return;
    }
}

/* ===== Game_Update_Render (00461710) - Gameplay frame ===== */
/* TODO: Full implementation in Phase 6. Stub for now. */
void Game_Update_Render(void)
{
    /* Placeholder - will contain keyboard input processing,
     * game state updates, and rendering */
    if (g_GameState == 0) {
        /* Active gameplay - would call FUN_0045daa0() etc. */
    }
}

/* ===== Handle_Menu_State (004611D0) ===== */
/* SEH wrapper around Menu_Init_And_Loop.
 * Original wraps in __try/__except with handler at 00474000.
 * On success (return 1): g_GameState = 0 (GAMEPLAY)
 * On failure (return 0): g_GameState = 6 (ERROR_RESTART) */
void Handle_Menu_State(void)
{
    int result = Menu_Init_And_Loop();
    /* -(result != 1) & 6 → if result==1: 0, else: 6 */
    g_GameState = (result != 1) ? 6 : 0;
}

/* ===== Init_New_Game (004228A0) ===== */
/* TODO: Full implementation in Phase 6 */
int Init_New_Game(void)
{
    LOG("[STUB] Init_New_Game called\n");
    return 1;
}

/* ===== Free_Game_Resources (0040FFC0) ===== */
void Free_Game_Resources(void)
{
    /* TODO: Free per-level resources
     * Mem_Free(DAT_00481f50);
     * Mem_Free(DAT_00487814);
     * Mem_Free(DAT_00489ea4);
     * Mem_Free(DAT_00489ea8);
     * etc.
     */
}
