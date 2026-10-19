# Advanced netcode plan

Status: Stages 1-2, Stage 3's event journal and Stage 4's local reconciliation,
pacing/authority prototypes and an isolated two-process exchange are implemented,
with checks below;
real presentation, live prediction, remote and cross-platform acceptance remain
open. Work is on `advanced-netcode`, based on stable v0.11 (`0909331`). No live
network rollback or transport replacement is enabled yet.
Main keeps the existing TCP delayed-lockstep implementation until acceptance.

## Goal and boundaries

Make remote matches responsive under moderate latency and jitter without
changing original gameplay semantics or excluding low-power ports. An 80 ms
round-trip connection is the first acceptance target, not a proven capability.
Keep a selectable conservative lockstep mode if rollback proves too expensive.
Do not silently change networking mode during a match.

This work does not include matchmaking, NAT traversal, relay services, host
migration, reconnect, spectators or mid-match joining. Direct IP and VPN play
remain the connection model. Keep the existing network-player limit initially.

## Stable v0.11 baseline findings that shape the design

- `source/game/netplay.cpp` uses protocol 4, a fixed three-tick input lead,
  host command bundles, tick-zero snapshots and periodic checksum correction.
  Its ping/pong traffic is currently keepalive-only.
- `source/game/gameloop.cpp` mixes wall-clock pacing, input/network preparation,
  ordered simulation calls and completion hooks. Its alternate early-return
  path must remain equivalent when extracting a replayable tick.
- `source/game/simulation_state.cpp` already captures RNG, pools, terrain and
  simulation counters. However, capture/restore/capture equality only checks
  serialization symmetry; it does not establish future replay determinism.
- Snapshots copy at least three bytes per terrain cell plus pools and other
  state. Per-tick full snapshots may be expensive. Measure before selecting
  full copies, periodic checkpoints or terrain deltas.
- Restore currently clears cosmetic particle tags. Frequent rollback would
  therefore need a different presentation reconciliation policy.
- Existing raw record serialization needs an architecture/pointer/padding
  audit before making stronger cross-platform compatibility claims.

## Proposed architecture, subject to the feasibility gates

Use host-authoritative bounded rollback: each peer applies its scheduled local
input and predicts missing remote inputs. The host resolves the canonical
input timeline; clients reconcile against confirmed bundles. The host must
also be able to rewind for accepted late client inputs, otherwise host-side
delay would remain hidden rather than solved.

Separate four owners:

1. Transport: sockets, bounded byte queues, connection events and timestamps.
   No gameplay access from a worker.
2. Session/input timeline: player slots, level generation, tick identifiers,
   confirmed/predicted inputs, acknowledgements and authority.
3. Simulation: one deterministic tick using supplied inputs, capture/restore
   and checksums, with no wall-clock wait, OS input sampling or socket I/O.
4. Presentation: rendering and reconcilable sound/VFX events. Never send GPU
   state or framebuffer contents as authoritative simulation data.

Start prediction by holding the last known input, with a hard prediction
window. Specify handling of action edges such as detonation rather than
inventing additional presses. Inputs for confirmed ticks are immutable;
duplicates are idempotent, conflicting duplicates are protocol errors. Late
inputs beyond the window cannot rewrite finalized scores or level changes.
The local late-input/resync policy is prototyped below; live transport and
acknowledgement policy remain design gates.

## Staged implementation and exit gates

### 1. Measure the existing baseline

Add timestamped RTT probes, smoothed RTT, variation, input/bundle arrival age,
stall duration and queue diagnostics. Call RTT variation an estimate, not
one-way latency. Add host-selected input delay negotiated before tick zero;
keep three ticks as the initial default and do not adjust it live yet.
Version the protocol for changed wire semantics and reject incompatible peers
clearly. Preserve existing lobby, team, focus, ESC and level-barrier behavior.

Exit: repeatable two-instance and real remote match evidence with recorded
latency/stall data; no gameplay changes. Measurements must reveal the current
host/client pacing asymmetry, not just display a ping number.

### 2. Prove local rewind and replay

Extract the exact simulation tick without changing call order, RNG order or
the alternate completion path. Audit derived caches, transient flags, global
state, audio ownership and wall-clock dependencies against snapshot coverage.
Use recorded inputs to run N ticks, restore, replay, then compare canonical
state and RNG after every tick. Include terrain destruction, fire, fluids,
turrets, deaths, pickups and round-end boundaries.

Measure snapshot bytes, capture/restore time, normal tick time and worst-case
replay bursts on representative large/busy levels. Choose history length and
memory/CPU caps from evidence; do not allocate an unbounded snapshot ring.
Use temporary diagnostic runs, not a new permanent CI test suite.

Exit: matching replay state plus a documented resource budget. If this fails,
fix state ownership or keep improved lockstep; do not enable network rollback.

### 3. Reconcile presentation and irreversible events

Assign stable tick/event identities to one-shot sounds and effects so replay
does not duplicate them. Rebuild or restore cosmetic tags without repeatedly
blanking glow; preserve looping audio, cameras, LOS and HUD ownership. A mute
flag alone is insufficient if sound bookkeeping influences later simulation.
Finalize scores, level unload/load and external writes only past the confirmed
frontier. Define pause/end/skip as synchronized control barriers.

Exit: forced local rollbacks produce no duplicate sound, disappearing VFX,
double scores or speculative level transitions.

### 4. Add bounded network prediction

Introduce explicit simulated, confirmed and oldest-restorable ticks; retain
input history and version canonical bundle revisions. Define clock pacing,
host late-input acceptance and client correction before wiring the loop.
Replace per-bundle RNG forcing with verified rollback-compatible RNG ownership;
do not conceal divergent simulation by overwriting its RNG every tick.
Cap prediction and resimulation work per frame. At the cap, wait visibly rather
than predicting forever. Resync uses an authoritative confirmed checkpoint
plus inputs, never an arbitrary speculative snapshot. Validate sizes, tick
windows and level generations before mutation; malformed restore must not
leave half-applied gameplay state.

Exit: identical confirmed timelines under late, duplicate and reordered input,
with clean disconnect, timeout, cancel/rejoin and round transitions.

### 5. Isolate I/O and evaluate transport

Once ownership boundaries are clear, move socket work to a bounded worker with
explicit shutdown, backpressure and copied events. Game state stays on the
game thread. Measure stalls again before deciding whether to replace TCP.

If ENet is warranted, prototype separate control/snapshot and input delivery
lanes. Define input redundancy, acknowledgements and sequencing before choosing
unreliable delivery. Preserve snapshot/control dependencies across channels.
Pin and license any dependency. A library adoption is not an acceptance result.

### 6. Human acceptance before merging

Compare stable lockstep and experimental rollback under controlled RTTs of
0/40/80/150 ms, jitter spikes, 1-5% loss and short outages. Record whether
figures describe round-trip or one-way delay. Start on one PC, then two physical
machines and a real VPN route. Cover Windows/Linux ARM64/macOS combinations
where available; label untested targets explicitly.

Exercise busy terrain/fluid scenes, multiple peers, simultaneous deaths,
pause/resume, focus changes, screenshots, level skip/end, cancellation and
disconnect. Record input feel, correction frequency, stall time, replay CPU
and history memory. Neither a successful build nor a same-PC match proves
internet or cross-architecture acceptance.

## Current implementation slice and evidence

Stage 1 introduced protocol 5 with timestamp-echo RTT probes, smoothed RTT
variation, local receive-age/queue/polling diagnostics and missing-input wait
statistics. The host selects a session-only 1-12 tick lead in the lobby, default
3; it is announced to clients and frozen in the start payload. Translations
cover all four shipped languages. Stable protocol 4 peers are incompatible.

Local Windows x64 checks on 2026-10-19:

- MSVC Release build, localization validation and whitespace checks pass.
- Two fresh staged instances negotiated a seven-tick lead and progressed.
- A temporary TCP proxy added 40 ms each way for 12 seconds, then alternated
  20/60 ms per direction for another 12 seconds while preserving stream order.
  It observed input tick 1482, bundle tick 1478 and checksum tick 1440, with no
  state-correction packet. Echo samples reflected the added delay and variation;
  wait diagnostics exposed host/client differences. This was idle-input play,
  not a combat/load, packet-loss or real internet acceptance test.
- The run exposed a pre-existing snapshot rejection when troopers are disabled:
  a zero-count waypoint array is intentionally unallocated. Capture/restore now
  permit that empty array, while rejecting non-empty data without a buffer.
  The resulting 3,634,977-byte initial snapshot round-tripped successfully.
- One-off socket probes confirmed rejection of protocol 4 packets and a
  negotiated zero-tick delay. No permanent test suite was introduced.

### Local rewind prototype (Stage 2)

`Gameplay_AdvanceSimulation` now advances one tick without pacing or network/
replay completion hooks, with supplied actions injected at the original input
call site. Normal play retains its existing catch-up loop, batch flag resets,
subsystem/RNG order and alternate trooper return. The diagnostic runs the full
session setup without its settings save/reload cycle. See
[rewind diagnostics](REPLAY_DIAGNOSTICS.md#local-rewind-feasibility-experiment).

The experiment exposed missing collision/visibility grids, fluid/edge scheduler
phases, and stale fields inherited from inactive entity slots. Snapshot version
4 includes the grids/timers and fixed pool storage, plus permits empty optional
turret data. This slice advanced the fingerprint to `tou-sim1-net5-snapshot4`
(superseded by Stage 3 below); older branch
snapshots/replays are incompatible too. No original simulation behavior was
changed to hide these mismatches.
An extraction check preserved the ordered 45 core call sites; only the
network/replay completion hooks moved to the outer pacing wrapper.

Representative Windows x64 runs after these fixes:

| Scenario | Verified ticks | Observed coverage |
| --- | ---: | --- |
| Minibase, 2 scripted humans | 1200 | Turning/firing, entities, deployed object, terrain mutation |
| Woods, 64 players, batch 9 | 1200 | 2266 entities, 250 fire particles, 5000 fluid sources, 111 deaths, 22055 changed tiles |
| Jungle, troopers on, batch 9 | 1200 | 150 alternate tick returns, 1320 fluid sources, 6113 changed tiles |
| Happyland, 12-tick history | 1200 | Generated terrain and scripted input |
| Minibase, forced end countdown | 40 | Terminal substate reproduced before any level unload |

World initialization may be time-seeded; figures are observations of individual
runs, not fixed golden expectations. Comparison includes every serialized byte
and RNG state/call count after every replayed tick. Sound and rendering were
disabled; this is not presentation or physical-input acceptance. A subsequent
two-instance TCP smoke advanced beyond tick 600 without checksum correction.

In the representative Woods run, the eight-tick history used about 48.0 MiB of
snapshot vector capacity; largest serialized state was 4,586,417 bytes. Actual
replayed simulation cost averaged 8.36 ms/window and peaked at 11.19 ms; restore
averaged 4.10 ms and peaked at 5.13 ms. Diagnostic windows including per-tick
capture/comparison averaged 24.95 ms, peaking at 30.61 ms. These desktop numbers
are not Raspberry Pi budgets. Full-copy history is viable for experiments but
not accepted as the final memory/CPU strategy; investigate checkpoint reuse,
optional restore hashing and terrain deltas before enabling expensive windows.

### Event journal and canonical LAN ticks (Stage 3 prototype)

The bounded [presentation timeline](PRESENTATION_TIMELINE.md) now buffers sound,
mix changes and shader events by tick/ordinal, replacing them on resimulation
and dispatching once on confirmation. Local checkpoints restore sound state
and pool-indexed cosmetic tags without clearing committed effect rings.
Physical channel generations protect sounds from stale virtual ownership.
Only the developer diagnostic enables the timeline, using a dry-run sink;
ordinary play still emits sound/VFX immediately.

Experimental LAN resets transient flags per successfully prepared simulation
tick, independent of catch-up grouping. Local play keeps legacy batch behavior.
This intentional network change advances the identity to
`tou-sim2-net5-snapshot4` (superseded below); packet shapes remained protocol 5. Earlier branch
builds are rejected by fingerprint.

Sound-enabled checks matched state/RNG, event digests, sound timers/virtual
references and particle tags. One 1200-tick 64-player Woods run with 9-versus-1
grouping confirmed 8794 sounds and 108 explosions once to the dry-run sink.
Jungle with troopers and 9-versus-2 grouping passed 600 ticks/75 alternate
returns; the terminal countdown matched for 40 ticks. A one-off ignored probe
exercised all dispatcher kinds, duplicate/stale confirmation, finalized-tick
rejection, queue limits and physical-track generation changes with stub sinks.
No CI test suite was added. A two-instance live TCP smoke advanced past tick
600 without checksum correction under the new reset policy.

### Input frontiers and transition barriers (Stage 4 foundation)

`InputTimeline` is a bounded 512-frame, four-player ledger with explicit
simulated, confirmed and caller-owned oldest-restorable state ticks. Live LAN
uses it with prediction set to zero. The pure model also supports holding the
last input up to a maximum 12-tick outstanding window, marking the earliest
changed predicted tick dirty, and replaying after the caller restores the world
and local presentation at dirty tick minus one. Prediction is not enabled on
the network. Matching late inputs may advance confirmation without replay;
changed used inputs cannot confirm until replay finishes. Duplicate inputs are
idempotent; conflicting duplicates and invalid/out-of-window inputs fail closed.
Old generations and expired input history do not mutate the current timeline.
Tick wrap is not supported as a continuing session.

Protocol 6 prefixes input, bundle and checksum packets with level generation.
Pause/resume/skip/end controls carry generation plus effective tick. A client
waits for that confirmed boundary; an already-passed control fails closed rather
than changing the past without rollback. Queues are bounded to 16 controls.
Pause/resume never overwrites simulation score counters. Natural completion is
derived independently from the same simulation ticks, not broadcast as a state
override that could bypass client scoring. Initial synchronization runs in the
preview too, avoiding a confirmed-only preview-control deadlock. No control is
broadcast after leaving gameplay for level teardown.

Outer score/level-slot updates and round-complete level teardown now require
both confirmed input and, when enabled, confirmed presentation. Snapshot 5 adds
the two live counters per team read by round finalization (32 bytes total);
rebuilt spatial bins remain excluded. Current identity is
`tou-sim2-net6-snapshot5`. Earlier branch peers and snapshots are incompatible.

Local Windows x64 evidence for this slice:

- MSVC Release, all localization overlays and whitespace checks pass.
- An ignored one-off probe linked the real input ledger and checked strict
  lockstep, 1999 ticks with ring wrap, the 12-tick limit, changed and matching
  late input, replay, duplicates/conflicts, retention and generation reset.
- Two staged live instances advanced past tick 600 with equal simulated and
  confirmed frontiers and no checksum correction.
- A real TCP relay injected a future pause at tick 104 and resumed that same
  tick after a short hold. The client log confirmed exact-tick application,
  ignored a wrong-generation control, and advanced to tick 594 without a
  correction. Wrong-generation input/checksum packets were also ignored and
  identical input duplicates accepted; a separate run disconnected a client
  sending a conflicting duplicate. This was idle-input synthetic control testing, not host keyboard
  or real internet acceptance.
- The forced 40-tick terminal rewind with 12-tick history and 9-versus-1 tick
  grouping passed snapshot/RNG/event comparison. The transition guard was
  closed before event confirmation and open afterwards. It did not execute
  the outer scoring update or unload/load itself.
- A 600-tick, 64-player Woods rewind passed with 12-tick history and 9-versus-1
  grouping under snapshot 5, observing fire, 5000 fluid sources, 42 deaths and
  15389 changed tiles. History vector capacity peaked at about 67.3 MiB;
  replay simulation averaged 13.30 ms/window (16.26 ms maximum), excluding
  restore/capture/comparison. These remain desktop, time-seeded observations,
  not low-power budgets or real output acceptance.

### Checkpoint-owned local reconciliation (Stage 4 prototype)

The [input reconciliation owner](INPUT_RECONCILIATION.md) now connects the
ledger to actual world restore/replay plus matching local presentation state.
It bounds the history/work window, fails closed on invalid input/checkpoint
state, replaces unconfirmed events, and handles a corrected terminal tick
without confirming an unused speculative tail. It is not created by live LAN.

The new `--rewind-late-input 1` mode compares late/reversed/duplicated remote
input against an all-inputs-known oracle at every corrected tick, including
world/RNG, local presentation metadata, event digests and alternate-return
behavior. Windows x64 runs passed Minibase, Woods, Jungle with troopers,
generated Happyland, window sizes 1/8/12 and the forced ending countdown.
Details and precise limitations are recorded in the linked guide.

### Rolling arrivals and bounded frame work (Stage 4 pacing prototype)

The checkpoint owner can now replay incrementally, deferring arrivals in a
bounded/deduplicated queue until the current replay boundary. A shared frame
pump admits at most the chosen replay-plus-forward tick budget and one restore
per call, with bounded elapsed clock debt. Confirmation and speculative output
remain blocked while dirty or mid-replay. Tick admission is bounded, not the
wall-clock cost of an individual restore/tick.

The rolling diagnostic (`--rewind-late-input 2`) compares confirmed prefixes
under partial/reordered arrivals, longer-than-window delays, duplicates and
0-80 ms synthetic frame intervals. Woods/Jungle/Happyland and the forced ending
passed with 1/3/12 work budgets. The one-tick Jungle run handled 481 deferred
arrivals across 326 sliced replay frames without exceeding one tick per frame.
Full measurements, stub-probe failure checks and synthetic-schedule limitations
are in [input reconciliation](INPUT_RECONCILIATION.md).

### Input authority and confirmed resync (Stage 4 local prototype)

The [authority records](INPUT_AUTHORITY.md) distinguish provisional canonical
commands from immutable sealed inputs, with per-tick revisions and a separate
resync epoch. Revision-fed clients cannot mix direct receipts into their ledger.
The host publisher exports actually used commands; confirmed snapshot export
uses the retained committed anchor rather than the speculative current world.
Resync preflights scope, size, integrity and snapshot structure before mutation,
then resets input/presentation history at a newer epoch. External same-tick
score/control changes block export/import rather than being silently undone.
These experimental record codecs have no live LAN message types yet.

The authority diagnostic (`--rewind-late-input 3`) passed corrected tick-by-tick
world/RNG/presentation equality, stale revision/epoch checks, repeated confirmed
imports and continuation across Woods, Jungle, Happyland and a forced ending.
A stub-world probe additionally covers host confirmed-anchor selection,
coalesced arrivals during replay and sealed-history conflicts. Exact evidence
and trust/validation limits are in the linked guide; these are not live peer
or authentication tests.

### Bounded two-process exchange (Stage 4 transport prototype)

The [per-peer exchange](NETCODE_EXCHANGE.md) separates received/stored acks from
committed-prefix hash progress, retains bounded outstanding records, and freezes
normal input/simulation through Offer/Ready/Commit/Applied/Resume resync phases.
An isolated loopback TCP driver connects two real simulation owners with
scripted inputs and a shared work budget; normal LAN remains unchanged.

Minibase, Jungle with troopers and generated Happyland passed committed-hash
agreement, initial/periodic resync and acknowledged finish. A 20/60 ms per-way
stream relay injected a checksum mismatch: both Woods processes recovered and
finished tick 240 in epoch 4. Invalid framing and withheld handshake replies
aborted both processes rather than resuming unsafely. Bounds, phase semantics,
one-off probes and explicit limitations are recorded in the linked guide.

Still required: real audible/GPU delivery, weapon-specific producers, live
mode/fingerprint/clock/input/liveness integration, multi-peer barriers, full
host-controlled pause/skip/end and multi-level/lobby acceptance,
remote/cross-platform checks, and low-power budgets. No live
prediction, rollback toggle or ENet migration is enabled. Keep main stable.

See [transport review](NETPLAY_TRANSPORT_REVIEW.md) and [LAN beta](LAN_BETA.md).
