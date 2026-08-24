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
  -> software rendering into a backend-neutral RGB565 framebuffer
  -> platform presentation through the selected render backend
  -> audio through the dynamically loaded FMOD library
```

The project deliberately remains 32-bit because original pointer sizes,
structure strides, overflow, and x87 behavior are part of the reconstruction.

## Rendering Boundary

The recovered renderer composes each frame into a 640x480 RGB565
`Framebuffer`. World, effects, particles, HUD, fog, and menu entry points share
that typed buffer and a single active `Viewport` record. Lifted renderer bodies
may still create local integer views where preserving original 32-bit pointer
arithmetic matters; those views no longer leak through subsystem call sites.

`RenderBackend` owns platform presentation. The current DirectDraw backend is
fully contained in `gfx_ddraw.cpp`: device initialization, surface lifecycle,
surface restoration, RGB565-to-ARGB conversion, aspect-ratio scaling, and the
final window blit. Simulation and software rendering code do not access
DirectDraw objects. A future SDL backend can consume the same `Framebuffer`
without changing gameplay rendering.

## Source Map

| Module | Responsibility |
| --- | --- |
| `winmain.cpp` | Windows entry point, focus handling, window/input ownership |
| `gameloop.cpp` | Top-level game states, match lifecycle, frame orchestration |
| `init.cpp` | Defaults, config persistence, menus, asset discovery, startup data |
| `config.h` | Byte-exact typed `options.cfg` layout and recovered field aliases |
| `menu.cpp` | Menu behavior, player setup, rendering helpers, config interaction |
| `sim.cpp` | Main simulation dispatcher and legacy entity behavior fallback |
| `entity.cpp` | Player ships, AI, physics, weapons, collisions, entity lifecycle |
| `entity_callbacks.cpp` | Recovered original-address callbacks for weapons/effects |
| `binary_compat.cpp` | Original MSVC RNG, raw little-endian access, wrapping math, x87 conversion |
| `effects.cpp` | Explosions, particles, lighting, terrain effects |
| `graphics.cpp` | Backend-neutral RGB565 frame composition, primitives, sprites |
| `render_backend.cpp` | Active presentation-backend interface and dispatch |
| `gfx_ddraw.cpp` | DirectDraw device, surfaces, RGB conversion, scaling, presentation |
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
| `gfx.h` | Graphics, assets, typed framebuffer/viewport, effects, and HUD declarations |
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
| `options.cfg` | User configuration generated beside the executable on first run |
| `fmod.dll` | Legacy audio runtime loaded dynamically at startup |

`scripts/package-release.ps1` is the canonical list of files included in a
release. It packages runtime files only: repository Markdown and the local
`options.cfg` are intentionally excluded. Keep it synchronized when adding a
new required runtime path.

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
and stable offsets. `Init_Memory_Pools` allocates 2600 physical records; the
gameplay loop's 2500 limit is an active-entity cap, not the allocation size.

The same file defines the independently recovered 0x40-byte projectile and
trooper records plus the two distinct 0x20-byte animated-particle and
debris/item records. Their storage globals and allocation sizes are typed, and
named capacity constants replace scattered pool-limit literals. Neutral field
names are intentional where different subtypes reuse the same offset.

### Player record layout

`types.h` also documents the verified portions of the original 0x598-byte
`PlayerData` record. The storage pool is now a `PlayerData *`, and
`Player_Get(index)` is the common typed boundary. This conversion was delayed
until the last direct byte-offset call sites had been lifted.

Typed player code currently covers position snapshots, keyboard input, timer
updates, steering, thrust/exhaust, core AI/life-state dispatch, positional
sound, and the complete gameplay weapon/effect dispatcher. Opaque fields and
raw call sites must be migrated only after their access width and meaning are
verified from the original binary.

Several record fields are deliberately named `auxiliary` or `scratch`. Original
callbacks reuse the same byte or integer as cooldown, color, lifetime, fuse,
collision guard, or cadence depending on entity type. Do not give one of these
fields a universal semantic name until every relevant callback has been traced.
In particular, offsets `+0x54` and `+0x5C` are byte-sized fields, while `+0x34`
stores a 32-bit guest callback address rather than a native C++ function pointer.

The entity pool is typed as `Entity *g_EntityPool`. Completed migrations cover
the main renderer and update body, callback dispatch, gameplay firing, pickup
rewards, ship exhaust, ambient/level spawns, and the major scanners and
constructors. Constructors still write only fields written by recovered code;
do not zero a whole record as cleanup. Deliberate packed-width views inside an
already selected record remain raw where a typed field would change the
original access width.

The projectile, trooper, particle, and debris/item pools are typed end-to-end:
allocation, construction, effect spawning, spatial binning, targeting/AI,
collision, rendering, death, expiry, and compaction. Their primary globals are
`g_ProjectilePool`, `g_TrooperPool`, `g_ParticlePool`, and
`g_DebrisItemPool`; declaration comments retain the original global names.

Packed-width helpers remain inside an already selected typed record only where
the original reads or writes across a nominal field boundary. Splitting those
operations into independent field assignments can change behavior.

## Config Ownership

`options.cfg` has one canonical, packed `GameConfig` representation in
`config.h`. Its typed layout is exactly 6408 bytes; the extra window-mode byte
remains appended for compatibility with decomp-generated saves. Recovered
`DAT_004837xx` names are aliases into that same record, so menu writes, runtime
reads, loading, and saving cannot drift into stale copies anymore.

`g_ConfigBlob` remains as a byte view for menu descriptors and fields whose
semantics are not proven yet. It is not separate storage. Offset assertions in
`config.h` protect the known binary layout, and the obsolete two-way sync
functions have been removed.

The ten bytes at original addresses `0x483963..0x48396c` are per-level physics
tuning, not part of the saved record. They now live in `LevelPhysicsTuning`;
the old reconstruction wrote them past the end of the config allocation.

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
