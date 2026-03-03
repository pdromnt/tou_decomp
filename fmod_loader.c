/*
 * fmod_loader.c - Runtime loader for FMOD 3.x DLL
 *
 * TDM-GCC-64's dlltool cannot generate a valid 32-bit import library,
 * so we load fmod.dll at runtime via LoadLibrary/GetProcAddress.
 * Each wrapper function has the exact __stdcall signature expected
 * by sound.cpp and entity.cpp, providing the _FSOUND_*@N symbols.
 */
#include <windows.h>

static HMODULE g_hFmod = NULL;

/* ---- Function pointer types ---- */
typedef float   (__stdcall *pfn_FSOUND_GetVersion)(void);
typedef void    (__stdcall *pfn_FSOUND_SetOutput)(int);
typedef void    (__stdcall *pfn_FSOUND_SetDriver)(int);
typedef void    (__stdcall *pfn_FSOUND_SetMixer)(int);
typedef int     (__stdcall *pfn_FSOUND_Init)(int, int, unsigned int);
typedef int     (__stdcall *pfn_FSOUND_GetError)(void);
typedef void    (__stdcall *pfn_FSOUND_Close)(void);
typedef void*   (__stdcall *pfn_FSOUND_Sample_Load)(int, const char*, unsigned int, int);
typedef void    (__stdcall *pfn_FSOUND_StopSound)(int);
typedef int     (__stdcall *pfn_FSOUND_GetMaxChannels)(void);
typedef void*   (__stdcall *pfn_FSOUND_Stream_OpenFile)(const char*, unsigned int, int);
typedef int     (__stdcall *pfn_FSOUND_Stream_Play)(int, void*);
typedef void    (__stdcall *pfn_FSOUND_Stream_Stop)(void*);
typedef void    (__stdcall *pfn_FSOUND_Stream_Close)(void*);
typedef int     (__stdcall *pfn_FSOUND_PlaySoundEx)(int, void*, void*, int);
typedef int     (__stdcall *pfn_FSOUND_SetVolume)(int, int);
typedef int     (__stdcall *pfn_FSOUND_SetPaused)(int, int);
typedef int     (__stdcall *pfn_FSOUND_SetPan)(int, int);
typedef int     (__stdcall *pfn_FSOUND_SetVolumeAbsolute)(int, int);
typedef void    (__stdcall *pfn_FSOUND_SetSFXMasterVolume)(int);
typedef void*   (__stdcall *pfn_FMUSIC_LoadSong)(const char*);
typedef int     (__stdcall *pfn_FMUSIC_PlaySong)(void*);
typedef int     (__stdcall *pfn_FMUSIC_StopSong)(void*);
typedef void    (__stdcall *pfn_FMUSIC_FreeSong)(void*);
typedef int     (__stdcall *pfn_FMUSIC_SetMasterVolume)(void*, int);

/* ---- Cached function pointers ---- */
static pfn_FSOUND_GetVersion        p_FSOUND_GetVersion;
static pfn_FSOUND_SetOutput         p_FSOUND_SetOutput;
static pfn_FSOUND_SetDriver         p_FSOUND_SetDriver;
static pfn_FSOUND_SetMixer          p_FSOUND_SetMixer;
static pfn_FSOUND_Init              p_FSOUND_Init;
static pfn_FSOUND_GetError          p_FSOUND_GetError;
static pfn_FSOUND_Close             p_FSOUND_Close;
static pfn_FSOUND_Sample_Load       p_FSOUND_Sample_Load;
static pfn_FSOUND_StopSound         p_FSOUND_StopSound;
static pfn_FSOUND_GetMaxChannels    p_FSOUND_GetMaxChannels;
static pfn_FSOUND_Stream_OpenFile   p_FSOUND_Stream_OpenFile;
static pfn_FSOUND_Stream_Play       p_FSOUND_Stream_Play;
static pfn_FSOUND_Stream_Stop       p_FSOUND_Stream_Stop;
static pfn_FSOUND_Stream_Close      p_FSOUND_Stream_Close;
static pfn_FSOUND_PlaySoundEx       p_FSOUND_PlaySoundEx;
static pfn_FSOUND_SetVolume         p_FSOUND_SetVolume;
static pfn_FSOUND_SetPaused         p_FSOUND_SetPaused;
static pfn_FSOUND_SetPan            p_FSOUND_SetPan;
static pfn_FSOUND_SetVolumeAbsolute p_FSOUND_SetVolumeAbsolute;
static pfn_FSOUND_SetSFXMasterVolume p_FSOUND_SetSFXMasterVolume;
static pfn_FMUSIC_LoadSong          p_FMUSIC_LoadSong;
static pfn_FMUSIC_PlaySong          p_FMUSIC_PlaySong;
static pfn_FMUSIC_StopSong          p_FMUSIC_StopSong;
static pfn_FMUSIC_FreeSong          p_FMUSIC_FreeSong;
static pfn_FMUSIC_SetMasterVolume   p_FMUSIC_SetMasterVolume;

/* ---- Init/cleanup ---- */
int FMOD_LoadLibrary(void)
{
    if (g_hFmod) return 1;
    g_hFmod = LoadLibraryA("fmod.dll");
    if (!g_hFmod) return 0;

    /* DLL exports have underscore prefix + @N suffix for stdcall */
    #define LOAD(var, name) var = (void*)GetProcAddress(g_hFmod, name)

    LOAD(p_FSOUND_GetVersion,        "_FSOUND_GetVersion@0");
    LOAD(p_FSOUND_SetOutput,         "_FSOUND_SetOutput@4");
    LOAD(p_FSOUND_SetDriver,         "_FSOUND_SetDriver@4");
    LOAD(p_FSOUND_SetMixer,          "_FSOUND_SetMixer@4");
    LOAD(p_FSOUND_Init,              "_FSOUND_Init@12");
    LOAD(p_FSOUND_GetError,          "_FSOUND_GetError@0");
    LOAD(p_FSOUND_Close,             "_FSOUND_Close@0");
    LOAD(p_FSOUND_Sample_Load,       "_FSOUND_Sample_Load@16");
    LOAD(p_FSOUND_StopSound,         "_FSOUND_StopSound@4");
    LOAD(p_FSOUND_GetMaxChannels,    "_FSOUND_GetMaxChannels@0");
    LOAD(p_FSOUND_Stream_OpenFile,   "_FSOUND_Stream_OpenFile@12");
    LOAD(p_FSOUND_Stream_Play,       "_FSOUND_Stream_Play@8");
    LOAD(p_FSOUND_Stream_Stop,       "_FSOUND_Stream_Stop@4");
    LOAD(p_FSOUND_Stream_Close,      "_FSOUND_Stream_Close@4");
    LOAD(p_FSOUND_PlaySoundEx,       "_FSOUND_PlaySoundEx@16");
    LOAD(p_FSOUND_SetVolume,         "_FSOUND_SetVolume@8");
    LOAD(p_FSOUND_SetPaused,         "_FSOUND_SetPaused@8");
    LOAD(p_FSOUND_SetPan,            "_FSOUND_SetPan@8");
    LOAD(p_FSOUND_SetVolumeAbsolute, "_FSOUND_SetVolumeAbsolute@8");
    LOAD(p_FSOUND_SetSFXMasterVolume,"_FSOUND_SetSFXMasterVolume@4");
    LOAD(p_FMUSIC_LoadSong,          "_FMUSIC_LoadSong@4");
    LOAD(p_FMUSIC_PlaySong,          "_FMUSIC_PlaySong@4");
    LOAD(p_FMUSIC_StopSong,          "_FMUSIC_StopSong@4");
    LOAD(p_FMUSIC_FreeSong,          "_FMUSIC_FreeSong@4");
    LOAD(p_FMUSIC_SetMasterVolume,   "_FMUSIC_SetMasterVolume@8");

    #undef LOAD
    return 1;
}

void FMOD_FreeLibrary(void)
{
    if (g_hFmod) {
        FreeLibrary(g_hFmod);
        g_hFmod = NULL;
    }
}

/* ---- Wrapper functions ---- */
/* These provide the _FSOUND_*@N symbols that sound.o and entity.o reference */

float __stdcall FSOUND_GetVersion(void) {
    return p_FSOUND_GetVersion ? p_FSOUND_GetVersion() : 0.0f;
}
void __stdcall FSOUND_SetOutput(int t) {
    if (p_FSOUND_SetOutput) p_FSOUND_SetOutput(t);
}
void __stdcall FSOUND_SetDriver(int d) {
    if (p_FSOUND_SetDriver) p_FSOUND_SetDriver(d);
}
void __stdcall FSOUND_SetMixer(int m) {
    if (p_FSOUND_SetMixer) p_FSOUND_SetMixer(m);
}
int __stdcall FSOUND_Init(int rate, int ch, unsigned int flags) {
    return p_FSOUND_Init ? p_FSOUND_Init(rate, ch, flags) : 0;
}
int __stdcall FSOUND_GetError(void) {
    return p_FSOUND_GetError ? p_FSOUND_GetError() : -1;
}
void __stdcall FSOUND_Close(void) {
    if (p_FSOUND_Close) p_FSOUND_Close();
}
void* __stdcall FSOUND_Sample_Load(int idx, const char *name, unsigned int mode, int len) {
    return p_FSOUND_Sample_Load ? p_FSOUND_Sample_Load(idx, name, mode, len) : NULL;
}
void __stdcall FSOUND_StopSound(int ch) {
    if (p_FSOUND_StopSound) p_FSOUND_StopSound(ch);
}
int __stdcall FSOUND_GetMaxChannels(void) {
    return p_FSOUND_GetMaxChannels ? p_FSOUND_GetMaxChannels() : 0;
}
void* __stdcall FSOUND_Stream_OpenFile(const char *fn, unsigned int mode, int memlen) {
    return p_FSOUND_Stream_OpenFile ? p_FSOUND_Stream_OpenFile(fn, mode, memlen) : NULL;
}
int __stdcall FSOUND_Stream_Play(int ch, void *stream) {
    return p_FSOUND_Stream_Play ? p_FSOUND_Stream_Play(ch, stream) : -1;
}
void __stdcall FSOUND_Stream_Stop(void *stream) {
    if (p_FSOUND_Stream_Stop) p_FSOUND_Stream_Stop(stream);
}
void __stdcall FSOUND_Stream_Close(void *stream) {
    if (p_FSOUND_Stream_Close) p_FSOUND_Stream_Close(stream);
}
int __stdcall FSOUND_PlaySoundEx(int ch, void *sptr, void *dsp, int startpaused) {
    return p_FSOUND_PlaySoundEx ? p_FSOUND_PlaySoundEx(ch, sptr, dsp, startpaused) : -1;
}
int __stdcall FSOUND_SetVolume(int ch, int vol) {
    return p_FSOUND_SetVolume ? p_FSOUND_SetVolume(ch, vol) : 0;
}
int __stdcall FSOUND_SetPaused(int ch, int paused) {
    return p_FSOUND_SetPaused ? p_FSOUND_SetPaused(ch, paused) : 0;
}
int __stdcall FSOUND_SetPan(int ch, int pan) {
    return p_FSOUND_SetPan ? p_FSOUND_SetPan(ch, pan) : 0;
}
int __stdcall FSOUND_SetVolumeAbsolute(int ch, int vol) {
    return p_FSOUND_SetVolumeAbsolute ? p_FSOUND_SetVolumeAbsolute(ch, vol) : 0;
}
void __stdcall FSOUND_SetSFXMasterVolume(int vol) {
    if (p_FSOUND_SetSFXMasterVolume) p_FSOUND_SetSFXMasterVolume(vol);
}
void* __stdcall FMUSIC_LoadSong(const char *name) {
    return p_FMUSIC_LoadSong ? p_FMUSIC_LoadSong(name) : NULL;
}
int __stdcall FMUSIC_PlaySong(void *mod) {
    return p_FMUSIC_PlaySong ? p_FMUSIC_PlaySong(mod) : 0;
}
int __stdcall FMUSIC_StopSong(void *mod) {
    return p_FMUSIC_StopSong ? p_FMUSIC_StopSong(mod) : 0;
}
void __stdcall FMUSIC_FreeSong(void *mod) {
    if (p_FMUSIC_FreeSong) p_FMUSIC_FreeSong(mod);
}
int __stdcall FMUSIC_SetMasterVolume(void *mod, int vol) {
    return p_FMUSIC_SetMasterVolume ? p_FMUSIC_SetMasterVolume(mod, vol) : 0;
}
