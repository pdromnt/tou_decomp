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
tou-level new project.toulevel.json visual.jpg [parallax.jpg]
tou-level new-gg project.toulevel.json width height theme
tou-level new-theme path/to/ggstuff "theme name"
tou-level import input.lev project.toulevel.json
tou-level validate project.toulevel.json
tou-level build project.toulevel.json output.lev
tou-level inspect output.lev
tou-level compare original.lev output.lev
```

`new` reads the JPEG dimensions, writes a transparent attribute TGA beside the
project, and refuses to overwrite either file. Open the resulting project in
the editor and paint its terrain and placements.

`new-gg` creates a blank procedural-level project and its constraint/attribute
TGA. It does not create a fake visual JPEG: the game builds the visible terrain
from the selected `ggstuff` theme when the level is loaded.

`new-theme` creates a new theme directory and a documented `info.txt` scaffold.
Add at least `s1.tga` and `t1.jpg` before using the theme; those art assets are
deliberately left to an image editor.

`compare` reports raw differences separately from meaningful structural
differences, because legacy placement records contain three unused tail bytes.

## Editor

Launch `tou-level-editor` with no arguments to open the project launcher. It can
open an existing `.toulevel.json`, import a compiled `.lev`, create a normal
project from its visual JPEG and optional parallax, or create a GG project at a
common canvas size. Import extracts the embedded JPEG payloads and reconstructs
the attribute TGA beside the new JSON project. Passing a project path still
opens it directly.

- Use the Terrain, Objects, Rules, and GG tabs instead of cycling hidden modes.
- The toolbar exposes undo, redo, save, validated export, fit, layer visibility,
  selected value, brush size, validation, pairing, and real GG preview. The two
  context-sensitive value buttons say what they do: `Opacity`, `Edit`, `Choose`,
  `Clear`, or `Value`.
- The bottom control strip always shows the current brush size and the mouse
  controls for the active tab. New projects start with a practical 9-pixel
  brush; `Brush -` or `[` can reduce it to the precise 1-pixel minimum.
- Left mouse: paint the selected terrain attribute.
- Right mouse: pick an attribute from the map.
- Middle mouse: pan.
- Mouse wheel: zoom around the cursor.
- `[` / `]`: change brush size from 1 through 63 pixels.
- `-` / `=`: change attribute-layer opacity in Terrain, or the selected value
  in the other tabs.
- `V`: toggle the visual layer.
- `A`: toggle the attribute layer.
- `F`: fit the map in the window.
- `Ctrl+S`: save the attribute TGA and project.
- `Ctrl+E`: export a `.lev` beside the project.
- `Ctrl+Z` / `Ctrl+Y`: undo and redo up to 64 terrain or placement edits.
- `Ctrl+N`: scaffold a new GG theme while editing a GG project.
- `P`: regenerate a GG preview with the actual game runtime.
- `Tab`: cycle editor tabs.

In placement mode, choose a known object in the sidebar and click the map to
create it. Click an existing marker to select it, drag to move it, and press
Delete or Backspace to remove it. Number keys `1` through `5` select the shown
semantic placement property; Up and Down change it within the verified runtime
range. `+` and `-` provide the same adjustment. New teleports receive unique
numbers; select one, click `Pair`, then click its partner to cross-link them.
The `Linked` action toggles a gate's runtime-created second wall segment. The
editor warns before discarding unsaved work.

The Rules tab edits maker/email metadata and normal-level rules: parallax, civilians, bombing,
water color/running behavior, physics multipliers, ambience, and parallax
aftertouch. Select a row and adjust it with `+`/`-` or Up/Down. These changes
participate in the same undo history and are stored in the project JSON. Select
the parallax row to choose or clear its JPEG.

The GG tab edits the theme, shape mode, procedural densities, exact seed, and up
to 16 two-line sign records. Its canvas remains the editable constraint map;
`Preview`/`P` launches the actual TOU generator and overlays the resulting final
terrain. Any edit marks that preview stale. See
[`../docs/GG_LEVELS.md`](../docs/GG_LEVELS.md) for the split between authored
data, theme assets, and runtime generation.

The turret selector and map use the seven recovered projectile sprite families,
including their runtime team-color transform and direction frame. Gates preview
graphics 58/59 on the map at their runtime anchor and facing; selecting one also
draws its wall axis and exact labeled anchor.

`Check` reports malformed values, overlaps, missing teleport destinations,
gate-wall overflow, edge-clipped sprites, and missing assets. Export is blocked
on errors and asks for confirmation on warnings.
