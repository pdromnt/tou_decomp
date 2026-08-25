#!/usr/bin/env python3
"""Fill missing catalog entries using machine translation for human review.

Existing translations are always preserved. This is an opt-in maintainer tool,
never part of the build or CI path.
"""

from __future__ import annotations

import html
import json
import re
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = {
    "es": ("es", "es.json", "Español"),
    "pt-BR": ("pt", "pt-br.json", "Português (Brasil)"),
    "fi": ("fi", "fi.json", "Suomi"),
}
CURATED = {
    "es": {
        "menu.tooltip.hold": "Mantén y", "menu.tooltip.drag": "arrastra",
        "menu.value.infinite": "INFINITO", "menu.value.random": "Aleatorio",
        "menu.game_mode.custom": "Personalizado",
        "menu.game_mode.quite_normal": "Bastante normal",
        "menu.game_mode.turret_wars": "Guerra de torretas",
        "menu.game_mode.cyberdeath": "Cyberdeath",
        "menu.game_mode.quick_rounds": "Rondas rápidas",
        "menu.game_mode.subspace_trench": "Trinchera subespacial",
        "menu.game_mode.base_defending": "Defensa de la base",
        "key.backspace": "Retroceso", "key.enter": "Intro",
        "key.left_ctrl": "Ctrl izquierdo", "key.right_ctrl": "Ctrl derecho",
        "key.left_shift": "Mayús izquierda", "key.right_shift": "Mayús derecha",
        "key.left_alt": "Alt izquierdo", "key.right_alt": "Alt derecho",
        "key.spacebar": "Barra espaciadora", "key.caps_lock": "Bloq Mayús",
        "key.num_lock": "Bloq Num", "key.scroll_lock": "Bloq Despl",
        "key.numpad": "teclado numérico", "key.pause": "Pausa", "key.home": "Inicio",
        "key.up_arrow": "Flecha arriba", "key.page_up": "Re Pág",
        "key.left_arrow": "Flecha izquierda", "key.right_arrow": "Flecha derecha",
        "key.end": "Fin", "key.down_arrow": "Flecha abajo", "key.page_down": "Av Pág",
        "key.insert": "Insertar", "key.delete": "Suprimir",
        "key.left_system": "Tecla sistema izq.", "key.right_system": "Tecla sistema der.",
        "key.application": "Tecla aplicación",
        "results.draw": "¡Empate!",
        "results.team_draw_format": "¡Los equipos %d y %d empatan!",
        "results.team_wins_format": "¡El equipo %d gana!",
        "results.debris_killed_format": "Escombros destruidos: %d",
        "results.elapsed_format": "Duración: %d h, %d min, %d s",
        "results.player_award_format": "%s (Jugador %d)",
        "results.team_award_format": "%s (Equipo %d)",
        "awards.most_valuable": "Más valioso", "awards.most_violent": "Más violento",
        "awards.survivor": "Superviviente", "awards.most_moving": "Más activo",
        "awards.most_explosive": "Más explosivo", "awards.base_builder": "Constructor de bases",
        "awards.most_useless": "Más inútil", "awards.greedy": "Premio a la codicia",
        "awards.the_best": "El mejor", "awards.odd": "Premio extraño",
        "awards.explosive": "Premio explosivo",
        "hud.frags_format": "Bajas: %d", "hud.lives_format": "Vidas: %d",
        "hud.you_are_dead": "¡Estás muerto!",
        "hud.pause_format": "Juego en pausa. Pulsa \"%s\" para continuar.",
        "hud.level_skipped": "Nivel omitido",
        "hud.everybody_died": "Empate. Todos murieron",
        "hud.team_wins_round_format": "El equipo %d gana la ronda",
        "hud.team_format": "Equipo %d", "hud.wins_the_round": "gana la ronda",
        "hud.team_1": "Equipo 1", "hud.team_2": "Equipo 2", "hud.team_3": "Equipo 3",
        "hud.all_teams_dead": "Todos los equipos han muerto",
        "hud.your_team_only_alive": "¡Tu equipo es el único con vida!",
        "hud.team_only_alive_format": "El equipo %d es el único con vida.",
        "pickup.full_energy": "Energía completa", "pickup.booby_trap": "Trampa",
        "pickup.death_ring": "Anillo de la muerte", "pickup.four_miniships": "4 Miniships",
        "pickup.six_insects": "6 insectos", "pickup.weapon_loaded": "Arma cargada",
        "pickup.faster_special": "Arma especial más rápida",
        "pickup.better_basic": "Arma básica mejorada",
        "pickup.small_medikit": "Botiquín pequeño", "pickup.large_medikit": "Botiquín grande",
        "pickup.hurry_up": "¡Date prisa!",
        "error.could_not_load_ships": "¡No se pudieron cargar las naves!",
        "menu.036.off": "Desactivado", "menu.037.on": "Activado",
        "menu.052.ok": "Aceptable", "menu.056.ok": "OK",
        "menu.063.controls": "Controles", "menu.094.plr": "Jug.",
        "menu.098.ship": "Nave", "menu.106.wins": "Victorias",
        "menu.107.frags": "Bajas",
        "menu.143.base_holding": "Conservar la base",
        "menu.152.tou_homepage": "Página de TOU:",
        "menu.217.both_digger_bullets": "Digger y balas",
        "menu.218.only_digger_very_tactical": "Solo Digger (muy táctico)",
        "menu.235.x_flip_levels": "Invertir niveles en X",
        "menu.237.player_amount": "Cantidad de jugadores",
        "menu.238.human_amount": "Cantidad de humanos",
        "menu.193.lots": "Muchos", "menu.205.lots": "Muchos",
        "menu.209.lots": "Muchos", "menu.278.lots": "Muchos",
        "menu.253.wall_bounciness": "Elasticidad de las paredes",
        "menu.273.level_default": "Predeterminado del nivel",
        "menu.263.resolution_up": "Aumentar resolución",
        "menu.264.resolution_down": "Reducir resolución",
        "menu.337.english": "English", "menu.338.espanol": "Español",
        "menu.339.portugues_brasil": "Português (Brasil)", "menu.340.suomi": "Suomi",
    },
    "pt-BR": {
        "menu.tooltip.hold": "Segure e", "menu.tooltip.drag": "arraste",
        "menu.value.infinite": "INFINITO", "menu.value.random": "Aleatório",
        "menu.game_mode.custom": "Personalizado",
        "menu.game_mode.quite_normal": "Bem normal",
        "menu.game_mode.turret_wars": "Guerra de torres",
        "menu.game_mode.cyberdeath": "Cyberdeath",
        "menu.game_mode.quick_rounds": "Rodadas rápidas",
        "menu.game_mode.subspace_trench": "Trincheira subespacial",
        "menu.game_mode.base_defending": "Defesa da base",
        "key.backspace": "Backspace", "key.enter": "Enter",
        "key.left_ctrl": "Ctrl esquerdo", "key.right_ctrl": "Ctrl direito",
        "key.left_shift": "Shift esquerdo", "key.right_shift": "Shift direito",
        "key.left_alt": "Alt esquerdo", "key.right_alt": "Alt direito",
        "key.spacebar": "Barra de espaço", "key.caps_lock": "Caps Lock",
        "key.num_lock": "Num Lock", "key.scroll_lock": "Scroll Lock",
        "key.numpad": "teclado numérico", "key.pause": "Pausa", "key.home": "Início",
        "key.up_arrow": "Seta para cima", "key.page_up": "Page Up",
        "key.left_arrow": "Seta para esquerda", "key.right_arrow": "Seta para direita",
        "key.end": "Fim", "key.down_arrow": "Seta para baixo", "key.page_down": "Page Down",
        "key.insert": "Inserir", "key.delete": "Excluir",
        "key.left_system": "Tecla do sistema esq.", "key.right_system": "Tecla do sistema dir.",
        "key.application": "Tecla de menu",
        "results.draw": "Empate!",
        "results.team_draw_format": "Equipes %d e %d empataram!",
        "results.team_wins_format": "Equipe %d venceu!",
        "results.debris_killed_format": "Destroços destruídos: %d",
        "results.elapsed_format": "Duração: %d h, %d min, %d s",
        "results.player_award_format": "%s (Jogador %d)",
        "results.team_award_format": "%s (Equipe %d)",
        "awards.most_valuable": "Mais valioso", "awards.most_violent": "Mais violento",
        "awards.survivor": "Sobrevivente", "awards.most_moving": "Mais ativo",
        "awards.most_explosive": "Mais explosivo", "awards.base_builder": "Construtor de bases",
        "awards.most_useless": "Mais inútil", "awards.greedy": "Prêmio ganancioso",
        "awards.the_best": "O melhor", "awards.odd": "Prêmio estranho",
        "awards.explosive": "Prêmio explosivo",
        "hud.frags_format": "Abates: %d", "hud.lives_format": "Vidas: %d",
        "hud.you_are_dead": "Você morreu!",
        "hud.pause_format": "Jogo pausado. Pressione \"%s\" para continuar.",
        "hud.level_skipped": "Fase ignorada",
        "hud.everybody_died": "Empate. Todos morreram",
        "hud.team_wins_round_format": "A equipe %d venceu a rodada",
        "hud.team_format": "Equipe %d", "hud.wins_the_round": "venceu a rodada",
        "hud.team_1": "Equipe 1", "hud.team_2": "Equipe 2", "hud.team_3": "Equipe 3",
        "hud.all_teams_dead": "Todas as equipes morreram",
        "hud.your_team_only_alive": "Sua equipe é a única sobrevivente!",
        "hud.team_only_alive_format": "A equipe %d é a única sobrevivente.",
        "pickup.full_energy": "Energia completa", "pickup.booby_trap": "Armadilha",
        "pickup.death_ring": "Anel da morte", "pickup.four_miniships": "4 Miniships",
        "pickup.six_insects": "6 insetos", "pickup.weapon_loaded": "Arma carregada",
        "pickup.faster_special": "Arma especial mais rápida",
        "pickup.better_basic": "Arma básica melhorada",
        "pickup.small_medikit": "Kit médico pequeno", "pickup.large_medikit": "Kit médico grande",
        "pickup.hurry_up": "Rápido!",
        "error.could_not_load_ships": "Não foi possível carregar as naves!",
        "menu.036.off": "Desligado", "menu.037.on": "Ligado",
        "menu.045.rapidfire": "Tiro rápido", "menu.052.ok": "Aceitável",
        "menu.063.controls": "Controles", "menu.094.plr": "Jog.",
        "menu.064.viewport_size": "Área de visão", "menu.065.full": "Total",
        "menu.069.yes_lots_of_them": "Sim, muitas", "menu.090.menu_button_detonate": "Botão de menu / detonar",
        "menu.098.ship": "Nave", "menu.107.frags": "Abates",
        "menu.143.base_holding": "Manter a base",
        "menu.152.tou_homepage": "Página do TOU:",
        "menu.217.both_digger_bullets": "Digger e balas",
        "menu.218.only_digger_very_tactical": "Apenas Digger (bem tático)",
        "menu.219.only_bullets": "Apenas balas",
        "menu.154.must_go_to_repair_at_end": "Deve ir ao reparo no fim",
        "menu.155.if_you_must_go_to_repair_at_end": "Se precisar ir ao reparo no fim:",
        "menu.193.lots": "Muitos", "menu.205.lots": "Muitos",
        "menu.209.lots": "Muitos", "menu.278.lots": "Muitos",
        "menu.235.x_flip_levels": "Espelhar fases no eixo X",
        "menu.237.player_amount": "Quantidade de jogadores",
        "menu.238.human_amount": "Quantidade de humanos",
        "menu.250.must_wait_for_reborn": "Aguardar renascimento",
        "menu.253.wall_bounciness": "Elasticidade das paredes",
        "menu.254.more_misc": "Mais opções diversas",
        "menu.273.level_default": "Padrão da fase",
        "menu.263.resolution_up": "Aumentar resolução",
        "menu.264.resolution_down": "Reduzir resolução",
        "menu.337.english": "English", "menu.338.espanol": "Español",
        "menu.339.portugues_brasil": "Português (Brasil)", "menu.340.suomi": "Suomi",
    },
    "fi": {
        "menu.tooltip.hold": "Pidä ja", "menu.tooltip.drag": "vedä",
        "menu.value.infinite": "RAJATON", "menu.value.random": "Satunnainen",
        "menu.game_mode.custom": "Mukautettu",
        "menu.game_mode.quite_normal": "Melko tavallinen",
        "menu.game_mode.turret_wars": "Tykkitornisota",
        "menu.game_mode.cyberdeath": "Cyberdeath",
        "menu.game_mode.quick_rounds": "Nopeat kierrokset",
        "menu.game_mode.subspace_trench": "Aliavaruushauta",
        "menu.game_mode.base_defending": "Tukikohdan puolustus",
        "key.backspace": "Askelpalautin", "key.enter": "Enter",
        "key.left_ctrl": "Vasen Ctrl", "key.right_ctrl": "Oikea Ctrl",
        "key.left_shift": "Vasen vaihto", "key.right_shift": "Oikea vaihto",
        "key.left_alt": "Vasen Alt", "key.right_alt": "Oikea Alt",
        "key.spacebar": "Välilyönti", "key.caps_lock": "Caps Lock",
        "key.num_lock": "Num Lock", "key.scroll_lock": "Scroll Lock",
        "key.numpad": "numeronäppäimistö", "key.pause": "Tauko", "key.home": "Home",
        "key.up_arrow": "Nuoli ylös", "key.page_up": "Page Up",
        "key.left_arrow": "Nuoli vasemmalle", "key.right_arrow": "Nuoli oikealle",
        "key.end": "End", "key.down_arrow": "Nuoli alas", "key.page_down": "Page Down",
        "key.insert": "Insert", "key.delete": "Delete",
        "key.left_system": "Vasen järjestelmänäppäin",
        "key.right_system": "Oikea järjestelmänäppäin",
        "key.application": "Valikkonäppäin",
        "results.draw": "Tasapeli!",
        "results.team_draw_format": "Joukkueet %d ja %d päätyivät tasapeliin!",
        "results.team_wins_format": "Joukkue %d voittaa!",
        "results.debris_killed_format": "Tuhottu romu: %d",
        "results.elapsed_format": "Kesto: %d h, %d min, %d s",
        "results.player_award_format": "%s (Pelaaja %d)",
        "results.team_award_format": "%s (Joukkue %d)",
        "awards.most_valuable": "Arvokkain", "awards.most_violent": "Väkivaltaisin",
        "awards.survivor": "Selviytyjä", "awards.most_moving": "Liikkuvin",
        "awards.most_explosive": "Räjähtävin", "awards.base_builder": "Tukikohdan rakentaja",
        "awards.most_useless": "Hyödyttömin", "awards.greedy": "Ahneuspalkinto",
        "awards.the_best": "Paras", "awards.odd": "Outo palkinto",
        "awards.explosive": "Räjähdyspalkinto",
        "hud.frags_format": "Tapot: %d", "hud.lives_format": "Elämät: %d",
        "hud.you_are_dead": "Olet kuollut!",
        "hud.pause_format": "Peli tauolla. Jatka painamalla \"%s\".",
        "hud.level_skipped": "Kenttä ohitettu",
        "hud.everybody_died": "Tasapeli. Kaikki kuolivat",
        "hud.team_wins_round_format": "Joukkue %d voittaa kierroksen",
        "hud.team_format": "Joukkue %d", "hud.wins_the_round": "voittaa kierroksen",
        "hud.team_1": "Joukkue 1", "hud.team_2": "Joukkue 2", "hud.team_3": "Joukkue 3",
        "hud.all_teams_dead": "Kaikki joukkueet ovat kuolleet",
        "hud.your_team_only_alive": "Vain sinun joukkueesi on elossa!",
        "hud.team_only_alive_format": "Vain joukkue %d on elossa.",
        "pickup.full_energy": "Täysi energia", "pickup.booby_trap": "Ansamiina",
        "pickup.death_ring": "Kuolemanrengas", "pickup.four_miniships": "4 Miniships",
        "pickup.six_insects": "6 hyönteistä", "pickup.weapon_loaded": "Ase ladattu",
        "pickup.faster_special": "Nopeampi erikoisase",
        "pickup.better_basic": "Parempi perusase",
        "pickup.small_medikit": "Pieni lääkintäpakkaus",
        "pickup.large_medikit": "Suuri lääkintäpakkaus",
        "pickup.hurry_up": "Kiirehdi!",
        "error.could_not_load_ships": "Aluksia ei voitu ladata!",
        "menu.036.off": "Pois päältä", "menu.037.on": "Päällä",
        "menu.045.rapidfire": "Sarjatuli", "menu.063.controls": "Ohjaimet",
        "menu.048.bombing": "Pommitus", "menu.064.viewport_size": "Näkymän koko",
        "menu.078.human_player_controls": "Ihmispelaajan ohjaimet",
        "menu.094.plr": "Pel.", "menu.107.frags": "Tapot",
        "menu.100.resolution": "Resoluutio", "menu.145.starting_weapon": "Aloitusase",
        "menu.143.base_holding": "Tukikohdan hallinta",
        "menu.150.random_turrets_at_start": "Satunnaiset tykkitornit alussa",
        "menu.152.tou_homepage": "TOU:n kotisivu:",
        "menu.185.gates": "Portit",
        "menu.186.level_s_own_turrets": "Kentän omat tykkitornit",
        "menu.191.some": "Jonkin verran", "menu.204.some": "Jonkin verran",
        "menu.208.some": "Jonkin verran", "menu.276.some": "Jonkin verran",
        "menu.217.both_digger_bullets": "Digger ja luodit",
        "menu.218.only_digger_very_tactical": "Vain Digger (hyvin taktinen)",
        "menu.235.x_flip_levels": "Peilaa kentät X-suunnassa",
        "menu.237.player_amount": "Pelaajien määrä",
        "menu.238.human_amount": "Ihmispelaajien määrä",
        "menu.253.wall_bounciness": "Seinän kimmoisuus",
        "menu.273.level_default": "Kentän oletus", "menu.318.level_default": "Kentän oletus",
        "menu.263.resolution_up": "Suurenna tarkkuutta",
        "menu.264.resolution_down": "Pienennä tarkkuutta",
        "menu.290.hard": "Vaikea", "menu.292.easy": "Helppo",
        "menu.308.friendly_fire": "Omien tulitus",
        "menu.337.english": "English", "menu.338.espanol": "Español",
        "menu.339.portugues_brasil": "Português (Brasil)", "menu.340.suomi": "Suomi",
    },
}
PRESERVE_KEYS = {
    "menu.000.tou", "menu.007.tunnels_of_the_underworld",
    "menu.017.hannu_kankaanpaa", "menu.019.sampsa_lehtonen",
    "menu.021.kimmo_palander", "menu.025.teemu_makinen",
    "menu.026.tapio_raevaara", "menu.027.teppo_kankaanpaa",
    "menu.152.tou_homepage", "menu.270.tougame_iobox_fi",
    "menu.271.http_tou_has_it", "menu.283.general_joe_knoff",
    "menu.303.0", "menu.304.1", "menu.305.2", "menu.306.3", "menu.307.4",
    "menu.321.c_gigamess_2002_all_rights_reserved",
    "menu.337.english", "menu.338.espanol", "menu.339.portugues_brasil",
    "menu.340.suomi",
}
MARKER = re.compile(r"\[\[(\d{3})\]\]\s*")


def translate_batch(target: str, entries: list[tuple[str, str]]) -> dict[str, str]:
    masked: list[tuple[str, str, list[str]]] = []
    for key, value in entries:
        tokens = re.findall(r"%(?:%|[-+0-9.]*[a-zA-Z])", value)
        for index, token in enumerate(tokens):
            value = value.replace(token, f"__FMT{index}__", 1)
        masked.append((key, value, tokens))

    payload = "\n".join(f"[[{i:03d}]] {value}" for i, (_, value, _) in enumerate(masked))
    query = urllib.parse.urlencode({
        "client": "gtx", "sl": "en", "tl": target, "dt": "t", "q": payload
    })
    with urllib.request.urlopen(
        "https://translate.googleapis.com/translate_a/single?" + query, timeout=30
    ) as response:
        translated = "".join(part[0] for part in json.load(response)[0])

    matches = list(MARKER.finditer(translated))
    if len(matches) != len(masked):
        raise RuntimeError(f"Translation markers lost: expected {len(masked)}, found {len(matches)}")
    result: dict[str, str] = {}
    for pos, match in enumerate(matches):
        index = int(match.group(1))
        end = matches[pos + 1].start() if pos + 1 < len(matches) else len(translated)
        value = html.unescape(translated[match.end():end].strip())
        key, _, tokens = masked[index]
        for token_index, token in enumerate(tokens):
            value = value.replace(f"__FMT{token_index}__", token)
        result[key] = value
    return result


def translate_resilient(target: str,
                        entries: list[tuple[str, str]]) -> dict[str, str]:
    try:
        return translate_batch(target, entries)
    except RuntimeError:
        if len(entries) <= 1:
            key, value = entries[0]
            tokens = re.findall(r"%(?:%|[-+0-9.]*[a-zA-Z])", value)
            for index, token in enumerate(tokens):
                value = value.replace(token, f"__FMT{index}__", 1)
            query = urllib.parse.urlencode({
                "client": "gtx", "sl": "en", "tl": target,
                "dt": "t", "q": value
            })
            with urllib.request.urlopen(
                "https://translate.googleapis.com/translate_a/single?" + query,
                timeout=30
            ) as response:
                value = "".join(part[0] for part in json.load(response)[0])
            for index, token in enumerate(tokens):
                value = value.replace(f"__FMT{index}__", token)
            return {key: html.unescape(value)}
        middle = len(entries) // 2
        result = translate_resilient(target, entries[:middle])
        result.update(translate_resilient(target, entries[middle:]))
        return result


def main() -> None:
    english = json.loads((ROOT / "lang/en.json").read_text(encoding="utf-8"))["strings"]
    for locale, (target, filename, name) in LANGUAGES.items():
        path = ROOT / "lang" / filename
        root = json.loads(path.read_text(encoding="utf-8"))
        strings = {key: value for key, value in root["strings"].items()
                   if key in english}
        missing = [(key, value) for key, value in english.items()
                   if key not in strings and key not in PRESERVE_KEYS and
                   key not in CURATED[locale] and value.strip()]
        batch_size = 20
        for start in range(0, len(missing), batch_size):
            strings.update(translate_resilient(target, missing[start:start + batch_size]))
            print(f"{locale}: translated {min(start + batch_size, len(missing))}/{len(missing)}")
        for key in PRESERVE_KEYS:
            if key in english:
                strings[key] = english[key]
        for key, value in english.items():
            strings.setdefault(key, value)
        strings.update(CURATED[locale])
        root = {"schemaVersion": 1, "locale": locale, "name": name,
                "strings": dict(sorted(strings.items()))}
        path.write_text(json.dumps(root, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
