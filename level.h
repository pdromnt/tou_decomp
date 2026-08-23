#ifndef TOU_LEVEL_H
#define TOU_LEVEL_H

#include "compat.h"

/* ===== Level/Map counts and arrays ===== */
extern int                   DAT_00485088;      /* total map/level count */
extern int                   DAT_0048508c;      /* GG theme/official level count */
extern void                 *DAT_00485090[300]; /* level name strings */
extern void                 *DAT_00485540[300]; /* level tile data strings */
extern void                 *DAT_004859f0[300]; /* level extra data strings */
extern char                  DAT_00485ea0[300]; /* level generated-map flags */
extern char                  DAT_00487f70[256]; /* formatted level count string */
extern char                  DAT_00480740[256]; /* current GG theme name buffer */
extern int                   DAT_00486484;      /* GG theme count */
extern void                 *DAT_00486488[300]; /* GG theme directory names */
extern int                   DAT_00485fcc;      /* music file count */
extern int                   DAT_00485fd0;      /* selected music index */
extern void                 *DAT_00485fd4[300]; /* music file name strings */

/* ===== GG Generator (gg_gen.cpp) ===== */
extern char          DAT_0048396e[];     /* theme name match buffer */
extern char          DAT_004808e1;       /* simple-mode flag */
extern int           DAT_0048071c;       /* left boundary */
extern int           DAT_00480724;       /* top boundary */
extern int           DAT_00480720;       /* right boundary */
extern int           DAT_00480728;       /* bottom boundary */
extern int           DAT_00480898;       /* tile count */
extern int           DAT_004808d0;       /* GG error code */
extern int           DAT_00480840;       /* pixel write cursor */
extern unsigned int  DAT_0048085c;       /* texture darkness copy */
extern void         *DAT_0048072c;       /* GG work buffer (8MB) */
extern void         *DAT_00481b54;       /* tile dimensions buffer */
extern void         *DAT_00480738;       /* GG buffer */
extern void         *DAT_00480734;       /* GG buffer */
extern void         *DAT_00480730;       /* GG buffer */
extern void         *DAT_004818e4;       /* GG buffer */
extern int           DAT_00480848;       /* half-total area */
extern int           DAT_00480844;       /* cleared area counter */
extern int           DAT_00480860;       /* darkness adjusted value */
extern int           DAT_0048084c;       /* tunnel style */
extern int           DAT_00480854;       /* lighting enable */
extern int           DAT_00480858;       /* border tunnel flag */
extern int           DAT_00480894;       /* main tile sprite index */
extern int           DAT_004808a8;       /* extra tile count */
extern int           DAT_004808c0;       /* decoration count */
extern int           DAT_004808b8;       /* creature count */
extern int           DAT_004808c8;       /* pickup count */
extern int           DAT_004808cc;       /* current sprite index for entity placement */
extern int           DAT_004808b0;       /* treasure count */
extern char          DAT_004839ee;       /* entity enable flag */
extern char          DAT_004839ef;       /* creature density */
extern short         DAT_004839f0;       /* treasure/pickup config */
extern DWORD         DAT_004839f4;       /* progress timer */
extern char          DAT_00481a40;       /* beach style flag */
extern char          DAT_00481a41;       /* texture darkness */
extern int           DAT_00481a34;       /* fixed width */
extern int           DAT_00481a38;       /* fixed height */
extern int           DAT_00481a3c;       /* water config */
extern char          DAT_00481a43;       /* sign name count */
extern char          DAT_00481a44[];     /* sign name table */
extern char          DAT_00481b44[];     /* sign name type table */
extern char          DAT_00481a31;       /* sign text X offset */
extern char          DAT_00481a32;       /* sign text Y offset */
extern int           DAT_00487888[26];   /* Markov starting letter CDF */
extern int           DAT_004892ec[676];  /* Markov transition matrix CDF */

/* GG Function Prototypes */
int  FUN_004143e0(int width, int height);
int  FUN_00415a60(void);              /* info.txt parser */
int  Calc_Power_Of_Two(int value);
int  FUN_00425820(int base, int exp);

/* ===== Level / Map Data (level.cpp) ===== */
extern unsigned int          DAT_004879f0;      /* map width (pixels + 14 border) */
extern unsigned int          DAT_004879f4;      /* map height (pixels + 14 border) */
extern int                   DAT_00487a00;      /* row stride (power-of-2) */
extern int                   DAT_00487a18;      /* shift amount (low byte only) */
extern int                   DAT_004879f8;      /* coarse grid cols (width>>4 + 2) */
extern int                   DAT_004879fc;      /* coarse grid rows (height>>4 + 2) */
extern int                   DAT_00487a04;      /* shadow grid cols (width/18 + 2) */
extern int                   DAT_00487a08;      /* shadow grid rows (height/18 + 2) */
extern void                 *DAT_0048782c;      /* tilemap pointer (byte per cell) */
extern void                 *DAT_00481f50;      /* background RGB565 pixel data */
extern void                 *DAT_00487828;      /* entity placement data (20 bytes each) */
extern int                   DAT_00489278;      /* entity placement count */
extern unsigned char         DAT_00483860[];     /* tile type table from .lev (0x39c bytes) */
extern void                 *DAT_00487814;      /* coarse grid buffer */
extern void                 *DAT_00489ea4;      /* shadow grid buffer 1 */
extern void                 *DAT_00489ea8;      /* shadow grid buffer 2 */
extern void                 *DAT_00489ea0;      /* swap/heightmap data */
extern void                 *DAT_00487820;      /* edge/boundary navigation data */
extern int                   DAT_00487a0c;      /* swap width */
extern int                   DAT_00487a10;      /* swap height */
extern unsigned short        DAT_0048384c;      /* tile fill color */
extern unsigned short        DAT_00480700;      /* turret tile color temp */
extern char                  DAT_0048396d;      /* generated-map flag */
extern char                  DAT_00483960;      /* swap-file enabled flag */
extern char                 *DAT_00486938;      /* current level name pointer */
extern int                   DAT_0048693c;      /* current level index (low byte used) */
extern char                  DAT_004892e4;      /* random mirror flag */
extern char                  DAT_004892e5;      /* difficulty flag (ship select) */
extern char                  DAT_00489d7c[];     /* error string buffer (256 bytes) */
extern void                 *DAT_00487aa4;      /* large game state buffer */
extern int                   DAT_00489254;      /* edge count */
extern void                 *DAT_00489e84;      /* edge record array */
/* Keep the storage pointer byte-addressable while legacy routines are lifted.
 * Typed access must go through Player_Get; changing this global itself to a
 * PlayerData pointer silently scales the many surviving binary byte offsets. */
extern unsigned char        *DAT_00487810;      /* player/ship runtime record storage */
static inline PlayerData *Player_Get(int index)
{
    return reinterpret_cast<PlayerData *>(DAT_00487810 + index * sizeof(PlayerData));
}
extern int                   DAT_00489240;      /* player count */
extern int                   DAT_00489244;      /* active (human) player count */
/* Match-in-progress flag. Set to 1 by the "Start match" menu action
 * (init.cpp case 0x1E) and cleared when returning to menu / on app init.
 * Gates: HUD stat layout (hud.cpp), team-base spawning (menu.cpp),
 * end-of-match timer/warnings (graphics.cpp, sim.cpp), and gates the
 * FUN_0045c300 game-mode preset call (runs only while this is 0).
 * Despite the old "network/tournament" label it is not about networking
 * — this game only ships team deathmatch. */
extern int                   DAT_0048764a;
extern int                   DAT_0048764b;      /* result flag (tournament) */
/* DAT_0048227c is a macro alias into g_ConfigBlob, not a separate variable.
 * In the original binary, address 0x0048227C = 0x00481F58 + 0x324, i.e. it's
 * just a pointer into the config blob. See level.cpp comment. */
#define DAT_0048227c (&g_ConfigBlob[0x324])
extern void                 *DAT_00487928;      /* entity type table (0x10000 bytes) */

/* ===== Function Prototypes: level.cpp ===== */
int  Load_Level_File(const char *level_name);
void FUN_00421310(void);             /* per-level water color computation + LUT rebuild */
void FUN_0045af70(void);             /* build 8 water color LUT tables */
void Assign_Water_Tile_Colors(void); /* set DAT_0048384c on all water tiles in DAT_00481f50 */
int  Load_SWP_Sky(const char *level_name);
int  Load_Image_Data(int jpeg_offset, int extra_offset, int entity_offset,
                     unsigned char *file_buf);

#endif /* TOU_LEVEL_H */
