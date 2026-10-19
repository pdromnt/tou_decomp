# Direct-IP LAN Beta

This is the first playable networking test, not the final multiplayer UI.
It supports one local player per computer, one host plus up to three clients,
two teams, and no computer players. Everyone must use the same game package and
level catalog.

The v0.11 source comparison with Uprising's ENet implementation is documented
in [Transport review](NETPLAY_TRANSPORT_REVIEW.md). The `advanced-netcode`
branch now has experimental protocol 6 diagnostics and negotiated input delay;
it cannot connect to stable v0.11 protocol 4 peers. TCP lockstep remains in use.
The local-rewind prototype also advances snapshot format/build identity to
`tou-sim2-net6-snapshot5`, rejecting older experimental branch builds. The
live network game still has no prediction or rollback.
Simulation revision 2 resets transient flags per simulation tick in LAN,
independent of local catch-up grouping. Local matches retain legacy batching.
Inputs, checksums and pause/skip controls carry the level generation. Controls
wait for their effective confirmed tick rather than interrupting catch-up.
The input ledger tracks simulated/confirmed frontiers; live prediction remains
zero. Initial synchronization also runs while the level preview is visible.
The [local reconciliation prototype](INPUT_RECONCILIATION.md) keeps prediction
diagnostic-only; the optional control-only anchor below does not replay physics.
The separate [peer exchange diagnostic](NETCODE_EXCHANGE.md) exercises two to four
processes on loopback. Only its confirmed-control barrier is integrated below,
not its predicted input/revision lane.

## Opt-in confirmed controls (experimental branch only)

Launch **every participant** with `--lan-confirmed-controls` to use confirmed
pause/resume/skip/end handoffs in the existing LAN menus or direct-IP CLI flow.
This selects protocol **7**, build identity `tou-sim2-net7-snapshot5-controls1`.
It cannot mix with default protocol 6 or stable v0.11 protocol 4 peers; mismatches
disconnect rather than silently switching mode. No menu toggle is exposed yet.

Gameplay still uses delayed TCP lockstep with zero prediction. The host remains
the only operator of ESC, next-level, end-match and preview-resume controls.
Each control transfers a full confirmed snapshot and waits for all clients.
Clients finish through the host's target tick before acknowledging; no side
may score/unload a level before its handoff releases. Resume reseeds neutral
input lead, and epoch-scoped commands/bundles/checksums discard old queued input.
P and window focus retain the existing multiplayer no-pause rules.

The last completed frame is held while the world is borrowed by the barrier.
Control installation preserves local sound ownership and particle tags, without
starting/stopping the prediction event journal. This has source/stub verification,
not human audible/shader acceptance. Full snapshots are bandwidth-heavy and the
handoff deadline is five seconds. Timeout, malformed control or checksum mismatch
aborts the whole session to the LAN-screen route; per-client correction is not
used in this opt-in mode. Default protocol 6 behavior is unchanged.

Windows x64 scripted runs exercised two and four real LAN processes through
pause/resume, skip, actual round accounting, next-level teardown/load and preview,
resume and end-match to the stats route. Each round accounted once, with no
awarded wins for skip/end. A 20/60 ms per-way relay passed the same route while
injecting stale commands/bundles and a wrong-generation control. Withheld Ready,
bad packet version and a current-epoch checksum mismatch returned both peers
to the LAN-screen route. Closing the third client during a four-player handoff
returned all three remaining processes to the LAN route without releasing it.
These are loopback/fault-injection observations, not
remote, cross-platform, physical-controller or visual/audio acceptance.

Developer route probe (use isolated asset/settings directories): host flags
`--lan-host --lan-auto-start --lan-confirmed-controls --lan-control-script --logging`;
clients use `--lan-join HOST:PORT --lan-confirmed-controls --logging`.
`--lan-auto-start-players N` makes CLI auto-start wait for 2-4 total players
(default 2). The script drives actual control APIs and level/scoring code, not
keyboard injection; `[NET] live control script PASS` marks the host reaching
the final stats route. It is not a permanent test suite or CI job.

## Start a session

Open **Team Deathmatch** from the main menu, then choose:

- **Local Deathmatch** for the original same-computer game.
- **LAN Deathmatch** for a direct-IP network session.

The host selects **Host LAN Match**, waits for the connected-player roster to
fill, and then selects **Start LAN Match**. The host is Team 1 and listens on
TCP port `27015`.

On `advanced-netcode`, click the yellow **Input delay** line in the host lobby
to cycle from 1 to 12 ticks (default 3). Clients see the host's choice; it is
locked when the match starts and is not saved in settings. At the default
integer 16 ms tick interval, three ticks are approximately 48 ms of input lead.
A larger lead may reduce missing-input stalls but adds control delay; it does
not eliminate the host-to-client command-bundle trip. The lobby also displays
smoothed round-trip latency and RTT variation, taking the maximum of each
measurement across connected peers. Variation is an estimate, not one-way
latency or a direct measurement of packet loss.

Each client selects **Join LAN Match**, edits the host/IP and port fields,
chooses Team 1 or Team 2, and selects **Connect**. Hostnames and IPv4 addresses
are accepted. Permit the game through the host firewall for private networks
if prompted. The status line and window title report the connection state and
assigned player number. Both sides see the compact roster with player, team,
ship, and implicit ready/loading state. The Join screen remembers the most
recent valid host, port, and team in `settings.json`.

The following command-line interface remains available for diagnostics and
custom ports. The host launches the game from its extracted directory:

```powershell
.\TOU.exe --lan-host --port 27015 --team 1 --logging
```

Linux and macOS use `./TOU` in place of `TOU.exe`. The port and team arguments
are optional; the defaults are port `27015` and Team 1.
The experimental host also accepts `--lan-input-delay 7` (valid range 1-12).
No flags are needed to select delay or see RTT in the lobby.

Each client launches:

```powershell
.\TOU.exe --lan-join 192.168.1.50:27015 --team 2 --logging
```

Replace the address with the host's LAN IPv4 address.

The host's existing rules and level list win; every computer contributes its
local Player 1 ship and controls. Connected clients are implicitly ready after
their level-loaded synchronization barrier.

In a LAN match, pressing `P` or unfocusing the game does not pause simulation.
Opening the host's `Esc` match-control menu pauses the synchronized session so
the host can resume, skip the level, or end the match. Clients cannot invoke
those session controls.

## What the beta currently proves

- Portable TCP connection and a versioned, bounds-checked protocol.
- Direct-IP Windows/Linux/macOS interoperability.
- Delayed per-tick action exchange; clients never send gameplay outcomes.
- A level-loaded barrier and authoritative host tick-zero snapshot, followed by
  host RNG sequencing, synchronized pause/exit state, and periodic state hashes.
- Explicit rejection when the simulation build, level/theme bytes, ships, or
  gameplay data do not match.
- Host snapshot correction after a periodic checksum mismatch.
- Clean session teardown back to the LAN screen after timeout or peer loss.

## Deliberate beta limits

- There is no explicit Ready button, discovery, AI, reconnect, mid-match join,
  asset transfer, NAT traversal, or internet relay.
- TCP lockstep favors correctness over latency. A slow or lost peer can stall
  the match until the disconnect is detected.
- Content fingerprinting covers the ordered normal-level bytes, GG theme files,
  ships, and gameplay data. Music, sound effects, help, and translations are
  intentionally not simulation compatibility inputs.
- The handshake's simulation compatibility revision is independent of the app
  version. Changing that identity rejects older identities; use the same build
  on every peer, including after the release-tagged ID was replaced.
- Snapshot correction recovers detected state drift; it is not prediction,
  rollback, or reconnection and may cause a visible hitch.
- Tick-zero snapshots are capped at 64 MiB. Very large authored/GG worlds can
  be rejected by this first beta instead of exhausting peer memory.

Logs append to `debug.txt` when `--logging` is supplied. When reporting a LAN
problem, keep the host and every client log and note each operating system,
architecture, level, approximate tick/time, and the last action performed.
Experimental `[NET]` lines include per-peer latest/smoothed RTT, smoothed RTT
variation, sample count and outgoing queue bytes. Match summaries every two
seconds include role, tick, negotiated delay, completed missing-input/bundle
wait episodes, total/max completed wait, current wait, arrival age and maximum
gap between network polls. Arrival age is time since local receipt of the first
remote input (host) or bundle (client), not travel time or input sampling age.
Intentional pauses and initial level barriers are excluded from input waits.
Counters reset at level load; RTT estimates persist for the connection.
Probe timing includes local polling and TCP queue delays: a screenshot, level
load or busy frame can raise it without the physical link becoming slower.
For a reproducible desync investigation, add `--lan-dump-state` to both command
lines. It writes periodic `lan-host-*.snap` / `lan-client-*.snap` captures and
matching action logs beside the executable; these can be large and may be
deleted after comparison.

For automated smoke testing only, `--lan-auto-start` makes a host begin with
its saved rules as soon as the first client completes the handshake. It starts
a two-player session immediately, so do not use it when gathering more peers.
