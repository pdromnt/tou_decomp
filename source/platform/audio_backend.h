#ifndef TOU_AUDIO_BACKEND_H
#define TOU_AUDIO_BACKEND_H

typedef unsigned int AudioSampleHandle;

int  Audio_Init(void);
void Audio_Shutdown(void);

AudioSampleHandle Audio_LoadSample(const char *path, int loop);
int  Audio_PlaySample(AudioSampleHandle sample, int volume, int pan);
void Audio_StopChannel(int channel);
void Audio_SetChannelVolume(int channel, int volume);
void Audio_SetChannelPan(int channel, int pan);
void Audio_SetSfxMasterVolume(int volume);

int  Audio_PlayMusic(const char *path, int volume, int paused);
void Audio_StopMusic(void);
void Audio_PauseMusic(int paused);
void Audio_SetMusicVolume(int volume);
int  Audio_HasMusic(void);

void Audio_SetMuted(int muted);
void Audio_StopAll(void);

#endif /* TOU_AUDIO_BACKEND_H */
