#include "fmod_mock.h"
#include "ijl_mock.h"
#include "tou.h"
#include <stdio.h>

// FMOD Stubs
float FSOUND_GetVersion(void) { return 3.75f; }
void FSOUND_SetOutput(int outputtype) {}
void FSOUND_SetDriver(int driver) {}
void FSOUND_SetMixer(int mixer) {}
int FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags) {
  return 1;
}
int FSOUND_GetError(void) { return 0; }
void FSOUND_Close(void) {}
FSOUND_SAMPLE *FSOUND_Sample_Load(int index, const char *name_or_data,
                                  unsigned int mode, int offset, int length) {
  return (FSOUND_SAMPLE *)1;
}

// IJL Stubs
int ijlInit(JPEG_CORE_PROPERTIES *jcprops) { return 0; }
int ijlFree(JPEG_CORE_PROPERTIES *jcprops) { return 0; }
int ijlRead(JPEG_CORE_PROPERTIES *jcprops, int ioType) { return 0; }

// Unknown Function Stubs (Initialization)
// void FUN_0041ead0(void) {} // Implemented in init.cpp
// int FUN_0041d480(void) { return 1; } // Implemented in init.cpp
void FUN_004610a0(void) {}
// int FUN_004620f0(void) { return 0; } // Implemented
// int FUN_00462010(HWND hWnd, int param_2) { return 1; } // Implemented
