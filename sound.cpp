#include "tou.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

// FMOD Wrapper (Assuming FMOD 3.75 or similar)
// We might need to load these dynamically if we want to be strict about the DLL
// imports But for now we assume we link against fmod.lib

// Global Sound Table
SoundEntry *g_SoundTable = NULL;     // DAT_00487874
extern int DAT_004892a0;             // Linked from memory.cpp
FSOUND_STREAM *lpIntroStream = NULL; // DAT_004806f8

// FMOD Globals/State (Mocking/Using known ones)
// DAT_00483720 likely holds setup config.
struct {
  int OutputType;     // _3_1_
  int Channels;       // _4_4_
                      // ...
} GlobalConfig = {0}; // Placeholder for DAT_00483720

// FMOD Prototypes (if not in fmod.h)
// ...

int Init_Sound_System(void) {
  int iVar1;
  unsigned int uVar2;
  float fVar3;

  // Clear some globals
  // DAT_004806f4 = 0; ...

  // Check Version
  if (FSOUND_GetVersion() < 3.50) { // _DAT_004753f8 assumed to satisfy 3.5
    // Error: "You are using the wrong version of FMOD.DLL!"
    return 0;
  }

  // Output Type
  switch (GlobalConfig.OutputType) {
  case 0:
    uVar2 = FSOUND_OUTPUT_WINMM;
    break;
  case 1:
    uVar2 = FSOUND_OUTPUT_DSOUND;
    break;
  case 2:
    uVar2 = FSOUND_OUTPUT_A3D;
    break;
  case 3:
    uVar2 = FSOUND_OUTPUT_NOSOUND;
    break;
  default:
    FSOUND_SetDriver(0);
    // ...
    break;
  }

  FSOUND_SetOutput(uVar2);
  FSOUND_SetDriver(0);
  FSOUND_SetMixer(FSOUND_MIXER_QUALITY_AUTODETECT); // 4

  // Init (Freq, Channels, Flags)
  // 0xac44 = 44100
  if (FSOUND_Init(44100, 32, FSOUND_INIT_GLOBALFOCUS)) {
    Load_Game_Sounds();
    return 1;
  }

  // Error Handling
  uVar2 = FSOUND_GetError();
  // Log error...
  FSOUND_Close();
  return 0;
}

void Load_Sound_Sample(const char bLoop, int index, int vol_priority,
                       const char *filename) {
  char path[96];
  char temp[96];
  unsigned int handle;
  unsigned int mode;

  // Construct Path: "sfx/" + filename + ".wav"
  strcpy(path, "sfx/");
  strcat(path, filename);
  strcat(path, ".wav");

  // Calculate Mode
  if (bLoop == 0) {
    mode = FSOUND_2D | FSOUND_LOOP_OFF; // 0x2002?
                                        // 0x2000 = FSOUND_HW2D?
                                        // FMOD 3.5:
                                        // 0x00000001 = FSOUND_LOOP_OFF
                                        // 0x00000002 = FSOUND_LOOP_NORMAL
                                        // 0x00002000 = FSOUND_HW2D
                                        // So 0x2001 or 0x2002 makes sense.
  } else {
    mode = FSOUND_2D | FSOUND_LOOP_NORMAL; // 0x2002? Wait loop is 2.
  }

  handle = (unsigned int)FSOUND_Sample_Load(-1, path, mode, 0);

  // Store in Table
  if (g_SoundTable) {
    g_SoundTable[index].handle = handle;

    // Volume/Recalculation?
    // Logic: if (0xff < vol * 2) store 0xff else store vol * 2
    int val = vol_priority * 2;
    if (val > 0xFF)
      val = 0xFF;
    g_SoundTable[index].volume = (unsigned char)val;
  }
}

void Load_Game_Sounds(void) {
  // Allocate Table
  g_SoundTable = (SoundEntry *)malloc(0x960);
  if (!g_SoundTable)
    return;
  memset(g_SoundTable, 0, 0x960);

  DAT_004892a0 += 0x960; // Memory tracking

  // Load Sounds
  // Format: Loop?, Index, Vol/Prio, Name
  Load_Sound_Sample(0, 0x1c, 0x40, "mini_r"); // s_mini_r_0047b7d0
  Load_Sound_Sample(0, 0x1d, 0x40, "mega.1");
  // ... Add all calls from 0040e9e0 ... (truncated list for now)
  Load_Sound_Sample(0, 0x1e, 0x40, "phot_r");
  // ...
}

// Play_Music
extern FSOUND_STREAM *lpIntroStream;
void Play_Music(void) {
  if (lpIntroStream) {
    FSOUND_Stream_Stop(lpIntroStream);
    FSOUND_Stream_Close(lpIntroStream);
    lpIntroStream = NULL;
  }

  lpIntroStream =
      FSOUND_Stream_OpenFile("music/mainmenu.ogg", FSOUND_LOOP | FSOUND_2D, 0);
  if (lpIntroStream) {
    FSOUND_Stream_Play(FSOUND_FREE, lpIntroStream);
    LOG("[INFO] Playing Music: music/mainmenu.ogg\n");
  } else {
    LOG("[ERROR] Failed to open music/mainmenu.ogg\n");
  }
}

// Stop All Sounds (0040e7c0)
void Stop_All_Sounds(void) {
  int max_channels = FSOUND_GetMaxChannels();
  for (int i = 0; i < max_channels; i++)
    FSOUND_StopSound(i);
  if (lpIntroStream) {
    FSOUND_Stream_Stop(lpIntroStream);
  }
}

// Cleanup Sound System (0040e880)
void Cleanup_Sound(void) {
  if (g_SoundTable) {
    // Free songs/streams if implementing FMUSIC
    // ...

    // FSOUND_Close();
    free(g_SoundTable);
    g_SoundTable = NULL;
  }
}

// Init Sound/Graphics for Menu (0040e130)
// Wrapper to start menu music
void FUN_0040e130(void) {
  LOG("[INFO] FUN_0040e130: Initializing Sound for Menu.\n");
  Stop_All_Sounds();
  Play_Music(); // Starts music/mainmenu.ogg
}
