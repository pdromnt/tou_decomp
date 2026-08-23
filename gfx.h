#ifndef TOU_GFX_H
#define TOU_GFX_H

#include "compat.h"
#include "types.h"

/* ===== DirectDraw Globals (graphics.cpp) ===== */
extern LPDIRECTDRAW          lpDD;              /* 00489EC8 */
extern LPDIRECTDRAWSURFACE   lpDDS_Primary;     /* 00489ED8 */
extern LPDIRECTDRAWSURFACE   lpDDS_Back;        /* 00489ECC */
extern LPDIRECTDRAWSURFACE   lpDDS_Offscreen;   /* 00489ED0 */
extern LPDIRECTDRAWSURFACE   DAT_00481d44;      /* 00481D44 - offscreen surface 640x480 */

/* ===== Frame / Render (memory.cpp) ===== */
extern unsigned short       *Software_Buffer;   /* 004877C0 */
extern unsigned char         g_FrameIndex;      /* 004877C8 */
extern char                  g_LoadedBgIndex;   /* 0048769C */
extern int                   g_IntroSplashIndex;/* 0048924C */

/* ===== Image Loader (assets.cpp) ===== */
extern int                   g_ImageWidth;      /* 00481D08 */
extern int                   g_ImageHeight;     /* 00481CFC */
extern int                   g_ImageBPP;        /* 00481D04 */
extern int                   g_ImageSize;       /* 00481D00 */

/* ===== Font (assets.cpp) ===== */
extern FontChar              Font_Char_Table[1024];
extern unsigned char        *Font_Pixel_Data;

/* ===== Display Mode (init.cpp) ===== */
extern int                   g_DisplayWidth;    /* 00489238 */
extern int                   g_DisplayHeight;   /* 0048923C */
extern int                   g_NumDisplayModes; /* 00483C00 */
extern int                   g_ModeWidths[16];  /* 00483C04 */
extern int                   g_ModeHeights[16]; /* 00483C44 */

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
extern void                 *DAT_004878f0[14];  /* Color palette LUT pointers (8KB each) */
extern char                  DAT_00481c58[80];   /* Entity config defaults table 1 */
extern char                  DAT_00481ca8[80];   /* Entity config defaults table 2 */
extern int                   DAT_00481d28;      /* Sprite RGB pixel write cursor */
extern int                   DAT_00481d24;      /* Sprite grayscale pixel write cursor */

/* ===== Color Transform (FUN_00424240) ===== */
extern int                   DAT_00481d0c;       /* color matrix min R */
extern int                   DAT_00481d10;       /* color matrix min G */
extern int                   DAT_00481d14;       /* color matrix min B */
extern int                   DAT_00481d18;       /* color threshold R */
extern int                   DAT_00481d1c;       /* color threshold G */
extern int                   DAT_00481d20;       /* color threshold B */
extern int                   DAT_00481d2c;       /* color transition R */
extern int                   DAT_00481d30;       /* color transition G */
extern int                   DAT_00481d34;       /* color transition B */

/* ===== Screen / Viewport dimensions ===== */
extern int                   DAT_00489238;      /* Screen/viewport width (default 640) */
extern int                   DAT_0048923c;      /* Screen/viewport height (default 480) */

/* ===== Function Prototypes: graphics.cpp ===== */
int  Init_DirectDraw(int width, int height);
void Render_Frame(void);
int  Render_Game_View(void);
void Render_Game_View_To(unsigned short *frame);
void Release_DirectDraw_Surfaces(void);
void Restore_Surfaces(void);

/* ===== Function Prototypes: assets.cpp ===== */
int   Load_Background_To_Buffer(char index);
void *Load_JPEG_Asset(const char *filename, int *width, int *height);
void *Load_JPEG_From_Memory(const unsigned char *data, int len, int *out_w, int *out_h);
void  Load_Fonts(void);
void  Draw_Text_To_Buffer(const char *str, int font_idx, int color_idx,
                          unsigned short *dest_buf, int stride, int hover,
                          int max_width, int len);

/* ===== Function Prototypes: effects.cpp ===== */
void FUN_0045a060(void);
void FUN_0045adc0(void);
void FUN_0045b2a0(void);
int  FUN_00422fc0(void);
void FUN_0040c280(int param_1, int param_2, int param_3, unsigned char param_4,
                  int param_5, int param_6, unsigned char param_7);
void FUN_0040c590(int frame, int player, int x, int y, unsigned char palette,
                  int buffer, int stride, unsigned char blend);
void FUN_0040c940(unsigned int px, unsigned int py, unsigned int buffer,
                  int stride, int intensity);
void FUN_0040dbd0(int buffer, unsigned int stride);
void FUN_0040dce0(int buffer, unsigned int stride);
void FUN_0040bb60(unsigned int buffer, unsigned int stride);
void FUN_0040a870(int buffer, unsigned int stride);
void FUN_0040d6c0(int buffer, int stride);
void FUN_0040d810(int buffer, unsigned int stride);
void FUN_0040caf0(int buffer, unsigned int stride);
void FUN_0040d930(int buffer, unsigned int stride);
void FUN_0040d360(int buffer, int stride);
void FUN_0040d100(int buffer, int stride);
void FUN_004076d0(int buffer, int stride);
int  FUN_004257e0(int cx, int cy, int px, int py);
int  FUN_004599f0(int src_x, int src_y, int dst_x, int dst_y, int side, float range_sqrt, int gravity);
char FUN_00459c70(int src_x, int src_y, int dst_x, int dst_y, int angle, float range_sqrt, int gravity);
int  FUN_00459e90(int mult1, int mult2, int weap_idx, float range_sqrt);

/* ===== Function Prototypes: hud.cpp ===== */
void FUN_0040aca0(int buffer, int stride, int player_idx);     /* pickup text */
void FUN_004094f0(int buffer, int stride, int team);           /* team status text */
void FUN_00409280(int buffer, int stride);                     /* timer display */
void FUN_00408f90(unsigned int palette, unsigned short *dest, int stride); /* minimap dot */
void FUN_004090e0(int buffer, int stride, unsigned int player_idx); /* minimap/radar */
void FUN_0040b860(int buffer, int stride, int player_idx);     /* health bar */
void FUN_0040b580(int buffer, int stride, int player_idx);     /* shield/energy bar */
void FUN_004095e0(unsigned int buffer, int stride, int player_idx);  /* fog of war */
void FUN_0040aaf0(int buffer, int stride, int x, int y, int weapon, char state); /* weapon icon */
void FUN_0040a710(int buffer, int stride, int x, int y, int loaded, int total);  /* ammo dots */
void FUN_0040a9e0(int buffer, int stride, int player_idx);     /* weapon grid */

#endif /* TOU_GFX_H */
