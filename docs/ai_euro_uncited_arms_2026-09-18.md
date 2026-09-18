# ai_euro.c manual-only citation hits — triage list

STATUS: open triage list (no investigation done; for the coordinator to schedule a sweep)

Scope: every `Colonization.pdf` / `manual` / `fandom` / `Skills Chart` /
`euro_unit_act §` citation left in `src/core/ai_euro.c` after the 2026-09-18
unit-act wave (bugs.md #512-#519). 36 functions, 55 hits.

Tags: **arm** = code an act stage calls, decision is manual-cited ·
**helper** = predicate/target-picker feeding an arm · **note** = comment-only
(retirement note or a cross-reference; no live manual-cited decision).

## Colony / goal side

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_colony_wants_construction_labor` | 1650 | helper |
| `ai_euro_colony_near_warehouse_cap` | 1683 | helper |
| `ai_euro_colony_wants_lumberjack_labor` | 1867 | helper |
| `ai_euro_is_military_name` | 1919 | note |
| `ai_euro_unit_is_food_labor` | 4487 | helper |
| `ai_euro_colony_inventory` | 5728 | helper |
| `ai_euro_colony_goals_tribe_seeds` | 10363 | arm |
| `ai_euro_colony_goals` (Stockade-threat LABOR deepen) | 10668-10669 | arm |
| `ai_euro_colony_threatened_by_war` | 15295 | helper (sole surviving caller pair: the two Stockade LABOR arms above) |
| `ai_euro_act_land_goal_consume` | 19318, 19322, 19329, 19376, 19382, 19390, 19437 | arm (7 hits — Skills-Chart profession→LABOR ladder) |
| `ai_euro_act_land_goal_dispatch` | 19622, 19683 | note |

## Treasure

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_europe_sail_target` | 2161 | helper |
| `ai_euro_treasure_coast_target` | 2214 | helper |
| `ai_euro_try_treasure_board_sail` | 4588 | arm |
| `ai_euro_cash_one_treasure` | 4652 | helper |
| `ai_euro_try_cash_treasure_europe` | 4702 | arm |
| `ai_euro_try_expected_treasure_harbor` | 4758 | arm |
| `ai_euro_unload_settle` (Treasure stays aboard) | 17002 | arm |
| `ai_euro_act_land_treasure` | 18994 | arm |

## Missionary

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_missionary_should_flee` | 2295-2296 | helper |
| `ai_euro_missionary_no_mission_target` | 2330 | helper |

## Pioneer / wagon / found

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_try_wagon_haul` | 5291 | arm |
| `ai_euro_pioneer_improve_target` | 5332, 5380 | helper |
| `ai_euro_try_pioneer_improve` | 5410 | arm |
| `ai_euro_found_with_unit` | 5502, 5514 | arm (Minuit land-purchase gate) |
| `ai_euro_act_land_roles` | 19066-19067, 19089 | arm |

## Land combat / fortify

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_move_scoring_gate` | 13312 | arm |
| `ai_euro_land_war_hunt_target` | 15483 | helper (distant land hunt — same shape as the naval hunt retired 2026-09-15) |
| `ai_euro_land_best_adjacent_foe` | 15658 | helper (twin of the naval picker deleted 2026-09-18) |
| `ai_euro_land_try_adjacent_attack` | 15905 | arm |
| `ai_euro_foreign_land_threat_near` | 15933-15934 | helper |
| `ai_euro_land_engage_then_hunt` | 17626 | arm |
| `ai_euro_act_land_hunt_scout` | 18872, 18890, 18895, 18915, 18917-18918 | arm (6 hits — LCR, wake, board, fortify-defense) |
| `ai_euro_act_land_fortify` | 19196, 19202, 19223-19224 | arm + 1 note (19196) |

## Ship

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_act_ship_war_trade` (Privateer re-aim, "fandom Drake") | 18409 | arm |
| `ai_euro_unit_act` | 19895 | note |

## Counts

- arm: 21 functions (32 hits)
- helper: 13 functions (16 hits)
- note: 5 functions (7 hits)

Highest-value next targets (twins of arms already proven invented today):
`ai_euro_land_best_adjacent_foe` + `ai_euro_land_try_adjacent_attack` and
`ai_euro_land_war_hunt_target` (the land copies of the naval picker/hunt
retired 2026-09-15/18 — DOS scores land foes only in LAB_521d_4d2e), and the
Privateer re-aim in `ai_euro_act_ship_war_trade`.
