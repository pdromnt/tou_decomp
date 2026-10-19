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

## Local rewind feasibility experiment

On `advanced-netcode`, run a bounded developer diagnostic without entering a
menu or joining a network session:

```powershell
.\TOU.exe --verify-rewind minibase --rewind-ticks 1200 --rewind-window 8
.\TOU.exe --verify-rewind woods --rewind-players 64 --rewind-batch 9
.\TOU.exe --verify-rewind jungle --rewind-troopers 1 --rewind-batch 9
.\TOU.exe --verify-rewind minibase --rewind-round-end 1
```

Use an exact catalog name (for example `happyland` for a generated level).
Unknown names print the catalog to `debug.txt` and return an error. Linux/macOS
use `./TOU`. Diagnostic logging is enabled automatically. Session configuration
is applied in memory without the normal save/reload cycle; it does not persist
the diagnostic's rules/player count. No permanent test suite or CI job is added.

Options: ticks 1-100000 (default 600), rewind window 1-12 (default 8), players
2-64 (default 2: first two scripted humans, remaining players AI), troopers 0/1
(default 0), catch-up batch 1-9 (default 1), and round-end 0/1 (default 0).
Commands exercise turning, thrust, brake, primary/secondary fire and detonation
without sampling devices or drawing from gameplay RNG. World initialization
can still use a time-based seed; each experiment records its own checkpoint,
not a cross-launch golden world.

Each window captures a checkpoint, records supplied actions/batch resets and
the full expected state after each tick, restores, then resimulates and checks
byte equality after every tick. The original alternate trooper return is
preserved. A terminal simulation substate ends the experiment before loading
another level; the forced round-end option exercises the real 40-tick countdown.

The history budget is 256 MiB of snapshot vector capacity, including the
comparison buffer, not a process-RAM limit. Oversized terrain is rejected early.
Reports include serialized snapshot size, allocated history-buffer peak,
capture/restore/forward/replay mean/max timings, and replay-window CPU cost.
The separately named `burst_with_comparison` also includes per-tick captures
and byte comparisons: it is not the expected cost of a production rollback.
Pool/death/terrain counters describe actual coverage; a passing run with zero
fire or fluid activity is not evidence that those subsystems were exercised.

Exit codes: 0 verified (possibly ending early at a terminal state), 2 invalid
options/initialization, 3 snapshot failure, 4 history budget exceeded, 5 divergent
state or alternate-return behavior. Failure logs the first differing offset.

This diagnostic disables audio and presentation; it does not make duplicate
sound/glow safe, validate real network prediction, or prove cross-platform
determinism. The legacy reset runs once per catch-up batch, so matching batches
are part of the recorded experiment. A network-wide batching policy remains
to be designed before prediction can be enabled.
