#!/usr/bin/env python3
"""Replace same-sized sprites in TOU's data/all3.gfx archive from PNG files."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


def parse_index(value: str) -> int:
    return int(value, 0)


def find_entries(data: bytearray) -> dict[int, tuple[int, int, int]]:
    entries: dict[int, tuple[int, int, int]] = {}
    offset = 0
    while offset < len(data):
        if offset + 21 > len(data):
            raise ValueError("Truncated all3.gfx entry header")
        sprite_index = struct.unpack_from("<H", data, offset + 1)[0]
        width, height = struct.unpack_from("<HH", data, offset + 15)
        pixel_offset = offset + 21
        pixel_bytes = width * height * 3
        if pixel_offset + pixel_bytes > len(data):
            raise ValueError(f"Truncated pixels for sprite 0x{sprite_index:X}")
        entries[sprite_index] = (pixel_offset, width, height)
        offset = pixel_offset + pixel_bytes
    return entries


def png_to_bgr(path: Path, expected_size: tuple[int, int]) -> bytes:
    with Image.open(path) as source:
        image = source.convert("RGBA")
    if image.size != expected_size:
        raise ValueError(
            f"{path} is {image.size[0]}x{image.size[1]}, expected "
            f"{expected_size[0]}x{expected_size[1]}"
        )

    output = bytearray(image.width * image.height * 3)
    for index, (red, green, blue, alpha) in enumerate(image.getdata()):
        # The legacy atlas has color-key transparency but no alpha channel.
        # Premultiplication retains Photoshop's softened dark edges without
        # turning transparent RGB data into visible halos.
        red = (red * alpha + 127) // 255
        green = (green * alpha + 127) // 255
        blue = (blue * alpha + 127) // 255
        output[index * 3 : index * 3 + 3] = bytes((blue, green, red))
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("indices", nargs="+", type=parse_index)
    parser.add_argument("--archive", type=Path, default=Path("data/all3.gfx"))
    parser.add_argument("--input", type=Path, default=Path("artwork/menu-sprites"))
    args = parser.parse_args()

    data = bytearray(args.archive.read_bytes())
    entries = find_entries(data)
    for sprite_index in args.indices:
        if sprite_index not in entries:
            raise ValueError(f"Sprite 0x{sprite_index:X} is not in {args.archive}")
        pixel_offset, width, height = entries[sprite_index]
        source = args.input / f"sprite-{sprite_index:03X}.png"
        pixels = png_to_bgr(source, (width, height))
        data[pixel_offset : pixel_offset + len(pixels)] = pixels
        print(f"0x{sprite_index:03X}: {source} -> {width}x{height}")

    args.archive.write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
