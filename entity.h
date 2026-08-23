#ifndef TOU_ENTITY_H
#define TOU_ENTITY_H

#include "compat.h"

/* ===== Memory (memory.cpp) ===== */
extern int                   g_MemoryTracker;   /* 004892A0 */
extern int                  *g_PhysicsParams;   /* 00487880 */
extern int                  *g_EntityConfig;    /* 00489EBC */
extern void                 *DAT_00487ab0;      /* Math table buffer */

/* ===== Entity init array (init.cpp) ===== */
extern unsigned char         DAT_00487ac0[6000]; /* 60 records × 100 bytes */

/* ===== Intro particle system (memory.cpp) ===== */
extern int                   DAT_00489248;      /* Entity count (also main entity count in gameplay) */
extern int                   DAT_00489250;      /* Particle count */
extern int                   DAT_0048925c;      /* Misc counter / edge record count */
extern DWORD                 DAT_004877f0;      /* Frame delta time */

/* ===== Entity Rendering Counts (effects.cpp) ===== */
extern int                   DAT_00489274;      /* static entity count (turrets) */
extern int                   DAT_0048924c;      /* dynamic entity count (troopers) */
extern int                   DAT_00489260;      /* projectile count */
extern int                   DAT_0048926c;      /* explosion count */
extern int                   DAT_00489264;      /* misc effect count */
extern int                   DAT_00489268;      /* debris/particle count */
extern int                   DAT_00487808;      /* active player viewport count */

/* ===== Entity Rendering Arrays (allocated in memory.cpp) ===== */
extern void                 *DAT_00489e98;      /* static entity array (16 bytes each) */
extern void                 *DAT_00487884;      /* trooper array (64 bytes each) */
extern void                 *DAT_00481f28;      /* projectile array (64 bytes each) */
extern void                 *DAT_00487a9c;      /* explosion array (32 bytes each) */
extern void                 *DAT_00487830;      /* debris/particle array (32 bytes each) */
extern void                 *DAT_00481f2c;      /* edge record array (32 bytes each) */
extern void                 *DAT_00487780;      /* misc effect array (32 bytes each) */
extern void                 *DAT_00487818;      /* projectile type table (0x140 bytes) */
extern void                 *DAT_00487aa8;      /* particle color palette (256 x RGB555, from pal.col) */
extern void                 *DAT_00481f4c;      /* ship color palette (256 x RGB555, from shipal.col) */
#define                      DAT_0048784c DAT_00487834[6]  /* alias: same memory in original */
extern void                 *DAT_0048781c;      /* entity link table base */
extern char                  DAT_0048373d;       /* friendly fire enabled flag */
extern void                 *DAT_00487ab8;      /* tile edge sprite table */
extern void                 *DAT_004876a0;      /* spawn point array (0xc00, stride 0xc) */
extern void                 *DAT_00487aa0;      /* decoration array (0x800, stride 0x10) */
extern void                 *DAT_00489e80;      /* wall segment array (16 * 0x20) */
extern void                 *DAT_00489e7c;      /* fluid source array (5000 * 0x20) */
extern unsigned short        DAT_0048384e;      /* laser pixel color A */
extern unsigned short        DAT_00483850;      /* laser pixel color B */

/* ===== Entity type definitions (memory.cpp) ===== */
extern void                 *DAT_00487abc;      /* Entity type table (0x11030 bytes, 128 types * 0x218 each) */

/* ===== Explosion/Particle data buffers (memory.cpp) ===== */
extern void                 *DAT_00481f20;      /* Explode descriptor table (160 bytes) */
extern void                 *DAT_00481f34;      /* Particle array (32 bytes * 2000) */
extern void                 *DAT_0048787c;      /* Explode pixel data (430KB) */
extern void                 *DAT_00487aac;      /* Explosion rotation frames (~6.4MB) */
extern void                 *DAT_004892e8;      /* Entity array (128 bytes * 2500) */
extern void                 *DAT_00489230;      /* Brightness remap LUT (128KB) */
extern void                 *DAT_004876a4[100]; /* Color palette tables */
extern void                 *DAT_0048792c[48];  /* Blend LUT tables */

/* ===== Ship/Player Data ===== */
extern int                   DAT_004877f8[4];   /* active player index table (up to 4 viewports) */
extern char                  DAT_00483738;       /* game mode (0=normal, 1=random, 2=config) */
extern short                 DAT_0048373a;       /* initial lives */
extern int                   DAT_00483830;       /* starting health */
extern void                 *DAT_0048780c;       /* ship stats table (0x40 per ship, 9 ships) */
extern unsigned char         DAT_0048236e[];     /* ship type per player (from level data) */
extern char                  DAT_004836ce[];     /* player config ship IDs */
extern char                  DAT_0048378e[];     /* ship-taken flags (9 entries) */
extern void                 *DAT_00489eac[4];    /* per-player visibility buffers */
extern int                   DAT_00487788[4];    /* per-player stat counters */

/* ===== End-Game Award System (init.cpp / FUN_0041d740) ===== */
extern unsigned char         DAT_00487368[362];  /* player award table: [0]=count, [n*0x20+1]=name */
extern char                  DAT_004874c9[6];    /* player award winner indices */
extern unsigned char         DAT_004874d4[362];  /* team award table: [0]=count, [n*0x20+1]=name */
extern char                  DAT_00487635[6];    /* team award winner indices */
extern unsigned char         DAT_00487648;       /* highest team score */
extern char                  DAT_00487644[4];    /* winning team indices */

/* ===== Entity Spawning Config ===== */
extern char                  DAT_00483737;       /* trooper difficulty (0=none, 1-3=density) */
extern char                  DAT_00483736;       /* debris difficulty */
extern char                  DAT_00483735;       /* team base placement mode */
extern char                  DAT_00483734;       /* critter spawn enable flag */
extern char                  DAT_0048373c;       /* team mode flag */
extern char                  DAT_0048372c;       /* ambient particle spawn mode (0=3x, 1=1x, 2=off) */
extern unsigned char         DAT_00483962;       /* team base probability % */
extern unsigned char         DAT_00483754[4];    /* entity enable flags [2]=walls, [3]=projectiles */
extern int                   DAT_00483758;       /* entity density config packed */

/* ===== Entity Spawning Counts ===== */
extern int                   DAT_00489270;       /* wall segment count */
extern int                   DAT_004892d4;       /* spawn point count */
extern int                   DAT_004892d8;       /* decoration count */
extern int                   DAT_004892dc;       /* visibility cursor X */
extern int                   DAT_004892e0;       /* visibility cursor Y */
extern int                   DAT_0048929c;       /* misc counter */
extern int                   DAT_004892c0;       /* misc counter */
extern int                   DAT_00489258;       /* misc counter */
extern char                  DAT_004892a4;       /* team victory flag */
extern char                  DAT_004892a5;       /* activation flag */
extern int                   DAT_00487834[12];   /* entity tracking counters */
extern float                 DAT_004892d0;       /* water level / weather effect */
extern float                 DAT_0048385c;       /* weather/temperature threshold */

/* ===== Entity Behavior (entity.cpp) ===== */
extern int  DAT_00486944[4];       /* per-team stat counters A */
extern int  DAT_00486954[4];       /* per-team stat counters B */
extern int  DAT_00486964;          /* team stat counter total */
extern int  DAT_00486968[80];      /* per-player kills stat array */
extern int  DAT_00486aa8[80];      /* per-player deaths stat array */
extern int  DAT_00486be8[80];      /* per-player damage received stats */
extern int  DAT_00486d28[80];      /* per-player building stats */
extern int  DAT_00486e68[80];      /* per-player damage dealt stats */
extern int  DAT_00486fa8[80];      /* per-player distance traveled */
extern int  DAT_004870e8[80];      /* per-player explosion stats */
extern int  DAT_00487228[80];      /* per-player pickup counter */
extern char DAT_00483747;           /* weapon auto-release mode flag */
extern char DAT_00483745;           /* detonation mode flag */

/* ===== Turret LOS / Targeting (FUN_00458010) ===== */
extern int                   DAT_00481ed0;       /* gravity for current weapon */
extern int                   DAT_00481edc;       /* target position X */
extern int                   DAT_00481ee0;       /* target position Y */
extern int                   DAT_00481ef4;       /* target velocity X */
extern int                   DAT_00481ef8;       /* target velocity Y */
extern int                   DAT_00481efc;       /* distance calculation result */
extern int                   DAT_00481f10;       /* intermediate distance from LOS calc */
extern int                   DAT_00481f00;       /* predicted distance 1 */
extern int                   DAT_00481f04;       /* predicted distance 2 */
extern int                   DAT_00481f08;       /* predicted angle 1 */
extern int                   DAT_00481f0c;       /* predicted angle 2 */
extern int                   DAT_00481ee4;       /* predicted target X (near) */
extern int                   DAT_00481ee8;       /* predicted target Y (near) */
extern int                   DAT_00481eec;       /* predicted target X (far) */
extern int                   DAT_00481ef0;       /* predicted target Y (far) */
extern char                  DAT_00481ed8;       /* arc side flag (0=low, 1=high) */
extern void                 *DAT_00489e90;       /* ballistic arc LUT (word table) */

/* ===== Turret Placement ===== */
extern int                   DAT_00489280;       /* turret array capacity */
extern int                   DAT_0048927c;       /* turret count */
extern int                   DAT_00481f48;       /* turret array ptr (cast from int) */
extern int                   DAT_00489284;       /* turret init counter */

/* ===== Trooper Spawn Placement ===== */
extern int                   DAT_004892c8;       /* trooper spawn point count */
extern int                   DAT_004892cc;       /* trooper spawn flag */

/* ===== Wall Particle System ===== */
void FUN_0044f630(int x, int y, int velX, int velY, float scale, int maxDist, int spread, char direction); /* wall segment ripple */

/* ===== Per-player start weapon (set in weapon loadout grid via right-click) ===== */
extern unsigned char g_StartWeapon[64];

/* ===== Function Prototypes: sim.cpp (gameplay subsystems) ===== */
void FUN_00460d50(void);        /* input/control update */
void FUN_004609e0(void);        /* physics step 1 */
void FUN_00460660(void);        /* half-rate physics */
void FUN_00460ac0(void);        /* collision */
void FUN_00413720(void);        /* entity logic */

/* ===== Terrain/Collision Globals ===== */
extern void                 *DAT_004876b8;      /* color degradation palette LUT */
extern unsigned short        DAT_00481e8c;      /* tile explosion color accumulator */
extern unsigned short        DAT_00481e8e;      /* tile explosion count accumulator */
extern unsigned char         DAT_00481e8f;      /* building collision result flag */
/* DAT_00487880 == g_PhysicsParams (defined in memory.cpp) */
extern char                  DAT_0048373b;      /* shared lives mode flag */
extern char                  DAT_00483744;      /* respawn delay mode */
extern void                 *DAT_00487704[4];   /* color degradation LUT pointers (palette[24..27]) */
extern unsigned int          DAT_00483840;      /* fire color match R threshold */
extern unsigned int          DAT_00483844;      /* fire color match G threshold */
extern unsigned int          DAT_00483848;      /* fire color match B threshold */

/* ===== Sub-functions ===== */
void FUN_0040fd70(int entity_idx, int snd, int x, int y, int vol_override = 0xFF, int param6 = 0x3E8); /* looping positional sound */
int  FUN_00450dd0(int x, int y);                           /* collision passability check */

void FUN_00454340(void);        /* projectile update */
void FUN_0044b0b0(void);        /* Entity_Behavior_Loop (entity.cpp) */
void FUN_0044e510(int *ent);    /* Boundary_Clamp (entity.cpp) */
void FUN_0040f9b0(int snd, int x, int y, int vol_override = 0xFF, int param5 = 0x3E8); /* positional sound */
void FUN_004357b0(int param_1, int param_2, int param_3, unsigned char param_4, char param_5,
                  int param_6, int param_7, int param_8, int param_9,
                  char param_10, char param_11, unsigned char param_12); /* AoE tile damage / explosion */
void FUN_004355d0(unsigned int param_1);         /* building collision for projectiles */
void FUN_00451e70(int particle_idx, int damage); /* building damage */
void FUN_00437cf0(int x, int y, int radius, int palette_id, int owner); /* explosion knockback */
void FUN_00434310(void);        /* weapon/terrain */
void FUN_004527e0(void);        /* sound update */
void FUN_00454b00(void);        /* animation */
void FUN_00458010(void);        /* AI targeting */
void FUN_00453cd0(void);        /* map logic */
void FUN_00455d50(void);        /* bullet update */
void FUN_004571f0(void);        /* explosion/damage */
void FUN_00453a80(void);        /* item/pickup */
void FUN_004573e0(void);        /* particle system */
void FUN_004133d0(char param);  /* turret sound */
void FUN_004533d0(void);        /* conditional half-rate */
void FUN_00453230(void);        /* round-end check */
void FUN_0045ddb2(void);        /* round-end cleanup */
void FUN_0045fc00(void);        /* score/stat update */
void FUN_0045e2c0(void);        /* network sync */
void FUN_004104c0(int index);   /* turret init per-entry */
void FUN_00460cf0(char a, unsigned char b); /* tile replacement helper */
int  FUN_0044dfb0(int player);  /* find spawn point for player */

/* ===== AI / Pathfinding Globals (entity.cpp) ===== */
extern int   DAT_00481eb4;       /* pathfinding frontier count A */
extern int   DAT_00481eb8;       /* pathfinding frontier count B */
extern int   DAT_00481ebc;       /* pathfinding frontier count C */
extern int   DAT_00481ec0;       /* pathfinding frontier count D */
extern int   DAT_00481ec4;       /* AI vision range X */
extern int   DAT_00481ec8;       /* AI vision range Y */
extern int  *DAT_00481ea4;       /* pathfinding read buffer A */
extern int  *DAT_00481e90;       /* pathfinding write buffer A */
extern int  *DAT_00481e9c;       /* pathfinding read count ptr A */
extern int  *DAT_00481eac;       /* pathfinding write count ptr A */
extern int  *DAT_00481ea8;       /* pathfinding read buffer B */
extern int  *DAT_00481e94;       /* pathfinding write buffer B */
extern int  *DAT_00481ea0;       /* pathfinding read count ptr B */
extern int  *DAT_00481eb0;       /* pathfinding write count ptr B */
extern int   DAT_00481e98;       /* pathfinding alternation flag */

/* ===== Pathfinding Work Buffers (memory.cpp) ===== */
extern void *DAT_00481f40;       /* pathfinding buffer 1 (8000 bytes) */
extern void *DAT_00481f3c;       /* pathfinding buffer 2 (8000 bytes) */
extern void *DAT_00481f38;       /* pathfinding buffer 3 (8000 bytes) */
extern void *DAT_00481f44;       /* pathfinding buffer 4 (8000 bytes) */

#endif /* TOU_ENTITY_H */
