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

/* ===== Sub-function stubs (implement in later tiers) ===== */
static void FUN_0044ad30(int *ent, int idx)   { (void)ent; (void)idx; }
static void FUN_0044bd50(int *ent)            { (void)ent; }
static void FUN_0044be20(int *ent)            { (void)ent; }
static void FUN_0044c9a0(int *ent)            { (void)ent; }
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
static void FUN_0044d650_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0044d860_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0044e950_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0044e5a0_impl(int *ent)          { (void)ent; }
static void FUN_0044ed90_impl(int *ent, int idx, int arg) { (void)ent; (void)idx; (void)arg; }
static void FUN_0044f840_impl(int *ent)          { (void)ent; }
static void FUN_0044f900_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_00450080_impl(int *ent, char t)  { (void)ent; (void)t; }
static int  FUN_00450f10_impl(int x, int y)      { (void)x; (void)y; return 0; }
static int  FUN_00450ce0_impl(int x, int y)      { (void)x; (void)y; return 0; }
static void FUN_00451010_impl(unsigned int *ent, char t, int idx) { (void)ent; (void)t; (void)idx; }
static void FUN_00451590_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0044dea0_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0040fb70_impl(int idx)           { (void)idx; }
static void FUN_00401000_impl(int idx)           { (void)idx; }
static void FUN_0044d930_impl(int *ent)          { (void)ent; }
static void FUN_0044d9f0_impl(int *ent)          { (void)ent; }
static void FUN_0044dbb0_impl(int *ent, int idx) { (void)ent; (void)idx; }
static void FUN_0044ea70_impl(int *ent)          { (void)ent; }
void FUN_0040f9b0(int snd, int x, int y) { (void)snd; (void)x; (void)y; }

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
 * Also spawns engine exhaust particles (simplified: skip particles for now). */
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
        /* Exhaust particle spawning — skip for now (visual only) */
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

    /* Apply velocity drag (original uses float multiply; approximate with 63/64) */
    ent[4] = (int)((double)ent[4] * 0.96875);
    ent[5] = (int)((double)ent[5] * 0.96875);

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
