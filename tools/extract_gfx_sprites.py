#!/usr/bin/env python3
"""Extract selected raw sprites from TOU's data/all3.gfx archive as PNGs."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


def parse_index(value: str) -> int:
    return int(value, 0)


def extract(archive: Path, output: Path, wanted: set[int]) -> list[tuple[int, int, int, int, Path]]:
    found: list[tuple[int, int, int, int, Path]] = []
    output.mkdir(parents=True, exist_ok=True)

    with archive.open("rb") as stream:
        while type_raw := stream.read(1):
            sprite_type = type_raw[0]
            index_raw = stream.read(2)
            metadata = stream.read(12)
            dimensions = stream.read(4)
            padding = stream.read(2)
            if len(index_raw) != 2 or len(metadata) != 12 or len(dimensions) != 4 or len(padding) != 2:
                raise ValueError("Truncated all3.gfx entry header")

            sprite_index = struct.unpack("<H", index_raw)[0]
            width, height = struct.unpack("<HH", dimensions)
            pixels = stream.read(width * height * 3)
            if len(pixels) != width * height * 3:
                raise ValueError(f"Truncated pixels for sprite 0x{sprite_index:X}")

            if sprite_index not in wanted:
                continue

            rgba = bytearray(width * height * 4)
            for pixel in range(width * height):
                blue, green, red = pixels[pixel * 3 : pixel * 3 + 3]
                alpha = 0 if red == 0 and green == 0 and blue == 0 else 255
                rgba[pixel * 4 : pixel * 4 + 4] = bytes((red, green, blue, alpha))

            destination = output / f"sprite-{sprite_index:03X}.png"
            Image.frombytes("RGBA", (width, height), bytes(rgba)).save(destination)
            found.append((sprite_index, sprite_type, width, height, destination))

    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("indices", nargs="+", type=parse_index, help="Sprite indices, decimal or 0xHEX")
    parser.add_argument("--archive", type=Path, default=Path("data/all3.gfx"))
    parser.add_argument("--output", type=Path, default=Path("artwork/extracted-sprites"))
    args = parser.parse_args()

    wanted = set(args.indices)
    found = extract(args.archive, args.output, wanted)
    found_indices = {entry[0] for entry in found}
    for index, sprite_type, width, height, destination in found:
        print(f"0x{index:03X}: type={sprite_type} size={width}x{height} -> {destination}")

    missing = sorted(wanted - found_indices)
    if missing:
        print("Missing: " + ", ".join(f"0x{index:X}" for index in missing))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
