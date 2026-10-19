# Experimental Input Reconciliation

`source/game/simulation_reconciliation.cpp` connects the bounded input ledger
to real world restore/resimulation and local presentation checkpoints. It is
used only by the developer late-input diagnostic on `advanced-netcode`.
Live LAN remains TCP lockstep, with prediction set to zero. No menus, normal
local play, wire formats, or compatibility fingerprints change in this slice.

## Ownership and bounds

- One game-thread owner for one loaded level, starting at simulation tick zero.
  Starting while LAN, replay playback/recording, or another event journal is
  active is rejected. The caller must stop the owner before level teardown.
- Two to four players in the diagnostic; the ledger supports up to four.
  A configured window of 1-12 ticks bounds prediction and each replay burst.
- A ring of window-plus-one full world snapshots, paired with local sound/
  particle-tag checkpoints, per-tick event digests and alternate-return results.
  The confirmed checkpoint is the restore anchor; old storage is reused.
- The diagnostic gives the owner 128 MiB for retained snapshot/tag vector
  capacities and reserves another 128 MiB for its independent oracle. These
  limits do not represent total process memory; journal storage, inline records,
  allocator overhead, game assets and transient allocations are separate.
- Even completely known inputs cannot run beyond the uncommitted event window.
  `advance()` returns `Waiting` until the caller confirms, rather than silently
  overwriting a needed checkpoint or overflowing the event journal.

## Correction flow

1. `submit()` records first-arriving actual input; missing input holds the last
   used action. Matching late inputs can confirm without resimulation. A changed
   predicted action marks the earliest dirty tick.
2. `advance()` will not proceed while dirty. `reconcile()` restores the world
   and local presentation checkpoint immediately before that tick, discards
   the uncommitted event tail, and replays with corrected input.
3. Every replay tick uses canonical per-tick transient resets and the ordinary
   unpaced simulation entry. It does not force the RNG from incoming bundles,
   poll sockets, render, emit deferred effects, or load/unload levels.
4. If correction reaches a terminal state earlier than the speculative end,
   unused simulated frames/checkpoints/events are invalidated. Actual future
   inputs may remain recorded, but they are not simulated or confirmed beyond
   that ending. If correction removes an earlier predicted ending, the caller
   can advance again.
5. `confirm()` commits only through contiguous actual input that matches the
   corrected simulation. Repeated confirmation emits nothing twice. Final
   score/level work requires both input and presentation confirmation.

Wrong generations and expired history do not modify the timeline. Conflicting
duplicates, malformed/out-of-window input, snapshot/restore failure, state
misalignment, or history-budget failure latch the owner as failed. No further
advancement or confirmation is allowed. A failed operation may already have
advanced/restored the world: it is not a transactional resync. The caller must
stop and replace the session from a trusted checkpoint or abort it; it must
not keep playing from that partial state.

## Local evidence

`--verify-rewind LEVEL --rewind-late-input 1 --rewind-canonical 1` runs an
all-inputs-known oracle for each window, restores the starting state, then
withholds every remote input while simulating predictions. Remote inputs arrive
newest-tick/player first, with identical duplicates and wrong-generation packets.
After correction, every tick must match the oracle's serialized world/RNG,
local sound/particle metadata, event digest and alternate-return result.
No oracle state or commands are read by the reconciliation owner itself.
No effects may commit during prediction/replay; each corrected window is
confirmed twice to check idempotency.

Windows x64 observations (individual, time-seeded runs):

| Scenario | Verified ticks | Corrections / replayed ticks | Other coverage |
| --- | ---: | ---: | --- |
| Minibase, 2 players, window 8 | 600 | 34 / 178 | 41 matching-input windows |
| Woods, 4 players, window 12 | 600 | 49 / 418 | Full world and event equality |
| Jungle, 4 players, troopers, window 12 | 600 | 49 / 418 | 75 alternate simulation returns |
| Happyland, 2 players, window 1 | 600 | 45 / 45 | Generated terrain, 555 matching-input windows |
| Minibase, 4 players, forced countdown | 40 | 3 / 32 | Confirmed-only terminal guard |

The Woods owner-plus-oracle vector capacity peaked at 127,960,125 bytes
(about 122.0 MiB). Its restore/replay/checkpoint work averaged 6.51 ms per
window, peaking at 8.17 ms; this is desktop diagnostic work, not a CM4 budget.

A one-off ignored probe linked the real owner, ledger and event journal with
a stub world/sinks to force early-terminal tail discard, removal of a predicted
ending, repeated confirmation, known-input window saturation, reset/generation,
budget rejection, capture/restore failure, and conflicting duplicate rejection.
Those particular failure/ending cases are not full-game acceptance evidence.
No permanent test suite or CI job was added.

## Still gated

Real audible/GPU delivery, weapon-specific producers, repeated partial arrivals
and catch-up pacing, host/client authoritative input revisions, confirmed-only
resync checkpoints, full level/lobby transitions, remote/cross-platform checks,
and low-power memory/CPU budgets remain open. The current diagnostic delivers
one reversed late batch per window, not a realistic network jitter schedule.
Do not enable live prediction just because this local comparison passes.
