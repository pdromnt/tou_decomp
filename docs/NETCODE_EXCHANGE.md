# Experimental bounded peer exchange

`source/game/netcode_exchange.cpp` joins receipts, canonical revisions,
acknowledgements and confirmed resync into one per-peer state machine. It owns
neither the world nor a socket. `NetcodeHostExchange` coordinates one host world
across up to three clients. `source/game/exchange_diagnostic.cpp` supplies
a separate loopback-only reliable TCP driver for two to four real game processes.
The diagnostic driver is isolated. The core exchange's control barrier is also
used by the [opt-in live LAN control mode](LAN_BETA.md); default LAN remains
protocol 6 delayed lockstep. No live prediction mode is enabled.

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

For multiple clients, the session coordinator exports one common anchor and
offers it to every peer. A Ready reply enters `HostReady`; it cannot apply the
host world on its own. Only when **all** peers are ready does the coordinator
install that anchor once and queue Commit for every connection. Applied replies
enter `HostApplied`; only when **all** peers applied does it queue every Resume
and reopen the host's input/simulation gate. Direct per-peer `begin_sync` is
disabled for coordinator-owned exchanges. A mismatch on any peer freezes host
advancement and resynchronizes the entire group, not just the reporting client.
The host's group progress is the minimum validated peer frontier. Duplicate
replies cannot stand in for a missing participant. A timeout or failed peer
aborts all coordinator members; there is no mid-barrier peer removal/quorum.

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

## Confirmed control handoffs

`NetcodeHostExchange::begin_control` prepares pause-menu, resume, skip-level
and end-match anchors using the existing snapshot barrier. These are not
immediate edits followed by a broadcast:

1. Stop admitting forward ticks. Keep receiving inputs, publishing revisions,
   reconciling and committing until the current simulated tick is fully
   confirmed. Already-simulated local inputs must drain too; otherwise the
   host can wait forever for its own missing input. The diagnostic bounds this
   quiescence wait to five seconds.
2. `export_controlled` requires a clean, fully confirmed current world and a
   valid source state. It briefly scopes only the substate, low-byte level
   counter and ending countdown while capturing a candidate, restoring the
   running fields on return or capture/allocation failure. It performs no
   audio/GPU delivery, score update, full-world restore or level I/O.
3. The controlled snapshot's hash binds the requested result. The host remains
   unchanged until every Ready, installs the new epoch once, then waits for
   every Applied before allowing the driver to continue. Clients install only
   on Commit. Delayed old-epoch inputs cannot undo the control.

OpenPauseMenu requires active play and enters the ESC menu, not the single-player
P-key pause. Resume accepts pause/ESC/preview. Skip/end require ESC/preview and
enter the original level-advance state with countdown zero; end additionally
sets the low byte of the level counter to the host's active-level count, retaining
the original upper bytes. Skip leaves the counter untouched. Invalid requests
return false without starting a barrier or changing the running world.

This correctness prototype transfers a **full snapshot for each control**, even
resume. Bandwidth/memory/latency optimization remains open. UI pacing/redraw,
score changes, actual level teardown/load, generation restart and returning to
the lobby are still the live driver's job. The world may be confirmed while the
group is awaiting Applied: irreversible transitions must also require the
session's `running()` gate, not only `world.can_finalize()`.
As with any resync, installation resets the owned presentation journal/loops;
real pause-time shader retention and audible continuity are not verified here.
That applies to the prediction-owning diagnostic. The live control-only owner
below deliberately preserves presentation and never starts a journal.

## Live control-only adapter

`--lan-confirmed-controls` selects protocol 7 on the real LAN connection. The
existing HELLO asset/catalog checks also compare a mode-specific build identity;
packet version mismatches fail before gameplay. Inputs stay in delayed lockstep.
Message 17 wraps a two-byte exchange type plus its existing bytes. Only
Offer/Commit/Resume are accepted from the host and Ready/Applied from clients.
Receipt/revision/progress/end diagnostic messages cannot enter this lane.

The host's menu requests a control before mutating state. A client defers the
Offer until its ordinary lockstep ledger reaches the exact target tick. Then
`start_live_control` borrows that confirmed current world, creates no event
journal, and refuses physics input/advancement. During ownership, gameplay and
rendering hold the completed frame; network/heartbeat polling continues. Both
entry and post-catch-up rendering checks cover a handoff starting mid-frame.
The controlled restore preserves presentation, including local audio handles
and particle tags. Stopping the borrowed owner never stops local sounds.

After release, each side resets its command ledger at that same tick, seeds
neutral input lead and resets pacing/redraw. Commands, bundles and checksums
in protocol 7 add an epoch immediately after the generation; old epochs are
ignored. Client Resume handling reseeds inline before parsing the next bundle
in the same socket read. Level load starts a new generation with epoch 1.
Default protocol 6 shapes and delayed-control path remain available unchanged.

The group gate blocks irreversible outer scoring/level teardown until release.
The regular scoring, resource loader and stats path then run normally, once per
round. Heartbeat/queue safeguards remain on the existing LAN transport; any
handoff timeout/failure or checksum mismatch aborts the whole session rather
than attempting a one-client correction during an epoch barrier. A five-second
deadline covers the complete handoff including snapshot transfer. No silent
fallback, reconnect, late join or reduced quorum is implemented.

Two/four-process Windows x64 live runs passed pause/resume, skip, accounting,
resource teardown/reload, generation-2 preview/resume and end to the stats route.
The delayed relay passed stale epoch commands/bundles and wrong-generation
control injection. Withheld Ready, bad stream version and current-epoch bad
checksum returned both peers to the LAN-screen route.
The four-player disconnect probe also closed the third client mid-handoff;
all remaining processes returned to LAN without releasing the control.
Owner probes verify
confirmed-only admission, presentation-preserving restore/stop and no physics
ticks/journal ownership. No human audio/visual or cross-platform acceptance is
claimed. See [LAN beta](LAN_BETA.md) for opt-in and developer-script commands.

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
- The driver supports two to four players, with one distinct slot per client.
  The host opens one loopback port per client (base port plus slot minus one).
  Slots are explicit diagnostic arguments, not a negotiated lobby or discovery.
- It seeds generated-level construction deterministically and uses scripted
  actions, initialized sound logic with a dry-run sink, and no renderer. It
  neither saves settings nor uses physical input or lobby UI. Control diagnostics
  stop at the confirmed transition anchor, before actual score/level teardown.
  A live driver still needs input resampling policy after an epoch reset.
- It binds/connects only `127.0.0.1`. Matching binaries/assets/configuration are
  a fixture prerequisite, not negotiated/authenticated by this diagnostic lane.

## Running the process diagnostic

From two isolated asset staging directories, run:

```text
TOU --verify-rewind woods --rewind-late-input 4 --rewind-canonical 1 --rewind-window 12 --rewind-ticks 240 --exchange-role 0 --exchange-port 27961 --logging
TOU --verify-rewind woods --rewind-late-input 4 --rewind-canonical 1 --rewind-window 12 --rewind-ticks 240 --exchange-role 1 --exchange-port 27961 --logging
```

Role 0 is host, role 1 client. Use two to four players, canonical ticks, sound
logic enabled and no forced round-ending option. The port is session-local.
For four processes add `--rewind-players 4` everywhere and launch three clients
with `--exchange-slot 1`, `2`, and `3` respectively (default slot 1). Use the same
base `--exchange-port` in each process; reserve three consecutive free ports.
Launch clients promptly: connection setup and each barrier have bounded waits.
Add `--exchange-controls 1` to all processes for pause/resume followed by normal
continuation and finish, `2` for pause/skip, or `3` for pause/end-match (default
`0`: no scripted controls). Pause is requested near one-quarter of the target;
skip/end stop at that confirmed tick and acknowledge its controlled hash. These
modes require `[EXCHANGE] controls PASS` as well as the normal PASS. They check
states, counter/countdown and agreement, not the outer scoring/next-level route.
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
- Four independent Woods processes reached tick 240 with group bootstrap and
  periodic resync, all exiting 0. A four-process relay with 20/60 ms per-way
  delay, staggered readiness and one client's corrupted Progress hash recovered
  the whole group to epoch 4, then acknowledged tick 240 and exited 0 everywhere.
- Four generated Happyland worlds reached tick 600, crossing receipt/revision
  ring wrap, with common bootstrap/periodic resync and acknowledged finish.
  All four processes exited 0 (window 12, three shared work ticks per pump).
- Withheld Ready, withheld Applied and a disconnected third client each caused
  all four processes to exit 8 without PASS. Withheld Ready left the host at
  the old epoch; withheld Applied allowed installation but never group resume.
- The ignored stub-world probe also checks three-peer all-ready/all-applied
  gating, one host installation, duplicate readiness before/after application,
  rejection of per-peer coordinator bypass, and whole-group timeout/disconnect.
- Four independent Woods processes passed pause/resume with continuation to
  tick 240, and separate pause/skip and pause/end runs with acknowledged final
  controlled hashes. Pause preparation did not change the host's running state.
  Four generated Happyland processes also passed pause/resume and tick-240 finish.
- A four-process 20/60 ms per-way relay passed pause/resume plus one-peer
  checksum recovery, reaching epoch 6/tick 240. Holding one client's control
  Ready or Applied past the deadline aborted all four without PASS: Ready did
  not install the pause; Applied did not release the group.
- Owner probes check invalid source/enum requests, no mutation on preparation
  or controlled capture failure, rejection while speculative, resume continuation,
  skip/end counter/countdown semantics, and low-byte preservation.
- An ignored stub-world exchange probe checks duplicate phase messages, freeze
  until Resume, slot spoofing, forged future acknowledgements, commit-before-
  offer, malformed size, bounded outbox exhaustion, timeout without epoch
  application and the checksum-recovery barrier. It uses a shared zero-tick
  stub for phase checks; independent worlds are evidenced by the process runs.

No permanent test suite or CI job was added. The isolated diagnostic evidence
does not establish live prediction, real internet/loss acceptance, audible/GPU
output, natural match progression, Linux/macOS behavior or CM4 cost.

## Next integration gates

Integrate predicted input/revisions and the bounded pump with the live clock,
input sampler and real sound/GPU delivery, retaining a completed display during
partial replay. Exercise natural wins, repeated matches, physical menu controls,
remote/cross-platform behavior and low-power costs. Optimize full-snapshot
controls before treating their latency/bandwidth as accepted. Keep conservative
lockstep available. The CLI control opt-in is not a rollback toggle or evidence
that live prediction is ready; no menu toggle is exposed yet.
