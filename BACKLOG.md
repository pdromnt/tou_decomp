# Tunnels of Underworld Backlog

The settings, localization, level-editor, portable-runtime, simulation, and
direct-IP LAN foundations are complete. Their implementation and forensic
notes live in `CODEBASE.md` and `docs/`; they are intentionally not duplicated
here as a wall of checked boxes.

There is no active bug or technical-debt backlog. New defects should be added
only after a concrete reproduction.

## Future Features

- Restore a simplified, expansion-friendly campaign using newly authored
  content. The original `sp1` through `sp14` levels are missing; see
  `docs/CAMPAIGN_FORENSICS.md`.
- Add gamepad support.
- Add community translation tooling and additional languages when translators
  are available.
- Improve LAN play only when demanded by testing: discovery, reconnect,
  mid-match join, prediction/rollback, internet relay/NAT traversal, AI,
  dedicated servers, spectators, or multiple local players per client.
- Consider optional GPU presentation/post-processing behind the authoritative
  RGB565 software framebuffer.
- Target Browser/WebAssembly last, after native desktop work remains stable.

## Rules for New Work

Original executable/runtime behavior remains authoritative for gameplay. Keep
update order, RNG order, fixed-point and x87 semantics, callback identities,
collision, terrain mutation, scoring, and subtype-specific effect setup intact.

Modern settings, localization, editor, display, and network UI may intentionally
differ. Semantic `DAT_`/`FUN_` renaming happens only with binary evidence in the
subsystem already being touched; it is not a standalone endless cleanup task.

A successful build is not runtime acceptance. Fix concrete regressions by
comparing the original runtime, assembly, reconstructed path, and hands-on
behavior in that order.
