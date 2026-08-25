# Cut Campaign / Mission Mode Forensics

## Verdict

`data/splay.jpg` belongs to a substantial single-player campaign prototype in
the retail executable. It is not used by any reachable released menu flow.
The code is much more complete than a mock-up, but the shipped game also omits
the fourteen level files it requests (`sp1.lev` through `sp14.lev`).

This makes the feature **cut content**, not a hidden mode that players merely
failed to discover. Restoring it is possible as a new project feature, but it
is not a matter of exposing one existing menu button.

## Binary Evidence

The authoritative addresses below refer to `TOU15b.exe`.

| Address | Evidence |
| --- | --- |
| `0x00425840` | 408-instruction briefing/progression page builder. Selects briefing text, portrait, mission, difficulty, player count, paging, and start action. |
| `0x00425FE0` | Menu background selection; page `0x1D` loads background index 2, `splay.jpg`. |
| `0x0041A95B` | Calls the campaign rules initializer when flag `0x0048764A` is nonzero. |
| `0x0045BA50` | Builds mission-specific players, teams, ships, weapons, difficulty, and gameplay rules. |
| `0x0041E460` | Routes campaign match completion back to briefing page `0x1D` and records success/failure. |
| `0x0042D21E` | Menu switch case for page `0x1D`; calls the briefing builder. |
| `0x0042D230` | Start action/page `0x1E`; enables campaign mode and starts the match. |

The current reconstruction's empty `FUN_00425840` and comments describing the
page as a harmless unused match-end hook were therefore inaccurate.

## Why It Cannot Be Opened Normally

The retail main menu is built statically at `0x0042A4B1`. It has Team
Deathmatch, Levels, Players, Options, Quick Help, Credits, and Exit; it creates
no campaign item. The otherwise unused `Campaign game` string exists at legacy
menu slot `0x128`, but the main menu never consumes it.

The state graph is circular:

1. A campaign result can enter briefing page `0x1D`, but only when the campaign
   flag is already set.
2. Page `0x1D` contains the `Let's kill!` action leading to `0x1E`.
3. Action `0x1E` is what sets the campaign flag and starts a mission.
4. No released menu item provides the initial transition into page `0x1D`.

There are also three campaign-unlock pages (`0x16`, `0x17`, and `0x18`). Page
`0x16` can advance to `0x17`, and `0x17` to `0x18`, but exhaustive references
to the current-page global show no released transition into `0x16`. They unlock
mission maxima 4, 8, and 14 respectively for all six progress tracks.

Even forcing the page in memory is not enough to play the campaign as shipped:
the briefing builder selects level identifiers `sp1` through `sp14`, and none
of those `.lev` files are present in the original or reconstructed level set.

## Recovered Structure

The campaign maintains six independent progress/unlock pairs:

- one or two human players; multiplied by
- Hard, Medium, or Easy difficulty.

The briefing page exposes:

- player count;
- skill (`Hard`, `Medium`, `Easy`);
- current mission and highest unlocked mission;
- next/previous briefing page where a mission has multiple pages;
- a speaker portrait/name;
- `Let's kill!` to begin a playable mission.

A win advances the selected progress slot up to index 14 and raises its unlock
limit. A loss keeps the current mission and replaces its briefing with Joe's
failure message. Index 14 is an epilogue with no playable start action.

### Characters / portrait slots

| Slot | Character |
| --- | --- |
| `0x11B` | General Joe Knoff |
| `0x11C` | Research engineer Nicole Molter |
| `0x11D` | Pilot Albert “Mood” Moody |
| `0x11E` | Field medic Martin Spear |
| `0x11F` | William Stanwood, president of the GDA |
| `0x120` | Pilot Henry Odom |
| `0x121` | Enemy leader Steve Hailhazard |

## Recovered Mission Sequence

The prose below summarizes the embedded briefings; it is not invented level
design. Exact terrain and placements cannot be recovered without the missing
maps.

| Index | Level ID | Briefing objective |
| ---: | --- | --- |
| 0 | `sp1` | Introduction, then eliminate an enemy spy. |
| 1 | `sp2` | Survive incoming artillery for about 60 seconds. |
| 2 | `sp3` | Destroy a small, lightly defended base and recover a new weapon. |
| 3 | `sp4` | Destroy another, more difficult enemy base. |
| 4 | `sp5` | Clear hostile activity in the woods with Mood. |
| 5 | `sp6` | Defend a base against waves for three minutes with Mood. |
| 6 | `sp7` | Destroy a base and rescue William with Martin aboard. |
| 7 | `sp8` | Assist Mood after he gets into trouble. |
| 8 | `sp9` | Defend the GDA main base from a huge assault; five ships are promised. |
| 9 | `sp10` | Destroy a heavily guarded base in two minutes with Henry. |
| 10 | `sp11` | Destroy a whole base with the recovered radial weapon. |
| 11 | `sp12` | Defend a base for five minutes while the nuclear weapon is unavailable. |
| 12 | `sp13` | Solo pickup of Martin at the lower-right repair pad under a strict limit. |
| 13 | `sp14` | Eliminate all enemy forces and Steve Hailhazard with Mood. |
| 14 | none | Victory epilogue; suggests replaying other skills or Team Deathmatch. |

## Mission Rules Recovered From `0x0045BA50`

The mode does not simply launch ordinary Team Deathmatch settings. It replaces
the active configuration before every mission:

- exactly one campaign level is active;
- the selected player count becomes one or two human pilots;
- mission tables add allies and a mission-dependent number of enemies;
- humans are assigned to team 1 and enemies to team 2;
- ship types and starting weapons are selected from mission tables;
- enemy difficulty is shifted by the chosen Hard/Medium/Easy setting;
- most normal rules are overwritten with fixed campaign values;
- individual missions apply special time/life/weapon overrides;
- campaign-only HUD paths display time and mission information differently.

The embedded enemy-count multiplier per mission is:

`1, 0, 3, 4, 4, 6, 3, 5, 15, 3, 5, 5, 0, 8, 0`

The accompanying ally-count table is:

`0, 0, 0, 0, 1, 1, 0, 1, 1, 2, 0, 0, 0, 1, 0`

These tables confirm that the briefings were backed by mission-specific setup
logic, not merely unused story text.

## Restoration Requirements

Before exposing Campaign in the modern menu:

1. Reconstruct `FUN_00425840` from the original assembly, including its
   multi-page briefing behavior and all six progress tracks.
2. Port `FUN_0045BA50` without changing its table indexing, signedness, player
   ordering, or difficulty arithmetic.
3. Separate the campaign flag from values since repurposed by LAN work.
4. Add explicit JSON fields for campaign progress/unlocks; the modern settings
   serializer intentionally excludes these currently hidden compatibility
   fields.
5. Recover the original `sp1..sp14` levels, if they exist anywhere, or design
   replacement missions explicitly identified as new work.
6. Restore/localize briefing text and verify portrait rendering over
   `splay.jpg`.
7. Validate success/failure, retry, progression, and every mission objective in
   both player-count and all three difficulty variants.

Without the missing maps, faithful restoration is impossible. A recreated
campaign can still reuse the recovered story and rule machinery, but it must be
documented as a reconstruction rather than original parity.
