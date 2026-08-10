# Europe finish bridge (`48d3` helpers + `38fd_55b6`)

Landfall helpers under `FUN_48d3_06ba` / nation EOT; optional Europe UI when
`DS:0x14c`. Bridge: [`between_turns.md`](between_turns.md) ·
[`europe_exit_landfall.md`](europe_exit_landfall.md) ·
[`nation_eot_ship_spawn.md`](nation_eot_ship_spawn.md).

## Helpers

| Body | Lines | Thunk | Role |
|------|-------|-------|------|
| `FUN_48d3_0002` | **77563–77588** | `291f_0aee` | Landfall/goto duration: mostly **1**; **2** if RNG>89 and docks>2 and colony-count probe; used from `00f2` rare Merc spawn and Europe-exit place |
| `FUN_48d3_03d0` | **77761–77781** | `2a1f_0246` | Walk units on tile: dec `+0x315a`; at 0 → move/act (`0880`/`08c6`). `06ba` calls twice before/after ship walk |
| `FUN_48d3_064e` | **77905–77938** | `2a1f_0238` | For each ship type `0xd..0x12` on tile: `0920` + spiral place `2a1f_0262`→`48d3_048e`. Tail of `06ba` |

## Europe screen (`FUN_38fd_55b6`)

| Item | Value |
|------|-------|
| Lines | **68114–68243** |
| Thunk | `FUN_281f_05fa` |
| Gate in `00f2` | @58378–80: if `DS:0x14c != 0` → `05fa(nation, DS:0x14e)` |

`06ba` / ship-ready may set `0x14c=1` and focus unit `0x14e`. Entry draws
focus-ship chrome when `param_2 >= 0`, then event loop until `0x9e38` clear
(demo `0x828` auto-exit branch).

## Linux reshape

| DOS | Linux |
|-----|-------|
| Inline Europe UI mid-`00f2` | `game_finish_end_turn` → `game_europe_deliver_bound_ships`; open Europe if `europe.open_on_dock` |
| Delay ticks / spiral place | Voyage timers + `units_spiral_place_hs_near` / `ai_europe_exit_to_map` (split) |
| Duration roll `0002` | Bound-ship / landfall timers (partial) |

Sources: [`src/core/game_loop.c`](../../src/core/game_loop.c).

## Sibling: SoL / king from `00f2`

| Call | Arg | Map |
|------|-----|-----|
| `291f_0a66`→`43f7_2424` @58392 | **`iVar1` = `DS:0x5394` nation id** | [`ai/king_ref.md`](../ai/king_ref.md) |
