/* fmod_imp.s - 32-bit import thunks for fmod.dll
 * Manually provides the _imp__ pointers that __declspec(dllimport) expects.
 * At runtime, the dynamic loader patches these with real fmod.dll addresses.
 * Assembled with: gcc -m32 -c fmod_imp.s -o fmod_imp.o
 */
    .section .idata$7
    .global _fmod_dll_iname
_fmod_dll_iname:
    .asciz "fmod.dll"

    .section .data

/* Helper: each _imp__ symbol is a function pointer, initially pointing to a
   no-op stub. The PE loader overwrites these when fmod.dll is loaded. */

    .section .text
    .balign 4
_fmod_stub_void:
    ret
_fmod_stub_void4:
    ret $4
_fmod_stub_void8:
    ret $8
_fmod_stub_void12:
    ret $12
_fmod_stub_void16:
    ret $16
_fmod_stub_int0:
    xorl %eax,%eax
    ret
_fmod_stub_int4:
    xorl %eax,%eax
    ret $4
_fmod_stub_int8:
    xorl %eax,%eax
    ret $8
_fmod_stub_int12:
    xorl %eax,%eax
    ret $12
_fmod_stub_int16:
    xorl %eax,%eax
    ret $16
_fmod_stub_float0:
    fldz
    ret

    .section .data
    .balign 4

    .global __imp__FSOUND_GetVersion@0
__imp__FSOUND_GetVersion@0:
    .long _fmod_stub_float0

    .global __imp__FSOUND_SetOutput@4
__imp__FSOUND_SetOutput@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_SetDriver@4
__imp__FSOUND_SetDriver@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_SetMixer@4
__imp__FSOUND_SetMixer@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_Init@12
__imp__FSOUND_Init@12:
    .long _fmod_stub_int12

    .global __imp__FSOUND_GetError@0
__imp__FSOUND_GetError@0:
    .long _fmod_stub_int0

    .global __imp__FSOUND_Close@0
__imp__FSOUND_Close@0:
    .long _fmod_stub_void

    .global __imp__FSOUND_Sample_Load@16
__imp__FSOUND_Sample_Load@16:
    .long _fmod_stub_int16

    .global __imp__FSOUND_StopSound@4
__imp__FSOUND_StopSound@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_GetMaxChannels@0
__imp__FSOUND_GetMaxChannels@0:
    .long _fmod_stub_int0

    .global __imp__FSOUND_Stream_OpenFile@12
__imp__FSOUND_Stream_OpenFile@12:
    .long _fmod_stub_int12

    .global __imp__FSOUND_Stream_Play@8
__imp__FSOUND_Stream_Play@8:
    .long _fmod_stub_int8

    .global __imp__FSOUND_Stream_Stop@4
__imp__FSOUND_Stream_Stop@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_Stream_Close@4
__imp__FSOUND_Stream_Close@4:
    .long _fmod_stub_void4

    .global __imp__FSOUND_PlaySoundEx@16
__imp__FSOUND_PlaySoundEx@16:
    .long _fmod_stub_int16

    .global __imp__FSOUND_SetVolume@8
__imp__FSOUND_SetVolume@8:
    .long _fmod_stub_int8

    .global __imp__FSOUND_SetPaused@8
__imp__FSOUND_SetPaused@8:
    .long _fmod_stub_int8

    .global __imp__FSOUND_SetPan@8
__imp__FSOUND_SetPan@8:
    .long _fmod_stub_int8

    .global __imp__FSOUND_SetVolumeAbsolute@8
__imp__FSOUND_SetVolumeAbsolute@8:
    .long _fmod_stub_int8

    .global __imp__FSOUND_SetSFXMasterVolume@4
__imp__FSOUND_SetSFXMasterVolume@4:
    .long _fmod_stub_void4

    .global __imp__FMUSIC_LoadSong@4
__imp__FMUSIC_LoadSong@4:
    .long _fmod_stub_int4

    .global __imp__FMUSIC_PlaySong@4
__imp__FMUSIC_PlaySong@4:
    .long _fmod_stub_int4

    .global __imp__FMUSIC_StopSong@4
__imp__FMUSIC_StopSong@4:
    .long _fmod_stub_int4

    .global __imp__FMUSIC_FreeSong@4
__imp__FMUSIC_FreeSong@4:
    .long _fmod_stub_void4
