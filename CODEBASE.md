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
| `tou.h` | Shared declarations; still oversized and scheduled for gradual splitting |

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
`tou.rc`. Generated objects, executables, traces, logs, and `dist/` packages are
ignored by Git.

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
