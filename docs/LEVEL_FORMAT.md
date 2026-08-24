# TOU `.lev` v1.4 Format

This documents the format consumed by `Load_Level_File` and
`Load_Image_Data`. Integer fields are little-endian. The original executable
and shipped level files remain authoritative.

## File header

| Offset | Size | Meaning |
| ---: | ---: | --- |
| `0x00` | 19 | ASCII magic `TOU level file v1.4` |
| `0x13` | 3 | Unknown flags/padding |
| `0x16` | 4 | Offset of embedded JPEG |
| `0x1A` | 4 | Extra-section offset; purpose not yet recovered |
| `0x1E` | 4 | Offset of entity/RLE section |
| `0x22` | `0x39C` | Per-level configuration and tile-property bytes |

The JPEG occupies `[jpeg_offset, entity_offset)`. Its decoded dimensions are
the playable image dimensions. Runtime maps add a seven-pixel border on every
side and use the next power of two as their row stride.

## Entity section

The section begins with a signed 32-bit entity count followed by 20-byte
records. The loader copies each record verbatim and adds seven to its X and Y
coordinates.

| Record offset | Size | Known use |
| ---: | ---: | --- |
| `0x00` | 4 | X coordinate |
| `0x04` | 4 | Y coordinate |
| `0x08` | 1 | Direction/variant in several placement types |
| `0x09` | 1 | Team/palette parameter |
| `0x0A` | 1 | Placement/entity type |
| `0x0B` | 1 | Type-specific parameter |
| `0x0C` | 1 | Orientation/subtype |
| `0x0D` | 1 | Type-specific parameter |
| `0x0E` | 6 | Not independently named yet |

Do not turn the remaining bytes into a universal struct until every placement
type has been checked; the original reuses them by type.

```c
#pragma pack(push, 1)
struct LevHeader {
    char magic[19];
    uint8_t flags[3];
    uint32_t jpeg_offset;
    uint32_t extra_offset;
    uint32_t entity_offset;
};

struct LevPlacement {
    int32_t x;
    int32_t y;
    uint8_t parameters[12];
};
#pragma pack(pop)
```

For example, `desert.lev` begins:

```text
54 4F 55 20 6C 65 76 65 6C 20 66 69 6C 65 20 76 31 2E 34
0D 0A 1A  BE 03 00 00  5F 96 01 00  E3 48 02 00
```

That decodes to the v1.4 magic, flag bytes `0D 0A 1A`, JPEG offset
`0x000003BE`, extra offset `0x0001965F`, and entity offset `0x000248E3`.

## Tilemap RLE

RLE starts immediately after the entity records. Each run is two bytes:

```text
remap_index = byte0 >> 2
run_length  = (byte0 & 3) + byte1 * 4
```

`remap_index` selects the recovered 34-byte table in `level.cpp`. An index of
33 or greater terminates the stream. Runs fill rows left-to-right inside the
seven-pixel border. Runtime border cells are then forced to solid tile `5`.

## Related `.SWP` cache

`swap/<level>.SWP` stores a little-endian 32-bit width and height followed by
`width * height` 16-bit X1R5G5B5 pixels. The loader converts them to RGB565.
