#include "audio_backend.h"
#include "fmod.h"

#include <string.h>
#include <vector>

static std::vector<FSOUND_SAMPLE *> s_Samples;
static FSOUND_STREAM *s_MusicStream = NULL;
static FMUSIC_MODULE *s_MusicModule = NULL;
static int s_MusicChannel = -1;
static int s_SfxMasterVolume = 255;
static int s_MusicVolume = 255;
static int s_MusicPaused = 0;
static int s_Muted = 0;

static int IsStreamMusic(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
        return 0;
    ext++;
    return _stricmp(ext, "mp3") == 0 || _stricmp(ext, "ogg") == 0 ||
           _stricmp(ext, "wma") == 0 || _stricmp(ext, "asf") == 0;
}

int Audio_Init(int max_channels, int legacy_output_type)
{
    if (!FMOD_LoadLibrary() || FSOUND_GetVersion() != 3.5f)
        return 0;

    int output_type = FSOUND_OUTPUT_WINMM;
    if (legacy_output_type == 1) output_type = FSOUND_OUTPUT_DSOUND;
    else if (legacy_output_type == 2) output_type = FSOUND_OUTPUT_A3D;
    else if (legacy_output_type == 3) output_type = FSOUND_OUTPUT_NOSOUND;
    FSOUND_SetOutput(output_type);
    FSOUND_SetDriver(0);
    FSOUND_SetMixer(FSOUND_MIXER_QUALITY_AUTODETECT);
    if (max_channels <= 0) max_channels = 32;
    if (!FSOUND_Init(44100, max_channels, FSOUND_INIT_GLOBALFOCUS))
        return 0;
    s_Samples.clear();
    s_Samples.push_back(NULL);
    return 1;
}

void Audio_Shutdown(void)
{
    Audio_StopAll();
    FSOUND_Close();
    s_Samples.clear();
}

AudioSampleHandle Audio_LoadSample(const char *path, int loop)
{
    unsigned int mode = FSOUND_2D | (loop ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF);
    FSOUND_SAMPLE *sample = (FSOUND_SAMPLE *)FSOUND_Sample_Load(-1, path, mode, 0);
    if (sample == NULL)
        return 0;
    s_Samples.push_back(sample);
    return (AudioSampleHandle)(s_Samples.size() - 1);
}

int Audio_PlaySample(AudioSampleHandle sample, int volume, int pan)
{
    if (sample == 0 || sample >= s_Samples.size())
        return -1;
    int channel = FSOUND_PlaySoundEx(FSOUND_FREE, s_Samples[sample], NULL, 1);
    if (channel < 0)
        return -1;
    FSOUND_SetVolume(channel, volume);
    FSOUND_SetPan(channel, pan);
    FSOUND_SetPaused(channel, 0);
    return channel;
}

void Audio_StopChannel(int channel) { if (channel >= 0) FSOUND_StopSound(channel); }
void Audio_SetChannelVolume(int channel, int volume) { if (channel >= 0) FSOUND_SetVolume(channel, volume); }
void Audio_SetChannelPan(int channel, int pan) { if (channel >= 0) FSOUND_SetPan(channel, pan); }

void Audio_SetSfxMasterVolume(int volume)
{
    s_SfxMasterVolume = volume;
    FSOUND_SetSFXMasterVolume(s_Muted ? 0 : volume);
}

int Audio_PlayMusic(const char *path, int volume, int paused)
{
    Audio_StopMusic();
    s_MusicVolume = volume;
    s_MusicPaused = paused ? 1 : 0;
    if (IsStreamMusic(path)) {
        s_MusicStream = FSOUND_Stream_OpenFile(path, FSOUND_LOOP | FSOUND_2D | 0x09, 0);
        if (s_MusicStream == NULL) return 0;
        s_MusicChannel = FSOUND_Stream_Play(FSOUND_FREE, s_MusicStream);
        if (s_MusicChannel < 0) return 0;
        FSOUND_SetPan(s_MusicChannel, -1);
        FSOUND_SetVolumeAbsolute(s_MusicChannel, s_Muted ? 0 : volume);
        FSOUND_SetPaused(s_MusicChannel, s_MusicPaused || s_Muted);
        return 1;
    }
    s_MusicModule = (FMUSIC_MODULE *)FMUSIC_LoadSong(path);
    if (s_MusicModule == NULL) return 0;
    FMUSIC_SetMasterVolume(s_MusicModule, s_Muted ? 0 : volume);
    return FMUSIC_PlaySong(s_MusicModule);
}

void Audio_StopMusic(void)
{
    if (s_MusicModule != NULL) {
        FMUSIC_StopSong(s_MusicModule);
        FMUSIC_FreeSong(s_MusicModule);
        s_MusicModule = NULL;
    }
    if (s_MusicStream != NULL) {
        FSOUND_Stream_Stop(s_MusicStream);
        FSOUND_Stream_Close(s_MusicStream);
        s_MusicStream = NULL;
    }
    s_MusicChannel = -1;
}

void Audio_PauseMusic(int paused)
{
    s_MusicPaused = paused ? 1 : 0;
    if (s_MusicStream != NULL && s_MusicChannel >= 0)
        FSOUND_SetPaused(s_MusicChannel, s_MusicPaused || s_Muted);
}

void Audio_SetMusicVolume(int volume)
{
    s_MusicVolume = volume;
    if (s_MusicStream != NULL && s_MusicChannel >= 0)
        FSOUND_SetVolumeAbsolute(s_MusicChannel, s_Muted ? 0 : volume);
    if (s_MusicModule != NULL)
        FMUSIC_SetMasterVolume(s_MusicModule, s_Muted ? 0 : volume);
}

int Audio_HasMusic(void) { return s_MusicStream != NULL || s_MusicModule != NULL; }

void Audio_SetMuted(int muted)
{
    s_Muted = muted ? 1 : 0;
    FSOUND_SetSFXMasterVolume(s_Muted ? 0 : s_SfxMasterVolume);
    Audio_SetMusicVolume(s_MusicVolume);
    Audio_PauseMusic(s_MusicPaused);
}

void Audio_StopAll(void)
{
    int max_channels = FSOUND_GetMaxChannels();
    for (int i = 0; i < max_channels; i++)
        FSOUND_StopSound(i);
    Audio_StopMusic();
}
