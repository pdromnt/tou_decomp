#!/usr/bin/env python3
"""Generate TOU's authoritative English catalog.

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
    "menu.tooltip.hold": "Hold &",
    "menu.tooltip.drag": "drag",
    "menu.value.infinite": "INFINITE",
    "menu.value.random": "Random",
    "menu.game_mode.custom": "Custom",
    "menu.game_mode.quite_normal": "Quite normal",
    "menu.game_mode.turret_wars": "Turret wars",
    "menu.game_mode.cyberdeath": "Cyberdeath",
    "menu.game_mode.quick_rounds": "Quick rounds",
    "menu.game_mode.subspace_trench": "Subspace trench",
    "menu.game_mode.base_defending": "Base defending",
    "key.backspace": "Backspace", "key.enter": "Enter",
    "key.left_ctrl": "Left ctrl", "key.right_ctrl": "Right ctrl",
    "key.left_shift": "Left shift", "key.right_shift": "Right shift",
    "key.left_alt": "Left alt", "key.right_alt": "Right alt",
    "key.spacebar": "Spacebar", "key.caps_lock": "Caps Lock",
    "key.num_lock": "Numlock", "key.scroll_lock": "Scroll lock",
    "key.numpad": "numpad", "key.pause": "Pause", "key.home": "Home",
    "key.up_arrow": "Up arrow", "key.page_up": "Page up",
    "key.left_arrow": "Left arrow", "key.right_arrow": "Right arrow",
    "key.end": "End", "key.down_arrow": "Down arrow",
    "key.page_down": "Page down", "key.insert": "Insert", "key.delete": "Delete",
    "key.left_system": "Left system key", "key.right_system": "Right system key",
    "key.application": "Application key",
    "key.escape": "Esc",
    "levels.hover.gg_author_format": "GG THEME AUTHOR: %s",
    "levels.hover.gg_name_format": "GG THEME NAME: %s",
    "levels.hover.level_author_format": "LEVEL AUTHOR: %s",
    "levels.hover.level_name_format": "LEVEL NAME: %s",
    "levels.hover.author_email_format": "AUTHOR'S EMAIL: %s",
    "levels.hover.level_type_format": "LEVEL TYPE: %s",
    "levels.hover.type.gg_theme": "GG THEME",
    "levels.hover.type.normal_level": "NORMAL LEVEL",
    "levels.summary_format": "You have %d levels and %d GG themes",
    "results.draw": "Draw!",
    "results.back_to_menu": "Back to the menu",
    "results.team_draw_format": "Team %d and team %d win with a draw!",
    "results.team_wins_format": "Team %d wins!",
    "results.debris_killed_format": "Debris killed: %d",
    "results.elapsed_format": "Game elapsed: %d hours, %d minutes, %d seconds",
    "results.player_award_format": "%s (Player %d)",
    "results.team_award_format": "%s (Team %d)",
    "awards.most_valuable": "Most valuable",
    "awards.most_violent": "Most violent",
    "awards.survivor": "Survivor",
    "awards.most_moving": "Most moving",
    "awards.most_explosive": "Most explosive",
    "awards.base_builder": "Base builder award",
    "awards.most_useless": "Most useless",
    "awards.greedy": "Greedy award",
    "awards.the_best": "The best",
    "awards.odd": "Odd award",
    "awards.explosive": "Explosive award",
    "hud.frags_format": "Frags: %d",
    "hud.lives_format": "Lives: %d",
    "hud.you_are_dead": "You are dead!",
    "hud.pause_format": "Game Paused. Press \"%s\" to continue.",
    "hud.back_to_game": "Back to the game",
    "hud.exit_to_menu": "Exit to menu",
    "hud.next_level": "Next level",
    "hud.round_continue_action": "to continue",
    "hud.round_current_wins": "Current wins:",
    "hud.round_exit_action": "to exit",
    "hud.round_exit_prefix": "and",
    "hud.round_level_format": "Level: %d / %d",
    "hud.level_skipped": "Level skipped",
    "hud.everybody_died": "Draw. Everybody died",
    "hud.team_wins_round_format": "Team %d wins the round",
    "hud.team_format": "Team %d",
    "hud.wins_the_round": "wins the round",
    "hud.team_1": "Team 1", "hud.team_2": "Team 2", "hud.team_3": "Team 3",
    "hud.all_teams_dead": "All teams are dead",
    "hud.your_team_only_alive": "Your team is the only team alive!",
    "hud.team_only_alive_format": "Team %d is the only team alive.",
    "pickup.full_energy": "Full energy", "pickup.booby_trap": "Booby trap",
    "pickup.death_ring": "Death Ring", "pickup.four_miniships": "4 Miniships",
    "pickup.six_insects": "6 Insects", "pickup.weapon_loaded": "Weapon loaded",
    "pickup.faster_special": "Faster special gun", "pickup.better_basic": "Better basic gun",
    "pickup.small_medikit": "Small medikit", "pickup.large_medikit": "Large medikit",
    "pickup.hurry_up": "Hurry up!",
    "error.could_not_load_ships": "Could not load ships!",
    "lan.landing_prompt": "Host a session or join one.",
    "lan.join_prompt": "Edit the host, port, and team, then connect.",
    "lan.roster_format": "%d/4 players: %s",
    "lan.roster_entry_format": "%sP%u T%u S%u %s",
    "lan.ready": "READY",
    "lan.loading": "LOADING",
    "lan.reject.session_full": "LAN session is full",
    "lan.reject.invalid_profile": "Invalid player profile",
    "lan.reject.level_content": "Level content mismatch; use the same game package",
    "lan.reject.gameplay_assets": "Gameplay asset mismatch; use the same game package",
    "lan.reject.simulation_build": "Simulation build mismatch; use the same TOU release",
    "lan.reject.unknown": "Unknown LAN refusal",
    "lan.disconnect.connection_lost": "Connection lost",
    "lan.disconnect.timed_out": "Peer timed out",
    "lan.disconnect.invalid_data": "Received invalid network data",
    "lan.disconnect.protocol_mismatch": "Incompatible LAN protocol version",
    "lan.disconnect.sync_failed": "Level synchronization failed",
    "lan.disconnect.session_ended": "LAN session ended by peer",
    "lan.team_format": "Team %d",
    "lan.error.client_not_ready": "A client is still connecting. Wait or ask them to reconnect.",
    "lan.error.no_clients": "No client is connected yet.",
    "lan.status.choose_host_or_join": "Choose either LAN host or LAN join.",
    "lan.status.client_joined_format": "Player %u joined; press Start when ready.",
    "lan.status.connect_failed_format": "Could not connect to %s:%u",
    "lan.status.connected_format": "Connected as Player %u; waiting for host",
    "lan.status.connecting_format": "Connecting to %s:%u",
    "lan.status.corrected_format": "State corrected at tick %u",
    "lan.status.correcting_format": "Correcting Player %u after mismatch at tick %u",
    "lan.status.correction_failed": "State correction failed; match paused",
    "lan.status.desync_format": "DESYNC at tick %u (local %llx, remote %llx)",
    "lan.status.disconnected_format": "Disconnected: %s",
    "lan.status.host_accept_failed": "Could not accept LAN client",
    "lan.status.host_failed_format": "Could not host on port %u",
    "lan.status.hosting_format": "Hosting on port %u; waiting for clients",
    "lan.status.invalid_address": "Invalid LAN address",
    "lan.status.level_loaded_client": "Level loaded; waiting for host synchronization",
    "lan.status.level_loaded_host": "Host level loaded; waiting for clients",
    "lan.status.match_starting": "Match starting",
    "lan.status.player_loaded_format": "Player %u loaded; waiting for synchronization",
    "lan.status.snapshot_failed": "Could not synchronize this level; it is too large for LAN beta",
    "lan.status.socket_init_failed": "Could not initialize network sockets",
    "lan.status.starting_players_format": "Starting %u-player LAN match",
    "lan.status.synchronized": "Synchronized; match active",
}

ASSIGNMENT = re.compile(
    r'^\s*g_MenuStrings\[(0x[0-9A-Fa-f]+|\d+)\]\s*=\s*\(char \*\)"((?:\\.|[^"\\])*)";'
)
DYNAMIC_SLOTS = {0x65, *range(0x71, 0x8C), *range(0x149, 0x14D),
                 0x15E, 0x15F, 0x160, 0x162, 0x164}


def decode_c_string(value: str) -> str:
    raw = ast.literal_eval(f'b"{value}"')
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


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


def write_catalog(code: str, name: str, strings: dict[str, str]) -> None:
    payload = {"schemaVersion": 1, "locale": code, "name": name, "strings": strings}
    filename = "pt-br" if code == "pt-BR" else code
    path = OUTPUT / f"{filename}.json"
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    OUTPUT.mkdir(exist_ok=True)
    english = read_english()
    write_catalog("en", "English", english)
    print(f"Generated {len(english)} authoritative English strings")


if __name__ == "__main__":
    main()
