# Experimental bounded peer exchange

`source/game/netcode_exchange.cpp` joins receipts, canonical revisions,
acknowledgements and confirmed resync into one per-peer state machine. It owns
neither the world nor a socket. `source/game/exchange_diagnostic.cpp` supplies
a separate loopback-only reliable TCP driver for two real game processes.
Neither module is used by normal LAN menus or matches. Live LAN is still
protocol 6 delayed lockstep; no user-facing prediction mode is enabled.

## Receipt versus commitment

| Message | Meaning |
| --- | --- |
| Receipt / ReceiptAck | Client action for its assigned slot was received/stored, possibly queued during replay; not a commitment to that simulation tick |
| Revision / RevisionAck | A particular canonical revision was received/stored; a later revision can supersede it before sealing |
| Progress | Client has committed a contiguous prefix; includes the canonical snapshot hash at that tick |
| End / EndAck | Host and client agree on the final committed tick/hash before the diagnostic closes |

Receipt packets include generation, epoch, tick, slot and actions. The host
rejects slot spoofing. Revision packets use the existing 22-byte prototype
record. Host publication must match the world's actually used actions and seal
state. Sealed publications retain their checkpoint hashes until peer progress
passes them; client progress cannot acknowledge an unpublished/unsealed future.
An ahead-of-publication revision acknowledgement is invalid. Stale scope and
older acknowledgements cannot move the peer frontier backwards.

The 64-slot receipt/revision windows cap outstanding work. Public send methods
return false on a barrier, exhausted window, invalid input or full queue; they
do not silently drop or mark an unqueued record as sent. A caller may drain
and retry with current canonical data. Receive-side inability to queue a reply
fails the exchange closed. Normal transport disconnect/heartbeat handling must
abort the session; it must not let a blocked peer continue independently.

## Resync barrier

```text
Host: export confirmed anchor -> Offer ----> Client: preflight; freeze
Host: freeze; wait             <---- Ready
Host: install new epoch       -> Commit ---> Client: install new epoch
Host: wait                    <---- Applied
Host: Resume then new inputs  ------------> Client: resume then new inputs
```

The initial harness connection uses the same barrier for an authoritative
tick-zero bootstrap. A periodic barrier proves continuation after replacement;
a mismatching Progress hash enters `NeedsSync` and uses the same recovery path.

Normal simulation/input publication must stop in all waiting phases. Old
receipt/revision traffic drains without mutating the world across a barrier.
Client input remains blocked after installation until Resume arrives. The host
queues Resume before returning to Running, so subsequent records follow it on
the reliable ordered lane. Duplicate Offer/Ready/Commit/Applied/Resume messages
are idempotent in their expected phases; unexpected ordering or stamp conflicts
fail closed. Every stamp binds generation, new epoch, base tick and state hash.
The transfer's state codec and preflight limits are described in
[input authority](INPUT_AUTHORITY.md).

A handshake/closing operation has a five-second total deadline, not refreshed
by duplicate replies. Timeout or disconnect aborts; there is no silent fallback
to another networking mode. Normal Running-phase liveness remains the future
live driver's heartbeat/transport responsibility. This protocol assumes reliable
ordered delivery, not unreliable UDP with arbitrary lost control messages.

## Bounds and driver scope

- At most 128 queued messages and 64 MiB plus 64 KiB of queued payload per peer.
  Only snapshot records may be large; other message shapes have exact sizes.
- The diagnostic stream uses `TOEX`, version 1, typed length and strictly
  consecutive nonzero sequence numbers in a 16-byte header. Normal LAN's `TOUN`
  protocol is untouched. Truncated framing, bad version/type/length, unexpected
  closure or semantic failure ends the diagnostic with exit code 8.
- The driver bounds incoming bytes, per-poll parsing and socket send/receive
  work. It uses nonblocking sockets after a bounded loopback connect/accept,
  three shared forward/replay ticks per pump and a 30-second overall deadline.
- Buffers, decoded snapshots, transfer copies and owner history have separate
  limits; they are **not** a low total-process-memory promise. No low-power
  acceptance or zero-copy transfer claim is made.
- The current driver supports two players/one client only. Per-peer state
  machines are a building block, not proof of multi-client barrier coordination.
- It seeds generated-level construction deterministically and uses scripted
  actions, initialized sound logic with a dry-run sink, and no renderer. It
  neither saves settings nor uses physical input, lobby UI or match transitions.
  A live driver still needs input resampling policy after an epoch reset.
- It binds/connects only `127.0.0.1`. Matching binaries/assets/configuration are
  a fixture prerequisite, not negotiated/authenticated by this diagnostic lane.

## Running the two-process diagnostic

From two isolated asset staging directories, run:

```text
TOU --verify-rewind woods --rewind-late-input 4 --rewind-canonical 1 --rewind-window 12 --rewind-ticks 240 --exchange-role 0 --exchange-port 27961 --logging
TOU --verify-rewind woods --rewind-late-input 4 --rewind-canonical 1 --rewind-window 12 --rewind-ticks 240 --exchange-role 1 --exchange-port 27961 --logging
```

Role 0 is host, role 1 client. Use exactly two players, canonical ticks, sound
logic enabled and no forced round-ending option. The port is session-local.
`[EXCHANGE] PASS` requires the acknowledged finish. Host `peer_confirmed` is
the client's latest validated committed prefix; the client log's peer field
only tracks its latest resync base (it does not receive host Progress reports).

Windows x64 observations:

- Minibase: both processes reached tick 120, with authoritative bootstrap and
  periodic resync to epoch 3, then acknowledged final tick/hash and exited 0.
- Jungle with troopers (window 8) and generated Happyland (window 12): both
  processes reached tick 180 and completed two resyncs and acknowledged finish.
- A temporary stream relay alternated 20/60 ms delay per direction, preserving
  stream order while scripted input ticks arrived late/out of order. It flipped
  one Progress hash. Both Woods instances reached tick 240 in epoch 4 after
  bootstrap, checksum recovery and periodic resync, with acknowledged finish.
  One run observed 17 host corrections/136 replayed ticks and 25 client
  corrections/175 replayed ticks; timings/counts vary between runs.
- The relay also injected an invalid stream version and separately withheld a
  Ready reply past the deadline. Both processes exited 8 without a PASS or
  accidental resume. These are injected failures, not unexpected test failures.
- An ignored stub-world exchange probe checks duplicate phase messages, freeze
  until Resume, slot spoofing, forged future acknowledgements, commit-before-
  offer, malformed size, bounded outbox exhaustion, timeout without epoch
  application and the checksum-recovery barrier. It uses a shared zero-tick
  stub for phase checks; independent worlds are evidenced by the process runs.

No permanent test suite or CI job was added. This does not establish real
internet/loss acceptance, live menu integration, real audible/GPU output,
natural match/level progression, more clients, Linux/macOS behavior or CM4 cost.

## Next integration gates

Coordinate all peers before epoch/control/level transitions, negotiate the
experimental mode and fingerprints on the real LAN connection, integrate local
input scheduling and heartbeat/backpressure policy, retain a completed display
frame during replay, and verify real presentation. Keep conservative lockstep
available. Do not enable normal LAN prediction merely because the isolated
two-process exchange works.
