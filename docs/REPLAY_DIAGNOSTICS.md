# Replay and Simulation Diagnostics

The replay harness exists to catch behavior-changing refactors and network
desynchronization. It records the initial authoritative snapshot, logical
player actions, and state checksums; it is not a user-facing demo format.

Record a match:

```powershell
.\TOU.exe --record-replay test.tourpl --logging
```

Start and play a normal match, then close the game. Replay it with:

```powershell
.\TOU.exe --replay test.tourpl --logging
```

Choose Team Deathmatch and press Start. The stored settings and initial state
take over when simulation begins. Live gameplay input is ignored. Playback
pauses and logs the first mismatching tick, or pauses after the final verified
tick. Replay files are tied to the current snapshot/config layout and are not a
stable release format.

The snapshot excludes SDL objects, audio channels, menus, text, and local
viewport geometry. It includes RNG state/call count, players, all mutable
gameplay pools, turrets, AI waypoints, map edges, entity tracking, schedulers,
scoring, fluids, destructible terrain, and the tile map. Payload size, level
dimensions, pool counts, magic, and version are validated before restore. A
capture -> restore -> capture byte check also guards every recorded initial
state. See `SIMULATION_STATE.md` for the complete boundary.
