/*
 * init.cpp - System initialization, DirectInput, game config
 * Addresses: System_Init_Check=0041D480, Early_Init_Vars=0041EAD0,
 *            Init_DirectInput=004620F0, Init_Game_Config=004207C0
 */
#include "tou.h"
#define INITGUID
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===== Globals defined in this module ===== */

/* DirectInput */
LPDIRECTINPUT        lpDI           = NULL;  /* 00489ED4 */
LPDIRECTINPUTDEVICE  lpDI_Keyboard  = NULL;  /* 00489EE4 */
LPDIRECTINPUTDEVICE  lpDI_Mouse     = NULL;  /* 00489EC0 */
HANDLE               hMouseEvent    = NULL;  /* 00489EE0 */

/* Sound config */
unsigned char DAT_00483720[8] = {0x5A,0x46,1,1, 0x40,0,0,0}; /* [0]=music vol 90%, [1]=SFX vol 70%, [2]=flag, [3]=output type 1=DSOUND */

/* Display mode struct: {flags, current_mode, desired_mode, ...} */
unsigned char DAT_00487640[4] = {0, 0xFF, 5, 0};

/* Display mode table */
int g_DisplayWidth  = 0;    /* 00489238 */
int g_DisplayHeight = 0;    /* 0048923C */
int g_NumDisplayModes = 10; /* 00483C00 */

/* Resolution tables (filled in System_Init_Check) */
int g_ModeWidths[10];    /* 00483C04 */
int g_ModeHeights[10];   /* 00483C44 */

/* Misc init globals */
int          g_SoundEnabled = 0;     /* 00487649 */
DWORD        g_FrameTimer   = 0;     /* 004877F4 */
unsigned char DAT_004877b1  = 0;
unsigned char DAT_004877a4  = 0;
DWORD        DAT_004892b8   = 0;

/* Config globals (004207c0 area) */
unsigned char g_ConfigBlob[6408]; /* 00481F58 - raw config data */

/* Helper: map original binary address to config blob pointer (32-bit only) */
#define CFG_ADDR(a) ((int)(uintptr_t)&g_ConfigBlob[(a) - 0x481F58])

/* Team color palette (RGB555 values, 4 entries) - init'd in Init_Game_Config */
unsigned short DAT_00483838[4] = {0};
unsigned short DAT_00483820 = 0;  /* Fade target color (RGB565) for blend-to-background LUTs */

/* Stub globals referenced in init */
int DAT_0048764b = 0;
int DAT_0048764a = 0;

/* Menu / session init (FUN_0042d8b0) */
char        **g_MenuStrings  = NULL;  /* 00481D3C */
void         *g_GameViewData = NULL;  /* 00481D40 */
char        **g_KeyNameTable = NULL;  /* 00481D88 */
unsigned char g_KeyOrderTable[47] = {0}; /* 00481D48 */
unsigned char DAT_00481d84   = 0;
unsigned char DAT_004877a8   = 0;
unsigned char DAT_004877bc   = 0;
unsigned char DAT_004877bd   = 0;
unsigned char DAT_004877c4   = 0;
unsigned char DAT_004877c9   = 0;  /* byte after g_FrameIndex */
unsigned char DAT_004877cc   = 0;
unsigned char DAT_004877ec   = 0;
int           DAT_00487824   = 0;
unsigned char DAT_00483724[4] = {0};
int           DAT_00487784   = 0;
unsigned char DAT_00483834   = 0;
unsigned char DAT_00483835   = 0;
int           DAT_00489e9c   = 0;
unsigned char g_KeyboardState[256] = {0}; /* 00481D8C - DirectInput keyboard state */
unsigned char DAT_004877e5   = 0;  /* input event trigger */
unsigned char DAT_004877e6   = 0;  /* input mode item index */
float         DAT_004877d4   = 0.0f;  /* scroll position (0.0 - 1.0) */
int           DAT_004877d8   = 0;  /* scrollbar area width */
int           DAT_004877dc   = 0;  /* scrollbar area top */
int           DAT_004877e0   = 0;  /* scrollbar area height */
int           DAT_004877ac   = 0;  /* scroll item start index */
int           DAT_004877b0   = 0;  /* scroll mode */

/* Level/map counts */
int DAT_00485088 = 0;   /* total map/level count */
int DAT_0048508c = 0;   /* GG theme/official level count */

/* ===== Stub functions (to be decompiled later) ===== */
void FUN_0041eae0(void) {}
void FUN_004265e0(int index) { (void)index; } /* Key binding auto-assign - stub */
/* FUN_0045a060 moved to effects.cpp */
/* FUN_0045b2a0 moved to effects.cpp */
void FUN_0041fc10(void) {}
void FUN_0041fe70(void) {}  /* Entity AI function pointers - not needed for intro rendering */
void FUN_0041f900(void) {}
void FUN_00422a10(void) {}
/* ===== FUN_0042d8b0 - Session/UI init (0042D8B0) ===== */
/* Initializes game state for menu, allocates and fills the key name table
 * (256 DirectInput scan code → display string) and menu string table (~350 entries).
 * Sets palette[3] = 0x7FF0 (gold) BEFORE sprite loading - critical for variant 3 color. */
void FUN_0042d8b0(void)
{
    int i;
    void *pvVar;

    /* --- Game state initialization --- */
    g_GameState = 0x96;              /* INTRO_INIT */
    DAT_00483838[3] = 0x7FF0;       /* palette[3] = bright gold (X1R5G5B5) */
    g_MouseDeltaX   = 0x5000000;    /* center mouse X (fixed-point) */
    g_MouseDeltaY   = 0x3c00000;    /* center mouse Y (fixed-point) */
    DAT_004877bc    = 0;
    DAT_004877bd    = 0;
    DAT_004877c9    = 0xFE;          /* frame sub-index */
    DAT_004877a8    = 0;
    DAT_004877c4    = 0;
    DAT_004877a4    = 3;
    DAT_004877b1    = 1;
    g_InputMode     = 0;             /* DAT_004877e4 */
    DAT_004877e8    = 0;
    DAT_004877cc    = 0;
    g_FrameTimer    = timeGetTime(); /* _DAT_004877f4 */
    DAT_004877ec    = 0;

    /* --- Allocate game view data (0x4718 = 18200 bytes) --- */
    g_GameViewData = Mem_Alloc(0x4718);

    /* --- Load font data (needed before any menu page build) --- */
    Load_Fonts();

    /* --- Key sort/priority table (DAT_00481d48..00481d76) --- */
    /* Maps an ordering index to DirectInput scan codes */
    static const unsigned char key_order[47] = {
        0x12, 0x0F, 0x16, 0x06, 0x05, 0x0C, 0x18, 0x01,  /* E,Tab,U,5,4,-,O,Esc */
        0x24, 0x11, 0x15, 0x22, 0x25, 0x10, 0x23, 0x13,  /* J,W,Y,G,K,Q,H,R */
        0x1F, 0x1B, 0x09, 0x27, 0x2D, 0x1D, 0x19, 0x2C,  /* S,],8,;,X,LCtrl,P,Z */
        0x1C, 0x26, 0x1A, 0x2E, 0x0B, 0x17, 0x02, 0x0A,  /* Enter,L,[,C,0,I,1,9 */
        0x1E, 0x28, 0x14, 0x2B, 0x0D, 0x0E, 0x00, 0x07,  /* A,',T,\,=,Bksp,?,6 */
        0x08, 0x03, 0x04, 0x29, 0x2A, 0x20, 0x21          /* 7,2,3,`,LShift,D,F */
    };
    memcpy(g_KeyOrderTable, key_order, 47);
    DAT_00481d84 = 0x2F;  /* DIK_V */

    /* --- Key name table (256 entries, one per DirectInput scan code) --- */
    g_KeyNameTable = (char **)Mem_Alloc(0x400);  /* 256 * 4 = 1024 bytes */

    /* Default: all keys show "???" */
    for (i = 0; i < 256; i++)
        g_KeyNameTable[i] = (char *)"???";

    /* Assign display names for each known scan code */
    /* Row 1: Escape through Backspace */
    g_KeyNameTable[0x01] = (char *)"?";          /* DIK_ESCAPE */
    g_KeyNameTable[0x02] = (char *)"1";          /* DIK_1 */
    g_KeyNameTable[0x03] = (char *)"2";          /* DIK_2 */
    g_KeyNameTable[0x04] = (char *)"3";          /* DIK_3 */
    g_KeyNameTable[0x05] = (char *)"4";          /* DIK_4 */
    g_KeyNameTable[0x06] = (char *)"5";          /* DIK_5 */
    g_KeyNameTable[0x07] = (char *)"6";          /* DIK_6 */
    g_KeyNameTable[0x08] = (char *)"7";          /* DIK_7 */
    g_KeyNameTable[0x09] = (char *)"8";          /* DIK_8 */
    g_KeyNameTable[0x0A] = (char *)"9";          /* DIK_9 */
    g_KeyNameTable[0x0B] = (char *)"0";          /* DIK_0 */
    g_KeyNameTable[0x0C] = (char *)"-";          /* DIK_MINUS */
    g_KeyNameTable[0x0D] = (char *)"=";          /* DIK_EQUALS */
    g_KeyNameTable[0x0E] = (char *)"Backspace";  /* DIK_BACK */
    g_KeyNameTable[0x0F] = (char *)"Tab";        /* DIK_TAB */
    /* Row 2: QWERTY */
    g_KeyNameTable[0x10] = (char *)"Q";
    g_KeyNameTable[0x11] = (char *)"W";
    g_KeyNameTable[0x12] = (char *)"E";
    g_KeyNameTable[0x13] = (char *)"R";
    g_KeyNameTable[0x14] = (char *)"T";
    g_KeyNameTable[0x15] = (char *)"Y";
    g_KeyNameTable[0x16] = (char *)"U";
    g_KeyNameTable[0x17] = (char *)"I";
    g_KeyNameTable[0x18] = (char *)"O";
    g_KeyNameTable[0x19] = (char *)"P";
    g_KeyNameTable[0x1A] = (char *)"[";          /* DIK_LBRACKET */
    g_KeyNameTable[0x1B] = (char *)"]";          /* DIK_RBRACKET */
    g_KeyNameTable[0x1C] = (char *)"Enter";      /* DIK_RETURN */
    g_KeyNameTable[0x1D] = (char *)"Left ctrl";  /* DIK_LCONTROL */
    /* Row 3: ASDF */
    g_KeyNameTable[0x1E] = (char *)"A";
    g_KeyNameTable[0x1F] = (char *)"S";
    g_KeyNameTable[0x20] = (char *)"D";
    g_KeyNameTable[0x21] = (char *)"F";
    g_KeyNameTable[0x22] = (char *)"G";
    g_KeyNameTable[0x23] = (char *)"H";
    g_KeyNameTable[0x24] = (char *)"J";
    g_KeyNameTable[0x25] = (char *)"K";
    g_KeyNameTable[0x26] = (char *)"L";
    g_KeyNameTable[0x27] = (char *)";";          /* DIK_SEMICOLON */
    g_KeyNameTable[0x28] = (char *)"'";          /* DIK_APOSTROPHE */
    g_KeyNameTable[0x29] = (char *)"'";          /* DIK_GRAVE (same string as apostrophe in original) */
    g_KeyNameTable[0x2A] = (char *)"Left shift"; /* DIK_LSHIFT */
    g_KeyNameTable[0x2B] = (char *)"\\";         /* DIK_BACKSLASH */
    /* Row 4: ZXCV */
    g_KeyNameTable[0x2C] = (char *)"Z";
    g_KeyNameTable[0x2D] = (char *)"X";
    g_KeyNameTable[0x2E] = (char *)"C";
    g_KeyNameTable[0x2F] = (char *)"V";
    g_KeyNameTable[0x30] = (char *)"B";
    g_KeyNameTable[0x31] = (char *)"N";
    g_KeyNameTable[0x32] = (char *)"M";
    g_KeyNameTable[0x33] = (char *)",";          /* DIK_COMMA */
    g_KeyNameTable[0x34] = (char *)".";          /* DIK_PERIOD */
    g_KeyNameTable[0x35] = (char *)"/";          /* DIK_SLASH */
    g_KeyNameTable[0x36] = (char *)"Right shift";
    g_KeyNameTable[0x37] = (char *)"* (numpad)";
    g_KeyNameTable[0x38] = (char *)"Left alt";
    g_KeyNameTable[0x39] = (char *)"Spacebar";
    g_KeyNameTable[0x3A] = (char *)"Caps Lock";
    /* Function keys */
    g_KeyNameTable[0x3B] = (char *)"F1";
    g_KeyNameTable[0x3C] = (char *)"F2";
    g_KeyNameTable[0x3D] = (char *)"F3";
    g_KeyNameTable[0x3E] = (char *)"F4";
    g_KeyNameTable[0x3F] = (char *)"F5";
    g_KeyNameTable[0x40] = (char *)"F6";
    g_KeyNameTable[0x41] = (char *)"F7";
    g_KeyNameTable[0x42] = (char *)"F8";
    g_KeyNameTable[0x43] = (char *)"F9";
    g_KeyNameTable[0x44] = (char *)"F10";
    /* Numpad & locks */
    g_KeyNameTable[0x45] = (char *)"Numlock";
    g_KeyNameTable[0x46] = (char *)"Scroll lock";
    g_KeyNameTable[0x47] = (char *)"7 (numpad)";
    g_KeyNameTable[0x48] = (char *)"8 (numpad)";
    g_KeyNameTable[0x49] = (char *)"9 (numpad)";
    g_KeyNameTable[0x4A] = (char *)"- (numpad)";
    g_KeyNameTable[0x4B] = (char *)"4 (numpad)";
    g_KeyNameTable[0x4C] = (char *)"5 (numpad)";
    g_KeyNameTable[0x4D] = (char *)"6 (numpad)";
    g_KeyNameTable[0x4E] = (char *)"+ (numpad)";
    g_KeyNameTable[0x4F] = (char *)"1 (numpad)";
    g_KeyNameTable[0x50] = (char *)"2 (numpad)";
    g_KeyNameTable[0x51] = (char *)"3 (numpad)";
    g_KeyNameTable[0x52] = (char *)"0 (numpad)";
    g_KeyNameTable[0x53] = (char *)". (numpad)";
    g_KeyNameTable[0x56] = (char *)"<";          /* DIK_OEM_102 */
    g_KeyNameTable[0x57] = (char *)"F11";
    g_KeyNameTable[0x58] = (char *)"F12";
    /* Extended keys */
    g_KeyNameTable[0x9C] = (char *)"Enter (numpad)";
    g_KeyNameTable[0x9D] = (char *)"Right ctrl";
    g_KeyNameTable[0xB3] = (char *)". (numpad)"; /* duplicate */
    g_KeyNameTable[0xB5] = (char *)"/ (numpad)";
    g_KeyNameTable[0xB7] = (char *)"SysRq";
    g_KeyNameTable[0xB8] = (char *)"Right alt";
    g_KeyNameTable[0xC5] = (char *)"Pause";
    g_KeyNameTable[0xC7] = (char *)"Home";
    g_KeyNameTable[0xC8] = (char *)"Up arrow";
    g_KeyNameTable[0xC9] = (char *)"Page up";
    g_KeyNameTable[0xCB] = (char *)"Left arrow";
    g_KeyNameTable[0xCD] = (char *)"Right arrow";
    g_KeyNameTable[0xCF] = (char *)"End";
    g_KeyNameTable[0xD0] = (char *)"Down arrow";
    g_KeyNameTable[0xD1] = (char *)"Page down";
    g_KeyNameTable[0xD2] = (char *)"Insert";
    g_KeyNameTable[0xD3] = (char *)"Delete";
    g_KeyNameTable[0xDB] = (char *)"Left winkey";
    g_KeyNameTable[0xDC] = (char *)"Right winkey";
    g_KeyNameTable[0xDD] = (char *)"Application key";

    /* --- Menu string table (350 entries) --- */
    g_MenuStrings = (char **)Mem_Alloc(0x578);  /* 350 * 4 = 1400 bytes */

    /* Allocate 50-byte dynamic buffers for player/stats entries */
    pvVar = Mem_Alloc(0x32);
    g_MenuStrings[0x65] = (char *)pvVar;  /* entry 101 - dynamic text */
    for (i = 0x1C4; i < 0x230; i += 4) {
        pvVar = Mem_Alloc(0x32);
        *(void **)((char *)g_MenuStrings + i) = pvVar;  /* entries [113]-[139] */
    }

    /* Main menu */
    g_MenuStrings[0x00] = (char *)"TOU";
    g_MenuStrings[0x01] = (char *)"Team deathmatch";
    g_MenuStrings[0x02] = (char *)"Levels";
    g_MenuStrings[0x03] = (char *)"Players";
    g_MenuStrings[0x04] = (char *)"Options";
    g_MenuStrings[0x05] = (char *)"Credits";
    g_MenuStrings[0x06] = (char *)"Exit game";
    g_MenuStrings[0x07] = (char *)"Tunnels Of the Underworld";

    /* Options menu */
    g_MenuStrings[0x08] = (char *)"Options menu";
    g_MenuStrings[0x09] = (char *)"Audio";
    g_MenuStrings[0x0A] = (char *)"Details & visuals";
    g_MenuStrings[0x0B] = (char *)"Event";
    g_MenuStrings[0x0C] = (char *)"Main rules";
    g_MenuStrings[0x0D] = (char *)"Misc";
    g_MenuStrings[0x0E] = (char *)"Ban weapons";
    g_MenuStrings[0x0F] = (char *)"Back";

    /* Credits */
    g_MenuStrings[0x10] = (char *)"Programming";
    g_MenuStrings[0x11] = (char *)"Hannu Kankaanp\xe4\xe4";  /* "Hannu Kankaanpää" */
    g_MenuStrings[0x12] = (char *)"Additional programming";
    g_MenuStrings[0x13] = (char *)"Sampsa Lehtonen";
    g_MenuStrings[0x14] = (char *)"Musics and sound effects";
    g_MenuStrings[0x15] = (char *)"Kimmo Palander";
    g_MenuStrings[0x16] = (char *)"Graphics and level design";
    g_MenuStrings[0x17] = (char *)"Documents and home pages";
    g_MenuStrings[0x18] = (char *)"Extra ideas and help";
    g_MenuStrings[0x19] = (char *)"Tapio Raevaara";
    g_MenuStrings[0x1A] = (char *)"Tapio Raevaara";
    g_MenuStrings[0x1B] = (char *)"GG level size";

    /* Details & visuals */
    g_MenuStrings[0x1C] = (char *)"GG level size";
    g_MenuStrings[0x1D] = (char *)"Additional graphics";
    g_MenuStrings[0x1E] = (char *)"Parallax background";
    g_MenuStrings[0x1F] = (char *)"Water";
    g_MenuStrings[0x20] = (char *)"Waves";
    g_MenuStrings[0x21] = (char *)"Run water speed";
    g_MenuStrings[0x22] = (char *)"No";
    g_MenuStrings[0x23] = (char *)"Yes";
    g_MenuStrings[0x24] = (char *)"On";
    g_MenuStrings[0x25] = (char *)"Off";

    /* Players */
    g_MenuStrings[0x26] = (char *)"Civilians";
    g_MenuStrings[0x27] = (char *)"Line of sight";
    g_MenuStrings[0x28] = (char *)"Style";
    g_MenuStrings[0x29] = (char *)"Detail";
    g_MenuStrings[0x2A] = (char *)"Refresh rate";
    g_MenuStrings[0x2B] = (char *)"Color";
    g_MenuStrings[0x2C] = (char *)":";
    g_MenuStrings[0x2D] = (char *)"Rapidfire";
    g_MenuStrings[0x2E] = (char *)"Snowy";
    g_MenuStrings[0x2F] = (char *)"Rainy";
    g_MenuStrings[0x30] = (char *)"Bombing";
    g_MenuStrings[0x31] = (char *)"Explosion color";

    /* Quality levels */
    g_MenuStrings[0x32] = (char *)"Best";
    g_MenuStrings[0x33] = (char *)"Good";
    g_MenuStrings[0x34] = (char *)"Ok";
    g_MenuStrings[0x35] = (char *)"Poor";
    g_MenuStrings[0x36] = (char *)"Very good";
    g_MenuStrings[0x37] = (char *)"Good";
    g_MenuStrings[0x38] = (char *)"OK";
    g_MenuStrings[0x39] = (char *)"Bad";
    g_MenuStrings[0x3A] = (char *)"Very bad";

    /* Rules */
    g_MenuStrings[0x3B] = (char *)"Shared lives";
    g_MenuStrings[0x3C] = (char *)"Weapon kickback";
    g_MenuStrings[0x3D] = (char *)"Misc rules";
    g_MenuStrings[0x3E] = (char *)"Music";
    g_MenuStrings[0x3F] = (char *)"Controls";

    /* Screen sizes */
    g_MenuStrings[0x40] = (char *)"Playes screen sizes";
    g_MenuStrings[0x41] = (char *)"Full";
    g_MenuStrings[0x42] = (char *)"Square";
    g_MenuStrings[0x43] = (char *)"1/4 screen";
    g_MenuStrings[0x44] = (char *)"Bubbles";
    g_MenuStrings[0x45] = (char *)"Yes, lots of them";
    g_MenuStrings[0x46] = (char *)"Yes";
    g_MenuStrings[0x47] = (char *)"No";
    g_MenuStrings[0x48] = (char *)"Next";
    g_MenuStrings[0x49] = (char *)"Last";
    g_MenuStrings[0x4A] = (char *)"Page";
    g_MenuStrings[0x4B] = (char *)"Page 1";
    g_MenuStrings[0x4C] = (char *)"Page 2";
    g_MenuStrings[0x4D] = (char *)"Page 3";
    g_MenuStrings[0x4E] = (char *)"Human player controls";
    g_MenuStrings[0x4F] = (char *)"Repair speed";

    /* Player labels */
    g_MenuStrings[0x50] = (char *)"Player 1";
    g_MenuStrings[0x51] = (char *)"Player 2";
    g_MenuStrings[0x52] = (char *)"Player 3";
    g_MenuStrings[0x53] = (char *)"Player 4";
    g_MenuStrings[0x54] = (char *)"Thrust";
    g_MenuStrings[0x55] = (char *)"Back";
    g_MenuStrings[0x56] = (char *)"Turn left";
    g_MenuStrings[0x57] = (char *)"Turn right";
    g_MenuStrings[0x58] = (char *)"Basic weapon";
    g_MenuStrings[0x59] = (char *)"Special weapon";
    g_MenuStrings[0x5A] = (char *)"Menu button";
    g_MenuStrings[0x5B] = (char *)"Access gained to 9 campaign levels";
    g_MenuStrings[0x5C] = (char *)"Level amount";
    g_MenuStrings[0x5D] = (char *)"Randomize levels";
    g_MenuStrings[0x5E] = (char *)"Plr";
    g_MenuStrings[0x5F] = (char *)"Computer";
    g_MenuStrings[0x60] = (char *)"Color";
    g_MenuStrings[0x61] = (char *)"Team";
    g_MenuStrings[0x62] = (char *)"Ship";
    g_MenuStrings[0x63] = (char *)"Visible";
    g_MenuStrings[0x64] = (char *)"Resolution";

    /* entries [0x65] already allocated as dynamic buffer above */

    /* Team stats */
    g_MenuStrings[0x66] = (char *)"Team stats";
    g_MenuStrings[0x67] = (char *)"Team 1";
    g_MenuStrings[0x68] = (char *)"Team 2";
    g_MenuStrings[0x69] = (char *)"Team 3";
    g_MenuStrings[0x6A] = (char *)"Wins";
    g_MenuStrings[0x6B] = (char *)"Frags";
    g_MenuStrings[0x6C] = (char *)"Deaths";
    g_MenuStrings[0x6D] = (char *)"Awards";
    g_MenuStrings[0x6E] = (char *)"Team awards";
    g_MenuStrings[0x6F] = (char *)"Player awards";
    g_MenuStrings[0x70] = (char *)"Player stats";

    /* entries [0x71]-[0x8B] = dynamic player stat buffers (allocated above) */

    /* Audio settings */
    g_MenuStrings[0x8C] = (char *)"Sound effects";
    g_MenuStrings[0x8D] = (char *)"Music volume";
    g_MenuStrings[0x8E] = (char *)"Effect volume";
    g_MenuStrings[0x8F] = (char *)"Base holding";
    g_MenuStrings[0x90] = (char *)"Allow MP3 music";
    g_MenuStrings[0x91] = (char *)"Starting weapon";
    g_MenuStrings[0x92] = (char *)"Last";
    g_MenuStrings[0x93] = (char *)"Random";
    g_MenuStrings[0x94] = (char *)"Default";
    g_MenuStrings[0x95] = (char *)"Game style";
    g_MenuStrings[0x96] = (char *)"Random turrets at start";
    g_MenuStrings[0x97] = (char *)"Random troopers at start";
    g_MenuStrings[0x98] = (char *)"Tou homepage:";
    g_MenuStrings[0x99] = (char *)"Lives";
    g_MenuStrings[0x9A] = (char *)"Must go to repair at end";
    g_MenuStrings[0x9B] = (char *)"If you must go to repair at end:";
    g_MenuStrings[0x9C] = (char *)"Timer before explodes";
    g_MenuStrings[0x9D] = (char *)"Death timer";
    g_MenuStrings[0x9E] = (char *)"Happens when timer runs to zero:";
    g_MenuStrings[0x9F] = (char *)"Everybody dies";
    g_MenuStrings[0xA0] = (char *)"Leading team wins (max total energy)";
    g_MenuStrings[0xA1] = (char *)"Energies go near zero";
    g_MenuStrings[0xA2] = (char *)"Heavy bombing starts";
    g_MenuStrings[0xA3] = (char *)"Energies >nearly zero & bombing";
    g_MenuStrings[0xA4] = (char *)"No timer";

    /* Timer durations */
    g_MenuStrings[0xA5] = (char *)"5 sec";
    g_MenuStrings[0xA6] = (char *)"10 sec";
    g_MenuStrings[0xA7] = (char *)"20 sec";
    g_MenuStrings[0xA8] = (char *)"30 sec";
    g_MenuStrings[0xA9] = (char *)"40 sec";
    g_MenuStrings[0xAA] = (char *)"50 sec";
    g_MenuStrings[0xAB] = (char *)"1 min";
    g_MenuStrings[0xAC] = (char *)"2 min";
    g_MenuStrings[0xAD] = (char *)"3 min";
    g_MenuStrings[0xAE] = (char *)"5 min";
    g_MenuStrings[0xAF] = (char *)"10 min";
    g_MenuStrings[0xB0] = (char *)"15 min";
    g_MenuStrings[0xB1] = (char *)"30 min";

    /* Game rules */
    g_MenuStrings[0xB2] = (char *)"Weapon loading meter";
    g_MenuStrings[0xB3] = (char *)"Destructable land";
    g_MenuStrings[0xB4] = (char *)"Gravity";
    g_MenuStrings[0xB5] = (char *)"Air resistance";
    g_MenuStrings[0xB6] = (char *)"Ship strength";
    g_MenuStrings[0xB7] = (char *)"Loading times";
    g_MenuStrings[0xB8] = (char *)"What can destroy a repair place?";
    g_MenuStrings[0xB9] = (char *)"Gates";
    g_MenuStrings[0xBA] = (char *)"Level's own turrets";

    /* Amount levels */
    g_MenuStrings[0xBB] = (char *)"Particle amount";
    g_MenuStrings[0xBC] = (char *)"Decoration amount";
    g_MenuStrings[0xBD] = (char *)"Negligible";
    g_MenuStrings[0xBE] = (char *)"A little";
    g_MenuStrings[0xBF] = (char *)"Some";
    g_MenuStrings[0xC0] = (char *)"Normal";
    g_MenuStrings[0xC1] = (char *)"Lots";
    g_MenuStrings[0xC2] = (char *)"Very much";
    g_MenuStrings[0xC3] = (char *)"Pause key";

    /* Screen background options */
    g_MenuStrings[0xC4] = (char *)"Picture outside players' screens";
    g_MenuStrings[0xC5] = (char *)"Blue plasma";
    g_MenuStrings[0xC6] = (char *)"Purple plasma";
    g_MenuStrings[0xC7] = (char *)"Cyan cool plasma";
    g_MenuStrings[0xC8] = (char *)"Just black color";
    g_MenuStrings[0xC9] = (char *)"Just dark blue color";

    /* Bonus */
    g_MenuStrings[0xCA] = (char *)"Bonus amount";
    g_MenuStrings[0xCB] = (char *)"None";
    g_MenuStrings[0xCC] = (char *)"Some";
    g_MenuStrings[0xCD] = (char *)"Lots";
    g_MenuStrings[0xCE] = (char *)"Full";
    g_MenuStrings[0xCF] = (char *)"None";
    g_MenuStrings[0xD0] = (char *)"Some";
    g_MenuStrings[0xD1] = (char *)"Lots";
    g_MenuStrings[0xD2] = (char *)"Very much";

    /* Respawn */
    g_MenuStrings[0xD3] = (char *)"Players reborn after death:";
    g_MenuStrings[0xD4] = (char *)"Immediately";
    g_MenuStrings[0xD5] = (char *)"1 sec delay";
    g_MenuStrings[0xD6] = (char *)"3 sec delay";
    g_MenuStrings[0xD7] = (char *)"8 sec delay";
    g_MenuStrings[0xD8] = (char *)"No time limit";

    /* Weapon destruction */
    g_MenuStrings[0xD9] = (char *)"Both digger & bullets";
    g_MenuStrings[0xDA] = (char *)"Only digger (very tactical)";
    g_MenuStrings[0xDB] = (char *)"Only bullets";
    g_MenuStrings[0xDC] = (char *)"Nothing";

    /* Ship tuning */
    g_MenuStrings[0xDD] = (char *)"Turning rate";
    g_MenuStrings[0xDE] = (char *)"Thrusting power";
    g_MenuStrings[0xDF] = (char *)"Red";
    g_MenuStrings[0xE0] = (char *)"More yellow";
    g_MenuStrings[0xE1] = (char *)"Brown";

    /* Secret menus */
    g_MenuStrings[0xE2] = (char *)"Secret Menu screen!";
    g_MenuStrings[0xE3] = (char *)"Now this is even more secret!";
    g_MenuStrings[0xE4] = (char *)"You have found a Supersecret area!";
    g_MenuStrings[0xE5] = (char *)"Access to a few campaign levels!";
    g_MenuStrings[0xE6] = (char *)" ";
    g_MenuStrings[0xE7] = (char *)"You really have too much spare time!";
    g_MenuStrings[0xE8] = (char *)"But now you've gained access to";
    g_MenuStrings[0xE9] = (char *)"all of the campaign levels!";

    /* Level options */
    g_MenuStrings[0xEA] = (char *)"Level options";
    g_MenuStrings[0xEB] = (char *)"X flip levels";
    g_MenuStrings[0xEC] = (char *)"Generated levels";
    g_MenuStrings[0xED] = (char *)"Player amount";
    g_MenuStrings[0xEE] = (char *)"Human amount";
    g_MenuStrings[0xEF] = (char *)"Weapons Menu";
    g_MenuStrings[0xF0] = (char *)"Weaponry";
    g_MenuStrings[0xF1] = (char *)"Players Menu";

    /* More timers */
    g_MenuStrings[0xF2] = (char *)"No timer";
    g_MenuStrings[0xF3] = (char *)"2 sec";
    g_MenuStrings[0xF4] = (char *)"5 sec";
    g_MenuStrings[0xF5] = (char *)"10 sec";
    g_MenuStrings[0xF6] = (char *)"15 sec";
    g_MenuStrings[0xF7] = (char *)"25 sec";
    g_MenuStrings[0xF8] = (char *)"50 sec";
    g_MenuStrings[0xF9] = (char *)"2 min";
    g_MenuStrings[0xFA] = (char *)"Must wait for reborn";
    g_MenuStrings[0xFB] = (char *)"HUD map";
    g_MenuStrings[0xFC] = (char *)"Collision damage";
    g_MenuStrings[0xFD] = (char *)"Wall bounciness";
    g_MenuStrings[0xFE] = (char *)"More misc";
    g_MenuStrings[0xFF] = (char *)"Quick help";
    g_MenuStrings[0x100] = (char *)"Output type:  ";
    g_MenuStrings[0x101] = (char *)"Direct Sound";
    g_MenuStrings[0x102] = (char *)"Waveout";
    g_MenuStrings[0x103] = (char *)"A3D";
    g_MenuStrings[0x104] = (char *)"No sound";
    g_MenuStrings[0x105] = (char *)"Sound driver";
    g_MenuStrings[0x106] = (char *)"LOS style";
    g_MenuStrings[0x107] = (char *)"Resolution up";
    g_MenuStrings[0x108] = (char *)"Resolution down";
    g_MenuStrings[0x109] = (char *)"Black";
    g_MenuStrings[0x10A] = (char *)"Dark grey";
    g_MenuStrings[0x10B] = (char *)"Dark cyan";
    g_MenuStrings[0x10C] = (char *)"White";
    g_MenuStrings[0x10D] = (char *)"Send feedback to";
    g_MenuStrings[0x10E] = (char *)"tougame@iobox.fi";
    g_MenuStrings[0x10F] = (char *)"http://tou.has.it";
    g_MenuStrings[0x110] = (char *)"No";
    g_MenuStrings[0x111] = (char *)"Level default";
    g_MenuStrings[0x112] = (char *)"Random";
    g_MenuStrings[0x113] = (char *)"No";
    g_MenuStrings[0x114] = (char *)"Some";
    g_MenuStrings[0x115] = (char *)"Normal";
    g_MenuStrings[0x116] = (char *)"Lots";
    g_MenuStrings[0x117] = (char *)"Sound channels:  ";
    g_MenuStrings[0x118] = (char *)"Let's kill!";
    g_MenuStrings[0x119] = (char *)"Skill:";
    g_MenuStrings[0x11A] = (char *)"Mission:";

    /* Character names */
    g_MenuStrings[0x11B] = (char *)"General Joe Knoff";
    g_MenuStrings[0x11C] = (char *)"Research engineer Nicole Molter";
    g_MenuStrings[0x11D] = (char *)"Pilot Albert Moody";
    g_MenuStrings[0x11E] = (char *)"Field medic Martin Spear";
    g_MenuStrings[0x11F] = (char *)"William Stanwood, the president of the known world";
    g_MenuStrings[0x120] = (char *)"Pilot Henry Odom";
    g_MenuStrings[0x121] = (char *)"Enemy force leader Steve Hailhazard";
    g_MenuStrings[0x122] = (char *)"Hard";
    g_MenuStrings[0x123] = (char *)"Medium";
    g_MenuStrings[0x124] = (char *)"Easy";
    g_MenuStrings[0x125] = (char *)"Next page";
    g_MenuStrings[0x126] = (char *)"Prev page";
    g_MenuStrings[0x127] = (char *)"Players:";

    /* Campaign */
    g_MenuStrings[0x128] = (char *)"Campaign game";
    g_MenuStrings[0x129] = (char *)"On";
    g_MenuStrings[0x12A] = (char *)"Transparent";
    g_MenuStrings[0x12B] = (char *)"Normal";
    g_MenuStrings[0x12C] = (char *)"Opaque";
    g_MenuStrings[0x12D] = (char *)"   You must restart TOU";
    g_MenuStrings[0x12E] = (char *)"after you change this";
    g_MenuStrings[0x12F] = (char *)"0";
    g_MenuStrings[0x130] = (char *)"1";
    g_MenuStrings[0x131] = (char *)"2";
    g_MenuStrings[0x132] = (char *)"3";
    g_MenuStrings[0x133] = (char *)"4";

    /* More rules */
    g_MenuStrings[0x134] = (char *)"Friendly fire";
    g_MenuStrings[0x135] = (char *)"Maximum weapon power";
    g_MenuStrings[0x136] = (char *)"Power increase speed";
    g_MenuStrings[0x137] = (char *)"Very slow";
    g_MenuStrings[0x138] = (char *)"Slow";
    g_MenuStrings[0x139] = (char *)"Normal speed";
    g_MenuStrings[0x13A] = (char *)"Fast";
    g_MenuStrings[0x13B] = (char *)"Very fast";
    g_MenuStrings[0x13C] = (char *)"Maximum";
    g_MenuStrings[0x13D] = (char *)"Always off";
    g_MenuStrings[0x13E] = (char *)"Level default";
    g_MenuStrings[0x13F] = (char *)"Always on";
    g_MenuStrings[0x140] = (char *)"Running water";
    g_MenuStrings[0x141] = (char *)"(c) GigaMess 2002. All rights reserved.";
    g_MenuStrings[0x142] = (char *)"Frames per second";
    g_MenuStrings[0x143] = (char *)"Small";
    g_MenuStrings[0x144] = (char *)"Medium";
    g_MenuStrings[0x145] = (char *)"Large";
    g_MenuStrings[0x146] = (char *)"HUGE";
}

/* ===== FUN_004236f0 - Sprite color variant generator (004236F0) ===== */
/* Creates up to 4 color-tinted copies of a type-9 sprite at indices:
 *   sprite_index+0, sprite_index+100, sprite_index+200, sprite_index+300
 * Each variant is tinted using a team color from DAT_00483838[].
 * Uses a 3x3 color matrix (per-channel LUTs with sqrt-of-squares blend).
 * Returns the new pixel write cursor (becomes DAT_00481d28). */
static int FUN_004236f0(int sprite_index, int color_param)
{
    unsigned char w = ((unsigned char *)DAT_00489e8c)[sprite_index];
    unsigned char h = ((unsigned char *)DAT_00489e88)[sprite_index];
    int total_pixels = (int)w * (int)h;
    int original_offset = ((int *)DAT_00489234)[sprite_index];

    int num_variants;
    if (color_param < 4) {
        num_variants = 1;
    } else {
        num_variants = 4;
    }

    /* Phase 1: Create pixel data copies for all variants */
    if (num_variants == 4) {
        int write_cursor = original_offset;

        for (int v = 0; v < 4; v++) {
            int src_offset = original_offset;  /* always copy from original */

            ((int *)DAT_00489234)[sprite_index + v * 100] = write_cursor;
            ((unsigned char *)DAT_00489e8c)[sprite_index + v * 100] = w;
            ((unsigned char *)DAT_00489e88)[sprite_index + v * 100] = h;

            if (total_pixels > 0) {
                unsigned short *src = (unsigned short *)DAT_00487ab4 + src_offset;
                unsigned short *dst = (unsigned short *)DAT_00487ab4 + write_cursor;
                for (int p = 0; p < total_pixels; p++) {
                    dst[p] = src[p];
                }
                write_cursor += total_pixels;
            }
        }
    }

    int pixel_cursor = ((int *)DAT_00489234)[sprite_index];

    if (num_variants == 0) {
        return pixel_cursor;
    }

    /* Phase 2: Color transformation.
     * Builds 9 LUTs (3 input channels x 3 output channels) per variant,
     * then applies sqrt-of-sum-of-squares color matrix per pixel.
     * Palette values in DAT_00483838 are X1R5G5B5; our pixels are RGB565. */

    for (int v = 0; v < num_variants; v++) {
        /* Extract palette RGB from X1R5G5B5 */
        unsigned short pal_val = (num_variants == 4) ? DAT_00483838[v] : DAT_00483838[color_param];
        int pal_R = ((pal_val >> 10) & 0x1F) << 3;
        int pal_G = ((pal_val >> 5) & 0x1F) << 3;
        int pal_B = (pal_val & 0x1F) << 3;

        /* Default color parameters */
        int c0[3] = {0x40, 0x40, 0x40};
        int c1[3] = {0x40, 0x40, 0x40};
        int bnd[3] = {0xB4, 0xB4, 0xB4};
        int upr[3] = {0x122, 0x122, 0x122};
        int pal_scl = 0x80;

        /* Sprite group overrides */
        int grp = sprite_index;
        if (sprite_index < 18000) grp = (sprite_index / 500) * 500;

        switch (grp) {
        case 10000: case 10500:
            c1[0]=0x20; c1[1]=0x20; c1[2]=0x60;
            bnd[0]=bnd[1]=bnd[2]=0x100;
            break;
        case 11000:
            c0[0]=c0[1]=c0[2]=0x60;
            bnd[1]=0x100;
            break;
        case 11500:
            c0[0]=c0[1]=c0[2]=0x80;
            bnd[0]=0xC8; bnd[1]=0x100;
            upr[0]=upr[1]=0xFF;
            break;
        case 12000:
            c0[0]=0xAA; c0[1]=0xAC; c0[2]=0x70;
            bnd[0]=0xAA; bnd[1]=0x100;
            upr[0]=0xFF; upr[1]=400;
            pal_scl=0x8C;
            break;
        case 12500:
            c0[0]=0x68; c0[1]=0x7C; c0[2]=0xE1;
            bnd[1]=0xBE; bnd[2]=0xA0;
            upr[1]=upr[2]=0xFF;
            break;
        case 13000:
            c0[0]=0x80;
            break;
        case 13500:
            c0[0]=c0[1]=c0[2]=0x60;
            bnd[1]=0xE6;
            upr[1]=0x136;
            break;
        case 14000:
            c0[0]=c0[1]=0x80; c0[2]=0x40;
            c1[0]=c1[1]=0x80; c1[2]=0x60;
            bnd[0]=bnd[1]=bnd[2]=0xDC;
            upr[1]=upr[2]=300;
            break;
        case 14500: case 15000:
            c0[0]=c0[1]=0x80; c0[2]=0x40;
            c1[0]=c1[1]=0x80; c1[2]=0x60;
            upr[0]=upr[1]=upr[2]=0xFF;
            break;
        case 17500:
            c0[0]=0x20; c0[1]=0x50; c0[2]=0x5A;
            break;
        case 18000: case 18001:
            c0[0]=0x80; c0[1]=0x6E; c0[2]=0x64;
            c1[0]=0xC8; c1[1]=0x64; c1[2]=0x32;
            bnd[0]=bnd[1]=bnd[2]=0x100;
            break;
        case 18005:
            c1[0]=0x78; c1[1]=0x50; c1[2]=0x3C;
            break;
        case 18006:
            c1[0]=0x50; c1[1]=0x96; c1[2]=0x64;
            break;
        case 18007: case 18008:
            c1[0]=0xF0; c1[1]=c1[2]=0;
            bnd[0]=bnd[1]=bnd[2]=0x100;
            break;
        case 18011: case 18012: case 18013:
            c0[0]=0x32; c0[1]=0x82; c0[2]=0x32;
            goto entity_tail;
        case 18014:
            c0[0]=0x5A; c0[1]=0x82; c0[2]=0x32;
            goto entity_tail;
        case 18015: case 18016:
            c0[0]=c0[1]=0x82; c0[2]=0x32;
            goto entity_tail;
        case 18017:
            c0[0]=c0[1]=0x82; c0[2]=0x1E;
            goto entity_tail;
        case 18018:
            c0[0]=0x96; c0[1]=0x50; c0[2]=0x14;
            goto entity_tail;
        case 18019: case 18020:
            c0[0]=0x96; c0[1]=c0[2]=0x14;
        entity_tail:
            c1[0]=c1[1]=c1[2]=0;
            bnd[1]=0xE6; bnd[2]=0x82;
            upr[1]=0x154; upr[2]=0xFF;
            break;
        }

        /* Scale palette by pal_scl (c0/c1 scales are always 0x80 = identity) */
        pal_R = pal_R * pal_scl >> 7;
        pal_G = pal_G * pal_scl >> 7;
        pal_B = pal_B * pal_scl >> 7;

        /* Build 9 LUTs: lut[iter*3 + output_ch][input_value] */
        int lut[9][256];
        int targets[9] = {pal_R, pal_G, pal_B, c0[0], c0[1], c0[2], c1[0], c1[1], c1[2]};

        for (int iter = 0; iter < 3; iter++) {
            int b = bnd[iter];
            int u = upr[iter];
            for (int och = 0; och < 3; och++) {
                int tgt = targets[iter * 3 + och];
                int *l = lut[iter * 3 + och];
                /* Part A: ramp 0 -> target over [0..boundary) */
                int acc = 0;
                int lim = (b < 256) ? b : 256;
                for (int i = 0; i < lim; i++) {
                    l[i] = (acc / b) >> 8;
                    acc += tgt * 256;
                }
                /* Part B: ramp target -> 256 over [boundary..256) */
                int rem = 256 - tgt;
                int span = u - b;
                if (span <= 0) span = 1;
                int delta = 0;
                for (int i = lim; i < 256; i++) {
                    l[i] = delta / span + tgt;
                    delta += rem;
                }
            }
        }

        /* Update variant metadata */
        if (num_variants == 4) {
            ((int *)DAT_00489234)[sprite_index + v * 100] = pixel_cursor;
            ((unsigned char *)DAT_00489e8c)[sprite_index + v * 100] = w;
            ((unsigned char *)DAT_00489e88)[sprite_index + v * 100] = h;
        }

        /* Per-pixel transform */
        unsigned short *pixels = (unsigned short *)DAT_00487ab4;
        for (int p = 0; p < total_pixels; p++) {
            unsigned short px = pixels[pixel_cursor];
            if (px != 0) {
                /* Decode RGB565 -> 5-bit channels -> 8-bit LUT indices */
                int ri = ((px >> 11) & 0x1F) << 3;
                int gi = ((px >> 6) & 0x1F) << 3;
                int bi = (px & 0x1F) << 3;

                /* Sum-of-squares per output channel */
                int sr = lut[0][ri]*lut[0][ri] + lut[3][gi]*lut[3][gi] + lut[6][bi]*lut[6][bi];
                int sg = lut[1][ri]*lut[1][ri] + lut[4][gi]*lut[4][gi] + lut[7][bi]*lut[7][bi];
                int sb = lut[2][ri]*lut[2][ri] + lut[5][gi]*lut[5][gi] + lut[8][bi]*lut[8][bi];

                int ro = (int)sqrt((double)sr);
                int go = (int)sqrt((double)sg);
                int bo = (int)sqrt((double)sb);

                if (ro > 255) ro = 255;
                if (go > 255) go = 255;
                if (bo > 255) bo = 255;

                if (ro < 8 && go < 4 && bo < 8) {
                    pixels[pixel_cursor] = 0;
                } else {
                    pixels[pixel_cursor] = (unsigned short)(
                        ((ro & 0xF8) << 8) | ((go & 0xFC) << 3) | (bo >> 3));
                }
            }
            pixel_cursor++;
        }
    }

    return pixel_cursor;
}

/* ===== FUN_00423150 - Load all3.gfx sprites (00423150) ===== */
/* Reads sprite entries: type(1) + index(2) + skip(12) + width(2) + height(2) + skip(2) + RGB24 pixels.
 * Converts RGB24 to RGB565 for types 0,1,9; grayscale for types 2,3,4.
 * Fills DAT_00487ab4 (pixels), DAT_00489234 (offsets), DAT_00489e8c (widths), DAT_00489e88 (heights). */
static int FUN_00423150(void)
{
    FILE *f = fopen("data\\all3.gfx", "rb");
    if (!f) return 0;

    int max_spr_idx = 0;
    int spr_count = 0;
    unsigned char type_byte;
    while (fread(&type_byte, 1, 1, f) == 1) {
        unsigned short sprite_index;
        fread(&sprite_index, 1, 2, f);

        /* Skip 12 bytes */
        fread((unsigned char *)DAT_00481cf8, 1, 12, f);

        unsigned short spr_w, spr_h;
        fread(&spr_w, 1, 2, f);
        fread(&spr_h, 1, 2, f);

        /* Skip 2 bytes */
        fread((unsigned char *)DAT_00481cf8, 1, 2, f);

        /* Read RGB24 pixel data into temp buffer */
        int pixel_count = (int)spr_w * (int)spr_h;
        fread((unsigned char *)DAT_00481cf8, 1, pixel_count * 3, f);

        /* Store dimensions */
        ((unsigned char *)DAT_00489e8c)[sprite_index] = (unsigned char)spr_w;
        ((unsigned char *)DAT_00489e88)[sprite_index] = (unsigned char)spr_h;
        if ((int)sprite_index > max_spr_idx) max_spr_idx = (int)sprite_index;
        spr_count++;

        switch (type_byte) {
        case 0: case 1: {
            /* BGR24 -> RGB565 conversion.
             * all3.gfx stores pixels as BGR (byte0=B, byte1=G, byte2=R).
             * Original binary encoded to X1R5G5B5 for DirectDraw surfaces.
             * We encode to RGB565 to match our compat rendering pipeline. */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d28;
            unsigned char *src = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                unsigned char b_val = src[p * 3];
                unsigned char g_val = src[p * 3 + 1];
                unsigned char r_val = src[p * 3 + 2];
                unsigned short pixel = ((r_val & 0xF8) << 8)
                    | ((g_val & 0xFC) << 3) | (b_val >> 3);
                ((unsigned short *)DAT_00487ab4)[DAT_00481d28] = pixel;
                DAT_00481d28++;
            }
            break;
        }
        case 9: {
            /* BGR24 -> RGB565 conversion (same as case 0/1) */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d28;
            unsigned char *src9 = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                unsigned char b_val = src9[p * 3];
                unsigned char g_val = src9[p * 3 + 1];
                unsigned char r_val = src9[p * 3 + 2];
                unsigned short pixel = ((r_val & 0xF8) << 8)
                    | ((g_val & 0xFC) << 3) | (b_val >> 3);
                ((unsigned short *)DAT_00487ab4)[DAT_00481d28] = pixel;
                DAT_00481d28++;
            }
            /* Generate 4 color-tinted variants at +0, +100, +200, +300 */
            DAT_00481d28 = FUN_004236f0((int)sprite_index, 0xFF);
            break;
        }
        case 2: case 3: case 4: {
            /* Grayscale: average RGB / 3 -> 8-bit */
            ((int *)DAT_00489234)[sprite_index] = DAT_00481d24;
            unsigned char *src = (unsigned char *)DAT_00481cf8;
            for (int p = 0; p < pixel_count; p++) {
                int sum = (int)src[p * 3] + (int)src[p * 3 + 1] + (int)src[p * 3 + 2];
                /* MSVC-style divide by 3: (sum * 0x55555556) >> 32 */
                ((unsigned char *)DAT_00489e94)[DAT_00481d24] = (unsigned char)(sum / 3);
                DAT_00481d24++;
            }
            break;
        }
        }
    }

    fclose(f);
    LOG("[LOAD] all3.gfx: %d RGB pixels, %d grayscale pixels, max_spr_idx=%d\n",
        DAT_00481d28, DAT_00481d24, max_spr_idx);
    return 1;
}

/* ===== FUN_004254b0 - Load loadtime.dat entity types (004254B0) ===== */
/* Reads entity type definitions into DAT_00487abc (0x218 bytes per type, 128 types).
 * Per type: 6 x 20-byte animation frame blocks, 7-byte header, 6 x 16-byte animation property blocks. */
static int FUN_004254b0(void)
{
    FILE *f = fopen("data\\loadtime.dat", "rb");
    if (!f) return 0;

    unsigned char tmp[20];
    int offset = 0;
    unsigned char *base = (unsigned char *)DAT_00487abc;

    while (offset < 0x11030) {
        /* 6 animation frame data blocks, 20 bytes each, at type+4 */
        for (int a = 0; a < 6; a++) {
            fread(base + offset + 4 + a * 0x14, 1, 0x14, f);
        }

        /* 7-byte header */
        fread(tmp, 1, 7, f);
        base[offset + 0x7C] = tmp[0];
        base[offset + 0x7D] = tmp[1];
        *(int *)(base + offset + 0x80) = (int)tmp[3] * (int)tmp[3] * 100;
        *(int *)(base + offset + 0x84) = (int)tmp[4];
        *(int *)(base + offset + 0x144) = (int)tmp[5] * 256 + (int)tmp[6];

        /* 6 animation property blocks, 16 bytes each */
        for (int a = 0; a < 6; a++) {
            fread(tmp, 1, 16, f);
            int anim_base = offset + 0xAC + a * 4;

            *(int *)(base + offset + 0x88 + a * 4) = (int)tmp[0] - 0x80;
            base[offset + 0xA0 + a] = tmp[1];
            base[offset + 0xA6 + a] = tmp[2];
            *(int *)(base + anim_base) = (int)tmp[3] * 256 + (int)tmp[4];
            *(int *)(base + anim_base + 0x18) =
                ((int)tmp[5] * 256 + (int)tmp[6]) * 0x1400;
            *(int *)(base + anim_base + 0x30) =
                ((int)tmp[7] * 256 + (int)tmp[8]) / 3;
            *(int *)(base + anim_base + 0x48) =
                (int)tmp[9] * 256 + (int)tmp[10];
            base[offset + 0x124 + a] = tmp[11];
            base[offset + 0x12A + a] = tmp[12];
            base[offset + 0x130 + a] = tmp[13];
            *(int *)(base + anim_base + 0x60) =
                (int)tmp[14] * 256 + (int)tmp[15];
        }

        offset += 0x218;
    }

    fclose(f);
    LOG("[LOAD] loadtime.dat: %d entity types loaded\n", offset / 0x218);
    return 1;
}

/* ===== FUN_00422740 - Master data loader (00422740) ===== */
int FUN_00422740(void)
{
    /* Clear sprite tables */
    DAT_00481d28 = 0;
    DAT_00481d24 = 0;
    for (int i = 0; i < 20000; i++) {
        ((int *)DAT_00489234)[i] = 0;
        ((unsigned char *)DAT_00489e8c)[i] = 0;
        ((unsigned char *)DAT_00489e88)[i] = 0;
    }

    /* Init sin/cos lookup table */
    Init_Math_Tables((int *)DAT_00487ab0, 0x800);

    /* Load sprites from all3.gfx */
    if (FUN_00423150() != 1) return 0;

    /* Load explosion sprites */
    if (FUN_00422fc0() != 1) return 0;

    /* Load entity type definitions from loadtime.dat */
    FUN_004254b0();

    /* TODO: Load palettes (pal_col, shipal_col) - not critical for intro entities */
    /* TODO: Load taulu2.tau raw data into DAT_00489e90 */
    /* TODO: FUN_00422900 - load names.dat */

    return 1;
}

/* ===== FUN_00420be0 - Entity type post-processing (00420BE0) ===== */
/* Calculates width/3 and height/3 for entity type animation sprites */
void FUN_00420be0(void)
{
    unsigned char *base = (unsigned char *)DAT_00487abc;
    int offset = 0;
    while (offset < 0x11030) {
        for (int i = 0; i < 6; i++) {
            int sprite_idx = *(int *)(base + offset + 0xF4 + i * 4);
            if (sprite_idx < 30000) {
                int w = (int)((unsigned char *)DAT_00489e8c)[sprite_idx];
                int h = (int)((unsigned char *)DAT_00489e88)[sprite_idx];
                base[offset + 0x136 + i] = (unsigned char)(w / 3);
                base[offset + 0x13C + i] = (unsigned char)(h / 3);
            } else {
                base[offset + 0x136 + i] = 0;
                base[offset + 0x13C + i] = 0;
            }
        }
        offset += 0x218;
    }
}
void FUN_0041e580(void) {}
int  FUN_00414060(void) { return 1; }
void FUN_00413f70(void) {}
void FUN_0041e4a0(void) {}
/* FUN_0045d7d0 moved to stubs.cpp */

/* ===== FUN_00425fe0 - Main game/menu render loop (00425FE0) ===== */
/* Called from Game_State_Manager case 0x01 every frame.
 * Handles one-shot init (menu page build + background load),
 * keyboard polling, input processing, and rendering.
 *
 * Original performs DDraw lock/copy/unlock/flip for rendering.
 * COMPAT: Uses Render_Frame() instead (RGB565→ARGB8888 windowed blit). */
void FUN_00425fe0(void)
{
    /* ---- One-shot init when DAT_004877b1 is set (by Game_State_Manager case 0x03) ---- */
    if (DAT_004877b1 != 0) {
        FUN_0042a470();  /* Build menu page layout based on DAT_004877a4 */
        DAT_004877b1 = 0;

        /* If FUN_0042a470 set g_GameState to 0x04 (start game), just return.
         * The main loop will dispatch to Game_State_Manager case 0x04. */
        if (g_GameState == 0x04) {
            return;
        }

        /* Load appropriate background JPEG */
        char bgIdx = (DAT_004877a4 == 0x1d) ? 2 : 0;
        Load_Background_To_Buffer(bgIdx);

        /* Render the game view (menu items etc.) to Software_Buffer */
        if (Render_Game_View() == 0) {
            g_GameState = 0xFE;  /* Fatal render error → shutdown */
            return;
        }
    }

    /* ---- Reset per-frame click state ---- */
    DAT_004877bc = 0;

    /* ---- Poll keyboard via DirectInput ---- */
    if (lpDI_Keyboard != NULL) {
        HRESULT hr = lpDI_Keyboard->GetDeviceState(256, g_KeyboardState);
        if (FAILED(hr)) {
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                lpDI_Keyboard->Acquire();
            }
            memset(g_KeyboardState, 0, 256);
        }
    }

    /* ---- F12 (scan 0x58): immediate exit ---- */
    if ((g_KeyboardState[0x58] & 0x80) != 0) {
        DAT_004877b1 = 1;
        DAT_004877a4 = 0xFE;
    }

    /* COMPAT: Sync game cursor from Windows cursor position.
     * Original used exclusive fullscreen with hidden system cursor;
     * the game rendered its own cursor sprite at (g_MouseDeltaX>>18, g_MouseDeltaY>>18).
     * In windowed mode, DirectInput reports relative deltas which desync from the
     * visible Windows cursor. Override with absolute position so hover/click works. */
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd_Main, &pt);
        g_MouseDeltaX = pt.x << 18;
        g_MouseDeltaY = pt.y << 18;
    }

    /* ---- Input processing (normal mode, g_InputMode == 0) ---- */
    if (g_InputMode == 0) {
        /* Primary click: Right Ctrl (0x9D) OR Left Mouse Button */
        if (((g_KeyboardState[0x9D] & 0x80) != 0 || (g_MouseButtons & 1) != 0) &&
            (DAT_004877bd & 1) == 0) {
            DAT_004877bc |= 1;
            DAT_004877bd |= 1;
        }
        if ((g_KeyboardState[0x9D] & 0x80) == 0 && (g_MouseButtons & 1) == 0 &&
            (DAT_004877bd & 1) != 0) {
            DAT_004877bd ^= 1;
        }

        /* Secondary click: Right Mouse Button */
        if (((g_KeyboardState[0x36] & 0x80) != 0 || (g_MouseButtons & 2) != 0) &&
            (DAT_004877bd & 2) == 0) {
            DAT_004877bc |= 2;
            DAT_004877bd |= 2;
        }
        if ((g_KeyboardState[0x36] & 0x80) == 0 && (g_MouseButtons & 2) == 0 &&
            (DAT_004877bd & 2) != 0) {
            DAT_004877bd ^= 2;
        }

        /* ESC (scan 0x01): Navigate back or center view */
        if ((g_KeyboardState[0x01] & 0x80) != 0 && (DAT_004877bd & 8) == 0) {
            if (DAT_004877a4 == 0 &&
                (g_MouseDeltaX != 0x5000000 || g_MouseDeltaY != 0x5b80000)) {
                /* Main menu, not centered → center the viewport */
                g_MouseDeltaX = 0x5000000;
                g_MouseDeltaY = 0x5b80000;
            } else {
                /* Navigate to parent page (DAT_004877c9 = back page) */
                DAT_004877b1 = 1;
                DAT_004877a4 = DAT_004877c9;
            }
            DAT_004877bd |= 8;
        }
        if ((g_KeyboardState[0x01] & 0x80) == 0 && (DAT_004877bd & 8) != 0) {
            DAT_004877bd ^= 8;
        }

        DAT_004877e5 = 1;  /* Mark input as processed */
    }

    /* ---- Clamp viewport position ---- */
    if (g_MouseDeltaX < 0x3c0000) g_MouseDeltaX = 0x3c0000;
    else if (g_MouseDeltaX > 0x9c40000) g_MouseDeltaX = 0x9c40000;
    if (g_MouseDeltaY < 0x3c0000) g_MouseDeltaY = 0x3c0000;
    else if (g_MouseDeltaY > 0x7440000) g_MouseDeltaY = 0x7440000;

    /* ---- Main game/menu logic tick ---- */
    FUN_00426650();

    /* ---- Render frame to screen ---- */
    /* COMPAT: Replaces original DDraw lock → FUN_00428650 → unlock → Blt/Flip */
    Render_Frame();

    /* ---- COMPAT: Frame rate limiter (~60fps) ---- */
    /* Original used DDraw exclusive fullscreen with vsync-locked flip chain.
     * In windowed mode we need explicit frame limiting. */
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

/* ===== FUN_004644af - sprintf-like string formatter (004644AF) ===== */
#include <stdarg.h>
void FUN_004644af(char *dest, const unsigned char *format, ...)
{
    if (!dest || !format) return;
    va_list args;
    va_start(args, format);
    vsprintf(dest, (const char *)format, args);
    va_end(args);
}

/* ===== FUN_00425840 - Reset/load defaults (00425840) ===== */
void FUN_00425840(void)
{
    /* TODO: decompile - resets config to defaults */
}

/* ===== FUN_00430200 - Create text menu item (00430200) ===== */
/* Adds a text-based menu item to g_GameViewData.
 * Returns text_height - 4 (spacing for next item). */
int FUN_00430200(int param_x, int param_y, int string_idx, int color_style,
                 int font_idx, unsigned char clickable, unsigned char render_mode,
                 unsigned char alignment, unsigned char nav_target)
{
    if (DAT_004877a8 > 0x15D)
        return 0;

    MenuItem *items = (MenuItem *)g_GameViewData;
    MenuItem *item = &items[DAT_004877a8];

    item->x = param_x;
    item->y = param_y;
    item->type = 0;            /* text item */
    item->color_style = color_style;
    item->font_idx = (char)font_idx;
    item->flag1 = 0;
    item->string_idx = string_idx;

    /* Calculate text width from font metrics */
    int text_width = 0;
    if (g_MenuStrings && g_MenuStrings[string_idx]) {
        const char *str = g_MenuStrings[string_idx];
        while (*str) {
            unsigned char c = (unsigned char)*str++;
            if (c == ' ') {
                text_width += Font_Char_Table[font_idx * 256 + 32].width;
            } else {
                int idx = font_idx * 256 + c;
                if (idx < 1024)
                    text_width += Font_Char_Table[idx].width;
            }
        }
    }

    int text_height = Font_Char_Table[font_idx * 256].height;

    item->width = text_width;
    item->height = text_height;
    item->hover_state = 0;
    item->clickable = clickable;
    item->nav_target = nav_target;
    item->linked_item = 20000;
    item->render_mode = render_mode;
    item->extra_data = 0;

    /* Apply alignment */
    switch (alignment) {
    case 1: item->x = 320 - text_width / 2; break;   /* center */
    case 2: item->x = 30; break;                       /* left margin */
    case 3: item->x = 610 - text_width; break;         /* right align */
    case 4: item->x = 100; break;                      /* left at 100 */
    case 5: item->x = 540 - text_width; break;         /* right at 540 */
    }

    DAT_004877a8++;
    return text_height - 4;
}

/* ===== FUN_0042ff80 - Create sprite menu item (0042FF80) ===== */
void FUN_0042ff80(int param_x, int param_y, int sprite_idx,
                  unsigned char clickable, unsigned char render_mode,
                  unsigned char alignment, unsigned char nav_target)
{
    if (DAT_004877a8 >= 0x15E)
        return;

    MenuItem *items = (MenuItem *)g_GameViewData;
    MenuItem *item = &items[DAT_004877a8];

    item->x = param_x;
    item->y = param_y;
    item->type = 1;            /* sprite item */
    item->color_style = sprite_idx;
    item->font_idx = 0;
    item->flag1 = 0;
    item->string_idx = 0;

    /* Get sprite dimensions from sprite tables */
    int spr_w = 0, spr_h = 0;
    if (DAT_00489e8c) spr_w = ((unsigned char *)DAT_00489e8c)[sprite_idx];
    if (DAT_00489e88) spr_h = ((unsigned char *)DAT_00489e88)[sprite_idx];

    item->width = spr_w;
    item->height = spr_h;
    item->hover_state = 0;
    item->clickable = clickable;
    item->nav_target = nav_target;
    item->linked_item = 20000;
    item->render_mode = render_mode;
    item->extra_data = 0;

    /* Apply alignment */
    switch (alignment) {
    case 1: item->x = 320 - spr_w / 2; break;
    case 2: item->x = 30; break;
    case 3: item->x = 610 - spr_w; break;
    case 4: item->x = 100; break;
    case 5: item->x = 540 - spr_w; break;
    }

    DAT_004877a8++;
}

/* ===== FUN_0042fc90 - Set extra data on last item (0042FC90) ===== */
void FUN_0042fc90(int value)
{
    if (DAT_004877a8 > 0) {
        MenuItem *items = (MenuItem *)g_GameViewData;
        items[DAT_004877a8 - 1].extra_data = value;
    }
}

/* ===== FUN_0042fcf0 - Link last two items for value cycling (0042FCF0) ===== */
/* Used for label+value pairs (e.g., "Sound: [On]").
 * Links the label item (count-2) and value item (count-1) together. */
void FUN_0042fcf0(void)
{
    if (DAT_004877a8 < 2) return;

    MenuItem *items = (MenuItem *)g_GameViewData;
    int n = DAT_004877a8;

    items[n - 1].linked_item = n - 2;  /* value → label */
    items[n - 2].linked_item = n - 1;  /* label → value */
    items[n - 2].width = 0x1B8;        /* combined width = 440 */
    items[n - 1].width = 0;            /* value item: no independent width */
    items[n - 2].color_style = 0x0B;   /* special action: combo control */
}

/* ===== FUN_0042fdf0 - Add separator/divider item (0042FDF0) ===== */
/* Creates a centered sprite separator at the given Y position.
 * Returns 8 (height of separator). */
int FUN_0042fdf0(int param_y)
{
    if (DAT_004877a8 < 0x15E) {
        MenuItem *items = (MenuItem *)g_GameViewData;
        MenuItem *item = &items[DAT_004877a8];

        /* Center separator sprite 0x19C */
        int spr_w = 0;
        if (DAT_00489e8c) spr_w = ((unsigned char *)DAT_00489e8c)[0x19C];
        item->x = 320 - spr_w / 2;
        item->y = param_y + 4;
        item->type = 1;
        item->color_style = 0x19C;
        item->font_idx = 0;
        item->flag1 = 0;
        item->string_idx = 0;
        item->width = 0;
        item->height = 0;
        item->hover_state = 0;
        item->clickable = 0;
        item->nav_target = 0xFF;
        item->linked_item = 20000;
        item->render_mode = 0;
        item->extra_data = 0;

        DAT_004877a8++;
    }
    return 8;
}

/* ===== FUN_0042fcb0 - Stub (0042FCB0) ===== */
void FUN_0042fcb0(void) {}

/* ===== FUN_0042a470 - Menu page builder (0042A470) ===== */
/* Builds the menu item layout for the current page (DAT_004877a4).
 * Each page creates menu items in g_GameViewData via FUN_00430200. */
void FUN_0042a470(void)
{
    int iVar3, iVar4, iVar7 = 0;
    unsigned char uVar12 = 0, uVar13 = 0;
    int uVar11 = 0;
    MenuItem *items;

    /* Clear state (runs before switch, matches original) */
    DAT_004877a8 = 0;     /* Menu item count */
    g_FrameIndex = 0;     /* Frame index for Software_Buffer display */
    DAT_004877d8 = 0;     /* Scrollbar area width = 0 (no scrollbar) */
    DAT_004877cc = 0;     /* Scroll decay */

    switch (DAT_004877a4) {
    case 0x00: /* Main menu page */
        FUN_0042ff80(0xe6, 0x20, 0x13, 0, 0, 0, 0xff);          /* title bar sprite */
        FUN_00430200(0, 0x7d, 7, 0, 0, 0, 0, 1, 0xff);          /* "Tunnels Of the Underworld" */
        FUN_00430200(0, 0xaa, 1, 2, 0, 1, 0, 1, 3);             /* "Team deathmatch" → page 3 */
        FUN_00430200(0, 200, 2, 2, 0, 1, 0, 1, 0x11);           /* "Levels" → page 0x11 */
        FUN_00430200(0, 0xe6, 3, 2, 0, 1, 0, 1, 0x12);          /* "Players" → page 0x12 */
        FUN_00430200(0, 0x104, 4, 2, 0, 1, 0, 1, 1);            /* "Options" → page 1 */
        FUN_00430200(0, 0x122, 0xff, 2, 0, 1, 0, 1, 0x1c);      /* string 0xFF → page 0x1C */
        FUN_00430200(0, 0x140, 5, 2, 0, 1, 0, 1, 2);            /* "Credits" → page 2 */
        FUN_00430200(0, 0x15e, 6, 2, 0, 1, 0, 1, 0xfe);         /* "Exit game" → exit */
        FUN_00430200(0, 0x19f, 0x98, 1, 3, 0, 0, 2, 0xff);      /* copyright line 1 */
        FUN_00430200(0, 0x1ae, 0x10f, 1, 3, 0, 0, 2, 0xff);     /* copyright line 2 */
        FUN_00430200(0, 0x1c2, 0x141, 0, 3, 0, 0, 2, 0xff);     /* version string */
        LOG("[MENU] Page 0: Main menu (%d items)\n", DAT_004877a8);
        DAT_004877c9 = 0xFE;  /* Back page = exit */
        DAT_004877b1 = 0;
        return;

    case 0x01: /* Options submenu */
        FUN_00430200(0, 0x28, 8, 1, 0, 0, 0, 1, 0xff);          /* "Options" heading */
        FUN_00430200(0, 0x50, 9, 2, 0, 1, 0, 1, 9);             /* "Sound" → page 9 */
        FUN_00430200(0, 0x72, 10, 2, 0, 1, 0, 1, 4);            /* "Video" → page 4 */
        FUN_00430200(0, 0x94, 0xc, 0, 0, 1, 0, 1, 6);           /* "Game modes" → page 6 */
        FUN_00430200(0, 0xb6, 0x3d, 2, 0, 1, 0, 1, 0x1f);       /* "Game" → page 0x1F */
        FUN_00430200(0, 0xd8, 0xb, 2, 0, 1, 0, 1, 5);           /* "Controls" → page 5 */
        FUN_00430200(0, 0xfa, 0xd, 2, 0, 1, 0, 1, 7);           /* "Keyboard" → page 7 */
        FUN_00430200(0, 0x11c, 0xfe, 2, 0, 1, 0, 1, 0x1b);      /* "Network" → page 0x1B */
        FUN_00430200(0, 0x13e, 0xe, 2, 0, 1, 0, 1, 8);          /* "Keys" → page 8 */
        FUN_00430200(0, 0x160, 0x3f, 2, 0, 1, 0, 1, 0xc);       /* "Name" → page 0xC */
        FUN_00430200(0, 0x194, 0xf, 2, 0, 1, 0, 1, 0);          /* "Back" → page 0 */
        LOG("[MENU] Page 1: Options (%d items)\n", DAT_004877a8);
        g_FrameIndex = 1;
        DAT_004877c9 = 0;  /* ESC → main menu */
        DAT_004877b1 = 0;
        return;

    case 0x02: /* Credits */
        FUN_00430200(0, 0x28, 5, 1, 0, 0, 0, 1, 0xff);          /* "Credits" heading */
        FUN_00430200(0, 0x50, 0x10, 2, 2, 0, 0, 2, 0xff);       /* credit line */
        FUN_00430200(0, 0x69, 0x16, 2, 2, 0, 0, 2, 0xff);       /* credit line */
        FUN_00430200(0, 0x69, 0x11, 0, 2, 0, 0, 3, 0xff);       /* credit line right */
        FUN_00430200(0, 0x82, 0x17, 2, 2, 0, 0, 2, 0xff);       /* credit line */
        FUN_00430200(0, 0xb4, 0x14, 2, 2, 0, 0, 2, 0xff);       /* credit line */
        FUN_00430200(0, 0xb4, 0x15, 0, 2, 0, 0, 3, 0xff);       /* credit line right */
        FUN_00430200(0, 0xe6, 0x13, 0, 2, 0, 0, 3, 0xff);       /* credit line */
        FUN_00430200(0, 0xff, 0x18, 2, 2, 0, 0, 2, 0xff);       /* credit line */
        FUN_00430200(0, 0xff, 0x1b, 0, 2, 0, 0, 3, 0xff);       /* credit line right */
        FUN_00430200(0, 0x118, 0x1a, 0, 2, 0, 0, 3, 0xff);      /* credit line */
        FUN_00430200(0, 0x131, 0x19, 0, 2, 0, 0, 3, 0xff);      /* credit line */
        FUN_00430200(0, 400, 0xf, 2, 0, 1, 0, 1, 0);            /* "Back" → main menu */
        LOG("[MENU] Page 2: Credits (%d items)\n", DAT_004877a8);
        DAT_004877c9 = 0;  /* ESC → main menu */
        DAT_004877b1 = 0;
        return;

    case 0x03: /* Start game - transition to gameplay */
        DAT_0048764a = 0;
        g_GameState = 0x04;
        DAT_004877b1 = 0;
        return;

    case 0x04: /* Video settings */
        FUN_00430200(0, 0x28, 10, 1, 0, 0, 0, 1, 0xff);           /* "Video" heading */
        FUN_00430200(0, 0x50, 100, 0, 2, 2, 0, 4, 0xff);          /* "Resolution" label */
        iVar7 = FUN_00430200(0, 0x50, 0, 0, 2, 1, 0xd, 5, 0xff);  /* resolution value */
        iVar7 = iVar7 + 0x50;
        FUN_0042fc90(CFG_ADDR(0x483725));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0x40, 2, 2, 2, 0, 4, 0xff);        /* "Detail" label */
        iVar3 = FUN_00430200(0, iVar7, 0x41, 2, 2, 1, 4, 5, 0xff); /* detail value */
        FUN_0042fc90(CFG_ADDR(0x483726));
        FUN_0042fcf0();
        iVar4 = iVar7 + iVar3 + 0x14;
        FUN_00430200(0, iVar4, 0x1e, 2, 2, 2, 0, 4, 0xff);        /* "Particles" label */
        iVar7 = FUN_00430200(0, iVar4, 0x22, 2, 2, 1, 1, 5, 0xff); /* particles value */
        FUN_0042fc90(CFG_ADDR(0x483727));
        FUN_0042fcf0();
        iVar3 = FUN_00430200(0, iVar4 + iVar7, 0x1f, 4, 2, 0, 0, 1, 0xff); /* info text */
        iVar3 = iVar4 + iVar7 + iVar3;
        iVar7 = FUN_0042fdf0(iVar3);                               /* separator */
        iVar3 = iVar3 + iVar7;
        FUN_00430200(0, iVar3, 0x20, 2, 2, 2, 0, 4, 0xff);        /* "Shadows" label */
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483729));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0x140, 2, 2, 2, 0, 4, 0xff);       /* "Lighting" label */
        iVar7 = FUN_00430200(0, iVar3, 0x13d, 2, 2, 1, 4, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x48372b));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0x21, 2, 2, 2, 0, 4, 0xff);        /* "Smoothing" label */
        iVar7 = FUN_00430200(0, iVar3, 0x137, 2, 2, 1, 0x11, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x48372a));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0x44, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x45, 2, 2, 1, 4, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48372c));
        FUN_0042fcf0();
        iVar4 = FUN_00430200(0, iVar3 + iVar7, 0x27, 4, 2, 0, 0, 1, 0xff); /* info */
        iVar4 = iVar3 + iVar7 + iVar4;
        iVar7 = FUN_0042fdf0(iVar4);                               /* separator */
        iVar4 = iVar4 + iVar7;
        FUN_00430200(0, iVar4, 0x28, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar4, 0x129, 2, 2, 1, 0x27, 5, 0xff);
        iVar4 = iVar4 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x48372d));
        FUN_0042fcf0();
        FUN_00430200(0, iVar4, 0x29, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar4, 0x36, 2, 2, 1, 2, 5, 0xff);
        iVar4 = iVar4 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x48372e));
        FUN_0042fcf0();
        FUN_00430200(0, iVar4, 0x2a, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar4, 0x32, 2, 2, 1, 3, 5, 0xff);
        iVar4 = iVar4 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x48372f));
        FUN_0042fcf0();
        FUN_00430200(0, iVar4, 0x2c, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar4, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483730));
        FUN_0042fcf0();
        FUN_00430200(0, iVar4 + iVar7, 0x2b, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar4 + iVar7, 0x109, 2, 2, 1, 0x27, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483731));
        FUN_0042fcf0();
        FUN_00430200(0, 0x1b8, 0xf, 2, 0, 1, 0, 1, 1);            /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x05: /* Controls */
        FUN_00430200(0, 0x28, 0xb, 1, 0, 0, 0, 1, 0xff);           /* "Controls" heading */
        FUN_00430200(0, 0x50, 0x26, 2, 2, 2, 0, 4, 0xff);          /* "Mouse sens" label */
        iVar3 = FUN_00430200(0, 0x50, 0x113, 2, 2, 1, 0x27, 5, 0xff);
        iVar3 = iVar3 + 0x50;
        FUN_0042fc90(CFG_ADDR(0x483734));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0x30, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar4 = FUN_00430200(0, iVar3, 0x110, 2, 2, 1, 4, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483735));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar3 + iVar4 + 8);
        iVar7 = iVar3 + iVar4 + 0x10 + iVar7;
        FUN_00430200(0, iVar7, 0x96, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar3 = FUN_00430200(0, iVar7, 0xcb, 2, 2, 1, 0x27, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x483736));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0x97, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar4 = FUN_00430200(0, iVar7, 0xcb, 2, 2, 1, 0x27, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483737));
        FUN_0042fcf0();
        iVar3 = FUN_0042fdf0(iVar7 + iVar4 + 8);
        iVar3 = iVar7 + iVar4 + 0x10 + iVar3;
        FUN_00430200(0, iVar3, 0xb9, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483756));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0xba, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar4 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483757));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar3 + iVar4 + 8);
        iVar7 = iVar3 + iVar4 + 0x10 + iVar7;
        FUN_00430200(0, iVar7, 0xca, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar7, 0xcf, 2, 2, 1, 0x27, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48375a));
        FUN_0042fcf0();
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x06: /* Game modes */
        FUN_00430200(0, 0x28, 0xc, 1, 0, 0, 0, 1, 0xff);           /* heading */
        /* Copy game mode names into dynamic menu strings */
        if (g_MenuStrings) {
            strcpy(g_MenuStrings[0x71], "Custom");
            strcpy(g_MenuStrings[0x72], "Quite normal");
            strcpy(g_MenuStrings[0x73], "Turret wars");
            strcpy(g_MenuStrings[0x74], "Cyberdeath");
            strcpy(g_MenuStrings[0x75], "Quick rounds");
            strcpy(g_MenuStrings[0x76], "Subspace trench");
            strcpy(g_MenuStrings[0x77], "Base defending");
        }
        FUN_00430200(0, 100, 0x95, 0, 2, 2, 0, 4, 0xff);           /* "Game type" label */
        iVar7 = FUN_00430200(0, 100, 0x71, 0, 2, 1, 0x30, 5, 0xff); /* game mode value */
        FUN_0042fc90(CFG_ADDR(0x483739));
        FUN_0042fcf0();
        iVar7 = iVar7 + 0x6e;
        FUN_00430200(0, iVar7, 0x91, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar3 = FUN_00430200(0, iVar7, 0x92, 2, 2, 1, 4, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483738));
        FUN_0042fcf0();
        iVar3 = iVar7 + iVar3 + 10;
        FUN_00430200(0, iVar3, 0x99, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar7 = FUN_00430200(0, iVar3, 0, 2, 2, 1, 9, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373a));
        FUN_0042fcf0();
        iVar3 = iVar3 + iVar7 + 10;
        FUN_00430200(0, iVar3, 0x134, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373d));
        FUN_0042fcf0();
        iVar3 = iVar3 + iVar7 + 10;
        FUN_00430200(0, iVar3, 0x3b, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373b));
        FUN_0042fcf0();
        iVar3 = iVar3 + iVar7 + 10;
        FUN_00430200(0, iVar3, 0x8f, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373c));
        FUN_0042fcf0();
        iVar7 = iVar3 + iVar7 + 10;
        FUN_00430200(0, iVar7, 0x142, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar7, 0, 2, 2, 1, 9, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483746));
        FUN_0042fcf0();
        FUN_00430200(0, 0x154, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x07: /* Keyboard settings */
        FUN_00430200(0, 0x28, 0xd, 1, 0, 0, 0, 1, 0xff);           /* "Keyboard" heading */
        FUN_00430200(0, 0x50, 0x2d, 2, 2, 2, 0, 4, 0xff);          /* label */
        iVar7 = FUN_00430200(0, 0x50, 0x22, 2, 2, 1, 1, 5, 0xff);
        iVar7 = iVar7 + 0x50;
        FUN_0042fc90(CFG_ADDR(0x483747));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xb4, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x13, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x483748));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xb5, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x14, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x483749));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xb6, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x15, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374a));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xdd, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x15, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374b));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xde, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x15, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374c));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xfc, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x13, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374d));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0xfd, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x13, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374e));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0x3c, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x16, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x483752));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0x135, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x34, 5, 0xff);
        iVar7 = iVar7 + iVar3;
        FUN_0042fc90(CFG_ADDR(0x48374f));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7, 0x136, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x16, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483750));
        FUN_0042fcf0();
        FUN_00430200(0, iVar7 + iVar3, 0xb7, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar7 + iVar3, 0x22, 2, 2, 1, 0x16, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483751));
        FUN_0042fcf0();
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x08: /* Keys page 1 */
        FUN_00430200(0, 0x28, 0xe, 1, 0, 0, 0, 1, 0xff);           /* "Keys" heading */
        FUN_00430200(0, 0x3c, 0x4b, 4, 0, 0, 0, 1, 0xff);          /* "Page 1" */
        items = (MenuItem *)g_GameViewData;
        iVar7 = 0;
        iVar3 = 0x5a;
        do {
            FUN_00430200(0, iVar3, (int)g_KeyOrderTable[iVar7], 2, 2, 2, 6, 4, 0xff);
            FUN_00430200(0, iVar3, 0x24, 2, 2, 1, 1, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x48375c) + (int)g_KeyOrderTable[iVar7]);
            items = (MenuItem *)g_GameViewData;
            items[DAT_004877a8 - 1].flag1 = (unsigned char)0xFA;
            items[DAT_004877a8 - 1].height = (int)g_KeyOrderTable[iVar7];
            FUN_0042fcf0();
            iVar3 = iVar3 + 0x12;
            iVar7 = iVar7 + 1;
        } while (iVar3 < 0x17a);
        FUN_00430200(0, 0x1a9, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        FUN_00430200(0, 0x1a9, 0x4a, 2, 0, 1, 0, 5, 0xff);         /* nav indicator */
        uVar13 = 10; uVar12 = 5; iVar7 = 0x48;
        break; /* → post-switch: add "Prev Page" button */

    case 0x09: /* Sound settings */
        FUN_00430200(0, 0x28, 9, 1, 0, 0, 0, 1, 0xff);             /* "Sound" heading */
        FUN_00430200(0, 0x50, 0x3e, 2, 2, 2, 0, 4, 0xff);          /* label */
        iVar3 = FUN_00430200(0, 0x50, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48371e));
        FUN_0042fcf0();
        iVar3 = iVar3 + 0x5a;
        FUN_00430200(0, iVar3, 0x8c, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar4 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48371f));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar3 + iVar4 + 10);
        iVar7 = iVar3 + iVar4 + 0x14 + iVar7;
        FUN_00430200(0, iVar7, 0x8d, 2, 2, 2, 0, 4, 0xff);         /* "Music vol" label */
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0xe, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483720));
        FUN_0042fcf0();
        iVar4 = iVar7 + iVar3 + 10;
        FUN_00430200(0, iVar4, 0x8e, 2, 2, 2, 0, 4, 0xff);         /* "SFX vol" label */
        iVar3 = FUN_00430200(0, iVar4, 0x22, 2, 2, 1, 0xe, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483721));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar4 + iVar3 + 10);
        iVar7 = iVar4 + iVar3 + 0x14 + iVar7;
        FUN_00430200(0, iVar7, 0x90, 2, 2, 2, 0, 4, 0xff);         /* label */
        iVar4 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483722));
        FUN_0042fcf0();
        iVar3 = FUN_0042fdf0(iVar7 + iVar4 + 10);
        iVar3 = iVar7 + iVar4 + 0x14 + iVar3;
        FUN_00430200(0, iVar3, 0x100, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar3, 0x101, 2, 2, 1, 0x27, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483723));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0x117, 2, 2, 2, 0, 4, 0xff);        /* label */
        iVar7 = FUN_00430200(0, iVar3, 0, 2, 2, 1, 5, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483724));
        FUN_0042fcf0();
        iVar3 = iVar3 + iVar7 + 10;
        iVar7 = FUN_00430200(0, iVar3, 0x12d, 0, 1, 0, 0, 4, 0xff); /* info line 1 */
        FUN_00430200(0, iVar3 + iVar7, 0x12e, 0, 1, 0, 0, 4, 0xff); /* info line 2 */
        FUN_00430200(0, 0x1a4, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x0A: /* Keys page 2 */
        FUN_00430200(0, 0x28, 0xe, 1, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x3c, 0x4c, 4, 0, 0, 0, 1, 0xff);          /* "Page 2" */
        items = (MenuItem *)g_GameViewData;
        iVar7 = 0x10;
        iVar3 = 0x5a;
        do {
            FUN_00430200(0, iVar3, (int)g_KeyOrderTable[iVar7], 2, 2, 2, 6, 4, 0xff);
            FUN_00430200(0, iVar3, 0x24, 2, 2, 1, 1, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x48375c) + (int)g_KeyOrderTable[iVar7]);
            items = (MenuItem *)g_GameViewData;
            items[DAT_004877a8 - 1].flag1 = (unsigned char)0xFA;
            items[DAT_004877a8 - 1].height = (int)g_KeyOrderTable[iVar7];
            FUN_0042fcf0();
            iVar3 = iVar3 + 0x12;
            iVar7 = iVar7 + 1;
        } while (iVar3 < 0x17a);
        FUN_00430200(0, 0x1a9, 0xf, 2, 0, 1, 0, 1, 1);
        FUN_00430200(0, 0x1a9, 0x4a, 2, 0, 1, 0, 5, 0xff);
        FUN_00430200(0, 0x196, 0x48, 2, 0, 1, 0, 5, 0xb);          /* "Prev" → page 0x0B */
        FUN_0042fcb0();
        FUN_00430200(0, 0x1a9, 0x4a, 2, 0, 1, 0, 4, 0xff);
        uVar13 = 8; uVar12 = 4; iVar7 = 0x49;
        break;

    case 0x0B: /* Keys page 3 */
        FUN_00430200(0, 0x28, 0xe, 1, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x3c, 0x4d, 4, 0, 0, 0, 1, 0xff);          /* "Page 3" */
        items = (MenuItem *)g_GameViewData;
        iVar7 = 0x20;
        iVar3 = 0x5a;
        do {
            FUN_00430200(0, iVar3, (int)g_KeyOrderTable[iVar7], 2, 2, 2, 6, 4, 0xff);
            FUN_00430200(0, iVar3, 0x24, 2, 2, 1, 1, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x48375c) + (int)g_KeyOrderTable[iVar7]);
            items = (MenuItem *)g_GameViewData;
            items[DAT_004877a8 - 1].flag1 = (unsigned char)0xFA;
            items[DAT_004877a8 - 1].height = (int)g_KeyOrderTable[iVar7];
            FUN_0042fcf0();
            iVar3 = iVar3 + 0x12;
            iVar7 = iVar7 + 1;
        } while (iVar3 < 0x168);
        FUN_00430200(0, 0x1a9, 0xf, 2, 0, 1, 0, 1, 1);
        FUN_00430200(0, 0x1a9, 0x4a, 2, 0, 1, 0, 4, 0xff);
        uVar13 = 10; uVar12 = 4; iVar7 = 0x49;
        break;

    case 0x0C: /* Name entry */
        FUN_00430200(0, 0x28, 0x3f, 1, 0, 0, 0, 1, 0xff);          /* "Name" heading */
        FUN_00430200(0, 0x50, 0x4e, 2, 0, 1, 0, 1, 0xf);           /* "Enter name" → page 0xF */
        FUN_0042fdf0(0xa0);
        FUN_00430200(0, 0xb4, 0xc3, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0xb4, 0, 2, 2, 1, 7, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x4837ba));
        FUN_0042fcf0();
        FUN_00430200(0, 0xd2, 0x106, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0xd2, 0, 2, 2, 1, 7, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x4837bb));
        FUN_0042fcf0();
        FUN_00430200(0, 0xf0, 0x107, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0xf0, 0, 2, 2, 1, 7, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x4837bc));
        FUN_0042fcf0();
        FUN_00430200(0, 0x10e, 0x108, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0x10e, 0, 2, 2, 1, 7, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x4837bd));
        FUN_0042fcf0();
        uVar11 = 0x168;
        goto back_to_options;

    case 0x0F: /* Name entry page 1 (character grid) */
        FUN_00430200(0, 10, 0x3f, 1, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x32, 0x4b, 4, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x55, 0x50, 4, 2, 0, 0, 1, 0xff);
        FUN_0042fdf0(0x68);
        FUN_00430200(0, 0x104, 0x51, 4, 2, 0, 0, 1, 0xff);
        FUN_0042fdf0(0x117);
        iVar4 = 0x54;
        iVar3 = 0x120;
        iVar7 = 0;
        do {
            FUN_00430200(0, iVar3 - 0xaf, iVar4, 2, 2, 2, 0, 4, 0xff);
            FUN_00430200(0, iVar3 - 0xaf, 0, 2, 2, 1, 7, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x48376e) + iVar4);
            FUN_0042fcf0();
            FUN_00430200(0, iVar3, iVar4, 2, 2, 2, 0, 4, 0xff);
            FUN_00430200(0, iVar3, 0, 2, 2, 1, 7, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x4837ca) + iVar7);
            FUN_0042fcf0();
            iVar7 = iVar7 + 1;
            iVar3 = iVar3 + 0x12;
            iVar4 = iVar4 + 1;
        } while (iVar3 < 0x19e);
        FUN_00430200(0, 0x1c2, 0xf, 2, 0, 1, 0, 1, 0xc);          /* "Back" → Name */
        FUN_00430200(0, 0x1c2, 0x4a, 2, 0, 1, 0, 5, 0xff);
        FUN_00430200(0, 0x1af, 0x48, 2, 0, 1, 0, 5, 0x10);         /* → page 0x10 */
        FUN_0042fcb0();
        DAT_004877c9 = 0xc;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x10: /* Name entry page 2 */
        FUN_00430200(0, 10, 0x3f, 1, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x32, 0x4c, 4, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x55, 0x52, 4, 2, 0, 0, 1, 0xff);
        FUN_0042fdf0(0x68);
        FUN_00430200(0, 0x104, 0x53, 4, 2, 0, 0, 1, 0xff);
        FUN_0042fdf0(0x117);
        iVar4 = 0x54;
        iVar3 = 0x120;
        iVar7 = 0;
        do {
            FUN_00430200(0, iVar3 - 0xaf, iVar4, 2, 2, 2, 0, 4, 0xff);
            FUN_00430200(0, iVar3 - 0xaf, 0, 2, 2, 1, 7, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x48377e) + iVar4);
            FUN_0042fcf0();
            FUN_00430200(0, iVar3, iVar4, 2, 2, 2, 0, 4, 0xff);
            FUN_00430200(0, iVar3, 0, 2, 2, 1, 7, 5, 0xff);
            FUN_0042fc90(CFG_ADDR(0x4837da) + iVar7);
            FUN_0042fcf0();
            iVar7 = iVar7 + 1;
            iVar3 = iVar3 + 0x12;
            iVar4 = iVar4 + 1;
        } while (iVar3 < 0x19e);
        FUN_00430200(0, 0x1c2, 0xf, 2, 0, 1, 0, 1, 0xc);
        FUN_00430200(0, 0x1c2, 0x4a, 2, 0, 1, 0, 4, 0xff);
        FUN_00430200(0, 0x1af, 0x49, 2, 0, 1, 0, 4, 0xf);          /* → page 0x0F */
        FUN_0042fcb0();
        DAT_004877c9 = 0xc;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x11: /* Levels (scrollable list) */
        FUN_00430200(0, 0x28, 2, 1, 0, 0, 0, 1, 0xff);             /* "Levels" heading */
        FUN_00430200(0, 0x1ae, 0xf, 2, 0, 1, 0, 1, 0);             /* "Back" → main menu */
        FUN_00430200(0, 400, 0xea, 2, 0, 1, 0, 1, 0x19);           /* → page 0x19 */
        FUN_00430200(0, 0x50, 0x5c, 2, 2, 2, 0, 4, 0xff);          /* level name label */
        FUN_00430200(0, 0x50, 0, 2, 2, 1, 9, 5, 0xff);             /* level name value */
        FUN_0042fc90(CFG_ADDR(0x481f58));
        FUN_0042fcf0();
        if (g_MenuStrings && g_MenuStrings[0x71])
            FUN_004644af(g_MenuStrings[0x71],
                (const unsigned char *)"You have %d levels and %d GG themes");
        FUN_00430200(10, 0x1cc, 0x71, 1, 3, 0, 0, 0, 0xff);        /* level count info */
        FUN_00430200(0, 100, 0x5d, 2, 2, 1, 0xc, 1, 0xff);         /* randomize */
        FUN_0042fdf0(0x7e);
        FUN_0042fdf0(0x182);
        DAT_004877d4 = 0;
        DAT_004877d8 = 0x212;
        DAT_004877dc = 0x90;
        DAT_004877e0 = 0xcc;
        FUN_0042ff80(0x212, 0x90, 0x19d, 1, 10, 0, 0xff);          /* scroll up arrow */
        iVar7 = DAT_004877e0 + 0x14 + DAT_004877dc;
        goto scrollbar_common;

    case 0x12: /* Players (scrollable list) */
        FUN_00430200(0, 0x14, 3, 1, 0, 0, 0, 1, 0xff);             /* "Players" heading */
        FUN_00430200(0, 0x1b8, 0xf, 2, 0, 1, 0, 1, 0);             /* "Back" → main menu */
        FUN_00430200(0, 0x19a, 0xef, 2, 0, 1, 0, 1, 0x1a);         /* → page 0x1A */
        DAT_004877ac = DAT_004877a8;
        FUN_00430200(0, 0x2d, 0xed, 2, 2, 2, 0, 4, 0xff);          /* label */
        FUN_00430200(0, 0x2d, 0, 2, 2, 1, 0x18, 5, 0xff);          /* value */
        FUN_0042fc90(CFG_ADDR(0x48227c));
        FUN_0042fcf0();
        FUN_00430200(0, 0x41, 0xee, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0x41, 0x12f, 2, 2, 1, 0x33, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48227d));
        FUN_0042fcf0();
        FUN_00430200(0x32, 0x5a, 0x5e, 2, 1, 0, 0, 0, 0xff);      /* column headers */
        FUN_00430200(0x6f, 0x5a, 0x60, 4, 1, 1, 0x20, 0, 0xff);
        FUN_00430200(0xbe, 0x5a, 0x61, 2, 1, 1, 0x21, 0, 0xff);
        FUN_00430200(0x104, 0x5a, 0x62, 4, 1, 1, 0x22, 0, 0xff);
        FUN_00430200(0x142, 0x5a, 99, 2, 1, 1, 0x23, 0, 0xff);
        FUN_00430200(0x19e, 0x5a, 0x5f, 4, 1, 1, 0x24, 0, 0xff);
        DAT_004877d4 = 0;
        DAT_004877d8 = 0x212;
        DAT_004877dc = 0x6e;
        DAT_004877e0 = 0x10e;
        FUN_0042ff80(0x212, 0x6e, 0x19d, 1, 10, 0, 0xff);          /* scroll up arrow */
        iVar7 = DAT_004877e0 + 0x14 + DAT_004877dc;
    scrollbar_common:
        FUN_0042ff80(DAT_004877d8, iVar7, 0x19e, 1, 0xb, 0, 0xff); /* scroll down arrow */
        DAT_004877ac = DAT_004877a8;
        DAT_004877b0 = 2;
        DAT_004877c9 = 0;
        g_FrameIndex = 0;
        DAT_004877b1 = 0;
        return;

    case 0x13: /* Results screen (requires game state) */
    case 0x14: /* Detailed statistics */
    case 0x15: /* Game log */
        /* These pages display post-game results. They need game state data
         * that isn't available until Phase 6 gameplay is implemented.
         * For now, redirect to main menu. */
        DAT_004877a4 = 0;
        DAT_004877b1 = 1;
        return;

    case 0x16: /* Color customization - team 1 */
        FUN_00430200(0x78, 0x7d, 0xe2, 0, 0, 0, 0, 1, 0xff);
        FUN_00430200(10, 10, 0xe6, 2, 0, 1, 0, 0, 0x17);
        FUN_00430200(0x78, 0xe1, 0xe5, 1, 2, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 0);
        { /* Enforce minimum color values */
            int *p = (int *)&g_ConfigBlob[0x483808 - 0x481F58];
            int *end = (int *)&g_ConfigBlob[0x483820 - 0x481F58];
            while (p < end) {
                for (int j = 0; j < 3; j++) {
                    if (*p < 4) *p = 4;
                    p++;
                }
            }
        }
        DAT_004877b1 = 0;
        return;

    case 0x17: /* Color customization - team 2 */
        FUN_00430200(0x78, 200, 0xe3, 0, 0, 0, 0, 1, 0xff);
        FUN_00430200(0x140, 0xe1, 0xe6, 2, 0, 1, 0, 0, 0x18);
        FUN_00430200(0x78, 0xe1, 0x5b, 1, 2, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 0);
        {
            int *p = (int *)&g_ConfigBlob[0x483808 - 0x481F58];
            int *end = (int *)&g_ConfigBlob[0x483820 - 0x481F58];
            while (p < end) {
                for (int j = 0; j < 3; j++) {
                    if (*p < 8) *p = 8;
                    p++;
                }
            }
        }
        DAT_004877b1 = 0;
        return;

    case 0x18: /* Color customization - confirm */
        FUN_00430200(0x78, 0x7d, 0xe4, 0, 0, 0, 0, 1, 0xff);
        FUN_00430200(0x78, 0x96, 0xe7, 2, 2, 0, 0, 1, 0xff);
        FUN_00430200(0x78, 200, 0xe8, 1, 2, 0, 0, 1, 0xff);
        FUN_00430200(0x78, 0xdc, 0xe9, 1, 2, 0, 0, 1, 0xff);
        {
            int *p = (int *)&g_ConfigBlob[0x483808 - 0x481F58];
            int *end = (int *)&g_ConfigBlob[0x483820 - 0x481F58];
            while (p < end) {
                for (int j = 0; j < 3; j++) {
                    if (*p < 0xe) *p = 0xe;
                    p++;
                }
            }
        }
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 0);
        DAT_004877b1 = 0;
        return;

    case 0x19: /* Level settings */
        FUN_00430200(0, 0x28, 0xea, 1, 0, 0, 0, 1, 0xff);          /* heading */
        FUN_00430200(0, 0x50, 0xeb, 2, 2, 2, 0, 4, 0xff);
        iVar7 = FUN_00430200(0, 0x50, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x481f59));
        FUN_0042fcf0();
        iVar7 = iVar7 + 0x5a;
        FUN_00430200(0, iVar7, 0x1c, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x143, 2, 2, 1, 0x27, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x481f5a));
        FUN_0042fcf0();
        iVar7 = iVar7 + iVar3 + 10;
        FUN_00430200(0, iVar7, 0xec, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0xe, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x481f5b));
        FUN_0042fcf0();
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 0x11);          /* "Back" → Levels */
        DAT_004877c9 = 0x11;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x1A: /* Player list (alternate view) */
        FUN_00430200(0, 0x14, 3, 1, 0, 0, 0, 1, 0xff);
        FUN_00430200(0, 0x1b8, 0xf, 2, 0, 1, 0, 1, 0);
        DAT_004877ac = DAT_004877a8;
        FUN_00430200(0, 0x2d, 0xed, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0x2d, 0, 2, 2, 1, 0x18, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48227c));
        FUN_0042fcf0();
        FUN_00430200(0, 0x41, 0xee, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, 0x41, 0x12f, 2, 2, 1, 0x33, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48227d));
        FUN_0042fcf0();
        FUN_00430200(10, 0x5a, 0x5e, 2, 1, 0, 0, 0, 0xff);
        FUN_00430200(0x6f, 0x5a, 0xf0, 4, 1, 1, 0x25, 0, 0xff);
        DAT_004877d4 = 0;
        DAT_004877d8 = 0x262;
        DAT_004877dc = 0x6e;
        DAT_004877e0 = 0x10e;
        FUN_0042ff80(0x262, 0x6e, 0x19d, 1, 10, 0, 0xff);
        FUN_0042ff80(DAT_004877d8, DAT_004877e0 + 0x14 + DAT_004877dc, 0x19e, 1, 0xb, 0, 0xff);
        DAT_004877ac = DAT_004877a8;
        DAT_004877b0 = 2;
        DAT_004877c9 = 0x12;
        g_FrameIndex = 0;
        DAT_004877b1 = 0;
        return;

    case 0x1B: /* Network settings */
        FUN_00430200(0, 0x28, 0xfe, 1, 0, 0, 0, 1, 0xff);          /* "Network" heading */
        FUN_00430200(0, 0x50, 0xb3, 2, 2, 2, 0, 4, 0xff);
        iVar7 = FUN_00430200(0, 0x50, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483753));
        FUN_0042fcf0();
        iVar3 = FUN_00430200(0, iVar7 + 0x50, 0xb8, 0xb, 1, 0, 0, 4, 0xff);
        iVar3 = iVar7 + 0x50 + iVar3;
        iVar7 = FUN_00430200(0, iVar3, 0xd9, 2, 2, 1, 0x17, 4, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483754));
        items = (MenuItem *)g_GameViewData;
        items[DAT_004877a8 - 1].width = 0x1B8;
        iVar7 = FUN_0042fdf0(iVar3 + 8);
        iVar7 = iVar3 + 0x10 + iVar7;
        FUN_00430200(0, iVar7, 0x4f, 2, 2, 2, 0, 4, 0xff);
        iVar4 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 0x16, 4, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483755));
        FUN_0042fcf0();
        iVar3 = FUN_0042fdf0(iVar7 + iVar4 + 8);
        iVar3 = iVar7 + iVar4 + 0x10 + iVar3;
        FUN_00430200(0, iVar3, 0xbb, 2, 2, 2, 0, 4, 0xff);
        iVar7 = FUN_00430200(0, iVar3, 0xbd, 2, 2, 1, 0x11, 5, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483758));
        FUN_0042fcf0();
        FUN_00430200(0, iVar3, 0xbc, 2, 2, 2, 0, 4, 0xff);
        iVar4 = FUN_00430200(0, iVar3, 0xbd, 2, 2, 1, 0x11, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483759));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar3 + iVar4 + 8);
        iVar7 = iVar3 + iVar4 + 0x10 + iVar7;
        iVar3 = FUN_00430200(0, iVar7, 0xc4, 0xb, 1, 0, 0, 4, 0xff);
        FUN_00430200(0, iVar7 + iVar3, 0xc5, 2, 2, 1, 0xf, 4, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48375b));
        items = (MenuItem *)g_GameViewData;
        items[DAT_004877a8 - 1].width = 0x1B8;
        FUN_00430200(0, 0x168, 0xf, 2, 0, 1, 0, 1, 1);             /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0x1C: /* In-game pause menu */
        g_FrameIndex = 2;
        DAT_004877c9 = 0;
        DAT_004877b1 = 0;
        return;

    case 0x1D: /* Reset defaults */
        FUN_00425840();
        DAT_004877b1 = 0;
        return;

    case 0x1E: /* Start match */
        DAT_0048764a = 1;
        g_GameState = 0x04;
        DAT_004877b1 = 0;
        return;

    case 0x1F: /* Game settings */
        FUN_00430200(0, 0x28, 0x3d, 1, 0, 0, 0, 1, 0xff);          /* "Game" heading */
        FUN_00430200(0, 0x50, 0x9a, 2, 2, 2, 0, 4, 0xff);
        iVar7 = FUN_00430200(0, 0x50, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373e));
        FUN_0042fcf0();
        iVar3 = FUN_00430200(0, iVar7 + 0x5a, 0x9b, 0xb, 1, 0, 0, 4, 0xff);
        iVar3 = iVar7 + 0x5a + iVar3;
        FUN_00430200(0, iVar3, 0x9c, 2, 2, 2, 0, 4, 0xff);
        iVar4 = FUN_00430200(0, iVar3, 0xf2, 2, 2, 1, 0x12, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x48373f));
        FUN_0042fcf0();
        iVar7 = FUN_0042fdf0(iVar3 + iVar4 + 8);
        iVar7 = iVar3 + iVar4 + 0x10 + iVar7;
        FUN_00430200(0, iVar7, 0x9d, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0xa4, 2, 2, 1, 0x10, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483740));
        FUN_0042fcf0();
        iVar3 = iVar7 + iVar3 + 10;
        iVar7 = FUN_00430200(0, iVar3, 0x9e, 0xb, 1, 0, 0, 4, 0xff);
        iVar3 = iVar3 + iVar7;
        iVar7 = FUN_00430200(0, iVar3, 0x9f, 2, 2, 1, 0xf, 4, 0xff);
        iVar3 = iVar3 + iVar7;
        FUN_0042fc90(CFG_ADDR(0x483741));
        items = (MenuItem *)g_GameViewData;
        items[DAT_004877a8 - 1].width = 0x1B8;
        iVar7 = FUN_0042fdf0(iVar3 + 8);
        iVar7 = iVar3 + 0x10 + iVar7;
        FUN_00430200(0, iVar7, 0xb2, 2, 2, 2, 0, 4, 0xff);
        iVar3 = FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483742));
        FUN_0042fcf0();
        iVar3 = iVar7 + iVar3 + 10;
        FUN_00430200(0, iVar3, 0xfb, 2, 2, 2, 0, 4, 0xff);
        iVar7 = FUN_00430200(0, iVar3, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483743));
        FUN_0042fcf0();
        iVar3 = iVar3 + iVar7 + 10;
        iVar7 = FUN_00430200(0, iVar3, 0xd3, 0xb, 1, 0, 0, 4, 0xff);
        iVar3 = iVar3 + iVar7;
        iVar7 = FUN_00430200(0, iVar3, 0xd4, 2, 2, 1, 0xf, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483744));
        iVar7 = iVar3 + iVar7 + 10;
        items = (MenuItem *)g_GameViewData;
        items[DAT_004877a8 - 1].width = 0x1B8;
        FUN_00430200(0, iVar7, 0xfa, 2, 2, 2, 0, 4, 0xff);
        FUN_00430200(0, iVar7, 0x22, 2, 2, 1, 1, 5, 0xff);
        FUN_0042fc90(CFG_ADDR(0x483745));
        FUN_0042fcf0();
        uVar11 = 0x1a4;
    back_to_options:
        FUN_00430200(0, uVar11, 0xf, 2, 0, 1, 0, 1, 1);            /* "Back" → Options */
        DAT_004877c9 = 1;
        g_FrameIndex = 1;
        DAT_004877b1 = 0;
        return;

    case 0xFE: /* Exit - save options and shutdown */
        Save_Options_Config();
        g_GameState = 0xFE;
        DAT_004877b1 = 0;
        return;

    default:
        DAT_004877d8 = 0;
        DAT_004877cc = 0;
        g_FrameIndex = 0;
        DAT_004877a8 = 0;
        DAT_004877b1 = 0;
        return;
    }

    /* Post-switch: shared code for key binding pages (cases 8, 0x0A, 0x0B) */
    FUN_00430200(0, 0x196, iVar7, 2, 0, 1, 0, uVar12, uVar13);
    FUN_0042fcb0();
    DAT_004877c9 = 1;
    g_FrameIndex = 1;
    DAT_004877b1 = 0;
}

/* ===== FUN_00427df0 - Menu item click handler (00427DF0) ===== */
/* Handles click on a menu item. For items with a nav_target,
 * navigates to that page. For value items, cycles the value.
 * param_1: item index, param_2: 1=primary click, 0=secondary */

/* Scroll speed constant (original: DAT_004756c8 = 0x3AC9A634 as IEEE754 float) */
static const float SCROLL_SPEED_K = 0.001539f;

void FUN_00427df0(int param_1, char param_2)
{
    if (!g_GameViewData) return;

    MenuItem *items = (MenuItem *)g_GameViewData;
    MenuItem *item = &items[param_1];

    /* Click direction: +1 for primary (left), -1 for secondary (right) */
    char cVar9 = (char)((-(param_2 != 1) & 0xFE) + 1);

    /* Navigation: if nav_target != 0xFF, switch to that page */
    if (item->nav_target != 0xFF) {
        DAT_004877a4 = item->nav_target;
        DAT_004877b1 = 1;
        return;
    }

    /* Sprite items: scroll arrows for scrollable pages */
    if (item->type == 1) {
        float fVar3;
        if (DAT_004877a4 == 0x11) {
            int iVar4 = (int)(g_ConfigBlob[0] & 0xFF) - 0x0F;
            fVar3 = (float)((iVar4 + ((unsigned)(iVar4 >> 31) >> 29)) >> 3);
        } else if (DAT_004877a4 == 0x12) {
            fVar3 = (float)(((int)(g_ConfigBlob[0x324] & 0xFF) - 10) / 6);
        } else if (DAT_004877a4 == 0x1A) {
            fVar3 = (float)(((int)(g_ConfigBlob[0x324] & 0xFF) - 3) / 2);
        } else if (DAT_004877a4 == 0x14) {
            fVar3 = (float)(((int)(g_ConfigBlob[0x324] & 0xFF) - 7) / 5);
        } else {
            fVar3 = 1.0f;
        }
        if (fVar3 <= 0.0f) fVar3 = 1.0f;

        unsigned char rm = item->render_mode;
        if (rm == 0x0A) { /* Scroll up */
            DAT_004877ec = 1;
            DAT_004877bd ^= 1;
            DAT_004877d4 -= ((float)DAT_004877f0 * SCROLL_SPEED_K) / fVar3;
            if (DAT_004877d4 < 0.0f) DAT_004877d4 = 0.0f;
            DAT_004877b0 = 2;
        } else if (rm == 0x0B) { /* Scroll down */
            DAT_004877ec = 1;
            DAT_004877bd ^= 1;
            DAT_004877d4 += ((float)DAT_004877f0 * SCROLL_SPEED_K) / fVar3;
            if (DAT_004877d4 > 1.0f) DAT_004877d4 = 1.0f;
            DAT_004877b0 = 2;
        }
    }

    /* Non-text items: bail out (catches type==1 after scroll, and any other) */
    if (item->type != 0) return;

    /* Text items: value cycling based on render_mode */
    unsigned char *data = (unsigned char *)(uintptr_t)item->extra_data;
    unsigned char bVar11 = item->render_mode;

    switch (bVar11) {
    case 1: { /* Toggle (0↔1) */
        *data = (*data == 0) ? 1 : 0;
        /* Key binding pages: flag1==0xFA means this toggles a key enable/disable */
        if (item->flag1 == 0xFA) {
            unsigned char *keyBindings = &g_ConfigBlob[0x1776]; /* DAT_004836ce */
            unsigned char *keyEnabled  = &g_ConfigBlob[0x1804]; /* DAT_0048375c */
            unsigned int scanCode = (unsigned int)item->height;
            for (int i = 0; i < 0x40; i++) {
                if (keyEnabled[scanCode] == 0) {
                    if ((unsigned char)keyBindings[i] == (unsigned char)scanCode) {
                        FUN_004265e0(i);
                    }
                } else {
                    if (keyBindings[i] == 0x2F) { /* '/' = unbound marker */
                        FUN_004265e0(i);
                    }
                }
            }
            /* If key was disabled, clear its entries in action map */
            if (keyEnabled[scanCode] == 0) {
                unsigned char *actionMap = &g_ConfigBlob[0x47A]; /* DAT_004823d2 */
                for (int off = 0x3C; off < 0xF00; off += 0x3C) {
                    actionMap[scanCode + off] = 0;
                }
                return;
            }
        }
        break;
    }

    case 2: { /* Multi-state cycle: 1 ↔ 0x28 ↔ 0x1E ↔ 0x0A ↔ 4 */
        unsigned char val = *data;
        if (cVar9 != -1) { /* Forward (primary click) */
            if (val == 1) *data = 0x28;
            else if (val == 0x28) *data = 0x1E;
            else if (val == 0x1E) *data = 0x0A;
            else if (val == 0x0A) *data = 4;
            else *data = 1; /* default/wrap: 4 → 1, unknown → 1 */
        } else { /* Backward (secondary click) */
            if (val == 1) *data = 4;
            else if (val == 4) *data = 0x0A;
            else if (val == 0x0A) *data = 0x1E;
            else if (val == 0x1E) *data = 0x28;
            else *data = 1; /* default/wrap: 0x28 → 1, unknown → 1 */
        }
        return;
    }

    case 3: { /* Decrement with wrap 1-4 */
        unsigned char val = *data - cVar9;
        *data = val;
        if (val == 0) { *data = 4; return; }
        if (val > 4) { *data = 1; return; }
        break;
    }

    case 4: { /* Increment with wrap 0-2 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 2; return; }
        if (val >= 3) { *data = 0; return; }
        break;
    }

    case 5: case 9: case 0x0E:
    case 0x13: case 0x14: case 0x15: case 0x16:
    case 0x18: case 0x1B: case 0x34:
    { /* Enter slider/drag input mode */
        if ((DAT_004877bd & 1) != 0) {
            DAT_004877bd ^= 1;
        }
        if (g_InputMode == 0) {
            DAT_004877e6 = (unsigned char)param_1;
            g_InputMode = 1;
            DAT_004877e8 = 0;
            return;
        }
        break;
    }

    case 7: /* Key-bind capture mode */
        g_InputMode = 2;
        DAT_004877e6 = (unsigned char)param_1;
        DAT_004877e8 = (int)*data;
        return;

    case 8: { /* Level/map cycle */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) {
            *data = (unsigned char)(DAT_00485088 - 1);
            return;
        }
        if ((int)(unsigned int)val >= DAT_00485088) {
            *data = 0;
            return;
        }
        break;
    }

    case 0x0C: { /* Randomize level order */
        int *levelOrder = (int *)&g_ConfigBlob[4]; /* DAT_00481f5c */
        int totalMaps = DAT_00485088;
        int ggThemes = DAT_0048508c;
        unsigned char threshold = g_ConfigBlob[3]; /* high byte of first config dword */
        int numEntries = (0x48227c - 0x481f5c) / 4; /* 200 level slots */
        for (int i = 0; i < numEntries; i++) {
            if (totalMaps <= ggThemes ||
                ((int)threshold <= rand() % 100 && ggThemes != 0)) {
                if (ggThemes > 0) levelOrder[i] = rand() % ggThemes;
            } else {
                int custom = totalMaps - ggThemes;
                if (custom > 0) levelOrder[i] = rand() % custom + ggThemes;
            }
        }
        return;
    }

    case 0x0D: { /* Resolution cycle */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) {
            *data = (unsigned char)(g_NumDisplayModes - 1);
            return;
        }
        if ((int)(unsigned int)val >= g_NumDisplayModes) {
            *data = 0;
            return;
        }
        break;
    }

    case 0x0F: { /* Cycle 0-4 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 4; return; }
        if (val >= 5) { *data = 0; return; }
        break;
    }

    case 0x10: { /* Cycle 0-13 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 0x0D; return; }
        if (val >= 0x0E) { *data = 0; return; }
        break;
    }

    case 0x11: { /* Cycle 0-5 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 5; return; }
        if (val > 5) { *data = 0; return; }
        break;
    }

    case 0x12: { /* Cycle 0-7 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 7; return; }
        if (val > 7) { *data = 0; return; }
        break;
    }

    case 0x17: { /* Cycle 0-3 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 3; return; }
        if (val >= 4) { *data = 0; return; }
        break;
    }

    case 0x1C: { /* Cycle 0-2 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 2; return; }
        if (val >= 3) { *data = 0; return; }
        break;
    }

    case 0x1D: { /* Cycle 0-8 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 8; return; }
        if (val >= 9) { *data = 0; return; }
        break;
    }

    case 0x1E: { /* Simple toggle 0↔1 */
        *data = (*data == 0) ? 1 : 0;
        return;
    }

    case 0x1F: { /* Cycle 0-4 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 4; return; }
        if (val >= 5) { *data = 0; return; }
        break;
    }

    case 0x20: case 0x21: case 0x22:
    case 0x23: case 0x24: case 0x25:
    { /* Game setup - enter input mode 3 */
        DAT_004877e8 = 0;
        g_InputMode = 3;
        DAT_004877e6 = (unsigned char)param_1;
        break;
    }

    case 0x26: { /* Key binding toggle with auto-assignment */
        unsigned char wasZero = (*data == 0) ? 1 : 0;
        *data = wasZero;
        unsigned char *keyBindings = &g_ConfigBlob[0x1776]; /* DAT_004836ce */
        unsigned char actionIdx = item->flag1;
        if (wasZero) {
            /* Toggled on: if binding is unbound, auto-assign */
            if (keyBindings[actionIdx] == 0x2F) {
                FUN_004265e0((int)actionIdx);
                return;
            }
        } else {
            /* Toggled off: if scan code matches binding, reassign */
            unsigned int scanCode = (unsigned int)item->height;
            if (scanCode == (unsigned int)keyBindings[actionIdx]) {
                FUN_004265e0((int)actionIdx);
                return;
            }
        }
        break;
    }

    case 0x27: { /* Cycle 0-3 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 3; return; }
        if (val >= 4) { *data = 0; return; }
        break;
    }

    case 0x2B: { /* Increment game counter */
        int *counter = (int *)&g_ConfigBlob[0x1894]; /* DAT_004837ec */
        (*counter)++;
        DAT_004877b1 = 1;
        return;
    }

    case 0x2C: { /* Decrement game counter */
        int *counter = (int *)&g_ConfigBlob[0x1894]; /* DAT_004837ec */
        (*counter)--;
        DAT_004877b1 = 1;
        return;
    }

    case 0x2D: { /* Cycle game mode (DAT_004837e8: 0-2) */
        int *mode    = (int *)&g_ConfigBlob[0x1890]; /* DAT_004837e8 */
        int *counter = (int *)&g_ConfigBlob[0x1894]; /* DAT_004837ec */
        if (cVar9 == -1) {
            (*mode)--;
            *counter = 0;
            if (*mode < 0) {
                *mode = 2;
                DAT_004877b1 = 1;
                return;
            }
        } else {
            (*mode)++;
            if (*mode > 2) {
                *mode = 0;
            }
        }
        *counter = 0;
        DAT_004877b1 = 1;
        return;
    }

    case 0x2E: { /* Modify game config array value */
        int *mode      = (int *)&g_ConfigBlob[0x1890]; /* DAT_004837e8 */
        int *toggle    = (int *)&g_ConfigBlob[0x188C]; /* DAT_004837e4 */
        int *configArr = (int *)&g_ConfigBlob[0x1898]; /* DAT_004837f0 */
        int *limitArr  = (int *)&g_ConfigBlob[0x18B0]; /* DAT_00483808 */
        int *counter   = (int *)&g_ConfigBlob[0x1894]; /* DAT_004837ec */
        int idx = *mode + *toggle * 3;
        if (cVar9 == -1) {
            if (configArr[idx] > 0) {
                configArr[idx]--;
                *counter = 0;
                DAT_004877b1 = 1;
                return;
            }
        } else {
            if (configArr[idx] < limitArr[idx]) {
                configArr[idx]++;
                *counter = 0;
                DAT_004877b1 = 1;
                return;
            }
        }
        break;
    }

    case 0x2F: { /* Toggle team selection (DAT_004837e4) */
        int *toggle  = (int *)&g_ConfigBlob[0x188C]; /* DAT_004837e4 */
        int *counter = (int *)&g_ConfigBlob[0x1894]; /* DAT_004837ec */
        *toggle = (*toggle == 0) ? 1 : 0;
        *counter = 0;
        DAT_004877b1 = 1;
        return;
    }

    case 0x30: { /* Cycle 0-6 */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) { *data = 6; return; }
        if (val > 6) { *data = 0; return; }
        break;
    }

    case 0x33: { /* Cycle 0-4 with player config update */
        unsigned char val = *data + cVar9;
        *data = val;
        if (val == 0xFF) *data = 4;
        else if (val > 4) *data = 0;
        /* Update player enable flags based on player count */
        unsigned char playerCount = g_ConfigBlob[0x325]; /* byte at DAT_0048227c+1 */
        unsigned char *playerEnable = &g_ConfigBlob[0x376]; /* DAT_004822ce */
        int i = 0;
        if (playerCount != 0) {
            do {
                playerEnable[i] = 1;
                i++;
            } while (i < (int)(unsigned int)playerCount);
        }
        if ((unsigned int)playerCount < 0x40) {
            memset(&playerEnable[playerCount], 0, 0x40 - playerCount);
        }
        return;
    }

    default:
        break;
    }
}

/* ===== FUN_00427a70 - Input mode key assignment (00427A70) ===== */
/* Processes mouse wheel/scroll input on value items while hovering.
 * TODO: implement in Phase 3 (options page). */
void FUN_00427a70(int param_1)
{
    (void)param_1;
    /* Stub - input mode processing not yet implemented */
}

/* ===== FUN_00426650 - Game/menu logic tick (00426650) ===== */
/* Handles frame timing, menu item hover/click processing,
 * page-specific dynamic content, and hover animation decay.
 * Original: ~500 lines covering all menu interaction. */
void FUN_00426650(void)
{
    /* Update frame delta time */
    DWORD now = timeGetTime();
    DAT_004877f0 = now - g_FrameTimer;
    g_FrameTimer = now;
    if (DAT_004877f0 > 1000) {
        DAT_004877f0 = 1000;
    }

    /* Scrollbar drag interaction (only when scrollbar is active) */
    /* TODO: Phase 3 - scrollbar for pages with many items
     * Requires DAT_004877d8 != 0 which we don't set yet. */

    /* Page-specific dynamic content rendering */
    /* TODO: Phase 3 - pages 0x14 (scores), 0x12 (players),
     * 0x1a (key bindings), 0x11 (levels) populate items dynamically
     * based on scroll position and data tables. */

    /* Click sound effect */
    /* Original: plays FMOD sound when any mouse button pressed,
     * if not in input mode, sound enabled, and sound config allows. */
    if (DAT_004877bc != 0 && g_InputMode == 0 && DAT_004877ec == 0
        && g_SoundEnabled != 0 && g_SoundTable != NULL) {
        /* TODO: FMOD click sound
         * FSOUND_PlaySoundEx(-1, g_SoundTable[0x155].handle, 0, 1);
         * FSOUND_SetVolume(ch, 0x80);
         * FSOUND_SetPan(ch, 0x80);
         * FSOUND_SetPaused(ch, 0); */
    }
    DAT_004877ec = 0;

    /* --- Hover detection and click processing --- */
    if (g_GameViewData == NULL) goto hover_decay;

    {
        MenuItem *items = (MenuItem *)g_GameViewData;
        int cursor_x = g_MouseDeltaX >> 18;
        int cursor_y = g_MouseDeltaY >> 18;

        for (int i = 0; i < DAT_004877a8; i++) {
            MenuItem *item = &items[i];

            /* Only process clickable items */
            if (item->clickable == 0) continue;

            /* Padding: 4px for text items (type==0), 0 for sprites */
            int pad = (item->type != 0) ? 0 : 4;

            /* Check cursor within item bounding box */
            if (item->x < cursor_x &&
                cursor_x < item->x + item->width &&
                item->y + pad < cursor_y &&
                cursor_y < item->y + item->height - pad)
            {
                /* Hover: set hover glow */
                item->hover_state = 0x2580000;

                /* Propagate hover to linked item */
                int linked = item->linked_item;
                if (linked != 20000) {
                    items[linked].hover_state = 0x2580000;
                }

                /* Click dispatch (left or right mouse button) */
                if ((DAT_004877bc & 1) || (DAT_004877bc & 2)) {
                    FUN_00427df0(i, DAT_004877bc & 1);

                    /* Also dispatch click to linked item */
                    if (linked != 20000) {
                        FUN_00427df0(linked, DAT_004877bc & 1);
                    }
                }

                /* Input mode: mouse wheel/key processing */
                if (DAT_004877e5 == 1) {
                    if (g_InputMode == 1) {
                        FUN_00427a70(i);
                        if (linked != 20000) {
                            FUN_00427a70(linked);
                        }
                    }
                    /* TODO: g_InputMode == 2 (keyboard char entry) */
                    /* TODO: g_InputMode == 3 (game setup randomization) */

                    /* Clear input flags after processing */
                    DAT_004877e8 = 0;
                    g_InputMode = 0;
                    DAT_004877e5 = 0;
                }
            }
        }
    }

hover_decay:
    /* --- Hover animation decay --- */
    /* Decrement hover_state for all items based on frame time.
     * hover_state uses fixed-point: >> 18 gives the visible intensity. */
    if (g_GameViewData != NULL) {
        MenuItem *items = (MenuItem *)g_GameViewData;
        for (int i = 0; i < DAT_004877a8; i++) {
            int decay = DAT_004877f0 << 18;
            if (decay == 0) decay = 1;

            items[i].hover_state -= decay;
            if (items[i].hover_state < 0) {
                items[i].hover_state = 0;
            }
        }
    }
}

int  FUN_0042fc40(void) { return 1; }
void FUN_0042fc10(void) {}
void FUN_0041a8c0(void) {}
/* ===== FUN_0041d740 - Compute end-game stats/awards (0041D740) ===== */
/* Original: computes player/team awards ("Most valuable", "Most violent",
 * "Survivor", etc.) and sets DAT_004877a4 to 0x13 (scoreboard) or 0x1D
 * (single-player results).
 * TODO: Full implementation with award calculations.
 * For now, route back to main menu page (0x00) so state 3 → state 1
 * shows the main menu instead of getting stuck on action page 3. */
void FUN_0041d740(void)
{
    LOG("[STUB] FUN_0041d740 - end game stats, returning to main menu\n");
    DAT_004877a4 = 0;  /* Main menu page */
}

/* ===== Early_Init_Vars (0041EAD0) ===== */
void Early_Init_Vars(void)
{
    g_MemoryTracker = 0;
}

/* ===== Init_Game_Config (004207C0) ===== */
void Init_Game_Config(void)
{
    /* Initialize config blob with defaults */
    g_ConfigBlob[0] = 1;     /* 00481F58 */
    g_ConfigBlob[1] = 0;     /* 00481F59 */
    g_ConfigBlob[2] = 1;     /* 00481F5A */
    g_ConfigBlob[3] = 0x32;  /* 00481F5B = 50 */

    /* Clear 800 bytes starting at offset 4 */
    memset(&g_ConfigBlob[4], 0, 800);

    /* Team color palette (X1R5G5B5) - used by FUN_004236f0 sprite variant generator.
     * NOTE: palette[3] is overwritten to 0x7FF0 (gold) by FUN_0042d8b0 before
     * sprites are loaded, so variant 3 is gold in practice, not gray. */
    DAT_00483838[0] = 0x1A56;  /* Teal/cyan */
    DAT_00483838[1] = 0x2ACA;  /* Green */
    DAT_00483838[2] = 0x6508;  /* Red */
    DAT_00483838[3] = 0x6739;  /* Gray (overwritten to 0x7FF0 gold before sprite load) */

    /* Sky pattern type: 0=sprite 0x40, 1=sprite 0x45, 2=sprite 0x46, 3=solid 0x446 */
    g_ConfigBlob[0x1803] = 2;   /* DAT_0048375b - default sky type (cloudy texture) */

    /* Sky config byte 0 (DAT_00483758): used by FUN_0041a8c0 as float LUT index */
    g_ConfigBlob[0x1800] = 3;   /* DAT_00483758 */

    /* TODO: Full config initialization (entity defaults, key bindings, etc.)
     * See DECOMP_PLAN.md Phase 2 for details.
     * For now, enough to pass System_Init_Check. */

    Load_Options_Config();

    /* DIAG: Log sky type after options.cfg load (may have overwritten our default) */
    LOG("[DIAG] Post-options sky_type = %d (g_ConfigBlob[0x1803])\n",
        (int)(unsigned char)g_ConfigBlob[0x1803]);
}

/* ===== Load_Options_Config (0042F360) ===== */
/* Reads 6408 bytes from options.cfg into g_ConfigBlob.
 * Original uses CRT _open/_read/_close; we use fopen for portability. */
void Load_Options_Config(void)
{
    FILE *fp = fopen("options.cfg", "rb");
    if (fp != NULL) {
        fread(g_ConfigBlob, 1, 6408, fp);
        fclose(fp);
    }
}

/* ===== Save_Options_Config (0042F320) ===== */
/* Writes 6408 bytes from g_ConfigBlob to options.cfg. */
void Save_Options_Config(void)
{
    FILE *fp = fopen("options.cfg", "wb");
    if (fp != NULL) {
        fwrite(g_ConfigBlob, 1, 6408, fp);
        fclose(fp);
    }
}

/* ===== Init_Math_Tables (00425780) ===== */
/* Generates sin LUT with 1.25x entries for cosine offset access */
void Init_Math_Tables(int *buffer, unsigned int count)
{
    unsigned int total = count + (count >> 2); /* count * 1.25 */
    double two_pi = 6.283185307179586;

    for (unsigned int i = 0; i < total; i++) {
        double angle = ((double)i / (double)count) * two_pi;
        buffer[i] = (int)(sin(angle) * 524288.0); /* scale = 2^19 = 0x80000 */
    }
}

/* ===== System_Init_Check (0041D480) ===== */
/* Returns: 1=success, 0=file-not-found, 2=no-levels */
int System_Init_Check(void)
{
    DWORD dwTime;
    int iVar2;

    dwTime = timeGetTime();
    srand(dwTime);  /* FUN_00464409 = srand */

    Init_Memory_Pools();
    Init_Math_Tables((int *)DAT_00487ab0, 0x800);
    Init_Game_Config();

    g_LoadedBgIndex = (char)0xFF;
    g_SoundEnabled = 1;

    /* Check sound config - byte+3: 3=NOSOUND */
    if (DAT_00483720[3] != 3) {
        iVar2 = Init_Sound_Hardware();
        if (iVar2 != 0)
            goto MainInit;
    }
    g_SoundEnabled = 0;

MainInit:
    FUN_0041eae0();  /* Entity table init */
    FUN_0045a060();  /* Color LUT generation */
    FUN_0045b2a0();  /* Blend LUT generation */

    /* Initialize physics params (80-byte heap buffer) */
    /* Byte offsets from Ghidra decompilation */
    *(int *)((char *)g_PhysicsParams + 0x08) = 0x19000;
    *(int *)((char *)g_PhysicsParams + 0x04) = 0xC0000;
    *(int *)((char *)g_PhysicsParams + 0x00) = 0;
    *(int *)((char *)g_PhysicsParams + 0x0C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x18) = 0x7800;
    *(int *)((char *)g_PhysicsParams + 0x14) = 0x80000;
    *(int *)((char *)g_PhysicsParams + 0x10) = 0x32;
    *(int *)((char *)g_PhysicsParams + 0x1C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x28) = 0;
    *(int *)((char *)g_PhysicsParams + 0x24) = 0x40000;
    *(int *)((char *)g_PhysicsParams + 0x20) = 0;
    *(int *)((char *)g_PhysicsParams + 0x2C) = 0x2710000;
    *(int *)((char *)g_PhysicsParams + 0x38) = 0;
    *(int *)((char *)g_PhysicsParams + 0x34) = 0x40000;
    *(int *)((char *)g_PhysicsParams + 0x30) = 0;
    *(int *)((char *)g_PhysicsParams + 0x3C) = 0x2710000;

    /* Initialize entity config (40-byte heap buffer) */
    g_EntityConfig[0] = 0x708;
    *((unsigned char *)(g_EntityConfig + 1)) = 8;
    *((unsigned char *)g_EntityConfig + 5) = 4;
    g_EntityConfig[2] = 0x76C;
    *((unsigned char *)(g_EntityConfig + 3)) = 8;
    *((unsigned char *)g_EntityConfig + 0x0D) = 4;
    g_EntityConfig[4] = 0xAF0;
    *((unsigned char *)(g_EntityConfig + 5)) = 0x0C;
    *((unsigned char *)g_EntityConfig + 0x15) = 2;

    FUN_0041fc10();
    FUN_0041fe70();
    FUN_0041f900();

    /* Clear entity array: DAT_00487ac0, 100-byte steps up to 0x489230 */
    /* TODO: implement when we have the DAT_00487ac0 address mapped */

    FUN_00422a10();
    FUN_0042d8b0();

    iVar2 = FUN_00422740();
    if (iVar2 != 1) {
        return 0;
    }

    FUN_00420be0();
    FUN_0041e580();

    iVar2 = FUN_00414060();
    if (iVar2 == 0) {
        return 2;
    }

    FUN_00413f70();

    /* Resolution table */
    g_ModeWidths[0] = 320;   g_ModeHeights[0] = 200;
    g_ModeWidths[1] = 320;   g_ModeHeights[1] = 240;
    g_ModeWidths[2] = 400;   g_ModeHeights[2] = 300;
    g_ModeWidths[3] = 512;   g_ModeHeights[3] = 384;
    g_ModeWidths[4] = 400;   g_ModeHeights[4] = 480;
    g_ModeWidths[5] = 640;   g_ModeHeights[5] = 480;
    g_ModeWidths[6] = 800;   g_ModeHeights[6] = 600;
    g_ModeWidths[7] = 1024;  g_ModeHeights[7] = 768;
    g_ModeWidths[8] = 1152;  g_ModeHeights[8] = 864;
    g_ModeWidths[9] = 1280;  g_ModeHeights[9] = 1024;

    g_SubState = 0;
    g_NeedsRedraw = 0;
    g_NumDisplayModes = 10;

    /* Display mode init */
    DAT_00487640[1] = 0xFF;   /* current mode = unset */
    DAT_00487640[2] = 5;      /* desired mode = index 5 (640x480) */

    FUN_0041e4a0();

    DAT_0048764b = 0;
    DAT_0048764a = 0;

    return 1;
}

/* ===== Init_DirectInput (004620F0) ===== */
int Init_DirectInput(void)
{
    HRESULT hr;
    DIPROPDWORD dipdw;

    /* 1. Create DirectInput interface */
    hr = DirectInputCreateA(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &lpDI, NULL);
    if (FAILED(hr)) {
        return 0;
    }

    /* 2. Keyboard setup */
    hr = lpDI->CreateDevice(GUID_SysKeyboard, &lpDI_Keyboard, NULL);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->SetDataFormat(&c_dfDIKeyboard);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->SetCooperativeLevel(hWnd_Main, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Keyboard->Acquire();
    if (FAILED(hr)) goto cleanup;

    /* 3. Mouse setup */
    hr = lpDI->CreateDevice(GUID_SysMouse, &lpDI_Mouse, NULL);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Mouse->SetDataFormat(&c_dfDIMouse);
    if (FAILED(hr)) goto cleanup;

    hr = lpDI_Mouse->SetCooperativeLevel(hWnd_Main, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) goto cleanup;

    /* 4. Mouse event notification */
    hMouseEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (hMouseEvent == NULL) {
        goto cleanup;
    }

    hr = lpDI_Mouse->SetEventNotification(hMouseEvent);
    if (FAILED(hr)) goto cleanup;

    /* 5. Set mouse buffer size to 16 entries */
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = 16;

    hr = lpDI_Mouse->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph);
    if (FAILED(hr)) goto cleanup;

    /* Acquire mouse (original relies on Input_Update re-acquire path,
     * but acquiring here avoids losing the first frame of input) */
    lpDI_Mouse->Acquire();

    return 1;

cleanup:
    /* Release everything on failure */
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
    return 0;
}
