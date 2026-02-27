/*
 * memory.cpp - Memory allocation, pool initialization
 * Addresses: Init_Memory_Pools=004201A0, Mem_Alloc=0046F4BD, Mem_Free=0046F4E6
 */
#include "tou.h"
#include <stdio.h>
#include <stdlib.h>

/* ===== Globals defined in this module ===== */
int g_MemoryTracker = 0;            /* 004892A0 */
unsigned short *Software_Buffer = NULL; /* 004877C0 */
unsigned char g_FrameIndex = 0;     /* 004877C8 */
char g_LoadedBgIndex = (char)0xFF;  /* 0048769C */
int g_IntroSplashIndex = 0;         /* 0048924C */
int *g_PhysicsParams = NULL;        /* 00487880 */
int *g_EntityConfig = NULL;         /* 00489EBC */
int DAT_00481f48 = 0;
int DAT_00489248 = 0;     /* Entity count (intro) */
int DAT_00489250 = 0;     /* Particle count (intro) */
int DAT_0048925c = 0;     /* Misc counter (intro) */
DWORD DAT_004877f0 = 0;   /* Frame delta time */

/* Memory pool pointers */
void *DAT_00487ab4 = NULL;
void *DAT_00489e94 = NULL;
void *DAT_00489234 = NULL;
void *DAT_00489e8c = NULL;
void *DAT_00487ab0 = NULL;  /* Math table buffer */
void *DAT_00489e88 = NULL;
void *DAT_00487aac = NULL;
void *DAT_0048787c = NULL;
void *DAT_00481f20 = NULL;
void *DAT_00481cf8 = NULL;
void *DAT_00481f24 = NULL;
void *DAT_00487878 = NULL;
void *DAT_00489e90 = NULL;
void *DAT_00489e84 = NULL;
void *DAT_00489e7c = NULL;
void *DAT_0048781c = NULL;
void *DAT_004892e8 = NULL;
void *DAT_00487884 = NULL;
void *DAT_00487a9c = NULL;
void *DAT_00481f34 = NULL;
void *DAT_00481f2c = NULL;
void *DAT_00481f28 = NULL;
void *DAT_00489e98 = NULL;
void *DAT_00487780 = NULL;
void *DAT_00487818 = NULL;
void *DAT_00487ab8 = NULL;
void *DAT_00487928 = NULL;

/* Pointer arrays */
void *DAT_004876a4[100];
void *DAT_0048792c[48];
void *DAT_004878f0[14];
void *DAT_00487704[4];

void *DAT_00487abc = NULL;    /* Entity type definitions (0x11030 bytes, 128 types * 0x218 each) */
int   DAT_00481d28 = 0;       /* Sprite RGB pixel write cursor */
int   DAT_00481d24 = 0;       /* Sprite grayscale pixel write cursor */

void *DAT_00489230 = NULL;
void *DAT_00487aa8 = NULL;
void *DAT_00481f4c = NULL;
void *DAT_00487828 = NULL;
void *DAT_004876a0 = NULL;
void *DAT_00487aa0 = NULL;
void *DAT_00481f40 = NULL;
void *DAT_00481f3c = NULL;
void *DAT_00481f38 = NULL;
void *DAT_00481f44 = NULL;

/* ===== Mem_Alloc (0046F4BD) ===== */
void *Mem_Alloc(size_t size)
{
    void *ptr = malloc(size);
    return ptr;
}

/* ===== Mem_Free (0046F4E6) ===== */
void Mem_Free(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
}

/* ===== Init_Memory_Pools (004201A0) ===== */
void Init_Memory_Pools(void)
{
#define ALLOC_POOL(ptr, size) \
    ptr = Mem_Alloc(size); \
    g_MemoryTracker += (int)(size);

    ALLOC_POOL(DAT_00487ab4, 2800000);
    ALLOC_POOL(DAT_00489e94, 40000);
    ALLOC_POOL(DAT_00489234, 80000);
    ALLOC_POOL(DAT_00489e8c, 20000);
    ALLOC_POOL(DAT_00489e88, 20000);
    ALLOC_POOL(DAT_00487aac, 0x61aa00);  /* ~6.4MB */
    ALLOC_POOL(DAT_0048787c, 430000);
    ALLOC_POOL(DAT_00481f20, 0xf0);
    ALLOC_POOL(DAT_00481cf8, 3600000);
    ALLOC_POOL(DAT_00481f24, 0x1900);
    ALLOC_POOL(DAT_00487878, 120000);
    ALLOC_POOL(DAT_00487ab0, 0x2800);

    ALLOC_POOL(DAT_00489e90, 70000);
    ALLOC_POOL(DAT_00489e84, 80000);
    ALLOC_POOL(DAT_00489e7c, 160000);
    ALLOC_POOL(DAT_0048781c, 0x30000);
    ALLOC_POOL(DAT_004892e8, 0x51400);

    ALLOC_POOL(DAT_00487884, 0x6400);
    ALLOC_POOL(DAT_00487a9c, 0xc80);
    ALLOC_POOL(DAT_00481f34, 64000);
    ALLOC_POOL(DAT_00481f2c, 48000);
    ALLOC_POOL(DAT_00481f28, 0x3200);
    ALLOC_POOL(DAT_00489e98, 0x40);
    ALLOC_POOL(DAT_00487780, 800);
    ALLOC_POOL(DAT_00487818, 0x140);

    /* Physics params - 80 bytes */
    g_PhysicsParams = (int *)Mem_Alloc(0x1000);
    g_MemoryTracker += 0x1000;

    /* Entity config - 40 bytes */
    g_EntityConfig = (int *)Mem_Alloc(0x100);
    g_MemoryTracker += 0x100;

    /* Software_Buffer: 640x480x16bit x 3 frames */
    Software_Buffer = (unsigned short *)Mem_Alloc(0x1c2000);
    g_MemoryTracker += 0x1c2000;

    /* DAT_004876a4: 100 pointers, 8KB each */
    for (int i = 0; i < 100; i++) {
        DAT_004876a4[i] = Mem_Alloc(0x2000);
        g_MemoryTracker += 0x2000;
    }

    /* DAT_0048792c: 48 pointers, 8KB each */
    for (int i = 0; i < 48; i++) {
        DAT_0048792c[i] = Mem_Alloc(0x2000);
        g_MemoryTracker += 0x2000;
    }

    /* DAT_004878f0: 14 pointers, 8KB each */
    for (int i = 0; i < 14; i++) {
        DAT_004878f0[i] = Mem_Alloc(0x2000);
        g_MemoryTracker += 0x2000;
    }

    /* DAT_00487704: 4 pointers, 2KB each */
    for (int i = 0; i < 4; i++) {
        DAT_00487704[i] = Mem_Alloc(0x800);
        g_MemoryTracker += 0x800;
    }

    ALLOC_POOL(DAT_00487abc, 0x11030);

    ALLOC_POOL(DAT_00487928, 0x10000);

    ALLOC_POOL(DAT_00489230, 0x20000);
    ALLOC_POOL(DAT_00487aa8, 0x200);
    ALLOC_POOL(DAT_00481f4c, 0x200);
    ALLOC_POOL(DAT_00487828, 0x5014);
    ALLOC_POOL(DAT_004876a0, 0xc00);
    ALLOC_POOL(DAT_00487aa0, 0x800);

    ALLOC_POOL(DAT_00481f40, 8000);
    ALLOC_POOL(DAT_00481f3c, 8000);
    ALLOC_POOL(DAT_00481f38, 8000);
    ALLOC_POOL(DAT_00481f44, 8000);

    DAT_00481f48 = 0;

#undef ALLOC_POOL
}
