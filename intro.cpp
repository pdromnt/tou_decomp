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

        if (g_ParticleCount >= PARTICLE_CAPACITY) break;

        ParticleRecord *particle = &g_ParticlePool[g_ParticleCount];

        particle->position_x = px * FIXED_SCALE;
        particle->position_y = py * FIXED_SCALE;

        /* Velocity: random magnitude * sin/cos of angle */
        int mag = rand() % 100;
        particle->velocity_x = (mag * sin_table[vel_angle]) >> 10;
        particle->velocity_y = (mag * sin_table[vel_angle + 0x200]) >> 10;
        particle->sprite_index = (unsigned char)sprite;
        particle->frame_number = (unsigned char)((rand() % 6) + 1);
        particle->frame_timer = 0x80;
        particle->flags_13 = 0;
        particle->owner_or_flags_14 = 0;
        particle->color_index = 0;

        g_ParticleCount++;
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
        if (g_EntityCount >= ENTITY_ACTIVE_CAPACITY) break;  /* max 2499 entities */

        int mag_rand = rand();
        int px = (rand() % 0xf0) + 200;    /* X: 200-439 */
        int py = (rand() % y_range) + y_base;

        int angle = FUN_004257e0(0x140, 0xf0, px, py);

        int entity_type = (rand() & 1) + 0x6c;  /* 0x6c or 0x6d */

        Entity *entity = &g_EntityPool[g_EntityCount];

        int fx = px * FIXED_SCALE;
        int fy = py * FIXED_SCALE;

        entity->position_x = fx;
        entity->position_y = fy;
        entity->velocity_x = (sin_table[angle] * (mag_rand % vel_max)) >> 9;
        entity->velocity_y = (sin_table[angle + 0x200] * (mag_rand % vel_max)) >> 9;
        entity->previous_x = fx;
        entity->previous_y = fy;
        entity->motion_x_10 = 0;
        entity->motion_y_14 = 0;
        entity->type = (unsigned char)entity_type;
        entity->variant_24 = 0;
        entity->state_20 = 0;
        entity->auxiliary_26 = 0xFF;
        entity->owner = 0xFF;
        entity->health_or_damage_28 = 0;

        int type_ints = entity_type * 0x86;  /* == entity_type * (0x218/4) */

        int ar = rand() % 3;
        entity->gravity_or_motion_38 = *(int *)(type_base + 0x88 + (ar + type_ints) * 4);

        ar = rand() % 3;
        entity->damage_44 = *(int *)(type_base + 0xC4 + (ar + type_ints) * 4);

        entity->scratch_48 = 0;

        ar = rand() % 3;
        entity->palette_value = *(int *)(type_base + 0xF4 + (ar + type_ints) * 4);

        entity->animation_frame = 0;

        ar = rand() % 3;
        entity->subtype = (unsigned char)ar;

        entity->callback_address = *(int *)(type_base + entity_type * 0x218);
        entity->counter_3c = 0;
        entity->timer_5c = 0;

        g_EntityCount++;

        /* Post-increment adjustments (offsets relative to new count) */
        entity->palette_value += 300;

        int ttl_rand = rand();
        uint32_t ttl_time = Platform_GetTicks();
        entity->health_or_damage_28 = ttl_rand % 1000 + 1000 + (int)ttl_time;

        int num_frames = (int)type_base[entity_type * 0x218 + 0x126];
        if (num_frames > 1) {
            entity->scratch_48 = rand() % (num_frames - 1);
        }

    }
}

/* ===== Helper: Update entity physics ===== */
/* Gravity, movement, animation frame advance, TTL-based removal */
static void Update_Entities(void)
{
    uint32_t currentTime = Platform_GetTicks();
    unsigned char *type_base = (unsigned char *)DAT_00487abc;

    int i = 0;
    while (i < g_EntityCount) {
        Entity *entity = &g_EntityPool[i];

        /* Gravity: velocity_y += 0x800 */
        int new_vy = entity->velocity_y + 0x800;
        entity->velocity_y = new_vy;

        /* Move: x += velocity_x * delta_time */
        entity->position_x += entity->velocity_x * (int)DAT_004877f0;

        /* Move: y += velocity_y * delta_time (using updated velocity) */
        entity->position_y += new_vy * (int)DAT_004877f0;

        /* Animation timer advance */
        unsigned char timer = entity->animation_frame + 1;
        entity->animation_frame = timer;

        /* Check animation threshold from entity type definition */
        int anim_idx = (int)entity->subtype;
        int etype = (int)entity->type;
        int type_offset = anim_idx + etype * 0x218;

        if (timer > type_base[type_offset + 0x12A]) {
            int frame = entity->scratch_48;
            entity->animation_frame = 0;
            entity->scratch_48 = frame + 1;
            if ((unsigned int)(frame + 1) >= (unsigned int)type_base[type_offset + 0x124]) {
                entity->scratch_48 = 0;
            }
        }

        /* TTL: remove expired entities (swap with last) */
        if (entity->health_or_damage_28 < (int)currentTime) {
            g_EntityCount--;
            if (i < g_EntityCount) {
                *entity = g_EntityPool[g_EntityCount];
                continue;  /* re-process swapped entity */
            }
        }

        i++;
    }
}

/* ===== Helper: Update particle physics ===== */
/* Moves particles, advances animation frames, removes dead particles */
static void Update_Particles(void)
{
    int i = 0;
    while (i < g_ParticleCount) {
        ParticleRecord *particle = &g_ParticlePool[i];

        /* Move: position += velocity * delta_time */
        particle->position_x += particle->velocity_x * (int)DAT_004877f0;
        particle->position_y += particle->velocity_y * (int)DAT_004877f0;

        /* Advance frame timer */
        unsigned char timer = particle->frame_timer + 1;
        particle->frame_timer = timer;

        if (timer > 0x81) {
            particle->frame_timer = 0x80;
            particle->frame_number += 1;
        }

        /* Check if animation is complete */
        unsigned char sprite_idx = particle->sprite_index;
        unsigned char *desc = (unsigned char *)DAT_00481f20 + (int)sprite_idx * 8;
        unsigned char num_frames = desc[6];

        if (particle->frame_number >= num_frames) {
            /* Remove particle: swap with last */
            g_ParticleCount--;
            if (i < g_ParticleCount) {
                *particle = g_ParticlePool[g_ParticleCount];
                /* Don't advance i - re-process swapped particle */
                continue;
            }
        }

        i++;
    }
}

/* ===== Intro_Sequence (0045C720) ===== */
void Intro_Sequence(void)
{
    static const uint32_t Durations[] = {3200, 8200, 10640};

    /* Intro finished (splash index >= 3) - do nothing */
    if (g_IntroSplashIndex >= 3) {
        return;
    }

    /* Legacy scan-code state is populated by the SDL adapter. */
    if ((g_KeyboardState[0x01] & 0x80) ||
        (g_KeyboardState[0x39] & 0x80) ||
        (g_KeyboardState[0x1C] & 0x80)) {
        /* Clear intro particles/entities so they don't bleed into menu */
        g_ParticleCount = 0;  /* particle count */
        g_EntityCount = 0;  /* entity count */
        GameState_Transition(GAME_STATE_NEW_GAME);
        Pause_Audio_Streams();
        return;
    }

    uint32_t currentTime = Platform_GetTicks();

    /* Check if current splash duration has elapsed */
    if (currentTime > (DAT_004892b8 + Durations[g_IntroSplashIndex])) {
        if (g_IntroSplashIndex == 0) {
            /* Splash 0 -> 1: Unpause music, switch to frame 1 */
            Pause_Audio_Streams();
            g_FrameIndex = 1;

            /* Spawn 0x96 (150) explosion particles */
            g_ParticleCount = 0;
            Spawn_Particles(0x96, 200, 0x50);

            /* Reset and spawn intro entities (shrapnel debris) */
            g_EntityCount = 0;
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
            g_ParticleCount = 0;  /* clear particles */
            g_EntityCount = 0;  /* clear entities */
            GameState_Transition(GAME_STATE_NEW_GAME);
        }
        g_IntroSplashIndex++;
    }

    /* Update timing */
    uint32_t now = Platform_GetTicks();
    DAT_004877f0 = now - g_FrameTimer;
    g_FrameTimer = now;

    /* Update particle physics */
    Update_Particles();

    /* Update entity physics (gravity, movement, animation, TTL removal) */
    Update_Entities();

    Render_Frame();

    /* COMPAT: Frame rate limiter (~60fps).
     * Original used an exclusive fullscreen flip chain which
     * was vsync-locked. Windowed mode runs uncapped, causing frame-count
     * based animation timers (particle +0x12) to advance way too fast. */
    {
        static uint32_t lastFrameTime = 0;
        uint32_t frameEnd = Platform_GetTicks();
        if (lastFrameTime != 0) {
            uint32_t elapsed = frameEnd - lastFrameTime;
            if (elapsed < 16) {
                Platform_Delay(16 - elapsed);
            }
        }
        lastFrameTime = Platform_GetTicks();
    }
}
