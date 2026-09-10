# Mechanics smell audit — 2026-09-10 (third sweep)

Nine-area Opus sweep (units/combat, game_loop/turn, ai_euro, native/king AI, colony core,
save/bridge, Europe/reports/FF, map/UI/popup, assets/sound/platform — the last never swept
before). Successor to docs/smell_audit_2026-09-09.md (closed); each section deduped against
it by substance. Much of the signal is defects created or left behind by the fix waves that
closed the prior audit. Findings pending user review; nothing fixed. Confidence H/M/L.
Line numbers as of HEAD 9e1a38f. Finding ids: section letter + local number (e.g. D1).
Sections also record verified-clean areas and refuted hypotheses so the next sweep skips them.

Totals: A9 B8 C9 D24 E8 F7 G10 H7 I13 = 95 findings.


---

# Sweep 3 — Area A: units / combat

Files: src/core/units.c, combat_strength.c, combat_analysis.c, unit_stack.c, unit_chrome.c (+ headers).
Line numbers as of HEAD 9e1a38f. Deduped against docs/smell_audit_2026-09-09.md section A (#1-#14) by substance.
Emphasis on the 2026-09-08/09 fix waves (commits f6cdcd0, 73a0ac3, ff6ab75, 4b605b6, a31100e, aa8b818, 9e1a38f).

1. units.c:1930-1957 — `units_domain_blocker_at` is the last copy of the FUN_5fef_0000 domain gate still testing the ATTACKER's own ship-ness, after the sibling was rewritten to the target-TILE water test. `units_best_defender_at:2033-2048` now derives `tile_domain` from `map_tile_is_water/high_seas(x,y)` (prior-audit #13 fix) and `units_enter_probe:6545` uses the destination tile too, but `units_domain_blocker_at` still computes `mover_sea = units_is_sea(pool, mover_id)` and skips candidates by that — while its own comment (:1918-1924) cites the same raw 99190-99196 and states the rule as "the attacker's *tile* water test". The two answers diverge in exactly the case the rewrite was about: a warship attacking out of a colony berth (sea unit on a land tile). It is not cosmetic — it gates the colony walk-in at :6233 (`units_try_capture_foreign_colony`) and the post-win entry at :7363, so best_defender can pick a land garrison, the fight resolves, and then the blocker check sees only hulls (or vice versa) and refuses/allows the wrong entry. Confirmed the tile reading is the right one: in FUN_5fef_1b0e the defender pick is `local_c8 = thunk_FUN_2a1f_0458(FUN_281f_07e0(...))` (viceroy_unpacked.c:100353-100354) and 07e0 = `unit_index_on_tile` (FUNCTION_CATALOG.md:1380) over the same param_2/param_3 the settlement probes at :100385/:100387 use. H

2. units.c:5263-5265 and :715 — two open-coded Treasure-value readers bypass `units_treasure_value_gold` (:747), the helper written precisely because a COL1-imported Treasure carries its value in the profession byte (+0x315b = gold/100), not in the LE16 `hold_goods_amount[0..1]` mirror. Both sites read the mirror only, so a save-loaded Treasure Train values at 0: at :5263 (combat loot) the ransom popup and the gold credit are silently skipped and the unit is then destroyed by `units_apply_land_loss_outcome`; at :715 (`units_cortes_cash_coastal_treasures`) the unit is `units_despawn`ed unconditionally at :722 even when `value == 0`, i.e. a DOS-authored save's Treasure is deleted for nothing. The four other consumers (:821, :912, :995, ai_euro.c:5013, game_loop.c:9933) all delegate to the helper. Secondary: the citation at :5258 is wrong — FUN_5fef_1908 (viceroy_unpacked.c:100158) is the Europe/King-galleon cash-in (`local_5c = *(byte*)(param_1*0x1c+0x315b) * 100`, the tax/Cortes share the port already ports at :791), not a combat-loot path; combat treasure capture is FUN_5fef_0352's `bVar12` arm (loser type 0xc, raw 99345). H

3. units.c:7351 vs :7327 / :7374 — the village-raid WIN branch charges only `units_move_cost`, while every other outcome of the same attack charges `cost + 3`: the loss branch at :7327 (`units_mp_charge(pool, atk_mp, cost + 3)`) and the ordinary land-win stay-put branch at :7374 (`drain = cost + 3`), both citing the 1b0e `*(char*)(unit+0x3149) += 3` that this file documents as "win or lose" at :7306-7313. The comment at :7318-7321 justifies the omission by saying the raid branch "already has its own complete, separately-cited MP model (FUN_4d56_4528)" — but the branch is a bare `units_move_cost` + charge with no citation of its own. So winning a village raid is 3 thirds cheaper than losing the same raid. M

4. units.c:7133-7134 — the `pre_park` / `pre_spent` discriminator added for bugs.md 429 is dead: `units_try_move` already returns `COLONIZE_ENTER_NO_MP` at :7065 when `units_remaining_mp(pool, unit_id) <= 0`, so by :7134 remaining is provably > 0 and both booleans are always false. Only the `units_move_crosses_shore` disjunct at :7156 can ever set `mp_spent_turn`, i.e. the "boarded already exhausted" half of the rule (and the `units_wake` refund guard at :7963 that depends on it) never fires. Either the MP check at :7065 should let a spent unit board, or the dead half should go — as written the comment describes behaviour the code cannot reach. M

5. units.c:2935 — `win_can_capture = win_euro && wt->attack > 0` applies the DOS type-table read (`*(char*)(local_4*0xe+0x5236) == 0 → bVar12 = false`, viceroy_unpacked.c:99378, local_4 = WINNER type) to the port's body model, which this same file's new doc block at :6029-6052 says is the wrong question for a port unit: a Colonists-type body carrying muskets is DOS's type 1 "Soldiers" (attack 2, 0x5236 set) and DOES capture there, but reads attack 0 here, so it silently falls through to demote/despawn instead of flipping the loser. `units_sweep_stack_after_loss:3118-3126` and `units_seize_noncombat_at:6455-6465` both reason the opposite way about the same body/type split within 200 lines. (Narrower than it looks — game_loop.c:9659 retypes colony-armed colonists to "Soldiers", so the body+kit form only arises from save import, seizure and demote — but those are exactly the units the neighbouring comments call out.) The `win_euro` half is DOS-correct: raw 99392 gates on `local_32 < 4`. M-L

6. units.c:5354 — `atk_noncombat_body = (at->attack == 0 && atk->muskets <= 0 && atk->horses <= 0)` is a fourth open-coded spelling of the combat-role predicate, left behind by the prior-audit-#12 consolidation that folded the copy in `units_best_defender_at` (now `units_is_combat_role(pool, u)` at :2064) and wrote a doc block at :6029 naming exactly *two* predicates. It is `!units_is_combat_role(pool, atk)` verbatim, so no behaviour difference today — pure re-divergence trap in the one place a wrong answer captures a whole defended stack (the bug the surrounding comment describes). L-M

7. unit_chrome.c:275-293 — `unit_chrome_corner_for_type` compares `display_type_index` against hardcoded DOS @UNIT ids (0x0d..0x12, 4/5/7/8/0x15/0x16, 10/11/12), but its argument comes from `units_display_type_index` (units.c:11199-11225), which returns a *pool* index from `units_find_type`. That is the exact id-space assumption the same fix wave removed from `units_combat_brave_vs_human_arty` (units.c:5088-5116, "a Linux POOL INDEX is not a DOS @UNIT id"). Harmless on the stock roster, wrong on synthetic fixtures/modded rosters — and the two files now state opposite conventions. L

8. combat_analysis.c:121-127 / :117-120 vs :459-465 — the `land_attack_bonus` parameter name and the function's header comment ("land_attack_bonus: land engage ×3/2") contradict the caller's own comment at :459-464, which says every attacker land or naval carries the ×3/2 and DOS 636c prints the row off DS:0x8d00 bit 0 for both domains (the suppression was removed). Stale name + stale doc over live code. L

9. units.c — residual raw `moves_left` writes/reads that the `units_mp_charge/exhaust/restore` centralisation (prior-audit #6) did not reach: writes at :7499 (transport docking), :7795 (trade route), :7873/:7895 (pillage), :9742 ("FUN_281f_0934 stand-in: exhaust MP"), :10240/:10269 (board), and a raw read at :7843 (`u->moves_left <= 0` in `units_pillage`). All are Euro-only today (pillage is reached only from game_loop.c:11396, wagons/ships/pioneers are never native), so this is latent, not live — but they are the same inversion class and the helpers exist two screens away. L

## Clean / checked, no finding
- The 1b0e peel percentages agree end to end between `combat_strength.c` and the analysis rows: colony `(tier+1)*2` → `(tier+1)*50%` (:135/:248-260), village 2/4 + capital doubling → 50/100 ×2 (:370-386 / :273-291), fortify +2 → +50%, terrain `terr_byte` → `terr_byte*25`, artillery `>>2` → −75%, arty-vs-raid `<<1` → +100%. The Cargo row's odd `(holds*100)>>3` is DOS-literal (viceroy_unpacked.c:101825).
- `units_ship_damage_vs_sink` is genuinely single-sourced across the naval, raid and fort-fire paths; the fatigue peel and the WoI Tory/Rebel peel read `units_remaining_mp` / the −1 "no record" sentinel correctly.
- COL1_INDIAN_WAR_BIT (read at units.c:3802) now has a production writer (ai_contact.c:9307), so prior-audit #44 no longer applies to this file.
- unit_stack.c is clean: hit-test (`col*rows + row`, :142) and render (`i/rows`, `i%rows`, :385-386) agree on column-major, and the spent-aware activation gate at :180 is correct.
- The repair-timer "winner strength" still has three spellings (`wt->defense` :3434, Privateer defense :3582, `attack_str << 1` :5893) across three copies of the 0352 damage tail — not re-filed, that is prior-audit #9's substance and the fort spelling is defensible (DOS's fort fights through a scratch @UNIT row whose 0x5235 byte *is* the fort strength).

---

# Sweep 3 — B. game_loop.c / turn.c (read-only audit, HEAD 9e1a38f)

Deduped against docs/smell_audit_2026-09-09.md section B (#15–#30) and the #60/#62/#63
production entries — all of those verified fixed in-tree and not re-filed. Findings below
are new, most of them introduced by the fix waves that closed those entries.

1. **src/core/turn.c:1679 — the "ran out of X" demand mask re-reads SoL *after* the same
   tick mutated it, re-opening exactly the snapshot split that the Phase-A hoist at :865
   was written to close.** `colony_craft_demand_mask(pool, colony, colony_prod_sol_bonus(col1,
   colony), craft_demand)` is evaluated at the Phase-K/L position, i.e. after Phase C/D update
   the SoL accumulator and latch bits and after F/G/H education (:1217) and I/J starve-kill
   (:919/:1295) rewrite the colonist roster. The craft pass whose demand this is supposed to
   mirror ran at :865–866 with `sol_b_phase_a` (`colony_craft_one_colony(pool, colony, delta,
   sol_b_phase_a)`), and the comment immediately above this line asserts the mask is "same
   recipe pass colony_craft_one_colony already ran this tick, sol_bonus-consistent" — which is
   now false on any latch-crossing / graduation / starvation turn. `sol_b_phase_a` is in scope
   (same function, declared 800 lines earlier); passing it is the whole fix. Symptom: spurious
   or missing @ORE/@SUGAR/@TOBACCO/… "Need X." chrome on precisely the turns the audit's #62
   invariant was meant to protect. Confirm by composing a colony that crosses the SoL latch on
   a tick where a craft input is at 0. **H**

2. **src/core/game_loop.c:12310 — `map_visible` omits `in_hall_of_fame` and `in_exploits`,
   so the goto pacer and the unit-activation cycle keep simulating underneath those two
   screens.** Every sibling screen set in the same file was updated to carry them —
   `game_turn_flow_allowed` (:10433), `game_service_woodcut`'s `screen_over_map` (:11857),
   the EOT `screen_over_map` (:12100) and the popup-presentation gate (:12165, whose comment
   names exploits/HoF explicitly as "the two screens the retire chain parks the player on") —
   but this one was not. Both flags are set with a live campaign still loaded and `in_menu`
   false: from the title menu (:7679 `in_menu = false; in_hall_of_fame = true`) and from
   `game_retire_after_score` (:5193/:5196). Under those screens the pacer still steps goto
   units, runs `units_advance_goto_one_step` (which can start combat and queue popups), fires
   the village-entry dispatch (:12359) and sails ships to Europe (:12466). End-of-turn itself
   is safe (`game_turn_flow_allowed` does list them), which is why this survived. Same class
   as prior audit #15. **M-H**

3. **src/core/game_loop.c:12430, :12470, :12515 (and :2984) — four hand-off sites still call
   the bare `turn_select_next_unit`, contradicting the comment at :8443 that says the
   per-frame activation cycle already uses the skip-aware form.** `game_after_unit_action`
   (:8445), `game_wait_next_unit` (:10567), MOVE_PIECES (:11238) and the cycle's own idle arm
   (:12569) all use `game_select_next_unit_awaiting_orders`; the three pacer arms (trade-route
   Europe departure, goto-ship-sailed, stalled-unit park) and the @SAILHOME "yes" arm at :2984
   do not, so any of them can park the live selection on a Fortified/Sentried unit for a frame
   — the exact flash the shared helper's header comment (:10511-10519) says must never happen.
   **M**

4. **src/core/turn.c:250 `turn_select_next_unit_awaiting_orders` is a byte-for-byte duplicate
   of `game_select_next_unit_awaiting_orders` (game_loop.c:10521), and turn.h:261 says so in
   writing** ("game_loop.c keeps a ColonizeGameState-shaped twin of this … that should fold
   into this one"). Only one caller uses the shared copy (turn.c:3986, the KING slice); all
   five game_loop call sites use the twin. Two bodies that must stay identical for the control
   cycle to behave the same inside and outside the turn processor — a known divergence trap,
   currently still in sync. **M-L (duplication, no behavioural split yet)**

5. **src/core/game_loop.c:9609 `game_colony_list_outside_roles` and src/core/colony.c:1696
   `colonies_list_eject_roles` build the SAME @ARMOPTIONS dialog two ways and have diverged:
   the outside-unit copy silently drops the Missionary row.** Both fill
   `csv->eject_roles` / `csv->eject_open` (game_loop.c:9264 for a unit standing outside,
   colony_screen.c:444 for a colonist inside). colony.c adds `COLONIZE_EJECT_MISSIONARY` when
   `colonies_has_church_or_cathedral()`; the game_loop copy's list ends at DRAGOON, and its
   applier `game_colony_apply_outside_role` (:9660) has no MISSIONARY case either (it would
   fall to `default:` and change type with no cost). Net effect: a colonist inside the colony
   can be blessed as a Missionary, the identical colonist standing on the fence one pixel away
   cannot. The four kit rows are otherwise line-for-line mirrored, including the
   `colonies_equip_tools_take` step rule — which is what makes the missing fifth row read as
   an omission rather than a deliberate rule. **M**

6. **src/core/turn.c:2866 vs src/core/game_loop.c:9442 — the DS:0x9418 fleet count that gates
   the 2-turn crossing is spelled two different ways, and the turn.c spelling under-counts the
   human.** `turn_route_damaged_ships` reads `col1->stuff.ship_counts[nation]` (the census,
   refreshed from the live unit pool only — col1_stuff_census.c:112), while
   `game_voyage_ship_count` deliberately adds `harbor_ships + expected_ships + bound_ships`
   because "DOS keeps a ship crossing to Europe as a live unit parked on its nation's sentinel
   diagonal … so it has no arrivals array" (game_loop.c:10046-10056 comment). Both feed the
   same `europe_voyage_turns_roll(rng, magellan, ships)` `ship_count > 2` gate. A human with 2
   hulls on the map and 3 in the lane rolls "3+ ships" from game_loop and "2 ships" from
   turn.c on the same turn. Fix is one call: turn.c should use the same reconstruction (or
   game_loop should read the census). **M-L**

7. **src/core/game_loop.c:11297 `MAP_MENU_ACTION_ANCHOR` carries the FORTIFY sibling's status
   strings verbatim** — failure says "Cannot fortify" and success says "Fortifying", for the
   ship row DOS labels *Anchor* (the two cases are otherwise identical copies of :11290-11305).
   Cosmetic, but it is the one place the port tells the player a ship is fortifying. **L**

8. **src/core/game_loop.c:12765 vs :12804 — the colony screen's Escape and Enter sub-panel
   cascades close the same five panels in different orders** (Esc: message, jobs, eject,
   dock_orders, custom_house, construction; Enter: message, eject, dock_orders, custom_house,
   jobs, construction), and the `C` hotkey at :12938 closes only `jobs_open` before opening
   Construction, so pressing C with the eject / dock-orders / Custom House panel up stacks
   Construction underneath a still-open panel. Only reachable when two panels are open at once,
   which the hit-test mostly prevents — filing as the tail of the #20 family, not as a repeat
   of it. **L**

## Checked and clean / refuted (not filed)

- Prior audit #15 (new-game wizard screen ownership), #16 (WoI sail gate — now on the shared
  tail at :10033 with a verified ndisasm citation), #18 (woodcut parking + EOT/idle service
  order), #19/#25 (lane-full restore, voyage roll before despawn), #20 (custom_house in the Esc
  cascade), #21/#17 (C2 crown gate, Section D hysteresis + once-only latch), #22 (human-nation
  scan), #23 (0bd0 sell-then-buy copies now identical), #24 (KING slice skip-aware hand-off),
  #27 (popup gate now excludes exploits/HoF/atlas), #28 (dead Europe `S`, ship Sentry) — all
  fixed in tree.
- turn.c:3060-3120 year-end Section E: I suspected the 1800/1850 era-end block was missing the
  Spring gate its 1790/1840 sibling carries. **Refuted** — viceroy_unpacked.c:58619-58630 puts
  `*(int *)0x538c == 0` inside the 0x6fe/0x730 condition only; the port matches DOS exactly.
- turn.c:148 `u->mp_spent_turn = 0` (bugs.md 429): citation checked against
  viceroy_unpacked.c:6355-6357 — DOS really does clear +0x3149 for every unit in the array at
  the day top, and the port's placement above the FORTIFY/skip `continue`s reproduces it. The
  only gap is a `control == 2` withdrawn Euro slot, whose units never get a refresh slice at
  all; not worth filing.
- `turn_run_colony_unit_construction` / `turn_run_colony_building_completion` both honour
  `turn_prod_nation_in_scope`, so the SETUP (AI) / FINISH (human) split does not double-run them.
- `turn_count_bells_and_crosses_for_nation`'s `prod_compose_stamp` path is correct for both the
  AI (SETUP) and human (FINISH) tick positions; the human's live-read fallback really is the
  pre-tick state.
- `europe_set_sail_from_harbor` copies the whole ship record, so the trade-route stamp survives
  the harbor→bound move at game_loop.c:11075 despite only `trade_stop` being re-written there.

---

# Sweep 3 — C: src/core/ai_euro.c (+ ai_euro.h)

Method: `git diff b4cc8b1~1..HEAD -- src/core/ai_euro.c` (995+/175− across the 11 "Smells"/"Teachers"
commits) read in full, then grep-driven cross-checks of the new code against
`original_sources_decompiled/viceroy_unpacked.c`, `original_sources_annotated/ai/colony_tick_5952_035e.md`,
`COLONIZE/NAMES.TXT`, `colony.h`, `units.h`, `docs/save_format_map.md`.
Deduped against `docs/smell_audit_2026-09-09.md` §C (#31-#44) — every finding below is
**new signal created by, or left behind by, those fixes**, not a re-file.

Spot-checks that came back CLEAN (worth recording so nobody re-does them):
`ring1`/`local_22` port (raw 94918-94960) is verbatim; `local_82 -= (want_pre − want)` matches
raw 94071; `k_20e6_type_combat[]` is byte-exact against NAMES.TXT @UNIT column 4 in the
*corrected* 6/7/8/9 row order, so audit #32's matcher fix did not desync the table; the
`wanted+(wanted>1)<local_82` / `local_82<wanted` pair matches raw 94194-94199; the +0x314a
origin-binding writer DOS runs at raw 94047-94049 *is* ported (ai_euro.c:6554-6577), just in a
different function; `ai_euro_try_attack` still declares the @SNEAK war, so the #106
peace-permissive foe picker did not open an undeclared-war hole.

---

1. **ai_euro.c:20642-20662 — the "sticky CONTACT re-hunt" tail is now unreachable dead code; smell #37's fix gave it the *identical* gate set to the block 20 lines above, which already runs the same loop.** :20619 runs `if (u->active && at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u))` → `ai_euro_land_try_adjacent_colony_seize` → `..._village_seize` → `ai_euro_land_try_adjacent_attack`; and `ai_euro_land_try_adjacent_attack` (:16845-16867) *is* an 8-step `best_adjacent_foe`/`try_attack` loop with the same no-progress break. :20642 then re-runs that loop under `at_war_land && is_land_hunter && !fortified` — a strict subset (it adds only "Euro peers only"). Any foe reachable at :20642 was already attacked at :20625, and MP is spent, so the tail can only ever hit the `foe < 0` or no-progress break. Either the gates are too tight (DOS's tail is a different arm than the seize/attack block) or the tail should be deleted; it currently pretends to do work it cannot do, and its 8-line comment still describes live behaviour. **M-H**. Confirm by instrumenting the `ai_euro_try_attack` call at :20657 over `golden_ai_turns` — expect zero hits.

2. **ai_euro.c:18771-18774 — the Pioneer→Soldier/Dragoon equip gate is cited to raw 94289-94311 but ports only 2 of the 4 conjuncts of DOS's "big settled town" disjunct.** DOS (viceroy_unpacked.c:94291-94293) is `local_90 = (local_2a == 0) && (pop > 10) && (FUN_281f_04d4() == 0) && ((+0x1b & 0x10) == 0)` — `local_2a` is the colony continent's G-stance (`-0x6790[nation*0x10+cont]`, the same quantity `ai_euro_continent_stance_at` already provides in this file) and `FUN_281f_04d4()` is a second unported probe. The port has `equip_pop > 10 && (c->ai_flags & NEEDS_COLONISTS) == 0` only. Net effect: on any continent where the AI *has* a stance assigned (i.e. exactly the war/expansion continents), DOS suppresses this arm and the port does not — every pop>11 colony with 50 muskets keeps re-typing arriving Pioneers. The `(ai_flags & 0x48)` half at :18772-18773 is correct. **M-H**. Confirm: the raw lines are unambiguous; the only open question is what `FUN_281f_04d4` resolves to.

3. **ai_euro.c:1573-1581 + :1598-1606 — DOS has TWO writers of +0x1b bit 0x10 (NEEDS_COLONISTS); only the second is ported, and the new unconditional `else`-clear now actively suppresses the first.** `colony_tick_5952_035e.md:487-490` (= viceroy_unpacked.c:94143-94146) is `if ((+0x1c & 0x10) && pop < 0x20) { +0x1b |= 0x10; +0x1c &= 0xef; }` — a one-shot hand-off from the +0x1c bit 0x10 latch, run *before* the formula writer at md:557-563 that :1576 ports. Two consequences: (a) the comment at :1574-1575 ("DOS clears the bit up front … and re-ORs it, which is this set/else-clear pair") is only true if both OR sites exist, so the `else` branch at :1578-1581 is stronger than DOS; (b) `COLONIZE_COLONY_FLAG_SMALL_AI` (+0x1c bit 0x10) is stamped every tick at :1600-1605 from an uncited `pop < 10` and is **never read and never cleared** anywhere in the port — DOS's only consumer of that bit is exactly the writer above, which also clears it. So the port has one uncited write-only flag and one missing DOS gate, on the same bit pair. **M-H**.

4. **ai_euro.c:20718-20726 — the despawn latch-hygiene loop resets `s_20e6_wagon_errand`, `s_20e6_hop_slot` and `s_20e6_hop_steps` but not `s_20e6_explore_fatigue`, although its own comment gives the identical argument for all of them.** The comment (:20720-20723) says the +0x3155/+0x3156 bytes "are part of the unit record too, so a reused id must not inherit a foreign hop commitment". `s_20e6_explore_fatigue` (:11488) is documented at :11482-11487 as DOS unit **+0x3154**, the third byte of the same cargo-hold family, and it is read at :11901 (explorer scoring) and mutated at :12396/:13006. A reused unit id inherits the dead unit's fatigue. 3-of-4 sibling coverage, no explanation. **H** (code-visible; no DOS question involved).

5. **The 255 empty-hold sentinel guard added by smell #36 reached 2 of ~5 sibling hold scans in this file.** Fixed: `ai_euro_hauler_free_holds` :5406, `ai_euro_0a60_holds_occupied` :8985. Still unguarded, all reading DOS-imported hulls:
   - **:8525** `if (sh->hold_goods_amount[hh] > 0) used++;` — a *third* open-coded copy of `holds_occupied` (cargo_count + occupied goods holds), inside the 5d04 Europe-buy loop, which also scans `COLONIZE_UNIT_CARGO_MAX` instead of `units_goods_hold_count` — i.e. exactly the two defects #36 fixed in the other copy, in the copy #36 did not find. It feeds `cap == used` and `cap - used > 2`, the buy budget.
   - **:8003** `out[u->hold_goods_type[h]]--;` under `hold_goods_amount[h] > 0` — the 5d04 cargo-demand tally, `COLONIZE_UNIT_CARGO_MAX` bound, no sentinel test.
   - **:7708 / :7715 / :7753** (`cb_reward_case` / `cb_reward_value` / `cb_sell_hold0`) gate on `hold_goods_amount[0] <= 0` only, so a sentinel hold 0 sells **255 units** at `euro_price−1` and credits the treasury for them.
   Should either be one shared helper or five identical predicates. **M-H**.

6. **ai_euro.c:13960-13997 (`ai_euro_ocean_colony_sail_score`) duplicates the DOS site that :17435 (`ai_euro_20e6_colony_sail_pick`) now ports structurally, with entirely invented arithmetic, and both are live in the same act.** The thin one's header (:13955) cites "LAB_521d_3558 peace colony-sail score (~89614–89711 thin)"; the structural one's fixes this session cite "raw 89663" (:17495) — the same DOS range. The thin scorer's terms (`pop*8 − d*4 + idle*8`, `+16` docks, `+14`, Stockade/Fort/Fortress `+8/+16/+24`) have no DOS basis; the structural one carries the real `rng(0,8) + ((0x11−pop)²+2)*4 − (pop−wanted)*2 … ±0x19 … +0x3c` ladder. Call order: `ai_euro_20e6_colony_sail_pick` at :17882 sets an `AI_MOVE` goto, then `ai_euro_try_ship_war_cargo_sail` (:19226, war only) re-scores with the thin function and can overwrite it. Two answers to one DOS question. **M**. (Sub-item: :13978's comment `/* dock flag 0x1b&0x10 stand-in */` mislabels bit 0x10 — that is NEEDS_COLONISTS, not a dock flag.)

7. **The port recomputes DS:0x95f2 (`continent_presence_flags`) twice, in two functions that disagree, and both cite a docs row that the raw source refutes.** `ai_euro_5952_continent_presence` (:9748-9810, new this session) filters `!units_is_on_map(u) || u->aboard_ship_id >= 0` and masks `u->nation_id & 0xf`; `ai_contact.c:9240-9252` (the war-declare half of the *same DOS body*, called 6 lines apart at ai_euro.c:10173) does neither. A passenger aboard a ship therefore sets bit 2 in one copy and not the other, which flips ai_contact's `(presence & 6) == 0` branch between "declare war on the tribe" and "cap expansion appetite". Separately, `docs/save_format_map.md:254` (row 156) and the new comment at ai_euro.c:9751-9754 both state the array is "OR'd (not cleared between nations, so accumulates across the full per-turn pass)" — viceroy_unpacked.c:78149-78151 zeroes `-0x6a0e[0..15]` at the top of **every** per-nation `FUN_4962_0018` call. ai_contact.c:9186-9188's comment ("zeroed at the top of every per-nation call") is the correct one. Doc + one comment are wrong; the code happens to be right. **M**.

8. **ai_euro.c:18790-18791 — the new Dragoon arm debits horses with the muskets constant.** `c->stock[COLONIZE_CARGO_HORSES] -= UNITS_EQUIP_MUSKETS; u->horses = UNITS_EQUIP_MUSKETS;` while `UNITS_EQUIP_HORSES` exists two lines apart in units.h (:1370-1371). Both are 50 today so behaviour is unchanged; it is a wrong-constant that will silently break if either is ever retuned. **L**.

9. **Stale comments left by this session's own fixes.**
   - :10149-10150 — the section-D header still lists "Col1 labor_shortage (+0x8e)" as a LABOR trigger; :10176-10187 removed exactly that disjunct and explains at length why it must not come back.
   - `colony.h:260-261` — "its one read site (ai_euro.c ~12103, DOS 88756) really does test bit 4" is now false in two ways: bit 0x04 has two readers (`ai_euro_20e6_wander_step` :12834 and the new `ai_euro_20e6_surplus_recall_arm` :12164-12196), and the cited line number moved.
   - :1247-1288 / :1290-1300 — `ai_euro_colony_ring_tier` and `ai_euro_colony_wanted_size` are 40 lines of citation around `return 2;` / a switch whose live answer is always 8. Correct and well argued, but they keep two unused parameters and a dead 0xc/0x20 table; the labor arm's clamp at :12297 (`wanted > 0x10`) and the colony-sail clamp at :17462 (`wanted > 0xc → 0x10`) are now both unreachable. Worth a note so a future reader does not "fix" the clamps.
   **L**.

---

Not filed, checked and judged fine: the 28c8 `plenty`/`section_done` port (raw 94604-94617 read line by line — matches, and the unported loop-entry guard `local_7e`/`local_80`/`local_1e` is explicitly documented at :4133-4139); `ai_euro_5d04_dos_type_of`'s collapse of all non-Privateer hulls to 0xd (every consumer only range-tests `0xc < t < 0x13`); `units_type_has_profession_slot` being fed a Linux `type_index` at :9390 and a DOS code at :9955 (the DS:0x30e table is DOS-indexed, so :9955 is the correct one, but a NAMES-loaded pool makes the two agree — latent, not live).

---

# Section D — Native / King AI (ai_contact.c, ai_diplo.c, ai_king.c, ai_popup.c)

Sweep 3, 2026-09-10, HEAD 9e1a38f. Read-only. Deduped against
docs/smell_audit_2026-09-09.md section D (#44-#59) by substance.

1. src/core/ai_king.c:571-580 — `ai_king_seed_backup_force_1a26` applies FUN_43f7_1a26's halving constants to the already-incremented pool instead of to DOS's pre-store accumulator, so all four foreign-intervention pools come out too big; evidence (verified directly in original_sources_decompiled/viceroy_unpacked.c:74769-74783): DOS is `iVar4 = pop/10 − diff; *0x53e2 = iVar4+8; … *0x53e2 = (iVar4+9)/2` — the halved value is `(pre+9)/2`, not `(stored+9)/2`. The port sets `pool0 = base+8` and then `pool0 = (pool0+9)/2 = (base+17)/2`, i.e. **+4 Regulars**; identically `pool1 = (iVar9+3)/2` vs DOS `(iVar9+2)/2` (+0/+1 Cont. Cav.), `pool3 = (iVar8+7)/2` vs `(iVar8+4)/2` (+1/+2 Artillery), `pool2 = (iVar7+local_4+7)/2` vs `(iVar7+local_4+4)/2` (+1/+2 Man-O-War). The inflated `pool2` then widens the `pool2*2` and `pool2*6` clamps below and the `backup_force[2]` gate that drives `ai_king_merc_offer`. The header's "confirmed byte-for-byte against viceroy_unpacked.c:74765-74795" claim is false. Confidence H.

2. src/core/ai_diplo.c:3300-3304 — `ai_diplo_military_score` was rewired (prior-audit #51) to return `col1->stuff.land_combat_strength[nation_id]`, but nothing in the port ever refreshes that mirror, so in a Linux-started game it is all-zero and both bands it gates are dead; evidence: the only writer is `col1_stuff_census_fill_blank` (src/core/col1_stuff_census.c:158), whose sole caller is src/core/col1_bridge.c:2861-2862 — inside `col1_bridge_capture` (the save-*writing* path, reached only from `game_save_col1_slot`, game_loop.c:4898/4957) and additionally guarded by `if (col1_stuff_census_window_is_blank(...))`, so the array is zero until the first (auto)save and frozen at that turn's values forever after. Contrast the Indian half of the same DOS census pair, which the port *does* run live every Indian nation-turn (`ai_contact_indian_census_4962_06b6`, ai_contact.c:4718, called at :4843 from `ai_contact_indian_relation_tick` ← ai.c:4972), and ai_contact.c:9203-9209, which states outright "the port never refreshes the DS:0x95b2 / 0x91cc mirrors for a Linux-started game" and therefore recomputes live via `ai_contact_land_combat_sum`. Downstream at score 0: ai_diplo.c:3555 (`self > AI_DIPLO_STRENGTH_MIN` war-fatigue peace roll) and :3574 (`self > other*2+20 && self > 30` declare pressure) can never fire — the exact two arms the constants note at ai_diplo.c:105-115 says they gate, and whose RNG draws that note claims the goldens depend on (the goldens load DOS saves, which carry DOS census values, so they hide this). The same frozen snapshot drives 153e worthiness (ai_diplo.c:3167/3182/3185/3196/3211), the mercenary "smite" price (ai_diplo.c:2680-2682, which then floors at `base = 10` → a flat 500g every time), the king intervention math (ai_king.c:3904-3905) and finding 1's pool seeding. Confirm by printing `stuff.land_combat_strength[]` on turn 5 of a new game. Confidence H.

3. src/core/ai_king.c:2645 — the REF invasion picks the *least* attractive colony because it walks DOS's ascending score list forwards; evidence: the port sorts `score[]` ascending (:2618) and then iterates from index 0, but `FUN_43f7_0982` walks the same ascending list **backwards** (`iVar4 = local_2a − local_48; local_6a = local_42[iVar4−1]`, viceroy_unpacked.c:74052-74056, `local_48` starting at 0), taking the highest score first — and the score is `colonists*(125−SoL) − 75*strength`, so high = large and weakly held. The port's own `ai_king_rank_nations_0218` header already establishes `FUN_291f_0ed0` as an ascending sort. Net effect: the REF lands on the smallest/best-defended port instead of the juiciest one. Confidence H.

4. src/core/ai_king.c:3667 — `ai_king_frigate_offer`'s "nation already owns a Frigate" guard bounds its loop with `ctx->units->unit_count`, a live population counter over a sparse slot array, so any despawn hole hides every unit past that index; evidence: units.c:1274-1277 decrements `pool->unit_count` on despawn while `units_clear_slot` leaves the array sparse, and this is the only `unit_count`-bounded loop in the file — every sibling uses `COLONIZE_UNITS_MAX` (ai_king.c:2327, :4028, :4451). Consequence: @KINGFRIGATE re-fires every 8th turn for a nation that already has one, each acceptance stacking a free Frigate plus a real +10% `ai_king_tax_hike_apply`. Confidence H.

5. src/core/ai_diplo.c:2739 — a ctx-bearing **+100** alarm delta still calls the half-writer `ai_diplo_indian_alarm_delta`, so the FUN_4cc6_00f2 escalation tail (mission expel + @INDIANBURN + its RNG draw) cannot fire from the one site guaranteed to land the pair at alarm 100; evidence: the line's own comment cites `FUN_281f_0d6c(tribe, h, 100, 0)`, and both ai_diplo.c:635-646 and ai_contact.c:311-320 document 0d6c as a thunk to the *whole* of 4cc6_00f2 — which is why prior-audit #46 moved every ctx-bearing site onto `ai_contact_alarm_delta_00f2`. ai_contact.c:4666-4676 asserts the remaining bare callers are "units.c, game_loop.c" that "genuinely have no turn context in hand"; this site is inside `ai_talk_answer`, which has `ColonizeTurnContext* ctx`, and ai_diplo.c already `#include "core/ai_contact.h"` (ai_diplo.c:2). Confidence H that it is inconsistent with #46; M that DOS burns missions here (confirm by reading 153e's third-party crusade arm around raw 97600 for the 0d6c call).

6. src/core/ai_king.c:3403 — the paid mercenary hire is a second, divergent implementation of the DOS call the free intervention already ports; evidence: `FUN_43f7_2022` ends with `thunk_FUN_2a1f_010a(uVar7)` = `FUN_43f7_10f0(1)` (viceroy_unpacked.c:75060-75070), reusing 10f0's colony roulette, water-tile scoring, Man-O-War and the `FUN_43f7_0082` type map (reading merc counts from `-0x61ba` instead of the pools), whereas `ai_king_do_merc_hire_at` spawns bare `"Regular"/"Dragoon"/"Artillery"` at `(hx, hy+1)` with no terrain test at all — the same water-spawn class bugs.md 261 fixed for 06a6, and contradicted by the sibling comment at ai_king.c:2938 which says 0082 yields Cont. Army (9) / Cont. Cav. (7) for the human at war. Confidence M-H.

7. src/core/ai_king.c:4529 — `ai_king_woi_pop_share_pct` recomputes crown-vs-human population from the colony pool with a per-colony `pop>0?pop:1` floor, where DOS reads the census mirror with symmetric +1 offsets; evidence: `FUN_3844_0442` computes `local_8 = (crown[0x940c]+1)*100 / ((human[0x940c]+1) + (crown[0x940c]+1))` (viceroy_unpacked.c:58510-58521; `0x940c` = `stuff.colony_pop_totals[]`, save_format_map.md row 243). The missing +1s move the @WARN3/@LOSING3 boundary — human 1 / crown 9 is 83% (warn) in DOS but 90% (instant surrender) here. Confidence M-H. (Note: `colony_pop_totals` is one of the frozen fields from finding 2, so fixing this without fixing 2 substitutes one wrong number for another.)

8. src/core/ai_contact.c:3769-3777 (also :5132-5134, :5783-5785, and the stamps at :3884, :5227, :5376, :5863, :5934) — the shared `s_visit_cooldown_until` throttle stamps `now_turn + 8`, directly contradicting the two comments added in the same pass that argue DOS's visit latch is **per-turn**; evidence: ai_contact.c:3843-3854 says "The latch is per-year, NOT permanent … each tribe/Euro pair resolves at most one gift-or-beg visit per turn, then is eligible again next turn — which is why DOS villages keep visiting instead of going permanently quiet", and ai_contact.c:29-45 says DOS's sleep is `contact_state`, cleared at the top of every turn by `ai_indian_midpass_clear_tables` (ai.c:4871, called from turn.c:3771). With the 8-turn stamp sitting on top of the (correct) `contact_state` latch, the port suppresses seven of every eight DOS-legal village visits per Indian nation. The 8 is uncited — its only pedigree is the now-stale line at :3770-3773, written when the throttle was the only latch. Confidence M-H on the contradiction; the call to make is whether the 8 is deliberate pacing.
   **RESOLVED 2026-09-10 — throttle removed.** DOS checked: `FUN_5bfb_022e` and its move-tail caller (viceroy 98653-98676, `aiStack_20[nation]` = once per nation per move) carry no turn counter; `contact_state` is cleared at the top of every `FUN_4d56_1b3a` call (viceroy 81704-81707), i.e. per turn. Pacing = walk-up trigger + per-turn latch + alarm ceiling + mood RNG, all already ported. `s_visit_cooldown_until` deleted (all 3 gates, 6 stamps, reset memset); the bugs.md-423 exclusion is carried by ai.c §9 arm order + the state-2 latch, re-asserted by the rewritten unit_ai_contact block. Stale "per-year" wording in ai.c/ai_contact.c fixed; bugs.md row 423 amended. 61/61 ctest green.

9. src/core/ai_king.c:998 — `ai_king_audience_roll` omits `FUN_38fd_5be8`'s first gate; evidence: DOS opens with `if (*(char *)(iVar1 + -0x6d68) == '\0') return 0;` (viceroy_unpacked.c:68433-68435; `-0x6d68` = `0x9298` colony_counts, the same table `ai_king_rank_nations_0218` cites at ai_king.c:474), and the port's gate list starts at the `turn < 30` test that is DOS's *second* line — so a human with zero colonies still gets tax audiences from turn 30 on. Confidence M.

10. src/core/ai_contact.c:3377, :10492-10493, :10517-10518, :10528 — prior-audit #54's "the `alarm_by_player >= 55` disjunct is provably dead" reduction was applied at 3 of 7 sibling sites, and the surviving four carry a comment asserting the opposite rationale; evidence: `ai_contact_pair_friction` (ai_contact.c:2197-2213) seeds `friction = ind->alarm_by_player[e]` and only ever raises it, so `friction >= X || alarm_by_player[e] >= X` is always just `friction >= X` — which is exactly why :2243, :2849 and :2924 were reduced. Still un-reduced: `:3377 friction >= 55 || ind->alarm_by_player[e] >= 55`; `:10493 friction < 40 && friction < 55 && ind->alarm_by_player[e] < 55` (the `friction < 55` conjunct is *also* dead under `friction < 40`); `:10518 friction >= 40 && friction < 55 && ind->alarm_by_player[e] < 55`; `:10528 ind->alarm_by_player[e] >= 55 || friction >= 55`. The comment at :3369-3375 justifies keeping the term with "Pair friction alone would always be ≥55 when alarm≥55, making gift-band unreachable" — it states the dominance relation and draws the wrong conclusion. Behaviour-neutral, but one predicate answered two ways in one file. Confidence H.

11. src/core/ai_king.c:2153 — `ai_king_10f0_score_tile` inverts DOS's ownership test, so its Man-O-War penalty can only fire on the wrong side; evidence: DOS checks whether the tile's stack belongs to the **crown** (`(*(byte *)(local_34*0x1c+0x3147) & 0xf) == *(byte *)0x53d2`) before subtracting 999 per type-0x12 hull (viceroy_unpacked.c:74350-74359), but the port returns −1 for any non-human unit first, so `-= 999` can only ever fire on the human's own MoW — rejecting a tile DOS accepts and accepting one DOS rejects. `u->type_index == 0x12` is also a raw id where every sibling name-matches (`ai_king_is_mow` :603, `ai_king_0982_spawn_pool_unit` :2345, the win gate at :4805 which explicitly notes "matched by NAME here (synthetic test pools reorder type indices)"). Confidence M.

12. src/core/ai_king.c:3517 / :457 — `head.rival_nation_slot_2` is written (:519) and never read, while the popup the site stubs already has its operand mapped; evidence: `ai_king_intervention_nation_slot`'s `slot_idx == 1` arm at :457 has no caller, and reports.c:1223 / turn.c:2571 read slot_1 only; DOS's consumer is `FUN_291f_0ac8(0, 0, *0x53d6)` supplying %STRING0 for @MERCENARIES (viceroy_unpacked.c:75048), the same `*0x53d6` that names the ally in 10f0's announce (:74396). The hardcoded `"Europe"` stand-in at :3517 is uncited. Confidence M.

13. src/core/ai_king.c:4612, :4653 — @WARN1/@WARN2 hardcode `tok.number0 = 1` / `tok.number1 = 1` where DOS fills all three number slots from live counts; evidence: `FUN_3844_0442` calls `FUN_281f_09ae(0, local_68…)` = coastal-port count, `(1, colony_counts[human])`, `(2, local_8)` (viceroy_unpacked.c:58552-58556). With 2 ports or 2 colonies left the popup still reads "all but 1". Confidence M.

14. src/core/ai_diplo.c:1585-1593 and src/core/ai_contact.c:9095-9103 — both new `settlement_owner_at` helpers double-offset the village owner, returning `4 + tribe[i].nation_id` where `tribe.nation_id` is already the 4..11 Col1 id; evidence: ai.c:68 ("TRIBE.TXT section → Col1 nation_id (4..11)"), and every consumer subtracts 4 (ai.c:2616, ai.c:2856, colony.c:618, colony.c:705; units.c:2213-2216 bounds-checks `>= 4 && <= 11`). A village tile therefore reports 8..15 while both function headers claim "Euro colony (0..3) or Indian village (>= 4)". Latent today — both call sites only test `>= 0` — but it is the id-space class this codebase keeps re-breaking, and the third copy of the helper (col1_stuff_census.c:34-55, the one the two new copies were cloned from) carries it too. Confidence H on the mis-offset, L on current impact.

15. src/core/ai_diplo.c:1610-1640 vs src/core/ai_contact.c:9109-9160 — two independent ports of the same DOS quantity (DS:0x95b2 exposed per-continent combat, FUN_4962_0018 gate 4962:022f-026e) that must be hand-kept in sync; evidence: this pass had to fix the identical "orders +0x314c vs ai_plan +0x314b" misreading in *both* (`ai_diplo_153e_exposed_combat_at` and `ai_contact_land_combat_sum(..., exposed_only=1)`), plus the third `settlement_owner_at` clone from finding 14, plus a fourth copy of the same gate at col1_stuff_census.c:183-190. Residual divergence: ai_diplo's copy has no `cap` parameter and clamps with `if (sum > 255) return 255`, ai_contact's with `if (sum >= cap) return cap`. Maintenance smell; no live behaviour difference found today. Confidence M.

16. src/core/ai_diplo.c:147-151, :672 (and src/core/ai_contact.c:589) — the constant `AI_DIPLO_INDIAN_PEACE_MEET 96u` is documented as a *scalar* relation value but every live use writes it as the *bitfield* 0x60 = MET|PEACE, and its scalar alias is dead; evidence: the defining comment (:147-149) reads "Peace feeler / first-meet content floor … Heal mid-band up to this ceiling; drift still climbs to 160", while the only two writes store it into `nation[e].relation_by_indian[idx]`, which the line above :672 declares "is the DOS 0x60 MET|PEACE flag byte, not a scalar". `AI_DIPLO_INDIAN_CONTENT_FLOOR` (:151) has no user at all, and both consumers the comment names — `ai_diplo_indian_peaceful_drift` (:480-489) and `ai_diplo_indian_peace_feeler` (:501-506) — are retired no-ops. Secondary: both writes are hard **assignments** (`= 96`) while the paired Indian-side write one line earlier is an **OR** (`euro_diplo[e] |= COL1_INDIAN_PEACE_BIT`), so the Euro→Indian half silently drops any other bit the byte held; and both bypass `ai_diplo_or_both`/`clear_both`, which ai_contact.c:9170-9180 calls "the sole mutation channel" for this matrix (ai.c:5167 is a third raw write). Confidence H on the dead alias + doc contradiction, M on whether the assign-vs-OR asymmetry can bite.

17. src/core/ai_king.c:4748 — the three lose branches run LOSING2 → LOSING1 → LOSING3, but DOS resolves one selector by last-write-wins in the order `ports==0 → 1`, `pct>=90 → 3`, `colonies==0 → 2` (viceroy_unpacked.c:58507-58532), so LOSING3 outranks LOSING1; the port shows @LOSING1 in the `ports==0 ∧ pct>=90 ∧ colonies>0` case where DOS shows @LOSING3. Confidence L-M.

18. src/core/ai_king.c:2069 — `ai_king_10f0_pick_colony` drops DOS's `local_24 < 10` candidate cap and gives 0-pop colonies weight 1; DOS considers only the first ten coastal human colonies and weights by the raw `+0x1f` byte (viceroy_unpacked.c:74312-74320), so both the roulette total and the candidate set diverge once the human passes ten ports. Confidence L.

19. src/core/ai_king.c:4859 — the @RETIRING2 wartime calendar end fires on `year >= 1850 && crown_units_alive > 0`, where DOS gates on `*(int *)0x538a == 0x73a` — an exact-year test with no crown-units condition (viceroy_unpacked.c:58630). A WoI in which the crown holds colonies but has zero live units skips the retire and runs past 1850. Confidence L; confirm by checking for a second 1850 stop in turn.c (none was found).

20. src/core/ai_popup.c:118, :139-142, :169 — bar-strip arm kinds 2 and 3 have no producer, so their ink arms and the sale-speedup's `|| kind == 2` disjunct are dead, and the comments describing kind 1 and kind 2 as two distinct producers are wrong; evidence: `ai_popup_enqueue_bar_message` hardcodes `kind = 1` (:118) and is the only push reaching this ring — its sole callers are game_loop.c:1318 (Europe `bar_event[]`, filled only by `europe_push_sale_status`, europe.c:3194) and turn.c:1802 (Custom House autosell); `ai_popup_enqueue_bar_message_kind` has no caller outside ai_popup.c. Yet ai_popup.h:428-437 and ai_popup.c:162-166 both describe "arm kind 1/2 — the Custom House autosell run **and** the European Status sell lines, the two producers that feed this ring". The net ×3 speedup is still sale-only, but by accident of there being no other producer, not by the guard the comment describes. Confidence H, severity L.

21. src/core/ai_diplo.c:688, :693, :717-721 — the "Native relations improve." status/popup arm of `ai_diplo_indian_matrix_tick` is unreachable and its guard is a no-op, while the function header still documents both; evidence: `feeler_healed` is only ever assigned from `ai_diplo_indian_peace_feeler` (:501-506), retired to `return 0` on 2026-08-27, so `else if (feeler_healed)` at :717 can never run and `if (prev_sticky != AI_DIPLO_STICKY_DEEP)` at :691 guards a call with no effect. Header at :676-681 still describes "peace feeler → sticky sync … feeler heal while sticky stays clear → 'Native relations improve.'". Confidence H, severity L.

22. src/core/ai_diplo.c:36, :39, :40 — all three `unknown26` index macros are dead, but only one says so; evidence: `AI_DIPLO_FLAG_BASE`, `AI_DIPLO_INDIAN_HOSTILE_STICKY` and `AI_DIPLO_PRIVATEER_SPAWN_SLOT` have no reference anywhere in src/ outside their own `#define` lines — the block became a union of named members (col1_save.h:672-690: `treaty_timer[4]`, `diplo_flag[4]`, `king_grace_counter`, `privateer_spawn_mask`, …) and every site uses those. Only the sticky macro carries the "Unused — kept as the block's index legend" note (:37-38), so the other two read as live. Confidence H, severity L.

23. Stale doc blocks (all L, all comment-only, grouped): src/core/ai_king.c:3839 — the ~30-line block above `ai_king_new_war_event` (@KINGNEWWAR, a peacetime crown declaration) documents the invented REF land hunt / MoW unload / capital rally / 1eca promote / merc gift that was explicitly retired at :4360 ("D1 closed 2026-09-07g"), so it reads as live design for deleted code. src/core/ai_king.c:4394 — the revolution-end header says "Win: WoI + year>=1850 + no crown REF units", but the ported gate at :4826 is the real DOS C1 triple (crown colonies == 0, crown land units < 1/8, `ef[0]+(ef[1]!=0)+(ef[3]!=0) < 4`) with **no year gate**, as the inner comment at :4790 says outright; the same block's "Lose: WoI + zero coastal human ports" omits the colony and pop-share losses below it. src/core/ai_king.c:1490 — `ai_king_succession`'s header says "rank the 4 powers ascending by pop*3 + colonies*2 + SoL-ish term", contradicting the function it calls and that function's own verified header (`ai_king_rank_nations_0218` = `ship_counts*3 + colony_counts*2 + census_pop_proxy`, :474, DS −0x6be8 / −0x6d68 / −0x6bf0); the corrected inline comment sits eight lines below the wrong one.

24. Clean / no divergence found: the new `AiPopupRequest.chain` contact-chain machinery (ai_popup.c:47-77, :625-650, :664-680, :706-712) — `ai_popup_fill_base` memsets the request so `chain` is always derived, every contact-family enqueue does pass nation_a = Euro 0..3 and nation_b = Indian 4..11 or Euro peer (spot-checked ai_contact.c:364/628/708/3942/5754/6304/6344/6610/6754/9518 and ai_diplo.c:2263/2294/2653), the tags deliberately excluded (WHACK, EURO_WAR, VILLAGE_WARN, INDIAN_LAND) really do pass a unit id in nation_a (ai_contact.c:1408, :1492), and `ai_popup_set_last_portrait` stamps `queue[queue_count-1]`, which every new `ai_contact_chief_flair` site reaches only after a successful enqueue. `ai_contact_raid_port_ship` (ai_contact.c:7295) omits an explicit `u->active` test but `units_is_on_map` (units.c:1200-1203) already covers active + live id + not embarked. `ai_king_sol_percent` (ai_king.c:817-877) keeps the Bolivar display boost inside the pop-weighted loop, matching colony_production.c:226 and combat_strength.c:651. In ai_king.c the `1d42` royal purse, the `38fd_5be8` audience ladder, `1eca` mobilization, the `0982` scoring formula, the `4826` win-gate triple and the `20e6` sail-home were all checked line-by-line against the decompile and match.

---

# Sweep 3 — E. Colony production cluster (colony.c, colony_production.c, colony_yield.c, colony_craft.c, colony_preview.c, ai.c, ai_goals.c, col1_stuff_census.c, colony.h)

Deduped against docs/smell_audit_2026-09-09.md §E (60-71) and §D (44-46,55) by substance. Items below are new, several of them introduced by the recent Phase-A / Hudson / warehouse-capacity waves.

1. **colony_production.c:449 (+ turn.c:956/1054 vs :1160) — DOS Phase I/J run BEFORE Phase C/D in the port, so the rebel accumulators and the SoL latch see a roster the DOS ones never see, and `colony_prod_tick_rebel_accumulators` re-derives bells instead of consuming the Phase-A stamp created for exactly this purpose.** `colony_eot_production.md`'s phase table is explicit: C = 57349-57414, D = 57415-57485, F/G/H = 57502-57614, **I birth = 57615**, **J starve-kill = 57623-57695**. turn.c's order is I (birth, :956-1008) → J (starve-kill, :1030-1085) → C/D (`colony_prod_tick_rebel_accumulators` + `colony_prod_refresh_sol_flags`, :1160/:1162) → F/G/H (:1215+). Consequences, all live: `cc->rebel_divisor += pop*2` uses the post-birth/post-starve head count; the bells the accumulator computes at :450 come from the post-I/J colonist array; and `colony_prod_sol_bonus` (tories = pop·(100−sol)) is likewise evaluated on the mutated roster. Worse, the function recomputes bells from scratch (`colony_prod_colony_bells_ff` at :450) while `colony->prod_bells_phase_a` already holds the Phase-A number stamped at turn.c:881 — the very "One number, two consumers" invariant this function's own header (colony_production.c:434-448, citing viceroy 57230/57231/57392) asserts. Fix is two-part: move the I/J block below F/G/H, and have the accumulator prefer `prod_bells_phase_a` when `prod_compose_stamp` matches, exactly as `turn_run_nation_ticks` already does. Confidence H (phase order is documented in-repo; only the magnitude of the numeric effect is uncertain — a birth/starve tick is needed to see it).

2. **ai_euro.c:3982-3993 `ai_euro_colony_has_docks` — the AI plot scorer answers the Fisherman docks gate with "coastal OR Docks-family building", while the tick, the preview and the two colony-screen copies all require the building; its own comment claims they are "the same shape".** turn.c:719-735, colony_preview.c:55-68, colony_screen.c:1596 and :3884, game_loop.c:9129 are five byte-identical building-only loops (turn.c's comment: "coastal placement alone is not enough"). ai_euro.c:3987 ORs in `colony_flags & COASTAL || map_tile_is_coastal(...)` and feeds that as `has_docks` into `colony_yield_for_worker` at :4028 and as `fishable` at :4182 — so the AI scores, assigns and food-plans Fisherman plots on dockless coastal colonies that the tick then pays 0 for. Same hole from the other side in **colony_yield.c:592-594**: `colony_yield_for_tile` hardcodes `has_docks = true`, so every one of its callers bypasses the gate — colony_screen.c:478 offers "Fisherman" in the human's job list for a dockless colony (yield > 0 there, 0 at the tick), and ai_euro.c:4464/:4595 score fisherman tiles the same way. In DOS there is no `has_docks` parameter at all: FUN_15eb_18ec reads the colony, so scorer and tick cannot disagree. Confidence M-H.

3. **colony_production.c:843/:846 — `colony_prod_worker_building_output_ctx` has no needle for "Rum Factory" or "Cigar Factory", so a colonist working the top tier of those two chains reads 0 output where colony_craft.c's recipe table pays him.** The badge/display path matches `"Rum Distill"` (misses "Rum Factory") and `"Tobacconist"` (misses "Cigar Factory"), while the sibling table colony_craft.c:59/:65 lists both explicitly, and the four other chains are covered on both sides ("Fur Fact", "Iron Works", "Textile", "Arsenal"). Two hand-maintained lists of the same fact that have drifted. Mostly masked today because colony_screen.c:2612-2620 overrides `amount` with `preview.craft_capacity[cargo]` when the preview is valid — but the local calc is the fallback, it is the value `colonies`-level unit tests assert, and `colony_prod_building_display_output_sol` is public API. Confidence H that the omission is real, M that it is user-visible.

4. **colony.c:1063 vs ai_euro.c:1626-1642 vs map.c:447 — three different definitions of `COLONIZE_COLONY_FLAG_COASTAL` (DOS colony +0x1c bit 0x40), one of them a side effect of picking the first build project, and the AI one clears the save-carried bit with an uncited predicate.** (a) At founding, colony.c only ORs the bit inside the `else if (map_tile_is_coastal(...))` Docks branch — a coastal colony founded at pop ≥ 3 (Stockade branch) or one where "Docks" is missing/owned never gets it. (b) `ai_euro_refresh_colony_ai_flags` recomputes it every AI turn from `!map_tile_is_land()` over **4** orthogonal neighbours (off-map reads as water ⇒ map-edge colonies become coastal) and **clears** it otherwise — so a diagonal-only-water colony that DOS marked coastal loses the bit on the port's first AI tick. (c) `map_tile_is_coastal` is 8-neighbour `map_tile_is_water`. The reader at ai_euro.c:14157 cites "raw 2054: +0x1c bit 0x40", i.e. the bit is DOS-real state; the writer at :1639 carries no citation. Both readers defensively OR with `map_tile_is_coastal`, which hides it today. Confidence M.

5. **ai.c:2738 + :2749-2753 — the `ai_indian_village_threat` header block still documents the term the code stopped using, and its reasoning is now false.** The header says `base = ... + min(indian.capitol_x, pop/2) + ...` and argues at length that "since capitol_x is a map column it is larger than pop/2 in practice, so it behaves as pop/2". The code at :2856-2871 was corrected on 2026-09-06d — the operand is `0x59a0 + nation_id*0x4e` = `indian[nation_id−4].tech` (+2), and `cap_term = min(tribe_tech, pop/2)` with tech in 0..~5 is a *small* cap, the opposite of the header's claim. A reader trusting the header would "restore" pop/2. Same block: `founding_fathers_nation_has(col1, c->nation_id, 16)` at :2919 spells Pocahontas as a literal where `FF_POCAHONTAS` (founding_fathers.h:78) exists two lines from other symbolic FF uses. Confidence H (textual contradiction).

6. **ai.c:3138 vs ai.c:2938 — two readers of `t->mission` use two different "no mission" sentinels, and the growth one indexes a 4-element array with the raw nibble.** `ai_indian_152e_village_growth` uses `t->mission != 0xff` then `mission_nation = t->mission & 0x0f`; `ai_indian_village_threat` uses `(int)(int8_t)t->mission >= 0`; every other reader in the port (ai_contact.c ×12, reports.c:3510, units.c:4178) uses `COL1_TRIBE_MISSION_NONE`. col1_save.h:729 documents "0xff none; else low nibble = European nation id (0..3)". The growth path then does `ind->euro_relation_accum[mission_nation]` (col1_save.h:854, `int8_t[4]`) unbounded at ai.c:3147/:3157/:3182 — a nibble of 4..15 walks off the array into the neighbouring `ColonizeCol1Indian` fields. Not producible by any port writer today (all write `e` ∈ 0..3, optionally | 0x10) so this is latent, but a DOS-authored save or a future writer makes it live; the sibling `col1_tribe_attitude`/`_set` used two lines later *are* bounds-checked, which is the tell. Confidence M (inconsistency certain, exploitability L).

7. **colony.h:339-340 — `COLONIZE_COLONY_FLAG_SMALL_AI` (0x10) and `COLONIZE_COLONY_FLAG_WAGON_TRAIN` (0x20) are write-only with no explanatory note, unlike every other flag in the block.** Sole writers are ai_euro.c:1601/:1604 and :1622/:1625; zero readers anywhere in src/ or tests/. Both are re-derived from live state every AI turn and both *clear* the bit on the negative arm, so a DOS-authored save's value is discarded rather than carried. The neighbouring bits that are legitimately write-only (`AI_WANTS_PIONEER_CLEAR`, `AI_NEEDS_MILITARY`) each carry an explicit "WRITE-ONLY here / documented debt" paragraph; these two carry no comment and no DOS citation at all, so there is nothing to distinguish "save-visible state we keep on purpose" from dead code. Confidence M-L (note, not necessarily a behaviour bug).

8. **ai_goals.c:546-560 — `ai_goals_site_nibble_074a` caches its "does this map carry map-gen site nibbles" probe in file-scope statics keyed only on `(map->seen pointer, width*height)`, and `ai_goals_reset` (:21) does not clear it.** A new game or a load that reuses the same `seen` allocation at the same address with the same dimensions (the common case — the pool is re-`calloc`'d per map, and the map size is fixed by the scenario) keeps the previous game's `s_nib_present` verdict, silently switching the whole founding-site extras term between the real nibble and the `unseen ? 4 : 0` fallback. Every other module-level cache in this file is reset (`s_goals`, `s_work`, `s_inv`, `s_plan`), and ai_contact.c got exactly this treatment in the previous audit (#58). Confirmable by calling `ai_init_new_game` twice with different map sources and logging `s_nib_present`. Confidence M-L.

## Checked and clean (no finding filed)
- Preview vs live tick, arithmetic pass: field/commons composition, AI food `difficulty>>1`, horse-breed inputs, craft ordering, hammers `Spring-only` next-tick gate and lumber cap (`stock + field_gross[LUMBER]` ≡ turn.c's post-field `stock`) all reconcile term for term between colony_preview.c and turn.c's `turn_produce_one_colony`. The bells/crosses FF-input derivation is now triplicated (colony_preview.c:192-212, turn.c:604-621, colony_production.c:422-433) but the three copies are currently identical.
- colony_craft.c recipe table order matches the documented DOS ledger (Ore→Tools first, Tools→Muskets last); the ore-shortfall→tools→muskets cascade works via shared stock in both the live and preview passes; #71's clamp is present in both.
- The four `alarm < 25/50/75` quartile copies (ai.c:2547, ai_contact.c:4955/:8912, ai_diplo.c:3646) still agree — prior audit #59 already carries the duplication note.
- The Food warehouse-capacity exemption introduced with the #25 fix is present at all three DOS-cited sites (colony.c:2903 spoilage skips cargo 0, colony.c:3021/:3070 unload confirm, colony_screen.c:2777 alert colour) — no fourth site was missed.
- `colony_prod_refresh_sol_flags` hysteresis chain, `colony_prod_horse_breed`, the `colony_prod_building_tier` needle ordering, `ai_goals_stack_settler_pick`/`_colony_balance_flags`/`_founding_expansion_urgency` transcriptions, and `col1_stuff_census.c`'s three counters (post-#74 — the ×8 `combat_unit_base_x8` values and the saturating/wrapping split are as cited) all read faithful.

---

# F. Save / bridge — sweep 3 (2026-09-10)

Files audited: `src/core/col1_bridge.c`, `col1_save.c`, `col1_save.h`, `col1_post_map.c`,
`savegame.c`, `save_load_dialog.c` (+ docs/savegame.md, docs/save_format_map.md).
Deduped against docs/smell_audit_2026-09-09.md §F (#72-#82): all eleven are fixed in
the tree (bounds on both route decoders, `holds_occupied` on all three cargo paths,
shared `col1_save_human_nation_from`, suppress-bit prose, head-stamp trio, building RMW,
probe head validation) — none re-filed. The findings below are new; #1/#4 are seams the
recent fix waves left behind.

Fixture evidence below was gathered by decoding the raw `.SAV` records directly
(head counts at +42/+44/+46, unit stride 28: `type`@+2, `nation|vis`@+3,
`moves_spent`@+5, `orders`@+8) across all 47 size-valid saves under `original_saves/`.

1. **col1_bridge.c:2355-2360 — the `moves_spent` refund that smell #75 removed for land
   units is still live for every transport.** The first arm of the export ladder,
   `if (aboard || (transport && (orders==SENTRY || orders==NONE || !units_orders_follow_goto(orders))))
   spent = 0`, fires for any ship/wagon whose orders are not GOTO/AI_SAIL/AI_MOVE/TRADE_ROUTE,
   so the "exhausted ⇒ export the whole allotment" branch four lines down (:2367-2394, the #75
   fix) is unreachable for transports: a ship the player moved 2 of 4 tiles saves as
   `moves = 0` and reloads with a full turn. The comment cites "idle transports … like
   COLONY00", but the code cannot distinguish idle from spent, and DOS contradicts the blanket
   rule: of 254 on-map Euro transports with non-goto orders in the fixture set, **104 carry a
   nonzero spent byte** — e.g. `original_saves/colony-prod-tests/COLONY00-original.SAV` unit 2
   (Merchantman, nation 3 = the human, orders 0, `moves = 18`), `dutch-campaign/COLONY01.SAV`
   unit 35 (Caravel, human, orders 0, `moves = 15`), unit 36 (Wagon Train, orders 0,
   `moves = 6`). AI ships escape because their orders are 11/12 (goto-following), which is why
   the TURN goldens never caught it. Confidence H. (Fixing it will move human-save goldens;
   the `orders==SENTRY || orders==NONE` disjuncts are also dead — `units_orders_follow_goto`
   already excludes both.)

2. **col1_bridge.c:2685-2828 — capture exports the Europe dock only through `(236,236)` mirror
   units, and three of the six dock-push sites never create one, so those immigrants are
   deleted by a save/load.** `col1_bridge_apply`'s own comment (:1267-1272) states the rule —
   "capture only walks the unit pool, so without the mirror a loaded dock colonist vanished
   from the next save" — and `europe_spawn_dock_mirror_unit` is called from exactly three
   places: turn.c:2471 (crosses immigrant), units.c:1169 (Brewster pick) and the import path
   itself. The dock pushes at **europe.c:323** (`europe_disembark_passengers_to_dock` — every
   passenger a ship carries *from* the New World to Europe, whose pool units were already
   despawned by `units_despawn_ship_with_cargo`), **ai_king.c:3986** (King's mercenary Veteran
   Soldiers) and **ai_diplo.c:2206** (unit sent home to Europe) create no mirror, so nothing in
   the save file records them: save → load loses the units outright. Confidence H (code path is
   unambiguous; worth a one-line runtime check to be certain no other site spawns the mirror).

3. **col1_bridge.c:1254-1302 — Europe-dock import ignores `europe_dock_push_load`'s failure,
   then stamps `dos_type` onto an unrelated immigrant and spawns an orphan mirror.**
   `EUROPE_DOCK_MAX` is 8 (europe.h:16) and DOS has no such cap: `original_saves/french-campaign/
   COLONY02.SAV` (human = nation 1) carries **24** nation-1 land units at Europe coords, and
   COLONY03/COLONY04/COLONY09 carry 20/13/13. Pushes 9..24 return false, after which
   `if (europe->dock_count > 0 && src->type <= 5) europe->dock[europe->dock_count-1].dos_type = src->type`
   overwrites dock[7]'s DOS type (wrong @ARMOPTIONS kit for a different immigrant, and
   `europe_apply_dock_unit_kit` is then applied to the new mirror from that same stale value),
   and the mirror unit is spawned regardless — 16 pool units at (236,236) with no dock row,
   invisible in the Europe screen. Sibling guards exist elsewhere (`europe->bound_ships <
   EUROPE_HARBOR_MAX` at :1155, `europe_enqueue_expected`'s own check), only this push is
   unchecked. Confidence H.

4. **col1_bridge.c:2846-2848 vs :1519-1529 — the head-stamp trio added for smell #78 is
   write-only, and import contradicts it.** `map_mode` (DS:0x5390) and `no_unit_selected`
   (DS:0x53c6) now get stamped on every capture, but grep shows no reader anywhere outside
   `tools/col1_json.c` and the unit test — `col1_bridge_apply` never looks at either, and when
   `active_unit == 0xffff` its fallback scan picks the first on-map unit as `selected_id`. So the
   four DOS View-Pieces saves the #78 comment itself surveys (`active_unit 0xffff`, `map_mode 1`)
   load as Move-Pieces with a unit selected and re-save as `map_mode 0`, i.e. the encoder now
   writes a field the decoder is guaranteed to invalidate. Confidence H that it is asymmetric,
   M on whether the idle state is worth restoring. Same class as the known
   "decoder honors bits the encoder zeroes", inverted.

5. **col1_bridge.c:2728-2730 vs :116-132 and :1366-1369 — four copies of the trade-route
   cursor validation, three different rules.** The map-unit decoder requires
   `route_valid && dest_count > 0 && route_sea == unit_sea`; the Europe-lane decoder
   (`col1_bridge_europe_ship_route_load`) requires `sea != 0` (equivalent for ships); but the
   Europe-lane *encoder*'s `on_route` test checks only `route_slot < 12 && dest_count > 0` — no
   domain test — and the map-unit encoder (:2451) checks neither `dest_count` nor `sea`. A ship
   whose `trade_route_plus1` points at a land route is therefore written out with `orders = 2`
   plus a cursor that both decoders then reject, dropping the automation on load instead of at
   save time. Low impact today (the Begin picker presumably prevents the mismatch) but it is the
   "guard in 3 of 4 sibling sites" shape and the encode/decode pair is not symmetric.
   Confidence M.

6. **docs/savegame.md:162-163 contradicts col1_bridge.c:2327-2340 on native `vis_mask`.** The
   doc says "Unit `vis_mask`: euro owner bit only on spawn/`units_set_nation`/capture; natives
   export **0**". Capture zeroes `vis` only for Europe-sentinel records (`x/y >= 200`) and
   otherwise round-trips `col1_vis_mask & 0x0f` for natives. The code is right and the prose is
   wrong: 492 of 3074 on-map native unit records in the fixtures carry a nonzero vis nibble
   (values 1/2/4/8 and combinations), i.e. DOS really does track which Europeans have seen a
   brave. (Checked the neighbouring claim too — "Euro owner bit always present on MAP units" —
   and it holds in the data: 3004/3004 on-map Euro records have their own bit set.)
   Confidence H, severity L (prose only, and it invites someone to "fix" the code to zero it).

7. **col1_save.c:708-719 — a failed write destroys the slot's previous save.**
   `col1_save_write_file` opens the target with `fopen(path, "wb")` (truncate) and only then
   runs `emit_to_stream`; any failure (disk full, short `fwrite`, the `emit` validation
   rejecting the head) leaves a truncated `COLONY0N.SAV` where the player's last save was, and
   `savegame_probe_col1_slot` will then list it as `(EMPTY)`. The sibling `col1_save_write_memory`
   has no such exposure (it validates into a private buffer). A write-to-temp-then-rename, or
   validating before `fopen`, would remove it. Confidence H on the mechanism, severity L-M.

**Clean:** `col1_post_map.c` (planes/tallies/cache all match the cited `FUN_67f4_0088` /
`FUN_6662_00f2` structure, tail preserved on rebuild), `col1_save.c`'s codec (read and write
walk the same section order and both derive sizes from one `col1_save_expected_size_counts`),
`save_load_dialog.c` (row format, widen-to-widest and the 0-7/0-9 slot split all match
savegame.md), and the head/stuff atlas rows in save_format_map.md agree with the struct
comments everywhere I cross-checked (focus vs viewport pair, mask bit 0x04 = suppress,
trade-stop load@+3 / unload@+6).

---

# Smell audit sweep 3 — G. Europe / trade / reports / FF (2026-09-10, HEAD 9e1a38f)

Files audited: `src/core/europe.c`, `reports.c`, `trade_screen.c`, `founding_fathers.c`,
`ff.c`, `new_game.c` (+ headers). Deduped against `docs/smell_audit_2026-09-09.md` §G —
#83, #85, #86, #87, #88, #90, #91, #92 all verified fixed in the tree and are NOT re-filed.
`ff.c` is a MADS font loader only (89 lines, nothing game-mechanical) — clean.
`trade_screen.c` re-checked end to end against its own DOS cites (0f2c strip x, 1064 column
bounds, 09da grid, nibble encode/decode pairing): clean, no finding.

1. **europe.c:3647 — AI colony dump-sell pays `euro_price`, but the DOS byte it cites
   (`DS:0x84BC`, decompile `-0x7b44`) is already `euro_price − 1`; every AI warehouse sale
   is 1 gold/ton too rich, and the citation in the comment block at :3578-3585 is a
   misread.** `-0x7b44` and `0x84bc` are the same address (signed vs unsigned 16-bit), and
   *every* writer of that 4×16 table stores the derived sell price, never the base:
   `viceroy_unpacked.c` FUN_38fd_0058 tail (58996-59002) `iVar11 = *(char*)(*(int*)0x84fc +
   cargo + 0x4c) − 1; if (iVar11 < 0) iVar11 = 0; *(0x9e12*0x10 + cargo − 0x7b44) = iVar11`,
   and the two nation-bind copies `viceroy_overlays.c:30272-30276` and `:36095-36100` do the
   identical `+0x4c − 1` clamp. The port's `nat->trade.euro_price[]` is the *record* array —
   `offsetof(ColonizeCol1Nation, trade.euro_price) == 0x4c` and `sizeof(ColonizeCol1Nation)
   == 0x13c` (verified by compiling the header), i.e. exactly the `+0x4c` DOS reads *before*
   the `−1`. So DOS's dump-sell price == `europe_sell_price()`, which is what this line was
   changed away from by the prior audit's #84 "Secondary" note. The `price <= 0` fallback at
   :3649 has the same defect (`eu->cargo[c].bid`, not `bid − 1`). Same misread propagates to
   the scoring sites that cite `DS:0x84bc` while reading the raw byte — ai_euro.c:14330
   (`v = euro_price*qty` written into `trade.gold[]`), :14184, :14421, ai_diplo.c:3139.
   Confidence H (mechanism), H (that comment and code disagree with 0058's tail).

2. **europe.c:1934-1937 — the @ARMOPTIONS buy/sell rows pass `col1 = NULL` to
   `europe_apply_trade_volume`, so the `trade.tons/tons2/gold` ledger is never written,
   directly contradicting this function's own comment 300 lines above.** europe.c:1639-1640
   states "The Crown's cut is still reflected in the per-cargo revenue ledger, which is
   FUN_38fd_1dfa's own (100−tax) term, applied below by the ledger call" — but
   `europe_apply_trade_volume`'s entire ledger block is inside `if (col1 && …)` (:2574), so
   with NULL only `eu->trade_nr[]` moves. Prior audit #50 fixed exactly this on the other
   five channels ("Residual on the wrapper: arm rows … no DOS arm located — #62"); #62 was
   then REFUTED *by locating the arm rows* (38fd:3b4a/3ba0/3bfc, each calling
   `291f_0a2e` = FUN_38fd_1dfa), so the reason for the residual is gone but the NULL stayed.
   Every caller (game_loop dock-menu handler) has `&game->col1` in hand.
   Confidence H (code contradicts its own comment); M on player-visible impact (F6 Economic
   tons/gold columns and the long-run tons2 price pool under-count arm trades).

3. **Two treasuries for the human nation with no per-turn sync: `europe.gold` is live,
   `col1.nation[human].gold` is written only at 4 scattered sites, and readers are split
   between them — so gold earned outside Europe evaporates and gold-gated AI decisions read
   a stale purse.** `col1_bridge_apply` seeds `europe->gold = nat->gold` (col1_bridge.c:1544)
   and `col1_bridge_capture` writes it back (`:1964`), but capture runs only from
   `game_save_col1` (game_loop.c:4957). In between, only game_loop.c:1446, :3018, :10910 and
   :11064 sync. Consequences found: (a) colony.c:1971-1972 credits colony-capture plunder to
   `nation[new].gold` only (units.c:6339 path) — the human's plunder is discarded at the
   next export, which overwrites `nat->gold` with `europe.gold`; (b) reports.c:3995 F10 Score
   treasury reads `nat->gold` (stale) while the no-col1 fallback 35 lines later reads
   `europe->gold`; (c) turn.c:2669 rival-strength `gold/100` and ai_king.c:1032 tax-event
   score read the stale value; ai_king.c:3398-3417 checks a price against `nat->gold`, then
   writes `ctx->europe->gold = nat->gold`, clobbering the live treasury with the stale one.
   europe.c:3501+3515 (Custom House) is the only europe.c writer that bumps *both*, which is
   why it looks correct and the harbor paths do not. Confirm by: load a save, spend in
   Europe, open F10 — the Score treasury line lags the sidebar's Gold. Confidence M-H.

4. **europe.c:3484 — the Custom House sells an AI nation's goods at the *human's* market
   price while taking that AI nation's own tax rate (:3446); DOS reads both from the one
   bound nation record.** `viceroy_unpacked.c` 57277-57302: price is
   `FUN_291f_09ea(cargo)` → FUN_38fd_0040 = `*(int*)0x84fc + cargo + 0x4c` and tax is
   `*(char *)(*(int *)0x84fc + 1)` — the same `0x84fc` record, so an AI colony's Custom House
   is priced off that AI's own `trade.euro_price`. `europe_sell_price()` reads
   `eu->cargo[].bid`, which col1_bridge binds to the human nation only (col1_bridge.c:1616).
   The file's own AI dump-sell 160 lines below (:3647) does the per-nation lookup with a
   documented fallback, so the two AI selling paths in one file disagree about where an AI's
   price comes from. turn.c:1756 calls this for every colony including AI ones.
   Confidence M-H.

5. **europe.c:549 — recruit-pool refills roll on a private glibc-style LCG seeded from the
   treasury (`1u + gold + recruit_passage + slot*17`), so the replacement colonist's class is
   a deterministic function of how much gold you happen to hold.** `europe_rng_next`
   (:27-34) is a local `s*1103515245+12345`; all five in-play refill sites pass
   `rng_state = NULL` (:1217 paid recruit, :1226 and :1242 free/FoY, :1284 and :1297
   immigrant), so the local seed is always used. DOS FUN_38fd_4884's refill tail rolls
   `46d4` off the shared game RNG (`FUN_281f_04d4`), the same stream the sibling *slot* pick
   in `europe_immigrant_from_pool` correctly takes a `ColonizeDosRng*` for (:1270). No
   citation anywhere for the seed formula, and the brief's deliberate-deviation list covers
   only the music picker. Secondary: `europe_apply_brewster` (:538) substitutes job `0x13`
   where its own comment (:527) says DOS writes `0x1c` — a difference the duplicate-check in
   `europe_roll_pool_profession` (:511) can see. Confidence M.

6. **The Europe boycott mirror is refreshed only while the Europe *screen renders*
   (game_loop.c:4770-4779), yet three sell paths that never touch that screen consult the
   mirror.** `europe_cargo_boycotted` (:3059) reads `eu->boycott_bitmap`; ai_king tea-party
   (ai_king.c:1201/1226) and ai_diplo embargo write only `nation.boycott_bitmap`. So an EOT
   trade-route unload (`europe_sell_hold` via game_loop.c:11054), a map/transport dump-sell
   (`europe_sell_unit_hold`, game_loop.c:10910) or a wagon route run in the same EOT as a
   tea party still sells the boycotted cargo. Mirror-image asymmetry inside the FF code:
   Fugger's elect clears only the nation word (founding_fathers.c:1193) while
   `europe_buyback_boycott` clears nation **and** mirror (:3120-3121). Confidence M on the
   inconsistency, M-L on reachability (depends on tea-party vs trade-route slice order).

7. **europe.c:2407 — the `docked_with_goods` scan is the one hold consumer left without the
   255 empty-hold sentinel guard, so a sentinel hold fires the once-per-game "Cargo from the
   New World" woodcut.** Every sibling guards it: `europe_goods_slots_used` :141,
   `europe_sell_hold` :3247, `europe_sell_hold_partial` :3302, `europe_best_sell_hold` :3963,
   `europe_buy_cargo` :3886, and reports.c:2768/:2831 (fixed as prior-audit #91, with a
   comment that explicitly names this as the shared rule). The value reaches the harbor ship
   unfiltered — `units_despawn_ship_with_cargo` (units.c:10825-10830) copies raw hold bytes
   into the Expected slot. Consumer: turn.c:2483 → `woodcut_fire(WOODCUT_CARGO_FROM_THE_NEW_WORLD)`.
   Confidence M (guard is clearly the odd one out; whether a 255 hold reaches a *harbor*
   ship in practice needs a save with one to confirm).

8. **founding_fathers.c:209 `founding_fathers_intervention_bells` is a read-only API with no
   caller — the counter it exposes is write-only.** Declared in founding_fathers.h:99,
   incremented at :654 from `founding_fathers_consume_woi_bell_pool`, and the field comment
   at :32 says "Score line: +1 per WoI bell-pool spend on foreign intervention" — but
   `reports_compute_score` (reports.c:3943-4040) has no such term and nothing else in
   src/ calls it. Either dead code or an unfinished Score line. Confidence H (dead), L on
   whether DOS wants the term at all.

9. **founding_fathers.c:1212 — stale comment contradicts the unified FF-ownership rule.**
   The de Witt case says "FA detailed strength already peeks head.founding_father[4]
   (reports)", but reports.c:3184 now tests `reports_ff_owned_by_nation(&col1->nation[human],
   REPORTS_FOREIGN_DE_WITT_FF)` — the per-nation bitmask, as the rest of the tree does after
   the #82/#83 unification. Otherwise the bitmask-vs-count sweep is clean: no remaining
   `head.founding_father[]` ownership read anywhere (only the write-once first-claimer stamp
   at :1360 and the Score/name-grid bitmask readers). Confidence H, severity L (comment only).

10. **europe.c:2004 — `europe_harbor_push` hard-codes `cargo_professions[i] = -1` while its
    mirror `europe_enqueue_expected` (:2049-2050) carries the professions through.** Only
    live caller with passengers is ai_diplo.c:2202, so the impact today is a gifted ship's
    passengers losing their @JOB (labels fall back to the base unit name via
    `reports_naval_passenger_label`). Confidence H that the pair diverges, L on impact.

---

# Sweep 3 — H: map / UI / popup (map.c, map_gen.c, map_menu.c, map_panel.c, popup.c, popup_msg.c, pedia.c, colony_screen.c, woodcut.c, declaration.c)

Deduped against docs/smell_audit_2026-09-09.md §H (#95–#106) by substance: those are all fixed and I re-verified the fixes at their new sites (caret rule, fog ocean-rescan, dest-halving, rumour +1 bias, tribe chrome sentinel, `pedia == 27`, dead PHYS0 bases, dead `case WOODCUT_A_NEW_WORLD`, popup `run[256]`). Nothing below repeats one.

1. **src/core/map_panel.c:1726 and :1527 — the fog-view sentinel that #100 fixed inside `map_panel_draw_tribe_chrome` still leaks into the rest of `map_panel_render`.** The parameter is *named* `human_nation` but game_loop.c:16087 passes `game_fog_nation(game)`, which is −1 under Complete Map and a *foreign* nation under SETVIEW (game_loop.c:3375-3386). Every fog use (:1288/:1302/:1317/:1335/:1436) treats −1 correctly, but two non-fog uses do not: `const bool own_stack = top && top->nation_id == human_nation;` (:1726) makes the player's **own** stack read as foreign — the sidebar collapses it to one "<Nationality> <Type>" row instead of the own-stack listing — and `map_panel_euro_country(..., human_nation)` (:1527, helper at :475) then never returns the player's custom country name for their own colonies. `map_panel_draw_tribe_chrome` (:601-608) resolves the sentinel back through `head.curr_nation_map_view` / `head.human_player` exactly because of #100; the same resolve is missing here. Confirm by opening Complete Map (or SETVIEW to another nation) and clicking one of your own multi-unit stacks. **M-H**

2. **src/core/colony_screen.c:3985-3992 vs :4131 — the Custom House popup hardcodes width 130 in a comment that cites the `@width=190` it is ignoring; its sibling dock-orders popup hardcodes 190.** The comment reads "GAME.TXT @CUSTOM's own @width=190 — the title … is the widest line", then sets `int dialog_w = 130;` and only grows it to the measured title width. COLONIZE/GAME.TXT:2086-2090 does carry `@width=190`, and `popup_msg_fill` already latched it (popup_msg.c:371) — but colony_screen.c is the only popup owner in the tree that never calls `popup_msg_section_width`/`popup_msg_take_pending_width` (ai_popup.c:205, save_load_dialog.c:105, game_loop.c:5982 all do). The dock-orders popup 140 lines later hardcodes exactly 190 for the identically-declared `@COLONYUNIT` (GAME.TXT:1767-1769), so the two siblings disagree about where the number comes from. **M**

3. **src/core/map_menu.c:843-867 — dead duplicate of `map_menu_layout_titles` whose constants have diverged from the live copy.** Both `map_menu_load` and `map_menu_layout_titles` (:1258) compute `title_x`/`title_w`, but the load-time copy uses gap `+4` (`x += menu->title_w + 4`, :852) where the live one uses `+6` (:1271), measures titles as `strlen*6` instead of `map_menu_text_width(font, …)`, and omits the `title_x + title_w > 320` clamp the live copy applies to PEDIA (:1285-1287). Nothing can read the load-time values: `map_menu_handle_input` (:1422) and `map_menu_render` (:1544) both call `map_menu_layout_titles` before touching `title_x`, and `map_menu_hit_title` (:1379) only runs inside handle_input. Either delete it or make it call the same helper — as it stands it is a diverged second answer to the same question. **M-L**

4. **src/core/map.c:1972-1998 — `map_move_cost_step` is missing the tribe/settlement cap arm that its DOS-cited sibling `map_move_spent_thirds` (:1940-1970) applies.** 465b:00e4 (`original_sources_annotated/ai/move_spent.c:124-136`, accessors.c:324-327) caps the step at 3 thirds when `tile_tribe_owner(dest) >= 0`; `map_move_spent_thirds` ports it, `map_move_cost_step` (whose header at map.h:346-350 claims "same rule at NAMES scale") does not, so a village/colony destination whose terrain class costs 2-3 reports 2-3 there and 1 in the sibling. Currently masked — the only callers of `map_move_cost_step` are tests/unit/test_units.c:4512 — but this is the third round-trip through these two helpers (audit #49, #97), and the header comment now overstates the agreement. **L** (dead outside tests; would matter the moment anything wires it).

5. **src/core/popup_msg.c:87-96 vs src/core/pedia.c:270-285 — two implementations of FUN_6f74_0c32's caret rule that disagree on 3+ carets.** popup_msg counts `while (line[caret] == '^') caret++;` and treats `caret >= 2` as centre; `pedia_caret_flags` (the copy that carries the DOS citation) eats exactly two and leaves a third as body text, matching its own header ("Anything further (a third caret, braces, spaces) is body text"). No `^^^` line exists in COLONIZE/GAME.TXT today, so this is latent, but the two parsers of the same DOS routine should not differ. **L**

6. **src/core/map.h:184-197 — the doc block for `map_tile_tribe_or_presence` has been orphaned onto `map_tile_is_lake`.** The comment describing "Owner … settlement (HAS_CITY) … else an occupying unit (HAS_UNIT) … FUN_1000_88c2 / FUN_137f_0428 … Cite: euro_unit_act.md T1.8 (0015bc hard-reject)" now sits immediately above a *second* comment block (the Lake one) and then `bool map_tile_is_lake(...)`; the function it actually documents is declared bare on the next line. The T1.8 hard-reject citation is the one thing justifying units.c:8213/:8615's use of the helper, so it should not read as documentation of the lake predicate. **L** (prose only; the code is right — FUN_137f_0428 does city-owner-else-unit-owner, viceroy_unpacked_2.c:5559-5569.)

7. **src/core/colony_screen.c:3970-4046 — `@CUSTOM`'s `@smallfont` directive is parsed nowhere.** GAME.TXT:2086-2090 carries `@checkbox` (honoured — the bullet at :3938) and `@smallfont` (not). `@smallfont` is honoured for the map menus (game_loop.c:4423) and the Pick Music dialog (pick_music.c:126), so this is the same dead-directive shape audit #99 filed against pick_music, one screen over. The colony screen has no tiny font loaded, so fixing it is not a one-liner — worth recording rather than silently diverging. **L**

## Clean / checked and refuted

- `declaration.c` — read in full; the #94 `continue` is now a real abort-the-run (`goto LAB_43f7_19f2` mirrored at :173-178), frame clamps and the sprite-index +1 are consistent with the header. No findings.
- `woodcut.c` — the `case WOODCUT_A_NEW_WORLD` note is accurate; ids 3/4/5 do have a live trigger (ai_contact.c:740-746, keyed on tribe *slot*), and the DS:0x540a bit arithmetic matches its `_Static_assert`.
- Building hit-test AABB (colony_screen.c:4779-4796): I checked the "inclusive" wording against FUN_1262_00f6 (viceroy_unpacked_2.c:2949-2950 — `x0 <= px && px <= x0 + w - 1`); the port's exclusive `px < x0 + w` is exactly equivalent. Not a defect.
- Pedia "Move Cost"/"Defense" (pedia.c:1632-1633, 1712-1714) read NAMES.TXT columns 1/2; I diffed all 29 rows of column 1 against `k_map_dos_terr_cost` (map.c:1830) — identical. The ×25 defense scale matches combat_analysis.c:231.
- The four cardinal-order tables that feed PHYS0 104+q (fog edges map.c:988-1118, land transitions map.c:1305-1354) all index `mapedit_card_dx/dy` with the same `q`, so fog and terrain edges cannot disagree on sprite id.
- `game_modal_open` (game_loop.c:16476) covers all 14 modal `open` flags declared in src/core/*.h — no modal is missing from the blocking invariant.
- `popup_markup_text_width`'s `plain[512]` cannot truncate in practice: every body buffer is `AI_POPUP_BODY_LEN` = 512, so a stripped copy always fits. Not filing it.

---

# Sweep 3 — area I: assets / loaders / sound / platform

Files audited: assets.c, ss.c, pik.c, madspack.c, ff.c, font.c, strutil.c, json_min.c,
settings.c, sound.c, gsound_vm.c, pick_music.c, opening.c, closing.c, dos_rng.c, main.c,
platform/diagnostics.c, platform/dos_compat/, platform/linux_sdl2/.

Checked and found clean: gsound_vm.c table/bounds handling (`handler_for`'s `id > bound`
verified against the real GSOUND.COL — DS:0xF8/FA/FC/FE hold absolute max ids 0x08 /
0x3f / 0x5c / 0x8026, so the raw-id compare is right); `GSOUND_TICK_HZ = 1193182/19903`
matches the driver's own PIT divisor 0x4DBF and is *not* a conflation of the two game
clocks; `dos_rng.c` matches FUN_1d1d_0e04 / FUN_19ef_0032 (the `& 0x7fff` seed mask is
FUN_19ef_001a's, folded in — only the comment is loose); no libc `rand()`/`srand()`
anywhere in the tree; `sound_pick_next_tune_id`'s seven category pools and the
category-0 back-classification (`tune<=6?1:…`) match asm 129f:01a8-02ad byte for byte
(the apparent tune-7 off-by-one between pool and classifier is DOS-real — do not "fix");
madspack/ss/pik/ff failure paths have no leaks or double frees.

1. src/core/sound.c:744-761 — `sound_dispatch_gated_unlocked` gates the two id classes with the wrong two of the three DOS sound options, and invents a gate for the chord stings. DOS FUN_12d8_000e (asm 12d8:000e-0050) sets BX=1 for `CMP CX,0x10 / JGE` (signed), then plays when BX; else a 0x20-bit id plays iff **DS:0xa0** != 0, and a 0x40-bit id is skipped iff **DS:0xa4** == 0. The options dialog FUN_2b5a_23ce (asm 2b5a:23ce-2447) binds checkbox 1 → DS:0xa2 → DS:0x5386 bit 0x2, checkbox 2 → DS:0xa0 → bit 0x4, checkbox 3 → DS:0xa4 → bit 0x8, and COLONIZE/GAME.TXT:111-118 `@SOUNDOPTIONS` lists those checkboxes as Background Music / Event Music / Sound Effects — the same order col1_save.h:127-136 already encodes (`Tut2` bit1 background_music, bit2 event_music, bit3 sound_effects). So DOS gates songs (0x20..0x3f) by **Event Music** and event ids (0x40..0x5c) by **Sound Effects**; the port gates them by `background_music` and `event_music`, i.e. both shifted one option. Observable: with Background Music off + Event Music on, DOS still plays explicitly-requested tunes (e.g. the declaration cue) and the port goes silent; with Sound Effects off, DOS silences the whole 0x40 class while the port still emits their MIDI stings (only `sound_vm_sfx_cb`:487 suppresses the COLDIG sample). Separately, ids ≥ 0x8020 hit DOS's *signed* `CMP CX,0x10` as negative ⇒ BX=1 ⇒ always played; the port's :748-753 gates them on `event_music` and the comment there admits it is a guess ("treated like event music"). Confidence H.

2. src/core/assets.c:171-175 — `assets_detect_madspack` reads the MADSPACK section count from header offset 13 while `madspack.c:221` reads it from offset 14; offset 14 is right. Verified on COLONIZE/EUROPE.PIK: bytes `4d…30 1a 00 03 00`, count = 3 at offset 14, whereas offset 13 yields 768. The startup asset probe (game_loop.c:7154) therefore logs `chunks=768` for every 3-section .PIK. The `n < 14` guard is also one short of the `hdr[14]` read, so at exactly n==14 it reads an uninitialized stack byte. Confidence H (empirically confirmed).

3. src/core/sound.c:857-871 + :890-900 + :727-735 — three smaller divergences in the same DS:0x9a/0x9e/0xa0/0xa2 scheduler. (a) The comment at :861 says "DOS only polls this with sound effects enabled or a pending change"; DS:0xa2 (the flag asm 129f:00fa tests) is **Background Music**, not sound effects — the code's `opts.background_music` is correct, the comment is not. (b) `sound_queue_unlocked` arms `pending` unconditionally, but DOS FUN_129f_02cc (asm 129f:02dd-02eb) arms DS:0x9e only when `[0xa0] != 0 && [0xa2] == 0`. (c) `sound_set_options` fades (`gsound_vm_play(vm,1)`) only when background_music went off; the DOS dialog tail (asm 2b5a:2447-2461) calls FUN_281f_04de(1) whenever *any* of the three flags is off. Confidence M-H for (a), M for (b)/(c).

4. src/main.c:134 vs src/platform/diagnostics.c:107-140 — the diagnostics banner is unreachable. `diag_set_info_enabled(prefs->debug_logs)` runs at main.c:134, but `diag_init` (main.c:95) has already emitted seven `diag_info` lines (log path, executable directory, argv[0], working directory, HOME, XDG_DATA_HOME) and `settings_init` its two ("Settings file: …", "Created default settings file: …"), all of which `diag_info` (diagnostics.c:179-182) drops because `g_info_enabled` is still false. Those are exactly the lines main.c:214 calls out as "for bug reports". Fix is ordering, not logic. Confidence H.

5. src/platform/dos_compat/dos_compat.c:8-35 — the whole tick/port-stub half of dos_compat is dead and its one live constant contradicts the documented DOS clock. `g_tick_rate_hz` is written by `dos_compat_set_tick_rate_hz` (only caller game_loop.c:7319, value 18) and read by nothing; `dos_compat_tick_count` returns a call counter with no relation to time and its only caller discards it (`(void)dos_compat_tick_count();`, game_loop.c:12010); `dos_compat_in_port`, `dos_compat_out_port`, `dos_compat_ptr_from_segment_offset` and `dos_compat_trace_unknown` have zero callers anywhere in src/, tools/ or tests/. The 18 Hz also contradicts the project's own tick documentation (game_loop.c:6756-6800: IRQ0 at PIT divisor 0x7a8 = 608.77 Hz, ÷10 = 60.877 Hz) — anyone who wires this up later inherits the wrong rate. Confidence H (dead), M (the 18 is a trap rather than a live bug).

6. src/core/reports.c:318 (+ colony_screen.c:721/734/747/760, europe.c:1080) — six surviving nearest-colour `remap_sheet_to_palette` call sites contradict the rule the same file states at reports.c:4538 ("the reserved-DAC-block pattern (crown-europe batch: merge, never remap)") and implemented for SCORE<nn>.SS. Worse, `view->icons` (ICONS.SS) is remapped **once at load** onto `backgrounds[COLONIZE_REPORT_RELIGIOUS]` (REPORT2.PIK) and then blitted on other screens: Congress page 1 draws it over REPORT3.PIK (reports.c:1365/1449) and the labor grid over REPORT4.PIK (reports.c:1759), whose palettes differ from REPORT2's in 85 of 256 entries. I checked the actual data: every index the LUT produces resolves to the same RGB under REPORT3/REPORT4, so nothing is visibly wrong *today* — this is a latent trap plus a rule violation, not a live defect. The helper itself exists as three byte-identical private copies (only variable names and blank lines differ) with no shared home in assets.c, which is where every other palette helper lives. Confidence H (duplication + rule contradiction), L (current visual impact — refuted by the data).

7. src/core/json_min.c:30-33, :72-73, :129-131, :192, :208, :231-232, :264-265 — the JSON parser checks no allocation for failure, while every sibling parser in the same layer does. `jv_new` dereferences `calloc` unchecked; `parse_raw_string`/`parse_array`/`parse_object` ignore both `malloc` and `realloc` results, and the `realloc` forms also leak the old block on failure. Compare assets.c:255-258 / :331-344 and madspack.c:201-206 / :228-233 / :253-257, which check and unwind every one. It parses settings.json and the sav_json tool's input. Confidence H (the asymmetry is factual); severity low unless allocation actually fails.

8. src/core/assets.c:299-311 — dead sentinel guards in the message-catalog parser. The enclosing test requires `line[1] < 'a' || line[1] > 'z'`, so `body` can never equal `"options"` or `"smallfont"`; the two `strcmp` guards at :310-311 can never fire. Lowercase directives already fall through the outer `if` and are stored as ordinary content lines — which is what pick_music.c:118/126 relies on — so the guards are leftovers, not the mechanism the comment at :299-300 describes. Confidence H, severity L.

9. src/core/opening.c:32-35 vs src/core/closing.c:302-307 — mirrored cinematics, one drops a hook. `opening_set_sound_hooks` accepts `set_bgm_fn` and discards it (`(void)set_bgm_fn;`) though game_loop.c:1078 passes `sound_set_bgm`; `closing_open` calls `g_closing_set_bgm(0)` before its cue precisely so `sound_play` takes the "no VICEROY pool" immediate branch (sound.c:917-925) instead of queueing behind the running song. Harmless today because `game_try_start_intro` only runs from main.c:198 with category still 0; it breaks the moment the intro becomes replayable. Confidence M.

10. src/core/settings.c — three read/write asymmetries. `"version"` is written (:110) and never read back, so `COLONIZE_SETTINGS_VERSION` cannot actually gate a migration. `settings_init` sets `g_settings_loaded = true` at :332 *before* `settings_load_file` at :348, so `settings_is_loaded()` reports true even on the corrupt-file path that returns false — and `game_try_start_intro` (game_loop.c:11962) keys off exactly that. `--scale` clamps to `>= 1` with no ceiling (main.c:73-75) while the settings-file path clamps 1..8 (settings.c:286), two answers to the same question. Confidence H, severity L.

11. src/core/assets.c:32-38 — `vga6_to8` silently passes through any channel > 63 instead of scaling, with no citation. On a palette where some channels exceed 63 the result is per-channel mixed scaling (that channel ends up ~4× darker relative to its neighbours), which would show as a hue shift rather than an obvious failure. All shipped VICEROY.PAL/COL768 data is 0..63 so nothing triggers it, but the guard is a heuristic guessing at input it never sees rather than a documented DOS rule. Confidence M, severity L.

12. src/platform/linux_sdl2/sdl_runtime.c:205 vs :529-536 — `platform_present` sizes its index→RGBA loop from `framebuffer->width * framebuffer->height` but `rgba_buffer` is allocated from the hardcoded 320×200 at :205; a larger framebuffer overruns the heap. Only main.c:216-221 supplies one and it is 320×200, so this is latent. Same function family hardcodes `0xFD` at :475 instead of `COLONIZE_SS_TRANSPARENT` (ss.h:10) — documented in platform.h:138, but it is the one place the magic number is re-typed. Confidence H (facts), severity L.

13. src/core/closing.c:198-207 — comment/code mismatch in the firework cue. The comment says "port elapsed 1/27/37/42 is DOS frame 1/27/37/42", but the test uses `frame = elapsed % n` (:197), not `elapsed`. With `repeats == -1` (the shipped CLOS-FWK row, :283) the sheet loops forever, so the four cues re-fire on every wrap rather than once as the comment implies. Whether DOS's `_anim_loop` re-triggers per loop is what would settle it; closing.h:47-49 cites the pre-increment counter but not the wrap behaviour. Confidence M.
