# Netplay transport comparison

This is a source review of the local Uprising shipping implementation, not a
claim of internet-latency testing or a plan to replace TOU's transport in v0.11.
Uprising's local-only reconstruction lab was not inspected or modified.

## What Uprising does

`source/platform/portable_network.c` owns ENet behind a platform transport API.
One SDL worker services sockets under a mutex, copies bounded packets into a
queue, and never updates gameplay memory. The game thread consumes copied
events. The transport exposes ENet's measured `roundTripTime`, implements UDP
session discovery, and maintains peer links that support host departure.

The inspected send path uses `ENET_PACKET_FLAG_RELIABLE` on channel 0, with one
channel per connection. It is reliable/ordered delivery over UDP, not magic
unreliable input delivery. A missing reliable packet can still hold up later
packets on that channel. These are source observations, not cross-platform or
poor-network acceptance results.

## What TOU can learn

1. **Measure before changing transport.** TOU's empty ping/pong messages are
   keepalives, not RTT/jitter measurements. Timestamped probes, visible latency
   and stall diagnostics would make real network reports much more useful.
2. **Make input delay tunable.** TOU currently schedules three ticks ahead
   (about 48 ms at the default integer 16 ms tick interval). A configurable or
   measured delay may improve pacing on higher-latency/jittery links, at the
   cost of more input lag. ENet alone does not remove this tradeoff.
3. **Separate transport ownership.** A bounded worker queue can keep I/O moving
   while loading, taking a screenshot or processing window events. Only the
   game thread should consume authoritative commands or apply snapshots.
4. **Avoid one ordered lane for everything.** If ENet is adopted later,
   separate reliable control/snapshot traffic from time-critical commands
   where protocol ordering permits. Simply putting today's TCP messages onto
   one reliable ENet channel preserves much of the same blocking behavior.
5. **Borrow capabilities, not Uprising's protocol.** LAN discovery and clean
   peer-lifecycle handling are useful examples, but host migration, prediction
   and reconnect need TOU-specific simulation/session rules.

## Recommendation

Keep TOU's existing TCP lockstep, validation, tick-zero synchronization and
snapshot correction unchanged for this release. First obtain a real remote
match report, then prioritize RTT/jitter reporting and adjustable input delay.
Consider an ENet transport boundary only if measured problems justify it.
It would require protocol/version handling, UDP firewall guidance, loss/jitter
testing and full multiplayer acceptance, not just linking another library.

For the current implementation and limitations, see [LAN beta](LAN_BETA.md).
For the experimental branch roadmap, see [advanced netcode](ADVANCED_NETCODE.md).
