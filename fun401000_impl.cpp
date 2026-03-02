/* FUN_00401000 — Main Entity Update / Weapon Fire Effect Spawner
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
        if (*(char *)(iVar12 + 0x35 + (int)DAT_00487810) != '\x01' ||
            *(int *)(iVar12 + 0xa4 + (int)DAT_00487810) != 0) {
            goto do_switch;
        }
        local_14 = 0x5a;
    }
    else if (local_14 < 0x2d) {
        goto do_switch;
    }

    /* For local_14 >= 0x2d (and 0x5a override): play sound, update stats */
    FUN_0040f9b0((int)local_14,
                 *(int *)(iVar12 + (int)DAT_00487810),
                 *(int *)(iVar12 + 4 + (int)DAT_00487810));
    iVar13 = (int)DAT_00487810;
    DAT_00486d28[idx] += (unsigned int)*(unsigned char *)((int)&DAT_00481ca8 + local_14);
    DAT_004870e8[idx] += (unsigned int)*(unsigned char *)((int)&DAT_00481c58 + local_14);

do_switch:
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
            *(unsigned int *)(DAT_00489248 * 0x80 + (int)DAT_004892e8 - 0x34) =
                ((int)uVar8 >> 3) + 30000 + ((uVar8 & 0x1fffff8) * 0x20 + (uVar8 & 0x3ffffff8)) * 4;
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
            int uVar10 = typeTable[0x86];
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
            int uVar10 = typeTable[0xa78];
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

    /* ===== CASE 0x2c: Homing projectile with recoil ===== */
    case 0x2c:
    {
        if (DAT_00489248 < 0x9c4) {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = 0; *(int *)(p + 0x1c) = 0;
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

    /* ===== CASE 0x2d: Trail projectile ===== */
    case 0x2d:
    {
        if (DAT_00489248 < 0x9c4) {
            int p = DAT_00489248 * 0x80 + (int)DAT_004892e8;
            *(int *)(p) = *(int *)(iVar12 + iVar13);
            *(int *)(p + 8) = *(int *)(iVar12 + 4 + (int)DAT_00487810);
            *(int *)(p + 0x18) = 0; *(int *)(p + 0x1c) = 0;
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
