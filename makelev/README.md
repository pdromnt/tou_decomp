# TOU Level Tools

This directory is the maintainable replacement for the original `makelev`
utilities. The historical executables remain outside the repository in the
workspace-level `makelev` directory and are behavioral oracles, not build
dependencies.

## What exists now

- `tou_level`: typed C++ library for project files and `.lev` v1.4 files.
- `tou-level`: validation, compilation, inspection, and comparison CLI.
- `tou-level-editor`: SDL3 terrain/attribute painter and export preview.
- `fixtures/jungle`: original Jungle source assets and a modern project file.
- `tests`: golden checks against the shipped Jungle level and format round trips.

The Jungle project currently compiles byte-for-byte identically to the shipped
`levels/jungle.lev`. Generated padding is deterministic instead of inheriting
the original converter's uninitialized bytes.

## Build

Configure the repository normally, then build one or all of these targets:

```text
cmake --build build --target tou-level tou-level-editor tou-level-tests
```

The interactive tools are written to `makelev/bin`.

## CLI

```text
tou-level validate project.toulevel.json
tou-level build project.toulevel.json output.lev
tou-level inspect output.lev
tou-level compare original.lev output.lev
```

`compare` reports raw differences separately from meaningful structural
differences, because legacy placement records contain three unused tail bytes.

## Editor foundation

Launch `tou-level-editor` with a `.toulevel.json` path. With no argument, a
developer build opens the Jungle fixture.

- Left mouse: paint the selected terrain attribute.
- Right mouse: pick an attribute from the map.
- Middle mouse: pan.
- Mouse wheel: zoom around the cursor.
- `[` / `]`: change brush size.
- `-` / `=`: change attribute-layer opacity.
- `V`: toggle the visual layer.
- `A`: toggle the attribute layer.
- `F`: fit the map in the window.
- `Ctrl+S`: save the attribute TGA and project.
- `Ctrl+E`: export a `.lev` beside the project.

The editor intentionally starts with the format-safe core and terrain painter.
Placement manipulation, metadata panels, project creation, undo/redo, and
in-game preview remain the next visual-editor work rather than being guessed
into the file format.
