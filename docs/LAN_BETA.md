# Direct-IP LAN Beta

This is the first playable networking test, not the final multiplayer UI.
It supports one local player per computer, one host plus up to three clients,
two teams, and no computer players. Everyone must use the same game package and
level catalog.

## Start a session

Open **Team Deathmatch** from the main menu, then choose:

- **Local Deathmatch** for the original same-computer game.
- **LAN Deathmatch** for a direct-IP network session.

The host selects **Host LAN Match**, waits for the connected-player roster to
fill, and then selects **Start LAN Match**. The host is Team 1 and listens on
TCP port `27015`.

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
- Snapshot correction recovers detected state drift; it is not prediction,
  rollback, or reconnection and may cause a visible hitch.
- Tick-zero snapshots are capped at 64 MiB. Very large authored/GG worlds can
  be rejected by this first beta instead of exhausting peer memory.

Logs append to `debug.txt` when `--logging` is supplied. When reporting a LAN
problem, keep the host and every client log and note each operating system,
architecture, level, approximate tick/time, and the last action performed.
For a reproducible desync investigation, add `--lan-dump-state` to both command
lines. It writes periodic `lan-host-*.snap` / `lan-client-*.snap` captures and
matching action logs beside the executable; these can be large and may be
deleted after comparison.

For automated smoke testing only, `--lan-auto-start` makes a host begin with
its saved rules as soon as the first client completes the handshake. It starts
a two-player session immediately, so do not use it when gathering more peers.
