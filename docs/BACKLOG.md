# Tunnels of Underworld Backlog

The settings, localization, level-editor, portable-runtime, simulation, and
direct-IP LAN foundations are complete. Their implementation and forensic
notes live in `docs/CODEBASE.md` and the other `docs/` guides; they are intentionally not duplicated
here as a wall of checked boxes.

New defects should be added only after a concrete reproduction.

## Experimental Branch: Advanced Netcode

- Follow the gated [advanced netcode plan](ADVANCED_NETCODE.md) on
  `advanced-netcode`, separate from stable main. Verify real sound/GPU delivery
  and weapon-specific producers, exercise confirmed-input/session transition
  barriers across full match/level changes, extend local reconciliation to
  rolling arrival/pacing and authoritative host/client revisions/resync, and
  profile low-power costs before live prediction. The local checkpoint owner
  and late-input oracle are described in [input reconciliation](INPUT_RECONCILIATION.md).
  Remote and cross-platform acceptance remain open.

## Active Work: Presentation Shaders

- Confirm macOS OpenGL and Linux/GLES compatibility and Raspberry Pi CM4
  performance with automatic shader-backend startup; unsupported devices retain
  normal SDL presentation and Pixel art remains the default scaling mode.
- Finish targeted VFX edge/performance checks: LOS/HUD boundaries, split screen,
  simultaneous deaths, pause/level cleanup, and high-player-count exhaust scenes.
  Measure the repeated bloom passes, CPU flame mask, and distortion on low-power
  GPUs before deciding whether quality controls are needed. The manual checklist
  lives in `docs/SHADER_PRESENTATION.md`.

## Future Features

- Restore a simplified, expansion-friendly campaign using newly authored
  content. The original `sp1` through `sp14` levels are missing; see
  `docs/CAMPAIGN_FORENSICS.md`.
- Extend gamepad acceptance beyond the tested Xbox Bluetooth menu/gameplay flow:
  other controller families, Linux/macOS, multiple local pads, LAN Player 1
  controls, reconnect, and focus transitions. See `docs/GAMEPADS.md`.
- Add community translation tooling and additional languages when translators
  are available.
- Improve LAN play only when demanded by testing: discovery, reconnect,
  mid-match join, prediction/rollback, internet relay/NAT traversal, AI,
  dedicated servers, spectators, or multiple local players per client.
- Extend optional post-processing only when desired, keeping gameplay and the
  authoritative software framebuffer independent of presentation effects.
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
