# TOU `.SHP` Ship Format

Ship files live in `ships/` and are loaded by the recovered function at
original address `0x004249C0`. Fields are little-endian.

## Header and statistics

| Field | Size | Meaning |
| --- | ---: | --- |
| Version/header | 1 | Must be zero |
| Name | Variable | NUL-terminated, maximum 31 bytes |
| Statistics | 14 | Ship gameplay parameters copied into the 64-byte runtime stats record |

Known transformations of the 14 statistics bytes include:

- byte 4 is multiplied by `0.4`;
- byte 5 becomes maximum health via `value * 0x7D000`;
- byte 8 is shifted left by two;
- a zero byte 7 forces the transformed byte 8 value to one.

Several remaining statistic meanings are deliberately unnamed until checked
against the original menus and gameplay code.

The shipped `PERU.SHP` is a concrete example:

```text
00                                      header
42 61 73 69 63 20 54 4F 55 20 73 68
69 70 00                                "Basic TOU ship" + NUL
04 01 03 32 32 1E 32 00 01 02 01 05
04 00                                   14 statistic bytes
```

Its raw health byte is `0x1E`, producing runtime maximum health
`0x1E * 0x7D000` before global configuration scaling.

## Rotation frames

Exactly 32 frames follow the stats. Each frame contains:

| Field | Size | Meaning |
| --- | ---: | --- |
| Unknown metadata | 12 | Preserved only by the source file; loader skips it |
| Width | 2 | Pixel width |
| Height | 2 | Pixel height |
| Padding/unknown | 2 | Skipped |
| Pixels | `width * height * 3` | RGB24, ordered R/G/B |

Pixels enter the original X1B5G5R5/X1R5G5B5 recoloring path. Player color and
the ship's fixed base color are applied before the final framebuffer-compatible
RGB565 conversion.

## Runtime representation

Each selected player receives a 64-byte ship-stat record and a private set of
32 rotation frames. These runtime records are not serialized `.SHP` structs;
the loader expands and transforms the compact file fields.

Do not assume unused bytes are padding. Format extensions should be tested with
the original executable before changing the loader.
