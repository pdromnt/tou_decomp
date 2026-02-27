/*
 * sound.cpp - FMOD 3.x sound init, samples, streaming music
 * Addresses: Init_Sound_Hardware=0040E530, Load_Game_Sounds=0040E9E0,
 *            Stop_All_Sounds=0040E7C0, Cleanup_Sound=0040E880,
 *            FUN_0040e130=0040E130, Pause_Audio_Streams=0040E7E0
 */
#include "tou.h"
#include <stdio.h>
#include <string.h>

/* ===== Globals defined in this module ===== */
SoundEntry     *g_SoundTable   = NULL;   /* 00487874 */
FSOUND_STREAM  *g_MusicStream  = NULL;   /* 004806F8 */
int             g_MusicChannel = -1;     /* 004806FC */

/* ===== Init_Sound_Hardware (0040E530) ===== */
int Init_Sound_Hardware(void)
{
    unsigned int outputType;

    if (FSOUND_GetVersion() < 3.50f) {
        LOG("[SND] FMOD version too old\n");
        return 0;
    }

    /* DAT_00483720[3] holds output type: 0=WINMM, 1=DSOUND, 2=A3D, 3=NOSOUND */
    switch (DAT_00483720[3]) {
    case 0:  outputType = FSOUND_OUTPUT_WINMM;   break;
    case 1:  outputType = FSOUND_OUTPUT_DSOUND;   break;
    case 2:  outputType = FSOUND_OUTPUT_A3D;      break;
    case 3:  outputType = FSOUND_OUTPUT_NOSOUND;  break;
    default: outputType = FSOUND_OUTPUT_NOSOUND;  break;
    }

    FSOUND_SetOutput(outputType);
    FSOUND_SetDriver(0);
    FSOUND_SetMixer(FSOUND_MIXER_QUALITY_AUTODETECT);

    if (FSOUND_Init(44100, 32, FSOUND_INIT_GLOBALFOCUS)) {
        LOG("[SND] FMOD initialized OK\n");
        Load_Game_Sounds();
        return 1;
    }

    LOG("[SND] FMOD init failed\n");
    FSOUND_Close();
    return 0;
}

/* ===== Load_Sound_Sample ===== */
static void Load_Sound_Sample(const char bLoop, int index, int vol_priority,
                              const char *filename)
{
    char path[96];
    unsigned int mode;

    strcpy(path, "sfx/");
    strcat(path, filename);
    strcat(path, ".wav");

    if (bLoop == 0) {
        mode = FSOUND_2D | FSOUND_LOOP_OFF;
    } else {
        mode = FSOUND_2D | FSOUND_LOOP_NORMAL;
    }

    unsigned int handle = (unsigned int)FSOUND_Sample_Load(-1, path, mode, 0);

    if (g_SoundTable) {
        g_SoundTable[index].handle = handle;
        int val = vol_priority * 2;
        if (val > 0xFF) val = 0xFF;
        g_SoundTable[index].volume = (unsigned char)val;
    }
}

/* ===== Load_Game_Sounds (0040E9E0) ===== */
void Load_Game_Sounds(void)
{
    g_SoundTable = (SoundEntry *)Mem_Alloc(0x960);
    if (!g_SoundTable)
        return;
    memset(g_SoundTable, 0, 0x960);
    g_MemoryTracker += 0x960;

    /* TODO: Full sound table from 0040E9E0 */
    Load_Sound_Sample(0, 0x1c, 0x40, "mini_r");
    Load_Sound_Sample(0, 0x1d, 0x40, "mega.1");
    Load_Sound_Sample(0, 0x1e, 0x40, "phot_r");
}

/* ===== FUN_0040e130 (0040E130) - Menu/intro music init ===== */
/*
 * Original: Complex music selection with file type detection.
 * Simplified: Stop existing, open stream, play, pause if in intro state.
 */
void FUN_0040e130(void)
{
    if (!g_SoundEnabled) {
        return;
    }

    /* Stop existing music */
    if (g_MusicStream) {
        FSOUND_Stream_Stop(g_MusicStream);
        FSOUND_Stream_Close(g_MusicStream);
        g_MusicStream = NULL;
    }
    g_MusicChannel = -1;

    /* Set SFX master volume from config */
    int sfxVol = ((DAT_00483720[1]) * 0xFF) / 100;
    FSOUND_SetSFXMasterVolume(sfxVol);

    /* Open stream - use mainmenu.ogg for now
     * Original selects from a music table based on game state */
    g_MusicStream = FSOUND_Stream_OpenFile("music/mainmenu.ogg",
                                           FSOUND_LOOP | FSOUND_2D | 0x09, 0);
    if (g_MusicStream) {
        g_MusicChannel = FSOUND_Stream_Play(FSOUND_FREE, g_MusicStream);
        if (g_MusicChannel != -1) {
            FSOUND_SetPan(g_MusicChannel, -1); /* center pan */
            /* KEY: Pause music if we're in intro state (0x97) */
            FSOUND_SetPaused(g_MusicChannel, DAT_004877a4 == 0x97);
            LOG("[SND] Music started, channel=%d, paused=%d\n",
                g_MusicChannel, DAT_004877a4 == 0x97);

            /* Set music volume */
            int musVol = ((DAT_00483720[0]) * 0xFF) / 100;
            FSOUND_SetVolumeAbsolute(g_MusicChannel, musVol);
        }
    }
}

/* ===== Pause_Audio_Streams (0040E7E0) - Actually UNPAUSES ===== */
void Pause_Audio_Streams(void)
{
    if (g_SoundEnabled && g_MusicStream) {
        FSOUND_SetPaused(g_MusicChannel, 0);
    }
}

/* ===== Stop_All_Sounds (0040E7C0) ===== */
void Stop_All_Sounds(void)
{
    if (!g_SoundEnabled) return;

    int max_channels = FSOUND_GetMaxChannels();
    for (int i = 0; i < max_channels; i++)
        FSOUND_StopSound(i);
    if (g_MusicStream) {
        FSOUND_Stream_Stop(g_MusicStream);
    }
}

/* ===== Cleanup_Sound (0040E880) ===== */
void Cleanup_Sound(void)
{
    if (g_SoundEnabled) {
        FSOUND_Close();
    }
    if (g_SoundTable) {
        free(g_SoundTable);
        g_SoundTable = NULL;
    }
}
