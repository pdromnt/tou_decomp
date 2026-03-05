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

/* ===== Init_Sound_Hardware (0040DF60) ===== */
/* Original at 0040DF60 (was mislabeled as 0040E530).
 * Clears music globals, checks FMOD version == 3.5, sets output/driver/mixer,
 * inits FMOD with sample rate 44100 and max channels from config. */
int Init_Sound_Hardware(void)
{
    unsigned int outputType;

    /* Load FMOD DLL at runtime (workaround for 64-bit toolchain) */
    if (!FMOD_LoadLibrary()) {
        LOG("[SND] Failed to load fmod.dll\n");
        return 0;
    }

    /* Clear music state */
    g_MusicStream  = NULL;   /* DAT_004806f8 */
    g_MusicChannel = 0;      /* DAT_004806fc */

    float ver = FSOUND_GetVersion();
    LOG("[SND] FMOD version: %.2f\n", ver);
    if (ver != 3.5f) {
        LOG("[SND] FMOD version mismatch (expected 3.5, got %.2f)\n", ver);
        return 0;
    }

    /* DAT_00483720[3] holds output type: 0=WINMM, 1=DSOUND, 2=A3D, 3=NOSOUND */
    switch (DAT_00483720[3]) {
    case 0:  outputType = FSOUND_OUTPUT_WINMM;   break;
    case 1:  outputType = FSOUND_OUTPUT_DSOUND;   break;
    case 2:  outputType = FSOUND_OUTPUT_A3D;      break;
    case 3:  outputType = FSOUND_OUTPUT_NOSOUND;  break;
    default: goto skip_output;
    }
    FSOUND_SetOutput(outputType);
skip_output:
    FSOUND_SetDriver(0);
    FSOUND_SetMixer(FSOUND_MIXER_QUALITY_AUTODETECT);

    /* Max channels from DAT_00483724[0] (blob offset 0x17CC) */
    int maxChannels = DAT_00483724[0] & 0xFF;
    if (maxChannels == 0) maxChannels = 32;  /* safety fallback */

    if (FSOUND_Init(44100, maxChannels, FSOUND_INIT_GLOBALFOCUS)) {
        LOG("[SND] FMOD initialized OK (output=%d, channels=%d)\n",
            outputType, maxChannels);
        Load_Game_Sounds();
        return 1;
    }

    LOG("[SND] FMOD init failed (error=%d)\n", FSOUND_GetError());
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
/*
 * Loads all game sound samples into g_SoundTable (300 entries × 8 bytes = 0x960).
 * The original binary has 16 "complex" loads that call FSOUND_Sample_Load directly
 * with raw byte-offsets into the table, followed by ~55 "simple" loads via
 * Load_Sound_Sample.  Both produce the same result, so we use Load_Sound_Sample
 * for everything (converting byte-offsets to array indices and halving the volume
 * to compensate for Load_Sound_Sample's ×2 scaling).
 */
void Load_Game_Sounds(void)
{
    g_SoundTable = (SoundEntry *)Mem_Alloc(0x960);
    if (!g_SoundTable)
        return;
    memset(g_SoundTable, 0, 0x960);
    g_MemoryTracker += 0x960;

    /* --- Complex loads: 8 base weapon sounds (table offsets 0x690-0x6C8 = indices 0xD2-0xD9) --- */
    Load_Sound_Sample(0, 0xD2, 0x10, "bas1_r");
    Load_Sound_Sample(0, 0xD3, 0x10, "bas2_r");
    Load_Sound_Sample(0, 0xD4, 0x10, "bas3_r");
    Load_Sound_Sample(0, 0xD5, 0x10, "bas4_r");
    Load_Sound_Sample(0, 0xD6, 0x10, "bas5_r");
    Load_Sound_Sample(0, 0xD7, 0x10, "bas6_r");
    Load_Sound_Sample(0, 0xD8, 0x10, "bas7_r");
    Load_Sound_Sample(0, 0xD9, 0x10, "bas8_r");

    /* --- Complex loads: dump, organ, infiltrator (various offsets) --- */
    Load_Sound_Sample(0, 0x01,  0x40, "dump_r");     /* offset 0x008, vol 0x80 */
    Load_Sound_Sample(0, 0x104, 0x40, "dump_h");     /* offset 0x820, vol 0x80 */
    Load_Sound_Sample(1, 0x02,  0x10, "orga_l");     /* offset 0x010, vol 0x20, looping */
    Load_Sound_Sample(0, 0x105, 0x30, "inf1_r");     /* offset 0x828, vol 0x60 */
    Load_Sound_Sample(0, 0x106, 0x30, "inf2_r");     /* offset 0x830, vol 0x60 */
    Load_Sound_Sample(0, 0x107, 0x30, "inf3_r");     /* offset 0x838, vol 0x60 */
    Load_Sound_Sample(0, 0x108, 0x30, "inf4_r");     /* offset 0x840, vol 0x60 */
    Load_Sound_Sample(0, 0x109, 0x30, "inf5_r");     /* offset 0x848, vol 0x60 */

    /* --- Simple loads via Load_Sound_Sample (0x0040e8e0) --- */
    Load_Sound_Sample(0, 0x10a, 0x30, "inf6_r");
    Load_Sound_Sample(1, 0x04,  0x60, "turb_l");     /* looping */
    Load_Sound_Sample(0, 0x05,  0x40, "coll_r");
    Load_Sound_Sample(0, 0x06,  0x40, "cars_r");
    Load_Sound_Sample(0, 0x07,  0x40, "tele_r");
    Load_Sound_Sample(0, 0x08,  0x40, "tour_r");
    Load_Sound_Sample(0, 0x09,  0x40, "kick_r");
    Load_Sound_Sample(0, 0x0a,  0x40, "pers_h");
    Load_Sound_Sample(0, 0x0b,  0x18, "nucb_r");
    Load_Sound_Sample(1, 0x0c,  0x40, "digg_l");     /* looping */
    Load_Sound_Sample(0, 0x0d,  0x40, "repe_r");
    Load_Sound_Sample(0, 0x0e,  0x40, "repb_r");
    Load_Sound_Sample(0, 0x0f,  0x40, "bone_r");
    Load_Sound_Sample(1, 0x10,  0x40, "flam_l");     /* looping */
    Load_Sound_Sample(0, 0x11,  0x40, "firb_1");
    Load_Sound_Sample(0, 0x12,  0x40, "airs_r");
    Load_Sound_Sample(0, 0x13,  0x40, "free_r");
    Load_Sound_Sample(0, 0x10b, 0x40, "free_h");
    Load_Sound_Sample(0, 0x14,  0x40, "plas_r");
    Load_Sound_Sample(0, 0x10c, 0x40, "plas_h");
    Load_Sound_Sample(0, 0x15,  0x40, "fire_r");
    Load_Sound_Sample(0, 0x16,  0x40, "bric_r");
    Load_Sound_Sample(0, 0x10d, 0x40, "bric_h");
    Load_Sound_Sample(0, 0x17,  0x40, "nucl_r");
    Load_Sound_Sample(0, 0x18,  0x30, "pilo_r");
    Load_Sound_Sample(0, 0x10e, 0x30, "engi_r");
    Load_Sound_Sample(0, 0x10f, 0x40, "land_r");
    Load_Sound_Sample(0, 0x110, 0x40, "basi_r");
    Load_Sound_Sample(0, 0x111, 0x40, "expl_r");
    Load_Sound_Sample(0, 0x1a,  0x20, "mult_r");
    Load_Sound_Sample(0, 0x1b,  0x40, "kami_r");
    Load_Sound_Sample(0, 0x1c,  0x40, "mini_r");
    Load_Sound_Sample(0, 0x1d,  0x40, "mega_1");
    Load_Sound_Sample(0, 0x1e,  0x40, "phot_r");
    Load_Sound_Sample(0, 0x1f,  0x40, "inse_r");
    Load_Sound_Sample(0, 0x22,  0x40, "firw_r");
    Load_Sound_Sample(0, 0x112, 0x80, "firw_!");
    Load_Sound_Sample(0, 0x113, 0x40, "magi_r");
    Load_Sound_Sample(0, 0x114, 0x80, "magi_!");
    Load_Sound_Sample(0, 0x23,  0x40, "gamm_1");
    Load_Sound_Sample(0, 0x11c, 0x40, "gamm_2");
    Load_Sound_Sample(0, 0x25,  0x40, "roma_r");
    Load_Sound_Sample(0, 0x27,  0x40, "kome_r");
    Load_Sound_Sample(0, 0x28,  0x40, "pipe_r");
    Load_Sound_Sample(0, 0x29,  0x40, "turr_r");
    Load_Sound_Sample(0, 0x2a,  0x40, "turr_r");     /* same sound, different index */
    Load_Sound_Sample(0, 0x2b,  0x40, "repa_r");
    Load_Sound_Sample(1, 0x2c,  0x40, "mach_l");     /* looping */
    Load_Sound_Sample(0, 0x2d,  0x40, "lase_r");
    Load_Sound_Sample(0, 0x2e,  0x40, "nall_r");
    Load_Sound_Sample(0, 0x115, 0x40, "butt_m");
    Load_Sound_Sample(0, 0x116, 0x10, "hit1_m");
    Load_Sound_Sample(0, 0x117, 0x10, "hit2_m");
    Load_Sound_Sample(0, 0x118, 0x10, "hit3_m");
    Load_Sound_Sample(0, 0x65,  0x20, "exp1_m");
    Load_Sound_Sample(0, 0x66,  0x20, "exp2_m");
    Load_Sound_Sample(0, 0x67,  0x20, "exp3_m");
    Load_Sound_Sample(0, 0x68,  0x20, "exp4_m");
    Load_Sound_Sample(0, 0x69,  0x20, "exp5_m");
    Load_Sound_Sample(0, 0x6a,  0x20, "exp6_m");
    Load_Sound_Sample(0, 0x6b,  0x20, "exp7_m");
    Load_Sound_Sample(0, 0x119, 0x30, "wats_m");
    Load_Sound_Sample(0, 0x11a, 0x7f, "watd_m");
    Load_Sound_Sample(0, 0x70,  0x40, "flat_m");
    Load_Sound_Sample(0, 0x11b, 0x20, "rebo_m");
    Load_Sound_Sample(0, 0x71,  0x40, "scr1_m");
    Load_Sound_Sample(0, 0x72,  0x40, "scr2_m");
    Load_Sound_Sample(0, 0x73,  0x40, "scr3_m");
    Load_Sound_Sample(0, 0x74,  0x40, "scr4_m");
    Load_Sound_Sample(0, 0x78,  0x40, "shot_r");
    Load_Sound_Sample(0, 0x79,  0x20, "spec_m");
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
