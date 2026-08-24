# Tunnels of Underworld Binary-Parity Plan

## Status

The dedicated binary-parity pass is complete as of 2026-08-23. Work from the
`accuracy/binary-parity` branch was compared repeatedly against the original
Windows executable, accepted through hands-on runtime testing, and merged into
`main`.

This file is now a completion record and the contract for handling future
discrepancies. General cleanup and modernization work lives in `BACKLOG.md`.

## Completed Surface

- Every selectable weapon and Mark received an original-versus-decomp gameplay
  pass, including weapon physics, collisions, terrain effects, lifecycle, and
  presentation fixes found during testing.
- Weapon Mark selector dots now reflect the available Marks and reset to Mark I
  when changing weapons.
- Turret targeting and enemy-ship AI were recovered from original code paths and
  runtime-tested.
- Options, controls, player setup, level hover details, post-match input, focus
  behavior, and team-stat presentation were repaired and accepted.
- The original MSVC RNG, fixed-width memory helpers, x87 conversion behavior,
  and original callback-address dispatch are part of the normal game build.
- The rebuilt game uses the recovered original icons and packages all required
  runtime assets.

## Production Integration

The former experimental `accuracy/` layer has been promoted into normal engine
modules:

- `binary_compat.*` preserves original 32-bit integer, RNG, memory-access, and
  x87 behavior.
- `entity_callbacks.*` owns the recovered address-based entity callback table
  and lifted weapon/effect callbacks.

There is no separate accuracy-test executable in the regular build. Focused
instrumentation or comparison programs should be created only when a concrete
discrepancy requires them, then kept out of release packages.

## Authority Order

When a future behavior discrepancy appears, use this order of authority:

1. Original executable and observed runtime behavior
2. Original assembly, debugger traces, and memory traces
3. Ghidra control flow and decompiler output
4. Existing reconstructed source and comments

The source remains a reconstruction. Readability changes must preserve integer
overflow, signed shifts, fixed-point ordering, RNG ordering, callback identity,
and x87 conversion behavior unless the original binary proves otherwise.

## Future Discrepancy Workflow

1. Reproduce the same controlled scenario in the original and decomp.
2. Identify the first observable or state divergence.
3. Recover the relevant original routine and inspect its assembly.
4. Trace inputs, outputs, globals, memory offsets, RNG calls, and update order.
5. Change the decomp from that evidence.
6. Rebuild and compare both runtimes again.
7. Obtain runtime acceptance before calling it fixed.

Optional entity tracing remains available by setting `TOU_ENTITY_TRACE=1`; it
writes the untracked diagnostic file `entity-trace.csv`. Delete it after the
investigation; it is not part of release packages.

## Intentional Compatibility Difference

The original executable is fullscreen-only. The decomp supports windowed and
fullscreen modes for compatibility and is titled
`Tunnels of Underworld - RE/Decompiled - v0.4`. That is intentional platform
behavior, not an unresolved gameplay discrepancy.

## Merge Gates

- [x] Gameplay and presentation comparison accepted by Pedro
- [x] Weapon, menu, turret, and enemy-ship AI passes completed
- [x] Experimental runtime code promoted into normal engine modules
- [x] Standalone accuracy-test executable removed from the normal workflow
- [x] Clean 32-bit MinGW build succeeds
- [x] Release package contents validated
- [x] Manually dispatched GitHub Actions workflow builds the package and creates
  the requested tagged release
