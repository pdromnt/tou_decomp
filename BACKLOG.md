# Tunnels of Underworld Roadmap & Backlog

This is the single living plan for the project. Completed archaeology is kept
brief; actionable work is ordered by dependency and product value.

## Project Contract

Preserve the accepted behavior of the original game while turning the SDL
decomp into a maintainable, multilingual, cross-platform game with creation
tools and simple LAN multiplayer.

The required order is:

1. faithful gameplay;
2. portable desktop runtime;
3. maintainable settings and text;
4. native level-authoring pipeline;
5. serializable simulation boundary;
6. constrained LAN multiplayer.

A successful build is not runtime acceptance. Refactors must not change update
order, RNG order, integer/float behavior, callbacks, collision, terrain,
physics, effects, scoring, or original single-machine gameplay.

## Current Baseline — v0.5

- Weapons and Marks, effects, particles, turrets, enemy ships, cars, infantry,
  civilians, menus, controls, Events, levels, audio, match results, and awards
  have had hands-on parity testing.
- SDL3 owns the entry point, window, display, input, focus, dialogs, rendering
  presentation, and audio. DirectDraw, DirectInput, WinMain, and FMOD are gone.
- The recovered software renderer still produces the RGB565 game framebuffer.
- Entity/player/projectile/trooper/particle/item pools are typed and capacity
  guarded. Compatibility views remain only where original access width matters.
- Original MSVC RNG, wrapping arithmetic, x87 conversion, fixed-point constants,
  and guest callback-address dispatch are isolated production code.
- `GameConfig` is one packed, offset-asserted compatibility record.
- CI builds warning-clean Windows x86/x64/ARM64, Linux x64/ARM64, and macOS
  Intel/Apple Silicon packages.
- `.lev`, `.gfx`, and `.SHP` format notes live under `docs/`; architecture and
  safe-refactoring guidance live in `CODEBASE.md`.

### Remaining portability acceptance

| Target | Status |
| --- | --- |
| Windows x64 | Accepted through hands-on gameplay |
| macOS Apple Silicon | Accepted through hands-on gameplay |
| Windows x86 legacy parity | Build maintained; final release smoke test pending |
| Windows ARM64 | Runtime acceptance pending |
| Linux x64 | Runtime acceptance pending |
| Linux ARM64 | Accepted through hands-on Raspberry Pi CM4 gameplay |
| macOS Intel | Runtime acceptance pending |

Browser/WebAssembly remains last-of-last and is not part of the current plan.

## Milestone Order

| Order | Milestone | Result |
| ---: | --- | --- |
| 1 | Human-readable settings | Versioned `settings.json` replaces binary saves |
| 2 | Internationalization | English, Spanish, Brazilian Portuguese, Finnish |
| 3 | Level tools | Native compiler/library, then visual editor |
| 4 | Network foundation | Command frames, snapshots, checksums, replay |
| 5 | LAN multiplayer | Direct-IP, host-authoritative, two-to-four players |
| Later | Responsiveness/expansion | Prediction, reconnect, discovery, internet play |

---

## M1 — Human-Readable Settings  [ACCEPTED]

Replace `options.cfg` with a versioned `settings.json` containing user-facing
settings rather than a JSON dump of recovered memory.

### M1.1 — Typed user-settings model

- [x] Add a normal `UserSettings` model independent of packed `GameConfig` storage.
- [x] Map verified user settings into legacy runtime fields at one boundary.
- [x] Cover visible options, key bindings, display/audio, player profiles, colors,
  ships, weapon availability/loadouts, and level selection.
- [x] Exclude reserved bytes and level-derived/runtime-only values.
- [x] Include `schemaVersion` and `language` from schema version 1.

### M1.2 — JSON load/save and migration

- [x] Read/write pretty-printed UTF-8 JSON.
- [x] Load defaults, overlay valid JSON fields, validate/clamp, then map to runtime.
- [x] Ignore unknown keys; recover malformed known fields individually.
- [x] Save atomically through a temporary file and replacement.
- [x] If JSON is absent and a valid `options.cfg` exists, migrate known values once,
  write JSON, and retain the binary file as a backup.
- [x] Preserve the current app-local macOS settings location.

### M1 acceptance

- Every visible setting survives save/restart on Windows, Linux, and macOS.
- A settings round trip yields identical runtime choices.
- Missing, corrupt, truncated, old-schema, and future-key files fail safely.
- Reset Defaults rewrites a valid current-schema file.

---

## M2 — Internationalization  [FOUNDATION IMPLEMENTED / TRANSLATION IN PROGRESS]

Initial locales:

- English (`en`), authoritative fallback
- Spanish (`es`)
- Brazilian Portuguese (`pt-BR`)
- Finnish (`fi`)

### M2.1 — Catalog runtime

- [x] Store UTF-8 catalogs under `lang/<locale>.json` with semantic keys.
- [x] Add `Text_Get(key)` with per-key English fallback and debug diagnostics.
- [x] Store language in `settings.json`; allow immediate switching from Options.
- Keep protocol values, logs, paths, player text, and level author metadata
  outside localization.
- [x] Validate JSON/UTF-8, English extraction, and overlay keys in CI.

### M2.2 — Font and layout coverage

- [x] Extend the bitmap-font path for required Latin glyphs, including Finnish
  `ä/ö/å`, Spanish accents/punctuation, and Portuguese diacritics.
- [x] Measure localized strings instead of assuming English widths.
- Define wrapping, alignment, truncation, and fallback-glyph behavior.
- Check every menu at supported resolutions in windowed and fullscreen modes.

### M2.3 — String extraction and translations

- [ ] Extract menus, HUD, results, awards, errors, controls, weapon/pickup names,
  prompts, and gameplay messages.
- Create and review complete `en`, `es`, `pt-BR`, and `fi` catalogs.

### M2 acceptance

- All four languages complete menu -> match -> results without missing glyphs,
  clipping, or accidental English except approved proper names.
- Missing keys/locales visibly fall back to English rather than blank text.

---

## M3 — Level Compiler and Editor  [P0/P1]

The repository contains original sample sources, documentation,
`level converter.exe`, and `COLPICK.EXE`, but not maintainable source for those
legacy tools. Build a native replacement instead of embedding them.

### M3.1 — Complete writer specification  [P0]

- Recover every `.lev` header, extra/config section, placement record, marker
  color, RLE rule, JPEG/parallax block, and GG field.
- Treat original converter output and shipped levels as authoritative.
- Add golden fixtures from `makelev/Jungle.*` and `Normal.txt`.
- Document every unsupported or still-unknown value explicitly.

### M3.2 — Shared `tou_level` library and CLI  [P0]

- Parse, validate, and write normal/GG level projects without game globals.
- Import visual JPEG, attribute TGA, optional parallax, and documented config.
- Replace COLPICK marker lookup with a named palette/schema.
- Produce structural comparison reports against original converter fixtures.
- Load generated levels in both the original game and decomp.

### M3.3 — Visual editor MVP  [P1]

- New/open/save project and export `.lev`.
- Visual and attribute layers with overlay/opacity controls.
- Terrain/placement palette replacing COLPICK.
- Select, move, configure, and delete spawn points, turrets, gates, repairs,
  mines, signs, water, and every understood record.
- Edit metadata, physics, water, civilians, bombing, ambience, parallax, and GG.
- Validate dimensions, image formats, values, missing assets, and overlapping
  single-pixel placements before export.
- Reuse game decoding/rendering rules for preview where practical.

### M3 acceptance

- Author and play a new normal level on Windows, Linux, and macOS.
- Rebuild sample source projects into behaviorally equivalent levels.
- Project save/reopen/export preserves every understood value.
- The compiler/editor never silently emits a malformed or partially understood
  `.lev`.

---

## M4 — Network Simulation Foundation  [P0]

This milestone changes no visible multiplayer behavior. It proves a match can
be driven and observed through stable, portable boundaries.

### M4.1 — Authoritative match-state inventory

- Inventory players, pools/order, RNG, terrain, fluids, timers, Events, rules,
  level progression, callbacks, scoring, and every gameplay-affecting global.
- Exclude renderer, audio channels, menus, SDL objects, local cameras, and text.
- Give guest callback addresses stable serialized identities.

### M4.2 — Command-frame input

- Convert one player's physical input into a compact versioned command per tick.
- Feed human, AI, replay, and future network commands through the same boundary.
- Preserve original command application, update, and RNG order.

### M4.3 — Snapshots and checksums

- Serialize authoritative state without pointers or compiler padding.
- Add versioned snapshot headers and deterministic per-tick checksums.
- Prove uninterrupted and save/restore runs remain identical.

### M4.4 — Deterministic replay harness

- Record initial snapshot plus command frames.
- Replay without live input and compare checksums at every tick.
- Find the first divergence before adding transport.
- Compare restore/replay behavior across x86, x64, and ARM64.

Determinism is a diagnostic and recovery tool. LAN v1 uses an authoritative
host, so cross-architecture peer lockstep is not required.

---

## M5 — Direct-IP LAN Multiplayer  [P0/P1]

### Fixed v1 product scope

- One player per computer; no split screen in network matches.
- Listen-server host plus up to three clients: four humans total.
- Direct `IP:port` join only.
- Two available teams; every player chooses one before readying.
- Host owns the level list and all gameplay/Event rules.
- Each machine contributes its Player 1/local profile and ship. Team choice is
  session-specific. The host validates the ship but does not replace it.
- Host can start only when all connected players are ready.
- No AI, discovery, public lobbies, accounts, matchmaking, dedicated server,
  NAT traversal, relay, spectators, reconnect, mid-match join, or asset transfer.

### M5.1 — Protocol and compatibility handshake  [P0]

- Select and document the transport/reliability library before implementation.
- Version every message; enforce sizes, bounds, sequences, ticks, and timeouts.
- Exchange protocol/build version plus selected level, ship, and
  gameplay-critical asset hashes.
- Require matching local content; do not download mods or levels.
- Reject mismatches, invalid profiles/ships/teams, AI-enabled sessions, full
  sessions, and malformed packets with explicit reasons.

### M5.2 — Host/join session UI  [P1]

```text
Main menu -> LAN Multiplayer -> Host / Join

Host: choose rules/levels -> open session -> choose team -> ready -> Start
Client: enter IP:port -> compatibility check -> send profile/ship
        -> choose team -> ready
```

- Roster shows connection, team, ship, and ready state.
- Only the host changes rules/levels or starts the match.
- Port and recent direct address may be persisted locally; no server browser.

### M5.3 — Authoritative match transport  [P0]

- Clients send command frames, never trusted gameplay outcomes.
- Host sends canonical roster/rules, initial state, authoritative updates,
  terrain changes, results, next-level state, and periodic checksums.
- Begin with conservative LAN input delay and snapshot correction.
- Handle clean quit, refusal, timeout, host loss, client loss, and return to the
  session screen between host-selected levels.

### M5 acceptance

- Windows, Linux, and macOS clients interoperate by direct IP.
- Two-, three-, and four-player sessions complete multiple levels.
- Every peer agrees on terrain, deaths, frags, winners, and level progression.
- Invalid clients cannot change rules, start, enable AI, or choose a third team.
- A 30-minute mixed-architecture LAN soak has no drift, leak, hang, or stale roster.

---

## Ongoing Technical Debt

These items are real but must not delay the ordered product milestones unless
they block one directly.

### High-value investigations  [P1]

- Recover tile/property-table semantics before introducing `TileType`,
  `IsSolid`, `IsWalkable`, or `IsFluid` helpers. Existing guessed values are not
  authoritative.
- Complete AI source extraction only after replay/checksums can prove that code
  movement preserved behavior.
- Finish particle-system boundaries without creating one generic spawn helper
  that destroys subtype-specific initialization or RNG ordering.
- Continue semantic `DAT_`/`FUN_` renaming by understood subsystem, retaining
  original addresses in comments.

### Safe cleanup  [P2]

- Introduce logical `GameAction` bindings above saved physical scan codes.
- Centralize math tables and pure vector helpers in a non-conflicting game-math
  module.
- Abstract the `all3.gfx` sprite atlas after all access widths are verified.
- Audit/remove truly unused globals and declarations with linker evidence.
- Determine whether reversed intro splash order is intentional; fix or document.
- Add focused parity harnesses only when a concrete regression needs internal
  traces. Do not restore a permanent second executable.

### Optional presentation work  [P3]

- A GPU presentation/post-processing backend may improve scaling and effects,
  but the verified RGB565 software renderer remains authoritative.

---

## Later Possibilities

- LAN discovery and saved recent servers
- Reconnect and mid-match join
- Local prediction and bounded rollback
- Internet play, relay/NAT traversal, or hosted lobbies
- AI in network matches
- Dedicated/headless server
- More than one local player per network client
- Additional translations and community translation tooling
- Browser/WebAssembly port

## Binary-Parity Workflow

Authority order:

1. original executable/runtime behavior;
2. original assembly, debugger, and memory traces;
3. Ghidra control flow and decompilation;
4. reconstructed source and comments.

For a discrepancy:

1. reproduce the same controlled scenario in both versions;
2. locate the first observable or state divergence;
3. inspect the original routine's assembly;
4. trace inputs, outputs, globals, offsets, RNG, timing, and update order;
5. change the decomp from that evidence;
6. rebuild and compare again;
7. obtain hands-on runtime acceptance before calling it fixed.

Optional entity tracing is available with `TOU_ENTITY_TRACE=1`; its untracked
`entity-trace.csv` output is diagnostic only and never belongs in releases.

Modern settings, localization, editor, display, and network UI may intentionally
differ. Original gameplay behavior may not.
