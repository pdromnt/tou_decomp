#!/usr/bin/env python3
"""Fail CI on malformed, stale, or incompatible localization catalogs."""

from __future__ import annotations

import json
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
    for locale in ("es", "pt-BR", "fi"):
        unknown = set(loaded[locale]) - english_keys
        if unknown:
            raise SystemExit(f"{locale}: unknown keys: {sorted(unknown)}")
    print(f"Validated {len(english_keys)} English keys and {len(CATALOGS) - 1} overlays")


if __name__ == "__main__":
    main()
