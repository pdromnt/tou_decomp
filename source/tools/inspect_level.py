#!/usr/bin/env python3
"""Inspect a TOU v1.4 .lev file without loading the game."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


MAGIC = b"TOU level file v1.4"
HEADER_SIZE = 0x22
CONFIG_SIZE = 0x39C
PLACEMENT_SIZE = 20


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def c_string(data: bytes) -> str:
    return data.split(b"\0", 1)[0].decode("latin-1", errors="replace")


def jpeg_dimensions(data: bytes) -> tuple[int, int] | None:
    if not data.startswith(b"\xff\xd8"):
        return None
    pos = 2
    sof = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
        0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF,
    }
    while pos + 4 <= len(data):
        if data[pos] != 0xFF:
            pos += 1
            continue
        while pos < len(data) and data[pos] == 0xFF:
            pos += 1
        if pos >= len(data):
            break
        marker = data[pos]
        pos += 1
        if marker in {0xD8, 0xD9} or 0xD0 <= marker <= 0xD7:
            continue
        if pos + 2 > len(data):
            break
        length = int.from_bytes(data[pos:pos + 2], "big")
        if length < 2 or pos + length > len(data):
            break
        if marker in sof and length >= 7:
            height = int.from_bytes(data[pos + 3:pos + 5], "big")
            width = int.from_bytes(data[pos + 5:pos + 7], "big")
            return width, height
        pos += length
    return None


def inspect(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE + CONFIG_SIZE:
        raise ValueError("file is too small for a v1.4 level")
    if data[:len(MAGIC)] != MAGIC or data[0x13:0x16] != b"\r\n\x1a":
        raise ValueError("wrong TOU v1.4 header")

    main_offset, parallax_offset, placement_offset = struct.unpack_from("<III", data, 0x16)
    if not (HEADER_SIZE + CONFIG_SIZE <= main_offset <= parallax_offset <= placement_offset < len(data)):
        raise ValueError("invalid section offsets")

    cfg = data[HEADER_SIZE:HEADER_SIZE + CONFIG_SIZE]
    gg = bool(cfg[0x10D])
    main = data[main_offset:parallax_offset]
    if gg:
        if len(main) != 4:
            raise ValueError(f"GG main payload is {len(main)} bytes, expected 4")
        dimensions = [u16(main, 0), u16(main, 2)]
    else:
        found = jpeg_dimensions(main)
        dimensions = list(found) if found else None

    count = i32(data, placement_offset)
    if count < 0 or count > 1024:
        raise ValueError(f"invalid placement count {count}")
    placement_end = placement_offset + 4 + count * PLACEMENT_SIZE
    if placement_end > len(data):
        raise ValueError("placement records run past end of file")

    placements = []
    for index in range(count):
        start = placement_offset + 4 + index * PLACEMENT_SIZE
        record = data[start:start + PLACEMENT_SIZE]
        placements.append({
            "index": index,
            "x": i32(record, 0),
            "y": i32(record, 4),
            "type": record[8],
            "parameters": list(record[9:14]),
            "unused_tail_hex": record[14:20].hex(),
        })

    rle = data[placement_end:]
    decoded_pixels = 0
    run_count = 0
    terminator = None
    for offset in range(0, len(rle) - 1, 2):
        b0, b1 = rle[offset], rle[offset + 1]
        if b0 == 0xFF:
            terminator = {
                "offset": placement_end + offset,
                "bytes": f"{b0:02x}{b1:02x}",
            }
            break
        palette_index = b0 >> 2
        if palette_index > 33:
            raise ValueError(f"invalid RLE palette index {palette_index}")
        decoded_pixels += (b0 & 3) + b1 * 4
        run_count += 1

    if terminator is None:
        raise ValueError("RLE terminator was not found")

    expected_pixels = dimensions[0] * dimensions[1] if dimensions else None
    return {
        "path": str(path),
        "size": len(data),
        "offsets": {
            "main": main_offset,
            "parallax": parallax_offset,
            "placements": placement_offset,
        },
        "payload": {
            "kind": "gg_dimensions" if gg else "jpeg",
            "dimensions": dimensions,
            "main_bytes": len(main),
            "parallax_bytes": placement_offset - parallax_offset,
        },
        "config": {
            "maker": c_string(cfg[0x000:0x080]),
            "email": c_string(cfg[0x080:0x100]),
            "parallax": bool(cfg[0x100]),
            "civilians": cfg[0x101],
            "bombing": cfg[0x102],
            "water_rgb": list(cfg[0x103:0x106]),
            "disable_running_water": bool(cfg[0x106]),
            "gravity_tenths": cfg[0x107],
            "resistance_tenths": cfg[0x108],
            "collision_damage_tenths": cfg[0x109],
            "bouncing_tenths": cfg[0x10A],
            "ambient": cfg[0x10B],
            "parallax_aftertouch": cfg[0x10C],
            "gg_level": gg,
            "gg_theme": c_string(cfg[0x10E:0x18E]),
            "gg_shape": bool(cfg[0x18E]),
            "repair_density": cfg[0x18F],
            "stuff_density": cfg[0x190],
            "sign_density": cfg[0x191],
            "random_seed": struct.unpack_from("<I", cfg, 0x194)[0],
            "sign_text_count": cfg[0x398],
        },
        "placements": placements,
        "rle": {
            "encoded_bytes_through_terminator": terminator["offset"] + 2 - placement_end,
            "run_count": run_count,
            "decoded_pixels": decoded_pixels,
            "expected_pixels": expected_pixels,
            "pixel_count_matches": expected_pixels == decoded_pixels if expected_pixels is not None else None,
            "terminator": terminator,
        },
    }


def compare(left: Path, right: Path) -> dict:
    a, b = left.read_bytes(), right.read_bytes()
    differences = [index for index, pair in enumerate(zip(a, b)) if pair[0] != pair[1]]
    return {
        "other": str(right),
        "same_size": len(a) == len(b),
        "byte_differences": len(differences) + abs(len(a) - len(b)),
        "first_differing_offsets": differences[:32],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("level", type=Path)
    parser.add_argument("--compare", type=Path)
    args = parser.parse_args()

    result = inspect(args.level)
    if args.compare:
        result["comparison"] = compare(args.level, args.compare)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
