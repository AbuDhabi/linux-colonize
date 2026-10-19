# Americas idle campaign (`golden_idle_campaign america`)

STATUS: alignment started 2026-10-19. Not a CTest gate yet. Sister campaigns (done):
[idle_campaign.md](idle_campaign.md), [idle_independence.md](idle_independence.md), whose method,
ignore list and DOSBox recipes apply here too.

## Fixtures

`original_saves/golden_idle_america_seed-100/` (readme.md there): VR_SOUND.EXE (seed 100, no
music picks), original Americas map, Spanish human (`head.human_player = 2`). Saves
`000001`..`000465` are every-turn autosaves (COLONY09), turn 9 (1501) .. turn 507 (1799). Turns
whose autosave went to the decade slot are missing (33 gaps, for example 17 -> 19); the harness
plays through them. The archive is one continuous DOS session from a load of 000001; the player
only presses Space.

Human slot 2 means England (0) and France (1) act after the day-top spent-MP clear and before
the autosave, so their spent bytes are compared; the Netherlands (3) act after it.

## Running

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america 1 465
./build/debug/golden_idle_campaign america N-1 N
```

The whole archive takes a few minutes in Release, longer in Debug; use `america 1 N` for a prefix.
Single pairs from a DOS save are hints only: a load drops unsaved DOS state, and 459 of 464
pairs differ (2026-10-19), most of them past save 80 by hundreds of fields.

### Resync (triage past the first divergence)

```
GOLDEN_IDLE_RESYNC=1 GOLDEN_IDLE_RESYNC_SAVE=/tmp/x/start_%d.sav GOLDEN_IDLE_KEEP_GOING=1 \
  GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav ./build/debug/golden_idle_campaign america 1 465
GOLDEN_IDLE_FROM=/tmp/x/start_K.sav ./build/debug/golden_idle_campaign america K N   # one pair
```

Before every step from an archived save K the harness swaps the run's DOS sections for save K
and keeps the run's own COLNXEXT tail (RTST: stance and goal tables, unit tail bytes, path and
rank caches, fog, LCG). Each pair is then judged from DOS's state plus the port's best model of
the unsaved tables, so a diff names a bug of that turn, not the cascade of an earlier one. Every
nation slot reseeds the LCG (DS:0x83a6 = 100 under VR_SOUND), so the carried LCG does not matter
and a replay from `start_K.sav` reproduces the pair (except where a port static survives the
load, see the reload lead below). HEAD 97a502f9: 47 of 464 pairs byte-identical (fresh loads: 6); 61 after the 2026-10-19 fixes below.

Resync artifacts, not bugs: bytes DOS keeps in memory past `unit_count` are not in the save, so a
unit created this turn into a reused slot carries the port's guess of the previous occupant's
leftovers (goto, facing, holds at or past `holds_occupied`, hold[5] 100/80/236/216 on non-
Pioneers); `.units[].vis_mask` and `.map.path_hex` diffs are mostly downstream of a unit that
moved differently.

## Divergence log

First continuous run (HEAD f0cc3bb4, 2026-10-19): saves 2..5, 7, 9, 10 byte-identical; small
diffs through save 26; hundreds of fields from save 27. After round 1: 2..20 byte-identical; 2..22 after the 5d04 retype fix; 2..26 after the 0def else-limb; 2..27 after the LCR burial fix; 2..31 after the save-28 fixes; 2..37, 39, 40 after the save-32 haul fix; 2..40 after the dock sentry fix; 2..42 after the 0a60 colony record index; 2..43 (and 54..56) after the treaty-cooldown attack.

| Save | Class | State |
|---|---|---|
| 6, 11, 13, 18 | New England/France units carry spent MP 3/12, DOS 0 | FIXED: Europe immigrant / 5c3c ship buy / 5d04 hire / Artillery spawn set `moves = 0`; FUN_1427_06b4 raw 7716 leaves +0x3149 spent at 0, so the unit keeps `units_max_mp` |
| 8 | `nations[0].rebel_sentiment` 5 vs 3 (self-heals at 9) | FIXED: `ai_king_sol_percent` counted the Col1 record of a colony the pool abandoned that tick (DOS deleted it, raw 58154). Lead: a colony founded earlier in the same turn has no Col1 record until export, so it is not counted (does DOS count it?) |
| 12..26 | `indians[7].alarm_by_player[1]` 0 vs 7 | FIXED: FUN_15eb_06d2 reads the 26e4 claim table built at the colony tick top, not the live map, and 07c2 prices without the purchased test; a Pioneer's purchase earlier in the tick still raises alarm (colony.c claim snapshot) |
| 19..23 | England's ship 0 reaches the high seas one turn late | FIXED: at an own berth 20e6's local_a8 is the carrier stack (0920 parks hull + boarded at -2,-2), not the colony tile's residents, so an emptied hull takes the 457e sail-home arm |
| 21, 22 | AI unit re-typed in Europe keeps old MP (spent 9 vs 0) | FIXED: the 5d04 hire ladder's type write (raw 92770-92783) leaves +0x3149 spent alone; `units_set_type_keep_spent` |
| 23..26 | `tribes[51].alarm[0].friction` 6 vs 0 | FIXED: a non-generous Brave visit after another village's generous one (`contact_state` 2) takes LAB_5bfb_0def's else-limb (FUN_5bfb_022e raw 96981-96986) and zeroes the visiting village's word; `ai_contact_try_village_reparations` skipped it |
| 27 | DOS has one more unit, tribe frictions | FIXED: Dutch dragoon's LCR burial mounds. `units_lcr_nearest_tribe_dist` used Manhattan + first-wins; FUN_4cc6_0356 (raw 80944-80987) uses `map_dos_dist` octile and `<=` (last wins), shifting the claim draw. The +100 alarm goes through the whole FUN_4cc6_00f2 (rand(0,10) escalation tail), not the bare relation delta |
| 28 | Brave facings, unit 64 path | FIXED (DOSBox from-load 27 trace): (1) FUN_5fef_1b0e raw 100753-100758: on an attacker loss the winner's promotion roll precedes the loss outcome and raid handoff; (2) the 20e6 wander fog enable (bVar20, raw 88620) calls FUN_2a1f_047c with 1, not 0, so adjacent Braves switch the far-probe fog terms off |
| 32, 33 | England Caravel units[58] full of sugar/furs roams instead of sailing home | FIXED: port-only early return in `ai_euro_try_ship_trade_haul_snapshot` (no tools/lumber/ore/muskets/horses/food and no free hold -> 0) skipped DOS's occupancy gate (raw 2166-2168 -> LAB_3fa6, sail home 'E') |
| 38 (resync 47, 69, 104) | 8 new England colonists in Europe (ai_plan 'X') get orders 0, DOS 1 | FIXED: the AI Fountain of Youth spawns its 8 dock colonists through `europe_nation_harbor_spawn`, which skipped FUN_38fd_0718 raw 59133 `+0x314c = 1` (SENTRY). The 5e52 immigrant got it back from the next 0a60; a spawn after the nation's 0a60 reaches the autosave without it |
| resync 44, 49, 52, 54, 76 | The 5d04 retype (Soldier, Dragoon, Pioneer, Missionary) lands on another dock colonist | FIXED: the dock list walk (`ai_euro_5d04_cb_list_iter_first/next`) went by slot; DOS raw 92627-92630 walks the tile chain newest first (FUN_281f_07e0 = 1427_005c + 0002 to the +0x315c end, FUN_281f_02e4 = 1427_004a via +0x315e) |
| resync 160+ | Lowest-slot units of a nation with more than 64 movers never act (keep MP, 0a60 orders 10) | FIXED: `ai_euro_dispatcher_turn_unit_waves` stopped after 64 outer passes, but each pass drains one wave-1 unit and FUN_521d_6d8e has no pass limit (pair 166 -> 167: England's slots 67, 62, 60, 52, 49). Biggest late-game class (signatures `o(10,0)` French Dragoons 854 unit-pairs, `o(10,6)` Soldiers 114, fortified English Soldiers' spent MP 117) |
| 41, 42 (reload from 47) | England unit 66 goto (50,41) vs DOS back at (45,40); 42 blows up (Dutch colony 3, unit count) | FIXED: 0a60's MILITARY approach parity `((i + turn) & 3)` used the colony pool slot; DOS raw 87778 uses the record index (local_3e). DOS compacts the colony array on a delete (caseD_7 raw 58154-58166), the pool keeps holes: England's (46,30) abandoned at save 8 left slot 0 empty, so goals went to the wrong nations from then on (a reload compacted the pool, hence reload != continuous from 47) |
| resync 43, 46, 56, 59 | DOS drops PEACE for one turn (0x60 -> 0x20 both ways) around an AI Euro-vs-Euro fight the port never fights | FIXED: FUN_465b_0000 raw 75553-75557 aborts an attack on a treaty partner only while the attacker's cooldown `nation.treaty_timer[def]` runs (re-armed to (6-difficulty)*2 by the 153e LAB_30de tail, decremented in the 6d8e prelude); the band (raw 75567-75593) clears 0x40 both ways and the attacker's 0x01, writes no WAR bit for a computer attacker; FUN_5bfb_13b0 re-signs later (`ai_diplo_465b_band`, `units_at_war_for_move_target`, `ai_euro_try_attack`) |
| resync 43 | Dutch Soldier with 1 third left beside capital (47,47) slams Indian alarm 80 -> 100 | FIXED: 465b raw 75505-75508 sends a computer/native mover with < 3 thirds to LAB_465b_01ce before the Privateer bit, the treaty band and the alarm slam; the port exited last |
| resync 57, 60 | French Dragoon keeps goto (50,41) in DOS, port zeroes it | FIXED: FUN_5bfb_12d0 raw 97248-97250 (treaty wake) writes +0x314c = 0 only; the port called `units_clear_orders`, which also clears the goto |
| resync 105-119, 151+ | `nations[].trade.*` (one market volume `nr` mirrored into all 4 records) | FIXED (one cause): the 20e6 delivery tally `acStack_c8` is a signed char that wraps (`ADD byte`, overlays.asm:136273, read with CBW); the port clamped at 127, so two 100-lots failed every colony's Horses gate and the ship dump-sold its hold. The market bookkeeping itself matches DOS (FUN_38fd_1d80/1dfa); remaining trade diffs are different purchases or sales |
| resync 228 (unit count at the 0x124 cap) | DOS burns two villages, the port's Dragoons walk onto the village tiles | FIXED: FUN_478c_002c (raw 76545-76564) builds the phantom defender by bumping DS:0x539c itself, without FUN_1427_06b4's room gate; the four port phantom spawns (village, colony, two Pioneer work phantoms) refused at the cap, so the attack became a plain move |
| reload | DS:0x8dc4 (`units_last_hold_amount`) and colony plot masks lost or misplaced across a load | FIXED: RTST records `RT_HOLD_AMOUNT` and `RT_COLONY_PLOTS_REC` (record order; the old per-slot `RT_COLONY_PLOTS` is no longer read); `units_reset_state` zeroes 8dc4. Resync pair 58 (ship buys 100 tools vs 16 of cargo 1) depended on it |
| 44 (continuous) | Dutch Merchantman u4 facing 3 vs 4, path visitor nibble at (53,47) 0x31 vs 0x11 | analysing |
| 28+ | not analysed | deferred to the next continuous pass |

Open leads from the 2026-10-19 resync pass (not fixed):
- Reload gaps (continuous != reload): passenger `cargo_ids` order is port-invented (the save keeps
  only the chain; `units_disembark_all` takes `cargo_ids[0]` first, units_cargo.c); the
  `col1_hold_raw` stale-byte mirror is refreshed only at load, so a hull that loses its cargo
  writes back old bytes, while DOS FUN_5fef_0352 raw 99616-99620 zeroes only +0x3150 (the
  continuous run is the wrong one); `repair_pending` (units.h) is not carried; the LCR counters
  DS:0x1dc6/0x1dc7 (`s_lcr_explored_total`, `s_lcr_cibola_total`) are not carried. The in-process
  reload gate cannot see statics; a fresh-process replay of a resync start file can.
- 28c8 seats New Amsterdam's rejoining veteran as a second Lumberjack on tribe land (buys it for
  200) where DOS makes him a Fisherman (resync 57: lumber shortfall/unmet arm raw 13081-13093).
- 465b's treaty band is missing on the no-defender colony path (raw 75467-75475), PEACE|WAR pairs
  are checked "at war" first, and @DECLAREWAR (raw 75585) is not shown for computer-attacker breaks.
- FUN_15eb_317c raw 13351 writes DS:0x8dc4 for any valid slot; the port skips amounts 0 and 255.

Pair-only (deferred, not seen in the continuous run): pairs 16..26 show
`colonies[5].ai_flags.short_defenders` True vs False and `specialty_cargo` 15 vs 8 from a load;
pairs 3..5 miss a colonist joining colony 1. Likely unsaved DOS state; revisit only if the
continuous run shows them.
