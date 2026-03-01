/*
 * stubs.cpp - Placeholder for miscellaneous undecompiled functions
 */
#include "tou.h"
#include <stdlib.h>
#include <string.h>

/* ===== Utility stubs (implement later) ===== */
int FUN_00410030(void) { return 0; }  /* conditional entity spawn */

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
void FUN_00434310(void) { /* weapon/terrain */ }
void FUN_004527e0(void) { /* sound update */ }
void FUN_00454b00(void) { /* animation */ }
void FUN_00458010(void) { /* AI targeting / turret behavior */ }
void FUN_00453cd0(void) { /* map logic */ }
void FUN_00455d50(void) { /* bullet update */ }
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
void FUN_0045e2c0(void) { /* network sync */ }
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
