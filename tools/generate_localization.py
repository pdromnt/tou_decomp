#!/usr/bin/env python3
"""Generate TOU's English catalog and starter translation overlays.

The legacy menu table remains the compatibility source while it is gradually
replaced with direct Text_Get() calls. Keys include both the stable numeric
slot and a readable slug so translators can work without reading C++.
"""

from __future__ import annotations

import ast
import json
import re
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "init.cpp"
OUTPUT = ROOT / "lang"

ASSIGNMENT = re.compile(
    r'^\s*g_MenuStrings\[(0x[0-9A-Fa-f]+|\d+)\]\s*=\s*\(char \*\)"((?:\\.|[^"\\])*)";'
)
DYNAMIC_SLOTS = {0x65, *range(0x71, 0x8C), *range(0x149, 0x14D)}


def decode_c_string(value: str) -> str:
    raw = ast.literal_eval(f'b"{value}"')
    return raw.decode("utf-8") if raw.startswith((b"\xc2", b"\xc3")) else raw.decode("latin-1")


def slug(value: str) -> str:
    plain = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    result = re.sub(r"[^a-z0-9]+", "_", plain.lower()).strip("_")
    return result[:48] or "text"


def read_english() -> dict[str, str]:
    strings: dict[str, str] = {}
    for line in SOURCE.read_text(encoding="utf-8").splitlines():
        match = ASSIGNMENT.match(line)
        if not match:
            continue
        index = int(match.group(1), 0)
        if index in DYNAMIC_SLOTS:
            continue
        value = decode_c_string(match.group(2))
        strings[f"menu.{index:03d}.{slug(value)}"] = value
    return dict(sorted(strings.items()))


def translated(english: dict[str, str], values: dict[int, str]) -> dict[str, str]:
    by_index = {int(key.split(".")[1]): key for key in english if key.startswith("menu.")}
    return {by_index[index]: text for index, text in values.items() if index in by_index}


def write_catalog(code: str, name: str, strings: dict[str, str]) -> None:
    payload = {"schemaVersion": 1, "locale": code, "name": name, "strings": strings}
    filename = "pt-br" if code == "pt-BR" else code
    path = OUTPUT / f"{filename}.json"
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    OUTPUT.mkdir(exist_ok=True)
    english = read_english()
    write_catalog("en", "English", english)
    write_catalog("es", "Español", translated(english, {
        1: "Combate a muerte por equipos", 2: "Niveles", 3: "Jugadores",
        4: "Opciones", 5: "Créditos", 6: "Salir del juego",
        8: "Menú de opciones", 9: "Audio", 10: "Detalles y gráficos",
        11: "Eventos", 12: "Reglas principales", 13: "Varios",
        14: "Prohibir armas", 15: "Atrás", 0x147: "Restablecer valores",
        0x148: "Valores restablecidos.", 0x14D: "Modo",
        0x14E: "Ventana", 0x14F: "Pantalla completa", 0x150: "Idioma",
    }))
    write_catalog("pt-BR", "Português (Brasil)", translated(english, {
        1: "Mata-mata em equipe", 2: "Fases", 3: "Jogadores",
        4: "Opções", 5: "Créditos", 6: "Sair do jogo",
        8: "Menu de opções", 9: "Áudio", 10: "Detalhes e gráficos",
        11: "Eventos", 12: "Regras principais", 13: "Diversos",
        14: "Banir armas", 15: "Voltar", 0x147: "Restaurar padrões",
        0x148: "Padrões restaurados.", 0x14D: "Modo",
        0x14E: "Janela", 0x14F: "Tela cheia", 0x150: "Idioma",
    }))
    write_catalog("fi", "Suomi", translated(english, {
        1: "Joukkuekuolonottelu", 2: "Kentät", 3: "Pelaajat",
        4: "Asetukset", 5: "Tekijät", 6: "Lopeta peli",
        8: "Asetusvalikko", 9: "Ääni", 10: "Yksityiskohdat ja grafiikka",
        11: "Tapahtumat", 12: "Pääsäännöt", 13: "Muut",
        14: "Kiellä aseita", 15: "Takaisin", 0x147: "Palauta oletukset",
        0x148: "Oletukset palautettu.", 0x14D: "Tila",
        0x14E: "Ikkuna", 0x14F: "Koko näyttö", 0x150: "Kieli",
    }))
    print(f"Generated {len(english)} English strings and 3 starter overlays")


if __name__ == "__main__":
    main()
