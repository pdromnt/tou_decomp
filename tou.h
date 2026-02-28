#ifndef TOU_H
#define TOU_H

#define DIRECTDRAW_VERSION 0x0100
#define DIRECTINPUT_VERSION 0x0700

#include <windows.h>
#include <ddraw.h>
#include <dinput.h>
#include <mmsystem.h>

#include "fmod_mock.h"

/* ===== Error Strings (matching binary string table) ===== */
/* 0047F0EC */ #define STR_ERR_DDRAW_INSTALL  "DirectDraw Init FAILED.\nInstall DirectX 7.0 to play TOU.\n\nRead readme.txt for more\ninformation."
/* 0047EF74 */ #define STR_ERR_DDRAW_MODE     "DirectDraw Init FAILED.\nCouldn't set video mode to 640x480 16-bit.\nYour video card must support this\nresolution to play TOU.\nRead readme.txt for more information"
/* 0047F098 */ #define STR_ERR_DDRAW_MEMORY   "DirectDraw Init FAILED.\nNot enough memory.\n\nRead readme.txt for more\ninformation."
/* 0047F058 */ #define STR_ERR_DINPUT         "DirectInput Init FAILED.\n\nRead readme.txt for more\ninformation."
/* 0047F048 */ #define STR_ERR_UNKNOWN        "Unknown error."
/* 0047F1B0 */ #define STR_ERR_INIT_FILENOTFOUND "Tou init failed!\nPossible reason: File not found.\n\nDo not delete any TOU files.\n\nAlso, be sure to run TOU\nfrom the TOU directory.\n\nRead readme.txt for more information."
/* 0047F14C */ #define STR_ERR_INIT_NOLEVELS  "Tou init failed!\nYou don't have any levels or GG themes!\n\nYou can't run the game without levels.\n\nRead readme.txt for more information."
/* 0047F018 */ #define STR_TITLE              "TOU v0.1"
/* 0047EB10 */ #define STR_CLASSNAME          "TOU"

/* ===== Structures ===== */

typedef struct {
    unsigned int handle;    /* FSOUND_SAMPLE* cast to uint */
    unsigned char volume;
    unsigned char padding[3];
} SoundEntry;

typedef struct {
    int pixel_offset;
    int width;
    int height;
    int unknown;
} FontChar;

typedef struct {
    int x;              /* 0x00 */
    int y;              /* 0x04 */
    int width;          /* 0x08 */
    int height;         /* 0x0C */
    int type;           /* 0x10: 0=text, 1=sprite */
    int color_style;    /* 0x14: color/style for rendering */
    char font_idx;      /* 0x18 */
    char _pad1[3];
    int string_idx;     /* 0x1C: index into g_MenuStrings */
    int hover_state;    /* 0x20: hover/scroll animation */
    unsigned char flag1;       /* 0x24 */
    unsigned char clickable;   /* 0x25 */
    unsigned char nav_target;  /* 0x26: target page (0xFF=none) */
    unsigned char _pad2;
    int linked_item;    /* 0x28 */
    unsigned char render_mode; /* 0x2C */
    unsigned char _pad3[3];
    int extra_data;     /* 0x30 */
} MenuItem; /* 0x34 = 52 bytes, max 350 items in g_GameViewData */

/* ===== DirectDraw Globals (graphics.cpp) ===== */
extern LPDIRECTDRAW          lpDD;              /* 00489EC8 */
extern LPDIRECTDRAWSURFACE   lpDDS_Primary;     /* 00489ED8 */
extern LPDIRECTDRAWSURFACE   lpDDS_Back;        /* 00489ECC */
extern LPDIRECTDRAWSURFACE   lpDDS_Offscreen;   /* 00489ED0 */

/* ===== DirectInput Globals (init.cpp) ===== */
extern LPDIRECTINPUT         lpDI;              /* 00489ED4 */
extern LPDIRECTINPUTDEVICE   lpDI_Keyboard;     /* 00489EE4 */
extern LPDIRECTINPUTDEVICE   lpDI_Mouse;        /* 00489EC0 */
extern HANDLE                hMouseEvent;       /* 00489EE0 */

/* ===== Window / App Globals (winmain.cpp) ===== */
extern HWND                  hWnd_Main;         /* 00489EDC */
extern int                   g_bIsActive;       /* 00489EC4 */

/* ===== Game State (winmain.cpp) ===== */
extern unsigned char         g_GameState;       /* 004877A0 - main state machine */

/* ===== Sub-State Globals (gameloop.cpp) ===== */
extern char                  g_MouseButtons;    /* 004877BE */
extern unsigned char         g_ProcessInput;    /* 00489295 */
extern unsigned char         g_SubState;        /* 00489296 */
extern unsigned char         g_NeedsRedraw;     /* 00489297 */
extern unsigned char         g_SurfaceReady;    /* 00489298 */
extern unsigned char         g_SubState2;       /* 00489299 */

/* ===== Timing (winmain.cpp) ===== */
extern DWORD                 g_TimerStart;      /* 004892B0 */
extern int                   g_TimerAux;        /* 004892B4 */

/* ===== Frame / Render (memory.cpp) ===== */
extern unsigned short       *Software_Buffer;   /* 004877C0 */
extern unsigned char         g_FrameIndex;      /* 004877C8 */
extern char                  g_LoadedBgIndex;   /* 0048769C */
extern int                   g_IntroSplashIndex;/* 0048924C */

/* ===== Sound (sound.cpp) ===== */
extern SoundEntry           *g_SoundTable;      /* 00487874 */
extern FSOUND_STREAM        *g_MusicStream;     /* 004806F8 */
extern int                   g_MusicChannel;    /* 004806FC */
extern int                   g_SoundEnabled;    /* 00487649 */

/* ===== Memory (memory.cpp) ===== */
extern int                   g_MemoryTracker;   /* 004892A0 */
extern int                  *g_PhysicsParams;   /* 00487880 */
extern int                  *g_EntityConfig;    /* 00489EBC */
extern void                 *DAT_00487ab0;      /* Math table buffer */

/* ===== Image Loader (assets.cpp) ===== */
extern int                   g_ImageWidth;      /* 00481D08 */
extern int                   g_ImageHeight;     /* 00481CFC */
extern int                   g_ImageBPP;        /* 00481D04 */
extern int                   g_ImageSize;       /* 00481D00 */

/* ===== Font (assets.cpp) ===== */
extern FontChar              Font_Char_Table[1024];
extern unsigned char        *Font_Pixel_Data;

/* ===== Mouse Input (gameloop.cpp) ===== */
extern int                   g_MouseDeltaX;     /* 004877B4 */
extern int                   g_MouseDeltaY;     /* 004877B8 */
extern char                  g_InputMode;       /* 004877E4 */
extern int                   DAT_004877e8;

/* ===== Display Mode (init.cpp) ===== */
extern int                   g_DisplayWidth;    /* 00489238 */
extern int                   g_DisplayHeight;   /* 0048923C */
extern int                   g_NumDisplayModes; /* 00483C00 */

/* ===== Config (init.cpp) ===== */
extern unsigned char         DAT_00483720[8];   /* Sound config */
extern unsigned char         DAT_00487640[4];   /* Display mode */
extern unsigned short        DAT_00483838[4];   /* Team color palette (RGB555) */
extern DWORD                 g_FrameTimer;      /* 004877F4 */
extern unsigned char         DAT_004877b1;
extern unsigned char         DAT_004877a4;
extern DWORD                 DAT_004892b8;

/* ===== Intro particle system (memory.cpp) ===== */
extern int                   DAT_00489248;      /* Entity count */
extern int                   DAT_00489250;      /* Particle count */
extern int                   DAT_0048925c;      /* Misc counter */
extern DWORD                 DAT_004877f0;      /* Frame delta time */

/* ===== Viewport (effects.cpp) ===== */
extern int                   DAT_004806dc;      /* viewport left */
extern int                   DAT_004806d0;      /* viewport right */
extern int                   DAT_004806e0;      /* viewport top */
extern int                   DAT_004806d4;      /* viewport bottom */
extern int                   DAT_004806d8;      /* viewport width */
extern int                   DAT_004806e4;      /* viewport height */
extern int                   DAT_004806e8;      /* camera/scroll Y */
extern int                   DAT_004806ec;      /* camera/scroll X */

/* ===== Sprite data tables (memory.cpp) ===== */
extern void                 *DAT_00487ab4;      /* Sprite pixel data RGB555 (2.8MB) */
extern void                 *DAT_00489e94;      /* Sprite grayscale data (40KB) */
extern void                 *DAT_00489234;      /* Sprite frame offsets (80KB, 20000 ints) */
extern void                 *DAT_00489e8c;      /* Sprite widths (20KB, bytes) */
extern void                 *DAT_00489e88;      /* Sprite heights (20KB, bytes) */
extern void                 *DAT_00481cf8;      /* Temp buffer (3.6MB) */
extern int                   DAT_00481d28;      /* Sprite RGB pixel write cursor */
extern int                   DAT_00481d24;      /* Sprite grayscale pixel write cursor */

/* ===== Entity type definitions (memory.cpp) ===== */
extern void                 *DAT_00487abc;      /* Entity type table (0x11030 bytes, 128 types * 0x218 each) */

/* ===== Explosion/Particle data buffers (memory.cpp) ===== */
extern void                 *DAT_00481f20;      /* Explode descriptor table (160 bytes) */
extern void                 *DAT_00481f34;      /* Particle array (32 bytes * 2000) */
extern void                 *DAT_0048787c;      /* Explode pixel data (430KB) */
extern void                 *DAT_004892e8;      /* Entity array (128 bytes * 2500) */
extern void                 *DAT_00489230;      /* Brightness remap LUT (128KB) */
extern void                 *DAT_004876a4[100]; /* Color palette tables */
extern void                 *DAT_0048792c[48];  /* Blend LUT tables */

/* ===== Menu / Session (init.cpp / FUN_0042d8b0) ===== */
extern char                **g_MenuStrings;     /* 00481D3C - 350-entry menu text table */
extern void                 *g_GameViewData;    /* 00481D40 - game view item array */
extern char                **g_KeyNameTable;    /* 00481D88 - 256-entry scan code name table */
extern unsigned char         g_KeyOrderTable[47]; /* 00481D48 - key sort/priority order */
extern unsigned char         DAT_00481d84;      /* extra key order byte */
extern unsigned char         g_KeyboardState[256]; /* 00481D8C - DirectInput keyboard state */

/* ===== Additional State Globals ===== */
extern unsigned char         DAT_004877a8;      /* game sub-flag */
extern unsigned char         DAT_004877bc;      /* input flag */
extern unsigned char         DAT_004877bd;      /* input flag */
extern unsigned char         DAT_004877c4;      /* render flag */
extern unsigned char         DAT_004877c9;      /* frame sub-index */
extern unsigned char         DAT_004877cc;      /* render flag */
extern unsigned char         DAT_004877e5;      /* input event trigger */
extern unsigned char         DAT_004877ec;      /* input accumulator */
extern int                   DAT_00487824;      /* menu display state */
extern unsigned char         DAT_00483724[4];   /* display config */
extern int                   DAT_00487784;      /* turret count */
extern unsigned char         DAT_00483834;      /* turret flag */
extern unsigned char         DAT_00483835;      /* trooper flag */
extern int                   DAT_00489e9c;      /* menu/game counter */

/* ===== Menu Scrollbar (init.cpp) ===== */
extern int                   DAT_004877d8;      /* scrollbar area width */
extern int                   DAT_004877dc;      /* scrollbar area top */
extern int                   DAT_004877e0;      /* scrollbar area height */
extern int                   DAT_004877ac;      /* scroll item start index */
extern int                   DAT_004877b0;      /* scroll mode */

/* ===== Misc ===== */
extern int                   DAT_00481f48;

/* ===== Debug Logging ===== */
void Log(const char *format, ...);
#define LOG Log

/* ===== Function Prototypes: winmain.cpp ===== */
extern "C" LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int Handle_Init_Error(HWND hWnd, unsigned char errorCode);

/* ===== Function Prototypes: init.cpp ===== */
void Early_Init_Vars(void);
int  System_Init_Check(void);
int  Init_DirectInput(void);
void Init_Game_Config(void);
void Init_Math_Tables(int *buffer, unsigned int count);

/* ===== Function Prototypes: memory.cpp ===== */
void  Init_Memory_Pools(void);
void *Mem_Alloc(size_t size);
void  Mem_Free(void *ptr);

/* ===== Function Prototypes: graphics.cpp ===== */
int  Init_DirectDraw(int width, int height);
void Render_Frame(void);
int  Render_Game_View(void);
void Render_Game_View_To(unsigned short *frame);
void Release_DirectDraw_Surfaces(void);
void Restore_Surfaces(void);

/* ===== Function Prototypes: sound.cpp ===== */
int  Init_Sound_Hardware(void);
void FUN_0040e130(void);
void Load_Game_Sounds(void);
void Play_Music(void);
void Stop_All_Sounds(void);
void Pause_Audio_Streams(void);
void Cleanup_Sound(void);

/* ===== Function Prototypes: assets.cpp ===== */
int   Load_Background_To_Buffer(char index);
void *Load_JPEG_Asset(const char *filename, int *width, int *height);
void  Load_Fonts(void);
void  Draw_Text_To_Buffer(const char *str, int font_idx, int color_idx,
                          unsigned short *dest_buf, int stride, int hover,
                          int max_width, int len);

/* ===== Function Prototypes: gameloop.cpp ===== */
void Game_State_Manager(void);
void Game_Update_Render(void);
void Input_Update(void);
void Handle_Menu_State(void);
void Intro_Sequence(void);
int  Init_New_Game(void);
void Free_Game_Resources(void);

/* ===== Function Prototypes: effects.cpp ===== */
void FUN_0045a060(void);
void FUN_0045b2a0(void);
int  FUN_00422fc0(void);
void FUN_0040d100(int buffer, int stride);
void FUN_004076d0(int buffer, int stride);
int  FUN_004257e0(int cx, int cy, int px, int py);

/* ===== Function Prototypes: menu.cpp ===== */
int  Menu_Init_And_Loop(void);
int  Load_Level_Resources(void);
void FUN_004102b0(void);
void FUN_0041bc50(void);

/* ===== Function Prototypes: init.cpp (config) ===== */
void Load_Options_Config(void);
void Save_Options_Config(void);

/* ===== Stub Prototypes (undecompiled functions) ===== */
void FUN_0041eae0(void);
/* FUN_0045a060 and FUN_0045b2a0 moved to effects.cpp prototypes above */
void FUN_0041fc10(void);
void FUN_0041fe70(void);
void FUN_0041f900(void);
void FUN_00422a10(void);
void FUN_0042d8b0(void);  /* Full implementation in init.cpp */
int  FUN_00422740(void);
void FUN_00420be0(void);
void FUN_0041e580(void);
int  FUN_00414060(void);
void FUN_00413f70(void);
void FUN_0041e4a0(void);
void FUN_0045d7d0(void);
void FUN_00425fe0(void);  /* Main game/menu render loop - implemented in init.cpp */
void FUN_0042a470(void);  /* Menu page builder */
void FUN_00426650(void);  /* Game/menu logic tick */
void FUN_00427df0(int item_idx, char click_type); /* Menu item click handler */
void FUN_00427a70(int item_idx); /* Input mode key assignment handler (stub) */
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
void FUN_0041a8c0(void);
void FUN_0041d740(void);

#endif /* TOU_H */
