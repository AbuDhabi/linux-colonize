# ai_euro.c manual-only citation hits — triage list

STATUS: open triage list; second wave resolved 2026-09-18 (see "Resolved" section).

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
| `ai_euro_foreign_land_threat_near` | 15933-15934 | helper |
| `ai_euro_land_engage_then_hunt` | 17626 | arm |
| `ai_euro_act_land_hunt_scout` | 18872, 18890, 18895, 18915, 18917-18918 | arm (6 hits — LCR, wake, board, fortify-defense) |
| `ai_euro_act_land_fortify` | 19196, 19202, 19223-19224 | arm + 1 note (19196) |

## Ship

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_unit_act` | 19895 | note |

## Resolved 2026-09-18 (second wave)

- `ai_euro_land_war_hunt_target` — **invented, deleted**. No DOS distant land
  hunt exists, the same finding that retired the naval hunt; DOS aims a land
  unit only through a FUN_521d_0a60 goal or the LAB_521d_4d2e wander step.
- `ai_euro_land_best_adjacent_foe` + `ai_euro_land_try_adjacent_attack` —
  **no DOS counterpart, kept as a marked GOLDEN-BACKED STAND-IN**. DOS scores
  an adjacent foe inside the LAB_521d_4d2e attack term (raw 88880-88940) and
  resolves it through FUN_465b_0000. The port has that term, but it requires
  `owner >= 0 && at_war` from the destination tile's layer3 owner nibble, so a
  foe on an unclaimed tile is never scored as a target. Deleting the pair left
  `golden_woi_ref01`'s REF unable to reduce two **defended** colonies (the
  `0x46`/`0x4c` seizes of undefended settlements still worked), i.e. the
  invented arm was papering over that gap. Both functions now carry a
  stand-in header naming the real fix (make the 4d2e attack term reachable on
  land and/or bind every eligible unit to a 0a60 MILITARY goal instead of one).
- `ai_euro_act_ship_war_trade` "fandom Drake" Privateer re-aim — **already
  dead**: the code went with the naval hunt earlier on 2026-09-18 and only the
  comment survived. Comment rewritten, no behaviour change.
- `ai_euro_colony_threatened_by_war` + `ai_euro_threatened_stockade_near` and
  their two Stockade LABOR arms (goal-side override + LABOR ladder bind) —
  **invented, deleted**. The goal table is written only by FUN_521d_0a60 (its
  'A' mark block spends colony +0x1e garrison_quota / +0x8e labor_shortage,
  neither a building-choice term) and the AI's one construction picker is the
  FUN_5952_035e cascade, which reads the colony's own +0x1b flags — nothing in
  DOS re-aims labour at a Stockade because an enemy is near.

Also removed: `ai_euro_colony_fort_bonus_at` came back with the stand-in
picker; `ai_euro_land_engage_then_hunt` is now `ai_euro_land_engage_adjacent`.
Tests deleted with the arms: `unit_land_war_hunt`,
`unit_land_war_hunt_multistep`, `unit_continental_army_land_hunt`,
`unit_continental_cavalry_land_hunt`, `unit_land_hunt_prefer_treasure`,
`unit_land_hunt_prefer_weak`, `unit_stockade_threat_labor`.
`unit_land_adjacent_combat_chain` is kept as the "AI soldier adjacent to an
enemy at war does attack" demonstration.

## Counts

- arm: 18 functions (28 hits)
- helper: 11 functions (13 hits)
- note: 5 functions (7 hits)

Highest-value next targets: `ai_euro_act_land_goal_consume` (7 hits, the
Skills-Chart profession→LABOR ladder), `ai_euro_act_land_hunt_scout` (6 hits),
`ai_euro_act_land_fortify` (3 arms), and the treasure cluster
(`ai_euro_try_treasure_board_sail` / `ai_euro_try_cash_treasure_europe` /
`ai_euro_try_expected_treasure_harbor`). Separately: making the LAB_521d_4d2e
attack term reachable for land units, which would retire the stand-in above.
