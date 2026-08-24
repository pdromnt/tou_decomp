#ifndef TOU_SOUND_H
#define TOU_SOUND_H

#include "audio_backend.h"
#include "types.h"

/* ===== Sound (sound.cpp) ===== */
extern SoundEntry           *g_SoundTable;      /* 00487874 */
extern int                   g_SoundEnabled;    /* 00487649 */

/* ===== Positional Sound System ===== */
#define                      DAT_00487840 DAT_00487834[3]  /* alias: 0x487840 = 0x487834 + 3*4 */

/* ===== Function Prototypes: sound.cpp ===== */
int  Init_Sound_Hardware(void);
void FUN_0040e130(void);
void Load_Game_Sounds(void);
void Play_Music(void);
void Stop_All_Sounds(void);
void Pause_Audio_Streams(void);
void Cleanup_Sound(void);

#endif /* TOU_SOUND_H */
