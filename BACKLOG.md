# TOU Decompilation — Refactor Backlog

Ordered by theme and priority. Each item includes acceptance criteria (AC).

The gameplay-parity pass is complete. These are maintenance and modernization
tasks, not permission to replace verified binary behavior wholesale. Refactors
should be small enough to compare and revert independently.

---

## Theme 1: Types & Data Structures

### T1.1 — Entity struct (128 bytes)  [IN PROGRESS / P0]
Replace raw entity blob with typed access without changing original behavior.

The verified `Entity` layout and offset assertions now live in `types.h`, and
the global pool is typed as `Entity *`. The first access migration covers the
callback lookup helper, pre-tick flag reset, and menu/intro renderer. Most
runtime code still uses raw accesses. The second batch covers the common
prologue of `FUN_00434310`: position history, animation bookkeeping, callback
identity, and the turret-projectile gravity guard. Migrate the remaining paths
separately in small, assembly-comparable batches. The third batch converts the
complete main gameplay entity renderer, including its intentional byte/word
views of `+0x24`, without changing render or RNG call order. The pool physically
allocates 2600 records (`0x51400 / 0x80`), while gameplay limits active entities
to 2500. The fourth, larger batch converts four complete entity-array scanners:
enemy proximity, owned-projectile detonation, Moving Sucker attraction/repulsion,
and force-field repulsion.

**Known layout:**
```
+0x00  int  pos_x        (fixed-point)
+0x04  int  prev_x
+0x08  int  pos_y
+0x0C  int  prev_y
+0x10  int  motion_x      (type-specific)
+0x14  int  motion_y      (type-specific)
+0x18  int  vel_x
+0x1C  int  vel_y
+0x20  byte state         (type-specific)
+0x21  byte type
+0x22  byte owner         (player/team, 0xFF when absent)
+0x24  short variant
+0x26  byte auxiliary     (cooldown/color/lifetime/guard by type)
+0x28  int  health_or_damage
+0x2C  int  scratch
+0x30  int  scratch
+0x34  uint behavior_cb   (original 32-bit virtual address, not a host pointer)
+0x38  int  gravity_or_motion
+0x3C  int  counter       (type-specific)
+0x40  byte subtype
+0x44  int  damage
+0x48  int  scratch
+0x4C  int  palette
+0x50  int  scratch
+0x54  byte anim_frame
+0x5C  byte timer         (type-specific)
+0x60  int  scratch       (type-specific fuse/state)
+0x64  byte scratch       (type-specific cadence/state)
+0x65  byte scratch
```

**Files to touch:** sim.cpp, entity.cpp, entity_callbacks.cpp, effects.cpp,
hud.cpp, menu.cpp

**AC:**
- `Entity` defined in `types.h`; `static_assert(sizeof(Entity) == 128)`
- `DAT_004892e8` becomes an `Entity *`; rename it separately under T13.1
- All `*(type *)((int)ent + offset)` replaced with `ent->field`
- Build compiles with zero warnings; no behavior change

---

### T1.2 — PlayerData struct (0x598 / 1432 bytes)  [P0]
Player array at `DAT_00487810`. Known clusters:
- `+0x00` ship type, `+0x04` team, `+0x08` lives, `+0x0C` health, `+0x10` shield, `+0x14` energy, `+0x18` score
- `+0xAC..0xB2` key scan codes (7 keys)
- `+0xB4` mouse sensitivity
- `+0xC0..0x130` weapon grid state
- `+0x200..0x400` visibility buffer pointer
- `+0x430..0x470` stat counters
- `+0x490..0x4A0` AI state

**Files:** init.cpp, entity.cpp, menu.cpp, gameloop.cpp, stubs.cpp

**AC:**
- `PlayerData` defined; `static_assert(sizeof(PlayerData) == 0x598)`
- `DAT_00487810` becomes `PlayerData *g_PlayerArray`
- All pointer arithmetic replaced

---

### T1.3 — ProjectileRecord struct (64 bytes)  [P1]
Array at `DAT_00481f28`. Used in stubs.cpp for collision, explosions, turret targeting.

**AC:**
- `ProjectileRecord` defined; `static_assert(sizeof == 64)`
- All `DAT_00481f28` pointer math replaced in stubs.cpp

---

### T1.4 — TrooperRecord struct (64 bytes)  [P1]
Array at `DAT_00487884`. Used in spatial grid binning and AI.

**AC:**
- `TrooperRecord` defined; `static_assert(sizeof == 64)`
- All pointer math replaced

---

### T1.5 — Particle struct (32 bytes)  [P1]
Arrays at `DAT_00487830` / `DAT_00481f34`. Used in debris, explosions, fluid effects.

**AC:**
- `Particle` defined; `static_assert(sizeof == 32)`
- All particle array pointer math replaced in stubs.cpp, effects.cpp

---

### T1.6 — TileType enum + accessors  [P1]
Tilemap stores one byte per cell. Magic constants scattered everywhere.

**Known values:** 0x00 empty, 0x01 solid, 0x02 water, 0x03 lava, 0x04 conveyor, 0x05 ice, 0x0E force field, 0x0F teleporter

**AC:**
- `enum TileType` defined with all known values
- Helper inlines: `IsSolid()`, `IsWalkable()`, `IsFluid()`
- All raw tile byte comparisons replaced

---

### T1.7 — Fixed-point constant 0x40000  [DONE]
Fixed-point scale with 18 fractional bits, hardcoded in 50+ locations. One whole
unit is `0x40000` (`1 << 18`); avoid the ambiguous and incorrect "18.14" label.

**AC:**
- [x] `#define FIXED_SCALE 0x40000` and `#define FIXED_SHIFT 18`
- Optional `FIXED_TO_INT()` / `INT_TO_FIXED()` helpers
- [x] All confirmed fixed-point-scale uses of raw `0x40000` replaced
- [x] Rebuilt `.text`, `.data`, and `.rdata` match the pre-refactor executable
  byte-for-byte

---

## Theme 2: Headers & Modules

### T2.1 — Split `tou.h` into subsystem headers  [DONE]
`tou.h` was 791 lines of intermixed externs. It is now a 24-line aggregate over:
- `types.h` — structs, enums, constants
- `gfx.h` — DirectDraw globals, render prototypes
- `input.h` — DI globals, keyboard/mouse state
- `sound.h` — FMOD globals, sound table
- `level.h` — map data, tilemap, .lev format
- `entity.h` — entity arrays, behavior callbacks, AI globals
- `gamestate.h` — state machine, timers, config
- `compat.h` — Windows/DirectX version macros

Keep `tou.h` as an aggregate include during transition.

`fixed_point.h` separately owns the verified world-coordinate constants.

**AC:**
- [x] Each header contains only relevant declarations; no circular includes
- [x] All `.cpp` files still compile with `#include "tou.h"`
- [x] Every subsystem header passes a standalone syntax check
- [x] Rebuilt `.text`, `.data`, and `.rdata` match the pre-split executable
  byte-for-byte

---

### T2.2 — Centralize math tables in `game_math.h` / `math.cpp`  [P2]
Trig tables, ballistic LUT (`DAT_00489e90`), and vector helpers currently live in init.cpp and entity.cpp.

**AC:**
- `game_math.h` declares all pure math functions without shadowing the system
  `<math.h>` header
- `math.cpp` contains all table init and helpers
- No math tables duplicated across files

---

## Theme 3: Memory Safety

### T3.1 — Bounds checks on all spawn/alloc sites  [P0]
Hard limits: 2500 entities, 2000 particles, 5000 fluid sources, 300 level names.

**Key functions:** `FUN_00413720`, `FUN_00434310`, `FUN_00454b00`, `FUN_0045fc00`, `FUN_00407210`, `FUN_00406d20`

**AC:**
- Every spawn function checks `count < capacity` before writing
- Overflow returns error/skips spawn (not assert in release)
- Capacity constants defined as `enum`/`constexpr` next to array declarations

---

### T3.2 — Replace `void *` pool pointers with typed arrays  [P1]
After T1.1–T1.5 are done, change declarations:
- `Entity *g_EntityArray` ← `DAT_004892e8`
- `PlayerData *g_PlayerArray` ← `DAT_00487810`
- `ProjectileRecord *g_ProjectileArray` ← `DAT_00481f28`
- `Particle *g_ParticleArray` ← `DAT_00487830`

**AC:**
- All `void *` pool pointers replaced with typed pointers
- All casts at call sites removed

---

### T3.3 — Audit `memcpy`/`memset` on typed structs  [P1]
Original binary uses raw memory ops on entity/player blobs.

**AC:**
- All `memset`/`memcpy` calls use `sizeof(Entity)` / `sizeof(PlayerData)`
- `static_assert` that structs are trivially copyable

---

## Theme 4: Renderer Abstraction

### T4.1 — `Framebuffer` struct  [P1]
Current: every render function takes `(int buffer, int stride)` and casts to `unsigned short *`.

```c
typedef struct {
    unsigned short *pixels;
    int width, height, stride;  // stride in pixels
} Framebuffer;
```

**AC:**
- `Framebuffer` defined in `gfx.h`
- All render functions take `Framebuffer *` instead of raw `(buffer, stride)`
- No `(unsigned short *)buffer` casts remain at call sites

---

### T4.2 — `Viewport` struct  [P1]
Viewport culling math duplicated in effects.cpp, hud.cpp, graphics.cpp.

```c
typedef struct {
    int left, top, right, bottom;
    int width, height;
    int scroll_x, scroll_y;
} Viewport;
```

**AC:**
- `Viewport` defined
- All viewport rect math uses the struct
- Clip helpers like `ClipRectToViewport()` added

---

### T4.3 — `RenderBackend` interface  [P2]
Isolate DirectDraw behind an interface for future SDL2/OpenGL migration.

```c
typedef struct {
    int  (*init)(int w, int h);
    void (*begin_frame)(Framebuffer *fb);
    void (*present)(void);
    void (*shutdown)(void);
} RenderBackend;
```

**AC:**
- `RenderBackend` interface defined
- DirectDraw implementation in `gfx_ddraw.cpp`
- `graphics.cpp` calls backend instead of `lpDD` directly
- Build still works with DirectDraw backend

---

### T4.4 — Sprite atlas abstraction  [P2]
`all3.gfx` is a custom packed format loaded by `FUN_00423150`. Abstract into:

```c
typedef struct {
    int pixel_offset, width, height;
} SpriteFrame;

void Sprite_Blit(const SpriteFrame *frame, int x, int y,
                 unsigned char palette, Framebuffer *fb);
```

**AC:**
- Sprite loader returns a `SpriteAtlas` struct with frame table
- All blit sites use `Sprite_Blit()` instead of raw pointer math into `DAT_00487ab4`

---

## Theme 5: Config System

### T5.1 — Replace 6408-byte blob with typed `GameConfig` struct  [P0]
Current: `g_ConfigBlob[6408]` with aliased offsets like `DAT_0048227c = &g_ConfigBlob[0x324]`.

**Known fields:** display mode, sound config, key bindings, fog settings, difficulty, team mode, game type, player names, ship selections, weapon loadouts.

**AC:**
- `GameConfig` struct with all known fields
- `Load_Options_Config()` deserializes blob -> struct
- `Save_Options_Config()` serializes struct -> blob
- All direct blob offset writes in menu code replaced with `config->field = value`
- `Sync_Config_From_Blob()` kept for loading; `Sync_Config_To_Blob()` becomes the save path

---

### T5.2 — JSON/INI serializer for config  [P2]
Once T5.1 is done, add human-readable config format.

**AC:**
- `Save_Config_JSON()` writes pretty-printed JSON
- `Load_Config_JSON()` reads it back
- Falls back to binary blob if JSON not found
- All 6408 bytes round-trip correctly

---

## Theme 6: State Machine

### T6.1 — Replace magic byte states with enum + transitions  [P1]
Current: `g_GameState` is a byte with values 0x01, 0x02, 0x96, 0x97, 0x98, 0xFE.

**AC:**
- `enum GameState { STATE_GAMEPLAY=1, STATE_MENU=2, STATE_INIT_GAME=3, STATE_QUICK_RESTART=4, STATE_GAME_OVER=5, STATE_ERROR_RESTART=6, STATE_INTRO_INIT=0x96, STATE_INTRO_RUN=0x97, STATE_NEW_GAME=0x98, STATE_SHUTDOWN=0xFE }`
- All `switch(g_GameState)` blocks updated
- `GameState_Transition(GameState from, GameState to)` helper for debug logging

---

### T6.2 — Introduce sub-state enums  [P2]
`g_SubState`, `g_SubState2`, and `DAT_00489299` interact non-obviously.

**AC:**
- Document what each sub-state means
- Replace magic values with named constants
- Add state transition logging (behind `#ifdef DEBUG`)

---

## Theme 7: Sound Abstraction

### T7.1 — `AudioEngine` interface  [P2]
Wrap FMOD 3.5 behind a portable interface.

```c
typedef struct {
    int  (*init)(void);
    int  (*load_sample)(const char *path);
    void (*play_sample)(int handle, int x, int y, int volume);
    void (*play_music)(const char *path);
    void (*stop_all)(void);
    void (*shutdown)(void);
} AudioEngine;
```

**AC:**
- Interface defined; FMOD implementation in `audio_fmod.cpp`
- All `FSOUND_*` / `FMUSIC_*` calls go through interface
- Positional audio math (distance attenuation, panning) extracted to pure function

---

## Theme 8: Input Abstraction

### T8.1 — `InputDevice` interface  [P2]
Abstract DirectInput so keyboard/mouse/gamepad can coexist.

```c
typedef struct {
    void (*poll)(void);
    int  (*key_down)(int scan_code);
    void (*get_mouse)(int *dx, int *dy, int *buttons);
} InputDevice;
```

**AC:**
- Interface defined; DI implementation in `input_dinput.cpp`
- `Input_Update()` calls through interface
- Windowed fallback (`GetCursorPos`/`GetAsyncKeyState`) moved into interface implementation

---

### T8.2 — Input mapping layer  [P2]
Map physical keys to logical actions instead of storing raw scan codes.

**AC:**
- `enum GameAction { ACTION_MOVE_UP, ACTION_FIRE, ... }`
- `Input_Bind(GameAction action, int scan_code)`
- Gameplay code queries `Input_IsActionDown(ACTION_FIRE)` instead of raw key state

### T8.3 — Expose Special/Explode binding in the controls menu  [DONE]
Completed during the menu parity pass. The existing menu action is presented as
`Menu Button/Detonate`; the gameplay's special weapon remains the secondary
weapon rather than this remote-detonation input.

**AC:**
- [x] Both players can inspect and rebind it
- [x] Saved bindings round-trip through `options.cfg`
- [x] Pipebomb and Smoking Nalle can be triggered using the rebound key

### T8.4 — Restore missing weapon-level selector dots  [DONE]
Completed and runtime-tested for the full weapon set, including Roman Candle
level 3 (`Fireworks Launcher`). Changing weapons also resets selection to Mark I
like the original.

**AC:**
- [x] Selector dot count comes from the weapon's actual available-level data
- [x] Roman Candle visibly exposes all three levels
- [x] Every weapon was audited for hidden or extra levels
- [x] Each displayed dot maps to the correct subtype

---

## Theme 9: AI & Pathfinding

### T9.1 — AI behavior extraction  [P2]
Current ship AI remains concentrated in `entity.cpp`; recovered weapon/effect
callbacks now live in `entity_callbacks.cpp`.

**AC:**
- Extract `FUN_0044ad30` into `ai_decide(Entity *e, PlayerData *player)`
- Extract `FUN_0044be20` into `ai_scan_threats()`
- Extract `FUN_00458010` into `ai_turret_target(Turret *t)`
- BFS pathfinding wrapped in `Pathfinder_FindPath(start, goal)`

---

## Theme 10: Particle & Effect Systems

### T10.1 — Particle system abstraction  [P2]
2000-entry particle array with inline physics in stubs.cpp.

**AC:**
- `ParticleSystem_Spawn(type, x, y, vx, vy, lifetime, palette)`
- `ParticleSystem_Update()` replaces inline tick code
- `ParticleSystem_Render(Framebuffer *fb)`

---

### T10.2 — Fluid simulation abstraction  [P2]
Water/lava propagation in `FUN_0045fc00`.

**AC:**
- `FluidSystem_Update()` wrapper
- `FluidSource_Spawn(x, y, type)`
- Separate from particle system (different physics rules)

---

## Theme 11: Platform Migration

### T11.1 — SDL2 renderer backend  [P2]
Once T4.3 (RenderBackend) is done, implement SDL2 backend.

**AC:**
- `gfx_sdl2.cpp` implements `RenderBackend`
- Compiles on Windows + Linux
- Software buffer blit replaced with SDL_Texture upload
- Windowed mode works without ddraw proxy DLL

---

### T11.2 — Modern GPU renderer (optional)  [P3]
OpenGL 3.3 or D3D11 backend for the software renderer.

**AC:**
- Upload RGB565 software buffer as texture
- Sprite blitting via instanced quads or texture atlas
- Post-processing for color LUTs (fog, brightness, water tint)

---

## Theme 12: Build & Tooling

### T12.0 — Build-only GitHub Actions validation  [DONE]
The `Build` workflow runs separately from release automation on every push and
pull request, with optional manual dispatch.

**AC:**
- [x] Clean 32-bit MinGW build on `windows-latest`
- [x] Result verified as `pei-i386`
- [x] Read-only repository permissions
- [x] No packaging, release creation, or tag mutation

---

### T12.1 — CMake build system  [P2]
Replace Makefile with cross-platform CMake.

**AC:**
- `CMakeLists.txt` builds all targets
- Finds SDL2 / FMOD / DirectX as appropriate
- Configures for 32-bit and 64-bit
- CI-friendly (no hardcoded paths)

---

### T12.2 — Targeted parity harnesses  [ON DEMAND]
The exploratory standalone accuracy-test executable was retired when the parity
runtime became production code. Do not maintain a second executable by default.
Add focused harnesses only for concrete regressions where a runtime comparison
alone cannot expose the first state divergence.

**AC:**
- Fixtures come from original-binary evidence or a recorded runtime scenario
- Harness does not alter RNG or update ordering in the normal game build
- Valuable deterministic fixtures may be promoted into CI later

---

## Theme 13: Code Quality

### T13.1 — Rename all `DAT_00xxxxxx` globals  [P1]
Rename globals to semantic names as subsystems are understood.

**AC:**
- `g_EntityArray`, `g_PlayerArray`, `g_TileMap`, `g_SpritePixels`, etc.
- Preserve original addresses in comments: `/* was DAT_004892e8 */`
- No functional change

---

### T13.2 — Replace Ghidra function names  [P2]
Rename `FUN_004xxxxx` to semantic names after understanding each function.

**Priority order:**
1. Entity behavior loop (`FUN_0044b0b0` -> `Entity_Behavior_Loop`)
2. Main render loop (`FUN_00425fe0` -> `Game_Render`)
3. Physics integration (`FUN_0044e1c0` -> `Physics_Integrate`)
4. Wall collision (`FUN_00450630` -> `Collision_Wall`)
5. Bullet collide (`FUN_00455d50` -> `Projectile_Collide`)
6. Explosion/damage (`FUN_004571f0` -> `Explosion_Damage`)
7. Turret update (`FUN_00454b00` -> `Turret_Update`)
8. AI targeting (`FUN_00458010` -> `AI_Turret_Target`)
9. Fluid spread (`FUN_0045fc00` -> `Fluid_Spread`)
10. Process deaths (`FUN_0045e2c0` -> `Entity_Process_Deaths`)

**AC:**
- Each renamed function has a comment explaining its original binary address
- All call sites updated (use `sed` or IDE rename)
- Build compiles

---

## Theme 14: Bug Fixes & Cleanup

### T14.1 — Fix `Sync_Config_To_Blob` dead code  [P1]
Currently marked dead because menu writes directly into blob offsets.

**AC:**
- After T5.1 (typed config), `Sync_Config_To_Blob()` becomes the canonical save path
- Menu code never writes to raw blob offsets
- Round-trip test: load -> modify -> save -> load produces identical config

---

### T14.2 — Remove unused globals  [P2]
Some `extern` declarations in the subsystem headers may reference dead code.

**AC:**
- Run linker with `--gc-sections` or grep for unused symbols
- Remove declarations with no definition
- Remove definitions with no references

---

### T14.3 — Intro splash index reversal  [P2]
Documented quirk: splash frame indices are reversed (starts at 2, then 1, then 0) despite `g_IntroSplashIndex` counting upward.

**AC:**
- Determine if this is intentional (artist preference) or a bug
- If bug: fix the frame lookup in `intro.cpp`
- If intentional: add explicit comment explaining the reversal

---

## Theme 15: Documentation

### T15.0 — Document the current source architecture  [DONE]
`CODEBASE.md` documents module ownership, runtime assets, callback dispatch,
binary-compatibility constraints, config ownership, tracing, builds, packaging,
and safe refactoring rules.

---

### T15.1 — Document `.lev` v1.4 format spec  [P1]
Write a standalone `docs/LEVEL_FORMAT.md`.

**AC:**
- Byte-level format description
- C struct layout for each section
- Example hex dump of a small level
- Entity record format (20 bytes)
- RLE tilemap encoding

---

### T15.2 — Document `.gfx` sprite format  [P2]
Write `docs/SPRITE_FORMAT.md`.

**AC:**
- Header layout (frame offset table, width/height tables)
- Pixel data layout (RGB555)
- Mask data layout (grayscale)
- Code example for loading a single frame

---

### T15.3 — Document `.SHP` ship format  [P2]
Write `docs/SHIP_FORMAT.md`.

**AC:**
- 64-byte record layout
- Field descriptions (speed, health, weapons, sprites)
- Example for one ship type

---

## Dependency Graph

```
T1.1 (Entity) ─┬─> T3.2 (typed pools)
               ├─> T9.1 (AI extraction)
               └─> T13.2 (rename)

T1.2 (Player) ─┬─> T3.2 (typed pools)
               └─> T5.1 (config)

T1.3 (Projectile) ──> T3.2
T1.4 (Trooper) ─────> T3.2
T1.5 (Particle) ────> T3.2

T2.1 (Header split) ──> all subsequent themes (cleaner includes)

T4.1 (Framebuffer) ─┬─> T4.2 (Viewport)
                    └─> T4.3 (RenderBackend)

T4.3 (RenderBackend) ──> T11.1 (SDL2)

T5.1 (Config struct) ─┬─> T5.2 (JSON)
                      └─> T14.1 (Sync fix)

T8.1 (InputDevice) ──> T8.2 (Input mapping)
T7.1 (AudioEngine) ──> T11.1 (SDL2, optional)
```

## Suggested Sprint Order

| Sprint | Items |
|--------|-------|
| 1 | T1.1, T1.2, T3.3 |
| 2 | T1.3, T1.4, T1.5, T1.6, T3.1, T3.2 |
| 3 | T4.1, T4.2 |
| 4 | T5.1, T6.1, T14.1 |
| 5 | T13.1, T13.2 (batch rename pass) |
| 6 | T4.3, T7.1, T8.1 |
| 7 | T9.1, T10.1, T10.2 |
| 8 | T11.1, T12.1 |
| 9 | T15.1, T15.2, T15.3 |
| 10 | T2.2, T11.2, T14.2, T14.3 (cleanup) |

---

*Backlog generated from source inspection of decompiled TOU v1.0 codebase.*
