# Tunnels of Underworld Development Plan

## Goal

Preserve the accepted behavior of the original game while turning the SDL
decomp into a maintainable, multilingual, cross-platform game with creation
tools and simple LAN multiplayer.

The project order is:

1. faithful game behavior;
2. portable desktop runtime;
3. maintainable settings and text;
4. native level-authoring pipeline;
5. deterministic/serializable simulation boundary;
6. constrained LAN multiplayer.

## Current Baseline — v0.5

- Original gameplay, weapons, effects, menus, AI, and match results have had a
  hands-on parity pass.
- SDL3 owns platform, input, display, and audio services.
- Windows x86/x64/ARM64, Linux x64/ARM64, and macOS Intel/Apple Silicon builds
  are produced by CI; runtime acceptance is recorded separately in
  `BACKLOG.md`.
- Runtime records, pool limits, config storage, RNG, callbacks, and the primary
  simulation boundaries are typed or isolated sufficiently for the next work.
- `.lev`, `.gfx`, and `.SHP` format notes live under `docs/`.

## Milestone 1 — Human-Readable Settings

Replace `options.cfg` with a versioned `settings.json` containing user-facing
settings only.

### Design

- Introduce a normal `UserSettings` model independent of the packed recovered
  `GameConfig` compatibility record.
- Map `UserSettings` into the legacy runtime fields at one boundary.
- Serialize named values, arrays, and stable identifiers; do not expose
  reserved/unknown bytes as JSON keys.
- Include `schemaVersion` and `language` from the first version.
- Load order: defaults, `settings.json`, validation/clamping, runtime mapping.
- One-time migration: if JSON is absent and `options.cfg` exists, load the old
  file, convert known settings, write JSON, and retain the old file as a backup.
- Save atomically through a temporary file and replacement.
- Unknown JSON keys are ignored for forward compatibility; malformed known
  values fall back individually instead of discarding the whole file.

### Gate

- Every visible option, player profile, key binding, ship/loadout choice,
  display mode, and audio value survives save/restart.
- A JSON round-trip produces the same runtime settings.
- Corrupt/truncated JSON safely falls back to defaults.
- macOS continues saving in the app resources location used by the current
  portable build.

## Milestone 2 — Localization

Initial locales:

- English (`en`), authoritative fallback
- Spanish (`es`)
- Brazilian Portuguese (`pt-BR`)
- Finnish (`fi`)

### Design

- Store UTF-8 catalogs under `lang/<locale>.json` with stable semantic keys.
- Replace user-facing literals with `Text_Get("key")`; keep logs, paths,
  binary identifiers, level author metadata, and player-entered text separate.
- Missing locale or key falls back to English and emits a debug-only warning.
- Extend the bitmap-font pipeline for all required Latin characters, including
  Finnish `ä/ö/å`, Spanish punctuation/accents, and Portuguese diacritics.
- Make menus measure localized strings instead of assuming English widths.
- Add Language to the options menu; changing it applies immediately and saves
  through `settings.json`.
- Keep gameplay/network protocol values language-neutral.

### Gate

- Every menu, HUD message, match-result label, award, error, control name,
  weapon name, and pickup message comes from a catalog.
- All four languages can complete the full menu-to-match-to-results flow
  without missing glyphs, clipping, or untranslated English except approved
  proper names.
- CI validates identical key sets and valid UTF-8/JSON for every catalog.

## Milestone 3 — Level Compiler and Editor

Build a supported replacement for the original `level converter.exe` /
MakeLev workflow, then put a visual editor on top of the same library.

### Phase A: format/compiler parity

- Treat original MakeLev output and shipped levels as the oracle.
- Finish recovery of every `.lev` section and every placement/config marker.
- Create a reusable `tou_level` library for parsing, validating, and writing
  levels without depending on the game runtime.
- Import the original source set: visual JPEG, attribute-map TGA, optional
  parallax JPEG, and documented text configuration.
- Compare compiler output structurally against original MakeLev fixtures and
  load every generated level in both the original game and decomp.
- Provide a command-line compiler first so the format is proven independently
  of a GUI.

### Phase B: editor MVP

- New/open/save project and export `.lev`.
- Visual layer plus attribute/collision layer with overlay and opacity control.
- Palette/tool picker replacing `COLPICK.EXE` for terrain attributes and
  placements.
- Place, select, move, configure, and delete spawn points, turrets, gates,
  repairs, mines, signs, water, and other recovered records.
- Edit level metadata, physics, water, civilians, bombing, ambience, parallax,
  and GG options.
- Validate dimensions, image formats, required files, unsupported values, and
  overlapping single-pixel placements before export.
- Preview using shared game decoding/rendering rules where practical.

### Gate

- A newly authored normal level exports and plays on all desktop targets.
- Existing sample source files rebuild into behaviorally equivalent levels.
- Opening and re-saving a supported editor project loses no known data.
- The editor cannot silently emit a malformed or partially understood `.lev`.

## Milestone 4 — Network Simulation Foundation

This milestone changes no visible multiplayer behavior. It proves that a match
can be driven and observed through stable boundaries.

- Define authoritative match state: players, pools and order, RNG, terrain,
  fluids, timers, Events, rules, level progression, and callback identities.
- Convert one local player's keys into a versioned command frame per tick.
- Route human, AI, replay, and eventually network commands through the same
  input boundary without changing original update/RNG order.
- Add versioned portable snapshots and deterministic checksums without native
  pointers or compiler padding.
- Record/replay matches locally and compare checksums per tick.
- Test snapshot restore and replay across x86, x64, and ARM64.

Determinism remains a diagnostic and recovery tool. LAN v1 uses an authoritative
host so cross-architecture lockstep is not a requirement.

## Milestone 5 — Direct-IP LAN Multiplayer

### Product scope

- One player per computer; no split screen in a network match.
- One host/listen server plus up to three joining clients: four humans total.
- Direct `IP:port` join only.
- Exactly two available teams; every player chooses either team before readying.
- No AI players, public lobby list, LAN discovery, matchmaking, accounts,
  dedicated server, NAT traversal, internet relay, spectators, mid-match join,
  or reconnect in the first version.
- Host selects the level list and all gameplay/Event rules.
- Each machine uses its Player 1/local profile and selected ship from its own
  Players settings. Team is chosen separately for the LAN session. The host
  validates the ship but does not replace it.
- Host starts only when every connected player is ready.

### Session flow

```text
Main menu -> LAN Multiplayer -> Host / Join

Host:
  choose rules and levels -> open session -> choose team -> ready

Client:
  enter IP:port -> compatibility check -> send local profile/ship
  -> choose team -> ready

Host presses Start -> authoritative match and host-owned level progression
```

### Technical shape

- Host-authoritative listen server; clients send command frames, not process
  memory or trusted gameplay outcomes.
- Versioned protocol with strict packet sizes, bounds validation, sequence
  numbers, tick numbers, timeouts, and explicit disconnect reasons.
- Handshake checks protocol/build compatibility plus hashes for selected levels,
  ship data, and gameplay-critical assets. LAN v1 requires matching local data;
  it does not transfer mods or levels.
- Host sends canonical rules, level order, roster, team assignments, initial
  snapshot, authoritative updates, terrain changes, and periodic checksums.
- Begin with conservative input delay and snapshot correction suitable for LAN.
  Local prediction and rollback are later improvements, not alpha blockers.
- Audio, menus, rendering, cameras, and localized strings never enter the
  authoritative state.

### Gate

- Windows, Linux, and macOS clients can join each other by direct IP.
- Two to four players can ready, play multiple host-selected levels, see the
  same terrain/results, and return to the session screen.
- Clients cannot start, change host rules, select a third team, enable AI, or
  submit invalid ship/team/input data.
- Clean behavior for host quit, client quit, timeout, version mismatch, asset
  mismatch, full session, and connection refusal.
- A 30-minute mixed-architecture LAN soak test finishes without state drift,
  runaway memory, or dead session state.

## Later Work

- LAN discovery and saved recent servers
- Reconnect and mid-match join
- Local prediction and bounded rollback
- Internet play, relay/NAT traversal, or hosted lobbies
- AI in network matches
- Dedicated/headless server
- More than one local player per network client
- Additional translations and community translation tooling

## Binary-Parity Contract

The original executable remains authoritative for gameplay. When behavior
differs, use this order:

1. original executable/runtime behavior;
2. original assembly, debugger, and memory traces;
3. Ghidra control flow and decompilation;
4. reconstructed source and comments.

For a discrepancy: reproduce both versions, find the first divergence, inspect
the original routine and state/RNG order, change the decomp from evidence,
rebuild, compare again, and obtain hands-on runtime acceptance.

Modern platform, settings, localization, editor, and network UI behavior may
intentionally differ. Physics, effects, timing, RNG ordering, collision,
terrain, scoring, and original single-machine gameplay must not drift.
