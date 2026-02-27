/*
 * intro.cpp - Intro splash screen sequence with explosion particles
 * Address: Intro_Sequence=0045C720
 *
 * Original flow (from Ghidra decompilation):
 *   - g_IntroSplashIndex counts 0->1->2->3 through splash screens
 *   - Frame indices are REVERSED: starts at 2, then 1, then 0
 *   - Music is paused during splash 0, unpaused at transition to splash 1
 *   - Durations are cumulative from start: 3200, 8200, 10640 ms
 *   - Explosion particles spawned at each transition
 *   - Exit state is 0x98 (new game)
 */
#include "tou.h"
#include <stdio.h>
#include <stdlib.h>

/* ===== Helper: Pick random explosion sprite index ===== */
/* Original uses a 4-way random branch selecting from sprite ranges */
static char Pick_Sprite_Index(void)
{
    int r = rand() % 4;
    if (r == 3) return 11;
    if (r == 2) return (char)((rand() % 4) + 7);
    if (r == 1) return (char)((rand() % 4) + 13);
    return (char)((rand() % 3) + 17);
}

/* ===== Helper: Spawn explosion particles ===== */
/* count: number of particles to spawn
 * y_base/y_range: random Y position = y_base + rand()%y_range */
static void Spawn_Particles(int count, int y_base, int y_range)
{
    int *sin_table = (int *)DAT_00487ab0;

    for (int n = 0; n < count; n++) {
        char sprite = Pick_Sprite_Index();

        int px = (rand() % 240) + 200;    /* X: 200-439 (center area) */
        int py = (rand() % y_range) + y_base;

        /* Compute angle from screen center to particle position */
        int angle = FUN_004257e0(320, 240, px, py);
        int vel_angle = (angle + 0xAA) & 0x7FF;  /* offset by ~30 degrees */

        if (DAT_00489250 >= 2000) break;

        int idx = DAT_00489250;
        int *part = (int *)((char *)DAT_00481f34 + idx * 0x20);

        part[0] = px * 0x40000;     /* x (fixed point) */
        part[1] = py * 0x40000;     /* y (fixed point) */

        /* Velocity: random magnitude * sin/cos of angle */
        int mag = rand() % 100;
        part[2] = (mag * sin_table[vel_angle]) >> 10;        /* dx */
        part[3] = (mag * sin_table[vel_angle + 0x200]) >> 10; /* dy (cos = sin + 512) */

        unsigned char *pb = (unsigned char *)part;
        pb[0x10] = (unsigned char)sprite;   /* sprite_index */
        pb[0x11] = (unsigned char)((rand() % 6) + 1); /* frame_number (start 1-6) */
        pb[0x12] = 0x80;                    /* frame_timer */
        pb[0x13] = 0;
        pb[0x14] = 0;
        pb[0x15] = 0;                       /* color_index (group 0 = fire) */

        DAT_00489250++;
    }
}

/* ===== Helper: Spawn intro entities (shrapnel debris) ===== */
/* Spawns exploding text fragment entities at each intro transition.
 * max_count: max entities to spawn in this call
 * y_base/y_range: random Y = y_base + rand()%y_range
 * vel_max: velocity magnitude range (rand()%vel_max)
 * Entity types 0x6c/0x6d, random animation from loadtime.dat. */
static void Spawn_Entities(int max_count, int y_base, int y_range, int vel_max)
{
    int *sin_table = (int *)DAT_00487ab0;
    unsigned char *type_base = (unsigned char *)DAT_00487abc;

    for (int n = 0; n < max_count; n++) {
        if (DAT_00489248 > 0x9c3) break;  /* max 2499 entities */

        int mag_rand = rand();
        int px = (rand() % 0xf0) + 200;    /* X: 200-439 */
        int py = (rand() % y_range) + y_base;

        int angle = FUN_004257e0(0x140, 0xf0, px, py);

        int entity_type = (rand() & 1) + 0x6c;  /* 0x6c or 0x6d */

        int idx = DAT_00489248;
        int *ent = (int *)((unsigned char *)DAT_004892e8 + idx * 0x80);
        unsigned char *eb = (unsigned char *)ent;

        int fx = px * 0x40000;
        int fy = py * 0x40000;

        ent[0] = fx;              /* +0x00: x (fixed point) */
        ent[2] = fy;              /* +0x08: y (fixed point) */
        ent[6] = (sin_table[angle] * (mag_rand % vel_max)) >> 9;       /* +0x18: velocity_x */
        ent[7] = (sin_table[angle + 0x200] * (mag_rand % vel_max)) >> 9; /* +0x1C: velocity_y */
        ent[1] = fx;              /* +0x04: x_prev */
        ent[3] = fy;              /* +0x0C: y_prev */
        ent[4] = 0;               /* +0x10 */
        ent[5] = 0;               /* +0x14 */
        eb[0x21] = (unsigned char)entity_type;
        *(short *)(eb + 0x24) = 0;
        eb[0x20] = 0;
        eb[0x26] = 0xFF;
        eb[0x22] = 0xFF;
        ent[10] = 0;              /* +0x28: TTL (set below after increment) */

        int type_ints = entity_type * 0x86;  /* == entity_type * (0x218/4) */

        int ar = rand() % 3;
        ent[14] = *(int *)(type_base + 0x88 + (ar + type_ints) * 4);   /* +0x38: sprite_anim_base */

        ar = rand() % 3;
        ent[17] = *(int *)(type_base + 0xC4 + (ar + type_ints) * 4);   /* +0x44 */

        ent[18] = 0;              /* +0x48: current_frame */

        ar = rand() % 3;
        ent[19] = *(int *)(type_base + 0xF4 + (ar + type_ints) * 4);   /* +0x4C: sprite_base_index */

        eb[0x54] = 0;             /* animation timer */

        ar = rand() % 3;
        eb[0x40] = (unsigned char)ar;   /* animation_index */

        ent[13] = *(int *)(type_base + entity_type * 0x218); /* +0x34 */
        ent[15] = 0;              /* +0x3C */
        eb[0x5C] = 0;

        DAT_00489248++;

        /* Post-increment adjustments (offsets relative to new count) */
        ent[19] += 300;           /* sprite base offset +300 */

        int ttl_rand = rand();
        DWORD ttl_time = timeGetTime();
        ent[10] = ttl_rand % 1000 + 1000 + (int)ttl_time;  /* TTL */

        int num_frames = (int)type_base[entity_type * 0x218 + 0x126];
        if (num_frames > 1) {
            ent[18] = rand() % (num_frames - 1);  /* starting animation frame */
        }

    }
}

/* ===== Helper: Update entity physics ===== */
/* Gravity, movement, animation frame advance, TTL-based removal */
static void Update_Entities(void)
{
    DWORD currentTime = timeGetTime();
    unsigned char *type_base = (unsigned char *)DAT_00487abc;

    int i = 0;
    int byte_offset = 0;

    while (i < DAT_00489248) {
        int *ent = (int *)((unsigned char *)DAT_004892e8 + byte_offset);
        unsigned char *eb = (unsigned char *)ent;

        /* Gravity: velocity_y += 0x800 */
        int new_vy = ent[7] + 0x800;
        ent[7] = new_vy;

        /* Move: x += velocity_x * delta_time */
        ent[0] += ent[6] * (int)DAT_004877f0;

        /* Move: y += velocity_y * delta_time (using updated velocity) */
        ent[2] += new_vy * (int)DAT_004877f0;

        /* Animation timer advance */
        unsigned char timer = eb[0x54] + 1;
        eb[0x54] = timer;

        /* Check animation threshold from entity type definition */
        int anim_idx = (int)eb[0x40];   /* animation_index */
        int etype = (int)eb[0x21];      /* entity_type */
        int type_offset = anim_idx + etype * 0x218;

        if (timer > type_base[type_offset + 0x12A]) {
            int frame = ent[18];    /* +0x48: current_frame */
            eb[0x54] = 0;           /* reset timer */
            ent[18] = frame + 1;
            if ((unsigned int)(frame + 1) >= (unsigned int)type_base[type_offset + 0x124]) {
                ent[18] = 0;        /* loop animation */
            }
        }

        /* TTL: remove expired entities (swap with last) */
        if (ent[10] < (int)currentTime) {
            DAT_00489248--;
            if (i < DAT_00489248) {
                unsigned char *last = (unsigned char *)DAT_004892e8 + DAT_00489248 * 0x80;
                memcpy(ent, last, 0x80);
                continue;  /* re-process swapped entity */
            }
        }

        i++;
        byte_offset += 0x80;
    }
}

/* ===== Helper: Update particle physics ===== */
/* Moves particles, advances animation frames, removes dead particles */
static void Update_Particles(void)
{
    int i = 0;
    int byte_offset = 0;

    while (i < DAT_00489250) {
        int *part = (int *)((char *)DAT_00481f34 + byte_offset);
        unsigned char *pb = (unsigned char *)part;

        /* Move: position += velocity * delta_time */
        part[0] += part[2] * (int)DAT_004877f0;
        part[1] += part[3] * (int)DAT_004877f0;

        /* Advance frame timer */
        unsigned char timer = pb[0x12] + 1;
        pb[0x12] = timer;

        if (timer > 0x81) {
            pb[0x12] = 0x80;
            pb[0x11] += 1;  /* advance animation frame */
        }

        /* Check if animation is complete */
        unsigned char sprite_idx = pb[0x10];
        unsigned char *desc = (unsigned char *)DAT_00481f20 + (int)sprite_idx * 8;
        unsigned char num_frames = desc[6];

        if (pb[0x11] >= num_frames) {
            /* Remove particle: swap with last */
            DAT_00489250--;
            if (i < DAT_00489250) {
                unsigned char *last = (unsigned char *)DAT_00481f34 + DAT_00489250 * 0x20;
                memcpy(part, last, 0x20);
                /* Don't advance i - re-process swapped particle */
                continue;
            }
        }

        i++;
        byte_offset += 0x20;
    }
}

/* ===== Intro_Sequence (0045C720) ===== */
void Intro_Sequence(void)
{
    static const DWORD Durations[] = {3200, 8200, 10640};

    /* Intro finished (splash index >= 3) - do nothing */
    if (g_IntroSplashIndex >= 3) {
        return;
    }

    /* Check for skip via keyboard (DirectInput in original, simplified here) */
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) ||
        (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
        g_GameState = 0x98;
        Pause_Audio_Streams();
        return;
    }

    DWORD currentTime = timeGetTime();

    /* Check if current splash duration has elapsed */
    if (currentTime > (DAT_004892b8 + Durations[g_IntroSplashIndex])) {
        if (g_IntroSplashIndex == 0) {
            /* Splash 0 -> 1: Unpause music, switch to frame 1 */
            Pause_Audio_Streams();
            g_FrameIndex = 1;

            /* Spawn 0x96 (150) explosion particles */
            DAT_00489250 = 0;
            Spawn_Particles(0x96, 200, 0x50);

            /* Reset and spawn intro entities (shrapnel debris) */
            DAT_00489248 = 0;
            Spawn_Entities(800, 200, 0x50, 100);
        }
        else if (g_IntroSplashIndex == 1) {
            /* Splash 1 -> 2: Switch to frame 0 */
            Pause_Audio_Streams();
            g_FrameIndex = 0;

            /* Spawn 0xDC (220) explosion particles */
            Spawn_Particles(0xDC, 180, 0x78);

            /* Spawn more entities (accumulates, doesn't reset count) */
            Spawn_Entities(0x5dc, 0xb4, 0x78, 0x8c);
        }
        else if (g_IntroSplashIndex == 2) {
            /* Splash 2 done: Move to new game state */
            g_GameState = 0x98;
        }
        g_IntroSplashIndex++;
    }

    /* Update timing */
    DWORD now = timeGetTime();
    DAT_004877f0 = now - g_FrameTimer;
    g_FrameTimer = now;

    /* Update particle physics */
    Update_Particles();

    /* Update entity physics (gravity, movement, animation, TTL removal) */
    Update_Entities();

    Render_Frame();

    /* COMPAT: Frame rate limiter (~60fps).
     * Original used DDraw exclusive fullscreen with flip chain which
     * was vsync-locked. Windowed mode runs uncapped, causing frame-count
     * based animation timers (particle +0x12) to advance way too fast. */
    {
        static DWORD lastFrameTime = 0;
        DWORD frameEnd = timeGetTime();
        if (lastFrameTime != 0) {
            DWORD elapsed = frameEnd - lastFrameTime;
            if (elapsed < 16) {
                Sleep(16 - elapsed);
            }
        }
        lastFrameTime = timeGetTime();
    }
}
