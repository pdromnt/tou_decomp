#!/usr/bin/env python3
"""Build supplemental Latin glyph atlases from TOU's original bitmap fonts."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
FONT_FILES = ("f_large.tga", "f_mini.tga", "f_med.tga", "f_tiny5d.tga")
ORIGINAL_ORDER = list("ABCDEFGHIJKLMNOPQRSTUVWXYZ") + ["Å", "Ä", "Ö"] + \
    list("abcdefghijklmnopqrstuvwxyz") + ["å", "ä", "ö"] + list("0123456789") + \
    list(".!?+-:;()/\\$%&'<>½\"#[]_,=*@")
TARGETS = (
    "ÀÁÂÃÇÉÊÍÑÓÔÕÚÜ"
    "àáâãçéêíñóôõúü"
    "¡¿"
)
COMPOSE = {
    "À": ("A", "grave"), "Á": ("A", "acute"), "Â": ("A", "circumflex"), "Ã": ("A", "tilde"),
    "Ç": ("C", "cedilla"), "É": ("E", "acute"), "Ê": ("E", "circumflex"), "Í": ("I", "acute"),
    "Ñ": ("N", "tilde"), "Ó": ("O", "acute"), "Ô": ("O", "circumflex"), "Õ": ("O", "tilde"),
    "Ú": ("U", "acute"), "Ü": ("U", "diaeresis"),
    "à": ("a", "grave"), "á": ("a", "acute"), "â": ("a", "circumflex"), "ã": ("a", "tilde"),
    "ç": ("c", "cedilla"), "é": ("e", "acute"), "ê": ("e", "circumflex"), "í": ("i", "acute"),
    "ñ": ("n", "tilde"), "ó": ("o", "acute"), "ô": ("o", "circumflex"), "õ": ("o", "tilde"),
    "ú": ("u", "acute"), "ü": ("u", "diaeresis"),
}


def segments(image: Image.Image) -> list[Image.Image]:
    rgb = image.convert("RGB")
    found: list[Image.Image] = []
    start = None
    for x in range(rgb.width):
        occupied = rgb.crop((x, 0, x + 1, rgb.height)).getbbox() is not None
        if occupied and start is None:
            start = x
        elif not occupied and start is not None:
            found.append(rgb.crop((start, 0, x, rgb.height)))
            start = None
    if start is not None:
        found.append(rgb.crop((start, 0, rgb.width, rgb.height)))
    return found


def accent(draw: ImageDraw.ImageDraw, width: int, top: int, kind: str, scale: int) -> None:
    center = width // 2
    y = max(0, top - (4 if scale > 1 else 3))
    ink = (255, 255, 255)
    if kind == "acute":
        draw.line((center - scale, y + 2 * scale, center + scale, y), fill=ink, width=scale)
    elif kind == "grave":
        draw.line((center - scale, y, center + scale, y + 2 * scale), fill=ink, width=scale)
    elif kind == "circumflex":
        draw.line((center - 2 * scale, y + scale, center, y), fill=ink, width=scale)
        draw.line((center, y, center + 2 * scale, y + scale), fill=ink, width=scale)
    elif kind == "tilde":
        draw.line((center - 2 * scale, y + scale, center - scale, y), fill=ink, width=scale)
        draw.line((center - scale, y, center + scale, y + scale), fill=ink, width=scale)
        draw.line((center + scale, y + scale, center + 2 * scale, y), fill=ink, width=scale)
    elif kind == "diaeresis":
        radius = max(1, scale)
        for x in (center - 2 * scale, center + 2 * scale):
            draw.rectangle((x - radius // 2, y, x + radius // 2, y + radius - 1), fill=ink)


def composed(base: Image.Image, kind: str) -> Image.Image:
    glyph = base.copy()
    bbox = glyph.getbbox()
    if not bbox:
        return glyph
    scale = 2 if glyph.height >= 24 else 1
    draw = ImageDraw.Draw(glyph)
    if kind == "cedilla":
        center = glyph.width // 2
        y = min(glyph.height - 2, bbox[3])
        draw.line((center, y, center - scale, min(glyph.height - 1, y + 2 * scale)),
                  fill=(255, 255, 255), width=scale)
    else:
        accent(draw, glyph.width, bbox[1], kind, scale)
    return glyph


def build(path: Path) -> None:
    source = Image.open(path).convert("RGB")
    glyphs = segments(source)
    if len(glyphs) != len(ORIGINAL_ORDER):
        raise RuntimeError(f"{path}: expected {len(ORIGINAL_ORDER)} glyphs, found {len(glyphs)}")
    originals = dict(zip(ORIGINAL_ORDER, glyphs))
    output: list[Image.Image] = []
    for character in TARGETS:
        if character == "¡":
            output.append(originals["!"].transpose(Image.Transpose.ROTATE_180))
        elif character == "¿":
            output.append(originals["?"].transpose(Image.Transpose.ROTATE_180))
        else:
            base, mark = COMPOSE[character]
            output.append(composed(originals[base], mark))

    width = sum(glyph.width + 1 for glyph in output)
    atlas = Image.new("RGB", (width, source.height))
    x = 0
    for glyph in output:
        atlas.paste(glyph, (x, 0))
        x += glyph.width + 1
    destination = path.with_name(path.stem + "_latin.tga")
    atlas.save(destination)
    print(f"{destination.relative_to(ROOT)}: {atlas.width}x{atlas.height}, {len(output)} glyphs")


def main() -> None:
    for name in FONT_FILES:
        build(ROOT / "data" / name)


if __name__ == "__main__":
    main()
