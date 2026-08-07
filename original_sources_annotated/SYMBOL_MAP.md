# Symbol map — Ghidra ↔ annotated ↔ Linux

Phase 1 AI-critical symbols + Euro early-settle Layer D. Prefer annotated
files when listed; otherwise use `original_sources_decompiled/viceroy_unpacked.c`.

## Functions

| Ghidra | Annotated name | File | Linux counterpart |
|--------|----------------|------|-------------------|
| `FUN_281f_04ca` | `ai_reseed_from_timer` | `ai/accessors.c` | `ai_nation_reseed` / `dos_rng` via timer word |
| `FUN_281f_04d4` | `rng_range` | `ai/accessors.c` | `ai_rng_range` / `dos_rng_range` |
| `FUN_281f_072c` | `tile_has_minor_river` (via `terrain_byte`) | `ai/accessors.c` | terrain `&0x40` in `ai_native_pick_dir` / `ai_dos_move_spent` |
| `FUN_137f_010e` | `terrain_byte` | `ai/accessors.c` | `ai_terrain_at` |
| `FUN_281f_0754` | `tile_fa_flags` | `ai/accessors.c` | `ai_mask_fa_flags` |
| `FUN_137f_0142` | `layer2_byte` | `ai/accessors.c` | `ai_layer2_at` |
| `FUN_281f_0768` | `ocean_or_high_seas` | `ai/accessors.c` | `ai_is_ocean_hs` |
| `FUN_13e4_0074` | `ocean_or_high_seas` (real body) | `ai/accessors.c` | `ai_is_ocean_hs` |
| `FUN_281f_06b4` | `continent_id` | `ai/accessors.c` | `ai_continent_id` |
| `FUN_137f_01ca` | `continent_id` (real body) | `ai/accessors.c` | `ai_continent_id` |
| `FUN_137f_01ac` | `layer3_byte` | `ai/accessors.c` | layer3 read |
| `FUN_137f_0194` | `layer3_ptr` | `ai/accessors.c` | — |
| `FUN_137f_0228` | `set_owner_nibble` | `ai/accessors.c` | `ai_set_owner_nibble` |
| `FUN_13e4_000e` | `decode_terrain_class` | `ai/accessors.c` | `ai_dos_terr_class` |
| `FUN_124c_0040` | `dos_dist` | `ai/accessors.c` | `ai_dos_dist` |
| `FUN_465b_0000` | `move_spent_add` / `move_spent_cost_head` | `ai/move_spent.c` (+ cost head in `accessors.c`) | `ai_dos_move_spent` + pulse ocean force |
| `FUN_281f_0696` / `FUN_137f_0358` | `euro_settlement_owner` | `ai/accessors.c` / `move_spent.c` | ocean force-to-max gate |
| `FUN_281f_090c` / `FUN_1427_065a` | `unit_max_mp` | `ai/unit_mp.c` | Brave table byte (=3) |
| `FUN_281f_097a` / `FUN_1427_13b0` | `unit_has_moves_remaining` | `ai/unit_mp.c` | pulse `spent < max_mp` |
| `FUN_281f_0934` / `FUN_1427_155e` | `unit_exhaust_mp` | `ai/unit_mp.c` | spent=max_mp |
| `FUN_281f_0916` / `FUN_1427_12f6` | `unit_tile_list_refresh` | `ai/unit_mp.c` | post-ADD; no 3149 |
| `FUN_281f_0948` / `FUN_1427_040c` | `stack_set_xy` | `ai/unit_mp.c` | post-ADD; no 3149 |
| `FUN_281f_08da` / `FUN_1427_0968` | `stack_facing_refresh` | `ai/unit_mp.c` | post-ADD; no 3149 |
| `FUN_281f_084e` / `FUN_1427_0ce6` | `unit_post_move_chrome` | `ai/unit_mp.c` | post-ADD; no 3149 |
| `FUN_281f_07fe` / `FUN_1427_0c72` | `unit_visibility_bits` | `ai/unit_mp.c` | post-ADD; no 3149 |
| `FUN_4d56_152e` | `village_growth_accum` | `ai/indian_nation_turn.c` | `ai_grow_villages` |
| `FUN_4d56_1816` | `indian_nation_turn` | `ai/indian_nation_turn.c` | `ai_indian_nation_turn` |
| `FUN_4d56_14fe` / `func_0x00042191` | `indian_unit_act` (14fe; Ghidra 42191 overlay collision) | `ai/indian_nation_turn.c` | quiet path in `ai_native_nation_pulse` |
| `func_0x0004219b` | `indian_pick_dir` | `ai/indian_nation_turn.c` | → `quiet_brave_pick_dir_asm` |
| (callgraph) | — | `ai/brave_spent_callgraph.md` | spent `0x3149` writers |
| quiet `20e6` / `LAB_521d_4ea9` | `quiet_brave_pick_dir_asm` | `ai/quiet_brave_scoring.c` | `ai_native_pick_dir_asm` via `AI_QUIET_ASM=1` |
| Init A/B dumps | `AI_LCG_AUDIT` / `AI_AB` / `AI_SCORE_DUMP` | `src/core/ai.c` | phase 7–8: fog `+8` flips `(47,53)`; far tiles AGREE SAV |
| `AI_ASM_STAY_SYNC` | audit stay-shaped +1 next | `src/core/ai.c` | matched RNG for score dump only |
| `LAB_521d_54f5` gate | `quiet_lab_54f5_gate` | `ai/quiet_brave_scoring.c` | not in Linux (reverted) |
| military −10 | `quiet_score_military_minus10` | `ai/quiet_brave_scoring.c` | diplomacy stub 0 |
| (apply step) | `quiet_brave_apply_step` | `ai/indian_nation_turn.c` | `ai_native_apply_step` |
| `FUN_281f_04d4(1,3)` quiet base | `quiet_score_base` | `ai/quiet_brave_scoring.c` | empiricism uses range(1,5)+stay |
| Init LCG audit | `AI_LCG_AUDIT=1` logs | `src/core/ai.c` | stay surplus +34 on init |
| quiet terrain ± | `quiet_score_terrain` | `ai/quiet_brave_scoring.c` | empiricism river/fa / home |
| quiet facing `−diff²×2` | `quiet_score_facing` | `ai/quiet_brave_scoring.c` | empiricism +4/−6/+3 |
| `bVar20` fog/explore | `quiet_score_fog_explore` | `ai/quiet_brave_scoring.c` | not in Linux pick_dir yet |
| `LAB_521d_52aa` colony pull | `quiet_score_colony_pull` | `ai/quiet_brave_scoring.c` | no-op early game |
| `FUN_281f_0302` | `map_tile_in_bounds` | `ai/accessors.c` | map inset |
| `FUN_281f_074a` / `0x168` | `tile_explore_mask` | `ai/accessors.c` | Euro-only +2; Indians skip |
| DS:`0x9faa` / `−0x6056` | `VICEROY_DS_COARSE_FOG` (size `0x10e`) | `viceroy_globals.h` | Linux `s_ai_coarse_fog` |
| explore `(x>>2)+(y>>2)*18` | `coarse_fog_explore_index` / `coarse_fog_unseen` | `ai/accessors.c` | +8 gate in quiet ASM |
| tribe `(y/5)+(x/5)*18` | `coarse_fog_tribe_index` | `ai/accessors.c` | `ai_coarse_fog_mark_tribe` |
| `FUN_281f_0682` / `0314` | `tile_owner_or_presence` | `ai/accessors.c` | **layer2 bit0** then owner |
| `FUN_281f_06d2` / `0428` | `tile_tribe_or_presence` | `ai/accessors.c` | tribe else presence |
| `FUN_281f_07e0` | `unit_index_on_tile` | `ai/accessors.c` | Linux unit-pool scan (cutover) |
| `FUN_281f_078c` | `terrain_class_at` | `ai/accessors.c` | `ai_dos_terr_class` |
| `FUN_521d_0000` | `clear_primary_goal_slot` | `ai/euro_goals.c` | — |
| `FUN_521d_001c` | `invalidate_nearby_secondary_goals` | `ai/euro_goals.c` | — |
| `FUN_521d_0072` | `primary_goal_shift_down` | `ai/euro_goals.c` | — |
| `FUN_521d_00a8` | `secondary_goal_shift_down` | `ai/euro_goals.c` | — |
| `FUN_521d_00de` | `work_queue_shift_down` | `ai/euro_goals.c` | — |
| `FUN_521d_0116` | `max_primary_goal_priority` | `ai/euro_goals.c` | — |
| `FUN_521d_016a` | `upsert_primary_goal` | `ai/euro_goals.c` | — |
| `FUN_521d_0214` | `upsert_secondary_goal` | `ai/euro_goals.c` | — |
| `FUN_521d_02be` | `upsert_work_queue` | `ai/euro_goals.c` | — |
| `FUN_521d_031c` | `clear_work_queue` | `ai/euro_goals.c` | — |
| `FUN_521d_0342` | `promote_secondary_to_primary` | `ai/euro_goals.c` | — |
| `FUN_521d_03a6` | `clear_secondary_goal_slots` | `ai/euro_goals.c` | — |
| `FUN_521d_03d0` | `founding_expansion_urgency` | `ai/euro_goals.c` | — |
| `FUN_521d_0492` | `colony_count_balance_flags` | `ai/euro_goals.c` | — |
| `FUN_521d_052c` | `unit_desirability_score` | `ai/euro_goals.c` | — |
| `FUN_521d_0600` | `composite_unit_priority` | `ai/euro_goals.c` | — |
| `FUN_521d_0656` | `walk_unit_stack_to_end` | `ai/euro_goals.c` | — |
| `FUN_521d_06ae` | `pick_best_adjacent_founding_tile` | `ai/euro_goals.c` | `ai_euro_found_tile_from_landfall` (PORT DEBT) |
| `FUN_521d_0896` | `filter_profession_by_distance_wealth` | `ai/euro_goals.c` | — |
| `FUN_521d_0906` | `probe_adjacent_contact_claim` | `ai/euro_goals.c` | — |
| `FUN_521d_20e6` | `move_scoring` (Euro/ocean thin; quiet done) | `ai/euro_dispatcher.c` + `ai/move_scoring.md` | quiet only |
| `FUN_521d_6d8e` | `euro_nation_turn` | `ai/euro_dispatcher.c` | `ai_euro_nation_turn` |
| `FUN_521d_0a60` | `euro_unit_colony_goals` (sectioned; mid-game PARKED) | `ai/euro_dispatcher.c` | early peels only |
| `FUN_521d_5d04` | `euro_nation_planning` (parked) | `ai/euro_dispatcher.c` | — |
| `FUN_521d_5b66` | `euro_unit_act` (thin map) | `ai/euro_dispatcher.c` + `ai/euro_unit_act.md` | `ai_unit_spend_goto` / peels |
| `thunk_FUN_2a1f_0488` | → `euro_unit_act` / `5b66` | `ai/euro_dispatcher.c` | peels |
| `thunk_FUN_2a1f_0554` | → `euro_nation_planning` / `5d04` | `ai/euro_dispatcher.c` | — |
| `thunk_FUN_2a1f_0578` | → `promote_secondary_to_primary` / `0342` | `ai/euro_dispatcher.c` | — |
| `thunk_FUN_2a1f_050c` | → `euro_unit_colony_goals` / `0a60` | `ai/euro_dispatcher.c` | — |
| `thunk_FUN_2a1f_04f4` | → `move_scoring` / `20e6` | `ai/euro_unit_act.md` | — |
| `thunk_FUN_2a1f_04ac` | → `pick_best_adjacent_founding_tile` / `06ae` | `ai/euro_goals.c` | — |

## DS addresses / globals

| Address | Annotated name | Meaning |
|---------|----------------|---------|
| `0x015c` | `VICEROY_DS_MAP_TERRAIN_PTR` | Terrain plane base |
| `0x0160` | `VICEROY_DS_MAP_LAYER2_PTR` | Layer2 / flags base |
| `0x0164` | `VICEROY_DS_MAP_LAYER3_PTR` | Layer3 continent/owner base |
| `0x2f76` | `VICEROY_DS_TERR_COST_TABLE` | Terrain class cost bytes |
| `0x2d12` | `VICEROY_DS_EURO_STICKY_UNIT` | Euro act anti-spin unit index |
| `0x2d14` | `VICEROY_DS_EURO_STICKY_CNT` | Euro act anti-spin counter |
| `0x3144` | `VICEROY_DS_UNITS_BASE` | `units[0].x` |
| `0x3146` | `ViceroyUnit.type` | unit type byte |
| `0x3147` | `ViceroyUnit.nation_id` | low nibble |
| `0x3149` | `ViceroyUnit.moves_spent` | MP spent this turn |
| `0x315a` | `ViceroyUnit.act_counter` | Col1 `turns_worked`; 1816 act loop |
| `0x5382` | `VICEROY_DS_GAME_FLAGS` | bit0 = NEW WORLD path in 1816 |
| `0x5394` | `VICEROY_DS_ACTIVE_NATION` | Current AI nation id |
| `0x5396` | `VICEROY_DS_HUMAN_NATION` | Human nation |
| `0x5398` | `VICEROY_DS_FOCUS_NATION` | Focus / camera nation |
| `0x539a` | `VICEROY_DS_TRIBE_COUNT` | Live tribe count |
| `0x539c` | `VICEROY_DS_UNIT_COUNT` | Live unit count |
| `0x539e` | `VICEROY_DS_COLONY_COUNT` | Live colony count |
| `0x53a6` | `VICEROY_DS_DIFFICULTY` | Difficulty byte (alarm RNG) |
| `0x54ee` | `VICEROY_DS_TRIBES_BASE` | `tribes[0].x` |
| `0x83a6` | `VICEROY_DS_TIMER_WORD` | LCG reseed (VR_SEED→100) |
| `0x8d4a` | `VICEROY_DS_CUR_TRIBE_PTR` | Current tribe pointer |
| `0x8d4e` | `VICEROY_DS_INDIAN_STATE_PTR` | Per-Indian nation state |
| `0x8d50` / `0x8d52` | `VICEROY_DS_CUR_INDIAN_*` | Alarm / contact helpers |
| `0x98b0` | `VICEROY_DS_AI_PRIMARY_GOALS` | Primary goals nation×64×4 (−0x6750) |
| `0x9eaa` | `VICEROY_DS_AI_SECONDARY_GOALS` | Secondary goals nation×16×4 (−0x6156) |
| `0xa0dc` | `VICEROY_DS_AI_WORK_QUEUE` | Work queue 16×6 (−0x5f24) |
| `0x9faa` | `VICEROY_DS_COARSE_FOG` | Coarse fog plane (also −0x6056) |

## Map / terrain bits

| Mask / value | Annotated | Linux |
|--------------|-----------|-------|
| terrain `&0x1f` | `VICEROY_TERRAIN_TYPE_MASK` | type decode |
| terrain `&0x20` | `VICEROY_TERRAIN_HILL_BIT` | hill class |
| terrain `&0x40` | `VICEROY_TERRAIN_RIVER_BIT` | minor river |
| type `0x19` / `0x1a` | ocean / high seas | `ai_is_ocean_hs` |
| layer2 `&0x0a` | `VICEROY_LAYER2_FA_MASK` | `ai_mask_fa_flags` |
| layer2 `&0x02` | `VICEROY_LAYER2_TRIBE` | tribe tile |
| layer3 high nibble `0xf` | `VICEROY_OWNER_UNOWNED` | owner −1 |

## Phase checklist

- [x] `original_sources_annotated/README.md` + docs pointers
- [x] `include/viceroy_types.h` + `include/viceroy_globals.h`
- [x] AI-hot accessors named in `ai/accessors.c`
- [x] `indian_nation_turn` structure readable without raw `0x….` in that file
- [x] Quiet Brave dir-pick / apply-step annotated (with PORT DEBT markers)
- [x] `indian_unit_act` as `FUN_4d56_14fe` (Ghidra `func_0x00042191` collision noted)
- [x] Post-ADD chrome → `FUN_1427_*` + `0x3149` R/W table (`move_spent.c` §6, `unit_mp.c`)
- [x] `ai/brave_spent_callgraph.md`
- [x] `euro_nation_turn` dispatcher shell (thunk wiring corrected)
- [x] `ai/euro_goals.c` goal tables + founding helpers
- [x] `FUN_521d_0a60` sectioned (mid-game PARKED)
- [x] `ai/euro_unit_act.md` + Euro/ocean notes in `move_scoring.md`
- [x] This symbol map

## Out of scope (still raw export only)

- `FUN_4d56_2154` / `2820` / `4528` raid clusters
- Full `FUN_521d_20e6` Euro/ocean/combat bodies (quiet + thin map done)
- Full `FUN_521d_5b66` order/combat arms (entry + thin map done)
- Full `FUN_521d_5d04` hire/treasury planning
- `FUN_465b_0000` foreign combat / diplomacy / colony-contact tails (section 3 PARKED in `move_spent.c`)
- Ghidra database renames / re-export
- Live DOS hang EXEs except named last resort **VR_B465X** (`dump_b465x3`) for post-`465b` spent writer
