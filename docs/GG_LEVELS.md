# Ground-Generated Levels

GG content comes in two related forms:

- A directory below `ggstuff` is a theme. The game discovers it directly and
  offers it as a fully dynamic GG entry whose dimensions and layout are chosen
  for each match.
- A custom GG `.lev` is a recipe. It stores dimensions, an attribute/constraint
  map, generator settings, a seed, and a theme name.

Neither form is a pre-rendered map. A normal level embeds its authored JPEG as
the final visible terrain; GG builds the visible terrain when the level loads.

## Authoring model

The editor canvas for a GG project is intentionally the constraint map. It can
define terrain behavior, water, authored repair blobs, and cyan sign markers,
but it is not a preview of the final artwork. The exported `.lev` main payload
contains only two little-endian 16-bit dimensions; the attribute map is stored
using the normal terrain RLE section.

The core settings are:

- Theme: a directory name below `ggstuff`.
- Generator shape: whether the authored attribute map follows shaped theme
  textures.
- Repair, stuff, and sign densities: whether/how densely the generator places
  those theme elements.
- Random seed: a fixed value reproduces the same random sequence. The legacy
  config's `*` form requests a different seed each play; the native editor
  currently authors explicit seeds only.

The original runtime loader seeds the generator from the level value, runs the
custom map in its `FUN_004143e0(0, 0)` constraint-map mode, then restores a
time-based gameplay RNG. A stored zero requests a tick-derived seed.

Manual repair regions are painted with the named repair attributes. Custom
signs use the cyan `GG sign location` attribute plus optional pairs of sign
text. The GG tab can create/remove up to 16 sign records and edit both 15-byte
legacy lines. The editor warns if sign markers exist without sign records.

## Theme anatomy

Every theme is a directory below `ggstuff` with an `info.txt`. The minimum useful
theme contains:

- `s1.tga`: a black/filled shape mask used to construct terrain.
- `t1.jpg`: the main ground texture.

Optional numbered assets add variety:

- `s2.tga`, ...: more filler shapes.
- `l1.tga`, ...: lawn/edge decoration.
- `d1.tga`, ...: repair-place artwork.
- `x1.tga`, ...: sign artwork.
- `p1.tga`, ...: decorative objects.
- `sd1.tga`, ...: shapes used below ground blocks.
- `px1.jpg`: theme parallax; otherwise the game derives it from the main
  texture.
- `t2.tga` and `t3.tga`: shaped-texture masks and border fine tuning.
- `ex1.jpg`: texture for explosive terrain.

Legacy theme TGAs are uncompressed 24-bit images. The runtime parses `info.txt`,
loads the available theme assets, selects the configured theme (or a random
fallback if it cannot be found), then runs either the standard cave/random-walk
generator or a beach-style generator. It subsequently applies water, lighting,
textures, decoration, entities, and pickups.

Use `Ctrl+N` in a GG editor project, or:

```text
tou-level new-theme path/to/ggstuff "my theme"
```

This creates the directory and an `info.txt` scaffold. It intentionally does
not synthesize art. Add `s1.tga` and `t1.jpg`; `Check` will reject an incomplete
theme before preview/export.

The original runtime string at `00475472` is `/GRASSATTR` (two `S` characters).
Several shipped
themes contain the historical `/GRASATTR` typo, which the original executable
silently ignores and masks with its default value; new themes should use the
correct spelling emitted by the scaffold.

## Runtime preview

Click `Preview` (or press `P`) to compile a temporary recipe and invoke TOU's
actual generator in a hidden process. The editor displays the game-produced
RGB565 terrain, so there is no approximate second generator. Continue painting
against the attribute layer; after any authored change, the preview is marked
stale until regenerated. A fixed seed makes repeated previews reproducible.
