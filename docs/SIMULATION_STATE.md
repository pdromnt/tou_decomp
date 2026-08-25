# Authoritative Simulation State

`simulation_state.cpp` defines the portable boundary used by replay diagnostics
and LAN state correction. Snapshot version 3 is a little-endian, fixed-width
format. All supported targets are little-endian and the serialized runtime
records have compile-time size and offset assertions in `types.h`.

## Included state

- simulation tick and original RNG state/call count;
- level dimensions, RGB565 terrain, and the terrain tile map;
- gameplay portions of every active player record;
- active entity, trooper/car, deployed-object, animated-particle, fire, debris,
  explosion, fluid-source, and wall-effect pools, including their order;
- mutable turrets, AI navigation waypoints, map edges, spawn points, ambient
  emitters, and static explosion/entity records;
- entity category tracking links and their twelve cursors;
- sub-frame, victory, activation, bombing, visibility, turret, AI waypoint, and
  spawn/emitter scheduler counters;
- team wins plus per-team and per-player match/award statistics.

Counts are validated against the original allocations before either capture or
restore. The level must already be loaded with identical dimensions and enough
storage; assets and resource allocation are deliberately outside the snapshot.

## Excluded state

- SDL windows, renderer objects, sockets, audio channels, and wall-clock time;
- menus, localization, strings, and input-device state;
- physical key bindings, local human/camera ownership, viewport geometry, and
  presentation-only player sound state;
- immutable lookup/configuration tables loaded from matching game assets.

LAN peers separately verify the simulation build ID, ordered level bytes, GG
theme bytes, ship files, and gameplay data before a match can start.

## Restore invariant

Replay recording and the host's tick-zero LAN barrier run an immediate
capture -> restore -> capture check. Byte inequality rejects the snapshot before
the match proceeds. Periodic checksums then detect drift; a divergent client is
restored from a current host snapshot rather than leaving the session paused.

Replay files remain diagnostics tied to the current snapshot version. They are
not a stable save-game format.
