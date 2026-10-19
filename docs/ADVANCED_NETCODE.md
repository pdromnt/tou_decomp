# Advanced netcode plan

Status: Stage 1 implemented, with local checks below; remote acceptance and
Stage 2 feasibility remain open. Work is on `advanced-netcode`, based on stable
v0.11 (`0909331`). No rollback or transport replacement is implemented yet.
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
The exact late-input/resync policy is a design gate, not yet decided.

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

Stage 1 now uses protocol 5 with timestamp-echo RTT probes, smoothed RTT
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

Still required: visual lobby acceptance, real remote measurements, other
platforms and the wider session regression matrix. Stage 2's local feasibility
experiment can proceed independently, but these checks are not a claim that
network rollback is ready. No ENet dependency, gameplay prediction or rollback
toggle until the relevant gates pass. Keep each slice in a separate reviewable
commit; main remains the stable release line.

See [transport review](NETPLAY_TRANSPORT_REVIEW.md) and [LAN beta](LAN_BETA.md).
