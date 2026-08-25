# RE/Decompilation of Tunnels of Underworld

<img width="321" height="252" alt="Tunnels of Underworld gameplay" src="https://github.com/user-attachments/assets/d9d58472-6413-4061-baef-b6deca4c8dd8" />
<img width="321" height="252" alt="Tunnels of Underworld gameplay" src="https://github.com/user-attachments/assets/b1d9f5a9-fc8d-4ef4-9b14-34e09aa18163" />

This repository contains a behavior-focused reconstruction of **Tunnels of
Underworld** (TOU), a 2002 Windows game created by
[Hannu Kankaanpää](https://github.com/hannukp) and the rest of GigaMess.

The original source appears to be lost. This project was recovered through
runtime comparison, disassembly, Ghidra, and an intentionally ugly-first static
reconstruction. The original executable remains the authority for gameplay.
I do not claim ownership of the original game or its assets.

The project has no official Discord server or subreddit. Please use the
repository's Issues and Pull Requests for collaboration.

## Current Status

The major gameplay-parity pass is complete. Weapons and Marks, enemy ships,
turrets, physics, menus, controls, levels, audio, particles, match statistics,
and effects have received hands-on comparison against the original game.

The modern runtime now provides:

- native Windows, Linux, and macOS builds for x86, x64, and ARM64 as applicable;
- SDL3 windowing, input, software-framebuffer presentation, and SDL_mixer audio;
- windowed/fullscreen display modes and immediate audio controls;
- English, Spanish, Brazilian Portuguese, and Finnish user interfaces;
- human-readable `settings.json` configuration;
- a visual normal/GG level editor and native level compiler; and
- direct-IP LAN deathmatch for two to four computers without split screen.

The source still looks like a decompilation. Original addresses, raw offsets,
and Ghidra-style names remain where changing them without binary evidence could
alter behavior. See [CODEBASE.md](CODEBASE.md) before refactoring,
[BACKLOG.md](BACKLOG.md) for actual future work, and [docs/](docs/) for the
recovered formats and subsystem notes.

## Running a Release

Download the archive matching your operating system and architecture, then
extract it completely. Run `TOU.exe` on Windows, `TOU` on Linux, or `TOU.app`
on macOS. Keep the executable/app with its packaged asset directories.

The game writes `settings.json` beside its assets. In a macOS package that is
`TOU.app/Contents/Resources`. The original default controls were designed for a
Windows keyboard; MacBook users should remap them under **Options → Controls**.
Right Option and Right Command are practical primary-action choices.

The reconstructed game identifies itself as
`Tunnels of Underworld - RE/Decompiled - v0.7` so it cannot be confused with
the original executable.

### LAN deathmatch

Open **Team Deathmatch → LAN Deathmatch**. One computer hosts and the other one
to three players connect by hostname/IP and TCP port (default `27015`). Every
computer must use the same game build, gameplay assets, ships, levels, and GG
themes. There is no AI, discovery, reconnect, relay, or public lobby in this
beta. See [docs/LAN_BETA.md](docs/LAN_BETA.md) for the full flow and diagnostics.

## Level Editor

Release archives include a separate `level-editor/` directory containing:

- `tou-level-editor`, the visual normal/GG project editor; and
- `tou-level-compiler`, the command-line compiler and inspector.

Start with [makelev/README.md](makelev/README.md) or the packaged HTML
[level-making guide](help/toudoc_makelev.htm). The recovered binary format is
documented in [docs/LEVEL_FORMAT.md](docs/LEVEL_FORMAT.md).

## Building

Requirements:

- a C/C++ compiler for the target platform;
- CMake 3.24 or newer; and
- Ninja or GNU Make.

The build fetches pinned SDL3 and SDL_mixer releases and links them statically:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
```

The output is `TOU.exe` on Windows, `TOU` on Linux, or `TOU.app` on macOS.
Windows packages do not require SDL DLLs or the Visual C++ redistributable. The
32-bit MinGW build remains the closest host to the original executable; native
x64 and ARM64 builds are also supported.

The `Build` GitHub Actions workflow runs on pushes and pull requests. It builds
Windows x86, Windows/Linux x64 and ARM64, and macOS Intel and Apple Silicon,
validates localization, checks executable architecture, and stages the full
runtime. It never publishes a release.

To create the Windows x64 archive layout used by CI:

```powershell
cmake --install build --config Release --prefix stage
./scripts/package-release.ps1 -Version local -Platform windows-x64 `
  -ExecutablePath ./stage/TOU.exe `
  -LevelEditorDirectory ./stage/level-editor
```

## Releases

The `Build release` workflow runs only through manual dispatch. Supply the
release tag/version when choosing **Run workflow**. It builds seven desktop
packages and creates or updates that GitHub Release. If the tag already exists,
the workflow builds the tagged commit; otherwise it creates the tag at the
selected commit. Ordinary commits and tag pushes do not publish releases.

Windows packages use ZIP. Linux and macOS packages use `.tar.gz` so executable
permissions survive extraction.

## Future Possibilities

- Gamepad support.
- A simplified, expansion-friendly campaign using newly authored maps.
- LAN discovery, reconnect, latency hiding, AI, and internet-friendly transport.
- Additional community-maintained translations.
- Optional GPU presentation/post-processing behind the authoritative software
  framebuffer.
- Browser/WebAssembly after native desktop development remains stable.

## Contributing

Behavior changes need evidence from the original executable. Refactors must
keep the x86 parity build and native builds working without accidentally
changing fixed-point arithmetic, RNG order, callback dispatch, or update order.
A successful build is not gameplay acceptance.

Useful diagnostic workflows are documented in
[docs/REPLAY_DIAGNOSTICS.md](docs/REPLAY_DIAGNOSTICS.md) and
[docs/SIMULATION_STATE.md](docs/SIMULATION_STATE.md).

## Tools Used

- MinGW-w64
- Ghidra and GhidrAssistMCP
- Runtime comparison on hardware capable of running the original game

## License

No license is currently offered. This is not a clean-room decompilation, and the
repository includes original game data for preservation purposes.
