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

See [CODEBASE.md](CODEBASE.md) for the source map and safe-refactoring guidance,
and [BACKLOG.md](BACKLOG.md) for the parity contract, current roadmap, and
remaining work.

## Running a Release

Download the archive matching your operating system and architecture and
extract it completely. Run `TOU.exe` on Windows, `TOU` on Linux, or
`TOU.app` on macOS. Do not move the executable or app away from its packaged
assets. The game creates `options.cfg` beside the assets; on macOS it lives in
`TOU.app/Contents/Resources`.

The original default controls were designed for a Windows keyboard and are
awkward on a MacBook. macOS players should open **Options → Controls** and
remap them after the first launch; Right Option and Right Command are practical
choices for the primary action keys.

The decomp supports windowed and fullscreen modes and identifies itself as
`Tunnels of Underworld - RE/Decompiled - v0.5` so it cannot be confused with
the original executable.

## Building

Requirements:

- A C/C++ compiler for the target platform
- CMake 3.24 or newer
- Ninja or GNU Make

The primary build statically links SDL3 and SDL_mixer and uses them for
presentation, input, WAV effects, and Ogg Vorbis/MP3 music:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
```

The output is `TOU.exe` on Windows, `TOU` on Linux, or `TOU.app` on macOS.
Both SDL libraries are fetched at pinned releases and statically linked. A
32-bit MinGW build remains the closest host to the original executable; x64
and ARM64 are also supported. Windows packages do not require SDL DLLs or the
Visual C++ redistributable.

The separate `Build` GitHub Actions workflow builds Windows x86, Windows and
Linux x64/ARM64, and macOS Intel/Apple Silicon for every push and pull request.
It validates the executable architecture but never publishes a release.

To create the same archive layout used by CI:

```powershell
./scripts/package-release.ps1 -Version local -Platform windows-x64 -ExecutablePath ./TOU.exe
```

## Releases

The `Build release` GitHub Actions workflow only runs when started manually.
Enter the release tag/version (for example `v1.0.0`) when choosing **Run
workflow**; it builds all supported desktop targets and creates the tagged
GitHub Release with seven platform/architecture packages attached. If that tag
already exists, the workflow builds its exact commit; otherwise it creates the
tag at the commit selected when starting the workflow. Rerunning an existing
release replaces its packages. Commits and tag pushes do not trigger releases.
Windows packages use ZIP; Linux and macOS use `.tar.gz` to preserve executable
permissions.

## Longer-Term Direction

SDL3 now owns presentation, windowing, input, audio, timing, dialogs, and file
discovery behind portable platform boundaries. Browser support comes only after
the native desktop builds are runtime-proven. Gamepads, non-split-screen netplay,
and better tooling for `.lev` and GG level formats remain later possibilities.

## Contributing

Behavior changes need evidence from the original executable. Pure refactors
must keep the x86 parity build and native builds working and avoid changing
fixed-point arithmetic, RNG order, callback dispatch, or update order by accident.

## Tools Used

- MinGW-w64
- Ghidra and GhidrAssistMCP
- Runtime comparison on hardware capable of running the original game

## License

No license is currently offered. This is not a clean-room decompilation, and the
repository includes original game data for preservation purposes.
