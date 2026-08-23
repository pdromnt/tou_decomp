# Decompilation of Tunnels of Underworld

This repository contains a behavior-focused reconstruction of **Tunnels of
Underworld** (TOU), an old Windows game created by
[hannukp](https://github.com/hannukp).

The original source appears to be lost. This project was recovered through
runtime comparison, disassembly, Ghidra, and a deliberately ugly-first static
reconstruction. I do not claim ownership of the original game or its assets.

Please note this project doesn't have an official Discord server or Subreddit,
the only way to collaborate or contact people who are members of the project is
via the repo's Issues page.

## Current Status

The major gameplay-parity pass is complete. Weapons and their selectable Marks,
enemy ships, turrets, physics, menus, controls, levels, audio, particles, and
effects have all received hands-on runtime testing against the original game.

The code is still recognizably a decompilation: original addresses, raw memory
offsets, and Ghidra-style names remain where changing them without stronger
types would risk behavior. Cleanup should be incremental and parity-preserving.

See [PLAN.md](PLAN.md) for the completed parity record,
[CODEBASE.md](CODEBASE.md) for the source map, and [BACKLOG.md](BACKLOG.md) for
future refactoring and platform work.

## Running a Release

Extract the complete release archive and run `TOU.exe` from that directory. Do
not move the executable away from `fmod.dll`, `options.cfg`, or the asset
directories.

The decomp intentionally runs windowed and identifies itself as
`Tunnels of Underworld - RE/Decompiled` so it cannot be confused with the original
fullscreen executable.

## Building on Windows

Requirements:

- 32-bit MinGW-w64 GCC/G++
- GNU Make (`mingw32-make`)
- `windres`
- Windows DirectDraw, DirectInput, and WinMM development libraries

Build from the repository root:

```powershell
mingw32-make clean
mingw32-make -j8
```

The output is `TOU.exe`. `build.bat` performs the same clean build and closes an
already-running decomp executable first.

The separate `Build` GitHub Actions workflow performs this 32-bit build for
every push and pull request. It validates the executable architecture but never
publishes a release.

To create the same archive layout used by CI:

```powershell
./scripts/package-release.ps1 -Version local
```

## Releases

The `Build release` GitHub Actions workflow only runs when started manually.
Enter the release tag/version (for example `v1.0.0`) when choosing **Run
workflow**; it builds the game and creates the tagged GitHub Release with the
package attached. If that tag already exists, the workflow builds its exact
commit; otherwise it creates the tag at the commit selected when starting the
workflow. Rerunning an existing release replaces its package. Commits and tag
pushes do not trigger releases.

## Longer-Term Direction

Once the recovered code is easier to maintain, likely follow-up work includes
an SDL renderer/input/audio port, Linux/macOS/browser support, gamepads,
non-split-screen netplay, and better tooling for `.lev` and GG level formats.

## Contributing

Behavior changes need evidence from the original executable. Pure refactors
must keep the 32-bit build working and avoid changing fixed-point arithmetic,
RNG order, callback dispatch, or update order by accident.

## Tools Used

- MinGW-w64
- Ghidra and GhidrAssistMCP
- Runtime comparison on hardware capable of running the original game

## License

No license is currently offered. This is not a clean-room decompilation, and the
repository includes original game data for preservation purposes.
