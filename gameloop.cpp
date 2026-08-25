/*
 * gameloop.cpp - Game state machine, input update, menu state
 * Addresses: Game_State_Manager=00461260, Input_Update=00462560,
 *            Handle_Menu_State=004611D0, Game_Update_Render=00461710
 */
#include "tou.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

/* ===== Globals defined in this module ===== */
char          g_MouseButtons = 0;     /* 004877BE */
unsigned char g_ProcessInput = 0;     /* 00489295 */
GameplaySubState g_SubState = GAMEPLAY_ACTIVE; /* 00489296 */
unsigned char g_NeedsRedraw  = 0;     /* 00489297 */
unsigned char g_SurfaceReady = 0;     /* 00489298 */
unsigned char g_SubState2    = 0;     /* 00489299 */

int  g_MouseDeltaX = 0;              /* 004877B4 */
int  g_MouseDeltaY = 0;              /* 004877B8 */
int  g_SpectatorCameraX = 0;
int  g_SpectatorCameraY = 0;
char g_InputMode   = 0;              /* 004877E4 */
int  DAT_004877e8  = 0;              /* alt X accumulator */
uint32_t     DAT_00489ee8 = 0;       /* Key repeat cooldown timestamp */
unsigned int DAT_00489eec = 0;       /* Last pressed key scan code */

/* Gameplay tick timing and counters */
char  DAT_00489288 = 0;              /* sub-frame counter (0-7, wraps) */

/* Pause menu state (unused — original binary has no visible pause menu selection) */

/* ===== Game_State_Manager (00461260) ===== */
void Game_State_Manager(void)
{
    int iVar2;

    switch (g_GameState) {
    case GAME_STATE_RENDER_GAMEPLAY:
        FUN_00425fe0();
        return;

    case GAME_STATE_RETURN_TO_MENU:
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        Handle_Menu_State();
        return;

    case GAME_STATE_INIT_GAMEPLAY:
        if (DAT_00487640[1] != 5) {
            DAT_00487640[1] = 5;
            g_DisplayWidth  = 640; /* Mode 5 = 640x480 */
            g_DisplayHeight = 480;

            iVar2 = RenderBackend_Configure(g_DisplayWidth, g_DisplayHeight);
            if (iVar2 == 0) {
                RenderBackend_Shutdown();
                Platform_ShowError(STR_ERR_RENDER_MODE);
                Request_App_Quit();
                GameState_Transition(GAME_STATE_SHUTDOWN);
                return;
            }
        }
        DAT_004877b1 = 1;
        DAT_004877bd = 0;       /* clear mouse button latch on menu entry */
        g_MouseButtons = 0;
        g_FrameTimer = Platform_GetTicks();

        /* Music: If coming from state 0x98 (new game after intro),
         * skip FUN_0040e130 since music is already playing from intro.
         * Otherwise call FUN_0040e130 to start menu/gameplay music. */
        if (DAT_004877a4 == 0x98) {
            DAT_004877a4 = 0;
        } else {
            FUN_0040e130();
        }

        /* Create game view surface */
        if (FUN_0042fc40()) {
            GameState_Transition(GAME_STATE_RENDER_GAMEPLAY);
        } else {
            GameState_Transition(GAME_STATE_SHUTDOWN);
        }

        g_bIsActive = 1;

        FUN_00425fe0();
        return;

    case GAME_STATE_QUICK_RESTART:
        FUN_0042fc10();
        FUN_0041a8c0();
        Handle_Menu_State();
        return;

    case GAME_STATE_GAME_OVER:
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        FUN_0041d740();
        GameState_Transition(GAME_STATE_INIT_GAMEPLAY);
        return;

    case GAME_STATE_ERROR_RESTART:
        g_SubState2 = 0;
        Stop_All_Sounds();
        Free_Game_Resources();
        FUN_0041d740();
        DAT_004877a4 = 0;
        GameState_Transition(GAME_STATE_INIT_GAMEPLAY);
        return;

    case GAME_STATE_INTRO_INIT:
        if (DAT_00487640[1] != 5) {
            DAT_00487640[1] = 5;
            g_DisplayHeight = 480;
            g_DisplayWidth  = 640;

            iVar2 = RenderBackend_Configure(g_DisplayWidth, g_DisplayHeight);
            if (iVar2 == 0) {
                RenderBackend_Shutdown();
                Platform_ShowError(STR_ERR_RENDER_MODE);
                Request_App_Quit();
                GameState_Transition(GAME_STATE_SHUTDOWN);
                return;
            }
        }
        DAT_004877b1 = 1;
        DAT_004877bd = 0;       /* clear mouse button latch on menu entry */
        g_MouseButtons = 0;
        g_FrameTimer  = Platform_GetTicks();
        DAT_004892b8  = Platform_GetTicks();
        g_IntroSplashIndex = 0;
        GameState_Transition(GAME_STATE_INTRO_RUN);
        DAT_004877a4  = 0x97;
        Load_Background_To_Buffer(1);
        FUN_0040e130();
        FUN_0045d7d0();
        /* fall through */

    case GAME_STATE_INTRO_RUN:
        Intro_Sequence();
        return;

    case GAME_STATE_NEW_GAME:
        DAT_004877a4 = 0x98;
        GameState_Transition(GAME_STATE_INIT_GAMEPLAY);
        /* Reset team palette[3] from gold (0x7FF0) back to gray (0x6739).
         * It gets set to gold in FUN_0042d8b0 before sprites load;
         * starting a new game resets it to the default gray team color. */
        DAT_00483838[3] = 0x6739;
        iVar2 = Init_New_Game();
        if (iVar2 != 1) {
            GameState_Transition(GAME_STATE_SHUTDOWN);
        }
        return;

    case GAME_STATE_SHUTDOWN:
        Request_App_Quit();
        GameState_Transition(GAME_STATE_STOPPED);
        return;

    default:
        return;
    }
}

/* ===== FUN_0045e1f0 — Pre-tick entity flag reset ===== */
/* Clears per-tick flags across 4 entity arrays before each simulation step. */
void FUN_0045e1f0(void)
{
    int i;

    /* Main entities (g_EntityPool, stride 0x80): clear damage flag if health < 30000 */
    for (i = 0; i < g_EntityCount; i++) {
        Entity *entity = &g_EntityPool[i];
        if (entity->palette_value < 30000 && entity->variant_24 == 1) {
            entity->variant_24 = 0;
        }
    }

    /* Troopers (g_TrooperPool, stride 0x40): clear hit flag */
    for (i = 0; i < g_TrooperCount; i++) {
        g_TrooperPool[i].palette_2c = 0;
    }

    /* Projectiles (g_ProjectilePool, stride 0x40): clear update flag */
    for (i = 0; i < g_ProjectileCount; i++) {
        g_ProjectilePool[i].palette_or_flags_1e = 0;
    }

    /* Players (DAT_00487810, stride 0x598): clear per-tick flags */
    for (i = 0; i < DAT_00489240; i++) {
        PlayerData *player = Player_Get(i);
        player->flag_a1 = 0;
        player->flag_a3 = 0;
    }
}

/* ===== Gameplay_Tick (0045DAA0) ===== */
/* Fixed-timestep game simulation loop.
 * Called from Game_Update_Render when g_SubState == 0 (active gameplay).
 * Implements the full framework from the original with all subsystem calls.
 * Individual subsystems are stubbed pending decompilation. */
static void Gameplay_Tick(void)
{
    unsigned int tick_interval;
    uint32_t now;
    int catch_up;
    int tick;

    if (DAT_00483746 < 1) DAT_00483746 = 60;
    tick_interval = (unsigned int)(1000 / DAT_00483746);

    /* Pre-tick setup: reset per-tick entity flags */
    FUN_0045e1f0();

    /* Wait until at least one tick interval has elapsed.
     * Original used a pure busy-wait (100% CPU). We use a zero-delay yield which
     * yields the current time-slice but returns as soon as the thread
     * can run again without burning 100% CPU. A one-millisecond delay was too coarse even
     * with 1ms timer resolution, causing ~1-2ms timing overshoot per
     * frame that accumulated into uneven frame spacing. */
    now = Platform_GetTicks();
    while ((now - g_TimerAux * tick_interval) - g_TimerStart < tick_interval) {
        Platform_Delay(0);
        now = Platform_GetTicks();
    }

    /* Calculate how many ticks to catch up (max 9) */
    now = Platform_GetTicks();
    catch_up = (int)((now - g_TimerAux * tick_interval - g_TimerStart) / tick_interval);
    if (catch_up > 9) {
        g_TimerStart += (catch_up - 9) * tick_interval;
        catch_up = 9;
    }

    /* Execute each tick */
    for (tick = 0; tick < catch_up; tick++) {
        if (!Netplay_PrepareSimulationTick(SimulationState_Tick() + 1))
            break;
        if (!Replay_PrepareSimulationTick(SimulationState_Tick() + 1))
            break;
        g_TimerAux++;

        /* Sub-frame counter: 0→1→2→...→7→0 */
        DAT_00489288++;
        if (DAT_00489288 >= 8) DAT_00489288 = 0;

        /* Activation pair logic */
        if (DAT_004892a5 != 0) {
            DAT_004892a5++;
            /* After 40 ticks of round-end countdown, trigger level transition.
             * Original at 0x00461776: when DAT_004892a5 > 0x28, enter the
             * natural match-complete state that stores the winner and scores. */
            if (DAT_004892a5 > 0x28) {
                g_SubState = GAMEPLAY_MATCH_COMPLETE;
            }
        }
        if (DAT_004892a5 == 0 && DAT_004892a4 != 0 && DAT_0048373e == 0) {
            DAT_004892a5 = 1;
        }

        /* ---- Subsystem calls ---- */
        FUN_00460d50();                          /* 1-RoundTimer */
        Memory_Trace_Check("1-RoundTimer");
        FUN_004609e0();                          /* 2-SpatialGrid */
        Memory_Trace_Check("2-SpatialGrid");
        if ((DAT_00489288 & 1) == 0) {
            FUN_00460660();                      /* 3-CollisionBitmap (half-rate) */
            Memory_Trace_Check("3-CollisionBitmap");
        }
        FUN_00460ac0();                          /* 4-RelocateEdge */
        Memory_Trace_Check("4-RelocateEdge");
        FUN_00413720();                          /* 5-Spawner */
        Memory_Trace_Check("5-Spawner");
        FUN_00454340();                          /* 6-Emitters */
        Memory_Trace_Check("6-Emitters");
        FUN_0044b0b0();                          /* 7-EntityBehavior */
        Memory_Trace_Check("7-EntityBehavior");
        FUN_00434310();                          /* 8-DebrisAnim */
        Memory_Trace_Check("8-DebrisAnim");
        ParticleSystem_Update();                 /* 9-Animated particles */
        Memory_Trace_Check("9-AnimatedParticles");

        /* Inline: effect/particle rotation and timer decrement */
        {
            int i;
            for (i = 0; i < DAT_00489264; i++) {
                uint8_t *base = static_cast<uint8_t *>(DAT_00487780) + i * 0x20;
                unsigned int *angle = reinterpret_cast<unsigned int *>(base + 0x10);
                int *timer_a = reinterpret_cast<int *>(base + 0x08);
                int *timer_b = reinterpret_cast<int *>(base + 0x0C);
                *angle = (*angle + 0x10) & 0x7FF;
                if (*timer_a > 0) (*timer_a)--;
                if (*timer_b > 0) (*timer_b)--;
            }
        }

        TrooperSystem_Update();                  /* 11-Troopers/cars */
        Memory_Trace_Check("11-Troopers");
        TurretSystem_UpdateTargeting();          /* 12-Turret LOS and firing */
        Memory_Trace_Check("12-TurretTargeting");
        FireParticleSystem_Update();             /* 13-Fire/smoke particles */
        Memory_Trace_Check("13-FireParticles");
        PickupSystem_Update();                   /* 14-Pickups */
        Memory_Trace_Check("14-Pickups");
        ExplosionSystem_UpdateLegacy();          /* 15-Legacy explosions */
        Memory_Trace_Check("15-Explosions");
        FUN_00453a80();                          /* 16-ItemAI */
        Memory_Trace_Check("16-ItemAI");
        TrapDoorSystem_Update();                 /* 17-Trap doors */
        Memory_Trace_Check("17-TrapDoors");

        /* Conditional: turret sound */
        if (DAT_00483834 != 0) {
            FUN_004133d0('\0');                   /* 18-TurretBehavior */
            Memory_Trace_Check("18-TurretBehavior");
        }

        /* Conditional: trooper-related + round-end check */
        if (DAT_00483835 != 0) {
            if ((DAT_00489288 & 1) == 0) {
                FUN_004533d0();                  /* 19-Elevators */
            }
            if (DAT_00489288 == 0) {
                /* Every 8th tick: round-end check causes early return */
                FUN_00453230();                  /* 20-WaypointCheck */
                {
                    const uint64_t checksum = SimulationState_OnTickComplete();
                    Netplay_AfterSimulationTick(SimulationState_Tick(), checksum);
                    Replay_AfterSimulationTick(SimulationState_Tick(), checksum);
                }
                return;
            }
        }
        FluidSystem_Update();                    /* 21-Fluid spread */
        Memory_Trace_Check("21-FluidSpread");
        FUN_0045e2c0();                          /* 22-Deaths */
        Memory_Trace_Check("22-Deaths");

        /* Inline: health clamping for specific game modes */
        if (DAT_004892a8 == 1) {
            char mode_byte = *((char *)&DAT_00483740 + 1);
            if (mode_byte == 2 || mode_byte == 4) {
                int p;
                for (p = 0; p < DAT_00489240; p++) {
                    PlayerData *player = Player_Get(p);
                    if (player->health > 0x1000) player->health = 0x1000;
                }
            }
        }

        /* Incremental visibility map update (200 cells per tick) */
        FUN_00449040('\0');

        /* Inline: trooper tile validation */
        {
            int i;
            for (i = 0; i < g_TrooperCount; i++) {
                TrooperRecord *trooper = &g_TrooperPool[i];
                trooper->palette_2c = 0;

                /* Check tile at trooper position */
                int tx = trooper->position_x >> 0x12;
                int ty = trooper->position_y >> 0x12;
                int tile_idx = static_cast<unsigned char *>(DAT_0048782c)[
                    (ty << (DAT_00487a18 & 0x1f)) + tx];
                if (TerrainProperty_Read(tile_idx, 0x01) == 1) {
                    trooper->animation_state_24 = 0;
                } else {
                    char stale = (char)trooper->animation_state_24;
                    stale++;
                    if (stale >= 6) stale = 0;
                    trooper->animation_state_24 = (uint8_t)stale;
                }
            }
        }
        {
            const uint64_t checksum = SimulationState_OnTickComplete();
            Netplay_AfterSimulationTick(SimulationState_Tick(), checksum);
            Replay_AfterSimulationTick(SimulationState_Tick(), checksum);
        }
    }  /* end tick loop */
}

/* ===== Game_Update_Render (00461710) - Gameplay frame ===== */
/* Main gameplay loop: keyboard input, game state updates, rendering.
 * Called from the SDL application loop when g_GameState == 0.
 *
 * Original: reads keyboard state, processes game keys (ESC, Enter,
 * configurable bindings), runs Gameplay_Tick when g_SubState == 0,
 * handles state transitions (pause, round end, game over),
 * then renders via the software framebuffer presentation path.
 *
 * g_SubState values in gameplay:
 *   0 = active play (runs Gameplay_Tick)
 *   1 = paused
 *   2 = confirm exit
 *   3 = post-round transition
 *   4 = menu/level preview (waiting for Enter)
 *   100 = round end
 *   101 = game over */
void Game_Update_Render(void)
{
    /* NOTE: Cursor sync is handled by the SDL main loop
     * so it runs for ALL game states, not just g_GameState==0. */

    /* ---- Input processing ---- */
    /* Key debounce: original uses DAT_00489ee8 (cooldown timestamp) +
     * DAT_00489eec (last key scan code). Cooldown clears when the
     * previously pressed key is released, allowing immediate response
     * to a different key. */
    if (g_ProcessInput != 0) {
        /* Clear cooldown when the previously-tracked key is released */
        if ((g_KeyboardState[DAT_00489eec] & 0x80) == 0) {
            DAT_00489ee8 = 0;
        }
        uint32_t now_input = Platform_GetTicks();

        /* Complete the otherwise partial zero-human fallback as a spectator
         * view. This camera is not inserted into the human-player table. */
        if (DAT_00487808 == 0 && g_SubState == GAMEPLAY_ACTIVE) {
            const int camera_step = 4 * FIXED_SCALE;
            if ((g_KeyboardState[0xc8] & 0x80) != 0) g_SpectatorCameraY -= camera_step;
            if ((g_KeyboardState[0xd0] & 0x80) != 0) g_SpectatorCameraY += camera_step;
            if ((g_KeyboardState[0xcb] & 0x80) != 0) g_SpectatorCameraX -= camera_step;
            if ((g_KeyboardState[0xcd] & 0x80) != 0) g_SpectatorCameraX += camera_step;

            int min_x = (7 + 320) * FIXED_SCALE;
            int min_y = (7 + 240) * FIXED_SCALE;
            int max_x = ((int)DAT_004879f0 - 7 - 320) * FIXED_SCALE;
            int max_y = ((int)DAT_004879f4 - 7 - 240) * FIXED_SCALE;
            if (max_x < min_x) min_x = max_x = ((int)DAT_004879f0 / 2) * FIXED_SCALE;
            if (max_y < min_y) min_y = max_y = ((int)DAT_004879f4 / 2) * FIXED_SCALE;
            if (g_SpectatorCameraX < min_x) g_SpectatorCameraX = min_x;
            if (g_SpectatorCameraX > max_x) g_SpectatorCameraX = max_x;
            if (g_SpectatorCameraY < min_y) g_SpectatorCameraY = min_y;
            if (g_SpectatorCameraY > max_y) g_SpectatorCameraY = max_y;
        }

        /* State 4: Level preview / stats screen.
         * First level (counter == 0): skip straight to gameplay.
         * Subsequent levels: show stats overlay, wait for Enter/F10. */
        if (g_SubState == GAMEPLAY_LEVEL_PREVIEW && Netplay_LocalSessionControlsAllowed()) {
            if ((unsigned char)DAT_0048693c == 0) {
                /* First level — no stats to show, start immediately */
                g_SubState = GAMEPLAY_ACTIVE;
                g_NeedsRedraw = 2;
                g_TimerStart = Platform_GetTicks();
                g_TimerAux = 0;
                g_FrameTimer = Platform_GetTicks();
            } else {
                if ((g_KeyboardState[0x44] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                    g_SubState = GAMEPLAY_LEVEL_ADVANCE;
                    *(unsigned char *)&DAT_0048693c = g_GameConfig.values.active_level_count;
                    Netplay_BroadcastGameplayStateNow();
                }
                if ((g_KeyboardState[0x1C] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                    DAT_00489ee8 = now_input + 500;
                    DAT_00489eec = 0x1C;
                    g_SubState = GAMEPLAY_ACTIVE;
                    g_NeedsRedraw = 2;
                    g_TimerStart = Platform_GetTicks();
                    g_TimerAux = 0;
                    g_FrameTimer = Platform_GetTicks();
                }
            }
        }

        /* State 2: ESC pause menu — F10/Enter/ESC.
         * Sprite 0x37 panel: "F10 Exit to menu / Enter Next level / Esc Back to the game" */
        if (g_SubState == GAMEPLAY_EXIT_MENU && Netplay_LocalSessionControlsAllowed()) {
            /* F10 (scan 0x44): exit to menu */
            if ((g_KeyboardState[0x44] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                *(unsigned char *)&DAT_0048693c = g_GameConfig.values.active_level_count;
                g_SubState = GAMEPLAY_LEVEL_ADVANCE;
                DAT_004892a5 = 0;
                Netplay_BroadcastGameplayStateNow();
            }
            /* Enter (scan 0x1C): next level */
            if ((g_KeyboardState[0x1C] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = 0x1C;
                g_SubState = GAMEPLAY_LEVEL_ADVANCE;
                DAT_004892a5 = 0;
                g_NeedsRedraw = 1;
                Netplay_BroadcastGameplayStateNow();
            }
        }

        /* P key (configurable) — toggle pause.
         * Only when g_SubState < 2 (not during ESC-menu or transitions). */
        if (g_SubState < GAMEPLAY_EXIT_MENU && !Netplay_IsMatchActive()) {
            unsigned char pause_key = DAT_004837ba;
            if ((g_KeyboardState[pause_key] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                g_SubState = (g_SubState == GAMEPLAY_ACTIVE)
                    ? GAMEPLAY_PAUSED : GAMEPLAY_ACTIVE;
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = (unsigned int)pause_key;
                g_NeedsRedraw = 1;
                g_TimerStart = Platform_GetTicks();
                g_TimerAux = 0;
                g_FrameTimer = Platform_GetTicks();
            }
        }

        /* ESC (scan 0x01) — toggle between active (0) and ESC menu (2).
         * Excluded from states 3 and 4. */
        if (Netplay_LocalSessionControlsAllowed() &&
            g_SubState != GAMEPLAY_ROUND_COMPLETE &&
            g_SubState != GAMEPLAY_LEVEL_PREVIEW) {
            if ((g_KeyboardState[0x01] & 0x80) != 0 && now_input >= DAT_00489ee8) {
                g_SubState = (g_SubState != GAMEPLAY_EXIT_MENU)
                    ? GAMEPLAY_EXIT_MENU : GAMEPLAY_ACTIVE;
                DAT_00489ee8 = now_input + 500;
                DAT_00489eec = 0x01;
                g_NeedsRedraw = 1;
                g_TimerStart = Platform_GetTicks();
                g_TimerAux = 0;
                g_FrameTimer = Platform_GetTicks();
            }
        }

        /* F12 (scan 0x58): immediate exit */
        if (g_KeyboardState[0x58] & 0x80) {
            GameState_Transition(GAME_STATE_SHUTDOWN);
            return;
        }
    }

    /* ---- Round-end state machine (substates 100/101 → 3 → next level) ---- */
    /* Original at 0x00461a10 / 0x00461a2d in Game_Update_Render.
     * Substate 3 is checked FIRST so it processes from the previous frame
     * (after the stats overlay had a chance to render). */
    if (g_SubState == GAMEPLAY_ROUND_COMPLETE) {
        g_SubState = GAMEPLAY_ACTIVE;
        if ((unsigned char)DAT_0048693c >= g_GameConfig.values.active_level_count) {
            /* All rounds completed → game over / scoreboard path */
            GameState_Transition(GAME_STATE_GAME_OVER);
        } else {
            /* More rounds to play → reload menu with next level */
            GameState_Transition(GAME_STATE_RETURN_TO_MENU);
        }
        return;
    }

    /* Level advance and natural match completion both enter round-complete,
     * increment the level slot, and fall through
     * to rendering so the stats overlay displays for one frame. */
    if (g_SubState == GAMEPLAY_LEVEL_ADVANCE ||
        g_SubState == GAMEPLAY_MATCH_COMPLETE) {
        if (g_SubState == GAMEPLAY_LEVEL_ADVANCE) {
            /* Level skip: clear victory flag */
            DAT_004892a4 = 0;
            DAT_00487640[0] = 0;
        } else {
            /* Natural round end: store winning team for display */
            DAT_00487640[0] = DAT_004892a4;
            /* Increment win counter for the winning team (teams 1-4) */
            unsigned char winner = (unsigned char)DAT_004892a4;
            if (winner >= 1 && winner <= 4) {
                g_TeamWins[winner - 1]++;
            }
        }
        DAT_004892a5 = 0;

        /* Advance to next level slot */
        g_SubState = GAMEPLAY_ROUND_COMPLETE;
        (*(unsigned char *)&DAT_0048693c)++;

        /* Preserve the completed round's live counters for the post-match pages.
         * Original Game_Update_Render 0x00461A6B-0x00461AE2 performs this copy
         * exactly once while transitioning substate 100/101 to substate 3. */
        if (DAT_00487aa4 != NULL) {
            unsigned char *team_stats = (unsigned char *)DAT_00487aa4;
            for (int team = 0; team < 4; team++) {
                int *live = (int *)(team_stats + team * 0x4000);
                DAT_00486944[team] = tou_binary::add_wrap_i32(
                    DAT_00486944[team], live[1]);
                DAT_00486954[team] = tou_binary::add_wrap_i32(
                    DAT_00486954[team], live[0]);
            }
            DAT_00486964 = tou_binary::add_wrap_i32(
                DAT_00486964,
                tou_binary::add_wrap_i32(DAT_0048929c,
                    *(int *)(team_stats + 0xC004)));
        }
        for (int player_index = 0; player_index < DAT_00489240; player_index++) {
            PlayerData *player = Player_Get(player_index);
            DAT_00486968[player_index] = tou_binary::add_wrap_i32(
                DAT_00486968[player_index], player->frag_count);
            DAT_00486aa8[player_index] = tou_binary::add_wrap_i32(
                DAT_00486aa8[player_index], player->death_count);
        }

        /* Fall through to rendering — stats overlay will be drawn this frame */
    }

    /* ---- Game logic update ---- */
    switch (g_SubState) {
    case GAMEPLAY_ACTIVE:
        Gameplay_Tick();
        break;

    case GAMEPLAY_LEVEL_PREVIEW:
        /* Update timing but don't run simulation */
        {
            uint32_t now = Platform_GetTicks();
            DAT_004877f0 = now - g_FrameTimer;
            g_FrameTimer = now;
        }
        break;

    default:
        /* Other states (paused, round end, etc.) - update timing */
        {
            uint32_t now = Platform_GetTicks();
            DAT_004877f0 = now - g_FrameTimer;
            g_FrameTimer = now;
        }
        break;
    }

    /* ---- Rendering ---- */
    Render_Frame();

    /* NOTE: No additional frame rate limiter here.
     * The original FUN_00461710 has NO separate frame limiter —
     * Gameplay_Tick (FUN_0045daa0) already regulates frame pacing
     * via its built-in busy-wait timing loop. Adding a second
     * Sleep-based limiter here caused timing interference that
     * produced uneven frame spacing (jittery motion). */
}

/* ===== Handle_Menu_State (004611D0) ===== */
/* SEH wrapper around Menu_Init_And_Loop.
 * Original wraps in __try/__except with handler at 00474000.
 * On success (return 1): g_GameState = 0 (GAMEPLAY)
 * On failure (return 0): g_GameState = 6 (ERROR_RESTART) */
void Handle_Menu_State(void)
{
    int result = Menu_Init_And_Loop();
    /* -(result != 1) & 6 → if result==1: 0, else: 6 */
    GameState_Transition(result != 1
        ? GAME_STATE_ERROR_RESTART : GAME_STATE_GAMEPLAY);
}

/* ===== Init_New_Game (004228A0) ===== */
/* Clears sprite arrays, reinitializes math tables, and loads sprites.
 * Called from Game_State_Manager case 0x98 (NEW_GAME).
 * Returns 1 on success, 0 on failure. */
int Init_New_Game(void)
{
    /* Clear sprite metadata tables and reload all sprites from disk */
    DAT_00481d28 = 0;
    DAT_00481d24 = 0;
    memset(DAT_00489234, 0, 20000 * 4);
    memset(DAT_00489e8c, 0, 20000);
    memset(DAT_00489e88, 0, 20000);
    Init_Math_Tables((int *)DAT_00487ab0, 0x800);
    FUN_00423150();

    return 1;
}

/* ===== Free_Game_Resources (0040FFC0) ===== */
/* Frees per-level resources allocated during Load_Level_Resources / Load_Level_File.
 * Called from Game_State_Manager on return-to-menu, game-over, and error-restart.
 * The original binary tail-calls FUN_0041bad0 via JMP; here we call normally. */
void Free_Game_Resources(void)
{
    Mem_Free(DAT_00481f50);    /* background RGB565 pixels */
    DAT_00481f50 = NULL;
    DAT_0048782c = NULL;       /* tilemap is part of DAT_00481f50 combined alloc */

    Mem_Free(DAT_00487814);    /* coarse grid buffer */
    DAT_00487814 = NULL;

    Mem_Free(DAT_00489ea4);    /* shadow grid 1 */
    DAT_00489ea4 = NULL;

    Mem_Free(DAT_00489ea8);    /* shadow grid 2 */
    DAT_00489ea8 = NULL;

    if (DAT_00483960 == '\x01') {
        Mem_Free(DAT_00489ea0);    /* swap/heightmap (only if swap-file enabled) */
        DAT_00489ea0 = NULL;
    }

    if (DAT_00487820 != NULL) {
        Mem_Free(DAT_00487820);    /* edge/boundary navigation data */
    }
    DAT_00487820 = NULL;

    FUN_0041bad0();    /* free per-player visibility buffers */

    /* Reset all gameplay entity/subsystem counters so they don't carry over
     * to the next level or persist in the menu (e.g. fluid bubbles). */
    g_EntityCount = 0;   /* emitter/complex particle count */
    g_ParticleCount = 0;   /* fire particle count */
    g_FluidSourceCount = 0;   /* fluid source count */
    g_DebrisItemCount = 0;   /* bullet count */
    DAT_0048926c = 0;   /* item/pickup count */
    DAT_00489270 = 0;   /* trap/door count */
    DAT_00489274 = 0;   /* turret/static entity count */
    DAT_004892d8 = 0;   /* spawner/emitter def count */
    g_TrooperCount = 0;   /* trooper count */
    g_MapEdgeCount = 0;   /* edge entity count */
    DAT_004892a8 = 0;   /* round timer */
}
