# Experimental presentation timeline

This is Stage 3 groundwork on `advanced-netcode`, not a live rollback feature.
Ordinary local and TCP LAN play do not enable the event timeline. They retain
their immediate sound/shader path. Only the developer rewind diagnostic
currently enables it, with actual output disabled.

## Ownership

`source/game/presentation_events.cpp` owns a game-thread-only ring of 16 ticks,
with at most 4096 events per tick. An event's identity is its tick and ordered
position within that tick. Re-simulation replaces the tick's event sequence.
Repeated or stale confirmation is idempotent. Finalized ticks cannot be
re-simulated, and an overflow makes the timeline unhealthy: confirmation then
fails before any partial range is emitted. The journal is separately bounded
(about 1.25 MiB of event payload at capacity); it is not part of network state.

Sound plays, stops, volume and pan changes are buffered at the SDL audio
boundary. During speculative execution, a deterministic 64-voice round-robin
allocator returns virtual channel IDs without consulting mixer completion
timing. Only confirmation maps those IDs to physical tracks. Track stealing
invalidates previous virtual ownership. This experimental voice policy is not
the ordinary mixer's quietest-channel selection and needs audible acceptance.
Physical channel generation stamps also reject stale stop/mix commands after
an immediate/menu sound has taken the same track.

Shader explosion, boost, laser, cutting-spark and organic-terrain events are
buffered before their backend/time checks. Cutting contacts are sampled during
the simulation, before terrain removal, rather than recomputed on confirmation.
Organic events retain their original tile/colour stamp; confirmation rejects a
stamp if that terrain has changed. Deferred cutting sparks currently use tick
event frequency rather than the immediate path's 40 ms wall-clock throttle.

A separate local checkpoint stores player sound timers, sound identities and
virtual channel references, the next virtual voice, and pool-indexed exhaust/
weapon-particle tags. These fields are not transmitted or hashed as gameplay.
`SimulationState_Restore(..., true)` preserves committed cosmetic rings and
organic tags; the matching local checkpoint then restores pool metadata.
Normal LAN snapshot correction still uses the default full cosmetic reset.
Level unload and session shutdown discard the journal and stop its owned loops.

Music, focus mute, settings volume and menu audio stay outside simulation ticks.
No GPU resources, framebuffer or physical mixer handles are rewound.

## Tick batching

Experimental LAN resets transient gameplay flags once per successfully prepared
simulation tick, not per wall-clock catch-up batch. It therefore uses simulation
identity `tou-sim2-net6-snapshot5`. Local play preserves legacy batching. The
wire protocol is now 6: simulation fingerprint validation rejects earlier
branch builds. The rewind diagnostic can compare canonical ticks grouped 9 at
a time forward against groups of 1 or 2 on replay.

## Evidence and remaining gates

Windows x64 Release build and localization validation pass. A 1200-tick Woods
run with 64 players and 9-versus-1 grouping matched canonical state, local
presentation state and event digests. One observed run confirmed 8794 sound
plays and 108 explosion events exactly once to the dry-run sink. Jungle's
trooper early-return path and the forced terminal countdown also matched.
These are individual time-seeded runs, not fixed golden event counts.

A one-off ignored build-directory probe exercised the real journal/dispatcher
with stub sinks: every event type dispatched once after replacement, duplicate
and stale confirmation emitted nothing, finalized-tick writes were rejected,
and ring/event overflow blocked partial output. It is not a permanent test suite
or CI job, nor a test of SDL/GPU delivery quality.
The probe also checked that a physical-track generation change prevents later
virtual stop, volume and pan commands from touching the new owner.

Still required for live prediction acceptance:

- Verify real audible output, loop transitions, voice stealing, focus/mute and
  multi-viewport rendering; dry-run equality cannot establish their quality.
- Exercise real boost/laser/spark/organic event producers and HUD/LOS/pause
  edges; current full-game evidence primarily covers sounds and explosions.
- Exercise the new confirmed-only score/level transition guard through full
  match/level changes. The terminal diagnostic verifies that the guard is
  closed before event commit and open afterwards, but does not unload a level.
- Profile the integrated [bounded reconciliation pump](INPUT_RECONCILIATION.md)
  with live authority/resync and clock pacing on low-power hardware.

The experimental branch now exposes live prediction under LAN Network settings.
It is not an accepted replacement for default lockstep; no ENet migration is enabled.
