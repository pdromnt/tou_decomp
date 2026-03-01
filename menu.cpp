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
#include <string.h>

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
/* Loads level file, initializes entity arrays, and prepares the game world.
 * Returns 1 on success, 0 on failure.
 *
 * Phase 6a: Loads real .lev files (hardcoded to "minibase" for testing).
 * Heavy subsystems (ships, particles, AI, pathfinding) are still stubbed
 * pending further decompilation. */
int Load_Level_Resources(void)
{
    int result;

    LOG("[LEVEL] Load_Level_Resources\n");

    /* Set up flags for basic single-player local mode */
    DAT_0048396d = 0;      /* not a generated map */
    DAT_00483960 = 0;      /* no swap file */
    DAT_004892e4 = 0;      /* no random mirror */
    DAT_004892e5 = 0;      /* no difficulty override */
    DAT_0048764a = 0;      /* not network mode */
    DAT_00489d7c[0] = '\0'; /* clear error buffer */

    /* Phase 6a: Hardcode "minibase" for initial testing.
     * TODO: Read level name from config (DAT_00486938 / level list). */
    result = Load_Level_File("minibase");
    if (result == 0) {
        LOG("[LEVEL] Load_Level_File FAILED: %s\n", DAT_00489d7c);
        return 0;
    }

    /* FUN_0041b010() - Ship/player init: STUB
     * Initializes player structures at DAT_00487810, sets random angles,
     * assigns ship types. Needs FUN_0041b5d0 (ship finalize). */
    LOG("[LEVEL] STUB: FUN_0041b010 (ship init) skipped\n");

    /* FUN_004249c0() - Ship sprite loading: STUB
     * Loads ship model sprites into the sprite table. Returns 1 on success.
     * Without this, ship rendering won't work but level data is valid. */
    LOG("[LEVEL] STUB: FUN_004249c0 (ship sprites) skipped\n");

    /* Clear extended entity state (matches original loop) */
    if (DAT_004892e8) {
        for (int i = 0; i < 0x51400; i += 0x80) {
            *(int *)((char *)DAT_004892e8 + i + 0x34) = 0;
        }
    }

    /* FUN_0041a370() - Player stat scaling: STUB */
    /* FUN_0041bfe0() - Particle/object spawning: STUB (huge ~3000 line fn) */
    /* FUN_0041d2e0() - Edge detection: STUB */
    /* FUN_0041aea0() - Player pathfinding: STUB */
    /* FUN_0041bed0() - Difficulty constants: STUB */
    /* FUN_00451500() - Team initialization: STUB */
    /* FUN_00449040('\x01') - Visibility map generation: STUB */

    /* Allocate large game state buffer if needed (original does this) */
    if (!DAT_00487aa4) {
        DAT_00487aa4 = Mem_Alloc(0x14000);  /* 4 * 0x4000 + overhead */
        if (DAT_00487aa4) {
            g_MemoryTracker += 0x14000;
        }
    }

    /* Clear game state buffer (matches original 0x10000 byte clear) */
    if (DAT_00487aa4) {
        memset(DAT_00487aa4, 0, 0x10000);
    }

    g_SurfaceReady = 2;

    LOG("[LEVEL] Load_Level_Resources complete: %ux%u map, stride=%d, %d entities\n",
        DAT_004879f0, DAT_004879f4, DAT_00487a00, DAT_00489278);
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
