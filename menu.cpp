/*
 * menu.cpp - Menu system, level resource loading
 * Addresses: Menu_Init_And_Loop=0045D950, Load_Level_Resources=0041A020
 *
 * Handle_Menu_State (004611D0) is in gameloop.cpp (thin SEH wrapper).
 * Menu_Init_And_Loop initializes resources and enters menu sub-state.
 * The actual menu rendering is driven by FUN_00425fe0 in gameplay state.
 */
#include "tou.h"
#include <stdio.h>

/* ===== Menu_Init_And_Loop (0045D950) ===== */
/* Initializes sound, switches resolution if needed, loads level resources,
 * and sets up the menu sub-state. Returns 1 on success, 0 on failure.
 *
 * Despite the name, this is NOT a loop — it's an init function.
 * The "loop" is the main message loop in WinMain, with Game_State_Manager
 * dispatching to FUN_00425fe0 when g_GameState=0/1 and g_SubState=4 (MENU). */
int Menu_Init_And_Loop(void)
{
    int iVar1;

    DAT_00487824 = 0;
    FUN_0040e130();  /* Init/start menu music */

    /* Resolution switch: if current mode != desired mode, try to change */
    if (DAT_00487640[1] != DAT_00487640[2]) {
        DAT_00483724[1] = DAT_00487640[2];
        DAT_00487640[1] = DAT_00487640[2];
        g_DisplayWidth  = *(int *)((char *)&g_NumDisplayModes + 4 + (unsigned int)DAT_00487640[2] * 4);
        g_DisplayHeight = *(int *)((char *)&g_NumDisplayModes + 4 + 0x40 + (unsigned int)DAT_00487640[2] * 4);

        Release_DirectDraw_Surfaces();
        iVar1 = Init_DirectDraw(g_DisplayWidth, g_DisplayHeight);

        /* If DDraw init fails, cycle through modes until one works */
        while (iVar1 == 0) {
            Release_DirectDraw_Surfaces();
            DAT_00487640[2] = DAT_00487640[2] + 1;
            if (g_NumDisplayModes <= (int)(unsigned int)DAT_00487640[2]) {
                DAT_00487640[2] = 0;
            }
            g_DisplayWidth  = *(int *)((char *)&g_NumDisplayModes + 4 + (unsigned int)DAT_00487640[2] * 4);
            g_DisplayHeight = *(int *)((char *)&g_NumDisplayModes + 4 + 0x40 + (unsigned int)DAT_00487640[2] * 4);
            DAT_00483724[1] = DAT_00487640[2];
            DAT_00487640[1] = DAT_00487640[2];
            Release_DirectDraw_Surfaces();
            iVar1 = Init_DirectDraw(g_DisplayWidth, g_DisplayHeight);
        }
    }

    /* Load level data and resources */
    iVar1 = Load_Level_Resources();
    if (iVar1 != 0) {
        DAT_00489e9c = 0;
        DAT_00487784 = 0;

        /* Place turrets if flag set */
        if (DAT_00483834 != 0) {
            FUN_004102b0();
        }

        /* Place troopers if flag set */
        if (DAT_00483835 != 0) {
            FUN_0041bc50();
        }

        /* Set up menu display sub-state */
        g_SubState = 0;
        if (g_SubState2 == 0) {
            g_SubState   = 4;   /* MENU_DISPLAY */
            g_NeedsRedraw = 1;
        }

        g_TimerStart = timeGetTime();
        g_TimerAux   = 0;
        return 1;
    }

    return 0;
}

/* ===== Load_Level_Resources (0041A020) ===== */
/* Loads level file (or generates random level), loads ships, initializes
 * entity arrays, and prepares the game world for play.
 * Returns 1 on success, 0 on failure.
 *
 * TODO: Full decompilation in Phase 6. For now, returns 1 (stub success)
 * to allow the menu state machine to function. Without real level data,
 * the game view will be empty but the state transitions will work. */
int Load_Level_Resources(void)
{
    LOG("[STUB] Load_Level_Resources - returning success\n");
    g_SurfaceReady = 2;
    return 1;
}

/* ===== FUN_004102b0 - Turret placement ===== */
/* Scans level tilemap for turret positions and places them.
 * TODO: Full decompilation in Phase 6. */
void FUN_004102b0(void)
{
    /* stub */
}

/* ===== FUN_0041bc50 - Trooper placement ===== */
/* Scans level tilemap for trooper spawn positions.
 * TODO: Full decompilation in Phase 6. */
void FUN_0041bc50(void)
{
    /* stub */
}
