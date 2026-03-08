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
int  DAT_00486944[4]  = {0};   /* per-team stat counters A */
int  DAT_00486954[4]  = {0};   /* per-team stat counters B */
int  DAT_00486964     = 0;     /* team stat counter total */
int  DAT_00486968[80] = {0};   /* per-player kills stat array */
int  DAT_00486aa8[80] = {0};   /* per-player deaths stat array */
int  DAT_00486be8[80] = {0};   /* per-player damage received stats */
int  DAT_00486d28[80] = {0};   /* per-player building stats */
int  DAT_00486e68[80] = {0};   /* per-player damage dealt stats */
int  DAT_00486fa8[80] = {0};   /* per-player distance traveled */
int  DAT_004870e8[80] = {0};   /* per-player explosion stats */
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
/* DAT_00487880 == g_PhysicsParams (defined in memory.cpp) */
char DAT_0048373b = '\0';       /* shared lives mode flag */
char DAT_00483744 = '\0';       /* respawn delay mode */

/* ===== AI / Pathfinding Globals ===== */
int   DAT_00481eb4 = 0;         /* pathfinding frontier count A */
int   DAT_00481eb8 = 0;         /* pathfinding frontier count B */
int   DAT_00481ebc = 0;         /* pathfinding frontier count C */
int   DAT_00481ec0 = 0;         /* pathfinding frontier count D */
int   DAT_00481ec4 = 0;         /* AI vision range X */
int   DAT_00481ec8 = 0;         /* AI vision range Y */
int  *DAT_00481ea4 = NULL;      /* pathfinding read buffer A */
int  *DAT_00481e90 = NULL;      /* pathfinding write buffer A */
int  *DAT_00481e9c = NULL;      /* pathfinding read count ptr A */
int  *DAT_00481eac = NULL;      /* pathfinding write count ptr A */
int  *DAT_00481ea8 = NULL;      /* pathfinding read buffer B */
int  *DAT_00481e94 = NULL;      /* pathfinding write buffer B */
int  *DAT_00481ea0 = NULL;      /* pathfinding read count ptr B */
int  *DAT_00481eb0 = NULL;      /* pathfinding write count ptr B */
int   DAT_00481e98 = 0;         /* pathfinding alternation flag */

/* Float/double constants from binary (AI decision making) */
static double _DAT_00475640 = 0.9;         /* health retreat threshold */
static double _DAT_00475680 = 0.6;         /* health aggression threshold */
static float  _DAT_004757a4 = 0.05f;       /* weapon cooldown scale */
static float  _DAT_004757a0 = 500.0f;      /* base fire rate bonus (no health cap) */
static float  _DAT_0047579c = 2500.0f;     /* energy cost scale */
static float  _DAT_00475798 = 60.0f;       /* minimum fire threshold */
static float  _DAT_00475790 = 0.1f;        /* velocity norm scale */
static float  _DAT_0047578c = 0.0009f;     /* velocity micro scale */
static float  _DAT_00475788 = 1.4e-05f;    /* velocity pico scale */
static float  _DAT_00475784 = 7.0e-05f;    /* distance micro scale */
static float  _DAT_00475780 = 10240.0f;    /* conveyor speed A */
static float  _DAT_0047577c = 30720.0f;    /* conveyor speed B */
static float  _DAT_00475778 = 35840.0f;    /* conveyor speed C */
static double _DAT_00475770 = 0.125;        /* LOS distance scale */
static float  _DAT_00475794 = 0.016666668f; /* fire rate norm (1/60) */
static float  _DAT_004757a8 = 0.5f;        /* melee velocity scale */

/* Weapon range data tables (from binary at 0x47ee0c) */
static int DAT_0047ee0c[4] = { 0x20, 0x19, 0x14, 0x12 };  /* base range per weapon level */
static int DAT_0047ee1c[4] = { 0x10, 0x0c, 0x09, 0x05 };  /* range bonus per weapon level */
static int DAT_0047ee2c[2] = { 0x02, 0x00 };               /* range minimum */
static int DAT_0047ee34[4] = { 0x07, 0x0a, 0x12, 0x28 };   /* step count per weapon level */
static float DAT_0047ee48[4] = { 0.5f, 0.6f, 0.9f, 1.0f }; /* weapon accuracy per level */

/* ===== FUN_0044f630 — Wall Segment Ripple Displacement (0044F630) ===== */
/* Finds nearest wall segment to impact point, then applies a sine-wave
 * displacement across nearby segments to create a ripple/wobble effect.
 * Called on wall bounce (FUN_0044f840) and particle-wall collision. */
static const double _DAT_004757d0 = 0.00005;  /* ripple intensity scale */

void FUN_0044f630(int x, int y, int velX, int velY, float scale,
                  int maxDist, int spread, char direction)
{
    int nearest = 0;
    int bestDist = maxDist;
    int tileX = x >> 0x12;
    int tileY = y >> 0x12;

    /* Phase 1: Find nearest wall segment (skip segment 0) */
    if (DAT_004892c8 <= 1) return;

    int *seg = (int *)((int)DAT_00487820 + 0x1c);  /* start at segment[1] */
    for (int i = 1; i < DAT_004892c8; i++) {
        int dy = tileY - seg[1];
        int dx = tileX - seg[0];
        int absDy = dy < 0 ? -dy : dy;
        int absDx = dx < 0 ? -dx : dx;
        int dist = absDy + absDx;
        if (dist < bestDist) {
            nearest = i;
            bestDist = dist;
        }
        seg = (int *)((char *)seg + 0x1c);
    }

    if (nearest == 0) return;

    /* Phase 2: Compute ripple intensity from impact velocity */
    int vxDiv4 = velX / 4;   /* rounding toward zero (matches CDQ+AND+ADD+SAR) */
    int absVx = vxDiv4 < 0 ? -vxDiv4 : vxDiv4;
    int absVy = velY < 0 ? -velY : velY;
    int speedFactor = (absVx + absVy) >> 9;
    float intensity = (float)((float)speedFactor * scale * (float)_DAT_004757d0 * (float)spread);

    /* Phase 3: Compute sine table step (byte offset per iteration) */
    int step = 0x800 / (spread * 4);
    int sinStepBytes = step * 4;

    /* Phase 4: Apply ripple across segment range [nearest-spread+1, nearest+spread-1] */
    int relOff = 1 - spread;
    if (relOff >= spread) return;

    int sinOffset = 0;
    int segIdx = nearest + relOff;
    int segByteOff = segIdx * 0x1c;

    while (relOff < spread) {
        if (segIdx >= 0 && segIdx < DAT_004892c8) {
            /* Check segment is close enough to the nearest on the main axis */
            int nearX = *(int *)((int)DAT_00487820 + nearest * 0x1c);
            int segX = *(int *)((int)DAT_00487820 + segByteOff);
            int xDiff = segX - nearX;
            int absXDiff = xDiff < 0 ? -xDiff : xDiff;
            int absRel = relOff < 0 ? -relOff : relOff;

            if (absXDiff <= absRel) {
                int sinVal = *(int *)((char *)DAT_00487ab0 + sinOffset);
                float disp = (float)sinVal * intensity;

                if (DAT_004892cc == -1) {
                    /* Vertical wall: displace field2 (offset +8) */
                    int *target = (int *)((int)DAT_00487820 + segByteOff + 8);
                    if (direction != 0)
                        *target = (int)((float)*target - disp);
                    else
                        *target = (int)(disp + (float)*target);
                } else {
                    /* Horizontal wall: displace field3 (offset +0xC) */
                    int *target = (int *)((int)DAT_00487820 + segByteOff + 0xc);
                    if (direction != 0)
                        *target = (int)((float)*target - disp);
                    else
                        *target = (int)(disp + (float)*target);
                }
            }
        }

        sinOffset += sinStepBytes;
        relOff++;
        segByteOff += 0x1c;
        segIdx++;
    }
}

/* ===== FUN_0040fd70 — Positional Sound with Entity Dedup (0040FD70) ===== */
/* Manages per-entity sound state: if same sound is already playing, reset timer.
 * If different sound, stop old one first. Uses spatial audio like FUN_0040f9b0.
 * param_1: entity index (as float, cast to int), param_2: sound ID,
 * param_3: world x, param_4: world y */
void FUN_0040fd70(int entity_idx, int param_2, int param_3, int param_4, int vol_override, int param6)
{
    int iVar6 = entity_idx * 0x598;

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
            /* Volume: table volume scaled by inverse distance
             * Boosted ×4 for modern Windows audio stack */
            int vol_byte = (int)*(unsigned char *)((int)g_SoundTable + param_2 * 8 + 4);
            int vol = (int)((float)vol_byte / minDist) * 8;

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

/* ===== FUN_004483c0 — Find Nearest Walkable Tile (004483C0) ===== */
/* Adjusts coordinates to the nearest walkable shadow-grid cell.
 * Used by A* pathfinding to snap start/end to valid positions. */
static void FUN_004483c0(int *param_1, int *param_2)
{
    int iVar1 = *param_2;
    if (*(char *)((int)DAT_00489ea4 + DAT_00487a04 * iVar1 + *param_1) != '\0')
        return;
    int iVar5 = iVar1 - 1;
    int local_c = -1;
    int iVar3 = iVar5 * DAT_00487a04;
    do {
        if ((0 < iVar5) && (iVar5 < DAT_00487a08)) {
            int iVar4 = -1;
            int iVar2 = *param_1 - 1;
            do {
                if (((0 < iVar2) && (iVar2 < DAT_00487a04)) &&
                   (*(char *)((int)DAT_00489ea4 + iVar3 + iVar2) != '\0')) {
                    *param_2 = iVar1 + local_c;
                    *param_1 = *param_1 + iVar4;
                    return;
                }
                iVar4++;
                iVar2++;
            } while (iVar4 < 2);
        }
        iVar3 += DAT_00487a04;
        local_c++;
        iVar5++;
        if (1 < local_c)
            return;
    } while (true);
}

/* ===== FUN_004495e0 — Dual Ray Wall Check (004495E0) ===== */
/* Fires two rays simultaneously; returns 1 if ray A hits wall first,
 * 2 if ray B hits wall first, 0 if neither hits within 5 steps. */
static int FUN_004495e0(int param_1, int param_2, int param_3, int param_4,
                        int param_5, int param_6)
{
    int iVar1 = param_1;
    int iVar2 = iVar1;
    int iVar3 = param_2;
    int count = 0;
    while (1) {
        iVar1 += param_3;
        param_2 += param_4;
        iVar2 += param_5;
        iVar3 += param_6;
        if ((0 < iVar1) && (0 < param_2) &&
            (iVar1 < (int)DAT_004879f0 * 0x40000) &&
            (param_2 < (int)DAT_004879f4 * 0x40000) &&
            (*(char *)((unsigned int)*(unsigned char *)((iVar1 >> 0x12) +
                       (int)DAT_0048782c +
                       ((param_2 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) *
                       0x20 + 1 + (int)DAT_00487928) == '\0'))
            return 1;
        if ((0 < iVar2) && (0 < iVar3) &&
            (iVar2 < (int)DAT_004879f0 * 0x40000) &&
            (iVar3 < (int)DAT_004879f4 * 0x40000) &&
            (*(char *)((unsigned int)*(unsigned char *)((iVar2 >> 0x12) +
                       (int)DAT_0048782c +
                       ((iVar3 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) *
                       0x20 + 1 + (int)DAT_00487928) == '\0'))
            return 2;
        count++;
        if (4 < count)
            return 0;
    }
}

/* ===== FUN_004494e0 — Forward Trajectory Collision (004494E0) ===== */
/* Checks if the entity's forward trajectory (based on ship speed/angle)
 * will hit an impassable tile within 7 steps. Returns 1 if collision. */
static int FUN_004494e0(int param_1, int param_2, int param_3, int param_4, int param_5)
{
    int iVar3 = param_5;
    int count = 0;
    int iVar2 = (int)DAT_00487810 + iVar3 * 0x598;
    unsigned int uVar4 = (unsigned int)*(unsigned char *)(iVar3 * 0x40 + 0x23 + (int)DAT_0048780c);
    iVar3 = *(int *)(iVar2 + 0x18);
    signed char sVar1 = (signed char)((0 < *(int *)(iVar2 + 0xd4)) + 0xb);
    while (1) {
        param_3 += ((int)(*(int *)((int)DAT_00487ab0 + iVar3 * 4) * (int)uVar4) >> sVar1) * 0x40;
        param_4 += ((int)(*(int *)((int)DAT_00487ab0 + 0x800 + iVar3 * 4) * (int)uVar4) >> sVar1) * 0x40;
        param_1 += param_3;
        param_2 += param_4;
        if ((0 < param_1) && (0 < param_2) &&
            (param_1 < (int)DAT_004879f0 * 0x40000) &&
            (param_2 < (int)DAT_004879f4 * 0x40000) &&
            (*(char *)((unsigned int)*(unsigned char *)((param_1 >> 0x12) +
                       (int)DAT_0048782c +
                       ((param_2 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) *
                       0x20 + 1 + (int)DAT_00487928) == '\0'))
            return 1;
        count++;
        if (6 < count)
            return 0;
    }
}

/* ===== FUN_004497c0 — Dodge/Strafe Decision (004497C0) ===== */
/* Decides whether the AI should strafe left/right or move forward/backward
 * based on the angle difference between facing and velocity direction. */
static void FUN_004497c0(int param_1, char param_2)
{
    unsigned long long uVar6 = (unsigned long long)(unsigned int)FUN_004257e0(
        0, 0, *(int *)(param_1 + 0x10), *(int *)(param_1 + 0x14));
    unsigned int uVar3 = (unsigned int)(*(int *)(param_1 + 0x18) - (int)uVar6) & 0x7ff;
    int iVar4 = 0x800 - (int)uVar3;
    unsigned int uVar2 = (uVar3 - 0x400) & 0x7ff;
    int thresh1, thresh2;
    if (param_2 == '\0') {
        thresh1 = 0x155;
        thresh2 = 0x155;
    } else {
        thresh1 = 0xe3;
        thresh2 = 0xe3;
    }

    if ((uVar3 < (unsigned int)thresh1) || (iVar4 < thresh1)) {
        unsigned int uVar_b = *(unsigned int *)(param_1 + 0xb8);
        unsigned int uVar_s = uVar_b | 0x40;
        *(unsigned int *)(param_1 + 0xb8) = uVar_s;
        if ((uVar_b & 4) != 0)
            *(unsigned int *)(param_1 + 0xb8) = uVar_s ^ 4;
        return;
    }
    if ((uVar2 < (unsigned int)thresh2) || (iVar4 < thresh2)) {
        unsigned int uVar_b = *(unsigned int *)(param_1 + 0xb8);
        unsigned int uVar_s = uVar_b | 4;
        *(unsigned int *)(param_1 + 0xb8) = uVar_s;
        if ((uVar_b & 0x40) != 0)
            *(unsigned int *)(param_1 + 0xb8) = uVar_s ^ 0x40;
    }
}

/* ===== FUN_004496e0 — Line-of-Sight Step Check (004496E0) ===== */
/* Walks from (param_1,param_2) toward (param_3,param_4) in tile-sized steps.
 * Returns 1 (as long long) if LOS is clear, 0 if blocked. */
static long long FUN_004496e0(int param_1, int param_2, int param_3, int param_4, char param_5)
{
    int dx = param_3 - param_1;
    int dy = param_4 - param_2;
    /* Compute step count from distance: sqrt(dx_tile^2 + dy_tile^2) * 0.125 */
    int dx_tile = dx >> 0x12;
    int dy_tile = dy >> 0x12;
    int dist_sq = dx_tile * dx_tile + dy_tile * dy_tile;
    int uVar1 = (int)(sqrt((double)dist_sq) * _DAT_00475770);  /* __ftol */
    if (uVar1 != 0) {
        if (param_5 != '\0') {
            if (0x18 < uVar1) return 0LL;
        } else {
            if (0x10 < uVar1) return 0LL;
        }
        if (0 < uVar1) {
            int step_dx = dx / uVar1;
            int step_dy = dy / uVar1;
            int steps = 0;
            do {
                param_2 += step_dy;
                param_1 += step_dx;
                unsigned int tileOff = (unsigned int)*(unsigned char *)((param_1 >> 0x12) +
                    (int)DAT_0048782c +
                    ((param_2 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20;
                if (*(char *)(tileOff + 1 + (int)DAT_00487928) == '\0')
                    return 0LL;
                steps++;
            } while (steps < uVar1);
        }
    }
    return 1LL;
}

/* ===== FUN_0044de10 — Validate Spawn Position (0044DE10) ===== */
/* Checks a 16x16 tile area is fully passable. Returns 1 if valid. */
static unsigned int FUN_0044de10(int param_1, unsigned int param_2)
{
    if (param_1 <= 0xd || param_1 >= (int)DAT_004879f0 - 0xe ||
        (int)param_2 <= 0xd || (int)param_2 >= (int)DAT_004879f4 - 0xe)
        return 0;
    int iVar3 = 0;
    unsigned int idx = ((param_2 - 8) << ((unsigned char)DAT_00487a18 & 0x1f)) - 8 + param_1;
    do {
        int iVar2 = 0;
        do {
            unsigned int uVar1 = idx++;
            if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + uVar1) *
                0x20 + 1 + (int)DAT_00487928) == '\0')
                return 0;
            iVar2++;
        } while (iVar2 < 0x10);
        iVar3++;
        idx = (idx - 0x10) + DAT_00487a00;
        if (0xf < iVar3)
            return 1;
    } while (1);
}

/* ===== FUN_0044ab20 — Find Nearest Enemy Player (0044AB20) ===== */
/* Scans all players for the nearest enemy within AI vision range.
 * Returns (index << 8) | 1 if found, or count & 0xFFFFFF00 if not. */
static unsigned int FUN_0044ab20(int *param_1)
{
    int iVar5 = (DAT_00481ec8 >> 0x12) * (DAT_00481ec8 >> 0x12);
    unsigned int uVar6 = 0;
    unsigned int local_4 = 0xffffffff;
    unsigned int uVar2 = (unsigned int)DAT_00489240;
    if (0 < DAT_00489240) {
        char *pcVar4 = (char *)((int)DAT_00487810 + 0x24);
        do {
            if ((pcVar4[8] != (char)param_1[0xb]) && (*pcVar4 == '\0')) {
                int iVar3 = (*(int *)(pcVar4 - 0x20) - param_1[1]) >> 0x12;
                int iVar1 = (*(int *)(pcVar4 - 0x24) - *param_1) >> 0x12;
                iVar1 = iVar1 * iVar1 + iVar3 * iVar3;
                if (iVar1 < iVar5) {
                    iVar5 = iVar1;
                    local_4 = uVar6;
                }
            }
            uVar6++;
            pcVar4 += 0x598;
        } while ((int)uVar6 < DAT_00489240);
        uVar2 = local_4;
        if (local_4 != 0xffffffff)
            return (local_4 & 0xffffff00) | 1;
    }
    return uVar2 & 0xffffff00;
}

/* ===== FUN_0044ac80 — Check Wall Above/Below for Evasion (0044AC80) ===== */
/* Checks tiles directly above and below entity for solid walls.
 * Sets movement flags for evasion. Returns 1 if wall found. */
static unsigned int FUN_0044ac80(int *param_1)
{
    unsigned int uVar2 = (unsigned int)param_1[1] >> 0x12;
    unsigned char bVar4 = (unsigned char)DAT_00487a18;
    int iVar5 = *param_1 >> 0x12;

    /* Check tile below (+1 row) */
    if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c +
        ((uVar2 + 1) << (bVar4 & 0x1f)) + iVar5) * 0x20 + 0x18 + (int)DAT_00487928) != '\0') {
        int idx = (uVar2 << (bVar4 & 0x1f)) + iVar5;
        char cVar1 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + idx) *
            0x20 + 4 + (int)DAT_00487928);
        if (cVar1 != '\0') {
            param_1[0x2e] |= 4;
        }
        return 1;
    }
    /* Check tile above (-1 row) */
    if (*(char *)((unsigned int)*(unsigned char *)(((uVar2 - 1) << (bVar4 & 0x1f)) +
        iVar5 + (int)DAT_0048782c) * 0x20 + 0x18 + (int)DAT_00487928) != '\0') {
        int idx = (uVar2 << (bVar4 & 0x1f)) + iVar5;
        char cVar1 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + idx) *
            0x20 + 4 + (int)DAT_00487928);
        if (cVar1 == '\0') {
            param_1[0x2e] |= 4;
        }
        return 1;
    }
    return 0;
}

/* ===== FUN_00449420 — Projectile Trajectory Prediction (00449420) ===== */
/* Predicts if a projectile from entity will hit a solid wall or
 * leave the map. Returns 1 if it will hit a wall. */
static int FUN_00449420(int *param_1)
{
    int iVar2 = *param_1;
    int iVar3 = param_1[1];
    int vx = param_1[4] << 3;
    int vy = param_1[5] << 3;
    int count = 0;
    do {
        iVar2 += vx;
        iVar3 += vy;
        vy += DAT_00483824 * 0x40;
        if ((0 < iVar2) && (0 < iVar3) &&
            (iVar2 < (int)DAT_004879f0 * 0x40000) &&
            (iVar3 < (int)DAT_004879f4 * 0x40000)) {
            unsigned int tileOff = (unsigned int)*(unsigned char *)((iVar2 >> 0x12) +
                (int)DAT_0048782c +
                ((iVar3 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20;
            if (*(char *)(tileOff + 0x18 + (int)DAT_00487928) != '\0')
                return 1;
            if (*(char *)(tileOff + (int)DAT_00487928 + 1) == '\0')
                return 0;
        }
        count++;
        if (0xe < count)
            return 0;
    } while (1);
}

/* ===== FUN_0044aa50 — Find Nearest Waypoint/Ammo Pickup (0044AA50) ===== */
/* Scans edge/waypoint records for nearest one matching entity's team.
 * Sets target position if found. Returns (index << 8) | 1 on success. */
static unsigned int FUN_0044aa50(int *param_1)
{
    int iVar1 = -1;
    int iVar5 = 0;
    int iVar4 = 999999999;
    int local_4 = -1;
    if (0 < DAT_00489254) {
        char *pcVar3 = (char *)((int)DAT_00489e84 + 8);
        do {
            if ((*pcVar3 == (char)param_1[0xb]) || (*pcVar3 == (char)-1)) {
                iVar1 = (*(int *)(pcVar3 - 4) - param_1[1]) >> 0x12;
                int iVar2 = (*(int *)(pcVar3 - 8) - *param_1) >> 0x12;
                iVar2 = iVar2 * iVar2 + iVar1 * iVar1;
                iVar1 = local_4;
                if (iVar2 < iVar4) {
                    iVar1 = iVar5;
                    iVar4 = iVar2;
                    local_4 = iVar5;
                }
            }
            iVar5++;
            pcVar3 += 0x10;
        } while (iVar5 < DAT_00489254);
        if (iVar1 != -1) {
            iVar1 *= 0x10;
            param_1[0x114] = *(int *)(iVar1 + (int)DAT_00489e84);
            int iVar4_2 = *(int *)(iVar1 + 4 + (int)DAT_00489e84);
            param_1[0x116] = 0;
            param_1[0x115] = iVar4_2;
            return (((unsigned int)iVar1) & 0xffffff00) | 1;
        }
    }
    return (unsigned int)DAT_00489254 & 0xffffff00;
}

/* ===== FUN_0044abb0 — Find Nearest Enemy and Set Target (0044ABB0) ===== */
/* Scans players for nearest enemy, sets targeting data on entity. */
static void FUN_0044abb0(int *param_1)
{
    int iVar5 = (DAT_00481ec8 >> 0x12) * (DAT_00481ec8 >> 0x12);
    int iVar1 = -1;
    int iVar4 = 0;
    int local_4 = -1;
    if (0 < DAT_00489240) {
        char *pcVar3 = (char *)((int)DAT_00487810 + 0x24);
        do {
            if ((pcVar3[8] != (char)param_1[0xb]) && (*pcVar3 == '\0')) {
                int iVar2 = (*(int *)(pcVar3 - 0x20) - param_1[1]) >> 0x12;
                iVar1 = (*(int *)(pcVar3 - 0x24) - *param_1) >> 0x12;
                iVar2 = iVar1 * iVar1 + iVar2 * iVar2;
                iVar1 = local_4;
                if (iVar2 < iVar5) {
                    iVar1 = iVar4;
                    iVar5 = iVar2;
                    local_4 = iVar4;
                }
            }
            iVar4++;
            pcVar3 += 0x598;
        } while (iVar4 < DAT_00489240);
        if (iVar1 != -1) {
            param_1[0x10e] = *(int *)(iVar1 * 0x598 + (int)DAT_00487810);
            iVar1 = *(int *)(iVar1 * 0x598 + 4 + (int)DAT_00487810);
            param_1[0x38] = 1;
            param_1[0x10f] = iVar1;
        }
    }
}

/* ===== FUN_00449c50 — Wall Avoidance with Ray Casting (00449C50) ===== */
/* Tests walls at 3 angle pairs around entity facing direction,
 * then checks forward/backward clearance. Calls dodge if stuck. */
static void FUN_00449c50(int *param_1, int param_2, int param_3)
{
    int iVar5 = param_1[1];
    int iVar1 = *param_1;
    unsigned int uVar2, uVar3;
    int iVar4;

    /* Test 1: +/-0x199 from facing */
    uVar2 = (param_1[6] + 0x199U) & 0x7ff;
    uVar3 = (param_1[6] - 0x199U) & 0x7ff;
    iVar4 = FUN_004495e0(iVar1, iVar5,
        *(int *)((int)DAT_00487ab0 + uVar2 * 4) * 3,
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar2 * 4) * 3,
        *(int *)((int)DAT_00487ab0 + uVar3 * 4) * 3,
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar3 * 4) * 3);
    if (iVar4 == 1) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 2;
        param_1[0x2e] = (int)s;
        if ((b & 1) != 0)
            param_1[0x2e] = (int)(s ^ 1);
    } else if (iVar4 == 2) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 1;
        param_1[0x2e] = (int)s;
        if ((b & 2) != 0)
            param_1[0x2e] = (int)(s ^ 2);
    }

    /* Test 2: +/-0x100 from facing */
    uVar3 = (param_1[6] - 0x100U) & 0x7ff;
    uVar2 = (param_1[6] + 0x100U) & 0x7ff;
    iVar4 = FUN_004495e0(iVar1, iVar5,
        *(int *)((int)DAT_00487ab0 + uVar2 * 4),
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar2 * 4),
        *(int *)((int)DAT_00487ab0 + uVar3 * 4),
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar3 * 4));
    if (iVar4 == 1) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 2;
        param_1[0x2e] = (int)s;
        if ((b & 1) != 0)
            param_1[0x2e] = (int)(s ^ 1);
    } else if (iVar4 == 2) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 1;
        param_1[0x2e] = (int)s;
        if ((b & 2) != 0)
            param_1[0x2e] = (int)(s ^ 2);
    }

    /* Test 3: +/-0x300 from facing */
    uVar3 = (param_1[6] - 0x300U) & 0x7ff;
    uVar2 = (param_1[6] + 0x300U) & 0x7ff;
    iVar4 = FUN_004495e0(iVar1, iVar5,
        *(int *)((int)DAT_00487ab0 + uVar2 * 4),
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar2 * 4),
        *(int *)((int)DAT_00487ab0 + uVar3 * 4),
        *(int *)((int)DAT_00487ab0 + 0x800 + uVar3 * 4));
    if (iVar4 == 1) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 2;
        param_1[0x2e] = (int)s;
        if ((b & 1) != 0)
            param_1[0x2e] = (int)(s ^ 1);
    } else if (iVar4 == 2) {
        unsigned int b = param_1[0x2e];
        unsigned int s = b | 1;
        param_1[0x2e] = (int)s;
        if ((b & 2) != 0)
            param_1[0x2e] = (int)(s ^ 2);
    }

    /* Forward clearance check: walk backward (facing - 0x400) 5 steps */
    uVar2 = (unsigned int)param_1[6] & 0x7ff;
    uVar3 = (param_1[6] - 0x400U) & 0x7ff;
    int local_14 = 0;
    int iVar4b = iVar5;
    int iVar6 = iVar1;
    do {
        iVar6 = iVar6 + *(int *)((int)DAT_00487ab0 + uVar3 * 4);
        iVar4b = iVar4b + *(int *)((int)DAT_00487ab0 + 0x800 + uVar3 * 4);
        if ((0 < iVar6) && (0 < iVar4b) &&
            (iVar6 < (int)DAT_004879f0 * 0x40000) &&
            (iVar4b < (int)DAT_004879f4 * 0x40000) &&
            (*(char *)((unsigned int)*(unsigned char *)((iVar6 >> 0x12) +
                (int)DAT_0048782c +
                ((iVar4b >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) *
                0x20 + 1 + (int)DAT_00487928) == '\0'))
            goto LAB_00449f58;
        local_14++;
    } while (local_14 < 5);

    /* Forward clearance check: walk forward 5 steps */
    local_14 = 0;
    iVar4b = iVar1;
    iVar6 = iVar5;
    while (1) {
        iVar4b = iVar4b + *(int *)((int)DAT_00487ab0 + uVar2 * 4);
        iVar6 = iVar6 + *(int *)((int)DAT_00487ab0 + 0x800 + uVar2 * 4);
        if ((0 < iVar4b) && (0 < iVar6) &&
            (iVar4b < (int)DAT_004879f0 * 0x40000) &&
            (iVar6 < (int)DAT_004879f4 * 0x40000) &&
            (*(char *)((unsigned int)*(unsigned char *)((iVar4b >> 0x12) +
                (int)DAT_0048782c +
                ((iVar6 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) *
                0x20 + 1 + (int)DAT_00487928) == '\0'))
            break;
        local_14++;
        if (4 < local_14) {
LAB_00449f58:
            if (20000 < param_2) {
                if ((90000 < param_2) && ((char)param_1[0x39] == '\0')) {
                    FUN_004497c0((int)param_1, '\0');
                }
                if ((120000 < param_2) && ((char)param_1[0x39] == '\0')) {
                    int res = FUN_004494e0(iVar1, iVar5, param_1[4] << 3,
                                           param_1[5] << 3, param_3);
                    if (res != 0) {
                        FUN_004497c0((int)param_1, '\0');
                    }
                }
            }
            return;
        }
    }
    /* Forward clear — set backward flag */
    uVar2 = (unsigned int)param_1[0x2e];
    uVar3 = uVar2 | 0x40;
    param_1[0x2e] = (int)uVar3;
    if ((uVar2 & 4) != 0) {
        param_1[0x2e] = (int)(uVar3 ^ 4);
    }
    goto LAB_00449f58;
}

/* ===== FUN_004498a0 — Pathfinding Movement (004498A0) ===== */
/* Advances along the pathfinding waypoint list, checks terrain tile types
 * for conveyors/speed tiles, and sets movement buttons accordingly.
 * The __ftol() calls compute velocity from entity speed/angle and tile effects. */
static void FUN_004498a0(int *param_1, int param_2)
{
    int *piVar4 = param_1;
    int iVar2 = param_1[0x3d];
    param_1[0x3d] = iVar2 + 1;
    if (2 < iVar2 + 1) {
        iVar2 = param_1[0x3b];
        param_1[0x3d] = 0;
        if (iVar2 == param_1[0x3c] - 1) {
            param_1[0x38] = 1;
        } else {
            long long lVar8 = FUN_004496e0(*param_1, param_1[1],
                param_1[iVar2 * 2 + 0x40] << 0x12,
                param_1[iVar2 * 2 + 0x41] << 0x12,
                (char)param_1[0x39]);
            if ((int)lVar8 != 0) {
                param_1[0x3a] = 0xfc;
                param_1[0x3b] = param_1[0x3b] + 1;
            }
        }
    }

    /* Get current waypoint target position */
    int wp_idx = piVar4[0x3b];
    int wp_x = piVar4[wp_idx * 2 + 0x3e];
    int wp_y = piVar4[wp_idx * 2 + 0x3f];

    int iVar2b = *piVar4;   /* entity x */
    int iVar3 = piVar4[1];  /* entity y */

    /* Compute tile-space delta to waypoint */
    int dx_tile = wp_x - (iVar2b >> 0x12);
    int dy_tile = wp_y - (iVar3 >> 0x12);

    /* Compute velocity from entity speed and angle:
     * vx = cos(angle) * speed, vy = sin(angle) * speed
     * The disassembly shows: ent[4] (vx), ent[5] (vy) are current velocity,
     * then it computes from distance to waypoint using sqrt and scaling. */
    int vx = piVar4[4];
    int vy = piVar4[5];
    /* Normalize velocity to get speed scalar */
    int vx_norm = ((vx + ((unsigned int)(vx >> 0x1f) >> 0x17)) >> 9);
    int vy_norm = ((vy + ((unsigned int)(vy >> 0x1f) >> 0x17)) >> 9);
    int speed_sq = vx_norm * vx_norm + vy_norm * vy_norm;
    float speed_f = sqrtf((float)speed_sq);

    /* Compute projected target position based on current velocity */
    /* dx = wp_x_tile - ent_x_tile, dy = wp_y_tile - ent_y_tile */
    /* The disasm computes: target_x = dx * (tiny_scale * speed) / dist - vx_term */
    /* target_y = dy * (tiny_scale * speed) / dist - vy_term */
    float dist_f = sqrtf((float)(dx_tile * dx_tile + dy_tile * dy_tile));
    float vel_scale = speed_f * _DAT_0047578c;

    int target_x, target_y;
    if (dist_f > 0.0f) {
        target_x = (int)((float)dx_tile * _DAT_00475788 * speed_f / dist_f - (float)vx * _DAT_00475790);
        target_y = (int)((float)dy_tile * _DAT_00475788 * speed_f / dist_f - (float)vy * _DAT_00475790);
    } else {
        target_x = (int)(-(float)vx * _DAT_00475790);
        target_y = (int)(-(float)vy * _DAT_00475790);
    }

    /* Compute combined distance metric */
    int combined = target_x * target_x + target_y * target_y;
    float combined_f = sqrtf((float)combined);
    /* Subtract gravity influence */
    target_y = (int)(combined_f * _DAT_00475784 * (float)target_y - (float)DAT_00483824);

    /* Check terrain tile type for conveyors/speed modifiers */
    unsigned char bVar1 = *(unsigned char *)(((iVar3 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f)) +
        (iVar2b >> 0x12) + (int)DAT_0048782c);

    /* Conveyor tile types 0x40-0x47, 0x64-0x73, 0x16-0x19 modify velocity */
    if (bVar1 == 0x40 || (99 < bVar1 && bVar1 < 0x68)) {
        target_y = (int)(_DAT_00475780 * vel_scale + (float)target_y);
    }
    if (bVar1 == 0x41 || (0x67 < bVar1 && bVar1 < 0x6c)) {
        target_y = (int)((float)target_y - _DAT_00475780 * vel_scale);
    }
    if (bVar1 == 0x42 || (0x6b < bVar1 && bVar1 < 0x70)) {
        target_x = (int)(_DAT_00475780 * vel_scale + (float)target_x);
    }
    if (bVar1 == 0x43 || (0x6f < bVar1 && bVar1 < 0x74)) {
        target_x = (int)((float)target_x - _DAT_00475780 * vel_scale);
    }
    if (bVar1 == 0x44) {
        target_y = (int)(_DAT_0047577c * vel_scale + (float)target_y);
    } else if (bVar1 == 0x45) {
        target_y = (int)((float)target_y - _DAT_0047577c * vel_scale);
    } else if (bVar1 == 0x46) {
        target_x = (int)(_DAT_0047577c * vel_scale + (float)target_x);
    } else if (bVar1 == 0x47) {
        target_x = (int)((float)target_x - _DAT_0047577c * vel_scale);
    } else if (bVar1 == 0x16) {
        target_y = (int)(_DAT_00475778 * vel_scale + (float)target_y);
    } else if (bVar1 == 0x17) {
        target_y = (int)((float)target_y - _DAT_00475778 * vel_scale);
    } else if (bVar1 == 0x18) {
        target_x = (int)(_DAT_00475778 * vel_scale + (float)target_x);
    } else if (bVar1 == 0x19) {
        target_x = (int)((float)target_x - _DAT_00475778 * vel_scale);
    }

    /* Compute angle to adjusted target and decide movement direction */
    unsigned long long uVar9 = (unsigned long long)(unsigned int)FUN_004257e0(0, 0, target_x, target_y);
    unsigned int uVar7 = ((unsigned int)piVar4[6] - (unsigned int)uVar9) & 0x7ff;

    unsigned int uVar6;
    if ((param_2 < 60000) || (uVar7 < 0x2aa) || ((int)(0x800 - uVar7) < 0x2aa)) {
        uVar6 = (unsigned int)piVar4[0x2e] | 4;  /* forward */
    } else {
        if ((0x198 < ((uVar7 - 0x400) & 0x7ff)) && (0x198 < (int)(0x800 - uVar7)))
            goto LAB_00449c28;
        uVar6 = (unsigned int)piVar4[0x2e] | 0x40;  /* backward */
    }
    piVar4[0x2e] = (int)uVar6;

LAB_00449c28:
    if (0x3ff < uVar7) {
        piVar4[0x2e] = piVar4[0x2e] | 1;  /* turn left */
        return;
    }
    piVar4[0x2e] = piVar4[0x2e] | 2;  /* turn right */
}

/* ===== FUN_00449fd0 — Weapon Fire Decision (00449FD0) ===== */
/* Checks if firing the current weapon would hit an enemy using ballistic
 * prediction with gravity. Sets fire button if projectile will intersect. */
static void FUN_00449fd0(int *param_1)
{
    int iVar19 = (int)DAT_00487abc;
    unsigned int uVar13 = (unsigned int)*(unsigned char *)((char)param_1[0xd] + 0x3c + (int)param_1);
    unsigned int uVar16 = (unsigned int)*(unsigned char *)((int)param_1 + 0x35);
    int iVar10 = iVar19 + uVar13 * 0x218;
    char cVar2 = *(char *)(iVar10 + 0x7c);

    if (cVar2 == '\0') return;
    if (cVar2 == '\x02') {
        param_1[0x2e] = param_1[0x2e] | 0x10;
        return;
    }
    if (cVar2 == '\x03') {
        if (param_1[0x116] != 2) return;
        if (*param_1 <= param_1[0x114] - 0x1900000) return;
        if (param_1[0x114] + 0x1900000 <= *param_1) return;
        if (param_1[1] <= param_1[0x115] - 0x1900000) return;
        if (param_1[0x115] + 0x1900000 <= param_1[1]) return;
        param_1[0x2e] = param_1[0x2e] | 0x10;
        return;
    }

    int iVar3 = *param_1;
    int iVar7 = iVar3 + DAT_00481ec4;
    int iVar14 = iVar3 - DAT_00481ec4;
    int iVar4 = param_1[1];
    int iVar8 = iVar4 + DAT_00481ec8;
    int iVar15 = iVar4 - DAT_00481ec8;

    /* Weapon parameters from level/type tables */
    unsigned int weap_level = (unsigned int)*(unsigned char *)((int)param_1 + 0xdd);
    int local_5c = DAT_0047ee1c[weap_level];
    int iVar20 = (int)weap_level - 1;
    int local_64 = DAT_0047ee0c[iVar20];
    int iVar5 = DAT_0047ee34[iVar20];
    float fVarAcc = DAT_0047ee48[iVar20];

    int iVar1b = uVar16 + uVar13 * 0x86;

    /* Compute fire rate from weapon stats */
    float fVar6 = (float)((unsigned int)*(unsigned char *)(iVar10 + 0xa0 + uVar16) *
                  (unsigned int)*(unsigned char *)((int)&DAT_00483750 + 2)) * _DAT_004757a4 +
                  (float)(*(int *)(iVar19 + 0xdc + iVar1b * 4) *
                  (unsigned int)*(unsigned char *)(param_1 + 0x27) *
                  DAT_0048382c >> 0xc);
    if (DAT_00483830 == 0) {
        fVar6 = fVar6 + _DAT_004757a0;
    } else if (*(unsigned char *)((int)&DAT_0048374c + 3) < 0xf1) {
        fVar6 = ((float)(unsigned int)(*(int *)(iVar19 + 0x10c + iVar1b * 4) * 7) /
                (float)DAT_00483830) * _DAT_0047579c + fVar6;
    }

    if (fVar6 < _DAT_00475798) {
        /* Scale down range when fire rate too low */
        float scale = (_DAT_00475798 - fVar6);
        local_64 = (int)((float)(0x32 - local_64) * scale * _DAT_00475794 + (float)local_64);
        local_5c = (int)((float)(0x1e - local_5c) * scale * _DAT_00475794 + (float)local_5c);
    }

    /* Compute projectile initial velocity based on weapon type */
    int local_60 = 0;
    int local_68 = 0;
    int facing = param_1[6];
    int proj_speed;

    if (uVar13 == 1) {
        /* Special weapon types with different speed indices */
        if (uVar16 == 0) {
            proj_speed = *(int *)(iVar19 + 0x2cc);
        } else if (uVar16 == 1) {
            proj_speed = *(int *)(iVar19 + 0x2c4);
        } else if (uVar16 == 2) {
            proj_speed = *(int *)(iVar19 + 0x2c8);
        } else {
            goto skip_proj;
        }
    } else {
        int offset = iVar1b;
        proj_speed = *(int *)(iVar19 + 0xac + offset * 4);
    }
    {
        /* Compute X velocity: cos(facing) * speed / 64 + entity_vx * accuracy */
        int cos_val = *(int *)((int)DAT_00487ab0 + facing * 4);
        int sin_val = *(int *)((int)DAT_00487ab0 + 0x800 + facing * 4);
        local_60 = (int)((float)(cos_val * proj_speed >> 6) + (float)param_1[4] * fVarAcc);
        local_68 = (int)((float)(sin_val * proj_speed >> 6) + (float)param_1[5] * fVarAcc);
    }
skip_proj:

    /* Iterate over all players and check if projectile will intersect */
    int local_2c = 0;
    if (0 < DAT_00489240) {
        unsigned char *local_4c = (unsigned char *)((int)DAT_0048780c + 0x23);
        int *piVar17 = (int *)((int)DAT_00487810 + 4);
        do {
            if (((char)param_1[0xb] != (char)piVar17[10]) &&
                (iVar14 < piVar17[-1]) && (piVar17[-1] < iVar7) &&
                (iVar15 < *piVar17) && (*piVar17 < iVar8)) {

                int local_58 = piVar17[-1];
                int local_6c = *piVar17;

                /* Enemy lead prediction: if enemy is alive and weapon level >= 2 */
                int iVar23 = 0;
                int iVar9 = 0;
                if ((char)piVar17[0x128] != '\0' || iVar20 < 2) {
                    iVar23 = 0;
                    iVar9 = 0;
                } else {
                    signed char sVar12 = (signed char)((0 < piVar17[0x34]) + 0xb);
                    iVar23 = ((int)(*(int *)((int)DAT_00487ab0 + piVar17[5] * 4) *
                              (unsigned int)*local_4c) >> sVar12) / 2;
                    iVar9 = ((int)(*(int *)((int)DAT_00487ab0 + 0x800 + piVar17[5] * 4) *
                             (unsigned int)*local_4c) >> sVar12) / 2;
                }

                int local_38 = 0;
                int iVar21 = local_68 * 4;
                int iVar22 = piVar17[4] * 4;
                int local_3c = local_64;
                int iVar18 = piVar17[3] << 2;

                if (0 < iVar5) {
                    int gravity_step = (*(int *)(iVar19 + 0x88 + iVar1b * 4) * DAT_00483828 * 0x10) / 2;
                    iVar23 = (iVar23 << 4) / 2;
                    int iVar10b = ((DAT_00483824 + iVar9) * 0x10) / 2;
                    int local_54 = iVar4;
                    int local_44 = iVar3;
                    do {
                        iVar21 += gravity_step;
                        local_54 += iVar21;
                        local_44 += local_60 * 4;
                        iVar21 += gravity_step;
                        iVar18 += iVar23;
                        local_58 += iVar18;
                        iVar22 += iVar10b;
                        local_6c += iVar22;
                        iVar18 += iVar23;
                        iVar22 += iVar10b;

                        /* Bounds check */
                        if (local_44 < 0 || (int)DAT_004879f0 <= (local_44 >> 0x12) ||
                            local_54 < 0 || (int)DAT_004879f4 <= (local_54 >> 0x12) ||
                            local_58 < 0 || (int)DAT_004879f0 <= (local_58 >> 0x12) ||
                            local_6c < 0 || (int)DAT_004879f4 <= (local_6c >> 0x12))
                            break;

                        /* Check passability at projectile position */
                        if (*(char *)((unsigned int)*(unsigned char *)(
                            ((local_54 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f)) +
                            (local_44 >> 0x12) + (int)DAT_0048782c) * 0x20 + (int)DAT_00487928) == '\0')
                            break;

                        local_3c += local_5c;

                        /* Distance check: projectile vs enemy */
                        int dist_x = local_44 - local_58;
                        if (dist_x < 0) dist_x = local_58 - local_44;
                        int dist_y = local_54 - local_6c;
                        if (dist_y < 0) dist_y = local_6c - local_54;

                        if (dist_y + dist_x < local_3c * 0x40000) {
                            param_1[0x2e] = param_1[0x2e] | 0x10;
                            return;
                        }
                        local_38++;
                    } while (local_38 < iVar5);
                }
            }
            local_2c++;
            local_4c += 0x40;
            piVar17 += 0x166;
        } while (local_2c < DAT_00489240);
    }
}

/* ===== FUN_0044a6b0 — Melee/Close Range Attack Decision (0044A6B0) ===== */
/* Similar to FUN_00449fd0 but for melee range. Checks if entity can
 * hit an enemy at close range with melee weapon (type index 1). */
static void FUN_0044a6b0(int *param_1)
{
    int iVar4 = *param_1;
    int iVar5 = param_1[1];
    int iVar6 = iVar4 - DAT_00481ec4;
    int iVar2 = iVar4 + DAT_00481ec4;
    int iVar7 = iVar5 - DAT_00481ec8;
    int iVar11 = DAT_00481ec8 + iVar5;
    unsigned char bVar3 = *(unsigned char *)((int)param_1 + 0xdd);
    int facing = param_1[6];

    /* Compute melee velocity from weapon type 1 speed (0x2c4 offset) */
    int melee_speed = *(int *)((int)DAT_00487abc + 0x2c4);
    int cos_val = *(int *)((int)DAT_00487ab0 + facing * 4);
    int sin_val = *(int *)((int)DAT_00487ab0 + 0x800 + facing * 4);

    /* __ftol: initial X velocity = cos*speed/64 + entity_vx * 0.5 */
    int lVar18 = (int)((float)(cos_val * melee_speed >> 6) + (float)param_1[4] * _DAT_004757a8);
    /* __ftol: initial Y velocity = sin*speed/64 + entity_vy * 0.5 */
    int lVar19 = (int)((float)(sin_val * melee_speed >> 6) + (float)param_1[5] * _DAT_004757a8);

    int local_34 = 0;
    if (0 < DAT_00489240) {
        unsigned char *local_48 = (unsigned char *)((int)DAT_0048780c + 0x23);
        int *piVar12 = (int *)((int)DAT_00487810 + 4);
        do {
            if (((char)param_1[0xb] != (char)piVar12[10]) &&
                (iVar6 < piVar12[-1]) && (piVar12[-1] < iVar2) &&
                (iVar7 < *piVar12) && (*piVar12 < iVar11)) {

                int local_58 = piVar12[-1];
                int local_50 = *piVar12;

                /* Enemy lead prediction */
                int iVar16 = 0;
                int iVar8 = 0;
                if ((char)piVar12[0x128] != '\0' || ((int)(bVar3 - 1) < 2)) {
                    iVar16 = 0;
                    iVar8 = 0;
                } else {
                    signed char sVar1 = (signed char)((0 < piVar12[0x34]) + 0xb);
                    iVar16 = ((int)(*(int *)((int)DAT_00487ab0 + piVar12[5] * 4) *
                              (unsigned int)*local_48) >> sVar1) / 2;
                    iVar8 = ((int)(*(int *)((int)DAT_00487ab0 + 0x800 + piVar12[5] * 4) *
                             (unsigned int)*local_48) >> sVar1) / 2;
                }

                int iVar14 = lVar19 * 4;
                int iVar15 = piVar12[4] * 4;
                int local_4c = 0x20;
                iVar16 = (iVar16 << 4) / 2;
                int local_38 = 0;
                int iVar13 = piVar12[3] << 2;
                int iVar17 = (*(int *)((int)DAT_00487abc + 0x2a0) * DAT_00483828 * 0x10) / 2;
                iVar8 = ((DAT_00483824 + iVar8) * 0x10) / 2;
                int local_54 = iVar5;
                int local_40 = iVar4;

                do {
                    iVar14 += iVar17;
                    iVar15 += iVar8;
                    local_54 += iVar14;
                    local_40 += lVar18 * 4;
                    iVar14 += iVar17;
                    iVar13 += iVar16;
                    local_58 += iVar13;
                    iVar13 += iVar16;
                    local_50 += iVar15;
                    iVar15 += iVar8;

                    /* Bounds check */
                    if (local_40 < 0 || (int)DAT_004879f0 <= (local_40 >> 0x12) ||
                        local_54 < 0 || (int)DAT_004879f4 <= (local_54 >> 0x12) ||
                        local_58 < 0 || (int)DAT_004879f0 <= (local_58 >> 0x12) ||
                        local_50 < 0 || (int)DAT_004879f4 <= (local_50 >> 0x12))
                        break;

                    /* Check passability */
                    if (*(char *)((unsigned int)*(unsigned char *)(
                        ((local_54 >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f)) +
                        (local_40 >> 0x12) + (int)DAT_0048782c) * 0x20 + (int)DAT_00487928) == '\0')
                        break;

                    local_4c += 4;

                    /* Distance check */
                    int dist_x = local_40 - local_58;
                    if (dist_x < 0) dist_x = local_58 - local_40;
                    int dist_y = local_54 - local_50;
                    if (dist_y < 0) dist_y = local_50 - local_54;

                    if (dist_y + dist_x < local_4c * 0x40000) {
                        param_1[0x2e] = param_1[0x2e] | 8;
                        return;
                    }
                    local_38++;
                } while (local_38 < 0x14);
            }
            local_34++;
            local_48 += 0x40;
            piVar12 += 0x166;
        } while (local_34 < DAT_00489240);
    }
}

/* ===== FUN_00448470 — A* Bidirectional Pathfinding (00448470) ===== */
/* Bidirectional BFS/wave expansion with 8-directional movement.
 * Fills waypoint array in entity for pathfinding movement. */
static int FUN_00448470(int *param_1, int param_2, int param_3)
{
    int local_60[24];
    unsigned char local_70[8];
    int local_68, local_64;
    int local_7c = 0;
    int local_78 = 0;
    int local_74 = 0;
    unsigned int local_80;
    int *local_84;
    int bVar14;
    int bVar15;
    int local_86_flag = 0; /* bool local_86 */
    int iVar10;

    param_1[0x3c] = 0;
    param_1[0x3b] = 0;

    /* 8-directional neighbor offsets */
    local_60[0] = DAT_00487a04;           /* S */
    local_60[1] = DAT_00487a04 + 1;       /* SE */
    local_60[2] = 1;                       /* E */
    local_60[3] = 1 - DAT_00487a04;       /* NE */
    local_60[4] = -DAT_00487a04;           /* N */
    local_60[5] = -1 - DAT_00487a04;      /* NW */
    local_60[6] = -1;                      /* W */
    local_60[7] = DAT_00487a04 - 1;       /* SW */

    /* Reverse direction offsets (index + 8) */
    local_60[8] = 0;    /* dx for dir 0 (S) */
    local_60[9] = 1;
    local_60[10] = 1;
    local_60[11] = 1;
    local_60[12] = 0;
    local_60[13] = -1;
    local_60[14] = -1;
    local_60[15] = -1;
    /* dy offsets (index + 16) */
    local_60[16] = 1;
    local_60[17] = 1;
    local_60[18] = 0;
    local_60[19] = -1;
    local_60[20] = -1;
    local_60[21] = -1;
    local_60[22] = 0;
    local_60[23] = 1;

    /* Direction bitmasks */
    local_70[0] = 1;
    local_70[1] = 2;
    local_70[2] = 4;
    local_70[3] = 8;
    local_70[4] = 0x10;
    local_70[5] = 0x20;
    local_70[6] = 0x40;
    local_70[7] = 0x80;

    unsigned int uVar8 = (unsigned int)DAT_00487a08 * (unsigned int)DAT_00487a04;

    /* Clear visited grid */
    DAT_00481eb4 = 0;
    DAT_00481eb8 = 0;
    DAT_00481ebc = 0;
    DAT_00481ec0 = 0;
    memset((void *)DAT_00489ea8, 0, uVar8);

    /* Initialize pathfinding buffers */
    DAT_00481ea4 = (int *)DAT_00481f40;
    DAT_00481e90 = (int *)DAT_00481f3c;
    DAT_00481e9c = &DAT_00481eb4;
    DAT_00481eac = &DAT_00481eb8;
    DAT_00481ea8 = (int *)DAT_00481f38;
    DAT_00481e94 = (int *)DAT_00481f44;
    DAT_00481ea0 = &DAT_00481ebc;
    DAT_00481eb0 = &DAT_00481ec0;
    DAT_00481e98 = 1;

    /* Convert world coords to shadow-grid coords: (x>>18 + 9) / 18 */
    local_68 = ((*param_1 >> 0x12) + 9) / 0x12;
    param_2 = ((param_2 >> 0x12) + 9) / 0x12;
    local_64 = ((param_1[1] >> 0x12) + 9) / 0x12;
    iVar10 = (param_3 >> 0x12) + 9;
    param_3 = iVar10 / 0x12;

    if ((local_68 == param_2) && (local_64 == param_3)) {
        DAT_00481e98 = 1;
        return 0;
    }

    FUN_004483c0(&local_68, &local_64);
    FUN_004483c0(&param_2, &param_3);

    /* Seed start position into frontier A */
    if (*DAT_00481eac < 1000) {
        DAT_00481e90[*DAT_00481eac * 2] = local_64 * DAT_00487a04 + local_68;
        *(char *)((int)DAT_00481e90 + *DAT_00481eac * 8 + 5) = 99;
        *(char *)(DAT_00481e90 + *DAT_00481eac * 2 + 1) = 1;
        *DAT_00481eac = *DAT_00481eac + 1;
    }

    /* Seed end position into frontier B */
    if (*DAT_00481eb0 < 1000) {
        DAT_00481e94[*DAT_00481eb0 * 2] = DAT_00487a04 * param_3 + param_2;
        *(char *)((int)DAT_00481e94 + *DAT_00481eb0 * 8 + 5) = (char)199;
        *(char *)(DAT_00481e94 + *DAT_00481eb0 * 2 + 1) = 1;
        *DAT_00481eb0 = *DAT_00481eb0 + 1;
    }

    /* Swap buffers based on alternation flag */
    int *puVar5;
    if (DAT_00481e98 != 0) {
        DAT_00481ea4 = (int *)DAT_00481f3c;
        DAT_00481e90 = (int *)DAT_00481f40;
        puVar5 = &DAT_00481eb4;
        DAT_00481e9c = &DAT_00481eb8;
        DAT_00481eac = &DAT_00481eb4;
        DAT_00481ea8 = (int *)DAT_00481f44;
        DAT_00481e94 = (int *)DAT_00481f38;
        DAT_00481ea0 = &DAT_00481ec0;
        DAT_00481eb0 = &DAT_00481ebc;
    } else {
        DAT_00481ea4 = (int *)DAT_00481f40;
        DAT_00481e90 = (int *)DAT_00481f3c;
        puVar5 = &DAT_00481eb8;
        DAT_00481e9c = &DAT_00481eb4;
        DAT_00481eac = &DAT_00481eb8;
        DAT_00481ea8 = (int *)DAT_00481f38;
        DAT_00481e94 = (int *)DAT_00481f44;
        DAT_00481ea0 = &DAT_00481ebc;
        DAT_00481eb0 = &DAT_00481ec0;
    }
    DAT_00481e98 = (DAT_00481e98 == 0) ? 1 : 0;

    local_7c = 0;
    local_78 = 0;
    local_74 = 0;

    /* Main BFS loop */
    do {
        *puVar5 = 0;
        iVar10 = 0;
        int *piVar11 = DAT_00481ea4;

        /* Process frontier A */
        if (0 < *DAT_00481e9c) {
            do {
                local_86_flag = !local_86_flag;
                int iVar7 = *piVar11;
                char cVar4 = (char)(*(char *)(piVar11 + 1)) - 1;
                *(char *)(piVar11 + 1) = cVar4;
                if (cVar4 == '\0') {
                    if (*(char *)(iVar7 + (int)DAT_00489ea8) == '\0') {
                        *(char *)(iVar7 + (int)DAT_00489ea8) = *(char *)((int)piVar11 + 5);
                        unsigned char bVar1 = *(unsigned char *)(iVar7 + (int)DAT_00489ea4);
                        /* Even directions first */
                        int iVar6 = 0;
                        int *piVar13 = local_60;
                        int found_meeting = 0;
                        do {
                            if ((local_70[iVar6] & bVar1) != 0) {
                                unsigned char bVar2 = *(unsigned char *)(*piVar13 + iVar7 + (int)DAT_00489ea8);
                                if (bVar2 == 0) {
                                    if (*DAT_00481eac < 1000) {
                                        DAT_00481e90[*DAT_00481eac * 2] = *piVar13 + iVar7;
                                        *(char *)((int)DAT_00481e90 + *DAT_00481eac * 8 + 5) = (char)(iVar6 + 1);
                                        *(char *)(DAT_00481e90 + *DAT_00481eac * 2 + 1) = 1;
                                        *DAT_00481eac = *DAT_00481eac + 1;
                                    }
                                } else if (99 < bVar2) {
                                    local_7c = local_60[iVar6] + iVar7;
                                    local_74 = 10000;
                                    iVar10 = *DAT_00481e9c + 5;
                                    local_78 = iVar7;
                                    found_meeting = 1;
                                    break;
                                }
                            }
                            iVar6 += 2;
                            piVar13 += 2;
                        } while (iVar6 < 8);

                        /* Odd directions */
                        if (!found_meeting) {
                            iVar6 = 1;
                            piVar13 = local_60 + 1;
                            do {
                                local_86_flag = !local_86_flag;
                                if ((local_70[iVar6] & bVar1) != 0) {
                                    unsigned char bVar2 = *(unsigned char *)(iVar7 + *piVar13 + (int)DAT_00489ea8);
                                    if (bVar2 == 0) {
                                        if (*DAT_00481eac < 1000) {
                                            DAT_00481e90[*DAT_00481eac * 2] = iVar7 + *piVar13;
                                            *(char *)((int)DAT_00481e90 + *DAT_00481eac * 8 + 5) = (char)(iVar6 + 1);
                                            *(char *)(DAT_00481e90 + *DAT_00481eac * 2 + 1) = local_86_flag + 1;
                                            *DAT_00481eac = *DAT_00481eac + 1;
                                        }
                                    } else if (99 < bVar2) {
                                        local_7c = local_60[iVar6] + iVar7;
                                        local_74 = 10000;
                                        iVar10 = *DAT_00481e9c + 5;
                                        local_78 = iVar7;
                                        break;
                                    }
                                }
                                iVar6 += 2;
                                piVar13 += 2;
                            } while (iVar6 < 8);
                        }
                    }
                } else if (*(char *)(iVar7 + (int)DAT_00489ea8) == '\0') {
                    /* Re-queue with decremented timer */
                    if (*DAT_00481eac < 1000) {
                        DAT_00481e90[*DAT_00481eac * 2] = *piVar11;
                        *(char *)((int)DAT_00481e90 + *DAT_00481eac * 8 + 5) = *(char *)((int)piVar11 + 5);
                        *(char *)(DAT_00481e90 + *DAT_00481eac * 2 + 1) = (char)piVar11[1];
                        *DAT_00481eac = *DAT_00481eac + 1;
                    }
                }
                piVar11 += 2;
                iVar10++;
            } while (iVar10 < *DAT_00481e9c);
        }

        if (*DAT_00481eac == 0 || iVar10 != *DAT_00481e9c) break;

        /* Process frontier B */
        iVar10 = 0;
        local_80 = 0;
        *DAT_00481eb0 = 0;
        local_84 = DAT_00481ea8;
        if (0 < *DAT_00481ea0) {
            do {
                local_86_flag = !local_86_flag;
                int iVar10b = *local_84;
                char cVar4 = (char)(*(char *)(local_84 + 1)) - 1;
                *(char *)(local_84 + 1) = cVar4;
                if (cVar4 == '\0') {
                    if (*(char *)(iVar10b + (int)DAT_00489ea8) == '\0') {
                        *(char *)(iVar10b + (int)DAT_00489ea8) = *(char *)((int)local_84 + 5);
                        unsigned char bVar1 = *(unsigned char *)(iVar10b + (int)DAT_00489ea4);
                        /* Even directions */
                        int iVar7 = 0;
                        int *piVar11b = local_60;
                        int found_meeting = 0;
                        do {
                            if ((bVar1 & local_70[iVar7]) != 0) {
                                unsigned char bVar2 = *(unsigned char *)(*piVar11b + iVar10b + (int)DAT_00489ea8);
                                if (bVar2 == 0) {
                                    if (*DAT_00481eb0 < 1000) {
                                        DAT_00481e94[*DAT_00481eb0 * 2] = *piVar11b + iVar10b;
                                        *(char *)((int)DAT_00481e94 + *DAT_00481eb0 * 8 + 5) = (char)(iVar7 + 'e');
                                        *(char *)(DAT_00481e94 + *DAT_00481eb0 * 2 + 1) = 1;
                                        *DAT_00481eb0 = *DAT_00481eb0 + 1;
                                    }
                                } else if (bVar2 < 100) {
                                    local_7c = local_60[iVar7] + iVar10b;
                                    local_80 = *DAT_00481ea0 + 5;
                                    local_74 = 10000;
                                    local_78 = iVar10b;
                                    found_meeting = 1;
                                    break;
                                }
                            }
                            iVar7 += 2;
                            piVar11b += 2;
                        } while (iVar7 < 8);

                        /* Odd directions */
                        if (!found_meeting) {
                            iVar7 = 1;
                            piVar11b = local_60 + 1;
                            do {
                                local_86_flag = !local_86_flag;
                                if ((bVar1 & local_70[iVar7]) != 0) {
                                    unsigned char bVar2 = *(unsigned char *)(*piVar11b + iVar10b + (int)DAT_00489ea8);
                                    if (bVar2 == 0) {
                                        if (*DAT_00481eb0 < 1000) {
                                            DAT_00481e94[*DAT_00481eb0 * 2] = *piVar11b + iVar10b;
                                            *(char *)((int)DAT_00481e94 + *DAT_00481eb0 * 8 + 5) = (char)(iVar7 + 'e');
                                            *(char *)(DAT_00481e94 + *DAT_00481eb0 * 2 + 1) = local_86_flag + 1;
                                            *DAT_00481eb0 = *DAT_00481eb0 + 1;
                                        }
                                    } else if (bVar2 < 100) {
                                        local_7c = local_60[iVar7] + iVar10b;
                                        local_80 = *DAT_00481ea0 + 5;
                                        local_74 = 10000;
                                        local_78 = iVar10b;
                                        break;
                                    }
                                }
                                iVar7 += 2;
                                piVar11b += 2;
                            } while (iVar7 < 8);
                        }
                    }
                } else if (*(char *)(iVar10b + (int)DAT_00489ea8) == '\0') {
                    if (*DAT_00481eb0 < 1000) {
                        DAT_00481e94[*DAT_00481eb0 * 2] = *local_84;
                        *(char *)((int)DAT_00481e94 + *DAT_00481eb0 * 8 + 5) = *(char *)((int)local_84 + 5);
                        *(char *)(DAT_00481e94 + *DAT_00481eb0 * 2 + 1) = (char)local_84[1];
                        *DAT_00481eb0 = *DAT_00481eb0 + 1;
                    }
                }
                local_84 += 2;
                iVar10 = (int)(local_80 + 1);
                local_80 = (unsigned int)iVar10;
            } while (iVar10 < *DAT_00481ea0);
        }

        if (*DAT_00481eb0 == 0 || iVar10 != *DAT_00481ea0) break;

        /* Swap buffers for next iteration */
        if (DAT_00481e98 != 0) {
            DAT_00481ea4 = (int *)DAT_00481f3c;
            DAT_00481e90 = (int *)DAT_00481f40;
            puVar5 = &DAT_00481eb4;
            DAT_00481e9c = &DAT_00481eb8;
            DAT_00481eac = &DAT_00481eb4;
            DAT_00481ea8 = (int *)DAT_00481f44;
            DAT_00481e94 = (int *)DAT_00481f38;
            DAT_00481ea0 = &DAT_00481ec0;
            DAT_00481eb0 = &DAT_00481ebc;
        } else {
            DAT_00481ea4 = (int *)DAT_00481f40;
            DAT_00481e90 = (int *)DAT_00481f3c;
            puVar5 = &DAT_00481eb8;
            DAT_00481e9c = &DAT_00481eb4;
            DAT_00481eac = &DAT_00481eb8;
            DAT_00481ea8 = (int *)DAT_00481f38;
            DAT_00481e94 = (int *)DAT_00481f44;
            DAT_00481ea0 = &DAT_00481ebc;
            DAT_00481eb0 = &DAT_00481ec0;
        }
        DAT_00481e98 = (DAT_00481e98 == 0) ? 1 : 0;
        local_74++;
        if (8999 < local_74) break;
    } while (true);

    /* Trace path from meeting point */
    iVar10 = 0;
    if (local_7c != 0) {
        int iVar7 = local_7c % DAT_00487a04;
        iVar10 = local_7c / DAT_00487a04;
        bVar14 = (99 < (unsigned char)*(char *)(iVar10 * DAT_00487a04 + iVar7 + (int)DAT_00489ea8)) ? 1 : 0;
        if (bVar14) {
            iVar7 = local_78 % DAT_00487a04;
            iVar10 = local_78 / DAT_00487a04;
        }

        /* Trace path from start side */
        unsigned int uVar9 = 0;
        char cVar4 = *(char *)(iVar10 * DAT_00487a04 + iVar7 + (int)DAT_00489ea8);
        while (cVar4 != 'c') {  /* 99 = 'c' */
            unsigned int uVar8b = uVar9 & 0x80000001;
            bVar15 = (uVar8b == 0) ? 1 : 0;
            if ((int)uVar8b < 0) {
                bVar15 = ((uVar8b - 1 | 0xfffffffe) == 0xffffffff) ? 1 : 0;
            }
            if (bVar15) {
                param_1[param_1[0x3c] * 2 + 0x3e] = iVar7 * 0x12;
                param_1[param_1[0x3c] * 2 + 0x3f] = iVar10 * 0x12;
                int iVar12 = param_1[0x3c];
                param_1[0x3c] = iVar12 + 1;
                if (100 < iVar12 + 1) break;
            }
            local_80 = (unsigned int)((unsigned char)(cVar4 + 3) & 7);
            iVar10 = iVar10 + local_60[local_80 + 0x10];
            iVar7 = iVar7 + local_60[local_80 + 8];
            uVar9++;
            cVar4 = *(char *)(iVar10 * DAT_00487a04 + iVar7 + (int)DAT_00489ea8);
        }

        /* Reverse the waypoint array */
        int iVar7b = param_1[0x3c];
        int iVar12 = 0;
        int iVar10b = iVar7b / 2;
        if (0 < iVar10b) {
            int *piVar11 = param_1 + 0x3f;
            do {
                int tmp1 = piVar11[-1];
                piVar11[-1] = param_1[(iVar7b - iVar12) * 2 + 0x3c];
                param_1[(param_1[0x3c] - iVar12) * 2 + 0x3c] = tmp1;
                int tmp2 = *piVar11;
                *piVar11 = param_1[(param_1[0x3c] - iVar12) * 2 + 0x3d];
                param_1[(param_1[0x3c] - iVar12) * 2 + 0x3d] = tmp2;
                iVar12++;
                iVar7b = param_1[0x3c];
                iVar10b = iVar7b / 2;
                piVar11 += 2;
            } while (iVar12 < iVar10b);
        }

        /* Trace path from end side */
        if (param_1[0x3c] < 0x65) {
            if (bVar14) {
                local_78 = local_7c;
            }
            iVar7 = local_78 % DAT_00487a04;
            uVar9 = 0;
            iVar10 = local_78 / DAT_00487a04;
            cVar4 = *(char *)(iVar10 * DAT_00487a04 + iVar7 + (int)DAT_00489ea8);
            while (cVar4 != (char)0xc7) {  /* 199 = 0xC7 */
                unsigned int uVar8b = uVar9 & 0x80000001;
                if ((int)uVar8b < 0) {
                    uVar8b = (uVar8b - 1 | 0xfffffffe) + 1;
                }
                if (uVar8b == 1) {
                    param_1[param_1[0x3c] * 2 + 0x3e] = iVar7 * 0x12;
                    param_1[param_1[0x3c] * 2 + 0x3f] = iVar10 * 0x12;
                    int iVar12b = param_1[0x3c];
                    param_1[0x3c] = iVar12b + 1;
                    if (100 < iVar12b + 1) {
                        return iVar10;
                    }
                }
                local_80 = (unsigned int)((unsigned char)(cVar4 + (char)0x9f) & 7);
                iVar10 = iVar10 + local_60[local_80 + 0x10];
                iVar7 = iVar7 + local_60[local_80 + 8];
                uVar9++;
                cVar4 = *(char *)(iVar10 * DAT_00487a04 + iVar7 + (int)DAT_00489ea8);
            }
        }
    }
    return iVar10;
}

/* ===== FUN_0044ad30 — Main AI Behavior Orchestrator (0044AD30) ===== */
/* Top-level AI function called for each bot entity. Orchestrates:
 * target acquisition, pathfinding, movement, and weapon firing. */
static void FUN_0044ad30(int *param_1, int param_2)
{
    int *piVar2 = param_1;

    DAT_00481ec4 = 0x6400000;
    DAT_00481ec8 = 0x4b00000;

    /* Dead entity: 1% chance to press special button, then return */
    if ((char)param_1[9] != '\0') {
        int iVar5 = rand();
        if (iVar5 % 100 == 0) {
            param_1[0x2e] = param_1[0x2e] | 0x20;
        }
        return;
    }

    /* Find nearest enemy player */
    unsigned int uVar3 = FUN_0044ab20(param_1);

    /* If no enemy found and health is low, try evasion */
    if (((char)uVar3 == '\0') &&
        ((double)param_1[8] / (double)*(int *)(param_2 * 0x40 + 0x28 + (int)DAT_0048780c) < _DAT_00475640)) {
        unsigned int uVar4 = FUN_0044ac80(param_1);
        if ((char)uVar4 != '\0') return;
        unsigned int uVar4b = FUN_00449420(param_1);
        if (uVar4b != 0) return;
    }

    /* If active target is in range, clear enemy tracking */
    if ((param_1[0x116] == 2) && ((char)uVar3 == '\0') &&
        (param_1[0x114] - DAT_00481ec4 < *param_1) &&
        (*param_1 < param_1[0x114] + DAT_00481ec4) &&
        (param_1[0x115] - DAT_00481ec8 < param_1[1]) &&
        (param_1[1] < DAT_00481ec8 + param_1[0x115])) {
        param_1[0x10e] = 0;
    }

    /* Set target destination */
    if (DAT_004892a4 == '\0') {
        if (param_1[0x10e] != 0) {
            param_1[0x114] = param_1[0x10e];
            param_1[0x115] = param_1[0x10f];
            param_1[0x116] = 2;
            goto LAB_0044aecc;
        }
        if ((double)param_1[8] / (double)*(int *)(param_2 * 0x40 + 0x28 + (int)DAT_0048780c) < _DAT_00475680)
            goto LAB_0044ae83;
        param_1[0x114] = param_1[0x117];
        param_1[0x115] = param_1[0x118];
    } else {
LAB_0044ae83:
        uVar3 = FUN_0044aa50(param_1);
        if ((char)uVar3 != '\0')
            goto LAB_0044aecc;
        param_1[0x114] = param_1[0x117];
        param_1[0x115] = param_1[0x118];
    }
    param_1[0x116] = 4;

LAB_0044aecc:
    /* Check if pathfinding needs refresh */
    {
        int iVar5 = param_1[0x3a];
        param_1[0x3a] = iVar5 - 1;
        if ((param_1[0x38] == 1) || (iVar5 - 1 < 0)) {
            /* Pick random valid position */
            int rnd_x = 0, rnd_y = 0;
            int attempts = 0;
            do {
                rnd_x = rand() % (int)DAT_004879f0;
                rnd_y = rand() % (int)DAT_004879f4;
                unsigned int valid = FUN_0044de10(rnd_x, (unsigned int)rnd_y);
                if ((char)valid != '\0') break;
                attempts++;
            } while (attempts < 100);
            piVar2[0x117] = rnd_x << 0x12;
            piVar2[0x118] = rnd_y << 0x12;

            FUN_00448470(piVar2, piVar2[0x114], piVar2[0x115]);
            piVar2[0x3a] = 0xfc;
            piVar2[0x38] = 0;
        }
    }

    /* Update nearest enemy target */
    FUN_0044abb0(piVar2);

    /* Compute speed scalar from velocity */
    int iVar5 = piVar2[4];
    int iVar6 = piVar2[5];
    iVar5 = ((int)(iVar5 + ((unsigned int)(iVar5 >> 0x1f) >> 21)) >> 0xb) * piVar2[4];
    iVar6 = ((int)(iVar6 + ((unsigned int)(iVar6 >> 0x1f) >> 21)) >> 0xb) * piVar2[5];
    iVar5 = ((int)(iVar5 + ((unsigned int)(iVar5 >> 0x1f) >> 21)) >> 0xb) +
            ((int)(iVar6 + ((unsigned int)(iVar6 >> 0x1f) >> 21)) >> 0xb);

    /* Movement decisions */
    FUN_004498a0(piVar2, iVar5);
    FUN_00449c50(piVar2, iVar5, param_2);
    FUN_00449fd0(piVar2);
    FUN_0044a6b0(piVar2);
}
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

    /* Compute sprite/palette index based on weapon type */
    int *sprIdx = NULL;
    if ((char)ent[0x23] == '\0') {
        unsigned int r = rand() & 0x80000003;
        if ((int)r < 0) r = (r - 1 | 0xfffffffc) + 1;
        sprIdx = (int *)(r + 0x5a);
    }
    if ((char)ent[0x23] == '\x01') {
        sprIdx = (int *)(rand() % 5 + 0x66);
    }
    if ((char)ent[0x23] == '\x02') {
        sprIdx = (int *)(rand() % 3 + 0x79);
    }
    if (2 < weapon_type) {
        sprIdx = NULL;
    }


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

        /* Set origin position */
        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x70) = ent[0];
        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x6C) = ent[1];

        /* Set palette color for pixel dot rendering (sprite_idx >= 30000) */
        if (sprIdx != NULL) {
            *(unsigned int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) =
                (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + (int)sprIdx * 2) + 30000;
        }

        /* Weapon type 8 + ship flag 'd': set special damage type */
        if (weapon_type == 8 && *(char *)(idx * 0x40 + 0x33 + (int)DAT_0048780c) == 'd') {
            *(unsigned char *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x60) = 0x19;
        }

        /* Difficulty mode: set energy field */
        if (DAT_004892e5 != '\0') {
            *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x3C) = 0x1d4c000;
        }
    }

    /* Spawn side projectiles (fire mode 1 or 2) */
    if ((*(char *)((int)ent + 0x8D) == '\x01' || *(char *)((int)ent + 0x8D) == '\x02') &&
        DAT_00489248 < 0xA28) {

        int shipStatOff = idx * 0x40;
        int *lut2 = (int *)DAT_00487ab0;

        /* Left side projectile: heading + 0x200 (perpendicular) */
        unsigned int side_angle = (heading + 0x200) & 0x7FF;
        int ship_radius = (int)(unsigned int)*(unsigned char *)(shipStatOff + 0x22 + (int)DAT_0048780c);
        int eoff = DAT_00489248 * 0x80;
        int ebase2 = (int)DAT_004892e8 + eoff;

        *(int *)(ebase2 + 0x00) = (ship_radius * lut2[side_angle]) / 2 + ent[0];
        *(int *)(ebase2 + 0x08) = (lut2[0x200 + side_angle] * ship_radius) / 2 + ent[1];
        *(int *)(ebase2 + 0x18) = (lut2[heading] * speed >> 6) + ent[4];
        *(int *)(ebase2 + 0x1C) = (lut2[(heading + 0x200) & 0x7FF] * speed >> 6) + ent[5];
        *(int *)(ebase2 + 0x04) = *(int *)(ebase2 + 0x00);
        *(int *)(ebase2 + 0x0C) = *(int *)(ebase2 + 0x08);

        *(unsigned short *)(ebase2 + 0x24) = 0;
        *(unsigned char *)(ebase2 + 0x20) = 0;
        *(unsigned char *)(ebase2 + 0x26) = 6;
        *(unsigned char *)(ebase2 + 0x22) = (unsigned char)idx;
        *(int *)(ebase2 + 0x28) = 0;

        if (weapon_type < 6) {
            *(unsigned char *)(ebase2 + 0x21) = 0;
            *(int *)(ebase2 + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + (unsigned int)weapon_type * 4);
            *(int *)(ebase2 + 0x44) = *(int *)((int)DAT_00487abc + 0xC4 + (unsigned int)weapon_type * 4);
            *(int *)(ebase2 + 0x4C) = *(int *)((int)DAT_00487abc + 0xF4 + (unsigned int)weapon_type * 4);
            *(char *)(ebase2 + 0x40) = (char)weapon_type;
        } else {
            *(unsigned char *)(ebase2 + 0x21) = 0x69;
            *(int *)(ebase2 + 0x38) = *(int *)((int)DAT_00487abc + 0xDC48 + (unsigned int)weapon_type * 4);
            *(int *)(ebase2 + 0x44) = *(int *)((int)DAT_00487abc + 0xDC84 + (unsigned int)weapon_type * 4);
            *(int *)(ebase2 + 0x4C) = *(int *)((int)DAT_00487abc + 0xDCB4 + (unsigned int)weapon_type * 4);
            *(char *)(ebase2 + 0x40) = (char)weapon_type - 6;
        }
        *(int *)(ebase2 + 0x34) = 0;
        *(int *)(ebase2 + 0x48) = 0;
        *(unsigned char *)(ebase2 + 0x54) = 0;
        *(int *)(ebase2 + 0x3C) = 0;

        DAT_00489248++;

        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x70) = ent[0];
        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x6C) = ent[1];
        if (sprIdx != NULL) {
            *(unsigned int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) =
                (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + (int)sprIdx * 2) + 30000;
        }

        /* Right side projectile: heading - 0x200 (opposite perpendicular) */
        if (DAT_00489248 < 0xA28) {
            side_angle = (heading - 0x200) & 0x7FF;
            eoff = DAT_00489248 * 0x80;
            ebase2 = (int)DAT_004892e8 + eoff;

            *(int *)(ebase2 + 0x00) = (ship_radius * lut2[side_angle]) / 2 + ent[0];
            *(int *)(ebase2 + 0x08) = (lut2[0x200 + side_angle] * ship_radius) / 2 + ent[1];
            *(int *)(ebase2 + 0x18) = (lut2[heading] * speed >> 6) + ent[4];
            *(int *)(ebase2 + 0x1C) = (lut2[(heading + 0x200) & 0x7FF] * speed >> 6) + ent[5];
            *(int *)(ebase2 + 0x04) = *(int *)(ebase2 + 0x00);
            *(int *)(ebase2 + 0x0C) = *(int *)(ebase2 + 0x08);

            *(unsigned short *)(ebase2 + 0x24) = 0;
            *(unsigned char *)(ebase2 + 0x20) = 0;
            *(unsigned char *)(ebase2 + 0x26) = 6;
            *(unsigned char *)(ebase2 + 0x22) = (unsigned char)idx;
            *(int *)(ebase2 + 0x28) = 0;

            if (weapon_type < 6) {
                *(unsigned char *)(ebase2 + 0x21) = 0;
                *(int *)(ebase2 + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + (unsigned int)weapon_type * 4);
                *(int *)(ebase2 + 0x44) = *(int *)((int)DAT_00487abc + 0xC4 + (unsigned int)weapon_type * 4);
                *(int *)(ebase2 + 0x4C) = *(int *)((int)DAT_00487abc + 0xF4 + (unsigned int)weapon_type * 4);
                *(char *)(ebase2 + 0x40) = (char)weapon_type;
            } else {
                *(unsigned char *)(ebase2 + 0x21) = 0x69;
                *(int *)(ebase2 + 0x38) = *(int *)((int)DAT_00487abc + 0xDC48 + (unsigned int)weapon_type * 4);
                *(int *)(ebase2 + 0x44) = *(int *)((int)DAT_00487abc + 0xDC84 + (unsigned int)weapon_type * 4);
                *(int *)(ebase2 + 0x4C) = *(int *)((int)DAT_00487abc + 0xDCB4 + (unsigned int)weapon_type * 4);
                *(char *)(ebase2 + 0x40) = (char)weapon_type - 6;
            }
            *(int *)(ebase2 + 0x34) = 0;
            *(int *)(ebase2 + 0x48) = 0;
            *(unsigned char *)(ebase2 + 0x54) = 0;
            *(int *)(ebase2 + 0x3C) = 0;

            DAT_00489248++;

            *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x70) = ent[0];
            *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x6C) = ent[1];
            if (sprIdx != NULL) {
                *(unsigned int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) =
                    (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + (int)sprIdx * 2) + 30000;
            }
        }
    }

    /* Play fire sound */
    unsigned int snd_type = (unsigned int)weapon_type;
    if (weapon_type > 7) snd_type = 7;
    FUN_0040f9b0(snd_type + 0xD2, ent[0], ent[1], 0x14);
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
    FUN_0040fd70(idx, 0xc, ent[0], ent[1]);

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
        FUN_0044f630(ent[0], ent[1], ent[4], ent[5], 1.0f, 0x18, 0xc,
                     *(char *)((int)ent + 0x49d));
    }
}

/* ===== FUN_0044f900 — Weapon Selection & Firing (0044F900) ===== */
/* Handles weapon cycling, firing charge, terrain speed effects, and
 * automatic weapon selection for AI-controlled entities. */
static void FUN_0044f900_impl(int *ent, int idx)
{
    unsigned char bVar3 = (unsigned char)DAT_00487a18;
    int localX = ent[0];
    int tileY = ent[1] >> 0x12;
    int tileX = localX >> 0x12;
    char local_5e = '\x04';  /* default terrain speed */

    /* Check current tile for terrain speed modifier */
    unsigned char bVar1 = *(unsigned char *)((tileY << (bVar3 & 0x1f)) + tileX + (int)DAT_0048782c);
    int tileProps = (unsigned int)bVar1 * 0x20 + (int)DAT_00487928;
    char cVar6 = *(char *)(tileProps + 0x18);
    if (cVar6 != '\0') {
        local_5e = *(char *)(tileProps + 0x19);
    }
    int local_5d = (cVar6 == '\0' || bVar1 == 0x1f) ? 1 : 0;

    /* Check tile below */
    bVar1 = *(unsigned char *)(((tileY + 1) << (bVar3 & 0x1f)) + tileX + (int)DAT_0048782c);
    tileProps = (unsigned int)bVar1 * 0x20 + (int)DAT_00487928;
    if (*(char *)(tileProps + 0x18) != '\0') {
        local_5e = *(char *)(tileProps + 0x19);
        local_5d = (bVar1 == 0x1f) ? 1 : 0;
    }

    /* Check tile above */
    bVar1 = *(unsigned char *)(((tileY - 1) << (bVar3 & 0x1f)) + tileX + (int)DAT_0048782c);
    tileProps = (unsigned int)bVar1 * 0x20 + (int)DAT_00487928;
    if (*(char *)(tileProps + 0x18) != '\0') {
        local_5e = *(char *)(tileProps + 0x19);
        local_5d = (bVar1 == 0x1f) ? 1 : 0;
    }

    /* If on terrain: find passable tile ahead using aim direction */
    if (cVar6 != '\0') {
        unsigned long long uVar16 = FUN_004257e0(localX, ent[1], ent[2], ent[3]);
        ent[4] = 0;
        ent[5] = 0;
        int attempts = 0;
        int iVar8 = ent[0];
        do {
            iVar8 = (*(int *)((int)DAT_00487ab0 + (int)uVar16 * 4) >> 2) + iVar8;
            ent[0] = iVar8;
            int iVar12 = ent[1] + (*(int *)((int)DAT_00487ab0 + 0x800 + (int)uVar16 * 4) >> 2);
            ent[1] = iVar12;
            int tx = iVar8 >> 0x12;
            iVar12 = iVar12 >> 0x12;
            if (0 < tx && tx < DAT_004879f0 && 0 < iVar12 && iVar12 < DAT_004879f4 &&
                *(char *)((unsigned int)*(unsigned char *)((iVar12 << ((unsigned char)DAT_00487a18 & 0x1f)) +
                    (int)DAT_0048782c + tx) * 0x20 + 1 + (int)DAT_00487928) == '\x01')
                break;
            attempts++;
        } while (attempts < 10);
        if (attempts == 10) {
            ent[0] = ent[2];
            ent[1] = ent[3];
        }
    }

    /* Terrain speed effect on turret rotation */
    cVar6 = *(char *)((unsigned int)*(unsigned char *)((ent[0] >> 0x12) + (int)DAT_0048782c +
        ((ent[1] >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20 + 4 + (int)DAT_00487928);
    *(unsigned char *)(ent + 0x28) = 0x14;

    /* Charge weapon if on matching terrain */
    if ((local_5e == (char)ent[0xb] || local_5e == -1) && !local_5d) {
        int iVar12 = ent[8] + (unsigned int)DAT_00483754[1] * 0x200;
        ent[8] = iVar12;
        int iVar8 = *(int *)(idx * 0x40 + 0x28 + (int)DAT_0048780c);
        if (iVar8 < iVar12) {
            ent[8] = iVar8;
            *(unsigned char *)((int)ent + 0xa1) = 1;
        } else {
            *(unsigned char *)((int)ent + 0xa1) = 2;
        }
        *(unsigned char *)((int)ent + 0xa2) = 10;
    }

    /* Turret rotation speed based on terrain */
    int iVar8 = ent[6];
    if (cVar6 == '\0') {
        if (0x400 < iVar8) {
            ent[6] = iVar8 - 0x80;
            if (iVar8 - 0x80 < 0x400) ent[6] = 0x400;
        }
        if (ent[6] < 0x400) {
            ent[6] += 0x80;
            if (ent[6] > 0x400) ent[6] = 0x400;
        }
    } else {
        if (0x400 < iVar8) {
            ent[6] = iVar8 + 0x80;
            if (ent[6] > 0x7ff) ent[6] = 0;
        }
        if (ent[6] < 0x400) {
            ent[6] -= 0x80;
            if (ent[6] < 0) ent[6] = 0;
        }
    }

    /* If terrain type doesn't match entity type, skip weapon logic */
    if ((local_5e != (char)ent[0xb]) && (local_5e != -1))
        return;

    /* Activation flags */
    if (DAT_004892a5 == '\0' && DAT_004892a4 != '\0') {
        DAT_004892a5 = '\x01';
    }

    /* Weapon selection logic (manual vs AI) */
    if (*(char *)((int)ent + 0xdd) == '\0') {
        /* Manual weapon selection — player controlled */
        if (199 < ent[0x122] && 0xdb < ent[0x121]) {
            /* Toggle weapon menu on button press (bit 0x20) */
            if ((ent[0x2f] & 0x20U) == 0 && (*(unsigned char *)(ent + 0x2e) & 0x20) != 0 &&
                *(char *)((int)ent + 0x9e) == '\0') {
                *(unsigned char *)((int)ent + 0x9e) = 1;
            } else if ((ent[0x2f] & 0x20U) == 0 && (*(unsigned char *)(ent + 0x2e) & 0x20) != 0 &&
                *(char *)((int)ent + 0x9e) == '\x01') {
                *(unsigned char *)((int)ent + 0x9e) = 0;
            }
        }

        int bVar4 = 0;
        if (*(char *)((int)ent + 0x9e) == '\x01') {
            /* Weapon menu open — navigate with d-pad */
            ent[0x30] = ent[0x2e];

            /* Check underwater tile check for aiming */
            if (cVar6 == '\0') {
                cVar6 = *(char *)((unsigned int)*(unsigned char *)((ent[0] >> 0x12) + (int)DAT_0048782c +
                    (((ent[1] >> 0x12) - 1) << ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20 + 0x18 + (int)DAT_00487928);
            } else {
                cVar6 = *(char *)((unsigned int)*(unsigned char *)((ent[0] >> 0x12) + (int)DAT_0048782c +
                    (((ent[1] >> 0x12) + 1) << ((unsigned char)DAT_00487a18 & 0x1f))) * 0x20 + 0x18 + (int)DAT_00487928);
            }
            if (cVar6 != '\0') {
                *(unsigned char *)((int)ent + 0xc5) = 2;
            }

            /* Left button: cycle weapon backward */
            if ((ent[0x2f] & 1U) == 0 && (ent[0x2e] & 1) != 0) {
                char cVar6b = (char)ent[0xd];
                *(char *)(ent + 0xd) = cVar6b - 1;
                if ((char)(cVar6b - 1) < '\0') {
                    *(char *)(ent + 0xd) = cVar6b + 5;
                    if (ent[0xe] < (int)(char)(cVar6b + 5))
                        *(char *)(ent + 0xd) = (char)ent[0xe];
                }
                char cVar2 = (char)ent[0xd];
                if ((int)cVar2 / 6 != (int)cVar6b / 6) {
                    *(char *)(ent + 0xd) = cVar2 + 6;
                    if (ent[0xe] < (int)(char)(cVar2 + 6))
                        *(char *)(ent + 0xd) = (char)ent[0xe];
                }
                bVar4 = 1;
            }

            /* Right button: cycle weapon forward */
            if ((ent[0x2f] & 2U) == 0 && (*(unsigned char *)(ent + 0x2e) & 2) != 0) {
                int group = (int)(char)ent[0xd] / 6;
                char cVar6b = (char)ent[0xd] + 1;
                *(char *)(ent + 0xd) = cVar6b;
                if (ent[0xe] < (int)cVar6b)
                    *(char *)(ent + 0xd) = (char)group * 6;
                cVar6b = (char)ent[0xd];
                if ((int)cVar6b / 6 != group)
                    *(char *)(ent + 0xd) = cVar6b - 6;
                bVar4 = 1;
            }

            /* Up button: cycle weapon group up */
            if ((*(unsigned char *)(ent + 0x2f) & 4) == 0 && (*(unsigned char *)(ent + 0x2e) & 4) != 0) {
                char cVar6b = (char)ent[0xd] - 6;
                *(char *)(ent + 0xd) = cVar6b;
                if (cVar6b < '\0') {
                    int maxGrp = ent[0xe] / 6;
                    short sVar7 = (char)(maxGrp + 1) * 6;
                    cVar6b = (char)sVar7 + cVar6b;
                    *(char *)(ent + 0xd) = cVar6b;
                    if (ent[0xe] < (int)cVar6b)
                        *(char *)(ent + 0xd) = cVar6b - 6;
                }
                bVar4 = 1;
            }

            /* Down button: cycle weapon group down */
            if ((*(unsigned char *)(ent + 0x2f) & 0x40) == 0 && (*(unsigned char *)(ent + 0x2e) & 0x40) != 0) {
                char cVar6b = (char)ent[0xd] + 6;
                *(char *)(ent + 0xd) = cVar6b;
                if (ent[0xe] < (int)(unsigned int)(unsigned char)cVar6b) {
                    *(char *)(ent + 0xd) = cVar6b % 6;
                }
                bVar4 = 1;
            }
        } else {
            /* Weapon menu closed — fire on release */
            if ((ent[0x2f] & 1U) != 0 && (*(unsigned char *)(ent + 0x2e) & 1) == 0 &&
                (*(unsigned char *)(ent + 0x30) & 1) == 0) {
                char cVar6b = (char)ent[0xd] - 1;
                *(char *)(ent + 0xd) = cVar6b;
                if (cVar6b < '\0')
                    *(char *)(ent + 0xd) = (char)ent[0xe];
                bVar4 = 1;
            }
            if ((ent[0x2f] & 2U) != 0 && (*(unsigned char *)(ent + 0x2e) & 2) == 0 &&
                (*(unsigned char *)(ent + 0x30) & 2) == 0) {
                char cVar6b = (char)ent[0xd] + 1;
                *(char *)(ent + 0xd) = cVar6b;
                if (ent[0xe] < (int)cVar6b)
                    *(unsigned char *)(ent + 0xd) = 0;
                bVar4 = 1;
            }
            /* Clear held bits when released */
            if ((ent[0x30] & 1U) != 0 && (*(unsigned char *)(ent + 0x2e) & 1) == 0)
                ent[0x30] ^= 1;
            if ((ent[0x30] & 2U) != 0 && (*(unsigned char *)(ent + 0x2e) & 2) == 0)
                ent[0x30] ^= 2;
        }

        /* Fire button check */
        if (((*(unsigned char *)(ent + 0x2f) & 8) != 0 && (*(unsigned char *)(ent + 0x2e) & 8) == 0)) {
            unsigned int weapIdx = (unsigned int)*(unsigned char *)((char)ent[0xd] + 0x3c + (int)ent);
            unsigned int uVar9 = weapIdx * 0x43;
            if (*(char *)((int)DAT_00487abc + 0x7d + weapIdx * 0x218) != '\0') {
                *(char *)((int)ent + 0x35) = *(char *)((int)ent + 0x35) + 1;
            } else if (!bVar4) {
                return;
            }
        } else if (!bVar4) {
            return;
        }

        /* Set up weapon firing */
        ent[0x29] = 0;
        unsigned char *pbVar14 = (unsigned char *)((char)ent[0xd] + 0x3c + (int)ent);
        *(unsigned char *)(ent + 0x32) = 0xfa;
        if (*(unsigned char *)((int)DAT_00487abc + 0x7d + (unsigned int)*pbVar14 * 0x218) <
            *(unsigned char *)((int)ent + 0x35)) {
            *(unsigned char *)((int)ent + 0x35) = 0;
        }
        iVar8 = (unsigned int)*(unsigned char *)((int)ent + 0x35) + (unsigned int)*pbVar14 * 0x86;
    } else {
        /* AI weapon selection — pick random available weapon */
        unsigned int uVar9 = ent[0xe];
        if (uVar9 == 0) return;
        if (0 < (int)uVar9) {
            unsigned char local_50[80];
            unsigned char *pbVar14 = (unsigned char *)(ent + 0xf);
            unsigned char *pbVar15 = local_50;
            unsigned int i;
            for (i = uVar9 >> 2; i != 0; i--) {
                *(unsigned int *)pbVar15 = *(unsigned int *)pbVar14;
                pbVar14 += 4; pbVar15 += 4;
            }
            for (i = uVar9 & 3; i != 0; i--) {
                *pbVar15 = *pbVar14;
                pbVar14++; pbVar15++;
            }

            int iVar12 = 0;
            iVar8 = 0;
            if ((int)uVar9 < 1) return;
            while (1) {
                iVar12 = rand();
                iVar12 = iVar12 % (int)uVar9;
                if (*(char *)((int)DAT_00487abc + 0x7c + (unsigned int)local_50[iVar12] * 0x218) != '\0')
                    break;
                int iVar13 = uVar9 - 1;
                unsigned int uVar11 = ent[0xe];
                uVar9--;
                iVar8++;
                local_50[iVar12] = local_50[iVar13];
                if ((int)uVar11 <= iVar8) return;
            }

            /* Find the weapon slot matching the random pick */
            iVar8 = 0;
            if (0 < ent[0xe]) {
                do {
                    if (*(unsigned char *)(iVar8 + 0x3c + (int)ent) == local_50[iVar12]) {
                        *(char *)(ent + 0xd) = (char)iVar8;
                        break;
                    }
                    iVar8++;
                } while (iVar8 < ent[0xe]);
            }
        }

        ent[0x29] = 0;
        unsigned char *pbVar14 = (unsigned char *)((char)ent[0xd] + 0x3c + (int)ent);
        iVar8 = rand();
        uVar9 = iVar8 % (int)(*(unsigned char *)((int)DAT_00487abc + 0x7d +
            (unsigned int)*pbVar14 * 0x218) + 1);
        *(char *)((int)ent + 0x35) = (char)uVar9;
        iVar8 = (uVar9 & 0xff) + (unsigned int)*pbVar14 * 0x86;
    }

    /* Apply weapon fire rate */
    ent[0x25] = *(int *)((int)DAT_00487abc + 0xdc + iVar8 * 4) *
        (unsigned int)*(unsigned char *)(ent + 0x27) * DAT_0048382c >> 0xc;
}

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
    int iVar4 = *(int *)(iVar2 + 4 + (int)g_PhysicsParams) >> 1;
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

            int damage = *(int *)(iVar2 + 8 + (int)g_PhysicsParams);
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
                *(int *)((int)DAT_00487ab0 + nextSin) >> 5;
            *(int *)(base2 + 0x0c) = (rand() % 0x32) *
                *(int *)(nextCos + (int)DAT_00487ab0) >> 5;
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

/* ===== FUN_00401000 — Main Entity Update / Weapon Fire Effect Spawner =====
 * Address: 00401000, ~22KB machine code
 *
 * When a weapon fires or an entity triggers an effect, this function spawns
 * the appropriate particles, projectiles, edge records, explosions, and debris.
 *
 * param idx = entity/player index
 * Entity data at DAT_00487810 + idx * 0x598
 * Particle data at DAT_004892e8 (stride 0x80, count DAT_00489248)
 * Entity type defs at DAT_00487abc (accessed as int[])
 */
static void FUN_00401000_impl(int idx)
{
    int *piVar1;
    int iVar11, iVar12, iVar13, iVar16;
    unsigned int uVar6, uVar7, uVar8;
    int local_c;
    unsigned int local_14;
    unsigned int local_8;
    int uVar10;
    unsigned char uVar9 = (unsigned char)idx;

    int *typeTable = (int *)DAT_00487abc;
    int *sincos    = (int *)DAT_00487ab0;
    /* sincos[angle] = sin, sincos[0x200 + angle] = cos (2048 entries each) */

    iVar12 = idx * 0x598;
    uVar7  = *(unsigned int *)(iVar12 + 0x18 + (int)DAT_00487810);

    local_14 = (unsigned int)*(unsigned char *)(
        *(char *)(iVar12 + 0x34 + (int)DAT_00487810) +
        iVar12 + 0x3c + (int)DAT_00487810);

    if (local_14 == 0x32) return;

    if (local_14 == 0x17) {
        if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x01' &&
            *(int *)(iVar12 + 0xa4 + (int)DAT_00487810) == 0) {
            local_14 = 0x5a;
        }
    }

    /* === Sound dispatch (first jump table in original) === */
    {
        int ex = *(int *)(iVar12 + (int)DAT_00487810);
        int ey = *(int *)(iVar12 + 4 + (int)DAT_00487810);
        char field35 = *(char *)(iVar12 + 0x35 + (int)DAT_00487810);

        if (local_14 > 0x2c) {
            /* Types > 44 (including 0x5a override): play sound using type as index */
            FUN_0040f9b0((int)local_14, ex, ey, 0xFF, 0x3E8);
        } else {
            /* Types 0-44: per-type sound dispatch */
            switch (local_14) {
            case 0:
                FUN_0040f9b0(0x78, ex, ey, 0xFF, 0x3E8);
                break;
            case 2:
                if (field35 == 0) FUN_0040fd70(idx, 2, ex, ey, 0xFF, 0x3E8);
                else FUN_0040f9b0(0x10E, ex, ey, 0xFF, 0x3E8);
                break;
            case 3:
                FUN_0040f9b0(rand() % 6 + 0x105, ex, ey, 0xFF, 0x3E8);
                break;
            case 4:
                FUN_0040fd70(idx, 4, ex, ey, 0xFF, 0x3E8);
                break;
            case 10: case 18: case 25:
                break; /* no sound (continuous-fire types) */
            case 12:
                FUN_0040fd70(idx, 0x0C, ex, ey, 0xFF, 0x3E8);
                break;
            case 16:
                FUN_0040fd70(idx, 0x10, ex, ey, 0xFF, 0x3E8);
                break;
            case 24:
                if (field35 == 0) FUN_0040f9b0(0x18, ex, ey, 0xFF, 0x3E8);
                else FUN_0040f9b0(0x10E, ex, ey, 0xFF, 0x3E8);
                break;
            case 34: {
                /* Sub-switch on field35 (0-3), >3 = no sound */
                static const int type34_sounds[] = {0xD3, 0xD5, 0x113, 0x22};
                if ((unsigned char)field35 <= 3)
                    FUN_0040f9b0(type34_sounds[(unsigned char)field35], ex, ey, 0xFF, 0x3E8);
                break;
            }
            case 44:
                FUN_0040fd70(idx, 0x2C, ex, ey, 0xFF, 0x3E8);
                break;
            default:
                /* Generic: play sound using weapon type as index */
                FUN_0040f9b0((int)local_14, ex, ey, 0xFF, 0x3E8);
                break;
            }
        }
    }

    /* Stats update (runs for ALL weapon types) */
    iVar13 = (int)DAT_00487810;
    DAT_00486d28[idx] += (unsigned int)*(unsigned char *)((int)&DAT_00481ca8 + local_14);
    DAT_004870e8[idx] += (unsigned int)*(unsigned char *)((int)&DAT_00481c58 + local_14);

    uVar9 = (unsigned char)idx;

    switch (local_14) {

    /* ===== CASE 0: Scatter debris (30 particles) ===== */
    case 0:
    {
        local_c = 0;
        do {
            if (0x9c3 < DAT_00489248) break;
            iVar13 = rand();
            uVar8 = (iVar13 % 0x46 - 0x23 + uVar7) & 0x7ff;
            iVar13 = rand();
            iVar13 = iVar13 % 0x46 + 0x46;
            {
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x18) = (sincos[uVar8] * iVar13 >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
                *(int *)(p + 0x1c) = (sincos[0x200 + uVar8] * iVar13 >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
                *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                *(unsigned char *)(p + 0x21) = 0;
                uVar8 = rand(); uVar8 &= 0x80000001;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffe) + 1;
                *(short *)(p + 0x24) = (short)uVar8 * 6;
                *(unsigned char *)(p + 0x20) = 0;
                *(unsigned char *)(p + 0x26) = 8;
                *(unsigned char *)(p + 0x22) = uVar9;
                *(int *)(p + 0x28) = 0;
                uVar8 = rand(); uVar8 &= 0x80000003;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
                *(int *)(p + 0x38) = typeTable[uVar8 + 0x22];
                uVar8 = rand(); uVar8 &= 0x80000003;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
                *(int *)(p + 0x44) = typeTable[uVar8 + 0x31];
                *(int *)(p + 0x48) = 0;
                uVar8 = rand(); uVar8 &= 0x80000003;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
                *(int *)(p + 0x4c) = typeTable[uVar8 + 0x3d];
                *(unsigned char *)(p + 0x54) = 0;
                uVar8 = rand(); uVar8 &= 0x80000003;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
                *(char *)(p + 0x40) = (char)uVar8;
                *(int *)(p + 0x34) = typeTable[0];
                *(int *)(p + 0x3c) = 0;
                *(unsigned char *)(p + 0x5c) = 0;
            }
            DAT_00489248++;
            iVar13 = rand();
            uVar8 = iVar13 % 100 + 0x14;
            {
                /* Grayscale particle color: original X1R5G5B5 (*0x421), converted to RGB565 (*0x841) */
                unsigned int gray5 = uVar8 >> 3;
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    ((gray5 << 11) | (gray5 << 6) | gray5) + 30000;
            }
            iVar13 = rand();
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = iVar13 % 0x32 + 0x50;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x70) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x6c) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            local_c++;
            iVar13 = (int)DAT_00487810;
        } while (local_c < 0x1e);
        break;
    }

    /* ===== CASE 1: Beam projectiles ===== */
    case 1:
    {
        char cVar3 = *(char *)(iVar12 + 0x35 + iVar13);
        if (cVar3 != '\0') {
            if (cVar3 == '\x01') {
                uVar8 = uVar7 - 0x2a;
                local_c = 0;
                do {
                    unsigned int ang = uVar8 & 0x7ff;
                    if (0x9c3 < DAT_00489248) break;
                    int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                    *(int *)(p) = *(int *)(iVar12 + iVar13);
                    *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                    *(int *)(p + 0x18) = (sincos[ang] * typeTable[0xb1] >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
                    *(int *)(p + 0x1c) = (sincos[0x200 + ang] * typeTable[0xb1] >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
                    *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                    *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                    *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                    *(unsigned char *)(p + 0x21) = 1;
                    *(unsigned short *)(p + 0x24) = 0;
                    *(unsigned char *)(p + 0x20) = 0;
                    *(unsigned char *)(p + 0x26) = 0xc;
                    *(unsigned char *)(p + 0x22) = uVar9;
                    *(int *)(p + 0x28) = 0;
                    *(int *)(p + 0x38) = typeTable[0xa8];
                    *(int *)(p + 0x44) = typeTable[0xb7];
                    *(int *)(p + 0x48) = 0;
                    *(int *)(p + 0x4c) = typeTable[0xc3];
                    *(unsigned char *)(p + 0x54) = 0;
                    *(unsigned char *)(p + 0x40) = 0;
                    *(int *)(p + 0x34) = typeTable[0x86];
                    *(int *)(p + 0x3c) = 0;
                    *(unsigned char *)(p + 0x5c) = 0;
                    uVar8 = ang + 0x2a;
                    DAT_00489248++;
                    local_c++;
                    iVar13 = (int)DAT_00487810;
                } while (local_c < 3);
            }
            else if (cVar3 == '\x02' && DAT_00489248 < 0x9c4) {
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(int *)(p) = *(int *)(iVar12 + iVar13);
                *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x18) = (sincos[uVar7] * typeTable[0xb2] >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
                *(int *)(p + 0x1c) = (sincos[0x200 + uVar7] * typeTable[0xb2] >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
                *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                *(unsigned char *)(p + 0x21) = 1;
                *(unsigned short *)(p + 0x24) = 0;
                *(unsigned char *)(p + 0x20) = 0;
                *(unsigned char *)(p + 0x26) = 0xc;
                *(unsigned char *)(p + 0x22) = uVar9;
                *(int *)(p + 0x28) = 0;
                *(int *)(p + 0x38) = typeTable[0xa9];
                *(int *)(p + 0x44) = typeTable[0xb8];
                *(int *)(p + 0x48) = 0;
                *(int *)(p + 0x4c) = typeTable[0xc4];
                *(unsigned char *)(p + 0x54) = 0;
                *(unsigned char *)(p + 0x40) = 1;
                *(int *)(p + 0x34) = typeTable[0x86];
                *(int *)(p + 0x3c) = 0;
                *(unsigned char *)(p + 0x5c) = 0;
                DAT_00489248++;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = 2;
                piVar1 = (int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x3c);
                *piVar1 *= 6;
                iVar13 = (int)DAT_00487810;
            }
            break;
        }
        if (0x9c3 < DAT_00489248) break;
        {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = (sincos[uVar7] * typeTable[0xb3] >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
            *(int *)(p + 0x1c) = (sincos[0x200 + uVar7] * typeTable[0xb3] >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 1;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0;
            *(unsigned char *)(p + 0x26) = 0xc;
            *(unsigned char *)(p + 0x22) = uVar9;
            *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[0xaa];
            *(int *)(p + 0x44) = typeTable[0xb9];
            *(int *)(p + 0x48) = 0;
            *(int *)(p + 0x4c) = typeTable[0xc5];
            *(unsigned char *)(p + 0x54) = 0;
            *(unsigned char *)(p + 0x40) = 2;
        }
        {
            uVar10 = typeTable[0x86];
            goto LAB_00401856;
        }
    }

    /* ===== CASE 2 ===== */
    case 2:
    {
        if (*(char *)(iVar12 + 0x35 + iVar13) == '\0') {
            if (DAT_00489248 < 0x9c4) {
                iVar13 = rand();
                uVar8 = (iVar13 % 0x78 - 0x3c + uVar7) & 0x7ff;
                iVar13 = idx * 0x40;
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(int *)(p) = (sincos[uVar7] >> 1) * *(int *)(iVar13 + 0x34 + (int)DAT_0048780c) + *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 8) = (sincos[0x200 + uVar7] >> 1) * *(int *)(iVar13 + 0x34 + (int)DAT_0048780c) + *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x18) = (typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x137] * sincos[uVar8] >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
                *(int *)(p + 0x1c) = (typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x137] * sincos[0x200 + uVar8] >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
                *(int *)(p + 4) = (sincos[uVar7] >> 1) * *(int *)(iVar13 + 0x34 + (int)DAT_0048780c) + *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 0xc) = (sincos[0x200 + uVar7] >> 1) * *(int *)(iVar13 + 0x34 + (int)DAT_0048780c) + *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                *(unsigned char *)(p + 0x21) = 2;
                iVar13 = rand();
                *(short *)(p + 0x24) = (short)(iVar13 % 6);
                *(unsigned char *)(p + 0x20) = 10;
                *(unsigned char *)(p + 0x26) = 0xf;
                *(unsigned char *)(p + 0x22) = uVar9;
                *(int *)(p + 0x28) = 0;
                *(int *)(p + 0x38) = typeTable[0x12e];
                *(int *)(p + 0x44) = typeTable[0x13d];
                *(int *)(p + 0x48) = 0;
                *(int *)(p + 0x4c) = typeTable[0x149];
                *(unsigned char *)(p + 0x54) = 0;
                *(unsigned char *)(p + 0x40) = 0;
                *(int *)(p + 0x34) = typeTable[0x10c];
                *(int *)(p + 0x3c) = 0;
                *(unsigned char *)(p + 0x5c) = 0;
                DAT_00489248++;
                *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x24) = 0x14;
                uVar8 = rand(); uVar8 &= 0x8000000f;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffff0) + 1;
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    *(unsigned short *)((int)DAT_00487aa8 + 0x140 + uVar8 * 2) + 30000;
                iVar13 = rand();
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = iVar13 % 400 + 0x96;
                iVar13 = (int)DAT_00487810;
            }
            break;
        }
        if (0x9c3 < DAT_00489248) break;
        {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = (sincos[uVar7] * 0x14 >> 4) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
            *(int *)(p + 0x1c) = (sincos[0x200 + uVar7] * 0x14 >> 4) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 0x14;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0;
            *(unsigned char *)(p + 0x26) = 0xff;
            *(unsigned char *)(p + 0x22) = uVar9;
            *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[0xa9b];
            *(int *)(p + 0x44) = typeTable[0xaaa];
            *(int *)(p + 0x48) = 0;
            *(int *)(p + 0x4c) = typeTable[0xab6];
            *(unsigned char *)(p + 0x54) = 0;
            *(unsigned char *)(p + 0x40) = 1;
        }
        {
            uVar10 = typeTable[0xa78];
LAB_00401856:
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 + 0x34) = uVar10;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 + 0x3c) = 0;
            *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 + 0x5c) = 0;
            DAT_00489248++;
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 3: Trooper spawn ===== */
    case 3:
    {
        if (399 < DAT_0048924c) break;
        if (*(char *)(iVar12 + 0x35 + iVar13) == '\0') {
            iVar13 = rand(); iVar11 = iVar13 % 0x3c + 0x5a;
        } else {
            iVar13 = rand(); iVar11 = iVar13 % 0x1e + 200;
        }
        iVar11 <<= 9;
        iVar13 = iVar12 + (int)DAT_00487810;
        uVar9 = *(unsigned char *)(iVar13 + 0x2c);
        {
            char cVar3a = *(char *)(iVar13 + 0x35) << 1;
            uVar8 = rand(); uVar8 &= 0x80000001;
            if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffe) + 1;
            char cVar4 = (char)uVar8;
            FUN_00407210(*(int *)(iVar13 + 8), *(int *)(iVar13 + 0xc), 0, 0, cVar4 * '\x02' + -1, iVar11, uVar9, cVar3a);
        }
        iVar13 = (int)DAT_00487810;
        break;
    }

    /* ===== CASE 4: Thruster trail ===== */
    case 4:
    {
        *(int *)(iVar12 + 0x10 + iVar13) += sincos[uVar7] >> 3;
        *(int *)(iVar12 + 0x14 + (int)DAT_00487810) += sincos[0x200 + uVar7] >> 3;
        iVar13 = (int)DAT_00487810;
        if ((*(unsigned char *)((*(int *)(iVar12 + (int)DAT_00487810) >> 0x16) +
             (int)DAT_00487814 + (*(int *)(iVar12 + 4 + (int)DAT_00487810) >> 0x16) * DAT_004879f8) & 8) != 0)
        {
            int ftol_count = (int)DAT_004892d0;
            local_c = 0;
            if (0 < ftol_count) {
                do {
                    if (0x9c3 < DAT_00489248) break;
                    iVar13 = rand(); iVar11 = rand();
                    uVar8 = (iVar11 % 0x15e + 0x351 + *(int *)(iVar12 + 0x18 + (int)DAT_00487810)) & 0x7ff;
                    int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                    *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810) + sincos[uVar7] * -8;
                    *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810) + sincos[0x200 + uVar7] * -8;
                    *(int *)(p + 0x18) = sincos[uVar8] * (iVar13 % 0x14) >> 3;
                    *(int *)(p + 0x1c) = sincos[0x200 + uVar8] * (iVar13 % 0x14) >> 3;
                    *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810) + sincos[uVar7] * -8;
                    *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810) + sincos[0x200 + uVar7] * -8;
                    *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                    *(unsigned char *)(p + 0x21) = 0x67;
                    iVar13 = rand(); *(short *)(p + 0x24) = (short)(iVar13 % 6);
                    *(unsigned char *)(p + 0x20) = 0;
                    *(unsigned char *)(p + 0x26) = 0xff;
                    *(unsigned char *)(p + 0x22) = 0xff;
                    *(int *)(p + 0x28) = 0;
                    *(int *)(p + 0x38) = typeTable[0x360c]; *(int *)(p + 0x44) = typeTable[0x361b];
                    *(int *)(p + 0x48) = 0; *(int *)(p + 0x4c) = typeTable[0x3627];
                    *(unsigned char *)(p + 0x54) = 0; *(unsigned char *)(p + 0x40) = 0;
                    *(int *)(p + 0x34) = typeTable[0x35ea]; *(int *)(p + 0x3c) = 0;
                    *(unsigned char *)(p + 0x5c) = 0;
                    DAT_00489248++;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = 0;
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x24) = 3;
                    uVar8 = rand(); uVar8 &= 0x80000007;
                    if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffff8) + 1;
                    *(char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) = (char)uVar8 + '\x14';
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0x12;
                    {
                        int pp = (int)DAT_004892e8 + DAT_00489248 * 0x80;
                        *(unsigned int *)(pp - 0x34) = *(unsigned short *)((int)DAT_00487aa8 + (unsigned int)*(unsigned char *)(pp - 0x1b) * 2) + 30000;
                    }
                    local_c++;
                    iVar13 = (int)DAT_00487810;
                } while (local_c < ftol_count);
            }
        }
        break;
    }

    /* ===== CASE 6: Trooper dynamic spawn ===== */
    case 6:
    {
        if (399 < DAT_0048924c) break;
        uVar9 = *(unsigned char *)(iVar12 + 0x2c + iVar13);
        iVar11 = rand(); iVar11 = (iVar11 % 100 + 300) * 0x200;
        uVar8 = rand(); uVar8 &= 0x80000001;
        if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffe) + 1;
        FUN_00407210(*(int *)(iVar12 + iVar13 + 8), *(int *)(iVar12 + iVar13 + 0xc),
                     0, 0, (char)uVar8 * '\x02' + -1, iVar11, uVar9, '\x01');
        iVar13 = (int)DAT_00487810;
        break;
    }

    /* ===== CASE 7: Teleport with particle effects ===== */
    case 7:
    {
        iVar13 = rand();
        if (iVar13 % 10 == 0) {
            piVar1 = (int *)(iVar12 + 0x20 + (int)DAT_00487810);
            *piVar1 += -0x4e2000;
            iVar13 = (int)DAT_00487810;
        } else {
            local_c = 0;
            int foundX = 0, foundY = 0;
            int found = 0;
            do {
                iVar13 = rand();
                foundX = iVar13 % 600 - 300 + (*(int *)(iVar12 + (int)DAT_00487810) >> 0x12);
                iVar11 = rand();
                foundY = iVar11 % 600 - 300 + (*(int *)(iVar12 + 4 + (int)DAT_00487810) >> 0x12);
                uVar6 = FUN_0044de10(foundX, foundY);
                if ((char)uVar6 == '\x01') { found = 1; break; }
                local_c++;
                iVar13 = (int)DAT_00487810;
            } while (local_c < 10);

            if (found) {
                /* Spawn teleport particles in circle around entity */
                unsigned int angleParam = 0;
                while (1) {
                    if (0x9c3 < DAT_00489248) break;
                    uVar6 = rand(); uVar6 &= 0x8000003f;
                    if ((int)uVar6 < 0) uVar6 = (uVar6 - 1 | 0xffffffc0) + 1;
                    unsigned int uVar15 = (unsigned int)FUN_004257e0(
                        *(int *)(iVar12 + (int)DAT_00487810) >> 0x12,
                        *(int *)(iVar12 + 4 + (int)DAT_00487810) >> 0x12, foundX, foundY);
                    iVar11 = sincos[uVar15];
                    iVar16 = sincos[0x200 + uVar15];
                    int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                    *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810);
                    *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                    *(int *)(p + 0x18) = ((int)(sincos[angleParam] * (int)uVar6) >> 10) + (iVar11 >> 1);
                    *(int *)(p + 0x1c) = ((int)(sincos[0x200 + angleParam] * (int)uVar6) >> 10) + (iVar16 >> 1);
                    *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                    *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                    *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                    *(unsigned char *)(p + 0x21) = 0x67;
                    iVar11 = rand(); *(short *)(p + 0x24) = (short)(iVar11 % 6);
                    *(unsigned char *)(p + 0x20) = 0;
                    *(unsigned char *)(p + 0x26) = 0xff;
                    *(unsigned char *)(p + 0x22) = 0xff;
                    *(int *)(p + 0x28) = 0;
                    *(int *)(p + 0x38) = typeTable[0x3611]; *(int *)(p + 0x44) = typeTable[0x3620];
                    *(int *)(p + 0x48) = 0; *(int *)(p + 0x4c) = typeTable[0x362c];
                    *(unsigned char *)(p + 0x54) = 0; *(unsigned char *)(p + 0x40) = 5;
                    *(int *)(p + 0x34) = typeTable[0x35ea]; *(int *)(p + 0x3c) = 0;
                    *(unsigned char *)(p + 0x5c) = 0;
                    DAT_00489248++;
                    uVar6 = rand(); uVar6 &= 0x80000007;
                    if ((int)uVar6 < 0) uVar6 = (uVar6 - 1 | 0xfffffff8) + 1;
                    angleParam += 0x40;
                    *(char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x24) = (char)uVar6 + '\x04';
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) = 0x27;
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0x21;
                    {
                        int pp2 = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                        *(unsigned int *)(pp2 - 0x34) = *(unsigned short *)((int)DAT_00487aa8 + (unsigned int)*(unsigned char *)(pp2 - 0x1b) * 2) + 30000;
                    }
                    if (0x1fff < (int)angleParam) break;
                }
                /* Spawn water edge records if in water */
                if ((*(unsigned char *)((*(int *)(iVar12 + (int)DAT_00487810) >> 0x16) +
                     (int)DAT_00487814 + (*(int *)(iVar12 + 4 + (int)DAT_00487810) >> 0x16) * DAT_004879f8) & 8) != 0)
                {
                    iVar11 = 0;
                    do {
                        if (0x5db < DAT_0048925c) break;
                        int e = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
                        *(int *)(e) = *(int *)(iVar12 + (int)DAT_00487810);
                        *(int *)(e + 4) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                        uVar6 = rand(); uVar6 &= 0x800000ff;
                        if ((int)uVar6 < 0) uVar6 = (uVar6 - 1 | 0xffffff00) + 1;
                        *(unsigned int *)(e + 8) = (0x80 - uVar6) * 0x400;
                        uVar6 = rand(); uVar6 &= 0x800000ff;
                        if ((int)uVar6 < 0) uVar6 = (uVar6 - 1 | 0xffffff00) + 1;
                        *(unsigned int *)(e + 0xc) = (0x80 - uVar6) * 0x400;
                        iVar16 = rand(); *(char *)(e + 0x10) = (char)(iVar16 % 5) + '\x02';
                        iVar16 = rand(); *(char *)(e + 0x11) = (char)(iVar16 % 3);
                        *(unsigned short *)(e + 0x12) = 0;
                        *(unsigned char *)(e + 0x14) = 0xff;
                        *(unsigned char *)(e + 0x15) = 0;
                        DAT_0048925c++;
                        iVar11++;
                    } while (iVar11 < 0x20);
                }
                /* Teleport entity */
                *(int *)(iVar12 + (int)DAT_00487810) = foundX * 0x40000;
                *(unsigned int *)(iVar12 + 4 + (int)DAT_00487810) = (unsigned int)foundY * 0x40000;
                *(int *)(iVar12 + 8 + (int)DAT_00487810) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(iVar12 + 0xc + (int)DAT_00487810) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(unsigned char *)(iVar12 + 0x4a0 + (int)DAT_00487810) = 0xff;
                iVar13 = (int)DAT_00487810;
                goto LAB_00402bc8;
            }
        }
        break;
    }

    case 10: case 0xc:
        *(int *)(iVar12 + 0xa4 + iVar13) = 300;
        iVar13 = (int)DAT_00487810;
        break;

    case 0xd:
        *(int *)(iVar12 + 0xa4 + iVar13) = 500;
        iVar13 = (int)DAT_00487810;
        break;

    /* ===== CASE 0x10: Fluid source + particle ===== */
    case 0x10:
    {
        iVar13 = rand();
        uVar8 = (iVar13 % 0x78 - 0x3c + uVar7) & 0x7ff;
        if (((*(unsigned char *)((*(int *)(iVar12 + (int)DAT_00487810) >> 0x16) +
             (int)DAT_00487814 + (*(int *)(iVar12 + 4 + (int)DAT_00487810) >> 0x16) * DAT_004879f8) & 8) != 0) &&
            ((float)_DAT_004753e0_d < DAT_0048385c) && (DAT_0048925c < 0x5dc))
        {
            int e = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
            *(int *)(e) = (sincos[uVar7] * 600 >> 6) + *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(e + 4) = (sincos[0x200 + uVar7] * 600 >> 6) + *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(e + 8) = (sincos[uVar8] * 0x32 >> 6) + (*(int *)(iVar12 + 0x10 + (int)DAT_00487810) >> 1);
            *(int *)(e + 0xc) = (sincos[0x200 + uVar8] * 0x32 >> 6) - 500 + (*(int *)(iVar12 + 0x14 + (int)DAT_00487810) >> 1);
            uVar6 = rand(); uVar6 &= 0x80000001;
            if ((int)uVar6 < 0) uVar6 = (uVar6 - 1 | 0xfffffffe) + 1;
            *(char *)(e + 0x10) = (char)uVar6;
            *(unsigned char *)(e + 0x11) = 0; *(unsigned short *)(e + 0x12) = 0;
            *(unsigned char *)(e + 0x14) = 0xff; *(unsigned char *)(e + 0x15) = 0;
            DAT_0048925c++;
        }
        iVar13 = (int)DAT_00487810;
        if (DAT_00489250 < 2000) {
            int pp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
            *(int *)(pp) = (sincos[uVar7] * 500 >> 6) + *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(pp + 4) = (sincos[0x200 + uVar7] * 500 >> 6) + *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(pp + 8) = (sincos[uVar8] * 0x30 >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
            *(int *)(pp + 0xc) = (sincos[0x200 + uVar8] * 0x30 >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
            uVar8 = rand(); uVar8 &= 0x80000003;
            if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
            *(char *)(pp + 0x10) = (char)uVar8 + '\x01';
            *(unsigned char *)(pp + 0x11) = 1; *(unsigned char *)(pp + 0x12) = 2;
            *(unsigned char *)(pp + 0x13) = 200; *(unsigned char *)(pp + 0x14) = uVar9;
            *(unsigned char *)(pp + 0x15) = 0;
            DAT_00489250++;
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    case 0x12:
        *(int *)(iVar12 + 0xa4 + iVar13) = 0x1c2;
        iVar13 = (int)DAT_00487810;
        break;

    /* ===== CASE 0x15: Scatter 14 particles ===== */
    case 0x15:
    {
        local_c = 0;
        do {
            if (0x9c3 < DAT_00489248) break;
            uVar8 = rand(); uVar8 &= 0x800007ff;
            if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffff800) + 1;
            iVar13 = rand(); iVar13 = iVar13 % 0x32 + 0x3c;
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = (sincos[uVar8] * iVar13 >> 6) + (*(int *)(iVar12 + 0x10 + (int)DAT_00487810) >> 1);
            *(int *)(p + 0x1c) = (sincos[0x200 + uVar8] * iVar13 >> 6) + (*(int *)(iVar12 + 0x14 + (int)DAT_00487810) >> 1);
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 0x11;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0x14;
            *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[0x908]; *(int *)(p + 0x44) = typeTable[0x917];
            *(int *)(p + 0x48) = 0; *(int *)(p + 0x4c) = typeTable[0x923];
            *(unsigned char *)(p + 0x54) = 0; *(unsigned char *)(p + 0x40) = 0;
            *(int *)(p + 0x34) = typeTable[0x8e6]; *(int *)(p + 0x3c) = 0;
            *(unsigned char *)(p + 0x5c) = 0;
            DAT_00489248++;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = 1;
            local_c++;
            iVar13 = (int)DAT_00487810;
        } while (local_c < 0xe);
        break;
    }

    /* ===== CASE 0x19: Multi-directional beam ===== */
    case 0x19:
    {
        char cVar3_19 = *(char *)(iVar12 + 0x35 + iVar13);
        if (cVar3_19 == '\0') {
            FUN_0040f9b0(0x10f, *(int *)(iVar12 + iVar13), *(int *)(iVar12 + 4 + iVar13));
            uVar8 = uVar7 - 0x80;
            local_c = 0;
            do {
                iVar13 = (int)DAT_00487810;
                if (0x9c3 < DAT_00489248) break;
                uVar8 &= 0x7ff;
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x18) = sincos[uVar8] >> 1;
                *(int *)(p + 0x1c) = sincos[0x200 + uVar8] >> 1;
                *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                *(unsigned char *)(p + 0x21) = 0x19;
                *(unsigned short *)(p + 0x24) = 0;
                *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0;
                *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
                *(int *)(p + 0x38) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd38];
                uVar8 += 0x80;
                *(int *)(p + 0x44) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd47];
                *(int *)(p + 0x48) = 0;
                *(int *)(p + 0x4c) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd53];
                *(unsigned char *)(p + 0x54) = 0;
                *(unsigned char *)(p + 0x40) = *(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810);
                *(int *)(p + 0x34) = typeTable[0xd16]; *(int *)(p + 0x3c) = 0;
                *(unsigned char *)(p + 0x5c) = 0;
                DAT_00489248++;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 0x9c4;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
                local_c++;
                iVar13 = (int)DAT_00487810;
            } while (local_c < 3);
        } else {
            int sndId = (cVar3_19 == '\x02') ? 0x111 : 0x110;
            FUN_0040f9b0(sndId, *(int *)(iVar12 + iVar13), *(int *)(iVar12 + 4 + iVar13));
            iVar13 = (int)DAT_00487810;
            if (DAT_00489248 < 0x9c4) {
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x18) = 0; *(int *)(p + 0x1c) = 0;
                *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
                *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
                *(unsigned char *)(p + 0x21) = 0x19;
                *(unsigned short *)(p + 0x24) = 0;
                *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0x32;
                *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
                *(int *)(p + 0x38) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd38];
                *(int *)(p + 0x44) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd47];
                *(int *)(p + 0x48) = 0;
                *(int *)(p + 0x4c) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0xd53];
                *(unsigned char *)(p + 0x54) = 0;
                *(unsigned char *)(p + 0x40) = *(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810);
                *(int *)(p + 0x34) = typeTable[0xd16]; *(int *)(p + 0x3c) = 0;
                *(unsigned char *)(p + 0x5c) = 0;
                DAT_00489248++;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 6000;
                iVar13 = (int)DAT_00487810;
            }
        }
        break;
    }

    /* ===== CASE 0x1a: Multi-beam (up to 10 beams, type 0x69) ===== */
    /* This case is extremely repetitive - spawns beams at various angles.
     * Due to output size constraints, I use a helper macro. */
    case 0x1a:
    {
        /* Spawn beam at heading+0x200 and heading-0x200 */
        #define SPAWN_BEAM_0x69(angleExpr) do { \
            uVar8 = (angleExpr) & 0x7ff; \
            if (DAT_00489248 < 0x9c4) { \
                int p = DAT_00489248 * 0x80 + (int)DAT_004892e8; \
                *(int *)(p) = *(int *)(iVar12 + (int)DAT_00487810); \
                *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810); \
                *(int *)(p + 0x18) = (sincos[uVar8] * 0xb4 >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810); \
                *(int *)(p + 0x1c) = (sincos[0x200 + uVar8] * 0xb4 >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810); \
                *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810); \
                *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810); \
                *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0; \
                *(unsigned char *)(p + 0x21) = 0x69; \
                *(unsigned short *)(p + 0x24) = 0; \
                *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0xf; \
                *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0; \
                *(int *)(p + 0x38) = typeTable[0x371d]; *(int *)(p + 0x44) = typeTable[0x372c]; \
                *(int *)(p + 0x48) = 0; *(int *)(p + 0x4c) = typeTable[0x3738]; \
                *(unsigned char *)(p + 0x54) = 0; *(unsigned char *)(p + 0x40) = 5; \
                *(int *)(p + 0x34) = typeTable[0x36f6]; *(int *)(p + 0x3c) = 0; \
                *(unsigned char *)(p + 0x5c) = 0; \
                DAT_00489248++; \
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 0x32; \
                iVar13 = (int)DAT_00487810; \
            } \
        } while(0)

        uVar8 = (*(int *)(iVar12 + 0x18 + iVar13) + 0x200U) & 0x7ff;
        SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + iVar13) + 0x200U);
        if (DAT_00489248 < 0x9c4) {
            SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x200U);
        }
        if (*(char *)(iVar12 + 0x35 + iVar13) != '\0') {
            SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) + 0x180U);
            if (DAT_00489248 < 0x9c4) {
                SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x180U);
            }
        }
        if (1 < *(unsigned char *)(iVar12 + 0x35 + iVar13)) {
            SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) + 0x280U);
            if (DAT_00489248 < 0x9c4) {
                SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x280U);
            }
        }
        if (2 < *(unsigned char *)(iVar12 + 0x35 + iVar13)) {
            SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x400U);
            if (DAT_00489248 < 0x9c4) {
                SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x380U);
                if (DAT_00489248 < 0x9c4) {
                    SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) + 0x380U);
                }
            }
        }
        if (3 < *(unsigned char *)(iVar12 + 0x35 + iVar13)) {
            SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) - 0x300U);
            if (DAT_00489248 < 0x9c4) {
                SPAWN_BEAM_0x69(*(int *)(iVar12 + 0x18 + (int)DAT_00487810) + 0x300U);
            }
        }
        #undef SPAWN_BEAM_0x69
        break;
    }

    /* ===== CASE 0x20: Edge record spawn ===== */
    case 0x20:
    {
        if (DAT_0048925c < 0x5dc) {
            int e = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
            *(int *)(e) = *(int *)(iVar12 + iVar13);
            *(int *)(e + 4) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(e + 8) = 0; *(int *)(e + 0xc) = 0;
            iVar13 = rand(); *(char *)(e + 0x10) = (char)(iVar13 % 3) + '\f';
            uVar8 = rand(); uVar8 &= 0x80000003;
            if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
            *(char *)(e + 0x11) = (char)uVar8;
            *(unsigned short *)(e + 0x12) = 0;
            *(unsigned char *)(e + 0x14) = uVar9; *(unsigned char *)(e + 0x15) = 0;
            DAT_0048925c++;
            *(unsigned char *)(DAT_0048925c * 0x20 + (int)DAT_00481f2c - 0xb) = 2;
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 0x21: Explosion record ===== */
    case 0x21:
    {
        if (DAT_0048926c < 100) {
            int e = DAT_0048926c * 0x20 + (int)DAT_00487a9c;
            *(int *)(e) = *(int *)(iVar12 + iVar13);
            *(int *)(e + 4) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(e + 8) = *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
            *(int *)(e + 0xc) = *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
            *(unsigned char *)(e + 0x18) = uVar9;
            *(int *)(e + 0x10) = *(int *)(iVar12 + 0x18 + (int)DAT_00487810);
            uVar8 = rand(); uVar8 &= 0x80000001;
            if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffe) + 1;
            *(char *)(e + 0x19) = (char)uVar8;
            iVar13 = rand(); *(char *)(e + 0x1a) = (char)(iVar13 % 0x50) + '\x14';
            *(int *)(e + 0x14) = 2000;
            DAT_0048926c++;
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 0x2b: Self-propelled projectile ===== */
    case 0x2b:
    {
        if (DAT_00489248 < 0x9c4) {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = *(int *)(iVar12 + 0x10 + (int)DAT_00487810) / 2;
            *(int *)(p + 0x1c) = 0;
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 0x2b;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0xff;
            *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[0x16a4]; *(int *)(p + 0x44) = typeTable[0x16b3];
            *(int *)(p + 0x48) = 0; *(int *)(p + 0x4c) = typeTable[0x16bf];
            *(unsigned char *)(p + 0x54) = 0; *(unsigned char *)(p + 0x40) = 0;
            *(int *)(p + 0x34) = typeTable[0x1682]; *(int *)(p + 0x3c) = 0;
            *(unsigned char *)(p + 0x5c) = 0;
            DAT_00489248++;
            piVar1 = (int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34);
            *piVar1 += (unsigned int)*(unsigned char *)(iVar12 + 0x2c + (int)DAT_00487810) * 100;
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 0x2c: Machinegun — rapid-fire projectile with recoil ===== */
    /* Original sets vx/vy=0 and relies on behavior callback at 0x438010 to
     * give velocity. Since we don't have the callback, compute velocity from
     * player heading with angular scatter for the chaotic stream effect. */
    case 0x2c:
    {
        if (DAT_00489248 < 0x9c4) {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            /* Compute velocity from heading with angular scatter */
            {
                int scatter = (rand() % 33) - 16;   /* ±16 angle units (~±3 degrees) */
                unsigned int fire_heading = (uVar7 + scatter) & 0x7FF;
                int speed = 500;
                *(int *)(p + 0x18) = (sincos[fire_heading] * speed) >> 6;
                *(int *)(p + 0x1c) = (sincos[(fire_heading + 0x200) & 0x7FF] * speed) >> 6;
            }
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 0x2c;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0xfe;
            *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x172a];
            *(int *)(p + 0x44) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x1739];
            *(int *)(p + 0x48) = 0;
            *(int *)(p + 0x4c) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x1745];
            *(unsigned char *)(p + 0x54) = 0;
            *(unsigned char *)(p + 0x40) = *(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810);
            *(int *)(p + 0x34) = typeTable[0x1708]; *(int *)(p + 0x3c) = 0;
            *(unsigned char *)(p + 0x5c) = 0;
            DAT_00489248++;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = *(int *)(iVar12 + 0x18 + (int)DAT_00487810);
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x70) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x6c) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            iVar13 = (int)DAT_00487810;
        }
        if (*(char *)(*(unsigned char *)(iVar12 + 0x35 + iVar13) + 0x5cc0 + (int)DAT_00487abc) != '\0') {
            piVar1 = (int *)(iVar12 + 0x10 + iVar13);
            *piVar1 -= sincos[uVar7] >> 7;
            *(int *)(iVar12 + 0x14 + (int)DAT_00487810) -= sincos[0x200 + uVar7] >> 7;
            *(unsigned char *)(iVar12 + 0xc4 + (int)DAT_00487810) = 3;
            uVar7 = rand(); uVar7 &= 0x800007ff;
            if ((int)uVar7 < 0) uVar7 = (uVar7 - 1 | 0xfffff800) + 1;
            *(int *)(iVar12 + 0x10 + (int)DAT_00487810) -= sincos[uVar7] >> 5;
            *(int *)(iVar12 + 0x14 + (int)DAT_00487810) -= sincos[0x200 + uVar7] >> 5;
            uVar7 = rand(); uVar7 &= 0x800007ff;
            if ((int)uVar7 < 0) uVar7 = (uVar7 - 1 | 0xfffff800) + 1;
            *(int *)(iVar12 + (int)DAT_00487810) += sincos[uVar7];
            piVar1 = (int *)(iVar12 + 4 + (int)DAT_00487810);
            *piVar1 += sincos[0x200 + uVar7];
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 0x2d: Laser — fast penetrating beam ===== */
    /* Original behavior callback at 0x0043f990 (shared with 0x2C rocket):
     *   vx = sincos[direction] * 2, vy = sincos[(dir+0x200)&0x7FF] * 2
     *   Runs position+collision loop TWICE per tick (very fast beam).
     *   Penetrates soft tiles, only stops at hard walls (tile flag 0x08).
     * We set velocity at spawn since direction doesn't change for lasers. */
    case 0x2d:
    {
        if (DAT_00489248 < 0x9c4) {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            /* Compute velocity from player heading (original callback formula) */
            {
                unsigned int heading = uVar7 & 0x7FF;
                *(int *)(p + 0x18) = sincos[heading] * 2;
                *(int *)(p + 0x1c) = sincos[(heading + 0x200) & 0x7FF] * 2;
            }
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(unsigned char *)(p + 0x21) = 0x2d;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = 0; *(unsigned char *)(p + 0x26) = 0xfe;
            *(unsigned char *)(p + 0x22) = uVar9; *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x17b0];
            *(int *)(p + 0x44) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x17bf];
            *(int *)(p + 0x48) = 0;
            *(int *)(p + 0x4c) = typeTable[*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) + 0x17cb];
            *(unsigned char *)(p + 0x54) = 0;
            *(unsigned char *)(p + 0x40) = *(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810);
            *(int *)(p + 0x34) = typeTable[0x178e]; *(int *)(p + 0x3c) = 0;
            *(unsigned char *)(p + 0x5c) = 0;
            DAT_00489248++;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 0x32;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = *(int *)(iVar12 + 0x18 + (int)DAT_00487810);
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x70) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x6c) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            iVar13 = (int)DAT_00487810;
        }
        break;
    }

    /* ===== CASE 0x5a: Delayed fire ===== */
    case 0x5a:
        *(int *)(iVar12 + 0xa4 + iVar13) = 400;
        iVar13 = (int)DAT_00487810;
        break;

    /* ===== DEFAULT + many explicit cases: Generic projectile ===== */
    /* Cases 5,8,9,0xb,0xe,0xf,0x11,0x13,0x14,0x16,0x17,0x18,0x1b-0x1f,0x22-0x2a,0x2e-0x59 */
    default:
    {
        if (0x9c3 < DAT_00489248) break;
        unsigned int param1_byte = 0;
        unsigned int localc_byte = 0x12;
        local_8 = uVar7;

        if (local_14 == 0xb || local_14 == 0xf || local_14 == 0x18 ||
            local_14 == 0x1d || local_14 == 0x25 || local_14 == 0x24 || local_14 == 0x27)
            local_8 = (uVar7 - 0x400) & 0x7ff;

        if (local_14 == 8) localc_byte = 0x12;
        else if (local_14 == 0x1c) localc_byte = 0xff;
        else if (local_14 == 0xb) localc_byte = 0xff;
        else {
            if (local_14 == 0xf || local_14 == 0x28) localc_byte = 0xff;
            if (local_14 == 0x22) {
                if (*(char *)(iVar12 + 0x35 + iVar13) != '\0') { localc_byte = 0xff; param1_byte = 0x6e; }
            } else if (local_14 == 0x1b) localc_byte = 0x32;
            else if (local_14 == 0x23) localc_byte = 0x78;
            else if (local_14 == 9) localc_byte = 0x1e;
        }

        {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            int typeOff = local_14 * 0x86;
            unsigned char wl = *(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810);
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = (typeTable[wl + typeOff + 0x2b] * sincos[local_8] >> 6) + *(int *)(iVar12 + 0x10 + (int)DAT_00487810);
            *(int *)(p + 0x1c) = (typeTable[wl + typeOff + 0x2b] * sincos[0x200 + local_8] >> 6) + *(int *)(iVar12 + 0x14 + (int)DAT_00487810);
            *(int *)(p + 4) = *(int *)(iVar12 + (int)DAT_00487810);
            *(int *)(p + 0xc) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x10) = 0; *(int *)(p + 0x14) = 0;
            *(char *)(p + 0x21) = (char)local_14;
            *(unsigned short *)(p + 0x24) = 0;
            *(unsigned char *)(p + 0x20) = (unsigned char)param1_byte;
            *(unsigned char *)(p + 0x26) = (unsigned char)localc_byte;
            *(unsigned char *)(p + 0x22) = uVar9;
            *(int *)(p + 0x28) = 0;
            *(int *)(p + 0x38) = typeTable[wl + typeOff + 0x22];
            *(int *)(p + 0x44) = typeTable[wl + typeOff + 0x31];
            *(int *)(p + 0x48) = 0;
            *(int *)(p + 0x4c) = typeTable[wl + typeOff + 0x3d];
            *(unsigned char *)(p + 0x54) = 0;
            *(unsigned char *)(p + 0x40) = wl;
            *(int *)(p + 0x34) = typeTable[local_14 * 0x86];
            *(int *)(p + 0x3c) = 0;
            *(unsigned char *)(p + 0x5c) = 0;
        }
        DAT_00489248++;

        /* Sound assignment for weapon type 0 */
        if (local_14 == 0) {
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\0') {
                uVar8 = rand(); uVar8 &= 0x80000003;
                if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffc) + 1;
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    *(unsigned short *)((int)DAT_00487aa8 + 0xb4 + uVar8 * 2) + 30000;
            }
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x01') {
                iVar13 = rand();
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    *(unsigned short *)((int)DAT_00487aa8 + 0xcc + (iVar13 % 5) * 2) + 30000;
            }
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x02') {
                iVar13 = rand();
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                    *(unsigned short *)((int)DAT_00487aa8 + 0xf2 + (iVar13 % 3) * 2) + 30000;
            }
        }

        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x70) = *(int *)(iVar12 + (int)DAT_00487810);
        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x6c) = *(int *)(iVar12 + 4 + (int)DAT_00487810);

        /* Type-specific post-spawn */
        if (local_14 == 0x16) {
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 0x12;
        } else if (local_14 == 0xb) {
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x01')
                *(int *)(iVar12 + 0x470 + (int)DAT_00487810) += 1;
        } else if (local_14 == 0x2e) {
            piVar1 = (int *)(iVar12 + 0x46c + (int)DAT_00487810); *piVar1 += 1;
        } else if (local_14 == 0x11) {
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = 1;
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x01')
                *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x60) = 0x1b;
            if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x02')
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x48) = (int)0xfffffff0;
        } else if (local_14 == 0x1d) {
            *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0;
            *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) = 0;
        } else if (local_14 == 0x27) {
            *(int *)(iVar12 + 0x468 + (int)DAT_00487810) += 1;
            *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
        } else {
            if (local_14 == 0x29 || local_14 == 0x2a) {
                piVar1 = (int *)(iVar12 + 0x478 + (int)DAT_00487810); *piVar1 += 1;
            }
            if (local_14 == 0x28) {
                *(int *)(iVar12 + 0x474 + (int)DAT_00487810) += 1;
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = uVar7;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x50) = 0x24;
            } else if (local_14 == 0x22) {
                if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\0') {
                    *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = uVar7;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
                    iVar13 = rand();
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x50) = iVar13 % 10 + 1;
                    uVar8 = rand(); uVar8 &= 0x80000001;
                    if ((int)uVar8 < 0) uVar8 = (uVar8 - 1 | 0xfffffffe) + 1;
                    *(char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x60) = (char)uVar8;
                } else {
                    *(int *)(iVar12 + 0x464 + (int)DAT_00487810) += 1;
                    *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = uVar7;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x50) = 0;
                }
            } else if (local_14 == 0x1b) {
                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = uVar7;
            } else {
                if (local_14 == 0x1c) {
                    *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = uVar7;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x20) = 0x157c;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x50) = 0;
                    goto LAB_00406a71;
                }
                if (local_14 == 0x26) {
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
                    iVar13 = rand();
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x44) = iVar13 % 0x7fb;
                    if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\0') {
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x68) = sincos[local_8] >> 3;
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x64) = sincos[0x200 + local_8] >> 3;
                    } else {
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x68) = 0;
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x64) = 0;
                    }
                    goto LAB_0040651d;
                }
                if (local_14 == 0xe) {
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x20) = 0x1130;
                    if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\0') {
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x68) = sincos[local_8] >> 3;
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x64) = sincos[0x200 + local_8] >> 3;
                    } else {
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x68) = 0;
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x64) = 0;
                    }
                    goto LAB_00406a71;
                }
                if (local_14 == 0x23) {
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x68) = sincos[local_8] >> 1;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x64) = sincos[0x200 + local_8] >> 1;
                    goto LAB_0040651d;
                }
                if (local_14 == 0x1f) {
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x5a) = 0xff;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x54) = 0;
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) = 0;
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x20) = 0x9c4;
LAB_00406a71:
                    piVar1 = (int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34);
                    *piVar1 += (unsigned int)*(unsigned char *)(iVar12 + 0x2c + (int)DAT_00487810) * 100;
                }
                else if (local_14 == 0xf || local_14 == 0x18) goto LAB_00406a71;

                if (local_14 == 0x18)
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x20) = 0x157c;
                else if (local_14 == 0xf) {
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x20) = 4000;
                    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0;
                }
                else if (local_14 == 0x17 && *(char *)(iVar12 + 0x35 + (int)DAT_00487810) == '\x01')
                    *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x58) = 0x19;
            }
        }

LAB_0040651d:
        {
            int pp = (int)DAT_004892e8 + DAT_00489248 * 0x80;
            iVar13 = (int)DAT_00487810;
            if (*(char *)((unsigned int)*(unsigned char *)(pp - 0x40) + local_14 * 0x218 + 0x130 + (int)DAT_00487abc) == '\x01') {
                unsigned int pIdx2 = 99;
                if (local_14 == 0xb) pIdx2 = 0;
                else if (local_14 == 0x17) pIdx2 = 1;
                else if (local_14 == 0xf) pIdx2 = 2;
                else if (local_14 == 0x18) pIdx2 = 3;
                else if (local_14 == 0x1f) pIdx2 = 4;
                else if (local_14 == 0x1c) pIdx2 = 5;
                else if (local_14 == 0xe) pIdx2 = 6;
                else if (local_14 == 0x2e) pIdx2 = 7;
                else if (local_14 == 0x27) pIdx2 = 8;
                *(unsigned char *)(pp - 0x24) = 6;
                *(int *)((int)DAT_0048781c + (pIdx2 * 0x1000 + DAT_00487834[pIdx2]) * 4) = DAT_00489248 - 1;
                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x30) = DAT_00487834[pIdx2];
                iVar13 = (int)DAT_00487810;
                DAT_00487834[pIdx2]++;
            }
        }
        break;
    }

    } /* end switch */

    /* Post-switch: Apply recoil from entity type table */
LAB_00402bc8:
    {
        unsigned char bVar2 = *(unsigned char *)(
            (unsigned int)*(unsigned char *)(iVar12 + 0x35 + iVar13) +
            local_14 * 0x218 + 0xa0 + (int)DAT_00487abc);
        if (bVar2 != 0) {
            piVar1 = (int *)(iVar12 + 0x10 + iVar13);
            *piVar1 -= (int)(((int)(sincos[uVar7] * (unsigned int)bVar2) >> 4) *
                       (unsigned int)*(unsigned char *)((int)&DAT_00483750 + 2)) >> 8;
            piVar1 = (int *)(iVar12 + 0x14 + (int)DAT_00487810);
            *piVar1 -= (int)(((int)((unsigned int)*(unsigned char *)(
                (unsigned int)*(unsigned char *)(iVar12 + 0x35 + (int)DAT_00487810) +
                local_14 * 0x218 + 0xa0 + (int)DAT_00487abc) *
                sincos[0x200 + uVar7]) >> 4) *
                (unsigned int)*(unsigned char *)((int)&DAT_00483750 + 2)) >> 8;
        }
    }
}

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
/* vol_override: 0xFF = use volume from g_SoundTable, else use this value directly */
void FUN_0040f9b0(int snd, int x, int y, int vol_override, int param5)
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
            /* Volume: use table volume or override, scaled by inverse distance */
            int vol_byte;
            if ((vol_override & 0xFF) != 0xFF) {
                vol_byte = vol_override & 0xFF;
            } else {
                vol_byte = (int)*(unsigned char *)((int)g_SoundTable + snd * 8 + 4);
            }
            /* Original formula: vol = (int)(vol_byte / minDist) * 2
             * Boosted ×4 for modern Windows audio stack (WASAPI vs DirectSound) */
            iVar7 = (int)((float)vol_byte / minDist) * 8;

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

                            /* Second emitter particle on the OPPOSITE side.
                             * Original uses (int)((double)heading + 1536.0) & 0x7FF
                             * = heading + 0x600 (270° offset, i.e. left side).
                             * First emitter is at heading + 0x200 (right side). */
                            if (DAT_00489248 < 0xA28) {
                                unsigned int uVar6b = (heading + 0x600) & 0x7FF;
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

/* ===== FUN_0044e3b0 — Force Field Attraction/Repulsion ===== */
/* Scans category-6 indexed entities (force field projectiles) for the first
 * enemy projectile within range.  If the projectile's flag at +0x40 is set,
 * attract the entity towards it; otherwise repel. */
static void FUN_0044e3b0(int *ent)
{
    int ex = ent[0];
    int ey = ent[1];
    unsigned int i = 0;

    if (DAT_0048784c == 0) return;

    int *pIdx = (int *)((int)DAT_0048781c + 0x18000);
    for (;;) {
        int *proj = (int *)(*pIdx * 0x80 + (int)DAT_004892e8);

        /* Check: different team AND within ±0x1040000 on both axes */
        if (*(char *)((int)DAT_00487810 + 0x2c +
             (unsigned int)*(unsigned char *)((int)proj + 0x22) * 0x598) !=
            (char)ent[0xb] &&
            ex - 0x1040000 < *proj && *proj < ex + 0x1040000 &&
            ey - 0x1040000 < proj[2] && proj[2] < ey + 0x1040000)
        {
            break;  /* found matching enemy projectile */
        }

        i++;
        pIdx++;
        if (DAT_0048784c <= i) return;  /* no match found */
    }

    int off = *pIdx * 0x80;
    unsigned long long angle = FUN_004257e0(
        *(int *)(off + (int)DAT_004892e8),
        *(int *)(off + 8 + (int)DAT_004892e8),
        ex, ey);
    int a = (int)angle;

    if (*(char *)(off + 0x40 + (int)DAT_004892e8) != '\0') {
        /* Attract: pull entity towards projectile (weaker force) */
        ent[4] += *(int *)((int)DAT_00487ab0 + a * 4) >> 2;
        ent[5] += *(int *)((int)DAT_00487ab0 + 0x800 + a * 4) >> 2;
    } else {
        /* Repel: push entity away from projectile (stronger force) */
        ent[4] -= *(int *)((int)DAT_00487ab0 + a * 4) >> 1;
        ent[5] -= *(int *)((int)DAT_00487ab0 + 0x800 + a * 4) >> 1;
    }
}

/* ===== FUN_0044e7a0 — Teleporter Check ===== */
/* Checks if entity is standing on a teleporter pad. If so, teleports it to
 * the paired destination with cooldown, sound, and visual effects. */
static void FUN_0044e7a0(int *ent)
{
    int bFound = 0;
    int i = 0;

    if (0 < DAT_00489264) {
        int off = 0;
        int base = (int)DAT_00487780;
        do {
            int ex = ent[0];
            if (*(int *)(base + off) - 0x280000 < ex &&
                ex < *(int *)(base + off) + 0x280000)
            {
                int ty = *(int *)(base + 4 + off);
                int ey = ent[1];
                if (ty - 0x280000 < ey && ey < ty + 0x280000) {
                    char team = *(char *)(base + 0x15 + off);
                    if (((char)ent[0xb] == team || team == '\x03') &&
                        (bFound = 1,
                         *(int *)(base + 8 + off) == 0) &&
                        *(char *)((int)ent + 0x47f) == '\0')
                    {
                        FUN_0040f9b0(7, ex, ey);
                        *(unsigned char *)((int)ent + 0x47f) = 0xfa;

                        int paired_base = base + 0x14;
                        int dest_off = (unsigned int)*(unsigned char *)(paired_base + off) * 0x20;

                        *(int *)((int)DAT_00487780 + 0xc + off) = 0x36;
                        *(int *)(dest_off + 0xc + (int)DAT_00487780) = 0x36;

                        ent[4] = 0;
                        ent[5] = 0;

                        FUN_004357b0(
                            *(int *)(dest_off + (int)DAT_00487780) >> 0x12,
                            *(int *)(dest_off + 4 + (int)DAT_00487780) >> 0x12,
                            9, 0, '\x02', 0, 0, 0, 0, '\0', '\0', 0xff);

                        base = (int)DAT_00487780;

                        /* Check if destination tile is walkable (type prop[1]==1) */
                        int dx = *(int *)(dest_off + (int)DAT_00487780) >> 0x12;
                        int dy = *(int *)(dest_off + 4 + (int)DAT_00487780) >> 0x12;
                        int tidx = dx + (dy << ((unsigned char)DAT_00487a18 & 0x1f));
                        if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + tidx) *
                            0x20 + 1 + (int)DAT_00487928) == '\x01')
                        {
                            *(unsigned char *)(ent + 0x128) = 0xff;
                            ent[0] = *(int *)(dest_off + (int)DAT_00487780);
                            ent[1] = *(int *)(dest_off + 4 + (int)DAT_00487780);
                            base = (int)DAT_00487780;
                        }
                    }
                }
            }
            i++;
            off += 0x20;
        } while (i < DAT_00489264);
    }

    /* Decrement cooldown timer */
    if (*(char *)((int)ent + 0x47f) != '\0') {
        *(char *)((int)ent + 0x47f) -= 1;
    }

    /* Reset cooldown if not touching any teleporter */
    if (!bFound) {
        *(unsigned char *)((int)ent + 0x47f) = 0;
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
/* Per-pixel sprite collision against tilemap using collision mask at sprite 0x19B.
 * Phase 1: Scan sprite footprint, count solid tile overlaps, accumulate collision normal.
 * Phase 2A (1-3 hits, entity alive): Replace solid tiles, apply drag, restore position.
 * Phase 2B (>3 hits or entity dead): Reflect velocity off surface, apply impact damage. */
static void FUN_00450630(int *ent, int player_idx)
{
    int col_x_sum = 0;   /* accumulated collision X offset */
    int col_y_sum = 0;   /* accumulated collision Y offset */
    int col_count = 0;   /* collision pixel count */
    int void_hit = 0;    /* hit a void tile (prop[0]==0) */

    /* Sprite 0x19B collision mask dimensions */
    int frame_off = *(int *)((int)DAT_00489234 + 0x66c);
    unsigned int spr_w = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b);
    unsigned int half_w = spr_w >> 1;
    unsigned char spr_h = *(unsigned char *)((int)DAT_00489e88 + 0x19b);
    unsigned int half_h = (unsigned int)(spr_h >> 1);

    /* Entity position in tile coords, centered on sprite */
    int sx = (ent[0] >> 0x12) - (int)half_w;
    int sy = (ent[1] >> 0x12) - (int)half_h;
    int tile_idx = (sy << ((unsigned char)DAT_00487a18 & 0x1f)) + sx;

    /* bVar2: if entity has active weapon slot AND weapon type == 0x0C,
     * use alternate passability check (offset 7 instead of 3) */
    int use_alt_pass = 0;
    if (ent[0x29] != 0 &&
        *(char *)((char)ent[0xd] + 0x3c + (int)ent) == '\x0c') {
        use_alt_pass = 1;
    }

    /* Phase 1: Scan sprite collision mask against tilemap */
    int row = 0;
    if (spr_h != 0) {
        do {
            int col = 0;
            if ((int)spr_w > 0) {
                do {
                    if (*(char *)((int)DAT_00489e94 + frame_off) != '\0') {
                        int tx = col + sx;
                        int ty = row + sy;
                        if (tx > 0 && tx < (int)DAT_004879f0 &&
                            ty > 0 && ty < (int)DAT_004879f4)
                        {
                            char *props = (char *)((unsigned int)*(unsigned char *)
                                ((int)DAT_0048782c + tile_idx) * 0x20 + (int)DAT_00487928);

                            /* Check passability: normal=props[3], flying=props[7] */
                            int blocked = 0;
                            if (props[3] == '\0' && !use_alt_pass) {
                                blocked = 1;
                            } else if (props[7] == '\0' && use_alt_pass) {
                                blocked = 1;
                            }

                            if (blocked) {
                                col_x_sum += (col - (int)half_w);
                                col_y_sum += (row - (int)half_h);
                                col_count++;
                                if (props[0xb] != '\0') {
                                    col_count = 0x14;  /* extra-solid tile */
                                }
                            }

                            /* Void tile check (prop[0] == 0) */
                            if (props[0] == '\0') {
                                void_hit = 1;
                            }
                        }
                    }
                    frame_off++;
                    tile_idx++;
                    col++;
                } while (col < (int)spr_w);
            }
            tile_idx += (int)DAT_00487a00 - (int)spr_w;
            row++;
        } while (row < (int)(unsigned int)spr_h);

        /* Energy explosion on void tile contact */
        if (void_hit && *(unsigned char *)((int)ent + 0xc6) != 0) {
            FUN_0044ea70_impl(ent);
        }

        if (col_count > 3)
            goto heavy_collision;
    }

    /* Light collision or no collision */
    if ((char)ent[9] == '\x01') {
        /* Dead entities always take the heavy path */
        goto heavy_collision;
    }

    if (col_count < 1) {
        return;  /* No collision */
    }

    /* Phase 2A: Light collision (1-3 pixels) — push through solid tiles */
    {
        int frame_off2 = *(int *)((int)DAT_00489234 + 0x66c);
        unsigned int w2 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b);
        int sx2 = (ent[0] >> 0x12) - (int)(w2 >> 1);
        int sy2 = (ent[1] >> 0x12) -
            (int)((unsigned int)(*(unsigned char *)((int)DAT_00489e88 + 0x19b)) >> 1);
        int idx2 = (sy2 << ((unsigned char)DAT_00487a18 & 0x1f)) + sx2;
        unsigned int h2 = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x19b);

        if (h2 != 0) {
            int r2 = 0;
            do {
                int c2 = 0;
                int tx2 = sx2;
                if (w2 > 0) {
                    do {
                        if (*(char *)((int)DAT_00489e94 + frame_off2) != '\0' &&
                            tx2 > 0 && tx2 < (int)DAT_004879f0 &&
                            sy2 > 0 && sy2 < (int)DAT_004879f4)
                        {
                            int ti = (unsigned int)*(unsigned char *)((int)DAT_0048782c + idx2) * 0x20
                                     + (int)DAT_00487928;
                            if (*(char *)(ti + 3) == '\0' && *(char *)(ti + 0xb) == '\0') {
                                /* Replace tile with its replacement (prop[0xe]) */
                                *(unsigned char *)((int)DAT_0048782c + idx2) =
                                    *(unsigned char *)(ti + 0xe);
                                /* Update color map */
                                unsigned char new_tile = *(unsigned char *)((int)DAT_0048782c + idx2);
                                if (*(char *)((unsigned int)new_tile * 0x20 + (int)DAT_00487928) == '\x01') {
                                    *(unsigned short *)((int)DAT_00481f50 + idx2 * 2) = 0;
                                } else {
                                    *(unsigned short *)((int)DAT_00481f50 + idx2 * 2) = DAT_0048384c;
                                }
                            }
                        }
                        frame_off2++;
                        idx2++;
                        c2++;
                        tx2++;
                    } while (c2 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b));
                }
                w2 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b);
                idx2 += (int)DAT_00487a00 - (int)w2;
                r2++;
                sy2++;
            } while (r2 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x19b));
        }

        /* Apply drag (multiply velocity by 0.95) */
        ent[4] = (int)((double)ent[4] * 0.95);
        ent[5] = (int)((double)ent[5] * 0.95);

        /* Restore to previous position */
        ent[0] = ent[2];
        ent[1] = ent[3];
    }
    return;

heavy_collision:
    {
        /* Compute absolute velocity sum */
        int abs_vx = ent[4];
        abs_vx = (abs_vx ^ (abs_vx >> 31)) - (abs_vx >> 31);
        int abs_vy = ent[5];
        abs_vy = (abs_vy ^ (abs_vy >> 31)) - (abs_vy >> 31);

        if (abs_vx + abs_vy <= 0x186a0) {
            /* Low speed: just stop */
            ent[4] = 0;
            ent[5] = 0;
        } else {
            /* Reflect velocity off collision surface */
            int wallAngle = FUN_004257e0(0, 0, col_x_sum, col_y_sum);
            int velAngle  = FUN_004257e0(0, 0, ent[4], ent[5]);
            int reflAngle = (wallAngle * 2 + 0x400 - velAngle) & 0x7ff;

            /* Compute speed magnitude */
            int vx6 = ent[4] >> 6;
            int vy6 = ent[5] >> 6;
            int squared = vx6 * vx6 + vy6 * vy6;
            double speed = sqrt((double)squared);

            /* Bounce factor from config: clamped to 64 */
            int bounce_raw = ((int)(unsigned char)g_ConfigBlob[0x1A12] *
                              (int)(unsigned char)g_ConfigBlob[0x17F6]) / 10;
            if (bounce_raw > 64) bounce_raw = 64;

            /* Compute reflected velocity using sin/cos LUT */
            /* Scale factor: 2^-19 (double at 0x4757e0 = 0x3EC0000000000000) */
            double lut_scale = 1.0 / 524288.0;  /* 2^-19 */

            int sinVal = *(int *)((int)DAT_00487ab0 + reflAngle * 4);
            int cosVal = *(int *)((int)DAT_00487ab0 + 0x800 + reflAngle * 4);

            ent[4] = (int)(speed * (double)sinVal * lut_scale * (double)bounce_raw);
            ent[5] = (int)(speed * (double)cosVal * lut_scale * (double)bounce_raw);

            /* Compute impact damage */
            int dmg_raw = (int)((double)(unsigned char)g_ConfigBlob[0x1A11] *
                                (double)(unsigned char)g_ConfigBlob[0x17F5] *
                                speed * 0.25f);

            if (dmg_raw > 0x45880 && (char)ent[9] == '\0') {
                /* Apply damage */
                ent[8] -= dmg_raw;
                /* Score: damage >> 13 */
                DAT_00486be8[player_idx] += (dmg_raw + (dmg_raw >> 31 & 0x1fff)) >> 0xd;
                /* Play random bounce sound */
                FUN_0040f9b0(rand() % 3 + 0x116, ent[0], ent[1]);
                /* Set hurt state */
                *(unsigned char *)((int)ent + 0xa3) = 1;
                *(unsigned char *)((int)ent + 0xc4) = 3;
            }
        }

        /* Restore to previous position */
        ent[0] = ent[2];
        ent[1] = ent[3];

        /* Phase 2B cleanup: clear solid tiles at restored position (only for alive entities) */
        if ((char)ent[9] == '\0') {
            int frame_off3 = *(int *)((int)DAT_00489234 + 0x66c);
            unsigned int w3 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b);
            int sx3 = (ent[2] >> 0x12) - (int)(w3 >> 1);
            int sy3 = (ent[3] >> 0x12) -
                (int)((unsigned int)(*(unsigned char *)((int)DAT_00489e88 + 0x19b)) >> 1);
            int idx3 = (sy3 << ((unsigned char)DAT_00487a18 & 0x1f)) + sx3;
            unsigned int h3 = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x19b);

            if ((int)h3 > 0) {
                int r3 = 0;
                do {
                    int c3 = 0;
                    int tx3 = sx3;
                    if ((int)w3 > 0) {
                        do {
                            if (*(char *)((int)DAT_00489e94 + frame_off3) != '\0' &&
                                tx3 > 0 && tx3 < (int)DAT_004879f0 &&
                                sy3 > 0 && sy3 < (int)DAT_004879f4)
                            {
                                int ti = (unsigned int)*(unsigned char *)((int)DAT_0048782c + idx3)
                                         * 0x20 + (int)DAT_00487928;
                                if (*(char *)(ti + 3) == '\0' && *(char *)(ti + 0xb) == '\0') {
                                    *(unsigned char *)((int)DAT_0048782c + idx3) =
                                        *(unsigned char *)(ti + 0xe);
                                    unsigned char new_tile = *(unsigned char *)
                                        ((int)DAT_0048782c + idx3);
                                    if (*(char *)((unsigned int)new_tile * 0x20 +
                                        (int)DAT_00487928) == '\x01') {
                                        *(unsigned short *)((int)DAT_00481f50 + idx3 * 2) = 0;
                                    } else {
                                        *(unsigned short *)((int)DAT_00481f50 + idx3 * 2) =
                                            DAT_0048384c;
                                    }
                                }
                            }
                            frame_off3++;
                            idx3++;
                            c3++;
                            tx3++;
                        } while (c3 < (int)(unsigned int)*(unsigned char *)
                                 ((int)DAT_00489e8c + 0x19b));
                    }
                    w3 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x19b);
                    idx3 += (int)DAT_00487a00 - (int)w3;
                    r3++;
                    sy3++;
                } while (r3 < (int)(unsigned int)*(unsigned char *)
                         ((int)DAT_00489e88 + 0x19b));
            }
        }
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

                /* Fire secondary weapon */
                if ((buttons & 0x10) && *(char *)((int)ent + 0xA2) == '\0' && ent[0x25] == 0) {
                    FUN_0044d860_impl((int *)ent, i);
                }
            }

            /* Detonate owned projectiles */
            FUN_0044d650_impl((int *)ent, i);

            /* Boundary clamp */
            FUN_0044e510((int *)ent);

            /* Entity type-specific behaviors */
            if (ent[0x29] != 0) {
                char anim_type = *(char *)((char)ent[0xD] + 0x3C + (int)ent);
                if (anim_type == 0x17 && *(char *)((int)ent + 0x35) == '\x01' &&
                    ent[0x29] % 5 == 0) {
                    FUN_00401000_impl(i);
                }
                if (anim_type == 0x0A) {
                    FUN_0044d930_impl((int *)ent);
                }
                if (anim_type == 0x0D && (ent[0x29] & 7) == 0) {
                    FUN_0044d9f0_impl((int *)ent);
                }
                if (anim_type == 0x12 && *(char *)((int)ent + 0xC6) == '\0' &&
                    ent[0x29] % 0x19 == 0) {
                    FUN_0044dbb0_impl((int *)ent, i);
                }
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
                FUN_0044e3b0((int *)ent);
            }

            /* Teleporter check */
            FUN_0044e7a0((int *)ent);
        }

        /* Read tile at current position */
        int tx = ent[0] >> 0x12;
        int ty = ent[1] >> 0x12;
        int shift = (unsigned char)DAT_00487a18 & 0x1F;
        unsigned short tile_val = *(unsigned char *)((int)DAT_0048782c + (ty << shift) + tx);

        /* Animation-type 0x0C entity behavior (uses tile data) */
        if (ent[0x29] != 0 &&
            *(char *)((char)ent[0xD] + 0x3C + (int)ent) == 0x0C) {
            FUN_0044ed90_impl((int *)ent, i, (unsigned int)tile_val);
        }

        /* Tile pickup interaction */

        /* Check if tile is walkable — clear underwater flag if so */
        if (*(char *)((int)DAT_00487928 + 0x18 + (unsigned int)tile_val * 0x20) == '\0' &&
            (char)ent[0x28] == '\0') {
            *(char *)((int)ent + 0x9E) = 0;
        }

        /* Wall collision (only if alive) */
        if (*(char *)((int)ent + 0x47C) == '\x01') {
            FUN_00450630((int *)ent, i);
        }

        /* Lava/pit death check + terrain weapon interaction */
        {
            int do_lava = 1;
            if ((int)ent[8] >= 1 && (int)ent[1] >= 0x80001 &&
                (int)ent[1] < (int)((DAT_004879f4 - 2) * 0x40000)) {
                /* In bounds with health: check tile enclosure for weapon selection */
                int ty2 = (int)ent[1] >> 0x12;
                int tx2 = (int)ent[0] >> 0x12;
                int sh2 = (unsigned char)DAT_00487a18 & 0x1F;
                unsigned char cur_walk = *(unsigned char *)((unsigned int)tile_val * 0x20 + 0x18 + (int)DAT_00487928);
                unsigned char below_idx = *(unsigned char *)((int)DAT_0048782c + ((ty2 + 1) << sh2) + tx2);
                unsigned char above_idx = *(unsigned char *)((int)DAT_0048782c + ((ty2 - 1) << sh2) + tx2);
                unsigned char below_walk = *(unsigned char *)((unsigned int)below_idx * 0x20 + 0x18 + (int)DAT_00487928);
                unsigned char above_walk = *(unsigned char *)((unsigned int)above_idx * 0x20 + 0x18 + (int)DAT_00487928);
                if (!((cur_walk == 0 && below_walk == 0 && above_walk == 0) ||
                      (*(char *)((unsigned int)tile_val * 0x20 + 1 + (int)DAT_00487928) != '\x01' &&
                       cur_walk == 0))) {
                    FUN_0044f900_impl((int *)ent, i);
                    do_lava = 0;
                }
            }
            if (do_lava && (char)ent[9] == '\0') {
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

        /* Pickup collision check */
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

        /* Water splash on water entry/exit (FUN_0044f840) */
        if ((char)ent[9] == '\0' && *(unsigned char *)((int)ent + 0x49E) == 0) {
            char *tile_rec = (char *)((unsigned int)tile_val * 0x20 + (int)DAT_00487928);
            if ((tile_rec[4] == '\x01' && *(char *)((int)ent + 0x49D) == '\0') ||
                (*tile_rec == '\x01' && *(char *)((int)ent + 0x49D) == '\x01')) {
                FUN_0044f840_impl((int *)ent);
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
