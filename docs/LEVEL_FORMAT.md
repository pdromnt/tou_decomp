# TOU `.lev` v1.4 Format

This documents the format written by the original `level converter.exe` and
consumed by TOU. Integers are little-endian. The original executable, converter
assembly, converter output, and shipped levels remain authoritative.

## Evidence

The original converter routines used for this reconstruction are:

| Address | Recovered purpose |
| --- | --- |
| `00401490` | Parse level text config |
| `00401DD0` | Build and write a `.lev` file |
| `00402580` | Load 24-bit uncompressed TGA |
| `00402660` | Convert attribute pixels, placements, and RLE |
| `00403650` | Integer power helper |
| `00403670` | Ceiling log2 helper |
| `00421540` | Original game payload, placement, and RLE loader |

Recompiling the checked-in `makelev/Jungle.*` sources with the original tool
produces a 157,417-byte file. It differs from shipped `levels/jungle.lev` at
only three unused bytes in its sole 20-byte placement record. All meaningful
header, config, JPEG, parallax, placement, and RLE bytes match.

## Source project

A normal level project uses one filename prefix:

- `<name>.txt`: configuration
- `<name>.jpg`: visible level image
- `<name>.tga`: 24-bit, uncompressed attribute map of identical dimensions
- `<name>p.jpg`: optional parallax image when `PARA` is enabled

A GG project uses the TGA as a procedural shape/attribute source. The output
does not embed the main JPG; it stores the TGA dimensions instead. The legacy
converter still probes `<name>.jpg` before parsing the config, so the old tool
requires that file even though a valid GG payload does not contain it. The help
documentation says GG levels do not use custom parallax; the container can
technically carry one if `PARA` is forced on.

The legacy config parser expects CRLF text. Its 22 recognized directives are:
`MAKER`, `EMAIL`, `PARA`, `CIVIL`, `BOMB`, `WATERC`, `DISABLERUN`, `GRAVITY`,
`RESISTANCE`, `COLLDAMAGE`, `BOUNCING`, `AMBIENT`, `PARALLAXAT`, `GGLEVEL`,
`GGTHEME`, `GGSHAPE`, `REPAIR`, `STUFFD`, `SIGND`, `RANDOMSEED`, `SIGNTEXT1`,
and `SIGNTEXT2`.

## File header

| Offset | Size | Meaning |
| ---: | ---: | --- |
| `0x00` | 19 | ASCII magic `TOU level file v1.4` |
| `0x13` | 3 | Fixed bytes `0D 0A 1A` |
| `0x16` | 4 | Main payload offset; always `0x3BE` from the original writer |
| `0x1A` | 4 | Optional parallax payload offset |
| `0x1E` | 4 | Placement/RLE section offset |
| `0x22` | `0x39C` | Packed per-level configuration |

For a normal level, the main JPEG occupies `[main_offset, parallax_offset)`.
The optional parallax JPEG occupies `[parallax_offset, placement_offset)`; the
two offsets are equal when no parallax is embedded.

For a GG level, the main payload is exactly four bytes:

```c
uint16_t attribute_width;
uint16_t attribute_height;
```

The original writer then uses the same optional-parallax and placement offsets.

## Configuration block

The 924-byte block at file offset `0x22` is a packed snapshot of the converter's
configuration state.

| Relative offset | Size | Directive / meaning |
| ---: | ---: | --- |
| `0x000` | `0x80` | `MAKER`, NUL-terminated legacy byte string |
| `0x080` | `0x80` | `EMAIL`, NUL-terminated legacy byte string |
| `0x100` | 1 | `PARA`, boolean |
| `0x101` | 1 | `CIVIL`, 0..100 |
| `0x102` | 1 | `BOMB`, 0..100 |
| `0x103` | 3 | `WATERC` red, green, blue |
| `0x106` | 1 | `DISABLERUN`, boolean |
| `0x107` | 1 | `GRAVITY / 10` |
| `0x108` | 1 | `RESISTANCE / 10` |
| `0x109` | 1 | `COLLDAMAGE / 10` |
| `0x10A` | 1 | `BOUNCING / 10` |
| `0x10B` | 1 | `AMBIENT` |
| `0x10C` | 1 | `PARALLAXAT` |
| `0x10D` | 1 | `GGLEVEL`, boolean |
| `0x10E` | `0x80` | `GGTHEME`, NUL-terminated legacy byte string |
| `0x18E` | 1 | `GGSHAPE`, boolean |
| `0x18F` | 1 | `REPAIR` density |
| `0x190` | 1 | `STUFFD` density |
| `0x191` | 1 | `SIGND` density |
| `0x192` | 2 | Alignment/reserved, normally zero |
| `0x194` | 4 | `RANDOMSEED`; `*` selects runtime-random mode |
| `0x198` | `0x200` | Up to 16 sign-text records, 32 bytes each |
| `0x398` | 1 | Sign-text record count |
| `0x399` | 3 | Reserved/padding |

`SIGNTEXT1` occupies the first 16-byte half of a record. `SIGNTEXT2` splits its
two comma-separated lines across both 16-byte halves. The old writer accepts at
most 16 records.

## Attribute palette

Attribute pixels must match exact RGB values. The complete named table lives in
[`LEVEL_PALETTE.json`](LEVEL_PALETTE.json). Index 33 (cyan) decodes to runtime
tile `0xFF`, which GG generation consumes as sign-placement metadata rather than
ordinary terrain. `colors.png` is the original visual reference.

The converter first extracts object-marker pixels into placement records, then
replaces/clears those pixels before terrain RLE. Marker families use the blue
channel as a discriminator:

| Blue | Placement | Packed red/green channels |
| ---: | --- | --- |
| 230 | Turret | `R=(armor-1)<<4 | (style-1)`; `G=direction<<2 | (team-1)` |
| 120 | Gate | `R=linked<<7 | (style-1)`; `G=(team-1)<<4 | (graphics-1)<<2 | (direction-1)` |
| 180 | Object | `R=type`; `G` is type-specific |
| 140 | Starting place | `R=team-1`, `G=0` |
| 190 | Teleport | `R=(team-1)<<6 | (number-1)`; `G=target-1` |

Turret styles entered as 1..7 are stored using the original remap
`[0, 1, 5, 2, 3, 4, 6]`. Directions are 0..31, with 32 meaning random.

Blue-180 object types are:

| Red | Object | Green packing |
| ---: | --- | --- |
| 0 | Smoke generator | `(density-1)<<5 | (size-1)<<3 | (direction-1)` |
| 1 | Flame generator | zero |
| 2 | Mine | `style-1` |
| 3 | Base building | `team-1` |

The original converter caps placement output at 1,024 records. Each input
marker is one exact pixel; antialiasing or a soft brush creates illegal colors.

## Placement section

The section begins with a signed 32-bit count followed by 20-byte records. The
runtime copies each record and adds seven to X and Y for its map border.

```c
#pragma pack(push, 1)
struct LevPlacement {
    int32_t x;
    int32_t y;
    uint8_t parameters[12];
};
#pragma pack(pop)
```

| Record offset | Size | Meaning |
| ---: | ---: | --- |
| `0x00` | 4 | X coordinate |
| `0x04` | 4 | Y coordinate |
| `0x08` | 1 | Type: turret 0, gate 1, object 2, start 3, teleport 4 |
| `0x09`..`0x0D` | 5 | Type-specific decoded parameters |
| `0x0E`..`0x13` | 6 | Unused by the recovered writer; may contain stale heap bytes |

Do not byte-compare the unused tail as meaningful state. The shipped Jungle
file contains zeroes there while a modern run of the original converter leaked
three otherwise irrelevant bytes from its allocation.

## Tilemap RLE

RLE begins immediately after the placement records. Each run is two bytes:

```text
palette_index = byte0 >> 2
run_length    = (byte0 & 3) + byte1 * 4
```

Runs are emitted on a color change or at the maximum length `0x3FF`. The stream
ends when the next run's first byte is the literal `FF`; the writer emits
`FF FF`. Palette index 33 is valid (`0x84`..`0x87` first byte depending on run
length) and must not be confused with that terminator. Runtime tiles are filled
left-to-right, top-to-bottom inside the seven-pixel border and remapped through
the recovered table in `level.cpp`.

## Related `.swp` cache

`swap/<level>.swp` stores a little-endian 32-bit width and height followed by
`width * height` 16-bit X1R5G5B5 pixels. The loader converts them to RGB565.

## Still to verify

- Exact runtime interpretation of every placement parameter, independently of
  the converter's packing.
- The original runtime's handling of a deliberately forced GG parallax payload.
