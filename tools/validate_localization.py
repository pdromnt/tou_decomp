#!/usr/bin/env python3
"""Fail CI on malformed, stale, or incompatible localization catalogs."""

from __future__ import annotations

import json
import re
from pathlib import Path

from generate_localization import ROOT, read_english

CATALOGS = {"en": "en.json", "es": "es.json", "pt-BR": "pt-br.json", "fi": "fi.json"}


def main() -> None:
    expected_english = read_english()
    loaded: dict[str, dict[str, str]] = {}
    for locale, filename in CATALOGS.items():
        path = ROOT / "lang" / filename
        root = json.loads(path.read_text(encoding="utf-8"))
        if root.get("schemaVersion") != 1 or root.get("locale") != locale:
            raise SystemExit(f"{filename}: invalid schemaVersion or locale")
        strings = root.get("strings")
        if not isinstance(strings, dict) or any(
            not isinstance(key, str) or not isinstance(value, str) or not value
            for key, value in strings.items()
        ):
            raise SystemExit(f"{filename}: strings must be non-empty UTF-8 strings")
        loaded[locale] = strings

    if loaded["en"] != expected_english:
        raise SystemExit("en.json is stale; run tools/generate_localization.py")
    english_keys = set(loaded["en"])
    supported_glyphs = set(range(32, 127)) | {
        0xA1, 0xBF, 0xBD, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC7,
        0xC9, 0xCA, 0xCD, 0xD1, 0xD3, 0xD4, 0xD5, 0xD6, 0xDA, 0xDC,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE7, 0xE9, 0xEA, 0xED,
        0xF1, 0xF3, 0xF4, 0xF5, 0xF6, 0xFA, 0xFC,
    }
    for locale in ("es", "pt-BR", "fi"):
        missing = english_keys - set(loaded[locale])
        unknown = set(loaded[locale]) - english_keys
        if missing or unknown:
            raise SystemExit(f"{locale}: missing={sorted(missing)}, unknown={sorted(unknown)}")
        for key, value in loaded[locale].items():
            expected = re.findall(r"%(?:%|[-+0-9.]*[a-zA-Z])", loaded["en"][key])
            actual = re.findall(r"%(?:%|[-+0-9.]*[a-zA-Z])", value)
            if expected != actual:
                raise SystemExit(f"{locale}: format placeholders differ for {key}")
            unsupported = sorted({ord(character) for character in value
                                  if ord(character) not in supported_glyphs})
            if unsupported:
                raise SystemExit(f"{locale}: unsupported glyphs in {key}: {unsupported}")
    print(f"Validated {len(english_keys)} English keys and {len(CATALOGS) - 1} overlays")


if __name__ == "__main__":
    main()
