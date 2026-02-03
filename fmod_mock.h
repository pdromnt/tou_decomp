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
#define FSOUND_LOOP 0x00000002

// Types
typedef void *FSOUND_SAMPLE;
typedef void *FSOUND_STREAM;
typedef void *FMUSIC_MODULE;

// Functions
#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) float __stdcall FSOUND_GetVersion(void);
__declspec(dllimport) void __stdcall FSOUND_SetOutput(int outputtype);
__declspec(dllimport) void __stdcall FSOUND_SetDriver(int driver);
__declspec(dllimport) void __stdcall FSOUND_SetMixer(int mixer);
__declspec(dllimport) int __stdcall
FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags);
__declspec(dllimport) int __stdcall FSOUND_GetError(void);
__declspec(dllimport) void __stdcall FSOUND_Close(void);
__declspec(dllimport) FSOUND_SAMPLE __stdcall
FSOUND_Sample_Load(int index, const char *name_or_data, unsigned int mode,
                   int length);
__declspec(dllimport) void __stdcall FSOUND_StopSound(int channel);
__declspec(dllimport) int __stdcall FSOUND_GetMaxChannels(void);

// Stream Functions
__declspec(dllimport) FSOUND_STREAM *__stdcall
FSOUND_Stream_OpenFile(const char *filename, unsigned int mode, int memlength);
__declspec(dllimport) int __stdcall FSOUND_Stream_Play(int channel,
                                                       FSOUND_STREAM *stream);
__declspec(dllimport) void __stdcall FSOUND_Stream_Stop(FSOUND_STREAM *stream);
__declspec(dllimport) void __stdcall FSOUND_Stream_Close(FSOUND_STREAM *stream);

// Music Functions
__declspec(dllimport) FMUSIC_MODULE *__stdcall
FMUSIC_LoadSong(const char *name);
__declspec(dllimport) int __stdcall FMUSIC_PlaySong(FMUSIC_MODULE *module);
__declspec(dllimport) int __stdcall FMUSIC_StopSong(FMUSIC_MODULE *module);
__declspec(dllimport) void __stdcall FMUSIC_FreeSong(FMUSIC_MODULE *module);

#ifdef __cplusplus
}
#endif

#endif // FMOD_MOCK_H
