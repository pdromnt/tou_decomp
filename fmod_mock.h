#ifndef FMOD_MOCK_H
#define FMOD_MOCK_H

// Constants — values from original binary (Ghidra: case 0->2, 1->1, 2->3, 3->0)
#define FSOUND_OUTPUT_NOSOUND 0
#define FSOUND_OUTPUT_DSOUND 1
#define FSOUND_OUTPUT_WINMM 2
#define FSOUND_OUTPUT_A3D 3

#define FSOUND_MIXER_QUALITY_AUTODETECT 4
#define FSOUND_INIT_GLOBALFOCUS 1

#define FSOUND_FREE -1
#define FSOUND_2D 0x2000
#define FSOUND_LOOP_OFF 1
#define FSOUND_LOOP_NORMAL 2
#define FSOUND_LOOP 0x00000002

// Types
typedef void *FSOUND_SAMPLE;
typedef void *FSOUND_STREAM;
typedef void *FMUSIC_MODULE;

// Runtime loader (fmod_loader.c) — call before any FMOD function
#ifdef __cplusplus
extern "C" {
#endif

int FMOD_LoadLibrary(void);
void FMOD_FreeLibrary(void);

// FMOD function prototypes (implemented as wrappers in fmod_loader.c)
float __stdcall FSOUND_GetVersion(void);
void __stdcall FSOUND_SetOutput(int outputtype);
void __stdcall FSOUND_SetDriver(int driver);
void __stdcall FSOUND_SetMixer(int mixer);
int __stdcall FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags);
int __stdcall FSOUND_GetError(void);
void __stdcall FSOUND_Close(void);
FSOUND_SAMPLE __stdcall FSOUND_Sample_Load(int index, const char *name_or_data, unsigned int mode, int length);
void __stdcall FSOUND_StopSound(int channel);
int __stdcall FSOUND_GetMaxChannels(void);

// Stream Functions
FSOUND_STREAM *__stdcall FSOUND_Stream_OpenFile(const char *filename, unsigned int mode, int memlength);
int __stdcall FSOUND_Stream_Play(int channel, FSOUND_STREAM *stream);
void __stdcall FSOUND_Stream_Stop(FSOUND_STREAM *stream);
void __stdcall FSOUND_Stream_Close(FSOUND_STREAM *stream);

// Channel control
int __stdcall FSOUND_PlaySoundEx(int channel, FSOUND_SAMPLE *sptr, void *dsp, int startpaused);
int __stdcall FSOUND_SetVolume(int channel, int vol);
int __stdcall FSOUND_SetPaused(int channel, int paused);
int __stdcall FSOUND_SetPan(int channel, int pan);
int __stdcall FSOUND_SetVolumeAbsolute(int channel, int vol);
void __stdcall FSOUND_SetSFXMasterVolume(int vol);

// Music Functions
FMUSIC_MODULE *__stdcall FMUSIC_LoadSong(const char *name);
int __stdcall FMUSIC_PlaySong(FMUSIC_MODULE *module);
int __stdcall FMUSIC_StopSong(FMUSIC_MODULE *module);
void __stdcall FMUSIC_FreeSong(FMUSIC_MODULE *module);
int __stdcall FMUSIC_SetMasterVolume(FMUSIC_MODULE *module, int vol);

#ifdef __cplusplus
}
#endif

#endif // FMOD_MOCK_H
