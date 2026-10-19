# Experimental input authority and confirmed resync

This is a local prototype on `advanced-netcode`, not a new live networking
mode. `source/game/input_authority.cpp` defines bounded canonical records and
their codecs; the reconciliation owner applies them only in `HostRevisions`
mode. Existing LAN remains protocol 6 TCP lockstep. Its packets, simulation
fingerprint and snapshot version are unchanged by this slice.

## Input ownership

- A direct-input owner receives immutable per-player actions, scoped by level
  generation and epoch. `canonical_input()` exports the actions actually used
  after replay, not a fresh sample or an unrelated prediction. It refuses export
  while dirty or mid-replay. Its seal flag requires contiguous known input.
- `InputAuthorityPublisher` versions a full tick's canonical actions. A change
  in actions or sealing state increments that tick's revision. Repeating the
  same publication reuses its revision. Its 512-slot ring cannot overwrite an
  unsealed tick; sealed history and revision counter wrap are protected.
- A revision-fed client cannot also submit direct receipts. Provisional host
  revisions replace predictions but never make input confirmed. A sealed
  revision supplies all actual actions for its tick; confirmation still waits
  for earlier holes and any required world replay.
- Lower revisions are stale and ignored; identical revisions are idempotent.
  Different data under the same revision is a conflict. A sealed tick cannot
  be changed or unsealed, even by a higher revision. Conflicting updates fail
  the owner before any further event confirmation.
- During sliced replay, queued revisions coalesce to the newest revision per
  tick within the existing 128-entry limit. Seals/conflicts are checked again
  when draining against the ledger. Raw receipts and revisions remain separate
  owner modes. No incoming record forces the gameplay RNG.

The experimental canonical input record is exactly 22 bytes, little-endian:
four u32 fields (generation, epoch, tick, revision), player count, seal byte,
and four action bytes. Inactive action slots must be zero, seal must be 0/1,
scope/revision must be nonzero, and action bits/player count are validated.
Decoding failure leaves the destination record unchanged. These bytes are not
yet assigned a message type in the live LAN protocol.

## Confirmed resync

`export_confirmed(new_epoch)` is restricted to a direct-input owner. It exports
the retained committed/confirmed checkpoint, which can be earlier than the
currently simulated world. It cannot export a speculative current snapshot.
The previous confirmed actions accompany the state so held-input prediction
continues from the correct base after import.

The experimental state envelope is a 37-byte header plus at most 64 MiB of
snapshot data: magic `TOUS`, envelope version 1, generation, new epoch, tick,
player count, four prior actions, payload length and an FNV-1a-64 payload hash.
It does not contain presentation checkpoints, physical device bindings, audio
handles, framebuffer/GPU data or sockets. Length and integrity are checked
before allocating the decoded payload. FNV is accidental-corruption detection,
**not authentication**; metadata and semantic record contents require a trusted
host connection and negotiated matching simulation/assets before use.

Import requires the current level generation, a strictly newer epoch, matching
player count, a tick no earlier than locally committed effects, and a bounded
forward jump. Snapshot magic/version, tick, counts, dimensions, allocation
capacities and exact body size are preflighted without mutation by
`SimulationState_Validate`. This is structural/context validation, not a complete
semantic sanitizer of every recovered runtime record.

The owner also compares its current world with its checkpoint before export or
import. External score, level-slot or control changes cannot be silently
discarded by restoring an older same-tick checkpoint. A future live driver must
checkpoint synchronized controls or stop at a session barrier before resync.

Only after preflight/allocation succeeds does import restore the world, reset
input history/debt/queued arrivals, and start the new epoch at the confirmed
tick. Owned audio loops stop; local audio bookkeeping and cosmetic tags are
rebuilt rather than copied from the remote machine. The event journal starts
already confirmed at the imported base, so old one-shots cannot be emitted
again. Its diagnostic event counters restart for that epoch. Old-epoch input
receipts and revisions are ignored, even if their tick is ahead of the base.

Malformed/stale imports are rejected without changing the world or poisoning
an otherwise healthy owner. An unexpected restore failure after successful
preflight fails the owner; there is no claim of a general transactional recovery
from allocator failure or external concurrent mutation. The owner is game-thread
only and refuses import into an already-failed session.

## Evidence and limits

`--verify-rewind LEVEL --rewind-late-input 3 --rewind-canonical 1` exercises the
real revision-fed client owner. A synthetic authority publishes provisional
commands and later seals corrected commands in reversed tick order. Records
round-trip through the real codecs. Every corrected tick must match the
all-inputs-known world/RNG, presentation metadata and event oracle. Periodic
resync envelopes use that oracle's confirmed checkpoint, reject a structurally
malformed copy with a recomputed checksum without world mutation, then import
and continue in a newer epoch. No host simulation or socket runs in this mode.

Windows x64 individual runs:

| Scenario | Verified ticks | Revisions | Confirmed resyncs | Extra coverage |
| --- | ---: | ---: | ---: | --- |
| Woods, 4 players, window 12 | 600 | 1200 | 12 | Corrected world/event equality |
| Jungle, 4 players, troopers, window 12 | 600 | 1200 | 12 | 75 alternate simulation returns |
| Happyland, 2 players, window 1 | 600 | 1200 | 150 | Generated terrain and epoch continuation |
| Minibase, 4 players, forced countdown | 40 | 80 | 0 | Sealed-only terminal confirmation |

History/oracle vector statistics exclude the temporary resync envelope/copy
buffers and extra preflight comparison allocation. They are not total process
memory or low-power budgets. Event counts after import describe the current
epoch, not all earlier committed output.

An ignored one-off probe links the real owner, publisher, ledger, journal and
codecs with a stub world. It checks a host exporting confirmed tick 1 while its
current world is speculative tick 6, epoch invalidation of old receipts,
coalescing mid-replay revisions, immutable sealed input, publisher ring wrap,
malformed records, and rejection after external round-state mutation. No
permanent test suite or CI job was added.

The [bounded peer exchange](NETCODE_EXCHANGE.md) now supplies receipt/revision
acks, committed hash progress and a coordinated resync handshake in an isolated two-to-four-process
TCP driver. It is not connected to normal LAN sessions. Live negotiation,
control/level barriers, input/clock/liveness integration, real audible/GPU delivery,
level/lobby transitions, remote/cross-platform checks and low-power costs remain
gated. No live prediction is enabled.
