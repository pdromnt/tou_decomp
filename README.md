# RE/Decompilation of Tunnels of Underworld
<img width="321" height="252" alt="image" src="https://github.com/user-attachments/assets/d9d58472-6413-4061-baef-b6deca4c8dd8" />
<img width="321" height="252" alt="image" src="https://github.com/user-attachments/assets/b1d9f5a9-fc8d-4ef4-9b14-34e09aa18163" />


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
not move the executable away from its asset directories. The game creates
`options.cfg` beside the executable after first run.

The decomp supports windowed and fullscreen modes and identifies itself as
`Tunnels of Underworld - RE/Decompiled - v0.4` so it cannot be confused with
the original executable.

## Building on Windows

Requirements:

- 32-bit MinGW-w64 GCC/G++
- CMake 3.24 or newer
- Ninja or GNU Make
- `windres`
- Windows DirectDraw and WinMM development libraries

The primary build statically links SDL3 and SDL_mixer and uses them for
presentation, input, WAV effects, and Ogg Vorbis music. Build from a 32-bit
MinGW environment:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
```

The output is `TOU.exe`. Both SDL libraries are fetched at pinned releases and
statically linked, so the game does not require separate SDL or FMOD DLLs. Pass
`--directdraw` to run the legacy presentation backend for A/B comparison.

The old Makefile remains temporarily available as a DirectDraw/DirectInput
fallback while the platform migration is underway.

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

The active SDL3 migration is moving presentation, windowing, input, and audio
behind portable platform boundaries. Native Linux and macOS builds are the
first portability target; browser support comes only after those are stable.
Gamepads, non-split-screen netplay, and better tooling for `.lev` and GG level
formats remain later possibilities.

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
