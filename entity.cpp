/*
 * entity.cpp - Entity behavior loop and player movement sub-functions
 * Addresses: FUN_0044b0b0 (Entity_Behavior_Loop), FUN_0044bb70, FUN_0044b990,
 *            FUN_0044bbb0, FUN_0044bf20, FUN_0044bfa0, FUN_0044e1c0,
 *            FUN_0044e510, FUN_00450630
 */
#include "tou.h"
#include <dinput.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ===== Globals defined in this module ===== */
int  DAT_00486fa8[16] = {0};   /* per-player distance traveled */
int  DAT_00486be8[16] = {0};   /* per-player damage received stats */
int  DAT_00486e68[16] = {0};   /* per-player friendly fire stats */
char DAT_00483747 = '\0';      /* weapon auto-release mode flag */
char DAT_00483745 = '\0';      /* detonation mode flag */

/* Positional sound globals */
char  DAT_0048371f = 1;        /* sound effects enabled flag */
int   DAT_00487840 = 0;        /* indexed entity count for proximity scan */
int   DAT_0048382c = 0;        /* game scaling constant 3 (fire rate scale) */

/* Float constants for positional audio (from binary at 0x4753xx) */
static float _DAT_004753fc = 6.6666667e-05f; /* 1.0f / 15000.0f */
static float _DAT_004753e8 = 1.0f;           /* min distance clamp */
static double _DAT_004753e0_d = 0.4;         /* very close threshold (double in original) */

/* Terrain/collision globals */
void *DAT_004876b8 = 0;         /* color degradation palette LUT */
unsigned short DAT_00481e8c = 0; /* tile explosion color accumulator */
unsigned short DAT_00481e8e = 0; /* tile explosion count accumulator */
void *DAT_00487880 = 0;         /* sprite bounding data table */
char DAT_0048373b = '\0';       /* shared lives mode flag */
char DAT_00483744 = '\0';       /* respawn delay mode */

/* Wall particle stub — empty until wall segment system is implemented */
void FUN_0044f630(int x, int y) { (void)x; (void)y; }

/* ===== FUN_0040fd70 — Positional Sound with Entity Dedup (0040FD70) ===== */
/* Manages per-entity sound state: if same sound is already playing, reset timer.
 * If different sound, stop old one first. Uses spatial audio like FUN_0040f9b0.
 * param_1: entity index (as float, cast to int), param_2: sound ID,
 * param_3: world x, param_4: world y */
void FUN_0040fd70(float param_1, int param_2, int param_3, int param_4)
{
    int iVar6 = (int)param_1 * 0x598;

    /* If same sound already assigned to this entity, just refresh timer */
    if (param_2 == *(int *)(iVar6 + 0x4b0 + (int)DAT_00487810)) {
        if (0 < *(int *)(iVar6 + 0x4a8 + (int)DAT_00487810)) {
            *(int *)(iVar6 + 0x4a8 + (int)DAT_00487810) = 6;
        }
    }
    else {
        /* Different sound — stop old channel and reset timer */
        FSOUND_StopSound(*(int *)(iVar6 + 0x4ac + (int)DAT_00487810));
        *(int *)(iVar6 + 0x4a8 + (int)DAT_00487810) = 0;
    }

    /* If timer is already active, don't re-trigger */
    if (*(int *)(iVar6 + 0x4a8 + (int)DAT_00487810) != 0) return;

    /* Check sound enabled and sample exists */
    if (DAT_0048371f == '\0' || g_SoundEnabled == 0) return;
    if (*(int *)((int)g_SoundTable + param_2 * 8) == 0) return;

    /* Set timer and record which sound */
    *(int *)(iVar6 + 0x4a8 + (int)DAT_00487810) = 6;
    *(int *)(iVar6 + 0x4b0 + (int)DAT_00487810) = param_2;

    float minDist = 10000.0f;
    int iVar8 = 0xff;

    /* Find nearest listener */
    if (0 < DAT_00487808) {
        int *pIdx = (int *)&DAT_004877f8;
        int count = DAT_00487808;
        do {
            int *piVar1 = (int *)((int)DAT_00487810 + (*pIdx) * 0x598);
            int iVar5 = (piVar1[1] - param_4) >> 0x12;
            int iVar3 = (piVar1[0] - param_3) >> 0x12;
            float fVar2 = (float)(iVar5 * iVar5 + iVar3 * iVar3) * _DAT_004753fc;
            if (fVar2 < _DAT_004753e8) fVar2 = _DAT_004753e8;
            if (fVar2 < minDist) {
                iVar8 = *pIdx;
                minDist = fVar2;
            }
            pIdx++;
            count--;
        } while (count != 0);

        if (iVar8 != 0xff) {
            /* Volume from inverse distance */
            int vol = (int)(1.0f / minDist) * 2;

            /* Pan from angle to nearest listener */
            int angle = FUN_004257e0(
                *(int *)((int)DAT_00487810 + iVar8 * 0x598),
                *(int *)((int)DAT_00487810 + iVar8 * 0x598 + 4),
                param_3, param_4);
            int pan = (*(int *)((int)DAT_00487ab0 + angle * 4) >> 0xc) + 0x80;
            if (minDist < (float)_DAT_004753e0_d) pan = 0x80;

            /* Clamp volume */
            if (vol >= 0x100) vol = 0xff;
            if (vol < 5) return;

            /* Play with spatial properties */
            int chan = FSOUND_PlaySoundEx(-1,
                (FSOUND_SAMPLE *)(*(int *)((int)g_SoundTable + param_2 * 8)), NULL, 1);
            FSOUND_SetVolume(chan, vol);
            FSOUND_SetPan(chan, pan);
            FSOUND_SetPaused(chan, 0);

            /* Store channel handle for later management */
            *(int *)(iVar6 + 0x4ac + (int)DAT_00487810) = chan;
        }
    }
}

/* ===== FUN_00450dd0 — Sprite-vs-Tile Collision Check (00450DD0) ===== */
/* Checks if sprite mask 0x19B overlaps any impassable tiles.
 * Returns 1 if collision detected, 0 if clear. */
int FUN_00450dd0(int x, int y)
{
    unsigned int spr_w = *(unsigned char *)((int)DAT_00489e8c + 0x19b);
    int frame_off = *(int *)((int)DAT_00489234 + 0x66c);
    unsigned int spr_h = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x19b);
    int sx = (x >> 0x12) - (int)(spr_w >> 1);
    int sy = (y >> 0x12) - (int)(spr_h >> 1);
    unsigned int idx = (sy << ((unsigned char)DAT_00487a18 & 0x1f)) + sx;

    /* Bounds check: sprite must be well within map margins */
    if (sx <= 6 || sy <= 6 ||
        (int)(spr_w + sx) >= DAT_004879f0 - 7 ||
        (int)(spr_h + sy) >= DAT_004879f4 - 7)
    {
        return 1;  /* out of bounds = collision */
    }

    int row = 0;
    if (spr_h != 0) {
        int tsy = sy;
        do {
            int col = 0;
            int tx = sx;
            if (spr_w != 0) {
                do {
                    if ((*(char *)((int)DAT_00489e94 + frame_off) != '\0') &&
                        (0 < tx) && (tx < DAT_004879f0) &&
                        (0 < tsy) && (tsy < DAT_004879f4) &&
                        (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + idx) *
                            0x20 + 1 + (int)DAT_00487928) == '\0'))
                    {
                        return 1;  /* impassable tile hit */
                    }
                    frame_off++;
                    idx++;
                    col++;
                    tx++;
                } while (col < (int)spr_w);
            }
            idx += DAT_00487a00 - spr_w;
            row++;
            tsy++;
        } while (row < (int)spr_h);
    }
    return 0;  /* no collision */
}

/* ===== Sub-function stubs (Tier 4+, implement later) ===== */
static void FUN_0044ad30(int *ent, int idx)   { (void)ent; (void)idx; }  /* AI behavior — needs 10 sub-functions */
static void FUN_0044c9a0(int *ent)            { (void)ent; }  /* ftol — handled inline by caller */
static void FUN_00401000_impl(int idx);  /* forward declaration — defined below */

/* ===== FUN_0044bd50 — Boundary Clamping (0044BD50) ===== */
static void FUN_0044bd50(int *ent)
{
    unsigned int buttons = (unsigned int)ent[0x2e]; /* +0xB8: button flags */
    int iVar2;

    if ((buttons & 1) != 0) {
        *(char *)(ent + 0x11f) = 0;
        ent[0] = ent[0] - 0x100000;
    }
    if ((buttons & 2) != 0) {
        *(char *)(ent + 0x11f) = 0;
        ent[0] = ent[0] + 0x100000;
    }
    if ((buttons & 4) != 0) {
        *(char *)(ent + 0x11f) = 0;
        ent[1] = ent[1] - 0x100000;
    }
    if ((buttons & 0x40) != 0) {
        *(char *)(ent + 0x11f) = 0;
        ent[1] = ent[1] + 0x100000;
    }

    if (*(char *)(ent + 0x11f) == '\0') {
        iVar2 = (ent[0x121] / 2) * 0x40000;
        if (ent[0] < iVar2) ent[0] = iVar2;
        iVar2 = (ent[0x122] / 2) * 0x40000;
        if (ent[1] < iVar2) ent[1] = iVar2;
        iVar2 = (DAT_004879f0 - ent[0x121] / 2) * 0x40000;
        if (iVar2 < ent[0]) ent[0] = iVar2;
        iVar2 = (DAT_004879f4 - ent[0x122] / 2) * 0x40000;
        if (iVar2 < ent[1]) ent[1] = iVar2;
    }
}

/* ===== FUN_0044be20 — Enemy Proximity Scanner (0044BE20) ===== */
static void FUN_0044be20(int *ent)
{
    int *piVar1;
    int bVar2 = 0, bVar3 = 0;
    unsigned int i = 0;
    int iVar4;

    if (DAT_00487840 != 0) {
        iVar4 = 0xc000;
        do {
            piVar1 = (int *)(*(int *)(iVar4 + (int)DAT_0048781c) * 0x80 + (int)DAT_004892e8);
            if ((*(char *)((int)DAT_00487810 + 0x2c +
                 (unsigned int)*(unsigned char *)((int)piVar1 + 0x22) * 0x598) !=
                 (char)ent[0xb]) &&
                (*(char *)((int)piVar1 + 0x21) == '\x18'))
            {
                if ((ent[0] - 0x1180000 < piVar1[0]) && (piVar1[0] < ent[0] + 0x1180000)) {
                    if ((ent[1] - 0x1180000 < piVar1[2]) && (piVar1[2] < ent[1] + 0x1180000)) {
                        if (((char)piVar1[0x10] != '\0') || bVar2) {
                            if (!bVar3) { ent[0x35] = 0x20; bVar3 = 1; }
                        } else {
                            ent[0x36] = 0x20; bVar2 = 1;
                        }
                    }
                }
            }
            i++;
            iVar4 += 4;
        } while (i < (unsigned int)DAT_00487840);
    }
}
/* ===== FUN_0044ca40 — Fire_Primary (0044CA40) ===== */
/* Creates a projectile entity in DAT_004892e8 at player's heading.
 * Sets fire cooldown, subtracts energy, spawns 1-3 projectile entities
 * depending on weapon type. Plays fire sound. */
static void FUN_0044ca40_impl(int *ent, int idx)
{
    unsigned int *uent = (unsigned int *)ent;
    int heading = ent[6];  /* +0x18 */
    unsigned char weapon_type = *(unsigned char *)(ent + 0x23);  /* +0x8C: weapon type byte */
    int *lut = (int *)DAT_00487ab0;

    /* Set "has fired" flag */
    *(unsigned char *)((int)ent + 0x1D) = 1;

    /* Set fire cooldown */
    if (DAT_004892e5 == '\x01') {
        ent[0x24] = 0xA0;  /* +0x90: long cooldown in difficulty mode */
    } else {
        ent[0x24] = 5;     /* +0x90: normal cooldown */
    }

    /* Subtract energy cost */
    int new_energy = ent[0x26] - 0x70;  /* +0x98: energy */
    int min_energy = (int)((unsigned int)DAT_00483830 + ((unsigned int)DAT_00483830 >> 31 & 0x7F)) >> 7;
    if (new_energy < min_energy) {
        /* Low energy: increase fire cooldowns */
        if ((unsigned int)ent[0x25] < 0x20) ent[0x25] = 0x20;
        if ((unsigned int)ent[0x24] < 0x20) ent[0x24] = 0x20;
        if (new_energy < 0) new_energy = 0;
    }
    ent[0x26] = new_energy;

    /* Determine projectile speed from entity type table */
    int speed = *(int *)((int)DAT_00487abc + 0xAC);
    if (weapon_type > 8) speed = 800;
    if (DAT_004892e5 != '\0') speed = 0xFA;

    /* Spawn primary projectile (straight ahead) */
    if ((*(char *)((int)ent + 0x8D) == '\0' || *(char *)((int)ent + 0x8D) == '\x02') &&
        DAT_00489248 < 0xA28) {
        int eoff = DAT_00489248 * 0x80;
        int ebase = (int)DAT_004892e8 + eoff;

        /* Position: player + 1 unit in heading direction */
        *(int *)(ebase + 0x00) = lut[heading] + ent[0];
        *(int *)(ebase + 0x08) = lut[(heading + 0x200) & 0x7FF] + ent[1];

        /* Velocity: heading * speed + player velocity */
        *(int *)(ebase + 0x18) = (lut[heading] * speed >> 6) + ent[4];
        *(int *)(ebase + 0x1C) = (lut[(heading + 0x200) & 0x7FF] * speed >> 6) + ent[5];

        /* Copy position to prev_position */
        *(int *)(ebase + 0x04) = *(int *)(ebase + 0x00);
        *(int *)(ebase + 0x0C) = *(int *)(ebase + 0x08);

        /* Entity metadata */
        *(unsigned short *)(ebase + 0x24) = 0;      /* state */
        *(unsigned char *)(ebase + 0x20) = 0;        /* flags */
        *(unsigned char *)(ebase + 0x26) = 6;        /* lifetime/color */
        *(unsigned char *)(ebase + 0x22) = (unsigned char)idx;  /* owner */
        *(int *)(ebase + 0x28) = 0;                  /* health */

        /* Sprite/type data from entity type table */
        if (weapon_type < 6) {
            *(unsigned char *)(ebase + 0x21) = 0;
            *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + (unsigned int)weapon_type * 4);
            *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xC4 + (unsigned int)weapon_type * 4);
            *(int *)(ebase + 0x4C) = *(int *)((int)DAT_00487abc + 0xF4 + (unsigned int)weapon_type * 4);
            *(char *)(ebase + 0x40) = (char)weapon_type;
        } else {
            *(unsigned char *)(ebase + 0x21) = 0x69;
            *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0xDC48 + (unsigned int)weapon_type * 4);
            *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xDC84 + (unsigned int)weapon_type * 4);
            *(int *)(ebase + 0x4C) = *(int *)((int)DAT_00487abc + 0xDCB4 + (unsigned int)weapon_type * 4);
            *(char *)(ebase + 0x40) = (char)weapon_type - 6;
        }
        *(int *)(ebase + 0x34) = 0;  /* behavior callback (not implemented) */
        *(int *)(ebase + 0x48) = 0;
        *(unsigned char *)(ebase + 0x54) = 0;
        *(int *)(ebase + 0x3C) = 0;

        DAT_00489248++;

        /* Set origin position in next entity slot header */
        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x70) = ent[0];
        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x6C) = ent[1];
    }

    /* Play fire sound */
    unsigned int snd_type = (unsigned int)weapon_type;
    if (weapon_type > 7) snd_type = 7;
    FUN_0040f9b0(snd_type + 0xD2, ent[0], ent[1]);
}
/* ===== FUN_0044d650 — Detonate Owned Projectiles (0044D650) ===== */
static void FUN_0044d650_impl(int *ent, int idx)
{
    if (((*(unsigned char *)((int)ent + 0xb8) & 0x20) == 0) ||
        (*(char *)((int)ent + 0xa2) != '\0'))
        return;

    int base = (int)DAT_004892e8;
    int count = DAT_00489248;
    int off, i;
    char c;

    /* Type 1: 0x0B (slot +0x470) -> 0xFA */
    if ((*(int *)((int)ent + 0x470) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            if ((*(char *)(off + 0x40 + base) == '\x01') &&
                (*(char *)(off + 0x21 + base) == '\x0b') &&
                (*(int *)(off + 0x2c + base) == 1) &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfa;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
    /* Type 2: 0x2E (slot +0x46C) -> 0xFB */
    if ((*(int *)((int)ent + 0x46c) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            if ((*(char *)(off + 0x21 + base) == 0x2e) &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfb;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
    /* Type 3: 0x27 (slot +0x468) -> 0xFB */
    if ((*(int *)((int)ent + 0x468) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            if ((*(char *)(off + 0x21 + base) == 0x27) &&
                (*(int *)(off + 0x2c + base) == 1) &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfb;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
    /* Type 4: 0x22 (slot +0x464) -> 0xFA */
    if ((*(int *)((int)ent + 0x464) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            if ((*(char *)(off + 0x21 + base) == 0x22) &&
                (*(char *)(off + 0x40 + base) != '\0') &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfa;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
    /* Type 5: 0x28 (slot +0x474) -> 0xFA */
    if ((*(int *)((int)ent + 0x474) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            if ((*(char *)(off + 0x21 + base) == 0x28) &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfa;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
    /* Type 6: 0x29/0x2A (slot +0x478) -> 0xFA */
    if ((*(int *)((int)ent + 0x478) != 0) && (0 < count)) {
        for (i = 0, off = 0; i < count; i++, off += 0x80) {
            c = *(char *)(off + 0x21 + base);
            if ((c == 0x29 || c == 0x2a) &&
                (*(unsigned char *)(off + 0x22 + base) == (unsigned char)idx)) {
                *(unsigned char *)(off + 0x20 + base) = 0xfa;
                base = (int)DAT_004892e8; count = DAT_00489248;
            }
        }
    }
}

/* ===== FUN_0044d860 — Fire Secondary Weapon (0044D860) ===== */
static void FUN_0044d860_impl(int *ent, int idx)
{
    unsigned char *pbVar1 = (unsigned char *)((int)ent +
        *(char *)((int)ent + 0x34) + 0x3c);
    unsigned int weapIdx = *(unsigned char *)((int)ent + 0x35) +
                           (unsigned int)(*pbVar1) * 0x86;

    unsigned int cooldown = (unsigned int)(*(int *)((int)DAT_00487abc + 0xdc + weapIdx * 4)) *
        (unsigned int)*(unsigned char *)((int)ent + 0x9c) *
        (unsigned int)DAT_0048382c >> 0xc;
    *(unsigned int *)((int)ent + 0x94) = cooldown;

    int newEnergy = *(int *)((int)ent + 0x98) +
        *(int *)((int)DAT_00487abc + 0x10c + weapIdx * 4) * -7;

    int minEnergy = (int)((unsigned int)DAT_00483830 +
        ((unsigned int)DAT_00483830 >> 0x1f & 0x7fU)) >> 7;
    if (newEnergy < minEnergy) {
        if (cooldown < 0x20) *(int *)((int)ent + 0x94) = 0x20;
        if (*(unsigned int *)((int)ent + 0x90) < 0x20)
            *(int *)((int)ent + 0x90) = 0x20;
        if (newEnergy < 0) newEnergy = 0;
    }
    *(int *)((int)ent + 0x98) = newEnergy;

    FUN_00401000_impl(idx);
}

/* ===== FUN_0044e950 — Overheat Penalty/Teleport (0044E950) ===== */
static void FUN_0044e950_impl(int *ent, int idx)
{
    int attempts = 0;
    int iVar1, iVar2;

    for (;;) {
        iVar1 = rand() % 0x46 - 0x23 + (ent[0] >> 0x12);
        iVar2 = rand() % 0x46 - 0x23 + (ent[1] >> 0x12);

        if ((0xd < iVar1) && (iVar1 < (int)DAT_004879f0 - 0xe) &&
            (0xd < iVar2) && (iVar2 < (int)DAT_004879f4 - 0xe))
        {
            int clear = 1;
            int tidx = ((iVar2 - 8) << ((unsigned char)DAT_00487a18 & 0x1f)) - 8 + iVar1;
            int row, col;
            for (row = 0; row < 16 && clear; row++) {
                for (col = 0; col < 16; col++) {
                    if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + tidx) *
                        0x20 + 1 + (int)DAT_00487928) == '\0') {
                        clear = 0; break;
                    }
                    tidx++;
                }
                if (clear) tidx += DAT_00487a00 - 16;
            }
            if (clear) {
                ent[8] -= 0x1f4000;
                DAT_00486be8[idx] += 0x30d;
                ent[4] = 0;
                ent[5] = 0;
                *(unsigned char *)((int)ent + 0x4a0) = 0xff;
                ent[0] = iVar1 * 0x40000;
                ent[1] = iVar2 * 0x40000;
                ent[2] = iVar1 * 0x40000;
                ent[3] = iVar2 * 0x40000;
                return;
            }
        }
        attempts++;
        if (attempts > 0xe) return;
    }
}

/* ===== FUN_0044e5a0 — Hazard Damage Particles (0044E5A0) ===== */
static void FUN_0044e5a0_impl(int *ent)
{
    int r = rand();
    int divisor = 3;  /* original reads from FPU; approximate */

    if ((r % divisor != 0) || (DAT_0048925c >= 0x5dc)) return;

    unsigned int angle = rand() & 0x1ff;

    if (DAT_0048925c >= 0x5dc) return;

    int base = DAT_0048925c * 0x20 + (int)DAT_00481f2c;

    *(int *)(base + 0x00) = (rand() % 10 - 5) * 0x40000 + ent[0];
    *(int *)(base + 0x04) = (rand() % 10 - 5) * 0x40000 + ent[1];

    int speed = rand() % 0x1e + 10;
    *(int *)(base + 0x08) = speed * *(int *)((int)DAT_00487ab0 + (angle + 0x300) * 4) >> 6;
    *(int *)(base + 0x0c) = speed * *(int *)((int)DAT_00487ab0 + 0x800 + (angle + 0x300) * 4) >> 6;

    unsigned int ptype = rand() & 3;
    *(char *)(base + 0x10) = (char)ptype;
    *(unsigned char *)(base + 0x11) = 0;
    *(unsigned short *)(base + 0x12) = 0;
    *(unsigned char *)(base + 0x14) = 0xff;
    *(unsigned char *)(base + 0x15) = 0;

    DAT_0048925c++;

    unsigned int flag = rand() & 3;
    if (flag == 0) {
        *(unsigned char *)(DAT_0048925c * 0x20 + (int)DAT_00481f2c - 0xb) = 1;
    }
}

/* ===== FUN_0044ed90 — Explosion Damage + Knockback (0044ED90) ===== */
/* param_1=entity ptr, param_2=player index (float in original, we use int),
   param_3=tile type at impact point */
static void FUN_0044ed90_impl(int *ent, int idx, unsigned int tile_type)
{
    int *piVar1 = ent;

    /* Play explosion sound */
    FUN_0040fd70((float)idx, 0xc, ent[0], ent[1]);

    /* Knockback all nearby entities */
    int playerIdx = 0;
    if (DAT_00489240 > 0) {
        int iVar4 = 0;
        for (int p = 0; p < DAT_00489240; p++) {
            if (p != idx && *(int *)(iVar4 + 0xa8 + (int)DAT_00487810) == 0) {
                if (*(int *)(iVar4 + (int)DAT_00487810) - 0x400000 < piVar1[0] &&
                    piVar1[0] < *(int *)(iVar4 + (int)DAT_00487810) + 0x400000)
                {
                    int iVar5 = *(int *)(iVar4 + 4 + (int)DAT_00487810);
                    if (iVar5 - 0x400000 < piVar1[1] && piVar1[1] < iVar5 + 0x400000) {
                        *(int *)(iVar4 + 0xa8 + (int)DAT_00487810) = 10;
                        unsigned long long uVar7 = FUN_004257e0(piVar1[0], piVar1[1],
                            *(int *)(iVar4 + (int)DAT_00487810),
                            *(int *)(iVar4 + 4 + (int)DAT_00487810));
                        unsigned int uVar2 = ((int)uVar7 + 0x200) & 0x7ff;
                        *(int *)(iVar4 + 0x10 + (int)DAT_00487810) =
                            *(int *)((int)DAT_00487ab0 + uVar2 * 4) >> 1;
                        *(int *)(iVar4 + 0x14 + (int)DAT_00487810) =
                            *(int *)((int)DAT_00487ab0 + 0x800 + uVar2 * 4) >> 1;
                    }
                }
            }
            iVar4 += 0x598;
        }
    }

    /* Check for destructible terrain below — spawn debris if present */
    if (((*(unsigned char *)((piVar1[0] >> 0x16) + (int)DAT_00487814 +
         (piVar1[1] >> 0x16) * DAT_004879f8) & 8) != 0) &&
        (*(char *)((unsigned int)*(unsigned char *)((piVar1[0] >> 0x12) + 7 +
            (int)DAT_0048782c + ((piVar1[1] >> 0x12) + 4 <<
            ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20 + 4 + (int)DAT_00487928) == '\x01') &&
        (DAT_00489248 < 0x9c4))
    {
        int r = rand();
        int iVar5 = r % 0x280 + 0x200;
        int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

        *(int *)(base + 0x00) = piVar1[0] + 0x1c0000;
        *(int *)(base + 0x08) = piVar1[1] + 0x100000;
        *(int *)(base + 0x18) = (rand() % 0x14 + 0x1e) *
            (*(int *)((int)DAT_00487ab0 + iVar5 * 4)) >> 6;
        *(int *)(base + 0x1c) = (rand() % 0x14 + 0x1e) *
            (*(int *)((int)DAT_00487ab0 + 0x800 + iVar5 * 4)) >> 6;
        *(int *)(base + 0x04) = piVar1[0] + 0x1c0000;
        *(int *)(base + 0x0c) = piVar1[1] + 0x100000;
        *(int *)(base + 0x10) = 0;
        *(int *)(base + 0x14) = 0;
        *(unsigned char *)(base + 0x21) = 100;
        *(short *)(base + 0x24) = (short)(rand() % 6);
        *(unsigned char *)(base + 0x20) = 0;
        *(unsigned char *)(base + 0x26) = 0xff;
        *(unsigned char *)(base + 0x22) = 0xff;
        *(int *)(base + 0x28) = 0;
        *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xd1e8);
        *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xd224);
        *(int *)(base + 0x48) = 0;
        *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xd254);
        *(unsigned char *)(base + 0x54) = 0;
        *(unsigned char *)(base + 0x40) = 0;
        *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xd160);
        *(int *)(base + 0x3c) = 0;
        *(unsigned char *)(base + 0x5c) = 0;

        DAT_00489248++;
        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = rand() % 0x3c + 0x46;
        *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
            (DAT_0048384c & 0xffff) + 30000;
    }

    /* Area-of-effect tile damage via FUN_004357b0 */
    if (((*(unsigned char *)(piVar1 + 0x29) & 7) == 0) ||
        (*(char *)((tile_type & 0xffff) * 0x20 + 6 + (int)DAT_00487928) == '\x01'))
    {
        DAT_00481e8c = 0;
        DAT_00481e8e = 0;

        unsigned char tileAtPrev = *(unsigned char *)((piVar1[2] >> 0x12) +
            (int)DAT_0048782c + ((piVar1[3] >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f)));
        char passable = *(char *)((unsigned int)tileAtPrev * 0x20 + 4 + (int)DAT_00487928);
        if (passable == '\0') tileAtPrev = 0;

        int iVar4 = piVar1[0] >> 0x12;
        int iVar5 = piVar1[1] >> 0x12;
        if (iVar4 > 0 && iVar5 > 0 && iVar4 < (int)DAT_004879f0 && iVar5 < (int)DAT_004879f4) {
            FUN_004357b0(iVar4, iVar5, 9, (int)(unsigned char)tileAtPrev,
                *(char *)((iVar5 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar4 +
                    (int)DAT_0048782c) == '\x0c',
                0, 0, 0, 0, '\x03', '\0', (unsigned char)idx);
        }

        /* Spawn scattered debris from destroyed tiles */
        unsigned int destroyedCount = (unsigned int)DAT_00481e8e >> 2;
        if (destroyedCount != 0 &&
            ((*(unsigned char *)((piVar1[0] >> 0x16) + (int)DAT_00487814 +
                (piVar1[1] >> 0x16) * DAT_004879f8) & 8) != 0))
        {
            int angleStep = 0x800 / (int)destroyedCount;
            int angle = 0;
            while (angle < 0x800) {
                if (DAT_00489248 > 0x9c3) return;

                unsigned int randAngle = (unsigned int)rand() & 0x7ff;
                int signedRand = (int)randAngle;
                if (signedRand < 0) signedRand = (signedRand - 1 | (int)0xfffff800) + 1;
                unsigned int uVar6 = (((signedRand + (signedRand >> 0x1f & 3)) >> 2) + angle) & 0x7ff;
                int iVar3 = rand() % 0x28 + 10;

                int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

                int rx = rand() & 7; if (rx < 0) rx = (rx - 1 | (int)0xfffffff8) + 1;
                *(unsigned int *)(base + 0x00) = ((unsigned int)(rx - 4) + (unsigned int)iVar4) * 0x40000;
                int ry = rand() & 7; if (ry < 0) ry = (ry - 1 | (int)0xfffffff8) + 1;
                *(unsigned int *)(base + 0x08) = ((unsigned int)(ry - 4) + (unsigned int)iVar5) * 0x40000;
                *(int *)(base + 0x18) = *(int *)((int)DAT_00487ab0 + uVar6 * 4) * iVar3 +
                    (piVar1[4] >> 2) >> 6;
                *(int *)(base + 0x1c) = *(int *)((int)DAT_00487ab0 + 0x800 + uVar6 * 4) * iVar3 +
                    (piVar1[4] >> 2) >> 6;

                int rx2 = rand() & 7; if (rx2 < 0) rx2 = (rx2 - 1 | (int)0xfffffff8) + 1;
                *(unsigned int *)(base + 0x04) = ((unsigned int)(rx2 - 4) + (unsigned int)iVar4) * 0x40000;
                int ry2 = rand() & 7; if (ry2 < 0) ry2 = (ry2 - 1 | (int)0xfffffff8) + 1;
                *(unsigned int *)(base + 0x0c) = ((unsigned int)(ry2 - 4) + (unsigned int)iVar5) * 0x40000;
                *(int *)(base + 0x10) = 0;
                *(int *)(base + 0x14) = 0;
                *(unsigned char *)(base + 0x21) = 100;
                *(short *)(base + 0x24) = (short)(rand() % 6);
                *(unsigned char *)(base + 0x20) = 0;
                *(unsigned char *)(base + 0x26) = 0xff;
                *(unsigned char *)(base + 0x22) = 0xff;
                *(int *)(base + 0x28) = 0;
                *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xd1e8);
                *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xd224);
                *(int *)(base + 0x48) = 0;
                *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xd254);
                *(unsigned char *)(base + 0x54) = 0;
                *(unsigned char *)(base + 0x40) = 0;
                *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xd160);
                *(int *)(base + 0x3c) = 0;
                *(unsigned char *)(base + 0x5c) = 0;

                DAT_00489248++;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = rand() % 0x5a + 0x5a;
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    (unsigned int)DAT_00481e8c + 30000;

                angle += angleStep;
            }
        }
    }
}

/* ===== FUN_0044f840 — Wall Bounce (0044F840) ===== */
static void FUN_0044f840_impl(int *ent)
{
    unsigned int absVelY = (unsigned int)(ent[5] ^ (ent[5] >> 0x1f)) - (unsigned int)(ent[5] >> 0x1f);
    unsigned int absVelX = (unsigned int)(ent[4] ^ (ent[4] >> 0x1f)) - (unsigned int)(ent[4] >> 0x1f);
    int totalSpeed = (int)(absVelY + absVelX);

    int snd = (totalSpeed < 900000) ? 0x119 : 0x11a;
    FUN_0040f9b0(snd, ent[0], ent[1]);

    ent[5] = ent[5] - ent[5] / 3;
    *(unsigned char *)((int)ent + 0x49e) = 10;
    ent[4] = ent[4] - ent[4] / 3;

    if (DAT_00483835 != '\0') {
        FUN_0044f630(ent[0], ent[1]);
    }
}

/* ===== FUN_0044f900 — Weapon Selection (0044F900) ===== */
/* Tier 4 — extremely complex, keep as stub */
static void FUN_0044f900_impl(int *ent, int idx) { (void)ent; (void)idx; }

/* ===== FUN_00450080 — Tile Interaction / Terrain Destruction (00450080) ===== */
static void FUN_00450080_impl(int *ent, char param_2)
{
    int iVar14 = ent[0] >> 0x12;
    int iVar5 = ent[1] >> 0x12;
    unsigned int uVar7, uVar11, uVar10;

    if (param_2 == '\0') {
        int iVar6 = (iVar5 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar14;
        unsigned short uVar3 = *(unsigned short *)((int)DAT_00481f50 + iVar6 * 2);
        uVar11 = (unsigned char)((char)(uVar3 >> 5) << 3) + 0x2d;
        uVar7 = (unsigned char)((unsigned char)(uVar3 >> 10) << 3) + 0x2d;
        uVar10 = (unsigned char)(*(char *)((int)DAT_00481f50 + iVar6 * 2) << 3) + 0x2d;
    } else {
        int iVar6 = (iVar5 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar14;
        unsigned short uVar3 = *(unsigned short *)((int)DAT_00481f50 + iVar6 * 2);
        uVar7 = (unsigned int)(unsigned char)((unsigned char)(uVar3 >> 10) << 3);
        uVar11 = (unsigned int)(unsigned char)((char)(uVar3 >> 5) << 3);
        uVar10 = (unsigned int)(unsigned char)(*(char *)((int)DAT_00481f50 + iVar6 * 2) << 3);
    }
    if (uVar7 > 0xff) uVar7 = 0xff;
    if (uVar11 > 0xff) uVar11 = 0xff;
    if (uVar10 > 0xff) uVar10 = 0xff;

    /* Outer destruction loop: 11x11 area (radius 5) — fade destructible tiles */
    int local_1c = 0;
    int iVar6 = iVar5 - 5;
    while (iVar6 < iVar5 + 6) {
        int iVar8 = iVar14 - 5;
        while (iVar8 < iVar14 + 6) {
            int iVar12 = iVar6 << ((unsigned char)DAT_00487a18 & 0x1f);
            char cVar2 = *(char *)((int)DAT_0048782c + iVar12 + iVar8);
            if (cVar2 == '\x04' || cVar2 == '\x15') {
                unsigned short *puVar1 = (unsigned short *)((int)DAT_00481f50 + (iVar12 + iVar8) * 2);
                *puVar1 = *(unsigned short *)((int)DAT_004876b8 +
                    (unsigned int)*(unsigned short *)((int)DAT_00489230 + (unsigned int)*puVar1 * 2) * 2);
                iVar12 = iVar6 << ((unsigned char)DAT_00487a18 & 0x1f);
                if (*(short *)((int)DAT_00481f50 + (iVar12 + iVar8) * 2) == 0) {
                    local_1c++;
                    *(unsigned char *)(iVar12 + (int)DAT_0048782c + iVar8) = 0;
                }
            }
            iVar8++;
        }
        iVar6++;
    }

    /* Inner destruction loop: 7x7 area (radius 3) — immediate destruction */
    iVar6 = iVar5 - 3;
    while (iVar6 < iVar5 + 4) {
        for (int iVar8 = iVar14 - 3; iVar8 < iVar14 + 4; iVar8++) {
            int iVar12 = iVar6 << ((unsigned char)DAT_00487a18 & 0x1f);
            char cVar2 = *(char *)((int)DAT_0048782c + iVar12 + iVar8);
            if (cVar2 == '\x04' || cVar2 == '\x15') {
                *(unsigned short *)((int)DAT_00481f50 + (iVar12 + iVar8) * 2) = 0;
                local_1c++;
                *(unsigned char *)((iVar6 << ((unsigned char)DAT_00487a18 & 0x1f)) +
                    (int)DAT_0048782c + iVar8) = 0;
            }
        }
        iVar6++;
    }

    /* Spawn debris particles for each destroyed tile */
    int local_10 = 0;
    while (local_10 < local_1c) {
        if (DAT_00489248 > 0x9c3) break;

        unsigned int uVar9 = (unsigned int)rand();
        int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

        *(int *)(base + 0x00) = iVar14 << 0x12;
        *(int *)(base + 0x08) = iVar5 << 0x12;

        unsigned int uVar4 = (unsigned int)ent[4];
        int r1 = rand();
        unsigned int uVar13 = (unsigned int)ent[5] >> 0x1f;
        int absVelX = (int)((uVar4 ^ ((int)uVar4 >> 0x1f)) - ((int)uVar4 >> 0x1f));
        int absVelY = (int)(((unsigned int)ent[5] ^ uVar13) - uVar13);
        *(int *)(base + 0x18) =
            ((r1 % 2000 + ((absVelX + absVelY) >> 9)) *
            (*(int *)((int)DAT_00487ab0 + (uVar9 & 0x7ff) * 4) >> 1) >> 0xb) - ((int)uVar4 >> 2);

        uVar4 = (unsigned int)ent[5];
        int r2 = rand();
        uVar13 = (unsigned int)ent[4] >> 0x1f;
        absVelY = (int)((uVar4 ^ ((int)uVar4 >> 0x1f)) - ((int)uVar4 >> 0x1f));
        absVelX = (int)(((unsigned int)ent[4] ^ uVar13) - uVar13);
        *(int *)(base + 0x1c) =
            ((r2 % 2000 + ((absVelY + absVelX) >> 9)) *
            (*(int *)((int)DAT_00487ab0 + 0x800 + (uVar9 & 0x7ff) * 4) >> 1) >> 0xb) - ((int)uVar4 >> 2);

        *(int *)(base + 0x04) = iVar14 << 0x12;
        *(int *)(base + 0x0c) = iVar5 << 0x12;
        *(int *)(base + 0x10) = 0;
        *(int *)(base + 0x14) = 0;
        *(unsigned char *)(base + 0x21) = 0x66;
        *(short *)(base + 0x24) = (short)(rand() % 6);
        *(unsigned char *)(base + 0x20) = 0;
        *(unsigned char *)(base + 0x26) = 0xff;
        *(unsigned char *)(base + 0x22) = 0xff;
        *(int *)(base + 0x28) = 0;
        *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xd618);
        *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xd654);
        *(int *)(base + 0x48) = 0;
        *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xd684);
        *(unsigned char *)(base + 0x54) = 0;
        *(unsigned char *)(base + 0x40) = 0;
        *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xd590);
        *(int *)(base + 0x3c) = 0;
        *(unsigned char *)(base + 0x5c) = 0;

        DAT_00489248++;
        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = rand() % 0x46 + 0x32;
        *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
            (((int)uVar10 >> 3) + ((uVar7 & 0x1f8) * 0x20 + (uVar11 & 0x3ff8)) * 4) & 0xffff;
        *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) += 30000;

        local_10++;
    }

    /* Velocity modification */
    if (param_2 != '\0') {
        /* Original uses __ftol from FPU — halve velocity as approximation */
        ent[4] = ent[4] / 2;
        ent[5] = ent[5] / 2;
    } else {
        ent[4] = ent[4] / 2;
        ent[5] = ent[5] / 2;
    }
}

/* ===== FUN_00450f10 — Lava Damage Zone Check (00450F10) ===== */
static int FUN_00450f10_impl(int x, int y)
{
    unsigned int spr_w = *(unsigned char *)((int)DAT_00489e8c + 0x19b);
    int frame_off = *(int *)((int)DAT_00489234 + 0x66c);
    unsigned char spr_h = *(unsigned char *)((int)DAT_00489e88 + 0x19b);
    int sx = (x >> 0x12) - (int)(spr_w >> 1);
    int sy = (y >> 0x12) - (int)(spr_h >> 1);
    unsigned int idx2 = (sy << ((unsigned char)DAT_00487a18 & 0x1f)) + sx;
    int row = 0;

    if (spr_h != 0) {
        do {
            int col = 0;
            int tx = sx;
            if (spr_w != 0) {
                do {
                    if ((*(char *)((int)DAT_00489e94 + frame_off) != '\0') &&
                        (0 < tx) && (tx < (int)DAT_004879f0) &&
                        (0 < sy) && (sy < (int)DAT_004879f4) &&
                        (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + idx2) *
                                   0x20 + 10 + (int)DAT_00487928) != '\0'))
                    {
                        return 1;
                    }
                    frame_off++;
                    idx2++;
                    col++;
                    tx++;
                } while (col < (int)spr_w);
            }
            idx2 += DAT_00487a00 - spr_w;
            row++;
            sy++;
        } while (row < (int)(unsigned int)spr_h);
    }
    return 0;
}

/* ===== FUN_00450ce0 — Pickup Collision Check (00450CE0) ===== */
static int FUN_00450ce0_impl(int x, int y)
{
    unsigned int spr_w = *(unsigned char *)((int)DAT_00489e8c + 0x19b);
    int frame_off = *(int *)((int)DAT_00489234 + 0x66c);
    unsigned char spr_h = *(unsigned char *)((int)DAT_00489e88 + 0x19b);
    int sx = (x >> 0x12) - (int)(spr_w >> 1);
    int sy = (y >> 0x12) - (int)(spr_h >> 1);
    unsigned int idx2 = (sy << ((unsigned char)DAT_00487a18 & 0x1f)) + sx;
    int row = 0;

    if (spr_h != 0) {
        do {
            int col = 0;
            int tx = sx;
            if (spr_w != 0) {
                do {
                    if ((*(char *)((int)DAT_00489e94 + frame_off) != '\0') &&
                        (0 < tx) && (tx < (int)DAT_004879f0) &&
                        (0 < sy) && (sy < (int)DAT_004879f4) &&
                        (*(unsigned char *)((int)DAT_0048782c + idx2) > 0xef))
                    {
                        return *(unsigned char *)((int)DAT_0048782c + idx2);
                    }
                    frame_off++;
                    idx2++;
                    col++;
                    tx++;
                } while (col < (int)spr_w);
            }
            idx2 += DAT_00487a00 - spr_w;
            row++;
            sy++;
        } while (row < (int)(unsigned int)spr_h);
    }
    return 0;
}

/* ===== FUN_00451010 — Collision Resolution / Wall Push (00451010) ===== */
/* Tries to push entity out of solid tiles in 4 cardinal directions.
   If all attempts fail, applies damage and spawns spark particles. */
static void FUN_00451010_impl(unsigned int *ent, char param_2, int param_3)
{
    unsigned int *puVar1 = ent;
    unsigned int local_c = ent[1];
    unsigned int *puVar5 = (unsigned int *)ent[0];
    int iVar2 = (unsigned int)*(unsigned char *)((unsigned int)(unsigned char)(param_2 + 0x10) *
        0x20 + 0x17 + (int)DAT_00489e80) * 0x10;
    unsigned int local_4 = 10;
    int iVar4 = *(int *)(iVar2 + 4 + (int)DAT_00487880) >> 1;
    unsigned int *param_1 = puVar5;
    unsigned int local_8 = local_c;

    while (1) {
        local_4--;
        if ((int)puVar5 >> 0x12 < (int)DAT_004879f0 - 1)
            puVar5 = (unsigned int *)((int)puVar5 + 0x10000);
        if (0 < (int)((unsigned int)param_1 & 0xfffc0000))
            param_1 = (unsigned int *)((int)param_1 - 0x10000);
        if ((int)local_8 >> 0x12 < (int)DAT_004879f4 - 1)
            local_8 += 0x40000;
        if (0 < (int)(local_c & 0xfffc0000))
            local_c -= 0x40000;

        unsigned int uVar3 = FUN_00450dd0((int)puVar5, puVar1[1]);
        if ((char)uVar3 == '\0') {
            puVar1[0] = (unsigned int)puVar5;
            puVar1[4] = puVar1[4] + (unsigned int)iVar4;
            return;
        }
        uVar3 = FUN_00450dd0((int)param_1, puVar1[1]);
        if ((char)uVar3 == '\0') {
            puVar1[0] = (unsigned int)param_1;
            puVar1[4] = puVar1[4] - (unsigned int)iVar4;
            return;
        }
        uVar3 = FUN_00450dd0(puVar1[0], local_8);
        if ((char)uVar3 == '\0') {
            puVar1[1] = local_8;
            puVar1[5] = puVar1[5] + (unsigned int)iVar4;
            return;
        }
        uVar3 = FUN_00450dd0(puVar1[0], local_c);
        if ((char)uVar3 == '\0') {
            puVar1[1] = local_c;
            puVar1[5] = puVar1[5] - (unsigned int)iVar4;
            return;
        }

        if (local_4 == 0) {
            /* All directions blocked — apply crush damage */
            if ((char)puVar1[9] != '\0') return;

            int damage = *(int *)(iVar2 + 8 + (int)DAT_00487880);
            puVar1[8] -= (unsigned int)damage;
            DAT_00486be8[param_3] += (int)((unsigned int)(damage + ((int)damage >> 0x1f & 0x1fff)) >> 0xd);

            int particleCount = (int)((unsigned int)damage / 0x1400);
            int freq = (-(unsigned int)((particleCount & 0xfffffff0) != 0) & 0xfffffffd) + 4;
            particleCount = ((int)((unsigned int)particleCount + ((int)(unsigned int)particleCount >> 0x1f & 7)) >> 3) + 1;
            int rFreq = rand();
            if (rFreq % freq != 0) return;

            for (int p = 0; p < particleCount; p++) {
                if (DAT_00489248 > 0x9c3) return;

                unsigned int angle = (unsigned int)rand() & 0x7ff;
                int speed = rand();
                int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

                *(unsigned int *)(base + 0x00) = puVar1[0];
                *(unsigned int *)(base + 0x08) = puVar1[1];
                *(int *)(base + 0x18) = (*(int *)((int)DAT_00487ab0 + angle * 4) *
                    (speed % 0x32) >> 6) + ((int)puVar1[4] >> 2);
                *(int *)(base + 0x1c) = (*(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) *
                    (speed % 0x32) >> 6) + ((int)puVar1[5] >> 2);
                *(unsigned int *)(base + 0x04) = puVar1[0];
                *(unsigned int *)(base + 0x0c) = puVar1[1];
                *(int *)(base + 0x10) = 0;
                *(int *)(base + 0x14) = 0;
                *(unsigned char *)(base + 0x21) = 0x67;
                *(unsigned short *)(base + 0x24) = 0;
                *(unsigned char *)(base + 0x20) = 0;
                *(unsigned char *)(base + 0x26) = 0xff;
                *(unsigned char *)(base + 0x22) = 0xff;
                *(int *)(base + 0x28) = 0;
                *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xd830);
                *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xd86c);
                *(int *)(base + 0x48) = 0;
                *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xd89c);
                *(unsigned char *)(base + 0x54) = 0;
                *(unsigned char *)(base + 0x40) = 0;
                *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xd7a8);
                *(int *)(base + 0x3c) = 0;
                *(unsigned char *)(base + 0x5c) = 0;

                DAT_00489248++;
                *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x24) = 6;
                int rCol = rand();
                *(char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) =
                    (char)(rCol % 0xc) + '\x14';
                *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0x12;
                int iVar2b = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(unsigned int *)(iVar2b - 0x34) =
                    *(unsigned short *)((int)DAT_00487aa8 +
                        (unsigned int)*(unsigned char *)(iVar2b - 0x1b) * 2) + 30000;
            }
            return;
        }
        if ((int)local_4 < 1) return;
    }
}

/* ===== FUN_00451590 — Death Handler (00451590) ===== */
static void FUN_00451590_impl(int *ent, int param_2)
{
    int *piVar4;
    int iVar7;

    if (*(char *)((int)ent + 0x4a2) == '\0') {
        /* Self/env kill */
        *(unsigned char *)((int)ent + 0xcb) = 200;
        ent[0x125] = ent[0x125] - 1;
        piVar4 = (int *)((unsigned int)*(unsigned char *)(ent + 0xb) * 0x4000 + 4 + (int)DAT_00487aa4);
    } else {
        unsigned char bVar1 = *(unsigned char *)((int)ent + 0x4a1);
        if (bVar1 < 0x50) {
            /* Killed by another player */
            unsigned int uVar5 = (unsigned int)*(unsigned char *)((int)DAT_00487810 + 0x2c +
                (unsigned int)bVar1 * 0x598);
            *(unsigned char *)((int)DAT_00487810 + (unsigned int)bVar1 * 0x598 + 0xcb) = 200;
            if (uVar5 != *(unsigned char *)(ent + 0xb)) {
                /* Enemy kill — increment killer's score */
                *(int *)((int)DAT_00487810 + 0x494 +
                    (unsigned int)*(unsigned char *)((int)ent + 0x4a1) * 0x598) += 1;
                piVar4 = (int *)(uVar5 * 0x4000 + 4 + (int)DAT_00487aa4);
                iVar7 = *piVar4 + 1;
                goto LAB_apply;
            }
            /* Team kill — decrement killer's score */
            *(int *)((int)DAT_00487810 + 0x494 +
                (unsigned int)*(unsigned char *)((int)ent + 0x4a1) * 0x598) -= 1;
            piVar4 = (int *)(uVar5 * 0x4000 + 4 + (int)DAT_00487aa4);
        } else if (bVar1 < 200) {
            /* Killed by entity type (bVar1 - 100) */
            unsigned int uVar5 = (unsigned int)(bVar1 - 100);
            if (uVar5 != *(unsigned char *)(ent + 0xb)) {
                piVar4 = (int *)(uVar5 * 0x4000 + 4 + (int)DAT_00487aa4);
                iVar7 = *piVar4 + 1;
                goto LAB_apply;
            }
            piVar4 = (int *)(uVar5 * 0x4000 + 4 + (int)DAT_00487aa4);
        } else {
            /* Environment kill (bVar1 >= 200) */
            DAT_0048929c++;
            *(unsigned char *)((int)ent + 0xcb) = 200;
            ent[0x125] = ent[0x125] - 1;
            piVar4 = (int *)((unsigned int)*(unsigned char *)(ent + 0xb) * 0x4000 + 4 + (int)DAT_00487aa4);
        }
    }
    iVar7 = *piVar4 - 1;

LAB_apply:
    *piVar4 = iVar7;

    /* Decrement lives */
    if (ent[10] != 0) ent[10] -= 1;

    /* Shared lives mode: subtract a life from all same-team players */
    if (DAT_0048373b != '\0' && DAT_00489240 > 0) {
        int iVar3 = 0;
        for (int p = 0; p < DAT_00489240; p++) {
            if (p != param_2 && (char)ent[0xb] == *(char *)(iVar3 + 0x2c + (int)DAT_00487810)) {
                unsigned int uVar5 = *(unsigned int *)(iVar3 + 0x28 + (int)DAT_00487810);
                if (uVar5 > 1) {
                    *(unsigned int *)(iVar3 + 0x28 + (int)DAT_00487810) = uVar5 - 1;
                    *(unsigned char *)(iVar3 + 0xcc + (int)DAT_00487810) = 200;
                }
            }
            iVar3 += 0x598;
        }
    }

    /* Set respawn timer based on game mode */
    if (DAT_00483744 == '\0')       ent[0xc] = 0;
    else if (DAT_00483744 == '\x01') ent[0xc] = 0x3f;
    else if (DAT_00483744 == '\x02') ent[0xc] = 0xbd;
    else if (DAT_00483744 == '\x03') ent[0xc] = 0x1f8;
    else                             ent[0xc] = -1;

    /* Increment team death counter */
    int *piVar4b = (int *)((unsigned int)*(unsigned char *)(ent + 0xb) * 0x4000 + (int)DAT_00487aa4);
    *piVar4b = *piVar4b + 1;

    /* Reset entity state */
    *(unsigned char *)(ent + 9) = 1;       /* mark as dead */
    ent[0x126] += 1;                       /* death count++ */
    ent[8] = 0;                            /* health = 0 */
    *(unsigned char *)((int)ent + 0x47d) = 0x32;  /* invuln timer */
    *(unsigned char *)((int)ent + 0xc6) = 0;
    ent[0x34] = 0;
    ent[0x29] = 0;
    *(unsigned char *)((int)ent + 0x4a1) = 0;
    *(unsigned char *)((int)ent + 0x4a2) = 0;
    *(unsigned char *)(ent + 0x33) = 200;  /* spawn protection */

    FUN_00451500();

    /* Explosion at death location */
    FUN_004357b0(ent[0] >> 0x12, ent[1] >> 0x12, 9, 0, '\0',
        0, 0, 0, 0, '\0', '\0', (unsigned char)param_2);

    /* Spawn death flash particle */
    if (DAT_00489250 < 2000) {
        int base2 = DAT_00489250 * 0x20 + (int)DAT_00481f34;
        *(int *)(base2 + 0x00) = ent[0];
        *(int *)(base2 + 0x04) = ent[1];
        *(int *)(base2 + 0x08) = 0;
        *(int *)(base2 + 0x0c) = 0;
        *(unsigned char *)(base2 + 0x10) = 0xb;
        *(unsigned char *)(base2 + 0x11) = 0;
        *(unsigned char *)(base2 + 0x12) = 0;
        *(unsigned char *)(base2 + 0x13) = 1;
        *(unsigned char *)(base2 + 0x14) = 0xff;
        *(unsigned char *)(base2 + 0x15) = 0;

        DAT_00489250++;
        *(unsigned char *)(DAT_00489250 * 0x20 + (int)DAT_00481f34 - 0xb) = 1;

        /* Play random death sound */
        int iVar7b = ent[1];
        int iVar8 = ent[0];
        int r = rand();
        FUN_0040f9b0(r % 7 + 0x65, iVar8, iVar7b);

        /* Camera shake / knockback */
        FUN_00437cf0(ent[0], ent[1], 500, param_2, -1);
    }

    /* Spawn 128 directional debris/gibs into fire particle array */
    int debrisAngleSin = 0;
    int debrisAngleCos = 0x800;
    for (int i = 0; i < 0x80; i++) {
        if (DAT_00489250 >= 2000) break;

        int nextCos = debrisAngleCos + 0x40;
        int nextSin = debrisAngleSin + 0x40;
        if (nextCos > 0x27ff) {
            nextSin -= 0x2000;
            nextCos -= 0x2000;
        }

        unsigned int ptype = (unsigned int)rand() & 3;

        if (DAT_00489250 < 2000) {
            int base2 = DAT_00489250 * 0x20 + (int)DAT_00481f34;
            *(int *)(base2 + 0x00) = ent[0];
            *(int *)(base2 + 0x04) = ent[1];
            *(int *)(base2 + 0x08) = (rand() % 0x32) *
                *(int *)((int)DAT_00487ab0 + nextSin * 4) >> 5;
            *(int *)(base2 + 0x0c) = (rand() % 0x32) *
                *(int *)(nextCos * 4 + (int)DAT_00487ab0) >> 5;
            *(char *)(base2 + 0x10) = (char)ptype + '\x01';
            unsigned int sub = (unsigned int)rand() & 3;
            *(char *)(base2 + 0x11) = (char)sub;
            *(unsigned char *)(base2 + 0x12) = 0;
            *(unsigned char *)(base2 + 0x13) = 199;
            *(unsigned char *)(base2 + 0x14) = (unsigned char)param_2;
            *(unsigned char *)(base2 + 0x15) = 0;
            DAT_00489250++;
        }
        debrisAngleSin = nextSin;
        debrisAngleCos = nextCos;
    }

    /* Spawn complex projectile debris (entity fragments) */
    /* The original uses __ftol to get particle count from FPU — approximate with 10 */
    int fragmentCount = 10;
    for (int i = 0; i < fragmentCount; i++) {
        if (DAT_00489248 > 0x9c3) return;

        unsigned int uVar5 = (unsigned int)rand() & 0x7ff;
        int speed = rand() % 0x46 + 10;
        unsigned int uVar6 = (unsigned int)rand() & 1;
        int iVar8 = (int)uVar6 + 0x6c;

        int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;
        *(int *)(base + 0x00) = ent[2];
        *(int *)(base + 0x08) = ent[3];
        *(int *)(base + 0x18) = (*(int *)((int)DAT_00487ab0 + uVar5 * 4) * speed >> 7) + ent[4];
        *(int *)(base + 0x1c) = (*(int *)((int)DAT_00487ab0 + 0x800 + uVar5 * 4) * speed >> 7) + ent[5];
        *(int *)(base + 0x04) = ent[2];
        *(int *)(base + 0x0c) = ent[3];
        *(int *)(base + 0x10) = 0;
        *(int *)(base + 0x14) = 0;
        *(char *)(base + 0x21) = (char)iVar8;
        *(unsigned short *)(base + 0x24) = 0;
        *(unsigned char *)(base + 0x20) = 0;
        *(unsigned char *)(base + 0x26) = 0xff;
        *(unsigned char *)(base + 0x22) = 0xff;
        *(int *)(base + 0x28) = 0;

        int iVar3 = iVar8 * 0x86;
        *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + (rand() % 3 + iVar3) * 4);
        *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xc4 + (rand() % 3 + iVar3) * 4);
        *(int *)(base + 0x48) = 0;
        *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xf4 + (rand() % 3 + iVar3) * 4);
        *(unsigned char *)(base + 0x54) = 0;
        *(char *)(base + 0x40) = (char)(rand() % 3);
        *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + iVar8 * 0x218);
        *(int *)(base + 0x3c) = 0;
        *(unsigned char *)(base + 0x5c) = 0;

        DAT_00489248++;
        int rCol = rand();
        if (rCol % 3 == 0) {
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) += 300;
        } else {
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) +=
                (unsigned int)*(unsigned char *)(ent + 0xb) * 100;
        }
        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = rand() % 0x5a + 100;
        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x38) =
            rand() % (int)(*(unsigned char *)((int)DAT_00487abc + 0x126 + iVar8 * 0x218) - 1);
    }
}

/* ===== FUN_0044dea0 — Respawn Handler (0044DEA0) ===== */
static void FUN_0044dea0_impl(int *ent, int idx)
{
    if (ent[10] == 0) return;

    int iVar2 = 0;
    int iVar1;
    while (1) {
        iVar1 = FUN_0044dfb0(idx);
        if (iVar1 != 0) break;
        if (*(char *)((int)ent + 0x26) != -1)
            *(char *)((int)ent + 0x26) = *(char *)((int)ent + 0x26) + 1;
        iVar2++;
        if (iVar2 > 7) return;
    }

    FUN_0040f9b0(0x11b, ent[0], ent[1]);

    int tileIdx = (ent[0] >> 0x12) +
        ((ent[1] >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f));
    unsigned char tileType = *(unsigned char *)((int)DAT_0048782c + tileIdx);
    if (*(char *)((unsigned int)tileType * 0x20 + 4 + (int)DAT_00487928) == '\x01')
        *(unsigned char *)((int)ent + 0x49f) = 0x3f;
    else
        *(unsigned char *)((int)ent + 0x49f) = 0;

    *(unsigned char *)((int)ent + 0x25) = 0;
    *(unsigned char *)(ent + 9) = 0;
    int health = *(int *)(idx * 0x40 + 0x28 + (int)DAT_0048780c);
    *(unsigned char *)(ent + 0x11f) = 1;
    ent[8] = health;
    *(unsigned char *)(ent + 0x127) = 0;
    ent[2] = ent[0]; ent[3] = ent[1];
    ent[4] = 0; ent[5] = 0;
    ent[0x24] = 0; ent[0x25] = 0;
    *(unsigned char *)(ent + 0x128) = 0xff;
    *(unsigned char *)((int)ent + 0x4a3) = 0x30;
    *(unsigned char *)((int)ent + 0x26) = 0;
    ent[0x26] = DAT_00483830;
}

/* ===== FUN_0040fb70 — Positional Sound Update (0040FB70) ===== */
static void FUN_0040fb70_impl(int idx)
{
    int iVar9 = idx * 0x598;
    int iVar6 = *(int *)(iVar9 + 0x4a8 + (int)DAT_00487810);

    if (1 < iVar6) {
        int iVar8 = 0xff;
        *(int *)(iVar9 + 0x4a8 + (int)DAT_00487810) = iVar6 - 1;
        float minDist = 10000.0f;

        if (0 < DAT_00487808) {
            int *pIdx = (int *)&DAT_004877f8;
            int count = DAT_00487808;
            int iVar1 = *(int *)(iVar9 + (int)DAT_00487810);
            int iVar2 = *(int *)(iVar9 + 4 + (int)DAT_00487810);

            do {
                int iVar7 = *pIdx;
                int iVar5 = (*(int *)((int)DAT_00487810 + iVar7 * 0x598 + 4) - iVar2) >> 0x12;
                int iVar4 = (*(int *)((int)DAT_00487810 + iVar7 * 0x598) - iVar1) >> 0x12;
                float fVar3 = (float)(iVar5 * iVar5 + iVar4 * iVar4) * _DAT_004753fc;
                if (fVar3 < _DAT_004753e8) fVar3 = _DAT_004753e8;
                if (fVar3 < minDist) {
                    iVar8 = iVar7;
                    minDist = fVar3;
                }
                pIdx++;
                count--;
            } while (count != 0);

            if (iVar8 != 0xff) {
                int vol = (int)(1.0f / minDist) * 2;
                unsigned long long angle = (unsigned long long)FUN_004257e0(
                    *(int *)((int)DAT_00487810 + iVar8 * 0x598),
                    *(int *)((int)DAT_00487810 + iVar8 * 0x598 + 4),
                    iVar1, iVar2);
                int pan = (*(int *)((int)DAT_00487ab0 + (int)angle * 4) >> 0xc) + 0x80;
                if (minDist < (float)_DAT_004753e0_d) pan = 0x80;

                if (vol >= 0x100) vol = 0xff;
                if (vol >= 5) {
                    FSOUND_SetPan(*(int *)(iVar9 + 0x4ac + (int)DAT_00487810), pan);
                    FSOUND_SetVolume(*(int *)(iVar9 + 0x4ac + (int)DAT_00487810), vol);
                }
            }
        }
    }

    if (*(int *)(iVar9 + 0x4a8 + (int)DAT_00487810) == 1) {
        *(int *)(iVar9 + 0x4a8 + (int)DAT_00487810) = 0;
        FSOUND_StopSound(*(int *)(iVar9 + 0x4ac + (int)DAT_00487810));
    }
}

/* ===== FUN_00401000 — Main Entity Update (Tier 4+, stub) ===== */
static void FUN_00401000_impl(int idx) { (void)idx; }

/* ===== FUN_0044d930 — Trooper Recruitment (0044D930) ===== */
static void FUN_0044d930_impl(int *ent)
{
    if ((unsigned int)ent[0x29] % 0x28 == 0) {
        FUN_0040f9b0(10, ent[0], ent[1]);
    }

    int i = 0, iVar3 = 0;
    if (0 < DAT_0048924c) {
        do {
            int iVar4 = ent[0] - *(int *)(iVar3 + (int)DAT_00487884);
            int iVar5 = ent[1] - *(int *)(iVar3 + 8 + (int)DAT_00487884);
            int r = rand();
            if ((r % 0x32 == 0) && (iVar4 < 0xb40000) && (-0xb40000 < iVar4) &&
                (iVar5 < 0xb40000) && (-0xb40000 < iVar5))
            {
                *(char *)(iVar3 + 0x1c + (int)DAT_00487884) = (char)ent[0xb];
            }
            i++;
            iVar3 += 0x40;
        } while (i < DAT_0048924c);
    }
}

/* ===== FUN_0044d9f0 — Repulsion Force Field (0044D9F0) ===== */
static void FUN_0044d9f0_impl(int *ent)
{
    int offset = 0;
    int i;
    for (i = 0; i < DAT_00489240; i++, offset += 0x598) {
        if (*(char *)(offset + 0x2c + (int)DAT_00487810) == (char)ent[0xb])
            continue;
        int ex = *(int *)(offset + (int)DAT_00487810);
        int px = ent[0];
        if ((ex - 0x1400000 < px) && (px < ex + 0x1400000)) {
            int ey = *(int *)(offset + 4 + (int)DAT_00487810);
            int py = ent[1];
            if ((ey - 0x1400000 < py) && (py < ey + 0x1400000)) {
                int angle = FUN_004257e0(px, py, ex, ey);
                *(int *)(offset + 0x10 + (int)DAT_00487810) +=
                    *(int *)((int)DAT_00487ab0 + angle * 4) >> 1;
                *(int *)(offset + 0x14 + (int)DAT_00487810) +=
                    *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 1;
            }
        }
    }

    int poff = 0;
    int pbase = (int)DAT_004892e8;
    for (i = 0; i < DAT_00489248; i++, poff += 0x80) {
        unsigned char owner = *(unsigned char *)(poff + 0x22 + pbase);
        char team;
        if (owner < 0x46) {
            team = *(char *)((int)DAT_00487810 + 0x2c + (unsigned int)owner * 0x598);
        } else {
            team = -5;
        }
        if (team == (char)ent[0xb]) continue;

        int ex2 = *(int *)(poff + pbase);
        int px2 = ent[0];
        if ((ex2 - 0x1400000 < px2) && (px2 < ex2 + 0x1400000)) {
            int ey2 = *(int *)(poff + 8 + pbase);
            int py2 = ent[1];
            if ((ey2 - 0x1400000 < py2) && (py2 < ey2 + 0x1400000)) {
                int angle = FUN_004257e0(px2, py2, ex2, ey2);
                *(int *)(poff + 0x18 + (int)DAT_004892e8) +=
                    *(int *)((int)DAT_00487ab0 + angle * 4) >> 1;
                *(int *)(poff + 0x1c + (int)DAT_004892e8) +=
                    *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 1;
                pbase = (int)DAT_004892e8;
            }
        }
    }
}

/* ===== FUN_0044dbb0 — Spawn Bomb/Mine (0044DBB0) ===== */
static void FUN_0044dbb0_impl(int *ent, int idx)
{
    if (DAT_00489248 >= 0x9c4) return;

    FUN_0040f9b0(0x12, ent[0], ent[1]);

    int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

    *(int *)(base + 0x00) = ent[0];
    *(int *)(base + 0x08) = ent[1] + 0x100000;
    *(int *)(base + 0x18) = ent[4] >> 3;
    *(int *)(base + 0x1c) = 0x14;
    *(int *)(base + 0x04) = ent[0];
    *(int *)(base + 0x0c) = ent[1] + 0x100000;
    *(int *)(base + 0x10) = 0;
    *(int *)(base + 0x14) = 0;
    *(unsigned char *)(base + 0x21) = 0x12;
    *(unsigned short *)(base + 0x24) = 0;
    *(unsigned char *)(base + 0x20) = 0;
    *(unsigned char *)(base + 0x26) = 0xfe;
    *(unsigned char *)(base + 0x22) = (unsigned char)idx;
    *(int *)(base + 0x28) = 0;
    *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0x2638);
    *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0x2674);
    *(int *)(base + 0x48) = 0;
    *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0x26a4);
    *(unsigned char *)(base + 0x54) = 0;
    *(unsigned char *)(base + 0x40) = 0;
    *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0x25b0);
    *(int *)(base + 0x3c) = 0;
    *(unsigned char *)(base + 0x5c) = 0;

    DAT_00489248++;
    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 600;
}

/* ===== FUN_0044ea70 — Energy Explosion Particles (0044EA70) ===== */
static void FUN_0044ea70_impl(int *ent)
{
    *(unsigned char *)((int)ent + 0xc6) = 0;

    int particleCount = 10;  /* original reads from FPU; approximate */

    FUN_0040f9b0(0x10b, ent[0], ent[1]);

    for (int i = 0; i < particleCount; i++) {
        if (DAT_00489248 > 0x9c3) return;

        unsigned int angle = rand() & 0x7ff;
        int speed = rand() % 0x28;

        int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

        *(int *)(base + 0x00) = ent[2];
        *(int *)(base + 0x08) = ent[3];
        *(int *)(base + 0x18) = *(int *)((int)DAT_00487ab0 + angle * 4) * speed >> 7;
        *(int *)(base + 0x1c) = *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) * speed >> 7;
        *(int *)(base + 0x04) = ent[2];
        *(int *)(base + 0x0c) = ent[3];
        *(int *)(base + 0x10) = 0;
        *(int *)(base + 0x14) = 0;
        *(unsigned char *)(base + 0x21) = 100;
        *(short *)(base + 0x24) = (short)(rand() % 6);
        *(unsigned char *)(base + 0x20) = 0;
        *(unsigned char *)(base + 0x26) = 0xff;
        *(unsigned char *)(base + 0x22) = 0xff;
        *(int *)(base + 0x28) = 0;
        *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xd1e8);
        *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0xd224);
        *(int *)(base + 0x48) = 0;
        *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0xd254);
        *(unsigned char *)(base + 0x54) = 0;
        *(unsigned char *)(base + 0x40) = 0;
        *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xd160);
        *(int *)(base + 0x3c) = 0;
        *(unsigned char *)(base + 0x5c) = 0;

        DAT_00489248++;

        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = rand() % 0x50 + 0x32;
        int colorIdx = rand() % 0xc;
        *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
            *(unsigned short *)((int)DAT_00487aa8 + 0xc6 + colorIdx * 2) + 30000;
    }
}

/* ===== FUN_0040f9b0 — Positional Sound Playback (0040F9B0) ===== */
void FUN_0040f9b0(int snd, int x, int y)
{
    float fVar2;
    int iVar3, iVar5, iVar7;
    int iVar8 = 0xff;
    float minDist = 10000.0f;

    if (DAT_0048371f == '\0' || g_SoundEnabled == 0) return;
    if (*(int *)((int)g_SoundTable + snd * 8) == 0) return;

    if (0 < DAT_00487808) {
        int *pIdx = (int *)&DAT_004877f8;
        int count = DAT_00487808;
        do {
            int *piVar1 = (int *)((int)DAT_00487810 + (*pIdx) * 0x598);
            iVar5 = (piVar1[1] - y) >> 0x12;
            iVar3 = (piVar1[0] - x) >> 0x12;
            fVar2 = (float)(iVar5 * iVar5 + iVar3 * iVar3) * _DAT_004753fc;
            if (fVar2 < _DAT_004753e8) fVar2 = _DAT_004753e8;
            if (fVar2 < minDist) {
                iVar8 = *pIdx;
                minDist = fVar2;
            }
            pIdx++;
            count--;
        } while (count != 0);

        if (iVar8 != 0xff) {
            iVar7 = (int)(1.0f / minDist) * 2;

            int angle = FUN_004257e0(
                *(int *)((int)DAT_00487810 + iVar8 * 0x598),
                *(int *)((int)DAT_00487810 + iVar8 * 0x598 + 4),
                x, y);
            int pan = (*(int *)((int)DAT_00487ab0 + angle * 4) >> 0xc) + 0x80;
            if (minDist < (float)_DAT_004753e0_d) pan = 0x80;

            if (iVar7 >= 0x100) iVar7 = 0xff;
            if (iVar7 < 5) return;

            int chan = FSOUND_PlaySoundEx(-1,
                (FSOUND_SAMPLE *)(*(int *)((int)g_SoundTable + snd * 8)), NULL, 1);
            FSOUND_SetVolume(chan, iVar7);
            FSOUND_SetPan(chan, pan);
            FSOUND_SetPaused(chan, 0);
        }
    }
}

/* ===== FUN_0044bb70 — Save_Previous_Positions ===== */
static void FUN_0044bb70(void)
{
    int i;
    if (DAT_00489240 > 0) {
        for (i = 0; i < DAT_00489240; i++) {
            int off = i * 0x598;
            *(int *)(DAT_00487810 + off + 0x08) = *(int *)(DAT_00487810 + off);
            *(int *)(DAT_00487810 + off + 0x0C) = *(int *)(DAT_00487810 + off + 4);
        }
    }
}

/* ===== FUN_0044b990 — Read_Keyboard_To_Buttons ===== */
/* Reads DirectInput keyboard state and maps per-player key bindings
 * to button flags at entity +0xB8. Uses lpDI_Keyboard (= DAT_00489ee4). */
static void FUN_0044b990(void)
{
    unsigned char keyState[256];
    HRESULT hr;
    int i;

    /* Save previous buttons → +0xBC, clear current → +0xB8 */
    for (i = 0; i < DAT_00489240; i++) {
        int off = i * 0x598;
        *(unsigned int *)(DAT_00487810 + off + 0xBC) = *(unsigned int *)(DAT_00487810 + off + 0xB8);
        *(unsigned int *)(DAT_00487810 + off + 0xB8) = 0;
    }

    if (lpDI_Keyboard == NULL) return;

    hr = lpDI_Keyboard->GetDeviceState(256, keyState);
    if (FAILED(hr)) {
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            lpDI_Keyboard->Acquire();
        }
        return;
    }

    /* Map key bindings to button flags for each human player */
    for (i = 0; i < DAT_00489240; i++) {
        int off = i * 0x598;
        int base = DAT_00487810 + off;

        /* Skip AI-controlled entities (+0xDD != 0) */
        if (*(char *)(base + 0xDD) != '\0')
            continue;

        /* 7 key bindings at +0xAC through +0xB2 → 7 button bits */
        unsigned char k0 = *(unsigned char *)(base + 0xAC);
        unsigned char k1 = *(unsigned char *)(base + 0xAD);
        unsigned char k2 = *(unsigned char *)(base + 0xAE);
        unsigned char k3 = *(unsigned char *)(base + 0xAF);
        unsigned char k4 = *(unsigned char *)(base + 0xB0);
        unsigned char k5 = *(unsigned char *)(base + 0xB1);
        unsigned char k6 = *(unsigned char *)(base + 0xB2);

        unsigned int *buttons = (unsigned int *)(base + 0xB8);
        if (keyState[k0] & 0x80) *buttons |= 0x01;  /* Turn Left */
        if (keyState[k1] & 0x80) *buttons |= 0x02;  /* Turn Right */
        if (keyState[k2] & 0x80) *buttons |= 0x04;  /* Thrust */
        if (keyState[k3] & 0x80) *buttons |= 0x08;  /* Fire Primary */
        if (keyState[k4] & 0x80) *buttons |= 0x10;  /* Fire Secondary */
        if (keyState[k5] & 0x80) *buttons |= 0x20;  /* Special/Detonate */
        if (keyState[k6] & 0x80) *buttons |= 0x40;  /* Brake */

    }
}

/* ===== FUN_0044bbb0 — Tick_Cooldown_Timers ===== */
/* Decrements ~22 cooldown/timer fields per entity each tick. */
static void FUN_0044bbb0(int base)
{
    /* int timers — decrement if nonzero */
    if (*(int *)(base + 0x90) != 0) (*(int *)(base + 0x90))--;
    if (*(int *)(base + 0x94) != 0) (*(int *)(base + 0x94))--;
    if (*(int *)(base + 0xA4) != 0) (*(int *)(base + 0xA4))--;
    if (*(int *)(base + 0xA8) != 0) (*(int *)(base + 0xA8))--;
    if (*(int *)(base + 0xD0) > 0)  (*(int *)(base + 0xD0))--;
    if (*(int *)(base + 0xD4) > 0)  (*(int *)(base + 0xD4))--;
    if (*(int *)(base + 0xD8) > 0)  (*(int *)(base + 0xD8))--;
    if (*(int *)(base + 0x30) > 0)  (*(int *)(base + 0x30))--;

    /* char timers — decrement if nonzero */
    if (*(char *)(base + 0xC4) != '\0') (*(char *)(base + 0xC4))--;
    if (*(char *)(base + 0xA2) != '\0') (*(char *)(base + 0xA2))--;
    if (*(char *)(base + 0xA0) != '\0') (*(char *)(base + 0xA0))--;
    if (*(char *)(base + 0xC5) != '\0') (*(char *)(base + 0xC5))--;
    if (*(char *)(base + 0xC8) != '\0') (*(char *)(base + 0xC8))--;
    if (*(char *)(base + 0xC9) != '\0') (*(char *)(base + 0xC9))--;
    if (*(char *)(base + 0xCB) != '\0') (*(char *)(base + 0xCB))--;
    if (*(char *)(base + 0xCC) != '\0') (*(char *)(base + 0xCC))--;
    if (*(char *)(base + 0xDC) != '\0') (*(char *)(base + 0xDC))--;
    if (*(char *)(base + 0x47E) != '\0') (*(char *)(base + 0x47E))--;
    if (*(char *)(base + 0x49E) != '\0') (*(char *)(base + 0x49E))--;
    if (*(char *)(base + 0x4A2) != '\0') (*(char *)(base + 0x4A2))--;
    if (*(char *)(base + 0x4A3) != '\0') (*(char *)(base + 0x4A3))--;

    /* Stun timer at +0xC6: random 1/128 chance of decrementing */
    if (*(char *)(base + 0xC6) != '\0') {
        if ((rand() & 0x7F) == 0) {
            (*(char *)(base + 0xC6))--;
        }
    }
}

/* ===== FUN_0044bf20 — Heading_Rotation ===== */
/* Rotates heading angle based on button flags (bits 1=left, 2=right).
 * Turn rate comes from ship stats table, halved during speed boost. */
static void FUN_0044bf20(int base, int player_idx)
{
    unsigned int buttons = *(unsigned int *)(base + 0xB8);
    unsigned char turn_rate = *(unsigned char *)((int)DAT_0048780c + player_idx * 0x40 + 0x24);
    int boost_active = (*(int *)(base + 0xD4) > 0) ? 1 : 0;

    /* Halve turn rate during boost */
    int rate = (int)turn_rate >> boost_active;

    if (buttons & 0x01) {  /* Turn Left */
        *(unsigned int *)(base + 0x18) = (*(int *)(base + 0x18) + rate) & 0x7FF;
    }
    if (buttons & 0x02) {  /* Turn Right */
        *(unsigned int *)(base + 0x18) = (*(int *)(base + 0x18) - rate) & 0x7FF;
    }
}

/* ===== FUN_0044bfa0 — Thrust_Application ===== */
/* Applies thrust acceleration in the heading direction using sin/cos LUT.
 * Spawns engine exhaust particles behind the ship at regular intervals. */
static void FUN_0044bfa0(int *ent, int player_idx)
{
    int heading = ent[6];  /* +0x18: heading angle 0-0x7FF */
    int cfg_off = player_idx * 0x40;
    unsigned char accel = *(unsigned char *)((int)DAT_0048780c + cfg_off + 0x23);
    int boost_active = (ent[0x35] > 0) ? 1 : 0;  /* +0xD4: speed boost timer */
    int shift = 0xB + boost_active;  /* halve thrust during boost */

    int *lut = (int *)DAT_00487ab0;

    /* vel_x += cos(heading) * accel >> shift */
    ent[4] += (lut[heading] * (int)accel) >> shift;
    /* vel_y += sin(heading) * accel >> shift */
    ent[5] += (lut[(heading + 0x200) & 0x7FF] * (int)accel) >> shift;

    /* Increment fire counter, spawn exhaust when it wraps */
    char counter = (char)ent[7] + 1;  /* +0x1C: exhaust counter */
    *(char *)(ent + 7) = counter;

    unsigned char fire_interval = *(unsigned char *)((int)DAT_0048780c + cfg_off + 0x2E);
    if (counter == (char)fire_interval) {
        *(char *)(ent + 7) = 0;

        /* Check collision bitmap: only spawn if in viewport area (bit 0x08) */
        if (DAT_00487814 != NULL) {
            int gx = ent[0] >> 0x16;
            int gy = ent[1] >> 0x16;
            if (gx >= 0 && gy >= 0 && gx < (int)DAT_004879f8 && gy < (int)DAT_004879fc) {
                if ((((unsigned char *)DAT_00487814)[gy * DAT_004879f8 + gx] & 0x08) != 0) {
                    /* Exhaust type from config byte at +0x2D:
                     *   0 = complex emitter (DAT_004892e8, stride 0x80) — big thruster
                     *   1 = fire type A (sprite 5 or 6)
                     *   2 = fire type B (sprite 3 or 4) */
                    char exhaust_type = *(char *)((int)DAT_0048780c + cfg_off + 0x2D);

                    if (exhaust_type == '\0') {
                        /* Type 0: Complex emitter particles in DAT_004892e8 (stride 0x80).
                         * Spawns TWO emitter entities processed by Entity_Debris_Animation.
                         * First emitter at heading+0x200, second at computed offset. */
                        if (DAT_00489248 < 0xA28 && DAT_004892e8 != NULL) {
                            unsigned int uVar6 = (heading + 0x200) & 0x7FF;
                            unsigned int uVar7 = (heading - 0x400) & 0x7FF;
                            int r = rand();
                            unsigned int uVar3 = (r % 0xA0 + 0x3B0 + heading) & 0x7FF;
                            int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

                            /* Position: slightly behind ship at reverse+side angle */
                            *(int *)(base + 0x00) = ent[0] + (lut[uVar6] + lut[uVar7] * 2) * 2;
                            *(int *)(base + 0x08) = ent[1] + (lut[(uVar6 + 0x200) & 0x7FF] + lut[(uVar7 + 0x200) & 0x7FF] * 2) * 2;
                            /* Velocity: spread-based */
                            *(int *)(base + 0x18) = (lut[uVar3] * 0x14 >> 5) + ent[4];
                            *(int *)(base + 0x1C) = (lut[(uVar3 + 0x200) & 0x7FF] * 0x14 >> 5) + ent[5];
                            /* Previous position = current */
                            *(int *)(base + 0x04) = *(int *)(base + 0x00);
                            *(int *)(base + 0x0C) = *(int *)(base + 0x08);
                            *(int *)(base + 0x10) = 0;
                            *(int *)(base + 0x14) = 0;
                            /* Behavior/type fields */
                            *(unsigned char *)(base + 0x21) = 0x67;
                            *(short *)(base + 0x24) = (short)(rand() % 6);
                            *(unsigned char *)(base + 0x20) = 0;
                            *(unsigned char *)(base + 0x26) = 0xFE;
                            *(unsigned char *)(base + 0x22) = (unsigned char)player_idx;
                            *(int *)(base + 0x28) = 0;
                            /* Sprite/animation data from entity type table */
                            *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xD830);
                            *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xD7A8);
                            *(int *)(base + 0x3C) = 0;
                            *(unsigned char *)(base + 0x40) = 0;
                            *(int *)(base + 0x44) = 0x2800;
                            *(int *)(base + 0x48) = 0;
                            *(unsigned char *)(base + 0x54) = 0;
                            *(unsigned char *)(base + 0x5C) = 1;
                            *(unsigned char *)(base + 0x64) = 0x72;
                            *(unsigned char *)(base + 0x65) = 0x7F;
                            *(int *)(base + 0x4C) = (int)*(unsigned short *)((int)DAT_00487aa8 + 0x7F * 2) + 30000;
                            DAT_00489248++;

                            /* Second emitter particle at different angle */
                            if (DAT_00489248 < 0xA28) {
                                unsigned int uVar6b = (heading + 0x200) & 0x7FF;
                                unsigned int uVar7b = (heading - 0x400) & 0x7FF;
                                r = rand();
                                uVar3 = (r % 0xA0 + 0x3B0 + heading) & 0x7FF;
                                base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

                                *(int *)(base + 0x00) = ent[0] + (lut[uVar6b] + lut[uVar7b] * 2) * 2;
                                *(int *)(base + 0x08) = ent[1] + (lut[(uVar6b + 0x200) & 0x7FF] + lut[(uVar7b + 0x200) & 0x7FF] * 2) * 2;
                                *(int *)(base + 0x18) = (lut[uVar3] * 0x14 >> 5) + ent[4];
                                *(int *)(base + 0x1C) = (lut[(uVar3 + 0x200) & 0x7FF] * 0x14 >> 5) + ent[5];
                                *(int *)(base + 0x04) = *(int *)(base + 0x00);
                                *(int *)(base + 0x0C) = *(int *)(base + 0x08);
                                *(int *)(base + 0x10) = 0;
                                *(int *)(base + 0x14) = 0;
                                *(unsigned char *)(base + 0x21) = 0x67;
                                *(short *)(base + 0x24) = (short)(rand() % 6);
                                *(unsigned char *)(base + 0x20) = 0;
                                *(unsigned char *)(base + 0x26) = 0xFE;
                                *(unsigned char *)(base + 0x22) = (unsigned char)player_idx;
                                *(int *)(base + 0x28) = 0;
                                *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0xD830);
                                *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0xD7A8);
                                *(int *)(base + 0x3C) = 0;
                                *(unsigned char *)(base + 0x40) = 0;
                                *(int *)(base + 0x44) = 0x2800;
                                *(int *)(base + 0x48) = 0;
                                *(unsigned char *)(base + 0x54) = 0;
                                *(unsigned char *)(base + 0x5C) = 1;
                                *(unsigned char *)(base + 0x64) = 0x72;
                                *(unsigned char *)(base + 0x65) = 0x7F;
                                *(int *)(base + 0x4C) = (int)*(unsigned short *)((int)DAT_00487aa8 + 0x7F * 2) + 30000;
                                DAT_00489248++;
                            }
                        }
                    } else {
                        /* Types 1 & 2: Simple fire particles in DAT_00481f34 (stride 0x20).
                         * Type 1 uses sprite 5 or 6, Type 2 uses sprite 3 or 4. */
                        if (DAT_00489250 < 2000) {
                            unsigned int rev = (heading - 0x400) & 0x7FF;
                            int r = rand();
                            unsigned int spread = (r % 0xA0 + 0x3B0 + heading) & 0x7FF;

                            int *part = (int *)((int)DAT_00481f34 + DAT_00489250 * 0x20);

                            /* Position: behind the ship */
                            part[0] = ent[0] + lut[rev] * 8;
                            part[1] = ent[1] + lut[(rev + 0x200) & 0x7FF] * 8;

                            /* Velocity: ship velocity + random backwards spread */
                            part[2] = (lut[spread] * 0x14 >> 5) + ent[4];
                            part[3] = (lut[(spread + 0x200) & 0x7FF] * 0x14 >> 5) + ent[5];

                            /* Sprite type depends on exhaust_type:
                             * type 1 → sprite 5 or 6, type 2 → sprite 3 or 4 */
                            int r2 = rand();
                            char spr_type;
                            if (exhaust_type == '\x01') {
                                spr_type = (char)((r2 & 1) + 5);
                            } else {
                                spr_type = (char)((r2 & 1) + 3);
                            }
                            *(char *)((int)part + 0x10) = spr_type;
                            *(unsigned char *)((int)part + 0x11) = 4;     /* start frame */
                            *(unsigned char *)((int)part + 0x12) = 0;     /* sub-frame */
                            *(unsigned char *)((int)part + 0x13) = 0xC5;  /* fire behavior */
                            *(unsigned char *)((int)part + 0x14) = 0xFF;  /* no owner */
                            *(unsigned char *)((int)part + 0x15) = 0;     /* color palette 0 */
                            DAT_00489250++;
                        }
                    }
                }
            }
        }
    }
}

/* ===== FUN_0044e510 — Boundary_Clamp ===== */
/* Prevents entity from going off the map. Resets position to edge and
 * zeroes velocity if out of bounds. */
void FUN_0044e510(int *ent)
{
    /* Left boundary */
    if (ent[0] < 0) {
        ent[0] = 0xC0000;    /* 3 tiles in */
        ent[1] = ent[3];     /* restore prev_y */
        ent[4] = 0;          /* zero vel */
        ent[5] = 0;
    }
    /* Right boundary */
    if (ent[0] >= (int)(DAT_004879f0 * 0x40000)) {
        ent[0] = ((int)DAT_004879f0 - 3) * 0x40000;
        ent[4] = 0;
        ent[1] = ent[3];
        ent[5] = 0;
    }
    /* Top boundary */
    if (ent[1] < 0) {
        ent[1] = 0xC0000;
        ent[0] = ent[2];     /* restore prev_x */
        ent[4] = 0;
        ent[5] = 0;
    }
    /* Bottom boundary */
    if (ent[1] >= (int)(DAT_004879f4 * 0x40000)) {
        ent[1] = ((int)DAT_004879f4 - 3) * 0x40000;
        ent[4] = 0;
        ent[0] = ent[2];
        ent[5] = 0;
    }
}

/* ===== FUN_0044e1c0 — Position_Integration ===== */
/* Integrates position from velocity, applies gravity, tile-based forces,
 * velocity drag, and speed cap. */
static void FUN_0044e1c0(int *ent)
{
    /* Position integration */
    ent[0] += ent[4];   /* pos_x += vel_x */
    ent[1] += ent[5];   /* pos_y += vel_y */

    /* Gravity */
    ent[5] += DAT_00483824;

    /* Boundary clamp */
    FUN_0044e510(ent);

    /* Read tile at current position */
    int tx = ent[0] >> 0x12;
    int ty = ent[1] >> 0x12;
    unsigned char tile = *(unsigned char *)((int)DAT_0048782c +
        (ty << ((unsigned char)DAT_00487a18 & 0x1F)) + tx);

    /* Tile-based conveyor/force effects */
    /* Weak conveyors (tiles 0x40-0x43, ranges 0x64-0x73) */
    if (tile == 0x40 || (tile >= 99 && tile < 0x68))
        ent[5] -= 0x2800;   /* push up */
    if (tile == 0x41 || (tile > 0x67 && tile < 0x6C))
        ent[5] += 0x2800;   /* push down */
    if (tile == 0x42 || (tile > 0x6B && tile < 0x70))
        ent[4] -= 0x2800;   /* push left */
    if (tile == 0x43 || (tile > 0x6F && tile < 0x74))
        ent[4] += 0x2800;   /* push right */

    /* Strong conveyors (tiles 0x44-0x47) */
    if (tile == 0x44)      ent[5] -= 0x7800;
    else if (tile == 0x45) ent[5] += 0x7800;
    else if (tile == 0x46) ent[4] -= 0x7800;
    else if (tile == 0x47) ent[4] += 0x7800;
    /* Very strong conveyors (tiles 0x16-0x19) */
    else if (tile == 0x16) ent[5] -= 0x8C00;
    else if (tile == 0x17) ent[5] += 0x8C00;
    else if (tile == 0x18) ent[4] -= 0x8C00;
    else if (tile == 0x19) ent[4] += 0x8C00;

    /* Apply velocity drag — config-based formula from original (0x0044e2c2):
     * drag_int = 1000 - ((byte)g_ConfigBlob[0x1A10] * (byte)g_ConfigBlob[0x17F1]) / 10
     * drag = drag_int * 0.001
     * Typical values: drag ≈ 0.99-0.997 (lose 0.3-1% per tick) */
    {
        int drag_product = (unsigned char)g_ConfigBlob[0x1A10] * (unsigned char)g_ConfigBlob[0x17F1];
        int drag_int = 1000 - drag_product / 10;
        double drag = (double)drag_int * 0.001;
        ent[4] = (int)((double)ent[4] * drag);
        ent[5] = (int)((double)ent[5] * drag);
    }

    /* Speed cap */
    int vx_scaled = ent[4] >> 8;
    int vy_scaled = ent[5] >> 8;
    int max_speed;

    /* Type 0x0C entities have lower speed cap */
    if (ent[0x29] != 0 &&
        *(char *)((char)ent[0xD] + 0x3C + (int)ent) == '\x0C') {
        max_speed = 0x118;
    } else {
        max_speed = 0x157C;
    }

    int speed_sq = vx_scaled * vx_scaled + vy_scaled * vy_scaled;
    int max_sq = max_speed * max_speed;

    if (speed_sq > max_sq && speed_sq > 0) {
        /* Normalize velocity to max_speed */
        double speed = sqrt((double)speed_sq);
        double scale = (double)max_speed / speed;
        ent[4] = (int)((double)vx_scaled * scale) << 8;
        ent[5] = (int)((double)vy_scaled * scale) << 8;
    }
}

/* ===== FUN_00450630 — Wall_Collision ===== */
/* Simplified wall collision: checks sprite footprint against tilemap.
 * If overlapping solid tiles, restores previous position and reflects velocity.
 * Full version uses per-pixel sprite collision mask at sprite 0x19B (411). */
static void FUN_00450630(int *ent, int player_idx)
{
    /* Get entity position in tile coordinates */
    int tx = ent[0] >> 0x12;
    int ty = ent[1] >> 0x12;
    int shift = (unsigned char)DAT_00487a18 & 0x1F;

    /* Check a 3x3 grid around entity center */
    int solid_count = 0;
    int push_x = 0, push_y = 0;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = tx + dx;
            int cy = ty + dy;
            if (cx < 0 || cy < 0 || cx >= (int)DAT_004879f0 || cy >= (int)DAT_004879f4)
                continue;

            int tile_idx = (cy << shift) + cx;
            unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_idx);
            char *tile_props = (char *)((int)DAT_00487928 + (unsigned int)tile * 0x20);

            /* Check passability flag at tile_table+3 */
            if (tile_props[3] == '\0') {
                solid_count++;
                push_x += dx;
                push_y += dy;
            }
        }
    }

    if (solid_count > 3) {
        /* Heavy collision: restore position, reflect and dampen velocity */
        unsigned int abs_vx = (ent[4] < 0) ? -ent[4] : ent[4];
        unsigned int abs_vy = (ent[5] < 0) ? -ent[5] : ent[5];

        if ((int)(abs_vx + abs_vy) < 0x186A1) {
            /* Low speed: just stop */
            ent[4] = 0;
            ent[5] = 0;
        } else {
            /* Reflect velocity and dampen */
            ent[4] = -(ent[4] >> 1);
            ent[5] = -(ent[5] >> 1);

            /* Impact damage based on speed */
            int impact = (int)(abs_vx + abs_vy) >> 0xD;
            if ((int)(abs_vx + abs_vy) > 0x45880 && (char)ent[9] == '\0') {
                ent[8] -= (int)(abs_vx + abs_vy);
                DAT_00486be8[player_idx] += impact;
                *(char *)((int)ent + 0xA3) = 1;
                *(char *)(ent + 0x31) = 3;
            }
        }
        /* Restore to previous position */
        ent[0] = ent[2];
        ent[1] = ent[3];
    }
    else if (solid_count > 0 && (char)ent[9] != '\x01') {
        /* Light collision: restore position, dampen velocity */
        ent[4] = (int)((double)ent[4] * -0.5);
        ent[5] = (int)((double)ent[5] * -0.5);
        ent[0] = ent[2];
        ent[1] = ent[3];
    }
}

/* ===== FUN_0044b0b0 — Entity_Behavior_Loop ===== */
/* Master per-tick entity update. Processes all players/ships:
 * saves positions, reads input, runs movement pipeline, handles
 * combat, pickups, death, and respawn. */
void FUN_0044b0b0(void)
{
    int i;
    unsigned int *ent;
    unsigned int buttons;

    /* Pre-pass: save previous positions for all entities */
    FUN_0044bb70();

    /* Read keyboard and map to button flags for human players */
    if (g_ProcessInput != 0) {
        FUN_0044b990();
    }

    /* Main entity loop */
    for (i = 0; i < DAT_00489240; i++) {
        ent = (unsigned int *)(i * 0x598 + DAT_00487810);

        /* Tick cooldown timers */
        FUN_0044bbb0((int)ent);

        /* Update positional sound */
        FUN_0040fb70_impl(i);

        /* AI decision-making for non-human entities */
        if (*(char *)((int)ent + 0xDD) != '\0') {
            FUN_0044ad30((int *)ent, i);
        }

        /* ---- DEAD entity handling ---- */
        if ((char)ent[9] == '\x01') {
            if (*(char *)((int)ent + 0x47D) == '\0') {
                /* Respawn freeze expired — allow spectator camera */
                if ((char)ent[0x120] != '\0') {
                    FUN_0044bd50((int *)ent);
                }
            } else {
                /* Decrement respawn freeze timer */
                (*(char *)((int)ent + 0x47D))--;
            }
        }
        /* ---- ALIVE entity handling ---- */
        else {
            /* Accumulate distance traveled (every 8th tick) */
            if (DAT_00489288 == '\0') {
                unsigned int abs_vx = (ent[4] < 0x80000000u) ? ent[4] : -(int)ent[4];
                unsigned int abs_vy = (ent[5] < 0x80000000u) ? ent[5] : -(int)ent[5];
                DAT_00486fa8[i] += (int)(abs_vy + abs_vx) >> 0x11;
            }

            /* Detect nearby threats */
            if (ent[0x36] == 0 && ent[0x35] == 0) {
                FUN_0044be20((int *)ent);
            }

            /* Random heading jitter during lock-on (ent[0x34] > 0) */
            if ((int)ent[0x34] > 0) {
                int rnd = rand() & 1;
                ent[6] = (ent[6] + rnd * 0x80 - 0x40) & 0x7FF;
            }

            /* Random heading jitter during evasion (ent[0x36] > 0) */
            if ((int)ent[0x36] > 0) {
                int rnd = rand() & 1;
                ent[6] = (ent[6] - 0x0D + rnd * 0x1A) & 0x7FF;
            }

            /* Overheating: increment heat when firing while moving */
            buttons = ent[0x2E];  /* +0xB8 */
            if ((buttons & 1) && (buttons & 2)) {
                *(char *)(ent + 0x37) = (char)ent[0x37] + 3;
            }

            /* Thrust application (if conditions met) */
            int can_thrust = 0;
            if ((*(char *)((int)ent + 0x9E) == '\0' || *(char *)((int)ent + 0xC5) != '\0' ||
                 ent[0x34] != 0) &&
                ((buttons & 4) || ent[0x34] != 0 || *(char *)((int)ent + 0xC5) != '\0') &&
                *(char *)((int)ent + 0xC6) == '\0') {
                FUN_0044bfa0((int *)ent, i);
                *(char *)(ent + 0x129) = 1;  /* +0x4A4: thrusting flag */
                can_thrust = 1;
            }
            if (!can_thrust) {
                *(char *)(ent + 0x129) = 0;
            }

            /* Heading rotation and weapon controls */
            if (*(char *)((int)ent + 0x9E) == '\0' && *(char *)((int)ent + 0xC6) == '\0') {
                /* Heading rotation */
                if (*(char *)((int)ent + 0x47E) == '\0') {
                    FUN_0044bf20((int)ent, i);
                }

                /* Brake */
                if (buttons & 0x40) {
                    FUN_0044c9a0((int *)ent);
                }

                /* Fire primary weapon */
                if ((buttons & 0x08) && ent[0x24] == 0 &&
                    (ent[0x26] > 0x6F || ent[0x26] == (unsigned int)DAT_00483830) &&
                    *(char *)((int)ent + 0x1D) == '\0' &&
                    DAT_00489248 < 0xA28 &&
                    *(char *)((int)ent + 0xA2) == '\0') {
                    FUN_0044ca40_impl((int *)ent, i);
                }

                /* Auto-release fire */
                if (!(buttons & 0x08) || DAT_00483747 == '\x01') {
                    *(char *)((int)ent + 0x1D) = 0;
                }

                /* Fire secondary weapon (stub) */
                if ((buttons & 0x10) && *(char *)((int)ent + 0xA2) == '\0' && ent[0x25] == 0) {
                    FUN_0044d860_impl((int *)ent, i);
                }
            }

            /* Detonate owned projectiles (stub) */
            FUN_0044d650_impl((int *)ent, i);

            /* Boundary clamp */
            FUN_0044e510((int *)ent);

            /* Entity type-specific behaviors (stubs) */
            if (ent[0x29] != 0) {
                /* Various entity-type checks — all stubbed for now */
            }

            /* Hazard damage at low health */
            if ((double)*(int *)(i * 0x598 + 0x20 + DAT_00487810) /
                (double)*(int *)(i * 0x40 + 0x28 + (int)DAT_0048780c) < 0.5) {
                int vp_x = ent[0] >> 0x16;
                int vp_y = ent[1] >> 0x16;
                if (vp_x >= 0 && vp_y >= 0 &&
                    vp_x < (int)DAT_004879f0 && vp_y < (int)DAT_004879f4) {
                    unsigned char grid_byte = *(unsigned char *)((int)DAT_00487814 +
                        vp_x + vp_y * DAT_004879f8);
                    if (grid_byte & 0x08) {
                        FUN_0044e5a0_impl((int *)ent);
                    }
                }
            }
        }

        /* Detonation mode check */
        if (DAT_00483745 == '\x01' && ent[0xC] != 0xFFFFFFFF) {
            if (ent[0xC] == 0 || (ent[0x2E] & 0x20)) {
                if ((char)ent[9] == '\x01') {
                    *(char *)((int)ent + 0x25) = 1;  /* trigger respawn */
                }
            }
        } else if ((ent[0x2E] & 0x20) || ent[0xC] == 0) {
            if ((char)ent[9] == '\x01') {
                *(char *)((int)ent + 0x25) = 1;
            }
        }

        /* Energy regeneration */
        unsigned int energy = ent[0x26] + ((unsigned int)DAT_00483750 & 0xFF);
        ent[0x26] = energy;
        if ((unsigned int)DAT_00483830 < energy) {
            ent[0x26] = (unsigned int)DAT_00483830;
        }
        if ((unsigned char)(*((char *)&DAT_0048374c + 3)) > 0xF0) {
            ent[0x26] = (unsigned int)DAT_00483830;
        }

        /* Overheat penalty */
        if ((unsigned char)ent[0x37] > 100) {
            FUN_0044e950_impl((int *)ent, i);
            *(char *)(ent + 0x37) = 0;
        }

        /* Trigger respawn if flagged */
        if (*(char *)((int)ent + 0x25) == '\x01') {
            FUN_0044dea0_impl((int *)ent, i);
        }

        /* Physics integration (only if alive flag +0x47C set) */
        if (*(char *)((int)ent + 0x47C) == '\x01') {
            FUN_0044e1c0((int *)ent);

            /* Force field effects (every 8th tick) */
            if (DAT_00489288 == '\0') {
                /* FUN_0044e3b0 stub — force field attraction/repulsion */
            }

            /* Teleporter check (stub) */
            /* FUN_0044e7a0 stub */
        }

        /* Read tile at current position */
        int tx = ent[0] >> 0x12;
        int ty = ent[1] >> 0x12;
        int shift = (unsigned char)DAT_00487a18 & 0x1F;
        unsigned short tile_val = *(unsigned char *)((int)DAT_0048782c + (ty << shift) + tx);

        /* Tile pickup interaction (stub) */

        /* Check if tile is walkable — clear underwater flag if so */
        if (*(char *)((int)DAT_00487928 + 0x18 + (unsigned int)tile_val * 0x20) == '\0' &&
            (char)ent[0x28] == '\0') {
            *(char *)((int)ent + 0x9E) = 0;
        }

        /* Wall collision (only if alive) */
        if (*(char *)((int)ent + 0x47C) == '\x01') {
            FUN_00450630((int *)ent, i);
        }

        /* Lava/pit death check */
        if (ent[8] < 1 || ent[1] < 0x80001 ||
            (int)ent[1] >= (int)((DAT_004879f4 - 2) * 0x40000)) {
            if ((char)ent[9] == '\0') {
                if (tile_val == 4 || tile_val == 0x15) {
                    FUN_00450080_impl((int *)ent, (tile_val == 4) ? '\0' : '\x01');
                }
            }
        }

        /* Lava damage zone check */
        int lava_result = FUN_00450f10_impl(ent[0], ent[1]);
        if ((char)lava_result != '\0') {
            ent[8] -= 0x19000;
            DAT_00486be8[i] += 0x0C;
        }

        /* Pickup collision check (stub) */
        int pickup = FUN_00450ce0_impl(ent[0], ent[1]);
        if ((char)pickup != '\0' && *(char *)((int)ent + 0x47C) == '\x01') {
            FUN_00451010_impl(ent, (char)pickup, i);
        }

        /* Death check */
        if ((int)ent[8] < 1) {
            if ((char)ent[9] == '\0') {
                FUN_00451590_impl((int *)ent, i);
            }
            if (ent[8] != 0) {
                /* Dead but health nonzero: force to zero */
            }
        } else {
            if ((char)ent[9] == '\x01') {
                ent[8] = 0;
            }
        }

        /* Update underwater tracking flag */
        *(char *)((int)ent + 0x49D) =
            *(unsigned char *)((unsigned int)tile_val * 0x20 + 4 + (int)DAT_00487928);

        /* Underwater counter */
        if (*(char *)((unsigned int)tile_val * 0x20 + 4 + (int)DAT_00487928) == '\x01') {
            char val = *(char *)((int)ent + 0x49F) + 4;
            if (val > 0x3F) val = 0x3F;
            *(char *)((int)ent + 0x49F) = val;
        } else {
            char val = *(char *)((int)ent + 0x49F) - 4;
            if (val < 0) val = 0;
            *(char *)((int)ent + 0x49F) = val;
        }
    }
}
