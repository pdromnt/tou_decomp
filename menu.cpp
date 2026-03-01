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
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

    /* Set up player count for single-player testing */
    if (DAT_00489240 == 0) {
        DAT_00489240 = 1;   /* 1 player */
        DAT_00489244 = 1;   /* 1 human */
    }

    /* Allocate player data array (0x598 bytes per player) */
    if (DAT_00487810 == 0) {
        void *player_mem = Mem_Alloc(DAT_00489240 * 0x598);
        if (player_mem) {
            DAT_00487810 = (int)player_mem;
            g_MemoryTracker += DAT_00489240 * 0x598;
            LOG("[LEVEL] Allocated player data: %d players, %d bytes\n",
                DAT_00489240, DAT_00489240 * 0x598);
        }
    }

    /* Allocate ship stats table (0x40 bytes per ship type, 9 types) */
    if (!DAT_0048780c) {
        DAT_0048780c = Mem_Alloc(9 * 0x40);
        if (DAT_0048780c) {
            g_MemoryTracker += 9 * 0x40;
        }
    }

    /* Set default ship type for player 0 (type 9 = random) */
    DAT_0048236e[0] = 9;

    /* Mark player 0 as human */
    if (DAT_00487810 != 0) {
        *(char *)(DAT_00487810 + 0x480) = 1;
    }

    /* FUN_0041b010() - Ship/player init */
    FUN_0041b010();
    LOG("[LEVEL] FUN_0041b010 (ship init) complete\n");

    /* FUN_004249c0() - Ship sprite loading */
    result = FUN_004249c0();
    if (result == 0) {
        LOG("[LEVEL] WARNING: FUN_004249c0 (ship sprites) failed - continuing without ships\n");
    } else {
        LOG("[LEVEL] FUN_004249c0 (ship sprites) loaded OK\n");
    }

    /* Clear extended entity state (matches original loop) */
    if (DAT_004892e8) {
        for (int i = 0; i < 0x51400; i += 0x80) {
            *(int *)((char *)DAT_004892e8 + i + 0x34) = 0;
        }
    }

    /* FUN_0041a370() - Player stat scaling: STUB */

    /* Allocate wall segment array (not in Init_Memory_Pools) */
    if (!DAT_00489e80) {
        DAT_00489e80 = Mem_Alloc(16 * 0x20);
        if (DAT_00489e80) {
            g_MemoryTracker += 16 * 0x20;
        }
    }

    /* Entity spawning - populates all entity arrays from level data */
    FUN_0041bfe0();
    LOG("[LEVEL] Entity spawning: %d entities, %d troopers, %d projectiles, %d debris, %d decor, %d spawns\n",
        DAT_00489248, DAT_0048924c, DAT_00489260, DAT_00489268, DAT_004892d8, DAT_004892d4);

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

/* ===== Globals for ship/player system ===== */
int           DAT_004877f8[4] = {0};   /* active player index table */
char          DAT_00483738 = 0;        /* game mode */
short         DAT_0048373a = 3;        /* initial lives (default 3) */
int           DAT_00483830 = 100;      /* starting health */
void         *DAT_0048780c = NULL;     /* ship stats table */
unsigned char DAT_0048236e[80] = {0};  /* ship type per player */
char          DAT_004836ce[80] = {0};  /* player config ship IDs */
char          DAT_0048378e[9] = {0};   /* ship-taken flags */
void         *DAT_00489eac[4] = {0};   /* per-player visibility buffers */
int           DAT_00487788[4] = {0};   /* per-player stat counters */

/* Color transform globals (used by FUN_00424240) */
int DAT_00481d0c = 0x80;
int DAT_00481d10 = 0x80;
int DAT_00481d14 = 0x80;
int DAT_00481d18 = 0xFF;
int DAT_00481d1c = 0xFF;
int DAT_00481d20 = 0xFF;
int DAT_00481d2c = 0xA0;
int DAT_00481d30 = 0xA0;
int DAT_00481d34 = 0xA0;

/* ===== Entity spawning globals ===== */
char          DAT_00483737 = 0;       /* trooper difficulty (0=none,1-3=density) */
char          DAT_00483736 = 0;       /* debris difficulty */
char          DAT_00483735 = 0;       /* team base placement mode */
char          DAT_0048373c = 0;       /* team mode flag */
unsigned char DAT_00483962 = 0;       /* team base probability % */
unsigned char DAT_00483754[4] = {0, 0, 1, 1}; /* entity enable flags [2]=walls, [3]=projectiles */

int           DAT_00489270 = 0;       /* wall segment count */
int           DAT_004892d4 = 0;       /* spawn point count */
int           DAT_004892d8 = 0;       /* decoration count */
int           DAT_004892dc = 0;       /* misc counter */
int           DAT_004892e0 = 0;       /* misc counter */
int           DAT_0048929c = 0;       /* misc counter */
int           DAT_004892c0 = 0;       /* misc counter */
int           DAT_00489258 = 0;       /* misc counter */
char          DAT_004892a4 = 0;       /* misc flag */
char          DAT_004892a5 = 0;       /* misc flag */
void         *DAT_00489e80 = NULL;    /* wall segment array */
int           DAT_00487834[12] = {0}; /* entity tracking counters */
float         DAT_004892d0 = 0.0f;    /* water level / weather */

/* Float constants from .rdata (0x475620-0x47562B) for water level calculation */
static const union { unsigned int u; float f; } _water_scale_0 = { 0x3651b717u };
static const union { unsigned int u; float f; } _water_scale_1 = { 0x370bcf65u };
static const union { unsigned int u; float f; } _water_scale_2 = { 0x37d1b717u };
#define WATER_SCALE_0 (_water_scale_0.f)
#define WATER_SCALE_1 (_water_scale_1.f)
#define WATER_SCALE_2 (_water_scale_2.f)

/* Ship filenames (from binary at 0x0047BE90..0x0047BEFC) */
static const char *ship_dir = "ships\\";
static const char *ship_files[9] = {
    "peru.shp",  /* type 0 - DAT_0047bef0 */
    "batm.shp",  /* type 1 - DAT_0047bee4 */
    "bee2.shp",  /* type 2 - DAT_0047bed8 */
    "sped.shp",  /* type 3 - DAT_0047becc */
    "xwin.shp",  /* type 4 - DAT_0047bec0 */
    "tief.shp",  /* type 5 - DAT_0047beb4 */
    "perh.shp",  /* type 6 - DAT_0047bea8 */
    "flyy.shp",  /* type 7 - DAT_0047be9c */
    "dest.shp",  /* type 8 - DAT_0047be90 */
};


/* ===== FUN_0041b010 - Player/Ship Initialization (0041B010) ===== */
/* Iterates over DAT_00489240 players at stride 0x598, initializes all fields,
 * assigns ship types, weapon loadout, then calls FUN_0041b5d0 (finalize). */
void FUN_0041b010(void)
{
    int i;
    int offset;
    unsigned int angle;
    int base;

    i = 0;
    if (0 < DAT_00489240) {
        offset = 0;
        do {
            base = DAT_00487810;

            /* Clear velocity/movement */
            *(int *)(offset + 0x10 + base) = 0;
            *(int *)(offset + 0x14 + base) = 0;

            /* Random angle (0..2047) */
            angle = rand() & 0x800007FF;
            if ((int)angle < 0) {
                angle = (angle - 1 | 0xFFFFF800) + 1;
            }
            *(unsigned int *)(offset + 0x18 + base) = angle;

            /* Clear flags */
            *(char *)(offset + 0x1C + base) = 0;
            *(char *)(offset + 0x1D + base) = 0;
            *(int *)(offset + 0x20 + base) = 0;
            *(char *)(offset + 0x24 + base) = 0;
            *(char *)(offset + 0x25 + base) = 0;
            *(char *)(offset + 0x26 + base) = 0;

            /* Lives */
            *(int *)(offset + 0x28 + base) = (int)DAT_0048373a;
            *(int *)(offset + 0x30 + base) = 0;

            /* Stats */
            *(int *)(offset + 0x90 + base) = 0;
            *(int *)(offset + 0x94 + base) = 0;
            *(int *)(offset + 0x98 + base) = DAT_00483830;  /* health */
            *(char *)(offset + 0x9C + base) = 0x40;  /* hitbox w */
            *(char *)(offset + 0x9D + base) = 0x40;  /* hitbox h */
            *(char *)(offset + 0x9E + base) = 0;
            *(char *)(offset + 0x9F + base) = 0;
            *(char *)(offset + 0xA0 + base) = 0;
            *(char *)(offset + 0xA1 + base) = 0;
            *(char *)(offset + 0xA2 + base) = 0;
            *(char *)(offset + 0xA3 + base) = 0;
            *(int *)(offset + 0xA4 + base) = 0;
            *(int *)(offset + 0xA8 + base) = 0;
            *(int *)(offset + 0xB8 + base) = 0;
            *(int *)(offset + 0xBC + base) = 0;
            *(int *)(offset + 0xC0 + base) = 0;
            *(char *)(offset + 0xC4 + base) = 0;
            *(char *)(offset + 0xC5 + base) = 0;
            *(char *)(offset + 0xC6 + base) = 0;
            *(char *)(offset + 0xC7 + base) = 0;   /* 199 decimal */
            *(char *)(offset + 0xC8 + base) = (char)0xFA;  /* 250 */
            *(char *)(offset + 0xC9 + base) = 0;
            *(char *)(offset + 0xCA + base) = 0;
            *(char *)(offset + 0xCB + base) = 0;
            *(char *)(offset + 0xCC + base) = 0;
            *(int *)(offset + 0xD0 + base) = 0;
            *(int *)(offset + 0xD8 + base) = 0;
            *(int *)(offset + 0xD4 + base) = 0;
            *(char *)(offset + 0xDC + base) = 0;
            *(char *)(offset + 0xDD + base) = 0;
            *(int *)(offset + 0xE0 + base) = 1;
            *(int *)(offset + 0xF0 + base) = 0;
            *(int *)(offset + 0xEC + base) = 0;
            *(int *)(offset + 0xF4 + base) = 0;
            *(char *)(offset + 0xE4 + base) = 0;
            *(int *)(offset + 0xE8 + base) = 0;

            /* Extended fields 0x430..0x478 */
            *(int *)(offset + 0x430 + base) = 0;
            *(int *)(offset + 0x434 + base) = 0;
            *(int *)(offset + 0x438 + base) = 0;
            *(int *)(offset + 0x43C + base) = 0;
            *(int *)(offset + 0x440 + base) = 0;
            *(int *)(offset + 0x444 + base) = 0;
            *(int *)(offset + 0x448 + base) = 0;
            *(int *)(offset + 0x44C + base) = 0;
            *(int *)(offset + 0x450 + base) = 0;
            *(int *)(offset + 0x454 + base) = 0;
            *(int *)(offset + 0x458 + base) = 0;
            *(int *)(offset + 0x464 + base) = 0;
            *(int *)(offset + 0x468 + base) = 0;
            *(int *)(offset + 0x46C + base) = 0;
            *(int *)(offset + 0x470 + base) = 0;
            *(int *)(offset + 0x474 + base) = 0;
            *(int *)(offset + 0x478 + base) = 0;
            *(char *)(offset + 0x47C + base) = 0;
            *(char *)(offset + 0x47D + base) = 0;
            *(char *)(offset + 0x47E + base) = 0;
            *(char *)(offset + 0x47F + base) = 0;
            *(int *)(offset + 0x494 + base) = 0;
            *(int *)(offset + 0x498 + base) = 0;
            *(char *)(offset + 0x49C + base) = 0;
            *(char *)(offset + 0x49D + base) = 0;
            *(char *)(offset + 0x49E + base) = 0;
            *(char *)(offset + 0x49F + base) = 0;
            *(char *)(offset + 0x4A0 + base) = (char)0xFF;
            *(char *)(offset + 0x4A1 + base) = 0;
            *(char *)(offset + 0x4A2 + base) = 0;
            *(char *)(offset + 0x4A3 + base) = 0x30;
            *(char *)(offset + 0x4A4 + base) = 0;

            /* Ship type selection based on game mode */
            base = DAT_00487810;
            if (DAT_00483738 == 1) {
                /* Mode 1: random from available types */
                if (0 < *(int *)(offset + 0x38 + base)) {
                    int r = rand();
                    *(char *)(offset + 0x34 + base) =
                        (char)(r % (*(int *)(offset + 0x38 + base) + 1));
                }
            } else if (DAT_00483738 == 2) {
                /* Mode 2: match to player config */
                int j = 0;
                *(char *)(offset + 0x34 + base) = 100;  /* sentinel */
                int max_type = *(int *)(offset + 0x38 + base);
                if (max_type != -1 && max_type + 1 > 0) {
                    do {
                        if (*(char *)(offset + base + 0x3C + j) == DAT_004836ce[i]) {
                            *(char *)(offset + 0x34 + base) = (char)j;
                            base = DAT_00487810;
                        }
                        j++;
                    } while (j < *(int *)(offset + 0x38 + base) + 1);
                }
                if (*(char *)(offset + 0x34 + base) == 'd') {
                    *(char *)(offset + 0x34 + base) = 0;
                }
            }

            base = DAT_00487810;
            *(int *)(offset + 0x4AC + base) = 0;
            *(int *)(offset + 0x4A8 + base) = 0;
            *(int *)(offset + 0x4B0 + base) = 0;

            /* Cap team count from entity type table */
            if (DAT_00487abc) {
                unsigned char ship_idx = *(unsigned char *)(
                    *(char *)(offset + 0x34 + base) + offset + 0x3C + base);
                unsigned char max_team = *(unsigned char *)(
                    (int)DAT_00487abc + 0x7D + (unsigned int)ship_idx * 0x218);
                unsigned char *team_ptr = (unsigned char *)(offset + 0x35 + base);
                if (max_team < *team_ptr) {
                    *team_ptr = max_team;
                }
            }

            i++;
            offset += 0x598;
        } while (i < DAT_00489240);
    }

    /* Second loop: AI difficulty assignment from packed config.
     * Original uses hardcoded address bounds (0x4822CE - &DAT_0048227c = 0x52),
     * iterating 80 - DAT_00489244 times. We bound by DAT_00489240 for safety. */
    if (DAT_00489244 < 0x50) {
        char *src = (char *)((char *)&DAT_0048227c + DAT_00489244 + 2);
        int off = DAT_00489244 * 0x598;
        int ai_count = 80 - DAT_00489244;  /* max AI player slots */
        int k = 0;
        while (k < ai_count && (DAT_00489244 + k) < DAT_00489240) {
            char val = *src;
            src++;
            *(char *)(off + 0xDD + DAT_00487810) = val + 1;
            off += 0x598;
            k++;
        }
    }

    /* Third loop: weapon loadout */
    i = 0;
    if (0 < DAT_00489240) {
        offset = 0;
        do {
            if (DAT_004892e5 == 1) {
                *(char *)(offset + 0x8C + DAT_00487810) = 3;
                *(char *)(offset + 0x8D + DAT_00487810) = 0;
            } else {
                int r1 = rand();
                *(char *)(offset + 0x8C + DAT_00487810) = (char)(r1 % 3);
                int r2 = rand();
                *(char *)(offset + 0x8D + DAT_00487810) = (char)(r2 % 3);
            }
            i++;
            offset += 0x598;
        } while (i < DAT_00489240);
    }

    FUN_0041b5d0();
}


/* ===== FUN_0041b5d0 - Ship Finalize / Spawn Positions (0041B5D0) ===== */
/* Builds active player index table, assigns spawn positions based on
 * player count and map dimensions, then calls FUN_0041bb00. */
void FUN_0041b5d0(void)
{
    int i, idx;
    int base, count;
    int local_4;

    /* Pass 1: collect active human players (flag at +0x480 == 1) */
    idx = 0;
    i = 0;
    DAT_00487808 = 0;
    base = DAT_00487810;
    count = DAT_00489240;

    if (0 < count) {
        int off = 0;
        do {
            if (*(char *)(off + 0x480 + base) == 1) {
                if (idx > 3) {
                    /* Max 4 human players */
                    *(char *)(off + 0x480 + base) = 0;
                    base = DAT_00487810;
                    count = DAT_00489240;
                    idx = DAT_00487808;
                }
                if (*(char *)(off + 0x480 + base) == 1) {
                    DAT_004877f8[idx] = i;
                    idx = DAT_00487808 + 1;
                    DAT_00487808 = idx;
                }
            }
            i++;
            off += 0x598;
        } while (i < count);
    }

    /* Pass 2: clear spawn position/direction fields */
    i = 0;
    if (0 < count) {
        int off = 0;
        do {
            *(int *)(off + 0x484 + base) = 0;
            *(int *)(off + 0x488 + DAT_00487810) = 0;
            i++;
            off += 0x598;
            base = DAT_00487810;
            idx = DAT_00487808;
        } while (i < DAT_00489240);
    }

    /* Determine layout mode */
    local_4 = idx;
    if (DAT_00483724[2] == 2) {
        local_4 = 4;  /* Force 4-player layout */
    }

    /* Pass 3: assign spawn positions based on player count */
    i = 0;
    if (0 < idx) {
        for (i = 0; i < idx; i++) {
            int pidx = DAT_004877f8[i];
            switch (local_4) {
            case 0:
                *(int *)(pidx * 0x598 + 0x484 + base) = 0;
                *(int *)(pidx * 0x598 + 0x488 + DAT_00487810) = 0;
                base = DAT_00487810;
                break;
            case 1:
                *(int *)(pidx * 0x598 + 0x484 + base) = g_DisplayWidth;
                *(int *)(pidx * 0x598 + 0x488 + DAT_00487810) = g_DisplayHeight;
                base = DAT_00487810;
                break;
            case 2:
                *(int *)(pidx * 0x598 + 0x484 + base) = g_DisplayWidth / 2;
                *(int *)(pidx * 0x598 + 0x488 + DAT_00487810) = g_DisplayHeight;
                base = DAT_00487810;
                break;
            case 3:
                *(int *)(pidx * 0x598 + 0x484 + base) = g_DisplayWidth / 2;
                *(int *)(pidx * 0x598 + 0x488 + DAT_00487810) = g_DisplayHeight / 2;
                base = DAT_00487810;
                break;
            case 4:
                *(int *)(pidx * 0x598 + 0x484 + base) = g_DisplayWidth / 2;
                *(int *)(pidx * 0x598 + 0x488 + DAT_00487810) = g_DisplayHeight / 2;
                base = DAT_00487810;
                break;
            }
        }
    }

    /* Pass 4: assign spawn directions per quadrant */
    for (i = 0; i < DAT_00487808; i++) {
        int pidx = DAT_004877f8[i];
        switch (i) {
        case 0:
            if (local_4 == 1) {
                *(int *)(pidx * 0x598 + 0x48C + base) = 0;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = 0;
                base = DAT_00487810;
            } else if (local_4 == 2) {
                *(int *)(pidx * 0x598 + 0x48C + base) = g_DisplayWidth / 2;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = 0;
                base = DAT_00487810;
            } else if (local_4 == 3 || local_4 == 4) {
                *(int *)(pidx * 0x598 + 0x48C + base) = g_DisplayWidth / 2;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = g_DisplayHeight / 2;
                base = DAT_00487810;
            }
            break;
        case 1:
            if (local_4 == 2) {
                *(int *)(pidx * 0x598 + 0x48C + base) = 0;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = 0;
                base = DAT_00487810;
            } else if (local_4 == 3 || local_4 == 4) {
                *(int *)(pidx * 0x598 + 0x48C + base) = 0;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = 0;
                base = DAT_00487810;
            }
            break;
        case 2:
            if (local_4 == 3 || local_4 == 4) {
                *(int *)(pidx * 0x598 + 0x48C + base) = 0;
                *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = g_DisplayHeight / 2;
                base = DAT_00487810;
            }
            break;
        case 3:
            *(int *)(pidx * 0x598 + 0x48C + base) = g_DisplayWidth / 2;
            *(int *)(pidx * 0x598 + 0x490 + DAT_00487810) = 0;
            base = DAT_00487810;
            break;
        }
    }

    /* Pass 5: aspect ratio correction for mode 1 */
    if (DAT_00483724[2] == 1 && DAT_00487808 > 0) {
        for (i = 0; i < DAT_00487808; i++) {
            int off = DAT_004877f8[i] * 0x598;
            int field_488 = *(int *)(off + 0x488 + base);
            int field_484 = *(int *)(off + 0x484 + base);
            if (field_484 < field_488) {
                *(int *)(off + 0x490 + base) += (field_488 - field_484) / 2;
                *(int *)(off + 0x488 + DAT_00487810) =
                    *(int *)(off + 0x484 + DAT_00487810);
                base = DAT_00487810;
            }
            field_484 = *(int *)(off + 0x484 + base);
            field_488 = *(int *)(off + 0x488 + base);
            if (field_488 < field_484) {
                *(int *)(off + 0x48C + base) += (field_484 - field_488) / 2;
                *(int *)(off + 0x484 + DAT_00487810) =
                    *(int *)(off + 0x488 + DAT_00487810);
                base = DAT_00487810;
            }
        }
    }

    /* Pass 6: disable players with too-small spawn areas */
    for (i = 0; i < DAT_00487808; i++) {
        int off = base + DAT_004877f8[i] * 0x598;
        if (*(int *)(base + 0x488 + DAT_004877f8[i] * 0x598) < 200 ||
            *(int *)(off + 0x484) < 0xDC) {
            *(char *)(off + 0x9E) = 0;
            base = DAT_00487810;
        }
    }

    FUN_0041bb00();
}


/* ===== FUN_0041bad0 - Free Player Visibility Buffers (0041BAD0) ===== */
/* Counterpart to FUN_0041bb00: frees the per-player visibility buffers.
 * Called from Free_Game_Resources (tail call in original binary). */
void FUN_0041bad0(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        if (DAT_00489eac[i] != NULL) {
            Mem_Free(DAT_00489eac[i]);
            DAT_00489eac[i] = NULL;
        }
    }
}


/* ===== FUN_0041bb00 - Post-Finalize (0041BB00) ===== */
/* Marks human players as alive, clears stats, allocates per-player buffers. */
void FUN_0041bb00(void)
{
    int i;

    /* Set alive flag for each human player */
    if (0 < DAT_00487808) {
        for (i = 0; i < DAT_00487808; i++) {
            *(char *)(DAT_00487810 + 0x4A0 + DAT_004877f8[i] * 0x598) = (char)0xFF;
        }
    }

    /* Clear per-player stats */
    DAT_00489eac[0] = NULL;
    DAT_00487788[0] = 0;
    DAT_00489eac[1] = NULL;
    DAT_00487788[1] = 0;
    DAT_00489eac[2] = NULL;
    DAT_00487788[2] = 0;
    DAT_00489eac[3] = NULL;
    DAT_00487788[3] = 0;

    /* Allocate per-player visibility buffers */
    for (i = 0; i < DAT_00487808; i++) {
        int off = DAT_004877f8[i] * 0x598;
        int w = *(int *)(off + 0x484 + DAT_00487810) + 0x20;
        int h = *(int *)(off + 0x488 + DAT_00487810) + 0x40;
        int size = w * h;

        DAT_00489eac[i] = Mem_Alloc(size);
        g_MemoryTracker += size;

        /* Zero the buffer (Mem_Alloc uses calloc, but match original) */
        if (DAT_00489eac[i]) {
            memset(DAT_00489eac[i], 0, size);
        }
    }
}


/* ===== FUN_00424240 - Sprite Color Transform (00424240) ===== */
/* Recolors ship sprites with team colors. Builds 3x3 LUT from team palette
 * and ship type, then applies sqrt-of-sum-of-squares blend to all 32 frames.
 *
 * In the original binary, this reads parameters from the caller's stack frame.
 * Here we pass them explicitly. */
void FUN_00424240(int ship_type, int ship_index, int team_index)
{
    unsigned int team_r, team_g, team_b;
    unsigned int base_r, base_g, base_b;
    int lut[3 * 256 * 3];  /* 3 output channels x 256 entries x 3 input channels */
    int frame_w, frame_h;
    int pixel_offset;

    /* Extract team color from X1R5G5B5 palette */
    unsigned short team_color = DAT_00483838[team_index];
    team_r = (unsigned char)((team_color >> 10) << 3);
    team_g = (unsigned char)(((team_color >> 5) & 0x1F) << 3);
    team_b = (unsigned char)((team_color & 0x1F) << 3);

    /* Read frame dimensions (same for all 32 rotation frames) */
    frame_w = *(int *)((char *)DAT_00487aac + 100000 + ship_index * 0x186A8);
    frame_h = *(int *)((char *)DAT_00487aac + ship_index * 0x186A8 + 0x186A4);

    /* Initialize base color weights (overridden by switch) */
    base_r = 0x50;
    base_g = 0x50;
    base_b = 0x50;

    /* Configure color transform per ship type */
    switch (ship_type) {
    case 0:
        base_b = 0x78; base_r = 0x46; base_g = 0x46;
        DAT_00481d10 = 0x80; DAT_00481d1c = 0x14A;
        DAT_00481d18 = 0xFF; DAT_00481d14 = 0x80;
        DAT_00481d0c = 0x80; DAT_00481d20 = 0xFF;
        DAT_00481d2c = 0xB4; DAT_00481d30 = 0x8C; DAT_00481d34 = 0xB4;
        break;
    case 1:
        base_b = 0x78; base_r = 0x46; base_g = 0x46;
        DAT_00481d10 = 200; DAT_00481d30 = 0xDC;
        DAT_00481d2c = 0xB4; DAT_00481d34 = 0xB4;
        DAT_00481d1c = 0xFF;
        DAT_00481d18 = 0xFF; DAT_00481d14 = 0x80;
        DAT_00481d0c = 0x80; DAT_00481d20 = 0xFF;
        break;
    case 2:
    case 5:
    case 6:
        base_r = 0x50; base_g = 0x50; base_b = 0x50;
        DAT_00481d0c = 0x80; DAT_00481d10 = 0x80; DAT_00481d14 = 0x80;
        DAT_00481d2c = 0xA0; DAT_00481d30 = 0xA0; DAT_00481d34 = 0xA0;
        DAT_00481d18 = 0x14A; DAT_00481d1c = 0x14A; DAT_00481d20 = 0x14A;
        break;
    case 3:
        base_r = 0x6E; base_g = 0x6E; base_b = 0x6E;
        DAT_00481d0c = 0x8C; DAT_00481d10 = 0xA0; DAT_00481d14 = 0x80;
        DAT_00481d2c = 0x5A; DAT_00481d30 = 0x6E; DAT_00481d34 = 0xA0;
        DAT_00481d18 = 0x96; DAT_00481d1c = 0xBE; DAT_00481d20 = 0xF0;
        break;
    case 4:
        DAT_00481d10 = 200;
        base_r = 100; base_g = 100; base_b = 100;
        DAT_00481d0c = 0x80; DAT_00481d14 = 0x80;
        DAT_00481d2c = 0x100; DAT_00481d30 = 0x8C; DAT_00481d34 = 0xBE;
        DAT_00481d18 = 0xFF; DAT_00481d1c = 0x14A; DAT_00481d20 = 0x1A4;
        break;
    case 7:
        base_b = 0x14; base_r = 0x5A; base_g = 0x5A;
        DAT_00481d0c = 0x80; DAT_00481d10 = 0x80; DAT_00481d14 = 0x80;
        DAT_00481d2c = 0xA0; DAT_00481d30 = 0xA0; DAT_00481d34 = 0xA0;
        DAT_00481d18 = 0x14A; DAT_00481d1c = 0x14A; DAT_00481d20 = 0x14A;
        break;
    case 8:
        DAT_00481d2c = 0xAA;
        base_r = 10; base_g = 10; base_b = 10;
        DAT_00481d34 = 0xA0; DAT_00481d30 = 0xA0;
        DAT_00481d10 = 0x80; DAT_00481d1c = 0x14A;
        DAT_00481d18 = 0xFF; DAT_00481d14 = 0x80;
        DAT_00481d0c = 0x80; DAT_00481d20 = 0xFF;
        break;
    }

    /* Scale base colors by min matrix values (>>7 = divide by 128) */
    {
        int *base_arr[3] = { (int *)&base_r, (int *)&base_g, (int *)&base_b };
        int *matrix = &DAT_00481d0c;  /* 3 consecutive ints */
        for (int ch = 0; ch < 3; ch++) {
            *(base_arr[0]) = (int)base_r * matrix[ch] >> 7;
            *(base_arr[1]) = (int)base_g * matrix[ch] >> 7;
            *(base_arr[2]) = (int)base_b * matrix[ch] >> 7;
        }
    }

    /* Actually, re-implement the LUT build more faithfully.
     * The original builds 3 LUTs (one per output R/G/B), each has 3×256 entries
     * (one sub-table per input R/G/B channel). */

    /* Scaled team colors (already multiplied by matrix) */
    int team_scaled[3][3];  /* [output_channel][input_channel] */
    team_scaled[0][0] = (int)team_r * DAT_00481d0c >> 7;  /* R-from-R */
    team_scaled[0][1] = (int)team_g * DAT_00481d0c >> 7;  /* G-from-R */
    team_scaled[0][2] = (int)team_b * DAT_00481d0c >> 7;  /* B-from-R */
    team_scaled[1][0] = (int)team_r * DAT_00481d10 >> 7;  /* R-from-G */
    team_scaled[1][1] = (int)team_g * DAT_00481d10 >> 7;  /* G-from-G */
    team_scaled[1][2] = (int)team_b * DAT_00481d10 >> 7;  /* B-from-G */
    team_scaled[2][0] = (int)team_r * DAT_00481d14 >> 7;  /* R-from-B */
    team_scaled[2][1] = (int)team_g * DAT_00481d14 >> 7;  /* G-from-B */
    team_scaled[2][2] = (int)team_b * DAT_00481d14 >> 7;  /* B-from-B */

    /* Build LUTs: 3 output channels, each with 3 input channel sub-tables */
    int transitions[3] = { DAT_00481d2c, DAT_00481d30, DAT_00481d34 };
    int thresholds[3] = { DAT_00481d18, DAT_00481d1c, DAT_00481d20 };

    for (int out_ch = 0; out_ch < 3; out_ch++) {
        int trans = transitions[out_ch];
        int thresh = thresholds[out_ch];

        /* Phase 1: ramp from 0 to team_scaled (0..trans entries) */
        if (trans > 0) {
            for (int k = 0; k < trans && k < 256; k++) {
                for (int in_ch = 0; in_ch < 3; in_ch++) {
                    int idx = out_ch * 768 + in_ch * 256 + k;
                    lut[idx] = (k * team_scaled[out_ch][in_ch] * 256 / trans) >> 8;
                }
            }
        }

        /* Phase 2: ramp from team_scaled to 255 (trans..255 entries) */
        int remain = thresh - trans;
        if (trans < 256 && remain > 0) {
            for (int k = 0; k < 256 - trans; k++) {
                for (int in_ch = 0; in_ch < 3; in_ch++) {
                    int base_val = team_scaled[out_ch][in_ch];
                    int target = 256 - base_val;
                    int idx = out_ch * 768 + in_ch * 256 + trans + k;
                    if (idx < 3 * 256 * 3) {
                        lut[idx] = base_val + (k * target) / remain;
                    }
                }
            }
        }
    }

    /* Apply LUTs to all 32 rotation frames */
    pixel_offset = 0;
    for (int frame = 0; frame < 32; frame++) {
        for (int row = 0; row < frame_h; row++) {
            for (int col = 0; col < frame_w; col++) {
                int byte_off = (pixel_offset + ship_index * 0xC354) * 2;
                unsigned short pixel = *(unsigned short *)((char *)DAT_00487aac + byte_off);

                if (pixel != 0) {
                    /* Extract RGB565 channels */
                    unsigned int pb = (unsigned char)((pixel & 0x1F) << 3);
                    unsigned int pg = (unsigned char)(((pixel >> 5) & 0x3F) << 2);
                    unsigned int pr = (unsigned char)(((pixel >> 11) & 0x1F) << 3);

                    /* Apply 3x3 LUT: sqrt(sum of squares) per output channel */
                    int sum_r = lut[0 * 768 + 0 * 256 + (pr & 0xFF)] *
                                lut[0 * 768 + 0 * 256 + (pr & 0xFF)] +
                                lut[0 * 768 + 1 * 256 + (pg & 0xFF)] *
                                lut[0 * 768 + 1 * 256 + (pg & 0xFF)] +
                                lut[0 * 768 + 2 * 256 + (pb & 0xFF)] *
                                lut[0 * 768 + 2 * 256 + (pb & 0xFF)];
                    int out_r = (int)sqrt((double)sum_r);

                    int sum_g = lut[1 * 768 + 0 * 256 + (pr & 0xFF)] *
                                lut[1 * 768 + 0 * 256 + (pr & 0xFF)] +
                                lut[1 * 768 + 1 * 256 + (pg & 0xFF)] *
                                lut[1 * 768 + 1 * 256 + (pg & 0xFF)] +
                                lut[1 * 768 + 2 * 256 + (pb & 0xFF)] *
                                lut[1 * 768 + 2 * 256 + (pb & 0xFF)];
                    int out_g = (int)sqrt((double)sum_g);

                    int sum_b = lut[2 * 768 + 0 * 256 + (pr & 0xFF)] *
                                lut[2 * 768 + 0 * 256 + (pr & 0xFF)] +
                                lut[2 * 768 + 1 * 256 + (pg & 0xFF)] *
                                lut[2 * 768 + 1 * 256 + (pg & 0xFF)] +
                                lut[2 * 768 + 2 * 256 + (pb & 0xFF)] *
                                lut[2 * 768 + 2 * 256 + (pb & 0xFF)];
                    int out_b = (int)sqrt((double)sum_b);

                    /* Clamp to 255 */
                    if (out_r > 255) out_r = 255;
                    if (out_g > 255) out_g = 255;
                    if (out_b > 255) out_b = 255;

                    /* Near-black → transparent */
                    if (out_r < 8 && out_g < 4 && out_b < 8) {
                        pixel = 0;
                    } else {
                        /* Pack as RGB565 */
                        pixel = (unsigned short)(
                            (out_b >> 3) +
                            (((out_r & 0x1F8) * 0x20 + (out_g & 0x3FF8)) * 4));
                    }
                }

                *(unsigned short *)((char *)DAT_00487aac + byte_off) = pixel;
                pixel_offset++;
            }
        }
    }
}


/* ===== FUN_004249c0 - Ship Sprite Loading (004249C0) ===== */
/* Loads .shp files for each player's ship type. Reads 32 rotation frames
 * per ship, converts RGB24→RGB565, stores in DAT_00487aac sprite buffer.
 * Returns 1 on success, 0 on failure. */
int FUN_004249c0(void)
{
    int ship_idx;
    int meta_offset;   /* offset into width/height arrays (stride 0x186A8) */
    int pixel_base;    /* offset into pixel data (stride 0xC354) */
    int stats_offset;  /* offset into ship stats table (stride 0x40) */
    int player_offset; /* offset into player data (stride 0x598) */
    char path[256];
    FILE *f;
    unsigned char header_byte;
    unsigned char stats_buf[14];
    unsigned char frame_header[12];
    unsigned short frame_w, frame_h;
    unsigned short frame_pad;
    unsigned char *rgb_buf;

    if (DAT_00489240 <= 0) return 1;

    meta_offset = 0;
    pixel_base = 0;
    stats_offset = 0;
    player_offset = 0;

    for (ship_idx = 0; ship_idx < DAT_00489240; ship_idx++) {
        unsigned int ship_type = (unsigned int)DAT_0048236e[ship_idx];

        /* Type 9 = random: pick an unused ship */
        if (ship_type == 9) {
            int avail = 0;
            for (int j = 0; j < 9; j++) {
                if (DAT_0048378e[j] != 0) avail++;
            }
            if (avail >= 9) {
                /* All taken, just pick random */
                ship_type = rand() % 9;
            } else {
                do {
                    ship_type = rand() % 9;
                } while (DAT_0048378e[ship_type] != 0);
            }
        }

        /* Clamp to valid range */
        if (ship_type > 8) ship_type = 0;

        /* Build path: "ships\<name>.shp" */
        strcpy(path, ship_dir);
        strcat(path, ship_files[ship_type]);

        LOG("[SHIP] Loading ship %d: type=%d file=%s\n", ship_idx, ship_type, path);

        f = fopen(path, "rb");
        if (!f) {
            LOG("[SHIP] ERROR: Cannot open %s\n", path);
            return 0;
        }

        /* Weapon overrides for specific ship types */
        if (ship_type == 6) {
            *(char *)(player_offset + 0x8C + DAT_00487810) = 8;
            *(char *)(player_offset + 0x8D + DAT_00487810) = 0;
        } else if (ship_type == 3) {
            *(char *)(player_offset + 0x8C + DAT_00487810) = 4;
            *(char *)(player_offset + 0x8D + DAT_00487810) = 1;
        }

        /* Read and validate header byte */
        fread(&header_byte, 1, 1, f);
        if (header_byte != 0) {
            LOG("[SHIP] ERROR: Invalid header byte in %s\n", path);
            fclose(f);
            return 0;
        }

        /* Read ship name (null-terminated, max 31 chars) into stats table */
        if (DAT_0048780c) {
            int j;
            char *name_dest = (char *)DAT_0048780c + stats_offset;
            for (j = 0; j < 0x1F; j++) {
                fread(&name_dest[j], 1, 1, f);
                if (name_dest[j] == '\0') break;
            }
            name_dest[j] = '\0';
            /* Seek past remaining name bytes if we broke early */
            if (j < 0x1F) {
                /* Need to position at stats_buf read point:
                 * We read header(1) + name chars(j+1 including null).
                 * But original reads one byte at a time until null or 31. */
            }
        }

        /* Read 14 bytes of ship stats */
        fread(stats_buf, 1, 14, f);

        if (DAT_0048780c) {
            char *sp = (char *)DAT_0048780c + stats_offset;
            sp[0x20] = stats_buf[0];
            sp[0x21] = stats_buf[1];
            sp[0x22] = stats_buf[2];
            sp[0x23] = stats_buf[3];
            sp[0x24] = stats_buf[4];  /* approximate: original uses __ftol */
            *(unsigned int *)(sp + 0x28) = (unsigned int)(unsigned char)stats_buf[5] * 0x7D000;
            sp[0x2C] = stats_buf[6];
            sp[0x2D] = stats_buf[7];
            sp[0x2E] = stats_buf[8] << 2;
            sp[0x2F] = stats_buf[9];
            sp[0x30] = stats_buf[10];
            sp[0x31] = stats_buf[11];
            sp[0x32] = stats_buf[12];
            sp[0x33] = stats_buf[13];
            *(int *)(sp + 0x34) = 10;

            if (sp[0x2D] == '\0') {
                sp[0x2E] = 1;
            }
        }

        /* Read 32 rotation frames */
        int total_pixels = 0;
        for (int frame = 0; frame < 32; frame++) {
            /* 12-byte frame header (mostly padding/unused) */
            fread(frame_header, 1, 12, f);

            /* Frame dimensions (2 bytes each, little-endian) */
            fread(&frame_w, 2, 1, f);
            fread(&frame_h, 2, 1, f);

            /* 2 bytes padding */
            fread(&frame_pad, 2, 1, f);

            /* Store dimensions in metadata area */
            *(int *)((char *)DAT_00487aac + 100000 + meta_offset) = (int)(frame_w & 0xFFFF);
            *(int *)((char *)DAT_00487aac + meta_offset + 0x186A4) = (int)(frame_h & 0xFFFF);

            /* Read RGB24 pixel data */
            int num_pixels = (int)(frame_w & 0xFFFF) * (int)(frame_h & 0xFFFF);
            int rgb_size = num_pixels * 3;

            rgb_buf = (unsigned char *)DAT_00481cf8;
            if (rgb_size > 0) {
                fread(rgb_buf, 1, rgb_size, f);
            }

            /* Convert RGB24 → RGB565 into DAT_00487aac */
            for (int row = 0; row < (int)(frame_h & 0xFFFF); row++) {
                for (int col = 0; col < (int)(frame_w & 0xFFFF); col++) {
                    int src_idx = (row * (int)(frame_w & 0xFFFF) + col) * 3;
                    unsigned char r_val = rgb_buf[src_idx];
                    unsigned char g_val = rgb_buf[src_idx + 1];
                    unsigned char b_val = rgb_buf[src_idx + 2];

                    /* RGB565 packing: matching original's bit layout */
                    unsigned short rgb565 = (unsigned short)(
                        (r_val >> 3) +
                        (((b_val & 0x1F8) * 0x20 + (g_val & 0x3FF8)) * 4));

                    int dst_off = (pixel_base + total_pixels) * 2;
                    *(unsigned short *)((char *)DAT_00487aac + dst_off) = rgb565;
                    total_pixels++;
                }
            }
        }

        /* Apply team color transform */
        FUN_00424240(ship_type, ship_idx, ship_idx % 4);

        /* Store computed value at +0x38 (approximate: original uses __ftol) */
        if (DAT_0048780c) {
            *(int *)((char *)DAT_0048780c + stats_offset + 0x38) = total_pixels;
        }

        fclose(f);

        /* Advance offsets */
        pixel_base += 0xC354;
        player_offset += 0x598;
        meta_offset += 0x186A8;
        stats_offset += 0x40;

        LOG("[SHIP] Ship %d loaded: %d total pixels\n", ship_idx, total_pixels);
    }

    return 1;
}


/* ===== FUN_00440ba0 - Tilemap Paint (STUB) (00440BA0) ===== */
/* Called by FUN_00406d20 for type-7 (team base) projectiles.
 * Paints a circular area on the tilemap. Non-critical for visual rendering. */
void FUN_00440ba0(int x, int y, int team, char param)
{
    /* stub */
}

/* ===== FUN_00457c70 - Wall Render Setup (STUB) (00457C70) ===== */
/* Called by FUN_00407400 after adding a wall segment. */
void FUN_00457c70(int index)
{
    /* stub */
}


/* ===== FUN_00407080 - Add Effect/Boss (00407080) ===== */
/* Adds an effect/boss entity to DAT_00487780 (stride 0x20, max 25). */
void FUN_00407080(int x, int y, unsigned char index, unsigned char type)
{
    unsigned int angle;

    if (DAT_00489264 < 0x19) {
        *(int *)((int)DAT_00487780 + DAT_00489264 * 0x20) = x;
        *(int *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 4) = y;
        *(unsigned char *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 0x14) = index;
        *(int *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 0xc) = 0;
        *(int *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 8) = 0;

        angle = rand();
        angle = angle & 0x800007FF;
        if ((int)angle < 0) {
            angle = (angle - 1 | 0xFFFFF800) + 1;
        }
        *(unsigned int *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 0x10) = angle;
        *(unsigned char *)((int)DAT_00487780 + DAT_00489264 * 0x20 + 0x15) = type;
        DAT_00489264 = DAT_00489264 + 1;
    }
}


/* ===== FUN_00407140 - Add Debris (00407140) ===== */
/* Adds a debris entity to DAT_00487830 (stride 0x20, max 100). */
void FUN_00407140(int x, int y, unsigned char type)
{
    int iVar1;

    if (DAT_00489268 < 100) {
        *(int *)((int)DAT_00487830 + DAT_00489268 * 0x20) = x;
        *(int *)((int)DAT_00487830 + DAT_00489268 * 0x20 + 4) = y;
        *(unsigned char *)((int)DAT_00487830 + DAT_00489268 * 0x20 + 10) = type;
        *(int *)((int)DAT_00487830 + DAT_00489268 * 0x20 + 0xc) = 2000;

        iVar1 = rand();
        *(char *)((int)DAT_00487830 + DAT_00489268 * 0x20 + 9) =
            (char)(iVar1 % (int)(*(unsigned char *)((unsigned int)type * 8 + 5 + (int)g_EntityConfig) - 1));

        iVar1 = rand();
        *(char *)((int)DAT_00487830 + DAT_00489268 * 0x20 + 8) =
            (char)(iVar1 % (int)(*(unsigned char *)((unsigned int)type * 8 + 4 + (int)g_EntityConfig) - 1));

        DAT_00489268 = DAT_00489268 + 1;
    }
}


/* ===== FUN_00407210 - Add Trooper (00407210) ===== */
/* Adds a trooper entity to DAT_00487884 (stride 0x40, max 400). */
void FUN_00407210(int x, int y, int vx, int vy, char dir, int speed, unsigned char type, char subtype)
{
    unsigned int *puVar1;

    if (DAT_0048924c < 400) {
        int base = (int)DAT_00487884 + DAT_0048924c * 0x40;

        *(int *)(base + 0x00) = x;
        *(int *)(base + 0x08) = y;
        *(int *)(base + 0x04) = x;
        *(int *)(base + 0x0c) = y;
        *(int *)(base + 0x10) = vx;
        *(int *)(base + 0x14) = vy;

        puVar1 = (unsigned int *)(base + 0x18);
        *puVar1 = *puVar1 & 0xFFFFFFFE;

        *(unsigned char *)(base + 0x1c) = type;
        *(char *)(base + 0x1d) = dir;
        *(int *)(base + 0x20) = speed;
        *(unsigned char *)(base + 0x24) = 0;
        *(char *)(base + 0x25) = subtype;

        if (subtype == 0) {
            *(int *)(base + 0x28) = 0xBB800;
        } else if (subtype == 2) {
            *(int *)(base + 0x28) = 0xFA000;
        } else {
            *(int *)(base + 0x28) = 0x271000;
        }

        *(unsigned char *)(base + 0x2c) = 0;
        *(unsigned char *)(base + 0x38) = 200;
        *(unsigned char *)(base + 0x39) = 0;

        if (dir == 1) {
            *(int *)(base + 0x30) = 0x200;
        } else {
            *(int *)(base + 0x30) = 0x600;
        }

        *(unsigned char *)(base + 0x2d) = 0;
        *(int *)(base + 0x34) = 0;
        DAT_0048924c = DAT_0048924c + 1;
    }
}


/* ===== FUN_00406d20 - Add Projectile (00406D20) ===== */
/* Adds a projectile/object to DAT_00481f28 (stride 0x40, max 200).
 * Also stamps the sprite footprint onto the tilemap. */
void FUN_00406d20(int x, int y, char type, int health, unsigned char team, unsigned char orientation)
{
    int iVar1;
    int *piVar2;
    int iVar3;
    unsigned short uVar4;
    int iVar5;
    unsigned char bVar6;
    unsigned short uVar7;
    int iVar8;
    int iVar9;

    if (DAT_00489260 < 200) {
        int base = (int)DAT_00481f28 + DAT_00489260 * 0x40;

        *(int *)(base + 0x00) = x;
        *(int *)(base + 0x04) = y;

        if (type == 7) {
            *(int *)(base + 0x08) = 0;
        } else if (orientation == 0) {
            iVar1 = rand();
            *(int *)(base + 0x08) = iVar1 % 0x7FF;
        } else {
            iVar1 = (unsigned int)orientation * 0x800 - 0x800;
            *(int *)(base + 0x08) = (int)(iVar1 + (iVar1 >> 0x1F & 0x1F)) >> 5;
        }

        *(char *)(base + 0x1c) = type;
        *(unsigned char *)(base + 0x1d) = team;
        *(int *)(base + 0x10) = health;
        *(int *)(base + 0x14) = 0;
        *(unsigned char *)(base + 0x1e) = 0;
        *(unsigned char *)(base + 0x20) = 0;

        if (type == 6) {
            *(unsigned char *)(base + 0x1f) = 0xFF;
        } else {
            *(unsigned char *)(base + 0x1f) = 10;
        }

        *(unsigned char *)(base + 0x21) = 0;
        *(int *)(base + 0x0c) = *(int *)(base + 0x08);
        *(unsigned char *)(base + 0x22) = 0;
        *(unsigned char *)(base + 0x23) = 0;
        *(int *)(base + 0x18) = 0;

        if (type == 7) {
            *(int *)(base + 0x14) = health;
            piVar2 = (int *)(base);
            FUN_00440ba0(*piVar2 >> 0x12, (piVar2[1] >> 0x12) - 10, (unsigned int)team, 0);
        }

        /* Stamp sprite footprint onto tilemap */
        uVar4 = 0;
        piVar2 = (int *)(base);
        iVar1 = *(int *)((unsigned int)*(unsigned char *)(base + 0x1c) * 0x20 + (int)DAT_00487818);
        int frame_off = *(int *)((int)DAT_00489234 + iVar1 * 4);
        bVar6 = *(unsigned char *)((int)DAT_00489e8c + iVar1);
        iVar9 = (*piVar2 >> 0x12) - (unsigned int)(bVar6 >> 1);
        iVar8 = (piVar2[1] >> 0x12) - (unsigned int)(*(unsigned char *)((int)DAT_00489e88 + iVar1) >> 1);
        iVar3 = (iVar8 << ((unsigned char)DAT_00487a18 & 0x1F)) + iVar9;

        if (*(unsigned char *)((int)DAT_00489e88 + iVar1) != 0) {
            do {
                uVar7 = 0;
                if (bVar6 != 0) {
                    do {
                        if ((*(char *)((int)DAT_00489e94 + frame_off) != 0) &&
                            ((int)((unsigned int)uVar7 + iVar9) > 0) &&
                            ((unsigned int)uVar7 + iVar9 < (unsigned int)DAT_004879f0) &&
                            ((int)((unsigned int)uVar4 + iVar8) > 0) &&
                            ((unsigned int)uVar4 + iVar8 < (unsigned int)DAT_004879f4)) {
                            iVar5 = (int)DAT_00487928 +
                                (unsigned int)*(unsigned char *)((int)DAT_0048782c + iVar3) * 0x20;
                            if (*(char *)(iVar5 + 0xb) == 0 && *(char *)(iVar5 + 0xa) == 0) {
                                *(unsigned char *)((int)DAT_0048782c + iVar3) =
                                    *(unsigned char *)(iVar5 + 8);
                            }
                        }
                        frame_off = frame_off + 1;
                        iVar3 = iVar3 + 1;
                        uVar7 = uVar7 + 1;
                    } while (uVar7 < *(unsigned char *)((int)DAT_00489e8c + iVar1));
                }
                bVar6 = *(unsigned char *)((int)DAT_00489e8c + iVar1);
                iVar3 = iVar3 + (DAT_00487a00 - (unsigned int)bVar6);
                uVar4 = uVar4 + 1;
            } while (uVar4 < *(unsigned char *)((int)DAT_00489e88 + iVar1));
        }
        DAT_00489260 = DAT_00489260 + 1;
    }
}


/* ===== FUN_00407400 - Add Wall Segment (00407400) ===== */
/* Adds a wall segment to DAT_00489e80 (stride 0x20, max 16). */
void FUN_00407400(int x, int y, char facing, unsigned char sprite, char mirror, unsigned char team)
{
    int iVar2;
    unsigned int uVar4;

    if ((int)DAT_00489270 < 0x10) {
        int base = (int)DAT_00489e80 + DAT_00489270 * 0x20;

        *(int *)(base + 0x00) = x;
        *(int *)(base + 0x04) = y;
        *(unsigned char *)(base + 0x14) = 1;
        *(char *)(base + 0x15) = facing;
        *(int *)(base + 0x08) = 0;
        *(unsigned char *)(base + 0x16) = 0;
        *(unsigned char *)(base + 0x17) = sprite;
        *(int *)(base + 0x0c) = *(int *)((unsigned int)sprite * 0x10 + 0x0c + (int)g_PhysicsParams);
        *(int *)(base + 0x10) = 0;

        uVar4 = DAT_00489270 & 0x80000003;
        if ((int)uVar4 < 0) {
            uVar4 = (uVar4 - 1 | 0xFFFFFFFC) + 1;
        }
        *(char *)(base + 0x18) = (char)uVar4;
        *(unsigned char *)(base + 0x1b) = 0xFF;

        if (mirror == 0) {
            *(unsigned char *)(base + 0x1a) = 0x3A;
        } else {
            *(unsigned char *)(base + 0x1a) = 0x3B;
        }

        /* Boundary checking for horizontal walls */
        if (facing == 0 || facing == 2) {
            int *piVar1 = (int *)(base);
            iVar2 = *piVar1;
            uVar4 = (unsigned int)(*(unsigned char *)((int)DAT_00489e8c +
                (unsigned int)*(unsigned char *)(base + 0x1a)) >> 1);
            if ((int)(iVar2 - uVar4) < 2) return;
            if (DAT_00487a00 - 2 <= (int)(iVar2 + uVar4)) return;
        }

        /* Boundary checking for vertical walls */
        if (facing == 1 || facing == 3) {
            iVar2 = *(int *)(base + 4);
            uVar4 = (unsigned int)(*(unsigned char *)(
                (unsigned int)*(unsigned char *)(base + 0x1a) + (int)DAT_00489e8c) >> 1);
            if ((int)(iVar2 - uVar4) < 2) return;
            if ((int)DAT_004879f4 - 2 <= (int)(iVar2 + uVar4)) return;
        }

        *(unsigned char *)(base + 0x19) = team;
        FUN_00457c70(DAT_00489270);
        DAT_00489270 = DAT_00489270 + 1;
    }
}


/* ===== FUN_0041bfe0 - Main Entity Spawner (0041BFE0) ===== */
/* Reads entity placements from level data and populates all entity arrays.
 * Also spawns random troopers and debris based on difficulty settings.
 * This is the function that makes entities visible in the game world. */
void FUN_0041bfe0(void)
{
    int iVar3, iVar4, iVar9, iVar10, iVar11, iVar15;
    unsigned int uVar5, uVar6;
    unsigned char bVar7;
    char cVar8, cVar14;
    unsigned char uVar13;

    /* Large local arrays for team base optimization */
    int local_690[128];     /* base X positions per team */
    int local_490[128];     /* base Y positions per team */
    unsigned int local_290[128]; /* base team indices */
    int local_90;           /* base count */

    /* Team base optimization arrays (4 teams x 0x181 stride) */
    unsigned int local_18a0[4 * 0x181]; /* per-team base lists: [team*0x181] = count, then data */
    int aiStackY_1ea0[4 * 0x181];       /* X coordinates */
    int aiStackY_1ca0[4 * 0x181];       /* Y coordinates */

    /* Best spawn positions */
    int aiStackY_8c[4];     /* best X */
    int aiStackY_7c[4];     /* best Y */
    int aiStackY_6c[4];     /* best active flags */
    int local_5c;           /* best minimum distance */
    int aiStackY_58[4];     /* candidate X */
    int aiStackY_48[4];     /* candidate Y */
    int stack_c8[4];        /* candidate active flags */

    /* ===== Phase 1: Zero all entity counts ===== */
    local_90 = 0;
    DAT_00489260 = 0;
    DAT_00489264 = 0;
    DAT_00489274 = 0;
    DAT_00489268 = 0;
    DAT_00489270 = 0;
    DAT_0048924c = 0;
    DAT_00489248 = 0;
    DAT_00489258 = 0;
    DAT_0048925c = 0;
    DAT_0048926c = 0;
    DAT_00489250 = 0;
    DAT_0048929c = 0;
    DAT_004892a4 = 0;
    DAT_004892a5 = 0;
    DAT_004892c0 = 0;
    DAT_004892d4 = 0;
    DAT_004892d8 = 0;
    DAT_004892dc = 0;
    DAT_004892e0 = 0;

    /* ===== Phase 2: Random trooper spawning ===== */
    if (DAT_00483737 != 0) {
        if (DAT_00483737 == 1) {
            iVar11 = ((int)(DAT_004879f4 + (DAT_004879f4 >> 0x1F & 0xFFU)) >> 8) *
                     ((int)(DAT_004879f0 + (DAT_004879f0 >> 0x1F & 0xFFU)) >> 8);
        } else {
            if (DAT_00483737 == 2) {
                iVar11 = DAT_004879f4 / 0xa0;
                iVar15 = DAT_004879f0 / 0xa0;
            } else {
                iVar11 = DAT_004879f4 / 100;
                iVar15 = DAT_004879f0 / 100;
            }
            iVar15 = iVar15 + ((int)DAT_004879f0 >> 0x1F);
            iVar11 = iVar11 * (iVar15 - (iVar15 >> 0x1F));
        }

        for (iVar15 = 0; iVar15 < iVar11; iVar15++) {
            do {
                iVar3 = rand();
                iVar9 = iVar3 % ((int)DAT_004879f0 - 0xe) + 7;
                iVar3 = rand();
                iVar3 = iVar3 % ((int)DAT_004879f4 - 0xe) + 7;
            } while (*(char *)((unsigned int)*(unsigned char *)(
                (iVar3 << ((unsigned char)DAT_00487a18 & 0x1F)) + (int)DAT_0048782c + iVar9) *
                0x20 + (int)DAT_00487928) == 0);

            cVar14 = 0;
            uVar13 = 3;
            iVar4 = rand();
            iVar4 = (iVar4 % 0x3c + 0x5a) * 0x200;
            uVar5 = rand();
            uVar5 = uVar5 & 0x80000001;
            if ((int)uVar5 < 0) {
                uVar5 = (uVar5 - 1 | 0xFFFFFFFE) + 1;
            }
            FUN_00407210(iVar9 * 0x40000, iVar3 * 0x40000, 0, 0,
                         (char)uVar5 * 2 - 1, iVar4, uVar13, cVar14);
        }
    }

    /* Dead loop (original loops 0 times - trooper variant) */
    for (iVar11 = 0; iVar11 < 0; iVar11++) {
        /* no-op */
    }

    /* ===== Phase 3: Random debris spawning ===== */
    if (DAT_00483736 != 0) {
        if (DAT_00483736 == 1) {
            iVar11 = ((int)(DAT_004879f4 + (DAT_004879f4 >> 0x1F & 0x1FFU)) >> 9) *
                     ((int)(DAT_004879f0 + (DAT_004879f0 >> 0x1F & 0x1FFU)) >> 9);
        } else {
            if (DAT_00483736 == 2) {
                iVar11 = DAT_004879f4 / 300;
                iVar15 = DAT_004879f0 / 300;
            } else {
                iVar11 = DAT_004879f4 / 200;
                iVar15 = DAT_004879f0 / 200;
            }
            iVar15 = iVar15 + ((int)DAT_004879f0 >> 0x1F);
            iVar11 = iVar11 * (iVar15 - (iVar15 >> 0x1F));
        }

        for (iVar15 = 0; iVar15 < iVar11; iVar15++) {
            do {
                iVar3 = rand();
                iVar9 = iVar3 % ((int)DAT_004879f0 - 0xe) + 7;
                iVar3 = rand();
                iVar3 = iVar3 % ((int)DAT_004879f4 - 0xe) + 7;
            } while (*(char *)((unsigned int)*(unsigned char *)(
                (iVar3 << ((unsigned char)DAT_00487a18 & 0x1F)) + (int)DAT_0048782c + iVar9) *
                0x20 + (int)DAT_00487928) == 0);

            bVar7 = 0;
            uVar13 = 3;
            iVar4 = rand();
            FUN_00406d20(iVar9 * 0x40000, iVar3 * 0x40000, (char)(iVar4 % 7), 0x4e2000, uVar13, bVar7);
        }
    }

    /* Dead loops (original loops 0 times - static entities, turrets, etc.) */
    for (iVar11 = 0; iVar11 < 0; iVar11++) { /* static entity no-op */ }
    for (iVar11 = 0; iVar11 < 0; iVar11++) { /* turret no-op */ }

    /* ===== Phase 4: Clear tracking array ===== */
    for (iVar11 = 0; iVar11 < 12; iVar11++) {
        DAT_00487834[iVar11] = 0;
    }

    /* ===== Phase 5: Process entity placements from level file ===== */
    iVar15 = 0;
    iVar11 = (int)DAT_00487828;

    while (iVar15 < DAT_00489278) {
        iVar3 = iVar15 * 0x14;

        switch (*(unsigned char *)(iVar3 + 8 + iVar11)) {
        case 0:
            /* Projectile placement */
            if (DAT_00483754[3] != 0) {
                bVar7 = *(unsigned char *)(iVar3 + 10 + iVar11);
                iVar9 = (int)(bVar7 + 1);
                iVar9 = iVar9 * iVar9 * iVar9 * 56000;
                if (bVar7 == 0x0F) {
                    iVar9 = 2000000000;
                }
                FUN_00406d20(
                    *(int *)(iVar3 + iVar11) << 0x12,
                    *(int *)(iVar3 + 4 + iVar11) << 0x12,
                    *(char *)(iVar3 + 9 + iVar11),
                    iVar9,
                    *(unsigned char *)(iVar3 + 0xb + iVar11),
                    *(unsigned char *)(iVar3 + 0xc + iVar11));
                iVar11 = (int)DAT_00487828;
            }
            break;

        case 1:
            /* Wall segment placement */
            if (DAT_00483754[2] != 0 && DAT_00489270 < 0x10) {
                FUN_00407400(
                    *(int *)(iVar3 + iVar11),
                    *(int *)(iVar3 + 4 + iVar11),
                    *(char *)(iVar3 + 0xc + iVar11),
                    *(unsigned char *)(iVar3 + 9 + iVar11),
                    *(char *)(iVar3 + 0xd + iVar11),
                    *(unsigned char *)(iVar3 + 0xb + iVar11));
                iVar11 = (int)DAT_00487828;

                /* Check for linked wall pair */
                if (*(char *)(iVar3 + 10 + (int)DAT_00487828) == 1 && DAT_00489270 < 0x10) {
                    int wx = *(int *)(iVar3 + (int)DAT_00487828);
                    int wy = *(int *)(iVar3 + 4 + (int)DAT_00487828);
                    bVar7 = (*(char *)(iVar3 + 0xc + (int)DAT_00487828) + 2) & 3;

                    if (bVar7 == 0) wy = wy + 1;
                    else if (bVar7 == 1) wx = wx + 1;
                    else if (bVar7 == 2) wy = wy - 1;
                    else if (bVar7 == 3) wx = wx - 1;

                    FUN_00407400(wx, wy, bVar7,
                        *(unsigned char *)(iVar3 + 9 + (int)DAT_00487828),
                        *(char *)(iVar3 + 0xd + (int)DAT_00487828),
                        *(unsigned char *)(iVar3 + 0xb + (int)DAT_00487828));

                    /* Link the two wall segments */
                    *(char *)((int)DAT_00489e80 + DAT_00489270 * 0x20 - 5) =
                        (char)DAT_00489270 - 2;
                    *(char *)((int)DAT_00489e80 + DAT_00489270 * 0x20 - 0x25) =
                        (char)DAT_00489270 - 1;
                    iVar11 = (int)DAT_00487828;
                }
            }
            break;

        case 2:
            /* Multi-type entity placement */
            switch (*(unsigned char *)(iVar3 + 9 + iVar11)) {
            case 0:
                /* Decoration type 0 */
                if (DAT_004892d8 < 0x78) {
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10) =
                        *(int *)(iVar3 + iVar11);
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 4) =
                        *(int *)(iVar3 + 4 + (int)DAT_00487828);
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xc) = 0;
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 8) =
                        (int)((unsigned int)*(unsigned char *)(iVar3 + 10 + (int)DAT_00487828) * 0x800) >> 3;
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xd) =
                        *(unsigned char *)(iVar3 + 0xb + (int)DAT_00487828);

                    switch (*(unsigned char *)(iVar3 + 0xc + (int)DAT_00487828)) {
                    case 0: *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xe) = 0x1e; break;
                    case 1: *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xe) = 10; break;
                    case 2: *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xe) = 4; break;
                    case 3: *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xe) = 2; break;
                    }

                    DAT_004892d8 = DAT_004892d8 + 1;
                    iVar15++;
                    iVar11 = (int)DAT_00487828;
                    continue;  /* skip the iVar15++ at end */
                }
                break;

            case 1:
                /* Decoration type 1 */
                if (DAT_004892d8 < 0x78) {
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10) =
                        *(int *)(iVar3 + iVar11);
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 4) =
                        *(int *)(iVar3 + 4 + (int)DAT_00487828);
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xc) = 1;
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 8) =
                        (int)((unsigned int)*(unsigned char *)(iVar3 + 10 + (int)DAT_00487828) * 0x800) >> 3;
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xd) =
                        *(unsigned char *)(iVar3 + 0xb + (int)DAT_00487828);
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xe) = 1;
                    DAT_004892d8 = DAT_004892d8 + 1;
                    iVar11 = (int)DAT_00487828;
                }
                break;

            case 2:
                /* Main entity */
                if (DAT_00489248 < 0x9c4) {
                    int ebase = (int)DAT_004892e8 + DAT_00489248 * 0x80;

                    *(int *)(ebase + 0x00) = *(int *)(iVar3 + iVar11) << 0x12;
                    *(int *)(ebase + 0x08) = *(int *)(iVar3 + 4 + (int)DAT_00487828) << 0x12;
                    *(int *)(ebase + 0x18) = 0;
                    *(int *)(ebase + 0x1c) = 0;
                    *(int *)(ebase + 0x04) = *(int *)(iVar3 + (int)DAT_00487828) << 0x12;
                    *(int *)(ebase + 0x0c) = *(int *)(iVar3 + 4 + (int)DAT_00487828) << 0x12;
                    *(int *)(ebase + 0x10) = 0;
                    *(int *)(ebase + 0x14) = 0;
                    *(unsigned char *)(ebase + 0x21) = 0x19;
                    *(unsigned short *)(ebase + 0x24) = 0;
                    *(unsigned char *)(ebase + 0x20) = 0;

                    iVar9 = rand();
                    *(char *)(ebase + 0x26) = (char)(iVar9 % 0x3c) + 2;
                    *(unsigned char *)(ebase + 0x22) = 0xFF;
                    *(int *)(ebase + 0x28) = 0;

                    unsigned char etype = *(unsigned char *)(iVar3 + 10 + (int)DAT_00487828);
                    *(int *)(ebase + 0x38) =
                        *(int *)((int)DAT_00487abc + 0x34e0 + (unsigned int)etype * 4);
                    *(int *)(ebase + 0x44) =
                        *(int *)((int)DAT_00487abc + 0x351c + (unsigned int)etype * 4);
                    *(int *)(ebase + 0x48) = 0;
                    *(int *)(ebase + 0x4c) =
                        *(int *)((int)DAT_00487abc + 0x354c + (unsigned int)etype * 4);
                    *(unsigned char *)(ebase + 0x54) = 0;
                    *(unsigned char *)(ebase + 0x40) = etype;
                    *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + 0x3458);
                    *(int *)(ebase + 0x3c) = 0;
                    *(unsigned char *)(ebase + 0x5c) = 0;

                    DAT_00489248 = DAT_00489248 + 1;
                    iVar11 = (int)DAT_00487828;

                    if (etype == 0) {
                        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x54) = 1;
                        *(unsigned char *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x5a) = 0;
                        iVar11 = (int)DAT_00487828;
                    }
                }
                break;

            case 3:
                /* Team base position — store in local arrays */
                if (local_90 < 128) {
                    local_690[local_90] = *(int *)(iVar3 + iVar11);
                    local_490[local_90] = *(int *)(iVar3 + 4 + iVar11);
                    local_290[local_90] = (unsigned int)*(unsigned char *)(iVar3 + 10 + iVar11);
                    local_90 = local_90 + 1;
                    iVar15++;
                    continue;
                }
                break;

            case 4:
                /* Turret decoration */
                if (DAT_00483834 != 0 && DAT_004892d8 < 0x78) {
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10) =
                        (*(int *)(iVar3 + iVar11) / 3) * 3;
                    *(int *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 4) =
                        (*(int *)(iVar3 + 4 + (int)DAT_00487828) / 3) * 3;
                    *(unsigned char *)((int)DAT_00487aa0 + DAT_004892d8 * 0x10 + 0xc) = 2;
                    DAT_004892d8 = DAT_004892d8 + 1;
                    iVar11 = (int)DAT_00487828;
                }
                break;
            }
            break;

        case 3:
            /* Spawn point */
            if (DAT_004892d4 < 0xfa) {
                *(int *)((int)DAT_004876a0 + DAT_004892d4 * 0xc) =
                    *(int *)(iVar3 + iVar11);
                *(int *)((int)DAT_004876a0 + 4 + DAT_004892d4 * 0xc) =
                    *(int *)(iVar3 + 4 + (int)DAT_00487828);
                *(unsigned char *)((int)DAT_004876a0 + 8 + DAT_004892d4 * 0xc) =
                    *(unsigned char *)(iVar3 + 9 + (int)DAT_00487828);
                DAT_004892d4 = DAT_004892d4 + 1;
                iVar11 = (int)DAT_00487828;
            }
            break;

        case 4: {
            /* Boss/special entity */
            cVar8 = 0;
            cVar14 = 0;
            for (iVar9 = 0; iVar9 < DAT_00489278; iVar9++) {
                if (*(char *)(iVar11 + 8 + iVar9 * 0x14) == 4) {
                    if (*(char *)(iVar3 + 0xb + iVar11) == *(char *)(iVar11 + iVar9 * 0x14 + 9)) {
                        iVar9 = DAT_00489278;
                        cVar14 = cVar8;
                    }
                    cVar8 = cVar8 + 1;
                }
            }
            int *piVar1 = (int *)(iVar11 + iVar15 * 0x14);
            FUN_00407080(piVar1[0] << 0x12, piVar1[1] << 0x12, cVar14,
                         *(unsigned char *)(iVar11 + 10 + iVar15 * 0x14));
            iVar11 = (int)DAT_00487828;
            break;
        }
        }

        iVar15++;
    }

    /* ===== Phase 6: Post-placement water level ===== */
    DAT_004892d0 = 0.0f;
    if (DAT_00483735 == 2 ||
        (DAT_00483735 == 1 && (rand() % 100 < (int)(unsigned int)DAT_00483962))) {
        iVar11 = rand() % 6;
        DAT_004892d0 = (float)DAT_004879f0;
        if (iVar11 == 0) {
            DAT_004892d0 = DAT_004892d0 * WATER_SCALE_2;
        } else if (iVar11 == 1) {
            DAT_004892d0 = DAT_004892d0 * WATER_SCALE_1;
        } else if (iVar11 == 2) {
            DAT_004892d0 = DAT_004892d0 * WATER_SCALE_0;
        } else {
            DAT_004892d0 = 0.0f;
        }
    }

    /* ===== Phase 7: Team base spawning (if bases exist and in network mode) ===== */
    if (local_90 != 0 && DAT_0048764a != 0) {
        FUN_00406d20(local_690[0] << 0x12, local_490[0] << 0x12, 7, 0xea6000,
                     (char)local_290[0], 0);
        if (local_290[0] == 1) {
            *(int *)((int)DAT_00481f28 + DAT_00489260 * 0x40 - 0x2c) = 0x465000;
            *(int *)((int)DAT_00481f28 + DAT_00489260 * 0x40 - 0x30) = 0x465000;
        }
    }

    /* ===== Phase 8: Spawn point optimization (team mode) ===== */
    if (DAT_0048373c != 0) {
        DAT_004892d4 = 0;
        local_5c = -1;
        local_18a0[0] = 0;
        local_18a0[0x181] = 0;
        local_18a0[0x302] = 0;
        local_18a0[0x483] = 0;

        /* Build per-team base position lists */
        for (iVar11 = 0; iVar11 < local_90; iVar11++) {
            if ((unsigned char)local_290[iVar11] == 3) {
                /* Team 3 = all teams */
                for (uVar5 = 0; (int)uVar5 < 4; uVar5++) {
                    unsigned int *puVar2 = local_18a0 + uVar5 * 0x181;
                    int tidx = uVar5 * 0x181;
                    aiStackY_1ea0[tidx + local_18a0[uVar5 * 0x181]] = local_690[iVar11];
                    aiStackY_1ca0[tidx + *puVar2] = local_490[iVar11];
                    local_18a0[tidx + *puVar2 + 0x80] = uVar5;
                    *puVar2 = *puVar2 + 1;
                }
            } else {
                uVar5 = (unsigned int)(unsigned char)local_290[iVar11];
                unsigned int *puVar2 = local_18a0 + uVar5 * 0x181;
                int tidx = uVar5 * 0x181;
                aiStackY_1ea0[tidx + local_18a0[uVar5 * 0x181]] = local_690[iVar11];
                aiStackY_1ca0[tidx + *puVar2] = local_490[iVar11];
                local_18a0[tidx + *puVar2 + 0x80] = uVar5;
                *puVar2 = *puVar2 + 1;
            }
        }

        /* 5000-iteration optimization loop */
        for (iVar11 = 0; iVar11 < 5000; iVar11++) {
retry_spawn:
            /* Generate candidate spawn positions per team */
            for (iVar15 = 0; iVar15 < 4; iVar15++) {
                stack_c8[iVar15] = 0;
            }

            for (iVar15 = 0; iVar15 < DAT_00489240; iVar15++) {
                uVar5 = (unsigned int)*(unsigned char *)(DAT_00487810 + 0x2c + iVar15 * 0x598);
                if (stack_c8[uVar5] == 0) {
                    iVar3 = 0;
                    do {
                        uVar6 = local_18a0[uVar5 * 0x181];
                        if (uVar6 == 0 || iVar3 > 7) {
                            iVar9 = rand();
                            iVar9 = iVar9 % ((int)DAT_004879f0 - 0xe) + 7;
                            iVar4 = rand();
                            iVar4 = iVar4 % ((int)DAT_004879f4 - 0xe) + 7;
                        } else {
                            iVar3++;
                            iVar9 = rand();
                            iVar4 = iVar9 % (int)uVar6 + uVar5 * 0x181;
                            uVar6 = rand();
                            uVar6 = uVar6 & 0x8000003F;
                            if ((int)uVar6 < 0) {
                                uVar6 = (uVar6 - 1 | 0xFFFFFFC0) + 1;
                            }
                            iVar9 = aiStackY_1ea0[iVar4] - uVar6 + 0x20;
                            uVar6 = rand();
                            uVar6 = uVar6 & 0x8000003F;
                            if ((int)uVar6 < 0) {
                                uVar6 = (uVar6 - 1 | 0xFFFFFFC0) + 1;
                            }
                            iVar4 = aiStackY_1ca0[iVar4] - uVar6 + 0x20;
                            if (iVar9 < 0) iVar9 = 0;
                            if ((int)DAT_004879f0 <= iVar9) iVar9 = (int)DAT_004879f0 - 1;
                            if (iVar4 < 0) iVar4 = 0;
                            if ((int)DAT_004879f4 <= iVar4) iVar4 = (int)DAT_004879f4 - 1;
                        }
                    } while (*(char *)((unsigned int)*(unsigned char *)(
                        (iVar4 << ((unsigned char)DAT_00487a18 & 0x1F)) + (int)DAT_0048782c + iVar9) *
                        0x20 + (int)DAT_00487928) == 0);

                    aiStackY_58[uVar5] = iVar9;
                    aiStackY_48[uVar5] = iVar4;
                    stack_c8[uVar5] = 1;
                }
            }

            /* Find minimum distance between any two teams */
            iVar15 = 99999999;
            for (iVar3 = 0; iVar3 < 3; iVar3++) {
                iVar9 = iVar3;
                if (stack_c8[iVar3] != 0) {
                    while ((iVar9 = iVar9 + 1) < 4) {
                        if (stack_c8[iVar9] != 0) {
                            iVar4 = aiStackY_58[iVar3] - aiStackY_58[iVar9];
                            if (iVar4 < 0) iVar4 = -iVar4;
                            iVar10 = aiStackY_48[iVar3] - aiStackY_48[iVar9];
                            if (iVar10 < 0) iVar10 = -iVar10;
                            if (iVar10 + iVar4 < iVar15) {
                                iVar15 = iVar10 + iVar4;
                            }
                        }
                    }
                }
            }

            /* If this is better than best so far, validate and save */
            if (local_5c < iVar15) {
                iVar3 = 0;
                for (;;) {
                    if (iVar3 > 3) {
                        /* All passed validation — save as best */
                        local_5c = iVar15;
                        for (iVar15 = 0; iVar15 < 4; iVar15++) {
                            aiStackY_8c[iVar15] = aiStackY_58[iVar15];
                            aiStackY_7c[iVar15] = aiStackY_48[iVar15];
                            aiStackY_6c[iVar15] = stack_c8[iVar15];
                        }
                        break;
                    }
                    if (stack_c8[iVar3] != 0) {
                        /* Validate walkable area around spawn */
                        iVar9 = aiStackY_58[iVar3];
                        iVar4 = aiStackY_48[iVar3];
                        if (iVar9 < 0x19 || (int)DAT_004879f0 - 0x19 <= iVar9 ||
                            iVar4 < 0x3c || (int)DAT_004879f4 - 0x1e <= iVar4) {
                            goto retry_spawn;
                        }
                        int scan = ((iVar4 - 0x32) << ((unsigned char)DAT_00487a18 & 0x1F)) - 0x10 + iVar9;
                        int failed = 0;
                        for (iVar4 = 0; iVar4 < 0x40 && !failed; iVar4 += 4) {
                            for (iVar10 = 0; iVar10 < 0x20; iVar10 += 4) {
                                if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + scan) *
                                    0x20 + (int)DAT_00487928) == 0) {
                                    failed = 1;
                                    break;
                                }
                                scan += 4;
                            }
                            if (!failed) {
                                scan = scan - 0x20 + DAT_00487a00 * 4;
                            }
                        }
                        if (failed) goto retry_spawn;
                    }
                    iVar3++;
                }
            }
        }

        /* Place final team bases and spawn points */
        for (iVar11 = 0; iVar11 < 4; iVar11++) {
            if (aiStackY_6c[iVar11] != 0) {
                FUN_00406d20(aiStackY_8c[iVar11] << 0x12, aiStackY_7c[iVar11] << 0x12,
                             7, 0xea6000, (char)iVar11, 0);
                for (iVar15 = 0; iVar15 < 4; iVar15++) {
                    if (DAT_004892d4 < 0xfa) {
                        iVar3 = rand();
                        *(int *)((int)DAT_004876a0 + DAT_004892d4 * 0xc) =
                            iVar3 % 0x60 - 0x30 + aiStackY_8c[iVar11];
                        iVar3 = rand();
                        *(int *)((int)DAT_004876a0 + 4 + DAT_004892d4 * 0xc) =
                            iVar3 % 0x60 - 0x30 + aiStackY_7c[iVar11];
                        *(char *)((int)DAT_004876a0 + 8 + DAT_004892d4 * 0xc) = (char)iVar11;
                        DAT_004892d4 = DAT_004892d4 + 1;
                    }
                }
            }
        }
    }
}

