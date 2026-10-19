# Experimental Input Reconciliation

`source/game/simulation_reconciliation.cpp` connects the bounded input ledger
to real world restore/resimulation and local presentation checkpoints. It is
used for prediction only by developer late-input diagnostics on `advanced-netcode`.
The opt-in live confirmed-control adapter borrows a current lockstep anchor,
not physics ownership. Live LAN still has zero prediction. Normal local play
and default protocol 6 are unchanged; opt-in controls use separate protocol 7.

## Ownership and bounds

- One game-thread owner for one loaded level, starting at simulation tick zero.
  Starting a prediction owner while LAN, replay playback/recording, or another
  event journal is active is rejected. The caller stops it before level teardown.
  The explicit live control-only entry instead requires a confirmed current
  LAN anchor, cannot submit inputs/advance physics, owns no journal, preserves
  presentation on restore/stop and is detached before releasing scoring/load.
- Two to four players in the diagnostic; the ledger supports up to four.
  A configured window of 1-12 ticks bounds prediction and total replay length.
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
   the uncommitted event tail, and replays with corrected input. Its optional
   tick budget slices that work across calls; `Replaying` means the old frontier
   has not yet been reached. `checkpoint()` hides the stale suffix while rewound.
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

Arrivals during a sliced replay return `Deferred` and enter a fixed 128-input
queue. Wrong generations are ignored; identical queued duplicates take no extra
space, conflicting queued duplicates and overflow fail closed. At the replay
boundary the queue is checked against the actual ledger, so a conflict with an
already-recorded input can also fail there. A new correction starts on a later
call, never halfway through the current replay. Confirmation remains blocked
while replaying or dirty; no speculative output is emitted.

## Catch-up pump

`pump(elapsed_ms, max_ticks, target_tick)` shares a 1-12 tick work allowance
between replay and forward simulation. It performs at most one world restore
per call. Replay consumes work allowance but not forward clock debt; the caller
can poll/receive between slices. `confirm()` remains an explicit caller action.
The default target is unrestricted; the diagnostic uses an upper tick limit
to avoid crossing its current oracle horizon.

Clock debt uses the existing 16 ms simulation period. A single elapsed sample
is capped at 250 ms and accumulated debt at the prediction-window duration.
Excess elapsed time is dropped and reported as clamped; a missing-input stall
cannot demand unlimited catch-up work. `stop()` clears debt, replay and queued
inputs. Tick work is bounded, **not hard wall-clock execution time**: one world
restore or simulation tick may still be expensive. Repeated direct calls to
`advance()`/`reconcile()` bypass the shared frame allowance, so a future live
driver must use one pump call per presentation frame, and retain a completed
display frame instead of rendering a partially rewound world.

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

### Rolling arrivals and sliced-work evidence

Mode `--rewind-late-input 2` delivers partial inputs on a deterministic virtual
arrival schedule, including delays longer than the prediction window, reordered
ticks, duplicates and wrong generations. Frame intervals cycle through 0-80 ms
to exercise stalls and catch-up debt. `--rewind-frame-budget` selects 1-12 total
replay/forward ticks per pump call (default 3). Confirmed prefixes are compared
with the full world/local/event oracle before committing, including prefixes
that become known after a second correction. Confirmation is repeated to check
idempotency. No pump may emit output, exceed its shared tick allowance, restore
twice, or exceed the bounded forward debt.

Windows x64 individual observations:

| Scenario | Verified ticks | Frame budget / observed maximum | Sliced frames / deferred inputs |
| --- | ---: | ---: | ---: |
| Woods, 4 players, window 12 | 600 | 3 / 3 | 156 / 270 |
| Jungle, 4 players, troopers, window 12 | 600 | 1 / 1 | 326 / 481 |
| Happyland, 2 players, window 1 | 600 | 3 / 1 | 0 / 0 |
| Minibase, 4 players, forced countdown | 40 | 1 / 1 | 18 / 44 |
| Minibase, 2 players, window 8 | 1200 | 12 / 8 | 0 / 0 |

Jungle matched 75 alternate simulation returns. Its one-tick pump averaged
0.48 ms/call and peaked at 4.91 ms; the three-tick Woods pump averaged 0.70 ms
and peaked at 8.73 ms, including restore/capture but excluding oracle/comparison.
These are desktop observations, not low-power budgets or real frame-rate tests.
The earlier reversed-batch diagnostic and 64-player rewind still pass, as does
the two-instance TCP control relay (no checksum correction).

The ignored stub-world probe additionally forces arrivals during replay that
require a second earlier correction, a hidden stale suffix, shared forward/
replay work limits, one restore per frame, a 5-second hitch/debt cap, queue
overflow/conflict and reset while replaying. No permanent suite was introduced.

## Still gated

The owner also prepares [confirmed control anchors](NETCODE_EXCHANGE.md) for
pause-menu/resume/skip/end. Preparation requires a fully confirmed current
tick, scopes/restores the original control fields during capture, and leaves
the running world unchanged. The all-peer barrier applies the candidate under
a new epoch. This does not run outer scoring or level I/O, and irreversible
transitions must wait for the group's release as well as world confirmation.

The [input authority prototype](INPUT_AUTHORITY.md) now distinguishes provisional
and sealed host revisions and imports scoped confirmed checkpoints under a new
epoch. Prediction remains diagnostic-only; direct receipt and revision-fed owners cannot
mix input sources. Prediction-owner resync resets the event journal at the
imported base; live control-only anchors never start/stop that journal.

Real audible/GPU delivery, weapon-specific producers, live clock/authority
integration of the [isolated peer exchange](NETCODE_EXCHANGE.md), natural-win/repeated-match acceptance,
remote/cross-platform checks, and low-power
memory/CPU budgets remain open. The rolling schedule is synthetic and still
drains at each window's oracle horizon; it does not prove continuous internet
play, real packet loss, transport behavior or a render loop during resimulation.
Do not enable live prediction just because this local comparison passes.
