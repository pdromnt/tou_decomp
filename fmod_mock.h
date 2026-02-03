#ifndef FMOD_MOCK_H
#define FMOD_MOCK_H

// Constants
#define FSOUND_OUTPUT_WINMM 0
#define FSOUND_OUTPUT_DSOUND 1
#define FSOUND_OUTPUT_A3D 2
#define FSOUND_OUTPUT_NOSOUND 3

#define FSOUND_MIXER_QUALITY_AUTODETECT 4
#define FSOUND_INIT_GLOBALFOCUS 1

#define FSOUND_FREE -1
#define FSOUND_2D 0x2000
#define FSOUND_LOOP_OFF 1
#define FSOUND_LOOP_NORMAL 2

// Types
typedef void *FSOUND_SAMPLE;

// Functions
float FSOUND_GetVersion(void);
void FSOUND_SetOutput(int outputtype);
void FSOUND_SetDriver(int driver);
void FSOUND_SetMixer(int mixer);
int FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags);
int FSOUND_GetError(void);
void FSOUND_Close(void);
FSOUND_SAMPLE *FSOUND_Sample_Load(int index, const char *name_or_data,
                                  unsigned int mode, int offset, int length);

#endif // FMOD_MOCK_H
