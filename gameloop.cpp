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
DWORD        DAT_00489ee8 = 0;       /* Key repeat cooldown timestamp */
unsigned int DAT_00489eec = 0;       /* Last pressed key scan code */

/* Gameplay tick timing and counters */
short DAT_00483746 = 60;             /* tick rate: ticks per second (default 60 → ~16.7ms/tick) */
char  DAT_00489288 = 0;              /* sub-frame counter (0-7, wraps) */
char  DAT_0048373e = 0;              /* activation guard flag */

/* Pause menu state (unused — original binary has no visible pause menu selection) */

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

        /* COMPAT: Ensure window has foreground/focus after DDraw operations.
         * Original ran DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN which auto-maintained
         * foreground. In windowed DDSCL_NORMAL mode, DDraw surface creation
         * (FUN_0042fc40) can cause the window to lose foreground, making
         * DISCL_FOREGROUND DirectInput devices lose acquisition. Force the
         * window to foreground so keyboard/mouse work immediately. */
        SetForegroundWindow(hWnd_Main);
        SetFocus(hWnd_Main);

        if (lpDI_Mouse != NULL) {
            lpDI_Mouse->Acquire();
        }
        if (lpDI_Keyboard != NULL) {
            lpDI_Keyboard->Acquire();
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

/* ===== FUN_0045e1f0 — Pre-tick entity flag reset ===== */
/* Clears per-tick flags across 4 entity arrays before each simulation step. */
void FUN_0045e1f0(void)
{
    int i;

    /* Main entities (DAT_004892e8, stride 0x80): clear damage flag if health < 30000 */
    for (i = 0; i < DAT_00489248; i++) {
        int off = i * 0x80;
        int base = (int)DAT_004892e8;
        if (*(unsigned int *)(base + off + 0x4C) < 30000 &&
            *(short *)(base + off + 0x24) == 1) {
            *(short *)(base + off + 0x24) = 0;
        }
    }

    /* Troopers (DAT_00487884, stride 0x40): clear hit flag */
    for (i = 0; i < DAT_0048924c; i++) {
        *(char *)(DAT_00487884 + i * 0x40 + 0x2C) = 0;
    }

    /* Projectiles (DAT_00481f28, stride 0x40): clear update flag */
    for (i = 0; i < DAT_00489260; i++) {
        *(char *)(DAT_00481f28 + i * 0x40 + 0x1E) = 0;
    }

    /* Players (DAT_00487810, stride 0x598): clear per-tick flags */
    for (i = 0; i < DAT_00489240; i++) {
        int off = i * 0x598;
        *(char *)(DAT_00487810 + off + 0xA1) = 0;
        *(char *)(DAT_00487810 + off + 0xA3) = 0;
    }
}

/* ===== Gameplay_Tick (0045DAA0) ===== */
/* Fixed-timestep game simulation loop.
 * Called from Game_Update_Render when g_SubState == 0 (active gameplay).
 * Implements the full framework from the original with all subsystem calls.
 * Individual subsystems are stubbed pending decompilation. */
static void Gameplay_Tick(void)
{
    unsigned int tick_interval;
    DWORD now;
    int catch_up;
    int tick;

    if (DAT_00483746 < 1) DAT_00483746 = 60;
    tick_interval = (unsigned int)(1000 / DAT_00483746);

    /* Pre-tick setup: reset per-tick entity flags */
    FUN_0045e1f0();

    /* Wait until at least one tick interval has elapsed.
     * Original used a pure busy-wait (100% CPU). We use Sleep(0) which
     * yields the current time-slice but returns as soon as the thread
     * can run again — near-microsecond precision with timeBeginPeriod(1)
     * active, without burning 100% CPU. Sleep(1) was too coarse even
     * with 1ms timer resolution, causing ~1-2ms timing overshoot per
     * frame that accumulated into uneven frame spacing. */
    now = timeGetTime();
    while ((now - g_TimerAux * tick_interval) - g_TimerStart < tick_interval) {
        Sleep(0);
        now = timeGetTime();
    }

    /* Calculate how many ticks to catch up (max 9) */
    now = timeGetTime();
    catch_up = (int)((now - g_TimerAux * tick_interval - g_TimerStart) / tick_interval);
    if (catch_up > 9) {
        g_TimerStart += (catch_up - 9) * tick_interval;
        catch_up = 9;
    }

    /* Debug: log first few ticks */
    {
        static int dbg_frame = 0;
        if (dbg_frame < 3) {
            LOG("TICK: frame=%d catch_up=%d f2c=%d f34=%d\n",
                dbg_frame, catch_up, DAT_0048925c, DAT_00489250);
        }
        dbg_frame++;
    }

    /* Execute each tick */
    for (tick = 0; tick < catch_up; tick++) {
        g_TimerAux++;

        /* Sub-frame counter: 0→1→2→...→7→0 */
        DAT_00489288++;
        if (DAT_00489288 >= 8) DAT_00489288 = 0;

        /* Activation pair logic */
        if (DAT_004892a5 != 0) {
            DAT_004892a5++;
        }
        if (DAT_004892a5 == 0 && DAT_004892a4 != 0 && DAT_0048373e == 0) {
            DAT_004892a5 = 1;
        }

        /* ---- Subsystem calls ---- */
        FUN_00460d50();                          /* 1-RoundTimer */
        FUN_004609e0();                          /* 2-SpatialGrid */
        if ((DAT_00489288 & 1) == 0) {
            FUN_00460660();                      /* 3-CollisionBitmap (half-rate) */
        }
        FUN_00460ac0();                          /* 4-RelocateEdge */
        FUN_00413720();                          /* 5-Spawner */
        FUN_00454340();                          /* 6-Emitters */
        FUN_0044b0b0();                          /* 7-EntityBehavior */
        FUN_00434310();                          /* 8-DebrisAnim */
        FUN_004527e0();                          /* 9-Projectiles */

        /* Inline: effect/particle rotation and timer decrement */
        {
            int i;
            for (i = 0; i < DAT_00489264; i++) {
                int base = (int)DAT_00487780 + i * 0x20;
                *(unsigned int *)(base + 0x10) = (*(unsigned int *)(base + 0x10) + 0x10) & 0x7FF;
                if (*(int *)(base + 0x08) > 0) (*(int *)(base + 0x08))--;
                if (*(int *)(base + 0x0C) > 0) (*(int *)(base + 0x0C))--;
            }
        }

        FUN_00454b00();                          /* 11-Turrets */
        FUN_00458010();                          /* 12-TurretLOS */
        FUN_00453cd0();                          /* 13-ParticlePhys */
        FUN_00455d50();                          /* 14-BulletCollide */
        FUN_004571f0();                          /* 15-Explosion */
        FUN_00453a80();                          /* 16-ItemAI */
        FUN_004573e0();                          /* 17-TrapDoor */

        /* Conditional: turret sound */
        if (DAT_00483834 != 0) {
            FUN_004133d0('\0');                   /* 18-TurretBehavior */
        }

        /* Conditional: trooper-related + round-end check */
        if (DAT_00483835 != 0) {
            if ((DAT_00489288 & 1) == 0) {
                FUN_004533d0();                  /* 19-Elevators */
            }
            if (DAT_00489288 == 0) {
                /* Every 8th tick: round-end check causes early return */
                FUN_00453230();                  /* 20-WaypointCheck */
                return;
            }
        }
        FUN_0045fc00();                          /* 21-FluidSpread */
        FUN_0045e2c0();                          /* 22-Deaths */

        /* Inline: health clamping for specific game modes */
        if (DAT_004892a8 == 1) {
            char mode_byte = *((char *)&DAT_00483740 + 1);
            if (mode_byte == 2 || mode_byte == 4) {
                int p;
                for (p = 0; p < DAT_00489240; p++) {
                    int *health = (int *)(DAT_00487810 + 0x20 + p * 0x598);
                    if (*health > 0x1000) *health = 0x1000;
                }
            }
        }

        /* Incremental visibility map update (200 cells per tick) */
        FUN_00449040('\0');

        /* Inline: trooper tile validation */
        {
            int i;
            for (i = 0; i < DAT_0048924c; i++) {
                int off = i * 0x40;
                int base = (int)DAT_00487884;
                *(char *)(base + off + 0x2C) = 0;

                /* Check tile at trooper position */
                int tx = *(int *)(base + off) >> 0x12;
                int ty = *(int *)(base + off + 8) >> 0x12;
                int tile_idx = *(unsigned char *)((int)DAT_0048782c +
                    (ty << (DAT_00487a18 & 0x1f)) + tx);
                if (*(char *)((int)DAT_00487928 + tile_idx * 0x20 + 1) == '\x01') {
                    *(char *)(base + off + 0x24) = 0;
                } else {
                    char stale = *(char *)(base + off + 0x24);
                    stale++;
                    if (stale >= 6) stale = 0;
                    *(char *)(base + off + 0x24) = stale;
                }
            }
        }
    }  /* end tick loop */
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

    /* NOTE: Cursor sync (GetCursorPos/ScreenToClient) moved to WinMain main loop
     * so it runs for ALL game states, not just g_GameState==0. */

    /* ---- Input processing ---- */
    /* Key debounce: original uses DAT_00489ee8 (cooldown timestamp) +
     * DAT_00489eec (last key scan code). Cooldown clears when the
     * previously pressed key is released, allowing immediate response
     * to a different key. */
    if (g_ProcessInput != 0) {
        /* Clear cooldown when the previously-tracked key is released */
        if ((g_KeyboardState[DAT_00489eec] & 0x80) == 0) {
            DAT_00489ee8 = 0;
        }
        DWORD now_input = timeGetTime();

        /* State 4: Level preview — F10 exits, Enter starts gameplay.
         * Original checks at 0x004617FF. ESC is NOT handled in state 4. */
        if (g_SubState == 4) {
            if ((g_KeyboardState[0x44] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                g_SubState = 100;
                *(unsigned char *)&DAT_0048693c = g_ConfigBlob[0];
            }
            if ((g_KeyboardState[0x1C] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = 0x1C;
                g_SubState = 0;
                g_NeedsRedraw = 2;
                g_TimerStart = timeGetTime();
                g_TimerAux = 0;
                g_FrameTimer = timeGetTime();
            }

            /* COMPAT: Auto-start gameplay (level preview camera not implemented) */
            g_SubState = 0;
            g_TimerStart = timeGetTime();
            g_TimerAux = 0;
            g_FrameTimer = timeGetTime();
        }

        /* State 2: ESC pause menu — F10/Enter/ESC.
         * Sprite 0x37 panel: "F10 Exit to menu / Enter Next level / Esc Back to the game" */
        if (g_SubState == 2) {
            /* F10 (scan 0x44): exit to menu */
            if ((g_KeyboardState[0x44] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                *(unsigned char *)&DAT_0048693c = g_ConfigBlob[0];
                g_SubState = 100;
                DAT_004892a5 = 0;
            }
            /* Enter (scan 0x1C): next level */
            if ((g_KeyboardState[0x1C] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = 0x1C;
                g_SubState = 100;
                DAT_004892a5 = 0;
                g_NeedsRedraw = 1;
            }
        }

        /* P key (configurable) — toggle pause.
         * Only when g_SubState < 2 (not during ESC-menu or transitions). */
        if (g_SubState < 2) {
            unsigned char pause_key = DAT_004837ba;
            if ((g_KeyboardState[pause_key] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                g_SubState = (g_SubState == 0) ? 1 : 0;
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = (unsigned int)pause_key;
                g_NeedsRedraw = 1;
                g_TimerStart = timeGetTime();
                g_TimerAux = 0;
                g_FrameTimer = timeGetTime();
            }
        }

        /* ESC (scan 0x01) — toggle between active (0) and ESC menu (2).
         * Excluded from states 3 and 4. */
        if (g_SubState != 3 && g_SubState != 4) {
            if ((g_KeyboardState[0x01] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                g_SubState = -(g_SubState != 2) & 2;
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = 0x01;
                g_NeedsRedraw = 1;
                g_TimerStart = timeGetTime();
                g_TimerAux = 0;
                g_FrameTimer = timeGetTime();
            }
        }

        /* F12 (scan 0x58): immediate exit */
        if (g_KeyboardState[0x58] & 0x80) {
            g_GameState = 0xFE;
            return;
        }
    }

    /* ---- Round-end state machine (substates 100/101 → 3) ---- */
    /* Original at 0x00461a10 / 0x00461a2d in Game_Update_Render.
     * Substate 100 = skip level (Enter), substate 101 = natural round end.
     * Both increment DAT_0048693c (level slot index) and transition to 3.
     * Substate 3 checks if all rounds are done → menu reload or game over. */
    if (g_SubState == 100 || g_SubState == 101) {
        DAT_00487640[0] = 0;
        DAT_004892a5 = 0;

        /* Advance to next level slot */
        g_SubState = 3;
        (*(unsigned char *)&DAT_0048693c)++;

        LOG("[GAME] Round end: level slot now %d / %d rounds\n",
            (int)(unsigned char)DAT_0048693c, (int)g_ConfigBlob[0]);
    }

    if (g_SubState == 3) {
        g_SubState = 0;
        if ((unsigned char)DAT_0048693c >= g_ConfigBlob[0]) {
            /* All rounds completed → game over / scoreboard path */
            g_GameState = 5;
        } else {
            /* More rounds to play → reload menu with next level */
            g_GameState = 2;
        }
        return;
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
    Render_Frame();

    /* NOTE: No additional frame rate limiter here.
     * The original FUN_00461710 has NO separate frame limiter —
     * Gameplay_Tick (FUN_0045daa0) already regulates frame pacing
     * via its built-in busy-wait timing loop. Adding a second
     * Sleep-based limiter here caused timing interference that
     * produced uneven frame spacing (jittery motion). */
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
    DAT_00481f50 = NULL;
    DAT_0048782c = NULL;       /* tilemap is part of DAT_00481f50 combined alloc */

    Mem_Free(DAT_00487814);    /* coarse grid buffer */
    DAT_00487814 = NULL;

    Mem_Free(DAT_00489ea4);    /* shadow grid 1 */
    DAT_00489ea4 = NULL;

    Mem_Free(DAT_00489ea8);    /* shadow grid 2 */
    DAT_00489ea8 = NULL;

    if (DAT_00483960 == '\x01') {
        Mem_Free(DAT_00489ea0);    /* swap/heightmap (only if swap-file enabled) */
        DAT_00489ea0 = NULL;
    }

    if (DAT_00487820 != NULL) {
        Mem_Free(DAT_00487820);    /* edge/boundary navigation data */
    }
    DAT_00487820 = NULL;

    FUN_0041bad0();    /* free per-player visibility buffers */

    /* Reset all gameplay entity/subsystem counters so they don't carry over
     * to the next level or persist in the menu (e.g. fluid bubbles). */
    DAT_00489248 = 0;   /* emitter/complex particle count */
    DAT_00489250 = 0;   /* fire particle count */
    DAT_00489258 = 0;   /* fluid source count */
    DAT_00489268 = 0;   /* bullet count */
    DAT_0048926c = 0;   /* item/pickup count */
    DAT_00489270 = 0;   /* trap/door count */
    DAT_00489274 = 0;   /* turret/static entity count */
    DAT_004892d8 = 0;   /* spawner/emitter def count */
    DAT_0048924c = 0;   /* trooper count */
    DAT_00489254 = 0;   /* edge entity count */
    DAT_004892a8 = 0;   /* round timer */
}
