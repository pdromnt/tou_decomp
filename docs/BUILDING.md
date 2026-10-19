# Building and Packaging

Run commands from the repository root (the local `decomp/` checkout), not from
the original game's directory one level above it. The original executables,
assets and tools outside the checkout are behavioral references, not build inputs.

## Layout

```text
.github/                 Build and release workflows
docs/                    Architecture, backlog, formats and manual checks
source/
  .build/                Ignored build caches, staging and release archives
  game/                  Gameplay, simulation, menus and game-facing rendering
  platform/              SDL input, controllers, audio, windowing and presentation
  level-editor/          Level library, compiler, visual editor and fixtures
  resources/             App icons, resource scripts and editable menu artwork
  third_party/           Vendored codecs/shaders and their licenses
  cmake/                 Dependency patches and generated-header templates
  scripts/               Build wrappers and release packaging
  tools/                 Asset inspection/extraction and localization utilities
CMakeLists.txt           Stable root build entry point
README.md
THIRD_PARTY_NOTICES.md
data/, levels/, etc.      Runtime assets; not moved into source/
```

## Windows

Install Visual Studio 2022's C++ build tools and a Windows SDK, plus CMake 3.24+
(the Visual Studio-bundled CMake is supported). Dependencies are pinned and
downloaded by CMake on first configuration.

```powershell
./source/scripts/build.ps1 -Architecture x64
```

`source/scripts/build.bat` forwards to that script, preferring
PowerShell 7 when available. They work from another working directory too.
Use `-Architecture Win32` or `-Architecture ARM64` for the other Windows targets;
the default build directories are `source/.build/x64/`, `source/.build/Win32/`
and `source/.build/ARM64/` (or `source/.build/native/` on Linux/macOS).
`-BuildDirectory`, `-Configuration`, `-Generator` and `-Parallel` are optional.
Close the game/editor before rebuilding; the wrapper does not kill processes.

Equivalent direct commands remain supported:

```powershell
cmake -S . -B source/.build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build source/.build/x64 --config Release --parallel 8
```

MSVC includes SDL's Windows Gaming Input and GameInput backends. MinGW-w64 can
still be used with a configured Ninja/Make toolchain, but older toolchains may
omit those backends. See [Gamepads](GAMEPADS.md) for the Xbox Bluetooth boundary.

## Linux and macOS

Use a native C/C++ toolchain, CMake 3.24+ and Ninja (or choose another generator).
Linux also needs SDL's audio/window-system development libraries; the current
dependency list is in `.github/workflows/build.yml`.

```sh
cmake -S . -B source/.build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build source/.build/native --parallel 8
```

The root entry point is intentional: configuring `source/` directly is not supported.
After a layout/toolchain change, prefer a fresh build directory rather than
reusing a cache created with a different generator or architecture.

## Outputs and releases

The local game output remains `TOU.exe`, `TOU` or `TOU.app` at the repository
root, beside the assets. Editor/compiler binaries go in root `level-editor/`;
that generated directory is ignored and is distinct from `source/level-editor/`.
Local settings, logs, build trees and packages remain untracked.
Build caches, staging and release archives live under ignored `source/.build/`;
they are disposable outputs, not shipping source. CI uses `source/.build/ci/`
and `source/.build/stage/`. Do not relocate an existing CMake cache: configure a
fresh directory instead, because caches contain absolute paths.

To stage and package a Windows x64 build:

```powershell
cmake --install source/.build/x64 --config Release --prefix source/.build/stage
./source/scripts/package-release.ps1 -Version local -Platform windows-x64 `
  -ExecutablePath ./source/.build/stage/TOU.exe -LevelEditorDirectory ./source/.build/stage/level-editor
```

Installation stages only the game, editor tools, runtime assets, focused editor
documentation and notices. It does not ship `source/`, build caches, settings or
the original reference tools. macOS assets/notices remain inside `TOU.app`.
Release archives are written to `source/.build/dist/`; Windows uses ZIP and Unix uses tar.gz.
The manual release workflow still builds all seven supported desktop targets.

## Developer utilities

```sh
python source/tools/validate_localization.py
python source/tools/generate_localization.py
python source/tools/inspect_level.py --help
```

The English generator reads `source/game/init.cpp` and writes `lang/en.json`;
translated catalogs and runtime fonts remain in `lang/` and `data/`.
Asset utilities default to the root asset directories; editable extracted menu
artwork lives in `source/resources/artwork/`. Run them from the repository root.
The translation bootstrap is a maintenance utility, not part of a normal build.
