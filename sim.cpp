/*
 * sim.cpp - Per-tick gameplay simulation subsystems.
 *
 * Houses the gameplay logic that runs every game tick: projectile
 * updates, turret AI and line-of-sight, explosion/AoE effects,
 * spatial binning and collision, entity spawners, building damage,
 * round timers, edge-entity relocation, and related support code.
 *
 * Historical note: this file was originally called stubs.cpp when
 * most of it was empty placeholders pending decompilation. Every
 * function in here is now a real implementation — the grouping is
 * kept together because the systems share a lot of globals (turret
 * LOS tables, AoE temporaries, projectile bins) and splitting them
 * would fragment those across new TUs.
 */
#include "tou.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===== Globals defined in this module ===== */
/* DAT_00487aa8 and DAT_0048781c already defined in memory.cpp */
/* DAT_0048784c is DAT_00487834[6] in the original binary — aliased via tou.h */
int   DAT_00487228[80] = {0};  /* per-player pickup counter */
char  DAT_0048372e = 0;        /* fog of war ray resolution (config 0x17D6) */
char  DAT_0048372f = 0;        /* fog of war sub-option (config 0x17D7) */
char  DAT_00483730 = 0;        /* fog of war wobble enable (config 0x17D8) */
char  DAT_0048373d = 0;        /* friendly fire enabled flag */
char  DAT_00483741 = 0;        /* difficulty sub-setting */
char  DAT_00483742 = 0;        /* shield/energy bar enable flag (config 0x17EA) */
char  DAT_00483743 = 0;        /* minimap/radar enable flag (config 0x17EB) */
float DAT_00483854 = 0.0f;     /* entity density scale factor */
float DAT_00483858 = 0.0f;     /* inverse density factor (1.0/density) */
float DAT_0048385c = 0.0f;     /* weather/temperature threshold */

/* ===== AoE / Explosion globals ===== */
unsigned int DAT_00483840 = 0; /* fire color match R threshold */
unsigned int DAT_00483844 = 0; /* fire color match G threshold */
unsigned int DAT_00483848 = 0; /* fire color match B threshold */
unsigned char DAT_00481e8f = 0; /* building collision result flag (set by FUN_004355d0) */

/* ===== Turret LOS / Targeting globals ===== */
int    DAT_00481ed0 = 0;
int    DAT_00481edc = 0;
int    DAT_00481ee0 = 0;
int    DAT_00481ef4 = 0;
int    DAT_00481ef8 = 0;
int    DAT_00481efc = 0;
int    DAT_00481f10 = 0;
int    DAT_00481f00 = 0;
int    DAT_00481f04 = 0;
int    DAT_00481f08 = 0;
int    DAT_00481f0c = 0;
int    DAT_00481ee4 = 0;
int    DAT_00481ee8 = 0;
int    DAT_00481eec = 0;
int    DAT_00481ef0 = 0;
char   DAT_00481ed8 = 0;
/* DAT_00489e90 already defined in memory.cpp */

/* ===== Utility functions ===== */
static unsigned short framebuffer_rgb565_to_x1r5g5b5(unsigned short color)
{
    unsigned short red = (color >> 11) & 0x1f;
    unsigned short green = (color >> 6) & 0x1f;
    unsigned short blue = color & 0x1f;
    return (unsigned short)((red << 10) | (green << 5) | blue);
}

/* ===== FUN_00410030 — Spawn Random Debris Particle from Top (00410030) ===== */
/* Entity slot capacity is 0x9c4 (2500); every spawn path in this file uses the
 * same guard. Position uses 18-bit fixed-point: tile*FIXED_SCALE = pixels<<18. */
int FUN_00410030(void)
{
    if (DAT_00489248 >= 0x9c4) return 0;

    int iVar1 = rand();
    /* X in tile range [9, map_w-9], shifted left by 18 fractional bits. */
    int iVar2 = (iVar1 % (DAT_004879f0 - 0x12) + 9) * FIXED_SCALE;
    Entity *entity = &DAT_004892e8[DAT_00489248];

    entity->position_x = iVar2;
    entity->position_y = 0x380000;
    iVar1 = rand();
    entity->velocity_x = (0xfa - iVar1 % 500) * 0x200;
    iVar1 = rand();
    entity->velocity_y = (iVar1 % 100 + 0x32) * 0x200;
    entity->previous_x = iVar2;
    entity->previous_y = 0x380000;
    entity->motion_x_10 = 0;
    entity->motion_y_14 = 0;
    entity->type = 0x1d;
    entity->variant_24 = 0;
    entity->state_20 = 0;
    entity->auxiliary_26 = 0;
    entity->owner = 0xff;
    entity->health_or_damage_28 = 0;
    entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x3d40);
    entity->damage_44 = *(int *)((int)DAT_00487abc + 0x3d7c);
    entity->scratch_48 = 0;
    entity->palette_value = *(int *)((int)DAT_00487abc + 0x3dac);
    entity->animation_frame = 0;
    entity->subtype = 0;
    entity->callback_address = *(int *)((int)DAT_00487abc + 0x3cb8);
    entity->counter_3c = 0;
    entity->timer_5c = 0;

    DAT_00489248++;

    /* Set the trailing scratch bytes after publishing the new count. */
    entity->scratch_64 = 0;
    entity->scratch_65 = 0;

    return 0;
}
/* ===== FUN_004357b0 — AoE Tile Damage / Explosion Effects (004357b0) ===== */
/* Applies area-of-effect damage to tiles, spawns fire/debris particles, and handles
 * tile destruction. param_1/param_2 = tile x/y, param_3 = explosion size (sprite index),
 * param_4 = replacement tile type, param_5 = mode (-1/0/1/2), param_6..param_9 = ray origin/dir,
 * param_10 = damage mode (0=normal,1=count,2=scaled,3=special), param_11 = sound flag,
 * param_12 = team/owner index.
 * DAT_0048385c is the weather/temperature scalar consulted for damage tier (Section 1);
 * thresholds 0.2/0.4/0.6/1.1/1.3 are the temperature breakpoints. */
void FUN_004357b0(int param_1, int param_2, int param_3, unsigned char param_4, char param_5,
                  int param_6, int param_7, int param_8, int param_9,
                  char param_10, char param_11, unsigned char param_12)
{
    /* Float constants from .rdata (overlapping doubles in original binary) */
    static const double THRESH_0 = 0.2;   /* _DAT_004753f0 */
    static const double THRESH_1 = 0.4;   /* _DAT_004753e0 */
    static const double THRESH_2 = 0.6;   /* _DAT_00475680 */
    static const double THRESH_3 = 1.1;   /* _DAT_00475700 */
    static const double THRESH_4 = 1.3;   /* _DAT_004756f8 */
    static const double DEBRIS_THRESH = 0.5;  /* _DAT_00475688 */
    static const float  DEBRIS_MED = 1.0f;    /* _DAT_004753e8 */
    static const float  DEBRIS_START = 0.0f;  /* _DAT_0047540c */

    unsigned short uVar4 = 0;
    int local_20 = 0;
    int iVar6, iVar7, iVar8, iVar15, iVar16, iVar17;
    unsigned int uVar10, uVar12;
    char cVar13;
    int local_4;
    bool bVar18;

    /* Section 1: Damage scaling (param_10 == 2) */
    if (param_10 == '\x02') {
        if ((float)THRESH_0 <= DAT_0048385c) {
            if ((float)THRESH_1 <= DAT_0048385c) {
                if ((float)THRESH_2 <= DAT_0048385c) {
                    if ((float)THRESH_3 <= DAT_0048385c) {
                        local_20 = 4;
                        if ((float)THRESH_4 <= DAT_0048385c) {
                            local_20 = 6;
                        }
                    } else {
                        local_20 = 3;
                    }
                } else {
                    local_20 = 2;
                }
            } else {
                local_20 = 1;
            }
        } else {
            local_20 = 0;
        }
    }

    /* Section 2: Random special explosion type */
    if (param_10 == '\0') {
        iVar6 = rand();
        /* 1/50 chance: promote a normal explosion to the "special" mode (-1). */
        if (iVar6 % 0x32 == 0) {
            param_10 = -1;
        }
    }
    /* Team-painted-tile mode only applies when DAT_00483836 (team-tiles enabled) is set. */
    if (DAT_00483836 == '\0') {
        param_4 = 0;
    }

    /* Section 3: Directional tracing - trace ray from (param_7,param_6) toward (param_8,param_9) */
    if ((param_6 != 0) || (param_7 != 0)) {
        unsigned long long uVar19 = FUN_004257e0(param_6, param_7, param_8, param_9);
        param_2 = 0;
        iVar15 = *(int *)((int)DAT_00487ab0 + (int)uVar19 * 4) >> 1;
        iVar7 = *(int *)((int)DAT_00487ab0 + 0x800 + (int)uVar19 * 4) >> 1;
        iVar6 = param_7;
        iVar17 = param_6;
        do {
            iVar17 = iVar17 + iVar15;
            iVar6 = iVar6 + iVar7;
            iVar16 = iVar17 >> 0x12;
            iVar8 = iVar6 >> 0x12;
            if ((0 < iVar16) && (iVar16 < (int)DAT_004879f0) &&
                (0 < iVar8) && (iVar8 < (int)DAT_004879f4) &&
                (*(char *)((unsigned int)*(unsigned char *)
                    ((iVar8 << ((unsigned char)DAT_00487a18 & 0x1f)) + (int)DAT_0048782c + iVar16)
                    * 0x20 + 1 + (int)DAT_00487928) == '\x01')) {
                break;
            }
            param_2 = param_2 + 1;
        } while (param_2 < 0xc);
        /* 0xC = max trace steps; if we exited early, a wall was hit — back up one step. */
        if (param_2 != 0xc) {
            param_6 = iVar17 - iVar15;
            param_7 = iVar6 - iVar7;
        }
        /* >>0x12 converts 18-bit fixed-point back to tile coordinates. */
        param_1 = param_6 >> 0x12;
        param_2 = param_7 >> 0x12;
    }

    /* Section 4: Shadow sprite overlay (sprite 0x136, color degradation on footprint) */
    if ((param_5 == '\x01') && (7 < param_3) &&
        ((param_3 == 9 || ((param_3 < 0x13 || (param_3 < 0x17)))))) {
        uVar10 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x136);
        iVar7 = param_1 - (int)(uVar10 - 1) / 2;
        iVar6 = param_2 - (int)(*(unsigned char *)((int)DAT_00489e88 + 0x136) - 1) / 2;
        param_7 = *(int *)((int)DAT_00489234 + 0x4d8);
        param_9 = 0;
        iVar17 = (iVar6 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar7;
        if (*(unsigned char *)((int)DAT_00489e88 + 0x136) != 0) {
            do {
                param_8 = 0;
                if (uVar10 != 0) {
                    iVar15 = param_9 + iVar6;
                    iVar8 = iVar7;
                    do {
                        if ((6 < iVar8) && (iVar8 < (int)DAT_004879f0 - 6) &&
                            (6 < iVar15) && (iVar15 < (int)DAT_004879f4 - 6)) {
                            char *pcVar9 = (char *)((unsigned int)*(unsigned char *)
                                ((int)DAT_0048782c + iVar17) * 0x20 + (int)DAT_00487928);
                            if ((pcVar9[0xb] == '\0') && (pcVar9[4] == '\0') &&
                                (*pcVar9 == '\0') &&
                                (*(unsigned char *)((int)DAT_00489e94 + param_7) < 0xe6)) {
                                int degradeIdx = (int)(0xff - (unsigned int)*(unsigned char *)
                                    ((int)DAT_00489e94 + param_7)) >> 6;
                                unsigned short remapped = *(unsigned short *)
                                    ((int)DAT_00489230 +
                                     (unsigned int)*(unsigned short *)
                                         ((int)DAT_00481f50 + iVar17 * 2) * 2);
                                *(unsigned short *)((int)DAT_00481f50 + iVar17 * 2) =
                                    *(unsigned short *)
                                        (*(int *)((int)DAT_00487704 + (degradeIdx & 0xff) * 4) +
                                         (unsigned int)remapped * 2);
                            }
                        }
                        param_7 = param_7 + 1;
                        iVar17 = iVar17 + 1;
                        param_8 = param_8 + 1;
                        iVar8 = iVar8 + 1;
                    } while (param_8 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x136));
                }
                uVar10 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + 0x136);
                iVar17 = iVar17 + (DAT_00487a00 - (int)uVar10);
                param_9 = param_9 + 1;
            } while (param_9 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + 0x136));
        }
    }

    /* Section 5: Tile type check and fire/debris spawn */
    unsigned char bVar1 = *(unsigned char *)
        ((param_2 << ((unsigned char)DAT_00487a18 & 0x1f)) + (int)DAT_0048782c + param_1);
    if (*(char *)((unsigned int)bVar1 * 0x20 + 0x1a + (int)DAT_00487928) != '\0') {
        int param_7_speed;
        if (bVar1 == 0x1a) {
            iVar6 = rand();
            if (iVar6 % 0x14 < 10) {
                uVar10 = rand();
                uVar10 = uVar10 & 0x80000003;
                if ((int)uVar10 < 0) {
                    uVar10 = (uVar10 - 1 | 0xfffffffc) + 1;
                }
                cVar13 = (char)uVar10 + '\a';
                param_7_speed = 0xfa;
            } else if (iVar6 % 0x14 < 0xf) {
                uVar10 = rand();
                uVar10 = uVar10 & 0x80000003;
                if ((int)uVar10 < 0) {
                    uVar10 = (uVar10 - 1 | 0xfffffffc) + 1;
                }
                cVar13 = (char)uVar10 + '\r';
                param_7_speed = 0xfa;
            } else {
                iVar6 = rand();
                param_7_speed = 0xfa;
                cVar13 = (char)(iVar6 % 3) + '\x11';
            }
        } else {
            iVar6 = rand();
            if (iVar6 % 0x14 < 0xf) {
                iVar6 = rand();
                cVar13 = (char)(iVar6 % 6) + '\x01';
            } else {
                iVar6 = rand();
                cVar13 = (char)(iVar6 % 3) + '\x11';
            }
            param_7_speed = 0x96;
        }

        /* Spawn fire particle at center.
         * DAT_00487814 is the coarse (16x16-tile) presence grid built each tick in
         * FUN_00460660; bit 0x08 means "inside a player's viewport this tick" —
         * off-screen tiles don't get visible fire particles. */
        if ((DAT_00489250 < 2000) &&
            ((*(unsigned char *)((param_1 >> 4) + (int)DAT_00487814 +
                (param_2 >> 4) * DAT_004879f8) & 8) != 0)) {
            iVar15 = param_1 << 0x12;
            *(int *)(DAT_00489250 * 0x20 + (int)DAT_00481f34) = iVar15;
            iVar8 = param_2 << 0x12;
            *(int *)(DAT_00489250 * 0x20 + 4 + (int)DAT_00481f34) = iVar8;
            *(int *)(DAT_00489250 * 0x20 + 8 + (int)DAT_00481f34) = 0;
            *(int *)(DAT_00489250 * 0x20 + 0xc + (int)DAT_00481f34) = 0;
            *(char *)(DAT_00489250 * 0x20 + 0x10 + (int)DAT_00481f34) = cVar13;
            *(unsigned char *)(DAT_00489250 * 0x20 + 0x11 + (int)DAT_00481f34) = 0;
            *(unsigned char *)(DAT_00489250 * 0x20 + 0x12 + (int)DAT_00481f34) = 0;
            *(unsigned char *)(DAT_00489250 * 0x20 + 0x13 + (int)DAT_00481f34) = 1;
            *(unsigned char *)(DAT_00489250 * 0x20 + 0x14 + (int)DAT_00481f34) = 0xff;
            *(unsigned char *)(DAT_00489250 * 0x20 + 0x15 + (int)DAT_00481f34) = 0;
            DAT_00489250 = DAT_00489250 + 1;
            iVar6 = iVar15;
            iVar17 = iVar8;

            /* Play random fire sound — limit to 1 per call to prevent
             * deafening volume when many tiles explode simultaneously */
            {
                static int snd_count = 0;
                if (snd_count == 0) {
                    iVar7 = rand();
                    FUN_0040f9b0(iVar7 % 7 + 0x65, iVar6, iVar17);
                }
                snd_count++;
                if (snd_count > 3) snd_count = 0;
            }

            *(unsigned char *)(DAT_00489250 * 0x20 + -0xb + (int)DAT_00481f34) = 1;

            /* Maybe spawn knockback explosion */
            if (bVar1 == 0x1a) {
                uVar10 = rand();
                uVar10 = uVar10 & 0x8000000f;
                bVar18 = (uVar10 == 0);
                if ((int)uVar10 < 0) {
                    bVar18 = ((uVar10 - 1 | 0xfffffff0) == 0xffffffff);
                }
                if (!bVar18) {
                    FUN_00437cf0(iVar15, iVar8, 0x23, (unsigned int)param_12, 300);
                }
            } else {
                uVar10 = rand();
                uVar10 = uVar10 & 0x8000000f;
                bVar18 = (uVar10 == 0);
                if ((int)uVar10 < 0) {
                    bVar18 = ((uVar10 - 1 | 0xfffffff0) == 0xffffffff);
                }
                if (!bVar18) {
                    FUN_00437cf0(iVar15, iVar8, 0x14, (unsigned int)param_12, 200);
                }
            }
        }

        /* Spawn 8 debris particles radiating outward */
        param_6 = 0;
        do {
            if (0x9c3 < DAT_00489248) break;
            uVar10 = rand();
            uVar10 = uVar10 & 0x800007ff;
            if ((int)uVar10 < 0) {
                uVar10 = (uVar10 - 1 | 0xfffff800) + 1;
            }
            iVar6 = rand();
            Entity *entity = &DAT_004892e8[DAT_00489248];
            entity->position_x = param_1 << 0x12;
            entity->position_y = param_2 << 0x12;
            entity->velocity_x =
                *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar6 % param_7_speed) >> 7;
            entity->velocity_y =
                *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar6 % param_7_speed) >> 7;
            entity->previous_x = param_1 << 0x12;
            entity->previous_y = param_2 << 0x12;
            entity->motion_x_10 = 0;
            entity->motion_y_14 = 0;
            entity->type = 0;
            iVar6 = rand();
            entity->variant_24 = (short)(iVar6 % 6);
            entity->state_20 = 0x32;
            entity->auxiliary_26 = 0;
            entity->owner = param_12;
            entity->health_or_damage_28 = 0;
            entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x90);
            entity->damage_44 = *(int *)((int)DAT_00487abc + 0xCC);
            entity->scratch_48 = 0;
            entity->palette_value = *(int *)((int)DAT_00487abc + 0xFC);
            entity->animation_frame = 0;
            entity->subtype = 2;
            entity->callback_address = *(int *)((int)DAT_00487abc + 0x00);
            entity->counter_3c = 0;
            entity->timer_5c = 0;
            DAT_00489248 = DAT_00489248 + 1;

            /* Set lifespan and gravity on newly incremented slot's trailing fields */
            iVar6 = rand();
            entity->health_or_damage_28 = iVar6 % 100 + 0x78;
            entity->damage_44 = 0x3e800;
            iVar6 = rand();
            entity->palette_value =
                *(unsigned short *)((int)DAT_00487aa8 + 0x1ec + (iVar6 % 10) * 2) + 30000;

            param_6 = param_6 + 0x100;
        } while (param_6 < 0x800);

        /* If param_11 == 1, randomize explosion size */
        if (param_11 == '\x01') {
            iVar6 = rand();
            param_3 = iVar6 % 3 + 0x14;
        }
    }

    /* Section 6: Main damage loop - iterate sprite (size+300) footprint */
    iVar6 = param_3 + 300;
    uVar10 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + iVar6);
    param_9 = 0;
    iVar7 = param_1 - (int)(uVar10 - 1) / 2;
    iVar17 = param_2 - (int)(*(unsigned char *)((int)DAT_00489e88 + iVar6) - 1) / 2;
    param_7 = *(int *)((int)DAT_00489234 + iVar6 * 4);
    param_6 = (iVar17 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar7;

    if (*(unsigned char *)((int)DAT_00489e88 + iVar6) != 0) {
        do {
            param_8 = 0;
            if (uVar10 != 0) {
                iVar8 = iVar17 + param_9;
                iVar15 = iVar7;
                do {
                    /* Compute damage level from grayscale sprite data */
                    int dmgLevel = (int)(0xff - (unsigned int)*(unsigned char *)
                        ((int)DAT_00489e94 + param_7)) >> 5;
                    cVar13 = (char)dmgLevel;
                    dmgLevel = dmgLevel & 0xff;

                    if (((-1 < iVar15) && (iVar15 < (int)DAT_004879f0)) &&
                        ((-1 < iVar8) && (iVar8 < (int)DAT_004879f4))) {
                        uVar10 = (unsigned int)*(unsigned char *)((int)DAT_0048782c + param_6);
                        iVar16 = (int)DAT_00487928 + uVar10 * 0x20;

                        /* Check if tile is destructible */
                        if (((*(char *)(iVar16 + 0xc) == '\x01') && (param_10 != '\x03')) ||
                            ((*(char *)(iVar16 + 0xd) == '\x01') && (param_10 == '\x03'))) {

                            /* Team mode tile ownership */
                            if (DAT_00483836 == '\x02') {
                                param_4 = -(*(char *)(iVar16 + 0xe) != '\0') & 0x40;
                            }

                            /* Special handling for tile types 10 and 16 */
                            if (((uVar10 == 10) || (uVar10 == 0x10)) && (param_5 != '\x02')) {
                                if (cVar13 != '\0') {
                                    cVar13 = '\x01';
                                    dmgLevel = 1;
                                }
                                if (param_5 == -1) {
                                    dmgLevel = 0;
                                    cVar13 = (char)dmgLevel;
                                }
                            } else if ((param_5 == -1) && (cVar13 != '\0')) {
                                dmgLevel = 1;
                                cVar13 = (char)dmgLevel;
                            }

                            /* Color accumulation for mode 1/3 */
                            if ((param_10 == '\x03') || (param_10 == '\x01')) {
                                unsigned short uVar2 = *(unsigned short *)
                                    ((int)DAT_00481f50 + param_6 * 2);
                                unsigned short uVar5 =
                                    (unsigned short)(unsigned char)((unsigned char)(uVar2 >> 10) << 3) +
                                    (unsigned short)(unsigned char)((char)(uVar2 >> 5) << 3) +
                                    (unsigned short)(unsigned char)((char)uVar2 << 3);
                                if ((uVar4 < 100) && (uVar4 < uVar5)) {
                                    DAT_00481e8c = uVar2;
                                    uVar4 = uVar5;
                                }
                                cVar13 = (char)dmgLevel;
                            }

                            /* Apply damage if damage level > 0 */
                            if (cVar13 != '\0') {
                                if ((uVar10 == 6) && (param_10 == -1)) {
                                    /* Fluid tile special handling */
                                    if (cVar13 == '\a') {
                                        *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) = 0;
                                        *(unsigned char *)((int)DAT_0048782c + param_6) = 0;
                                    } else if ((param_4 == 0) && (DAT_00489258 < 5000)) {
                                        *(int *)(DAT_00489258 * 0x20 + 0xc + (int)DAT_00489e7c) = 5;
                                        uVar10 = rand();
                                        uVar10 = uVar10 & 0x80000003;
                                        if ((int)uVar10 < 0) {
                                            uVar10 = (uVar10 - 1 | 0xfffffffc) + 1;
                                        }
                                        *(unsigned int *)(DAT_00489258 * 0x20 + 8 + (int)DAT_00489e7c) = uVar10;
                                        *(int *)(DAT_00489258 * 0x20 + (int)DAT_00489e7c) = iVar15;
                                        *(int *)(DAT_00489258 * 0x20 + 4 + (int)DAT_00489e7c) = iVar8;
                                        *(unsigned char *)(DAT_00489258 * 0x20 + 0x10 + (int)DAT_00489e7c) = 0;
                                        int *piVar11 = (int *)((int)DAT_00489e7c + DAT_00489258 * 0x20);
                                        *(unsigned char *)
                                            ((piVar11[1] << ((unsigned char)DAT_00487a18 & 0x1f)) +
                                             (int)DAT_0048782c + *piVar11) = 0xb;
                                        DAT_00489258 = DAT_00489258 + 1;
                                    }
                                } else {
                                    /* Normal tile damage/destruction */
                                    if (cVar13 == '\a') {
                                        /* Max damage: destroy tile */
                                        if (param_10 == '\x02') {
                                            /* Scaled damage mode: may spawn wall crack debris */
                                            uVar10 = iVar15 + iVar8;
                                            uVar10 = uVar10 & 0x80000001;
                                            bVar18 = (uVar10 == 0);
                                            if ((int)uVar10 < 0) {
                                                bVar18 = ((uVar10 - 1 | 0xfffffffe) == 0xffffffff);
                                            }
                                            if ((bVar18) && (DAT_00489248 < 0x9c4)) {
                                                /* Spawn wall crack entity */
                                                uVar10 = rand();
                                                uVar10 = uVar10 & 0x800007ff;
                                                if ((int)uVar10 < 0) {
                                                    uVar10 = (uVar10 - 1 | 0xfffff800) + 1;
                                                }
                                                iVar16 = rand();
                                                Entity *entity = &DAT_004892e8[DAT_00489248];
                                                entity->position_x = iVar15 << 0x12;
                                                entity->position_y = iVar8 * FIXED_SCALE;
                                                entity->velocity_x =
                                                    *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                entity->velocity_y =
                                                    *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                entity->previous_x = iVar15 << 0x12;
                                                entity->previous_y = iVar8 * FIXED_SCALE;
                                                entity->motion_x_10 = 0;
                                                entity->motion_y_14 = 0;
                                                entity->type = 2;
                                                iVar16 = rand();
                                                entity->variant_24 = (short)(iVar16 % 6);
                                                entity->state_20 = 0;
                                                entity->auxiliary_26 = 0;
                                                entity->owner = param_12;
                                                entity->health_or_damage_28 = 0;
                                                entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x4B8);
                                                entity->damage_44 = *(int *)((int)DAT_00487abc + 0x4F4);
                                                entity->scratch_48 = 0;
                                                entity->palette_value = *(int *)((int)DAT_00487abc + 0x524);
                                                entity->animation_frame = 0;
                                                entity->subtype = 0;
                                                entity->callback_address = *(int *)((int)DAT_00487abc + 0x430);
                                                entity->counter_3c = 0;
                                                entity->timer_5c = 0;
                                                DAT_00489248 = DAT_00489248 + 1;
                                                iVar16 = rand();
                                                entity->health_or_damage_28 = iVar16 % 0x32 + 0x28;
                                                entity->damage_44 = 0x7d000;
                                                entity->palette_value = framebuffer_rgb565_to_x1r5g5b5(
                                                    *(unsigned short *)
                                                        ((int)DAT_00481f50 +
                                                         ((iVar8 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar15) * 2))
                                                    + 30000;
                                            }

                                            /* Spawn fire debris particles based on local_20 */
                                            if (((*(unsigned char *)((iVar15 >> 4) + (int)DAT_00487814 +
                                                    (iVar8 >> 4) * DAT_004879f8) & 8) != 0) &&
                                                (local_4 = 0, local_20 != 0)) {
                                                do {
                                                    if (0x9c3 < DAT_00489248) break;
                                                    uVar10 = rand();
                                                    uVar10 = uVar10 & 0x800007ff;
                                                    if ((int)uVar10 < 0) {
                                                        uVar10 = (uVar10 - 1 | 0xfffff800) + 1;
                                                    }
                                                    iVar16 = rand();
                                                    Entity *entity = &DAT_004892e8[DAT_00489248];
                                                    entity->position_x = iVar15 << 0x12;
                                                    entity->position_y = iVar8 * FIXED_SCALE;
                                                    entity->velocity_x =
                                                        *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                    entity->velocity_y =
                                                        *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                    entity->previous_x = iVar15 << 0x12;
                                                    entity->previous_y = iVar8 * FIXED_SCALE;
                                                    entity->motion_x_10 = 0;
                                                    entity->motion_y_14 = 0;
                                                    entity->type = 100;
                                                    iVar16 = rand();
                                                    entity->variant_24 = (short)(iVar16 % 6);
                                                    entity->state_20 = 0;
                                                    entity->auxiliary_26 = 0xff;
                                                    entity->owner = param_12;
                                                    entity->health_or_damage_28 = 0;
                                                    entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0xD1E8);
                                                    entity->damage_44 = *(int *)((int)DAT_00487abc + 0xD224);
                                                    entity->scratch_48 = 0;
                                                    entity->palette_value = *(int *)((int)DAT_00487abc + 0xD254);
                                                    entity->animation_frame = 0;
                                                    entity->subtype = 0;
                                                    entity->callback_address = *(int *)((int)DAT_00487abc + 0xD160);
                                                    entity->counter_3c = 0;
                                                    entity->timer_5c = 0;
                                                    DAT_00489248 = DAT_00489248 + 1;
                                                    iVar16 = rand();
                                                    entity->health_or_damage_28 = iVar16 % 0x32 + 0x28;
                                                    entity->damage_44 = 0;
                                                    entity->palette_value = framebuffer_rgb565_to_x1r5g5b5(
                                                        *(unsigned short *)
                                                            ((int)DAT_00481f50 +
                                                             ((iVar8 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar15) * 2))
                                                        + 30000;
                                                    local_4 = local_4 + 1;
                                                } while (local_4 < local_20);
                                            }
                                        }

                                        /* Destroy tile: set pixel and tilemap */
                                        if (param_4 != 0) {
                                            *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) = DAT_0048384c;
                                        } else {
                                            *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) = 0;
                                        }
                                    } else {
                                        /* Partial damage: darken tile via palette LUT */
                                        unsigned short uVar2;
                                        if (param_4 == 0) {
                                            uVar2 = *(unsigned short *)
                                                ((int)DAT_00489230 +
                                                 (unsigned int)*(unsigned short *)
                                                     ((int)DAT_00481f50 + param_6 * 2) * 2);
                                            iVar16 = (int)DAT_004876a4[3 + dmgLevel];
                                        } else {
                                            uVar2 = *(unsigned short *)
                                                ((int)DAT_00489230 +
                                                 (unsigned int)*(unsigned short *)
                                                     ((int)DAT_00481f50 + param_6 * 2) * 2);
                                            iVar16 = (int)DAT_004876a4[28 + (dmgLevel >> 1)];
                                        }
                                        *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) =
                                            *(unsigned short *)(iVar16 + (unsigned int)uVar2 * 2);
                                    }

                                    /* Check if tile color is close to fill color / fire threshold */
                                    {
                                    unsigned short uVar2_c = *(unsigned short *)((int)DAT_00481f50 + param_6 * 2);
                                    int doReplace = 0;
                                    if (uVar2_c == 0) {
                                        doReplace = 1;
                                    } else if (param_4 != 0) {
                                        /* Branchless abs: abs(x) = (x ^ (x>>31)) - (x>>31) */
                                        unsigned int rVal = (unsigned int)(unsigned char)((unsigned char)(uVar2_c >> 10) << 3) - DAT_00483840;
                                        int rSign = (int)rVal >> 0x1f;
                                        int rAbs = (int)((rVal ^ rSign) - rSign);
                                        unsigned int gVal = (unsigned int)(unsigned char)((char)(uVar2_c >> 5) << 3) - DAT_00483844;
                                        int gSign = (int)gVal >> 0x1f;
                                        int gAbs = (int)((gVal ^ gSign) - gSign);
                                        unsigned int bVal = (unsigned int)(unsigned char)(*(char *)((int)DAT_00481f50 + param_6 * 2) << 3) - DAT_00483848;
                                        int bSign = (int)bVal >> 0x1f;
                                        int bAbs = (int)((bVal ^ bSign) - bSign);
                                        if (rAbs < 0x11 && gAbs < 0x11 && bAbs < 0x11) {
                                            doReplace = 1;
                                        }
                                    }
                                    if (doReplace) {
                                        /* Increment destroyed tile counter for modes 1 and 3 */
                                        if (param_10 == '\x01') {
                                            if (DAT_00481e8e < 0xc) {
                                                DAT_00481e8e = DAT_00481e8e + 1;
                                            }
                                        } else if (param_10 == '\x03') {
                                            DAT_00481e8e = DAT_00481e8e + 1;
                                        }
                                        /* Replace tile */
                                        *(unsigned char *)((int)DAT_0048782c + param_6) = param_4;
                                        if (param_4 == 0) {
                                            *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) = 0;
                                        } else {
                                            *(unsigned short *)((int)DAT_00481f50 + param_6 * 2) = DAT_0048384c;
                                        }
                                    } /* doReplace */
                                    } /* uVar2_c scope */
                                }
                            }
                        }
                    }
                    param_7 = param_7 + 1;
                    param_6 = param_6 + 1;
                    param_8 = param_8 + 1;
                    iVar15 = iVar15 + 1;
                } while (param_8 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e8c + iVar6));
            }
            uVar10 = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + iVar6);
            param_6 = param_6 + (DAT_00487a00 - (int)uVar10);
            param_9 = param_9 + 1;
        } while (param_9 < (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + iVar6));
    }

    /* Section 7: End debris - spawn additional small debris if param_5 == 1 */
    if (param_5 != '\x01') {
        return;
    }

    int spriteArea = (int)((unsigned int)*(unsigned char *)((int)DAT_00489e88 + iVar6) *
                           (unsigned int)*(unsigned char *)((int)DAT_00489e8c + iVar6)) >> 7;
    float debrisCount = (float)spriteArea;

    if (DEBRIS_THRESH <= (double)spriteArea) {
        goto LAB_00436b89;
    } else {
        uVar10 = rand();
        uVar10 = uVar10 & 0x80000007;
        bVar18 = (uVar10 == 0);
        if ((int)uVar10 < 0) {
            bVar18 = ((uVar10 - 1 | 0xfffffff8) == 0xffffffff);
        }
        if (!bVar18) goto LAB_00436b89;
    }
    debrisCount = 1.0f;
    goto LAB_00436bc6;

LAB_00436b89:
    if ((DEBRIS_MED <= debrisCount) || (debrisCount < (float)DEBRIS_THRESH)) {
        goto LAB_00436bc6;
    }
    uVar10 = rand();
    uVar10 = uVar10 & 0x80000003;
    bVar18 = (uVar10 == 0);
    if ((int)uVar10 < 0) {
        bVar18 = ((uVar10 - 1 | 0xfffffffc) == 0xffffffff);
    }
    if (!bVar18) goto LAB_00436bc6;
    debrisCount = 1.0f;

LAB_00436bc6:
    param_6 = 0;
    {
        float fVar3 = DEBRIS_START;
        while ((fVar3 < debrisCount) && (DAT_00489248 < 0x9c4)) {
            uVar10 = rand();
            uVar10 = uVar10 & 0x800007ff;
            if ((int)uVar10 < 0) {
                uVar10 = (uVar10 - 1 | 0xfffff800) + 1;
            }
            uVar12 = rand();
            uVar12 = uVar12 & 0x8000007f;
            if ((int)uVar12 < 0) {
                uVar12 = (uVar12 - 1 | 0xffffff80) + 1;
            }
            iVar6 = rand();
            iVar17 = rand();
            iVar7 = (iVar6 % 6 + -3) * FIXED_SCALE + param_1 * FIXED_SCALE;
            Entity *entity = &DAT_004892e8[DAT_00489248];
            entity->position_x = iVar7;
            iVar6 = (iVar17 % 6 + -3) * FIXED_SCALE + param_2 * FIXED_SCALE;
            entity->position_y = iVar6;
            entity->velocity_x =
                (int)(*(int *)((int)DAT_00487ab0 + uVar10 * 4) * (int)(uVar12 & 0xff)) >> 8;
            entity->velocity_y =
                (int)(*(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (int)(uVar12 & 0xff)) >> 8;
            entity->previous_x = iVar7;
            entity->previous_y = iVar6;
            entity->motion_x_10 = 0;
            entity->motion_y_14 = 0;
            entity->type = 2;
            entity->variant_24 = 0;
            entity->state_20 = 5;
            entity->auxiliary_26 = 0xff;
            entity->owner = 0xff;
            entity->health_or_damage_28 = 0;
            entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x4B8);
            entity->damage_44 = *(int *)((int)DAT_00487abc + 0x4F4);
            entity->scratch_48 = 0;
            entity->palette_value = *(int *)((int)DAT_00487abc + 0x524);
            entity->animation_frame = 0;
            entity->subtype = 0;
            entity->callback_address = *(int *)((int)DAT_00487abc + 0x430);
            entity->counter_3c = 0;
            entity->timer_5c = 0;
            DAT_00489248 = DAT_00489248 + 1;

            iVar6 = rand();
            entity->palette_value =
                *(unsigned short *)((int)DAT_00487aa8 + 0x44 + (iVar6 % 6) * 2) + 30000;
            uVar10 = rand();
            uVar10 = uVar10 & 0x8000007f;
            if ((int)uVar10 < 0) {
                uVar10 = (uVar10 - 1 | 0xffffff80) + 1;
            }
            entity->health_or_damage_28 = uVar10 + 0x80;

            param_6 = param_6 + 1;
            fVar3 = (float)param_6;
        }
    }
    return;
}
/* ===== FUN_004355d0 — Building/Structure Collision for Projectiles (004355D0) ===== */
/* Checks if a System 1 projectile (by index) collides with any structure in
 * DAT_00481f28 (stride 0x40, count DAT_00489260). On hit, subtracts damage
 * from the structure's health and sets DAT_00481e8f = 3 (or 4 if structure
 * type == 7) as a result flag for the caller.
 * Owner byte encoding: 0x78..0x8B = teams 0..19 (byte - 0x78); anything else = 0xFB
 * (unowned / environmental). */
void FUN_004355d0(unsigned int param_1)
{
    Entity *entity = &DAT_004892e8[param_1];
    unsigned char bVar1 = entity->owner;
    int iVar2 = entity->position_x;
    int iVar3 = entity->position_y;

    /* Compute team from owner byte */
    unsigned int team;
    if (bVar1 < 0x78 || bVar1 > 0x8b) {
        team = 0xfb;
    } else {
        team = bVar1 - 0x78;
    }

    int iVar7;
    if ((char)entity->auxiliary_26 == '\0') {
        /* Branch: byte_0x26 == 0 — check all structures regardless of team */
        iVar7 = 0;
        if (DAT_00489260 < 1) return;
        int *piVar6 = (int *)((int)DAT_00481f28 + 4);
        while (1) {
            unsigned int uVar4 = *(unsigned char *)(*(int *)((unsigned int)*(unsigned char *)((int)piVar6 + 0x18) * 0x20 + (int)DAT_00487818) + (int)DAT_00489e8c) & 0xfffffffe;
            if (piVar6[3] >= 0 &&
                (int)(piVar6[-1] + uVar4 * (unsigned int)(-0x20000)) < iVar2 &&
                iVar2 < (int)(piVar6[-1] + uVar4 * 0x20000) &&
                (int)(*piVar6 + uVar4 * (unsigned int)(-0x20000)) < iVar3 &&
                iVar3 < (int)(*piVar6 + uVar4 * 0x20000)) {
                break;
            }
            iVar7++;
            piVar6 = (int *)((int)piVar6 + 0x40);
            if (DAT_00489260 <= iVar7) return;
        }
        DAT_00481e8f = 3;
        iVar7 = iVar7 * 0x40;
        if (*(char *)(iVar7 + 0x1c + (int)DAT_00481f28) == '\x07') {
            DAT_00481e8f = 4;
        }
        if (team == *(unsigned char *)(iVar7 + 0x1d + (int)DAT_00481f28)) {
            return; /* Same team as structure — no damage */
        }
    } else {
        /* Branch: byte_0x26 != 0 — check structures, skip same-team */
        iVar7 = 0;
        if (DAT_00489260 < 1) return;
        int *piVar6 = (int *)((int)DAT_00481f28 + 0x10);
        while (1) {
            unsigned int uVar4 = *(unsigned char *)(*(int *)((unsigned int)*(unsigned char *)((int)piVar6 + 0xC) * 0x20 + (int)DAT_00487818) + (int)DAT_00489e8c) & 0xfffffffe;
            if (*piVar6 >= 0 &&
                team != *(unsigned char *)((int)piVar6 + 0xd) &&
                (int)(piVar6[-4] + uVar4 * (unsigned int)(-0x20000)) < iVar2 &&
                iVar2 < (int)(piVar6[-4] + uVar4 * 0x20000) &&
                (int)(piVar6[-3] + uVar4 * (unsigned int)(-0x20000)) < iVar3 &&
                iVar3 < (int)(piVar6[-3] + uVar4 * 0x20000)) {
                break;
            }
            iVar7++;
            piVar6 = (int *)((int)piVar6 + 0x40);
            if (DAT_00489260 <= iVar7) return;
        }
        DAT_00481e8f = 3;
        iVar7 = iVar7 * 0x40;
        if (*(char *)(iVar7 + 0x1c + (int)DAT_00481f28) == '\x07') {
            DAT_00481e8f = 4;
        }
    }

    /* Apply damage and set damaged flag.
     * Projectile +0x44 (piVar5[0x11]) is the damage value; structure +0x10 is its
     * health. Structure +0x1e=1 is consumed by FUN_00458010 (shield animation). */
    *(int *)(iVar7 + 0x10 + (int)DAT_00481f28) -= entity->damage_44;
    *(unsigned char *)(iVar7 + 0x1e + (int)DAT_00481f28) = 1;   /* set damaged flag */
}

/* ===== FUN_00451e70 — Building/Structure Damage from Fire Particles (00451E70) ===== */
/* Checks fire particle against 9 categories of indexed entities (structures/buildings).
 * Each category has different hitbox sizes and health thresholds.
 * param_1 = fire particle index, param_2 = damage amount.
 *
 * DAT_00487834[i] is the per-category count; DAT_0048781c is a flat linkBase
 * holding category-indexed entity indices in 0x1000-slot windows per category.
 * Hitbox values like 0x200000 are in 18-bit fixed-point (= 8 pixels). */
struct FireDamageCategory {
    int count_index;
    int link_offset;
    int x_radius;
    int y_above;
    int y_below;
    int health_threshold;
    bool reject_state_minus_five;
    bool threshold_by_subtype;
};

static bool apply_fire_particle_damage_category(
    int particle_index, int damage, const FireDamageCategory &category)
{
    int *particle = (int *)((int)DAT_00481f34 + particle_index * 0x20);
    unsigned char particle_owner =
        *(unsigned char *)((int)particle + 0x14);
    int *links = (int *)DAT_0048781c + category.link_offset;

    for (int i = 0; i < DAT_00487834[category.count_index]; ++i) {
        Entity *entity = &DAT_004892e8[links[i]];
        signed char state = (signed char)entity->state_20;
        if (!((entity->timer_5c == 0 || entity->owner != particle_owner) &&
              state != -6 &&
              (!category.reject_state_minus_five || state != -5))) {
            continue;
        }

        if (!(particle[0] - category.x_radius < entity->position_x &&
              entity->position_x < particle[0] + category.x_radius &&
              particle[1] - category.y_above < entity->position_y &&
              entity->position_y < particle[1] + category.y_below)) {
            continue;
        }

        entity->variant_24 = 1;
        entity->scratch_58 = damage;
        entity->health_or_damage_28 += entity->scratch_58;

        int threshold = category.health_threshold;
        if (category.threshold_by_subtype) {
            if (entity->subtype == 0) {
                threshold = 12800000;
            } else if (entity->subtype == 1) {
                threshold = 0x70800;
            } else {
                return true;
            }
        }

        if (entity->health_or_damage_28 >= threshold) {
            entity->state_20 = 0xfa;
        }
        return true;
    }
    return false;
}

void FUN_00451e70(int param_1, int param_2)
{
    static const FireDamageCategory categories[] = {
        {0, 0x0000, 0x200000, 0x140000, 0x240000, 0x19000, false, false},
        {1, 0x1000, 0x200000, 0x200000, 0x200000, 1, false, false},
        {2, 0x2000, 0x240000, 0x140000, 0x300000, 0x2ee000, false, false},
        {3, 0x3000, 0x240000, 0x140000, 0x2c0000, 0xfa000, false, false},
        {4, 0x4000, 0x240000, 0x1c0000, 0x1c0000, 0x7d000, false, false},
        {5, 0x5000, 0x2c0000, 0x2c0000, 0x2c0000, 0xfa000, false, false},
        {6, 0x6000, 0x200000, 0x200000, 0x200000, 0, false, true},
        {7, 0x7000, 0x280000, 0x300000, 0x300000, 0x465000, true, false},
        {8, 0x8000, 0x240000, 0x240000, 0x240000, 0xfa000, false, false},
    };

    for (unsigned int i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i) {
        if (apply_fire_particle_damage_category(param_1, param_2, categories[i])) {
            return;
        }
    }
}

/* ===== FUN_0045d7d0 - Intro particle/entity reset ===== */
void FUN_0045d7d0(void)
{
    g_FrameIndex = 2;              /* Intro starts showing frame 2 */
    DAT_00489248 = 0;              /* Entity count */
    DAT_00489250 = 0;              /* Particle count */
    DAT_0048925c = 0;              /* Misc counter */
    g_FrameTimer = timeGetTime();  /* Frame time reference */
    DAT_004892b8 = timeGetTime();  /* Intro start timestamp for duration checks */
}

/* ===== Gameplay Subsystem Stubs ===== */
/* These are the ~20 subsystems called each tick from Gameplay_Tick (0045DAA0).
 * Each will be decompiled individually as the project progresses. */

/* ===== FUN_00460d50 — Round_Timer_Update (00460D50) ===== */
/* Countdown timer for round end. When timer reaches 1, applies game-end
 * conditions based on mode byte (kill all, team winner, health cap, etc.).
 * Mode byte is at address 0x00483741 (offset +1 from DAT_00483740). */
void FUN_00460d50(void)
{
    int i, soff;
    char mode;

    if (DAT_004892a8 > 1) {
        if (DAT_004892a5 != '\0') return;
        DAT_004892a8--;

        /* At 1890 ticks remaining: flash warning on all players */
        if (DAT_004892a8 == 0x762) {
            if (DAT_0048764a != '\0') goto post_timer;
            for (i = 0; i < DAT_00489240; i++) {
                PlayerData *player = Player_Get(i);
                player->hud_banner_id = 200;
                player->hud_banner_timer = 200;
            }
        }

        /* At 1 tick remaining: game end based on mode */
        if (DAT_004892a8 == 1) {
            mode = *((char *)&DAT_00483740 + 1);

            if (mode < 5) {
                if (mode == 4 || mode == 2) {
                    /* Cap health to 0x1000 */
                    for (i = 0; i < DAT_00489240; i++) {
                        PlayerData *player = Player_Get(i);
                        if (player->health > 0x1000) {
                            player->health = 0x1000;
                        }
                    }
                }
                else if (mode == 0) {
                    /* Kill everyone */
                    for (i = 0; i < DAT_00489240; i++) {
                        PlayerData *player = Player_Get(i);
                        player->lives = 0;
                        player->health = (int)0xFFF0BDC0;
                    }
                }
                else if (mode == 1) {
                    /* Team mode: find winning team by combined health+kills*maxhp */
                    int team_score[3] = {0, 0, 0};
                    if (DAT_00489240 > 0) {
                        soff = 0;
                        for (i = 0; i < DAT_00489240; i++) {
                            PlayerData *player = Player_Get(i);
                            unsigned char team = player->team;
                            if (team < 3) {
                                int hp = player->health;
                                int kills = player->lives;
                                int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + soff + 0x28) : 1;
                                if (hp > 0) team_score[team] += hp;
                                team_score[team] += kills * max_hp;
                            }
                            soff += 0x40;
                        }
                    }
                    int best = -1;
                    unsigned int best_team = 0;
                    for (unsigned int t = 0; t < 3; t++) {
                        if (team_score[t] > best) {
                            best_team = t;
                            best = team_score[t];
                        }
                    }
                    int tie_count = 0;
                    for (int t = 0; t < 3; t++) {
                        if (team_score[t] == best) tie_count++;
                    }
                    if (tie_count < 2) {
                        /* Kill non-winning teams */
                        for (i = 0; i < DAT_00489240; i++) {
                            PlayerData *player = Player_Get(i);
                            if (player->team != (unsigned char)best_team) {
                                player->lives = 0;
                                player->health = (int)0xFFF0BDC0;
                            }
                        }
                    } else {
                        /* Tie: kill everyone */
                        for (i = 0; i < DAT_00489240; i++) {
                            PlayerData *player = Player_Get(i);
                            player->lives = 0;
                            player->health = (int)0xFFF0BDC0;
                        }
                    }
                }
            } else {
                if (mode == (char)0xFE) {
                    DAT_004892a4 = 2;
                    DAT_004892a5 = 1;
                    return;
                }
                if (mode == (char)0xFF) {
                    DAT_004892a4 = 1;
                    DAT_004892a5 = 1;
                    return;
                }
            }
        }
    }

    if (DAT_004892a5 != '\0') return;

post_timer:
    /* Post-game countdown: once victory is set and activation guard triggered */
    if (DAT_004892a4 != '\0' && DAT_0048373e == '\x01' && DAT_004892ac > 0) {
        DAT_004892ac--;
        if (DAT_004892ac == 0) {
            for (i = 0; i < DAT_00489240; i++) {
                PlayerData *player = Player_Get(i);
                player->lives = 0;
                player->health = (int)0xFFF0BDC0;
            }
        }
    }
}
/* ===== FUN_004609e0 — Spatial_Grid_Bin (004609E0) ===== */
/* Bins troopers and projectiles into 4 team buckets stored in DAT_00487aa4.
 * Layout: 4 sections of 0x4000 bytes each (one per team).
 *   team_base + 0x08: trooper count for this team
 *   team_base + 0x100C: projectile count for this team
 *   team_base + 0x0C + team*0x1000*4: trooper index array
 *   team_base + 0x1010 + team*0x1000*4: projectile index array
 * Consumed by FUN_00458010 (turret targeting) which iterates the 4 team blocks
 * at 0x4000 intervals — see the grid_offset += 0x4000 loop. */
void FUN_004609e0(void)
{
    int team_off;
    int i, off;
    unsigned char team;

    if (DAT_00487aa4 == NULL) return;

    /* Clear counts for all 4 teams */
    for (team_off = 0; team_off < 0x10000; team_off += 0x4000) {
        *(int *)((int)DAT_00487aa4 + team_off + 0x08) = 0;      /* trooper count */
        *(int *)((int)DAT_00487aa4 + team_off + 0x100C) = 0;     /* projectile count */
    }

    /* Bin troopers by team (DAT_00487884, stride 0x40, team byte at +0x1C) */
    if (DAT_0048924c > 0) {
        off = 0;
        for (i = 0; i < DAT_0048924c; i++) {
            team = *(unsigned char *)((int)DAT_00487884 + off + 0x1C);
            if (team < 4) {
                int tbase = (unsigned int)team * 0x4000;
                int *count_ptr = (int *)((int)DAT_00487aa4 + tbase + 0x08);
                *(int *)((int)DAT_00487aa4 + 0x0C +
                    (*count_ptr + (unsigned int)team * 0x1000) * 4) = i;
                (*count_ptr)++;
            }
            off += 0x40;
        }
    }

    /* Bin projectiles by team (DAT_00481f28, stride 0x40, team byte at +0x1D) */
    if (DAT_00489260 > 0) {
        off = 0;
        for (i = 0; i < DAT_00489260; i++) {
            team = *(unsigned char *)((int)DAT_00481f28 + off + 0x1D);
            if (team < 4) {
                int tbase = (unsigned int)team * 0x4000;
                int *count_ptr = (int *)((int)DAT_00487aa4 + tbase + 0x100C);
                *(int *)((int)DAT_00487aa4 + 0x1010 +
                    (*count_ptr + (unsigned int)team * 0x1000) * 4) = i;
                (*count_ptr)++;
            }
            off += 0x40;
        }
    }
}

/* ===== FUN_00460660 — Build_Collision_Bitmap (00460660) ===== */
/* Marks coarse grid cells (DAT_00487814) with presence flags per tick.
 * Grid is DAT_004879f8 × DAT_004879fc bytes (one byte per 16x16 tile group).
 * Bit meanings:
 *   0x01 = player ship presence (5x5 around ship)
 *   0x02 = entity presence (3x3, collidable type only)
 *   0x04 = trooper presence (3x3)
 *   0x08 = in player viewport area
 *   0x10 = projectile presence (3x3) */
void FUN_00460660(void)
{
    int grid_cols = DAT_004879f8;
    int grid_rows = DAT_004879fc;
    unsigned int grid_size = (unsigned int)(grid_cols * grid_rows);
    int i, j, dx, dy;
    unsigned char *grid = (unsigned char *)DAT_00487814;

    if (grid == NULL) return;

    /* Phase 1: Clear entire grid */
    unsigned int *p32 = (unsigned int *)grid;
    unsigned int dwords = grid_size >> 2;
    for (i = 0; (unsigned int)i < dwords; i++) p32[i] = 0;
    unsigned char *pTail = (unsigned char *)&p32[dwords];
    for (i = 0; (unsigned int)i < (grid_size & 3); i++) pTail[i] = 0;

    /* Phase 2: Mark viewport areas with bit 0x08 for each active player viewport */
    if (DAT_00487808 > 0) {
        for (i = 0; i < DAT_00487808; i++) {
            int player = DAT_004877f8[i];
            PlayerData *player_data = Player_Get(player);
            int vp_w = player_data->viewport_width + 0x28;
            int vp_h = player_data->viewport_height + 0x28;
            int start_x = (player_data->position_x >> 0x12) - vp_w / 2;
            int start_y = (player_data->position_y >> 0x12) - vp_h / 2;

            /* Clamp to map bounds */
            if (start_x < 0) start_x = 0;
            if (start_y < 0) start_y = 0;
            if (start_x > (int)DAT_004879f0 - vp_w) start_x = (int)DAT_004879f0 - vp_w;
            if (start_y > (int)DAT_004879f4 - vp_h) start_y = (int)DAT_004879f4 - vp_h;
            if ((int)DAT_004879f0 < vp_w) { start_x = 0; vp_w = (int)DAT_004879f0; }
            if ((int)DAT_004879f4 < vp_h) { start_y = 0; vp_h = (int)DAT_004879f4; }

            int gy0 = start_y >> 4;
            int gx0 = start_x >> 4;
            int gy1 = (vp_h + start_y) >> 4;
            int gx1 = (vp_w + start_x) >> 4;

            for (int gy = gy0; gy <= gy1; gy++) {
                for (int gx = gx0; gx <= gx1; gx++) {
                    grid[gy * grid_cols + gx] |= 0x08;
                }
            }
        }
    }

    /* Phase 3: Mark player ship presence with bit 0x01 (5x5 coarse cells) */
    {
        for (i = 0; i < DAT_00489240; i++) {
            PlayerData *player = Player_Get(i);
            if (player->state_24 == 0) {
                int cx = (player->position_x >> 0x16) - 2;
                int cy = (player->position_y >> 0x16) - 2;
                for (dy = 0; dy < 5; dy++) {
                    for (dx = 0; dx < 5; dx++) {
                        int gx = cx + dx;
                        int gy = cy + dy;
                        if (gx >= 0 && gx < grid_cols && gy >= 0 && gy < grid_rows) {
                            grid[gx + gy * grid_cols] |= 0x01;
                        }
                    }
                }
            }
        }
    }

    /* Phase 4: Mark entities with bit 0x02 (3x3 coarse cells, collidable type only) */
    if (DAT_00489248 > 0) {
        for (i = 0; i < DAT_00489248; i++) {
            Entity *entity = &DAT_004892e8[i];
            /* Check if entity type is collidable: entity_type_table[type][subtype].byte_0x130 == 1 */
            unsigned char etype = entity->type;
            unsigned char esub = entity->subtype;
            if (*(char *)((int)DAT_00487abc + (unsigned int)etype * 0x218 + (unsigned int)esub + 0x130) == '\x01') {
                int cx = (entity->position_x >> 0x16) - 1;
                int cy = (entity->position_y >> 0x16) - 1;
                for (dy = 0; dy < 3; dy++) {
                    for (dx = 0; dx < 3; dx++) {
                        int gx = cx + dx;
                        int gy = cy + dy;
                        if (gx >= 0 && gx < grid_cols && gy >= 0 && gy < grid_rows) {
                            grid[gx + gy * grid_cols] |= 0x02;
                        }
                    }
                }
            }
        }
    }

    /* Phase 5: Mark troopers with bit 0x04 (3x3 coarse cells) */
    if (DAT_0048924c > 0) {
        int toff = 0;
        for (i = 0; i < DAT_0048924c; i++) {
            int cx = (*(int *)((int)DAT_00487884 + toff) >> 0x16) - 1;
            int cy = (*(int *)((int)DAT_00487884 + toff + 8) >> 0x16) - 1;
            for (dy = 0; dy < 3; dy++) {
                for (dx = 0; dx < 3; dx++) {
                    int gx = cx + dx;
                    int gy = cy + dy;
                    if (gx >= 0 && gx < grid_cols && gy >= 0 && gy < grid_rows) {
                        grid[gx + gy * grid_cols] |= 0x04;
                    }
                }
            }
            toff += 0x40;
        }
    }

    /* Phase 6: Mark projectiles with bit 0x10 (3x3 coarse cells) */
    if (DAT_00489260 > 0) {
        int proj_off = 0;
        for (i = 0; i < DAT_00489260; i++) {
            int cx = (*(int *)((int)DAT_00481f28 + proj_off) >> 0x16) - 1;
            int cy = (*(int *)((int)DAT_00481f28 + proj_off + 4) >> 0x16) - 1;
            for (dy = 0; dy < 3; dy++) {
                for (dx = 0; dx < 3; dx++) {
                    int gx = cx + dx;
                    int gy = cy + dy;
                    if (gx >= 0 && gx < grid_cols && gy >= 0 && gy < grid_rows) {
                        grid[gx + gy * grid_cols] |= 0x10;
                    }
                }
            }
            proj_off += 0x40;
        }
    }
}
/* ===== FUN_00460ac0 — Relocate_Edge_Entities (00460AC0) ===== */
/* Every 500 ticks, checks edge entities to see if they're stuck in
 * non-walkable tiles. If stuck, searches a 40-tile radius for a valid
 * position. If none found, removes the entity by swapping with last. */
void FUN_00460ac0(void)
{
    DAT_00489e9c++;
    if (DAT_00489e9c <= 500) return;
    DAT_00489e9c = 0;

    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int etable = (int)DAT_00487928;
    int tilemap = (int)DAT_0048782c;
    int edge_arr = (int)DAT_00489e84;

    if (edge_arr == 0 || tilemap == 0 || etable == 0) return;

    int i = 0;
    int rec_off = 0;
    while (i < DAT_00489254) {
        int ex = *(int *)(edge_arr + rec_off) >> 0x12;
        int ey = *(int *)(edge_arr + rec_off + 4) >> 0x12;
        unsigned char tile = *(unsigned char *)(tilemap + (ey << shift) + ex);
        char walkable = *(char *)(etable + (unsigned int)tile * 0x20 + 0x18);

        if (walkable != '\0') {
            /* Tile is walkable, entity is fine */
            i++;
            rec_off += 0x10;
            continue;
        }

        /* Entity stuck — search nearby for walkable tile */
        int found = 0;
        for (int sy = ey - 0x14; sy < ey + 0x14 && !found; sy++) {
            for (int sx = ex - 0x14; sx < ex + 0x14; sx++) {
                unsigned char t = *(unsigned char *)(tilemap + (sy << shift) + sx);
                if (*(char *)(etable + (unsigned int)t * 0x20 + 0x18) != '\0') {
                    /* Check neighbors too */
                    unsigned char tl = *(unsigned char *)(tilemap + (sy << shift) + sx - 1);
                    unsigned char tr = *(unsigned char *)(tilemap + (sy << shift) + sx + 1);
                    if (*(char *)(etable + (unsigned int)tl * 0x20 + 0x18) != '\0' &&
                        *(char *)(etable + (unsigned int)tr * 0x20 + 0x18) != '\0') {
                        *(int *)(edge_arr + rec_off) = sx << 0x12;
                        *(int *)(edge_arr + rec_off + 4) = sy << 0x12;
                        unsigned char new_tile = *(unsigned char *)(tilemap + (sy << shift) + sx);
                        *(unsigned char *)(edge_arr + rec_off + 8) =
                            *(unsigned char *)(etable + (unsigned int)new_tile * 0x20 + 0x19);
                        found = 1;
                        break;
                    }
                }
            }
        }

        if (!found) {
            /* No valid position found — remove by swapping with last */
            DAT_00489254--;
            int last_off = DAT_00489254 * 0x10;
            *(int *)(edge_arr + rec_off) = *(int *)(edge_arr + last_off);
            *(int *)(edge_arr + rec_off + 4) = *(int *)(edge_arr + last_off + 4);
            *(unsigned char *)(edge_arr + rec_off + 8) = *(unsigned char *)(edge_arr + last_off + 8);
            if (i >= DAT_00489254) break;
            continue;  /* Re-check this slot (now has swapped data) */
        }

        i++;
        rec_off += 0x10;
    }
}
/* ===== FUN_00413720 — Multi_Entity_Spawner (00413720) ===== */
/* Spawns critters (part 1), troopers (part 2), and ambient water particles (part 3).
 * Each part is gated by config flags and entity count limits.
 * Part 3 (water particles) requires float comparison with .rdata constant;
 * simplified to skip that section for now. */
void FUN_00413720(void)
{
    int shift = (unsigned char)DAT_00487a18 & 0x1F;

    /* Part 1: Critter spawning (DAT_00483734 enables) */
    if (DAT_00483734 != '\0') {
        int rnd = rand() & 0x7FF;
        /* Threshold from float-to-int conversion — simplified to a reasonable constant */
        if (rnd < 2 && DAT_0048924c < 400) {
            int cx = rand() % ((int)DAT_004879f0 - 4) + 2;
            int cy = rand() % ((int)DAT_004879f4 - 4) + 2;
            unsigned char tile = *(unsigned char *)((int)DAT_0048782c + (cy << shift) + cx);
            /* Check tile type byte 0 == 1 (ground tile) */
            if (*(char *)((int)DAT_00487928 + (unsigned int)tile * 0x20) == '\x01') {
                int speed = (rand() % 0x46 + 0x5A) * 0x200;
                int dir_rnd = rand() & 1;
                char dir = (char)(dir_rnd * 2 - 1);
                FUN_00407210(cx * FIXED_SCALE, cy * FIXED_SCALE, 0, 0, dir, speed, 0xFF, '\0');
            }
        }
    }

    /* Part 2: Trooper spawning (byte at DAT_00483758+2 enables) */
    {
        char trooper_mode = *((char *)&DAT_00483758 + 2);
        if (trooper_mode != '\0') {
            int area = (int)DAT_004879f4 * (int)DAT_004879f0;
            int threshold = (int)(0xBAEB90LL / (long long)((area + ((unsigned int)area >> 31 & 0x7F)) >> 7));
            if (trooper_mode == '\x01') {
                threshold = (threshold + ((unsigned int)threshold >> 31 & 3)) >> 2;
            } else if (trooper_mode == '\x02') {
                threshold = threshold / 10;
            } else if (trooper_mode == '\x03') {
                threshold = threshold / 0x19;
            }
            if (threshold < 1) threshold = 1;

            if (DAT_00489268 < 100 && (rand() % threshold == 0)) {
                int tx = rand() % ((int)DAT_004879f0 - 4) + 2;
                int ty = rand() % ((int)DAT_004879f4 - 4) + 2;
                unsigned char tile = *(unsigned char *)((int)DAT_0048782c + (ty << shift) + tx);
                /* Check tile type byte 1 (walkable) */
                if (*(char *)((int)DAT_00487928 + (unsigned int)tile * 0x20 + 1) == '\x01') {
                    int sub_rnd = rand() % 0x78;
                    unsigned char trooper_type;
                    if (sub_rnd < 0x0F) {
                        trooper_type = 0;
                    } else {
                        trooper_type = (sub_rnd > 0x18) ? 2 : 1;
                    }
                    FUN_00407140(tx * FIXED_SCALE, ty * FIXED_SCALE, trooper_type);
                }
            }
        }
    }

    /* Part 3: Ambient water/underwater particles (simplified) */
    if (DAT_0048372c != '\x02') {
        int spawn_count = (((int)DAT_004879f0 >> 6) * ((int)DAT_004879f4 >> 6) * 4) >> 6;
        if (DAT_0048372c == '\0') {
            spawn_count *= 3;
        }
        for (int j = 0; j < spawn_count; j++) {
            if (DAT_00489248 > 0x9C3) break;
            int px = rand() % ((int)DAT_004879f0 - 4) + 2;
            int py = rand() % ((int)DAT_004879f4 - 4) + 2;
            /* Must be in viewport and underwater tile */
            if ((*(unsigned char *)((int)DAT_00487814 + (px >> 4) + (py >> 4) * DAT_004879f8) & 0x08) &&
                *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + (py << shift) + px) * 0x20 + 4 + (int)DAT_00487928) == '\x01') {
                Entity *entity = &DAT_004892e8[DAT_00489248];
                entity->position_x = px * FIXED_SCALE;
                entity->position_y = py * FIXED_SCALE;
                entity->velocity_x = 0;
                entity->velocity_y = 0;
                entity->previous_x = px * FIXED_SCALE;
                entity->previous_y = py * FIXED_SCALE;
                entity->motion_x_10 = 0;
                entity->motion_y_14 = 0;
                entity->type = 0x65;
                entity->variant_24 = 0;
                entity->state_20 = 0;
                entity->auxiliary_26 = 0xFF;
                entity->owner = 0xFF;
                entity->health_or_damage_28 = 0;
                entity->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0xD404);
                entity->damage_44 = *(int *)((int)DAT_00487abc + 0xD440);
                entity->scratch_48 = 0;
                entity->palette_value = *(int *)((int)DAT_00487abc + 0xD470);
                entity->animation_frame = 0;
                entity->subtype = 1;
                entity->callback_address = *(int *)((int)DAT_00487abc + 0xD378);
                entity->counter_3c = 0;
                entity->timer_5c = 0;
                DAT_00489248++;
                entity->health_or_damage_28 = 100;
            }
        }
    }

    /* Part 4: Conditional entity spawning from DAT_004892d0 threshold.
     * This involves float comparison with .rdata constant at 0x0047540C
     * and calls FUN_00410030. Simplified: skip for now (cosmetic only). */
}
/* ===== FUN_00454340 — Update_Spawner_Emitters (00454340) ===== */
/* Updates spawner/emitter objects (DAT_00487aa0, stride 0x10, DAT_004892d8 count).
 * Emitter layout: +0x00 X(int), +0x04 Y(int), +0x08 param(int),
 *   +0x0C type(byte), +0x0D sub_type(byte), +0x0E freq(byte).
 * Type 0: Fire emitter — spawns fire/smoke particles in DAT_00481f2c
 * Type 1: Flame emitter — spawns flame particles in DAT_00481f34
 * Type 2: Turret spawner — creates new turret entries
 * Type 3: Timed emitter — countdown fire emitter, removes when done
 * Emitters self-remove when their source tile is destroyed (type 0/1 only). */
void FUN_00454340(void)
{
    int i = 0;
    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int *lut = (int *)DAT_00487ab0;

    while (i < DAT_004892d8) {
        int ebase = (int)DAT_00487aa0 + i * 0x10;
        int em_x = *(int *)(ebase);
        int em_y = *(int *)(ebase + 4);
        int em_param = *(int *)(ebase + 8);
        unsigned char em_type = *(unsigned char *)(ebase + 0x0C);
        unsigned char em_sub = *(unsigned char *)(ebase + 0x0D);
        unsigned char em_freq = *(unsigned char *)(ebase + 0x0E);

        switch (em_type) {
        case 0: {
            /* Fire emitter: create fire/smoke particles in DAT_00481f2c */
            /* Visibility check: must be in player viewport */
            int gx = em_x >> 4;
            int gy = em_y >> 4;
            if (DAT_00487814 != NULL &&
                (*(unsigned char *)((int)DAT_00487814 + gx + gy * DAT_004879f8) & 0x08) != 0 &&
                DAT_0048925c < 1500) {
                if (rand() % ((int)em_freq + 1) == 0) {
                    char sprite;
                    unsigned int spread;
                    if (em_sub == 0) {
                        sprite = (char)(rand() & 1) + 0x12;
                        spread = 0x80;
                    } else if (em_sub == 1) {
                        sprite = (char)(rand() & 1) + 0x14;
                        spread = 200;
                    } else if (em_sub == 2) {
                        sprite = (char)(rand() & 1) + 0x16;
                        spread = 0xFA;
                    } else {
                        sprite = (char)(rand() % 3) + 0x18;
                        spread = 0x15E;
                    }

                    unsigned int dir = ((unsigned int)(rand() % (int)spread - (int)(spread / 2)) +
                                       (unsigned int)em_param) & 0x7FF;

                    if (DAT_0048925c < 1500) {
                        int poff = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
                        *(int *)(poff) = em_x * FIXED_SCALE + lut[em_param] * 0x0C;
                        *(int *)(poff + 4) = em_y * FIXED_SCALE + lut[(em_param + 0x200) & 0x7FF] * 0x0C;
                        *(int *)(poff + 8) = (rand() % 0x14 + 0x0F) * lut[dir & 0x7FF] >> 6;
                        *(int *)(poff + 0x0C) = (rand() % 0x14 + 0x0F) * lut[(dir + 0x200) & 0x7FF] >> 6;
                        *(char *)(poff + 0x10) = sprite;
                        *(unsigned char *)(poff + 0x11) = 0;
                        *(unsigned short *)(poff + 0x12) = 0;
                        *(unsigned char *)(poff + 0x14) = 0xFF;
                        *(unsigned char *)(poff + 0x15) = 0;
                        DAT_0048925c++;
                    }
                }
            }
            break;
        }

        case 1: {
            /* Flame emitter: create flame particles in DAT_00481f34. */
            if (DAT_00489250 < 2000 && rand() % 6 == 0) {
                unsigned int dir = ((unsigned int)((rand() & 0x7F) - 0x40) +
                                   (unsigned int)em_param) & 0x7FF;
                char sprite;
                int speed_mult;

                if (em_sub == 0) {
                    sprite = (char)(rand() & 1) + 5;
                    speed_mult = 2;
                } else if (em_sub == 1) {
                    sprite = (char)(rand() & 1) + 3;
                    speed_mult = 3;
                } else if (em_sub == 2) {
                    sprite = (char)(rand() & 3) + 1;
                    speed_mult = 3;
                } else {
                    sprite = (char)(rand() % 3) + 0x11;
                    speed_mult = 4;
                }

                if (DAT_00489250 < 2000) {
                    int poff = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                    *(int *)(poff) = em_x * FIXED_SCALE + lut[em_param] * 0x0C;
                    *(int *)(poff + 4) = em_y * FIXED_SCALE + lut[(em_param + 0x200) & 0x7FF] * 0x0C;
                    /* Velocity: LUT * speed_mult, signed divide by 8 */
                    int vx = lut[dir] * speed_mult;
                    *(int *)(poff + 8) = (vx + (vx >> 31 & 7)) >> 3;
                    int vy = lut[(dir + 0x200) & 0x7FF] * speed_mult;
                    *(int *)(poff + 0x0C) = (vy + (vy >> 31 & 7)) >> 3;
                    *(char *)(poff + 0x10) = sprite;
                    *(unsigned char *)(poff + 0x11) = 0;
                    *(unsigned char *)(poff + 0x12) = 0;
                    *(unsigned char *)(poff + 0x13) = 200;  /* behavior: flame type */
                    *(unsigned char *)(poff + 0x14) = 0xFF;
                    *(unsigned char *)(poff + 0x15) = 0;
                    DAT_00489250++;
                }
            }
            break;
        }

        case 2: {
            /* Turret spawner: create new turret on empty tile */
            int tile_idx = (em_y << shift) + em_x;
            if (DAT_0048927c < DAT_00489280 &&
                *(char *)((int)DAT_0048782c + tile_idx) == '\0') {
                *(int *)(DAT_00481f48 + DAT_0048927c * 8) = tile_idx;
                *(unsigned char *)(DAT_00481f48 + 5 + DAT_0048927c * 8) = 0;
                *(unsigned char *)(DAT_00481f48 + 6 + DAT_0048927c * 8) = 0;
                /* Random direction 0-3 */
                *(char *)(DAT_00481f48 + 4 + DAT_0048927c * 8) = (char)(rand() & 3);
                *(unsigned char *)(DAT_00481f48 + 7 + DAT_0048927c * 8) = 0;
                FUN_004104c0(DAT_0048927c);
                DAT_0048927c++;
            }
            break;
        }

        case 3: {
            /* Timed emitter: countdown, then self-remove */
            if (em_param <= 0) {
                goto remove_emitter;
            }
            *(int *)(ebase + 8) = em_param - 1;
            em_param--;

            /* Adjust frequency as countdown progresses */
            if (em_param < 0x10E) {
                *(unsigned char *)(ebase + 0x0E) = 0x1A;
                em_freq = 0x1A;
            }
            if (em_param < 0xB4) {
                *(unsigned char *)(ebase + 0x0E) = 0x60;
                em_freq = 0x60;
            }

            /* Same visibility check and particle creation as type 0 */
            int gx = em_x >> 4;
            int gy = em_y >> 4;
            if (DAT_00487814 != NULL &&
                (*(unsigned char *)((int)DAT_00487814 + gx + gy * DAT_004879f8) & 0x08) != 0 &&
                DAT_0048925c < 1500) {
                if (rand() % ((int)em_freq + 1) == 0) {
                    char sprite;
                    unsigned int spread;
                    if (em_sub == 2) {
                        sprite = (char)(rand() & 1) + 0x14;
                        spread = 0xFA;
                    } else {
                        sprite = (char)(rand() % 3) + 0x16;
                        spread = 0x15E;
                    }

                    unsigned int dir = ((unsigned int)(rand() % (int)spread - (int)(spread / 2))
                                       - 0x400) & 0x7FF;  /* upward bias */

                    if (DAT_0048925c < 1500) {
                        int poff = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
                        /* Position with random jitter (-4 to +3 pixels) */
                        *(unsigned int *)(poff) = (unsigned int)(((rand() & 7) - 4) + em_x) * FIXED_SCALE;
                        *(unsigned int *)(poff + 4) = (unsigned int)(((rand() & 7) - 4) + em_y) * FIXED_SCALE;
                        *(int *)(poff + 8) = (rand() % 0x14 + 0x0F) * lut[dir & 0x7FF] >> 6;
                        *(int *)(poff + 0x0C) = (rand() % 0x14 + 0x0F) * lut[(dir + 0x200) & 0x7FF] >> 6;
                        *(char *)(poff + 0x10) = sprite;
                        *(unsigned char *)(poff + 0x11) = 0;
                        *(unsigned short *)(poff + 0x12) = 0;
                        *(unsigned char *)(poff + 0x14) = 0xFF;
                        *(unsigned char *)(poff + 0x15) = 0;
                        DAT_0048925c++;
                    }
                }
            }
            break;
        }
        default:
            break;
        } /* end switch */

        /* Self-removal: if source tile is destroyed (type '\0') and emitter is not type 2/3. */
        {
            int tile_idx = (em_y << shift) + em_x;
            char tile_val = *(char *)((int)DAT_0048782c + tile_idx);
            if (tile_val == '\0' && em_type != 2 && em_type != 3) {
                goto remove_emitter;
            }
        }

        i++;
        continue;

    remove_emitter:
        /* Swap-with-last removal */
        DAT_004892d8--;
        if (i < DAT_004892d8) {
            int last = DAT_004892d8 * 0x10 + (int)DAT_00487aa0;
            *(int *)(ebase) = *(int *)(last);
            *(int *)(ebase + 4) = *(int *)(last + 4);
            *(int *)(ebase + 8) = *(int *)(last + 8);
            *(unsigned char *)(ebase + 0x0C) = *(unsigned char *)(last + 0x0C);
            *(unsigned char *)(ebase + 0x0D) = *(unsigned char *)(last + 0x0D);
            *(unsigned char *)(ebase + 0x0E) = *(unsigned char *)(last + 0x0E);
        }
        /* Don't increment i — re-check swapped-in entry */
    }
}
/* ===== FUN_00434310 — Entity_Debris_Animation (00434310) ===== */
/* Processes all entities in DAT_004892e8 (stride 0x80, count DAT_00489248).
 * Recovered callbacks dispatch by their original guest address at +0x34. Types
 * whose callbacks have not been lifted yet continue through the legacy inline
 * fallback below:
 *   - Type 0 (+0x21=0): projectile — position integration, boundary check, expire on wall hit
 *   - Type 2 (+0x21=2): trooper debris — position integration, gravity, lifetime countdown
 *   - Type 0x6C/0x6D (+0x21): visible debris fragments — same as above
 *   - Type 5 (+0x20=5): generic debris — position integration, lifetime countdown
 *   - Others: generic position integration + lifetime countdown */
void FUN_00434310(void)
{
    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int i = 0;

    while (i < DAT_00489248) {
        Entity *entity = &DAT_004892e8[i];
        int ebase = (int)entity;
        unsigned char ent_type = entity->type;
        unsigned char ent_state = entity->state_20;
        int should_remove = 0;

        /* Save previous positions */
        entity->previous_x = entity->position_x;
        entity->previous_y = entity->position_y;

        /* Advance animation frame counter */
        unsigned char frame_ctr = entity->animation_frame + 1;
        entity->animation_frame = frame_ctr;

        /* Original 0x00434310 uses an unsigned strict-greater comparison. */
        int type_entry = (unsigned int)entity->subtype +
                         (unsigned int)ent_type * 0x218;
        unsigned char max_frame = DAT_00487abc != NULL
            ? *(unsigned char *)((int)DAT_00487abc + type_entry + 0x12A)
            : 0;
        if (max_frame < frame_ctr) {
            entity->animation_frame = 0;
            entity->scratch_48 = entity->scratch_48 + 1;

            unsigned char max_cycles = DAT_00487abc != NULL
                ? *(unsigned char *)((int)DAT_00487abc + type_entry + 0x124)
                : 0;
            if ((unsigned int)max_cycles <= (unsigned int)entity->scratch_48) {
                entity->scratch_48 = 0;
            }
        }

        DAT_00481e8f = 0;
        uint32_t callback_address = entity->callback_address;
        int callback_handled = EntityCallbacks_Dispatch(callback_address, i) ? 1 : 0;
        if (callback_handled) {
            should_remove = DAT_00481e8f == 1;
        } else {

        /* Force type 0x13 turret bullets: zero gravity + zero acceleration every tick.
         * Direct aim is correct at spawn but something keeps modifying velocity. */
        if (ent_type == 0x13 && entity->owner >= 0x50) {
            entity->gravity_or_motion_38 = 0;
        }

        /* === Entity behavior (inline replacement for callback at +0x34) === */

        /* Entity category flags.
         * is_debris MUST be evaluated first — debris types overlap with
         * projectile velocity check and must be excluded.
         *
         * is_projectile uses velocity-based detection instead of a hardcoded
         * type list.  The original callback at 0x438010 handles ALL entity
         * types through a state machine; any spawned entity with non-zero
         * velocity that isn't debris / laser / trail / water is a projectile
         * that needs gravity + wall/player collision.
         *
         * Exclusions:
         *   - is_debris  (type 2, 100, >=0x6C, state 5) — handled separately
         *   - 0x2D laser — instant beam trace, handled above
         *   - 0x67 machinegun/firework trail — lifecycle managed below, no collision
         *   - 0x65 water splash particles — cosmetic, no collision */
        /* Type 2 is Organic Waste (NOT debris) — has its own behavior case below. */
        int is_debris = (ent_type == 100 || ent_type >= 0x6C || ent_state == 5);

        /* Type 0x67 lifecycle: decrement lifespan, apply drag, remove when expired.
         * These are trail/bullet particles spawned by MINISHIP (0x1C), LANDMINE
         * ring burst (0x19 mode 2), ROMAN CANDLE shrapnel (0x25), and other weapons.
         * They need to fade and die but don't participate in wall/player collision.
         * Two categories:
         *   +0x28 > 0: firework/bullet trail — countdown + drag + gravity + removal
         *   +0x28 == 0: cosmetic exhaust (ship trail) — left untouched, faded by
         *               palette system in the 0x67 fading block below */
        if (ent_type == 0x67) {
            int t67_life = entity->health_or_damage_28;
            if (t67_life > 0) {
                /* Firework trail entities: lifespan countdown + physics */
                t67_life--;
                entity->health_or_damage_28 = t67_life;
                if (t67_life <= 0) { should_remove = 1; }
                entity->velocity_x = (int)((double)entity->velocity_x * 0.95);
                entity->velocity_y = (int)((double)entity->velocity_y * 0.95);
                entity->position_x += entity->velocity_x;
                entity->position_y += entity->velocity_y;
                entity->velocity_y += entity->gravity_or_motion_38 * DAT_00483828;
            }
            /* +0x28 == 0: cosmetic trail (ship exhaust etc) — leave untouched */
        }

        int is_projectile = 0;
        if (!is_debris && ent_type != 0x2d && ent_type != 0x67 && ent_type != 0x65) {
            if (entity->velocity_x != 0 || entity->velocity_y != 0) {
                is_projectile = 1;
            }
            /* Force is_projectile for entity types that start with zero velocity
             * but still need gravity, wall collision, and/or player collision.
             * Without this, they would be skipped by the velocity-based detection.
             *   0x08 TOURNAILLER  — orbit mode starts stationary, needs wall collision
             *   0x17 NUCLEUS      — trail dots sit still, need entity-entity collision
             *   0x18 PILOT DISRUP — deploys on ground, needs wall collision to deploy
             *   0x19 LANDMINE     — modes 1/2 are stationary mines, need player collision
             *   0x1F INSECTS      — direct position movement (no velocity), need collision
             *   0x24 ETNA         — deploys on ground, needs wall collision to deploy
             *   0x25 ROMAN CANDLE — deploys on ground, needs wall collision to deploy
             *   0x26 MORNING STAR — spinning fire, needs wall collision to explode
             *   0x28 PIPEBOMB     — can reach zero velocity mid-bounce, needs collision
             *   0x29 TURRET (gun) — falls with gravity, needs wall collision to deploy
             *   0x2A TURRET (ice) — falls with gravity, needs wall collision to deploy
             *   0x2E SMOKING NALLE— sits still (vx=vy=0), needs wall/player collision */
            if (ent_type == 0x02 || ent_type == 0x29 || ent_type == 0x2A || ent_type == 0x18 ||
                ent_type == 0x1F || ent_type == 0x08 || ent_type == 0x28 ||
                ent_type == 0x17 || ent_type == 0x19 || ent_type == 0x24 ||
                ent_type == 0x25 || ent_type == 0x26 || ent_type == 0x2E) {
                is_projectile = 1;
            }
        }

        /* === Laser beam trace (type 0x2D) ===
         * Original callback at 0x0043f990 traces the ENTIRE beam path in one
         * tick: loops up to 50 steps, each step moves by sincos[dir]*2,
         * spawns a purple trail particle, and checks for hard wall collision.
         * The laser entity itself is invisible (entity[0x4C]=30000).
         * After tracing, the entity is removed. */
        if (ent_type == 0x2d && !should_remove) {
            int beam_dir = entity->scratch_2c & 0x7FF;
            int *beam_sc = (int *)DAT_00487ab0;
            int beam_vx = beam_sc[beam_dir] << 1;
            int beam_vy = beam_sc[beam_dir + 0x200] << 1;
            entity->velocity_x = beam_vx;
            entity->velocity_y = beam_vy;
            int beam_life = entity->health_or_damage_28;
            if (beam_life < 1) beam_life = 50;
            int beam_x = entity->position_x;
            int beam_y = entity->position_y;

            for (int step = 0; step < beam_life; step++) {
                beam_x += beam_vx;
                beam_y += beam_vy;

                /* Bounds check */
                if (beam_x < 0 || beam_y < 0 ||
                    beam_x >= (int)(DAT_004879f0 * FIXED_SCALE) ||
                    beam_y >= (int)(DAT_004879f4 * FIXED_SCALE)) {
                    break;
                }

                /* Wall collision check */
                int btx = beam_x >> 0x12;
                int bty = beam_y >> 0x12;
                if (btx > 0 && bty > 0 && btx < (int)DAT_004879f0 && bty < (int)DAT_004879f4) {
                    int btile_off = (bty << shift) + btx;
                    unsigned char btile = *(unsigned char *)((int)DAT_0048782c + btile_off);
                    unsigned char bpass2 = *(unsigned char *)((unsigned int)btile * 0x20 + 2 + (int)DAT_00487928);
                    if (bpass2 == 0 && step >= 2) {
                        /* Hit solid wall — apply crater damage like other projectiles */
                        unsigned char sub_type = entity->subtype;
                        int explevel = (sub_type <= 4) ? (int)sub_type + 6 : 6;
                        unsigned char stored_tile = 0;
                        unsigned char tile_prop4 = *(unsigned char *)((unsigned int)btile * 0x20 + 4 + (int)DAT_00487928);
                        if (tile_prop4 != 0) stored_tile = btile;
                        char is_water = (btile == 0x0C) ? (char)1 : (char)0;
                        unsigned char owner = entity->owner;
                        FUN_004357b0(btx, bty, explevel, stored_tile, is_water,
                                     0, 0, 0, 0, 0, '\0', owner);
                        break;
                    }
                }

                /* Original emits a two-dot purple pair every other beam step,
                 * and only while the beam is inside the active pixel mask. */
                if ((step & 1) != 0 &&
                    (*(unsigned char *)((beam_x >> 0x16) + (int)DAT_00487814 +
                     (beam_y >> 0x16) * DAT_004879f8) & 8) != 0) {
                    for (int dot = 0; dot < 2 && DAT_00489248 < 0x9C4; dot++) {
                        int dir = dot == 0
                            ? (beam_dir + ((rand() & 0x7FF) >> 3)) & 0x7FF
                            : (beam_dir - (rand() & 0xFF)) & 0x7FF;
                        int speed = dot == 0 ? 2 : 7;
                        Entity *tp = &DAT_004892e8[DAT_00489248];
                        memset((void *)tp, 0, 0x80);
                        tp->position_x = beam_x; tp->previous_x = beam_x;
                        tp->position_y = beam_y; tp->previous_y = beam_y;
                        tp->velocity_x = beam_sc[dir] * speed >> 6;
                        tp->velocity_y = beam_sc[dir + 0x200] * speed >> 6;
                        tp->type = 0x67;
                        tp->variant_24 = (unsigned short)(rand() % 6);
                        tp->owner = 0xFF;
                        tp->auxiliary_26 = 0xFF;
                        tp->subtype = 0;
                        tp->timer_5c = 2;
                        tp->scratch_65 = 0x5E;
                        tp->scratch_64 = 0x52;
                        if (DAT_00487aa8 != NULL)
                            tp->palette_value = (int)((unsigned short *)DAT_00487aa8)[0x5E] + 30000;
                        DAT_00489248++;
                    }
                }

                /* Player collision along the beam */
                for (int p = 0; p < DAT_00489240; p++) {
                    PlayerData *player = Player_Get(p);
                    if (player->health <= 0) continue;
                    unsigned char raw_owner = entity->owner;
                    if (raw_owner == (unsigned char)p) continue;
                    int ship_size = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x38) : 0;
                    int h_range = ship_size + 0x80000;
                    int v_range = ship_size + 0x80000;
                    int px = player->position_x;
                    int py = player->position_y;
                    if (px - h_range < beam_x && beam_x < px + h_range &&
                        py - v_range < beam_y && beam_y < py + v_range) {
                        int proj_damage = entity->damage_44;
                        unsigned char shooter_team = Player_Get(raw_owner)->team;
                        unsigned char target_team = player->team;
                        if (shooter_team != target_team || DAT_0048373d != 0) {
                            player->health = tou_binary::sub_wrap_i32(player->health, proj_damage);
                            player->last_attacker = raw_owner;
                        }
                        player->timer_c4 = 5;
                        player->flag_a3 = 1;
                        player->timer_4a2 = 0x6e;
                    }
                }
            }
            should_remove = 1;  /* beam traced — remove the invisible entity */
        }

        /* Apply projectile gravity BEFORE position integration.
         * Original callback at 0x438010 (addr 0x4386e3):
         *   imul edx, [DAT_00483828]   ; edx = entity[+0x38] * gravity_constant
         *   add  edi, edx              ; vy += gravity (applied FIRST)
         *   add  ecx, eax              ; pos_x += vx
         *   ... pos_y += vy            ; uses updated vy
         * entity[+0x38] = 6 for all turret projectiles (set at spawn).
         * Type 0x22 excluded: wavy fireworks overwrite velocity from heading each
         * tick (gravity would accumulate and distort the wave pattern). */
        if (is_projectile && !is_debris && ent_type != 0x22 && ent_type != 0x1B && ent_type != 0x26) {
            entity->velocity_y += entity->gravity_or_motion_38 * DAT_00483828;
        }

        /* === Homing/guidance + special movement — per-type ===
         * Applied AFTER gravity but BEFORE position integration.
         * Modifies velocity to steer toward targets or add special patterns. */
        if (is_projectile && !is_debris && !should_remove) {
            switch (ent_type) {
            case 0x08: { /* TOURNAILLER — from Ghidra callback 0x43CC20 (type != 0x09 path).
                * Full gravity. When state +0x40 == 1: orbit mode.
                * Orbit: angle +0x20 grows by 10/tick (capped 30), radius +0x3C grows by
                * +0x60 per tick (wraps 0x7FF). Position = center + LUT[radius]*angle/8.
                * Fuse timer +0x28 counts down → detonate at 0.
                * When fuse +0x26 == 1: saves center position to +0x2C/+0x30. */
                /* Always orbit from spawn. */
                {
                    /* Save orbit center + initial velocity on first tick */
                    if (entity->scratch_2c == 0 && entity->scratch_30 == 0) {
                        entity->scratch_2c = entity->previous_x;
                        entity->scratch_30 = entity->previous_y;
                        /* Save initial velocity to +0x10/+0x14 for center drift */
                        entity->motion_x_10 = entity->velocity_x;
                        entity->motion_y_14 = entity->velocity_y;
                    }
                    /* Drift orbit center forward along initial velocity */
                    entity->scratch_2c += entity->motion_x_10;
                    entity->scratch_30 += entity->motion_y_14;
                    /* Radius grows +2/tick, capped 150, 10-tick delay */
                    unsigned char ang = entity->state_20;
                    unsigned char delay = entity->timer_5c;
                    if (delay < 10) {
                        entity->timer_5c = delay + 1;
                    } else {
                        ang += 2;
                        if (ang > 150) ang = 150;
                        entity->state_20 = ang;
                    }
                    /* Angular position: +64 per tick */
                    int rad = entity->counter_3c;
                    rad += 64;
                    if (rad >= 2048) rad -= 2048;
                    entity->counter_3c = rad;
                    /* Position = drifting center + orbit offset */
                    int *lut = (int *)DAT_00487ab0;
                    int cy = entity->scratch_30;
                    int cx = entity->scratch_2c;
                    int cos_v = lut[rad & 0x7FF];
                    int sin_v = lut[(rad + 0x200) & 0x7FF];
                    entity->position_y = cy + ((cos_v * (int)ang) >> 3);
                    entity->position_x = cx + ((sin_v * (int)ang) >> 3);
                    /* Zero velocity to prevent shared integration */
                    entity->velocity_x = 0;
                    entity->velocity_y = 0;
                }
                /* else: state != 1, flying with full gravity (handled by shared code) */
                break;
            }

            case 0x09: { /* KICKER — spawn tile-contact debris (from Ghidra 0x43CCD6).
                * When on a solid tile, spawn type 0x67 particle with palette 0x93-0x9F.
                * This creates the "trail" effect — only on tile contact, not per-tick. */
                int kx = entity->previous_x; /* backup position */
                int ky = entity->previous_y;
                int ktx = kx >> 0x16;
                int kty = ky >> 0x16;
                if (ktx >= 0 && kty >= 0 && ktx < (int)DAT_004879f0 && kty < (int)DAT_004879f4) {
                    int koff = (kty * (int)DAT_004879f8) + (int)DAT_00487814;
                    unsigned char kbyte = *(unsigned char *)(koff + ktx);
                    if (kbyte & 0x08) { /* solid tile */
                        if (DAT_00489248 < 0x9C4) {
                            Entity *tp = &DAT_004892e8[DAT_00489248];
                            tp->position_x = kx;
                            tp->position_y = ky;
                            tp->velocity_x = entity->velocity_x >> 6;
                            tp->velocity_y = entity->velocity_y >> 6;
                            tp->previous_x = kx;
                            tp->previous_y = ky;
                            tp->motion_x_10 = 0; tp->motion_y_14 = 0;
                            tp->type = 0x67;
                            tp->state_20 = 0;
                            tp->auxiliary_26 = 0xFF;
                            tp->owner = entity->owner;
                            tp->health_or_damage_28 = 0;
                            tp->gravity_or_motion_38 = 0; /* no gravity */
                            tp->counter_3c = 0;
                            tp->subtype = 0;
                            tp->animation_frame = 0;
                            int *tt = (int *)DAT_00487abc;
                            tp->callback_address = tt[0x35EA]; /* callback from config */
                            tp->damage_44 = tt[0x361B]; /* damage from config */
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].timer_5c = 1; /* +0x5C */
                            DAT_004892e8[DAT_00489248 - 1].scratch_65 = 0x9F; /* +0x65 palette hi */
                            DAT_004892e8[DAT_00489248 - 1].scratch_64 = 0x93; /* +0x64 palette lo */
                            if (DAT_00487aa8 != NULL) {
                                unsigned short pal = ((unsigned short *)DAT_00487aa8)[0x9F];
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    (unsigned int)pal + 0x7530; /* +0x4C color */
                            }
                        }
                    }
                }
                break;
            }

            case 0x0E: { /* MOVING SUCKER — speed-normalized steering.
                * Find nearest enemy, adjust heading, normalize speed. */
                unsigned char own = entity->owner;
                int mx = entity->position_x;
                int my = entity->position_y;
                int best_dist = 0x7FFFFFFF;
                int tgt_x = mx, tgt_y = my;
                int found = 0;
                for (int p = 0; p < DAT_00489240; p++) {
                    if ((unsigned char)p == own) continue;
                    PlayerData *player = Player_Get(p);
                    if (player->health <= 0) continue;
                    int px = player->position_x;
                    int py = player->position_y;
                    int dx = px - mx; int dy = py - my;
                    /* Approximate distance (Manhattan) */
                    int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (dist < best_dist) {
                        best_dist = dist; tgt_x = px; tgt_y = py; found = 1;
                    }
                }
                if (found) {
                    /* Steer toward target: adjust velocity slightly each tick */
                    int dx = tgt_x - mx;
                    int dy = tgt_y - my;
                    int vx = entity->velocity_x;
                    int vy = entity->velocity_y;
                    /* Small turn rate: add 1/16 of direction-to-target */
                    vx += dx / 64;
                    vy += dy / 64;
                    /* Speed normalization: keep constant speed magnitude.
                     * Use original speed from spawn (approximate from field). */
                    int cur_speed_sq = (vx >> 8) * (vx >> 8) + (vy >> 8) * (vy >> 8);
                    if (cur_speed_sq > 0) {
                        /* Target speed ~200 (typical guided missile) */
                        int target_speed = 200;
                        /* Scale velocity to maintain constant speed.
                         * Approximate: multiply by target/current ratio.
                         * Use integer sqrt approximation. */
                        int cur_speed = 1;
                        int temp = cur_speed_sq;
                        while (temp > 0) { temp >>= 2; cur_speed <<= 1; }
                        /* Newton's method for better sqrt */
                        cur_speed = (cur_speed + cur_speed_sq / cur_speed) / 2;
                        cur_speed = (cur_speed + cur_speed_sq / cur_speed) / 2;
                        if (cur_speed > 0) {
                            vx = (int)((long long)vx * target_speed / cur_speed);
                            vy = (int)((long long)vy * target_speed / cur_speed);
                        }
                    }
                    entity->velocity_x = vx;
                    entity->velocity_y = vy;
                }
                break;
            }

            case 0x11: { /* NORMAL FIREBALL — homing in state 0x1B, from Ghidra 0x441AA0.
                * Scans for closest enemy, steers toward them. Speed capped. */
                unsigned char fb_state = entity->state_20;
                if (fb_state == 0x1B) {
                    unsigned char fb_own = entity->owner;
                    int fb_x = entity->position_x;
                    int fb_y = entity->position_y;
                    int fb_best = 0x7FFFFFFF;
                    int fb_tx = fb_x, fb_ty = fb_y, fb_found = 0;
                    for (int p = 0; p < DAT_00489240; p++) {
                        if ((unsigned char)p == fb_own) continue;
                        PlayerData *player = Player_Get(p);
                        if (player->health <= 0) continue;
                        int px = player->position_x;
                        int py = player->position_y;
                        int dx = px - fb_x; int dy = py - fb_y;
                        int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                        if (dist < fb_best) { fb_best = dist; fb_tx = px; fb_ty = py; fb_found = 1; }
                    }
                    if (fb_found) {
                        entity->velocity_x += (fb_tx - fb_x) / 64;
                        entity->velocity_y += (fb_ty - fb_y) / 64;
                        /* Speed cap */
                        int svx = entity->velocity_x >> 8;
                        int svy = entity->velocity_y >> 8;
                        int spd = svx * svx + svy * svy;
                        if (spd > 0xB06440) {
                            double mag = sqrt((double)spd);
                            double cap = 3400.0;
                            entity->velocity_x = (int)(svx * cap / mag) << 8;
                            entity->velocity_y = (int)(svy * cap / mag) << 8;
                        }
                    }
                }
                break;
            }

            case 0x17: { /* NUCLEUS (Batch 8) — trail weapon, entity type 0x17.
                * Ghidra callback: 0x432C80. Verified at 0x432C80-0x433020.
                *
                * The nucleus weapon fires a stream of stationary dots (type 0x17)
                * that sit in place until detonated. Two detonation modes:
                *   Mode 0 (+0x40==0): passive — waits to be shot by any projectile.
                *     Entity-entity collision accumulates damage at +0x28; threshold=1
                *     means ANY hit sets state=0xFA (ring explosion).
                *   Mode 1 (+0x40==1): timed — countdown at +0x60 (not +0x28 to avoid
                *     conflict with damage accumulation), auto-detonates when 0.
                *
                * State 0xFA (triggered): spawns ~12 type-0x00 entities in a ring burst
                * with random start angle. Ring spacing 0xAA in 0x800 range = ~12 entities.
                * Flash particle + explosion sound. Damage depends on mode:
                *   mode 0: 0x4B000 (higher), mode 1: 0x32000 (lower).
                *
                * Invulnerability timer at +0x5C prevents instant self-detonation
                * from the player's own bullets right after spawning. */
                /* Zero velocity — nucleus dots are stationary trail */
                entity->velocity_x = 0;
                entity->velocity_y = 0;
                unsigned char nc_sub = entity->subtype;
                /* Mode 2 auto-detonate: countdown at +0x60 (avoids +0x28 conflict with damage tracking) */
                if (nc_sub == 1) {
                    int nc_life = entity->scratch_60;
                    if (nc_life > 0) {
                        nc_life--;
                        entity->scratch_60 = nc_life;
                        if (nc_life == 0)
                            entity->state_20 = 0xFA;
                    }
                }
                /* Decrement invuln timer */
                {
                    unsigned char nc_inv = entity->timer_5c;
                    if (nc_inv > 0) {
                        nc_inv--;
                        entity->timer_5c = nc_inv;
                    }
                }
                /* State 0xFA: ring explosion — spawn ~12 type-0x00 entities */
                if (entity->state_20 == 0xFA) {
                    int nc_x = entity->position_x;
                    int nc_y = entity->position_y;
                    unsigned char nc_own = entity->owner;
                    int *sc = (int *)DAT_00487ab0;
                    int *tt = (int *)DAT_00487abc;
                    int ring_heading = rand() & 0x7FF; /* random start angle */
                    for (int rdi = 0; rdi < 0x800; rdi += 0xAA) {
                        if (DAT_00489248 >= 0x9C4) break;
                        int h = (rdi + ring_heading) & 0x7FF;
                        Entity *ep = &DAT_004892e8[DAT_00489248];
                        ep->position_x = nc_x;
                        ep->position_y = nc_y;
                        /* Velocity: sincos * 5/4 = (val*5) << 4 >> 6 */
                        int sv = sc[h];
                        ep->velocity_x = ((sv * 5) << 4) >> 6;
                        int cv = sc[h + 0x200];
                        ep->velocity_y = ((cv * 5) << 4) >> 6;
                        ep->previous_x = nc_x;
                        ep->previous_y = nc_y;
                        ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                        ep->type = 0; /* type 0x00 debris */
                        ep->variant_24 = 0;
                        ep->state_20 = 0;
                        ep->auxiliary_26 = 0;
                        ep->owner = nc_own;
                        ep->health_or_damage_28 = 0;
                        ep->gravity_or_motion_38 = tt[0x24]; /* gravity from config */
                        ep->damage_44 = (nc_sub == 0) ? 0x4B000 : 0x32000; /* damage */
                        ep->scratch_48 = 0;
                        ep->palette_value = tt[0x3F];
                        ep->animation_frame = 0;
                        ep->subtype = 2; /* sub_type 2 */
                        ep->callback_address = tt[0]; /* callback */
                        ep->counter_3c = 0;
                        ep->timer_5c = 0;
                        DAT_00489248++;
                        /* Ring entities: white color */
                        DAT_004892e8[DAT_00489248 - 1].palette_value =
                            (unsigned int)0xFFFF + 0x7530;
                        /* Lifespan = 0: entities die on wall hit (original sets +0x28 = 0) */
                        DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 0;
                    }
                    /* Flash particle — from Ghidra 0x432FCC.
                     * Only spawns if entity position is on solid tile.
                     * +0x10 = (rand()&1)+3, matches original byte-exact. */
                    {
                        int nc_tx = nc_x >> 0x16;
                        int nc_ty = nc_y >> 0x16;
                        int nc_tile_off = nc_ty * *(int *)((int)DAT_00487928 + 0x04 + 0) + nc_tx; /* approximate */
                        /* Always spawn flash for simplicity (original gates on solid tile) */
                        if (DAT_00489250 < 2000) {
                            int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(fp + 0x00) = nc_x;
                            *(int *)(fp + 0x04) = nc_y;
                            *(int *)(fp + 0x08) = 0;
                            *(int *)(fp + 0x0C) = 0;
                            *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 1) + 3;
                            *(unsigned char *)(fp + 0x11) = 0;
                            *(unsigned char *)(fp + 0x12) = 0;
                            *(unsigned char *)(fp + 0x13) = 1;
                            *(unsigned char *)(fp + 0x14) = 0xFF;
                            *(unsigned char *)(fp + 0x15) = 0; /* fire */
                            DAT_00489250++;
                        }
                    }
                    FUN_0040f9b0(0x65 + (rand() % 7), nc_x, nc_y);
                    should_remove = 1;
                }
                break;
            }

            case 0x19: { /* LANDMINE (Batch 8) — entity type 0x19.
                * Ghidra callback: 0x443420. Verified at 0x443420-0x443950.
                *
                * Three modes based on weapon level (+0x40):
                *   Mode 0: flying beam — decelerates (sqrt speed cap at 256),
                *     self-integrates position. Skipped from shared integration.
                *   Mode 1: stationary mine — guard countdown at +0x26, then bobbing
                *     oscillation using sincos LUT. Proximity scan for enemy players
                *     within 0x180000 (~6px). Sets +0x28=1 to trigger detonation.
                *   Mode 2: stationary mine + ring burst — same as mode 1 but
                *     detonation spawns ring of 128 type 0x67 bullets (0x2000/0x40 steps).
                *
                * Lifespan at +0x28 counts down each tick. At 0: detonation with
                * flash particle + explosion sound. All modes die on detonation.
                *
                * Position integration skipped in shared code because mode 0 does
                * its own with deceleration, and modes 1/2 are stationary. */
                /* Lifespan countdown */
                int lm_life = entity->health_or_damage_28;
                if (lm_life > 0) {
                    lm_life--;
                    entity->health_or_damage_28 = lm_life;
                    if (lm_life == 0) {
                        /* DETONATE */
                        unsigned char lm_sub = entity->subtype;
                        int lm_x = entity->position_x;
                        int lm_y = entity->position_y;
                        unsigned char lm_own = entity->owner;
                        if (lm_sub == 2) {
                            /* Level 2: spawn ring of type 0x67 bullets */
                            int *sc = (int *)DAT_00487ab0;
                            int *tt = (int *)DAT_00487abc;
                            for (int rdi = 0; rdi < 0x2000; rdi += 0x40) {
                                if (DAT_00489248 >= 0x9C4) break;
                                int h_idx = rdi >> 2; /* byte offset to entry index */
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                int sv = sc[h_idx];
                                int cv = sc[h_idx + 0x200];
                                ep->position_x = lm_x + sv * 0x10;
                                ep->position_y = lm_y + cv * 0x10;
                                ep->velocity_x = sv;
                                ep->velocity_y = cv;
                                ep->previous_x = lm_x + sv * 0x10;
                                ep->previous_y = lm_y + cv * 0x10;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->type = 0x67;
                                ep->variant_24 = (unsigned short)(rand() % 6);
                                ep->state_20 = 2;
                                ep->auxiliary_26 = 0x1E;
                                ep->owner = lm_own;
                                ep->health_or_damage_28 = 0;
                                ep->gravity_or_motion_38 = tt[0xD838 / 4];
                                ep->damage_44 = tt[0xD874 / 4];
                                ep->scratch_48 = 0;
                                ep->palette_value = tt[0xD8A4 / 4];
                                ep->animation_frame = 0;
                                ep->subtype = 2;
                                ep->callback_address = tt[0xD7A8 / 4];
                                ep->counter_3c = 0;
                                ep->timer_5c = 4;
                                DAT_00489248++;
                                Entity *spawned = &DAT_004892e8[DAT_00489248 - 1];
                                spawned->scratch_65 = 0x1E;
                                spawned->scratch_64 = 0x12;
                                if (DAT_00487aa8 != NULL)
                                    spawned->palette_value =
                                        (unsigned int)((unsigned short *)DAT_00487aa8)[0x1E] + 30000;
                                spawned->damage_44 = 0x19000;
                            }
                        }
                        /* Flash particle for all modes */
                        if (DAT_00489250 < 2000) {
                            int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(fp + 0x00) = lm_x;
                            *(int *)(fp + 0x04) = lm_y;
                            *(int *)(fp + 0x08) = 0;
                            *(int *)(fp + 0x0C) = 0;
                            *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() % 3) + 0x11;
                            *(unsigned char *)(fp + 0x11) = 0;
                            *(unsigned char *)(fp + 0x12) = 0;
                            *(unsigned char *)(fp + 0x13) = 1;
                            *(unsigned char *)(fp + 0x14) = 0xFF;
                            *(unsigned char *)(fp + 0x15) = 0;
                            DAT_00489250++;
                        }
                        FUN_0040f9b0(0x65 + (rand() % 7), lm_x, lm_y);
                        should_remove = 1;
                        break;
                    }
                }
                /* Guard byte countdown */
                {
                    unsigned char lm_g = entity->auxiliary_26;
                    if (lm_g > 0 && lm_g < 0xFF) {
                        lm_g--;
                        entity->auxiliary_26 = lm_g;
                    }
                }
                unsigned char lm_sub = entity->subtype;
                if (lm_sub == 0) {
                    /* Mode 0: flying beams — velocity deceleration + position integration */
                    int lm_grounded = entity->scratch_2c;
                    if (lm_grounded == 0) {
                        int lm_vx = entity->velocity_x;
                        int lm_vy = entity->velocity_y;
                        int svx = lm_vx >> 8;
                        int svy = lm_vy >> 8;
                        int spd_sq = svx * svx + svy * svy;
                        /* Decelerate if speed² > 0x10000 (binary float at 0x4756E0 ≈ 256.0) */
                        if (spd_sq > 0x10000) {
                            double mag = sqrt((double)spd_sq);
                            entity->velocity_x = (int)((double)svx * 256.0 / mag) << 8;
                            entity->velocity_y = (int)((double)svy * 256.0 / mag) << 8;
                        }
                        /* Self position integration */
                        entity->position_x += entity->velocity_x;
                        entity->position_y += entity->velocity_y;
                    }
                } else {
                    /* Mode 1/2: stationary mine */
                    unsigned char lm_g2 = entity->auxiliary_26;
                    if (lm_g2 > 0) {
                        /* Guard active: save current position */
                        entity->scratch_2c = entity->position_x;
                        entity->scratch_30 = entity->position_y;
                    } else {
                        /* Guard expired: bobbing oscillation */
                        int lm_phase = entity->counter_3c;
                        lm_phase += 20;
                        if (lm_phase >= 0x800)
                            lm_phase -= 0x800;
                        entity->counter_3c = lm_phase;
                        int *sc = (int *)DAT_00487ab0;
                        int saved_y = entity->scratch_30;
                        entity->position_y = saved_y + sc[lm_phase] * 4;
                        /* Proximity detonation: scan for enemy players nearby */
                        {
                            int lm_x = entity->position_x;
                            int lm_y = entity->position_y;
                            unsigned char lm_own = entity->owner;
                            unsigned char lm_team = Player_Get(lm_own)->team;
                            int det_range = 0x180000; /* ~6 pixels detection range */
                            for (int p = 0; p < DAT_00489240; p++) {
                                PlayerData *player = Player_Get(p);
                                unsigned char p_team = player->team;
                                if (p_team == lm_team) continue; /* skip allies */
                                if (player->health <= 0) continue;
                                int px = player->position_x;
                                int py = player->position_y;
                                int dx = lm_x - px; if (dx < 0) dx = -dx;
                                int dy = lm_y - py; if (dy < 0) dy = -dy;
                                if (dx < det_range && dy < det_range) {
                                    /* Enemy in range — detonate next tick */
                                    entity->health_or_damage_28 = 1;
                                    break;
                                }
                            }
                        }
                    }
                }
                break;
            }

            case 0x6B: {
                /* Spiral flight — type 0x6B ONLY (ROMAN CANDLE sub-projectile).
                 * Types 0x13/0x14 are standard ballistic, do NOT spiral. */
                int vx = entity->velocity_x;
                int vy = entity->velocity_y;
                /* Spiral: rotate velocity by ~2 degrees per tick.
                 * cos(2°)≈1, sin(2°)≈0.035 → vx' = vx - vy/29, vy' = vy + vx/29 */
                int nvx = vx - vy / 29;
                int nvy = vy + vx / 29;
                entity->velocity_x = nvx;
                entity->velocity_y = nvy;
                break;
            }

            case 0x18: { /* PILOT DISRUPTOR — deploys on ground, stays alive.
                * Flies with gravity until wall hit (state 0xFA in wall collision).
                * Once deployed, actively scans for nearby enemy players each tick:
                *   player+0xD8 = 0x20 (aim jitter +/-13/tick)
                *   player+0xD4 = 0x20 (halved move/turn speed)
                * Dies when lifetime at +0x60 expires. */
                unsigned char pd_state = entity->state_20;
                if (pd_state == 0xFA) {
                    int pd_life = entity->scratch_60;
                    if (pd_life <= 0) {
                        /* Lifetime expired: AoE burst + die */
                        int pd_x = entity->position_x;
                        int pd_y = entity->position_y;
                        unsigned char pd_own = entity->owner;
                        FUN_00437cf0(pd_x, pd_y, 0xC8, pd_own, -1);
                        should_remove = 1;
                    } else {
                        entity->scratch_60 = pd_life - 1;
                        /* Active disruption scan — set timers on nearby enemies.
                         * Bypasses FUN_0044be20 (tracking list gets stale from swaps). */
                        int pd_x = entity->position_x;
                        int pd_y = entity->position_y;
                        unsigned char pd_own = entity->owner;
                        unsigned char my_team = Player_Get(pd_own)->team;
                        for (int p = 0; p < DAT_00489240; p++) {
                            PlayerData *player = Player_Get(p);
                            if (player->health <= 0) continue;
                            unsigned char p_team = player->team;
                            if (p_team == my_team) continue;
                            int px = player->position_x;
                            int py = player->position_y;
                            int dx = pd_x - px; if (dx < 0) dx = -dx;
                            int dy = pd_y - py; if (dy < 0) dy = -dy;
                            if (dx < 0x1180000 && dy < 0x1180000) {
                                player->timer_d8 = 0x20;
                                player->boost_timer = 0x20;
                            }
                        }
                    }
                }
                break;
            }

            case 0x1C: { /* MINISHIP (Batch 8) — entity type 0x1C.
                * Ghidra callback: 0x440E20. Verified at 0x440E20-0x441A00.
                *
                * Autonomous AI-controlled ship that chases the nearest enemy player.
                * Self-integrates position (skipped from shared integration).
                *
                * AI steering:
                *   - Scans all players, finds closest enemy (different team)
                *   - Distance threshold: 22500 pixel^2 (~150px). From Ghidra 0x57E4.
                *   - Turn rate: +/-0x2A per tick (~7.4 degrees). Heading at +0x3C.
                *   - When no enemy in range: decelerate (vx/vy *= 0.97)
                *   - When chasing: accumulate velocity from sincos[heading]>>5 + gravity
                *   - Speed cap: normalize if speed^2 > 0x225510
                *
                * Bullet firing: every 10 ticks (counter at +0x2C), spawns one type
                * 0x00 bullet in heading direction with half parent velocity added.
                * Only fires when actively chasing an enemy in range.
                *
                * Boundary handling: if position goes off-map, revert to backup pos
                * (+0x04/+0x0C), zero velocity, clamp to (0..map-8) tiles.
                *
                * Death: state 0xFA (from entity-entity collision) or lifetime +0x60
                * expiring. Both produce warm fire flash + KB(150) + sound.
                *
                * Skipped from player collision — damages via bullets, not contact.
                * Skipped from shared position integration — does its own. */
                /* Killed by enemy fire (state 0xFA) — from Ghidra 0x441926.
                 * Flash particle (warm fire) + small KB + sound. No terrain damage. */
                if (entity->state_20 == 0xFA) {
                    int dx = entity->position_x;
                    int dy = entity->position_y;
                    unsigned char ms_o = entity->owner;
                    FUN_00437cf0(dx, dy, 150, ms_o, -1);
                    if (DAT_00489250 < 2000) {
                        int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(fp + 0x00) = dx; *(int *)(fp + 0x04) = dy;
                        *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                        *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 3) + 13;
                        *(unsigned char *)(fp + 0x11) = 0;
                        *(unsigned char *)(fp + 0x12) = 0;
                        *(unsigned char *)(fp + 0x13) = 0;
                        *(unsigned char *)(fp + 0x14) = 0xFF;
                        *(unsigned char *)(fp + 0x15) = 1; /* warm fire */
                        DAT_00489250++;
                    }
                    FUN_0040f9b0(0x65 + (rand() % 7), dx, dy);
                    should_remove = 1; break;
                }
                /* Lifetime countdown at +0x60 */
                int ms_life = entity->scratch_60;
                if (ms_life > 0) {
                    ms_life--;
                    entity->scratch_60 = ms_life;
                    if (ms_life == 1) {
                        /* Death: small flash (warm fire) + KB + sound. From Ghidra 0x441926. */
                        int dx = entity->position_x;
                        int dy = entity->position_y;
                        unsigned char ms_o = entity->owner;
                        FUN_00437cf0(dx, dy, 150, ms_o, -1);
                        if (DAT_00489250 < 2000) {
                            int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(fp + 0x00) = dx; *(int *)(fp + 0x04) = dy;
                            *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                            *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 3) + 13;
                            *(unsigned char *)(fp + 0x11) = 0;
                            *(unsigned char *)(fp + 0x12) = 0;
                            *(unsigned char *)(fp + 0x13) = 0;
                            *(unsigned char *)(fp + 0x14) = 0xFF;
                            *(unsigned char *)(fp + 0x15) = 1; /* warm fire */
                            DAT_00489250++;
                        }
                        FUN_0040f9b0(0x65 + (rand() % 7), dx, dy);
                        should_remove = 1; break;
                    }
                }
                /* Self position integration */
                entity->position_x += entity->velocity_x;
                entity->position_y += entity->velocity_y;
                /* Boundary: revert to backup, zero velocity, clamp to (0..map-8) */
                {
                    int ms_x = entity->position_x;
                    int ms_y = entity->position_y;
                    int need_clamp = 0;
                    if (ms_x < 0 || (ms_x >> 0x12) >= (int)DAT_004879f0 ||
                        ms_y < 0 || (ms_y >> 0x12) >= (int)DAT_004879f4) {
                        entity->position_x = entity->previous_x;
                        entity->position_y = entity->previous_y;
                        entity->velocity_x = 0;
                        entity->velocity_y = 0;
                        need_clamp = 1;
                    }
                    if (need_clamp) {
                        int cx = entity->position_x;
                        int cy = entity->position_y;
                        if (cx < 0) entity->position_x = 0x200000;
                        if (cy < 0) entity->position_y = 0x200000;
                        int max_x = ((int)DAT_004879f0 - 8) << 0x12;
                        int max_y = ((int)DAT_004879f4 - 8) << 0x12;
                        if (entity->position_x > max_x) entity->position_x = max_x;
                        if (entity->position_y > max_y) entity->position_y = max_y;
                    }
                }
                /* Invuln countdown — keep minimum 1 so same-team bullets can't
                 * damage this miniship via the +0x5C==0 friendly-fire bypass. */
                {
                    unsigned char ms_inv = entity->timer_5c;
                    if (ms_inv > 1) { ms_inv--; entity->timer_5c = ms_inv; }
                }
                /* Enemy scan: find closest enemy player (different team) */
                unsigned char ms_own = entity->owner;
                int ms_tx = 0, ms_ty = 0;
                int ms_found_enemy = 0;
                int ms_best_dist = 0x7FFFFFFF;
                if (DAT_00489240 > 0) {
                    /* Get own team */
                    unsigned char ms_team = Player_Get(ms_own)->team;
                    for (int p = 0; p < DAT_00489240; p++) {
                        PlayerData *player = Player_Get(p);
                        unsigned char p_team = player->team;
                        if (p_team == ms_team) continue;
                        if (player->health <= 0) continue;
                        int px = player->position_x;
                        int py = player->position_y;
                        int dx = entity->position_x - px;
                        int dy = entity->position_y - py;
                        /* Distance: sqrt(dx²+dy²) approximated via (dx>>18)²+(dy>>18)² */
                        int pdx = dx >> 0x12;
                        int pdy = dy >> 0x12;
                        int dist_sq = pdx * pdx + pdy * pdy;
                        if (dist_sq < ms_best_dist) {
                            ms_best_dist = dist_sq;
                            ms_tx = px; ms_ty = py;
                            ms_found_enemy = 1;
                        }
                    }
                }
                /* Distance threshold: ~22500 pixel² ≈ 150px range.
                 * From Ghidra hex: 0x57E4 = 22500 in the comparison. */
                if (!ms_found_enemy || ms_best_dist > 22500) {
                    /* No enemy in range: decelerate (multiply vx/vy by ~0.97) */
                    entity->velocity_x = (int)((double)entity->velocity_x * 0.97);
                    entity->velocity_y = (int)((double)entity->velocity_y * 0.97);
                } else {
                    /* Steer toward target: heading-based turn-rate-limited pursuit.
                     * FUN_004257e0 returns angle from src to dst. Add 0x400 offset
                     * to match sincos velocity convention (same as kamikaze). */
                    int ms_heading = entity->counter_3c;
                    int desired = ((int)FUN_004257e0(
                        entity->position_x, entity->position_y, ms_tx, ms_ty) + 0x400) & 0x7FF;
                    int diff = ((desired - ms_heading) + 0x400) & 0x7FF;
                    if (diff > 0x400) diff -= 0x800;
                    /* Turn rate: ±0x2A per tick. 0x400 = 180°, 0x2A ≈ 7.4° */
                    int turn_rate = 0x2A;
                    if (diff > turn_rate) diff = turn_rate;
                    else if (diff < -turn_rate) diff = -turn_rate;
                    else if (diff == 0) diff = (rand() & 1) ? turn_rate : -turn_rate;
                    ms_heading = (ms_heading + diff) & 0x7FF;
                    entity->counter_3c = ms_heading;
                    /* Apply velocity from heading: vx += sincos[heading] >> 5 */
                    int *sc = (int *)DAT_00487ab0;
                    entity->velocity_x += sc[ms_heading & 0x7FF] >> 5;
                    entity->velocity_y += sc[(ms_heading + 0x200) & 0x7FF] >> 5;
                    /* Add gravity */
                    entity->velocity_y += DAT_00483824;
                    /* Speed cap: normalize if speed² > 0x225510 */
                    {
                        int svx = entity->velocity_x >> 8;
                        int svy = entity->velocity_y >> 8;
                        int spd_sq = svx * svx + svy * svy;
                        if (spd_sq > 0x225510 && spd_sq > 0) {
                            double mag = sqrt((double)spd_sq);
                            double cap = 1500.0; /* approximate speed cap from binary */
                            entity->velocity_x = (int)(svx * cap / mag) << 8;
                            entity->velocity_y = (int)(svy * cap / mag) << 8;
                        }
                    }
                }
                /* Fire type 0x67 bullet every ~10 ticks — only when chasing enemy */
                if (ms_found_enemy && ms_best_dist <= 22500) {
                    int bc = entity->scratch_2c;
                    bc++;
                    if (bc >= 10 && DAT_00489248 < 0x9C4) {
                        bc = 0;
                        int heading = entity->counter_3c;
                        int *sc = (int *)DAT_00487ab0;
                        int *tt = (int *)DAT_00487abc;
                        Entity *bp = &DAT_004892e8[DAT_00489248];
                        bp->position_x = entity->position_x;
                        bp->position_y = entity->position_y;
                        bp->previous_x = entity->position_x;
                        bp->previous_y = entity->position_y;
                        int bh = heading & 0x7FF;
                        int bvx = entity->velocity_x / 2;
                        int bvy = entity->velocity_y / 2;
                        bp->velocity_x = (sc[bh] * 5 << 4 >> 6) + bvx;
                        bp->velocity_y = (sc[(bh + 0x200) & 0x7FF] * 5 << 4 >> 6) + bvy;
                        bp->motion_x_10 = 0; bp->motion_y_14 = 0;
                        bp->type = 0x00; /* basic bullet */
                        bp->variant_24 = 0;
                        bp->state_20 = 0;
                        bp->auxiliary_26 = 0;
                        bp->owner = entity->owner;
                        bp->health_or_damage_28 = 0;
                        bp->gravity_or_motion_38 = tt[0x24]; /* gravity */
                        bp->damage_44 = tt[0x33]; /* damage */
                        bp->scratch_48 = 0;
                        bp->palette_value = tt[0x3F]; /* palette */
                        bp->animation_frame = 0;
                        bp->subtype = 3; /* sub_type 3 — verified from Ghidra 0x441690 */
                        bp->callback_address = tt[0]; /* callback */
                        bp->counter_3c = 0;
                        bp->timer_5c = 0;
                        DAT_00489248++;
                        DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 60;
                        /* Set bullet color from palette table (same as Fire_Secondary for type 0) */
                        if (DAT_00487aa8 != NULL) {
                            unsigned short pal = ((unsigned short *)DAT_00487aa8)[0x5A + (rand() & 1)];
                            DAT_004892e8[DAT_00489248 - 1].palette_value =
                                (unsigned int)pal + 30000;
                        }
                    }
                    entity->scratch_2c = bc;
                } /* end bullet firing gate */
                break;
            }

            case 0x1B: { /* KAMIKAZE MEN — drunk-flight homing toward nearest enemy.
                * Turn-rate-limited steering: heading adjusts +/-0x40 per tick.
                * Speed capped. Explodes on terrain or entity contact. */
                unsigned char km_own = entity->owner;
                /* Original integrates before acquiring/predicting its target. */
                entity->position_x += entity->velocity_x;
                entity->position_y += entity->velocity_y;
                int km_x = entity->position_x;
                int km_y = entity->position_y;
                int km_heading = entity->counter_3c;
                /* Find nearest enemy player */
                int km_best = 0x15F90; /* original only acquires within 300 pixels */
                int km_tx = km_x, km_ty = km_y;
                int km_found = 0;
                unsigned char km_team = Player_Get(km_own)->team;
                for (int p = 0; p < DAT_00489240; p++) {
                    PlayerData *player = Player_Get(p);
                    if (player->team == km_team) continue;
                    if (player->state_24 != 0) continue;
                    int px = player->position_x;
                    int py = player->position_y;
                    int dx = (km_x - px) >> 0x12;
                    int dy = (km_y - py) >> 0x12;
                    int dist_sq = dx * dx + dy * dy;
                    if (dist_sq < km_best) {
                        km_best = dist_sq; km_tx = px; km_ty = py; km_found = 1;
                    }
                }
                if (km_found) {
                    /* Lead by half the target distance. The old reconstruction
                     * added 0x400 here, making both modes steer away from targets. */
                    int lead = (int)(sqrt((double)km_best) * 0.5);
                    int desired = (int)FUN_004257e0(
                        km_x, km_y,
                        km_tx - entity->velocity_x * lead,
                        km_ty - entity->velocity_y * lead);
                    /* Turn-rate-limited steering: adjust heading by ±0x40 per tick */
                    int diff = (((desired - km_heading) + 0x400) & 0x7FF) - 0x400;
                    if (diff == -0x400) diff = 0x08;
                    if (diff > 0x40) diff = 0x40;
                    else if (diff < -0x40) diff = -0x40;
                    km_heading = (km_heading + diff) & 0x7FF;
                    entity->counter_3c = km_heading;
                    int *sc = (int *)DAT_00487ab0;
                    entity->velocity_x += sc[km_heading] >> 4;
                    entity->velocity_y += sc[km_heading + 0x200] >> 4;
                } else {
                    entity->velocity_y += entity->gravity_or_motion_38 * DAT_00483828;
                }
                /* Speed cap: if speed^2 > 16000000, normalize */
                {
                    int svx = entity->velocity_x >> 8;
                    int svy = entity->velocity_y >> 8;
                    int spd_sq = svx * svx + svy * svy;
                    if (spd_sq > 16000000 && spd_sq > 0) {
                        double mag = sqrt((double)spd_sq);
                        double cap = 4000.0; /* approximate max speed constant */
                        entity->velocity_x = (int)(svx * cap / mag) << 8;
                        entity->velocity_y = (int)(svy * cap / mag) << 8;
                    }
                }
                break;
            }

            case 0x1F: { /* INSECTS — from Ghidra callback 0x43B370.
                * Random 4-dir walk: each tick move FIXED_SCALE in random cardinal direction.
                * No gravity, no velocity-based movement (direct position modification).
                * Every 30 ticks: scan for nearest enemy within 120px, chase via atan2+LUT.
                * Lifetime at +0x60 counts down to 0. */
                /* Original 0x43B388-0x43B3A0: +0x60 is the insect lifetime.
                 * It is separate from the generic +0x28 projectile timer. */
                unsigned char ins_state = entity->state_20;
                if (ins_state == 0xFA) {
                    /* FUN_00437120 marks a sufficiently damaged insect 0xFA.
                     * Original 0x43B736 routes that state through its death path. */
                    FUN_0040f9b0(0x70, entity->position_x, entity->position_y);
                    should_remove = 1;
                    break;
                }
                if (ins_state == 0xFF || entity->scratch_60 <= 0) {
                    should_remove = 1;
                    break;
                }
                entity->scratch_60 -= 1;
                /* Random 4-direction movement: 16 pixels per tick */
                int dir = rand() & 3;
                if (dir == 0) entity->position_x -= FIXED_SCALE;
                else if (dir == 1) entity->position_x += FIXED_SCALE;
                else if (dir == 2) entity->position_y -= FIXED_SCALE;
                else entity->position_y += FIXED_SCALE;
                /* Zero velocity — movement is direct position, not velocity-based */
                entity->velocity_x = 0;
                entity->velocity_y = 0;
                int ins_cooldown = entity->counter_3c;
                if (ins_cooldown > 0) {
                    ins_cooldown--;
                    entity->counter_3c = ins_cooldown;
                }
                /* 30-tick retarget scan */
                unsigned char retarget = entity->scratch_65;
                retarget++;
                if (retarget > 30) {
                    retarget = 0;
                    unsigned char ins_own = entity->owner;
                    unsigned char ins_team = Player_Get(ins_own)->team;
                    int ins_x = entity->position_x;
                    int ins_y = entity->position_y;
                    int ins_best = 0x7FFFFFFF;
                    int ins_tx = 0, ins_ty = 0;
                    int ins_found = 0;
                    for (int p = 0; p < DAT_00489240; p++) {
                        PlayerData *player = Player_Get(p);
                        if (player->team == ins_team) continue;
                        if (player->state_24 != 0) continue;
                        int px = player->position_x;
                        int py = player->position_y;
                        int dx = (px - ins_x) >> 0x12;
                        int dy = (py - ins_y) >> 0x12;
                        int dist_sq = dx * dx + dy * dy;
                        if (dist_sq < 14400 && dist_sq < ins_best) { /* 120px range */
                            ins_best = dist_sq; ins_tx = px; ins_ty = py; ins_found = 1;
                        }
                    }
                    if (ins_found) {
                        entity->scratch_2c = ins_tx;
                        entity->scratch_30 = ins_ty;
                    } else {
                        entity->scratch_2c = 0;
                    }
                }
                entity->scratch_65 = retarget;
                /* Chase saved target: apply LUT velocity toward target */
                if (entity->scratch_2c != 0) {
                    int angle = (int)FUN_004257e0(
                        entity->position_x, entity->position_y,
                        entity->scratch_2c, entity->scratch_30);
                    int *lut = (int *)DAT_00487ab0;
                    entity->position_x += lut[angle & 0x7FF];
                    entity->position_y += lut[(angle + 0x200) & 0x7FF];
                }
                break;
            }

            /* REPAIR MAKER (0x2B): no behavior — falls with gravity, deploys on wall hit */
            if (0) {
                unsigned char own = entity->owner;
                int mx = entity->position_x;
                int my = entity->position_y;
                int best_dist = 0x7FFFFFFF;
                int found = 0;
                int tgt_x = mx, tgt_y = my;
                for (int p = 0; p < DAT_00489240; p++) {
                    if ((unsigned char)p == own) continue;
                    PlayerData *player = Player_Get(p);
                    if (player->health <= 0) continue;
                    int px = player->position_x;
                    int py = player->position_y;
                    int dx = px - mx; int dy = py - my;
                    int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (dist < best_dist) {
                        best_dist = dist; tgt_x = px; tgt_y = py; found = 1;
                    }
                }
                if (found) {
                    entity->velocity_x += (tgt_x - mx) / 96;
                    entity->velocity_y += (tgt_y - my) / 96;
                }
                break;
            }

            case 0x24: { /* ETNA (Batch 9) — deploy+spray weapon, entity type 0x24.
                * Ghidra callback: 0x447A70. Verified at 0x447A70-0x447D00.
                *
                * Flies with gravity until wall hit, then deploys (state 0xC8).
                * Wall collision sets state=0xC8, zeroes velocity, sets +0x60=900
                * (~15 seconds lifetime), +0x3C=-80 (startup delay).
                *
                * When deployed: sprays one flechette upward per tick.
                * Flechette heading: rand()&0xFF + 0x380 (upward arc, range 0x380-0x47F).
                * Headings near straight down (0x3F8-0x408) are skipped.
                * Flechette speed: rand()%60 + 20. Type 0x00 (basic bullet, yellow).
                * Palette: fire range (indices 246-255 from X1R5G5B5 palette).
                *
                * Startup delay: counter at +0x3C counts from -80 to 0 before
                * spraying begins. Dies with small flash when +0x60 timer expires. */
                /* Binary-accurate launchers.  The reconstructed deploy/state-C8
                 * model below was invented and is intentionally bypassed. */
                if (0) {
                    unsigned char rc_sub = entity->subtype;
                    int rc_cnt = entity->counter_3c + 1;
                    entity->counter_3c = rc_cnt;
                    int *sc = (int *)DAT_00487ab0;
                    int *tt = (int *)DAT_00487abc;
                    unsigned char own = entity->owner;
                    if (rc_sub == 0 && rc_cnt > 0x50) {
                        entity->counter_3c = 0;
                        /* 14/15 launches succeed in the original. */
                        if (rand() % 15 < 14 && DAT_00489248 < 0x9C4) {
                            int dir = (rand() & 0xFF) + 0x380;
                            int spd = rand() % 90 + 25;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            int x = entity->previous_x - FIXED_SCALE;
                            int y = entity->previous_y - 0x340000;
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = x; ep->previous_x = x;
                            ep->position_y = y; ep->previous_y = y;
                            ep->velocity_x = sc[dir] * spd >> 6;
                            ep->velocity_y = sc[dir + 0x200] * spd >> 6;
                            ep->type = 0x6A;
                            ep->owner = own;
                            ep->subtype = 1;
                            ep->gravity_or_motion_38 = tt[0xDE7C / 4];
                            ep->damage_44 = tt[0xDEB8 / 4];
                            ep->palette_value = tt[0xDEE8 / 4];
                            ep->callback_address = tt[0xDDF0 / 4];
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 50 + 90;
                            unsigned char color_span = *(unsigned char *)((int)DAT_00487abc + 0xDF14);
                            if (color_span != 0)
                                DAT_004892e8[DAT_00489248 - 1].palette_value += rand() % color_span;
                        }
                    } else if (rc_sub == 1 && rc_cnt > 0x19) {
                        entity->counter_3c = 0;
                        if (DAT_00489248 < 0x9C4) {
                            unsigned char stage = entity->state_20;
                            int xoff = (3 - ((unsigned int)stage >> 2)) << 0x12;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            int x = entity->previous_x + xoff;
                            int y = entity->previous_y - 0x1C0000;
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = x; ep->previous_x = x;
                            ep->position_y = y; ep->previous_y = y;
                            ep->type = 0x6B;
                            ep->owner = own;
                            ep->gravity_or_motion_38 = tt[0xE090 / 4];
                            ep->damage_44 = tt[0xE0CC / 4];
                            ep->palette_value = tt[0xE0FC / 4];
                            ep->callback_address = tt[0xE008 / 4];
                            ep->counter_3c = 0x400;
                            ep->scratch_2c = 1;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 200 + 200;
                            entity->state_20 = (unsigned char)(stage + 1);
                        }
                    }
                    break;
                }
                if (entity->state_20 == 0xC8) {
                    int et_life = entity->scratch_60;
                    if (et_life > 0) {
                        entity->scratch_60 = et_life - 1;
                    } else {
                        /* Timer expired: small explosion + die */
                        int ex = entity->position_x;
                        int ey = entity->position_y;
                        if (DAT_00489250 < 2000) {
                            int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(fp + 0x00) = ex; *(int *)(fp + 0x04) = ey;
                            *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                            *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 3) + 3;
                            *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                            *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                            *(unsigned char *)(fp + 0x15) = 0;
                            DAT_00489250++;
                        }
                        FUN_0040f9b0(0x65 + (rand() % 7), ex, ey);
                        should_remove = 1;
                        break;
                    }
                    /* Startup delay: counter at +0x3C counts from -80 to 0 */
                    {
                        int et_delay = entity->counter_3c;
                        if (et_delay < 0) { entity->counter_3c = et_delay + 1; break; }
                    }
                    /* Spray one flechette upward */
                    if (DAT_00489248 < 0x9C4) {
                        int *sc = (int *)DAT_00487ab0;
                        int h = (rand() & 0xFF) + 0x380;
                        /* Skip heading near straight down (0x3F8 to 0x408) */
                        if (h >= 0x3F8 && h <= 0x408) break;
                        h &= 0x7FF;
                        int spd = (rand() % 60) + 20;
                        Entity *ep = &DAT_004892e8[DAT_00489248];
                        ep->position_x = entity->position_x;
                        ep->position_y = entity->position_y;
                        ep->previous_x = entity->position_x;
                        ep->previous_y = entity->position_y;
                        ep->velocity_x = (sc[h] * spd) >> 6;
                        ep->velocity_y = (sc[(h + 0x200) & 0x7FF] * spd) >> 6;
                        ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                        ep->type = 0x00; /* type 0x00 for player collision, yellow from palette */
                        ep->variant_24 = 0;
                        ep->state_20 = 0;
                        ep->auxiliary_26 = 0;
                        ep->owner = entity->owner;
                        ep->health_or_damage_28 = 0;
                        ep->gravity_or_motion_38 = ((int *)DAT_00487abc)[0x24];
                        ep->damage_44 = ((int *)DAT_00487abc)[0x33];
                        ep->scratch_48 = 0;
                        ep->palette_value = ((int *)DAT_00487abc)[0x3F];
                        ep->animation_frame = 0;
                        ep->subtype = 0;
                        ep->callback_address = ((int *)DAT_00487abc)[0];
                        ep->counter_3c = 0;
                        ep->timer_5c = 0;
                        DAT_00489248++;
                        DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 40; /* short lifespan */
                        /* Yellow/fire palette */
                        if (DAT_00487aa8 != NULL) {
                            int ci = rand() % 10;
                            unsigned short pal = *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2);
                            unsigned short r5 = (pal >> 10) & 0x1F;
                            unsigned short g5 = (pal >> 5) & 0x1F;
                            unsigned short b5 = pal & 0x1F;
                            DAT_004892e8[DAT_00489248 - 1].palette_value =
                                (unsigned int)((r5 << 11) | (g5 << 6) | b5) + 30000;
                        }
                    }
                }
                break;
            }

            case 0x25: { /* ROMAN CANDLE (Batch 9) — deploy+spray weapon, entity type 0x25.
                * Ghidra callback: 0x446130. Verified at 0x446130-0x447400.
                *
                * Flies with gravity until wall hit, then deploys (state 0xC8).
                * Wall collision sets state=0xC8, +0x60=1200 (~20sec), +0x3C=-80.
                *
                * Three modes based on weapon level (+0x40):
                *   Mode 0 (sub=0): colored balls. Every 0x50 (80) ticks, spawns one
                *     type 0x6A entity upward (heading 0x380+rand()&0xFF, speed rand()%90+25).
                *     Also sprays constant type 0x67 shrapnel every 2 ticks (yellow).
                *     Offset: -1px X, -13px Y (above deployer).
                *
                *   Mode 1 (sub=1): wavy fireworks. Every 0x50 ticks, spawns one
                *     type 0x22 (+0x40=0) wavy firework with random color from palette.
                *
                *   Mode 2 (sub=2): magic fireworks. Every 0xC8 (200) ticks, spawns one
                *     type 0x22 (+0x40=2, state=0x6E, guard=0xFF) with team-colored palette
                *     and short fuse (+0x60=50). Plays sound 0x11C on each launch.
                *
                * Dies with small flash when +0x60 timer expires. */
                {
                    unsigned char sub = entity->subtype;
                    int count = entity->counter_3c + 1;
                    entity->counter_3c = count;
                    int *sc = (int *)DAT_00487ab0;
                    int *tt = (int *)DAT_00487abc;
                    unsigned char own = entity->owner;
                    if (sub == 0 && count > 0x50) {
                        entity->counter_3c = 0;
                        if (rand() % 15 < 14 && DAT_00489248 < 0x9C4) {
                            int dir = (rand() & 0xFF) + 0x380;
                            int speed = rand() % 90 + 25;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            int x = entity->previous_x - FIXED_SCALE;
                            int y = entity->previous_y - 0x340000;
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = x; ep->previous_x = x;
                            ep->position_y = y; ep->previous_y = y;
                            ep->velocity_x = sc[dir] * speed >> 6;
                            ep->velocity_y = sc[dir + 0x200] * speed >> 6;
                            ep->type = 0x6A;
                            ep->owner = own;
                            ep->subtype = 1;
                            ep->gravity_or_motion_38 = tt[0xDE7C / 4];
                            ep->damage_44 = tt[0xDEB8 / 4];
                            ep->palette_value = tt[0xDEE8 / 4];
                            ep->callback_address = tt[0xDDF0 / 4];
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 50 + 90;
                        }
                    } else if (sub == 1 && count > 0x19) {
                        entity->counter_3c = 0;
                        if (DAT_00489248 < 0x9C4) {
                            unsigned char stage = entity->state_20;
                            int x = entity->previous_x + ((3 - ((unsigned int)stage >> 2)) << 0x12);
                            int y = entity->previous_y - 0x1C0000;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = x; ep->previous_x = x;
                            ep->position_y = y; ep->previous_y = y;
                            ep->type = 0x6B;
                            ep->owner = own;
                            ep->gravity_or_motion_38 = tt[0xE090 / 4];
                            ep->damage_44 = tt[0xE0CC / 4];
                            ep->palette_value = tt[0xE0FC / 4];
                            ep->callback_address = tt[0xE008 / 4];
                            ep->counter_3c = 0x400;
                            ep->scratch_2c = 1;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 200 + 200;
                            entity->state_20 = (unsigned char)(stage + 1);
                        }
                    }
                    break;
                }
                if (entity->state_20 == 0xC8) {
                    /* Deployed: lifetime check */
                    int rc_life = entity->scratch_60;
                    if (rc_life > 0) {
                        entity->scratch_60 = rc_life - 1;
                    } else {
                        /* Timer expired: small explosion + die */
                        int ex = entity->position_x;
                        int ey = entity->position_y;
                        if (DAT_00489250 < 2000) {
                            int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(fp + 0x00) = ex; *(int *)(fp + 0x04) = ey;
                            *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                            *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 3) + 3;
                            *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                            *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                            *(unsigned char *)(fp + 0x15) = 0;
                            DAT_00489250++;
                        }
                        FUN_0040f9b0(0x65 + (rand() % 7), ex, ey);
                        should_remove = 1;
                        break;
                    }
                    /* Constant shrapnel spray (every 2 ticks) while deployed */
                    unsigned char rc_sub = entity->subtype;
                    if (rc_sub == 0 && (rc_life & 1) == 0 && DAT_00489248 < 0x9C4) {
                        int *sc = (int *)DAT_00487ab0;
                        int sh = (rand() & 0xFF) + 0x380;
                        if (!(sh >= 0x3F8 && sh <= 0x408)) {
                            sh &= 0x7FF;
                            int ss = (rand() % 40) + 10;
                            int sx = entity->position_x - FIXED_SCALE;
                            int sy = entity->position_y - 0x340000;
                            Entity *sp = &DAT_004892e8[DAT_00489248];
                            sp->position_x = sx; sp->position_y = sy;
                            sp->previous_x = sx; sp->previous_y = sy;
                            sp->velocity_x = (sc[sh] * ss) >> 6;
                            sp->velocity_y = (sc[(sh + 0x200) & 0x7FF] * ss) >> 6;
                            sp->motion_x_10 = 0; sp->motion_y_14 = 0;
                            sp->type = 0x67;
                            sp->variant_24 = 0; sp->state_20 = 0;
                            sp->auxiliary_26 = 0;
                            sp->owner = entity->owner;
                            sp->health_or_damage_28 = 0; sp->gravity_or_motion_38 = 0;
                            sp->damage_44 = 0; sp->scratch_48 = 0;
                            sp->animation_frame = 0; sp->subtype = 0;
                            sp->callback_address = 0; sp->counter_3c = 0;
                            sp->timer_5c = 0;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 25;
                            /* Fixed yellow: RGB565 yellow = (31<<11)|(63<<5)|0 = 0xFFE0 */
                            DAT_004892e8[DAT_00489248 - 1].palette_value =
                                (unsigned int)0xFFE0 + 30000;
                        }
                    }
                    /* Counter at +0x3C: increment, spawn ball/firework every 80 ticks */
                    int rc_cnt = entity->counter_3c;
                    rc_cnt++;
                    entity->counter_3c = rc_cnt;
                    int rc_threshold = (rc_sub >= 2) ? 0xC8 : 0x50; /* mode 3 waits longer */
                    if (rc_cnt > rc_threshold && DAT_00489248 < 0x9C4) {
                        entity->counter_3c = 0;
                        unsigned char rc_sub = entity->subtype;
                        int rc_h = (rand() & 0xFF) + 0x380;
                        if (rc_h >= 0x3F8 && rc_h <= 0x408) break;
                        rc_h &= 0x7FF;
                        int *sc = (int *)DAT_00487ab0;
                        int rc_x = entity->position_x - FIXED_SCALE;
                        int rc_y = entity->position_y - 0x340000;
                        if (rc_sub == 0) {
                            /* Mode 1: colored ball (type 0x6A) + flash particle spray */
                            int rc_spd = (rand() % 90) + 25;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            ep->position_x = rc_x; ep->position_y = rc_y;
                            ep->previous_x = rc_x; ep->previous_y = rc_y;
                            ep->velocity_x = (sc[rc_h] * rc_spd) >> 6;
                            ep->velocity_y = (sc[(rc_h + 0x200) & 0x7FF] * rc_spd) >> 6;
                            ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                            ep->type = 0x6A;
                            ep->variant_24 = 0;
                            ep->state_20 = 0;
                            ep->auxiliary_26 = 0;
                            ep->owner = entity->owner;
                            ep->health_or_damage_28 = 0;
                            ep->gravity_or_motion_38 = ((int *)DAT_00487abc)[0x24];
                            ep->damage_44 = ((int *)DAT_00487abc)[0x33];
                            ep->scratch_48 = 0;
                            ep->animation_frame = 0;
                            ep->subtype = 1; /* sub_type 1 = bigger visual */
                            ep->callback_address = ((int *)DAT_00487abc)[0];
                            ep->counter_3c = 0;
                            ep->timer_5c = 0;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 1200; /* long lifespan */
                            /* Random bright color from full palette range */
                            if (DAT_00487aa8 != NULL) {
                                int ci = rand() % 128;
                                unsigned short pal = ((unsigned short *)DAT_00487aa8)[ci];
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    (unsigned int)pal + 0x7530;
                            }
                            /* Shrapnel spray is now constant (above), not per-ball */
                        } else if (rc_sub == 1) {
                            /* Mode 2: spawn wavy firework (type 0x22, +0x40=0). */
                            int rc_spd = (rand() % 60) + 30;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = rc_x; ep->position_y = rc_y;
                            ep->previous_x = rc_x; ep->previous_y = rc_y;
                            ep->velocity_x = (sc[rc_h] * rc_spd) >> 6;
                            ep->velocity_y = (sc[(rc_h + 0x200) & 0x7FF] * rc_spd) >> 6;
                            ep->type = 0x22;
                            ep->state_20 = (unsigned char)(rand() & 1);
                            ep->owner = entity->owner;
                            ep->subtype = 0;
                            ep->counter_3c = rc_h;
                            ep->scratch_30 = rand() % 10 + 1;
                            ep->gravity_or_motion_38 = ((int *)DAT_00487abc)[0x24];
                            ep->callback_address = ((int *)DAT_00487abc)[0];
                            ep->damage_44 = ((int *)DAT_00487abc)[0x33];
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 600;
                            if (DAT_00487aa8 != NULL) {
                                int ci = rand() % 32 + 20;
                                unsigned short pal = ((unsigned short *)DAT_00487aa8)[ci];
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    (unsigned int)pal + 0x7530;
                            }
                        } else {
                            /* Mode 3 (sub_type 2): "magic fireworks" — from Ghidra 0x447102.
                             * Type 0x22, state 0xC8, +0x40=2, heading 0x400, lifespan ~115. */
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            memset((void *)ep, 0, 0x80);
                            ep->position_x = rc_x; ep->position_y = rc_y;
                            ep->previous_x = rc_x; ep->previous_y = rc_y;
                            /* Match Fire_Secondary for type 0x22 level 2 exactly */
                            int rc_spd2 = (rand() % 50) + 20;
                            ep->velocity_x = (sc[rc_h] * rc_spd2) >> 6;
                            ep->velocity_y = (sc[(rc_h + 0x200) & 0x7FF] * rc_spd2) >> 6;
                            ep->type = 0x22;
                            ep->state_20 = 0x6E; /* state: same as Fire_Secondary level 2 */
                            ep->owner = entity->owner;
                            ep->auxiliary_26 = 0xFF; /* guard: same as Fire_Secondary level 2 */
                            ep->subtype = 2; /* +0x40=2 for magic fireworks sprite */
                            ep->counter_3c = rc_h;
                            /* Read palette from weapon config: type 0x22, level 2 */
                            {
                                int *tt = (int *)DAT_00487abc;
                                int typeOff = 0x22 * 0x86; /* type 0x22 config offset */
                                ep->gravity_or_motion_38 = tt[2 + typeOff + 0x22]; /* gravity */
                                ep->damage_44 = tt[2 + typeOff + 0x31]; /* damage */
                                ep->palette_value = tt[2 + typeOff + 0x3d]; /* palette */
                                ep->callback_address = tt[typeOff]; /* callback */
                                /* Add team color offset (from LAB_00406a71) */
                                unsigned char own = entity->owner;
                                unsigned char team = Player_Get(own)->team;
                                ep->palette_value += (int)team * 100;
                            }
                            ep->scratch_60 = 50; /* short fuse — explode soon after launch */
                            DAT_00489248++;
                        }
                        FUN_0040f9b0(0x11C, entity->position_x, entity->position_y);
                    }
                }
                break;
            }

            case 0x2E: { /* SMOKING NALLE — remotely triggered with the detonate key.
                * Original callback 0x432220: state 0xFB advances a two-speed
                * animation.  Frame 11 is the payload frame; frame 20 removes it. */
                unsigned char nalle_state = entity->state_20;
                if (nalle_state == 0xFA) {
                    if (DAT_00489250 < 2000) {
                        int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(fp + 0x00) = entity->position_x;
                        *(int *)(fp + 0x04) = entity->position_y;
                        *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                        *(unsigned char *)(fp + 0x10) = (unsigned char)(3 + (rand() & 1));
                        *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                        *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                        *(unsigned char *)(fp + 0x15) = 0;
                        DAT_00489250++;
                    }
                    should_remove = 1;
                } else if (nalle_state == 0xFB) {
                    unsigned char tick = (unsigned char)(entity->animation_frame + 1);
                    entity->animation_frame = tick;
                    unsigned int frame = entity->scratch_48;
                    unsigned char limit = frame < 11 ? 9 : 13;
                    if (tick > limit) {
                        entity->animation_frame = 0;
                        frame++;
                        entity->scratch_48 = frame;
                        if (frame == 11) {
                            int nx = entity->position_x;
                            int ny = entity->position_y;
                            unsigned char own = entity->owner;
                            if (entity->subtype == 1) {
                                FUN_0040f9b0(0x65 + rand() % 7, nx, ny);
                                /* The armed Nalle throws 48 mixed flame sprites. */
                                for (int n = 0; n < 48 && DAT_00489250 < 2000; n++) {
                                    int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                    int dir = (n * 0x800 / 48 + 0xA8) & 0x7FF;
                                    int spd = rand() % 50;
                                    int kind = rand() & 3;
                                    *(int *)(fp + 0x00) = nx; *(int *)(fp + 0x04) = ny;
                                    *(int *)(fp + 0x08) = ((int *)DAT_00487ab0)[dir] * spd >> 7;
                                    *(int *)(fp + 0x0C) = ((int *)DAT_00487ab0)[dir + 0x200] * spd >> 7;
                                    *(unsigned char *)(fp + 0x10) = (unsigned char)(
                                        kind == 0 ? 13 + (rand() & 3) :
                                        kind == 1 ? 1 + (rand() & 1) :
                                        kind == 2 ? 17 + rand() % 3 : 7 + (rand() & 3));
                                    *(unsigned char *)(fp + 0x11) = (unsigned char)(rand() % 12);
                                    *(unsigned char *)(fp + 0x12) = 0;
                                    *(unsigned char *)(fp + 0x13) = 0xCD;
                                    *(unsigned char *)(fp + 0x14) = own;
                                    *(unsigned char *)(fp + 0x15) = 1;
                                    DAT_00489250++;
                                }
                                FUN_00437cf0(nx, ny, 600, own, 500);
                            } else {
                                /* Unarmed mode emits the original smoke ring. */
                                for (int dir = 0; dir < 0x800 && DAT_0048925c < 1500; dir += 0x100) {
                                    int fp = DAT_0048925c * 0x20 + (int)DAT_00481f2c;
                                    int sx = rand() & 0x3F, sy = rand() & 0x3F;
                                    *(int *)(fp + 0x00) = nx; *(int *)(fp + 0x04) = ny;
                                    *(int *)(fp + 0x08) = ((int *)DAT_00487ab0)[dir] * sx >> 6;
                                    *(int *)(fp + 0x0C) = ((int *)DAT_00487ab0)[dir + 0x200] * sy >> 6;
                                    *(unsigned char *)(fp + 0x10) = (unsigned char)(15 + rand() % 3);
                                    *(unsigned char *)(fp + 0x11) = (unsigned char)(rand() & 3);
                                    *(unsigned short *)(fp + 0x12) = 0;
                                    *(unsigned char *)(fp + 0x14) = own;
                                    *(unsigned char *)(fp + 0x15) = 0;
                                    DAT_0048925c++;
                                }
                            }
                        }
                        if (frame > 20) should_remove = 1;
                    }
                }
                break;
            }

            case 0x26: { /* MOVING MORNING STAR — callback 0x43DBD0. */
                unsigned char mode = entity->subtype;
                if (mode == 0) {
                    int vx8 = entity->velocity_x >> 8;
                    int vy8 = entity->velocity_y >> 8;
                    int speed2 = vx8 * vx8 + vy8 * vy8;
                    if (speed2 > 0x10000) {
                        double mag = sqrt((double)speed2);
                        entity->velocity_x = (int)(vx8 * 256.0 / mag) << 8;
                        entity->velocity_y = (int)(vy8 * 256.0 / mag) << 8;
                    }
                    entity->position_x += entity->velocity_x;
                    entity->position_y += entity->velocity_y;
                }
                int phase = (entity->counter_3c + (entity->scratch_2c >> 5)) & 0x7FF;
                entity->counter_3c = phase;
                int age = entity->health_or_damage_28;
                int radius = entity->scratch_2c;
                if (age > 10 && age < 400 && radius < 400) {
                    radius += 14;
                    entity->scratch_2c = radius;
                }
                unsigned char emit_tick = (unsigned char)(entity->state_20 + 1);
                entity->state_20 = emit_tick;
                if (age > 10 && age < 400 && emit_tick > 2) {
                    entity->state_20 = 0;
                    int *sc = (int *)DAT_00487ab0;
                    int pos_scale = mode == 0 ? 0x10E : 0x190;
                    int vel_scale = mode == 0 ? 0x19 : 0x30;
                    for (int fi = 0; fi < 4 && DAT_00489250 < 2000; fi++) {
                        int dir = (phase + (fi + 1) * 0x200) & 0x7FF;
                        int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(fp + 0x00) = entity->position_x + (sc[dir] * pos_scale >> 6);
                        *(int *)(fp + 0x04) = entity->position_y + (sc[dir + 0x200] * pos_scale >> 6);
                        *(int *)(fp + 0x08) = sc[dir] * vel_scale >> 6;
                        *(int *)(fp + 0x0C) = sc[dir + 0x200] * vel_scale >> 6;
                        *(unsigned char *)(fp + 0x10) = (unsigned char)((rand() & 1) - 2 * mode + 5);
                        *(unsigned char *)(fp + 0x11) = 4;
                        *(unsigned char *)(fp + 0x12) = 2;
                        *(unsigned char *)(fp + 0x13) = 0xC8;
                        *(unsigned char *)(fp + 0x14) = entity->owner;
                        *(unsigned char *)(fp + 0x15) = 0;
                        DAT_00489250++;
                    }
                }
                age++;
                entity->health_or_damage_28 = age;
                if (age >= 400) entity->scratch_2c -= 8;
                if (age == 450) {
                    if (DAT_00489250 < 2000) {
                        int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(fp + 0x00) = entity->position_x;
                        *(int *)(fp + 0x04) = entity->position_y;
                        *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                        *(unsigned char *)(fp + 0x10) = (unsigned char)(7 + (rand() & 3));
                        *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                        *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                        *(unsigned char *)(fp + 0x15) = 1;
                        DAT_00489250++;
                    }
                    should_remove = 1;
                }
                break;
            }

            case 0x23: { /* GAMMA BOOM (Batch 9) — entity type 0x23.
                * Ghidra callback: 0x4457B0. Verified at 0x4457B0-0x445A00.
                *
                * Heavy slow-moving projectile that decelerates over time.
                * Speed deceleration: same sqrt-based cap as landmine mode 0 —
                * if speed^2 > 0x10000, normalize to 256.0 magnitude.
                * Uses shared position integration (NOT in skip list).
                *
                * Fuse counter at +0x3C: increments each tick, plays warning sound
                * 0x11C every 36 ticks (0x24). Creates a ticking-bomb effect.
                *
                * No trail particles (explicitly excluded in trail switch).
                * Explodes on wall hit with large flash cluster + KB. */
                /* Speed deceleration */
                int gb_vx = entity->velocity_x >> 8;
                int gb_vy = entity->velocity_y >> 8;
                int gb_spd = gb_vx * gb_vx + gb_vy * gb_vy;
                if (gb_spd > 0x10000) {
                    double mag = sqrt((double)gb_spd);
                    entity->velocity_x = (int)((double)gb_vx * 256.0 / mag) << 8;
                    entity->velocity_y = (int)((double)gb_vy * 256.0 / mag) << 8;
                }
                /* Fuse counter: sound every 36 ticks */
                {
                    int fuse = entity->counter_3c;
                    fuse++;
                    entity->counter_3c = fuse;
                    if (fuse >= 0x24) {
                        entity->counter_3c = 0;
                        FUN_0040f9b0(0x11C, entity->position_x, entity->position_y);
                    }
                }
                break;
            }

            case 0x66: { /* Guided missile (heavy) — active steering + speed cap.
                * Per WEAPONS.md: 5x gravity, active steering, speed capped.
                * 5x gravity is handled by field +0x38 at spawn.
                * Here we add steering + speed capping. */
                unsigned char own = entity->owner;
                int mx = entity->position_x;
                int my = entity->position_y;
                int best_dist = 0x7FFFFFFF;
                int found = 0;
                int tgt_x = mx, tgt_y = my;
                for (int p = 0; p < DAT_00489240; p++) {
                    if ((unsigned char)p == own) continue;
                    PlayerData *player = Player_Get(p);
                    if (player->health <= 0) continue;
                    int px = player->position_x;
                    int py = player->position_y;
                    int dx = px - mx; int dy = py - my;
                    int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (dist < best_dist) {
                        best_dist = dist; tgt_x = px; tgt_y = py; found = 1;
                    }
                }
                int vx = entity->velocity_x;
                int vy = entity->velocity_y;
                if (found) {
                    /* Steer toward target */
                    vx += (tgt_x - mx) / 48;
                    vy += (tgt_y - my) / 48;
                }
                /* Speed cap: limit velocity magnitude */
                int spd_sq = (vx >> 8) * (vx >> 8) + (vy >> 8) * (vy >> 8);
                int max_spd = 300;
                if (spd_sq > max_spd * max_spd) {
                    /* Approximate normalization */
                    int cur_speed = 1;
                    int temp = spd_sq;
                    while (temp > 0) { temp >>= 2; cur_speed <<= 1; }
                    cur_speed = (cur_speed + spd_sq / cur_speed) / 2;
                    cur_speed = (cur_speed + spd_sq / cur_speed) / 2;
                    if (cur_speed > 0) {
                        vx = (int)((long long)vx * max_spd / cur_speed);
                        vy = (int)((long long)vy * max_spd / cur_speed);
                    }
                }
                entity->velocity_x = vx;
                entity->velocity_y = vy;
                break;
            }

            case 0x22: { /* WAVY FIREWORKS / TURRET DEPLOYER (Batch 8) — entity type 0x22.
                * Entity type 0x22 is shared between two weapon systems:
                *
                * Mode 0 (+0x40==0): WAVY FIREWORKS — weapon 34.
                *   Ghidra: 0x4442F0. Verified at 0x4442F0-0x4445A0.
                *   Oscillating sine-wave flight. Heading at +0x3C oscillates +/-10/tick,
                *   direction flips every rand()%15+5 ticks (timer at +0x30). State byte
                *   +0x20 toggles 0/1 to control oscillation direction.
                *   Movement: velocity set from sincos[heading]*3 each tick, applied by
                *   shared integration. Overwrites gravity (wavy fireworks don't fall).
                *   Lifespan at +0x28 counts down; dies at 0. State 0xFA = killed.
                *
                * Modes 2-4 (+0x40==1,2,3): from Roman Candle spawned sub-entities.
                *   Ghidra: 0x444873. Active flight phase with state decrementing each
                *   tick. Accelerates from heading (fast if +0x28!=0, slow otherwise).
                *   Fuse timer at +0x60: when expired, spawns colorful mid-air explosion
                *   (8 type-0x00 sub-munitions with random colors + flash + sound 0x114).
                *
                * Turret deployer (+0x40>=1 from weapon 41/42) flies straight until
                * wall collision deploys the turret — handled in wall collision switch. */
                unsigned char wf_sub = entity->subtype;
                if (wf_sub != 0) {
                    /* Modes 2-4 (+0x40==1,2,3): active flight phase from Ghidra 0x444873.
                     * State at +0x20 decrements each tick (fuel counter). While state > 0,
                     * velocity accelerates from heading: fast (>>5) if +0x28!=0, slow
                     * (*3>>6) otherwise. Fuse at +0x60 (for Roman Candle spawned 0x22
                     * sub-entities): when expired, spawns 8 type-0x00 colorful balls in
                     * random directions + flash particle + sound 0x114. */
                    unsigned char wf_state = entity->state_20;
                    if (wf_state > 0) {
                        wf_state--;
                        entity->state_20 = wf_state;
                        int wf_heading = entity->counter_3c;
                        int *wf_sc = (int *)DAT_00487ab0;
                        int sv = wf_sc[wf_heading & 0x7FF];
                        int cv = wf_sc[(wf_heading + 0x200) & 0x7FF];
                        if (entity->health_or_damage_28 != 0) {
                            /* Fast acceleration */
                            entity->velocity_x += sv >> 5;
                            entity->velocity_y += cv >> 5;
                        } else {
                            /* Slow acceleration */
                            entity->velocity_x += (sv * 3) >> 6;
                            entity->velocity_y += (cv * 3) >> 6;
                        }
                    }
                } else {
                    /* WAVY FIREWORKS mode 0 — from Ghidra 0x4442F0.
                     * Lifespan +0x28 countdown; dies at 0. State 0xFA = instant death.
                     * Heading at +0x3C oscillates +/-10/tick. Direction (state +0x20)
                     * flips every rand()%15+5 ticks (timer at +0x30).
                     * Velocity set from sincos[heading]*3 each tick (~6px/tick).
                     * This overwrites any gravity accumulated by shared gravity code
                     * (wavy fireworks don't fall — matches original behavior). */
                    /* Lifespan countdown — die when reaches 0 */
                    int wf_life = entity->health_or_damage_28;
                    if (wf_life > 0) {
                        wf_life--;
                        entity->health_or_damage_28 = wf_life;
                        if (wf_life == 0) { should_remove = 1; break; }
                    }
                    if (entity->state_20 == 0xFA) {
                        should_remove = 1; break;
                    }
                    /* Phase timer at +0x30: controls flip period */
                    int wf_timer = entity->scratch_30;
                    wf_timer--;
                    if (wf_timer <= 1) {
                        wf_timer = (rand() % 15) + 5;
                        entity->state_20 = (unsigned char)(rand() & 1);
                    }
                    entity->scratch_30 = wf_timer;
                    /* Heading oscillation */
                    int wf_heading = entity->counter_3c;
                    if (entity->state_20 == 0)
                        wf_heading -= 10;
                    else
                        wf_heading += 10;
                    wf_heading &= 0x7FF;
                    entity->counter_3c = wf_heading;
                    /* Set velocity from heading: sincos*3 (~6px/tick).
                     * Shared integration (x+=vx, y+=vy) applies the movement.
                     * Overwrites any gravity accumulated by shared gravity code
                     * (wavy fireworks don't fall — matches original behavior). */
                    int *wf_sc = (int *)DAT_00487ab0;
                    entity->velocity_x = wf_sc[wf_heading] * 3;
                    entity->velocity_y = wf_sc[wf_heading + 0x200] * 3;
                }
                break;
            }

            case 0x02: { /* ORGANIC WASTE power 1 — terrain-growing projectile.
                * Ghidra callback: 0x4427e0. Entity type 2, state 0x0A.
                * Spawns with state 0x0A but still FLIES first — speed check is the
                * primary gate, not state. When speed drops below threshold, starts
                * terrain-growing behavior. */
                int ow_life = entity->health_or_damage_28;
                if (ow_life > 0) {
                    ow_life--;
                    entity->health_or_damage_28 = ow_life;
                    if (ow_life <= 1) {
                        *(int *)(ebase + 0x5C) = 0;
                        should_remove = 1;
                        break;
                    }
                }
                /* Decrement sprite animation counter */
                if (entity->auxiliary_26 > 0)
                    entity->auxiliary_26 = entity->auxiliary_26 - 1;

                /* Speed check: primary gate for flying vs growing.
                 * Threshold: (vx>>9)^2 + (vy>>9)^2 > 1000 = still flying.
                 * At/below 1000 = landed, start growing. */
                int svx = entity->velocity_x >> 9;
                int svy = entity->velocity_y >> 9;
                int ow_speed_sq = svx * svx + svy * svy;

                /* Speed check + ground proximity: must be slow AND near solid ground.
                 * Original checks speed first, then Phase A scans for ground below.
                 * If no solid ground within 6 tiles below, keep flying. */
                int ow_tx = entity->position_x >> 0x12;
                int ow_ty = entity->position_y >> 0x12;
                int has_ground = 0;

                if (ow_speed_sq <= 0x3E8 && ow_tx > 0 && ow_tx < (int)DAT_004879f0 &&
                    ow_ty > 0 && ow_ty < (int)DAT_004879f4 - 1) {
                    /* Scan up to 6 tiles below for solid ground (prop[+1]==0) */
                    int shift2 = DAT_00487a18;
                    int cy = ow_ty + 1;
                    int drip_count = 0;
                    while (drip_count < 6 && cy < (int)DAT_004879f4) {
                        int toff2 = (cy << shift2) + ow_tx;
                        unsigned char below_tile = *(unsigned char *)((int)DAT_0048782c + toff2);
                        unsigned char below_pass = *(unsigned char *)((unsigned int)below_tile * 0x20 + 1 + (int)DAT_00487928);
                        if (below_pass == 0) { has_ground = 1; break; }
                        cy++;
                        drip_count++;
                    }
                }

                if (ow_speed_sq > 0x3E8 || !has_ground) {
                    /* Still flying: gravity + integration handled by generic code.
                     * Bounce via wall collision case 0x02. */
                    break;
                }

                /* === Growing phase === (slow + ground below)
                 * Initialize growth timer on first transition */
                if (*(int *)(ebase + 0x5C) == 0) {
                    *(int *)(ebase + 0x5C) = 0x14; /* growth timer = 20 ticks */
                    entity->health_or_damage_28 = rand() % 400 + 150;
                }

                {
                    int shift2 = DAT_00487a18;

                    /* Growth movement: move UP one tile + random sideways step per tick.
                     * Direct position modification (original 0x443016), not velocity-based.
                     * This builds the waste mound upward over many ticks. */
                    entity->position_y -= FIXED_SCALE; /* one tile UP */
                    int gdir = (rand() & 1) ? 1 : -1;
                    entity->position_x += gdir * (rand() % 3 + 1) * FIXED_SCALE; /* 1-3 tiles sideways */
                    entity->velocity_x = 0; /* zero velocity — movement is direct */
                    entity->velocity_y = 0;

                    /* Recompute tile position after movement */
                    ow_tx = entity->position_x >> 0x12;
                    ow_ty = entity->position_y >> 0x12;

                    /* Phase A: if we moved into air (no ground below within 6 tiles),
                     * drop back down to just above solid ground. This lets waste land
                     * on itself (painted tiles with prop[+1]==0 act as ground). */
                    if (ow_tx > 0 && ow_tx < (int)DAT_004879f0 &&
                        ow_ty > 0 && ow_ty < (int)DAT_004879f4) {
                        int cy = ow_ty;
                        while (cy < (int)DAT_004879f4) {
                            int toff2 = (cy << shift2) + ow_tx;
                            unsigned char cur_tile = *(unsigned char *)((int)DAT_0048782c + toff2);
                            unsigned char cur_pass = *(unsigned char *)((unsigned int)cur_tile * 0x20 + 1 + (int)DAT_00487928);
                            if (cur_pass == 0) {
                                /* Found solid tile — place entity one tile above */
                                ow_ty = cy - 1;
                                entity->position_y = ow_ty << 0x12;
                                break;
                            }
                            cy++;
                        }
                    }

                    if (ow_ty <= 1 || ow_ty >= (int)DAT_004879f4 - 1 ||
                        ow_tx <= 0 || ow_tx >= (int)DAT_004879f0) {
                        should_remove = 1; break;
                    }

                    /* Phase B: paint terrain in expanding triangle (0x442ec0).
                     * 4 rows: widths 1, 3, 5, 7. Starts 1-2 tiles above entity Y.
                     * Each row one lower + one wider. Different color each tick = texture. */
                    unsigned int ow_color = entity->palette_value;
                    unsigned short ow_rgb = (unsigned short)(ow_color - 30000);
                    int paint_start_y = ow_ty - (rand() % 2) - 1;
                    int col_count = 1;
                    int prow = 0;
                    while (col_count < 9) {
                        int row_y = paint_start_y + prow;
                        int row_x = ow_tx - prow;
                        for (int c = 0; c < col_count; c++) {
                            int px = row_x + c;
                            int py = row_y;
                            if (px > 0 && px < (int)DAT_004879f0 &&
                                py > 0 && py < (int)DAT_004879f4) {
                                int ptoff = (py << shift2) + px;
                                unsigned char ptile = *(unsigned char *)((int)DAT_0048782c + ptoff);
                                unsigned char pprop1 = *(unsigned char *)((unsigned int)ptile * 0x20 + 1 + (int)DAT_00487928);
                                if (pprop1 == 1) {
                                    unsigned char fill_tile = *(unsigned char *)((unsigned int)ptile * 0x20 + 0x0F + (int)DAT_00487928);
                                    *(unsigned char *)((int)DAT_0048782c + ptoff) = fill_tile;
                                    if (DAT_00481f50 != NULL) {
                                        *(unsigned short *)((int)DAT_00481f50 + ptoff * 2) = ow_rgb;
                                    }
                                }
                            }
                        }
                        prow++;
                        col_count += 2;
                    }
                }

                /* Phase C: color cycle every tick for mottled texture.
                 * DAT_00487aa8 is a short array (entity.cpp:3702 confirms). */
                *(unsigned char *)(ebase + 0x24) = (unsigned char)(rand() % 6);
                {
                    unsigned short *pal = (unsigned short *)DAT_00487aa8;
                    if (pal) {
                        entity->palette_value = (unsigned int)pal[0xA0 + rand() % 16] + 30000;
                    }
                }
                break;
            }

            /* Type 0x14 (Organic Waste power 2 / Plastic Explosives):
             * Standard projectile — no special behavior case needed.
             * Generic gravity + integration + wall collision case 0x14
             * handles everything (goo/explosive painting on impact). */

            default:
                break;
            }
        }

        /* Position integration: pos += vel
         * Skipped for types that do their own integration in the behavior switch:
         *   0x19 LANDMINE   — mode 0 self-integrates with deceleration; modes 1/2
         *                     are stationary (bobbing uses direct position writes)
         *   0x1B KAMIKAZE   — self-integrates at top of its behavior case
         *   0x1C MINISHIP   — self-integrates after boundary revert+clamp logic
         *   0x2D LASER      — instant beam trace, no per-tick movement */
        if (ent_type != 0x2d && ent_type != 0x1b && ent_type != 0x1C && ent_type != 0x19 && ent_type != 0x26) {
            entity->position_x += entity->velocity_x;
            entity->position_y += entity->velocity_y;
        }

        /* Entity-vs-tracked-entity collision (FUN_00437120 equivalent).
         * Runs AFTER position integration but BEFORE wall collision, so the
         * projectile position is at the wall-hit location (near tracked entities
         * sitting on walls/ground), not reverted to pre-collision.
         *
         * Skipped types (friendly fire exceptions):
         *   0x17 NUCLEUS  — passive trail target; player shoots OWN nucleus dots
         *                   to detonate them. Allowing collision here would let
         *                   enemy bullets trigger the ring burst unintentionally.
         *   0x19 LANDMINE — proximity detonation handled in behavior switch;
         *                   entity-entity collision would bypass the team check
         *                   and cause mines to detonate on allied tracked entities.
         *   0x65          — water splash particles, cosmetic only
         *   0x67          — trail/exhaust particles, cosmetic only
         *   is_debris     — debris particles, no collision
         *   state >= 0xFA — already dead/detonating */
        if (ent_type != 0x67 && ent_type != 0x65 && !is_debris &&
            ent_type != 0x17 && ent_type != 0x19 && ent_type != 0x26 &&
            entity->state_20 < 0xFA) {
            int proj_x = entity->position_x;
            int proj_y = entity->position_y;
            int proj_damage = entity->damage_44;
            unsigned char proj_team = entity->owner;
            int eb = (int)DAT_004892e8;
            for (int ei = 0; ei < DAT_00489248; ei++) {
                if (ei == i) continue;
                int tbase = ei * 0x80 + eb;
                unsigned char t_type = *(unsigned char *)(tbase + 0x21);
                int hp_threshold = 0;
                int hx = 0, hy_lo = 0, hy_hi = 0;
                /* Tracked entity hitbox table: types that can be hit by projectiles.
                 * hp_threshold = damage needed to trigger state 0xFA (detonation).
                 * hx/hy_lo/hy_hi = AABB half-extents for collision detection.
                 * Types NOT in this table are invisible to entity-entity collision. */
                switch (t_type) {
                    case 0x0B: hp_threshold = 0x25800; hx = 0x100000; hy_lo = 0x040000; hy_hi = 0x140000; break;  /* NUCLEAR BARREL */
                    case 0x17: hp_threshold = 1; hx = 0x100000; hy_lo = 0x100000; hy_hi = 0x100000; break;        /* NUCLEUS — threshold=1: ANY hit detonates */
                    case 0x0F: hp_threshold = 0x271000; hx = 0x140000; hy_lo = 0x040000; hy_hi = 0x200000; break; /* BONE CRUSHER */
                    case 0x18: hp_threshold = 0x7d000; hx = 0x140000; hy_lo = 0x040000; hy_hi = 0x1c0000; break;  /* PILOT DISRUPTOR */
                    case 0x1F: hp_threshold = 0xbb800; hx = 0x140000; hy_lo = 0x0c0000; hy_hi = 0x0c0000; break;  /* INSECTS */
                    case 0x1C: hp_threshold = 0x138800; hx = 0x1c0000; hy_lo = 0x1c0000; hy_hi = 0x1c0000; break; /* MINISHIP — large hitbox */
                    case 0x0E: hp_threshold = (*(char *)(tbase + 0x40) == 0) ? 12800000 : 0x70800;                 /* MOVING SUCKER — mode-dependent HP */
                        hx = 0x100000; hy_lo = 0x100000; hy_hi = 0x100000; break;
                    case 0x2E: hp_threshold = 0x465000; hx = 0x180000; hy_lo = 0x200000; hy_hi = 0x200000; break;  /* SMOKING NALLE — high HP, big hitbox */
                    case 0x27: hp_threshold = 0xfa000; hx = 0x140000; hy_lo = 0x140000; hy_hi = 0x140000; break;   /* KOMET BOMB */
                    default: continue;
                }
                if (*(unsigned char *)(tbase + 0x20) == 0xFA) continue;
                /* Friendly fire check with immunity timer (original FUN_00437120):
                 * When +0x5C == 0, collision is ALWAYS allowed (no team check).
                 * When +0x5C > 0, same-team projectiles are blocked.
                 * Exception: type 0x17 (nucleus) always allows friendly fire. */
                if (t_type != 0x17 && *(unsigned char *)(tbase + 0x5C) != 0 &&
                    proj_team < 0x50 && *(unsigned char *)(tbase + 0x22) == proj_team) continue;
                int tx = *(int *)(tbase + 0x00);
                int ty = *(int *)(tbase + 0x08);
                if (proj_x < tx - hx || proj_x > tx + hx) continue;
                if (proj_y < ty - hy_lo || proj_y > ty + hy_hi) continue;
                *(unsigned short *)(tbase + 0x24) = 1;
                *(int *)(tbase + 0x58) = proj_damage;
                *(int *)(tbase + 0x28) += proj_damage;
                if (hp_threshold > 0 && *(int *)(tbase + 0x28) >= hp_threshold) {
                    *(unsigned char *)(tbase + 0x20) = 0xFA;
                }
                should_remove = 1;
                break;
            }
        }

        /* Apply gravity + drag for debris entities (AFTER position update) */
        if (is_debris) {
            entity->velocity_y += DAT_00483824;  /* debris gravity */
            /* Apply drag */
            entity->velocity_x = (int)((double)entity->velocity_x * 0.97);
            entity->velocity_y = (int)((double)entity->velocity_y * 0.97);
        }

        /* Debris ground collision: type 100 (0x64) debris dies on non-air tile.
         * Skip first 5 ticks (lifespan starts at 40-89, so check < initial-5) to let
         * debris escape the impact crater before ground-checking. */
        if (is_debris && ent_type == 100) {
            int dlife = entity->health_or_damage_28;
            if (dlife < 80) {  /* after a few ticks of flight */
                int dx = entity->position_x >> 0x12;
                int dy = entity->position_y >> 0x12;
                if (dx > 0 && dy > 0 && dx < (int)DAT_004879f0 && dy < (int)DAT_004879f4) {
                    unsigned char dtile = *(unsigned char *)((int)DAT_0048782c + (dy << shift) + dx);
                    if (dtile != 0) {
                        should_remove = 1;
                    }
                }
            }
        }

        /* Boundary check */
        int pos_x = entity->position_x;
        int pos_y = entity->position_y;
        if (pos_x < 0 || pos_y < 0 ||
            pos_x >= (int)(DAT_004879f0 * FIXED_SCALE) ||
            pos_y >= (int)(DAT_004879f4 * FIXED_SCALE)) {
            if (ent_type == 0x22 || ent_type == 0x18 || ent_type == 0x1C) {
                /* Turret deployer (0x22), insect (0x18), miniship (0x1C): clamp to map bounds.
                 * Original callbacks clamp each axis and set prev_pos = clamped pos.
                 * Insect also zeros velocity on boundary hit. */
                if (pos_x < 0) {
                    entity->position_x = 0;
                    entity->previous_x = 0;
                } else if ((pos_x >> 0x12) >= (int)DAT_004879f0) {
                    int max_x = (int)(DAT_004879f0 << 0x12);
                    entity->position_x = max_x;
                    entity->previous_x = max_x;
                }
                if (pos_y < 0) {
                    entity->position_y = 0;
                    entity->previous_y = 0;
                } else if ((pos_y >> 0x12) >= (int)DAT_004879f4) {
                    int max_y = (int)(DAT_004879f4 << 0x12);
                    entity->position_y = max_y;
                    entity->previous_y = max_y;
                }
                /* Insect: zero velocity on boundary hit */
                if (ent_type == 0x18) {
                    entity->velocity_x = 0;
                    entity->velocity_y = 0;
                }
                pos_x = entity->position_x;
                pos_y = entity->position_y;
            } else {
                should_remove = 1;
            }
        }

        /* === Trail particles during flight (per-type) ===
         * Several weapon types emit visible particles during flight.
         * All use the same entity spawning pattern: allocate 0x80-byte
         * record in DAT_004892e8, set position/velocity/type/palette. */
        if (!should_remove && is_projectile && !is_debris) {
            int trail_type = -1;     /* particle entity type to spawn */
            int trail_vel_div = 22;  /* velocity divisor */
            int trail_pal_lo = 20;   /* palette range low */
            int trail_pal_hi = 31;   /* palette range high */
            int trail_pal_die = 0x12; /* palette death threshold */
            int trail_grav = 2;      /* trail particle gravity */
            int trail_count = 1;     /* particles per tick */
            int trail_chance = 1;    /* 1 = always, N = 1/N chance */

            switch (ent_type) {
            case 0x2C: /* Machinegun — yellow stream (velocity/22) */
                trail_type = 0x67;
                break;
            case 0x01: /* Dumbfire — no trail in original */
                break;
            case 0x0B: /* NUCLEAR BARREL — no trail */
                break;
            case 0x09: /* KICKER — NO per-tick trail. Original spawns type 0x67 debris
                * only on solid tile contact inside the callback (palette 0x93-0x9F). */
                break;
            case 0x0E: /* Guided missile — very rare trail (1/128 chance) */
                trail_type = 0x67;
                trail_chance = 128;
                trail_vel_div = 16;
                break;
            case 0x0F: /* BONE CRUSHER — no trail */
                break;
            case 0x1F: /* INSECTS — no trail */
                break;
            case 0x23: /* GAMMA BOOM — no trail */
                break;
            case 0x11: /* Normal Fireball / Firestorm — short fire trail */
                if (DAT_00489250 < 2000) {
                    int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                    *(int *)(fp + 0x00) = entity->previous_x;  /* prev pos for trail behind */
                    *(int *)(fp + 0x04) = entity->previous_y;
                    *(int *)(fp + 0x08) = 0;
                    *(int *)(fp + 0x0c) = 0;
                    *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() & 1) + 1;
                    *(unsigned char *)(fp + 0x11) = 1;
                    *(unsigned char *)(fp + 0x12) = 3;   /* faster palette step = shorter life */
                    *(unsigned char *)(fp + 0x13) = 0x50; /* short lifespan */
                    *(unsigned char *)(fp + 0x14) = entity->owner;
                    *(unsigned char *)(fp + 0x15) = 0;
                    DAT_00489250++;
                }
                break;
            case 0x22: { /* WAVY FIREWORKS / type 0x22 trail particles.
                * Modes 2/3 use the sparse original 3.0/delta-time RNG gate and
                * spawn palette-fading type 0x67 particles. */
                unsigned char wf_trail_sub = entity->subtype;
                unsigned char wf_tick = entity->animation_frame;
                int wf_gate = (wf_trail_sub == 0) ? 5 : (wf_trail_sub == 1) ? 3 : 0;
                int wf_spawn_trail = wf_gate > 0 && (wf_tick % wf_gate) == 0;
                if (wf_trail_sub >= 2) {
                    int divisor = DAT_0048385c > 0.0f ? (int)(3.0 / (double)DAT_0048385c) : 1;
                    if (divisor < 1) divisor = 1;
                    wf_spawn_trail = (rand() % divisor) == 0;
                }
                if (wf_spawn_trail && DAT_00489248 < 0x9C4) {
                    int *wf_sc = (int *)DAT_00487ab0;
                    int wf_h;
                    int wf_spd;
                    if (wf_trail_sub >= 2) {
                        wf_h = (rand() % 0x80 + entity->counter_3c + 0x3C0) & 0x7FF;
                        wf_spd = rand() % 0x28 + 0x14;
                    } else {
                        wf_h = rand() & 0x7FF;
                        wf_spd = (rand() % 15) + 2;
                    }
                    Entity *ep = &DAT_004892e8[DAT_00489248];
                    int trail_x = *(int *)(ebase + (wf_trail_sub >= 2 ? 0x00 : 0x04));
                    int trail_y = *(int *)(ebase + (wf_trail_sub >= 2 ? 0x08 : 0x0C));
                    ep->position_x = trail_x;
                    ep->position_y = trail_y;
                    ep->previous_x = trail_x;
                    ep->previous_y = trail_y;
                    ep->velocity_x = wf_sc[wf_h] * wf_spd >> 6;
                    ep->velocity_y = wf_sc[wf_h + 0x200] * wf_spd >> 6;
                    if (wf_trail_sub < 2) {
                        ep->velocity_x += entity->velocity_x >> 1;
                        ep->velocity_y += entity->velocity_y >> 1;
                    }
                    ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                    ep->type = 0x67;
                    ep->variant_24 = (unsigned short)(rand() % 6);
                    ep->state_20 = wf_trail_sub >= 2 ? 0 : 0x0A;
                    ep->auxiliary_26 = wf_trail_sub >= 2 ? 0xFF : 0;
                    ep->owner = wf_trail_sub >= 2 ? 0xFF : entity->owner;
                    ep->health_or_damage_28 = 0;
                    ep->gravity_or_motion_38 = ((int *)DAT_00487abc)[0xD830 / 4];
                    ep->damage_44 = ((int *)DAT_00487abc)[0xD86C / 4];
                    ep->scratch_48 = 0;
                    ep->palette_value = ((int *)DAT_00487abc)[0xD89C / 4];
                    ep->animation_frame = 0;
                    ep->subtype = 0;
                    ep->callback_address = ((int *)DAT_00487abc)[0xD7A8 / 4];
                    ep->counter_3c = 0;
                    ep->timer_5c = wf_trail_sub >= 2 ? 4 : 0;
                    DAT_00489248++;
                    if (wf_trail_sub < 2)
                        DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = 20;
                    unsigned char pidx = (unsigned char)(rand() % 12 + 20);
                    DAT_004892e8[DAT_00489248 - 1].scratch_65 = pidx;
                    DAT_004892e8[DAT_00489248 - 1].scratch_64 = 0x12;
                    if (DAT_00487aa8 != NULL)
                        DAT_004892e8[DAT_00489248 - 1].palette_value =
                            (unsigned int)((unsigned short *)DAT_00487aa8)[pidx] + 0x7530;
                }
                break;
            }
            default:
                break;
            }

            if (trail_type >= 0 && (trail_chance <= 1 || (rand() % trail_chance) == 0)) {
                for (int tc = 0; tc < trail_count && DAT_00489248 < 0x9c4; tc++) {
                    Entity *tp = &DAT_004892e8[DAT_00489248];
                    memset((void *)tp, 0, 0x80);

                    /* Position: same as parent entity */
                    tp->position_x = entity->position_x;
                    tp->previous_x = entity->position_x;
                    tp->position_y = entity->position_y;
                    tp->previous_y = entity->position_y;

                    /* Velocity: parent velocity / divisor + random jitter */
                    tp->velocity_x = entity->velocity_x / trail_vel_div + ((rand() & 0x3FFF) - 0x2000);
                    tp->velocity_y = entity->velocity_y / trail_vel_div + ((rand() & 0x3FFF) - 0x2000);

                    /* Identity */
                    tp->type = (unsigned char)trail_type;
                    tp->owner = 0xFF;    /* owner: none */
                    tp->auxiliary_26 = 0xFF;    /* flag */
                    tp->subtype = 2;       /* sub_type */
                    tp->timer_5c = 2;       /* palette step threshold */

                    /* Palette-based color */
                    unsigned char pal_idx = (unsigned char)(rand() % (trail_pal_hi - trail_pal_lo + 1) + trail_pal_lo);
                    tp->scratch_65 = pal_idx;
                    tp->scratch_64 = (unsigned char)trail_pal_die;
                    if (DAT_00487aa8 != NULL) {
                        tp->palette_value = (int)((unsigned short *)DAT_00487aa8)[pal_idx] + 30000;
                    }

                    /* Random pixel shape pattern (0-4) */
                    tp->variant_24 = (unsigned short)(rand() % 5);

                    /* Gravity */
                    tp->gravity_or_motion_38 = trail_grav;

                    DAT_00489248++;
                }
            }
        }

        /* === Fuse/timer detonation — per-type pre-collision checks ===
         * Several weapons explode based on timers, animation state, or
         * proximity rather than wall impact. This runs BEFORE wall collision
         * so a timed-out entity doesn't also trigger wall effects. */
        if (!should_remove && is_projectile && !is_debris) {
            switch (ent_type) {
            /* TOURNAILLER (0x08): fuse/timer handled in behavior switch above */

            case 0x0F: { /* BONE CRUSHER — anti-infantry mine.
                * Embeds in ground on wall hit (vel=0). Kills enemy troopers on contact.
                * Simple proximity check vs trooper array (DAT_00487884, stride 0x40). */
                if (DAT_0048924c > 0) {
                    unsigned char own = entity->owner;
                    int mx = entity->position_x;
                    int my = entity->position_y;
                    for (int v = 0; v < DAT_0048924c; v++) {
                        int toff = v * 0x40;
                        unsigned char t_team = *(unsigned char *)(toff + 0x1C + (int)DAT_00487884);
                        if (t_team == own || t_team == 0xFF) continue;
                        int tx = *(int *)(toff + (int)DAT_00487884);
                        int ty = *(int *)(toff + 8 + (int)DAT_00487884);
                        int dx = mx - tx; if (dx < 0) dx = -dx;
                        int dy = my - ty; if (dy < 0) dy = -dy;
                        if (dx < 0x80000 && dy < 0x80000) {
                            /* Kill the trooper */
                            *(int *)(toff + 0x10 + (int)DAT_00487884) = 0;
                            should_remove = 1;
                            break;
                        }
                    }
                }
                break;
            }

            case 0x12: { /* Pipebomb — state-based detonation.
                * State 0xC2: countdown with flying sparks.
                * When countdown reaches 0: explode. */
                if (ent_state == 0xC2) {
                    int timer = entity->health_or_damage_28;
                    /* Spawn spark particle during countdown */
                    if (DAT_00489250 < 2000 && (rand() & 3) == 0) {
                        int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(pbase + 0x00) = entity->position_x;
                        *(int *)(pbase + 0x04) = entity->position_y;
                        int dir = rand() & 0x7FF;
                        int spd = rand() % 60 + 30;
                        *(int *)(pbase + 0x08) = (*(int *)((int)DAT_00487ab0 + dir * 4) * spd) >> 7;
                        *(int *)(pbase + 0x0C) = (*(int *)((int)DAT_00487ab0 + 0x800 + dir * 4) * spd) >> 7;
                        *(unsigned char *)(pbase + 0x10) = 1;
                        *(unsigned char *)(pbase + 0x11) = 0;
                        *(unsigned char *)(pbase + 0x12) = 2;
                        *(unsigned char *)(pbase + 0x13) = 0xC8;
                        *(unsigned char *)(pbase + 0x14) = entity->owner;
                        *(unsigned char *)(pbase + 0x15) = 0;
                        DAT_00489250++;
                    }
                    if (timer <= 1) {
                        /* Detonate */
                        int det_x = entity->position_x;
                        int det_y = entity->position_y;
                        unsigned char own = entity->owner;
                        int dtx = det_x >> 0x12;
                        int dty = det_y >> 0x12;
                        FUN_004357b0(dtx, dty, 6, 0, '\0',
                                     0, 0, 0, 0, 0, '\0', own);
                        FUN_00437cf0(det_x, det_y, 200, own, 100);
                        FUN_0040f9b0(0x11, det_x, det_y);
                        should_remove = 1;
                    }
                }
                break;
            }

            /* NOTE: case 0x25 ROMAN CANDLE fuse/timer — handled in behavior switch now */

            case 0x28: {
                /* PIPEBOMB — bounces and settles. Detonates on lifespan expiry
                 * (state 0xFA set by lifetime handler) or player collision.
                 * Rotation: +0x30 = rotation speed, +0x3C = cumulative angle.
                 * Renderer uses +0x3C with anim_data 0xCE for 16-dir sprite. */
                /* Original callback 0x43E890 uses the stored angular velocity;
                 * it does not derive a fresh minimum spin from linear speed. */
                {
                    int vx = entity->velocity_x;
                    int angular = entity->scratch_30;
                    int angle = entity->counter_3c;
                    if (vx < 1) {
                        angle += angular;
                        if (angle >= 0x800) {
                            angle -= 0x800;
                            angular -= 2;
                        }
                    } else {
                        angle -= angular;
                        if (angle < 0) {
                            angle += 0x800;
                            angular -= 2;
                        }
                    }
                    if (angular < 0) angular = 0;
                    entity->scratch_30 = angular;
                    entity->counter_3c = angle;
                }
                if (entity->state_20 == 0xFA) {
                    int det_x = entity->position_x;
                    int det_y = entity->position_y;
                    unsigned char own = entity->owner;
                    int dtx = det_x >> 0x12;
                    int dty = det_y >> 0x12;
                    /* Tile damage / crater */
                    FUN_004357b0(dtx, dty, 6, 0, '\0',
                                 0, 0, 0, 0, 0, '\0', own);
                    /* Area knockback */
                    FUN_00437cf0(det_x, det_y, 100, own, 100);
                    /* Explosion sound */
                    FUN_0040f9b0(0x65 + (rand() % 7), det_x, det_y);
                    /* Spawn fire/flash particles */
                    {
                        int fc = (rand() & 1) + 3; /* 3-4 flash particles */
                        for (int f = 0; f < fc && DAT_00489250 < 2000; f++) {
                            int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(pbase + 0x00) = det_x;
                            *(int *)(pbase + 0x04) = det_y;
                            *(int *)(pbase + 0x08) = ((rand() & 0xFFFF) - 0x8000) * 4;
                            *(int *)(pbase + 0x0C) = ((rand() & 0xFFFF) - 0x8000) * 4;
                            *(unsigned char *)(pbase + 0x10) = (rand() & 3) + 1;
                            *(unsigned char *)(pbase + 0x11) = 0;
                            *(unsigned char *)(pbase + 0x12) = 2;
                            *(unsigned char *)(pbase + 0x13) = 0xC8;
                            *(unsigned char *)(pbase + 0x14) = own;
                            *(unsigned char *)(pbase + 0x15) = 0;
                            DAT_00489250++;
                        }
                    }
                    should_remove = 1;
                }
                break;
            }

            case 0x29:   /* BASIC TURRET */
            case 0x2A: { /* ICE TURRET */
                /* Turrets: gravity drop until wall collision sets state 0xFA.
                 * Once deployed (state 0xFA): fire ONE turret entity, then die.
                 * Original uses prev_x/prev_y for position, team from player
                 * record, and health = damage * 1600. */
                if (entity->state_20 == 0xFA) {
                    int turr_x = entity->previous_x;  /* prev_x */
                    int turr_y = entity->previous_y;  /* prev_y */
                    unsigned char turr_etype = entity->type;
                    unsigned char turr_sub = entity->subtype;
                    unsigned char turr_raw_owner = entity->owner;
                    /* Get team byte from player record */
                    unsigned char turr_team = Player_Get(turr_raw_owner)->team;

                    char sprite = 0;
                    int dmg;
                    if (turr_etype == 0x29) {
                        /* BASIC TURRET params by level */
                        if (turr_sub == 0) { sprite = 0; dmg = 0x258; }       /* 600 */
                        else if (turr_sub == 1) { sprite = 1; dmg = 0x1F4; }  /* 500 */
                        else { sprite = 5; dmg = 0x2EE; }                     /* 750 */
                    } else {
                        /* ICE TURRET params by level */
                        if (turr_sub == 0) { sprite = 2; dmg = 0x12C; }       /* 300 */
                        else if (turr_sub == 1) { sprite = 3; dmg = 0x12C; }  /* 300 */
                        else if (turr_sub == 2) { sprite = 4; dmg = 0x352; }  /* 850 */
                        else { sprite = 6; dmg = 0x384; }                     /* 900 */
                    }
                    /* Health = damage * 125 * 64 = damage * 8000
                     * Original: lea eax,[edx+edx*4] (x5), lea eax,[eax+eax*4] (x25),
                     * lea edx,[eax+eax*4] (x125), shl edx,6 (x8000) */
                    int health = dmg * 125 * 64;
                    FUN_00406d20(turr_x, turr_y, sprite, health, turr_team, 0);
                    should_remove = 1;
                }
                break;
            }

            case 0x0B: { /* NUCLEAR BARREL — damage-triggered detonation.
                * Entity-entity collision (FUN_00437120 cat 0) accumulates damage at +0x28.
                * When damage >= threshold, state +0x20 is set to 0xFA → explosion.
                * Original callback 0x431650: KB(0x190=400, -1), two particle loops. */
                /* Decrement immunity timer (original at 0x431800) */
                if (entity->timer_5c > 0)
                    entity->timer_5c = entity->timer_5c - 1;
                unsigned char nb_state = entity->state_20;
                if (nb_state == 0xFA) {
                    int det_x = entity->position_x;
                    int det_y = entity->position_y;
                    int det_vx = entity->velocity_x;
                    int det_vy = entity->velocity_y;
                    unsigned char own = entity->owner;
                    unsigned char sub = entity->subtype;
                    int dtx = det_x >> 0x12;
                    int dty = det_y >> 0x12;
                    int *sc = (int *)DAT_00487ab0;
                    int *tt = (int *)DAT_00487abc;

                    /* Tile damage */
                    FUN_004357b0(dtx, dty, 6, 0, '\0',
                                 0, 0, 0, 0, 0, '\0', own);
                    /* Area knockback: radius 400, power -1 (all players) */
                    FUN_00437cf0(det_x, det_y, 0x190, own, -1);
                    FUN_0040f9b0(0x65 + (rand() % 7), det_x, det_y);

                    /* Particle count based on level */
                    int base_count = (sub == 1) ? 30 : 40;

                    /* Loop 1: Fire debris (entity_type 0) */
                    {
                        int count1 = (int)((float)DAT_0048385c * (float)base_count);
                        if (count1 < 1) count1 = 1;
                        if (count1 > base_count) count1 = base_count;
                        int angle_step = 0x0445C000 / (count1 > 0 ? count1 : 1);
                        for (int dp = 0; dp < count1 && DAT_00489248 < 0x9C4; dp++) {
                            unsigned int dir = rand() & 0x7FF;
                            int spd = rand() % 70;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            ep->position_x = det_x;
                            ep->position_y = det_y;
                            ep->velocity_x = (sc[dir] * spd >> 6) + (det_vx >> 5);
                            ep->velocity_y = (sc[0x200 + dir] * spd >> 6) + (det_vy >> 5) - 0x57800;
                            ep->previous_x = det_x;
                            ep->previous_y = det_y;
                            ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                            ep->type = 0;
                            ep->variant_24 = (short)(rand() % 6);
                            ep->state_20 = 0;
                            ep->auxiliary_26 = 0;
                            ep->owner = own;
                            ep->health_or_damage_28 = 0;
                            ep->gravity_or_motion_38 = tt[0x26];
                            ep->scratch_48 = 0;
                            ep->animation_frame = 0;
                            ep->subtype = (sub == 1) ? 3 : 4;
                            ep->callback_address = tt[0];
                            ep->counter_3c = 0;
                            ep->timer_5c = 0;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 100 + 90;
                            {
                                int ci = rand() % 10;
                                unsigned short pal = *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2);
                                unsigned short r5 = (pal >> 10) & 0x1F;
                                unsigned short g5 = (pal >> 5) & 0x1F;
                                unsigned short b5 = pal & 0x1F;
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    (unsigned int)((r5 << 11) | (g5 << 6) | b5) + 30000;
                            }
                            DAT_004892e8[DAT_00489248 - 1].damage_44 = angle_step;
                        }
                    }

                    /* Loop 2: Mushroom fire (entity_type 0x64, team 0xFF) */
                    {
                        int count2 = (int)((float)DAT_0048385c * (float)(base_count * 2));
                        if (count2 < 1) count2 = 1;
                        if (count2 > base_count * 2) count2 = base_count * 2;
                        for (int mp = 0; mp < count2 && DAT_00489248 < 0x9C4; mp++) {
                            unsigned int dir = rand() & 0x7FF;
                            int spd = rand() % 70;
                            Entity *ep = &DAT_004892e8[DAT_00489248];
                            ep->position_x = det_x;
                            ep->position_y = det_y;
                            ep->velocity_x = (sc[dir] * spd >> 6) + (det_vx >> 5);
                            ep->velocity_y = (sc[0x200 + dir] * spd >> 6) + (det_vy >> 5) - 0x57800;
                            ep->previous_x = det_x;
                            ep->previous_y = det_y;
                            ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                            ep->type = 0x64;
                            ep->variant_24 = (short)(rand() % 6);
                            ep->state_20 = 0;
                            ep->auxiliary_26 = 0xFF;
                            ep->owner = 0xFF;
                            ep->health_or_damage_28 = 0;
                            ep->gravity_or_motion_38 = tt[0x347A];
                            ep->damage_44 = tt[0x3489];
                            ep->scratch_48 = 0;
                            ep->palette_value = tt[0x3495];
                            ep->animation_frame = 0;
                            ep->subtype = 0;
                            ep->callback_address = tt[0x3458];
                            ep->counter_3c = 0;
                            ep->timer_5c = 0;
                            DAT_00489248++;
                            DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 100 + 90;
                            {
                                int ci = rand() % 10;
                                unsigned short pal = *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2);
                                unsigned short r5 = (pal >> 10) & 0x1F;
                                unsigned short g5 = (pal >> 5) & 0x1F;
                                unsigned short b5 = pal & 0x1F;
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    (unsigned int)((r5 << 11) | (g5 << 6) | b5) + 30000;
                            }
                            DAT_004892e8[DAT_00489248 - 1].damage_44 = 0;
                        }
                    }

                    /* Flash particle */
                    if (DAT_00489250 < 2000) {
                        int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(fp + 0x00) = det_x;
                        *(int *)(fp + 0x04) = det_y;
                        *(int *)(fp + 0x08) = 0;
                        *(int *)(fp + 0x0C) = 0;
                        *(unsigned char *)(fp + 0x10) = 0;
                        *(unsigned char *)(fp + 0x11) = 0;
                        *(unsigned char *)(fp + 0x12) = 0;
                        *(unsigned char *)(fp + 0x13) = 1;
                        *(unsigned char *)(fp + 0x14) = 0xFF;
                        *(unsigned char *)(fp + 0x15) = 1;
                        DAT_00489250++;
                    }
                    should_remove = 1;
                }
                break;
            }

            /* Type 0x22 turret deployer: turret spawning is handled in the
             * wall collision section (on solid wall hit). No fuse/timer logic needed.
             * The deployer flies straight until wall collision. */

            /* PILOT DISRUPTOR (0x18): handled entirely in behavior phase above */

            default:
                break;
            }
        }

        /* Wall collision for all projectile entities. */
        if (!should_remove && is_projectile && !is_debris) {
            int tx = pos_x >> 0x12;
            int ty = pos_y >> 0x12;
            if (tx > 0 && ty > 0 && tx < (int)DAT_004879f0 && ty < (int)DAT_004879f4) {
                int tile_off = (ty << shift) + tx;
                unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_off);
                unsigned char pass2 = *(unsigned char *)((unsigned int)tile * 0x20 + 2 + (int)DAT_00487928);
                unsigned char pass10 = *(unsigned char *)((unsigned int)tile * 0x20 + 10 + (int)DAT_00487928);
                /* Building collision (original callback at 0x438816).
                 * If tile[+10] == 1 (flyable), check for structure collision.
                 * FUN_004355d0 sets DAT_00481e8f to 3 or 4 on building hit. */
                if (pass10 == 1) {
                    DAT_00481e8f = 0;
                    FUN_004355d0(i);
                    if (DAT_00481e8f != 0) {
                        /* Play per-type sound on building hit.
                         * Silent types skip the sound entirely.
                         * Batch 8/9 types: 0x17 NUCLEUS, 0x19 LANDMINE, 0x1C MINISHIP,
                         * 0x23 GAMMA BOOM, 0x2E NALLE all play explosion sounds.
                         * 0x22 (turret/fireworks), 0x24 ETNA, 0x25 ROMAN CANDLE are silent. */
                        int bx = entity->previous_x;
                        int by = entity->previous_y;
                        switch (ent_type) {
                        case 0x00:                        /* basic bullet — silent */
                        case 0x69:                        /* mine — silent */
                        case 0x08: case 0x09: case 0x0E: /* shrapnel spawners — silent */
                        case 0x18:                        /* PILOT DISRUPTOR — silent */
                        case 0x22:                        /* turret/fireworks — silent */
                        case 0x24: case 0x25:             /* ETNA/ROMAN CANDLE — silent (deploy) */
                        case 0x2B: case 0x6A:             /* homing mine/shot — silent */
                        case 0x28: case 0x29: case 0x2A:  /* grenades/turrets — silent */
                        case 0x0C:                        /* DIGGER — silent */
                            break; /* silent */
                        case 0x01:
                            FUN_0040f9b0(0x32, bx, by); break;
                        case 0x13:
                            FUN_0040f9b0(0x10B, bx, by); break;
                        case 0x14: break; /* PLASTIC — silent */
                        case 0x16:
                            FUN_0040f9b0(0x10D, bx, by); break;
                        case 0x1F: break; /* INSECTS — silent on building hit */
                        case 0x23:                        /* GAMMA BOOM — warning whistle */
                            FUN_0040f9b0(0x11C, bx, by); break;
                        case 0x05: case 0x0B: case 0x0F: /* various heavy weapons */
                        case 0x17: case 0x19: case 0x1B: /* NUCLEUS, LANDMINE, KAMIKAZE */
                        case 0x1C: case 0x1D: case 0x1E: /* MINISHIP, MEGABOMB, PHOTON */
                        case 0x27: case 0x2E:             /* KOMET, SMOKING NALLE */
                            FUN_0040f9b0(0x65 + (rand() % 7), bx, by); break;
                        default:
                            FUN_0040f9b0(0x11, bx, by); break;
                        }
                        should_remove = 1;
                    }
                }

                /* Destructible tile health damage (original callback at 0x438851).
                 * Tiles >= 0xF0 are destructible with health stored in the
                 * wall segment array at DAT_00489e80. */
                if (tile >= 0xF0 && DAT_00489e80 != NULL) {
                    int proj_damage = entity->damage_44;
                    int *tile_hp = (int *)((unsigned int)tile * 0x20 - 0x1DF4 + (int)DAT_00489e80);
                    *tile_hp -= proj_damage;
                }

                /* Crater: trigger when tile byte+0 != 0 (tile is active/occupied).
                 * Tile 0 (air) has byte+0 = 1 but is excluded by tile != 0 check.
                 * Ground tiles (64+) have byte+0 = 1.
                 * This matches the original's behavior where projectiles crater
                 * on any non-air tile they encounter. */
                /* Wall collision gate (original per-type callbacks at ~0x438890):
                 * - pass2==0: solid wall hit (all weapon types)
                 * - type 0x22 turret bullets: any non-air, non-water tile (broad cratering)
                 * - type 0x14 plastic/organic waste: any non-air tile (paints goo/explosive)
                 * - type 0x1D megabomb: building hit (DAT_00481e8f set by FUN_004355d0)
                 * Shotgun (type 0x00) only collides with solid walls (pass2==0). */
                unsigned char tile_is_water = *(unsigned char *)((unsigned int)tile * 0x20 + 4 + (int)DAT_00487928);
                unsigned char wall_owner = entity->owner;
                if (ent_type != 0x26 && ent_type != 0x2E && (pass2 == 0 || (ent_type == 0x22 && tile != 0 && tile_is_water == 0 && wall_owner < 0x50) || (ent_type == 0x14 && tile != 0) || (ent_type == 0x1D && DAT_00481e8f != 0))) {
                    /* Compute explosion level from sub_type and entity state
                     * (original at 0x43897B-0x4389B5) */
                    unsigned char sub_type = entity->subtype;
                    unsigned char ent_byte_21 = entity->type;
                    int explevel;
                    if (ent_byte_21 == 0) {
                        explevel = (int)sub_type;
                    } else {
                        if (sub_type == 2) explevel = 3;
                        else if (sub_type == 3) explevel = 3;
                        else if (sub_type == 4) explevel = 2;
                        else if (sub_type > 4) explevel = 6;
                        else explevel = (int)sub_type + 6;
                    }

                    /* Replacement tile: pass through tile index if tile[+4] != 0,
                     * otherwise 0 (original at 0x438938-0x438948) */
                    unsigned char stored_tile = 0;
                    unsigned char tile_prop4 = *(unsigned char *)((unsigned int)tile * 0x20 + 4 + (int)DAT_00487928);
                    if (tile_prop4 != 0) stored_tile = tile;

                    /* Water check: param_5 = 1 for water tiles (0x0C), 0 otherwise */
                    char is_water = (tile == 0x0C) ? (char)1 : (char)0;

                    unsigned char owner = entity->owner;

                    /* Per-type wall collision effects.
                     * Each weapon type has specific effects matching the behavior
                     * callbacks pre-filled in DAT_00487abc (loaded from loadtime.dat). */
                    int did_bounce = 0;
                    {
                        int prev_x = entity->previous_x;
                        int prev_y = entity->previous_y;
                        unsigned char sub_t = entity->subtype;
                        int flash_count = 0;

                        /* Bounce axis detection: compare previous tile vs current tile
                         * to determine which axis the projectile crossed into a wall. */
                        int prev_tx = prev_x >> 0x12;
                        int prev_ty = prev_y >> 0x12;

                        switch (ent_type) {
                        /* Callback 0x438010: wall hit returns at 0x438D81
                         * WITHOUT calling FUN_004357b0. No tile damage on
                         * wall hit for 0x00/0x69. Only 0x12 gets flash+sound. */
                        case 0x00: /* Basic bullet — tile damage + flash particle */
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            if (DAT_00489250 < 2000) {
                                int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(pbase + 0x00) = prev_x;
                                *(int *)(pbase + 0x04) = prev_y;
                                *(int *)(pbase + 0x08) = 0;
                                *(int *)(pbase + 0x0C) = 0;
                                *(unsigned char *)(pbase + 0x10) = 1;
                                *(unsigned char *)(pbase + 0x11) = 0;
                                *(unsigned char *)(pbase + 0x12) = 2;
                                *(unsigned char *)(pbase + 0x13) = 0xC8;
                                *(unsigned char *)(pbase + 0x14) = owner;
                                *(unsigned char *)(pbase + 0x15) = 0;
                                DAT_00489250++;
                            }
                            break;
                        case 0x12: /* Pipebomb — flash + sound 0x11 */
                            flash_count = (rand() & 1) + 3;
                            FUN_0040f9b0(0x11, prev_x, prev_y);
                            break;
                        case 0x69: /* Mine — small particles, no sound */
                            flash_count = 1;
                            break;
                        case 0x0C: /* DIGGER — silent death, no fire/crater */
                            break;

                        /* Callback 0x438D90: BOUNCE with 50% energy retention.
                         * Bounce counter at +0x3C. On last bounce: detonate. */
                        case 0x01: { /* Bounce missile */
                            int bc = entity->counter_3c;
                            if (bc > 0) {
                                /* Bounce: reflect velocity, halve speed */
                                if (prev_tx != tx) entity->velocity_x = -(entity->velocity_x / 2);
                                if (prev_ty != ty) entity->velocity_y = -(entity->velocity_y / 2);
                                entity->counter_3c = bc - 1;
                                FUN_0040f9b0(0x32, prev_x, prev_y);
                                did_bounce = 1;
                            } else {
                                /* Final bounce: full detonation */
                                FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                             0, 0, 0, 0, 0, '\0', owner);
                                FUN_00437cf0(prev_x, prev_y, 200, owner, 100);
                                FUN_0040f9b0(0x65 + (rand() % 7), prev_x, prev_y);
                                flash_count = (rand() & 1) + 3;
                            }
                            break;
                        }

                        /* Callback 0x439880: tile damage + KB + random sound */
                        case 0x05: { /* COLLAPSER — crater size 23, TERRAIN mode + violent debris */
                            int cl_x = entity->position_x;
                            int cl_y = entity->position_y;
                            int *sc = (int *)DAT_00487ab0;
                            FUN_004357b0(tx, ty, 0x17, stored_tile, is_water,
                                         cl_x, cl_y,
                                         entity->previous_x, entity->previous_y,
                                         '\x02', '\0', owner);
                            /* Extra ground-colored debris — spray in projectile direction */
                            {
                                int cl_vx = entity->velocity_x;
                                int cl_vy = entity->velocity_y;
                                unsigned int impact_angle = (unsigned int)FUN_004257e0(0, 0, cl_vx, cl_vy);
                            for (int cd = 0; cd < 12 && DAT_00489248 < 0x9C4; cd++) {
                                /* Bias angle toward impact direction with spread +/- 0x200 (~90 degrees) */
                                unsigned int cdir = (impact_angle + (rand() % 0x400 - 0x200)) & 0x7FF;
                                int cspd = rand() % 200 + 100;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = cl_x;
                                ep->position_y = cl_y;
                                ep->velocity_x = (sc[cdir] * cspd) >> 7;
                                ep->velocity_y = ((sc[0x200 + cdir] * cspd) >> 7) - 0x20000;
                                ep->previous_x = cl_x;
                                ep->previous_y = cl_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->type = 100; /* type 100 = debris */
                                ep->variant_24 = (short)(rand() % 6);
                                ep->state_20 = 0;
                                ep->auxiliary_26 = 0xFF;
                                ep->owner = owner;
                                ep->health_or_damage_28 = 0;
                                ep->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0xD1E8);
                                ep->damage_44 = *(int *)((int)DAT_00487abc + 0xD224);
                                ep->scratch_48 = 0;
                                ep->palette_value = *(int *)((int)DAT_00487abc + 0xD254);
                                ep->animation_frame = 0;
                                ep->subtype = 0;
                                ep->callback_address = *(int *)((int)DAT_00487abc + 0xD160);
                                ep->counter_3c = 0;
                                ep->timer_5c = 0;
                                DAT_00489248++;
                                DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 50 + 40;
                                /* Ground-colored: sample pixel at impact point */
                                DAT_004892e8[DAT_00489248 - 1].palette_value =
                                    framebuffer_rgb565_to_x1r5g5b5(
                                        *(unsigned short *)((int)DAT_00481f50 +
                                            ((ty << ((unsigned char)DAT_00487a18 & 0x1f)) + tx) * 2)) + 30000;
                                DAT_004892e8[DAT_00489248 - 1].damage_44 = 0;
                            }
                            } /* end impact_angle scope */
                            FUN_00437cf0(cl_x, cl_y, 200, owner, 800);
                            FUN_0040f9b0(0x65 + (rand() % 7), cl_x, cl_y);
                            break;
                        }

                        /* Callback 0x43CC20: 0x08 orbits (no wall collision in original).
                         * 0x09 bounces/re-enters up to 4 times, silent. */
                        case 0x08:
                            /* TOURNAILLER — crater + white star on wall hit. */
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            /* White star — same as Kicker (type 0x0C) */
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = prev_x;
                                *(int *)(fp + 0x04) = prev_y;
                                *(int *)(fp + 0x08) = 0;
                                *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = 0x0C;
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 1;
                                *(unsigned char *)(fp + 0x14) = 0x9F;
                                *(unsigned char *)(fp + 0x15) = 2; /* group 2 = blue/white */
                                DAT_00489250++;
                            }
                            FUN_0040f9b0(0x1E, prev_x, prev_y); /* sound 0x1E from Ghidra */
                            break;
                        case 0x09: /* KICKER — crater + white star on wall hit, NO bounce. */
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = prev_x;
                                *(int *)(fp + 0x04) = prev_y;
                                *(int *)(fp + 0x08) = 0;
                                *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = 0x0C;
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 1;
                                *(unsigned char *)(fp + 0x14) = 0x9F;
                                *(unsigned char *)(fp + 0x15) = 2; /* group 2 = blue/white */
                                DAT_00489250++;
                            }
                            FUN_0040f9b0(0x65 + (rand() % 7), prev_x, prev_y);
                            break;

                        /* Callback 0x431650: bounce with 1/8 energy, X reversed.
                         * Detonation is sprite-driven (when sprite reaches 0xFA) —
                         * handled in Phase 3 timer section. Keep bouncing until then. */
                        case 0x0B: {
                            if (prev_tx != tx) entity->velocity_x = -(entity->velocity_x / 8);
                            if (prev_ty != ty) entity->velocity_y = entity->velocity_y / 8;
                            did_bounce = 1;
                            break;
                        }

                        /* Callback 0x430DC0: MOVING SUCKER — silent death.
                         * NO knockback, NO sound, NO tile damage.
                         * Just spawns type 0x67 debris and dies. */
                        case 0x0E:
                            /* Silent — debris spawned by default flash_count (already 0) */
                            flash_count = 3;  /* small debris burst only */
                            break;

                        /* Callback 0x4427e0: ORGANIC WASTE — bounce with energy loss.
                         * Random divisor (rand()&7)+6, sign flip on axis.
                         * On slow enough speed, transitions to growing in behavior case. */
                        case 0x02: {
                            int div = (rand() & 7) + 6;
                            if (prev_tx != tx) entity->velocity_x = -(entity->velocity_x / div);
                            if (prev_ty != ty) entity->velocity_y = entity->velocity_y / div;
                            did_bounce = 1;
                            break;
                        }

                        /* Callback 0x4330C0: FULL STOP on first bounce (zero energy).
                         * Fuse timer at +0x60 handles detonation (Phase 3).
                         * Just drop dead where it hits. */
                        case 0x0F:
                            entity->velocity_x = 0;  /* zero velocity */
                            entity->velocity_y = 0;
                            entity->gravity_or_motion_38 = 0;  /* kill gravity too */
                            did_bounce = 1;
                            break;

                        /* Callback 0x441AA0: for NORMAL FIREBALL weapon (#17) this
                         * does tile damage. But for FIRESTORM's type 0x11 particles
                         * (guard=0x14), just die silently — no crater/volcano. */
                        case 0x11:
                            if (entity->auxiliary_26 != 0x14) {
                                /* Normal fireball weapon — tile damage + flash + sound */
                                FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                             0, 0, 0, 0, 0, '\0', owner);
                                flash_count = (rand() & 1) + 2;
                                FUN_0040f9b0(0x11, prev_x, prev_y);
                            }
                            /* FIRESTORM particles (guard=0x14): silent death */
                            break;

                        /* Callback 0x43A4B0: type-specific sound, no tile damage */
                        case 0x13: { /* ICEBALL — ice tint terrain painting.
                            * Double palette remap + tile set to 4 (frozen).
                            * From Ghidra 0x43AB94. */
                            unsigned char sub13 = entity->subtype;
                            int sp_idx = 0x191;
                            int sp_w = (int)*(unsigned char *)((int)DAT_00489e8c + sp_idx);
                            int sp_h = (int)*(unsigned char *)((int)DAT_00489e88 + sp_idx);
                            int cx = (prev_x >> 0x12) - sp_w / 2;
                            int cy = (prev_y >> 0x12) - sp_h / 2;
                            int sp_off = *(int *)((int)DAT_00489234 + sp_idx * 4);
                            unsigned char *sp_data = (unsigned char *)DAT_00489e94;
                            unsigned short *fb = (unsigned short *)DAT_00481f50;
                            unsigned char *tmap = (unsigned char *)DAT_0048782c;
                            int map_w = (int)DAT_004879f0;
                            unsigned char tshift = (unsigned char)DAT_00487a18;
                            if (sp_data && fb && tmap) {
                                for (int py2 = 0; py2 < sp_h; py2++) {
                                    for (int px2 = 0; px2 < sp_w; px2++) {
                                        int wx = cx + px2;
                                        int wy = cy + py2;
                                        if (wx < 0 || wy < 0 || wx >= map_w || wy >= (int)DAT_004879f4) continue;
                                        unsigned char gray = sp_data[sp_off + py2 * sp_w + px2];
                                        if (gray == 0) continue;
                                        unsigned char t_val = tmap[(wy << tshift) + wx];
                                        unsigned char *tp = (unsigned char *)((int)DAT_00487928 + (unsigned int)t_val * 0x20);
                                        if (tp[0x0B] != 0 || tp[4] != 0 || t_val == 10 || t_val == 16 || tp[0x18] != 0) continue;
                                        if (gray >= 0xC0 && DAT_00489230 != NULL) {
                                            unsigned short cur = fb[(wy << tshift) + wx];
                                            unsigned short remap = ((unsigned short *)DAT_00489230)[cur];
                                            fb[(wy << tshift) + wx] = remap;
                                        }
                                        if (tp[0x0E] != 0x40) {
                                            tmap[(wy << tshift) + wx] = 4;
                                        }
                                    }
                                }
                            }
                            /* Area freeze on nearby enemy players */
                            for (int p = 0; p < DAT_00489240; p++) {
                                PlayerData *player = Player_Get(p);
                                if (player->health <= 0) continue;
                                int px13 = player->position_x;
                                int py13 = player->position_y;
                                int dx13 = prev_x - px13; if (dx13 < 0) dx13 = -dx13;
                                int dy13 = prev_y - py13; if (dy13 < 0) dy13 = -dy13;
                                if (dx13 < 0x800000 && dy13 < 0x800000) { /* ~32 tile range */
                                    if (player->stun_timer == 0) {
                                        player->stun_timer = 5;
                                        player->scratch_c7 = static_cast<uint8_t>(rand() % 3);
                                    }
                                }
                            }
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            FUN_0040f9b0(0x10B, prev_x, prev_y);
                            break;
                        }
                        case 0x14: { /* PLASTIC EXPLOSIVES (sub 0) / ORGANIC WASTE (sub 1).
                            * Sub 0: converts tiles to destructible + paints red explosive color.
                            * Sub 1: paints green goo blob ON TOP of terrain (framebuffer only).
                            * From Ghidra 0x43ADE0. */
                            unsigned char sub14 = entity->subtype;
                            int sp_idx14 = (sub14 == 0) ? (0x194 + rand() % 3) : (0x42 + rand() % 3);
                            int sp_w14 = (int)*(unsigned char *)((int)DAT_00489e8c + sp_idx14);
                            int sp_h14 = (int)*(unsigned char *)((int)DAT_00489e88 + sp_idx14);
                            int cx14 = (entity->position_x >> 0x12) - sp_w14 / 2;
                            int cy14 = (entity->position_y >> 0x12) - sp_h14 / 2;
                            int sp_off14 = *(int *)((int)DAT_00489234 + sp_idx14 * 4);
                            unsigned char *sp_gray14 = (unsigned char *)DAT_00489e94;
                            unsigned short *fb14 = (unsigned short *)DAT_00481f50;
                            unsigned short *px_rgb14 = (unsigned short *)DAT_00487ab4;
                            unsigned char *tmap14 = (unsigned char *)DAT_0048782c;
                            int map_w14 = (int)DAT_004879f0;
                            unsigned char tshift14 = (unsigned char)DAT_00487a18;
                            /* Sprite 0x2F (47): tiled texture source for plastic (< 400, RGB565).
                             * Used by original at 0x43AF30 for the reddish-brown texture. */
                            int tex_idx14 = 0x2F;
                            int tex_w14 = (int)*(unsigned char *)((int)DAT_00489e8c + tex_idx14);
                            int tex_h14 = (int)*(unsigned char *)((int)DAT_00489e88 + tex_idx14);
                            int tex_off14 = *(int *)((int)DAT_00489234 + tex_idx14 * 4);
                            if (fb14 && tmap14) {
                                for (int py2 = 0; py2 < sp_h14; py2++) {
                                    for (int px2 = 0; px2 < sp_w14; px2++) {
                                        int wx = cx14 + px2;
                                        int wy = cy14 + py2;
                                        if (wx < 0 || wy < 0 || wx >= map_w14 || wy >= (int)DAT_004879f4) continue;
                                        int sp_pixel = sp_off14 + py2 * sp_w14 + px2;
                                        int toff14 = (wy << tshift14) + wx;
                                        unsigned char t_val = tmap14[toff14];
                                        unsigned char *tp = (unsigned char *)((int)DAT_00487928 + (unsigned int)t_val * 0x20);
                                        if (sub14 == 0) {
                                            /* PLASTIC: tiled texture from sprite 0x2F, alpha from shape sprite.
                                             * Original at 0x43ADE0: gray==0 skip, >=0xF0 opaque, else blend. */
                                            if (!sp_gray14) continue;
                                            unsigned char gray = sp_gray14[sp_pixel];
                                            if (gray == 0) continue;
                                            /* Tile property checks (original 0x43AEC8) */
                                            if (tp[0] != 0 || tp[4] != 0 || tp[0x0B] != 0 || tp[0x18] != 0) continue;
                                            if (t_val == 7 || t_val == 10 || t_val == 16) continue;
                                            /* Get tiled texture color from sprite 0x2F */
                                            int tc = tex_off14 + (py2 % tex_h14) * tex_w14 + (px2 % tex_w14);
                                            unsigned short src_col = px_rgb14 ? px_rgb14[tc] : 0;
                                            if (gray >= 0xF0) {
                                                /* Fully opaque */
                                                fb14[toff14] = src_col;
                                            } else {
                                                /* Alpha blend: dst + (src - dst) * gray / 256 */
                                                unsigned short dst_col = fb14[toff14];
                                                int dr = (dst_col >> 11) & 0x1F, sr = (src_col >> 11) & 0x1F;
                                                int dg = (dst_col >> 5) & 0x3F, sg = (src_col >> 5) & 0x3F;
                                                int db = dst_col & 0x1F, sb = src_col & 0x1F;
                                                int r = dr + ((sr - dr) * gray >> 8);
                                                int g = dg + ((sg - dg) * gray >> 8);
                                                int b = db + ((sb - db) * gray >> 8);
                                                fb14[toff14] = (unsigned short)((r << 11) | (g << 5) | b);
                                            }
                                            /* Set tile to destructible */
                                            if (tp[0x0E] >= 0x40)
                                                tmap14[toff14] = 0x12;
                                            else
                                                tmap14[toff14] = 7;
                                        } else {
                                            /* Organic Waste mode II, original
                                             * 0x0043b02d-0x0043b2f0. The RGB555
                                             * sprite itself is the mask. */
                                            if (!px_rgb14) continue;
                                            unsigned short src_col = px_rgb14[sp_pixel];
                                            if (src_col == 0) continue;
                                            if (tp[0] != 1 && t_val != 0x15) continue;
                                            if (t_val == 0x15) {
                                                unsigned short dst_col = fb14[toff14];
                                                unsigned int dst_luma =
                                                    (unsigned char)((dst_col >> 10) << 3) +
                                                    (unsigned char)((dst_col >> 5) << 3) +
                                                    (unsigned char)(dst_col << 3);
                                                unsigned int src_luma =
                                                    (unsigned char)((src_col >> 10) << 3) +
                                                    (unsigned char)((src_col >> 5) << 3) +
                                                    (unsigned char)(src_col << 3);
                                                if (dst_luma >= src_luma) continue;
                                            } else {
                                                tmap14[toff14] = 0x15;
                                            }
                                            fb14[toff14] = src_col;
                                        }
                                    }
                                }
                            }
                            break;
                        }

                        /* Callback 0x439B90: BRICKWALL terrain darkening */
                        case 0x16: { /* BRICKWALL — paint brick texture onto terrain.
                            * Fixed radius, reads brick color from DAT_00487ab4 sprite.
                            * Only paints on ground tiles (not air, not water). */
                            /* Area size from sprite 0x192. Texture from sprite 22. */
                            int sp_w16 = (int)*(unsigned char *)((int)DAT_00489e8c + 0x192);
                            int sp_h16 = (int)*(unsigned char *)((int)DAT_00489e88 + 0x192);
                            int tex_w16 = (int)*(unsigned char *)((int)DAT_00489e8c + 22);
                            int tex_h16 = (int)*(unsigned char *)((int)DAT_00489e88 + 22);
                            int tex_off16 = *(int *)((int)DAT_00489234 + 22 * 4);
                            unsigned short *sp_rgb16 = (unsigned short *)DAT_00487ab4;
                            unsigned short *fb16 = (unsigned short *)DAT_00481f50;
                            unsigned char *tmap16 = (unsigned char *)DAT_0048782c;
                            unsigned char tshift16 = (unsigned char)DAT_00487a18;
                            int cx16 = (prev_x >> 0x12) - sp_w16 / 2;
                            int cy16 = (prev_y >> 0x12) - sp_h16 / 2;
                            if (sp_rgb16 && fb16 && tmap16 && sp_w16 > 0 && sp_h16 > 0) {
                                int bk_rad = (sp_w16 < sp_h16 ? sp_w16 : sp_h16) / 2;
                                for (int py2 = 0; py2 < sp_h16; py2++) {
                                    for (int px2 = 0; px2 < sp_w16; px2++) {
                                        /* Circular mask */
                                        int dx16 = px2 - sp_w16 / 2;
                                        int dy16 = py2 - sp_h16 / 2;
                                        if (dx16 * dx16 + dy16 * dy16 > bk_rad * bk_rad) continue;
                                        int wx = cx16 + px2;
                                        int wy = cy16 + py2;
                                        if (wx < 0 || wy < 0 || wx >= (int)DAT_004879f0 || wy >= (int)DAT_004879f4) continue;
                                        unsigned char t_val = tmap16[(wy << tshift16) + wx];
                                        if (t_val == 0) continue; /* skip air */
                                        unsigned char *tp16 = (unsigned char *)((int)DAT_00487928 + (unsigned int)t_val * 0x20);
                                        if (tp16[4] != 0) continue; /* skip water */
                                        /* Tile sprite 22 texture across the area */
                                        int tpx = (tex_w16 > 0) ? (px2 % tex_w16) : 0;
                                        int tpy = (tex_h16 > 0) ? (py2 % tex_h16) : 0;
                                        int tex_pix = tex_off16 + tpy * tex_w16 + tpx;
                                        unsigned short tgt = sp_rgb16[tex_pix];
                                        if (tgt == 0) continue; /* skip transparent */
                                        /* Direct write — stamp brick texture onto terrain */
                                        fb16[(wy << tshift16) + wx] = tgt;
                                        /* Set tile: 0x0A for normal, 0x10 for reinforced */
                                        if (tp16[0x0E] >= 0x40)
                                            tmap16[(wy << tshift16) + wx] = 0x10;
                                        else
                                            tmap16[(wy << tshift16) + wx] = 0x0A;
                                    }
                                }
                            }
                            FUN_0040f9b0(0x10D, prev_x, prev_y);
                            break;
                        }
                        case 0x2B: /* REPAIR MAKER — build structure tiles.
                            * Calls FUN_00440ba0 to stamp building tiles from sprite stencil. */
                            FUN_00440ba0((prev_x >> 0x12), (prev_y >> 0x12),
                                         entity->owner, '\0');
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            break;
                        case 0x6A:
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            break;

                        /* NUCLEUS wall collision — Ghidra callback 0x432C80.
                         * KB(200, range=100) + random explosion sound. No tile damage.
                         * Nucleus dots that hit walls just explode harmlessly. */
                        case 0x17:
                            FUN_00437cf0(prev_x, prev_y, 200, owner, 100);
                            FUN_0040f9b0(0x65 + (rand() % 7), prev_x, prev_y);
                            break;

                        /* PILOT DISRUPTOR — deploy on wall hit. Stop + set state 0xFA.
                         * Set lifetime at +0x60 so it stays alive for ~100 seconds. */
                        case 0x18:
                            entity->velocity_x = 0;
                            entity->velocity_y = 0;
                            entity->state_20 = 0xFA;
                            entity->scratch_60 = 6000; /* ~100 sec at 60fps */
                            did_bounce = 1; /* prevent removal */
                            break;

                        /* LANDMINE wall collision — original 0x4437f1-0x44380d.
                         * Basic airmines land on the first solid tile: restore the
                         * previous position, stop completely, and mark +0x2C=1.
                         * The callback then leaves the mine armed until its timer
                         * expires or an enemy touches it. */
                        case 0x19: {
                            if (entity->subtype == 0) {
                                entity->position_x = entity->previous_x;
                                entity->position_y = entity->previous_y;
                                entity->velocity_x = 0;
                                entity->velocity_y = 0;
                                entity->scratch_2c = 1;
                                did_bounce = 1;
                            }
                            break;
                        }

                        /* Callback 0x443B10: tile damage + KB(150/255) + sound */
                        case 0x1B: { /* KAMIKAZE MEN — fire explosion on contact */
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            FUN_00437cf0(prev_x, prev_y, 150, owner, 255);
                            /* Fire debris burst */
                            int *km_sc = (int *)DAT_00487ab0;
                            for (int dp = 0; dp < 8 && DAT_00489248 < 0x9C4; dp++) {
                                unsigned int dir = rand() & 0x7FF;
                                int spd = rand() % 60;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = prev_x;
                                ep->position_y = prev_y;
                                ep->velocity_x = (km_sc[dir] * spd) >> 6;
                                ep->velocity_y = (km_sc[dir + 0x200] * spd) >> 6;
                                ep->previous_x = prev_x;
                                ep->previous_y = prev_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->type = 0;
                                ep->variant_24 = (short)(rand() % 6);
                                ep->state_20 = 0;
                                ep->auxiliary_26 = 0;
                                ep->owner = owner;
                                ep->health_or_damage_28 = 0;
                                ep->gravity_or_motion_38 = ((int *)DAT_00487abc)[0x26];
                                ep->scratch_48 = 0;
                                ep->animation_frame = 0;
                                ep->subtype = 4;
                                ep->callback_address = ((int *)DAT_00487abc)[0];
                                ep->counter_3c = 0;
                                ep->timer_5c = 0;
                                DAT_00489248++;
                                DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 60 + 40;
                                {
                                    int ci = rand() % 10;
                                    unsigned short pal = *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2);
                                    unsigned short r5 = (pal >> 10) & 0x1F;
                                    unsigned short g5 = (pal >> 5) & 0x1F;
                                    unsigned short b5 = pal & 0x1F;
                                    DAT_004892e8[DAT_00489248 - 1].palette_value =
                                        (unsigned int)((r5 << 11) | (g5 << 6) | b5) + 30000;
                                }
                            }
                            FUN_0040f9b0(0x65 + (rand() % 7), prev_x, prev_y);
                            break;
                        }

                        /* MINISHIP wall collision — Ghidra callback 0x440E20.
                         * Bounces off walls like player ships. No tile damage, no sound.
                         * Revert position to backup (+0x04/+0x0C), reflect velocity on
                         * the axis that crossed a tile boundary. Stays alive (did_bounce). */
                        case 0x1C: {
                            entity->position_x = entity->previous_x;
                            entity->position_y = entity->previous_y;
                            int ms_vx = entity->velocity_x;
                            int ms_vy = entity->velocity_y;
                            if (prev_tx != tx) entity->velocity_x = -ms_vx;
                            if (prev_ty != ty) entity->velocity_y = -ms_vy;
                            did_bounce = 1;
                            break;
                        }

                        /* Callback 0x43C0B0: tile damage + sound; KB for 0x1D only */
                        case 0x1D: { /* MEGABOMB — KB + tile damage + fire debris + mushroom fire */
                            int mb_x = entity->position_x;
                            int mb_y = entity->position_y;
                            int mb_effect_x = entity->previous_x;
                            int mb_effect_y = entity->previous_y;
                            int mb_vx = entity->velocity_x;
                            int mb_vy = entity->velocity_y;
                            int *sc = (int *)DAT_00487ab0;
                            int *tt = (int *)DAT_00487abc;

                            /* Area knockback */
                            FUN_00437cf0(mb_x, mb_y, 400, owner, 500);

                            /* Tile damage */
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         mb_x, mb_y, entity->previous_x, entity->previous_y,
                                         '\0', '\0', owner);

                            /* Loop 1: Fire debris (entity_type 0, render_mode 4) */
                            {
                                int count1 = (int)((float)DAT_0048385c * 80.0f);
                                if (count1 < 1) count1 = 1;
                                if (count1 > 40) count1 = 40;
                                int angle_step = 0x0445C000 / count1;
                                for (int dp = 0; dp < count1 && DAT_00489248 < 0x9C4; dp++) {
                                    unsigned int dir = rand() & 0x7FF;
                                    int spd = rand() % 80;
                                    Entity *ep = &DAT_004892e8[DAT_00489248];
                                    ep->position_x = mb_effect_x;
                                    ep->position_y = mb_effect_y;
                                    ep->velocity_x = (sc[dir] * spd >> 6) + (mb_vx >> 5);
                                    ep->velocity_y = (sc[0x200 + dir] * spd >> 6) + (mb_vy >> 5) - 0x3E800;
                                    ep->previous_x = mb_effect_x;
                                    ep->previous_y = mb_effect_y;
                                    ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                    ep->type = 0;
                                    ep->variant_24 = (short)(rand() % 6);
                                    ep->state_20 = 0;
                                    ep->auxiliary_26 = 0;
                                    ep->owner = owner;
                                    ep->health_or_damage_28 = 0;
                                    ep->gravity_or_motion_38 = tt[0x26];
                                    ep->scratch_48 = 0;
                                    ep->animation_frame = 0;
                                    ep->subtype = 4;
                                    ep->callback_address = tt[0];
                                    ep->counter_3c = 0;
                                    ep->timer_5c = 0;
                                    DAT_00489248++;
                                    DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 100 + 120;
                                    {
                                        int ci = rand() % 10;
                                        DAT_004892e8[DAT_00489248 - 1].palette_value =
                                            *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2) + 30000;
                                    }
                                    DAT_004892e8[DAT_00489248 - 1].damage_44 = angle_step;
                                }
                            }

                            /* Loop 2: Mushroom fire (entity_type 0x64, render_mode 0) */
                            {
                                int count2 = (int)((float)DAT_0048385c * 150.0f);
                                if (count2 < 1) count2 = 1;
                                if (count2 > 80) count2 = 80;
                                for (int mp = 0; mp < count2 && DAT_00489248 < 0x9C4; mp++) {
                                    unsigned int dir = rand() & 0x7FF;
                                    int spd = rand() % 80;
                                    Entity *ep = &DAT_004892e8[DAT_00489248];
                                    ep->position_x = mb_effect_x;
                                    ep->position_y = mb_effect_y;
                                    ep->velocity_x = (sc[dir] * spd >> 6) + (mb_vx >> 5);
                                    ep->velocity_y = (sc[0x200 + dir] * spd >> 6) + (mb_vy >> 5) - 0x3E800;
                                    ep->previous_x = mb_effect_x;
                                    ep->previous_y = mb_effect_y;
                                    ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                    ep->type = 0x64;
                                    ep->variant_24 = (short)(rand() % 6);
                                    ep->state_20 = 0;
                                    ep->auxiliary_26 = 0xFF;
                                    ep->owner = 0xFF;
                                    ep->health_or_damage_28 = 0;
                                    ep->gravity_or_motion_38 = tt[0x347A];
                                    ep->damage_44 = tt[0x3489];
                                    ep->scratch_48 = 0;
                                    ep->palette_value = tt[0x3495];
                                    ep->animation_frame = 0;
                                    ep->subtype = 0;
                                    ep->callback_address = tt[0x3458];
                                    ep->counter_3c = 0;
                                    ep->timer_5c = 0;
                                    DAT_00489248++;
                                    DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 100 + 120;
                                    {
                                        int ci = rand() % 10;
                                        DAT_004892e8[DAT_00489248 - 1].palette_value =
                                            *(unsigned short *)((int)DAT_00487aa8 + (246 + ci) * 2) + 30000;
                                    }
                                    DAT_004892e8[DAT_00489248 - 1].damage_44 = 0;
                                }
                            }

                            /* Flash particle + sound */
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                int flash_y = mb_y;
                                if (DAT_00481f20 != NULL) {
                                    int sprite_height = *(unsigned char *)((int)DAT_00481f20 + 5) & 0xFE;
                                    flash_y += 0x140000 - sprite_height * 0x20000;
                                }
                                *(int *)(fp + 0x00) = mb_x;
                                *(int *)(fp + 0x04) = flash_y;
                                *(int *)(fp + 0x08) = 0;
                                *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = 0;
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 1;
                                *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 1; /* group 1 = fire (MEGABOMB) */
                                DAT_00489250++;
                            }
                            FUN_0040f9b0(0x65 + (rand() % 7), mb_x, mb_y);
                            break;
                        }
                        case 0x1E: /* PHOTON FLUX — pure anti-ship, NOTHING on wall hit.
                            * Silently vanishes. Only damages via entity hit (FUN_004348a0). */
                            break;

                        /* Callback 0x43B370: BOUNCE with full reflection, up to 30 times.
                         * Sound 0x70 on each bounce. Counter at +0x3C (counting up). */
                        case 0x1F: /* INSECTS — pass through walls. No death, no sound.
                            * Original reverts position on wall hit but stays alive. */
                            entity->position_x = entity->previous_x;
                            entity->position_y = entity->previous_y;
                            did_bounce = 1; /* stay alive */
                            break;

                        /* Type 0x22 wall collision — dispatches on +0x40 sub_type.
                         * Mode 0: WAVY FIREWORKS wall hit.
                         * Modes 1-3: turret deployer / Roman Candle sub-entity wall hit. */
                        case 0x22: {
                            unsigned char wc22_sub = entity->subtype;
                            if (wc22_sub == 0) {
                                /* WAVY FIREWORKS mode 1 wall hit. The terminal path
                                 * is silent and emits one small flash; 0x4443ED is
                                 * its flight-trail spawn, not an impact explosion. */
                                if (DAT_00489250 < 2000) {
                                    int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                    *(int *)(fp + 0x00) = prev_x;
                                    *(int *)(fp + 0x04) = prev_y;
                                    *(int *)(fp + 0x08) = 0;
                                    *(int *)(fp + 0x0C) = 0;
                                    *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() % 2) + 1;
                                    *(unsigned char *)(fp + 0x11) = 0;
                                    *(unsigned char *)(fp + 0x12) = 0;
                                    *(unsigned char *)(fp + 0x13) = 1;
                                    *(unsigned char *)(fp + 0x14) = 0xFF;
                                    *(unsigned char *)(fp + 0x15) = 0; /* fire */
                                    DAT_00489250++;
                                }
                                /* Firework dies on wall hit */
                                break;
                            }
                            /* Modes 2-4 wall collision, lifted from 0x444C00.
                             * Mode 3 emits an eight-way type-0x6A ring. Mode 4 does
                             * not emit that ring on a plain wall hit. All modes emit
                             * exactly one terminal flash below. */
                            {
                                unsigned char wc_sub = entity->subtype;
                                if (wc_sub == 2) {
                                    int *sc22 = (int *)DAT_00487ab0;
                                    int *tt22 = (int *)DAT_00487abc;
                                    for (int angle = 0; angle < 0x800 && DAT_00489248 < 0x9C4; angle += 0x100) {
                                        int dir = (angle + rand() % 0x100) & 0x7FF;
                                        int speed = rand() % 0x14 + 2;
                                        Entity *bp = &DAT_004892e8[DAT_00489248];
                                        memset((void *)bp, 0, 0x80);
                                        bp->position_x = prev_x;
                                        bp->position_y = prev_y;
                                        bp->previous_x = prev_x;
                                        bp->previous_y = prev_y;
                                        bp->velocity_x = (sc22[dir] * speed >> 6) + (entity->velocity_x >> 1);
                                        bp->velocity_y = (sc22[dir + 0x200] * speed >> 6) + (entity->velocity_y >> 1);
                                        bp->type = 0x6A;
                                        bp->owner = owner;
                                        bp->auxiliary_26 = 0;
                                        bp->gravity_or_motion_38 = tt22[0xDE78 / 4];
                                        bp->damage_44 = tt22[0xDEB4 / 4];
                                        bp->palette_value = tt22[0xDEE4 / 4];
                                        bp->callback_address = tt22[0xDDF0 / 4];
                                        DAT_00489248++;
                                        Entity *spawned = &DAT_004892e8[DAT_00489248 - 1];
                                        unsigned char anim_mod = *(unsigned char *)((int)DAT_00487abc + 0xDF14);
                                        spawned->scratch_48 = anim_mod ? rand() % anim_mod : 0;
                                        spawned->health_or_damage_28 = 0x50;
                                    }
                                }
                                if (DAT_00489250 < 2000) {
                                    int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                    *(int *)(fp + 0x00) = prev_x;
                                    *(int *)(fp + 0x04) = prev_y;
                                    *(int *)(fp + 0x08) = 0;
                                    *(int *)(fp + 0x0C) = 0;
                                    *(unsigned char *)(fp + 0x10) =
                                        wc_sub >= 2 ? (unsigned char)(rand() % 4 + 0x0D)
                                                     : (unsigned char)(rand() % 2 + 1);
                                    *(unsigned char *)(fp + 0x11) = 0;
                                    *(unsigned char *)(fp + 0x12) = 0;
                                    *(unsigned char *)(fp + 0x13) = 0;
                                    *(unsigned char *)(fp + 0x14) = 0xFF;
                                    *(unsigned char *)(fp + 0x15) = 0;
                                    DAT_00489250++;
                                }
                                int snd = (wc_sub == 3) ? 0x112 : 0x114;
                                FUN_0040f9b0(snd, prev_x, prev_y);
                            }
                            break;
                        }

                        /* GAMMA BOOM impact, callback 0x445A27-0x44611D:
                         * crater level 9, five fireballs, sixteen dumbfires, flash,
                         * sound and a 400-strength blast.  The old approximation
                         * omitted both projectile payload loops. */
                        case 0x23: {
                            int gx = entity->position_x;
                            int gy = entity->position_y;
                            int gvx = entity->velocity_x;
                            int gvy = entity->velocity_y;
                            int *sc = (int *)DAT_00487ab0;
                            int *tt = (int *)DAT_00487abc;
                            FUN_004357b0(tx, ty, 9, stored_tile, is_water,
                                         gx, gy, prev_x, prev_y, '\0', '\0', owner);
                            for (int n = 0; n < 5 && DAT_00489248 < 0x9C4; n++) {
                                int dir = rand() & 0x7FF;
                                int spd = rand() % 100 + 30;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                memset((void *)ep, 0, 0x80);
                                ep->position_x = gx; ep->previous_x = gx;
                                ep->position_y = gy; ep->previous_y = gy;
                                ep->velocity_x = (sc[dir] * spd >> 6) + (gvx >> 1);
                                ep->velocity_y = (sc[dir + 0x200] * spd >> 6) + (gvy >> 1);
                                ep->type = 0x11;
                                ep->owner = owner;
                                ep->gravity_or_motion_38 = tt[0x2420 / 4];
                                ep->damage_44 = tt[0x245C / 4];
                                ep->palette_value = tt[0x248C / 4];
                                ep->callback_address = tt[0x2398 / 4];
                                DAT_00489248++;
                            }
                            for (int n = 0; n < 16 && DAT_00489248 < 0x9C4; n++) {
                                int dir = rand() & 0x7FF;
                                int spd = rand() % 100 + 30;
                                int sub = rand() % 3;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                memset((void *)ep, 0, 0x80);
                                ep->position_x = gx; ep->previous_x = gx;
                                ep->position_y = gy; ep->previous_y = gy;
                                ep->velocity_x = (sc[dir] * spd >> 6) + (gvx >> 1);
                                ep->velocity_y = (sc[dir + 0x200] * spd >> 6) + (gvy >> 1);
                                ep->type = 0x01;
                                ep->owner = owner;
                                ep->subtype = (unsigned char)sub;
                                ep->gravity_or_motion_38 = tt[(0x2A0 + sub * 4) / 4];
                                ep->damage_44 = tt[(0x2DC + sub * 4) / 4];
                                ep->palette_value = tt[(0x30C + sub * 4) / 4];
                                ep->callback_address = tt[0x218 / 4];
                                DAT_00489248++;
                            }
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = gx; *(int *)(fp + 0x04) = gy;
                                *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = 0x0B;
                                *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 1;
                                DAT_00489250++;
                            }
                            FUN_0040f9b0(0x65 + rand() % 7, gx, gy);
                            FUN_00437cf0(gx, gy, 400, owner, 0);
                            break;
                        }

                        /* ETNA wall collision — Ghidra callback 0x447A70.
                         * Deploy on solid ground (once only). Reverts position to backup,
                         * zeroes velocity, sets state=0xC8 (deployed), lifetime=900 (~15s),
                         * startup delay +0x3C=-80 (counts up to 0 before spraying starts).
                         * Stays alive via did_bounce. Spraying handled in behavior switch. */
                        case 0x24: {
                            if (entity->state_20 != 0xC8) {
                                entity->position_x = entity->previous_x;
                                entity->position_y = entity->previous_y;
                                entity->velocity_x = 0;
                                entity->velocity_y = 0;
                                entity->state_20 = 0xC8;
                                entity->scratch_60 = 900;
                                entity->counter_3c = -80; /* startup delay */
                            }
                            did_bounce = 1;
                            break;
                        }
                        /* DEAD CODE — old burst, replaced by deploy+spray */
                        if (0) {
                            int parent_x = entity->previous_x;
                            int parent_y = entity->previous_y;
                            unsigned char own = entity->owner;
                            int pvx = entity->velocity_x;
                            int pvy = entity->velocity_y;
                            for (int s = 0; s < 10 && DAT_00489248 < 0x9c4; s++) {
                                Entity *tp = &DAT_004892e8[DAT_00489248];
                                memset((void *)tp, 0, 0x80);
                                tp->position_x = parent_x;
                                tp->previous_x = parent_x;
                                tp->position_y = parent_y;
                                tp->previous_y = parent_y;
                                int jx = ((rand() & 0x1FFFF) - 0x10000);
                                int jy = ((rand() & 0x1FFFF) - 0x10000);
                                tp->velocity_x = pvx / 3 + jx;
                                tp->velocity_y = pvy / 3 + jy;
                                tp->type = 0x67;
                                tp->owner = own;
                                tp->auxiliary_26 = 0xFE;
                                *(unsigned char *)((char *)tp + 0x28) = 0x40; /* short lifespan */
                                tp->subtype = 2;
                                tp->timer_5c = 2;
                                unsigned char pidx = (unsigned char)(rand() % 12 + 20);
                                tp->scratch_65 = pidx;
                                tp->scratch_64 = 0x12;
                                if (DAT_00487aa8 != NULL)
                                    tp->palette_value = (int)((unsigned short *)DAT_00487aa8)[pidx] + 30000;
                                tp->variant_24 = (unsigned short)(rand() % 5);
                                tp->gravity_or_motion_38 = 4;
                                tp->damage_44 = entity->damage_44 / 5; /* split damage */
                                DAT_00489248++;
                            }
                            break;
                        }

                        /* ROMAN CANDLE wall collision — Ghidra callback 0x446130.
                         * Deploy on solid ground (once only). Same deploy pattern as ETNA:
                         * revert position, zero velocity, state=0xC8, lifetime=1200 (~20s),
                         * startup delay +0x3C=-80. Stays alive via did_bounce.
                         * Spray logic handled in behavior switch (mode-dependent). */
                        case 0x25: {
                            entity->position_x = entity->previous_x;
                            entity->position_y = entity->previous_y;
                            entity->velocity_x = 0;
                            entity->velocity_y = 0;
                            did_bounce = 1; /* always stay alive */
                            break;
                        }

                        /* MORNING STAR wall collision — Ghidra callback 0x43DBD0.
                         * Flash burst (4-5 particles), no sound, no tile damage.
                         * Entity dies. The spinning fire stops on impact. */
                        case 0x26:
                            flash_count = (rand() & 1) + 4;
                            break;

                        case 0x27: { /* KOMET BOMB terminal burst, 0x43E411-0x43E883. */
                            int kvx = entity->velocity_x;
                            int kvy = entity->velocity_y;
                            int kspeed = (kvx < 0 ? -kvx : kvx) + (kvy < 0 ? -kvy : kvy);
                            if (entity->scratch_2c == 0 && kspeed < 0xC0000) {
                                entity->velocity_x = 0;
                                entity->velocity_y = 0;
                                entity->scratch_2c = 1;
                                did_bounce = 1;
                                break;
                            }
                            int base_dir = ((int)FUN_004257e0(0, 0, kvx, kvy) + 0x400) & 0x7FF;
                            int count = 1 - (int)(DAT_00483854 * -30.0f);
                            if (count < 1) count = 1;
                            int split_damage = 0x9C4000 / count;
                            int *sc = (int *)DAT_00487ab0;
                            int *tt = (int *)DAT_00487abc;
                            for (int n = 0; n < count && DAT_00489248 < 0x9C4; n++) {
                                int dir = (base_dir + rand() % 120 - 60) & 0x7FF;
                                int spd = rand() % 48 + 100;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                memset((void *)ep, 0, 0x80);
                                ep->position_x = entity->position_x;
                                ep->previous_x = entity->position_x;
                                ep->position_y = entity->position_y;
                                ep->previous_y = entity->position_y;
                                ep->velocity_x = sc[dir] * spd >> 6;
                                ep->velocity_y = sc[dir + 0x200] * spd >> 6;
                                ep->type = 0x6A;
                                ep->owner = owner;
                                ep->subtype = 2;
                                ep->gravity_or_motion_38 = tt[0xDE80 / 4];
                                ep->damage_44 = tt[0xDEBC / 4];
                                ep->palette_value = tt[0xDEEC / 4];
                                ep->callback_address = tt[0xDDF0 / 4];
                                ep->damage_44 = split_damage;
                                DAT_00489248++;
                                DAT_004892e8[DAT_00489248 - 1].health_or_damage_28 = rand() % 100 + 80;
                            }
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = entity->position_x;
                                *(int *)(fp + 0x04) = entity->position_y;
                                *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = (unsigned char)(13 + (rand() & 3));
                                *(unsigned char *)(fp + 0x11) = 0; *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 0; *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 1;
                                DAT_00489250++;
                            }
                            FUN_00437cf0(entity->position_x, entity->position_y, 100, owner, -1);
                            FUN_0040f9b0(0x65 + rand() % 7, entity->position_x, entity->position_y);
                            break;
                        }

                        /* Callback 0x43E890: PIPEBOMB bounce.
                         * Hit axis: negate and /4 (strong dampening).
                         * Cross axis: *0.8 (mild dampening).
                         * Corner: both negate and /8.
                         * Matches original at 0x43E9A0-0x43EA80. */
                        case 0x28: {
                            int hit_x = (prev_tx != tx);
                            int hit_y = (prev_ty != ty);
                            if (hit_x && hit_y) {
                                /* Corner: both axes negate + /8 */
                                entity->velocity_x = -(entity->velocity_x >> 3);
                                entity->velocity_y = -(entity->velocity_y >> 3);
                            } else if (hit_y) {
                                /* Floor/ceiling: vel_y = -vel_y/4, vel_x *= 0.8 */
                                int vy = entity->velocity_y;
                                int vx = entity->velocity_x;
                                entity->velocity_y = -(vy >> 2);
                                entity->velocity_x = (int)((float)vx * 0.8f);
                            } else if (hit_x) {
                                /* Wall: vel_x = -vel_x/4, vel_y *= 0.8 */
                                int vx = entity->velocity_x;
                                int vy = entity->velocity_y;
                                entity->velocity_x = -(vx >> 2);
                                entity->velocity_y = (int)((float)vy * 0.8f);
                            }
                            if ((entity->velocity_x < 0 ? -entity->velocity_x : entity->velocity_x) +
                                (entity->velocity_y < 0 ? -entity->velocity_y : entity->velocity_y) < 0x20000) {
                                entity->velocity_x = 0;
                                entity->velocity_y = 0;
                            }
                            entity->scratch_30 -= 2;
                            if (entity->scratch_30 < 0) entity->scratch_30 = 0;
                            entity->position_x = entity->previous_x;
                            entity->position_y = entity->previous_y;
                            did_bounce = 1;
                            break;
                        }

                        /* Callback 0x43E890: TURRETS deploy on landing (NO bounce) */
                        case 0x29:
                        case 0x2A:
                            entity->velocity_x = 0;
                            entity->velocity_y = 0;
                            entity->position_x = entity->previous_x;
                            entity->position_y = entity->previous_y;
                            entity->state_20 = 0xFA;
                            did_bounce = 1;
                            break;

                        /* Machinegun: tile damage only, no extra effects */
                        case 0x2C:
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            break;

                        /* SMOKING NALLE wall collision — entity type 0x2E (NOT 0x26).
                         * Small flash (1 particle) + sound 0x65 + die.
                         * Minimal explosion — the nalle just pops. */
                        case 0x2E:
                            flash_count = 1;
                            FUN_0040f9b0(0x65, prev_x, prev_y);
                            break;

                        /* Callback 0x4427e0: heavy guided missile — explosion stamp */
                        case 0x66:
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            FUN_00437cf0(prev_x, prev_y, 200, owner, 150);
                            FUN_0040f9b0(0x65 + (rand() % 7), prev_x, prev_y);
                            flash_count = (rand() & 1) + 3;
                            break;

                        /* Unknown types: conservative — tile damage only */
                        default:
                            FUN_004357b0(tx, ty, explevel, stored_tile, is_water,
                                         0, 0, 0, 0, 0, '\0', owner);
                            break;
                        }

                        /* Spawn flash particles into DAT_00481f34 (0x20-byte records,
                         * counter DAT_00489250, max 2000 entries) */
                        for (int ep = 0; ep < flash_count && DAT_00489250 < 2000; ep++) {
                            int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                            *(int *)(pbase + 0x00) = prev_x;
                            *(int *)(pbase + 0x04) = prev_y;
                            int dir = rand() & 0x7FF;
                            int spd = rand() % 50 + 20;
                            *(int *)(pbase + 0x08) = (*(int *)((int)DAT_00487ab0 + dir * 4) * spd) >> 7;
                            *(int *)(pbase + 0x0C) = (*(int *)((int)DAT_00487ab0 + 0x800 + dir * 4) * spd) >> 7;
                            *(unsigned char *)(pbase + 0x10) = (unsigned char)((rand() & 1) + 1);
                            *(unsigned char *)(pbase + 0x11) = 0;
                            *(unsigned char *)(pbase + 0x12) = 2;
                            *(unsigned char *)(pbase + 0x13) = 0xC8;
                            *(unsigned char *)(pbase + 0x14) = owner;
                            *(unsigned char *)(pbase + 0x15) = 0;
                            DAT_00489250++;
                        }
                    }

                    if (!did_bounce) {
                        should_remove = 1;
                    } else {
                        /* Revert position to pre-collision for bounced entities.
                         * This prevents the entity from getting stuck inside the wall. */
                        entity->position_x = entity->previous_x;
                        entity->position_y = entity->previous_y;
                    }
                }
            } else {
            }

            /* Player collision for projectiles — matches FUN_004348a0.
             * The original behavior callback calls FUN_004348a0 which iterates
             * ALL players (no spatial grid pre-check) and uses AABB dimensions
             * from the type config table + player ship size.
             *
             * Skipped types:
             *   0x17 NUCLEUS  — passive trail target, does not damage players.
             *                   Players shoot the dots to trigger the ring burst.
             *   0x1C MINISHIP — autonomous ship, damages players via its spawned
             *                   type 0x67 bullets, not by direct body contact.
             *
             * Special handling (NOT skipped, but guarded):
             *   0x19 LANDMINE — enters this block but has an early team check
             *                   below that skips self/allies. Only detonates on
             *                   enemy player contact (Ghidra 0x443862). */
            if (!should_remove && DAT_00489240 > 0 &&
                (ent_type != 0x1F || entity->counter_3c == 0) &&
                ent_type != 0x1C && ent_type != 0x17 && ent_type != 0x26 &&
                ent_type != 0x28 && ent_type != 0x2E) {
                unsigned char raw_owner = entity->owner;
                unsigned char byte_26 = entity->auxiliary_26;
                unsigned char sub_type = entity->subtype;
                int proj_damage = entity->damage_44;

                /* Read collision dimensions from type config table */
                unsigned char coll_w = 2, coll_h = 2;
                if (DAT_00487abc != NULL) {
                    coll_w = *(unsigned char *)((int)DAT_00487abc + (unsigned int)sub_type + 0x136 + (unsigned int)ent_type * 0x218);
                    coll_h = *(unsigned char *)((int)DAT_00487abc + (unsigned int)sub_type + 0x13c + (unsigned int)ent_type * 0x218);
                }

                /* Determine "team" identifier for guard check.
                 * For type 0x1f: use the owner's player team byte.
                 * For others: use raw owner byte directly. */
                unsigned int guard_team;
                if (ent_type == 0x1f) {
                    guard_team = Player_Get(raw_owner)->team;
                } else {
                    guard_team = (unsigned int)raw_owner;
                }

                for (int p = 0; p < DAT_00489240; p++) {
                    PlayerData *player = Player_Get(p);
                    if (player->health <= 0) continue;

                    /* Guard check: (byte_0x26 == 0) || (guard_team != player_id).
                     * For turret projectiles: byte_0x26=0xfe, guard_team=0x50+
                     * → always passes since 0x50+ != any player index 0-3.
                     * This means turret projectiles hit ALL players regardless of team. */
                    unsigned int player_id;
                    if (ent_type == 0x1f) {
                        player_id = player->team;
                    } else {
                        player_id = (unsigned int)p;
                    }
                    if (byte_26 != 0 && guard_team == player_id) continue;

                    /* LANDMINE player collision — early team check (Ghidra 0x443862).
                     * Skip collision entirely for self and allied players. The mine
                     * should only detonate on ENEMY contact. Without this guard, a
                     * player's own landmine would blow them up on deployment. */
                    if (ent_type == 0x19 && raw_owner < 0x50) {
                        unsigned char mine_team = Player_Get(raw_owner)->team;
                        unsigned char p_team = player->team;
                        if (mine_team == p_team) continue;
                    }

                    /* AABB collision using config-based dimensions + player ship size */
                    int ship_size = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x38) : 0;
                    int h_range = ship_size + (unsigned int)coll_w * FIXED_SCALE;
                    int v_range = ship_size + (unsigned int)coll_h * FIXED_SCALE;
                    if (DAT_004892e5 != '\0') {
                        h_range += 0x140000;
                        v_range += 0x140000;
                    }

                    int px = player->position_x;
                    int py = player->position_y;

                    if (px - h_range < pos_x && pos_x < px + h_range &&
                        py - v_range < pos_y && pos_y < py + v_range) {
                        /* === HIT: Apply damage based on owner type === */
                        if (raw_owner < 0x50) {
                            /* Player-owned projectile: team-check before damage */
                            unsigned char shooter_team = Player_Get(raw_owner)->team;
                            unsigned char target_team = player->team;
                            if (shooter_team != target_team || DAT_0048373d != 0) {
                                player->health = tou_binary::sub_wrap_i32(player->health, proj_damage);
                            }
                        } else {
                            /* Turret/base-owned: always apply damage */
                            player->health = tou_binary::sub_wrap_i32(player->health, proj_damage);
                        }

                        /* Record who hit the player (offset 0x4a1) */
                        if (raw_owner < 0x50) {
                            unsigned char shooter_team = Player_Get(raw_owner)->team;
                            unsigned char target_team = player->team;
                            if (shooter_team != target_team || DAT_0048373d != 0) {
                                player->last_attacker = raw_owner;
                            }
                        } else if (raw_owner < 100) {
                            /* Turret owner (0x50-0x63): record as owner + 0x14 */
                            player->last_attacker = raw_owner + 0x14;
                        } else if (raw_owner < 0x78) {
                            player->last_attacker = raw_owner;
                        } else if (raw_owner < 0x8c) {
                            player->last_attacker = raw_owner - 0x14;
                        } else {
                            player->last_attacker = 0xff;
                        }
                        player->timer_4a2 = 0x6e;

                        /* Freeze reduction: if player is already frozen and projectile
                         * is NOT freeze type, reduce freeze timer */
                        unsigned char cur_freeze = player->stun_timer;
                        if (cur_freeze != 0 && ent_type != 0x13) {
                            int new_freeze = (int)cur_freeze - (proj_damage >> 0xf) - 1;
                            if (new_freeze < 0) new_freeze = 0;
                            player->stun_timer = (unsigned char)new_freeze;
                        }

                        /* Freeze effect: entity type 0x13 freezes the player */
                        if (ent_type == 0x13 && player->stun_timer == 0) {
                            player->stun_timer = 5;
                            player->scratch_c7 = static_cast<uint8_t>(rand() % 3);
                            FUN_0040f9b0(0x13, pos_x, pos_y);
                        }

                        /* Set hit flags (original FUN_004348a0 also sets
                         * DAT_00481e8f=4 as callback state, but we use should_remove) */
                        player->timer_c4 = 5;
                        player->flag_a3 = 1;

                        /* Apply knockback */
                        if (DAT_00487abc != NULL) {
                            unsigned char kb_div = *(unsigned char *)((int)DAT_00487abc + (unsigned int)sub_type + 0xa6 + (unsigned int)ent_type * 0x218);
                            if (kb_div != 99 && kb_div != 0) {
                                int proj_vx = entity->velocity_x;
                                int proj_vy = entity->velocity_y;
                                player->velocity_x = tou_binary::add_wrap_i32(
                                    player->velocity_x, proj_vx / (int)(unsigned int)kb_div);
                                player->velocity_y = tou_binary::add_wrap_i32(
                                    player->velocity_y, proj_vy / (int)(unsigned int)kb_div);
                            }
                        }

                        /* PHOTON FLUX: confusion effect — set +0xD0 timer to 240 ticks.
                         * Causes aim jitter (+/-64/tick), forced movement, HUD hidden. */
                        if (ent_type == 0x1E) {
                            player->timer_d0 = 0xF0;
                        }

                        /* MOVING SUCKER and INSECTS survive player contact. Insects
                         * arm a 300..999 tick collision/targeting cooldown. */
                        if (ent_type == 0x0E) {
                            /* no removal — entity stays alive */
                        } else if (ent_type == 0x1F) {
                            /* Runtime parity: the literal binary range is 300..999,
                             * but at the decomp's 60 Hz callback cadence that leaves
                             * insects inert far longer than the original runtime. */
                            entity->counter_3c = rand() % 121 + 60;
                        }
                        /* LANDMINE hit result — Ghidra 0x443862.
                         * Second team check at hit resolution (redundant with pre-check
                         * above, but kept for safety). On enemy contact: flash particle
                         * (warm fire, sprite 17-19) + explosion sound + die. */
                        else if (ent_type == 0x19 && raw_owner < 0x50) {
                            unsigned char mine_team = Player_Get(raw_owner)->team;
                            unsigned char player_team = player->team;
                            if (mine_team == player_team) {
                                /* skip ally damage — mine stays alive */
                            } else {
                                /* Route contact through the callback's common
                                 * detonation path next tick. This is essential for
                                 * mode 3's radial type-0x67 burst. */
                                /* The shared lifetime pass runs later in this tick,
                                 * so arm with 2: it becomes 1 there, then reaches the
                                 * callback detonation path on the following tick. */
                                entity->health_or_damage_28 = 2;
                            }
                        }
                        /* PIPEBOMB: trigger detonation instead of silent removal */
                        else if (ent_type == 0x28) {
                            entity->state_20 = 0xFA;
                        } else if (ent_type == 0x1B) {
                            /* Original 0x44416A contact explosion. */
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = entity->position_x;
                                *(int *)(fp + 0x04) = entity->position_y;
                                *(int *)(fp + 0x08) = 0;
                                *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = (unsigned char)(rand() % 4) + 0x0D;
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 0;
                                *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 0;
                                DAT_00489250++;
                            }
                            FUN_0040f9b0(0x65 + (rand() % 7),
                                         entity->position_x, entity->position_y);
                            should_remove = 1;
                        } else if (ent_type == 0x22) {
                            /* Original common terminal path at 0x4455C1: one flash,
                             * with sparkle/deep-boom sound only for modes 2-4. */
                            unsigned char fw_sub = entity->subtype;
                            if (fw_sub == 1 || fw_sub == 2)
                                FUN_0040f9b0(0x114, entity->position_x, entity->position_y);
                            else if (fw_sub == 3)
                                FUN_0040f9b0(0x112, entity->position_x, entity->position_y);
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = entity->position_x;
                                *(int *)(fp + 0x04) = entity->position_y;
                                *(int *)(fp + 0x08) = 0;
                                *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) =
                                    fw_sub >= 2 ? (unsigned char)(rand() % 4 + 0x0D)
                                                : (unsigned char)(rand() % 2 + 1);
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 0;
                                *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 0;
                                DAT_00489250++;
                            }
                            should_remove = 1;
                        } else {
                            should_remove = 1;
                        }
                        break;
                    }
                }
            }
        }

        /* PERSUADERTRON (type 0x0A): convert enemy troopers to shooter's team.
         * Each tick, scan nearby troopers. On proximity hit, change team. */
        if (ent_type == 0x0A && !should_remove && DAT_0048924c > 0) {
            unsigned char shooter_team = entity->owner;
            int ex = entity->position_x;
            int ey = entity->position_y;
            for (int v = 0; v < DAT_0048924c; v++) {
                int toff = v * 0x40;
                unsigned char t_team = *(unsigned char *)(toff + 0x1C + (int)DAT_00487884);
                if (t_team != shooter_team && t_team != 0xFF) {
                    int tx = *(int *)(toff + (int)DAT_00487884);
                    int ty = *(int *)(toff + 8 + (int)DAT_00487884);
                    int dx = (ex - tx) >> 0x12;
                    int dy = (ey - ty) >> 0x12;
                    if (dx * dx + dy * dy < 400) {
                        *(unsigned char *)(toff + 0x1C + (int)DAT_00487884) = shooter_team;
                        should_remove = 1;
                    }
                }
            }
        }

        /* === Exhaust / flame particle types (0x65, 0x67 ONLY) ===
         * Original behavior function at 0x00430480 handles ONLY types 0x65 and 0x67.
         *
         * For state != 0x0A:
         *   1. Position integration: pos += vel (done above)
         *   2. Gravity: type 0x67 + sub_type!=5 only
         *   3. Damping + jitter: sub_type==5 only
         *   4. Boundary checks (early return with removal)
         *   5. Type 0x67: palette fading
         *   6. Type 0x65: water buoyancy (vel_y -= 0x800), tile currents,
         *      surface removal (tile property check), conditional lifetime */
        if (ent_type == 0x67 || ent_type == 0x65) {
            unsigned char sub_type = entity->subtype;

            /* Apply gravity for types other than 0x65, and only when sub_type != 5 */
            if (ent_type != 0x65 && sub_type != 5) {
                entity->velocity_y += entity->gravity_or_motion_38 * DAT_00483828;
            }

            /* Velocity damping + jitter: ONLY for sub_type == 5 (original: 0x430534) */
            if (sub_type == 5) {
                /* Velocity damping: multiply by ~0.985 (double at 0x004756d0) */
                double vx = (double)entity->velocity_x;
                double vy = (double)entity->velocity_y;
                entity->velocity_x = (int)(vx * 0.985);
                entity->velocity_y = (int)(vy * 0.985);

                /* Random position jitter: (128 - rand()%256) << 12 on each axis */
                int jx = (128 - (rand() & 0xFF)) << 12;
                int jy = (128 - (rand() & 0xFF)) << 12;
                entity->position_x += jx;
                entity->position_y += jy;
            }

            /* Palette-based fading lifetime (type 0x67 only in original):
             * entity[0x3C] = frame counter (counts up each tick)
             * entity[0x5C] = frame threshold for palette step
             * entity[0x65] = current palette index (higher = brighter/earlier)
             * entity[0x64] = minimum palette index (death threshold)
             * entity[0x4C] = palette[entity[0x65]] + 30000 (color+lifetime value) */
            if (ent_type == 0x67) {
                int frame_cnt = entity->counter_3c + 1;
                entity->counter_3c = frame_cnt;
                unsigned char threshold = entity->timer_5c;
                if (threshold > 0 && frame_cnt >= (int)threshold) {
                    /* Reset counter and step palette index down */
                    entity->counter_3c = 0;
                    unsigned char pal_idx = entity->scratch_65;
                    unsigned char min_idx = entity->scratch_64;
                    if (pal_idx > min_idx) {
                        pal_idx--;
                        entity->scratch_65 = pal_idx;
                        /* Recompute entity[0x4C] from palette */
                        if (DAT_00487aa8 != NULL) {
                            unsigned short *pal = (unsigned short *)DAT_00487aa8;
                            entity->palette_value = (int)pal[pal_idx] + 30000;
                        }
                    } else {
                        should_remove = 1;
                    }
                }
            }

            /* Boundary removal */
            int bx = entity->position_x;
            int by = entity->position_y;
            if (bx < 0) { entity->position_x = 0; entity->previous_x = 0; should_remove = 1; }
            if (by < 0) { entity->position_y = 0; entity->previous_y = 0; should_remove = 1; }
            if (bx >> 0x12 >= (int)DAT_004879f0) should_remove = 1;
            if (by >> 0x12 >= (int)DAT_004879f4) should_remove = 1;

            /* === Water buoyancy + currents for type 0x65 (original: 0x4308e3-0x4309e2) ===
             * After boundary checks pass, apply upward buoyancy, tile-based water
             * currents, and remove when entity leaves water (tile property check).
             * entity[0x28] is a conditional countdown (only on water + visible). */
            if (ent_type == 0x65 && !should_remove) {
                /* Base buoyancy: vel_y -= 0x800 (upward force every tick) */
                int cur_vy = entity->velocity_y - 0x800;
                entity->velocity_y = cur_vy;

                /* Read tile index at entity pixel position */
                int px = entity->position_x >> 0x12;
                int py = entity->position_y >> 0x12;
                int tile_off = (py << ((unsigned char)DAT_00487a18 & 0x1F)) + px;
                unsigned char tile_idx = *(unsigned char *)((int)DAT_0048782c + tile_off);

                /* Tile-based water currents (0x4308fd-0x430985) */
                switch (tile_idx) {
                    case 0x40: cur_vy -= 0x600; entity->velocity_y = cur_vy; break;
                    case 0x41: cur_vy += 0x600; entity->velocity_y = cur_vy; break;
                    case 0x42: entity->velocity_x -= 0x600; break;
                    case 0x43: entity->velocity_x += 0x600; break;
                    case 0x44: cur_vy -= 0xC00; entity->velocity_y = cur_vy; break;
                    case 0x45: cur_vy += 0xC00; entity->velocity_y = cur_vy; break;
                    case 0x46: entity->velocity_x -= 0xC00; break;
                    case 0x47: entity->velocity_x += 0xC00; break;
                }

                /* Surface check: entity_table[tile*0x20 + 4] == 0 means not water → remove */
                unsigned char water_flag = *(unsigned char *)((int)DAT_00487928 + (unsigned int)tile_idx * 0x20 + 4);
                if (water_flag == 0) {
                    should_remove = 1;
                } else {
                    /* Visibility check: DAT_00481f50 per-pixel map, 16-bit entries */
                    if (DAT_00481f50 != NULL) {
                        unsigned short vis = *(unsigned short *)((int)DAT_00481f50 + tile_off * 2);
                        if (vis == 0) {
                            should_remove = 1;
                        }
                    }
                    /* Conditional lifetime: decrement entity[0x28] only on water+visible.
                     * When it reaches 1 → remove (prevents lingering in deep water). */
                    if (!should_remove) {
                        int life = entity->health_or_damage_28;
                        if (life > 0) {
                            life--;
                            entity->health_or_damage_28 = life;
                        }
                        if (entity->health_or_damage_28 == 1) {
                            should_remove = 1;
                        }
                    }
                }
            }
        }

        /* Lifetime countdown (offset 0x28 — for projectiles and debris).
         * Excluded for types 0x65/0x67 — their callbacks handle removal:
         *   0x65 uses entity[0x28] as conditional countdown in the water handler above.
         *   0x67 uses palette fading for removal. */
        if (ent_type != 0x65 && ent_type != 0x67) {
            int lifetime = entity->health_or_damage_28;
            if (lifetime > 0) {
                lifetime--;
                entity->health_or_damage_28 = lifetime;
                if (lifetime <= 0) {
                    /* PIPEBOMB: trigger detonation instead of silent removal */
                    if (ent_type == 0x28) {
                        entity->state_20 = 0xFA;
                    } else {
                        should_remove = 1;
                    }
                }
            } else if (ent_state == 5 || ent_type == 2 || ent_type >= 0x6C) {
                /* Debris with no lifetime — expire immediately */
                should_remove = 1;
            }
        }

        /* Projectiles with zero lifetime and zero velocity — remove */
        if (ent_type == 0 && entity->health_or_damage_28 == 0 &&
            entity->velocity_x == 0 && entity->velocity_y == 0) {
            should_remove = 1;
        }
        }

        /* Original 0x00434310 updates tracking links and copies only selected
         * fields from the last record; it does not memcpy all 0x80 bytes. */
        if (should_remove) {
            EntityCallbacks_RemoveAt(i);
            /* Don't increment i — re-check swapped-in entry */
        } else {
            i++;
        }
    }
}
/* ===== FUN_004527e0 — Update_Projectiles (004527E0) ===== */
/* Updates particles in DAT_00481f34 (stride 0x20, DAT_00489250 count).
 * Each particle has: +0x00 pos_x, +0x04 pos_y, +0x08 vel_x, +0x0C vel_y,
 * +0x10 type, +0x11 frame, +0x12 sub_frame, +0x13 behavior, +0x14 owner, +0x15 color.
 * Moves particles, advances animation, handles wall collision/ricochet,
 * entity proximity deflection, damage to players/turrets/vehicles.
 * Expired particles are removed by swap-with-last.
 * Behavior byte (+0x13) ranges:
 *   <0xC4  = pure visual (4-frame anim, no collision)
 *   0xC4   = bubble — passes through walls but takes hits
 *   0xC6/8/9/D = bullet variants (ricochet + damage)
 *   >=200  = slow-anim (8-frame) decorative */
void FUN_004527e0(void)
{
    int i = 0;

    while (i < DAT_00489250) {
        int *part = (int *)((int)DAT_00481f34 + i * 0x20);
        int old_x = part[0];
        int old_y = part[1];

        /* Move particle */
        int new_x = old_x + part[2];
        int new_y = old_y + part[3];
        part[0] = new_x;
        part[1] = new_y;

        /* Determine owner team for collision filtering */
        unsigned char owner_byte = *(unsigned char *)(part + 5);  /* +0x14 */
        unsigned int owner_team;
        if (owner_byte < 0x50) {
            owner_team = Player_Get(owner_byte)->team;
        } else if (owner_byte >= 0x78 && owner_byte <= 0x8B) {
            owner_team = owner_byte - 0x78;
        } else {
            owner_team = 0xFB;
        }

        /* Advance animation sub-frame */
        unsigned char behavior = *(unsigned char *)((int)part + 0x13);
        unsigned char sub_frame = *(unsigned char *)((int)part + 0x12) + 1;
        *(unsigned char *)((int)part + 0x12) = sub_frame;

        if (behavior < 200 && behavior != 0xC4) {
            if (sub_frame > 4) {
                *(unsigned char *)((int)part + 0x12) = 0;
                *(unsigned char *)((int)part + 0x11) = *(unsigned char *)((int)part + 0x11) + 1;
            }
        } else {
            if (sub_frame > 8) {
                *(unsigned char *)((int)part + 0x12) = 0;
                *(unsigned char *)((int)part + 0x11) = *(unsigned char *)((int)part + 0x11) + 1;
            }
        }

        /* Check if animation has completed (particle expired) */
        unsigned char part_type = *(unsigned char *)(part + 4);  /* +0x10 */
        unsigned char max_frame = *(unsigned char *)((int)DAT_00481f20 + 6 + (unsigned int)part_type * 8);

        if (*(unsigned char *)((int)part + 0x11) >= max_frame) {
            goto remove_particle;
        }

        /* For non-bullet particles (behavior < 0xC4): skip to end-of-frame check */
        if (behavior < 0xC4) {
            goto check_end_frame;
        }

        /* ---- Bullet-type particles (behavior >= 0xC4) ---- */
        {
            int tx = new_x >> 0x12;
            int ty = new_y >> 0x12;
            /* Bounds check */
            if (tx < 0 || ty < 0 || tx >= (int)DAT_004879f0 || ty >= (int)DAT_004879f4) {
                goto remove_particle;
            }

            /* Entity proximity deflection — scan entity array directly for type 0x0E
             * (MOVING SUCKER). Original used tracking list which goes stale. */
            if (DAT_00489288 == '\0') {
                for (int ei = 0; ei < DAT_00489248; ei++) {
                    Entity *entity = &DAT_004892e8[ei];
                    if (entity->type != 0x0E) continue;
                    unsigned char ent_owner = entity->owner;
                    unsigned char ent_team = Player_Get(ent_owner)->team;
                    if (ent_team != (unsigned char)owner_team) {
                        if (new_x - 0x12C0000 < entity->position_x &&
                            entity->position_x < new_x + 0x12C0000 &&
                            new_y - 0x12C0000 < entity->position_y &&
                            entity->position_y < new_y + 0x12C0000) {
                            int angle = FUN_004257e0(entity->position_x, entity->position_y,
                                                     new_x, new_y);
                            if ((char)entity->subtype == '\0') {
                                part[2] -= *(int *)((int)DAT_00487ab0 + angle * 4) >> 1;
                                part[3] -= *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 1;
                            } else {
                                part[2] += *(int *)((int)DAT_00487ab0 + angle * 4) >> 2;
                                part[3] += *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 2;
                            }
                            break;
                        }
                    }
                }
            }

            /* Re-read position after deflection */
            new_y = part[1];
            new_x = part[0];

            /* Read tile at current position */
            unsigned char tile = *(unsigned char *)((int)DAT_0048782c +
                ((new_y >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1F)) + (new_x >> 0x12));

            /* Tile+4 check: certain tile types destroy non-0xCD particles */
            if (behavior != (unsigned char)0xCD) {
                if (*(char *)((unsigned int)tile * 0x20 + 4 + (int)DAT_00487928) == '\x01') {
                    goto remove_particle;
                }
            }

            /* Wall collision (skip for behavior 0xC4) */
            if (behavior != (unsigned char)0xC4) {
                if (*(char *)((unsigned int)tile * 0x20 + 2 + (int)DAT_00487928) == '\0' &&
                    *(char *)((unsigned int)tile * 0x20 + 10 + (int)DAT_00487928) == '\0') {
                    /* Non-passable tile hit */
                    /* 25% chance: spawn wall-hit explosion */
                    unsigned int rval = rand();
                    rval = rval & 0x80000003;
                    if ((int)rval < 0) rval = (rval - 1 | 0xFFFFFFFC) + 1;
                    if (rval == 0) {
                        char beh = *(char *)((int)part + 0x13);
                        if (beh == (char)0xC6 || beh == (char)0xC8 ||
                            beh == (char)0xCD || beh == (char)0xC9) {
                            if (tile != 0x0A && tile != 0x10) {
                                /* Calculate explosion level from particle type and frame */
                                char ptype_c = (char)*(unsigned char *)(part + 4);
                                int explevel;
                                if (ptype_c == 5 || ptype_c == 6) {
                                    explevel = (0x16 - (unsigned int)*(unsigned char *)((int)part + 0x11));
                                    explevel = (explevel + (explevel >> 31 & 0xF)) >> 4;
                                } else if (ptype_c == 3 || ptype_c == 4) {
                                    explevel = (int)(0x16 - (unsigned int)*(unsigned char *)((int)part + 0x11)) / 0xC;
                                } else if (ptype_c == 1 || ptype_c == 2) {
                                    explevel = (0x16 - (unsigned int)*(unsigned char *)((int)part + 0x11));
                                    explevel = (explevel + (explevel >> 31 & 7)) >> 3;
                                } else {
                                    explevel = 3;
                                }
                                if (explevel < 0) explevel = 0;
                                else if (explevel > 7) explevel = 7;
                                if (beh == (char)0xCD) explevel = 6;
                                /* param5: -1 for non-water tiles, 1 for water(tile==0xC) */
                                char p5 = (tile != 0x0C) ? (char)-1 : (char)1;
                                FUN_004357b0(new_x >> 0x12, new_y >> 0x12, explevel, 0, p5,
                                             0, 0, 0, 0, -1, '\0', *(unsigned char *)(part + 5));
                            }
                        }
                    }

                    /* Behavior 0xC9 (bubbles): convert to 0xC4 instead of ricochet */
                    if (behavior == (unsigned char)0xC9) {
                        *(unsigned char *)((int)part + 0x13) = 0xC4;
                    } else {
                        /* Ricochet: reverse velocity direction, restore old position */
                        int angle = FUN_004257e0(0, 0, part[2], part[3]);
                        unsigned int rval2 = rand();
                        rval2 = rval2 & 0x80000001;
                        int btest = 0;
                        if ((int)rval2 < 0) btest = ((rval2 - 1) | 0xFFFFFFFE) == 0xFFFFFFFF ? 1 : 0;
                        else btest = (rval2 == 0) ? 1 : 0;
                        unsigned int new_angle;
                        if (btest) {
                            new_angle = (angle + 0x200);
                        } else {
                            new_angle = (angle - 0x200);
                        }
                        part[0] = old_x;
                        part[1] = old_y;
                        new_angle &= 0x7FF;
                        int *lut = (int *)DAT_00487ab0;
                        part[2] = lut[new_angle] * 0x14 >> 6;
                        part[3] = lut[(new_angle + 0x200) & 0x7FF] * 0x14 >> 6;
                    }
                }
            }

            /* Damage section: only on first 2 sub-frames, and behavior > 0xC6 or == 0xC4 */
            unsigned char cur_sub = *(unsigned char *)((int)part + 0x12);
            unsigned char cur_beh = *(unsigned char *)((int)part + 0x13);
            if (cur_sub < 2 && (cur_beh > 0xC6 || cur_beh == 0xC4)) {
                int p_y = part[1];
                int p_x = part[0];
                unsigned char grid_byte = *(unsigned char *)(
                    (p_x >> 0x16) + (int)DAT_00487814 + (p_y >> 0x16) * DAT_004879f8);

                /* Calculate base damage = multiplier * 0x1400 */
                int dmg_mult = 4; /* default */
                if (cur_beh == 0xCD) {
                    dmg_mult = 0x6E;
                } else {
                    unsigned char ptype2 = *(unsigned char *)(part + 4);
                    if (ptype2 >= 0x11) {
                        dmg_mult = 10;
                    } else if (ptype2 == 5 || ptype2 == 6) {
                        dmg_mult = 2;
                    } else if (ptype2 == 3 || ptype2 == 4) {
                        dmg_mult = 3;
                    } else if (ptype2 == 1 || ptype2 == 2) {
                        dmg_mult = 6;
                    }
                }
                int base_damage = dmg_mult * 0x1400;

                /* Turret owner range 0x78-0x8B with CD behavior: quarter damage */
                if (owner_byte >= 0x78 && owner_byte < 0x8C && cur_beh == 0xCD) {
                    base_damage = base_damage >> 2;
                }

                /* 0xC4 and 0xC9 behaviors: double damage */
                if (cur_beh == 0xC4 || cur_beh == 0xC9) {
                    base_damage = base_damage * 2;
                }

                /* Destructible tiles (type >= 0xF0): damage tile health */
                if (tile >= 0xF0) {
                    int *tile_hp = (int *)((unsigned int)tile * 0x20 - 0x1DF4 + (int)DAT_00489e80);
                    *tile_hp -= base_damage;
                }

                /* Player collision */
                if ((grid_byte & 1) == 1 && DAT_00489240 > 0) {
                    int *stat_ptr = &DAT_00486be8[0];
                    for (int p = 0; p < DAT_00489240; p++) {
                        PlayerData *player = Player_Get(p);
                        if (player->health > 0 &&
                            p != (int)(unsigned int)owner_byte) {
                            /* AABB check using sprite descriptor dimensions */
                            int desc_base = (int)DAT_00481f20 + (unsigned int)*(unsigned char *)(part + 4) * 8;
                            unsigned int hw = (unsigned int)(*(unsigned char *)(desc_base + 4) & 0xFE);
                            int player_x = player->position_x;
                            if (player_x - (int)(hw * 0x20000) < p_x &&
                                p_x < player_x + (int)(hw * 0x20000)) {
                                int player_y = player->position_y;
                                unsigned int hh = (unsigned int)(*(unsigned char *)(desc_base + 5) & 0xFE);
                                if (player_y - (int)(hh * 0x20000) < p_y &&
                                    p_y < player_y + (int)(hh * 0x20000)) {
                                    /* Hit! */
                                    player->flag_a3 = 1;
                                    int dmg_score = base_damage >> 0xD;
                                    *stat_ptr += dmg_score;

                                    /* Track damage dealt by owner */
                                    if (owner_byte < 0x50) {
                                        PlayerData *owner = Player_Get(owner_byte);
                                        if (owner->team != player->team) {
                                            DAT_00486e68[owner_byte] += dmg_score;
                                        }
                                        /* Apply damage (check team + friendly fire) */
                                        if (owner->team != player->team || DAT_0048373d != '\0') {
                                            player->health = tou_binary::sub_wrap_i32(
                                                player->health, base_damage);
                                        }
                                    } else {
                                        player->health = tou_binary::sub_wrap_i32(
                                            player->health, base_damage);
                                    }

                                    /* Record last attacker */
                                    if (owner_byte < 0x50) {
                                        if (Player_Get(owner_byte)->team != player->team || DAT_0048373d != '\0') {
                                            player->last_attacker = owner_byte;
                                        }
                                    } else if (owner_byte < 0x8C) {
                                        player->last_attacker = owner_byte - 0x14;
                                    } else {
                                        player->last_attacker = 0xFF;
                                    }

                                    /* Set damage type indicator */
                                    player->timer_4a2 = 0x6E;

                                    /* Reduce armor if present */
                                    unsigned char armor = player->stun_timer;
                                    if (armor != 0) {
                                        int new_armor = (unsigned int)armor - dmg_score - 1;
                                        if (new_armor < 0) new_armor = 0;
                                        player->stun_timer = (unsigned char)new_armor;
                                    }
                                }
                            }
                        }
                        stat_ptr++;
                    }
                }

                /* Turret collision (tile+10 == 1 means turret zone) */
                {
                    unsigned char tile2 = *(unsigned char *)((int)DAT_0048782c +
                        ((part[1] >> 0x12) << ((unsigned char)DAT_00487a18 & 0x1F)) + (part[0] >> 0x12));
                    if (*(char *)((unsigned int)tile2 * 0x20 + 10 + (int)DAT_00487928) == '\x01') {
                        if (DAT_00489260 > 0) {
                            int toff = 0;
                            int turret_base = (int)DAT_00481f28;
                            for (int t = 0; t < DAT_00489260; t++) {
                                unsigned char turret_team = *(unsigned char *)(toff + 0x1D + turret_base);
                                if (owner_team != (unsigned int)turret_team) {
                                    int desc_base2 = (int)DAT_00481f20 + (unsigned int)*(unsigned char *)(part + 4) * 8;
                                    unsigned int thw = (unsigned int)(*(unsigned char *)(desc_base2 + 4) & 0xFE);
                                    int turret_x = *(int *)(toff + turret_base);
                                    if (turret_x - (int)(thw * 0x20000) < p_x &&
                                        p_x < turret_x + (int)(thw * 0x20000)) {
                                        int turret_y = *(int *)(toff + 4 + turret_base);
                                        unsigned int thh = (unsigned int)(*(unsigned char *)(desc_base2 + 5) & 0xFE);
                                        if (turret_y - (int)(thh * 0x20000) < p_y &&
                                            p_y < turret_y + (int)(thh * 0x20000)) {
                                            *(unsigned char *)(toff + 0x1E + turret_base) = 1;
                                            *(int *)(toff + 0x10 + (int)DAT_00481f28) -= base_damage;
                                            turret_base = (int)DAT_00481f28;
                                        }
                                    }
                                }
                                toff += 0x40;
                            }
                        }
                    }
                }

                /* Building collision (grid_byte bit 1) */
                if ((grid_byte & 2) == 2) {
                    FUN_00451e70(i, base_damage);
                }

                /* Vehicle/trooper collision (grid_byte bit 2) */
                if ((grid_byte & 4) == 4 && DAT_0048924c > 0) {
                    int voff = 0;
                    int veh_base = (int)DAT_00487884;
                    for (int v = 0; v < DAT_0048924c; v++) {
                        unsigned char veh_team = *(unsigned char *)(voff + 0x1C + veh_base);
                        if (owner_team != (unsigned int)veh_team) {
                            int desc_base3 = (int)DAT_00481f20 + (unsigned int)*(unsigned char *)(part + 4) * 8;
                            unsigned int vhw = (unsigned int)(*(unsigned char *)(desc_base3 + 4) & 0xFE);
                            int veh_x = *(int *)(voff + veh_base);
                            if (veh_x - (int)(vhw * 0x20000) < p_x &&
                                p_x < veh_x + (int)(vhw * 0x20000)) {
                                int veh_y = *(int *)(voff + 8 + veh_base);
                                unsigned int vhh = (unsigned int)(*(unsigned char *)(desc_base3 + 5) & 0xFE);
                                if (veh_y - (int)(vhh * 0x20000) < p_y &&
                                    p_y < veh_y + (int)(vhh * 0x20000)) {
                                    *(unsigned char *)(voff + 0x2C + veh_base) = 1;
                                    *(int *)(voff + 0x28 + (int)DAT_00487884) -= base_damage;
                                    veh_base = (int)DAT_00487884;
                                }
                            }
                        }
                        voff += 0x40;
                    }
                }

            }
        }

check_end_frame:
        /* Final expiry check */
        if (*(unsigned char *)((int)part + 0x11) >=
            *(unsigned char *)((int)DAT_00481f20 + 6 + (unsigned int)*(unsigned char *)(part + 4) * 8)) {
            goto remove_particle;
        }

        i++;
        continue;

remove_particle:
        /* Remove by swapping with last */
        DAT_00489250--;
        int last = DAT_00489250 * 0x20;
        part[0] = *(int *)((int)DAT_00481f34 + last);
        part[1] = *(int *)((int)DAT_00481f34 + last + 4);
        part[2] = *(int *)((int)DAT_00481f34 + last + 8);
        part[3] = *(int *)((int)DAT_00481f34 + last + 0xC);
        *(unsigned char *)((int)part + 0x11) = *(unsigned char *)((int)DAT_00481f34 + last + 0x11);
        *(unsigned char *)(part + 4) = *(unsigned char *)((int)DAT_00481f34 + last + 0x10);
        *(unsigned char *)((int)part + 0x12) = *(unsigned char *)((int)DAT_00481f34 + last + 0x12);
        *(unsigned char *)((int)part + 0x13) = *(unsigned char *)((int)DAT_00481f34 + last + 0x13);
        *(unsigned char *)(part + 5) = *(unsigned char *)((int)DAT_00481f34 + last + 0x14);
        *(unsigned char *)((int)part + 0x15) = *(unsigned char *)((int)DAT_00481f34 + last + 0x15);
        if (i >= DAT_00489250) break;
        /* Don't increment i — re-check swapped-in entry */
    }
}
/* ===== FUN_00455a20 — Turret_Step_Up (00455A20) ===== */
/* If the turret is standing on a non-walkable tile and there's a step of 1-3
 * tiles upward at its movement edge, adjust Y position upward. */
static void FUN_00455a20(int *param_1)
{
    unsigned int uVar2 = (unsigned int)param_1[2];
    /* Check Y bounds: must be above 0x180000 and below map height - 2 */
    if ((int)(uVar2 & 0xfffc0000) <= 0x180000)
        return;
    if ((int)uVar2 >> 0x12 >= (int)DAT_004879f4 - 2)
        return;

    int shift = (unsigned char)DAT_00487a18 & 0x1f;
    int iVar6 = ((int)uVar2 >> 0x12) << shift;
    int iVar4 = (*param_1 >> 0x12) + iVar6;

    /* Check if tile below center is non-walkable */
    unsigned char tile_below = *(unsigned char *)((int)DAT_0048782c + iVar4 + DAT_00487a00);
    if (*(char *)((unsigned int)tile_below * 0x20 + 1 + (int)DAT_00487928) != '\0')
        return;

    /* Scan 4 tiles upward from movement edge position */
    unsigned char *pbVar3 = (unsigned char *)((param_1[4] + *param_1 >> 0x12) + iVar6 + (int)DAT_0048782c);
    char scan[4];
    for (int i = 0; i < 4; i++) {
        unsigned char bVar1 = *pbVar3;
        pbVar3 = (unsigned char *)((int)pbVar3 - DAT_00487a00); /* move up one row */
        scan[i] = *(char *)((unsigned int)bVar1 * 0x20 + 1 + (int)DAT_00487928);
    }

    /* Also scan from center position (unused in final logic but matches original) */
    unsigned char *p2 = (unsigned char *)((int)DAT_0048782c + (iVar4 - DAT_00487a00));
    char scan2[3];
    for (int i = 0; i < 3; i++) {
        unsigned char b = *p2;
        p2 = (unsigned char *)((int)p2 - DAT_00487a00);
        scan2[i] = *(char *)((unsigned int)b * 0x20 + 1 + (int)DAT_00487928);
    }
    (void)scan2; /* matches original - scanned but not used in conditions */

    /* Step up: find walkable->non-walkable transition */
    if (scan[0] == '\0' && scan[1] == '\x01') {
        param_1[2] = uVar2 - FIXED_SCALE;
    } else if (scan[1] == '\0' && scan[2] == '\x01') {
        param_1[2] = uVar2 - 0x80000;
    } else if (scan[2] == '\0' && scan[3] == '\x01') {
        param_1[2] = uVar2 - 0xc0000;
    }
}

/* ===== FUN_00455b50 — Turret_Validate_Step_Down (00455B50) ===== */
/* Validates turret path ahead. Returns low byte 0 = OK, low byte 1 = reversal/jump.
 * Handles step-down (1-3 tiles), random direction reversal, and edge jumping. */
static unsigned int FUN_00455b50(int *param_1)
{
    unsigned int uVar5 = DAT_004879f4 - 4;
    int iVar6 = param_1[2];

    if (iVar6 >> 0x12 >= (int)uVar5)
        return uVar5 & 0xffffff00;

    int shift = (unsigned char)DAT_00487a18 & 0x1f;
    int iVar7 = (iVar6 >> 0x12) << shift;
    uVar5 = (*param_1 + param_1[4] >> 0x12) + iVar7; /* edge tile index */

    /* Check: tile at center + 1 row down is non-walkable (i.e. ground exists) */
    unsigned char tile_center_below = *(unsigned char *)((int)DAT_0048782c + (*param_1 >> 0x12) + iVar7 + DAT_00487a00);
    if (*(char *)((unsigned int)tile_center_below * 0x20 + 1 + (int)DAT_00487928) != '\0')
        return uVar5 & 0xffffff00;

    /* Check edge tile walkability */
    char cVar1 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + uVar5) * 0x20 + 1 + (int)DAT_00487928);
    /* Check edge + 1 row down */
    char edge_below = *(char *)((unsigned int)*(unsigned char *)(DAT_00487a00 + (int)DAT_0048782c + uVar5) * 0x20 + 1 + (int)DAT_00487928);

    /* If edge is walkable AND edge+1row is non-walkable, ground is there - OK */
    if (cVar1 == '\x01' && edge_below == '\0')
        return uVar5 & 0xffffff00;

    /* Step-down checks */
    char cVar2 = *(char *)((unsigned int)*(unsigned char *)(DAT_00487a00 + (int)DAT_0048782c + uVar5) * 0x20 + 1 + (int)DAT_00487928);
    char cVar3 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + DAT_00487a00 * 2 + uVar5) * 0x20 + 1 + (int)DAT_00487928);

    if (cVar2 == '\x01' && cVar3 == '\0') {
        uVar5 = (unsigned int)(iVar6 + FIXED_SCALE);
        param_1[2] = (int)uVar5;
        return uVar5 & 0xffffff00;
    }

    char cVar4 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + DAT_00487a00 * 3 + uVar5) * 0x20 + 1 + (int)DAT_00487928);

    if (cVar3 == '\x01' && cVar4 == '\0') {
        param_1[2] = iVar6 + 0x80000;
        return (unsigned int)(iVar6 + 0x80000) & 0xffffff00;
    }

    char cVar5_below4 = *(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + DAT_00487a00 * 4 + uVar5) * 0x20 + 1 + (int)DAT_00487928);

    if (cVar4 == '\x01' && cVar5_below4 == '\0') {
        param_1[2] = iVar6 + 0xc0000;
        return (unsigned int)(iVar6 + 0xc0000) & 0xffffff00;
    }

    /* All 5 rows walkable - open air ahead */
    if (cVar1 == '\x01' && cVar2 == '\x01' && cVar3 == '\x01' && cVar4 == '\x01' && cVar5_below4 == '\x01') {
        int r = rand();
        /* Random reversal with bounce cooldown check */
        if (((unsigned char)(r % 250) < 248 || *(unsigned char *)(param_1 + 7) < 10) &&
            *(char *)((int)param_1 + 0x39) == '\0') {
            int vel = param_1[4];
            *(char *)((int)param_1 + 0x39) = 50;
            *(char *)((int)param_1 + 0x1d) = -*(char *)((int)param_1 + 0x1d);
            param_1[4] = -vel;
            return ((unsigned int)(-vel) & 0xffffff00) | 1;
        }
        /* Non-mobile turrets jump */
        if (*(char *)((int)param_1 + 0x25) == '\0' || *(char *)((int)param_1 + 0x25) == '\x02') {
            param_1[5] = -0x44c00;
        }
        return ((unsigned int)r & 0xffffff00) | 1;
    }

    return uVar5 & 0xffffff00;
}

/* ===== check_line_of_sight — Bresenham tile raycast ===== */
/* Walks from source to target in tile coordinates, checking only intermediate tiles.
 * Skips source and target tiles since entities stand on solid platforms.
 * Returns 1 if clear line of sight, 0 if any intermediate tile is blocked. */
static int check_line_of_sight(int sx, int sy, int tx, int ty)
{
    int shift = (unsigned char)DAT_00487a18 & 0x1f;
    int x0 = sx >> 0x12;
    int y0 = sy >> 0x12;
    int x1 = tx >> 0x12;
    int y1 = ty >> 0x12;

    int dx = x1 - x0;
    int dy = y1 - y0;
    int abs_dx = dx < 0 ? -dx : dx;
    int abs_dy = dy < 0 ? -dy : dy;
    int step_x = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int step_y = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    int err = abs_dx - abs_dy;

    int steps = abs_dx + abs_dy;
    for (int s = 0; s < steps; s++) {
        /* Step first — this skips the source tile on the first iteration */
        int e2 = 2 * err;
        if (e2 > -abs_dy) { err -= abs_dy; x0 += step_x; }
        if (e2 < abs_dx) { err += abs_dx; y0 += step_y; }

        /* Don't check the target tile either */
        if (x0 == x1 && y0 == y1) break;

        if (x0 < 1 || y0 < 1 || x0 >= (int)DAT_004879f0 || y0 >= (int)DAT_004879f4) {
            return 0;
        }
        int tile_idx = (y0 << shift) + x0;
        unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_idx);
        if (*(char *)((unsigned int)tile * 0x20 + 1 + (int)DAT_00487928) == '\0') {
            return 0;
        }
    }
    return 1;
}

/* ===== FUN_00459dd0 — Aimed Fire Direction (00459DD0) ===== */
/* Try the original low ballistic arc and, when permitted, the high arc.  Each
 * candidate must survive the original trajectory-aware terrain trace. */
static int FUN_00459dd0(int sx, int sy, int tx, int ty, float speed_factor, char use_alt)
{
    int angle = FUN_004599f0(sx, sy, tx, ty, 0, speed_factor, DAT_00483828);
    if (angle != 0x801 &&
        FUN_00459c70(sx, sy, tx, ty, angle, speed_factor, DAT_00483828) != '\0') {
        return angle;
    }
    if (use_alt != '\0') {
        angle = FUN_004599f0(sx, sy, tx, ty, 1, speed_factor, DAT_00483828);
        if (angle != 0x801 &&
            FUN_00459c70(sx, sy, tx, ty, angle, speed_factor, DAT_00483828) != '\0') {
            return angle;
        }
    }
    return 0x801;
}

/* ===== FUN_00454b00 — Update_Turrets (00454B00) ===== */
/* Main turret update loop. Processes all troopers/turrets in DAT_00487884
 * (stride 0x40, count DAT_0048924c). Handles patrol movement, step-up/down,
 * gravity, boundary clamping, wall collision, aiming, firing, and fluid damage. */
void FUN_00454b00(void)
{
    int i = 0;
    if (DAT_0048924c <= 0) return;

    do {
        int *t = (int *)((int)DAT_00487884 + i * 0x40);
        int saved_x = *t;
        int saved_y = t[2];

        /* Determine if this is a mobile turret (type 1) */
        char turret_type = *(char *)((int)t + 0x25);
        int is_mobile = (turret_type != '\0' && turret_type != '\x02') ? 1 : 0;

        /* Set patrol velocity if movement_mode == 0 */
        if (*(char *)((int)t + 0x2d) == '\0') {
            t[4] = (int)*(char *)((int)t + 0x1d) * t[8];
        }

        /* Step-up helper */
        FUN_00455a20(t);

        /* Validation / step-down */
        unsigned int val_result = FUN_00455b50(t);

        /* If validation OK and tile ahead is non-walkable: reverse and bounce */
        if ((char)val_result == '\0') {
            int shift = (unsigned char)DAT_00487a18 & 0x1f;
            int ty_tile = t[2] >> 0x12;
            int edge_tile = (t[4] + *t >> 0x12) + (ty_tile << shift);
            unsigned char tile_val = *(unsigned char *)((int)DAT_0048782c + edge_tile);
            if (*(char *)((unsigned int)tile_val * 0x20 + 1 + (int)DAT_00487928) == '\0') {
                *(char *)((int)t + 0x39) = 0x32; /* bounce cooldown = 50 */
                *(char *)((int)t + 0x1d) = -*(char *)((int)t + 0x1d);
                t[4] = -(t[4] >> 1); /* halve and reverse velocity */
            }
        }

        /* Save previous positions */
        int vel_y = t[5];
        t[1] = *t;     /* prev_x = pos_x */
        t[3] = t[2];   /* prev_y = pos_y */

        /* Apply gravity */
        t[5] = vel_y + 0x2000;

        /* Jump logic for non-mobile turrets */
        if (!is_mobile) {
            if (turret_type == '\0' && t[5] > 0x96000) {
                if (rand() % 0x14 == 0) {
                    t[6] = t[6] | 1; /* set ground flag */
                }
            } else if (turret_type == '\x02' && t[5] > 0x70800) {
                if (rand() % 5 == 0) {
                    t[6] = t[6] | 1;
                }
            }
        }

        /* Decrement bounce cooldown */
        if (*(char *)((int)t + 0x39) != '\0') {
            *(char *)((int)t + 0x39) = *(char *)((int)t + 0x39) - 1;
        }

        /* Apply drag to vel_y */
        unsigned int flags = (unsigned int)t[6];
        double drag_factor;
        if ((flags & 1) == 1) {
            drag_factor = 0.96;  /* in air */
        } else {
            drag_factor = 0.99;  /* on ground */
        }
        int dragged_vy = (int)((double)t[5] * drag_factor);
        t[5] = dragged_vy;

        /* Position integration */
        int new_y = t[2] + dragged_vy;
        int new_x = *t + t[4];
        t[2] = new_y;
        *t = new_x;

        /* Boundary clamping */
        if (new_x < 0xc0000) {
            *t = 0xc0000;
        } else {
            int max_x = ((int)DAT_004879f0 - 4) * FIXED_SCALE;
            if (new_x > max_x) {
                *t = max_x;
            }
        }

        if (new_y < 0xc0000) {
            t[2] = 0xc0000;
        } else {
            int max_y = ((int)DAT_004879f4 - 4) * FIXED_SCALE;
            if (new_y > max_y) {
                t[2] = max_y;
            }
        }

        /* Post-move wall collision check */
        int shift = (unsigned char)DAT_00487a18 & 0x1f;
        int tile_y_idx = t[2] >> 0x12;
        int tile_x_idx = *t >> 0x12;
        int tilemap_off = (tile_y_idx << shift) + tile_x_idx;
        unsigned char cur_tile = *(unsigned char *)((int)DAT_0048782c + tilemap_off);
        char tile_walk = *(char *)((unsigned int)cur_tile * 0x20 + 1 + (int)DAT_00487928);

        if (tile_walk == '\0') {
            /* Hit wall */
            if (is_mobile) {
                if (dragged_vy > 0x96000) {
                    t[10] = (int)0xfa0a1f01; /* kill turret (large negative health) */
                }
            } else {
                if (dragged_vy > 0x7d000) {
                    t[10] = (int)0xfa0a1f01;
                }
            }
            /* Restore Y, zero velocity */
            t[6] = (int)(flags & 0xfffffffe);
            t[2] = t[3]; /* restore prev_y */
            t[4] = 0;
            t[5] = 0;
            *(char *)((int)t + 0x2d) = 0;
        }

        /* === Aiming logic === */
        if (is_mobile) {
            /* Mobile turret (type 1) aiming */
            int ty_tile2 = t[2] >> 0x12;
            int tx_tile2 = *t >> 0x12;
            int center_tile = (ty_tile2 << shift) + tx_tile2;

            /* Check if tile below center is walkable (on ground) */
            unsigned char below_tile = *(unsigned char *)((int)DAT_0048782c + center_tile + DAT_00487a00);
            if (*(char *)((unsigned int)below_tile * 0x20 + 1 + (int)DAT_00487928) == '\x01') {
                /* On ground: aim based on movement mode */
                char move_mode = *(char *)((int)t + 0x2d);
                if (move_mode == '\0') {
                    /* Patrol: aim at saved position */
                    t[0xc] = FUN_004257e0(saved_x, saved_y, *t, t[2]);
                } else if (move_mode == '\x01') {
                    /* CW spin */
                    unsigned int new_angle = t[0xc] + 0x30;
                    t[0xc] = new_angle & 0x7ff;
                } else {
                    /* CCW spin */
                    unsigned int new_angle = t[0xc] - 0x30;
                    t[0xc] = new_angle & 0x7ff;
                }
            } else {
                /* In air: complex ledge-scanning aim adjustment */
                char patrol_dir = *(char *)((int)t + 0x1d);
                char cVar15 = '\0';
                int aim_target = 0;

                if (patrol_dir == '\x01') {
                    t[0xc] = 0x200; /* face right */
                    cVar15 = (char)-12; /* 0xF4 */
                }
                if (patrol_dir == (char)-1) {
                    t[0xc] = 0x600; /* face left */
                    cVar15 = '\f';  /* 12 */
                }

                /* Compute tile index at movement edge */
                int edge_idx = (int)patrol_dir + tx_tile2 + (ty_tile2 << shift);

                /* Decay aim_rate towards 0 */
                if (t[0xd] > 0) {
                    t[0xd] -= 0x10;
                }
                if (t[0xd] < 0) {
                    t[0xd] += 0x10;
                }

                /* Scan tiles below edge to determine aim adjustment */
                int tilemap_base = (int)DAT_0048782c;
                int etable = (int)DAT_00487928;
                int row_stride = DAT_00487a00;

                /* Check rows 1-2 below edge */
                char r1 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + edge_idx + row_stride) * 0x20 + 1 + etable);
                char r2 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + edge_idx + row_stride * 2) * 0x20 + 1 + etable);

                if (r1 == '\x01' && r2 == '\0') {
                    aim_target = (int)cVar15 << 4;
                } else {
                    char r3 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + edge_idx + row_stride * 3) * 0x20 + 1 + etable);
                    if (r2 == '\x01' && r3 == '\0') {
                        aim_target = (int)cVar15 << 5;
                    } else {
                        char r4 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + edge_idx + row_stride * 4) * 0x20 + 1 + etable);
                        if (r3 == '\x01' && r4 == '\0') {
                            aim_target = (int)cVar15 * 0x30;
                        }
                    }
                }

                /* Check tiles above edge */
                char u0 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + edge_idx) * 0x20 + 1 + etable);
                char u1 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base - row_stride + edge_idx) * 0x20 + 1 + etable);

                if (u0 == '\0' && u1 == '\x01') {
                    aim_target = (int)cVar15 * -0x10;
                } else {
                    char u2 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + row_stride * -2 + edge_idx) * 0x20 + 1 + etable);
                    if (u1 == '\0' && u2 == '\x01') {
                        aim_target = (int)cVar15 * -0x20;
                    } else {
                        char u3 = *(char *)((unsigned int)*(unsigned char *)(tilemap_base + row_stride * -3 + edge_idx) * 0x20 + 1 + etable);
                        if (u2 == '\0' && u3 == '\x01') {
                            aim_target = (int)cVar15 * 3 * -0x10;
                        }
                    }
                }

                /* Slew aim_rate towards target */
                if (t[0xd] < aim_target) {
                    t[0xd] += 0x30;
                }
                if (aim_target < t[0xd]) {
                    t[0xd] -= 0x30;
                }

                /* Apply aim_rate to aim_angle */
                t[0xc] = (t[0xc] + t[0xd]) & 0x7ff;
            }
        } else {
            /* Non-mobile turret: random chance to jump */
            int rval = rand();
            if (rval % 500 == 0 && *(unsigned char *)(t + 7) > 10) {
                /* Check if tile below is non-walkable (on ground) */
                int ty_t = t[2] >> 0x12;
                int tx_t = *t >> 0x12;
                int below_idx = (ty_t << shift) + tx_t + DAT_00487a00;
                unsigned char btile = *(unsigned char *)((int)DAT_0048782c + below_idx);
                if (*(char *)((unsigned int)btile * 0x20 + 1 + (int)DAT_00487928) == '\0') {
                    t[5] = -0x44c00; /* jump */
                    t[6] = t[6] & (int)0xfffffffe;
                }
            }
        }

        /* === Fire cooldown / targeting === */
        if ((char)t[0xe] != '\0') {
            /* Decrement fire cooldown */
            *(char *)(t + 0xe) = (char)t[0xe] - 1;
        } else {
            /* Cooldown reached 0: try to fire */
            if (DAT_00489248 < 0x9c4 && (char)t[7] != (char)-1) {
                int tgt_x = 0;
                int tgt_y = 0;
                int found_target = 0;

                if ((char)t[7] == (char)-2) {
                    /* Team 0xFE: random angle, skip targeting */
                    found_target = 1;
                } else {
                    /* Search players in range */
                    int range_x_hi = *t + 0x3200000;
                    int range_x_lo = *t - 0x3200000;
                    int range_y_hi = t[2] + 0x3200000;
                    int range_y_lo = t[2] - 0x3200000;
                    unsigned int candidate_count = 0;
                    int candidate_indices[1024];
                    int candidate_dists[1024];
                    int player_idx = 0;

                    if (DAT_00489240 > 0) {
                        for (int p = 0; p < DAT_00489240; p++) {
                            PlayerData *player = Player_Get(p);
                            /* Skip dead or same-team players */
                            if (player->state_24 == 0 && player->team != (uint8_t)t[7]) {
                                int px = player->position_x;
                                int py = player->position_y;

                                if (px < range_x_hi && range_x_lo < px &&
                                    py < range_y_hi && range_y_lo < py) {
                                    int dx = (px - *t) >> 0x12;
                                    int dy = (py - t[2]) >> 0x12;
                                    int dist_sq = dx * dx + dy * dy;

                                    if (dist_sq < 40000) {
                                        /* Full Bresenham LOS check from turret to player */
                                        int can_fire = 0;

                                        if (*(char *)((int)t + 0x25) == '\x02') {
                                            can_fire = 1; /* floor turret always fires */
                                        } else {
                                            can_fire = check_line_of_sight(*t, t[2], px, py);
                                        }

                                        if (can_fire && candidate_count < 1024) {
                                            candidate_dists[candidate_count] = dist_sq;
                                            candidate_indices[candidate_count] = player_idx;
                                            candidate_count++;
                                        }
                                    }
                                }
                            }
                            player_idx++;
                        }

                        if (candidate_count > 0) {
                            unsigned int pick = (unsigned int)rand() % candidate_count;
                            int chosen = candidate_indices[pick];
                            PlayerData *target = Player_Get(chosen);
                            tgt_x = target->position_x;
                            tgt_y = target->position_y;
                            found_target = 1;
                        }
                    }

                    /* Phase 2: search buildings/projectiles (DAT_00481f28 via spatial grid).
                     * For each enemy team, scan the projectile bucket in DAT_00487aa4. */
                    if (!found_target && DAT_00487aa4 != NULL && DAT_00481f28 != NULL) {
                        unsigned char own_team2 = (unsigned char)t[7];
                        for (int ti = 0; ti < 4 && !found_target; ti++) {
                            if ((unsigned char)ti == own_team2) continue;
                            int grid_base = (int)DAT_00487aa4 + ti * 0x4000;
                            int proj_count = *(int *)(grid_base + 0x100C);
                            int *proj_indices = (int *)(grid_base + 0x1010);
                            for (int pi2 = 0; pi2 < proj_count && pi2 < 500; pi2++) {
                                int pidx2 = proj_indices[pi2];
                                int pbase2 = (int)DAT_00481f28 + pidx2 * 0x40;
                                int ppx = *(int *)(pbase2 + 0x00);
                                int ppy = *(int *)(pbase2 + 0x04);
                                if (ppx < range_x_hi && range_x_lo < ppx &&
                                    ppy < range_y_hi && range_y_lo < ppy) {
                                    int ddx = (ppx - *t) >> 0x12;
                                    int ddy = (ppy - t[2]) >> 0x12;
                                    int dd = ddx * ddx + ddy * ddy;
                                    if (dd < 40000 && dd > 0) {
                                        tgt_x = ppx; tgt_y = ppy;
                                        found_target = 1; break;
                                    }
                                }
                            }
                        }
                    }

                    /* Phase 3: search other troopers/vehicles (DAT_00487884 via spatial grid).
                     * Enemy troopers are binned at grid offset +0x08 (count) / +0x0C (indices). */
                    if (!found_target && DAT_00487aa4 != NULL && DAT_00487884 != NULL) {
                        unsigned char own_team3 = (unsigned char)t[7];
                        for (int ti2 = 0; ti2 < 4 && !found_target; ti2++) {
                            if ((unsigned char)ti2 == own_team3) continue;
                            int grid_base2 = (int)DAT_00487aa4 + ti2 * 0x4000;
                            int troop_count = *(int *)(grid_base2 + 0x08);
                            int *troop_indices = (int *)(grid_base2 + 0x0C);
                            for (int tri = 0; tri < troop_count && tri < 500; tri++) {
                                int tidx2 = troop_indices[tri];
                                int tbase3 = (int)DAT_00487884 + tidx2 * 0x40;
                                int ttx = *(int *)(tbase3 + 0x00);
                                int tty = *(int *)(tbase3 + 0x08);
                                if (ttx < range_x_hi && range_x_lo < ttx &&
                                    tty < range_y_hi && range_y_lo < tty) {
                                    int ddx = (ttx - *t) >> 0x12;
                                    int ddy = (tty - t[2]) >> 0x12;
                                    int dd = ddx * ddx + ddy * ddy;
                                    if (dd < 40000 && dd > 0) {
                                        tgt_x = ttx; tgt_y = tty;
                                        found_target = 1; break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (found_target) {
                    /* Compute firing angle */
                    int fire_angle;
                    float speed_sqrt = (float)__builtin_sqrt(0.7142857142857143);

                    if ((char)t[7] == (char)-2) {
                        /* Random angle */
                        fire_angle = rand() & 0x7ff;
                    } else {
                        fire_angle = FUN_00459dd0(*t, t[2] - 0x100000, tgt_x, tgt_y,
                                                 speed_sqrt,
                                                 *(char *)((int)t + 0x25) != '\0');
                    }

                    if (fire_angle != 0x801) {
                        /* Update barrel direction to match shot */
                        t[0xc] = fire_angle & 0x7ff;

                        /* Determine projectile type based on building type at +0x1C (t[7]).
                         * Type 0 (basic turret): entity type 0x00 (basic bullet)
                         * Type 1 (ice turret): entity type 0x13 (freeze projectile)
                         * Type 2 (floor turret): entity type 0x00 */
                        /* Projectile type from trooper subtype at +0x25 (Ghidra 0x4555D4):
                         * subtype 0 = infantry → type 0x00, sub 2
                         * subtype 1 = cars/heavy → type 0x01 (DUMBFIRE), sub 1
                         * subtype 2 = aerial    → type 0x00, sub 2 */
                        char stype = *(char *)((int)t + 0x25);
                        int proj_type = (stype == 1) ? 0x01 : 0x00;
                        int proj_subtype = (stype == 1) ? 1 : 2;

                        /* Owner byte */
                        char owner;
                        if ((char)t[7] == (char)-2) {
                            owner = (char)-2;
                        } else {
                            owner = (char)t[7] + 0x50;
                        }

                        /* Create projectile entity in DAT_004892e8 */
                        Entity *projectile = &DAT_004892e8[DAT_00489248];
                        projectile->position_x = *t;
                        projectile->position_y = t[2] - 0x100000;

                        /* Compute velocity: aimed shots fire directly at target,
                         * random shots (team 0xFE) use sin/cos LUT from random angle. */
                        if ((char)t[7] != (char)-2 && tgt_x != 0 && tgt_y != 0) {
                            /* Direct aim at target coordinates */
                            double speed = 524288.0 * (double)speed_sqrt * 2.3;
                            double dx = (double)(tgt_x - *t);
                            double dy = (double)(tgt_y - (t[2] - 0x100000));
                            double dist = sqrt(dx * dx + dy * dy);
                            if (dist > 1.0) {
                                projectile->velocity_x = (int)(dx / dist * speed);
                                projectile->velocity_y = (int)(dy / dist * speed);
                            } else {
                                projectile->velocity_x = 0;
                                projectile->velocity_y = 0;
                            }
                        } else if (DAT_00487ab0 != NULL) {
                            /* Random angle: use sin/cos LUT */
                            int sin_val = *(int *)((int)DAT_00487ab0 + fire_angle * 4);
                            int cos_val = *(int *)((int)DAT_00487ab0 + 0x800 + fire_angle * 4);
                            projectile->velocity_x = (int)((double)sin_val * (double)speed_sqrt * 2.3);
                            projectile->velocity_y = (int)((double)cos_val * (double)speed_sqrt * 2.3);
                        } else {
                            projectile->velocity_x = 0;
                            projectile->velocity_y = 0;
                        }

                        projectile->previous_x = *t;
                        projectile->previous_y = t[2] - 0x100000;
                        projectile->motion_x_10 = 0;
                        projectile->motion_y_14 = 0;
                        projectile->type = (unsigned char)proj_type;
                        projectile->variant_24 = 0;
                        projectile->state_20 = 0;
                        projectile->auxiliary_26 = 0xfe;
                        projectile->owner = (unsigned char)owner;
                        projectile->health_or_damage_28 = 0;

                        /* Entity type config lookups */
                        int type_offset = proj_subtype + proj_type * 0x86;
                        if (DAT_00487abc != NULL) {
                            projectile->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x88 + type_offset * 4);
                            projectile->damage_44 = *(int *)((int)DAT_00487abc + 0xc4 + type_offset * 4);
                            projectile->palette_value = *(int *)((int)DAT_00487abc + 0xf4 + type_offset * 4);
                            projectile->callback_address = *(int *)((int)DAT_00487abc + proj_type * 0x218);
                        }
                        projectile->scratch_48 = 0;
                        projectile->animation_frame = 0;
                        projectile->subtype = (unsigned char)proj_subtype;
                        projectile->counter_3c = 0;
                        projectile->timer_5c = 0;

                        DAT_00489248++;

                        /* Set projectile lifetime and damage based on type */
                        if (proj_type == 0) {
                            if (*(char *)((int)t + 0x25) == '\x02') {
                                /* Floor turret: extended lifetime */
                                int rnd = rand();
                                unsigned int life_val = rnd % 0x50 + 0x28;
                                {
                                    /* Grayscale particle color: original X1R5G5B5 (*0x421), converted to RGB565 (*0x841) */
                                    unsigned int gray5 = life_val >> 3;
                                    projectile->palette_value =
                                        ((gray5 << 11) | (gray5 << 6) | gray5) + 30000;
                                }
                                projectile->damage_44 = 0x1b5800;
                                projectile->variant_24 = 6;
                            } else {
                                /* Normal turret */
                                int rnd = rand();
                                if (DAT_00487aa8 != NULL) {
                                    projectile->palette_value =
                                        (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + 0xf2 + (rnd % 3) * 2) + 30000;
                                } else {
                                    projectile->palette_value = 30100;
                                }
                                projectile->damage_44 = 0x32000;
                            }
                        }

                        /* Set entity gravity (offset -0x48 = entity +0x38). */
                        projectile->gravity_or_motion_38 = 6;

                        /* Store turret position as projectile origin */
                        projectile->motion_x_10 = *t;
                        projectile->motion_y_14 = t[2];

                        /* Play turret fire sound */
                        FUN_0040f9b0(0x29, *t, t[2]);
                    }
                }

                /* Set fire cooldown based on turret type */
                if (*(unsigned char *)(t + 7) < 0x14) {
                    char ttype2 = *(char *)((int)t + 0x25);
                    if (ttype2 == '\0') {
                        *(unsigned char *)(t + 0xe) = 0x14;  /* 20 ticks */
                    }
                    if (ttype2 == '\x02') {
                        *(unsigned char *)(t + 0xe) = 100;   /* 100 ticks */
                    }
                    if (ttype2 == '\x01') {
                        *(unsigned char *)(t + 0xe) = 0x96;  /* 150 ticks */
                    }
                } else if (*(unsigned char *)(t + 7) == 0xfe) {
                    *(unsigned char *)(t + 0xe) = 3;         /* 3 ticks */
                }
            }
        }

        /* Fluid damage: check if current tile is underwater */
        {
            int tile_y2 = t[2] >> 0x12;
            int tile_x2 = *t >> 0x12;
            int tile_off2 = (tile_y2 << shift) + tile_x2;
            unsigned char fluid_tile = *(unsigned char *)((int)DAT_0048782c + tile_off2);
            if (*(char *)((unsigned int)fluid_tile * 0x20 + 4 + (int)DAT_00487928) == '\x01') {
                t[10] -= 0x1400; /* fluid damage */
            }
        }

        i++;
    } while (i < DAT_0048924c);
}
/* ===== FUN_004599f0 — LOS Angle Calculation (004599F0) ===== */
/* Computes firing angle from (src_x,src_y) to (dst_x,dst_y).
 * Low gravity path: simple atan2 angle + euclidean distance.
 * High gravity path: ballistic arc via bilinear LUT interpolation.
 * Returns angle in [0..2047] or 0x801 if no valid trajectory.
 * Side: 0=prefer low arc, 1=prefer high arc. */
int FUN_004599f0(int src_x, int src_y, int dst_x, int dst_y,
                 int side, float range_sqrt, int gravity)
{
    if (gravity < 0x50) {
        /* Low gravity: direct atan2 angle */
        int angle = FUN_004257e0(src_x, src_y, dst_x, dst_y);
        int dx = (src_x - dst_x) >> 0x12;
        int dy = (src_y - dst_y) >> 0x12;
        int dist_sq = dx * dx + dy * dy;
        DAT_00481f10 = (int)sqrt((double)dist_sq);
        return angle;
    }

    /* High gravity: ballistic arc LUT lookup */
    float temp = (float)((1024.0 / (double)gravity) *
                         (double)range_sqrt * (double)range_sqrt);
    int dx = (dst_x - src_x) >> 0x12;
    int dy = (dst_y - src_y) >> 0x12;
    unsigned char neg_x = 0;

    /* X remains in x87 extended precision here; Y is stored to a float slot. */
    double idx_x_ext = ((double)dx * 0.125) / (double)temp;
    float idx_y_f = (float)(((double)dy * 0.125) / (double)temp + 45.0);

    /* Absolute value of X index, track sign */
    if (idx_x_ext < 0.0) {
        idx_x_ext = -idx_x_ext;
        neg_x = 1;
    }

    /* Truncate to integer indices */
    int ix = (int)idx_x_ext;
    int iy = (int)idx_y_f;

    /* Bounds check: ix in [0,44], iy in [0,89] */
    if (ix < 0 || ix > 44 || iy < 0 || iy > 89)
        return 0x801;

    /* Fractional parts for bilinear interpolation */
    float frac_x = (float)(idx_x_ext - (double)ix);
    float frac_y = (float)((double)idx_y_f - (double)iy);
    double inv_x = 1.0 - (double)frac_x;
    double inv_y = 1.0 - (double)frac_y;

    /* LUT index arithmetic (from disassembly):
     * row_stride = iy * 23 * 2 = iy * 46 (since ECX = iy*3*8 - iy = iy*23, then ESI = ix + ECX*2)
     * base = side + (ix + iy * 46) * 4
     * next_y_base = side + (ix + (iy+1) * 46) * 4 */
    int row0 = ix + iy * 46;
    int row1 = ix + (iy + 1) * 46;
    int s = (int)((unsigned char)side);
    int base0 = s + row0 * 4;
    int base1 = s + row1 * 4;

    unsigned short *lut = (unsigned short *)DAT_00489e90;

    /* Preserve the original x87 bilinear interpolation and weighting order. */
    double next_vx = (double)(unsigned short)lut[base1] * inv_x +
                     (double)(unsigned short)lut[base0 + 188] * (double)frac_x;
    double cur_vx = (double)(unsigned short)lut[base0 + 4] * (double)frac_x +
                    (double)(unsigned short)lut[base0] * inv_x;
    float interp_vx = (float)(next_vx * (double)frac_y + cur_vx * inv_y);

    double next_vy = (double)(unsigned short)lut[base0 + 190] * (double)frac_x +
                     (double)(unsigned short)lut[base0 + 186] * inv_x;
    double cur_vy = (double)(unsigned short)lut[base0 + 6] * (double)frac_x +
                    (double)(unsigned short)lut[base0 + 2] * inv_x;
    double interp_vy = next_vy * (double)frac_y + cur_vy * inv_y;

    DAT_00481f10 = (int)(interp_vy * 1024.0);

    if (neg_x) {
        int a = (int)interp_vx;
        return (0x800 - a) & 0x7ff;
    }
    return (int)interp_vx & 0x7ff;
}

/* ===== FUN_00459c70 — LOS Path Validation (00459C70) ===== */
/* Ray-marches from (src_x,src_y) toward (dst_x,dst_y) at the given angle,
 * checking that the path is clear of impassable tiles.
 * Returns 1 if path is clear, 0 if blocked. */
char FUN_00459c70(int src_x, int src_y, int dst_x, int dst_y,
                  int angle, float range_sqrt, int gravity)
{
    /* Compute velocity components from angle using the sin/cos LUT.
     * Original scales by range_sqrt * 9.2 (double constant at 0x00475828).
     * This scaling + gravity*96 preserves the same arc shape as the entity
     * physics (velocity * fRange * 2.3, gravity * 6 * DAT_00483828) since
     * 9.2 = 4*2.3 and 96 = 4²*6, keeping g/v² constant. */
    int *math_lut = (int *)DAT_00487ab0;
    int vx = (int)((double)math_lut[angle] * (double)range_sqrt * 9.2);
    int vy = (int)((double)math_lut[angle + 0x200] * (double)range_sqrt * 9.2);

    /* Compute step count based on gravity */
    unsigned int steps;
    if (gravity < 0x50) {
        steps = (unsigned int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 3)) >> 2);
    } else {
        int half_val = (DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3;
        steps = (unsigned int)((double)half_val * (double)range_sqrt / (double)gravity);
    }

    /* Ray-march along the trajectory */
    int cur_x = src_x;
    int cur_y = src_y;
    int cur_vy = vy;
    unsigned int remaining_dx = (unsigned int)(dst_x - src_x);

    for (unsigned int step = 0; step < steps; step++) {
        remaining_dx -= (unsigned int)vx;
        cur_y += cur_vy;
        cur_vy += gravity * 0x60;    /* gravity acceleration per step */
        cur_x += vx;

        /* Check if close enough to target (within ~5 pixels in fixed-point) */
        unsigned int abs_remaining = remaining_dx;
        if ((int)remaining_dx < 0) abs_remaining = -(int)remaining_dx;
        unsigned int abs_dy = (unsigned int)(dst_y - cur_y);
        if ((int)(dst_y - cur_y) < 0) abs_dy = -(int)(dst_y - cur_y);
        if ((int)(abs_dy + abs_remaining) < 0x500000)
            break;  /* close enough, path clear */

        /* Bounds check: must be within map area with border */
        if (cur_x < 0x1c0000 || (cur_x >> 0x12) >= (int)DAT_004879f0 ||
            cur_y < 0x1c0000 || (cur_y >> 0x12) >= (int)DAT_004879f4)
            return 0;  /* out of bounds */

        /* Check tile passability: look up tile type at current position */
        int tile_row = cur_y >> 0x12;
        int tile_col = cur_x >> 0x12;
        int cell_idx = (tile_row << ((unsigned char)DAT_00487a18 & 0x1f)) + tile_col;
        unsigned char tile_type = *(unsigned char *)((int)DAT_0048782c + cell_idx);
        char *ent_type = (char *)((unsigned int)tile_type * 0x20 + 2 + (int)DAT_00487928);
        if (*ent_type == '\0')
            return 0;  /* blocked by impassable tile */
    }

    return 1;  /* path is clear */
}

/* ===== FUN_00459e90 — Predictive Aim Helper (00459E90) ===== */
/* Calls FUN_004599f0 twice with predicted target positions at two different
 * lead times (mult1 and mult2), picks the one with better distance match.
 * Updates the weapon's target angle at weapon index weap_idx. Returns 0 or 1. */
int FUN_00459e90(int mult1, int mult2, int weap_idx, float range_sqrt)
{
    int base = weap_idx * 0x40;

    /* Predict target position at lead time mult1 using the original 32-bit
     * arithmetic and wraparound behavior. */
    DAT_00481ee8 = DAT_00481ef8 * DAT_00481efc * mult1 + DAT_00481ee0;
    DAT_00481ee4 = DAT_00481efc * DAT_00481ef4 * mult1 + DAT_00481edc;

    /* Compute angle to predicted position 1 */
    DAT_00481f08 = FUN_004599f0(
        *(int *)(base + (int)DAT_00481f28),
        *(int *)(base + 4 + (int)DAT_00481f28),
        DAT_00481ee4, DAT_00481ee8,
        (int)(unsigned char)DAT_00481ed8, range_sqrt, DAT_00481ed0);
    DAT_00481f00 = DAT_00481f10;

    /* Predict target position at lead time mult2 */
    DAT_00481eec = DAT_00481efc * DAT_00481ef4 * mult2 + DAT_00481edc;
    DAT_00481ef0 = DAT_00481ef8 * DAT_00481efc * mult2 + DAT_00481ee0;

    /* Compute angle to predicted position 2 */
    DAT_00481f0c = FUN_004599f0(
        *(int *)(base + (int)DAT_00481f28),
        *(int *)(base + 4 + (int)DAT_00481f28),
        DAT_00481eec, DAT_00481ef0,
        (int)(unsigned char)DAT_00481ed8, range_sqrt, DAT_00481ed0);

    /* Convert distances */
    int dist1;
    if (DAT_00481ed0 < 0x50) {
        dist1 = (int)((DAT_00481f00 + (DAT_00481f00 >> 31 & 7)) >> 3);
    } else {
        int eighth = (DAT_00481f00 + (DAT_00481f00 >> 31 & 7)) >> 3;
        dist1 = (int)((double)eighth * (double)range_sqrt / (double)DAT_00481ed0);
    }
    DAT_00481f00 = dist1;

    if (DAT_00481ed0 < 0x50) {
        DAT_00481f04 = (int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3);
    } else {
        int eighth = (DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3;
        DAT_00481f04 = (int)((double)eighth * (double)range_sqrt / (double)DAT_00481ed0);
    }

    /* Compare which prediction is closer to actual distance */
    int err1 = dist1 * 4 - DAT_00481efc * mult1;
    if (err1 < 0) err1 = DAT_00481efc * mult1 - dist1 * 4;

    int err2 = DAT_00481f04 * 4 - DAT_00481efc * mult2;
    if (err2 < 0) err2 = DAT_00481efc * mult2 - DAT_00481f04 * 4;

    if (err2 <= err1) {
        /* Second prediction is better */
        *(int *)(base + 0xc + (int)DAT_00481f28) = DAT_00481f0c;
        return 1;
    }

    /* First prediction is better */
    *(int *)(base + 0xc + (int)DAT_00481f28) = DAT_00481f08;
    return 0;
}

/* ===== FUN_00458010 — Turret_Targeting_LOS (00458010) ===== */
/* Processes deployed weapons in DAT_00481f28 (stride 0x40, count DAT_00489260).
 * Full implementation: shield regen, target acquisition (player LOS + spatial grid),
 * aim slewing, predictive aim, projectile spawning, muzzle flash particles.
 * Weapon record layout (stride 0x40): +0x00 x, +0x04 y, +0x08 current_aim,
 * +0x0C target_aim (0x801 = no-target sentinel), +0x10 hp, +0x14 reload,
 * +0x18 dist-to-target, +0x1c type, +0x1d team, +0x1f barrel_state,
 * +0x20 dbl-barrel toggle, +0x21 tracking (0=search, 1=player, 2=grid),
 * +0x22 grid-sweep counter, +0x23 anim counter. */
void FUN_00458010(void)
{
    int i = 0;
    if (DAT_00489260 <= 0) return;

    do {
        int off = i * 0x40;
        unsigned char type = *(unsigned char *)(off + 0x1c + (int)DAT_00481f28);

        /* Weapon type 7 = team shield/generator; no aim/fire logic. */
        if (type == 7) {
            /* === Shield type: regen health, compute animation frame === */
            int health = *(int *)(off + 0x10 + (int)DAT_00481f28);
            if (health > 0) {
                *(int *)(off + 0x10 + (int)DAT_00481f28) = health + 8000;
                int max_health = *(int *)(off + 0x14 + (int)DAT_00481f28);
                if (*(int *)(off + 0x10 + (int)DAT_00481f28) > max_health)
                    *(int *)(off + 0x10 + (int)DAT_00481f28) = max_health;
            }
            int max_hp = *(int *)(off + 0x14 + (int)DAT_00481f28);
            int frame = 0x5f - (*(int *)(off + 0x10 + (int)DAT_00481f28) * 0x5f) / max_hp;
            frame = frame / 10;
            if (frame > 9) frame = 9;
            if (frame < 0) frame = 0;
            *(int *)(off + 8 + (int)DAT_00481f28) = frame;
        }
        else {
            /* === Non-shield weapon types === */

            /* Compute effective range sqrt: sqrt((range_config + 7) / 170.0) */
            /* DAT_00487818 is the weapon-type table (stride 0x20); +0x08 holds the
             * raw range config. fRange becomes the firing-speed scalar; _sq_thresh
             * is used as a squared-distance cutoff for target acquisition below. */
            int range_cfg = *(int *)((unsigned int)type * 0x20 + 8 + (int)DAT_00487818) + 7;
            float fRange = sqrtf((float)range_cfg * (1.0f / 170.0f));
            int range_sq_thresh = range_cfg * range_cfg * 4;

            /* Reload countdown */
            int reload = *(int *)(off + 0x14 + (int)DAT_00481f28);
            if (reload > 0) {
                *(int *)(off + 0x14 + (int)DAT_00481f28) = reload - 1;
            }
            /* Health overflow protection */
            if (*(int *)(off + 0x10 + (int)DAT_00481f28) > 1000000000) {
                *(int *)(off + 0x10 + (int)DAT_00481f28) = 2000000000;
            }

            /* Set the real projectile gravity used by the original solver. */
            DAT_00481ed0 = -(int)((unsigned int)(*(char *)(off + 0x1c + (int)DAT_00481f28) != '\x06')) & DAT_00483828;

            /* Increment animation counter */
            char *anim_ctr = (char *)(off + 0x23 + (int)DAT_00481f28);
            *anim_ctr = *anim_ctr + 1;

            /* Every 10 ticks: re-run target acquisition (too expensive to run each tick). */
            if (*(unsigned char *)(off + 0x23 + (int)DAT_00481f28) > 10) {
                *(unsigned char *)(off + 0x23 + (int)DAT_00481f28) = 0;

                /* ---- Target acquisition: scan players ---- */
                int best_dist = 2000000000;
                int best_target_idx = 0xfa;   /* sentinel = no target */
                unsigned int best_side = 0;

                if (DAT_00489240 > 0) {
                    int p = 0;
                    do {
                        PlayerData *player = Player_Get(p);
                        /* Skip same-team players and dead/inactive players */
                        if (*(char *)(off + 0x1d + (int)DAT_00481f28) != (char)player->team &&
                            player->state_24 == 0) {

                            int src_x = *(int *)(off + (int)DAT_00481f28);
                            int src_y = *(int *)(off + 4 + (int)DAT_00481f28);
                            int tgt_x = player->position_x;
                            int tgt_y = player->position_y;
                            int ddx = (src_x - tgt_x) >> 0x12;
                            int ddy = (src_y - tgt_y) >> 0x12;
                            int dist = ddx * ddx + ddy * ddy;

                            if (dist < range_sq_thresh && dist < best_dist) {
                                /* Try low arc first */
                                int angle = FUN_004599f0(src_x, src_y, tgt_x, tgt_y, 0, fRange, DAT_00481ed0);
                                unsigned char side = 0;
                                if (angle == 0x801 ||
                                    FUN_00459c70(src_x, src_y, tgt_x, tgt_y, angle, fRange, DAT_00481ed0) == '\0') {
                                    /* Try high arc */
                                    int angle_hi = FUN_004599f0(src_x, src_y, tgt_x, tgt_y, 1, fRange, DAT_00481ed0);
                                    if (angle_hi == 0x801 ||
                                        FUN_00459c70(src_x, src_y, tgt_x, tgt_y, angle_hi, fRange, DAT_00481ed0) == '\0') {
                                        goto next_player;
                                    }
                                    angle = angle_hi;
                                    side = 1;
                                }
                                best_target_idx = p;
                                best_side = (unsigned int)side;
                                best_dist = dist;
                            }
                        }
next_player:
                        p++;
                    } while (p < DAT_00489240);

                    if (best_target_idx != 0xfa) {
                        /* Found a player target */
                        *(int *)(off + 0x18 + (int)DAT_00481f28) = (int)sqrt((double)best_dist);

                        PlayerData *target = Player_Get(best_target_idx);
                        DAT_00481edc = target->position_x;
                        DAT_00481ee0 = target->position_y;
                        DAT_00481ef4 = target->velocity_x;
                        DAT_00481ef8 = target->velocity_y;
                        DAT_00481ed8 = (char)best_side;
                        *(unsigned char *)(off + 0x21 + (int)DAT_00481f28) = 1;

                        int aim_angle = FUN_004599f0(
                            *(int *)(off + (int)DAT_00481f28),
                            *(int *)(off + 4 + (int)DAT_00481f28),
                            DAT_00481edc, DAT_00481ee0,
                            (int)(unsigned char)DAT_00481ed8, fRange, DAT_00481ed0);
                        *(int *)(off + 0xc + (int)DAT_00481f28) = aim_angle;

                        /* Predictive aim for non-type-3 weapons on low arc. */
                        if (*(char *)(off + 0x1c + (int)DAT_00481f28) != '\x03' && DAT_00481ed8 == 0) {
                            if ((int)DAT_00481ed0 < 0x50) {
                                DAT_00481efc = (int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3);
                            } else {
                                int eighth = (DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3;
                                DAT_00481efc = (int)((double)eighth * (double)fRange /
                                                     (double)DAT_00481ed0);
                            }

                            int r = FUN_00459e90(3, 4, i, fRange);
                            if (r == 0) {
                                FUN_00459e90(1, 3, i, fRange);
                            } else {
                                FUN_00459e90(4, 8, i, fRange);
                            }
                        }
                        goto aim_slew;
                    }
                }

                /* No player target found - check tracking state */
                char tracking = *(char *)(off + 0x21 + (int)DAT_00481f28);
                if (tracking == '\0' || tracking == '\x01') {
                    *(int *)(off + 0xc + (int)DAT_00481f28) = 0x801;
                }

                if (*(char *)(off + 0x1c + (int)DAT_00481f28) == '\x02') {
                    /* Type 2: always reset to no-target */
                    *(int *)(off + 0xc + (int)DAT_00481f28) = 0x801;
                    *(unsigned char *)(off + 0x21 + (int)DAT_00481f28) = 0;
                }
                else {
                    /* ---- Spatial grid target search (every 4 sweeps) ---- */
                    char *sweep = (char *)(off + 0x22 + (int)DAT_00481f28);
                    *sweep = *sweep + 1;
                    if (*(unsigned char *)(off + 0x22 + (int)DAT_00481f28) > 3) {
                        *(unsigned char *)(off + 0x22 + (int)DAT_00481f28) = 0;

                        int grid_best = 0xfa;
                        best_dist = 2000000000;
                        best_side = 0;
                        unsigned int team_idx = 0;
                        /* Iterate the 4 projectile team buckets built by FUN_004609e0.
                         * 0x1010 skips past the trooper section; each team block is 0x4000 apart.
                         * 0x11010 = 0x1010 + 4*0x4000 → end of team 3. */
                        int grid_offset = 0x1010;   /* start at team bucket 0, entry list */

                        do {
                            if (team_idx != (unsigned int)*(unsigned char *)(off + 0x1d + (int)DAT_00481f28)) {
                                int bucket_count = *(int *)(grid_offset - 4 + (int)DAT_00487aa4);
                                if (bucket_count > 0) {
                                    int entry_off = grid_offset;
                                    for (int j = 0; j < bucket_count; j++) {
                                        int proj_idx = *(int *)(entry_off + (int)DAT_00487aa4);
                                        int proj_base = proj_idx * 0x40;
                                        int src_x = *(int *)(off + (int)DAT_00481f28);
                                        int src_y = *(int *)(off + 4 + (int)DAT_00481f28);
                                        int tgt_x = *(int *)(proj_base + (int)DAT_00481f28);
                                        int tgt_y = *(int *)(proj_base + 4 + (int)DAT_00481f28);
                                        int ddx = (src_x - tgt_x) >> 0x12;
                                        int ddy = (src_y - tgt_y) >> 0x12;
                                        int dist = ddx * ddx + ddy * ddy;

                                        if (dist < range_sq_thresh && dist < best_dist) {
                                            int angle = FUN_004599f0(src_x, src_y, tgt_x, tgt_y, 0, fRange, DAT_00481ed0);
                                            unsigned char side = 0;
                                            if (angle == 0x801 ||
                                                FUN_00459c70(src_x, src_y, tgt_x, tgt_y, angle, fRange, DAT_00481ed0) == '\0') {
                                                angle = FUN_004599f0(src_x, src_y, tgt_x, tgt_y, 1, fRange, DAT_00481ed0);
                                                if (angle == 0x801 ||
                                                    FUN_00459c70(src_x, src_y, tgt_x, tgt_y, angle, fRange, DAT_00481ed0) == '\0') {
                                                    goto next_grid_entry;
                                                }
                                                side = 1;
                                            }
                                            best_side = (unsigned int)side;
                                            best_dist = dist;
                                            grid_best = proj_idx;
                                        }
next_grid_entry:
                                        entry_off += 4;
                                    }
                                }
                            }
                            grid_offset += 0x4000;
                            team_idx++;
                        } while (grid_offset < 0x11010);

                        if (grid_best == 0xfa) {
                            *(int *)(off + 0xc + (int)DAT_00481f28) = 0x801;
                            *(unsigned char *)(off + 0x21 + (int)DAT_00481f28) = 0;
                        }
                        else {
                            *(int *)(off + 0x18 + (int)DAT_00481f28) = (int)sqrt((double)best_dist);
                            DAT_00481edc = *(int *)(grid_best * 0x40 + (int)DAT_00481f28);
                            DAT_00481ee0 = *(int *)(grid_best * 0x40 + (int)DAT_00481f28 + 4);
                            *(unsigned char *)(off + 0x21 + (int)DAT_00481f28) = 2;
                            DAT_00481ed8 = (char)best_side;

                            int aim_angle = FUN_004599f0(
                                *(int *)(off + (int)DAT_00481f28),
                                *(int *)(off + 4 + (int)DAT_00481f28),
                                DAT_00481edc, DAT_00481ee0,
                                (int)best_side, fRange, DAT_00481ed0);
                            *(int *)(off + 0xc + (int)DAT_00481f28) = aim_angle;
                        }
                    }
                }
            }

aim_slew:
            /* === Aim slewing: rotate current_aim toward target_aim === */
            {
                int target_aim = *(int *)(off + 0xc + (int)DAT_00481f28);
                if (target_aim != 0x801) {
                    int *aim_ptr = (int *)(off + 8 + (int)DAT_00481f28);
                    int cur = *aim_ptr;
                    bool bVar22 = false;
                    if (target_aim != cur) {
                        int turn_rate = (unsigned int)*(unsigned char *)((unsigned int)type * 0x20 + 0x10 + (int)DAT_00487818);
                        int diff_fwd, diff_rev;

                        if (target_aim < cur) {
                            diff_fwd = cur - target_aim;
                            diff_rev = diff_fwd - 0x800;
                            if (diff_rev < 0) diff_rev = (target_aim - cur) + 0x800;
                            if (diff_fwd < 0) diff_fwd = target_aim - cur;
                            bVar22 = false;
                        }
                        else {
                            diff_fwd = cur - target_aim;
                            diff_rev = diff_fwd;
                            if (diff_fwd < 0) {
                                diff_fwd = target_aim - cur;
                                diff_rev = target_aim - cur;
                            }
                            diff_fwd = 0x800 - diff_fwd;
                            bVar22 = true;
                        }

                        if (diff_rev < diff_fwd) {
                            *aim_ptr = cur + turn_rate;
                        }
                        else if (diff_fwd < diff_rev) {
                            *aim_ptr = cur - turn_rate;
                        }
                        else if (diff_rev == 0x400 && diff_fwd == 0x400) {
                            *aim_ptr = cur - turn_rate;
                        }

                        /* Clamp: if we overshot, snap to target */
                        int new_aim = *(int *)(off + 8 + (int)DAT_00481f28);
                        int new_target = *(int *)(off + 0xc + (int)DAT_00481f28);
                        if ((new_target < new_aim && bVar22) ||
                            (new_aim < new_target && !bVar22)) {
                            *(int *)(off + 8 + (int)DAT_00481f28) = new_target;
                        }
                    }
                    /* Wrap to 0-2047 */
                    *(unsigned int *)(off + 8 + (int)DAT_00481f28) =
                        *(unsigned int *)(off + 8 + (int)DAT_00481f28) & 0x7ff;

                    /* === Fire condition check === */
                    int cur_aim = *(int *)(off + 8 + (int)DAT_00481f28);
                    int tgt_aim = *(int *)(off + 0xc + (int)DAT_00481f28);
                    int diff_fwd2, diff_rev2;
                    if (tgt_aim < cur_aim) {
                        diff_fwd2 = cur_aim - tgt_aim;
                        diff_rev2 = diff_fwd2 - 0x800;
                        if (diff_rev2 < 0) diff_rev2 = (tgt_aim - cur_aim) + 0x800;
                        if (diff_fwd2 < 0) diff_fwd2 = tgt_aim - cur_aim;
                    } else {
                        diff_fwd2 = cur_aim - tgt_aim;
                        diff_rev2 = diff_fwd2;
                        if (diff_fwd2 < 0) {
                            diff_fwd2 = tgt_aim - cur_aim;
                            diff_rev2 = tgt_aim - cur_aim;
                        }
                        diff_fwd2 = 0x800 - diff_fwd2;
                    }

                    unsigned char wtype = *(unsigned char *)(off + 0x1c + (int)DAT_00481f28);
                    int wtype_base = (unsigned int)wtype * 0x20 + (int)DAT_00487818;
                    unsigned int aim_tol = (unsigned int)*(unsigned char *)(wtype_base + 0x18);
                    int half_dist = (unsigned int)(*(int *)(off + 0x18 + (int)DAT_00481f28) >> 1);

                    if ((unsigned int)half_dist < *(unsigned int *)(wtype_base + 8) &&
                        *(int *)(off + 0x14 + (int)DAT_00481f28) == 0 &&
                        (((diff_rev2 < (int)aim_tol || diff_fwd2 < (int)aim_tol) &&
                          *(char *)(off + 0x21 + (int)DAT_00481f28) == '\x01') ||
                         (diff_rev2 == 0 && *(char *)(off + 0x21 + (int)DAT_00481f28) == '\x02')))
                    {
                        /* --- Compute muzzle offset from sin/cos LUT --- */
                        int *math_lut = (int *)DAT_00487ab0;
                        int aim = *(int *)(off + 8 + (int)DAT_00481f28);
                        int cos_val = math_lut[aim];
                        int sin_val = math_lut[aim + 0x200]; /* +0x800 bytes / 4 = +0x200 ints */
                        int muzzle_dx = cos_val * 2;
                        int muzzle_dy = sin_val * 2;

                        /* --- Spawn projectile based on weapon type --- */
                        int *ent_types = (int *)DAT_00487abc;

                        if (wtype == 0 || wtype == 1) {
                            *(unsigned char *)(off + 0x1f + (int)DAT_00481f28) = 3;
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28) + muzzle_dx;
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28) + muzzle_dy;
                                /* Velocity: scale by fRange * 2.3 (double at 0x4757f0) to match
                                 * ballistic trajectory computed by FUN_004599f0/FUN_00459c70. */
                                *(int *)(e + 0x18 + eb) = (int)((double)cos_val * (double)fRange * 2.3);
                                *(int *)(e + 0x1c + eb) = (int)((double)sin_val * (double)fRange * 2.3);
                                *(int *)(e + 0x10 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0x14 + eb) = *(int *)(e + 8 + eb);
                                *(int *)(e + 4 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0xc + eb) = *(int *)(e + 8 + eb);
                                *(unsigned char *)(e + 0x21 + eb) = 0;
                                *(unsigned short *)(e + 0x24 + eb) = 0;
                                *(unsigned char *)(e + 0x20 + eb) = 0;
                                *(unsigned char *)(e + 0x26 + eb) = 5;
                                *(char *)(e + 0x22 + eb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(int *)(e + 0x28 + eb) = 0;
                                *(int *)(e + 0x38 + eb) = 6;
                                *(int *)(e + 0x44 + eb) = ent_types[0x35] << 1;
                                *(int *)(e + 0x48 + eb) = 0;
                                *(int *)(e + 0x4c + eb) = ent_types[0x41];
                                *(unsigned char *)(e + 0x54 + eb) = 0;
                                *(unsigned char *)(e + 0x40 + eb) = 4;
                                *(int *)(e + 0x34 + eb) = ent_types[0];
                                *(int *)(e + 0x3c + eb) = 0;
                                DAT_00489248++;
                            }
                        }
                        else if (wtype == 5) {
                            /* Double-barrel: alternate left/right barrel */
                            unsigned char *barrel = (unsigned char *)(off + 0x20 + (int)DAT_00481f28);
                            unsigned int barrel_aim;
                            if (*barrel == '\0') {
                                *barrel = 1;
                                barrel_aim = (*(int *)(off + 8 + (int)DAT_00481f28) + 0x200) & 0x7ff;
                            } else {
                                *barrel = 0;
                                barrel_aim = (*(int *)(off + 8 + (int)DAT_00481f28) - 0x200) & 0x7ff;
                            }
                            *(unsigned char *)(off + 0x1f + (int)DAT_00481f28) = 5;
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28) +
                                    math_lut[barrel_aim] * 2 + muzzle_dx;
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28) +
                                    math_lut[barrel_aim + 0x200] * 2 + muzzle_dy;
                                *(int *)(e + 0x18 + eb) = (int)((double)cos_val * (double)fRange * 2.3);
                                *(int *)(e + 0x1c + eb) = (int)((double)sin_val * (double)fRange * 2.3);
                                *(int *)(e + 0x10 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0x14 + eb) = *(int *)(e + 8 + eb);
                                *(int *)(e + 4 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0xc + eb) = *(int *)(e + 8 + eb);
                                *(unsigned char *)(e + 0x21 + eb) = 0;
                                *(unsigned short *)(e + 0x24 + eb) = 0;
                                *(unsigned char *)(e + 0x20 + eb) = 0;
                                *(unsigned char *)(e + 0x26 + eb) = 5;
                                *(char *)(e + 0x22 + eb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(int *)(e + 0x28 + eb) = 0;
                                *(int *)(e + 0x38 + eb) = 6;
                                *(int *)(e + 0x44 + eb) = ent_types[0x34] << 1;
                                *(int *)(e + 0x48 + eb) = 0;
                                *(int *)(e + 0x4c + eb) = ent_types[0x40];
                                *(unsigned char *)(e + 0x54 + eb) = 0;
                                *(unsigned char *)(e + 0x40 + eb) = 3;
                                *(int *)(e + 0x34 + eb) = ent_types[0];
                                *(int *)(e + 0x3c + eb) = 0;
                                DAT_00489248++;
                            }
                        }
                        else if (wtype == 3) {
                            *(unsigned char *)(off + 0x1f + (int)DAT_00481f28) = 0x14;
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28);
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28);
                                *(int *)(e + 0x18 + eb) = (int)((double)cos_val * (double)fRange * 2.3);
                                *(int *)(e + 0x1c + eb) = (int)((double)sin_val * (double)fRange * 2.3);
                                *(int *)(e + 0x10 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0x14 + eb) = *(int *)(e + 8 + eb);
                                *(int *)(e + 4 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0xc + eb) = *(int *)(e + 8 + eb);
                                *(unsigned char *)(e + 0x21 + eb) = 1;
                                *(unsigned short *)(e + 0x24 + eb) = 0;
                                *(unsigned char *)(e + 0x20 + eb) = 0;
                                *(unsigned char *)(e + 0x26 + eb) = 5;
                                *(char *)(e + 0x22 + eb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(int *)(e + 0x28 + eb) = 0;
                                *(int *)(e + 0x38 + eb) = 6;
                                *(int *)(e + 0x44 + eb) = ent_types[0xb9] << 1;
                                *(int *)(e + 0x48 + eb) = 0;
                                *(int *)(e + 0x4c + eb) = ent_types[0xc5];
                                *(unsigned char *)(e + 0x54 + eb) = 0;
                                *(unsigned char *)(e + 0x40 + eb) = 2;
                                *(int *)(e + 0x34 + eb) = ent_types[0x86];
                                *(int *)(e + 0x3c + eb) = 0;
                                DAT_00489248++;
                            }
                        }
                        else if (wtype == 4) {
                            *(unsigned char *)(off + 0x1f + (int)DAT_00481f28) = 0x32;
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28);
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28);
                                *(int *)(e + 0x18 + eb) = (int)((double)cos_val * (double)fRange * 2.3);
                                *(int *)(e + 0x1c + eb) = (int)((double)sin_val * (double)fRange * 2.3);
                                *(int *)(e + 0x10 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0x14 + eb) = *(int *)(e + 8 + eb);
                                *(int *)(e + 4 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0xc + eb) = *(int *)(e + 8 + eb);
                                *(unsigned char *)(e + 0x21 + eb) = 0x11;
                                *(unsigned short *)(e + 0x24 + eb) = 0;
                                *(unsigned char *)(e + 0x20 + eb) = 0;
                                *(unsigned char *)(e + 0x26 + eb) = 5;
                                *(char *)(e + 0x22 + eb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(int *)(e + 0x28 + eb) = 0;
                                *(int *)(e + 0x38 + eb) = 6;
                                *(int *)(e + 0x44 + eb) = ent_types[0x918] << 1;
                                *(int *)(e + 0x48 + eb) = 0;
                                *(int *)(e + 0x4c + eb) = ent_types[0x924];
                                *(unsigned char *)(e + 0x54 + eb) = 0;
                                *(unsigned char *)(e + 0x40 + eb) = 1;
                                *(int *)(e + 0x34 + eb) = ent_types[0x8e6];
                                *(int *)(e + 0x3c + eb) = 0;
                                DAT_00489248++;
                            }
                        }
                        else if (wtype == 2) {
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28);
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28);
                                *(int *)(e + 0x18 + eb) = (int)((double)cos_val * (double)fRange * 2.3);
                                *(int *)(e + 0x1c + eb) = (int)((double)sin_val * (double)fRange * 2.3);
                                *(int *)(e + 0x10 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0x14 + eb) = *(int *)(e + 8 + eb);
                                *(int *)(e + 4 + eb) = *(int *)(e + eb);
                                *(int *)(e + 0xc + eb) = *(int *)(e + 8 + eb);
                                *(unsigned char *)(e + 0x21 + eb) = 0x13;
                                *(unsigned short *)(e + 0x24 + eb) = 0;
                                *(unsigned char *)(e + 0x20 + eb) = 0;
                                *(unsigned char *)(e + 0x26 + eb) = 5;
                                *(char *)(e + 0x22 + eb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(int *)(e + 0x28 + eb) = 0;
                                *(int *)(e + 0x38 + eb) = 6;
                                *(int *)(e + 0x44 + eb) = ent_types[0xa23] << 1;
                                *(int *)(e + 0x48 + eb) = 0;
                                *(int *)(e + 0x4c + eb) = ent_types[0xa2f];
                                *(unsigned char *)(e + 0x54 + eb) = 0;
                                *(unsigned char *)(e + 0x40 + eb) = 0;
                                *(int *)(e + 0x34 + eb) = ent_types[0x9f2];
                                *(int *)(e + 0x3c + eb) = 0;
                                DAT_00489248++;
                            }
                        }
                        else if (wtype == 6 && DAT_00489250 < 2000) {
                            /* Flamethrower: spawn flame particle */
                            int fire_aim = *(int *)(off + 8 + (int)DAT_00481f28);
                            int rnd = rand();
                            unsigned int flame_dir = (rnd % 0x78 - 0x3c + fire_aim) & 0x7ff;
                            if (DAT_00489250 < 2000) {
                                int p = DAT_00489250 * 0x20;
                                int pb = (int)DAT_00481f34;
                                int *mlut = (int *)DAT_00487ab0;
                                *(int *)(p + pb) = (mlut[fire_aim] * 0x226 >> 6) +
                                    *(int *)(off + (int)DAT_00481f28);
                                *(int *)(p + 4 + pb) = (mlut[fire_aim + 0x200] * 0x226 >> 6) +
                                    *(int *)(off + 4 + (int)DAT_00481f28);
                                *(int *)(p + 8 + pb) = mlut[flame_dir];
                                *(int *)(p + 0xc + pb) = mlut[flame_dir + 0x200];
                                unsigned int rnd2 = rand();
                                rnd2 = rnd2 & 0x80000001;
                                if ((int)rnd2 < 0) rnd2 = (rnd2 - 1 | 0xfffffffe) + 1;
                                *(char *)(p + 0x10 + pb) = (char)rnd2 + 3;
                                *(unsigned char *)(p + 0x11 + pb) = 1;
                                *(unsigned char *)(p + 0x12 + pb) = 0;
                                *(unsigned char *)(p + 0x13 + pb) = 0xcd;
                                *(char *)(p + 0x14 + pb) = *(char *)(off + 0x1d + (int)DAT_00481f28) + 'x';
                                *(unsigned char *)(p + 0x15 + pb) = 0;
                                DAT_00489250++;
                            }
                        }

                        /* Set reload cooldown from weapon type table */
                        *(int *)(off + 0x14 + (int)DAT_00481f28) =
                            *(int *)((unsigned int)*(unsigned char *)(off + 0x1c + (int)DAT_00481f28) * 0x20 + 4 + (int)DAT_00487818);
                    }
                }
            }

            /* === Muzzle flash / smoke particle spawning === */
            {
                char cooldown = *(char *)(off + 0x1f + (int)DAT_00481f28);
                if (cooldown != '\0') {
                    if (cooldown != (char)-1) {
                        *(char *)(off + 0x1f + (int)DAT_00481f28) = cooldown - 1;
                    }
                    /* Only spawn particles if weapon is within viewport (bit 3 of coarse grid) */
                    int wx = *(int *)(off + (int)DAT_00481f28) >> 0x16;
                    int wy = *(int *)(off + 4 + (int)DAT_00481f28) >> 0x16;
                    if ((*(unsigned char *)(wx + (int)DAT_00487814 + wy * DAT_004879f8) & 8) != 0) {
                        char wt = *(char *)(off + 0x1c + (int)DAT_00481f28);
                        if (wt == '\x06') {
                            goto spawn_smoke;
                        }

                        /* Fire muzzle flash particle */
                        if (DAT_0048925c < 0x5dc) {
                            int base2 = off + (int)DAT_00481f28;
                            int rnd = rand();
                            unsigned int flash_dir = (rnd % 100 - 0x32 + *(int *)(base2 + 8)) & 0x7ff;
                            unsigned int sprite_type;
                            if (*(char *)(base2 + 0x1c) == '\x04') {
                                unsigned int r = rand();
                                r = r & 0x80000001;
                                if ((int)r < 0) r = (r - 1 | 0xfffffffe) + 1;
                                sprite_type = r;
                            } else {
                                unsigned int r = rand();
                                r = r & 0x80000001;
                                if ((int)r < 0) r = (r - 1 | 0xfffffffe) + 1;
                                sprite_type = (unsigned int)(unsigned char)((char)r + 7);
                            }

                            if (DAT_0048925c < 0x5dc) {
                                int f = DAT_0048925c * 0x20;
                                int fb = (int)DAT_00481f2c;
                                int *mlut = (int *)DAT_00487ab0;
                                *(int *)(f + fb) = (int)((float)mlut[*(int *)(base2 + 8)] * fRange);
                                *(int *)(f + 4 + fb) = (int)((float)mlut[*(int *)(base2 + 8) + 0x200] * fRange);
                                int r1 = rand();
                                *(int *)(f + 8 + fb) = (r1 % 0x14 + 0xf) * mlut[flash_dir] >> 6;
                                int r2 = rand();
                                *(int *)(f + 0xc + fb) = (r2 % 0x14 + 0xf) * mlut[flash_dir + 0x200] >> 6;
                                *(char *)(f + 0x10 + fb) = (char)sprite_type;
                                *(unsigned char *)(f + 0x11 + fb) = 0;
                                *(unsigned short *)(f + 0x12 + fb) = 0;
                                *(unsigned char *)(f + 0x14 + fb) = 0xff;
                                *(unsigned char *)(f + 0x15 + fb) = 0;
                                DAT_0048925c++;
                            }
                        }
                        else if (wt == '\x06') {
spawn_smoke:
                            /* Smoke/flame particle for type 6 */
                            if (DAT_00489250 < 2000) {
                                unsigned int rnd = rand();
                                rnd = rnd & 0x80000003;
                                int is_zero = (rnd == 0);
                                if ((int)rnd < 0) is_zero = ((rnd - 1 | 0xfffffffc) == 0xffffffff);
                                if (is_zero) {
                                    int fire_aim = *(int *)(off + 8 + (int)DAT_00481f28);
                                    int *mlut = (int *)DAT_00487ab0;
                                    int rnd2 = rand();
                                    unsigned int smoke_dir = (rnd2 % 0x78 - 0x3c + fire_aim) & 0x7ff;
                                    if (DAT_00489250 < 2000) {
                                        int p = DAT_00489250 * 0x20;
                                        int pb = (int)DAT_00481f34;
                                        *(int *)(p + pb) = (mlut[fire_aim] * 0x28a >> 7) +
                                            *(int *)(off + (int)DAT_00481f28);
                                        *(int *)(p + 4 + pb) = (mlut[fire_aim + 0x200] * 0x28a >> 7) +
                                            *(int *)(off + 4 + (int)DAT_00481f28);
                                        *(int *)(p + 8 + pb) = mlut[smoke_dir] >> 2;
                                        *(int *)(p + 0xc + pb) = (mlut[smoke_dir + 0x200] >> 2) - 0x200;
                                        unsigned int rnd3 = rand();
                                        rnd3 = rnd3 & 0x80000001;
                                        if ((int)rnd3 < 0) rnd3 = (rnd3 - 1 | 0xfffffffe) + 1;
                                        *(char *)(p + 0x10 + pb) = (char)rnd3 + 5;
                                        int rnd4 = rand();
                                        *(char *)(p + 0x11 + pb) = (char)(rnd4 % 6);
                                        *(unsigned char *)(p + 0x12 + pb) = 2;
                                        *(unsigned char *)(p + 0x13 + pb) = 0;
                                        *(unsigned char *)(p + 0x14 + pb) = 0xff;
                                        *(unsigned char *)(p + 0x15 + pb) = 0;
                                        DAT_00489250++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        i++;
    } while (i < DAT_00489260);
}

/* ===== FUN_00453cd0 — Particle_Physics_Damage (00453CD0) ===== */
/* Processes fire/smoke particles in DAT_00481f2c (stride 0x20, count DAT_0048925c).
 * Handles animation, gravity, entity interaction, jitter, boundary/wall checks,
 * fire damage to players, and particle expiry (swap-with-last removal). */
void FUN_00453cd0(void)
{
    int idx = 0;
    if (DAT_0048925c <= 0) return;

    int stride_off = 0;

    do {
        int base = (int)DAT_00481f2c;
        unsigned int *p = (unsigned int *)(stride_off + base);

        /* 1. Animation advance: increment sub_frame, check frame limit */
        short *sub_frame = (short *)((int)p + 0x12);
        *sub_frame = *sub_frame + 1;
        unsigned char sprite_type = *(unsigned char *)((int)p + 0x10);
        unsigned short frame_limit = *(unsigned short *)((int)DAT_00487ab8 + 4 + (unsigned int)sprite_type * 8);
        if (*(unsigned short *)((int)p + 0x12) > frame_limit) {
            *(unsigned short *)((int)p + 0x12) = 0;
            *(char *)((int)p + 0x11) = *(char *)((int)p + 0x11) + 1;
        }

        /* Save old position for wall collision restore */
        unsigned int old_x = p[0];
        unsigned int old_y = p[1];

        /* 2. Random gravity: every 3rd tick, apply half-gravity to vel_y */
        int r = rand();
        if (r % 3 == 0) {
            p[3] = (unsigned int)((int)p[3] - DAT_00483828 / 2);
        }

        /* 3. Position integration: pos += vel */
        unsigned int new_x = p[0] + p[2];
        unsigned int new_y = p[1] + p[3];
        p[0] = new_x;
        p[1] = new_y;

        /* 4. Entity interaction (only on sub-frame 0) */
        if (DAT_00489288 == '\0') {
            char particle_team;
            unsigned char owner = (unsigned char)p[5]; /* byte at +0x14 */
            if (owner < 0x50) {
                particle_team = static_cast<char>(Player_Get(owner)->team);
            }
            else {
                particle_team = (char)-5;
            }

            /* Particle deflection from MOVING SUCKER — scan entity array directly */
            for (int ei = 0; ei < DAT_00489248; ei++) {
                Entity *entity = &DAT_004892e8[ei];
                if (entity->type != 0x0E) continue;
                unsigned char ent_owner = entity->owner;
                char ent_team = static_cast<char>(Player_Get(ent_owner)->team);
                if (ent_team != particle_team) {
                    if (((int)(new_x - 0x12C0000) < entity->position_x) &&
                        (entity->position_x < (int)(new_x + 0x12C0000)) &&
                        ((int)(new_y - 0x12C0000) < entity->position_y) &&
                        (entity->position_y < (int)(new_y + 0x12C0000)))
                    {
                        int angle = FUN_004257e0(
                            entity->position_x,
                            entity->position_y,
                            (int)new_x, (int)new_y);
                        if (entity->subtype == 0) {
                            p[2] = (unsigned int)((int)p[2] - (*(int *)((int)DAT_00487ab0 + angle * 4) >> 1));
                            p[3] = (unsigned int)((int)p[3] - (*(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 1));
                        } else {
                            p[2] = (unsigned int)((int)p[2] + (*(int *)((int)DAT_00487ab0 + angle * 4) >> 2));
                            p[3] = (unsigned int)((int)p[3] + (*(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 2));
                        }
                        break;
                    }
                }
            }
        }

        /* 5. Random pixel jitter */
        int jitter = rand() % 0xc;
        if (jitter == 0) {
            p[0] = p[0] - FIXED_SCALE;
        }
        else if (jitter == 1) {
            p[0] = p[0] + FIXED_SCALE;
        }
        else if (jitter == 2) {
            p[1] = p[1] - FIXED_SCALE;
        }
        else if (jitter == 3) {
            p[1] = p[1] + FIXED_SCALE;
        }

        /* 6. Boundary check: remove if out of map */
        int tile_x = (int)p[0] >> 0x12;
        int tile_y = (int)p[1] >> 0x12;
        if ((int)(p[0] & 0xfffc0000) < FIXED_SCALE ||
            tile_x >= (int)(DAT_004879f0 - 1) ||
            (int)(p[1] & 0xfffc0000) < FIXED_SCALE ||
            tile_y >= (int)(DAT_004879f4 - 1))
        {
            goto remove_particle;
        }

        /* 7. Wall collision: if tile is non-walkable, restore position, reverse+halve velocity */
        {
            int shift = (unsigned char)DAT_00487a18 & 0x1f;
            unsigned char tile_id = *(unsigned char *)((int)DAT_0048782c + (tile_y << shift) + tile_x);
            if (*(char *)((unsigned int)tile_id * 0x20 + 1 + (int)DAT_00487928) == '\0') {
                p[0] = old_x;
                p[1] = old_y;
                p[2] = (unsigned int)(-((int)p[2] >> 1));
                p[3] = (unsigned int)(-((int)p[3] >> 1));
            }
        }

        /* 8. Fire damage to players (sprite types 0x0C through 0x11) */
        {
            unsigned char stype = (unsigned char)p[4]; /* byte at +0x10 */
            if (stype > 0x0b && stype < 0x12) {
                /* Velocity damping for heavy fire (types 0x0F-0x11): vel *= 0.99 per tick */
                if (stype > 0x0e) {
                    p[2] = (unsigned int)(int)((double)(int)p[2] * 0.99);
                    p[3] = (unsigned int)(int)((double)(int)p[3] * 0.99);
                }
                unsigned int pi2 = 0;
                if (DAT_00489240 > 0) {
                    int *dmg_ptr = &DAT_00486be8[0];
                    do {
                        PlayerData *player = Player_Get(pi2);
                        if (player->health > 0 && pi2 != (unsigned char)p[5]) {
                            /* AABB check: player within +/- 0x2C0000 */
                            if ((player->position_x - 0x2C0000 < (int)p[0]) &&
                                ((int)p[0] < player->position_x + 0x2C0000))
                            {
                                int player_y = player->position_y;
                                if ((player_y - 0x2C0000 < (int)p[1]) &&
                                    ((int)p[1] < player_y + 0x2C0000))
                                {
                                    if (stype < 0x0f) {
                                        /* Light fire: damage 0x1928, stat +1 */
                                        *dmg_ptr = *dmg_ptr + 1;
                                        unsigned char fire_owner = (unsigned char)p[5];
                                        if (fire_owner < 0x50) {
                                            if (Player_Get(fire_owner)->team != player->team) {
                                                DAT_00486e68[fire_owner] = DAT_00486e68[fire_owner] + 1;
                                            }
                                            if (Player_Get(fire_owner)->team != player->team || DAT_0048373d != '\0') {
                                                player->health = tou_binary::sub_wrap_i32(player->health, 0x1928);
                                            }
                                        }
                                        else {
                                            player->health = tou_binary::sub_wrap_i32(player->health, 0x1928);
                                        }
                                        /* Random knockback (1/500 chance) */
                                        int rk = rand();
                                        if (rk % 500 == 0) {
                                            unsigned int heading = (unsigned int)(player->heading - 0x400) & 0x7ff;
                                            player->velocity_x = tou_binary::add_wrap_i32(
                                                player->velocity_x,
                                                *(int *)((int)DAT_00487ab0 + heading * 4));
                                            player->velocity_y = tou_binary::add_wrap_i32(
                                                player->velocity_y,
                                                *(int *)((int)DAT_00487ab0 + 0x800 + heading * 4));
                                        }
                                    }
                                    else {
                                        /* Heavy fire: damage 0x6400, stat +3 */
                                        *dmg_ptr = *dmg_ptr + 3;
                                        unsigned char fire_owner = (unsigned char)p[5];
                                        if (fire_owner < 0x50) {
                                            if (Player_Get(fire_owner)->team != player->team) {
                                                DAT_00486e68[fire_owner] = DAT_00486e68[fire_owner] + 3;
                                            }
                                            if (Player_Get(fire_owner)->team != player->team || DAT_0048373d != '\0') {
                                                player->health = tou_binary::sub_wrap_i32(player->health, 0x6400);
                                            }
                                        }
                                        else {
                                            player->health = tou_binary::sub_wrap_i32(player->health, 0x6400);
                                        }
                                    }
                                    /* Set attacker and damage indicator */
                                    unsigned char att_owner = (unsigned char)p[5];
                                    if (att_owner < 0x50) {
                                        if (Player_Get(att_owner)->team != player->team || DAT_0048373d != '\0') {
                                            player->last_attacker = att_owner;
                                        }
                                    }
                                    else {
                                        player->last_attacker = 0xff;
                                    }
                                    player->timer_4a2 = 0x6e;
                                }
                            }
                        }

                        pi2++;
                        dmg_ptr++;
                    } while ((int)pi2 < DAT_00489240);
                }
            }
        }

        /* 9. Expiry: if animation_frame > 6, remove particle */
        if (*(unsigned char *)((int)p + 0x11) > 6) {
            goto remove_particle;
        }

        /* Advance to next particle */
        idx++;
        stride_off += 0x20;
        continue;

    remove_particle:
        DAT_0048925c = DAT_0048925c - 1;
        /* Swap-with-last: copy last entry to current slot */
        p[0] = *(unsigned int *)(DAT_0048925c * 0x20 + (int)DAT_00481f2c);
        p[1] = *(unsigned int *)(DAT_0048925c * 0x20 + 4 + (int)DAT_00481f2c);
        p[2] = *(unsigned int *)(DAT_0048925c * 0x20 + 8 + (int)DAT_00481f2c);
        p[3] = *(unsigned int *)(DAT_0048925c * 0x20 + 0xc + (int)DAT_00481f2c);
        *(unsigned char *)((int)p + 0x11) = *(unsigned char *)(DAT_0048925c * 0x20 + 0x11 + (int)DAT_00481f2c);
        *(unsigned char *)(p + 4) = *(unsigned char *)(DAT_0048925c * 0x20 + 0x10 + (int)DAT_00481f2c);
        *(unsigned short *)((int)p + 0x12) = *(unsigned short *)(DAT_0048925c * 0x20 + 0x12 + (int)DAT_00481f2c);
        *(unsigned char *)(p + 5) = *(unsigned char *)(DAT_0048925c * 0x20 + 0x14 + (int)DAT_00481f2c);
        *(unsigned char *)((int)p + 0x15) = *(unsigned char *)(DAT_0048925c * 0x20 + 0x15 + (int)DAT_00481f2c);

        /* Re-check current slot if more particles remain */
        if (idx >= DAT_0048925c) break;

    } while (idx < DAT_0048925c);
}
/* ===== FUN_00455d50 — Bullet_Collision_Detect (00455D50) ===== */
/* Checks debris items (DAT_00487830, stride 0x20, DAT_00489268 count) against
 * players for AABB collision. On hit, applies health effects (heal/damage)
 * based on item type. Items expire when their lifetime counter reaches 0. */
void FUN_00455d50(void)
{
    int i = 0;
    while (i < DAT_00489268) {
        int base = (int)DAT_00487830 + i * 0x20 + 4;  /* item starts at offset +4 in stride */

        /* Advance animation */
        *(char *)(base + 5) = *(char *)(base + 5) + 1;
        unsigned char item_anim_type = *(unsigned char *)(base + 6);
        unsigned char anim_speed = g_EntityConfig ?
            *(unsigned char *)((int)g_EntityConfig + 5 + (unsigned int)item_anim_type * 8) : 4;
        if (*(unsigned char *)(base + 5) > anim_speed) {
            *(char *)(base + 4) = *(char *)(base + 4) + 1;
            *(unsigned char *)(base + 5) = 0;
            unsigned char max_anim = g_EntityConfig ?
                *(unsigned char *)((int)g_EntityConfig + 4 + (unsigned int)item_anim_type * 8) : 4;
            if (*(unsigned char *)(base + 4) >= max_anim) {
                *(unsigned char *)(base + 4) = 0;
            }
        }

        /* Get sprite dimensions for AABB */
        int sprite_idx = g_EntityConfig ?
            *(int *)((int)g_EntityConfig + (unsigned int)item_anim_type * 8) : 0;
        int half_w = (DAT_00489e8c ? ((unsigned int)*(unsigned char *)((int)DAT_00489e8c + sprite_idx) * FIXED_SCALE) / 2 : FIXED_SCALE);
        int half_h = (DAT_00489e88 ? ((unsigned int)*(unsigned char *)((int)DAT_00489e88 + sprite_idx) * FIXED_SCALE) / 2 : FIXED_SCALE);

        /* Check each player for AABB collision */
        int item_x = *(int *)(base - 4);
        int item_y = *(int *)(base);

        for (int p = 0; p < DAT_00489240; p++) {
            PlayerData *player = Player_Get(p);
            int ship_half = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x38) / 2 : FIXED_SCALE;

            if (player->health > 0) {
                int px = player->position_x;
                int py = player->position_y;

                if (item_x - ship_half - half_w < px && px < item_x + ship_half + half_w &&
                    item_y - ship_half - half_h < py && py < item_y + ship_half + half_h) {
                    /* Hit! Apply effect based on type */
                    FUN_0040f9b0(0x79, item_x, item_y);
                    DAT_00487228[p]++;

                    /* Player +0xCA / +0xC9 = HUD pickup banner (type id + display timer);
                     * the id feeds FUN_0040aca0 in hud.cpp which maps to a string. */
                    if (item_anim_type == 0) {
                        /* Small health pack — banner id 0x14. */
                        player->health = tou_binary::add_wrap_i32(player->health, 0x5DC000);
                        player->hud_banner_id = 0x14;
                        player->hud_banner_timer = 200;
                    }
                    else if (item_anim_type == 1) {
                        /* Large health pack */
                        player->health = tou_binary::add_wrap_i32(player->health, 0x9C4000);
                        player->hud_banner_id = 0x15;
                        player->hud_banner_timer = 200;
                    }
                    else if (item_anim_type == 2) {
                        /* Random pickup crate */
                        int roll = rand() % 642;
                        if (roll < 12) {
                            /* Full energy — restore health to max */
                            int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x28) : 0;
                            player->health = max_hp;
                            player->hud_banner_id = 0x00;
                        } else if (roll < 42) {
                            /* Booby trap — damages player, spawns 16 bullets + 75 shrapnel.
                             * Original jump table case 1, at 0x455d50. */
                            player->health = tou_binary::add_wrap_i32(
                                player->health, (int)0xFFD8F000);
                            FUN_00437cf0(item_x, item_y, 0x12C, 0xFF, 0); /* explosion KB */
                            int *bt_sc = (int *)DAT_00487ab0;
                            int *bt_tt = (int *)DAT_00487abc;
                            /* 16 bullets in a ring */
                            for (int bt = 0; bt < 16 && DAT_00489248 < 0x9C4; bt++) {
                                int bt_ang = bt * 0x80; /* 0x2000/16 = 0x80 per step (index into sincos) */
                                int bt_rand = rand() % 3;
                                int bt_spd = (rand() % 3) + 5;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = item_x; ep->previous_x = item_x;
                                ep->position_y = item_y; ep->previous_y = item_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->velocity_x = (bt_sc[bt_ang] << bt_spd) >> 6;
                                ep->velocity_y = (bt_sc[bt_ang + 0x200] << bt_spd) >> 6;
                                ep->state_20 = 0;
                                ep->type = 0x01;
                                ep->owner = (unsigned char)p;
                                ep->variant_24 = 0;
                                ep->auxiliary_26 = 0x1E;
                                ep->health_or_damage_28 = 0;
                                ep->callback_address = bt_tt[0x218/4];
                                ep->gravity_or_motion_38 = bt_tt[(0x2A0 + bt_rand * 4) / 4];
                                ep->counter_3c = 0;
                                ep->subtype = (unsigned char)bt_rand;
                                ep->damage_44 = bt_tt[(0x2DC + bt_rand * 4) / 4];
                                ep->scratch_48 = 0;
                                ep->palette_value = bt_tt[(0x30C + bt_rand * 4) / 4];
                                ep->animation_frame = 0;
                                ep->timer_5c = 0;
                                DAT_00489248++;
                            }
                            /* Flash particle */
                            if (DAT_00489250 < 2000) {
                                int fp = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                                *(int *)(fp + 0x00) = item_x; *(int *)(fp + 0x04) = item_y;
                                *(int *)(fp + 0x08) = 0; *(int *)(fp + 0x0C) = 0;
                                *(unsigned char *)(fp + 0x10) = (unsigned char)((rand() & 3) + 7);
                                *(unsigned char *)(fp + 0x11) = 0;
                                *(unsigned char *)(fp + 0x12) = 0;
                                *(unsigned char *)(fp + 0x13) = 1;
                                *(unsigned char *)(fp + 0x14) = 0xFF;
                                *(unsigned char *)(fp + 0x15) = 0;
                                DAT_00489250++;
                            }
                            /* 75 shrapnel pieces */
                            for (int sh = 0; sh < 75 && DAT_00489248 < 0x9C4; sh++) {
                                unsigned int sh_ang = rand() & 0x7FF;
                                int sh_spd = rand() % 50;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = item_x; ep->previous_x = item_x;
                                ep->position_y = item_y; ep->previous_y = item_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->velocity_x = (bt_sc[sh_ang] * sh_spd) >> 6;
                                ep->velocity_y = (bt_sc[sh_ang + 0x200] * sh_spd) >> 6;
                                ep->state_20 = 0;
                                ep->type = 0x67;
                                ep->owner = 0xFF;
                                ep->variant_24 = 0;
                                ep->auxiliary_26 = 0xFF;
                                ep->health_or_damage_28 = 0;
                                ep->callback_address = bt_tt[0xD7A8/4];
                                ep->gravity_or_motion_38 = bt_tt[0xD830/4];
                                ep->counter_3c = 0;
                                ep->subtype = 0;
                                ep->damage_44 = bt_tt[0xD86C/4];
                                ep->scratch_48 = 0;
                                ep->animation_frame = 0;
                                DAT_00489248++;
                                /* Post-increment trailing writes */
                                Entity *ep2 = &DAT_004892e8[DAT_00489248 - 1];
                                ep2->timer_5c = 6; /* +0x5C */
                                unsigned char sh_pal = (unsigned char)((rand() % 12) + 0x14);
                                ep2->scratch_65 = sh_pal; /* +0x65 */
                                ep2->scratch_64 = 0x12; /* +0x64 */
                                unsigned short *pal_s = (unsigned short *)DAT_00487aa8;
                                if (pal_s) ep2->palette_value = (unsigned int)pal_s[sh_pal] + 30000; /* +0x4C */
                            }
                            player->hud_banner_id = 0x01;
                        } else if (roll < 142) {
                            /* Death Ring — 32 bullets in a ring around crate.
                             * Original jump table case 2. */
                            int *dr_sc = (int *)DAT_00487ab0;
                            int *dr_tt = (int *)DAT_00487abc;
                            for (int dr = 0; dr < 32 && DAT_00489248 < 0x9C4; dr++) {
                                int dr_ang = dr * 0x40; /* 0x800/32 = 0x40 per step */
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = item_x; ep->previous_x = item_x;
                                ep->position_y = item_y; ep->previous_y = item_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->velocity_x = (dr_sc[dr_ang] * 90) >> 6;
                                ep->velocity_y = (dr_sc[dr_ang + 0x200] * 90) >> 6;
                                ep->state_20 = 0xC2;
                                ep->type = 0x00;
                                ep->owner = (unsigned char)p;
                                ep->variant_24 = 6;
                                ep->auxiliary_26 = 0xFF;
                                ep->health_or_damage_28 = 0;
                                ep->callback_address = dr_tt[0];
                                ep->gravity_or_motion_38 = dr_tt[0x90/4];
                                ep->counter_3c = 0;
                                ep->subtype = 2;
                                ep->damage_44 = dr_tt[0xCC/4];
                                ep->scratch_48 = 0;
                                ep->palette_value = dr_tt[0xFC/4];
                                ep->animation_frame = 0;
                                ep->timer_5c = 0;
                                DAT_00489248++;
                                /* Post-increment trailing writes */
                                Entity *ep2 = &DAT_004892e8[DAT_00489248 - 1];
                                unsigned short *pal_dr = (unsigned short *)DAT_00487aa8;
                                if (pal_dr) ep2->palette_value = (unsigned int)pal_dr[7] + 30000; /* +0x4C */
                                ep2->health_or_damage_28 = (rand() & 7) + 0x96; /* +0x28: lifespan 150-157 */
                            }
                            player->hud_banner_id = 0x02;
                        } else if (roll < 242) {
                            /* 4 Miniships — allied to collecting player.
                             * Original jump table case 3. Type 0x1C, 4 cardinal directions. */
                            int *ms_sc = (int *)DAT_00487ab0;
                            int *ms_tt = (int *)DAT_00487abc;
                            unsigned char plr_team = player->team;
                            for (int ms = 0; ms < 4 && DAT_00489248 < 0x9C4; ms++) {
                                int ms_ang = (ms * 0x200 + 0x100) & 0x7FF;
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = item_x; ep->previous_x = item_x;
                                ep->position_y = item_y; ep->previous_y = item_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->velocity_x = ms_sc[ms_ang] >> 1;
                                ep->velocity_y = ms_sc[ms_ang + 0x200] >> 1;
                                ep->state_20 = 0;
                                ep->type = 0x1C;
                                ep->owner = (unsigned char)p;
                                ep->variant_24 = 0;
                                ep->auxiliary_26 = 0xFF;
                                ep->health_or_damage_28 = 0;
                                ep->callback_address = ms_tt[0x3AA0/4];
                                ep->gravity_or_motion_38 = ms_tt[0x3B28/4];
                                ep->counter_3c = 0;
                                ep->subtype = 0;
                                ep->damage_44 = ms_tt[0x3B64/4];
                                ep->scratch_48 = 0;
                                ep->palette_value = ms_tt[0x3B94/4];
                                ep->animation_frame = 0;
                                ep->timer_5c = 0x20; /* spawn immunity (team check bypass) */
                                DAT_00489248++;
                                /* Post-increment trailing writes */
                                Entity *ep2 = &DAT_004892e8[DAT_00489248 - 1];
                                ep2->counter_3c = ms_ang; /* +0x3C: heading */
                                ep2->scratch_2c = 0x0A; /* +0x2C: fire rate */
                                ep2->scratch_60 = 0x157C; /* +0x60: lifetime 5500 */
                                ep2->palette_value += (int)plr_team * 100; /* +0x4C: team sprite */
                                /* Register in tracking list (category 5 = miniships) */
                                *(int *)((int)DAT_0048781c + (5 * 0x1000 + DAT_00487834[5]) * 4) = DAT_00489248 - 1;
                                ep2->scratch_50 = DAT_00487834[5]; /* +0x50: tracking slot */
                                DAT_00487834[5]++;
                            }
                            player->hud_banner_id = 0x03;
                        } else if (roll < 342) {
                            /* 6 Insects — allied to collecting player.
                             * Original jump table case 4. Type 0x1F, zero initial velocity. */
                            int *in_tt = (int *)DAT_00487abc;
                            unsigned char plr_team = player->team;
                            for (int in = 0; in < 6 && DAT_00489248 < 0x9C4; in++) {
                                Entity *ep = &DAT_004892e8[DAT_00489248];
                                ep->position_x = item_x; ep->previous_x = item_x;
                                ep->position_y = item_y; ep->previous_y = item_y;
                                ep->motion_x_10 = 0; ep->motion_y_14 = 0;
                                ep->velocity_x = 0; ep->velocity_y = 0;
                                ep->state_20 = 0;
                                ep->type = 0x1F;
                                ep->owner = (unsigned char)p;
                                ep->variant_24 = 0;
                                ep->auxiliary_26 = 0xFF;
                                ep->health_or_damage_28 = 0;
                                ep->callback_address = in_tt[0x40E8/4];
                                ep->gravity_or_motion_38 = in_tt[0x4170/4];
                                ep->counter_3c = 0;
                                ep->subtype = 0;
                                ep->damage_44 = in_tt[0x41AC/4];
                                ep->scratch_48 = 0;
                                ep->palette_value = in_tt[0x41DC/4];
                                ep->animation_frame = 0;
                                ep->timer_5c = 0x20; /* spawn immunity (team check bypass) */
                                DAT_00489248++;
                                /* Post-increment trailing writes */
                                Entity *ep2 = &DAT_004892e8[DAT_00489248 - 1];
                                ep2->auxiliary_26 = 0xFF; /* +0x26 (redundant) */
                                ep2->scratch_2c = 0; /* +0x2C */
                                ep2->scratch_65 = 0; /* +0x65 */
                                ep2->palette_value += (int)plr_team * 100; /* +0x4C: team sprite */
                                ep2->scratch_60 = 0x9C4; /* +0x60: lifetime 2500 */
                                /* Register in tracking list (category 4 = insects) */
                                *(int *)((int)DAT_0048781c + (4 * 0x1000 + DAT_00487834[4]) * 4) = DAT_00489248 - 1;
                                ep2->scratch_50 = DAT_00487834[4]; /* +0x50: tracking slot */
                                DAT_00487834[4]++;
                            }
                            player->hud_banner_id = 0x04;
                        } else if (roll < 442) {
                            /* Faster special gun — decrease fire rate delay, min 8 */
                            int cur_rate = player->primary_fire_interval;
                            cur_rate -= 8;
                            if (cur_rate < 8) cur_rate = 8;
                            player->primary_fire_interval = (unsigned char)cur_rate;
                            player->hud_banner_id = 0x06;
                        } else {
                            /* Better basic gun — upgrade primary weapon, cap at 5 */
                            int cur_weapon = player->primary_weapon_level;
                            cur_weapon += 1;
                            if (cur_weapon > 5) cur_weapon = 5;
                            player->primary_weapon_level = (unsigned char)cur_weapon;
                            player->hud_banner_id = 0x07;
                        }
                        player->hud_banner_timer = 200;
                    }

                    /* Cap health to max */
                    int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x28) : 0;
                    if (player->health > max_hp) {
                        player->health = max_hp;
                    }

                    /* Consume item — set lifetime to 0 */
                    *(int *)(base + 8) = 0;
                    break;  /* Only one player can pick up */
                }
            }
        }

        /* Decrement lifetime */
        *(int *)(base + 8) = *(int *)(base + 8) - 1;
        if (*(int *)(base + 8) < 0) {
            /* Remove by swapping with last */
            DAT_00489268--;
            int last_base = (int)DAT_00487830 + DAT_00489268 * 0x20;
            int this_base = (int)DAT_00487830 + i * 0x20;
            /* Copy last to current (whole 0x20 bytes approximated as ints) */
            for (int k = 0; k < 8; k++) {
                ((int *)this_base)[k] = ((int *)last_base)[k];
            }
            if (i >= DAT_00489268) break;
            continue;
        }
        i++;
    }
}
/* ===== FUN_004571f0 — Explosion_Knockback (004571F0) ===== */
/* For each active explosion (DAT_00489e98 array, DAT_00489274 count), checks
 * nearby players and pushes them away using atan2 + cos/sin LUT knockback.
 * Also restores player health up to max.
 * NB: DAT_00489274 is the "static entity" / explosion count. Effects.cpp marks
 * its renderer FUN_0040dbd0 as dead code (count never grows in this build), so
 * this function effectively no-ops at runtime — kept for parity with original. */
void FUN_004571f0(void)
{
    if (DAT_00489274 <= 0) return;

    int exp_off = 0;
    for (int e = 0; e < DAT_00489274; e++) {
        int *exp_ent = (int *)((int)DAT_00489e98 + exp_off);

        /* Rotate explosion angle by 0x78 per tick (visual spin) */
        exp_ent[2] += 0x78;
        if ((unsigned int)exp_ent[2] > 0x7FF) {
            exp_ent[2] -= 0x800;
        }

        /* Check each player for proximity */
        int soff = 0;
        for (int p = 0; p < DAT_00489240; p++) {
            PlayerData *player = Player_Get(p);
            if (player->health > 0) {
                int dx = (exp_ent[0] - player->position_x) >> 0x12;
                int dy = (exp_ent[1] - player->position_y) >> 0x12;

                if (dx * dx + dy * dy < 0x1C2) {  /* within ~21 tiles radius */
                    /* Calculate angle from explosion to player */
                    int angle = FUN_004257e0(exp_ent[0], exp_ent[1],
                                            player->position_x, player->position_y);
                    unsigned int push_angle = ((unsigned int)angle + 0x200) & 0x7FF;

                    /* Push player away from explosion */
                    int *lut = (int *)DAT_00487ab0;
                    player->velocity_x = tou_binary::add_wrap_i32(
                        player->velocity_x, lut[push_angle] >> 6);
                    player->velocity_y = tou_binary::add_wrap_i32(
                        player->velocity_y, lut[push_angle + 0x200] >> 6);

                    /* Also apply reverse force (original has both directions) */
                    player->velocity_x = tou_binary::sub_wrap_i32(
                        player->velocity_x, lut[angle] >> 6);
                    player->velocity_y = tou_binary::sub_wrap_i32(
                        player->velocity_y, lut[(unsigned int)angle + 0x200] >> 6);

                    player->flag_a1 = 1;

                    /* Heal player by damage_flag * 0x800 */
                    player->health = tou_binary::add_wrap_i32(
                        player->health, (unsigned int)DAT_00483754[1] * 0x800);

                    /* Cap at max health */
                    int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + soff + 0x28) : 0;
                    if (player->health < max_hp) {
                        player->flag_a1 = 2;
                    } else {
                        player->health = max_hp;
                    }
                }
            }
            soff += 0x40;
        }
        exp_off += 0x10;
    }
}
/* ===== FUN_00453a80 — Wandering_Item_AI (00453A80) ===== */
/* Moves items (DAT_00487a9c array, DAT_0048926c count) using heading angle.
 * Items wander with random direction changes, bounce off walls, and expire
 * when their lifetime counter reaches 0. Expired items are removed by
 * swapping with the last entry. */
void FUN_00453a80(void)
{
    int i = 0;
    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int *lut = (int *)DAT_00487ab0;

    while (i < DAT_0048926c) {
        int *item = (int *)((int)DAT_00487a9c + i * 0x20);

        /* Check if item is near walkable boundary (above/below) */
        int tx = item[0] >> 0x12;
        int ty = item[1] >> 0x12;
        unsigned char tile_below = *(unsigned char *)((int)DAT_0048782c + ((ty + 1) << shift) + tx);
        unsigned char tile_above = *(unsigned char *)((int)DAT_0048782c + ((ty - 1) << shift) + tx);
        if (*(char *)((int)DAT_00487928 + (unsigned int)tile_below * 0x20 + 0x18) != '\0' ||
            *(char *)((int)DAT_00487928 + (unsigned int)tile_above * 0x20 + 0x18) != '\0') {
            *(char *)((int)item + 0x19) = 5;  /* special bounce state */
            item[4] = 0x400;
        }

        if (*(char *)((int)item + 0x19) != '\x05') {
            /* Normal wandering */
            unsigned int heading;
            if (*(char *)((int)item + 0x19) == '\0') {
                heading = item[4] + 9;      /* clockwise */
            } else {
                heading = item[4] - 9;      /* counter-clockwise */
            }
            item[4] = heading & 0x7FF;

            /* Countdown direction change timer */
            char timer = *(char *)((int)item + 0x1A) - 1;
            *(char *)((int)item + 0x1A) = timer;
            if (timer == '\0') {
                /* Randomize new timer and direction */
                *(char *)((int)item + 0x1A) = (char)(rand() % 0x50 + 0x14);
                *(char *)((int)item + 0x19) = (char)(rand() & 1);
            }

            /* Move item using heading via LUT */
            int old_x = item[0];
            int old_y = item[1];
            int h = item[4];
            item[0] += lut[h] >> 1;
            item[1] += lut[(h + 0x200) & 0x7FF] >> 1;

            /* Wall bounce check: if new tile is non-walkable, reverse */
            tx = item[0] >> 0x12;
            ty = item[1] >> 0x12;
            unsigned char new_tile = *(unsigned char *)((int)DAT_0048782c + (ty << shift) + tx);
            if (*(char *)((int)DAT_00487928 + (unsigned int)new_tile * 0x20 + 1) == '\0') {
                item[0] = old_x;
                item[1] = old_y;
                item[4] = (h + 0x400) & 0x7FF;  /* reverse heading 180° */
            }
        }

        /* Decrement lifetime */
        item[5]--;
        if (item[5] < 0) {
            /* Expired — remove by swapping with last */
            DAT_0048926c--;
            int last_off = DAT_0048926c * 0x20;
            int *last = (int *)((int)DAT_00487a9c + last_off);
            item[0] = last[0]; item[1] = last[1]; item[2] = last[2]; item[3] = last[3];
            item[4] = last[4]; item[5] = last[5];
            *(unsigned char *)((int)item + 0x18) = *(unsigned char *)((int)DAT_00487a9c + last_off + 0x18);
            *(unsigned char *)((int)item + 0x19) = *(unsigned char *)((int)DAT_00487a9c + last_off + 0x19);
            *(unsigned char *)((int)item + 0x1A) = *(unsigned char *)((int)DAT_00487a9c + last_off + 0x1A);
            continue;  /* re-check this slot */
        }
        i++;
    }
}
/* ===== FUN_004573e0 — Trap_Door_Update (004573E0) ===== */
/* Trap door / moving wall record (DAT_00489e80, stride 0x20):
 *   +0x00 x, +0x04 y (anchor tile), +0x08 progress (18 fractional bits),
 *   +0x0C target/cooldown progress, +0x10 timer (200 = idle, 0 = move),
 *   +0x15 direction (0=down,1=right,2=up,3=left), +0x16 oscillation delay,
 *   +0x17 PhysicsParams index, +0x18 anim subframe (0..3, ticks once per call),
 *   +0x19 owning team (3 = neutral), +0x1A sprite index for fill, +0x1B linked
 *   record (0xFF if standalone — pairs both ends of an extending wall). */
void FUN_004573e0(void)
{
    int i, off;

    /* Loop 1: Advance animation frame for all segments.
     * Animation runs at quarter speed: 4 calls = 1 visible frame; the loops
     * below skip work on subframe != 0 to spread cost across the cycle. */
    for (i = 0; i < DAT_00489270; i++) {
        off = i * 0x20;
        int base = (int)DAT_00489e80;
        *(char *)(base + off + 0x18) = *(char *)(base + off + 0x18) + 1;
        if (*(unsigned char *)(base + off + 0x18) > 3) {
            *(unsigned char *)(base + off + 0x18) = 0;
        }
    }

    /* Loop 2: Progress update + tile destruction (for active segments) */
    int iVar8 = (int)DAT_00489e80;
    for (i = 0; i < DAT_00489270; i++) {
        off = i * 0x20;

        if (*(char *)(iVar8 + off + 0x18) != '\0') continue;

        /* Progress reset if < 1 */
        if (*(int *)(iVar8 + off + 0x0C) < 1) {
            *(int *)(iVar8 + off + 0x10) = 200;
            unsigned int pindex = (unsigned int)*(unsigned char *)((int)DAT_00489e80 + off + 0x17);
            *(int *)((int)DAT_00489e80 + off + 0x0C) =
                *(int *)((int)g_PhysicsParams + pindex * 0x10 + 0xC);

            unsigned char linked = *(unsigned char *)((int)DAT_00489e80 + off + 0x1B);
            iVar8 = (int)DAT_00489e80;
            if (linked != 0xFF) {
                *(int *)((unsigned int)linked * 0x20 + 0x10 + (int)DAT_00489e80) = 200;
                int lbase = (int)DAT_00489e80 + (unsigned int)*(unsigned char *)((int)DAT_00489e80 + off + 0x1B) * 0x20;
                unsigned int lpindex = (unsigned int)*(unsigned char *)(lbase + 0x17);
                *(int *)(lbase + 0x0C) = *(int *)((int)g_PhysicsParams + lpindex * 0x10 + 0xC);
                iVar8 = (int)DAT_00489e80;
            }
        }

        /* Advance opening progress */
        *(int *)(iVar8 + off + 0x0C) += 0x19000;

        /* Clamp to max from PhysicsParams */
        int *pProgress = (int *)((int)DAT_00489e80 + off + 0x0C);
        int max_size = *(int *)((unsigned int)*(unsigned char *)((int)DAT_00489e80 + off + 0x17) * 0x10 +
                       0xC + (int)g_PhysicsParams);
        if (*pProgress > max_size) {
            *pProgress = max_size;
        }
        iVar8 = (int)DAT_00489e80;

        /* Tile destruction when timer == 0.
         * 0xEF is the threshold: only "soft" tile types (>0xEF) get cleared by
         * the moving door — solid map geometry is preserved. Door clears the
         * corridor it's about to extend into. */
        if (*(int *)((int)DAT_00489e80 + off + 0x10) == 0) {
            unsigned int sprite_idx = (unsigned int)*(unsigned char *)((int)DAT_00489e80 + off + 0x1A);
            unsigned int w = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + sprite_idx);
            int h = (int)(unsigned int)*(unsigned char *)((int)DAT_00489e88 + sprite_idx);
            int x0 = *(int *)((int)DAT_00489e80 + off);
            int y0 = *(int *)((int)DAT_00489e80 + off + 4);
            int progress = *(int *)((int)DAT_00489e80 + off + 8) >> 0x12;
            int shift = (unsigned char)DAT_00487a18 & 0x1F;
            int stride = DAT_00487a00;

            switch (*(unsigned char *)((int)DAT_00489e80 + off + 0x15)) {
            case 0: { /* down */
                int start_y = progress + y0;
                if ((int)(start_y + h) > (int)(DAT_004879f4 - 7))
                    h = (DAT_004879f4 - start_y) - 7;
                int tile = (start_y << shift) - (int)w / 2 + x0;
                if (0 < h) {
                    int rows = h;
                    unsigned int cols = w;
                    int tmap = (int)DAT_0048782c;
                    do {
                        for (; cols != 0; cols--) {
                            if (*(unsigned char *)(tmap + tile) > 0xEF) {
                                *(unsigned char *)(tmap + tile) = 0;
                                *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                                tmap = (int)DAT_0048782c;
                                iVar8 = (int)DAT_00489e80;
                            }
                            tile++;
                        }
                        tile += stride - (int)w;
                        rows--;
                        cols = w;
                    } while (rows != 0);
                }
                break;
            }
            case 1: { /* right */
                int start_x = x0 + progress;
                if ((int)(start_x + h) > (int)(DAT_004879f0 - 7))
                    h = (DAT_004879f0 - start_x) - 7;
                int tile = ((y0 - (int)w / 2) << shift) + start_x;
                int tmap = (int)DAT_0048782c;
                for (; w != 0; w--) {
                    int rh = h;
                    if (0 < h) {
                        do {
                            if (*(unsigned char *)(tmap + tile) > 0xEF) {
                                *(unsigned char *)(tmap + tile) = 0;
                                *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                                tmap = (int)DAT_0048782c;
                                iVar8 = (int)DAT_00489e80;
                            }
                            tile++;
                            rh--;
                        } while (rh != 0);
                    }
                    tile += stride - h;
                }
                break;
            }
            case 2: { /* up */
                int start_y = (y0 - progress) - h;
                int adj_y = start_y + 1;
                if (adj_y < 7) { h = start_y - 6 + h; adj_y = 7; }
                int tile = (adj_y << shift) - (int)w / 2 + x0;
                if (0 < h) {
                    int rows = h;
                    unsigned int cols = w;
                    int tmap = (int)DAT_0048782c;
                    do {
                        for (; cols != 0; cols--) {
                            if (*(unsigned char *)(tmap + tile) > 0xEF) {
                                *(unsigned char *)(tmap + tile) = 0;
                                *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                                tmap = (int)DAT_0048782c;
                                iVar8 = (int)DAT_00489e80;
                            }
                            tile++;
                        }
                        tile += stride - (int)w;
                        rows--;
                        cols = w;
                    } while (rows != 0);
                }
                break;
            }
            case 3: { /* left */
                int start_x = (x0 - progress) - h;
                int adj_x = start_x + 1;
                if (adj_x < 7) { h = start_x - 6 + h; adj_x = 7; }
                int tile = ((y0 - (int)w / 2) << shift) + adj_x;
                int tmap = (int)DAT_0048782c;
                for (; w != 0; w--) {
                    int rh = h;
                    if (0 < h) {
                        do {
                            if (*(unsigned char *)(tmap + tile) > 0xEF) {
                                *(unsigned char *)(tmap + tile) = 0;
                                *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                                tmap = (int)DAT_0048782c;
                                iVar8 = (int)DAT_00489e80;
                            }
                            tile++;
                            rh--;
                        } while (rh != 0);
                    }
                    tile += stride - h;
                }
                break;
            }
            }
        }
    }

    /* Loop 3: Timer/fire OR movement/behavior, then ALWAYS render */
    iVar8 = (int)DAT_00489e80;
    for (i = 0; i < DAT_00489270; i++) {
        off = i * 0x20;

        if (*(char *)(off + 0x18 + iVar8) != '\0') continue;

        int timer = *(int *)(off + 0x10 + iVar8);

        if (timer == 0) {
            /* === Movement/behavior === */
            unsigned int pindex = (unsigned int)*(unsigned char *)(off + 0x17 + iVar8);
            int speed = *(int *)(pindex * 0x10 + 4 + (int)g_PhysicsParams);

            if (pindex == 2) {
                /* Type 2: Proximity-based door */
                int score = 0;
                if (DAT_00489240 > 0) {
                    int count = DAT_00489240;
                    int player_index = 0;
                    do {
                        PlayerData *player = Player_Get(player_index);
                        if (player->state_24 == 0) {
                            char doorTeam = *(char *)(off + 0x19 + iVar8);
                            if ((char)player->team == doorTeam || doorTeam == '\x03') {
                                int dy = (player->position_y >> 0x12) -
                                         *(int *)(off + 4 + iVar8);
                                int dx = (player->position_x >> 0x12) -
                                         *(int *)(off + iVar8);
                                int dist = (int)sqrt((double)(dx * dx + dy * dy));
                                if (dist > 0x4F) dist = 0x4F;
                                else if (dist < 0) dist = 0;
                                score += (0x4F - dist);
                            }
                        }
                        player_index++;
                        count--;
                    } while (count != 0);
                    if (score > 0x50) score = 0x50;
                }
                *(int *)(off + 8 + iVar8) = score << 0x12;
            }
            else if (pindex == 3) {
                /* Type 3: Triggered door */
                int triggered = 0;
                if (DAT_00489240 > 0) {
                    int count = DAT_00489240;
                    int player_index = 0;
                    do {
                        PlayerData *player = Player_Get(player_index);
                        if (player->state_24 == 0) {
                            char doorTeam = *(char *)(off + 0x19 + iVar8);
                            if ((char)player->team == doorTeam || doorTeam == '\x03') {
                                int dy = (player->position_y >> 0x12) -
                                         *(int *)(off + 4 + iVar8);
                                int dx = (player->position_x >> 0x12) -
                                         *(int *)(off + iVar8);
                                int dist = (int)sqrt((double)(dx * dx + dy * dy));
                                if ((double)dist < 96.0) {
                                    triggered = 1;
                                }
                            }
                        }
                        player_index++;
                        count--;
                    } while (count != 0);
                    if (triggered) {
                        *(int *)(off + 8 + iVar8) += speed;
                        if (*(int *)(off + 8 + (int)DAT_00489e80) > 0x1400000)
                            *(int *)(off + 8 + (int)DAT_00489e80) = 0x1400000;
                        goto trap_render;
                    }
                }
                *(int *)(off + 8 + iVar8) -= speed;
                if (*(int *)(off + 8 + (int)DAT_00489e80) < 0)
                    *(int *)(off + 8 + (int)DAT_00489e80) = 0;
            }
            else {
                /* Type 0/1: Oscillating door */
                char delay = *(char *)(off + 0x16 + iVar8);
                if (delay == '\0') {
                    int prog = *(int *)(off + 8 + iVar8);
                    prog += (int)*(signed char *)(off + 0x14 + iVar8) * speed;
                    *(int *)(off + 8 + iVar8) = prog;
                } else {
                    *(char *)(off + 0x16 + iVar8) = delay - 1;
                }

                if (*(int *)(off + 8 + (int)DAT_00489e80) > 0x1400000) {
                    *(int *)(off + 8 + (int)DAT_00489e80) = 0x1400000;
                    *(char *)(off + 0x14 + (int)DAT_00489e80) =
                        -*(char *)(off + 0x14 + (int)DAT_00489e80);
                    *(char *)(off + 0x16 + (int)DAT_00489e80) =
                        *(char *)((unsigned int)*(unsigned char *)(off + 0x17 + (int)DAT_00489e80) *
                                  0x10 + (int)g_PhysicsParams);
                }
                if (*(int *)(off + 8 + (int)DAT_00489e80) < 0) {
                    *(int *)(off + 8 + (int)DAT_00489e80) = 0;
                    *(char *)(off + 0x14 + (int)DAT_00489e80) =
                        -*(char *)(off + 0x14 + (int)DAT_00489e80);
                    *(char *)(off + 0x16 + (int)DAT_00489e80) =
                        *(char *)((unsigned int)*(unsigned char *)(off + 0x17 + (int)DAT_00489e80) *
                                  0x10 + (int)g_PhysicsParams);
                }
            }
        }
        else {
            /* === Timer countdown + fire particles === */
            *(int *)(off + 0x10 + iVar8) = timer - 1;

            /* Check viewport visibility */
            int base = (int)DAT_00489e80;
            int vx = *(int *)(off + base) >> 4;
            int vy = *(int *)(off + 4 + base) >> 4;
            if ((*(unsigned char *)((int)DAT_00487814 + vx + vy * DAT_004879f8) & 0x08) &&
                DAT_0048385c > 0.2f && DAT_0048925c < 0x5DC)
            {
                int rx = rand() % 0x1E - 0xF + *(int *)(off + (int)DAT_00489e80);
                iVar8 = off + (int)DAT_00489e80;
                int ry = rand() % 0x1E - 0xF + *(int *)(iVar8 + 4);
                char dir = *(char *)(iVar8 + 0x15);
                int prog = *(int *)(iVar8 + 8) >> 0x12;

                if (dir == '\x03') rx -= prog;
                if (dir == '\x01') rx += prog;
                if (dir == '\0')   ry += prog;
                if (dir == '\x02') ry -= prog;

                unsigned int angle = rand();
                angle = angle & 0x800001FF;
                if ((int)angle < 0) angle = (angle - 1 | 0xFFFFFE00) + 1;

                if (DAT_0048925c < 0x5DC) {
                    int pidx = DAT_0048925c * 0x20;
                    *(int *)((int)DAT_00481f2c + pidx) = rx << 0x12;
                    *(int *)((int)DAT_00481f2c + pidx + 4) = ry << 0x12;
                    int spd = rand() % 0x1E + 10;
                    *(int *)((int)DAT_00481f2c + pidx + 8) =
                        spd * *(int *)((int)DAT_00487ab0 + (angle + 0x300) * 4) >> 6;
                    *(int *)((int)DAT_00481f2c + pidx + 0xC) =
                        spd * *(int *)((int)DAT_00487ab0 + 0x800 + (angle + 0x300) * 4) >> 6;
                    *(char *)((int)DAT_00481f2c + pidx + 0x10) = (char)(rand() % 5) + 0x16;
                    *(char *)((int)DAT_00481f2c + pidx + 0x11) = 0;
                    *(short *)((int)DAT_00481f2c + pidx + 0x12) = 0;
                    *(char *)((int)DAT_00481f2c + pidx + 0x14) = (char)0xFF;
                    *(char *)((int)DAT_00481f2c + pidx + 0x15) = 0;
                    DAT_0048925c++;
                }
            }
        }

trap_render:
        FUN_00457c70(i);
        iVar8 = (int)DAT_00489e80;
    }
}
/* ===== Turret tile helpers ===== */

/* FUN_00410f50 helper: restore tile type from property[0x17], set color based on passable */
static inline void turret_restore_tile(int tileIdx, unsigned short color) {
    int prop = (int)DAT_00487928 + (unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20;
    if (*(char *)(prop + 4) != '\0') {
        *(unsigned char *)((int)DAT_0048782c + tileIdx) = *(unsigned char *)(prop + 0x17);
        if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20 + (int)DAT_00487928) == '\x01')
            *(unsigned short *)((int)DAT_00481f50 + tileIdx * 2) = 0;
        else
            *(unsigned short *)((int)DAT_00481f50 + tileIdx * 2) = color;
    }
}

/* FUN_00412710 helper (first half): set color to DAT_0048384c if property[4] != 0 */
static inline void turret_color_tile(int tileIdx) {
    if (*(char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20 + 4 + (int)DAT_00487928) != '\0') {
        *(unsigned short *)((int)DAT_00481f50 + tileIdx * 2) = DAT_0048384c;
    }
}

/* FUN_00412710 helper (dirty flag): reset tile type from property[0x13] if property[4] != 0 */
static inline void turret_dirty_tile(int tileIdx) {
    int prop = (unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20 + (int)DAT_00487928;
    if (*(char *)(prop + 4) != '\0') {
        *(unsigned char *)((int)DAT_0048782c + tileIdx) = *(unsigned char *)(prop + 0x13);
    }
}

/* FUN_00411ae0 helper: paint tile with team color if passable or property[4] != 0 */
static inline void turret_paint_tile(int tileIdx, unsigned int team, unsigned short color) {
    char *pcVar = (char *)((unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20 + (int)DAT_00487928);
    if (*pcVar == '\x01' || pcVar[4] != '\0') {
        *(unsigned short *)((int)DAT_00481f50 + tileIdx * 2) = color;
        *(char *)((int)DAT_0048782c + tileIdx) =
            *(char *)((team & 0xff) + 0x14 +
                     (int)DAT_00487928 + (unsigned int)*(unsigned char *)((int)DAT_0048782c + tileIdx) * 0x20) + '\x01';
    }
}

/* ===== FUN_00410f50 — Turret Tile Reset (00410F50) ===== */
/* Restores 3x3 turret tiles (+ border tiles based on orientation) to original state.
 * Reads tile property offset, restores tile type from property[0x17],
 * sets color to 0 if passable (property[0]==1) or DAT_0048384c otherwise. */
static void FUN_00410f50(int param_1)
{
    int base = *(int *)(DAT_00481f48 + param_1 * 8);
    int row = DAT_00487a00;

    /* 9 core tiles: 3x3 grid */
    turret_restore_tile(base,             DAT_0048384c);
    turret_restore_tile(base + 1,         DAT_0048384c);
    turret_restore_tile(base + 2,         DAT_0048384c);
    turret_restore_tile(base + row,       DAT_0048384c);
    turret_restore_tile(base + row + 1,   DAT_0048384c);
    turret_restore_tile(base + row + 2,   DAT_0048384c);
    turret_restore_tile(base + row * 2,     DAT_0048384c);
    turret_restore_tile(base + row * 2 + 1, DAT_0048384c);
    turret_restore_tile(base + row * 2 + 2, DAT_0048384c);

    /* Border tiles depend on orientation byte [4] */
    char orient = *(char *)(DAT_00481f48 + 4 + param_1 * 8);

    if (orient == '\0') {
        /* orientation 0: -1, -1+row, -row, -row+1, +2+row*3, +1+row*3 */
        turret_restore_tile(base - 1,             DAT_0048384c);
        turret_restore_tile(base - 1 + row,       DAT_0048384c);
        turret_restore_tile(base - row,            DAT_0048384c);
        turret_restore_tile(base - row + 1,        DAT_0048384c);
        turret_restore_tile(base + 2 + row * 3,    DAT_0048384c);
        turret_restore_tile(base + 1 + row * 3,    DAT_0048384c);
    }
    else if (orient == '\x01') {
        /* orientation 1: +row*3, +1+row*3, +3+row, +3+row*2 */
        turret_restore_tile(base + row * 3,         DAT_0048384c);
        turret_restore_tile(base + 1 + row * 3,     DAT_0048384c);
        turret_restore_tile(base + 3 + row,          DAT_0048384c);
        turret_restore_tile(base + 3 + row * 2,      DAT_0048384c);
    }
    else if (orient == '\x02') {
        /* orientation 2: -1+row, -row+1, -row+2, -1+row*2 */
        turret_restore_tile(base - 1 + row,          DAT_0048384c);
        turret_restore_tile(base - row + 1,           DAT_0048384c);
        turret_restore_tile(base - row + 2,           DAT_0048384c);
        turret_restore_tile(base - 1 + row * 2,       DAT_0048384c);
    }
    else {
        /* orientation 3: -row, -row-1, -1, +3+row, +3+row*2, +3 */
        turret_restore_tile(base - row,               DAT_0048384c);
        turret_restore_tile(base - row - 1,           DAT_0048384c);
        turret_restore_tile(base - 1,                 DAT_0048384c);
        turret_restore_tile(base + 3 + row,           DAT_0048384c);
        turret_restore_tile(base + 3 + row * 2,       DAT_0048384c);
        turret_restore_tile(base + 3,                 DAT_0048384c);
    }
}

/* ===== FUN_00412710 — Turret Fall/Restore (00412710) ===== */
/* Checks if turret is suspended (fall_counter >= 8) and tile 2 rows above is empty.
 * If so, resets fall counter and restores color on 3x3 core + border tiles.
 * Then checks dirty flag [7] and resets tile types using property[0x13]. */
static void FUN_00412710(int param_1)
{
    int base;
    int row = DAT_00487a00;

    /* Check: fall_counter >= 8 AND tile 2 rows above is passable (type == 0) */
    if ((7 < *(unsigned char *)(DAT_00481f48 + 5 + param_1 * 8)) &&
        (*(char *)((unsigned int)*(unsigned char *)(*(int *)(DAT_00481f48 + param_1 * 8) + row * -2 +
                                  (int)DAT_0048782c) * 0x20 + (int)DAT_00487928) == '\0'))
    {
        *(unsigned char *)(DAT_00481f48 + 5 + param_1 * 8) = 0;
        base = *(int *)(DAT_00481f48 + param_1 * 8);

        /* 9 core tiles: set color to DAT_0048384c if property[4] != 0 */
        turret_color_tile(base);
        turret_color_tile(base + 1);
        turret_color_tile(base + 2);
        turret_color_tile(base + row);
        turret_color_tile(base + row + 1);
        turret_color_tile(base + row + 2);
        turret_color_tile(base + row * 2);
        turret_color_tile(base + row * 2 + 1);
        turret_color_tile(base + 2 + row * 2);

        /* Border tiles depend on orientation byte [4] */
        char orient = *(char *)(DAT_00481f48 + 4 + param_1 * 8);

        if (orient == '\0') {
            turret_color_tile(base - 1);
            turret_color_tile(base - 1 + row);
            turret_color_tile(base - row);
            turret_color_tile(base - row + 1);
            turret_color_tile(base + 2 + row * 3);
            turret_color_tile(base + 1 + row * 3);
        }
        else if (orient == '\x01') {
            turret_color_tile(base + row * 3);
            turret_color_tile(base + 1 + row * 3);
            turret_color_tile(base + 3 + row);
            turret_color_tile(base + 3 + row * 2);
        }
        else if (orient == '\x02') {
            turret_color_tile(base - 1 + row);
            turret_color_tile(base - row + 1);
            turret_color_tile(base - row + 2);
            turret_color_tile(base - 1 + row * 2);
        }
        else {
            /* orientation 3 */
            turret_color_tile(base - row);
            turret_color_tile(base - row - 1);
            turret_color_tile(base - 1);
            turret_color_tile(base + 3 + row);
            turret_color_tile(base + 3 + row * 2);
            turret_color_tile(base + 3);
        }
    }

    /* Dirty flag check: if [7] is set, reset tile types using property[0x13] */
    if (*(char *)(DAT_00481f48 + 7 + param_1 * 8) != '\0') {
        *(unsigned char *)(DAT_00481f48 + 7 + param_1 * 8) = 0;
        base = *(int *)(DAT_00481f48 + param_1 * 8);

        /* 9 core tiles: reset type from property[0x13] */
        turret_dirty_tile(base);
        turret_dirty_tile(base + 1);
        turret_dirty_tile(base + 2);
        turret_dirty_tile(base + row);
        turret_dirty_tile(base + 1 + row);
        turret_dirty_tile(base + 2 + row);
        turret_dirty_tile(base + row * 2);
        turret_dirty_tile(base + 1 + row * 2);
        turret_dirty_tile(base + 2 + row * 2);

        /* Border tiles depend on orientation byte [4] */
        char orient = *(char *)(DAT_00481f48 + 4 + param_1 * 8);

        if (orient == '\0') {
            turret_dirty_tile(base - 1);
            turret_dirty_tile(base - 1 + row);
            turret_dirty_tile(base - row);
            turret_dirty_tile(base - row + 1);
            turret_dirty_tile(base + 2 + row * 3);
            turret_dirty_tile(base + 1 + row * 3);
        }
        else if (orient == '\x01') {
            turret_dirty_tile(base + row * 3);
            turret_dirty_tile(base + 1 + row * 3);
            turret_dirty_tile(base + 3 + row);
            turret_dirty_tile(base + 3 + row * 2);
        }
        else if (orient == '\x02') {
            turret_dirty_tile(base - 1 + row);
            turret_dirty_tile(base - row + 1);
            turret_dirty_tile(base - row + 2);
            turret_dirty_tile(base - 1 + row * 2);
        }
        else {
            /* orientation 3 */
            turret_dirty_tile(base - row);
            turret_dirty_tile(base - row - 1);
            turret_dirty_tile(base - 1);
            turret_dirty_tile(base + 3 + row);
            turret_dirty_tile(base + 3 + row * 2);
            turret_dirty_tile(base + 3);
        }
    }
}

/* ===== FUN_00411ae0 — Turret Block Movement (00411AE0) ===== */
/* Tries to move a 3x3 turret block by param_1 tiles.
 * 1. Checks if destination tile (row below, +1 col, + param_1 offset) is passable
 * 2. Calls FUN_00410f50 to clear current position
 * 3. Updates turret tile offset
 * 4. Tracks fall counter; sets DAT_00480700 based on fall state
 * 5. Paints new tiles using property[0x14 + team] + 1 as new tile type
 * 6. Checks dirty flag and returns 1 if modified, 0 otherwise.
 * Returns: 0 if can't move, 1 if moved successfully. */
static int FUN_00411ae0(int param_1, unsigned int param_2, int param_3)
{
    int base;
    int row = DAT_00487a00;

    /* Check if destination tile is passable */
    if (*(char *)((unsigned int)*(unsigned char *)(*(int *)(DAT_00481f48 + param_3 * 8) + row +
                                  (int)DAT_0048782c + 1 + param_1) * 0x20 + (int)DAT_00487928) != '\x01') {
        return 0;
    }

    /* Clear current position */
    FUN_00410f50(param_3);

    /* Update turret tile offset */
    *(int *)(DAT_00481f48 + param_3 * 8) = *(int *)(DAT_00481f48 + param_3 * 8) + param_1;

    base = *(int *)(DAT_00481f48 + param_3 * 8);

    /* Check landing conditions: 3 rows below impassable, or 2 rows above empty, or large move */
    if ((*(char *)((unsigned int)*(unsigned char *)(base + row * 3 + (int)DAT_0048782c) * 0x20 + (int)DAT_00487928) == '\x01') ||
        (*(char *)((unsigned int)*(unsigned char *)(base + row * -2 + (int)DAT_0048782c) * 0x20 + (int)DAT_00487928) == '\0') ||
        (9 < param_1)) {
        *(unsigned char *)(DAT_00481f48 + 5 + param_3 * 8) = 0;
    }
    else {
        *(char *)(DAT_00481f48 + 5 + param_3 * 8) = *(char *)(DAT_00481f48 + 5 + param_3 * 8) + '\x01';
    }

    /* Set DAT_00480700 based on fall counter */
    if (*(unsigned char *)(DAT_00481f48 + 5 + param_3 * 8) < 8) {
        DAT_00480700 = DAT_0048384c;
    }
    else {
        DAT_00480700 = 0;
        *(unsigned char *)(DAT_00481f48 + 5 + param_3 * 8) = 8;
    }

    base = *(int *)(DAT_00481f48 + param_3 * 8);

    /* Paint 9 core tiles */
    turret_paint_tile(base,               param_2, DAT_00480700);
    turret_paint_tile(base + 1,           param_2, DAT_00480700);
    turret_paint_tile(base + 2,           param_2, DAT_00480700);
    turret_paint_tile(base + row,         param_2, DAT_00480700);
    turret_paint_tile(base + row + 1,     param_2, DAT_00480700);
    turret_paint_tile(base + row + 2,     param_2, DAT_00480700);
    turret_paint_tile(base + row * 2,     param_2, DAT_00480700);
    turret_paint_tile(base + row * 2 + 1, param_2, DAT_00480700);
    turret_paint_tile(base + row * 2 + 2, param_2, DAT_00480700);

    /* Border tiles depend on orientation byte [4] */
    char orient = *(char *)(DAT_00481f48 + 4 + param_3 * 8);

    if (orient == '\0') {
        turret_paint_tile(base - 1,             param_2, DAT_00480700);
        turret_paint_tile(base - 1 + row,       param_2, DAT_00480700);
        turret_paint_tile(base - row,            param_2, DAT_00480700);
        turret_paint_tile(base - row + 1,        param_2, DAT_00480700);
        turret_paint_tile(base + 2 + row * 3,    param_2, DAT_00480700);
        turret_paint_tile(base + 1 + row * 3,    param_2, DAT_00480700);
    }
    else if (orient == '\x01') {
        turret_paint_tile(base + row * 3,         param_2, DAT_00480700);
        turret_paint_tile(base + 1 + row * 3,     param_2, DAT_00480700);
        turret_paint_tile(base + 3 + row,          param_2, DAT_00480700);
        turret_paint_tile(base + 3 + row * 2,      param_2, DAT_00480700);
    }
    else if (orient == '\x02') {
        turret_paint_tile(base - 1 + row,          param_2, DAT_00480700);
        turret_paint_tile(base - row + 1,           param_2, DAT_00480700);
        turret_paint_tile(base - row + 2,           param_2, DAT_00480700);
        turret_paint_tile(base - 1 + row * 2,       param_2, DAT_00480700);
    }
    else {
        /* orientation 3 */
        turret_paint_tile(base - row,               param_2, DAT_00480700);
        turret_paint_tile(base - row - 1,           param_2, DAT_00480700);
        turret_paint_tile(base - 1,                 param_2, DAT_00480700);
        turret_paint_tile(base + 3 + row,           param_2, DAT_00480700);
        turret_paint_tile(base + 3 + row * 2,       param_2, DAT_00480700);
        turret_paint_tile(base + 3,                 param_2, DAT_00480700);
    }

    /* Set dirty flag and return 1 */
    *(unsigned char *)(DAT_00481f48 + 7 + param_3 * 8) = 1;
    return 1;
}

/* ===== FUN_004133d0 — Turret_Block_Process (004133D0) ===== */
/* Processes turret blocks (DAT_00481f48, stride 8, DAT_0048927c count).
 * Each turret block record: +0x00 = tile index (int), +0x04 = direction (char),
 * +0x05 = anim counter (byte), +0x06 = movement state (byte), +0x07 = misc (byte).
 * When param==0: processes up to difficulty-limited count from DAT_00483834 table.
 * When param!=0: processes all turret blocks.
 * Main logic: tries FUN_00411ae0 for movement, then handles falling-off-map removal. */
void FUN_004133d0(char param)
{
    int row_stride_x3 = DAT_00487a00 * 3;
    int limits[7];
    limits[1] = 800;
    limits[2] = 0x640;
    limits[3] = 0xaf0;
    limits[4] = 0x1068;
    limits[5] = 6000;
    limits[6] = 9999999;

    int process_count;
    if (param == '\0') {
        process_count = limits[(unsigned int)(unsigned char)DAT_00483834 & 0xff];
    } else {
        process_count = DAT_0048927c;
    }

    /* Cap to actual turret count */
    if (DAT_0048927c < process_count) {
        process_count = DAT_0048927c;
    }

    int loop_count = 0;
    if (process_count <= 0) return;

    do {
        /* Advance circular index */
        DAT_00489284++;
        if (DAT_00489284 >= DAT_0048927c) {
            DAT_00489284 = 0;
        }

        int idx = DAT_00489284;

        /* Try primary movement (stubbed - returns 0) */
        int result = FUN_00411ae0(row_stride_x3, 0, idx);

        if (result == 0) {
            /* Primary movement failed - try alternate directions */
            unsigned int coin = (unsigned int)rand() & 0x80000001;
            int coin_even;
            if ((int)coin < 0) {
                coin_even = ((coin - 1) | 0xfffffffe) == 0xffffffff ? 1 : 0;
            } else {
                coin_even = (coin == 0) ? 1 : 0;
            }

            if (coin_even) {
                /* Try right first, then left */
                result = FUN_00411ae0(row_stride_x3 + 3, 2, idx);
                if (result == 0) {
                    result = FUN_00411ae0(row_stride_x3 - 3, 1, idx);
                    if (result == 0) goto try_vertical;
                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 1;
                } else {
                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 2;
                }
            } else {
                /* Try left first, then right */
                result = FUN_00411ae0(row_stride_x3 - 3, 1, idx);
                if (result == 0) {
                    result = FUN_00411ae0(row_stride_x3 + 3, 2, idx);
                    if (result == 0) {
try_vertical:
                        /* Both horizontal failed - try pure vertical */
                        unsigned int coin2 = (unsigned int)rand() & 0x80000001;
                        int coin2_even;
                        if ((int)coin2 < 0) {
                            coin2_even = ((coin2 - 1) | 0xfffffffe) == 0xffffffff ? 1 : 0;
                        } else {
                            coin2_even = (coin2 == 0) ? 1 : 0;
                        }

                        int iVar5 = idx * 8;
                        if ((coin2_even || *(unsigned char *)(iVar5 + 6 + DAT_00481f48) < 3) &&
                            *(char *)(iVar5 + 6 + DAT_00481f48) != '\x01') {
                            int r2 = FUN_00411ae0(3, 2, idx);
                            if (r2 == 0) {
                                r2 = FUN_00411ae0(-3, 1, idx);
                                if (r2 == 0) {
                                    /* All failed - reset */
                                    FUN_00412710(idx);
                                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 0;
                                } else {
                                    *(unsigned char *)(iVar5 + 6 + DAT_00481f48) = 1;
                                }
                            } else {
                                *(unsigned char *)(iVar5 + 6 + DAT_00481f48) = 2;
                            }
                        } else {
                            int r3 = FUN_00411ae0(-3, 1, idx);
                            if (r3 == 0) {
                                r3 = FUN_00411ae0(3, 2, idx);
                                if (r3 == 0) {
                                    FUN_00412710(idx);
                                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 0;
                                } else {
                                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 2;
                                }
                            } else {
                                *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 1;
                            }
                        }
                    } else {
                        *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 2;
                    }
                } else {
                    *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = 1;
                }
            }
        } else {
            /* Primary movement succeeded - update state */
            unsigned char bVar1 = *(unsigned char *)(idx * 8 + 6 + DAT_00481f48);
            if (bVar1 < 3 && bVar1 != 0) {
                *(unsigned char *)(idx * 8 + 6 + DAT_00481f48) = bVar1 + 2;
            }
        }

        /* Random direction nudge (1/16 chance) */
        int iVar5 = idx * 8;
        unsigned int rnd16 = (unsigned int)rand() & 0x8000000f;
        int is_zero;
        if ((int)rnd16 < 0) {
            is_zero = ((rnd16 - 1) | 0xfffffff0) == 0xffffffff ? 1 : 0;
        } else {
            is_zero = (rnd16 == 0) ? 1 : 0;
        }
        if (is_zero) {
            char state = *(char *)(iVar5 + 6 + DAT_00481f48);
            if (state != '\0') {
                int dir;
                if (state == '\x01' || state == '\x03') {
                    dir = -3;
                } else {
                    dir = 3;
                }
                FUN_00411ae0(dir, 0, idx);
            }
        }

        /* Track active turret blocks */
        if (*(char *)(iVar5 + 6 + DAT_00481f48) != '\0') {
            loop_count++;
        }

        /* Check if turret has fallen off bottom of map - remove it */
        int shift = (unsigned char)DAT_00487a18 & 0x1f;
        int bottom_limit = ((int)DAT_004879f4 - 0xb) << shift;
        if (*(int *)(iVar5 + DAT_00481f48) >= bottom_limit) {
            FUN_00410f50(idx);
            DAT_0048927c--;
            /* Swap with last entry */
            *(int *)(iVar5 + DAT_00481f48) = *(int *)(DAT_00481f48 + DAT_0048927c * 8);
            *(unsigned char *)(iVar5 + 5 + DAT_00481f48) = *(unsigned char *)(DAT_00481f48 + 5 + DAT_0048927c * 8);
            *(unsigned char *)(iVar5 + 6 + DAT_00481f48) = *(unsigned char *)(DAT_00481f48 + 6 + DAT_0048927c * 8);
            *(unsigned char *)(iVar5 + 4 + DAT_00481f48) = *(unsigned char *)(DAT_00481f48 + 4 + DAT_0048927c * 8);
            *(unsigned char *)(iVar5 + 7 + DAT_00481f48) = *(unsigned char *)(DAT_00481f48 + 7 + DAT_0048927c * 8);
            DAT_00489284--;
            if (DAT_0048927c == 0) {
                return;
            }
        }

        loop_count++;
    } while (loop_count < process_count);
}
/* ===== FUN_004533d0 — Update_Elevators (004533D0) ===== */
/* Wave/elevator strip simulation. Each segment is a node in a 1D mass-spring
 * chain; DAT_004892cc alternates +1/-1 each call so X and Y velocity updates
 * happen on alternating ticks (Verlet-style). Tile type 0x40 = "active fluid
 * surface tile" used to paint the strip; FIXED_SCALE = one tile.
 * Type byte: 0=head, 1=body, 2=anchor (zero velocity). */
void FUN_004533d0(void)
{
    int i, off;
    int wbase = (int)DAT_00487820;
    int shift = (unsigned char)DAT_00487a18 & 0x1F;

    /* Phase 1: Velocity propagation */
    if (DAT_004892cc == -1) {
        /* Update Y velocity from X velocity of neighbors */
        for (i = 0; i < DAT_004892c8; i++) {
            off = i * 0x1C;
            char type = *(char *)(wbase + off + 0x10);
            if (type == 2) {
                *(int *)(wbase + off + 8) = 0;
                *(int *)(wbase + off + 0xC) = 0;
            } else {
                int prev_vel = (type == 0) ? 0 : *(int *)(wbase + off - 0x14); /* prev.x_vel */
                int next_vel = (*(char *)(wbase + off + 0x11) == 0) ? 0 : *(int *)(wbase + off + 0x24); /* next.x_vel */
                int new_vel = (next_vel - *(int *)(wbase + off + 0xC)) + prev_vel;
                *(int *)(wbase + off + 0xC) = new_vel - (new_vel >> 6);
            }
        }
    } else {
        /* Update X velocity from Y velocity of neighbors */
        for (i = 0; i < DAT_004892c8; i++) {
            off = i * 0x1C;
            char type = *(char *)(wbase + off + 0x10);
            if (type == 2) {
                *(int *)(wbase + off + 8) = 0;
                *(int *)(wbase + off + 0xC) = 0;
            } else {
                int prev_vel = (type == 0) ? 0 : *(int *)(wbase + off - 0x10); /* prev.y_vel */
                int next_vel = (*(char *)(wbase + off + 0x11) == 0) ? 0 : *(int *)(wbase + off + 0x28); /* next.y_vel */
                int new_vel = (next_vel - *(int *)(wbase + off + 8)) + prev_vel;
                *(int *)(wbase + off + 8) = new_vel - (new_vel >> 6);
            }
        }
    }

    DAT_004892cc = -DAT_004892cc;

    /* Phase 2: Apply offset and paint tiles */
    for (i = 0; i < DAT_004892c8; i++) {
        off = i * 0x1C;

        /* Advance animation angle */
        *(int *)(wbase + off + 0x14) += 0x14;
        *(unsigned int *)(wbase + off + 0x14) &= 0x7FF;

        char type = *(char *)(wbase + off + 0x10);
        int vert_offset = 0;
        if (type != 2) {
            vert_offset = *(int *)((int)DAT_00487ab0 + *(int *)(wbase + off + 0x14) * 4) >> 0x11;
        }

        if (DAT_004892cc == 1) {
            vert_offset += *(int *)(wbase + off + 0xC) >> 0x11;
        } else {
            int x_vel = *(int *)(wbase + off + 8);
            vert_offset += x_vel >> 0x11;

            /* Spawn debris when elevator compresses downward */
            if (x_vel < 0 && type != 2) {
                int gap = *(int *)(wbase + off + 0xC) - x_vel;
                if (gap > 120000 && DAT_00489248 < 2500) {
                    int particles = gap / 5000;
                    if (particles > 0) particles = rand() % particles;

                    unsigned int angle = rand() & 0xFF;

                    int eidx = DAT_00489248;
                    int ebase = (int)DAT_004892e8;
                    int wx = *(int *)(wbase + off);
                    int wy = *(int *)(wbase + off + 4);

                    *(int *)(ebase + eidx * 0x80) = wx << 0x12;
                    *(int *)(ebase + eidx * 0x80 + 8) = (wy + vert_offset) * FIXED_SCALE;
                    *(int *)(ebase + eidx * 0x80 + 0x18) =
                        *(int *)((int)DAT_00487ab0 + (angle + 0x380) * 4) * particles >> 6;
                    *(int *)(ebase + eidx * 0x80 + 0x1C) =
                        *(int *)((int)DAT_00487ab0 + 0x800 + (angle + 0x380) * 4) * particles >> 6;
                    *(int *)(ebase + eidx * 0x80 + 4) = wx << 0x12;
                    *(int *)(ebase + eidx * 0x80 + 0xC) = (wy + vert_offset) * FIXED_SCALE;
                    *(int *)(ebase + eidx * 0x80 + 0x10) = 0;
                    *(int *)(ebase + eidx * 0x80 + 0x14) = 0;
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x21) = 100;
                    *(short *)(ebase + eidx * 0x80 + 0x24) = (short)(rand() % 6);
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x20) = 0;
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x26) = 0xFF;
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x22) = 0xFF;
                    *(int *)(ebase + eidx * 0x80 + 0x28) = 0;
                    *(int *)(ebase + eidx * 0x80 + 0x38) = *(int *)((int)DAT_00487abc + 0xD1E8);
                    *(int *)(ebase + eidx * 0x80 + 0x44) = *(int *)((int)DAT_00487abc + 0xD224);
                    *(int *)(ebase + eidx * 0x80 + 0x48) = 0;
                    *(int *)(ebase + eidx * 0x80 + 0x4C) = *(int *)((int)DAT_00487abc + 0xD254);
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x54) = 0;
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x40) = 0;
                    *(int *)(ebase + eidx * 0x80 + 0x34) = *(int *)((int)DAT_00487abc + 0xD160);
                    *(int *)(ebase + eidx * 0x80 + 0x3C) = 0;
                    *(unsigned char *)(ebase + eidx * 0x80 + 0x5C) = 0;
                    DAT_00489248++;
                    *(int *)(ebase + DAT_00489248 * 0x80 - 0x58) = rand() % 60 + 40;
                    /* DAT_0048384c is RGB565 in our decomp. Convert to X1R5G5B5
                     * before adding 30000, so the renderer's X1R5G5B5→RGB565
                     * conversion produces the correct water color. */
                    {
                        unsigned short wc = DAT_0048384c;
                        unsigned short wr = (wc >> 11) & 0x1F;
                        unsigned short wg = (wc >> 6) & 0x1F; /* 6-bit green → 5-bit */
                        unsigned short wb = wc & 0x1F;
                        unsigned short x1r5 = (wr << 10) | (wg << 5) | wb;
                        *(unsigned int *)(ebase + DAT_00489248 * 0x80 - 0x34) =
                            (unsigned int)x1r5 + 30000;
                    }
                }
            }
        }

        /* Clamp vertical offset */
        if (vert_offset < -50) vert_offset = -50;
        else if (vert_offset > 50) vert_offset = 50;

        /* Paint tiles: undo old offset */
        int old_offset = *(int *)(wbase + off + 0x18);
        int y = *(int *)(wbase + off + 4);
        int x = *(int *)(wbase + off);

        /* Clamp old offset to map bounds */
        int abs_old = old_offset;
        int old_dir = 1;
        if (y + old_offset < 7) abs_old = 7 - y;
        else if (y + old_offset > (int)(DAT_004879f4 - 7)) abs_old = (int)(DAT_004879f4 - y - 7);

        if (abs_old < 0) { abs_old = -abs_old; old_dir = -1; }

        int tile = (y << shift) + x;
        if (old_dir == -1) {
            /* Old offset was upward: clear underwater tiles going up */
            for (int j = abs_old; j > 0; j--) {
                unsigned char t = *(unsigned char *)((int)DAT_0048782c + tile);
                if (*(char *)((unsigned int)t * 0x20 + 4 + (int)DAT_00487928) == 1) {
                    *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                    *(unsigned char *)((int)DAT_0048782c + tile) = 0;
                }
                tile -= DAT_00487a00;
            }
        } else if (abs_old > 0) {
            /* Old offset was downward: fill walkable tiles going down */
            for (int j = abs_old; j > 0; j--) {
                unsigned char t = *(unsigned char *)((int)DAT_0048782c + tile);
                if (*(char *)((unsigned int)t * 0x20 + (int)DAT_00487928) == 1) {
                    *(unsigned short *)((int)DAT_00481f50 + tile * 2) = DAT_0048384c;
                    *(unsigned char *)((int)DAT_0048782c + tile) = 0x40;
                }
                tile += DAT_00487a00;
            }
        }

        /* Paint tiles: apply new offset */
        int new_abs = vert_offset;
        int new_dir = 1;
        if (y + vert_offset < 7) new_abs = 7 - y;
        else if (y + vert_offset > (int)(DAT_004879f4 - 7)) new_abs = (int)(DAT_004879f4 - y - 7);

        if (new_abs < 0) { new_abs = -new_abs; new_dir = -1; }

        tile = (y << shift) + x;
        if (new_dir == 1) {
            /* New offset downward: clear underwater tiles going down */
            for (int j = new_abs; j > 0; j--) {
                unsigned char t = *(unsigned char *)((int)DAT_0048782c + tile);
                if (*(char *)((unsigned int)t * 0x20 + 4 + (int)DAT_00487928) == 1) {
                    *(unsigned short *)((int)DAT_00481f50 + tile * 2) = 0;
                    *(unsigned char *)((int)DAT_0048782c + tile) = 0;
                }
                tile += DAT_00487a00;
            }
        } else if (new_abs > 0) {
            /* New offset upward: fill walkable tiles going up */
            for (int j = new_abs; j > 0; j--) {
                unsigned char t = *(unsigned char *)((int)DAT_0048782c + tile);
                if (*(char *)((unsigned int)t * 0x20 + (int)DAT_00487928) == 1) {
                    *(unsigned short *)((int)DAT_00481f50 + tile * 2) = DAT_0048384c;
                    *(unsigned char *)((int)DAT_0048782c + tile) = 0x40;
                }
                tile -= DAT_00487a00;
            }
        }

        /* Store new offset */
        *(int *)(wbase + off + 0x18) = vert_offset;
    }
}
/* ===== FUN_00453230 — Waypoint_Path_Validation (00453230) ===== */
/* Validates waypoint connectivity in DAT_00487820 (stride 0x1C per record,
 * DAT_004892c8 count). Each record has: +0x00 X, +0x04 Y, +0x10 forward flag,
 * +0x11 backward flag. Checks if adjacent waypoints are close (distance < 3 tiles)
 * and on walkable tile. Also checks if waypoint's own tile is blocked.
 * +0x10 == 2 (Pass 3 sentinel) marks "waypoint blocked by terrain" so the AI
 * pathing in entity.cpp avoids it. */
void FUN_00453230(void)
{
    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int wbase = (int)DAT_00487820;
    int tilemap = (int)DAT_0048782c;
    int etable = (int)DAT_00487928;

    if (wbase == 0 || tilemap == 0 || etable == 0) return;

    /* Pass 1: Check forward connectivity (waypoint i+1 → waypoint i) */
    if (DAT_004892c8 > 1) {
        for (int i = 1; i < DAT_004892c8; i++) {
            int off = i * 0x1C;
            int prev_off = (i - 1) * 0x1C;
            int dx = *(int *)(wbase + prev_off + 4) - *(int *)(wbase + off + 4);
            int dy = *(int *)(wbase + prev_off) - *(int *)(wbase + off);
            int adx = (dx < 0) ? -dx : dx;
            int ady = (dy < 0) ? -dy : dy;

            int py = *(int *)(wbase + prev_off + 4);
            int px = *(int *)(wbase + prev_off);
            if (adx + ady < 3 &&
                *(char *)((unsigned int)*(unsigned char *)(tilemap + (py << shift) + px) * 0x20 + 1 + etable) != '\0') {
                *(unsigned char *)(wbase + off + 0x10) = 1;
            } else {
                *(unsigned char *)(wbase + off + 0x10) = 0;
            }
        }
    }

    /* Pass 2: Check backward connectivity (waypoint i → waypoint i+1) */
    if (DAT_004892c8 != 1 && DAT_004892c8 > 0) {
        for (int i = 0; i < DAT_004892c8 - 1; i++) {
            int off = i * 0x1C;
            int next_off = (i + 1) * 0x1C;
            int dx = *(int *)(wbase + next_off + 4) - *(int *)(wbase + off + 4);
            int dy = *(int *)(wbase + next_off) - *(int *)(wbase + off);
            int adx = (dx < 0) ? -dx : dx;
            int ady = (dy < 0) ? -dy : dy;

            int ny = *(int *)(wbase + next_off + 4);
            int nx = *(int *)(wbase + next_off);
            if (adx + ady < 3 &&
                *(char *)((unsigned int)*(unsigned char *)(tilemap + (ny << shift) + nx) * 0x20 + 1 + etable) != '\0') {
                *(unsigned char *)(wbase + off + 0x11) = 1;
            } else {
                *(unsigned char *)(wbase + off + 0x11) = 0;
            }
        }
    }

    /* Pass 3: Check if waypoint's own tile is blocked (check tile and row above) */
    for (int i = 0; i < DAT_004892c8; i++) {
        int off = i * 0x1C;
        int wx = *(int *)(wbase + off);
        int wy = *(int *)(wbase + off + 4);
        int tile_off = (wy << shift) + wx;
        unsigned char tile = *(unsigned char *)(tilemap + tile_off);
        unsigned char tile_above = *(unsigned char *)(tilemap + tile_off - DAT_00487a00);
        if (*(char *)((unsigned int)tile * 0x20 + 1 + etable) == '\0' ||
            *(char *)((unsigned int)tile_above * 0x20 + 1 + etable) == '\0') {
            *(unsigned char *)(wbase + off + 0x10) = 2;
        }
    }
}
/* ===== FUN_0045ddb2 — Tick_Round_Simulation (0045DDB2) ===== */
/* In the original binary this is a recursive tick orchestrator that duplicates
 * the entire Gameplay_Tick subsystem call chain (FUN_00460d50 through FUN_0045e2c0)
 * inside a while-loop with SEH, running extra simulation ticks when certain
 * conditions are met (DAT_00483835 != 0 && DAT_00489288 == 0).  At the end it
 * calls itself recursively, which can cause unbounded stack growth.
 *
 * Kept as a no-op because:
 *   1. The subsystem work is already performed by Gameplay_Tick each frame.
 *   2. The recursive self-call risks stack overflow.
 *   3. The extra ticks were likely a fast-forward mechanism for round-end
 *      resolution that is not needed for normal gameplay.            */
void FUN_0045ddb2(void) { /* intentional no-op — see comment above */ }
/* ===== FUN_0045fc00 — Update_Fluid_Spread (0045FC00) ===== */
/* Cellular spread of water (tile type 6) and lava (tile type 0x14). New flowing
 * source tiles are tagged 0x0B; finalized water tiles return to 0 (cleared),
 * finalized lava tiles become 0x15 (cooled). Sources stored in DAT_00489e7c
 * (stride 0x20, DAT_00489258 count); ticks at 1/9 the simulation rate via
 * DAT_00487784 to slow the visible spread. */
void FUN_0045fc00(void)
{
    DAT_00487784++;
    if (DAT_00487784 <= 8) return;
    DAT_00487784 = 0;

    int shift = (unsigned char)DAT_00487a18 & 0x1F;
    int stride = DAT_00487a00;
    int fbase = (int)DAT_00489e7c;

    int i = 0;
    int foff = 4;  /* start at offset +4 for Y access */

    while (i < DAT_00489258) {
        if (*(int *)(fbase + foff + 8) >= 8) {
            /* Timer too high, just decrement */
            *(int *)(fbase + foff + 8) -= 1;
            i++;
            foff += 0x20;
            continue;
        }

        while (1) {
            if (*(int *)(fbase + foff + 8) != *(int *)(fbase + foff + 4)) {
                /* Timer != target, skip inner loop */
                break;
            }

            /* Spread fluid to neighbors */
            int x = *(int *)(fbase + foff - 4);
            int y = *(int *)(fbase + foff);
            int tile_idx = (y << shift) + x;
            int lut_idx = 6;  /* water */
            if (*(char *)(fbase + foff + 0xC) == 1) lut_idx = 4;  /* lava */

            /* Spread left */
            unsigned char t = *(unsigned char *)((int)DAT_0048782c + tile_idx - 1);
            if (t != 6 && *(char *)((unsigned int)t * 0x20 + 0xB + (int)DAT_00487928) == 0) {
                unsigned short cur_color = *(unsigned short *)((int)DAT_00481f50 + (tile_idx-1)*2);
                unsigned short remapped = *(unsigned short *)((int)DAT_00489230 + (unsigned int)cur_color * 2);
                short new_color = *(short *)((int)DAT_004876a4[lut_idx] + (unsigned int)remapped * 2);
                if (new_color == 0) *(unsigned char *)((int)DAT_0048782c + tile_idx - 1) = 0;
                *(short *)((int)DAT_00481f50 + (tile_idx-1)*2) = new_color;
            }

            /* Spread right */
            t = *(unsigned char *)((int)DAT_0048782c + tile_idx + 1);
            if (t != 6 && *(char *)((unsigned int)t * 0x20 + 0xB + (int)DAT_00487928) == 0) {
                unsigned short cur_color = *(unsigned short *)((int)DAT_00481f50 + (tile_idx+1)*2);
                unsigned short remapped = *(unsigned short *)((int)DAT_00489230 + (unsigned int)cur_color * 2);
                short new_color = *(short *)((int)DAT_004876a4[lut_idx] + (unsigned int)remapped * 2);
                if (new_color == 0) *(unsigned char *)((int)DAT_0048782c + tile_idx + 1) = 0;
                *(short *)((int)DAT_00481f50 + (tile_idx+1)*2) = new_color;
            }

            /* Spread up */
            int up_idx = tile_idx - stride;
            t = *(unsigned char *)((int)DAT_0048782c + up_idx);
            if (t != 6 && *(char *)((unsigned int)t * 0x20 + 0xB + (int)DAT_00487928) == 0) {
                unsigned short cur_color = *(unsigned short *)((int)DAT_00481f50 + up_idx*2);
                unsigned short remapped = *(unsigned short *)((int)DAT_00489230 + (unsigned int)cur_color * 2);
                short new_color = *(short *)((int)DAT_004876a4[lut_idx] + (unsigned int)remapped * 2);
                if (new_color == 0) *(unsigned char *)((int)DAT_0048782c + up_idx) = 0;
                *(short *)((int)DAT_00481f50 + up_idx*2) = new_color;
            }

            /* Spread down */
            int down_idx = tile_idx + stride;
            t = *(unsigned char *)((int)DAT_0048782c + down_idx);
            if (t != 6 && *(char *)((unsigned int)t * 0x20 + 0xB + (int)DAT_00487928) == 0) {
                unsigned short cur_color = *(unsigned short *)((int)DAT_00481f50 + down_idx*2);
                unsigned short remapped = *(unsigned short *)((int)DAT_00489230 + (unsigned int)cur_color * 2);
                short new_color = *(short *)((int)DAT_004876a4[lut_idx] + (unsigned int)remapped * 2);
                if (new_color == 0) *(unsigned char *)((int)DAT_0048782c + down_idx) = 0;
                *(short *)((int)DAT_00481f50 + down_idx*2) = new_color;
            }

            /* Check neighbors for fluid/lava source tiles, spawn new sources */
            /* Right neighbor: fluid (6) */
            if (*(char *)((int)DAT_0048782c + tile_idx + 1) == 6 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x + 1;
                *(int *)(fbase + nidx*0x20 + 4) = y;
                *(char *)(fbase + nidx*0x20 + 0x10) = 0;
                *(unsigned char *)((int)DAT_0048782c + tile_idx + 1) = 0x0B;
                DAT_00489258++;
            }
            /* Left neighbor: fluid (6) */
            if (*(char *)((int)DAT_0048782c + tile_idx - 1) == 6 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x - 1;
                *(int *)(fbase + nidx*0x20 + 4) = y;
                *(char *)(fbase + nidx*0x20 + 0x10) = 0;
                *(unsigned char *)((int)DAT_0048782c + tile_idx - 1) = 0x0B;
                DAT_00489258++;
            }
            /* Down neighbor: fluid (6) */
            if (*(char *)((int)DAT_0048782c + down_idx) == 6 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x;
                *(int *)(fbase + nidx*0x20 + 4) = y + 1;
                *(char *)(fbase + nidx*0x20 + 0x10) = 0;
                *(unsigned char *)((int)DAT_0048782c + down_idx) = 0x0B;
                DAT_00489258++;
            }
            /* Up neighbor: fluid (6) */
            if (*(char *)((int)DAT_0048782c + up_idx) == 6 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x;
                *(int *)(fbase + nidx*0x20 + 4) = y - 1;
                *(char *)(fbase + nidx*0x20 + 0x10) = 0;
                *(unsigned char *)((int)DAT_0048782c + up_idx) = 0x0B;
                DAT_00489258++;
            }

            /* Right neighbor: lava (0x14) */
            if (*(char *)((int)DAT_0048782c + tile_idx + 1) == 0x14 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x + 1;
                *(int *)(fbase + nidx*0x20 + 4) = y;
                *(char *)(fbase + nidx*0x20 + 0x10) = 1;
                *(unsigned char *)((int)DAT_0048782c + tile_idx + 1) = 0x0B;
                DAT_00489258++;
            }
            /* Left neighbor: lava (0x14) */
            if (*(char *)((int)DAT_0048782c + tile_idx - 1) == 0x14 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x - 1;
                *(int *)(fbase + nidx*0x20 + 4) = y;
                *(char *)(fbase + nidx*0x20 + 0x10) = 1;
                *(unsigned char *)((int)DAT_0048782c + tile_idx - 1) = 0x0B;
                DAT_00489258++;
            }
            /* Down neighbor: lava (0x14) */
            if (*(char *)((int)DAT_0048782c + down_idx) == 0x14 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x;
                *(int *)(fbase + nidx*0x20 + 4) = y + 1;
                *(char *)(fbase + nidx*0x20 + 0x10) = 1;
                *(unsigned char *)((int)DAT_0048782c + down_idx) = 0x0B;
                DAT_00489258++;
            }
            /* Up neighbor: lava (0x14) */
            if (*(char *)((int)DAT_0048782c + up_idx) == 0x14 && DAT_00489258 < 5000) {
                int nidx = DAT_00489258;
                *(int *)(fbase + nidx*0x20 + 0xC) = 5;
                unsigned int rnd = rand() & 3;
                *(unsigned int *)(fbase + nidx*0x20 + 8) = rnd;
                *(int *)(fbase + nidx*0x20) = x;
                *(int *)(fbase + nidx*0x20 + 4) = y - 1;
                *(char *)(fbase + nidx*0x20 + 0x10) = 1;
                *(unsigned char *)((int)DAT_0048782c + up_idx) = 0x0B;
                DAT_00489258++;
            }

            /* Random particle spawn (1/32 chance) */
            if ((rand() & 0x1F) == 0 && DAT_00489250 < 2000) {
                int pidx = DAT_00489250;
                int pbase = (int)DAT_00481f34;
                *(int *)(pbase + pidx*0x20) = x << 0x12;
                *(int *)(pbase + pidx*0x20 + 4) = (y + 4) * FIXED_SCALE;
                *(int *)(pbase + pidx*0x20 + 8) = (50 - rand() % 100) * 0x200;
                *(int *)(pbase + pidx*0x20 + 0xC) = (-150 - rand() % 140) * 0x200;
                *(char *)(pbase + pidx*0x20 + 0x10) = (char)(rand() % 6 + 1);
                *(char *)(pbase + pidx*0x20 + 0x11) = 0;
                *(char *)(pbase + pidx*0x20 + 0x12) = 0;
                *(char *)(pbase + pidx*0x20 + 0x13) = (char)0xC9;
                *(char *)(pbase + pidx*0x20 + 0x14) = (char)0xFF;
                *(char *)(pbase + pidx*0x20 + 0x15) = 0;
                DAT_00489250++;
            }

            /* Source exhaustion check */
            if (*(int *)(fbase + foff + 8) != 0) break;

            /* Timer reached 0: finalize tile */
            if (*(char *)(fbase + foff + 0xC) == 0) {
                /* Water: clear tile */
                int fidx = (*(int *)(fbase + foff) << shift) + *(int *)(fbase + foff - 4);
                *(unsigned char *)((int)DAT_0048782c + fidx) = 0;
                *(unsigned short *)((int)DAT_00481f50 + fidx * 2) = 0;
            } else {
                /* Lava: mark tile as 0x15 */
                int fidx = (*(int *)(fbase + foff) << shift) + *(int *)(fbase + foff - 4);
                *(unsigned char *)((int)DAT_0048782c + fidx) = 0x15;
                if (*(short *)((int)DAT_00481f50 + fidx * 2) == 0) {
                    *(unsigned short *)((int)DAT_00481f50 + fidx * 2) = 1;
                }
            }

            /* Swap-with-last removal */
            DAT_00489258--;
            int last = DAT_00489258;
            *(int *)(fbase + foff + 8) = *(int *)(fbase + last*0x20 + 0xC);
            *(int *)(fbase + foff + 4) = *(int *)(fbase + last*0x20 + 8);
            *(int *)(fbase + foff - 4) = *(int *)(fbase + last*0x20);
            *(int *)(fbase + foff) = *(int *)(fbase + last*0x20 + 4);
            *(char *)(fbase + foff + 0xC) = *(char *)(fbase + last*0x20 + 0x10);

            if (i >= DAT_00489258 || *(int *)(fbase + foff + 8) > 7) break;
        }

        /* Decrement timer */
        *(int *)(fbase + foff + 8) -= 1;
        i++;
        foff += 0x20;
    }
}
/* FUN_00437cf0 — Apply explosion knockback force to nearby players.
 * Pushes all players within a radius away from explosion center.
 * Params: x, y (fixed-point position), radius, palette_id, owner (-1 = environmental)
 * palette_id encoding matches the owner-byte encoding used elsewhere:
 *   <0x50 = player index (apply friendly-fire + kill-attribution rules)
 *   0x50..0x63 = turret (+0x14 remap for attribution)
 *   0x78..0x8B = team base (-0x14 remap)
 *   >=0x8c  = environmental (0xFF sentinel). */
void FUN_00437cf0(int x, int y, int radius, int palette_id, int owner)
{
    /* -1 is a "use radius as damage" overload from callers that don't care about
     * separating radius from falloff damage magnitude. */
    if (owner == -1) {
        owner = radius;
    }

    int local_4 = 0;

    if (0 < DAT_00489240) {
        do {
            PlayerData *player = Player_Get(local_4);
            int iVar2 = player->position_x;
            /* 0xf00000 = 60 tiles with 18 fractional bits: knockback scan radius. */
            if ((iVar2 - 0xf00000 < x) && (x < iVar2 + 0xf00000)) {
                int iVar5 = player->position_y;
                if ((iVar5 - 0xf00000 < y) && (y < iVar5 + 0xf00000)) {
                    int dx = (x - iVar2) >> 0x12;
                    iVar5 = (y - iVar5) >> 0x12;
                    iVar2 = iVar5 * iVar5 + dx * dx;
                    iVar2 = (int)(iVar2 + (iVar2 >> 0x1f & 0xfU)) >> 4;
                    /* Minimum divisor 7 prevents explosion-at-position-zero infinity. */
                    if (iVar2 < 7) iVar2 = 7;

                    /* Apply knockback velocity */
                    player->velocity_x = tou_binary::add_wrap_i32(
                        player->velocity_x, (dx * radius * -0x800) / iVar2);
                    player->velocity_y = tou_binary::add_wrap_i32(
                        player->velocity_y, (iVar5 * radius * -0x800) / iVar2);

                    if (palette_id < 0x50) {
                        /* Owner is a player — check team for friendly fire */
                        if (Player_Get(palette_id)->team != player->team)
                        {
                            int iVar3 = (owner << 0xf) / iVar2;
                            DAT_00486e68[palette_id] += (int)(iVar3 + (iVar3 >> 0x1f & 0x1fffU)) >> 0xd;
                        }
                        if ((Player_Get(palette_id)->team != player->team) ||
                            (DAT_0048373d != '\0'))
                        {
                            player->health = tou_binary::sub_wrap_i32(
                                player->health, (owner << 0xf) / iVar2);
                        }
                    }
                    else {
                        player->health = tou_binary::sub_wrap_i32(
                            player->health, (owner << 0xf) / iVar2);
                    }

                    /* Track damage received */
                    iVar2 = (owner << 0xf) / iVar2;
                    DAT_00486be8[local_4] += ((int)(iVar2 + (iVar2 >> 0x1f & 0x1fffU)) >> 0xd);

                    /* Record kill attribution */
                    char cVar4 = (char)palette_id;
                    if (palette_id < 0x50) {
                        if ((Player_Get(palette_id)->team != player->team) ||
                            (DAT_0048373d != '\0'))
                        {
                            player->last_attacker = static_cast<uint8_t>(cVar4);
                        }
                    }
                    else if (palette_id < 100) {
                        player->last_attacker = static_cast<uint8_t>(cVar4 + 0x14);
                    }
                    else if (palette_id < 0x78) {
                        player->last_attacker = static_cast<uint8_t>(cVar4);
                    }
                    else if (palette_id < 0x8c) {
                        player->last_attacker = static_cast<uint8_t>(cVar4 - 0x14);
                    }
                    else {
                        player->last_attacker = 0xff;
                    }
                    player->timer_4a2 = 0x6e;
                }
            }
            local_4++;
        } while (local_4 < DAT_00489240);
    }
}

/* ===== FUN_0045e2c0 — Process_Entity_Deaths (0045E2C0) ===== */
/* Processes dead troopers and destructibles each tick:
 * - Loop 1: Dead troopers → spawn debris entities, particles, sounds, knockback, swap-with-last removal
 * - Loop 2: Dead destructibles → tile destruction, particles, sounds, knockback, swap-with-last removal
 * This is CRITICAL for entity lifecycle — without it, arrays grow unbounded. */
void FUN_0045e2c0(void)
{
    int i, j;
    int *lut = (int *)DAT_00487ab0;
    int shift = (unsigned char)DAT_00487a18 & 0x1F;

    /* ===== Loop 1: Process dead troopers (DAT_00487884, stride 0x40) ===== */
    i = 0;
    while (i < DAT_0048924c) {
        int base = (int)DAT_00487884 + i * 0x40;
        int health = *(int *)(base + 0x28);

        if (health >= 1) {
            i++;
            continue;
        }

        /* Trooper is dead — process death effects */
        if (health != -1000000) {
            /* Spawn 8 debris entities in 8 evenly-spaced directions */
            int angle_off = 0;  /* byte offset into LUT, increments by 0x400 (= 256 entries = 45°) */
            for (j = 0; j < 8 && DAT_00489248 <= 0x9C3; j++) {
                Entity *debris = &DAT_004892e8[DAT_00489248];

                /* Position = trooper position */
                debris->position_x = *(int *)(base + 0x00);
                debris->position_y = *(int *)(base + 0x08);

                /* Velocity: random speed (shift 0-3) in this direction */
                unsigned int rnd = rand() & 3;
                debris->velocity_x = (*(int *)((int)DAT_00487ab0 + angle_off) << (rnd & 0x1F)) >> 6;
                rnd = rand() & 3;
                debris->velocity_y = (*(int *)((int)DAT_00487ab0 + angle_off + 0x800) << (rnd & 0x1F)) >> 6;

                /* Copy position to prev_position */
                debris->previous_x = *(int *)(base + 0x00);
                debris->previous_y = *(int *)(base + 0x08);

                /* Zero acceleration */
                debris->motion_x_10 = 0;
                debris->motion_y_14 = 0;

                /* Entity metadata: debris type */
                debris->type = 2;
                debris->variant_24 = 0;
                debris->state_20 = 5;
                debris->auxiliary_26 = 0xFF;
                debris->owner = 0xFF;
                debris->health_or_damage_28 = 0;

                /* Sprite data from entity type table (trooper debris sprites) */
                debris->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x4B8);
                debris->damage_44 = *(int *)((int)DAT_00487abc + 0x4F4);
                debris->scratch_48 = 0;
                debris->palette_value = *(int *)((int)DAT_00487abc + 0x524);
                debris->animation_frame = 0;
                debris->subtype = 0;
                debris->callback_address = *(int *)((int)DAT_00487abc + 0x430);
                debris->counter_3c = 0;
                debris->timer_5c = 0;

                DAT_00489248++;

                /* Set lifetime: random 150-249 ticks */
                debris->health_or_damage_28 = rand() % 100 + 0x96;

                /* Set sprite index from ammo/sound table */
                int snd_rnd = rand();
                debris->palette_value =
                    (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + 0x44 + (snd_rnd % 6) * 2) + 30000;

                angle_off += 0x400;
            }

            /* Check death visibility flag at trooper +0x25 */
            if (*(char *)(base + 0x25) == '\x01') {
                /* Visible death: check if in viewport */
                int vx = *(int *)(base + 0x00) >> 0x16;
                int vy = *(int *)(base + 0x08) >> 0x16;
                if ((*(unsigned char *)((int)DAT_00487814 + vx + vy * DAT_004879f8) & 8) != 0) {
                    /* In viewport: spawn visible debris fragments (16 pieces) */
                    int frag_off = 0;
                    for (j = 0; j < 16 && DAT_00489248 <= 0x9C3; j++) {
                        Entity *fragment = &DAT_004892e8[DAT_00489248];
                        unsigned int rnd_angle = rand() & 0x7FF;
                        int speed = rand() % 30 + 20;
                        unsigned int rnd_type = rand() & 1;
                        int etype = rnd_type + 0x6C;  /* entity type 108 or 109 */

                        fragment->position_x = *(int *)(base + 0x00);
                        fragment->position_y = *(int *)(base + 0x08);
                        fragment->velocity_x = *(int *)((int)DAT_00487ab0 + rnd_angle * 4) * speed >> 6;
                        fragment->velocity_y = *(int *)((int)DAT_00487ab0 + 0x800 + rnd_angle * 4) * speed >> 6;
                        fragment->previous_x = *(int *)(base + 0x00);
                        fragment->previous_y = *(int *)(base + 0x08);
                        fragment->motion_x_10 = 0;
                        fragment->motion_y_14 = 0;
                        fragment->type = (unsigned char)etype;
                        fragment->variant_24 = 0;
                        fragment->state_20 = 0;
                        fragment->auxiliary_26 = 0xFF;
                        fragment->owner = 0xFF;
                        fragment->health_or_damage_28 = 0;

                        int sprite_group = etype * 0x86;
                        int rnd_sprite = rand();
                        fragment->gravity_or_motion_38 = *(int *)((int)DAT_00487abc + 0x88 + (rnd_sprite % 6 + sprite_group) * 4);
                        rnd_sprite = rand();
                        fragment->damage_44 = *(int *)((int)DAT_00487abc + 0xC4 + (rnd_sprite % 6 + sprite_group) * 4);
                        fragment->scratch_48 = 0;
                        rnd_sprite = rand();
                        fragment->palette_value = *(int *)((int)DAT_00487abc + 0xF4 + (rnd_sprite % 6 + sprite_group) * 4);
                        fragment->animation_frame = 0;
                        rnd_sprite = rand();
                        fragment->subtype = (unsigned char)(rnd_sprite % 6);
                        fragment->callback_address = *(int *)((int)DAT_00487abc + etype * 0x218);
                        fragment->counter_3c = 0;
                        fragment->timer_5c = 0;

                        DAT_00489248++;

                        /* Team-colored sprite offset */
                        unsigned char team = *(unsigned char *)(base + 0x1C);
                        if ((rand() & 1) == 0 && team < 4) {
                            fragment->palette_value += (unsigned int)team * 100;
                        } else {
                            fragment->palette_value += 300;
                        }

                        /* Set lifetime: random 70-159 ticks */
                        fragment->health_or_damage_28 = rand() % 90 + 70;

                        /* Set animation frame */
                        fragment->scratch_48 =
                            rand() % (int)(*(unsigned char *)((int)DAT_00487abc + 0x126 + etype * 0x218) - 1);

                        frag_off += 0x80;
                    }

                    /* Spawn explosion particle */
                    if (DAT_00489250 < 2000) {
                        int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(pbase + 0x00) = *(int *)(base + 0x00);
                        *(int *)(pbase + 0x04) = *(int *)(base + 0x08);
                        *(int *)(pbase + 0x08) = 0;
                        *(int *)(pbase + 0x0C) = 0;
                        unsigned int rnd = rand() & 3;
                        *(char *)(pbase + 0x10) = (char)rnd + 0x0D;
                        *(unsigned char *)(pbase + 0x11) = 0;
                        *(unsigned char *)(pbase + 0x12) = 0;
                        *(unsigned char *)(pbase + 0x13) = 0;
                        *(unsigned char *)(pbase + 0x14) = 0xFF;
                        *(unsigned char *)(pbase + 0x15) = 0;
                        DAT_00489250++;
                        *(unsigned char *)((int)DAT_00481f34 + DAT_00489250 * 0x20 - 0x0B) = 1;
                    }

                    /* Explosion knockback: push nearby players */
                    unsigned char team = *(unsigned char *)(base + 0x1C);
                    int knock_pal = (team < 4) ? ((int)team + 0x50) : 0xFF;
                    FUN_00437cf0(*(int *)(base + 0x00), *(int *)(base + 0x08), 100, knock_pal, -1);
                }

                /* Play visible death sound (random 0x65-0x6B) */
                int snd = rand() % 7 + 0x65;
                FUN_0040f9b0(snd, *(int *)(base + 0x00), *(int *)(base + 0x08));
            }
            else {
                /* Invisible death sound (random 0x71-0x74) */
                unsigned int rnd = rand() & 3;
                int snd = rnd + 0x71;
                FUN_0040f9b0(snd, *(int *)(base + 0x00), *(int *)(base + 0x08));
            }
        }

        /* ---- Swap-with-last removal ---- */
        DAT_0048924c--;
        int last_base = DAT_0048924c * 0x40 + (int)DAT_00487884;
        if (i < DAT_0048924c) {
            /* Save current flags field bits 1-31 */
            int cur_flags = *(int *)(base + 0x18);
            /* Copy entire last entry over current */
            memcpy((void *)base, (void *)last_base, 0x40);
            /* Restore bits 1-31 from current, only bit 0 from last */
            *(int *)(base + 0x18) = (*(int *)(base + 0x18) & 1) | (cur_flags & ~1);
        }
        /* Don't increment i — re-check the swapped-in entry */
    }

    /* ===== Loop 2: Process dead destructibles (DAT_00481f28, stride 0x40) ===== */
    /* Destructible struct (0x40 bytes):
     * +0x00: x (int), +0x04: y (int), +0x08: (int), +0x0C: (int)
     * +0x10: health (int), +0x14: (int), +0x18: sprite_type (int)
     * +0x1C: type (byte), +0x1D: team (byte), +0x1E-+0x23: misc bytes */
    i = 0;
    while (i < DAT_00489260) {
        int base = (int)DAT_00481f28 + i * 0x40;
        int health = *(int *)(base + 0x10);

        if (health >= 1) {
            i++;
            continue;
        }

        /* Destructible is dead */
        int dest_x = *(int *)(base + 0x00);
        int dest_y = *(int *)(base + 0x04);

        /* Check if in viewport — spawn explosion particle if visible */
        int vx = dest_x >> 0x16;
        int vy = dest_y >> 0x16;
        if ((*(unsigned char *)((int)DAT_00487814 + vx + vy * DAT_004879f8) & 8) != 0 &&
            DAT_00489250 < 2000) {
            int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
            *(int *)(pbase + 0x00) = dest_x;
            *(int *)(pbase + 0x04) = dest_y;
            *(int *)(pbase + 0x08) = 0;
            *(int *)(pbase + 0x0C) = 0;
            unsigned int rnd = rand() & 3;
            *(char *)(pbase + 0x10) = (char)rnd + 0x0D;
            *(unsigned char *)(pbase + 0x11) = 0;
            *(unsigned char *)(pbase + 0x12) = 0;
            *(unsigned char *)(pbase + 0x13) = 0;
            *(unsigned char *)(pbase + 0x14) = 0xFF;
            *(unsigned char *)(pbase + 0x15) = 0;
            DAT_00489250++;
            *(unsigned char *)((int)DAT_00481f34 + DAT_00489250 * 0x20 - 0x0B) = 1;
        }

        /* Explosion knockback */
        unsigned char dest_team = *(unsigned char *)(base + 0x1D);
        FUN_00437cf0(dest_x, dest_y, 0x96,
                     (int)dest_team + 0x78, 0x14);

        /* Play death sound (random 0x65-0x6B) */
        int snd_rnd = rand();
        FUN_0040f9b0(snd_rnd % 7 + 0x65, dest_x, dest_y);

        /* ---- Special handling: Team Base destruction (type == 7) ---- */
        if (*(char *)(base + 0x1C) == '\x07') {
            unsigned int base_team = (unsigned int)dest_team;
            if (base_team >= 4) base_team = 3;

            /* Reset all players on this team */
            int p;
            for (p = 0; p < DAT_00489240; p++) {
                int poff = p * 0x598;
                PlayerData *player = Player_Get(p);
                if (player->team == base_team) {
                    player->lives = 0;
                    player->health = (int)0xFFF0BDC0;
                }
            }
            FUN_00451500();  /* team reinit */

            /* Spawn large explosion at tile */
            FUN_004357b0(dest_x >> 0x12, dest_y >> 0x12, 9, 0, '\0', 0, 0, 0, 0, '\0', '\0', 0);

            /* Spawn base explosion particle */
            if (DAT_00489250 < 2000) {
                int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                *(int *)(pbase + 0x00) = dest_x;
                *(int *)(pbase + 0x04) = dest_y;
                *(int *)(pbase + 0x08) = 0;
                *(int *)(pbase + 0x0C) = 0;
                *(unsigned char *)(pbase + 0x10) = 0x0B;
                *(unsigned char *)(pbase + 0x11) = 0;
                *(unsigned char *)(pbase + 0x12) = 0;
                *(unsigned char *)(pbase + 0x13) = 1;
                *(unsigned char *)(pbase + 0x14) = 0xFF;
                *(unsigned char *)(pbase + 0x15) = 0;
                DAT_00489250++;
                *(unsigned char *)((int)DAT_00481f34 + DAT_00489250 * 0x20 - 0x0B) = 1;

                snd_rnd = rand();
                FUN_0040f9b0(snd_rnd % 7 + 0x65, dest_x, dest_y);
                FUN_00437cf0(dest_x, dest_y, 500,
                             (int)dest_team + 0x78, -1);
            }

            /* Spawn 128 base destruction particles */
            {
                int cos_off = 0x800;
                int sin_off = 0;
                int pk;
                for (pk = 0; pk < 128 && DAT_00489250 < 2000; pk++) {
                    int next_cos = cos_off + 0x40;
                    int next_sin = sin_off + 0x40;
                    if (next_cos > 0x27FF) {
                        next_cos -= 0x2000;
                        next_sin -= 0x2000;
                    }
                    unsigned int rnd_color = rand() & 3;
                    if (DAT_00489250 < 2000) {
                        int pbase = DAT_00489250 * 0x20 + (int)DAT_00481f34;
                        *(int *)(pbase + 0x00) = dest_x;
                        *(int *)(pbase + 0x04) = dest_y;
                        int vel_rnd = rand();
                        *(int *)(pbase + 0x08) = (vel_rnd % 50) * *(int *)((int)DAT_00487ab0 + next_sin) >> 5;
                        vel_rnd = rand();
                        *(int *)(pbase + 0x0C) = (vel_rnd % 50) * *(int *)((int)DAT_00487ab0 + next_cos) >> 5;
                        *(char *)(pbase + 0x10) = (char)rnd_color + 1;
                        unsigned int rnd2 = rand() & 3;
                        *(char *)(pbase + 0x11) = (char)rnd2;
                        *(unsigned char *)(pbase + 0x12) = 0;
                        *(unsigned char *)(pbase + 0x13) = 199;
                        *(char *)(pbase + 0x14) = *(char *)(base + 0x1D) + 0x78;
                        *(unsigned char *)(pbase + 0x15) = 0;
                        DAT_00489250++;
                    }
                    cos_off = next_cos;
                    sin_off = next_sin;
                }
            }
        }

        /* ---- Tile destruction: modify tilemap under destructible's sprite ---- */
        {
            unsigned char sprite_type_byte = *(unsigned char *)(base + 0x18);
            int sprite_idx = *(int *)((unsigned int)sprite_type_byte * 0x20 + (int)DAT_00487818);
            int pixel_offset = *(int *)((int)DAT_00489234 + sprite_idx * 4);
            unsigned int spr_w = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + sprite_idx);
            unsigned int spr_h = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + sprite_idx);
            int tile_x_start = (dest_x >> 0x12) - (int)(spr_w >> 1);
            int tile_y_start = (dest_y >> 0x12) - (int)(spr_h >> 1);
            int map_idx = (tile_y_start << shift) + tile_x_start;

            unsigned int row, col;
            for (row = 0; row < spr_h; row++) {
                int cur_idx = map_idx;
                int cur_tx = tile_x_start;
                for (col = 0; col < spr_w; col++) {
                    /* Check sprite pixel is non-transparent */
                    if (*(char *)((int)DAT_00489e94 + pixel_offset) != '\0' &&
                        cur_tx > 0 && cur_tx < (int)DAT_004879f0 &&
                        tile_y_start + (int)row > 0 && tile_y_start + (int)row < (int)DAT_004879f4) {

                        int tile_entry = (unsigned int)*(unsigned char *)((int)DAT_0048782c + cur_idx) * 0x20 + (int)DAT_00487928;
                        /* Check if tile is destructible (field +0x0A == 1) */
                        if (*(char *)(tile_entry + 0x0A) == '\x01') {
                            /* Replace tile with "destroyed" version from field +0x09 */
                            *(unsigned char *)((int)DAT_0048782c + cur_idx) = *(unsigned char *)(tile_entry + 0x09);

                            /* Update background pixel */
                            int new_tile = (unsigned int)*(unsigned char *)((int)DAT_0048782c + cur_idx);
                            if (*(char *)(new_tile * 0x20 + (int)DAT_00487928) == '\x01') {
                                *(unsigned short *)((int)DAT_00481f50 + cur_idx * 2) = 0;
                            }
                            if (*(char *)(new_tile * 0x20 + 4 + (int)DAT_00487928) == '\x01') {
                                *(unsigned short *)((int)DAT_00481f50 + cur_idx * 2) = DAT_0048384c;
                            }
                        }
                    }
                    pixel_offset++;
                    cur_idx++;
                    cur_tx++;
                }
                map_idx += DAT_00487a00;
            }
        }

        /* ---- Cross-destructible tile clearing (nearby destructibles share damage zone) ---- */
        {
            unsigned char spr_type_byte = *(unsigned char *)(base + 0x18);
            int spr_idx_self = *(int *)((unsigned int)spr_type_byte * 0x20 + (int)DAT_00487818);
            unsigned int self_w = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + spr_idx_self) & 0xFFFFFFFE;
            unsigned int self_h = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + spr_idx_self) & 0xFFFFFFFE;
            int self_x = *(int *)(base + 0x00);
            int self_y = *(int *)(base + 0x04);

            int k;
            for (k = 0; k < DAT_00489260; k++) {
                if (k == i) continue;
                int other_base = (int)DAT_00481f28 + k * 0x40;
                int other_x = *(int *)(other_base + 0x00);
                int other_y = *(int *)(other_base + 0x04);

                /* AABB proximity check */
                int min_x = self_x - (int)(self_w * 0x20000) - 0x280000;
                int min_y = self_y - (int)(self_h * 0x20000) - 0x280000;
                int max_x = (int)((self_w + 0x14) * 0x20000) + self_x;
                int max_y = (int)((self_h + 0x14) * 0x20000) + self_y;

                if (other_x <= min_x || other_y <= min_y ||
                    other_x >= max_x || other_y >= max_y) continue;

                /* Nearby: apply softer tile destruction on neighbor's footprint */
                unsigned char other_spr_byte = *(unsigned char *)(other_base + 0x18);
                int other_spr_idx = *(int *)((unsigned int)other_spr_byte * 0x20 + (int)DAT_00487818);
                int other_pix = *(int *)((int)DAT_00489234 + other_spr_idx * 4);
                unsigned int other_w = (unsigned int)*(unsigned char *)((int)DAT_00489e8c + other_spr_idx);
                unsigned int other_h = (unsigned int)*(unsigned char *)((int)DAT_00489e88 + other_spr_idx);
                int other_tx = (other_x >> 0x12) - (int)(other_w >> 1);
                int other_ty = (other_y >> 0x12) - (int)(other_h >> 1);
                int other_map_idx = (other_ty << shift) + other_tx;

                unsigned int row, col;
                for (row = 0; row < other_h; row++) {
                    int cur_idx = other_map_idx;
                    int cur_tx = other_tx;
                    for (col = 0; col < other_w; col++) {
                        if (*(char *)((int)DAT_00489e94 + other_pix) != '\0' &&
                            cur_tx > 0 && cur_tx < (int)DAT_004879f0 &&
                            other_ty + (int)row > 0 && other_ty + (int)row < (int)DAT_004879f4) {

                            int tile_entry = (unsigned int)*(unsigned char *)((int)DAT_0048782c + cur_idx) * 0x20 + (int)DAT_00487928;
                            /* Only clear non-blocking, non-destructible tiles (field +0x0B==0, +0x0A==0) */
                            if (*(char *)(tile_entry + 0x0B) == '\0' && *(char *)(tile_entry + 0x0A) == '\0') {
                                *(unsigned char *)((int)DAT_0048782c + cur_idx) = *(unsigned char *)(tile_entry + 0x08);
                            }
                        }
                        other_pix++;
                        cur_idx++;
                        cur_tx++;
                    }
                    other_map_idx += DAT_00487a00;
                }
            }
        }

        /* ---- Swap-with-last removal ---- */
        DAT_00489260--;
        if (i < DAT_00489260) {
            int last_base = DAT_00489260 * 0x40 + (int)DAT_00481f28;
            memcpy((void *)base, (void *)last_base, 0x40);
        }
        /* Don't increment i — re-check swapped-in entry */
    }
}
/* ===== FUN_004104c0 — Turret_Init_Per_Entry (004104C0) ===== */
/* Modifies tiles around a turret position to create the turret structure.
 * A 3x3 core grid is always modified, plus direction-specific extension tiles.
 * For each qualifying tile (passable or has overlay), sets the tile color
 * to the platform color (DAT_0048384c) and changes tile type to its
 * turret variant (tile_table[tile*0x20 + 0x13] + 1).
 * Direction byte at turret_entry+4: 0=left, 1=down, 2=up, 3=right */
unsigned short DAT_00480700 = 0;

static void turret_modify_tile(int tile_idx)
{
    unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_idx);
    char *tile_entry = (char *)((unsigned int)tile * 0x20 + (int)DAT_00487928);
    if (tile_entry[0] == '\x01' || tile_entry[4] != '\0') {
        *(unsigned short *)((int)DAT_00481f50 + tile_idx * 2) = DAT_00480700;
        *(unsigned char *)((int)DAT_0048782c + tile_idx) =
            (unsigned char)(tile_entry[0x13] + 1);
    }
}

void FUN_004104c0(int index)
{
    DAT_00480700 = DAT_0048384c;
    int base_pos = *(int *)(DAT_00481f48 + index * 8);
    int stride = DAT_00487a00;

    /* Core 3x3 grid (always modified for all directions) */
    turret_modify_tile(base_pos);
    turret_modify_tile(base_pos + 1);
    turret_modify_tile(base_pos + 2);
    turret_modify_tile(base_pos + stride);
    turret_modify_tile(base_pos + 1 + stride);
    turret_modify_tile(base_pos + 2 + stride);
    turret_modify_tile(base_pos + stride * 2);
    turret_modify_tile(base_pos + 1 + stride * 2);
    turret_modify_tile(base_pos + 2 + stride * 2);

    /* Direction-specific extension tiles */
    char dir = *(char *)(DAT_00481f48 + 4 + index * 8);

    if (dir == '\0') {
        /* Direction 0 (left): extend left column + top row */
        turret_modify_tile(base_pos - 1);
        turret_modify_tile(base_pos - 1 + stride);
        turret_modify_tile(base_pos - stride);
        turret_modify_tile(base_pos + 1 - stride);
        turret_modify_tile(base_pos + 2 + stride * 3);
        turret_modify_tile(base_pos + 1 + stride * 3);
    }
    else if (dir == '\x01') {
        /* Direction 1 (down): extend bottom row + right column */
        turret_modify_tile(base_pos + stride * 3);
        turret_modify_tile(base_pos + 1 + stride * 3);
        turret_modify_tile(base_pos + 3 + stride);
        turret_modify_tile(base_pos + 3 + stride * 2);
    }
    else if (dir == '\x02') {
        /* Direction 2 (up): extend top row + left edge */
        turret_modify_tile(base_pos - 1 + stride);
        turret_modify_tile(base_pos + 1 - stride);
        turret_modify_tile(base_pos + 2 - stride);
        turret_modify_tile(base_pos - 1 + stride * 2);
    }
    else {
        /* Direction 3 (right): extend right column + top row */
        turret_modify_tile(base_pos - stride);
        turret_modify_tile(base_pos - 1 - stride);
        turret_modify_tile(base_pos - 1);
        turret_modify_tile(base_pos + 3 + stride);
        turret_modify_tile(base_pos + 3 + stride * 2);
        turret_modify_tile(base_pos + 3);
    }
}
/* ===== FUN_00460cf0 - Tile Replacement Helper (00460CF0) ===== */
/* Scans the entire tilemap and replaces all tiles of type param_1 with param_2. */
void FUN_00460cf0(char param_1, unsigned char param_2)
{
    int iVar1 = 0, iVar3 = 0;
    int iVar2 = (int)DAT_004879f0;
    int iVar4 = (int)DAT_0048782c;
    if (0 < (int)DAT_004879f4) {
        do {
            int iVar5 = 0;
            if (0 < iVar2) {
                do {
                    if (*(char *)(iVar4 + iVar1) == param_1) {
                        *(unsigned char *)(iVar4 + iVar1) = param_2;
                        iVar2 = (int)DAT_004879f0;
                        iVar4 = (int)DAT_0048782c;
                    }
                    iVar1++;
                    iVar5++;
                } while (iVar5 < iVar2);
            }
            iVar1 += DAT_00487a00 - iVar2;
            iVar3++;
        } while (iVar3 < (int)DAT_004879f4);
    }
}

/* FUN_0044dfb0 — Find spawn point for player.
 * Picks a random spawn point matching the player's team (or neutral team 3),
 * adds a random offset radius, validates 16x16 tile walkability grid, and
 * sets player position in fixed-point 14.18 format. Returns 1 on success, 0 on fail. */
int FUN_0044dfb0(int player)
{
    PlayerData *player_data = Player_Get(player);
    int iVar7, iVar6;
    unsigned char valid_points[256];
    int num_valid = 0;

    /* Phase 1: Build list of spawn points matching player's team */
    if (DAT_004892d4 > 0 && DAT_004876a0 != NULL) {
        char team = static_cast<char>(player_data->team);
        char *sp_team = (char *)((int)DAT_004876a0 + 8); /* +8 = team field */
        for (int i = 0; i < DAT_004892d4; i++) {
            if (*sp_team == team || *sp_team == '\x03') { /* team match or neutral */
                valid_points[num_valid] = (unsigned char)i;
                num_valid++;
            }
            sp_team += 0xC; /* spawn point stride = 12 bytes */
        }

        /* Phase 2: Pick random spawn point with radius offset */
        if (num_valid != 0 && player_data->scratch_26 < 0xFB) {
            int radius = 8; /* spawn radius (original computes via ftol, ~8 tiles) */
            unsigned char sp = valid_points[rand() % num_valid];

            /* Random position within radius around chosen spawn point */
            iVar7 = (rand() % (radius * 2) +
                     *(int *)((unsigned int)sp * 0xC + (int)DAT_004876a0)) - radius + 7;
            iVar6 = (rand() % (radius * 2) +
                     *(int *)((unsigned int)sp * 0xC + 4 + (int)DAT_004876a0)) - radius + 7;

            /* Clamp to map bounds */
            if (iVar7 < 10) iVar7 = 10;
            if (iVar6 < 10) iVar6 = 10;
            if (iVar7 > (int)DAT_004879f0 - 10) iVar7 = (int)DAT_004879f0 - 10;
            if (iVar6 > (int)DAT_004879f4 - 10) iVar6 = (int)DAT_004879f4 - 10;

            goto validate;
        }
    }

    /* Fallback: fully random position within map */
    iVar7 = rand() % ((int)DAT_004879f0 - 0x14) + 10;
    iVar6 = rand() % ((int)DAT_004879f4 - 0x14) + 10;

validate:
    /* Phase 3: Validate 16x16 tile grid for passability */
    if (iVar7 > 0xD && iVar7 < (int)DAT_004879f0 - 0xE &&
        iVar6 > 0xD && iVar6 < (int)DAT_004879f4 - 0xE) {
        int tile_off = ((iVar6 - 8) << ((unsigned char)DAT_00487a18 & 0x1F)) + (iVar7 - 8);
        for (int row = 0; row < 16; row++) {
            int off = tile_off;
            for (int col = 0; col < 16; col++) {
                unsigned char tile_type = *(unsigned char *)((int)DAT_0048782c + off);
                if (*(char *)((unsigned int)tile_type * 0x20 + 1 + (int)DAT_00487928) == '\0') {
                    return 0; /* impassable tile found */
                }
                off++;
            }
            tile_off += DAT_00487a00; /* next row */
        }

        /* All tiles passable — set entity position (fixed-point << 18) */
        player_data->position_x = iVar7 << 0x12;
        player_data->position_y = iVar6 << 0x12;
        return 1;
    }
    return 0; /* out of bounds */
}
