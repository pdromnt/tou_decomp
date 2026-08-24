#include "audio_backend.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <math.h>
#include <vector>

struct SdlSample {
    MIX_Audio *audio;
    bool loop;
};

struct SdlChannel {
    MIX_Track *track;
    int volume;
    int pan;
};

static MIX_Mixer *s_Mixer = NULL;
static MIX_Track *s_MusicTrack = NULL;
static MIX_Audio *s_MusicAudio = NULL;
static std::vector<SdlSample> s_Samples;
static std::vector<SdlChannel> s_Channels;
static int s_SfxMasterVolume = 255;
static int s_MusicVolume = 255;
static int s_MusicPaused = 0;
static int s_Muted = 0;

enum { AUDIO_CHANNEL_COUNT = 64 };

static int ClampByte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

static void ApplyChannelMix(SdlChannel &channel)
{
    if (channel.track == NULL)
        return;

    const float gain = ((float)ClampByte(channel.volume) / 255.0f) *
                       ((float)ClampByte(s_SfxMasterVolume) / 255.0f);
    MIX_SetTrackGain(channel.track, gain);

    /* Preserve the original engine's constant-power pan curve. Every bundled
     * SFX is mono, so avoid SDL_mixer's linear example curve here. */
    const float position = (float)ClampByte(channel.pan) / 255.0f;
    MIX_StereoGains stereo;
    stereo.left = sqrtf(1.0f - position);
    stereo.right = sqrtf(position);
    MIX_SetTrackStereo(channel.track, &stereo);
}

int Audio_Init(void)
{
    if (!MIX_Init()) {
        SDL_Log("SDL_mixer initialization failed: %s", SDL_GetError());
        return 0;
    }
    s_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (s_Mixer == NULL) {
        SDL_Log("SDL_mixer device creation failed: %s", SDL_GetError());
        MIX_Quit();
        return 0;
    }

    s_Samples.clear();
    s_Samples.push_back(SdlSample{ NULL, false }); /* handle zero is invalid */
    s_Channels.assign(AUDIO_CHANNEL_COUNT, SdlChannel{ NULL, 255, 128 });
    for (size_t i = 0; i < s_Channels.size(); i++) {
        s_Channels[i].track = MIX_CreateTrack(s_Mixer);
        if (s_Channels[i].track == NULL) {
            Audio_Shutdown();
            return 0;
        }
    }
    s_MusicTrack = MIX_CreateTrack(s_Mixer);
    if (s_MusicTrack == NULL) {
        Audio_Shutdown();
        return 0;
    }
    return 1;
}

void Audio_Shutdown(void)
{
    if (s_MusicTrack != NULL) {
        MIX_StopTrack(s_MusicTrack, 0);
        MIX_SetTrackAudio(s_MusicTrack, NULL);
    }
    if (s_MusicAudio != NULL) {
        MIX_DestroyAudio(s_MusicAudio);
        s_MusicAudio = NULL;
    }
    for (size_t i = 0; i < s_Channels.size(); i++) {
        if (s_Channels[i].track != NULL) {
            MIX_StopTrack(s_Channels[i].track, 0);
            MIX_SetTrackAudio(s_Channels[i].track, NULL);
        }
    }
    for (size_t i = 1; i < s_Samples.size(); i++) {
        if (s_Samples[i].audio != NULL)
            MIX_DestroyAudio(s_Samples[i].audio);
    }
    s_Samples.clear();
    s_Channels.clear();
    s_MusicTrack = NULL;

    if (s_Mixer != NULL) {
        MIX_DestroyMixer(s_Mixer);
        s_Mixer = NULL;
    }
    MIX_Quit();
}

AudioSampleHandle Audio_LoadSample(const char *path, int loop)
{
    if (s_Mixer == NULL)
        return 0;
    MIX_Audio *audio = MIX_LoadAudio(s_Mixer, path, true);
    if (audio == NULL) {
        SDL_Log("Could not load sample %s: %s", path, SDL_GetError());
        return 0;
    }
    s_Samples.push_back(SdlSample{ audio, loop != 0 });
    return (AudioSampleHandle)(s_Samples.size() - 1);
}

int Audio_PlaySample(AudioSampleHandle sample, int volume, int pan)
{
    if (sample == 0 || sample >= s_Samples.size() || s_Channels.empty())
        return -1;

    size_t selected = s_Channels.size();
    int quietest = 256;
    for (size_t i = 0; i < s_Channels.size(); i++) {
        MIX_Track *track = s_Channels[i].track;
        if (!MIX_TrackPlaying(track) && !MIX_TrackPaused(track)) {
            selected = i;
            break;
        }
        if (s_Channels[i].volume < quietest) {
            quietest = s_Channels[i].volume;
            selected = i;
        }
    }
    if (selected >= s_Channels.size())
        return -1;

    SdlChannel &channel = s_Channels[selected];
    MIX_StopTrack(channel.track, 0);
    if (!MIX_SetTrackAudio(channel.track, s_Samples[sample].audio))
        return -1;
    channel.volume = ClampByte(volume);
    channel.pan = ClampByte(pan);
    ApplyChannelMix(channel);

    SDL_PropertiesID options = 0;
    if (s_Samples[sample].loop) {
        options = SDL_CreateProperties();
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    }
    const bool played = MIX_PlayTrack(channel.track, options);
    if (options != 0)
        SDL_DestroyProperties(options);
    return played ? (int)selected : -1;
}

void Audio_StopChannel(int channel)
{
    if (channel >= 0 && (size_t)channel < s_Channels.size())
        MIX_StopTrack(s_Channels[(size_t)channel].track, 0);
}

void Audio_SetChannelVolume(int channel, int volume)
{
    if (channel < 0 || (size_t)channel >= s_Channels.size())
        return;
    s_Channels[(size_t)channel].volume = ClampByte(volume);
    ApplyChannelMix(s_Channels[(size_t)channel]);
}

void Audio_SetChannelPan(int channel, int pan)
{
    if (channel < 0 || (size_t)channel >= s_Channels.size())
        return;
    s_Channels[(size_t)channel].pan = ClampByte(pan);
    ApplyChannelMix(s_Channels[(size_t)channel]);
}

void Audio_SetSfxMasterVolume(int volume)
{
    s_SfxMasterVolume = ClampByte(volume);
    for (size_t i = 0; i < s_Channels.size(); i++)
        ApplyChannelMix(s_Channels[i]);
}

int Audio_PlayMusic(const char *path, int volume, int paused)
{
    Audio_StopMusic();
    if (s_Mixer == NULL || s_MusicTrack == NULL)
        return 0;

    s_MusicAudio = MIX_LoadAudio(s_Mixer, path, false);
    if (s_MusicAudio == NULL) {
        SDL_Log("Could not load music %s: %s", path, SDL_GetError());
        return 0;
    }
    if (!MIX_SetTrackAudio(s_MusicTrack, s_MusicAudio)) {
        Audio_StopMusic();
        return 0;
    }
    s_MusicVolume = ClampByte(volume);
    MIX_SetTrackGain(s_MusicTrack, (float)s_MusicVolume / 255.0f);
    SDL_PropertiesID options = SDL_CreateProperties();
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    bool played = MIX_PlayTrack(s_MusicTrack, options);
    SDL_DestroyProperties(options);
    s_MusicPaused = paused ? 1 : 0;
    if (played && (s_MusicPaused || s_Muted))
        MIX_PauseTrack(s_MusicTrack);
    return played ? 1 : 0;
}

void Audio_StopMusic(void)
{
    if (s_MusicTrack != NULL) {
        MIX_StopTrack(s_MusicTrack, 0);
        MIX_SetTrackAudio(s_MusicTrack, NULL);
    }
    if (s_MusicAudio != NULL) {
        MIX_DestroyAudio(s_MusicAudio);
        s_MusicAudio = NULL;
    }
    s_MusicPaused = 0;
}

void Audio_PauseMusic(int paused)
{
    s_MusicPaused = paused ? 1 : 0;
    if (s_MusicTrack == NULL || s_MusicAudio == NULL)
        return;
    if (s_MusicPaused || s_Muted)
        MIX_PauseTrack(s_MusicTrack);
    else
        MIX_ResumeTrack(s_MusicTrack);
}

void Audio_SetMusicVolume(int volume)
{
    s_MusicVolume = ClampByte(volume);
    if (s_MusicTrack != NULL)
        MIX_SetTrackGain(s_MusicTrack, (float)s_MusicVolume / 255.0f);
}

int Audio_HasMusic(void)
{
    return s_MusicAudio != NULL;
}

void Audio_SetMuted(int muted)
{
    s_Muted = muted ? 1 : 0;
    if (s_Mixer != NULL)
        MIX_SetMixerGain(s_Mixer, s_Muted ? 0.0f : 1.0f);
    if (s_MusicTrack != NULL && s_MusicAudio != NULL) {
        if (s_Muted || s_MusicPaused)
            MIX_PauseTrack(s_MusicTrack);
        else
            MIX_ResumeTrack(s_MusicTrack);
    }
}

void Audio_StopAll(void)
{
    for (size_t i = 0; i < s_Channels.size(); i++)
        MIX_StopTrack(s_Channels[i].track, 0);
    Audio_StopMusic();
}
