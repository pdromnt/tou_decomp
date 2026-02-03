#include "tou.h"
#include <stdio.h>
#include <stdlib.h>

// Global Memory Tracker
int DAT_004892a0 = 0; // Global Memory Tracker

// Memory Pools / Buffers
// Declaring them as void* for now, will refine types later
void *DAT_00487ab4 = NULL;
void *DAT_00489e94 = NULL;
void *DAT_00489234 = NULL;
void *DAT_00489e8c = NULL;
void *DAT_00487ab0 = NULL; // Missing definition
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
void *DAT_004892e8 = NULL; // Fixed to void*
void *DAT_00487884 = NULL;
void *DAT_00487a9c = NULL;
void *DAT_00481f34 = NULL;
void *DAT_00481f2c = NULL;
void *DAT_00481f28 = NULL;
void *DAT_00489e98 = NULL;
void *DAT_00487780 = NULL;
void *DAT_00487818 = NULL;
// DAT_00487880 already defined in init.cpp as array
void *DAT_00487ab8 = NULL;
void *DAT_00487928 = NULL;
int *DAT_00487880 = NULL; // Moved definition here
// Pointers arrays
void *DAT_004876a4[100]; // Range 4876a4-487780
void *DAT_0048792c[48];  // Range 48792c-4879ec?
void *DAT_004878f0[14];  // Range 4878f0-487928?
void *DAT_00487704[4];   // 4 pointers used in color init

void *DAT_00489230 = NULL;
void *DAT_00487aa8 = NULL;
void *DAT_00481f4c = NULL;
void *DAT_00487828 = NULL;
void *DAT_004876a0 =
    NULL; // Used in stack pointer in WinMain? No, stack was &DAT_004766a0
void *DAT_00487aa0 = NULL;
void *DAT_00481f40 = NULL;
void *DAT_00481f3c = NULL;
void *DAT_00481f38 = NULL;
void *DAT_00481f44 = NULL;
unsigned short *DAT_004877c0 = NULL;
char DAT_0048769c = 0xFF;       // Last background index
unsigned char DAT_004877c8 = 0; // Frame Sub-index
int DAT_0048924c = 0;           // Intro Splash Index
int DAT_00481f48 = 0;

// Unknown Globals
int DAT_00489296 = 0;
int DAT_00489297 = 0;
int DAT_00489298 = 0;

// Memory Allocator (0046f4bd)
void *Mem_Alloc(size_t size) {
  void *ptr;
  while (1) {
    ptr = malloc(size);
    if (ptr != NULL) {
      return ptr;
    }
    // Original has a handler callback check here (PTR_FUN_0047f24c)
    // For now, if malloc fails, we assume valid failure unless handler exists.
    // Simplified: return NULL if malloc fails.
    return NULL;
  }
}

// Init Memory Pools (004201a0)
// Allocates massive amounts of memory for game systems
void Init_Memory_Pools(void) {
// Helper macro to alloc and track
#define ALLOC_POOL(ptr, size)                                                  \
  ptr = Mem_Alloc(size);                                                       \
  DAT_004892a0 += (int)size;

  // Allocations (Sizes extracted from decomp)
  ALLOC_POOL(DAT_00487ab4, 2800000);
  ALLOC_POOL(DAT_00489e94, 40000);
  ALLOC_POOL(DAT_00489234, 80000);
  ALLOC_POOL(DAT_00489e8c, 20000);
  ALLOC_POOL(DAT_00489e88, 20000);
  ALLOC_POOL(DAT_00487aac, 0x61aa00); // ~6.4MB
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

  // ... Skipping some smaller ones or implementing full list ...
  // Implementing a few more critical ones
  ALLOC_POOL(DAT_00487818, 0x140);

  DAT_00487880 = (int *)Mem_Alloc(0x1000); // Allocated 4KB for config struct
  DAT_004892a0 += 0x1000;

  // Software_Buffer (640x480x16b * 3 frames)
  DAT_004877c0 = (unsigned short *)Mem_Alloc(0x1c2000);
  DAT_004892a0 += 0x1c2000;

  // DAT_004876a4 (100 pointers)
  for (int i = 0; i < 100; i++) {
    DAT_004876a4[i] = Mem_Alloc(0x2000);
    DAT_004892a0 += 0x2000;
  }

  // DAT_0048792c (48 pointers)
  for (int i = 0; i < 48; i++) {
    DAT_0048792c[i] = Mem_Alloc(0x2000);
    DAT_004892a0 += 0x2000;
  }

  // DAT_004878f0 (14 pointers)
  for (int i = 0; i < 14; i++) {
    DAT_004878f0[i] = Mem_Alloc(0x2000);
    DAT_004892a0 += 0x2000;
  }

  // DAT_00487704 (4 pointers)
  for (int i = 0; i < 4; i++) {
    DAT_00487704[i] = Mem_Alloc(0x800);
    DAT_004892a0 += 0x800;
  }

  ALLOC_POOL(DAT_00487928, 0x10000); // 64KB for large table

  ALLOC_POOL(DAT_00489230, 0x20000);
  ALLOC_POOL(DAT_00487aa8, 0x200);
  ALLOC_POOL(DAT_00481f4c, 0x200);
  ALLOC_POOL(DAT_00487828, 0x5014);
  ALLOC_POOL(DAT_004876a0, 0xc00);
  ALLOC_POOL(DAT_00487aa0, 0x800);

  DAT_00481f48 = 0;
}

// Memory Free Wrapper (0046f4e6)
void Mem_Free(void *ptr) {
  if (ptr != NULL) {
    free(ptr);
  }
}
