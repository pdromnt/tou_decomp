# TOU `all3.gfx` Sprite Format

`data/all3.gfx` is a stream of entries with no separate file header. All
multi-byte values are little-endian.

| Entry field | Size | Meaning |
| --- | ---: | --- |
| Type | 1 | Pixel conversion/variant mode |
| Sprite index | 2 | Destination frame index |
| Unknown metadata | 12 | Currently skipped by the runtime |
| Width | 2 | Pixel width |
| Height | 2 | Pixel height |
| Unknown/padding | 2 | Currently skipped |
| Pixels | `width * height * 3` | Packed BGR24 pixels |

Entries continue until EOF. Width and height are retained as bytes by the
original runtime, so shipped sprites must fit in 255 pixels per dimension.

## Entry types

- Types `0` and `1`: BGR24 is converted directly to the RGB565 color atlas.
- Type `9`: pixels first enter the original X1R5G5B5 team-color pipeline;
  four tinted variants are generated, then converted to RGB565.
- Types `2`, `3`, and `4`: each pixel becomes an eight-bit grayscale value
  using `(B + G + R) / 3` and is stored in the mask atlas.
- Unknown types are parsed but currently contribute no pixels.

The runtime keeps separate tables for frame pixel offset, width, and height.
Color pixels and grayscale masks also have independent cursors.

## Minimal loader outline

```text
while read(type):
    index = read_u16()
    skip(12)
    width, height = read_u16(), read_u16()
    skip(2)
    pixels = read(width * height * 3)
    decode pixels according to type
```

The shipped file and `FUN_00423150` at original address `0x00423150` are the
format oracle.

