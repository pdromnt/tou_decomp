/*
 * stubs.cpp - Placeholder for miscellaneous undecompiled functions
 */
#include "tou.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===== Globals defined in this module ===== */
/* DAT_00487aa8 and DAT_0048781c already defined in memory.cpp */
int   DAT_0048784c = 0;        /* entity-to-entity link count */
int   DAT_00487228[16] = {0};  /* per-player pickup counter */
char  DAT_0048373d = 0;        /* friendly fire enabled flag */
float DAT_0048385c = 0.0f;     /* weather/temperature threshold */

/* ===== AoE / Explosion globals ===== */
unsigned int DAT_00483840 = 0; /* fire color match R threshold */
unsigned int DAT_00483844 = 0; /* fire color match G threshold */
unsigned int DAT_00483848 = 0; /* fire color match B threshold */

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

/* ===== Utility stubs (implement later) ===== */
/* ===== FUN_00410030 — Spawn Random Debris Particle from Top (00410030) ===== */
int FUN_00410030(void)
{
    if (DAT_00489248 >= 0x9c4) return 0;

    int iVar1 = rand();
    int iVar2 = (iVar1 % (DAT_004879f0 - 0x12) + 9) * 0x40000;
    int base = DAT_00489248 * 0x80 + (int)DAT_004892e8;

    *(int *)(base + 0x00) = iVar2;
    *(int *)(base + 0x08) = 0x380000;
    iVar1 = rand();
    *(int *)(base + 0x18) = (0xfa - iVar1 % 500) * 0x200;
    iVar1 = rand();
    *(int *)(base + 0x1c) = (iVar1 % 100 + 0x32) * 0x200;
    *(int *)(base + 0x04) = iVar2;
    *(int *)(base + 0x0c) = 0x380000;
    *(int *)(base + 0x10) = 0;
    *(int *)(base + 0x14) = 0;
    *(unsigned char *)(base + 0x21) = 0x1d;
    *(unsigned short *)(base + 0x24) = 0;
    *(unsigned char *)(base + 0x20) = 0;
    *(unsigned char *)(base + 0x26) = 0;
    *(unsigned char *)(base + 0x22) = 0xff;
    *(int *)(base + 0x28) = 0;
    *(int *)(base + 0x38) = *(int *)((int)DAT_00487abc + 0x3d40);
    *(int *)(base + 0x44) = *(int *)((int)DAT_00487abc + 0x3d7c);
    *(int *)(base + 0x48) = 0;
    *(int *)(base + 0x4c) = *(int *)((int)DAT_00487abc + 0x3dac);
    *(unsigned char *)(base + 0x54) = 0;
    *(unsigned char *)(base + 0x40) = 0;
    *(int *)(base + 0x34) = *(int *)((int)DAT_00487abc + 0x3cb8);
    *(int *)(base + 0x3c) = 0;
    *(unsigned char *)(base + 0x5c) = 0;

    DAT_00489248++;

    /* Set type/subtype bytes on the newly incremented slot */
    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1c) = 0;
    *(unsigned char *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x1b) = 0;

    return 0;
}
/* ===== FUN_004357b0 — AoE Tile Damage / Explosion Effects (004357b0) ===== */
/* Applies area-of-effect damage to tiles, spawns fire/debris particles, and handles
 * tile destruction. param_1/param_2 = tile x/y, param_3 = explosion size (sprite index),
 * param_4 = replacement tile type, param_5 = mode (-1/0/1/2), param_6..param_9 = ray origin/dir,
 * param_10 = damage mode (0=normal,1=count,2=scaled,3=special), param_11 = sound flag,
 * param_12 = team/owner index. */
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
        if (iVar6 % 0x32 == 0) {
            param_10 = -1;
        }
    }
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
        if (param_2 != 0xc) {
            param_6 = iVar17 - iVar15;
            param_7 = iVar6 - iVar7;
        }
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

        /* Spawn fire particle at center */
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

            /* Play random fire sound */
            iVar7 = rand();
            FUN_0040f9b0(iVar7 % 7 + 0x65, iVar6, iVar17);

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
            int ebase = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(ebase + 0x00) = param_1 << 0x12;
            *(int *)(ebase + 0x08) = param_2 << 0x12;
            *(int *)(ebase + 0x18) =
                *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar6 % param_7_speed) >> 7;
            *(int *)(ebase + 0x1c) =
                *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar6 % param_7_speed) >> 7;
            *(int *)(ebase + 0x04) = param_1 << 0x12;
            *(int *)(ebase + 0x0c) = param_2 << 0x12;
            *(int *)(ebase + 0x10) = 0;
            *(int *)(ebase + 0x14) = 0;
            *(unsigned char *)(ebase + 0x21) = 0;
            iVar6 = rand();
            *(short *)(ebase + 0x24) = (short)(iVar6 % 6);
            *(unsigned char *)(ebase + 0x20) = 0x32;
            *(unsigned char *)(ebase + 0x26) = 0;
            *(unsigned char *)(ebase + 0x22) = param_12;
            *(int *)(ebase + 0x28) = 0;
            *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x90);
            *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xCC);
            *(int *)(ebase + 0x48) = 0;
            *(int *)(ebase + 0x4c) = *(int *)((int)DAT_00487abc + 0xFC);
            *(unsigned char *)(ebase + 0x54) = 0;
            *(unsigned char *)(ebase + 0x40) = 2;
            *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + 0x00);
            *(int *)(ebase + 0x3c) = 0;
            *(unsigned char *)(ebase + 0x5c) = 0;
            DAT_00489248 = DAT_00489248 + 1;

            /* Set lifespan and gravity on newly incremented slot's trailing fields */
            iVar6 = rand();
            *(int *)(DAT_00489248 * 0x80 + -0x58 + (int)DAT_004892e8) = iVar6 % 100 + 0x78;
            *(int *)(DAT_00489248 * 0x80 + -0x3c + (int)DAT_004892e8) = 0x3e800;
            iVar6 = rand();
            *(unsigned int *)(DAT_00489248 * 0x80 + -0x34 + (int)DAT_004892e8) =
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
                                                int ebase = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                                                *(int *)(ebase + 0x00) = iVar15 << 0x12;
                                                *(int *)(ebase + 0x08) = iVar8 * 0x40000;
                                                *(int *)(ebase + 0x18) =
                                                    *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                *(int *)(ebase + 0x1c) =
                                                    *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                *(int *)(ebase + 0x04) = iVar15 << 0x12;
                                                *(int *)(ebase + 0x0c) = iVar8 * 0x40000;
                                                *(int *)(ebase + 0x10) = 0;
                                                *(int *)(ebase + 0x14) = 0;
                                                *(unsigned char *)(ebase + 0x21) = 2;
                                                iVar16 = rand();
                                                *(short *)(ebase + 0x24) = (short)(iVar16 % 6);
                                                *(unsigned char *)(ebase + 0x20) = 0;
                                                *(unsigned char *)(ebase + 0x26) = 0;
                                                *(unsigned char *)(ebase + 0x22) = param_12;
                                                *(int *)(ebase + 0x28) = 0;
                                                *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x4B8);
                                                *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0x4F4);
                                                *(int *)(ebase + 0x48) = 0;
                                                *(int *)(ebase + 0x4c) = *(int *)((int)DAT_00487abc + 0x524);
                                                *(unsigned char *)(ebase + 0x54) = 0;
                                                *(unsigned char *)(ebase + 0x40) = 0;
                                                *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + 0x430);
                                                *(int *)(ebase + 0x3c) = 0;
                                                *(unsigned char *)(ebase + 0x5c) = 0;
                                                DAT_00489248 = DAT_00489248 + 1;
                                                iVar16 = rand();
                                                *(int *)(DAT_00489248 * 0x80 + -0x58 + (int)DAT_004892e8) =
                                                    iVar16 % 0x32 + 0x28;
                                                *(int *)(DAT_00489248 * 0x80 + -0x3c + (int)DAT_004892e8) = 0x7d000;
                                                *(unsigned int *)(DAT_00489248 * 0x80 + -0x34 + (int)DAT_004892e8) =
                                                    *(unsigned short *)
                                                        ((int)DAT_00481f50 +
                                                         ((iVar8 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar15) * 2)
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
                                                    int eb2 = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                                                    *(int *)(eb2 + 0x00) = iVar15 << 0x12;
                                                    *(int *)(eb2 + 0x08) = iVar8 * 0x40000;
                                                    *(int *)(eb2 + 0x18) =
                                                        *(int *)((int)DAT_00487ab0 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                    *(int *)(eb2 + 0x1c) =
                                                        *(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (iVar16 % 2000) >> 10;
                                                    *(int *)(eb2 + 0x04) = iVar15 << 0x12;
                                                    *(int *)(eb2 + 0x0c) = iVar8 * 0x40000;
                                                    *(int *)(eb2 + 0x10) = 0;
                                                    *(int *)(eb2 + 0x14) = 0;
                                                    *(unsigned char *)(eb2 + 0x21) = 100;
                                                    iVar16 = rand();
                                                    *(short *)(eb2 + 0x24) = (short)(iVar16 % 6);
                                                    *(unsigned char *)(eb2 + 0x20) = 0;
                                                    *(unsigned char *)(eb2 + 0x26) = 0xff;
                                                    *(unsigned char *)(eb2 + 0x22) = param_12;
                                                    *(int *)(eb2 + 0x28) = 0;
                                                    *(int *)(eb2 + 0x38) = *(int *)((int)DAT_00487abc + 0xD1E8);
                                                    *(int *)(eb2 + 0x44) = *(int *)((int)DAT_00487abc + 0xD224);
                                                    *(int *)(eb2 + 0x48) = 0;
                                                    *(int *)(eb2 + 0x4c) = *(int *)((int)DAT_00487abc + 0xD254);
                                                    *(unsigned char *)(eb2 + 0x54) = 0;
                                                    *(unsigned char *)(eb2 + 0x40) = 0;
                                                    *(int *)(eb2 + 0x34) = *(int *)((int)DAT_00487abc + 0xD160);
                                                    *(int *)(eb2 + 0x3c) = 0;
                                                    *(unsigned char *)(eb2 + 0x5c) = 0;
                                                    DAT_00489248 = DAT_00489248 + 1;
                                                    iVar16 = rand();
                                                    *(int *)(DAT_00489248 * 0x80 + -0x58 + (int)DAT_004892e8) =
                                                        iVar16 % 0x32 + 0x28;
                                                    *(int *)(DAT_00489248 * 0x80 + -0x3c + (int)DAT_004892e8) = 0;
                                                    *(unsigned int *)(DAT_00489248 * 0x80 + -0x34 + (int)DAT_004892e8) =
                                                        *(unsigned short *)
                                                            ((int)DAT_00481f50 +
                                                             ((iVar8 << ((unsigned char)DAT_00487a18 & 0x1f)) + iVar15) * 2)
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
            iVar7 = (iVar6 % 6 + -3) * 0x40000 + param_1 * 0x40000;
            int eb3 = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(eb3 + 0x00) = iVar7;
            iVar6 = (iVar17 % 6 + -3) * 0x40000 + param_2 * 0x40000;
            *(int *)(eb3 + 0x08) = iVar6;
            *(int *)(eb3 + 0x18) =
                (int)(*(int *)((int)DAT_00487ab0 + uVar10 * 4) * (int)(uVar12 & 0xff)) >> 8;
            *(int *)(eb3 + 0x1c) =
                (int)(*(int *)((int)DAT_00487ab0 + 0x800 + uVar10 * 4) * (int)(uVar12 & 0xff)) >> 8;
            *(int *)(eb3 + 0x04) = iVar7;
            *(int *)(eb3 + 0x0c) = iVar6;
            *(int *)(eb3 + 0x10) = 0;
            *(int *)(eb3 + 0x14) = 0;
            *(unsigned char *)(eb3 + 0x21) = 2;
            *(unsigned short *)(eb3 + 0x24) = 0;
            *(unsigned char *)(eb3 + 0x20) = 5;
            *(unsigned char *)(eb3 + 0x26) = 0xff;
            *(unsigned char *)(eb3 + 0x22) = 0xff;
            *(int *)(eb3 + 0x28) = 0;
            *(int *)(eb3 + 0x38) = *(int *)((int)DAT_00487abc + 0x4B8);
            *(int *)(eb3 + 0x44) = *(int *)((int)DAT_00487abc + 0x4F4);
            *(int *)(eb3 + 0x48) = 0;
            *(int *)(eb3 + 0x4c) = *(int *)((int)DAT_00487abc + 0x524);
            *(unsigned char *)(eb3 + 0x54) = 0;
            *(unsigned char *)(eb3 + 0x40) = 0;
            *(int *)(eb3 + 0x34) = *(int *)((int)DAT_00487abc + 0x430);
            *(int *)(eb3 + 0x3c) = 0;
            *(unsigned char *)(eb3 + 0x5c) = 0;
            DAT_00489248 = DAT_00489248 + 1;

            iVar6 = rand();
            *(unsigned int *)(DAT_00489248 * 0x80 + -0x34 + (int)DAT_004892e8) =
                *(unsigned short *)((int)DAT_00487aa8 + 0x44 + (iVar6 % 6) * 2) + 30000;
            uVar10 = rand();
            uVar10 = uVar10 & 0x8000007f;
            if ((int)uVar10 < 0) {
                uVar10 = (uVar10 - 1 | 0xffffff80) + 1;
            }
            *(unsigned int *)(DAT_00489248 * 0x80 + -0x58 + (int)DAT_004892e8) = uVar10 + 0x80;

            param_6 = param_6 + 1;
            fVar3 = (float)param_6;
        }
    }
    return;
}
/* ===== FUN_00451e70 — Building/Structure Damage from Fire Particles (00451E70) ===== */
/* Checks fire particle against 9 categories of indexed entities (structures/buildings).
 * Each category has different hitbox sizes and health thresholds.
 * param_1 = fire particle index, param_2 = damage amount */
void FUN_00451e70(int param_1, int param_2)
{
    int *piVar3, *piVar4, *piVar5;
    int iVar6;
    unsigned int local_8;

    int pbase = (int)DAT_00481f34;
    int ebase = (int)DAT_004892e8;
    int *linkBase = (int *)DAT_0048781c;

    /* Category 0: DAT_00487834[0], offset +0x0000, hitbox 0x200000 x (0x140000 to 0x240000), health 0x19000 */
    local_8 = 0;
    if (DAT_00487834[0] != 0) {
        int *pIdx = linkBase;
        do {
            piVar3 = (int *)(ebase + (*pIdx) * 0x80);
            if ((((char)piVar3[0x17] == '\0') ||
                (*(char *)((int)piVar3 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar3[8] != -6))
            {
                piVar4 = (int *)(param_1 * 0x20 + pbase);
                iVar6 = *piVar4;
                if ((iVar6 - 0x200000 < *piVar3) && (*piVar3 < iVar6 + 0x200000)) {
                    iVar6 = piVar4[1];
                    if ((iVar6 - 0x140000 < piVar3[2]) && (piVar3[2] < iVar6 + 0x240000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0x19000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[0]);
    }

    /* Category 1: DAT_00487834[1], offset +0x1000, hitbox 0x200000 x 0x200000, health 1 */
    local_8 = 0;
    if (DAT_00487834[1] != 0) {
        int *pIdx = linkBase + 0x1000;
        do {
            piVar3 = (int *)(ebase + (*pIdx) * 0x80);
            if ((((char)piVar3[0x17] == '\0') ||
                (*(char *)((int)piVar3 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar3[8] != -6))
            {
                piVar4 = (int *)(param_1 * 0x20 + pbase);
                iVar6 = *piVar4;
                if ((iVar6 - 0x200000 < *piVar3) && (*piVar3 < iVar6 + 0x200000)) {
                    iVar6 = piVar4[1];
                    if ((iVar6 - 0x200000 < piVar3[2]) && (piVar3[2] < iVar6 + 0x200000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 1) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[1]);
    }

    /* Category 2: DAT_00487834[2], offset +0x2000, hitbox 0x240000 x (0x140000 to 0x300000), health 0x2ee000 */
    local_8 = 0;
    if (DAT_00487834[2] != 0) {
        int *pIdx = linkBase + 0x2000;
        do {
            piVar3 = (int *)(ebase + (*pIdx) * 0x80);
            if ((((char)piVar3[0x17] == '\0') ||
                (*(char *)((int)piVar3 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar3[8] != -6))
            {
                piVar4 = (int *)(param_1 * 0x20 + pbase);
                iVar6 = *piVar4;
                if ((iVar6 - 0x240000 < *piVar3) && (*piVar3 < iVar6 + 0x240000)) {
                    iVar6 = piVar4[1];
                    if ((iVar6 - 0x140000 < piVar3[2]) && (piVar3[2] < iVar6 + 0x300000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0x2ee000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[2]);
    }

    /* Category 3: DAT_00487834[3] (=DAT_00487840), offset +0x3000, hitbox 0x240000 x (0x140000 to 0x2c0000), health 0xfa000 */
    local_8 = 0;
    if (DAT_00487834[3] != 0) {
        int *pIdx = linkBase + 0x3000;
        do {
            iVar6 = *pIdx;
            piVar4 = (int *)(iVar6 * 0x80 + ebase);
            if (((*(char *)(iVar6 * 0x80 + 0x5c + ebase) == '\0') ||
                (*(char *)((int)piVar4 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar4[8] != -6))
            {
                piVar5 = (int *)(pbase + param_1 * 0x20);
                int iVar2 = *piVar5;
                if ((iVar2 - 0x240000 < *piVar4) && (*piVar4 < iVar2 + 0x240000)) {
                    iVar2 = piVar5[1];
                    if ((iVar2 - 0x140000 < piVar4[2]) && (piVar4[2] < iVar2 + 0x2c0000)) {
                        iVar6 = iVar6 * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0xfa000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[3]);
    }

    /* Category 4: DAT_00487834[4], offset +0x4000, hitbox 0x240000 x 0x1c0000, health 0x7d000 */
    local_8 = 0;
    if (DAT_00487834[4] != 0) {
        int *pIdx = linkBase + 0x4000;
        do {
            iVar6 = (*pIdx) * 0x80;
            piVar4 = (int *)(iVar6 + ebase);
            if (((*(char *)(iVar6 + 0x5c + ebase) == '\0') ||
                (*(char *)((int)piVar4 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar4[8] != -6))
            {
                piVar5 = (int *)(pbase + param_1 * 0x20);
                iVar6 = *piVar5;
                if ((iVar6 - 0x240000 < *piVar4) && (*piVar4 < iVar6 + 0x240000)) {
                    iVar6 = piVar5[1];
                    if ((iVar6 - 0x1c0000 < piVar4[2]) && (piVar4[2] < iVar6 + 0x1c0000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0x7d000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[4]);
    }

    /* Category 5: DAT_00487834[5], offset +0x5000, hitbox 0x2c0000 x 0x2c0000, health 0xfa000 */
    local_8 = 0;
    if (DAT_00487834[5] != 0) {
        int *pIdx = linkBase + 0x5000;
        do {
            iVar6 = *pIdx;
            piVar4 = (int *)(iVar6 * 0x80 + ebase);
            if (((*(char *)(iVar6 * 0x80 + 0x5c + ebase) == '\0') ||
                (*(char *)((int)piVar4 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar4[8] != -6))
            {
                piVar5 = (int *)(pbase + param_1 * 0x20);
                int iVar2 = *piVar5;
                if ((iVar2 - 0x2c0000 < *piVar4) && (*piVar4 < iVar2 + 0x2c0000)) {
                    iVar2 = piVar5[1];
                    if ((iVar2 - 0x2c0000 < piVar4[2]) && (piVar4[2] < iVar2 + 0x2c0000)) {
                        iVar6 = iVar6 * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0xfa000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[5]);
    }

    /* Category 6: DAT_00487834[6] (=DAT_0048784c), offset +0x6000, hitbox 0x200000 x 0x200000, health varies by subtype */
    local_8 = 0;
    if (DAT_00487834[6] != 0) {
        int *pIdx = linkBase + 0x6000;
        do {
            piVar4 = (int *)((*pIdx) * 0x80 + ebase);
            if ((((char)piVar4[0x17] == '\0') ||
                (*(char *)((int)piVar4 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar4[8] != -6))
            {
                piVar5 = (int *)(pbase + param_1 * 0x20);
                iVar6 = *piVar5;
                if ((iVar6 - 0x200000 < *piVar4) && (*piVar4 < iVar6 + 0x200000)) {
                    iVar6 = piVar5[1];
                    if ((iVar6 - 0x200000 < piVar4[2]) && (piVar4[2] < iVar6 + 0x200000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        char cVar1 = *(char *)(iVar6 + 0x40 + ebase);
                        if (cVar1 == '\0') {
                            if (*(int *)(iVar6 + 0x28 + ebase) < 12800000) return;
                            *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                            return;
                        }
                        if (cVar1 != '\x01') return;
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0x70800) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[6]);
    }

    /* Category 7: DAT_00487834[7], offset +0x7000, hitbox 0x280000 x 0x300000, health 0x465000 */
    local_8 = 0;
    if (DAT_00487834[7] != 0) {
        int *pIdx = linkBase + 0x7000;
        do {
            piVar3 = (int *)(ebase + (*pIdx) * 0x80);
            if ((((char)piVar3[0x17] == '\0') ||
                (*(char *)((int)piVar3 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               (((char)piVar3[8] != -6 && ((char)piVar3[8] != -5))))
            {
                piVar4 = (int *)(param_1 * 0x20 + pbase);
                iVar6 = *piVar4;
                if ((iVar6 - 0x280000 < *piVar3) && (*piVar3 < iVar6 + 0x280000)) {
                    iVar6 = piVar4[1];
                    if ((iVar6 - 0x300000 < piVar3[2]) && (piVar3[2] < iVar6 + 0x300000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0x465000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
        } while (local_8 < (unsigned int)DAT_00487834[7]);
    }

    /* Category 8: DAT_00487834[8], offset +0x8000, hitbox 0x240000 x 0x240000, health 0xfa000 */
    local_8 = 0;
    if (DAT_00487834[8] == 0) return;
    {
        int *pIdx = linkBase + 0x8000;
        do {
            iVar6 = (*pIdx) * 0x80;
            piVar4 = (int *)(iVar6 + ebase);
            if (((*(char *)(iVar6 + 0x5c + ebase) == '\0') ||
                (*(char *)((int)piVar4 + 0x22) != *(char *)(param_1 * 0x20 + 0x14 + pbase))) &&
               ((char)piVar4[8] != -6))
            {
                piVar5 = (int *)(pbase + param_1 * 0x20);
                iVar6 = *piVar5;
                if ((iVar6 - 0x240000 < *piVar4) && (*piVar4 < iVar6 + 0x240000)) {
                    iVar6 = piVar5[1];
                    if ((iVar6 - 0x240000 < piVar4[2]) && (piVar4[2] < iVar6 + 0x240000)) {
                        iVar6 = (*pIdx) * 0x80;
                        *(unsigned short *)(iVar6 + 0x24 + ebase) = 1;
                        *(int *)(iVar6 + 0x58 + ebase) = param_2;
                        *(int *)(iVar6 + 0x28 + ebase) += *(int *)(iVar6 + 0x58 + ebase);
                        if (*(int *)(iVar6 + 0x28 + ebase) < 0xfa000) return;
                        *(unsigned char *)(iVar6 + 0x20 + ebase) = 0xfa;
                        return;
                    }
                }
            }
            local_8++;
            pIdx++;
            if (DAT_00487834[8] <= (int)local_8) return;
        } while (1);
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
    int i, off, soff;
    char mode;

    if (DAT_004892a8 > 1) {
        if (DAT_004892a5 != '\0') return;
        DAT_004892a8--;

        /* At 1890 ticks remaining: flash warning on all players */
        if (DAT_004892a8 == 0x762) {
            if (DAT_0048764a != '\0') goto post_timer;
            for (i = 0; i < DAT_00489240; i++) {
                off = i * 0x598;
                *(unsigned char *)(DAT_00487810 + off + 0xCA) = 200;
                *(unsigned char *)(DAT_00487810 + off + 0xC9) = 200;
            }
        }

        /* At 1 tick remaining: game end based on mode */
        if (DAT_004892a8 == 1) {
            mode = *((char *)&DAT_00483740 + 1);

            if (mode < 5) {
                if (mode == 4 || mode == 2) {
                    /* Cap health to 0x1000 */
                    for (i = 0; i < DAT_00489240; i++) {
                        off = i * 0x598;
                        if (*(int *)(DAT_00487810 + off + 0x20) > 0x1000) {
                            *(int *)(DAT_00487810 + off + 0x20) = 0x1000;
                        }
                    }
                }
                else if (mode == 0) {
                    /* Kill everyone */
                    for (i = 0; i < DAT_00489240; i++) {
                        off = i * 0x598;
                        *(int *)(DAT_00487810 + off + 0x28) = 0;
                        *(int *)(DAT_00487810 + off + 0x20) = (int)0xFFF0BDC0;
                    }
                }
                else if (mode == 1) {
                    /* Team mode: find winning team by combined health+kills*maxhp */
                    int team_score[3] = {0, 0, 0};
                    if (DAT_00489240 > 0) {
                        int poff = 0; soff = 0;
                        for (i = 0; i < DAT_00489240; i++) {
                            unsigned char team = *(unsigned char *)(DAT_00487810 + poff + 0x2C);
                            if (team < 3) {
                                int hp = *(int *)(DAT_00487810 + poff + 0x20);
                                int kills = *(int *)(DAT_00487810 + poff + 0x28);
                                int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + soff + 0x28) : 1;
                                if (hp > 0) team_score[team] += hp;
                                team_score[team] += kills * max_hp;
                            }
                            poff += 0x598; soff += 0x40;
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
                            off = i * 0x598;
                            if (*(unsigned char *)(DAT_00487810 + off + 0x2C) != (unsigned char)best_team) {
                                *(int *)(DAT_00487810 + off + 0x28) = 0;
                                *(int *)(DAT_00487810 + off + 0x20) = (int)0xFFF0BDC0;
                            }
                        }
                    } else {
                        /* Tie: kill everyone */
                        for (i = 0; i < DAT_00489240; i++) {
                            off = i * 0x598;
                            *(int *)(DAT_00487810 + off + 0x28) = 0;
                            *(int *)(DAT_00487810 + off + 0x20) = (int)0xFFF0BDC0;
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
                off = i * 0x598;
                *(int *)(DAT_00487810 + off + 0x28) = 0;
                *(int *)(DAT_00487810 + off + 0x20) = (int)0xFFF0BDC0;
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
 *   team_base + 0x1010 + team*0x1000*4: projectile index array */
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
            int *ent = (int *)(DAT_00487810 + player * 0x598);
            int vp_w = *(int *)(DAT_00487810 + player * 0x598 + 0x484) + 0x28;
            int vp_h = ent[0x122] + 0x28;  /* +0x488 */
            int start_x = (ent[0] >> 0x12) - vp_w / 2;
            int start_y = (ent[1] >> 0x12) - vp_h / 2;

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
        int poff = 0;
        for (i = 0; i < DAT_00489240; i++) {
            if (*(char *)(poff + 0x24 + DAT_00487810) == '\0') {  /* alive check */
                int cx = (*(int *)(poff + DAT_00487810) >> 0x16) - 2;
                int cy = (*(int *)(poff + 4 + DAT_00487810) >> 0x16) - 2;
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
            poff += 0x598;
        }
    }

    /* Phase 4: Mark entities with bit 0x02 (3x3 coarse cells, collidable type only) */
    if (DAT_00489248 > 0) {
        int eoff = 0;
        for (i = 0; i < DAT_00489248; i++) {
            /* Check if entity type is collidable: entity_type_table[type][subtype].byte_0x130 == 1 */
            unsigned char etype = *(unsigned char *)((int)DAT_004892e8 + eoff + 0x21);
            unsigned char esub = *(unsigned char *)((int)DAT_004892e8 + eoff + 0x40);
            if (*(char *)((int)DAT_00487abc + (unsigned int)etype * 0x218 + (unsigned int)esub + 0x130) == '\x01') {
                int cx = (*(int *)((int)DAT_004892e8 + eoff) >> 0x16) - 1;
                int cy = (*(int *)((int)DAT_004892e8 + eoff + 8) >> 0x16) - 1;
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
            eoff += 0x80;
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
                FUN_00407210(cx * 0x40000, cy * 0x40000, 0, 0, dir, speed, 0xFF, '\0');
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
                    FUN_00407140(tx * 0x40000, ty * 0x40000, trooper_type);
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
                int eoff = DAT_00489248 * 0x80;
                int ebase = (int)DAT_004892e8 + eoff;
                *(int *)(ebase) = px * 0x40000;
                *(int *)(ebase + 8) = py * 0x40000;
                *(int *)(ebase + 0x18) = 0;
                *(int *)(ebase + 0x1C) = 0;
                *(int *)(ebase + 4) = px * 0x40000;
                *(int *)(ebase + 0x0C) = py * 0x40000;
                *(int *)(ebase + 0x10) = 0;
                *(int *)(ebase + 0x14) = 0;
                *(unsigned char *)(ebase + 0x21) = 0x65;
                *(unsigned short *)(ebase + 0x24) = 0;
                *(unsigned char *)(ebase + 0x20) = 0;
                *(unsigned char *)(ebase + 0x26) = 0xFF;
                *(unsigned char *)(ebase + 0x22) = 0xFF;
                *(int *)(ebase + 0x28) = 0;
                *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0xD404);
                *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xD440);
                *(int *)(ebase + 0x48) = 0;
                *(int *)(ebase + 0x4C) = *(int *)((int)DAT_00487abc + 0xD470);
                *(unsigned char *)(ebase + 0x54) = 0;
                *(unsigned char *)(ebase + 0x40) = 1;
                *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + 0xD378);
                *(int *)(ebase + 0x3C) = 0;
                *(unsigned char *)(ebase + 0x5C) = 0;
                DAT_00489248++;
                *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x58) = 100;
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
                        *(int *)(poff) = em_x * 0x40000 + lut[em_param] * 0x0C;
                        *(int *)(poff + 4) = em_y * 0x40000 + lut[(em_param + 0x200) & 0x7FF] * 0x0C;
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
            /* Flame emitter: create flame particles in DAT_00481f34 */
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
                    *(int *)(poff) = em_x * 0x40000 + lut[em_param] * 0x0C;
                    *(int *)(poff + 4) = em_y * 0x40000 + lut[(em_param + 0x200) & 0x7FF] * 0x0C;
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
                        *(unsigned int *)(poff) = (unsigned int)(((rand() & 7) - 4) + em_x) * 0x40000;
                        *(unsigned int *)(poff + 4) = (unsigned int)(((rand() & 7) - 4) + em_y) * 0x40000;
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
        } /* end switch */

        /* Self-removal: if source tile is destroyed (type '\0') and emitter is not type 2/3 */
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
 * The original calls per-entity behavior callbacks via function pointers at +0x34,
 * with SEH to catch crashes. Since callbacks reference original binary addresses,
 * we handle entity behavior inline based on entity type:
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
        int ebase = i * 0x80 + (int)DAT_004892e8;
        unsigned char ent_type = *(unsigned char *)(ebase + 0x21);
        unsigned char ent_state = *(unsigned char *)(ebase + 0x20);
        int should_remove = 0;

        /* Save previous positions */
        *(int *)(ebase + 0x04) = *(int *)(ebase + 0x00);
        *(int *)(ebase + 0x0C) = *(int *)(ebase + 0x08);

        /* Advance animation frame counter */
        unsigned char frame_ctr = *(unsigned char *)(ebase + 0x54) + 1;
        *(unsigned char *)(ebase + 0x54) = frame_ctr;

        /* Check animation wrap (simplified: wrap at 8 frames) */
        int type_entry = (unsigned int)*(unsigned char *)(ebase + 0x40) +
                         (unsigned int)ent_type * 0x218;
        unsigned char max_frame = 8;
        if (DAT_00487abc != NULL) {
            unsigned char tbl_max = *(unsigned char *)((int)DAT_00487abc + type_entry + 0x12A);
            if (tbl_max > 0) max_frame = tbl_max;
        }
        if (frame_ctr >= max_frame) {
            *(unsigned char *)(ebase + 0x54) = 0;
            *(int *)(ebase + 0x48) = *(int *)(ebase + 0x48) + 1;

            /* Check max cycles */
            unsigned char max_cycles = 0xFF;
            if (DAT_00487abc != NULL) {
                max_cycles = *(unsigned char *)((int)DAT_00487abc + type_entry + 0x124);
            }
            if (max_cycles > 0 && (unsigned int)*(int *)(ebase + 0x48) >= (unsigned int)max_cycles) {
                *(int *)(ebase + 0x48) = 0;
            }
        }

        /* === Entity behavior (inline replacement for callback at +0x34) === */

        /* Position integration: pos += vel */
        *(int *)(ebase + 0x00) += *(int *)(ebase + 0x18);
        *(int *)(ebase + 0x08) += *(int *)(ebase + 0x1C);

        /* Apply gravity for debris entities */
        if (ent_type == 2 || ent_type >= 0x6C || ent_state == 5) {
            *(int *)(ebase + 0x1C) += DAT_00483824;  /* gravity */
            /* Apply drag */
            *(int *)(ebase + 0x18) = (int)((double)*(int *)(ebase + 0x18) * 0.97);
            *(int *)(ebase + 0x1C) = (int)((double)*(int *)(ebase + 0x1C) * 0.97);
        }

        /* Boundary check */
        int pos_x = *(int *)(ebase + 0x00);
        int pos_y = *(int *)(ebase + 0x08);
        if (pos_x < 0 || pos_y < 0 ||
            pos_x >= (int)(DAT_004879f0 * 0x40000) ||
            pos_y >= (int)(DAT_004879f4 * 0x40000)) {
            should_remove = 1;
        }

        /* Wall collision for non-debris entities (projectiles) */
        if (!should_remove && ent_type == 0 && ent_state != 5) {
            int tx = pos_x >> 0x12;
            int ty = pos_y >> 0x12;
            if (tx > 0 && ty > 0 && tx < (int)DAT_004879f0 && ty < (int)DAT_004879f4) {
                int tile_off = (ty << shift) + tx;
                unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_off);
                /* Check tile passability (field +3 in tile table = 0 means solid) */
                if (*(char *)((unsigned int)tile * 0x20 + 3 + (int)DAT_00487928) == '\0') {
                    /* Hit wall: apply damage to tile and expire */
                    FUN_00451e70(i, 0x5000);
                    should_remove = 1;
                }
            }
        }

        /* Lifetime countdown */
        int lifetime = *(int *)(ebase + 0x28);
        if (lifetime > 0) {
            lifetime--;
            *(int *)(ebase + 0x28) = lifetime;
            if (lifetime <= 0) {
                should_remove = 1;
            }
        } else if (ent_state == 5 || ent_type == 2 || ent_type >= 0x6C) {
            /* Debris with no lifetime — expire immediately */
            should_remove = 1;
        }

        /* Projectiles with zero lifetime: check if they have velocity.
         * If no velocity and no lifetime, they should have been initialized wrong
         * — give them a default lifetime. */
        if (ent_type == 0 && lifetime == 0 &&
            *(int *)(ebase + 0x18) == 0 && *(int *)(ebase + 0x1C) == 0) {
            should_remove = 1;
        }

        /* === Removal via swap-with-last === */
        if (should_remove) {
            DAT_00489248--;
            if (i < DAT_00489248) {
                int last = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                memcpy((void *)ebase, (void *)last, 0x80);
            }
            /* Don't increment i — re-check swapped-in entry */
        } else {
            i++;
        }
    }
}
/* ===== FUN_004527e0 — Update_Projectiles (004527E0) ===== */
/* Updates particles in DAT_00481f34 (stride 0x20, DAT_00489250 count).
 * Each particle has: +0x00 pos_x, +0x04 pos_y, +0x08 vel_x, +0x0C vel_y,
 * +0x10 type, +0x11 frame, +0x12 sub_frame, +0x13 behavior, +0x14 owner.
 * Moves particles, advances animation, handles wall collision/ricochet,
 * applies damage to nearby entities. Expired particles are removed. */
void FUN_004527e0(void)
{
    int shift = (unsigned char)DAT_00487a18 & 0x1F;
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

        /* Determine team for collision purposes */
        unsigned char owner = *(unsigned char *)(part + 5);  /* +0x14 */

        /* Advance animation sub-frame */
        unsigned char behavior = *(unsigned char *)((int)part + 0x13);
        unsigned char sub_frame = *(unsigned char *)((int)part + 0x12) + 1;
        *(unsigned char *)((int)part + 0x12) = sub_frame;

        int frame_limit = (behavior < 200 && behavior != 0xC4) ? 5 : 9;
        if (sub_frame > (unsigned char)frame_limit) {
            *(unsigned char *)((int)part + 0x12) = 0;
            *(unsigned char *)((int)part + 0x11) = *(unsigned char *)((int)part + 0x11) + 1;
        }

        /* Check if animation has completed (particle expired) */
        unsigned char part_type = *(unsigned char *)(part + 4);  /* +0x10 */
        unsigned char max_frame = DAT_00481f20 ?
            *(unsigned char *)((int)DAT_00481f20 + 6 + (unsigned int)part_type * 8) : 10;

        if (*(unsigned char *)((int)part + 0x11) >= max_frame) {
            goto remove_particle;
        }

        /* For bullet-type particles (behavior >= 0xC4): check bounds */
        if (behavior >= 0xC4) {
            int tx = new_x >> 0x12;
            int ty = new_y >> 0x12;
            if (tx < 0 || ty < 0 || tx >= (int)DAT_004879f0 || ty >= (int)DAT_004879f4) {
                goto remove_particle;
            }

            /* Check tile walkability — bounce off walls */
            unsigned char tile = *(unsigned char *)((int)DAT_0048782c + (ty << shift) + tx);
            if (*(char *)((unsigned int)tile * 0x20 + 2 + (int)DAT_00487928) == '\0' &&
                *(char *)((unsigned int)tile * 0x20 + 10 + (int)DAT_00487928) == '\0') {
                /* Hit non-passable tile — ricochet or expire */
                if (behavior == 0xC9 || behavior == 0xC4) {
                    /* These types expire on wall hit */
                    goto remove_particle;
                }
                /* Ricochet: reverse velocity, restore position */
                int angle = FUN_004257e0(0, 0, part[2], part[3]);
                int new_angle;
                if (rand() & 1) {
                    new_angle = (angle + 0x200) & 0x7FF;
                } else {
                    new_angle = (angle - 0x200) & 0x7FF;
                }
                part[0] = old_x;
                part[1] = old_y;
                int *lut = (int *)DAT_00487ab0;
                part[2] = lut[new_angle] * 0x14 >> 6;
                part[3] = lut[(new_angle + 0x200) & 0x7FF] * 0x14 >> 6;
            }

            /* Simplified damage: check player collision via coarse grid */
            if (*(unsigned char *)((int)part + 0x12) < 2 && behavior >= 0xC4) {
                int gx = part[0] >> 0x16;
                int gy = part[1] >> 0x16;
                if (gx >= 0 && gy >= 0 && gx < (int)DAT_004879f8 && gy < (int)DAT_004879fc) {
                    unsigned char grid_byte = *(unsigned char *)((int)DAT_00487814 + gx + gy * DAT_004879f8);

                    /* Calculate base damage */
                    int base_damage = 0x5000;  /* default */
                    unsigned char ptype = *(unsigned char *)(part + 4);
                    if (ptype == 5 || ptype == 6) base_damage = 0x2800;
                    else if (ptype == 3 || ptype == 4) base_damage = 0x3C00;
                    else if (ptype == 1 || ptype == 2) base_damage = 0x7800;
                    if (behavior == 0xCD) base_damage = 0x8DC00;

                    /* Check player presence */
                    if ((grid_byte & 1) && DAT_00489240 > 0) {
                        int poff = 0;
                        for (int p = 0; p < DAT_00489240; p++) {
                            if (*(int *)(poff + 0x20 + DAT_00487810) > 0 && p != (int)owner) {
                                /* Simple AABB check */
                                int px = *(int *)(poff + DAT_00487810);
                                int py = *(int *)(poff + 4 + DAT_00487810);
                                int range = 0x40000 * 2;  /* ~2 tiles */
                                if (part[0] > px - range && part[0] < px + range &&
                                    part[1] > py - range && part[1] < py + range) {
                                    *(unsigned char *)(poff + 0xA3 + DAT_00487810) = 1;
                                    DAT_00486be8[p] += base_damage >> 0xD;

                                    /* Apply damage (check team) */
                                    if (owner < 0x50) {
                                        if (*(char *)(DAT_00487810 + 0x2C + (unsigned int)owner * 0x598) !=
                                            *(char *)(poff + 0x2C + DAT_00487810) || DAT_0048373d != '\0') {
                                            *(int *)(poff + 0x20 + DAT_00487810) -= base_damage;
                                        }
                                    } else {
                                        *(int *)(poff + 0x20 + DAT_00487810) -= base_damage;
                                    }
                                }
                            }
                            poff += 0x598;
                        }
                    }
                }
            }
        }

        /* Check max frame again after all processing */
        if (*(unsigned char *)((int)part + 0x11) >= max_frame) {
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
        /* Don't increment i — re-check this slot */
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
        param_1[2] = uVar2 - 0x40000;
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
        uVar5 = (unsigned int)(iVar6 + 0x40000);
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

/* ===== FUN_00459dd0 — Simplified Aimed Fire Direction (00459DD0) ===== */
/* The original calls FUN_004599f0 (trajectory computation) and FUN_00459c70
 * (line-of-sight validation). Since those are complex undecompiled functions,
 * we simplify to just return the direct angle to target, or 0x801 (no shot). */
static int FUN_00459dd0_simplified(int sx, int sy, int tx, int ty, float speed_factor, char use_alt)
{
    (void)speed_factor;
    (void)use_alt;
    /* Simple LOS check: verify tile at midpoint between source and target is walkable */
    int mx = (sx + tx) / 2;
    int my = (sy + ty) / 2;
    int shift = (unsigned char)DAT_00487a18 & 0x1f;
    int tile_x = mx >> 0x12;
    int tile_y = my >> 0x12;
    if (tile_x > 0 && tile_y > 0 && tile_x < (int)DAT_004879f0 && tile_y < (int)DAT_004879f4) {
        int tile_idx = (tile_y << shift) + tile_x;
        unsigned char tile = *(unsigned char *)((int)DAT_0048782c + tile_idx);
        char walkable = *(char *)((unsigned int)tile * 0x20 + 1 + (int)DAT_00487928);
        if (walkable == '\0') {
            return 0x801; /* blocked */
        }
    }
    return FUN_004257e0(sx, sy, tx, ty);
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
            int max_x = ((int)DAT_004879f0 - 4) * 0x40000;
            if (new_x > max_x) {
                *t = max_x;
            }
        }

        if (new_y < 0xc0000) {
            t[2] = 0xc0000;
        } else {
            int max_y = ((int)DAT_004879f4 - 4) * 0x40000;
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
                        int poff = 0;
                        for (int p = 0; p < DAT_00489240; p++) {
                            /* Skip dead or same-team players */
                            if (*(char *)(poff + 0x24 + DAT_00487810) == '\0' &&
                                *(char *)(poff + 0x2c + DAT_00487810) != (char)t[7]) {
                                int px = *(int *)(poff + DAT_00487810);
                                int py = *(int *)(poff + 4 + DAT_00487810);

                                if (px < range_x_hi && range_x_lo < px &&
                                    py < range_y_hi && range_y_lo < py) {
                                    int dx = (px - *t) >> 0x12;
                                    int dy = (py - t[2]) >> 0x12;
                                    int dist_sq = dx * dx + dy * dy;

                                    if (dist_sq < 40000) {
                                        /* LOS check using sin/cos LUT */
                                        int angle = FUN_004257e0(*t, t[2], px, py);
                                        int can_fire = 0;

                                        if (*(char *)((int)t + 0x25) == '\x02') {
                                            can_fire = 1; /* floor turret always fires */
                                        } else if (DAT_00487ab0 != NULL) {
                                            int sin_val = *(int *)((int)DAT_00487ab0 + angle * 4);
                                            int cos_val = *(int *)((int)DAT_00487ab0 + 0x800 + angle * 4);
                                            int check_x = (*t + sin_val * 4) >> 0x12;
                                            int check_y = (t[2] + cos_val * 4) >> 0x12;
                                            int check_tile = (check_y << shift) + check_x;
                                            unsigned char ct = *(unsigned char *)((int)DAT_0048782c + check_tile);
                                            if (*(char *)((unsigned int)ct * 0x20 + 1 + (int)DAT_00487928) == '\x01') {
                                                can_fire = 1;
                                            }
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
                            poff += 0x598;
                        }

                        if (candidate_count > 0) {
                            unsigned int pick = (unsigned int)rand() % candidate_count;
                            int chosen = candidate_indices[pick];
                            tgt_x = *(int *)(DAT_00487810 + chosen * 0x598);
                            tgt_y = *(int *)(DAT_00487810 + chosen * 0x598 + 4);
                            found_target = 1;
                        }
                    }

                    /* Note: original also searches spatial grid (DAT_00487aa4) for
                     * projectiles and other troopers. Simplified: skip those searches. */
                }

                if (found_target) {
                    /* Compute firing angle */
                    int fire_angle;
                    float speed_sqrt = (float)__builtin_sqrt(0.7142857142857143);

                    if ((char)t[7] == (char)-2) {
                        /* Random angle */
                        fire_angle = rand() & 0x7ff;
                    } else {
                        /* Aimed fire (simplified) */
                        fire_angle = FUN_00459dd0_simplified(*t, t[2] - 0x100000, tgt_x, tgt_y,
                                                            speed_sqrt,
                                                            *(char *)((int)t + 0x25) != '\0');
                    }

                    if (fire_angle != 0x801) {
                        /* Determine projectile type */
                        int proj_type = 0;
                        int proj_subtype = 2;

                        char ttype = *(char *)((int)t + 0x25);
                        if (ttype == '\0' || (char)t[7] == (char)-2 || ttype == '\x02') {
                            proj_type = 0;
                            proj_subtype = 2;
                        } else if (ttype == '\x01') {
                            proj_type = 1;
                            proj_subtype = 1;
                        }

                        /* Owner byte */
                        char owner;
                        if ((char)t[7] == (char)-2) {
                            owner = (char)-2;
                        } else {
                            owner = (char)t[7] + 0x50;
                        }

                        /* Create projectile entity in DAT_004892e8 */
                        int ebase = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                        *(int *)(ebase + 0x00) = *t;
                        *(int *)(ebase + 0x08) = t[2] - 0x100000;

                        /* Compute velocity from angle using sin/cos LUT */
                        if (DAT_00487ab0 != NULL) {
                            int sin_val = *(int *)((int)DAT_00487ab0 + fire_angle * 4);
                            int cos_val = *(int *)((int)DAT_00487ab0 + 0x800 + fire_angle * 4);
                            *(int *)(ebase + 0x18) = (int)((double)sin_val * (double)speed_sqrt * 2.3);
                            *(int *)(ebase + 0x1c) = (int)((double)cos_val * (double)speed_sqrt * 2.3);
                        } else {
                            *(int *)(ebase + 0x18) = 0;
                            *(int *)(ebase + 0x1c) = 0;
                        }

                        *(int *)(ebase + 0x04) = *t;
                        *(int *)(ebase + 0x0c) = t[2] - 0x100000;
                        *(int *)(ebase + 0x10) = 0;
                        *(int *)(ebase + 0x14) = 0;
                        *(char *)(ebase + 0x21) = (char)proj_type;
                        *(short *)(ebase + 0x24) = 0;
                        *(char *)(ebase + 0x20) = 0;
                        *(char *)(ebase + 0x26) = (char)0xfe;
                        *(char *)(ebase + 0x22) = owner;
                        *(int *)(ebase + 0x28) = 0;

                        /* Entity type config lookups */
                        int type_offset = proj_subtype + proj_type * 0x86;
                        if (DAT_00487abc != NULL) {
                            *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + type_offset * 4);
                            *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xc4 + type_offset * 4);
                            *(int *)(ebase + 0x4c) = *(int *)((int)DAT_00487abc + 0xf4 + type_offset * 4);
                            *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + proj_type * 0x218);
                        }
                        *(int *)(ebase + 0x48) = 0;
                        *(char *)(ebase + 0x54) = 0;
                        *(char *)(ebase + 0x40) = (char)proj_subtype;
                        *(int *)(ebase + 0x3c) = 0;
                        *(char *)(ebase + 0x5c) = 0;

                        DAT_00489248++;

                        /* Set projectile lifetime and damage based on type */
                        if (proj_type == 0) {
                            if (*(char *)((int)t + 0x25) == '\x02') {
                                /* Floor turret: extended lifetime */
                                int rnd = rand();
                                unsigned int life_val = rnd % 0x50 + 0x28;
                                *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                                    (life_val >> 3) + 30000 +
                                    ((life_val & 0x1fffff8) * 0x20 + (life_val & 0x3ffffff8)) * 4;
                                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x3c) = 0x1b5800;
                                *(short *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x5c) = 6;
                            } else {
                                /* Normal turret */
                                int rnd = rand();
                                if (DAT_00487aa8 != NULL) {
                                    *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                                        (unsigned int)*(unsigned short *)((int)DAT_00487aa8 + 0xf2 + (rnd % 3) * 2) + 30000;
                                } else {
                                    *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) = 30100;
                                }
                                *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x3c) = 0x32000;
                            }
                        }

                        /* Set entity behavior type */
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x48) = 6;

                        /* Store turret position as projectile origin */
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x70) = *t;
                        *(int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x6c) = t[2];
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
        DAT_00481f10 = (int)sqrtf((float)dist_sq);
        return angle;
    }

    /* High gravity: ballistic arc LUT lookup */
    float range_sq = range_sqrt * range_sqrt;
    float temp = (1024.0f / (float)gravity) * range_sq;
    int dx = (dst_x - src_x) >> 0x12;
    int dy = (dst_y - src_y) >> 0x12;
    unsigned char neg_x = 0;

    float idx_x_f = ((float)dx * 0.125f) / temp;
    float idx_y_f = ((float)dy * 0.125f) / temp + 45.0f;

    /* Absolute value of X index, track sign */
    if (idx_x_f < 0.0f) {
        idx_x_f = -idx_x_f;
        neg_x = 1;
    }

    /* Truncate to integer indices */
    int ix = (int)idx_x_f;
    int iy = (int)idx_y_f;

    /* Bounds check: ix in [0,44], iy in [0,89] */
    if (ix < 0 || ix > 44 || iy < 0 || iy > 89)
        return 0x801;

    /* Fractional parts for bilinear interpolation */
    float frac_x = 1.0f - (idx_x_f - (float)ix);
    float frac_y = 1.0f - (idx_y_f - (float)iy);

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

    /* Read 4 velocity-X entries for bilinear interpolation */
    float vx_00 = (float)(unsigned short)lut[base0];           /* (ix, iy) */
    float vx_01 = (float)(unsigned short)lut[base0 + 4];       /* (ix, iy) + 4 */
    float vx_10 = (float)(unsigned short)lut[base0 + 188];     /* (ix, iy) + 0xBC */
    float vx_11 = (float)(unsigned short)lut[base1];            /* (ix, iy+1) */

    /* Bilinear interpolation for X velocity */
    float interp_vx = vx_11 * frac_x + vx_10 * frac_y;
    interp_vx = interp_vx * frac_y;
    float tmp2 = vx_01 * frac_x + vx_00 * frac_y;
    interp_vx += tmp2 * frac_x;

    /* Read 4 velocity-Y entries (offset by 190, 186, 6, 2 in word indices) */
    float vy_00 = (float)(unsigned short)lut[base0 + 190];
    float vy_01 = (float)(unsigned short)lut[base0 + 186];
    float vy_10 = (float)(unsigned short)lut[base0 + 6];
    float vy_11 = (float)(unsigned short)lut[base0 + 2];

    float interp_vy = vy_00 * frac_x + vy_01 * frac_y;
    interp_vy = interp_vy * frac_y;
    float tmp3 = vy_10 * frac_x + vy_11 * frac_y;
    interp_vy += tmp3 * frac_x;

    DAT_00481f10 = (int)(interp_vy * 1024.0f);

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
    /* Compute velocity components from angle using the sin/cos LUT */
    int *math_lut = (int *)DAT_00487ab0;
    int vx = math_lut[angle];                    /* cos component */
    int vy = math_lut[angle + 0x200];            /* sin component (offset 0x800 bytes / 4) */

    /* Compute step count based on gravity */
    unsigned int steps;
    if (gravity < 0x50) {
        steps = (unsigned int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 3)) >> 2);
    } else {
        steps = (unsigned int)(DAT_00481f10 / (int)range_sqrt);
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

    /* Predict target position at lead time mult1 */
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
        dist1 = (int)((float)DAT_00481f00 / range_sqrt);
    }
    DAT_00481f00 = dist1;

    if (DAT_00481ed0 < 0x50) {
        DAT_00481f04 = (int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3);
    } else {
        DAT_00481f04 = (int)((float)DAT_00481f10 / range_sqrt);
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
 * aim slewing, predictive aim, projectile spawning, muzzle flash particles. */
void FUN_00458010(void)
{
    int i = 0;
    if (DAT_00489260 <= 0) return;

    do {
        int off = i * 0x40;
        unsigned char type = *(unsigned char *)(off + 0x1c + (int)DAT_00481f28);

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

            /* Set gravity for this weapon type */
            DAT_00481ed0 = -(int)((unsigned int)(*(char *)(off + 0x1c + (int)DAT_00481f28) != '\x06')) & DAT_00483828;

            /* Increment animation counter */
            char *anim_ctr = (char *)(off + 0x23 + (int)DAT_00481f28);
            *anim_ctr = *anim_ctr + 1;

            if (*(unsigned char *)(off + 0x23 + (int)DAT_00481f28) > 10) {
                *(unsigned char *)(off + 0x23 + (int)DAT_00481f28) = 0;

                /* ---- Target acquisition: scan players ---- */
                int best_dist = 2000000000;
                int best_target_idx = 0xfa;   /* sentinel = no target */
                unsigned int best_side = 0;

                if (DAT_00489240 > 0) {
                    int p = 0;
                    int p_off = 0;
                    int player_base = DAT_00487810;
                    do {
                        /* Skip same-team players and dead/inactive players */
                        if (*(char *)(off + 0x1d + (int)DAT_00481f28) != *(char *)(p_off + 0x2c + player_base) &&
                            *(char *)(p_off + 0x24 + player_base) == '\0') {

                            int src_x = *(int *)(off + (int)DAT_00481f28);
                            int src_y = *(int *)(off + 4 + (int)DAT_00481f28);
                            int tgt_x = *(int *)(p_off + player_base);
                            int tgt_y = *(int *)(p_off + 4 + player_base);
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
                                    angle = FUN_004599f0(src_x, src_y, tgt_x, tgt_y, 1, fRange, DAT_00481ed0);
                                    if (angle == 0x801 ||
                                        FUN_00459c70(src_x, src_y, tgt_x, tgt_y, angle, fRange, DAT_00481ed0) == '\0') {
                                        goto next_player;
                                    }
                                    side = 1;
                                }
                                best_target_idx = p;
                                best_side = (unsigned int)side;
                                best_dist = dist;
                            }
                        }
next_player:
                        p++;
                        p_off += 0x598;
                    } while (p < DAT_00489240);

                    if (best_target_idx != 0xfa) {
                        /* Found a player target */
                        *(int *)(off + 0x18 + (int)DAT_00481f28) = (int)sqrtf((float)best_dist);

                        DAT_00481edc = *(int *)(DAT_00487810 + best_target_idx * 0x598);
                        int tgt_off = DAT_00487810 + best_target_idx * 0x598;
                        DAT_00481ee0 = *(int *)(tgt_off + 4);
                        DAT_00481ef4 = *(int *)(tgt_off + 0x10);
                        DAT_00481ef8 = *(int *)(tgt_off + 0x14);
                        DAT_00481ed8 = (char)best_side;
                        *(unsigned char *)(off + 0x21 + (int)DAT_00481f28) = 1;

                        int aim_angle = FUN_004599f0(
                            *(int *)(off + (int)DAT_00481f28),
                            *(int *)(off + 4 + (int)DAT_00481f28),
                            DAT_00481edc, DAT_00481ee0,
                            (int)(unsigned char)DAT_00481ed8, fRange, DAT_00481ed0);
                        *(int *)(off + 0xc + (int)DAT_00481f28) = aim_angle;

                        /* Predictive aim for non-type-3 weapons on low arc */
                        if (*(char *)(off + 0x1c + (int)DAT_00481f28) != '\x03' && DAT_00481ed8 == 0) {
                            if ((int)DAT_00481ed0 < 0x50) {
                                DAT_00481efc = (int)((DAT_00481f10 + (DAT_00481f10 >> 31 & 7)) >> 3);
                            } else {
                                DAT_00481efc = (int)((float)DAT_00481f10 / fRange);
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
                            *(int *)(off + 0x18 + (int)DAT_00481f28) = (int)sqrtf((float)best_dist);
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
                        int muzzle_dx = (int)((float)cos_val * fRange);
                        int muzzle_dy = (int)((float)sin_val * fRange);

                        /* --- Spawn projectile based on weapon type --- */
                        int *ent_types = (int *)DAT_00487abc;

                        if (wtype == 0 || wtype == 1) {
                            *(unsigned char *)(off + 0x1f + (int)DAT_00481f28) = 3;
                            if (DAT_00489248 < 0x9c4) {
                                int e = DAT_00489248 * 0x80;
                                int eb = (int)DAT_004892e8;
                                *(int *)(e + eb) = *(int *)(off + (int)DAT_00481f28) + muzzle_dx;
                                *(int *)(e + 8 + eb) = *(int *)(off + 4 + (int)DAT_00481f28) + muzzle_dy;
                                *(int *)(e + 0x18 + eb) = cos_val;
                                *(int *)(e + 0x1c + eb) = sin_val;
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
                                *(int *)(e + 0x18 + eb) = cos_val;
                                *(int *)(e + 0x1c + eb) = sin_val;
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
                                *(int *)(e + 0x18 + eb) = cos_val;
                                *(int *)(e + 0x1c + eb) = sin_val;
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
                                *(int *)(e + 0x18 + eb) = cos_val;
                                *(int *)(e + 0x1c + eb) = sin_val;
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
                                *(int *)(e + 0x18 + eb) = cos_val;
                                *(int *)(e + 0x1c + eb) = sin_val;
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
                particle_team = *(char *)(DAT_00487810 + 0x2c + (unsigned int)owner * 0x598);
            }
            else {
                particle_team = (char)-5;
            }

            unsigned int link_idx = 0;
            if ((unsigned int)DAT_0048784c != 0) {
                int *link_table = (int *)((int)DAT_0048781c + 0x18000);
                do {
                    int ent_idx = link_table[link_idx];
                    int *ent = (int *)(ent_idx * 0x80 + (int)DAT_004892e8);

                    /* Check team differs */
                    unsigned char ent_owner = *(unsigned char *)((int)ent + 0x22);
                    char ent_team = *(char *)(DAT_00487810 + 0x2c + (unsigned int)ent_owner * 0x598);

                    if (ent_team != particle_team) {
                        /* AABB check: particle within +/- 0x12C0000 of entity */
                        if (((int)(new_x - 0x12C0000) < ent[0]) &&
                            (ent[0] < (int)(new_x + 0x12C0000)) &&
                            ((int)(new_y - 0x12C0000) < ent[2]) &&
                            (ent[2] < (int)(new_y + 0x12C0000)))
                        {
                            int ent_off = ent_idx * 0x80;
                            int angle = FUN_004257e0(
                                *(int *)(ent_off + (int)DAT_004892e8),
                                *(int *)(ent_off + 8 + (int)DAT_004892e8),
                                (int)new_x, (int)new_y);

                            if (*(char *)(ent_off + 0x40 + (int)DAT_004892e8) == '\0') {
                                /* Repel: vel -= lut[angle] >> 1 */
                                p[2] = (unsigned int)((int)p[2] - (*(int *)((int)DAT_00487ab0 + angle * 4) >> 1));
                                p[3] = (unsigned int)((int)p[3] - (*(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 1));
                            }
                            else {
                                /* Attract: vel += lut[angle] >> 2 */
                                p[2] = (unsigned int)((int)p[2] + (*(int *)((int)DAT_00487ab0 + angle * 4) >> 2));
                                p[3] = (unsigned int)((int)p[3] + (*(int *)((int)DAT_00487ab0 + 0x800 + angle * 4) >> 2));
                            }
                            break;
                        }
                    }

                    link_idx++;
                } while (link_idx < (unsigned int)DAT_0048784c);
            }
        }

        /* 5. Random pixel jitter */
        int jitter = rand() % 0xc;
        if (jitter == 0) {
            p[0] = p[0] - 0x40000;
        }
        else if (jitter == 1) {
            p[0] = p[0] + 0x40000;
        }
        else if (jitter == 2) {
            p[1] = p[1] - 0x40000;
        }
        else if (jitter == 3) {
            p[1] = p[1] + 0x40000;
        }

        /* 6. Boundary check: remove if out of map */
        int tile_x = (int)p[0] >> 0x12;
        int tile_y = (int)p[1] >> 0x12;
        if ((int)(p[0] & 0xfffc0000) < 0x40000 ||
            tile_x >= (int)(DAT_004879f0 - 1) ||
            (int)(p[1] & 0xfffc0000) < 0x40000 ||
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
                unsigned int pi2 = 0;
                if (DAT_00489240 > 0) {
                    int poff = 0;
                    int *dmg_ptr = &DAT_00486be8[0];
                    int pbase = DAT_00487810;
                    do {
                        int *hp_ptr = (int *)(poff + 0x20 + pbase);
                        if (*hp_ptr > 0 && pi2 != (unsigned char)p[5]) {
                            /* AABB check: player within +/- 0x2C0000 */
                            if ((*(int *)(poff + pbase) - 0x2C0000 < (int)p[0]) &&
                                ((int)p[0] < *(int *)(poff + pbase) + 0x2C0000))
                            {
                                int player_y = *(int *)(poff + 4 + pbase);
                                if ((player_y - 0x2C0000 < (int)p[1]) &&
                                    ((int)p[1] < player_y + 0x2C0000))
                                {
                                    if (stype < 0x0f) {
                                        /* Light fire: damage 0x1928, stat +1 */
                                        *dmg_ptr = *dmg_ptr + 1;
                                        unsigned char fire_owner = (unsigned char)p[5];
                                        if (fire_owner < 0x50) {
                                            if (*(char *)(pbase + 0x2c + (unsigned int)fire_owner * 0x598) !=
                                                *(char *)(poff + 0x2c + pbase)) {
                                                DAT_00486e68[fire_owner] = DAT_00486e68[fire_owner] + 1;
                                            }
                                            if (*(char *)(pbase + 0x2c + (unsigned int)(unsigned char)p[5] * 0x598) !=
                                                *(char *)(poff + 0x2c + pbase) || DAT_0048373d != '\0') {
                                                *hp_ptr = *hp_ptr - 0x1928;
                                            }
                                        }
                                        else {
                                            *hp_ptr = *hp_ptr - 0x1928;
                                        }
                                        /* Random knockback (1/500 chance) */
                                        int rk = rand();
                                        pbase = DAT_00487810;
                                        if (rk % 500 == 0) {
                                            unsigned int heading = (unsigned int)(*(int *)(poff + 0x18 + DAT_00487810) - 0x400) & 0x7ff;
                                            *(int *)(poff + 0x10 + DAT_00487810) =
                                                *(int *)(poff + 0x10 + DAT_00487810) +
                                                *(int *)((int)DAT_00487ab0 + heading * 4);
                                            int *push_y = (int *)(poff + 0x14 + DAT_00487810);
                                            *push_y = *push_y + *(int *)((int)DAT_00487ab0 + 0x800 + heading * 4);
                                            pbase = DAT_00487810;
                                        }
                                    }
                                    else {
                                        /* Heavy fire: damage 0x6400, stat +3 */
                                        *dmg_ptr = *dmg_ptr + 3;
                                        unsigned char fire_owner = (unsigned char)p[5];
                                        if (fire_owner < 0x50) {
                                            if (*(char *)(pbase + 0x2c + (unsigned int)fire_owner * 0x598) !=
                                                *(char *)(poff + 0x2c + pbase)) {
                                                DAT_00486e68[fire_owner] = DAT_00486e68[fire_owner] + 3;
                                            }
                                            if (*(char *)(pbase + 0x2c + (unsigned int)(unsigned char)p[5] * 0x598) !=
                                                *(char *)(poff + 0x2c + pbase) || DAT_0048373d != '\0') {
                                                *hp_ptr = *hp_ptr - 0x6400;
                                                pbase = DAT_00487810;
                                            }
                                        }
                                        else {
                                            *hp_ptr = *hp_ptr - 0x6400;
                                            pbase = DAT_00487810;
                                        }
                                    }
                                    /* Set attacker and damage indicator */
                                    unsigned char att_owner = (unsigned char)p[5];
                                    if (att_owner < 0x50) {
                                        if (*(char *)(pbase + 0x2c + (unsigned int)att_owner * 0x598) !=
                                            *(char *)(poff + 0x2c + pbase) || DAT_0048373d != '\0') {
                                            *(unsigned char *)(poff + 0x4a1 + pbase) = att_owner;
                                            pbase = DAT_00487810;
                                        }
                                    }
                                    else {
                                        *(unsigned char *)(poff + 0x4a1 + pbase) = 0xff;
                                        pbase = DAT_00487810;
                                    }
                                    *(unsigned char *)(poff + 0x4a2 + pbase) = 0x6e;
                                    pbase = DAT_00487810;
                                }
                            }
                        }

                        pi2++;
                        dmg_ptr++;
                        poff += 0x598;
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
        int half_w = (DAT_00489e8c ? ((unsigned int)*(unsigned char *)((int)DAT_00489e8c + sprite_idx) * 0x40000) / 2 : 0x40000);
        int half_h = (DAT_00489e88 ? ((unsigned int)*(unsigned char *)((int)DAT_00489e88 + sprite_idx) * 0x40000) / 2 : 0x40000);

        /* Check each player for AABB collision */
        int item_x = *(int *)(base - 4);
        int item_y = *(int *)(base);

        for (int p = 0; p < DAT_00489240; p++) {
            int poff = p * 0x598;
            int ship_half = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x38) / 2 : 0x40000;

            if (*(int *)(poff + 0x20 + DAT_00487810) > 0) {
                int px = *(int *)(poff + DAT_00487810);
                int py = *(int *)(poff + 4 + DAT_00487810);

                if (item_x - ship_half - half_w < px && px < item_x + ship_half + half_w &&
                    item_y - ship_half - half_h < py && py < item_y + ship_half + half_h) {
                    /* Hit! Apply effect based on type */
                    FUN_0040f9b0(0x79, item_x, item_y);
                    DAT_00487228[p]++;

                    if (item_anim_type == 0) {
                        /* Health pack */
                        *(int *)(poff + 0x20 + DAT_00487810) += 0x5DC000;
                        *(unsigned char *)(poff + 0xCA + DAT_00487810) = 0x14;
                        *(unsigned char *)(poff + 0xC9 + DAT_00487810) = 200;
                    }
                    else if (item_anim_type == 1) {
                        /* Large health pack */
                        *(int *)(poff + 0x20 + DAT_00487810) += 0x9C4000;
                        *(unsigned char *)(poff + 0xCA + DAT_00487810) = 0x15;
                        *(unsigned char *)(poff + 0xC9 + DAT_00487810) = 200;
                    }

                    /* Cap health to max */
                    int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + p * 0x40 + 0x28) : 0;
                    if (*(int *)(poff + 0x20 + DAT_00487810) > max_hp) {
                        *(int *)(poff + 0x20 + DAT_00487810) = max_hp;
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
 * Also restores player health up to max. */
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
        int poff = 0;
        int soff = 0;
        for (int p = 0; p < DAT_00489240; p++) {
            if (*(int *)(DAT_00487810 + poff + 0x20) > 0) {  /* alive check */
                int dx = (exp_ent[0] - *(int *)(DAT_00487810 + poff)) >> 0x12;
                int dy = (exp_ent[1] - *(int *)(DAT_00487810 + poff + 4)) >> 0x12;

                if (dx * dx + dy * dy < 0x1C2) {  /* within ~21 tiles radius */
                    /* Calculate angle from explosion to player */
                    int angle = FUN_004257e0(exp_ent[0], exp_ent[1],
                                            *(int *)(DAT_00487810 + poff),
                                            *(int *)(DAT_00487810 + poff + 4));
                    unsigned int push_angle = ((unsigned int)angle + 0x200) & 0x7FF;

                    /* Push player away from explosion */
                    int *lut = (int *)DAT_00487ab0;
                    *(int *)(DAT_00487810 + poff + 0x10) += lut[push_angle] >> 6;
                    *(int *)(DAT_00487810 + poff + 0x14) += lut[push_angle + 0x200] >> 6;

                    /* Also apply reverse force (original has both directions) */
                    *(int *)(DAT_00487810 + poff + 0x10) -= lut[angle] >> 6;
                    *(int *)(DAT_00487810 + poff + 0x14) -= lut[(unsigned int)angle + 0x200] >> 6;

                    *(unsigned char *)(DAT_00487810 + poff + 0xA1) = 1;

                    /* Heal player by damage_flag * 0x800 */
                    *(int *)(DAT_00487810 + poff + 0x20) += (unsigned int)DAT_00483754[1] * 0x800;

                    /* Cap at max health */
                    int max_hp = DAT_0048780c ? *(int *)((int)DAT_0048780c + soff + 0x28) : 0;
                    if (*(int *)(DAT_00487810 + poff + 0x20) < max_hp) {
                        *(unsigned char *)(DAT_00487810 + poff + 0xA1) = 2;
                    } else {
                        *(int *)(DAT_00487810 + poff + 0x20) = max_hp;
                    }
                }
            }
            poff += 0x598;
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
void FUN_004573e0(void)
{
    int i, off;

    /* Loop 1: Advance animation frame for all segments */
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

        /* Tile destruction when timer == 0 */
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
                    char *pcTeam = (char *)((int)DAT_00487810 + 0x2C);
                    do {
                        if (pcTeam[-8] == '\0') {
                            char doorTeam = *(char *)(off + 0x19 + iVar8);
                            if (*pcTeam == doorTeam || doorTeam == '\x03') {
                                int dy = (*(int *)(pcTeam - 0x28) >> 0x12) -
                                         *(int *)(off + 4 + iVar8);
                                int dx = (*(int *)(pcTeam - 0x2C) >> 0x12) -
                                         *(int *)(off + iVar8);
                                int dist = (int)sqrt((double)(dx * dx + dy * dy));
                                if (dist > 0x4F) dist = 0x4F;
                                else if (dist < 0) dist = 0;
                                score += (0x4F - dist);
                            }
                        }
                        pcTeam += 0x598;
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
                    char *pcTeam = (char *)((int)DAT_00487810 + 0x2C);
                    do {
                        if (pcTeam[-8] == '\0') {
                            char doorTeam = *(char *)(off + 0x19 + iVar8);
                            if (*pcTeam == doorTeam || doorTeam == '\x03') {
                                int dy = (*(int *)(pcTeam - 0x28) >> 0x12) -
                                         *(int *)(off + 4 + iVar8);
                                int dx = (*(int *)(pcTeam - 0x2C) >> 0x12) -
                                         *(int *)(off + iVar8);
                                int dist = (int)sqrt((double)(dx * dx + dy * dy));
                                if ((double)dist < 96.0) {
                                    triggered = 1;
                                }
                            }
                        }
                        pcTeam += 0x598;
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
                    *(int *)(ebase + eidx * 0x80 + 8) = (wy + vert_offset) * 0x40000;
                    *(int *)(ebase + eidx * 0x80 + 0x18) =
                        *(int *)((int)DAT_00487ab0 + (angle + 0x380) * 4) * particles >> 6;
                    *(int *)(ebase + eidx * 0x80 + 0x1C) =
                        *(int *)((int)DAT_00487ab0 + 0x800 + (angle + 0x380) * 4) * particles >> 6;
                    *(int *)(ebase + eidx * 0x80 + 4) = wx << 0x12;
                    *(int *)(ebase + eidx * 0x80 + 0xC) = (wy + vert_offset) * 0x40000;
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
                    *(unsigned int *)(ebase + DAT_00489248 * 0x80 - 0x34) =
                        (DAT_0048384c & 0xFFFF) + 30000;
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
 * and on walkable tile. Also checks if waypoint's own tile is blocked. */
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
                *(int *)(pbase + pidx*0x20 + 4) = (y + 4) * 0x40000;
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
 * Params: x, y (fixed-point position), radius, palette_id, owner (-1 = environmental) */
void FUN_00437cf0(int x, int y, int radius, int palette_id, int owner)
{
    if (owner == -1) {
        owner = radius;
    }

    int iVar6 = 0;
    int local_4 = 0;

    if (0 < DAT_00489240) {
        do {
            int iVar2 = *(int *)(iVar6 + (int)DAT_00487810);
            if ((iVar2 - 0xf00000 < x) && (x < iVar2 + 0xf00000)) {
                int iVar5 = *(int *)(iVar6 + 4 + (int)DAT_00487810);
                if ((iVar5 - 0xf00000 < y) && (y < iVar5 + 0xf00000)) {
                    int dx = (x - iVar2) >> 0x12;
                    iVar5 = (y - iVar5) >> 0x12;
                    iVar2 = iVar5 * iVar5 + dx * dx;
                    iVar2 = (int)(iVar2 + (iVar2 >> 0x1f & 0xfU)) >> 4;
                    if (iVar2 < 7) iVar2 = 7;

                    /* Apply knockback velocity */
                    int *pVelX = (int *)(iVar6 + 0x10 + (int)DAT_00487810);
                    *pVelX = *pVelX + (dx * radius * -0x800) / iVar2;
                    *(int *)(iVar6 + 0x14 + (int)DAT_00487810) =
                        *(int *)(iVar6 + 0x14 + (int)DAT_00487810) + (iVar5 * radius * -0x800) / iVar2;

                    int base = (int)DAT_00487810;
                    if (palette_id < 0x50) {
                        /* Owner is a player — check team for friendly fire */
                        if (*(char *)((int)DAT_00487810 + 0x2c + palette_id * 0x598) !=
                            *(char *)(iVar6 + 0x2c + (int)DAT_00487810))
                        {
                            int iVar3 = (owner << 0xf) / iVar2;
                            DAT_00486e68[palette_id] += (int)(iVar3 + (iVar3 >> 0x1f & 0x1fffU)) >> 0xd;
                        }
                        if ((*(char *)((int)DAT_00487810 + 0x2c + palette_id * 0x598) !=
                             *(char *)(iVar6 + 0x2c + (int)DAT_00487810)) ||
                            (DAT_0048373d != '\0'))
                        {
                            int *pHP = (int *)(iVar6 + 0x20 + (int)DAT_00487810);
                            *pHP = *pHP - (owner << 0xf) / iVar2;
                            base = (int)DAT_00487810;
                        }
                    }
                    else {
                        int *pHP = (int *)(iVar6 + 0x20 + (int)DAT_00487810);
                        *pHP = *pHP - (owner << 0xf) / iVar2;
                        base = (int)DAT_00487810;
                    }

                    /* Track damage received */
                    iVar2 = (owner << 0xf) / iVar2;
                    DAT_00486be8[local_4] += ((int)(iVar2 + (iVar2 >> 0x1f & 0x1fffU)) >> 0xd);

                    /* Record kill attribution */
                    char cVar4 = (char)palette_id;
                    if (palette_id < 0x50) {
                        if ((*(char *)(base + 0x2c + palette_id * 0x598) !=
                             *(char *)(iVar6 + 0x2c + base)) ||
                            (DAT_0048373d != '\0'))
                        {
                            *(char *)(iVar6 + 0x4a1 + base) = cVar4;
                            base = (int)DAT_00487810;
                        }
                    }
                    else if (palette_id < 100) {
                        *(char *)(iVar6 + 0x4a1 + base) = cVar4 + 0x14;
                        base = (int)DAT_00487810;
                    }
                    else if (palette_id < 0x78) {
                        *(char *)(iVar6 + 0x4a1 + base) = cVar4;
                        base = (int)DAT_00487810;
                    }
                    else if (palette_id < 0x8c) {
                        *(char *)(iVar6 + 0x4a1 + base) = cVar4 - 0x14;
                        base = (int)DAT_00487810;
                    }
                    else {
                        *(unsigned char *)(iVar6 + 0x4a1 + base) = 0xff;
                        base = (int)DAT_00487810;
                    }
                    *(unsigned char *)(iVar6 + 0x4a2 + base) = 0x6e;
                }
            }
            local_4++;
            iVar6 += 0x598;
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
                int ebase = DAT_00489248 * 0x80 + (int)DAT_004892e8;

                /* Position = trooper position */
                *(int *)(ebase + 0x00) = *(int *)(base + 0x00);
                *(int *)(ebase + 0x08) = *(int *)(base + 0x08);

                /* Velocity: random speed (shift 0-3) in this direction */
                unsigned int rnd = rand() & 3;
                *(int *)(ebase + 0x18) = (*(int *)((int)DAT_00487ab0 + angle_off) << (rnd & 0x1F)) >> 6;
                rnd = rand() & 3;
                *(int *)(ebase + 0x1C) = (*(int *)((int)DAT_00487ab0 + angle_off + 0x800) << (rnd & 0x1F)) >> 6;

                /* Copy position to prev_position */
                *(int *)(ebase + 0x04) = *(int *)(base + 0x00);
                *(int *)(ebase + 0x0C) = *(int *)(base + 0x08);

                /* Zero acceleration */
                *(int *)(ebase + 0x10) = 0;
                *(int *)(ebase + 0x14) = 0;

                /* Entity metadata: debris type */
                *(unsigned char *)(ebase + 0x21) = 2;     /* type: trooper debris */
                *(unsigned short *)(ebase + 0x24) = 0;     /* state */
                *(unsigned char *)(ebase + 0x20) = 5;      /* flags */
                *(unsigned char *)(ebase + 0x26) = 0xFF;   /* color: none */
                *(unsigned char *)(ebase + 0x22) = 0xFF;   /* owner: none */
                *(int *)(ebase + 0x28) = 0;                /* health */

                /* Sprite data from entity type table (trooper debris sprites) */
                *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x4B8);
                *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0x4F4);
                *(int *)(ebase + 0x48) = 0;
                *(int *)(ebase + 0x4C) = *(int *)((int)DAT_00487abc + 0x524);
                *(unsigned char *)(ebase + 0x54) = 0;
                *(unsigned char *)(ebase + 0x40) = 0;
                *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + 0x430);
                *(int *)(ebase + 0x3C) = 0;
                *(unsigned char *)(ebase + 0x5C) = 0;

                DAT_00489248++;

                /* Set lifetime: random 150-249 ticks */
                *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x58) = rand() % 100 + 0x96;

                /* Set sprite index from ammo/sound table */
                int snd_rnd = rand();
                *(unsigned int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) =
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
                        int ebase = DAT_00489248 * 0x80 + (int)DAT_004892e8;
                        unsigned int rnd_angle = rand() & 0x7FF;
                        int speed = rand() % 30 + 20;
                        unsigned int rnd_type = rand() & 1;
                        int etype = rnd_type + 0x6C;  /* entity type 108 or 109 */

                        *(int *)(ebase + 0x00) = *(int *)(base + 0x00);
                        *(int *)(ebase + 0x08) = *(int *)(base + 0x08);
                        *(int *)(ebase + 0x18) = *(int *)((int)DAT_00487ab0 + rnd_angle * 4) * speed >> 6;
                        *(int *)(ebase + 0x1C) = *(int *)((int)DAT_00487ab0 + 0x800 + rnd_angle * 4) * speed >> 6;
                        *(int *)(ebase + 0x04) = *(int *)(base + 0x00);
                        *(int *)(ebase + 0x0C) = *(int *)(base + 0x08);
                        *(int *)(ebase + 0x10) = 0;
                        *(int *)(ebase + 0x14) = 0;
                        *(char *)(ebase + 0x21) = (char)etype;
                        *(unsigned short *)(ebase + 0x24) = 0;
                        *(unsigned char *)(ebase + 0x20) = 0;
                        *(unsigned char *)(ebase + 0x26) = 0xFF;
                        *(unsigned char *)(ebase + 0x22) = 0xFF;
                        *(int *)(ebase + 0x28) = 0;

                        int sprite_group = etype * 0x86;
                        int rnd_sprite = rand();
                        *(int *)(ebase + 0x38) = *(int *)((int)DAT_00487abc + 0x88 + (rnd_sprite % 6 + sprite_group) * 4);
                        rnd_sprite = rand();
                        *(int *)(ebase + 0x44) = *(int *)((int)DAT_00487abc + 0xC4 + (rnd_sprite % 6 + sprite_group) * 4);
                        *(int *)(ebase + 0x48) = 0;
                        rnd_sprite = rand();
                        *(int *)(ebase + 0x4C) = *(int *)((int)DAT_00487abc + 0xF4 + (rnd_sprite % 6 + sprite_group) * 4);
                        *(unsigned char *)(ebase + 0x54) = 0;
                        rnd_sprite = rand();
                        *(char *)(ebase + 0x40) = (char)(rnd_sprite % 6);
                        *(int *)(ebase + 0x34) = *(int *)((int)DAT_00487abc + etype * 0x218);
                        *(int *)(ebase + 0x3C) = 0;
                        *(unsigned char *)(ebase + 0x5C) = 0;

                        DAT_00489248++;

                        /* Team-colored sprite offset */
                        unsigned char team = *(unsigned char *)(base + 0x1C);
                        if ((rand() & 1) == 0 && team < 4) {
                            *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) += (unsigned int)team * 100;
                        } else {
                            *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x34) += 300;
                        }

                        /* Set lifetime: random 70-159 ticks */
                        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x58) = rand() % 90 + 70;

                        /* Set animation frame */
                        *(int *)((int)DAT_004892e8 + DAT_00489248 * 0x80 - 0x38) =
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
                if (*(unsigned char *)(DAT_00487810 + poff + 0x2C) == base_team) {
                    *(int *)(DAT_00487810 + poff + 0x28) = 0;        /* clear something */
                    *(int *)(DAT_00487810 + poff + 0x20) = (int)0xFFF0BDC0;  /* force death */
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
                        *(int *)(pbase + 0x08) = (vel_rnd % 50) * *(int *)((int)DAT_00487ab0 + next_sin * 4) >> 5;
                        vel_rnd = rand();
                        *(int *)(pbase + 0x0C) = (vel_rnd % 50) * *(int *)((int)DAT_00487ab0 + next_cos * 4) >> 5;
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
 * The real function uses the level's spawn point system. This simplified version
 * picks a position from the entity spawn point array (DAT_004876a0) or
 * falls back to map center. Sets player position in fixed-point 14.18 format. */
int FUN_0044dfb0(int player)
{
    int poff = player * 0x598;

    /* Try to use spawn points populated by entity spawning */
    if (DAT_004892d4 > 0 && DAT_004876a0 != NULL) {
        /* Pick a spawn point (round-robin by player index) */
        int sp_idx = player % DAT_004892d4;
        int sp_x = *(int *)((char *)DAT_004876a0 + sp_idx * 0xc);
        int sp_y = *(int *)((char *)DAT_004876a0 + sp_idx * 0xc + 4);
        /* Convert tile coords to fixed-point 14.18 */
        *(int *)(DAT_00487810 + poff) = sp_x << 18;
        *(int *)(DAT_00487810 + poff + 4) = sp_y << 18;
        return 1;
    }

    /* Fallback: center of map */
    int cx = (int)(DAT_004879f0 / 2);
    int cy = (int)(DAT_004879f4 / 2);
    *(int *)(DAT_00487810 + poff) = cx << 18;
    *(int *)(DAT_00487810 + poff + 4) = cy << 18;
    return 1;
}
