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

DIRECT_ENGLISH = {
    "levels.hover.gg_author_format": "GG THEME AUTHOR: %s",
    "levels.hover.gg_name_format": "GG THEME NAME: %s",
    "levels.hover.level_author_format": "LEVEL AUTHOR: %s",
    "levels.hover.level_name_format": "LEVEL NAME: %s",
    "levels.hover.author_email_format": "AUTHOR'S EMAIL: %s",
    "levels.hover.level_type_format": "LEVEL TYPE: %s",
    "levels.hover.type.gg_theme": "GG THEME",
    "levels.hover.type.normal_level": "NORMAL LEVEL",
    "levels.summary_format": "You have %d levels and %d GG themes",
}

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
    strings: dict[str, str] = dict(DIRECT_ENGLISH)
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


def translated(english: dict[str, str], values: dict[int, str],
               direct: dict[str, str]) -> dict[str, str]:
    by_index = {int(key.split(".")[1]): key for key in english if key.startswith("menu.")}
    result = {by_index[index]: text for index, text in values.items() if index in by_index}
    result.update(direct)
    return result


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
        0x5C: "Cantidad de niveles", 0x5D: "Aleatorizar niveles",
        0xEA: "Opciones del nivel",
    }, {
        "levels.hover.gg_author_format": "AUTOR DEL TEMA GG: %s",
        "levels.hover.gg_name_format": "NOMBRE DEL TEMA GG: %s",
        "levels.hover.level_author_format": "AUTOR DEL NIVEL: %s",
        "levels.hover.level_name_format": "NOMBRE DEL NIVEL: %s",
        "levels.hover.author_email_format": "CORREO DEL AUTOR: %s",
        "levels.hover.level_type_format": "TIPO DE NIVEL: %s",
        "levels.hover.type.gg_theme": "TEMA GG",
        "levels.hover.type.normal_level": "NIVEL NORMAL",
        "levels.summary_format": "Tienes %d niveles y %d temas GG",
    }))
    write_catalog("pt-BR", "Português (Brasil)", translated(english, {
        1: "Mata-mata em equipe", 2: "Fases", 3: "Jogadores",
        4: "Opções", 5: "Créditos", 6: "Sair do jogo",
        8: "Menu de opções", 9: "Áudio", 10: "Detalhes e gráficos",
        11: "Eventos", 12: "Regras principais", 13: "Diversos",
        14: "Banir armas", 15: "Voltar", 0x147: "Restaurar padrões",
        0x148: "Padrões restaurados.", 0x14D: "Modo",
        0x14E: "Janela", 0x14F: "Tela cheia", 0x150: "Idioma",
        0x5C: "Quantidade de fases", 0x5D: "Embaralhar fases",
        0xEA: "Opções da fase",
    }, {
        "levels.hover.gg_author_format": "AUTOR DO TEMA GG: %s",
        "levels.hover.gg_name_format": "NOME DO TEMA GG: %s",
        "levels.hover.level_author_format": "AUTOR DA FASE: %s",
        "levels.hover.level_name_format": "NOME DA FASE: %s",
        "levels.hover.author_email_format": "E-MAIL DO AUTOR: %s",
        "levels.hover.level_type_format": "TIPO DA FASE: %s",
        "levels.hover.type.gg_theme": "TEMA GG",
        "levels.hover.type.normal_level": "FASE NORMAL",
        "levels.summary_format": "Você tem %d fases e %d temas GG",
    }))
    write_catalog("fi", "Suomi", translated(english, {
        1: "Joukkuekuolonottelu", 2: "Kentät", 3: "Pelaajat",
        4: "Asetukset", 5: "Tekijät", 6: "Lopeta peli",
        8: "Asetusvalikko", 9: "Ääni", 10: "Yksityiskohdat ja grafiikka",
        11: "Tapahtumat", 12: "Pääsäännöt", 13: "Muut",
        14: "Kiellä aseita", 15: "Takaisin", 0x147: "Palauta oletukset",
        0x148: "Oletukset palautettu.", 0x14D: "Tila",
        0x14E: "Ikkuna", 0x14F: "Koko näyttö", 0x150: "Kieli",
        0x5C: "Kenttien määrä", 0x5D: "Sekoita kentät",
        0xEA: "Kenttäasetukset",
    }, {
        "levels.hover.gg_author_format": "GG-TEEMAN TEKIJÄ: %s",
        "levels.hover.gg_name_format": "GG-TEEMAN NIMI: %s",
        "levels.hover.level_author_format": "KENTÄN TEKIJÄ: %s",
        "levels.hover.level_name_format": "KENTÄN NIMI: %s",
        "levels.hover.author_email_format": "TEKIJÄN SÄHKÖPOSTI: %s",
        "levels.hover.level_type_format": "KENTÄN TYYPPI: %s",
        "levels.hover.type.gg_theme": "GG-TEEMA",
        "levels.hover.type.normal_level": "NORMAALI KENTTÄ",
        "levels.summary_format": "Sinulla on %d kenttää ja %d GG-teemaa",
    }))
    print(f"Generated {len(english)} English strings and 3 starter overlays")


if __name__ == "__main__":
    main()
