# TOU Codebase Guide

## Read This First

This is a 32-bit, behavior-first reconstruction of a Windows game. It is not yet
a conventional modern C++ project. Raw offsets and original addresses often
encode facts that have not been safely expressed as types.

The safest cleanup rule is: **name and isolate understood behavior before
changing its representation**. A build passing does not prove gameplay parity.

## Runtime Shape

`WinMain` initializes platform services and game data, then enters the main game
loop. The broad flow is:

```text
winmain.cpp
  -> initialization and asset/config loading
  -> menu or match state
  -> input, simulation, entity callbacks, effects
  -> software rendering into the DirectDraw framebuffer
  -> audio through the dynamically loaded FMOD library
```

The project deliberately remains 32-bit because original pointer sizes,
structure strides, overflow, and x87 behavior are part of the reconstruction.

## Source Map

| Module | Responsibility |
| --- | --- |
| `winmain.cpp` | Windows entry point, focus handling, window/input ownership |
| `gameloop.cpp` | Top-level game states, match lifecycle, frame orchestration |
| `init.cpp` | Defaults, config persistence, menus, asset discovery, startup data |
| `menu.cpp` | Menu behavior, player setup, rendering helpers, config interaction |
| `sim.cpp` | Main simulation dispatcher and legacy entity behavior fallback |
| `entity.cpp` | Player ships, AI, physics, weapons, collisions, entity lifecycle |
| `entity_callbacks.cpp` | Recovered original-address callbacks for weapons/effects |
| `binary_compat.cpp` | Original MSVC RNG, raw little-endian access, wrapping math, x87 conversion |
| `effects.cpp` | Explosions, particles, lighting, terrain effects |
| `graphics.cpp` | DirectDraw setup, framebuffer rendering, primitives, sprites |
| `hud.cpp` | Match HUD, weapon grid, Mark selector, scores |
| `assets.cpp` | Font and image asset loading |
| `level.cpp` | `.lev` loading and swap/height-map data |
| `gg_gen.cpp` | Procedural GG level/theme generation |
| `sound.cpp` | Music, effects, positional sound, FMOD-facing game logic |
| `fmod_loader.c` | Runtime loading of the bundled legacy `fmod.dll` |
| `intro.cpp` | Intro presentation |
| `memory.cpp` | Recovered allocation and shared-memory helpers |
| `math.cpp` | Small math compatibility helpers |
| `utils.cpp` | Optional debug logging |
| `tou.h` | Aggregate include retained while source files migrate to narrower headers |
| `types.h` | Shared recovered structures |
| `gfx.h` | Graphics, assets, viewport, effects, and HUD declarations |
| `input.h` | DirectInput and keyboard/mouse declarations |
| `sound.h` | FMOD-facing audio declarations |
| `level.h` | Level data, loading, and GG generator declarations |
| `entity.h` | Entity pools, simulation, AI, collision, and spawning declarations |
| `gamestate.h` | Application state, config, menu, lifecycle, and memory declarations |
| `compat.h` | Legacy Windows and DirectX API-level includes |
| `fixed_point.h` | Verified 18-fractional-bit world-coordinate constants |

## Runtime Data

The executable expects these paths relative to its working directory:

| Path | Contents |
| --- | --- |
| `data/` | Fonts, palettes, sprite atlases, menu images, name tables |
| `levels/` | Standard `.lev` maps |
| `ggstuff/` | Procedural-level themes and graphics |
| `music/` | Menu and level music |
| `sfx/` | Weapon, UI, ship, and environment sounds |
| `ships/` | `.SHP` ship definitions |
| `swap/` | Precomputed level sky/height-map data |
| `options.cfg` | Original 6408-byte configuration blob |
| `fmod.dll` | Legacy audio runtime loaded dynamically at startup |

`scripts/package-release.ps1` is the canonical list of files included in a
release. Keep it synchronized when adding a new required runtime path.

## Binary-Compatibility Layer

`binary_compat.*` exists because mathematically similar modern C++ is not always
behaviorally equivalent to the original executable. It provides:

- explicit little-endian reads and writes into recovered records;
- defined 32-bit wrapping arithmetic and signed right shifts;
- the original embedded MSVC `rand`/`srand` algorithm and call counter;
- x87-style float-to-integer conversion on 32-bit x86.

`tou.h` maps normal `rand` and `srand` calls to this implementation. Do not
replace it with the host CRT or a C++ random engine.

## Entity Callback Architecture

The original game stores callback addresses in 128-byte entity records. During
the first decomp pass, much of that behavior became a large approximate switch
in `sim.cpp`. The parity pass restored address-based dispatch:

1. `EntityCallbacks_Init` installs the recovered callback table.
2. `sim.cpp` reads the guest callback address from entity offset `+0x34`.
3. `EntityCallbacks_Dispatch` runs a lifted callback when available.
4. Unknown/unlifted callbacks continue through the legacy fallback path.
5. `EntityCallbacks_RemoveAt` preserves the original selective compaction and
   tracking updates.

Callback constants intentionally retain original virtual addresses. They are
identity keys, not host function pointers.

### Entity record layout

`types.h` documents the verified `Entity` record and asserts its 0x80-byte size
and stable offsets. `Init_Memory_Pools` allocates `0x51400` bytes at original
address `0x004203b5`, which is 2600 physical records; the gameplay loop's 2500
limit is an active-entity cap, not the allocation size.

Several record fields are deliberately named `auxiliary` or `scratch`. Original
callbacks reuse the same byte or integer as cooldown, color, lifetime, fuse,
collision guard, or cadence depending on entity type. Do not give one of these
fields a universal semantic name until every relevant callback has been traced.
In particular, offsets `+0x54` and `+0x5C` are byte-sized fields, while `+0x34`
stores a 32-bit guest callback address rather than a native C++ function pointer.

The runtime pool is now typed as `Entity *DAT_004892e8`. Raw accesses are being
replaced in small batches so each changed routine can be compared independently
against its previous generated x86 and, where relevant, the original executable.
The first batch covers callback record lookup, pre-tick flag reset, and the
menu/intro entity renderer. The second batch covers the common dispatcher
prologue in `FUN_00434310`: previous-position capture, animation bookkeeping,
callback identity, and the turret-projectile gravity guard. Its large legacy
behavior fallback and weapon-heavy paths remain raw. The third batch converts
the complete main gameplay entity renderer in `FUN_0040bb60`. Offset `+0x24`
still intentionally has both byte and 16-bit views there: the byte selects a
sprite palette, while the full word selects a pixel-dot shape.

## Config Ownership

`options.cfg` is represented by `g_ConfigBlob`. Much of the menu writes directly
to blob offsets while runtime code also mirrors selected values in globals.
That split is ugly but currently intentional. Do not make
`Sync_Config_To_Blob()` a universal save step: it can overwrite newer menu
values with stale globals. The typed-config migration in `BACKLOG.md` must solve
ownership before changing this behavior.

## Building and Cleaning

```powershell
mingw32-make -j8
mingw32-make clean
```

The Makefile compiles every C/C++ source as 32-bit and embeds `icon.ico` through
`tou.rc`. Generated objects, executables, logs, and `dist/` packages are ignored
by Git.

`.github/workflows/build.yml` repeats the 32-bit build and PE architecture check
on pushes and pull requests. It is build-only; publishing remains exclusive to
the manually dispatched release workflow.

There is intentionally no permanent standalone test executable. When a binary
discrepancy needs instrumentation, add the smallest targeted harness, compare it
against the original, and decide whether its fixtures are valuable enough to
keep before merging.

## Optional Entity Trace

Set `TOU_ENTITY_TRACE=1` before launching the decomp to write
`entity-trace.csv`. Tracing is off by default. The generated diagnostic is not
part of release packages and should be deleted after use. Use it only for
controlled original-versus-decomp investigations because file I/O can disturb
timing.

## Refactoring Rules

- Preserve original addresses in comments when renaming recovered functions.
- Introduce typed structures only after every occupied offset and stride is
  verified; use `static_assert` for recovered sizes.
- Keep integer signedness and overflow explicit.
- Preserve RNG call count and ordering.
- Preserve entity/particle update and compaction order.
- Do not reuse an effect record for convenience without proving that the
  original allocator, updater, and renderer do the same.
- Validate behavior in the running game after meaningful simulation changes.
