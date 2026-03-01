/*
 * stubs.cpp - Placeholder for miscellaneous undecompiled functions
 */
#include "tou.h"
#include <stdlib.h>
#include <string.h>

/* ===== Globals defined in this module ===== */
/* DAT_00487aa8 and DAT_0048781c already defined in memory.cpp */
int   DAT_0048784c = 0;        /* entity-to-entity link count */
int   DAT_00487228[16] = {0};  /* per-player pickup counter */
char  DAT_0048373d = 0;        /* friendly fire enabled flag */

/* ===== Utility stubs (implement later) ===== */
int FUN_00410030(void) { return 0; }  /* conditional entity spawn */
void FUN_004357b0(int x, int y, int size, int p3, int p4, int p5, int p6, int p7, int p8,
                  int p9, char p10, unsigned char p11)
{
    (void)x; (void)y; (void)size; (void)p3; (void)p4; (void)p5;
    (void)p6; (void)p7; (void)p8; (void)p9; (void)p10; (void)p11;
}
void FUN_00451e70(int particle_idx, int damage) { (void)particle_idx; (void)damage; }

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
void FUN_00454340(void) { /* projectile update */ }
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
void FUN_00454b00(void) { /* animation */ }
void FUN_00458010(void) { /* AI targeting / turret behavior */ }
void FUN_00453cd0(void) { /* map logic */ }
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
void FUN_004573e0(void) { /* particle system */ }
void FUN_004133d0(char param) { (void)param; /* turret sound */ }
void FUN_004533d0(void) { /* conditional half-rate subsystem */ }
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
void FUN_0045ddb2(void) { /* round-end cleanup */ }
void FUN_0045fc00(void) { /* score/stat update */ }
/* FUN_00437cf0 — Apply explosion knockback force to nearby players.
 * Pushes all players within a radius away from explosion center.
 * Params: x, y (fixed-point position), radius, palette_id, owner (-1 = environmental) */
void FUN_00437cf0(int x, int y, int radius, int palette_id, int owner)
{
    (void)x; (void)y; (void)radius; (void)palette_id; (void)owner;
    /* TODO: push nearby players away from (x,y) using atan2+LUT.
     * For now, stub — explosions won't push players but entities still die correctly. */
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
void FUN_004104c0(int index) { (void)index; /* turret per-entry init */ }
void FUN_00460cf0(char a, int b) { (void)a; (void)b; /* entity config helper */ }

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
