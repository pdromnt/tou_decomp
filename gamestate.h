#ifndef TOU_GAMESTATE_H
#define TOU_GAMESTATE_H

#include <stddef.h>
#include <stdint.h>

#include "types.h"

/* ===== Error Strings (matching binary string table) ===== */
/* COMPAT */   #define STR_ERR_RENDER_INIT    "Video renderer initialization failed.\n\nRun with --logging for details."
/* COMPAT */   #define STR_ERR_RENDER_MODE    "Video renderer configuration failed.\n\nRun with --logging for details."
/* 0047F1B0 */ #define STR_ERR_INIT_FILENOTFOUND "Tou init failed!\nPossible reason: File not found.\n\nDo not delete any TOU files.\n\nAlso, be sure to run TOU\nfrom the TOU directory.\n\nRead readme.txt for more information."
/* 0047F14C */ #define STR_ERR_INIT_NOLEVELS  "Tou init failed!\nYou don't have any levels or GG themes!\n\nYou can't run the game without levels.\n\nRead readme.txt for more information."
/* 0047F018 */ #define STR_TITLE              "Tunnels of Underworld - RE/Decompiled - v0.4"

/* ===== Window / App Globals (main.cpp) ===== */
extern int                   g_bIsActive;       /* 00489EC4 */

typedef enum GameState : unsigned char {
    GAME_STATE_GAMEPLAY = 0x00,
    GAME_STATE_RENDER_GAMEPLAY = 0x01,
    GAME_STATE_RETURN_TO_MENU = 0x02,
    GAME_STATE_INIT_GAMEPLAY = 0x03,
    GAME_STATE_QUICK_RESTART = 0x04,
    GAME_STATE_GAME_OVER = 0x05,
    GAME_STATE_ERROR_RESTART = 0x06,
    GAME_STATE_INTRO_INIT = 0x96,
    GAME_STATE_INTRO_RUN = 0x97,
    GAME_STATE_NEW_GAME = 0x98,
    GAME_STATE_SHUTDOWN = 0xfe,
    GAME_STATE_STOPPED = 0xff
} GameState;

typedef enum GameplaySubState : unsigned char {
    GAMEPLAY_ACTIVE = 0,
    GAMEPLAY_PAUSED = 1,
    GAMEPLAY_EXIT_MENU = 2,
    GAMEPLAY_ROUND_COMPLETE = 3,
    GAMEPLAY_LEVEL_PREVIEW = 4,
    GAMEPLAY_LEVEL_ADVANCE = 100,
    GAMEPLAY_MATCH_COMPLETE = 101
} GameplaySubState;

extern GameState             g_GameState;       /* 004877A0 - main state machine */
void GameState_Transition(GameState next_state);
void Request_App_Quit(void);

/* ===== Sub-State Globals (gameloop.cpp) ===== */
extern char                  g_MouseButtons;    /* 004877BE */
extern unsigned char         g_ProcessInput;    /* 00489295 */
extern GameplaySubState      g_SubState;        /* 00489296 */
extern unsigned char         g_NeedsRedraw;     /* 00489297 */
extern unsigned char         g_SurfaceReady;    /* 00489298 */
extern unsigned char         g_SubState2;       /* 00489299 */
extern uint32_t              DAT_00489ee8;      /* Key repeat cooldown timestamp */
extern unsigned int          DAT_00489eec;      /* Last pressed key scan code */

/* ===== Timing (main.cpp) ===== */
extern uint32_t              g_TimerStart;      /* 004892B0 */
extern int                   g_TimerAux;        /* 004892B4 */

/* ===== Config (init.cpp) ===== */
extern unsigned char         DAT_00487640[4];   /* Display mode */
extern uint32_t              g_FrameTimer;      /* 004877F4 */
extern unsigned char         DAT_004877b1;
extern unsigned char         DAT_004877a4;
extern uint32_t              DAT_004892b8;
extern unsigned int          DAT_004892bc;      /* elapsed round time (ms) */
extern float                 DAT_004877d4;      /* scroll position (0.0 - 1.0) */

/* ===== Menu / Session (init.cpp / FUN_0042d8b0) ===== */
extern char                **g_MenuStrings;     /* 00481D3C - 350-entry menu text table */
extern void                 *g_GameViewData;    /* 00481D40 - game view item array */
extern char                **g_KeyNameTable;    /* 00481D88 - 256-entry scan code name table */
extern unsigned char         g_KeyOrderTable[47]; /* 00481D48 - key sort/priority order */
extern unsigned char         DAT_00481d84;      /* extra key order byte */
extern unsigned char         g_KeyboardState[256]; /* 00481D8C - legacy scan-code state */

/* ===== Additional State Globals ===== */
extern int                   DAT_004877a8;      /* active menu item count */
extern unsigned char         DAT_004877bc;      /* input flag */
extern unsigned char         DAT_004877bd;      /* input flag */
extern unsigned char         DAT_004877c4;      /* render flag */
extern unsigned char         DAT_004877c9;      /* frame sub-index */
extern int                   DAT_004877cc;      /* scroll/hover decay counter */
extern unsigned char         DAT_004877e5;      /* input event trigger */
extern unsigned char         DAT_004877ec;      /* input accumulator */
extern int                   DAT_00487824;      /* menu display state */
extern unsigned char         g_WindowMode;      /* 0=windowed, 1=borderless fullscreen */
extern int                   DAT_00487784;      /* turret count */
extern int                   DAT_00489e9c;      /* menu/game counter */

/* ===== Menu Scrollbar (init.cpp) ===== */
extern int                   DAT_004877d8;      /* scrollbar area width */
extern int                   DAT_004877dc;      /* scrollbar area top */
extern int                   DAT_004877e0;      /* scrollbar area height */
extern int                   DAT_004877ac;      /* scroll item start index */
extern int                   DAT_004877b0;      /* scroll mode */

/* ===== Gameplay Tick (gameloop.cpp) ===== */
extern char                  DAT_00489288;       /* sub-frame counter (0-7) */

/* ===== Difficulty / Team Config ===== */
extern int                   DAT_004892a8;       /* difficulty constant 1 (round length) */
extern int                   DAT_004892ac;       /* difficulty constant 2 */

/* ===== Debug Logging (enabled with --logging launch arg) ===== */
extern int g_LogEnabled;
void Log(const char *format, ...);
#define LOG Log

/* ===== Function Prototypes: init.cpp ===== */
void Early_Init_Vars(void);
int  System_Init_Check(void);
void Init_Game_Config(void);
void Set_Config_Defaults(void);      /* hardcoded defaults → g_ConfigBlob (no I/O) */
void Reset_Config_To_Defaults(void); /* defaults → g_ConfigBlob → options.cfg → globals */
void Init_Math_Tables(int *buffer, unsigned int count);
void FUN_0041a8c0(void);          /* session/level init */
void FUN_0045c300(void);          /* game mode presets (local) */

/* ===== Function Prototypes: memory.cpp ===== */
void  Init_Memory_Pools(void);
void *Mem_Alloc(size_t size);
void  Mem_Free(void *ptr);

/* ===== Function Prototypes: gameloop.cpp ===== */
void Game_State_Manager(void);
void Game_Update_Render(void);
void Handle_Menu_State(void);
void Intro_Sequence(void);
int  Init_New_Game(void);
int  FUN_00423150(void);       /* Load sprites from all3.gfx */
void Free_Game_Resources(void);
void FUN_0045e1f0(void);       /* pre-tick entity flag reset */

/* ===== Function Prototypes: menu.cpp ===== */
int  Menu_Init_And_Loop(void);
int  Load_Level_Resources(void);
void FUN_004102b0(void);
void FUN_0041bc50(void);
void FUN_0041b010(void);
void FUN_0041b5d0(void);
void FUN_0041bad0(void);
void FUN_0041bb00(void);
int  FUN_004249c0(void);
void FUN_00424240(int ship_type, int ship_index, int color_r, int color_g, int color_b, int palette_index);
void FUN_0041bfe0(void);
void FUN_00407210(int x, int y, int vx, int vy, char dir, int speed, unsigned char type, char subtype);
void FUN_00406d20(int x, int y, char type, int health, unsigned char team, unsigned char orientation);
void FUN_00407400(int x, int y, char facing, unsigned char sprite, char mirror, unsigned char team);
void FUN_00407080(int x, int y, unsigned char index, unsigned char type);
void FUN_00407140(int x, int y, unsigned char type);
void FUN_00440ba0(int x, int y, int team, char param);
void FUN_00457c70(int index);
int  FUN_00410030(void);        /* conditional entity spawn */
void FUN_0041a370(void);        /* player stat scaling */
void FUN_0041bed0(void);        /* difficulty constants */
void FUN_00451500(void);        /* team initialization */
void FUN_0041d2e0(void);        /* edge detection */
void FUN_0041aea0(void);        /* player spawn init */
void FUN_00449040(char param);  /* visibility map (0=incremental, 1=full) */

/* ===== Function Prototypes: init.cpp (config) ===== */
void Load_Options_Config(void);   /* reads options.cfg → g_ConfigBlob */
void Save_Options_Config(void);   /* writes g_ConfigBlob → options.cfg */

/* ===== Utility functions (init.cpp) ===== */
void FUN_004644af(char *dest, const unsigned char *format, ...);
void FUN_00425840(void);
void FUN_004265e0(int index);

/* ===== Stub Prototypes (undecompiled functions) ===== */
void FUN_0041eae0(void);
/* FUN_0045a060 and FUN_0045b2a0 moved to effects.cpp prototypes above */
void FUN_0041fc10(void);
void FUN_0041f900(void);
void FUN_0042d8b0(void);  /* Session/UI init (init.cpp) */
int  FUN_00422740(void);
int  FUN_004252d0(void);    /* Load pal.col + shipal.col color palettes */
void FUN_00420be0(void);
void FUN_0041e580(void);
int  FUN_00414060(void);
void FUN_00413f70(void);
void FUN_0041e4a0(void);
void FUN_0045d7d0(void);
void FUN_00425fe0(void);  /* Main game/menu render loop (init.cpp) */
void FUN_0042a470(void);  /* Menu page builder */
void FUN_00426650(void);  /* Game/menu logic tick */
void FUN_00427df0(int item_idx, char click_type); /* Menu item click handler */
void FUN_00427a70(int item_idx); /* Input mode key assignment handler */
int  FUN_00430200(int x, int y, int string_idx, int color_style, int font_idx,
                  unsigned char clickable, unsigned char render_mode,
                  unsigned char alignment, unsigned char nav_target);
void FUN_0042ff80(int x, int y, int sprite_idx, unsigned char clickable,
                  unsigned char render_mode, unsigned char alignment,
                  unsigned char nav_target);
void FUN_0042fc90(int value);
void FUN_0042fcf0(void);
int  FUN_0042fdf0(int y);
void FUN_0042fcb0(void);
int  FUN_0042fc40(void);
void FUN_0042fc10(void);
void FUN_0041d740(void);

#endif /* TOU_GAMESTATE_H */
