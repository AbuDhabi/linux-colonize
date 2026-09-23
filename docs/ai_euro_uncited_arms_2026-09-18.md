# ai_euro.c manual-only citation hits — triage list

STATUS: open triage list; second wave resolved 2026-09-18, plus the 2026-09-18f
5b66/goal-walk pass (bugs.md #525/#526) — see the two "Resolved" sections.
2026-09-22: the adjacent-attack stand-in pair discussed below is deleted
(bugs.md #521 FIXED — `0x5239` is the @UNIT cost column, not a zero column).

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
| ~~`ai_euro_colony_wants_lumberjack_labor`~~ | — | **DELETED 2026-09-23 (bugs.md #890)** — invented Expert-Lumberjack LABOR bind, no DOS body; the real forced-lumberjack arm is FUN_5952_035e raw 94659-94679 in `ai_euro_colony_jobs.c` |
| `ai_euro_is_military_name` | 1919 | note |
| `ai_euro_unit_is_food_labor` | 4487 | helper |
| `ai_euro_colony_inventory` | 5728 | helper |
| `ai_euro_colony_goals_tribe_seeds` | 10363 | arm |
| `ai_euro_act_land_goal_consume` | 19318, 19322, 19329, 19376, 19382, 19390, 19437 | arm (7 hits — Skills-Chart profession→LABOR ladder) |
| `ai_euro_act_land_goal_dispatch` | 19622, 19683 | note |

## Treasure — **RESOLVED 2026-09-23 (bugs.md #745/#746): deleted, not parked**

Every row below was a Linux invention. `FUN_521d_20e6`'s treasure band (raw
89997-90040) is the only AI treasure code in DOS: cash at ANY own colony
untaxed → destroy; else walk to the NEAREST own colony on the same landmass
(no coastline term); else walk to the nearest own unit on the landmass; else
destroy when the adjacent-claim probe names the human. `FUN_4720_049e` (the AI
ship cargo pick, raw 76067-76513) has no `+0x3146 == '\n'` term, so an AI
treasure is never loaded onto a hull, and the King-galleon/Cortes transport
offer (`FUN_465b_0000` raw 75800) is human-control gated (bugs.md #478).

| Function | Resolution |
|---|---|
| `ai_euro_europe_sail_target` | KEPT — its other caller is the FUN_4393 Europe export / Privateer-loot sail; treasure caller deleted |
| `ai_euro_treasure_coast_target` | DELETED |
| `ai_euro_try_treasure_board_sail` | DELETED |
| `ai_euro_find_boardable_ship` | DELETED |
| `ai_euro_cash_one_treasure` | DELETED |
| `ai_euro_treasure_gold_from_unit` | DELETED |
| `ai_euro_try_cash_treasure_europe` | DELETED (all 4 call sites) |
| `ai_euro_try_expected_treasure_harbor` | DELETED |
| `ai_euro_unload_settle` / passenger picker (Treasure stays aboard) | DELETED |
| `ai_euro_act_land_treasure` | REWRITTEN as the literal band (arm 1 + `ai_euro_20e6_47b9_dead_end` arms 2/3) |

## Missionary

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_missionary_should_flee` | 2295-2296 | helper |
| `ai_euro_missionary_no_mission_target` | 2330 | helper |

## Pioneer / wagon / found

| Function | Lines | Tag |
|---|---|---|
| `ai_euro_try_wagon_haul` | 5291 | arm |
| ~~`ai_euro_pioneer_improve_target`~~ | — | DELETED 2026-09-22 (bugs.md #610) |
| ~~`ai_euro_try_pioneer_improve`~~ | — | DELETED 2026-09-22 (bugs.md #610); replaced by `ai_euro_move_scoring_gate`'s raw 90183-90206 order-9 arm (#611) and `ai_euro_5952_improve_best_plot` (raw 94402-94551, #612) |
| `ai_euro_found_with_unit` | 5502, 5514 | arm (Minuit land-purchase gate) |
| `ai_euro_act_land_roles` | 19066-19067, 19089 | arm |

## Land combat / fortify

| Function | Lines | Tag |
|---|---|---|
| ~~`ai_euro_move_scoring_gate`~~ | ~~13312~~ | **resolved 2026-09-18e** (bugs.md #522) |
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
  **2026-09-18b (bugs.md #521), partially closed.** Two genuine DOS
  divergences behind the stall are fixed:
  (1) raw 88883-88885 — the LAB_52aa gate is
  `(0x5382 & 1) == 0 || ((owner < 4 && control[owner] == 0) || owner > 3)`;
  during the War of Independence an attack is scored only against a
  human-controlled player slot (DS:0x543f + n*0x34 == 0) or a tribe. The port
  spelled it `!woi || owner < 0` and so rejected every claimed tile.
  (2) FUN_465b_0000 / FUN_5fef_0000 — a step onto an occupied tile resolves
  against the tile's **best defender**, never the head of the stack;
  `ai_euro_score_move` and the goal-dispatch drain loop both read
  `units_id_at` and then skipped sea units, so a berthed foreign hull hid a
  colony's garrison. With both fixed the stand-in-free REF leaves **1**
  surviving colony instead of 2, so the pair is still kept. The residue is
  volume, not reachability: on land every @UNIT col9 byte (DS:0x5239) is 0, so
  the LAB_52aa odds core `((Sum col9 + 1) / stack) * base` is 0 against any 2+
  defender stack — DOS's own scorer declines a stacked colony and the assault
  must come from the FUN_521d_0a60 MILITARY goal walk. That binding is dropped
  here the moment the unit arrives: 0a60 tile housekeeping sets act_state = 10
  for any unit adjacent to a foreign claim (raw 87566-87568, DOS-literal) and
  the goal-consumption tail reads a goal only at act_state == 0x0b, so exactly
  one attack per approach is produced.
  **2026-09-18c — act_state 10 resolved, lead closed; the pair is still kept.**
  `unit+0x314c == 10` has exactly one writer in the whole game (FUN_521d_0a60,
  raw 87567) and means *"a foreign unit or settlement stands on one of my eight
  neighbours"* — FUN_521d_0906 probes FUN_281f_0682 (layer2 bit0 unit owner)
  and FUN_281f_06be (layer2 bit1 settlement owner) through FUN_521d_0896, which
  passes any Euro owner unconditionally. Its readers, in flow order:
  raw 87560-87563 clear 1/2/3 and `>9 && order != '1'` to 0 *before* the probe;
  raw 88160 `< 10` resets the order code (so 10/0x0b keep theirs);
  raw 88164 assigns fresh goals only at 0/5/6, so a 10 unit gets none;
  raw 88404-88406 (20e6 entry) admits it (`< 10` fails);
  raw 90210-90214 lists it with 0/5/6, so it drops straight into the
  LAB_521d_4d2e wander scorer with no goal pathing;
  raw 90399-90404 demotes it to 5 with order code `'0'` on every 20e6 exit;
  raw 90552 (FUN_521d_5b66) dispatches a goal only at 0x0b.
  So **DOS unbinds an adjacent unit from its goal exactly as this port does** —
  the goal walk is not the assault route, and the premise that a missing
  `act_state == 10` arm was hiding one is wrong. ~~`case 10` of FUN_521d_5b66's
  order switch (raw 91195) is unreachable~~ — **corrected 2026-09-18f**: the
  real switch was read off 5b66's own 198 bytes (see the Resolved entry below).
  Its `case 0xa` is a three-instruction thunk to `FUN_281f_0934` (finish the
  unit), not the 1800-line body Ghidra shows at raw 91195 — that body is
  inlined corruption. It is still effectively unreachable, because 20e6's
  LAB_5a78 tail demotes 10 to 5 before the switch runs.
  Ported literally: the raw 90399-90404 tail demote now runs on every
  `ai_euro_move_scoring_gate` exit.
  The residue really is DOS's own odds core. Case 0 of FUN_1427_0d38 is
  `DI += DS:0x5239[type * 0xe]` — jump table at `1427:0d78`, entry 0 →
  `1427:0d96`, read byte-exact from `viceroy_unpacked.asm` — i.e. Σ @UNIT col9,
  zero for every land type, so `((Σcol9 + 1) / stack) * base` is 0 against any
  2+ land defender stack and the tile scores −999. Statically DOS parks the
  unit beside the colony instead (the `0x46` arm, raw 89011-89029, gated on
  `local_ea` = "LAB_52aa was reached"). ~~That arm is unported.~~ **Ported
  2026-09-18d** as `ai_euro_20e6_border_park_arm`, its own arm in the LAB_4d2e
  tail of `ai_euro_move_scoring_gate` (DOS order: `local_ce == 0` → `local_8e
  == 0` → the 0x42/0x65 pioneer codes → this arm → the 0x39 fallthrough). It
  moves no golden counter, because the land-arms branch of the gate was not
  reached at all in `golden_ai_turns` / `smoke_ai_*` / `golden_woi_ref01` (the
  FOUND-goal branch or one of the early returns won every time — fixed in
  #522/#526), and the
  stand-in therefore still cannot be deleted: with
  `ai_euro_land_try_adjacent_attack` disabled the REF still leaves 1 surviving
  colony and the endgame latch never fires. **Still unidentified: what makes
  the DOS REF actually break a 2+-defender colony. Needs a DOSBox-X trace.**
  Two more divergences fixed on the way, both independently DOS-correct:
  (3) the uncited at-war land-hunter early return in
  `ai_euro_move_scoring_gate` (FUN_521d_20e6 has no war gate at all — its only
  act-state gates are raw 88404-88406 and raw 90210-90219) — removed, so at-war
  land units reach the 4d2e scorer as in DOS;
  (4) the goal-dispatch drain loop's nation-blind `units_id_at` fallback: a
  second own column standing on the step tile was handed to
  `ai_euro_try_attack` (a no-op on an own-nation target) and the loop broke
  every turn, freezing whole REF columns two tiles short. FUN_465b_0000 reaches
  combat only for a foreign owner nibble. With the stand-in, `golden_woi_ref01`
  now takes all seven colonies at **t15** (was t21).
  Lead (c) — "the port's normal-branch test `(here < 0 && pres < 0)` is
  narrower than DOS's `local_5c < 0` at raw 88686" — is **refuted**: the port's
  test is raw 88774-88776 (`(unit_on_tile < 0 && FUN_281f_06d2 < 0) ||
  owner == nation`) and matches. Raw 88686 (asm 521d:5210) is a different,
  inert gate: `local_90 == 0 || local_34 != 0 || local_5c < 0 ||
  local_10 == nation` else *skip the neighbour* — it only bites for a NON-ship
  unit standing on an ocean tile, which cannot happen on land. The one real
  narrowing left there is the Euro-attack entry itself (asm 521d:5260): DOS
  scores an attack whenever `(diplo & 0x40) == 0` (not at peace, unmet
  included), where the port requires `at_war || unmet-Privateer`; that
  narrowing is deliberate and documented at the call site.
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

## Resolved 2026-09-18f (bugs.md #525 / #526)

- **`FUN_521d_5b66` decoded off its own bytes.** Ghidra's 1800-line
  decompile at raw 90446-92257 is the known corruption; the real function is
  198 bytes at `OVL14_L0000` 0x5b66-0x5c37
  (`viceroy_overlays.asm` ~139884-139975):

  ```
  5b66  if (+0x3149 == 0) goto 5bcd            ; spent nothing yet this turn
        if (+0x314c != 0x0b) goto 5bcd         ; not goal-bound
        if ((DS:0x523d[type*0xe] & 1) == 0) goto 5bda   ; SKIP 20e6 entirely
        if (FUN_281f_0984(x, y, nation) == 0) goto 5bda ; no adjacent foreigner
        if (+0x314b != 'E') goto 5bcd
        DS:0x9456[nation]--                    ; release the Europe-lane slot
  5bcd  if (FUN_521d_20e6(unit) != 0) return    ; via the 521d:7308 trampoline
  5bda  switch (+0x314c)  ; jump table at CS:0x5c2a, range 7..12
          7    -> FUN_1000_93ea = FUN_479b_076e   found colony
          8    -> 0x93b2        = FUN_479b_01a6   clear / plow
          9    -> FUN_1000_9406 = FUN_479b_0526   build road
          0x0b -> FUN_1000_96aa = FUN_479b_0972   WALK THE GOAL
          0x0c -> same                            walk the committed step
          0x0a and out of range -> FUN_1000_8b24 = FUN_281f_0934  finish unit
  ```

  `DS:0x523d` bit 0 is set only on rows 16/17/18 (Privateer / Frigate /
  Man-O-War), so **every land unit and every transport with a goal and MP left
  skips 20e6 outright and just walks** — only a warship can be diverted into
  the move scorer by an adjacent foreigner.

- **`FUN_479b_0972` is the goal walk** (raw 77052-77120, clean): set
  `DS:0x1dd6` (0xffff at 0x0c, else the nation nibble), ask
  `FUN_2a1f_0210 = FUN_6662_0f74` (the pathfinder) for one step toward
  `+0x314d/e`, execute it with `FUN_2a1f_0142 = FUN_465b_0000`
  (`FUN_291f_044e` for a control-0 player), bail **keeping the binding**
  unless the unit now stands on the goal; on arrival enter a terrain-0x1a
  settlement, reset the type-2 ring latch, and at 0x0b finish the unit and
  clear `+0x314c` (0x02 and 0x0c keep theirs). Pathfinder failure clears
  `+0x314c` too.

- **The DOS AI scratch bytes are real save fields.** The Col1 unit record is
  0x1c bytes based at DS:0x3144, so `+0x3148` = `col1_flags15`, `+0x3149` =
  the MP **spent** byte, `+0x314b` = `col1_ai_plan`, `+0x314c` = `orders`,
  `+0x314d/e` = `goto_x`/`goto_y` — and the port's own @ORDERS constants ARE
  those act-state values (7/8/9 = 5b66's cases 7/8/9,
  `UNITS_ORDER_AI_SAIL` 11 = 0x0b, `UNITS_ORDER_AI_MOVE` 12 = 0x0c). The
  file-local `s_0a60_pilot_state` mirror is **retired**; 0a60's goal commit,
  its housekeeping, the 3558/10be board marks, the 0x46/0x47 order codes and
  the LAB_5a78 demote all write the real bytes.

- **`ai_goals_primary_code_at`** (new, `ai_goals.c`) replaces the mirror's
  Linux-only `goal_code` field: DOS has no per-unit goal-code byte and
  re-reads the table at `+0x314d/e`.

- **Deliberate divergences, each measured:**
  - The 0a60 commit binds **land units only**. DOS binds ships too, but the
    port's 0a60 ocean-tile FOUND/CONTACT producers are not faithful enough:
    binding ships moves every AI transport off its landfall and loses four of
    the six real DOS save pairs. Open lead.
  - The raw 87560-87564 turn-top act-state clear is ported for the `>= 10`
    arm only. Its `== 1 || == 2 || == 3` arm means "aboard ship / in transit /
    off-map", state this port keeps in `aboard_ship_id`, while orders 1 here
    doubles as the first-colony landfall latch.
  - The raw 90210-90219 gate keeps `ai_euro_has_useful_goto` as its spelling
    of "act_state 0x0b with the goal elsewhere" — now a faithful spelling
    rather than a stand-in, since every course stamps the real bytes. DOS
    additionally sends act_state 1/2/3 to the tail; see above.
  - `ai_euro_land_is_fortified` excludes the LAB_5a78 demote's own
    `+0x314b = '0' / +0x314c = 5` pair, which is DOS's "idle, re-evaluate next
    call" marker (0a60 hands such a unit a fresh goal, raw 88164), not a
    fortify order.

- **Not resolved:** the #521 stand-in still cannot go. With
  `ai_euro_land_try_adjacent_attack` disabled `golden_woi_ref01` leaves 1
  colony standing and never latches the endgame; with it, endgame = 2 at t16.
  The `LAB_52aa` odds core scoring 0 against every 2+-defender land stack
  remains the blocker and still needs a DOSBox-X trace.

## Counts

- arm: 18 functions (28 hits)
- helper: 11 functions (13 hits)
- note: 5 functions (7 hits)

Highest-value next targets: `ai_euro_act_land_goal_consume` (7 hits, the
Skills-Chart profession→LABOR ladder), `ai_euro_act_land_hunt_scout` (6 hits),
`ai_euro_act_land_fortify` (3 arms). ~~and the treasure cluster~~
**Treasure cluster resolved 2026-09-23 (bugs.md #745/#746) — deleted
outright; see the Treasure section above.**

~~Separately: why the gate's land-arms branch is never entered in any golden
(460 calls → 284 FOUND courses, 176 early returns, 0 land arms).~~
**Resolved 2026-09-18e (bugs.md #522).** The 460 calls classified as 284
`ai_goals_best_found_tile_near` courses (all nation 2, the crown: 87 at
act_state 10, 72 at 0x0b, 68 at 5, 57 at 0), 153 ship bails, 22 "military name
on own colony" and 1 "colony wants construction labor". DOS agrees with none of
them: at raw 90210-90219 act_state 0/5/6/10 goes to LAB_4d2e unconditionally
and 0x0b only bails to the tail when `FUN_281f_0984` finds no adjacent
foreigner; at raw 88584-88612 an armed unit on its own colony either garrisons
(< 2 armed in the chain) or falls into LAB_4d2e (>= 2) — it never returns
without scoring. Both sites are now ported, the three uncited early returns
are deleted, and the land arms take 118 wander steps in `golden_woi_ref01`.

The stand-in is **still required**: with `ai_euro_land_try_adjacent_attack`
disabled the REF now parks beside the human colonies (1368 `0x46` border-park
exits) and 2 colonies survive. The residue is unchanged and still the only
open question — DOS's own odds core (`FUN_1427_0d38` case 0 = Σ @UNIT col9, 0
on land) scores 0 against any 2+-defender stack, so `local_ce` is never set and
DOS parks instead of assaulting. **What makes the DOS REF actually break a
2+-defender colony needs a DOSBox-X trace.** With the stand-in the war now ends
at t11 (@LOSING3 surrender) rather than t15-t17.
