/*
 * gameloop.cpp - Game state machine, input update, menu state
 * Addresses: Game_State_Manager=00461260, Input_Update=00462560,
 *            Handle_Menu_State=004611D0, Game_Update_Render=00461710
 */
#include "tou.h"
#include <dinput.h>
#include <stdio.h>
#include <string.h>

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

        /* Music: If coming from state 0x98 (new game after intro),
         * skip FUN_0040e130 since music is already playing from intro.
         * Otherwise call FUN_0040e130 to start menu/gameplay music. */
        if (DAT_004877a4 == 0x98) {
            DAT_004877a4 = 0;
        } else {
            FUN_0040e130();
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

    case 0x98: /* New game - reload sprites, reset palette, init session */
        DAT_004877a4 = 0x98;
        g_GameState = 3;
        /* Reset team palette[3] from gold (0x7FF0) back to gray (0x6739).
         * It gets set to gold in FUN_0042d8b0 before sprites load;
         * starting a new game resets it to the default gray team color. */
        DAT_00483838[3] = 0x6739;
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

/* ===== Gameplay_Tick (0045DAA0) ===== */
/* Fixed-timestep game simulation update.
 * Called from Game_Update_Render when g_SubState == 0 (active gameplay).
 * TODO: Full implementation with all ~20 subsystems. */
static void Gameplay_Tick(void)
{
    /* Fixed timestep: 1000 / DAT_00483746 ms per tick, capped at 9 catch-up ticks.
     * For now, just advance the frame timer to keep time flowing. */
    DWORD now = timeGetTime();
    DAT_004877f0 = now - g_FrameTimer;
    g_FrameTimer = now;
    if (DAT_004877f0 > 1000) {
        DAT_004877f0 = 1000;
    }

    /* Increment tick counter */
    g_TimerAux++;

    /* TODO Phase 6: All gameplay subsystems:
     * FUN_00460d50() - input/control update
     * FUN_004609e0() - physics/movement
     * FUN_00460660() - AI (every other tick)
     * FUN_00460ac0() - collision
     * FUN_00413720() - entity update
     * FUN_00454340() - projectile update
     * FUN_0044b0b0() - particle/effects
     * FUN_00434310() - world/terrain
     * FUN_004527e0() - sound update
     * Entity rotation/timer loop
     * FUN_00454b00() - weapon/item
     * FUN_00458010() - camera/viewport
     * FUN_00453cd0() - spawn/respawn
     * FUN_00455d50() - score/objective
     * FUN_004571f0(), FUN_00453a80() - UI/HUD
     * FUN_004573e0() - status effects
     * FUN_0045fc00(), FUN_0045e2c0() - network/sync
     * FUN_00449040() - damage/health
     */
}

/* ===== Game_Update_Render (00461710) - Gameplay frame ===== */
/* Main gameplay loop: keyboard input, game state updates, rendering.
 * Called from WinMain when g_GameState == 0.
 *
 * Original: reads DirectInput keyboard, processes game keys (ESC, Enter,
 * configurable bindings), runs Gameplay_Tick when g_SubState == 0,
 * handles state transitions (pause, round end, game over),
 * then renders via FUN_00407720 → DDraw blit/flip.
 *
 * g_SubState values in gameplay:
 *   0 = active play (runs Gameplay_Tick)
 *   1 = paused
 *   2 = confirm exit
 *   3 = post-round transition
 *   4 = menu/level preview (waiting for Enter)
 *   100 = round end
 *   101 = game over */
void Game_Update_Render(void)
{
    /* ---- Read keyboard via DirectInput ---- */
    if (g_ProcessInput != 0 && lpDI_Keyboard != NULL) {
        HRESULT hr = lpDI_Keyboard->GetDeviceState(256, g_KeyboardState);
        if (FAILED(hr)) {
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                lpDI_Keyboard->Acquire();
            }
            memset(g_KeyboardState, 0, 256);
        }
    }

    /* ---- COMPAT: Sync cursor from Windows position ---- */
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd_Main, &pt);
        g_MouseDeltaX = pt.x << 18;
        g_MouseDeltaY = pt.y << 18;
    }

    /* ---- Input processing ---- */
    if (g_ProcessInput != 0) {
        /* ESC key (scan 0x01) */
        static unsigned char esc_prev = 0;
        if ((g_KeyboardState[0x01] & 0x80) && !esc_prev) {
            if (g_SubState == 0 || g_SubState == 4) {
                /* Active gameplay or level preview: end round → return to menu.
                 * Original: ESC sets g_SubState=100 which accumulates scores
                 * then transitions to g_GameState=2 (RETURN_TO_MENU).
                 * Simplified: go directly to state 5 (GAME_OVER) which
                 * routes through state 3 → state 1 (back to main menu). */
                LOG("[GAME] ESC pressed (substate=%d) → state 5 (GAME_OVER)\n", g_SubState);
                g_GameState = 5;
                return;
            }
        }
        esc_prev = (g_KeyboardState[0x01] & 0x80) ? 1 : 0;

        /* Enter key (scan 0x1C) - start gameplay from level preview */
        static unsigned char enter_prev = 0;
        if ((g_KeyboardState[0x1C] & 0x80) && !enter_prev) {
            if (g_SubState == 4) {
                /* Level preview → start active gameplay */
                LOG("[GAME] Enter pressed → starting gameplay (substate 4→0)\n");
                g_SubState = 0;
                g_TimerStart = timeGetTime();
                g_TimerAux = 0;
                g_FrameTimer = timeGetTime();
            }
        }
        enter_prev = (g_KeyboardState[0x1C] & 0x80) ? 1 : 0;

        /* F12 (scan 0x58): immediate exit */
        if (g_KeyboardState[0x58] & 0x80) {
            g_GameState = 0xFE;
            return;
        }
    }

    /* ---- Game logic update ---- */
    switch (g_SubState) {
    case 0:   /* Active gameplay */
        Gameplay_Tick();
        break;

    case 4:   /* Level preview / waiting for Enter */
        /* Update timing but don't run simulation */
        {
            DWORD now = timeGetTime();
            DAT_004877f0 = now - g_FrameTimer;
            g_FrameTimer = now;
        }
        break;

    default:
        /* Other states (paused, round end, etc.) - update timing */
        {
            DWORD now = timeGetTime();
            DAT_004877f0 = now - g_FrameTimer;
            g_FrameTimer = now;
        }
        break;
    }

    /* ---- Rendering ---- */
    /* Original: locks DDraw surface, calls FUN_00407720() (game world renderer),
     * blits to back buffer, flips.
     * COMPAT: Render_Frame() copies Software_Buffer to DDraw surface.
     * Since FUN_00407720 (game world renderer) is not yet implemented,
     * Software_Buffer still contains the last menu background/content. */
    Render_Frame();

    /* ---- Frame rate limiter (~60fps) ---- */
    {
        static DWORD lastFrame = 0;
        DWORD end = timeGetTime();
        if (lastFrame != 0) {
            DWORD elapsed = end - lastFrame;
            if (elapsed < 16) Sleep(16 - elapsed);
        }
        lastFrame = timeGetTime();
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
/* Clears sprite arrays, reinitializes math tables, and loads sprites.
 * Called from Game_State_Manager case 0x98 (NEW_GAME).
 * Returns 1 on success, 0 on failure. */
int Init_New_Game(void)
{
    LOG("[INIT] Init_New_Game\n");

    /* Original clears sprite pixel cursors + all 20000 entries in
     * frame offset / width / height tables, then calls FUN_00423150()
     * to reload sprites from disk. Since we don't have the sprite
     * file loader yet, SKIP the clearing — sprites loaded during
     * System_Init_Check remain valid and must be preserved.
     *
     * TODO: When FUN_00423150 (sprite file loader) is implemented:
     *   DAT_00481d28 = 0;  DAT_00481d24 = 0;
     *   memset(DAT_00489234, 0, 20000 * 4);
     *   memset(DAT_00489e8c, 0, 20000);
     *   memset(DAT_00489e88, 0, 20000);
     *   Init_Math_Tables((int *)DAT_00487ab0, 0x800);
     *   FUN_00423150();
     */

    /* Reinitialize sin/cos lookup tables (safe — doesn't destroy sprites) */
    if (DAT_00487ab0) {
        Init_Math_Tables((int *)DAT_00487ab0, 0x800);
    }

    LOG("[INIT] Init_New_Game - math tables reinit (sprites preserved)\n");

    return 1;
}

/* ===== Free_Game_Resources (0040FFC0) ===== */
/* Frees per-level resources allocated during Load_Level_Resources / Load_Level_File.
 * Called from Game_State_Manager on return-to-menu, game-over, and error-restart.
 * The original binary tail-calls FUN_0041bad0 via JMP; here we call normally. */
void Free_Game_Resources(void)
{
    Mem_Free(DAT_00481f50);    /* background RGB565 pixels */
    Mem_Free(DAT_00487814);    /* coarse grid buffer */
    Mem_Free(DAT_00489ea4);    /* shadow grid 1 */
    Mem_Free(DAT_00489ea8);    /* shadow grid 2 */

    if (DAT_00483960 == '\x01') {
        Mem_Free(DAT_00489ea0);    /* swap/heightmap (only if swap-file enabled) */
    }

    if (DAT_00487820 != NULL) {
        Mem_Free(DAT_00487820);    /* edge/boundary navigation data */
    }
    DAT_00487820 = NULL;

    FUN_0041bad0();    /* free per-player visibility buffers */
}
