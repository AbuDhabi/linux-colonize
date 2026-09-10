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

   **RESOLVED 2026-09-10 — blocker now reads the target tile, like its sibling.**
   Re-read the raw myself: the gate is `if (bVar2) { local_16 = (0xd <= cand.type && cand.type
   <= 0x12); if (local_c != local_16) goto skip; }` at viceroy_unpacked.c:99186-99195 (the
   audit's "99190-99196" is one line off — 99190-99192 is just the `else { local_16 = 1; }`
   arm), and `local_c` is set at :99147 by `local_c = FUN_281f_0768(0x281f,uVar1,uVar6)` with
   `uVar1/uVar6` = `param_2`'s x/y from `0x3144/0x3145` (:99138-99139), guarded by the
   in-bounds `FUN_281f_0302` at :99144-99145 which is the only thing that sets `bVar2`. So
   `local_c` is ocean_or_high_seas of the tile `param_2` stands on, and `param_2` is the first
   unit on the target tile (`uVar17 = FUN_281f_07e0(0x281f,uVar16); local_c8 =
   thunk_FUN_2a1f_0458(0x281f,uVar17)` at :100353-100354) — never the attacker, which is
   `param_3`. `mover_sea = units_is_sea(pool, mover_id)` replaced with the same `tile_domain`
   idiom `units_best_defender_at` uses: `map_tile_is_water(map,x,y) ||
   map_tile_is_high_seas(map,x,y)`, over the same map resolution as
   `units_combat_strength_ctx` (`g_units_occupancy_map ?: g_units_fallout_map`), falling back
   to the attacker-ship-ness proxy only when no map is wired (headless fixtures) — exactly the
   sibling's fallback, since DOS's `bVar2` is false only for an absent `param_2`, never for an
   absent world. Both callers (:6233 walk-in, :7363 post-win entry) pass the tile they already
   test elsewhere, so no signature change: `mover_id` is still live for the self-skip and for
   that fallback, `mover_nation` for the own-nation skip. The :1913-1929 comment was rewritten
   to state the rule as the scanned tile's water test with the verified line numbers, and to
   point at the sibling's derivation for the full asm quote instead of duplicating it.

2. units.c:5263-5265 and :715 — two open-coded Treasure-value readers bypass `units_treasure_value_gold` (:747), the helper written precisely because a COL1-imported Treasure carries its value in the profession byte (+0x315b = gold/100), not in the LE16 `hold_goods_amount[0..1]` mirror. Both sites read the mirror only, so a save-loaded Treasure Train values at 0: at :5263 (combat loot) the ransom popup and the gold credit are silently skipped and the unit is then destroyed by `units_apply_land_loss_outcome`; at :715 (`units_cortes_cash_coastal_treasures`) the unit is `units_despawn`ed unconditionally at :722 even when `value == 0`, i.e. a DOS-authored save's Treasure is deleted for nothing. The four other consumers (:821, :912, :995, ai_euro.c:5013, game_loop.c:9933) all delegate to the helper. Secondary: the citation at :5258 is wrong — FUN_5fef_1908 (viceroy_unpacked.c:100158) is the Europe/King-galleon cash-in (`local_5c = *(byte*)(param_1*0x1c+0x315b) * 100`, the tax/Cortes share the port already ports at :791), not a combat-loot path; combat treasure capture is FUN_5fef_0352's `bVar12` arm (loser type 0xc, raw 99345). H

   **RESOLVED 2026-09-10 — both readers routed through the helper; citation replaced.**
   Both open-coded readers confirmed present and both fixed. Combat loot (now
   `const int loot_gold = units_treasure_value_gold(def);`) simply delegates. The Cortes
   coastal sweep also delegates, and the unconditional despawn at the old :722 became a
   `continue` when the value is 0. Checked the despawn against DOS first: DOS has no zero
   case at all — FUN_5fef_1908 reads `*(byte *)(param_1 * 0x1c + 0x315b) * 100` with no
   guard (raw 100158) and every DOS Treasure has that byte set — so a 0 here can only be
   the helper's own documented blind spot (a save Treasure worth exactly 2800 has
   `profession == UNITS_JOB_NONE` and reads back as unset). Deleting a unit on the strength
   of that is strictly worse than leaving it standing, and the sibling sweep
   `units_king_galleon_offer_coastal_treasures` already `continue`s on `value <= 0` rather
   than despawning, so the two now agree. The value-bearing path is unchanged: with the
   helper the value is real, `europe_cash_treasure` runs and the unit is despawned exactly
   as before.
   Secondary confirmed and corrected: FUN_5fef_1908 (raw 100158) is indeed the
   Europe/King-galleon cash-in, not combat. The combat site is FUN_5fef_0352 (raw 99292),
   in the capture arm guarded by `bVar12 && local_32 < 4 && bVar11` at raw 99392 — the
   audit's "raw 99345" is the `bVar12` *assignment* band, not the arm. The value read there
   is `iVar17 = *(char *)(iVar18 + 0x315b) * 100` at raw 99404, with `iVar18 = param_1 *
   0x1c` and `param_1` the LOSER (the same record `FUN_281f_0894(0x281f,param_1,local_32)`
   at :99395 hands to the winner's nation), printed by message 0x1b1f in the loser-type-0xc
   arm at raw 99407-99408. That is what the new comment cites.
   Noted while reading, not filed as part of this finding: DOS's arm *transfers ownership*
   of the Treasure and only displays the value; the gold is credited later, when the
   captured Treasure is delivered. The port instead credits (or ransoms) the gold at combat
   time and lets `units_apply_land_loss_outcome` destroy the unit. That is a separate,
   larger divergence from the same DOS band and is out of scope here — the fix above only
   makes the value the port reads the right one.

3. units.c:7351 vs :7327 / :7374 — the village-raid WIN branch charges only `units_move_cost`, while every other outcome of the same attack charges `cost + 3`: the loss branch at :7327 (`units_mp_charge(pool, atk_mp, cost + 3)`) and the ordinary land-win stay-put branch at :7374 (`drain = cost + 3`), both citing the 1b0e `*(char*)(unit+0x3149) += 3` that this file documents as "win or lose" at :7306-7313. The comment at :7318-7321 justifies the omission by saying the raid branch "already has its own complete, separately-cited MP model (FUN_4d56_4528)" — but the branch is a bare `units_move_cost` + charge with no citation of its own. So winning a village raid is 3 thirds cheaper than losing the same raid. M

   **RESOLVED 2026-09-10 — raid win now pays the same `cost + 3` as every other outcome.** Confirmed against the raw before touching anything: the surcharge is charged at *FUN_5fef_1b0e entry*, `if (param_5 != 0) { *(char *)(iVar23 + 0x3149) = *(char *)(iVar23 + 0x3149) + '\x03'; }` (viceroy_unpacked.c:100341-100343), where `param_5` is the attack flag `FUN_465b_0000` passes as the last argument of `FUN_291f_0a14(0x281f,param_1,param_2,param_3,local_1c,1)` at :75692 — and `FUN_291f_0a14` is a bare far-call thunk to 1b0e (:36081-36086). Because that write precedes the roll and precedes any branch, DOS has no notion of a per-outcome MP price, and the village raid reaches combat through exactly that call (dest-has-foreign-unit sets `bVar4`, :75484-75488), so the exemption was unfounded: units.c:7411 now charges `cost + 3` and the misleading "already has its own complete, separately-cited MP model (FUN_4d56_4528)" justification at :7363 is replaced by the citation above plus an enumeration of the four sites that share the charge. A regression assertion was added to the existing village-temp block (test_units.c:8052-8060 / :8097-8112) pinning `moves_left == before − (cost + 3)` after a won raid, since nothing had covered raid MP. **Residue, out of scope:** the same DOS band exhausts the attacker outright — `if (param_5 != 0) { FUN_281f_0934(...); }` at :100376, and `0934` → `1427_155e` (:8880-8887) writes `spent = max allotment` — and 465b's step cost is skipped on attacks (`spent += local_40` sits inside `if (!bVar4)`, :75639-75640), so both the port's "ship-slow survives the surcharge" model and its "step cost applied unconditionally before combat" comment are questionable; fixing that means rewriting the shared model and the `ship-slow` test, and belongs in its own pass.

4. units.c:7133-7134 — the `pre_park` / `pre_spent` discriminator added for bugs.md 429 is dead: `units_try_move` already returns `COLONIZE_ENTER_NO_MP` at :7065 when `units_remaining_mp(pool, unit_id) <= 0`, so by :7134 remaining is provably > 0 and both booleans are always false. Only the `units_move_crosses_shore` disjunct at :7156 can ever set `mp_spent_turn`, i.e. the "boarded already exhausted" half of the rule (and the `units_wake` refund guard at :7963 that depends on it) never fires. Either the MP check at :7065 should let a spent unit board, or the dead half should go — as written the comment describes behaviour the code cannot reach. M

   **RESOLVED 2026-09-10 — confirmed dead; both booleans removed.** NO_MP gate at :7104 precedes the site and nothing between touches the mover's MP (village temp-defender spawn exhausts only the phantom). Mark is now `if (units_move_crosses_shore(...))`; comment states shore-cross is the only reachable spend and names the half to restore if the MP gate is ever relaxed. Stale "boarded already exhausted" parenthetical in `units_wake` corrected. board-enter test still passes.

5. units.c:2935 — `win_can_capture = win_euro && wt->attack > 0` applies the DOS type-table read (`*(char*)(local_4*0xe+0x5236) == 0 → bVar12 = false`, viceroy_unpacked.c:99378, local_4 = WINNER type) to the port's body model, which this same file's new doc block at :6029-6052 says is the wrong question for a port unit: a Colonists-type body carrying muskets is DOS's type 1 "Soldiers" (attack 2, 0x5236 set) and DOES capture there, but reads attack 0 here, so it silently falls through to demote/despawn instead of flipping the loser. `units_sweep_stack_after_loss:3118-3126` and `units_seize_noncombat_at:6455-6465` both reason the opposite way about the same body/type split within 200 lines. (Narrower than it looks — game_loop.c:9659 retypes colony-armed colonists to "Soldiers", so the body+kit form only arises from save import, seizure and demote — but those are exactly the units the neighbouring comments call out.) The `win_euro` half is DOS-correct: raw 99392 gates on `local_32 < 4`. M-L

   **RESOLVED 2026-09-10 — winner gate is now `units_is_combat_role`.** DOS gates capture on the WINNER's DS:0x5236 attack byte (raw 99378-80, `local_4` = param_2 = winner) — "is the winner a combatant" — and DOS's armed colonist IS type 1 Soldiers, so the port's type-table read answered NO for a unit DOS calls a soldier, dropping beaten Colonists/Wagons to demote/despawn instead of a nation flip. Spelled with `units_is_combat_role` (the body-model half of the audit-#12 split); a port Scout (Colonists body + horses) also gains capture, DOS-correct (@UNIT Scouts attack 1). The two-predicates doc block at units.c:6089 no longer names the type-table read as the 0352 winner gate.

6. units.c:5354 — `atk_noncombat_body = (at->attack == 0 && atk->muskets <= 0 && atk->horses <= 0)` is a fourth open-coded spelling of the combat-role predicate, left behind by the prior-audit-#12 consolidation that folded the copy in `units_best_defender_at` (now `units_is_combat_role(pool, u)` at :2064) and wrote a doc block at :6029 naming exactly *two* predicates. It is `!units_is_combat_role(pool, atk)` verbatim, so no behaviour difference today — pure re-divergence trap in the one place a wrong answer captures a whole defended stack (the bug the surrounding comment describes). L-M

   **RESOLVED 2026-09-10 — verified identical, folded to `!units_is_combat_role(pool, atk)`.** `at` is null-guarded at :5168 and nothing mutates `atk`/`type_index` before the use; comment now cites the :6092 doc block.

7. unit_chrome.c:275-293 — `unit_chrome_corner_for_type` compares `display_type_index` against hardcoded DOS @UNIT ids (0x0d..0x12, 4/5/7/8/0x15/0x16, 10/11/12), but its argument comes from `units_display_type_index` (units.c:11199-11225), which returns a *pool* index from `units_find_type`. That is the exact id-space assumption the same fix wave removed from `units_combat_brave_vs_human_arty` (units.c:5088-5116, "a Linux POOL INDEX is not a DOS @UNIT id"). Harmless on the stock roster, wrong on synthetic fixtures/modded rosters — and the two files now state opposite conventions. L

   **RESOLVED 2026-09-10 — premise confirmed, documented as invariant.** The argument is a DOS @UNIT id (FUN_112b_01ba raw 2102-2118) and callers pass a pool index; identity holds because `units_load_types` appends NAMES.TXT @UNIT rows in file order and that section is exactly Colonists 0 … Mtd. Warriors 0x16 (verified, no comment/blank rows). Left numeric, param renamed `dos_unit_type_id`, and documented as resting on the NAMES.TXT ordering invariant with a "do not copy into a rules path" warning. Cosmetic-only blast radius; a real conversion needs all six caller files. New lead: DOS's fourth arm is Artillery+damaged (raw 2109-2111), not aboard — see Leads.

8. combat_analysis.c:121-127 / :117-120 vs :459-465 — the `land_attack_bonus` parameter name and the function's header comment ("land_attack_bonus: land engage ×3/2") contradict the caller's own comment at :459-464, which says every attacker land or naval carries the ×3/2 and DOS 636c prints the row off DS:0x8d00 bit 0 for both domains (the suppression was removed). Stale name + stale doc over live code. L

   **RESOLVED 2026-09-10 — stale name/doc over correct code; renamed `attack_bonus`.** DOS: FUN_157e_004a raw 8926-8928 ORs bit 0 of DS:0x8d00 for `param_2 != 0` with no domain test; 636c raw 101874-91 walks the bit with label 0x2e54 value 0x32; the ×3/2 itself is unconditional at raw 100458, ahead of the is-ship flags. Both callers pass the plain `COMBAT_FLAG_MODE_ATK` bit. Value unchanged, comment/name only. CRLF preserved.

9. units.c — residual raw `moves_left` writes/reads that the `units_mp_charge/exhaust/restore` centralisation (prior-audit #6) did not reach: writes at :7499 (transport docking), :7795 (trade route), :7873/:7895 (pillage), :9742 ("FUN_281f_0934 stand-in: exhaust MP"), :10240/:10269 (board), and a raw read at :7843 (`u->moves_left <= 0` in `units_pillage`). All are Euro-only today (pillage is reached only from game_loop.c:11396, wagons/ships/pioneers are never native), so this is latent, not live — but they are the same inversion class and the helpers exist two screens away. L

   **RESOLVED 2026-09-10 — five sites routed through the central helpers; board writes deliberately raw.** `units_mp_exhaust` at :7559 (docking), :7855 (trade route, now matching the `units_set_orders` park rule), :7937/:7959 (pillage), :9807 (pioneer stand-in); pillage read is `units_remaining_mp(...) <= 0`. Board writes (:10305/:10345) left raw with a comment: that 0 is the hold's park sentinel, and every aboard reader (`units_unload_passenger` :10555, `units_first_cargo_with_moves`, `units_first_landfall_cargo`) reads it back literally in Euro space — converting the writer alone would hand a native passenger `max_mp` and make the latent case worse. All conversions value-identical for Euro units.

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

   **RESOLVED 2026-09-10 — mask now reads the Phase-A snapshot.** Confirmed the split is real
   and the fix is the one-liner: :865 and :1679 are both inside `turn_produce_one_colony`
   (opens :631, the only function in that span — no `^}` between :629 and :1800), which is
   already the per-colony body, so `sol_b_phase_a` at function-body scope IS this colony's
   snapshot at the :1679 site — no per-colony snapshot array needed. Call changed to
   `colony_craft_demand_mask(pool, colony, sol_b_phase_a, craft_demand)`. The now-false
   "sol_bonus-consistent" claim in the comment above was left in place as the dated 2026-08-24
   record and a new dated paragraph appended under it naming the Phase-A boundary (:865), the
   three mutation phases that fall between (C/D SoL accumulator + latch bits, F/G/H education,
   I/J birth/starve-kill) and the "one number, two consumers" invariant
   (colony_production.c:419-433). Lumber is untouched: its gate comes from
   `colony_prod_colony_hammers`'s `out_lumber_use`, which that function documents as
   sol_bonus-independent (the `0` it is passed at :1681 is deliberate, not a second instance
   of this bug).

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

   **RESOLVED 2026-09-10 — `in_hall_of_fame` and `in_exploits` added to `map_visible`.**
   Confirmed on all four counts. Both flags live outside `game_modal_open` (game_loop.c
   :16476, whose 14 flags are all dialog/modal state), so the popup-blocking invariant's
   "new modals must join game_modal_open" does not apply here — these are screens, and
   every other screen test in the file lists them by hand: `game_turn_flow_allowed`
   (:10433-10435), `game_service_woodcut`'s `screen_over_map` (:11857-11859), the EOT
   `screen_over_map` (:12100-12102) and the popup-presentation gate (:12164-12166). The
   file's own note at :12053 states the rule outright ("It is a screen, not a modal, so it
   joins the screen tests (`screen_over_map`, `map_visible`, `game_screen_name`) rather
   than `game_modal_open`"), so the fix belongs exactly where the finding says. Verified
   both setters: :7679 (title menu → Hall of Fame, `in_menu = false`) and
   `game_retire_after_score` (:5193 exploits / :5196 Hall of Fame), both with a campaign
   still loaded, so `in_menu` covered neither. The gated block is `if (game->units_ok &&
   game->world_map_ok && map_visible)` and runs to :12595 — it does contain the goto pacer,
   the village-entry dispatch and the Europe sail hand-off the finding names, with the
   unit-activation cycle at its tail. No further screen flag was missing: `map_visible` now
   carries the same set as the other three tests (report/menu/Europe/colony/pedia/debug
   atlas/HoF/exploits/new-game wizard) with `game_modal_open` on top as before. The header
   comment's overlay list was updated to match and a paragraph added naming the
   screen-not-modal reason, so the next edit does not drop them again.

3. **src/core/game_loop.c:12430, :12470, :12515 (and :2984) — four hand-off sites still call
   the bare `turn_select_next_unit`, contradicting the comment at :8443 that says the
   per-frame activation cycle already uses the skip-aware form.** `game_after_unit_action`
   (:8445), `game_wait_next_unit` (:10567), MOVE_PIECES (:11238) and the cycle's own idle arm
   (:12569) all use `game_select_next_unit_awaiting_orders`; the three pacer arms (trade-route
   Europe departure, goto-ship-sailed, stalled-unit park) and the @SAILHOME "yes" arm at :2984
   do not, so any of them can park the live selection on a Fortified/Sentried unit for a frame
   — the exact flash the shared helper's header comment (:10511-10519) says must never happen.
   **M**

   **RESOLVED 2026-09-10 — all four hand-offs now skip-aware.** @SAILHOME "yes", trade-route Europe departure, goto-ship-sailed, and stalled-unit park all call `game_select_next_unit_awaiting_orders` (forward declaration added at :1142). Zero bare `turn_select_next_unit` calls remain outside turn.c's definition and the skip loop's inner step.

4. **src/core/turn.c:250 `turn_select_next_unit_awaiting_orders` is a byte-for-byte duplicate
   of `game_select_next_unit_awaiting_orders` (game_loop.c:10521), and turn.h:261 says so in
   writing** ("game_loop.c keeps a ColonizeGameState-shaped twin of this … that should fold
   into this one"). Only one caller uses the shared copy (turn.c:3986, the KING slice); all
   five game_loop call sites use the twin. Two bodies that must stay identical for the control
   cycle to behave the same inside and outside the turn processor — a known divergence trap,
   currently still in sync. **M-L (duplication, no behavioural split yet)**

   **RESOLVED 2026-09-10 — folded; turn.c:250 is the single source.** game_loop's copy is a 3-line adapter adding only the `units_ok` guard its callers need; all game_loop call sites untouched; turn.h:261 comment rewritten to state the fold is done. (turn.h is LF, not CRLF — the CRLF memory list is stale on this file.)

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

   **RESOLVED 2026-09-10 — DOS confirms the outside dialog does offer Missionary; the game_loop copy was simply short a row.** `@ARMOPTIONS` is a red herring: GAME.TXT:1753 is the twelve-row *Europe dock* menu, and the colony-screen unit click is `@COLONYUNIT`+`@UNITOPTIONS` (GAME.TXT:1767/1771) — the Leave-as role list has no tag of its own. DOS builds **both** lists in one function, `FUN_2f2b_348c` (`viceroy_unpacked.c:50466`): `param_1 != 0` selects the six-row leave-as mode (`local_fa = 0x13, local_146 = 6`, i.e. professions 0x13..0x18 = Colonist / Pioneer / Soldier / Scout / Dragoon / **Missionary** — `FUNCTION_CATALOG.md:395` records the same 0x13..0x18 switch for `FUN_15eb_0d8e`), and a people-band index at or past the colony's colonist count (`*(char*)(*(int*)0x8542+0x1f)`) *forces* that mode — that index is exactly an outside unit, and the outside-unit accessors (`FUN_281f_0bc8(-(count−index))`, the tools byte at `iVar5*0x1c+0x3159`, the Continental type bytes 9/7 at `+0x3146`) are read in that same branch. Rows are gated one at a time through `FUN_281f_0bb4` → `FUN_15eb_3454` (`viceroy_unpacked.c:13518`), whose `>= 0x13` arm is profession-indexed and location-blind: the gear rows test colony stock against `FUN_15eb_0d8e`'s cargo list (tools `0x14` = 20, muskets/horses `0x32` = 50, matching `UNITS_EQUIP_*`) and row 0x18 asks only `FUN_15eb_038e(0x25)` — the Church bit (`0x25` Church / `0x26` Cathedral, the indices confirmed alongside Stable/Printing Press in `building_production.md:350`). Church-alone suffices in DOS because `FUN_364b_0114` only ever *sets* a finished building's bit (`FUN_281f_0bbe(b, 1)`, never a clear) and Church is Cathedral's prerequisite, so the port's Church-or-Cathedral disjunction is the same test. Fixed by making the two copies row-for-row identical rather than folding them (the single-source form needs a `colony.h` prototype, left open): `game_loop.c:9611` adds `game_colony_can_bless`, `:9656` `game_colony_list_outside_roles` takes the colony pool and appends the church-gated `COLONIZE_EJECT_MISSIONARY` row, and `:9714` the applier gained a real `case COLONIZE_EJECT_MISSIONARY:` that re-tests the gate, names the type `"Missionaries"` outright (`units_equip_role_type_name` has no Missionary arm — DOS re-types gear changes through the `@JOB`→`@UNIT` table, which the bless does not use) and leaves `profession` alone, which is what makes DOS's "Cancel Missionary Status" gate (`profession != 0x18`) work. The applier's `default:` no longer shares `break` with `COLONIZE_EJECT_COLONIST`: an id that is not one of the six rows now returns false instead of re-typing for free (`:9749`), and the moves/orders exhaust test now also fires on a type change (`:9788`) so a bless — the one row that moves no crates — still ends the unit's turn, as the inside twin's ejected body always has. Both copies now carry a comment naming the other and the single DOS function. Two residues, both pre-existing in *both* copies and both left open: DOS **greys** a short-stock row (`FUN_15eb_3454` returns `0xffff`, rendered disabled at `2f2b:3d32` via `FUN_291f_01b6`) where the port omits it, and DOS refuses every row but Colonist to an Indian Convert (`if ((0x13 < param_1) && (cur_prof == 0x1b)) return 0;`) — a rule `europe.c` already ports for the dock menu.

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

   **RESOLVED 2026-09-10 — turn.c now counts hulls the way game_loop does; the game_loop comment checks out.** `FUN_48d3_0002` gates on `DS:0x9418[nation]` (`viceroy_unpacked.asm 48d3:003b cmp byte [bx+0x9418],0x3`), the tally `FUN_4962_0018` builds by walking the whole unit array and bumping `0x9418` for every unit of that nation with type 0x0d..0x12 (`4962:0300-0365`); since DOS parks a ship crossing to Europe as a live unit on its nation's sentinel diagonal (228/232/244+n), harbour, expected and bound hulls are all inside it. The port hoists only the **human's** Europe-side ships out of the unit pool into `EuropeScreen`, so both live-pool spellings under-count him by exactly those three arrays — `stuff.ship_counts[]` included, since `col1_stuff_census.c:112` walks the live pool. `turn.c:2803` adds `turn_voyage_ship_count(ctx, nation)` = `units_count_sea_for_nation` plus `harbor_ships + expected_ships + bound_ships` when the Europe screen is bound to that nation, and `:2932` feeds it to `europe_voyage_turns_roll` in place of the census read (the `: 1` no-col1 fallback goes with it). Called with the damaged hull still active, matching `game_ship_sail_to_europe`, which deliberately takes its count before the despawn (`game_loop.c:10074`). **The AI half of the question resolves the other way**: an AI nation's crossing ships stay on the diagonal as live units in this port too, so a live-pool count *is* the DOS tally for them — and this site is `nation == ctx->human_nation`-gated in any case. The two functions stay twins by comment rather than by call: a shared helper would need a prototype in `turn.h`, which is CRLF and off-limits this pass. New lead: `ai_king.c:3654` (the KINGFRIGATE gift spawn) is a third spelling — bare `units_count_sea_for_nation` with no Europe adds — and it fires for the human nation, so it under-counts exactly as turn.c did.

7. **src/core/game_loop.c:11297 `MAP_MENU_ACTION_ANCHOR` carries the FORTIFY sibling's status
   strings verbatim** — failure says "Cannot fortify" and success says "Fortifying", for the
   ship row DOS labels *Anchor* (the two cases are otherwise identical copies of :11290-11305).
   Cosmetic, but it is the one place the port tells the player a ship is fortifying. **L**

   **RESOLVED 2026-09-10 — "Cannot anchor" / "Anchoring in harbor".** DOS arms no status line for either Fortify row (FUN_2b5a_1112 raw 42364-42421 never appends to DS:0x2d54), so both old strings were Linux chrome; the wording comes from the only place DOS words the ship variant, GAME.TXT:1782 @SHIPOPTIONS `Anchor in harbor ("Fortify")`. Order set is still Fortify/Fortified; only status text changed. New lead: the plain F-key path (game_loop.c:14855) still prints "Fortifying" for ships.

8. **src/core/game_loop.c:12765 vs :12804 — the colony screen's Escape and Enter sub-panel
   cascades close the same five panels in different orders** (Esc: message, jobs, eject,
   dock_orders, custom_house, construction; Enter: message, eject, dock_orders, custom_house,
   jobs, construction), and the `C` hotkey at :12938 closes only `jobs_open` before opening
   Construction, so pressing C with the eject / dock-orders / Custom House panel up stacks
   Construction underneath a still-open panel. Only reachable when two panels are open at once,
   which the hit-test mostly prevents — filing as the tail of the #20 family, not as a repeat
   of it. **L**

   **RESOLVED 2026-09-10 — the concrete C-hotkey bug is REFUTED, but the cascades had really drifted; unified.** Every `colony_screen_open_*` already hand-rolled its own close cascade, so Construction could not stack — but `open_jobs` alone did not close a message popup and half-mutated (`jobs_tile_index` written, `jobs_open` left false) on the centre-tile early-out. New `colony_screen_close_subpanels()` (colony_screen.h:373, colony_screen.c:257) closes all six in one canonical order; all seven openers and the `C` hotkey (game_loop.c:13053) call it; Enter's cascade reordered to Escape's order; `open_jobs` resolves the tile delta before touching panel state. Esc-dismisses-one-per-press and Enter-acts-on-topmost semantics preserved; net −20 lines.

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

   **RESOLVED 2026-09-10 — confirmed dead twice over, deleted.** Statically: the tail's gate set is a strict subset of the :20619 block, which already runs the identical 8-step `best_adjacent_foe`/`try_attack` loop with the same no-progress break. Empirically: a stderr probe on the tail's `ai_euro_try_attack` got zero hits across the full ctest suite including all goldens. Replacement comment records the fold, the §2c / viceroy 75567-75593 citation, and the zero-hit result.

2. **ai_euro.c:18771-18774 — the Pioneer→Soldier/Dragoon equip gate is cited to raw 94289-94311 but ports only 2 of the 4 conjuncts of DOS's "big settled town" disjunct.** DOS (viceroy_unpacked.c:94291-94293) is `local_90 = (local_2a == 0) && (pop > 10) && (FUN_281f_04d4() == 0) && ((+0x1b & 0x10) == 0)` — `local_2a` is the colony continent's G-stance (`-0x6790[nation*0x10+cont]`, the same quantity `ai_euro_continent_stance_at` already provides in this file) and `FUN_281f_04d4()` is a second unported probe. The port has `equip_pop > 10 && (c->ai_flags & NEEDS_COLONISTS) == 0` only. Net effect: on any continent where the AI *has* a stance assigned (i.e. exactly the war/expansion continents), DOS suppresses this arm and the port does not — every pop>11 colony with 50 muskets keeps re-typing arriving Pioneers. The `(ai_flags & 0x48)` half at :18772-18773 is correct. **M-H**. Confirm: the raw lines are unambiguous; the only open question is what `FUN_281f_04d4` resolves to.

   **RESOLVED 2026-09-10 — all four conjuncts of `local_90` now ported.** The dropped far-call arguments came out of the annotated dump (`original_sources_annotated/ai/colony_tick_5952_035e.md:641-648`), which spells raw 94292-94293 as `iStack_90 = (uStack_2a == 0) && ('\n' < +0x1f) && (FUN_1000_86c4(0x181f,0,3) == 0) && ((+0x1b & 0x10) == 0)` — so `FUN_281f_04d4` here is `dos_rng_range(0,3)`, a plain 1-in-4 roll, the same identification this file already carries at :14367 (`FUN_1000_86c4` = `FUN_281f_04d4` = `dos_rng_range`, address_mapping.csv:844), and `local_2a` is the continent G-stance that `ai_euro_continent_stance_at` mirrors. `ai_euro.c:18798-18841` now computes `equip_local_90` from all four conjuncts in DOS's order and feeds it to `equip_demand` as DOS's `((+0x1b & 0x48) != 0 || local_90 != 0)` (raw 94304-94306). Two placement details protect the shared LCG: the draw is the third conjunct, so `&&` keeps it unreached unless stance is 0 and population exceeds 10, and it sits inside a hoisted `on_tile` test so that walking this act's per-colony loop cannot draw once per colony of the nation. Golden `TURN6→7` Quebec is unaffected — pop 1 short-circuits before the roll, and its Soldier comes from the 0x48 half as the block's comment already said. The population gate deliberately keeps DOS's literal `> 10` rather than the absorption `+1` compensation its `population > 1` sibling needed: widening it would offset this narrowing.

3. **ai_euro.c:1573-1581 + :1598-1606 — DOS has TWO writers of +0x1b bit 0x10 (NEEDS_COLONISTS); only the second is ported, and the new unconditional `else`-clear now actively suppresses the first.** `colony_tick_5952_035e.md:487-490` (= viceroy_unpacked.c:94143-94146) is `if ((+0x1c & 0x10) && pop < 0x20) { +0x1b |= 0x10; +0x1c &= 0xef; }` — a one-shot hand-off from the +0x1c bit 0x10 latch, run *before* the formula writer at md:557-563 that :1576 ports. Two consequences: (a) the comment at :1574-1575 ("DOS clears the bit up front … and re-ORs it, which is this set/else-clear pair") is only true if both OR sites exist, so the `else` branch at :1578-1581 is stronger than DOS; (b) `COLONIZE_COLONY_FLAG_SMALL_AI` (+0x1c bit 0x10) is stamped every tick at :1600-1605 from an uncited `pop < 10` and is **never read and never cleared** anywhere in the port — DOS's only consumer of that bit is exactly the writer above, which also clears it. So the port has one uncited write-only flag and one missing DOS gate, on the same bit pair. **M-H**.

   **RESOLVED 2026-09-10 — first writer ported, `else`-clear retired, and the `+0x1c` stamp deleted as invented.** Raw 94143-94146 (= colony_tick_5952_035e.md:487-490) is now at `ai_euro.c:10073-10099`, in its exact DOS position: immediately after the flag byte's `+0x1b &= 7` inside `ai_euro_colony_threat_seed_5952`, before every other flag writer of the tick, as `if ((colony_flags & SMALL_AI) && population < 0x20) { ai_flags |= NEEDS_COLONISTS; colony_flags &= ~SMALL_AI; }`. The formula writer at :1574-1589 is now set-only, matching DOS (md:557-563 is an `|= 0x10` with no clear); the bit's per-tick clear is that `&= 7`, which is guaranteed to precede it — `ai_euro_colony_threat_seed_5952` is static with the single call site at :10169, runs immediately before `ai_euro_refresh_colony_ai_flags` on the same colony, and has no early return between its entry guard and the clear. The `+0x1c` half turned out worse than "uncited": grepping all three decompiled images (`viceroy_unpacked.c`, `viceroy_overlays.c`, `viceroy_unpacked_2.c`) over every `(byte *)(x + 0x1c)` access and every `| 0x10` / `& 0xef` store finds **no writer of +0x1c bit 0x10 anywhere in DOS** — raw 94143's read-and-clear is the bit's only reference in the image, so it is a save-borne one-shot, never a per-tick reading of colony size. The `pop < 10` stamp at :1598-1606 is therefore deleted (`ai_euro.c:1605-1617` keeps the reasoning); left in place alongside the new hand-off it would have re-armed the latch every tick and pinned NEEDS_COLONISTS on for every colony under population 10. No fixture risk: no `*.json` under `original_saves/`, `test-saves*/` or `tests/` has `small_colony_ai` set, so the hand-off fires in no current golden, and removing a flag nothing read plus a clear the `&= 7` already performs is behaviour-neutral there.

4. **ai_euro.c:20718-20726 — the despawn latch-hygiene loop resets `s_20e6_wagon_errand`, `s_20e6_hop_slot` and `s_20e6_hop_steps` but not `s_20e6_explore_fatigue`, although its own comment gives the identical argument for all of them.** The comment (:20720-20723) says the +0x3155/+0x3156 bytes "are part of the unit record too, so a reused id must not inherit a foreign hop commitment". `s_20e6_explore_fatigue` (:11488) is documented at :11482-11487 as DOS unit **+0x3154**, the third byte of the same cargo-hold family, and it is read at :11901 (explorer scoring) and mutated at :12396/:13006. A reused unit id inherits the dead unit's fatigue. 3-of-4 sibling coverage, no explanation. **H** (code-visible; no DOS question involved).
   **RESOLVED 2026-09-10 — fourth sibling now reset.** `s_20e6_explore_fatigue[i] = 0;` added to the despawn latch-hygiene loop (ai_euro.c:20725, loop now :20714-20728) and the comment reworded to cover +0x3154 alongside +0x3155/+0x3156. Reset value 0 is the array's own fresh state: it is a `uint8_t` file-scope static (zero-initialised at :11488), only ever `++`'d from 0 with cap 0x7f (:12396-12397) and decremented by 8 under a `> 8` guard (:13006-13007), and the read at :11901 substitutes 0 for out-of-range ids — so 0 is exactly "never explored", not a sentinel. No fifth sibling: the full set of file-scope `[COLONIZE_UNITS_MAX]` statics in ai_euro.c is `s_deferred_found`, `s_unloaded_this_turn`, `s_euro_last_dir`, `s_violate_last_turn`, `s_euro_roam_wander`, `s_4393_claim_{turn,colony,valid}`, `s_20e6_wagon_errand`, `s_0a60_pilot_state`, `s_20e6_explore_fatigue`, `s_20e6_hop_steps`, `s_20e6_hop_slot`, and of those only the four 20e6 latches shadow +0x315x cargo-hold bytes — +0x315a (cower) is `u->turns_worked` and +0x315b (Treasure) is `u->profession`, i.e. real record fields that die with the record. The rest need no addition here: `s_deferred_found`, `s_unloaded_this_turn` and `s_0a60_pilot_state` are already `memset` unconditionally at the top of the same block; `s_4393_claim_*` is turn-stamped (:5544 requires `claim_turn == turn`) and cleared at :10103; `s_violate_last_turn` is a turn stamp whose only stale effect is suppressing one notify; `s_euro_roam_wander` shadows +0x314c, not the cargo-hold family; and `s_euro_last_dir` (+0x314f) is left alone because its declaration at :36-45 already argues the stale-inherit case explicitly and accepts it. No behaviour change in a real single-pool game; this is the same despawn/reuse hygiene as its three siblings.

5. **The 255 empty-hold sentinel guard added by smell #36 reached 2 of ~5 sibling hold scans in this file.** Fixed: `ai_euro_hauler_free_holds` :5406, `ai_euro_0a60_holds_occupied` :8985. Still unguarded, all reading DOS-imported hulls:
   - **:8525** `if (sh->hold_goods_amount[hh] > 0) used++;` — a *third* open-coded copy of `holds_occupied` (cargo_count + occupied goods holds), inside the 5d04 Europe-buy loop, which also scans `COLONIZE_UNIT_CARGO_MAX` instead of `units_goods_hold_count` — i.e. exactly the two defects #36 fixed in the other copy, in the copy #36 did not find. It feeds `cap == used` and `cap - used > 2`, the buy budget.
   - **:8003** `out[u->hold_goods_type[h]]--;` under `hold_goods_amount[h] > 0` — the 5d04 cargo-demand tally, `COLONIZE_UNIT_CARGO_MAX` bound, no sentinel test.
   - **:7708 / :7715 / :7753** (`cb_reward_case` / `cb_reward_value` / `cb_sell_hold0`) gate on `hold_goods_amount[0] <= 0` only, so a sentinel hold 0 sells **255 units** at `euro_price−1` and credits the treasury for them.
   Should either be one shared helper or five identical predicates. **M-H**.

   **RESOLVED 2026-09-10 — all listed sites plus one unlisted sibling guarded.** New sentinel-aware `ai_euro_5d04_hold_amount()` (~:7714) used at `cb_reward_case`/`cb_reward_value`/`cb_sell_hold0` — the worst case: a sentinel hold 0 sold 255 units at `euro_price−1` and credited the treasury. The 5d04 cargo-demand tally now guards the sentinel and bounds on `units_goods_hold_count`; the open-coded holds_occupied copy at :8525 now calls `ai_euro_0a60_holds_occupied`; the 457e "cargo >0xc or ==8 aboard" scan (found by grep, not in the finding) guarded too. Every `hold_goods_amount` read in the file is now sentinel-aware.

6. **ai_euro.c:13960-13997 (`ai_euro_ocean_colony_sail_score`) duplicates the DOS site that :17435 (`ai_euro_20e6_colony_sail_pick`) now ports structurally, with entirely invented arithmetic, and both are live in the same act.** The thin one's header (:13955) cites "LAB_521d_3558 peace colony-sail score (~89614–89711 thin)"; the structural one's fixes this session cite "raw 89663" (:17495) — the same DOS range. The thin scorer's terms (`pop*8 − d*4 + idle*8`, `+16` docks, `+14`, Stockade/Fort/Fortress `+8/+16/+24`) have no DOS basis; the structural one carries the real `rng(0,8) + ((0x11−pop)²+2)*4 − (pop−wanted)*2 … ±0x19 … +0x3c` ladder. Call order: `ai_euro_20e6_colony_sail_pick` at :17882 sets an `AI_MOVE` goto, then `ai_euro_try_ship_war_cargo_sail` (:19226, war only) re-scores with the thin function and can overwrite it. Two answers to one DOS question. **M**. (Sub-item: :13978's comment `/* dock flag 0x1b&0x10 stand-in */` mislabels bit 0x10 — that is NEEDS_COLONISTS, not a dock flag.)

   **RESOLVED 2026-09-10 — thin scorer deleted; premise partly REVERSED.** The call order was the opposite of the finding's claim: the thin path (`ai_euro_try_ship_war_cargo_sail` in `ai_euro_unit_act`) ran FIRST and governed real movement; the structural pick only got the leftover next-act goto. Structural port re-verified term-by-term against raw 89614-89711 and kept; the thin scorer and its war-cargo arm deleted rather than re-routed — DOS enters 3558 exactly once per act from the 20e6 unload/settle flow (re-routing would double-draw the shared LCG in the peace branch), and a goods-only hull is the delivery matrix's business (raw 2047-2139), not 3558's. Military passengers still reach the structural pick through the settle gate. `unit_war_cargo_fortress_prefer` (test_ai_euro_war.c) asserted the invented Stockade/Fort/Fortress ladder and is removed with a comment.

7. **The port recomputes DS:0x95f2 (`continent_presence_flags`) twice, in two functions that disagree, and both cite a docs row that the raw source refutes.** `ai_euro_5952_continent_presence` (:9748-9810, new this session) filters `!units_is_on_map(u) || u->aboard_ship_id >= 0` and masks `u->nation_id & 0xf`; `ai_contact.c:9240-9252` (the war-declare half of the *same DOS body*, called 6 lines apart at ai_euro.c:10173) does neither. A passenger aboard a ship therefore sets bit 2 in one copy and not the other, which flips ai_contact's `(presence & 6) == 0` branch between "declare war on the tribe" and "cap expansion appetite". Separately, `docs/save_format_map.md:254` (row 156) and the new comment at ai_euro.c:9751-9754 both state the array is "OR'd (not cleared between nations, so accumulates across the full per-turn pass)" — viceroy_unpacked.c:78149-78151 zeroes `-0x6a0e[0..15]` at the top of **every** per-nation `FUN_4962_0018` call. ai_contact.c:9186-9188's comment ("zeroed at the top of every per-nation call") is the correct one. Doc + one comment are wrong; the code happens to be right. **M**.

   **RESOLVED 2026-09-10 — single shared computation, both callers wired.** New `ai_contact_continent_presence_4962(ctx, nation_id, cont)` (ai_contact.c:9179-9291, declared ai_contact.h:80-92) carries the full per-bit citation of raw 78149-78312 (bit 1 settlements no nation filter, bit 2 unit-loop else-arm nation<4, bit 4 colony else-arm unmasked; zeroed per-nation) plus why passengers need an explicit filter (DOS parks them at (−2,−2), the port rides them on the carrier tile) and why it must be a slot walk. `ai_contact_colony_tick_war_5952` calls it (~90 inline lines removed); `ai_euro_5952_continent_presence` is now a one-line delegate — which also fixes that copy's real defect, an id-walk (`units_get_const(pool, i)`) where DOS slot-walks. Note the tree had drifted since 9e1a38f: 2de74e0 had already corrected ai_contact's filters and docs row 156, so the aboard/nibble divergence described here was already gone; the duplication and the id-walk were what remained. Refuted-citation comments rewritten.

8. **ai_euro.c:18790-18791 — the new Dragoon arm debits horses with the muskets constant.** `c->stock[COLONIZE_CARGO_HORSES] -= UNITS_EQUIP_MUSKETS; u->horses = UNITS_EQUIP_MUSKETS;` while `UNITS_EQUIP_HORSES` exists two lines apart in units.h (:1370-1371). Both are 50 today so behaviour is unchanged; it is a wrong-constant that will silently break if either is ever retuned. **L**.

   **RESOLVED 2026-09-10 — debits `UNITS_EQUIP_HORSES`.** No behavior change (both 50).

9. **Stale comments left by this session's own fixes.**
   - :10149-10150 — the section-D header still lists "Col1 labor_shortage (+0x8e)" as a LABOR trigger; :10176-10187 removed exactly that disjunct and explains at length why it must not come back.
   - `colony.h:260-261` — "its one read site (ai_euro.c ~12103, DOS 88756) really does test bit 4" is now false in two ways: bit 0x04 has two readers (`ai_euro_20e6_wander_step` :12834 and the new `ai_euro_20e6_surplus_recall_arm` :12164-12196), and the cited line number moved.
   - :1247-1288 / :1290-1300 — `ai_euro_colony_ring_tier` and `ai_euro_colony_wanted_size` are 40 lines of citation around `return 2;` / a switch whose live answer is always 8. Correct and well argued, but they keep two unused parameters and a dead 0xc/0x20 table; the labor arm's clamp at :12297 (`wanted > 0x10`) and the colony-sail clamp at :17462 (`wanted > 0xc → 0x10`) are now both unreachable. Worth a note so a future reader does not "fix" the clamps.
   **L**.

   **RESOLVED 2026-09-10 — all three comment groups corrected.** (a) ai_euro.c section-D header no longer lists labor_shortage (+0x8e) as a LABOR trigger and says it must not return; (b) colony.h bit-0x04 note now names both readers (`ai_euro_20e6_surplus_recall_arm`, `ai_euro_20e6_wander_step`) and says "grep the macro, don't trust a line number"; (c) the ring_tier/wanted_size block now records that both clamps (`wanted > 0x10`, `wanted > 0xc → 0x10`) are unreachable by construction, DOS-literal, and must be neither repaired nor deleted.

---

Not filed, checked and judged fine: the 28c8 `plenty`/`section_done` port (raw 94604-94617 read line by line — matches, and the unported loop-entry guard `local_7e`/`local_80`/`local_1e` is explicitly documented at :4133-4139); `ai_euro_5d04_dos_type_of`'s collapse of all non-Privateer hulls to 0xd (every consumer only range-tests `0xc < t < 0x13`); `units_type_has_profession_slot` being fed a Linux `type_index` at :9390 and a DOS code at :9955 (the DS:0x30e table is DOS-indexed, so :9955 is the correct one, but a NAMES-loaded pool makes the two agree — latent, not live).

---

# Section D — Native / King AI (ai_contact.c, ai_diplo.c, ai_king.c, ai_popup.c)

Sweep 3, 2026-09-10, HEAD 9e1a38f. Read-only. Deduped against
docs/smell_audit_2026-09-09.md section D (#44-#59) by substance.

1. src/core/ai_king.c:571-580 — `ai_king_seed_backup_force_1a26` applies FUN_43f7_1a26's halving constants to the already-incremented pool instead of to DOS's pre-store accumulator, so all four foreign-intervention pools come out too big; evidence (verified directly in original_sources_decompiled/viceroy_unpacked.c:74769-74783): DOS is `iVar4 = pop/10 − diff; *0x53e2 = iVar4+8; … *0x53e2 = (iVar4+9)/2` — the halved value is `(pre+9)/2`, not `(stored+9)/2`. The port sets `pool0 = base+8` and then `pool0 = (pool0+9)/2 = (base+17)/2`, i.e. **+4 Regulars**; identically `pool1 = (iVar9+3)/2` vs DOS `(iVar9+2)/2` (+0/+1 Cont. Cav.), `pool3 = (iVar8+7)/2` vs `(iVar8+4)/2` (+1/+2 Artillery), `pool2 = (iVar7+local_4+7)/2` vs `(iVar7+local_4+4)/2` (+1/+2 Man-O-War). The inflated `pool2` then widens the `pool2*2` and `pool2*6` clamps below and the `backup_force[2]` gate that drives `ai_king_merc_offer`. The header's "confirmed byte-for-byte against viceroy_unpacked.c:74765-74795" claim is false. Confidence H.
   **RESOLVED 2026-09-10 — halvings now run off the pre-store accumulators.** Re-read the raw block line by line: 74769 `iVar4 = pop/10 - diff`, 74770 `*0x53e2 = iVar4 + 8`, 74771 `iVar7 = (4-diff)>>1`, 74772 `iVar9 = ((field_combat+1)>>4) + iVar7`, 74773 `*0x53e4 = iVar9 + 1`, 74774 `iVar8 = iVar7 + ((land_strength+1)>>5)`, 74775 `*0x53e8 = iVar8 + 3`, 74776 `*0x53e6 = iVar7 + local_4 + 3`, then 74777-74784 halve `iVar4/iVar9/iVar8/(iVar7+local_4)` with +9/+2/+4/+4 and overwrite all four addresses. The four `+8/+1/+3/+3` stores are dead: nothing between 74770 and 74784 reads 0x53e2…0x53e8, so the increment never reaches the halving. ai_king.c:580-588 now keeps `iVar4`/`iVar7`/`iVar9`/`iVar8` as named accumulators and computes `pool0 = (iVar4+9)/2`, `pool1 = (iVar9+2)/2`, `pool3 = (iVar8+4)/2`, `pool2 = (iVar7+local_4+4)/2` directly, with a comment citing 74769-74784. The clamp cascade below (`pool2*2` caps on pool3/pool1, then `pool2*6 - pool3 - pool1` on pool0, raw 74785-74799) was re-verified against the raw and left as-is — it already matched, it was only being fed inflated inputs. The false "confirmed byte-for-byte" wording in the function header is replaced with the accurate claim (formula-to-address mapping read off 74765-74799, unit types off 74424).

2. src/core/ai_diplo.c:3300-3304 — `ai_diplo_military_score` was rewired (prior-audit #51) to return `col1->stuff.land_combat_strength[nation_id]`, but nothing in the port ever refreshes that mirror, so in a Linux-started game it is all-zero and both bands it gates are dead; evidence: the only writer is `col1_stuff_census_fill_blank` (src/core/col1_stuff_census.c:158), whose sole caller is src/core/col1_bridge.c:2861-2862 — inside `col1_bridge_capture` (the save-*writing* path, reached only from `game_save_col1_slot`, game_loop.c:4898/4957) and additionally guarded by `if (col1_stuff_census_window_is_blank(...))`, so the array is zero until the first (auto)save and frozen at that turn's values forever after. Contrast the Indian half of the same DOS census pair, which the port *does* run live every Indian nation-turn (`ai_contact_indian_census_4962_06b6`, ai_contact.c:4718, called at :4843 from `ai_contact_indian_relation_tick` ← ai.c:4972), and ai_contact.c:9203-9209, which states outright "the port never refreshes the DS:0x95b2 / 0x91cc mirrors for a Linux-started game" and therefore recomputes live via `ai_contact_land_combat_sum`. Downstream at score 0: ai_diplo.c:3555 (`self > AI_DIPLO_STRENGTH_MIN` war-fatigue peace roll) and :3574 (`self > other*2+20 && self > 30` declare pressure) can never fire — the exact two arms the constants note at ai_diplo.c:105-115 says they gate, and whose RNG draws that note claims the goldens depend on (the goldens load DOS saves, which carry DOS census values, so they hide this). The same frozen snapshot drives 153e worthiness (ai_diplo.c:3167/3182/3185/3196/3211), the mercenary "smite" price (ai_diplo.c:2680-2682, which then floors at `base = 10` → a flat 500g every time), the king intervention math (ai_king.c:3904-3905) and finding 1's pool seeding. Confirm by printing `stuff.land_combat_strength[]` on turn 5 of a new game. Confidence H.
   **RESOLVED 2026-09-10 — score recomputed live.** `ai_diplo_military_score` (ai_diplo.c:3345) now returns a new static `ai_diplo_land_combat_strength_live` instead of the mirror. Scaling: the helper reproduces `col1_stuff_census_tally_units`'s `land_combat_strength` writer (col1_stuff_census.c:145-160) term for term — every ACTIVE unit of the nation whose *type* domain is LAND (passengers in a hold included, as the census twin's own comment requires, DOS scanning the whole unit array), Σ `combat_unit_base_x8(u->id, 1)` = Σ FUN_281f_09c8(u,1), accumulated into a 16-bit word that **wraps** rather than saturating like the two byte rows beside it. So the ×8 scale is unchanged and a loaded DOS save yields the word the save already carries; the AI_DIPLO_STRENGTH_* bands keep their (unrescaled) values, and their note at ai_diplo.c:105-124 now records that they are reached in a Linux-started game at all. `ai_contact_land_combat_sum` was deliberately **not** reused: it is `static` in ai_contact.c (exporting it is another file, out of this pass's scope), and its `for (i=0..255) units_get_const(pool, i)` walk treats the loop variable as an *id*, so it silently drops every unit whose id ≥ 256 — ids are handed out monotonically and never recycled (units.c:328). The new helper slot-walks and passes `u->id`, like the census writer. `gcc -fsyntax-only` clean; suite run by the wave's main thread. **Residue (not fixed here):** the DS-verbatim 153e reads (:3196/:3211 and the `tribe_data_9184` compares at :3167/:3182/:3185) and the smite price (:2680-2682) still read the frozen `stuff`, as does ai_king.c:3904-3905 (another file). Each of those *sums or compares* `land_combat_strength` against `field_combat_totals` / `tribe_data_9184`, which have no live recompute anywhere in ai_diplo.c, so making one half live would compare a live number against a frozen one — worse than the present state. Closing them needs either a live per-turn census refresh or live twins for those two tables; finding 15's duplication smell also grows by one copy (this is now the fifth port of the FUN_4962_0018 family), and the export/dedupe belongs in an ai_contact.c-owning pass.

3. src/core/ai_king.c:2645 — the REF invasion picks the *least* attractive colony because it walks DOS's ascending score list forwards; evidence: the port sorts `score[]` ascending (:2618) and then iterates from index 0, but `FUN_43f7_0982` walks the same ascending list **backwards** (`iVar4 = local_2a − local_48; local_6a = local_42[iVar4−1]`, viceroy_unpacked.c:74052-74056, `local_48` starting at 0), taking the highest score first — and the score is `colonists*(125−SoL) − 75*strength`, so high = large and weakly held. The port's own `ai_king_rank_nations_0218` header already establishes `FUN_291f_0ed0` as an ascending sort. Net effect: the REF lands on the smallest/best-defended port instead of the juiciest one. Confidence H.
   **RESOLVED 2026-09-10 — picker walks the list from the top.** ai_king.c:2663 is now `for (int i = n - 1; i >= 0; --i)`. Traced the DOS traversal in full before reversing, per the question raised in the finding: `local_48` is a **rejection counter, not a landing-wave cursor**. Raw 74048-74049 zeroes it at the head of *every* one of the three relaxing passes; 74050 is `while (local_48 < local_2a && local_1c != 0)`; 74051 clears `local_1c` at the top of each iteration; and the only `local_48 = local_48 + 1` is at 74084-74086, inside the `local_44 < 2 && regulars + cd + ca < need` rejection arm that also re-arms `local_1c`. So an accepted candidate exits both loops immediately, a rejected one steps `local_48` and the walk moves one slot further down — `local_42[(local_2a - local_48) - 1]`, i.e. plain descending order restarted per pass. There is no cross-call or cross-wave persistence (the whole array is rebuilt from scratch each `FUN_43f7_0982` entry), so straight reverse iteration is the exact traversal; the port's existing per-pass restart, `continue`-on-reject and `break`-on-accept already matched. Also verified 74094's `if (local_48 < local_2a)` landing gate is only reachable-false when `local_2a == 0` (pass 2 has no rejection arm, so it always accepts its first candidate) — the port's `pick >= 0` test covers it. Comments at :2596 and :2655-2662 updated to say the list is sorted ascending and walked from the top, with the raw citation.

4. src/core/ai_king.c:3667 — `ai_king_frigate_offer`'s "nation already owns a Frigate" guard bounds its loop with `ctx->units->unit_count`, a live population counter over a sparse slot array, so any despawn hole hides every unit past that index; evidence: units.c:1274-1277 decrements `pool->unit_count` on despawn while `units_clear_slot` leaves the array sparse, and this is the only `unit_count`-bounded loop in the file — every sibling uses `COLONIZE_UNITS_MAX` (ai_king.c:2327, :4028, :4451). Consequence: @KINGFRIGATE re-fires every 8th turn for a nation that already has one, each acceptance stacking a free Frigate plus a real +10% `ai_king_tax_hike_apply`. Confidence H.
   **RESOLVED 2026-09-10 — guard bounded by COLONIZE_UNITS_MAX.** ai_king.c:3689 now iterates the full slot array like its siblings (post-edit line numbers: :2337 the 0982 pool-unit sweep, :4037, :4475). Confirmed the sparse-slot premise at units.c:1274-1277: `units_clear_slot(unit)` runs first and leaves the slot in place, then `if (pool->unit_count > 0) pool->unit_count--` — so after any despawn the live units at the tail of the array sit past `unit_count` and were invisible to this loop, and `unit_count` is a population counter with no relation to the highest occupied index. It was the only `unit_count`-bounded loop in ai_king.c; a grep confirms none remain. The loop body already tested `u->active`, so widening the bound is safe on the empty slots the walk now sees. Comment added at :3685-3688 recording why the bound is the array size.

5. src/core/ai_diplo.c:2739 — a ctx-bearing **+100** alarm delta still calls the half-writer `ai_diplo_indian_alarm_delta`, so the FUN_4cc6_00f2 escalation tail (mission expel + @INDIANBURN + its RNG draw) cannot fire from the one site guaranteed to land the pair at alarm 100; evidence: the line's own comment cites `FUN_281f_0d6c(tribe, h, 100, 0)`, and both ai_diplo.c:635-646 and ai_contact.c:311-320 document 0d6c as a thunk to the *whole* of 4cc6_00f2 — which is why prior-audit #46 moved every ctx-bearing site onto `ai_contact_alarm_delta_00f2`. ai_contact.c:4666-4676 asserts the remaining bare callers are "units.c, game_loop.c" that "genuinely have no turn context in hand"; this site is inside `ai_talk_answer`, which has `ColonizeTurnContext* ctx`, and ai_diplo.c already `#include "core/ai_contact.h"` (ai_diplo.c:2). Confidence H that it is inconsistent with #46; M that DOS burns missions here (confirm by reading 153e's third-party crusade arm around raw 97600 for the 0d6c call).
   **RESOLVED 2026-09-10 — switched to the full writer; DOS confirms.** The requested read: 153e's third-party crusade arm is `if (local_a == 2) { if (iVar17 < 4) { FUN_281f_0a10(0x281f, param_2, iVar17, 0x40); *(param_2 + iVar17*0x13c + -0x77c4) |= 2; } else { FUN_281f_0d6c(0x281f, iVar17 + -4, param_2, 100, 0); } }` (viceroy_unpacked.c:97756-97770) — so the Euro third party gets clear-PEACE + set-WAR and the **Indian** third party gets 0d6c(+100), i.e. the whole of FUN_4cc6_00f2, escalation tail included. ai_diplo.c:2739 now calls `ai_contact_alarm_delta_00f2(ctx, k->third, h, 100)`; `k->third` is already `tr + 4` (4..11), which is the `nation_id` convention that function takes (ai_contact.c:331, `idx = nation_id - 4`). Comment rewritten with the raw citation. Effect: the mission expel + @INDIANBURN and their `rng(0,10) <= cap+1` draw are now reachable from the one site guaranteed to land the pair at 100 — the draw only fires when the pair actually reaches alarm 100 while formally at PEACE, which is DOS's own gate, so the shared RNG stream moves only where DOS moves it. `gcc -fsyntax-only` clean.

6. src/core/ai_king.c:3403 — the paid mercenary hire is a second, divergent implementation of the DOS call the free intervention already ports; evidence: `FUN_43f7_2022` ends with `thunk_FUN_2a1f_010a(uVar7)` = `FUN_43f7_10f0(1)` (viceroy_unpacked.c:75060-75070), reusing 10f0's colony roulette, water-tile scoring, Man-O-War and the `FUN_43f7_0082` type map (reading merc counts from `-0x61ba` instead of the pools), whereas `ai_king_do_merc_hire_at` spawns bare `"Regular"/"Dragoon"/"Artillery"` at `(hx, hy+1)` with no terrain test at all — the same water-spawn class bugs.md 261 fixed for 06a6, and contradicted by the sibling comment at ai_king.c:2938 which says 0082 yields Cont. Army (9) / Cont. Cav. (7) for the human at war. Confidence M-H.

   **RESOLVED 2026-09-10 — paid hire now shares the 10f0 port; divergent spawner deleted; two sub-bugs fixed en route.** Raw confirmed: 2022 tails into `FUN_43f7_10f0(1)` (75068/75377-75381) and the paid flag changes exactly four things — no pool gate/decrement (74379, 74445), counts from DS:0x9e46 (74424), @MERCS naming rival slot 2 with no music switch (74396-406), carrier Man-O-War despawned after unload (74451). `ai_king_foreign_intervene_ex` wraps shared `ai_king_10f0_land(ctx, from_bells, paid, merc_counts)`; `ai_king_do_merc_hire_at` debits and calls it. The old bare `(hx, hy+1)` spawner (the water-spawn class of bugs.md 261) and its hand-rolled @MERCS popup are gone. Sub-bugs: the extra-unit coin flip was inverted (DOS `rng(0,1)==0` → slot-3 Artillery; port had `extra_flag ? Artillery : Dragoons` in both offer text and spawn), and the @MERCS tokens were "the colonies"/"Trained" where DOS splices the landing colony and the slot-2 nationality adjective.

7. src/core/ai_king.c:4529 — `ai_king_woi_pop_share_pct` recomputes crown-vs-human population from the colony pool with a per-colony `pop>0?pop:1` floor, where DOS reads the census mirror with symmetric +1 offsets; evidence: `FUN_3844_0442` computes `local_8 = (crown[0x940c]+1)*100 / ((human[0x940c]+1) + (crown[0x940c]+1))` (viceroy_unpacked.c:58510-58521; `0x940c` = `stuff.colony_pop_totals[]`, save_format_map.md row 243). The missing +1s move the @WARN3/@LOSING3 boundary — human 1 / crown 9 is 83% (warn) in DOS but 90% (instant surrender) here. Confidence M-H. (Note: `colony_pop_totals` is one of the frozen fields from finding 2, so fixing this without fixing 2 substitutes one wrong number for another.)

   **RESOLVED 2026-09-10 — reads `stuff.colony_pop_totals[]` with the symmetric +1s.** The raw's `if (byte == 0) x = ~byte + 1;` arms are a sign-extension artifact — both spell the plain byte. Blank-census fixtures fall back to summing the live pools before the +1s. An empty world now answers 50, DOS's own answer (no latch reacts to it). Two test_ai_king.c fixtures recalibrated (crown pop 227→232 for the 85% band, 90→100 for the ≥90% @LOSING3 bar) — both were port-authored constants fitted to the old formula, not DOS evidence; comments cite the +1 formula.

8. src/core/ai_contact.c:3769-3777 (also :5132-5134, :5783-5785, and the stamps at :3884, :5227, :5376, :5863, :5934) — the shared `s_visit_cooldown_until` throttle stamps `now_turn + 8`, directly contradicting the two comments added in the same pass that argue DOS's visit latch is **per-turn**; evidence: ai_contact.c:3843-3854 says "The latch is per-year, NOT permanent … each tribe/Euro pair resolves at most one gift-or-beg visit per turn, then is eligible again next turn — which is why DOS villages keep visiting instead of going permanently quiet", and ai_contact.c:29-45 says DOS's sleep is `contact_state`, cleared at the top of every turn by `ai_indian_midpass_clear_tables` (ai.c:4871, called from turn.c:3771). With the 8-turn stamp sitting on top of the (correct) `contact_state` latch, the port suppresses seven of every eight DOS-legal village visits per Indian nation. The 8 is uncited — its only pedigree is the now-stale line at :3770-3773, written when the throttle was the only latch. Confidence M-H on the contradiction; the call to make is whether the 8 is deliberate pacing.
   **RESOLVED 2026-09-10 — throttle removed.** DOS checked: `FUN_5bfb_022e` and its move-tail caller (viceroy 98653-98676, `aiStack_20[nation]` = once per nation per move) carry no turn counter; `contact_state` is cleared at the top of every `FUN_4d56_1b3a` call (viceroy 81704-81707), i.e. per turn. Pacing = walk-up trigger + per-turn latch + alarm ceiling + mood RNG, all already ported. `s_visit_cooldown_until` deleted (all 3 gates, 6 stamps, reset memset); the bugs.md-423 exclusion is carried by ai.c §9 arm order + the state-2 latch, re-asserted by the rewritten unit_ai_contact block. Stale "per-year" wording in ai.c/ai_contact.c fixed; bugs.md row 423 amended. 61/61 ctest green.

9. src/core/ai_king.c:998 — `ai_king_audience_roll` omits `FUN_38fd_5be8`'s first gate; evidence: DOS opens with `if (*(char *)(iVar1 + -0x6d68) == '\0') return 0;` (viceroy_unpacked.c:68433-68435; `-0x6d68` = `0x9298` colony_counts, the same table `ai_king_rank_nations_0218` cites at ai_king.c:474), and the port's gate list starts at the `turn < 30` test that is DOS's *second* line — so a human with zero colonies still gets tax audiences from turn 30 on. Confidence M.

   **RESOLVED 2026-09-10 — `colony_counts[human] == 0 → return 0` added ahead of the `turn < 30` test**, with a live-count fallback for blank census windows; forward declaration of `ai_king_human_colonies` at :974.

10. src/core/ai_contact.c:3377, :10492-10493, :10517-10518, :10528 — prior-audit #54's "the `alarm_by_player >= 55` disjunct is provably dead" reduction was applied at 3 of 7 sibling sites, and the surviving four carry a comment asserting the opposite rationale; evidence: `ai_contact_pair_friction` (ai_contact.c:2197-2213) seeds `friction = ind->alarm_by_player[e]` and only ever raises it, so `friction >= X || alarm_by_player[e] >= X` is always just `friction >= X` — which is exactly why :2243, :2849 and :2924 were reduced. Still un-reduced: `:3377 friction >= 55 || ind->alarm_by_player[e] >= 55`; `:10493 friction < 40 && friction < 55 && ind->alarm_by_player[e] < 55` (the `friction < 55` conjunct is *also* dead under `friction < 40`); `:10518 friction >= 40 && friction < 55 && ind->alarm_by_player[e] < 55`; `:10528 ind->alarm_by_player[e] >= 55 || friction >= 55`. The comment at :3369-3375 justifies keeping the term with "Pair friction alone would always be ≥55 when alarm≥55, making gift-band unreachable" — it states the dominance relation and draws the wrong conclusion. Behaviour-neutral, but one predicate answered two ways in one file. Confidence H.

   **RESOLVED 2026-09-10 — all four dead disjuncts removed, comments fixed.** Dominance re-verified per site (each compares the `ai_contact_pair_friction` return, which seeds from `alarm_by_player[e]` and only rises). :3379 → `friction >= 55`; :10591 → `friction < 40`; :10616 → `friction >= 40 && friction < 55`; :10626 → `friction >= 55`. The misleading rationale at :3369-3375 replaced; the separate per-tribe friction max feeding the message band is documented as deliberate (it excludes the alarm seed, which keeps the "refuse gifts" wording reachable). Sites reading a single tribe's `t->alarm[e].friction` (:2080, :4121, :4218-4219, :4253-4254, :4409, :7077) checked and correctly left alone.

11. src/core/ai_king.c:2153 — `ai_king_10f0_score_tile` inverts DOS's ownership test, so its Man-O-War penalty can only fire on the wrong side; evidence: DOS checks whether the tile's stack belongs to the **crown** (`(*(byte *)(local_34*0x1c+0x3147) & 0xf) == *(byte *)0x53d2`) before subtracting 999 per type-0x12 hull (viceroy_unpacked.c:74350-74359), but the port returns −1 for any non-human unit first, so `-= 999` can only ever fire on the human's own MoW — rejecting a tile DOS accepts and accepting one DOS rejects. `u->type_index == 0x12` is also a raw id where every sibling name-matches (`ai_king_is_mow` :603, `ai_king_0982_spawn_pool_unit` :2345, the win gate at :4805 which explicitly notes "matched by NAME here (synthetic test pools reorder type indices)"). Confidence M.

   **RESOLVED 2026-09-10 — ownership polarity fixed; the two questions split.** 07e0 stack-owner vs crown (`*0x53d2`) drives the −999 per Man-O-War (matched by name via `ai_king_is_mow`, not `type_index == 0x12`); 0682 presence-owner stays the empty-or-human gate. Net change: the human's own Man-O-War no longer poisons a tile DOS accepts.

12. src/core/ai_king.c:3517 / :457 — `head.rival_nation_slot_2` is written (:519) and never read, while the popup the site stubs already has its operand mapped; evidence: `ai_king_intervention_nation_slot`'s `slot_idx == 1` arm at :457 has no caller, and reports.c:1223 / turn.c:2571 read slot_1 only; DOS's consumer is `FUN_291f_0ac8(0, 0, *0x53d6)` supplying %STRING0 for @MERCENARIES (viceroy_unpacked.c:75048), the same `*0x53d6` that names the ally in 10f0's announce (:74396). The hardcoded `"Europe"` stand-in at :3517 is uncited. Confidence M.

   **Note 2026-09-10 — premise now half-stale.** The D6 fix routes the paid merc announce through the shared 10f0 path, which reads `head.rival_nation_slot_2`, so "written and never read" no longer holds; what remains open is only the :457 `slot_idx == 1` arm's caller-less state.

   **REFUTED 2026-09-10 — the field IS read; the real bug beside it fixed.** `ai_king_intervention_nation_slot(ctx, human, paid ? 1 : 0)` at ai_king.c:3203 reaches the slot-1 arm via the merc-hire accept (:3589), so neither "never read" nor "caller-less arm" holds after the D6 fix. The block's one live claim — @MERCENARIES `%STRING0` hardcoded "Europe" — was real: DOS raw 75050 `FUN_291f_0ac8(0, 0, *0x53d6)` splices the country name of rival slot 2, the power selling the mercenaries; now resolved from the live slot.

13. src/core/ai_king.c:4612, :4653 — @WARN1/@WARN2 hardcode `tok.number0 = 1` / `tok.number1 = 1` where DOS fills all three number slots from live counts; evidence: `FUN_3844_0442` calls `FUN_281f_09ae(0, local_68…)` = coastal-port count, `(1, colony_counts[human])`, `(2, local_8)` (viceroy_unpacked.c:58552-58556). With 2 ports or 2 colonies left the popup still reads "all but 1". Confidence M.

   **RESOLVED 2026-09-10 — all three number slots filled from live counts.** New `ai_king_warn_numbers` (:4745) fills %NUMBER0 = coastal-port count, %NUMBER1 = colonies held, %NUMBER2 = pop-share pct (the D7-corrected formula) once before choosing the popup, at all three warn sites; the hardcoded 1s and their "all but 1" fallback strings are gone.

14. src/core/ai_diplo.c:1585-1593 and src/core/ai_contact.c:9095-9103 — both new `settlement_owner_at` helpers double-offset the village owner, returning `4 + tribe[i].nation_id` where `tribe.nation_id` is already the 4..11 Col1 id; evidence: ai.c:68 ("TRIBE.TXT section → Col1 nation_id (4..11)"), and every consumer subtracts 4 (ai.c:2616, ai.c:2856, colony.c:618, colony.c:705; units.c:2213-2216 bounds-checks `>= 4 && <= 11`). A village tile therefore reports 8..15 while both function headers claim "Euro colony (0..3) or Indian village (>= 4)". Latent today — both call sites only test `>= 0` — but it is the id-space class this codebase keeps re-breaking, and the third copy of the helper (col1_stuff_census.c:34-55, the one the two new copies were cloned from) carries it too. Confidence H on the mis-offset, L on current impact.

   **RESOLVED 2026-09-10 — confirmed and fixed in both copies; the id space is absolute 0..11.**
   DOS settles it directly: `FUN_137f_03e4` (viceroy_unpacked.c:6839-6856) returns
   `FUN_137f_0200(x,y)` unmodified, and `FUN_137f_0200` (:6687-6698) is
   `FUN_137f_01ac(x,y) >> 4 & 0xf`, mapping 0xf to −1 — a single tile owner nibble.
   That one nibble holds Europeans and tribes alike, which is exactly why the
   sibling one function up (:6813-6836, the villages-only variant) filters with a
   bare `if (owner < 4) owner = -1;` instead of subtracting anything. So the DOS
   return value is the absolute nation id: 0..3 Euro, 4..11 Indian.
   `ColonizeCol1Tribe.nation_id` (col1_save.h:725) is stored in that same absolute
   space — `ai.c:627` writes it from `k_tribe_txt_nations[].nation_id` (":68 → Col1
   nation_id (4..11)"), `ai.c:1129` falls back to `indian + 4`, and every reader
   indexes `col1->indian[]` with `nation_id - 4` (ai.c:2616/2856, colony.c:618/705)
   or bounds-checks `>= 4 && <= 11` (units.c:2258/2331). The `4 +` was therefore a
   real double offset; both helpers now return the field as stored.
   Callers audited — one each, and neither compensated: `ai_diplo.c`'s
   `ai_diplo_153e_exposed_combat_at` and `ai_contact.c`'s `ai_contact_land_combat_sum`
   both only ask `>= 0` for the DOS "standing on a settlement" gate, so the bug was
   latent exactly as the finding says and this fix changes no behaviour today. No
   `- 4` compensation exists anywhere against either helper.
   Not fixed here: the third clone, `col1_stuff_census_settlement_at`
   (col1_stuff_census.c:34-55), carries the identical `4 +` and is outside this
   agent's file scope — it needs the same one-line change, and its own caller gate
   (:183-190) likewise only tests `>= 0`, so it too is latent. Finding 15's
   consolidation proposal would retire all three copies at once.
   Incidental: the ai_contact.c helper and the ~70 lines of comment around it had
   nine doubly-UTF-8-encoded punctuation marks (`\xc3\xa2\xc2\x80\xc2\x94` for an em
   dash, etc.) left by an earlier edit; repaired in the same pass.

15. src/core/ai_diplo.c:1610-1640 vs src/core/ai_contact.c:9109-9160 — two independent ports of the same DOS quantity (DS:0x95b2 exposed per-continent combat, FUN_4962_0018 gate 4962:022f-026e) that must be hand-kept in sync; evidence: this pass had to fix the identical "orders +0x314c vs ai_plan +0x314b" misreading in *both* (`ai_diplo_153e_exposed_combat_at` and `ai_contact_land_combat_sum(..., exposed_only=1)`), plus the third `settlement_owner_at` clone from finding 14, plus a fourth copy of the same gate at col1_stuff_census.c:183-190. Residual divergence: ai_diplo's copy has no `cap` parameter and clamps with `if (sum > 255) return 255`, ai_contact's with `if (sum >= cap) return cap`. Maintenance smell; no live behaviour difference found today. Confidence M.

   **RESOLVED 2026-09-10 — unified on the exported `ai_contact_land_combat_sum`.** ai_diplo's exposed-row clone AND its `settlement_owner_at` clone deleted (~100 lines); both 153e callers now go through the ai_contact helper, whose header carries the FUN_4962_0018 citations and table-per-argument map. Clamp difference (255 vs cap) proven equivalent. The per-nation twin in col1_stuff_census.c is a different table (save-time) and stays. FALLOUT: the old weak-stub fallbacks in ai_diplo.c shadowed the real ai_contact/ai_euro functions in any colonize_core link where nothing else pulled those .o files (unit_ai_diplo ran stubbed `ai_euro_10ec_war_worthy` and, briefly, a stubbed exposure sum). Fixed by moving all four stubs to `ai_contact_link_stubs.c`, compiled ONLY by the four slim test targets (unit_units / unit_combat_strength / unit_colonies / unit_reports); weak definitions in ai_diplo.c are now banned by the note left there.

16. src/core/ai_diplo.c:147-151, :672 (and src/core/ai_contact.c:589) — the constant `AI_DIPLO_INDIAN_PEACE_MEET 96u` is documented as a *scalar* relation value but every live use writes it as the *bitfield* 0x60 = MET|PEACE, and its scalar alias is dead; evidence: the defining comment (:147-149) reads "Peace feeler / first-meet content floor … Heal mid-band up to this ceiling; drift still climbs to 160", while the only two writes store it into `nation[e].relation_by_indian[idx]`, which the line above :672 declares "is the DOS 0x60 MET|PEACE flag byte, not a scalar". `AI_DIPLO_INDIAN_CONTENT_FLOOR` (:151) has no user at all, and both consumers the comment names — `ai_diplo_indian_peaceful_drift` (:480-489) and `ai_diplo_indian_peace_feeler` (:501-506) — are retired no-ops. Secondary: both writes are hard **assignments** (`= 96`) while the paired Indian-side write one line earlier is an **OR** (`euro_diplo[e] |= COL1_INDIAN_PEACE_BIT`), so the Euro→Indian half silently drops any other bit the byte held; and both bypass `ai_diplo_or_both`/`clear_both`, which ai_contact.c:9170-9180 calls "the sole mutation channel" for this matrix (ai.c:5167 is a third raw write). Confidence H on the dead alias + doc contradiction, M on whether the assign-vs-OR asymmetry can bite.

   **RESOLVED 2026-09-10 — 96 == 0x60 == MET|PEACE, a bitfield; writes now or-both.** DOS never assigns the byte: the surrender idiom is FUN_43f7_0108 `clear_both(0xb)` then `or_both(0x60)` (raw 73555-73557). The capital-surrender write (was one-sided `|= 0x40` plus raw `= 96`) and the ai_contact meet write both route through `ai_diplo_or_both` so no other bit is clobbered; the three dead scalar constants (PEACE_MEET, CONTENT_FLOOR, FEELER_HEAL, plus DRIFT_CAP) deleted with tombstones. Open lead: ai.c:5218 still raw-writes `relation_by_indian[idx] = 0`. (Closed 2026-09-10 — routed through `ai_diplo_clear_both(col1, e, nation_id, 0xff)`; see the batch-of-20 lead 4 stamp.)

17. src/core/ai_king.c:4748 — the three lose branches run LOSING2 → LOSING1 → LOSING3, but DOS resolves one selector by last-write-wins in the order `ports==0 → 1`, `pct>=90 → 3`, `colonies==0 → 2` (viceroy_unpacked.c:58507-58532), so LOSING3 outranks LOSING1; the port shows @LOSING1 in the `ports==0 ∧ pct>=90 ∧ colonies>0` case where DOS shows @LOSING3. Confidence L-M.

   **RESOLVED 2026-09-10 — precedence is colonies > share > ports; branch order swapped.** DOS patches a single digit into `"@LOSING%d"` and the three tests overwrite each other (raw 58507 ports / 58526-27 share / 58532-33 colonies), so last-write-wins makes colonies strongest; @LOSING3 now precedes @LOSING1. Same edit fixed all three `%STRING2` slots: DOS raw 58538 fills the country name of rival slot 1 (the ally the deposed viceroy flees to), not a literal "Europe".

18. src/core/ai_king.c:2069 — `ai_king_10f0_pick_colony` drops DOS's `local_24 < 10` candidate cap and gives 0-pop colonies weight 1; DOS considers only the first ten coastal human colonies and weights by the raw `+0x1f` byte (viceroy_unpacked.c:74312-74320), so both the roulette total and the candidate set diverge once the human passes ten ports. Confidence L.

   **RESOLVED 2026-09-10 — ten-candidate roulette, raw +0x1f weights.** Gather now stops at 10 candidates (`local_24 < 10`, the fixed `abStack_30[10]`), weights by the raw population byte with no `max(pop,1)` floor, and the roulette walks the candidate array (raw 74312-74331) instead of re-scanning the colony list. An all-zero-weight candidate set falls through to the caller's weakest-port fallback rather than DOS's silent no-landing.

19. src/core/ai_king.c:4859 — the @RETIRING2 wartime calendar end fires on `year >= 1850 && crown_units_alive > 0`, where DOS gates on `*(int *)0x538a == 0x73a` — an exact-year test with no crown-units condition (viceroy_unpacked.c:58630). A WoI in which the crown holds colonies but has zero live units skips the retire and runs past 1850. Confidence L; confirm by checking for a second 1850 stop in turn.c (none was found).

   **RESOLVED 2026-09-10 — year-only gate; the crown-units term was invented.** DS:0x538a is the year word (init 0x5d4 = 1492 at raw 121601, incremented raw 6455), so 0x73a = 1850 and raw 58630 gates the wartime retire on the year alone — a colony-holding, unit-less crown no longer runs past 1850 forever. `>=` kept over `==` (the DOS arm is unconditional so the year cannot step past it). Also corrected in the same popup: `%STRING0` is the difficulty title (raw 58635, table −0x7c6c), not "Viceroy", and the estate colony is the LAST max-population human colony (raw 58636 `<=`). `ai_king_crown_units_alive` removed.

20. src/core/ai_popup.c:118, :139-142, :169 — bar-strip arm kinds 2 and 3 have no producer, so their ink arms and the sale-speedup's `|| kind == 2` disjunct are dead, and the comments describing kind 1 and kind 2 as two distinct producers are wrong; evidence: `ai_popup_enqueue_bar_message` hardcodes `kind = 1` (:118) and is the only push reaching this ring — its sole callers are game_loop.c:1318 (Europe `bar_event[]`, filled only by `europe_push_sale_status`, europe.c:3194) and turn.c:1802 (Custom House autosell); `ai_popup_enqueue_bar_message_kind` has no caller outside ai_popup.c. Yet ai_popup.h:428-437 and ai_popup.c:162-166 both describe "arm kind 1/2 — the Custom House autosell run **and** the European Status sell lines, the two producers that feed this ring". The net ×3 speedup is still sale-only, but by accident of there being no other producer, not by the guard the comment describes. Confidence H, severity L.

   **PARTLY REFUTED 2026-09-10 — DOS arms kinds 1 and 3; only the speedup disjunct was dead.** The ink switch's `1 || 2` is DOS's own (FUN_1009_0004, raw 449-463) and kind 3 has a real DOS producer (`FUN_281f_0de0(0x479b, 0x16, 3)`, raw 77369) merely unported, so the arms stay as transcription; nothing anywhere passes 2. Deleted the sale-speedup's dead `|| kind == 2`; the "two producers" comments were wrong differently than the audit read — DOS arms 1 and 3, and the Linux ring has exactly one producer (kind 1).

21. src/core/ai_diplo.c:688, :693, :717-721 — the "Native relations improve." status/popup arm of `ai_diplo_indian_matrix_tick` is unreachable and its guard is a no-op, while the function header still documents both; evidence: `feeler_healed` is only ever assigned from `ai_diplo_indian_peace_feeler` (:501-506), retired to `return 0` on 2026-08-27, so `else if (feeler_healed)` at :717 can never run and `if (prev_sticky != AI_DIPLO_STICKY_DEEP)` at :691 guards a call with no effect. Header at :676-681 still describes "peace feeler → sticky sync … feeler heal while sticky stays clear → 'Native relations improve.'". Confidence H, severity L.

   **RESOLVED 2026-09-10 — dead arm, guard and flag deleted.** Unreachable since the 2026-08-27 feeler retirement; `feeler_healed`, the `prev_sticky != DEEP` no-op guard and the "Native relations improve." chrome arm removed, header rewritten. Existing coverage at tests/unit/test_ai_diplo.c:1783-1823 already asserts the retired feeler writes no status line, so tests now agree with the code rather than passing by accident. `ai_diplo_indian_peace_feeler` itself stays (make-peace path).

22. src/core/ai_diplo.c:36, :39, :40 — all three `unknown26` index macros are dead, but only one says so; evidence: `AI_DIPLO_FLAG_BASE`, `AI_DIPLO_INDIAN_HOSTILE_STICKY` and `AI_DIPLO_PRIVATEER_SPAWN_SLOT` have no reference anywhere in src/ outside their own `#define` lines — the block became a union of named members (col1_save.h:672-690: `treaty_timer[4]`, `diplo_flag[4]`, `king_grace_counter`, `privateer_spawn_mask`, …) and every site uses those. Only the sticky macro carries the "Unused — kept as the block's index legend" note (:37-38), so the other two read as live. Confidence H, severity L.

   **RESOLVED 2026-09-10 — all three deleted under one tombstone.** Confirmed unreferenced across src/ and tests/; removed per the file's "X lived here / retired <date>" convention. The unknown26 index legend the sticky macro claimed to preserve already lives in the block comment above.

23. Stale doc blocks (all L, all comment-only, grouped): src/core/ai_king.c:3839 — the ~30-line block above `ai_king_new_war_event` (@KINGNEWWAR, a peacetime crown declaration) documents the invented REF land hunt / MoW unload / capital rally / 1eca promote / merc gift that was explicitly retired at :4360 ("D1 closed 2026-09-07g"), so it reads as live design for deleted code. src/core/ai_king.c:4394 — the revolution-end header says "Win: WoI + year>=1850 + no crown REF units", but the ported gate at :4826 is the real DOS C1 triple (crown colonies == 0, crown land units < 1/8, `ef[0]+(ef[1]!=0)+(ef[3]!=0) < 4`) with **no year gate**, as the inner comment at :4790 says outright; the same block's "Lose: WoI + zero coastal human ports" omits the colony and pop-share losses below it. src/core/ai_king.c:1490 — `ai_king_succession`'s header says "rank the 4 powers ascending by pop*3 + colonies*2 + SoL-ish term", contradicting the function it calls and that function's own verified header (`ai_king_rank_nations_0218` = `ship_counts*3 + colony_counts*2 + census_pop_proxy`, :474, DS −0x6be8 / −0x6d68 / −0x6bf0); the corrected inline comment sits eight lines below the wrong one.

   **RESOLVED 2026-09-10 — all three blocks rewritten.** ai_king.c:3839's invented FUN_43f7_2022 design replaced with a real FUN_38fd_5930 @KINGNEWWAR header (raw 68305-68415, gates + grant), pointing at the D1 closure; the revolution-end header now states the DOS win triple (no year gate), the single-selector @LOSING%d last-write-wins precedence and the 1850 calendar stop (file-top summary fixed with it); the succession header now spells the verified `ships*3 + colonies*2 + census_pop_proxy` (raw 73627-73629).

24. Clean / no divergence found: the new `AiPopupRequest.chain` contact-chain machinery (ai_popup.c:47-77, :625-650, :664-680, :706-712) — `ai_popup_fill_base` memsets the request so `chain` is always derived, every contact-family enqueue does pass nation_a = Euro 0..3 and nation_b = Indian 4..11 or Euro peer (spot-checked ai_contact.c:364/628/708/3942/5754/6304/6344/6610/6754/9518 and ai_diplo.c:2263/2294/2653), the tags deliberately excluded (WHACK, EURO_WAR, VILLAGE_WARN, INDIAN_LAND) really do pass a unit id in nation_a (ai_contact.c:1408, :1492), and `ai_popup_set_last_portrait` stamps `queue[queue_count-1]`, which every new `ai_contact_chief_flair` site reaches only after a successful enqueue. `ai_contact_raid_port_ship` (ai_contact.c:7295) omits an explicit `u->active` test but `units_is_on_map` (units.c:1200-1203) already covers active + live id + not embarked. `ai_king_sol_percent` (ai_king.c:817-877) keeps the Bolivar display boost inside the pop-weighted loop, matching colony_production.c:226 and combat_strength.c:651. In ai_king.c the `1d42` royal purse, the `38fd_5be8` audience ladder, `1eca` mobilization, the `0982` scoring formula, the `4826` win-gate triple and the `20e6` sail-home were all checked line-by-line against the decompile and match.

---

# Sweep 3 — E. Colony production cluster (colony.c, colony_production.c, colony_yield.c, colony_craft.c, colony_preview.c, ai.c, ai_goals.c, col1_stuff_census.c, colony.h)

Deduped against docs/smell_audit_2026-09-09.md §E (60-71) and §D (44-46,55) by substance. Items below are new, several of them introduced by the recent Phase-A / Hudson / warehouse-capacity waves.

1. **colony_production.c:449 (+ turn.c:956/1054 vs :1160) — DOS Phase I/J run BEFORE Phase C/D in the port, so the rebel accumulators and the SoL latch see a roster the DOS ones never see, and `colony_prod_tick_rebel_accumulators` re-derives bells instead of consuming the Phase-A stamp created for exactly this purpose.** `colony_eot_production.md`'s phase table is explicit: C = 57349-57414, D = 57415-57485, F/G/H = 57502-57614, **I birth = 57615**, **J starve-kill = 57623-57695**. turn.c's order is I (birth, :956-1008) → J (starve-kill, :1030-1085) → C/D (`colony_prod_tick_rebel_accumulators` + `colony_prod_refresh_sol_flags`, :1160/:1162) → F/G/H (:1215+). Consequences, all live: `cc->rebel_divisor += pop*2` uses the post-birth/post-starve head count; the bells the accumulator computes at :450 come from the post-I/J colonist array; and `colony_prod_sol_bonus` (tories = pop·(100−sol)) is likewise evaluated on the mutated roster. Worse, the function recomputes bells from scratch (`colony_prod_colony_bells_ff` at :450) while `colony->prod_bells_phase_a` already holds the Phase-A number stamped at turn.c:881 — the very "One number, two consumers" invariant this function's own header (colony_production.c:434-448, citing viceroy 57230/57231/57392) asserts. Fix is two-part: move the I/J block below F/G/H, and have the accumulator prefer `prod_bells_phase_a` when `prod_compose_stamp` matches, exactly as `turn_run_nation_ticks` already does. Confidence H (phase order is documented in-repo; only the magnitude of the numeric effect is uncertain — a birth/starve tick is needed to see it).
   **RESOLVED 2026-09-10 - I/J moved below F/G/H; accumulator now consumes the Phase A stamp.**
   Both halves confirmed against the raw before touching anything. Phase C really does sit
   immediately after the Phase B cargo loop (`local_8e = FUN_281f_0c86(...)` at
   viceroy_unpacked.c:57349) and takes its population term straight off the colony byte
   `*(char *)(iVar12 + 0x1f) * 2` at :57377 - the pre-birth, pre-starve count. Phase I's birth
   test is at :57615-57622 and Phase J's kill loop at :57623-57691, with the abandon arm
   jumping to the epilogue (`thunk_FUN_291f_0254(...); goto LAB_364b_1ae2;`, :57690-57692), so
   DOS reaches the vanish having already run C, D and F/G/H. The bells claim is exact:
   `local_ba = FUN_281f_0b50(0x281f,0x12,0)` is assigned once, at :57230, handed to the
   congress at :57231, and then read again unchanged at :57392 for the rebel dividend - the
   only writes in between are Phase C's own `+= local_8e / -0x14` / crown `-(x >> 1)`
   adjustments (:57353-57358). No second compose anywhere.

   Fix as filed. In `turn_produce_one_colony` the whole Phase I/J block (starvation latch,
   birth, easy-difficulty mercy, starve-kill, @VANISH/abandon, @FOOD1/@FOOD2/@FOODLOW chrome)
   moved verbatim from just after the food-consumption block to just after the F/G/H education
   block, leaving Phases C/D, horse breeding and education above it in DOS order. Both ends
   carry a comment naming the phase and its citation. `colony_prod_tick_rebel_accumulators`
   now prefers `colony->prod_bells_phase_a` when `colony->prod_compose_stamp ==
   col1->head.turn + 1`, the same guard `turn_run_nation_ticks` uses, and falls back to
   `colony_prod_colony_bells_ff` only for callers that never composed (direct unit-test
   callers, colonies ticked without a col1). The header in colony_production.h now says where
   the function must be called from and where its bells come from.

   Two side effects worth flagging, both DOS-faithful rather than incidental. Horse breeding
   now spends its food before Phase I's 200-food birth test instead of after it - correct,
   since DOS composes horses in Phase A and applies them with the rest of the cargos in Phase B
   (:57238-57348), long before the birth check reads `stock[+0x9a]`. And a colony that starves
   to nothing now has its rebel accumulators, SoL latch and education resolved before it
   abandons, which is what the `goto LAB_364b_1ae2` above shows DOS doing. Existing unit
   fixtures were checked and none of them assert the old order: the starve / net-zero / #63
   craft cases all pass `col1 = NULL`, so C/D is a no-op and Phase H is skipped for want of an
   rng, and the birth fixture holds no horses so breeding is 0. Goldens are binary and
   untouched - a birth or starve turn is exactly where a diff would show up, so the main thread
   should expect movement there and read it as the fix landing rather than as a regression.

2. **ai_euro.c:3982-3993 `ai_euro_colony_has_docks` — the AI plot scorer answers the Fisherman docks gate with "coastal OR Docks-family building", while the tick, the preview and the two colony-screen copies all require the building; its own comment claims they are "the same shape".** turn.c:719-735, colony_preview.c:55-68, colony_screen.c:1596 and :3884, game_loop.c:9129 are five byte-identical building-only loops (turn.c's comment: "coastal placement alone is not enough"). ai_euro.c:3987 ORs in `colony_flags & COASTAL || map_tile_is_coastal(...)` and feeds that as `has_docks` into `colony_yield_for_worker` at :4028 and as `fishable` at :4182 — so the AI scores, assigns and food-plans Fisherman plots on dockless coastal colonies that the tick then pays 0 for. Same hole from the other side in **colony_yield.c:592-594**: `colony_yield_for_tile` hardcodes `has_docks = true`, so every one of its callers bypasses the gate — colony_screen.c:478 offers "Fisherman" in the human's job list for a dockless colony (yield > 0 there, 0 at the tick), and ai_euro.c:4464/:4595 score fisherman tiles the same way. In DOS there is no `has_docks` parameter at all: FUN_15eb_18ec reads the colony, so scorer and tick cannot disagree. Confidence M-H.

   **RESOLVED 2026-09-10 — gate unified on one colony-reading answer; two of the three
   named call sites refuted.** Re-read the raw: the gate is not at 11925-11939 (that block
   is the silver/mountain forcing the terrain doc cites) but at the tail of FUN_15eb_18ec,
   `if ((7 < local_14) && (iVar3 = FUN_15eb_038e(6), iVar3 == 0)) local_26 = 0;`
   (viceroy_unpacked.c:11967-11969), and `FUN_15eb_038e(group)` is one line —
   `FUN_15eb_035e(*(undefined2 *)0x8dc6, group)` — so it reads the CURRENT-COLONY global
   and takes no colony argument. The audit's core claim holds: DOS has no `has_docks`
   parameter, and its gate is @BUILDING group 6 alone, never coastal placement.
   Fix, in DOS's shape as far as the file ownership allowed: `colony_yield_colony_has_docks(pool, colony)`
   is now the single answer, living next to the pipeline that consumes it, plus
   `colony_yield_for_tile_in_colony`, a colony-taking `colony_yield_for_tile` whose gate is
   derived rather than assumed. `colony_yield_for_worker`'s signature is untouched, so
   turn.c and the tests keep compiling; its `has_docks` doc now says where the value must
   come from. `ai_euro_colony_has_docks` is deleted — the coastal OR was the real defect —
   and its two consumers (28c8 scorer, the `fishable` food plan) call the shared answer, as
   does `ai_euro_colony_needs_colonists_5952`, whose own local test named only "Docks" and
   would have missed an upgraded slot. colony_preview.c and both colony_screen.c copies now
   call it too, leaving turn.c and game_loop.c (other owners) as the only remaining
   hand-copies, both already correct and both source-compatible with adopting it later.
   Refutations: (a) **colony_screen.c:478 is not a bug.** DOS lists Fisherman on a dockless
   colony and answers the pick with GAME.TXT @NODOCKS — the port already does exactly that
   at game_loop.c:9233 and :13490 — and the drawn number is gated, so the row correctly
   reads "Fisherman (0)". The ungated `has_docks = true` there is right; a comment now says
   why, so the next sweep does not "fix" it. (b) **ai_euro.c:4595 never sees the gate**: its
   caller filters to Sugar/Tobacco/Cotton/Fur before the loop, so no Fisherman job reaches
   it. Only **ai_euro.c:4464** was live on that side — `ai_euro_colony_free_fisherman_field`
   would walk an Expert Fisherman up to 8 tiles to a dockless colony and seat him on a plot
   worth 0; it now returns early on no-docks and scores through the colony-taking entry
   point. `colony_yield_for_tile`'s hardcoded `true` is kept deliberately: its remaining
   callers (founding-site scoring at ai_euro.c:3722/:3830, the map probes, the tests) have
   no colony, and DOS scores sites off the raw class table, not 18ec. No test encoded the
   bug — test_turn.c:2430/:2440 already asserts the docks/no-docks split through
   `colony_yield_for_worker` and still passes unchanged.

3. **colony_production.c:843/:846 — `colony_prod_worker_building_output_ctx` has no needle for "Rum Factory" or "Cigar Factory", so a colonist working the top tier of those two chains reads 0 output where colony_craft.c's recipe table pays him.** The badge/display path matches `"Rum Distill"` (misses "Rum Factory") and `"Tobacconist"` (misses "Cigar Factory"), while the sibling table colony_craft.c:59/:65 lists both explicitly, and the four other chains are covered on both sides ("Fur Fact", "Iron Works", "Textile", "Arsenal"). Two hand-maintained lists of the same fact that have drifted. Mostly masked today because colony_screen.c:2612-2620 overrides `amount` with `preview.craft_capacity[cargo]` when the preview is valid — but the local calc is the fallback, it is the value `colonies`-level unit tests assert, and `colony_prod_building_display_output_sol` is public API. Confidence H that the omission is real, M that it is user-visible.

   **RESOLVED 2026-09-10 — both needles added; the other chains audited clean.** `"Rum Factory"` and `"Cigar Factory"` added to `colony_prod_worker_building_output_ctx` with a comment tying the list to colony_craft.c's `k_recipes`. All six chains checked against the real building names (colony_screen.c:1896-1914 / col1_bridge.c:254-272): weaver, fur, blacksmith, gunsmith were already complete — the two named were the only gaps; `colony_prod_building_tier` is covered by its generic `"Factory"` needle.

4. **colony.c:1063 vs ai_euro.c:1626-1642 vs map.c:447 — three different definitions of `COLONIZE_COLONY_FLAG_COASTAL` (DOS colony +0x1c bit 0x40), one of them a side effect of picking the first build project, and the AI one clears the save-carried bit with an uncited predicate.** (a) At founding, colony.c only ORs the bit inside the `else if (map_tile_is_coastal(...))` Docks branch — a coastal colony founded at pop ≥ 3 (Stockade branch) or one where "Docks" is missing/owned never gets it. (b) `ai_euro_refresh_colony_ai_flags` recomputes it every AI turn from `!map_tile_is_land()` over **4** orthogonal neighbours (off-map reads as water ⇒ map-edge colonies become coastal) and **clears** it otherwise — so a diagonal-only-water colony that DOS marked coastal loses the bit on the port's first AI tick. (c) `map_tile_is_coastal` is 8-neighbour `map_tile_is_water`. The reader at ai_euro.c:14157 cites "raw 2054: +0x1c bit 0x40", i.e. the bit is DOS-real state; the writer at :1639 carries no citation. Both readers defensively OR with `map_tile_is_coastal`, which hides it today. Confidence M.

   **RESOLVED 2026-09-10 — one predicate, stamped at founding, set-only.** The DOS writer is FUN_364b_1ba8 (raw 58039 clears colony+0x1c, raw 58105-58110 ORs 0x40) whose predicate FUN_15eb_00a2 (raw 9340-9374) is the INSET 8-neighbour ocean/high-seas probe plus `FUN_281f_06b4(lowest-water-region) == 1` — open sea only, so lake-only and map-edge sites do not qualify. New shared `map_tile_is_open_sea_adjacent()` (map.c:1187); colony.c stamps it unconditionally at founding (was inside the Docks branch — pop≥3 / missing-Docks holes); ai_euro's 4-neighbour recompute-AND-CLEAR replaced by set-only self-heal through the shared predicate. Un-grepped consumer found and thereby fixed: combat_strength.c:588 reads the bit raw for the Bombard row icon, so human coastal colonies founded at pop≥3 previously drew Artillery instead of Man-O-War. Deliberately unchanged: the Docks-vs-Warehouse first-project pick still uses `map_tile_is_coastal` (port-invented default pinned by test_colonies.c:942 and the seed-100 goldens) — left as a lead. Golden fixtures MID02.SAV / LATE01_POST.SAV regenerated: two colony +0x1c bytes gained 0x40, closer to DOS.

5. **ai.c:2738 + :2749-2753 — the `ai_indian_village_threat` header block still documents the term the code stopped using, and its reasoning is now false.** The header says `base = ... + min(indian.capitol_x, pop/2) + ...` and argues at length that "since capitol_x is a map column it is larger than pop/2 in practice, so it behaves as pop/2". The code at :2856-2871 was corrected on 2026-09-06d — the operand is `0x59a0 + nation_id*0x4e` = `indian[nation_id−4].tech` (+2), and `cap_term = min(tribe_tech, pop/2)` with tech in 0..~5 is a *small* cap, the opposite of the header's claim. A reader trusting the header would "restore" pop/2. Same block: `founding_fathers_nation_has(col1, c->nation_id, 16)` at :2919 spells Pocahontas as a literal where `FF_POCAHONTAS` (founding_fathers.h:78) exists two lines from other symbolic FF uses. Confidence H (textual contradiction).

   **RESOLVED 2026-09-10 — header rewritten to the real term.** Raw 81038 reads `tribe.nation_id*0x4e + 0x59a0`, and 0x59a0+4*0x4e = 0x5ad8 = `indian[].tech`: the term is `min(tribe.tech, pop/2)`, and the old "capitol_x exceeds pop/2" reasoning is called out as backwards (tech 0..5 is a binding cap). ai.c:2970 now names `FF_POCAHONTAS` instead of a bare constant.

6. **ai.c:3138 vs ai.c:2938 — two readers of `t->mission` use two different "no mission" sentinels, and the growth one indexes a 4-element array with the raw nibble.** `ai_indian_152e_village_growth` uses `t->mission != 0xff` then `mission_nation = t->mission & 0x0f`; `ai_indian_village_threat` uses `(int)(int8_t)t->mission >= 0`; every other reader in the port (ai_contact.c ×12, reports.c:3510, units.c:4178) uses `COL1_TRIBE_MISSION_NONE`. col1_save.h:729 documents "0xff none; else low nibble = European nation id (0..3)". The growth path then does `ind->euro_relation_accum[mission_nation]` (col1_save.h:854, `int8_t[4]`) unbounded at ai.c:3147/:3157/:3182 — a nibble of 4..15 walks off the array into the neighbouring `ColonizeCol1Indian` fields. Not producible by any port writer today (all write `e` ∈ 0..3, optionally | 0x10) so this is latent, but a DOS-authored save or a future writer makes it live; the sibling `col1_tribe_attitude`/`_set` used two lines later *are* bounds-checked, which is the tell. Confidence M (inconsistency certain, exploitability L).

   **RESOLVED 2026-09-10 — both readers unified on one DOS-literal helper pair, index bounded.** Checked DOS before touching either side: FUN_4d56_152e does `uVar7 = (uint)*(char *)(iVar8 + 5); if (-1 < (int)uVar7) uVar7 &= 0xf;` (viceroy_unpacked.c:81472-81476) — it sign-extends the mission byte *first* and masks the nibble only when the result is non-negative, and FUN_4cc6_03f8 is identical (:81040 sign-extends into `local_e`, :81090 gates on `-1 < (int)local_e`). So the *threat* reader's `(int8_t) >= 0` was the DOS-exact one and the growth reader's `!= 0xff` was the outlier; both agree on the documented domain, so this was latent as filed. New file statics `ai_indian_tribe_has_mission` / `ai_indian_tribe_mission_nation` (ai.c:2719-2760) now carry the single reading — presence spelled through `COL1_TRIBE_MISSION_NONE` plus the DOS sign test, nation as `COL1_TRIBE_MISSION_NATION_MASK` bounded to 0..3 — and both call sites use them (ai.c:2982, ai.c:3188), killing the last `0x0fu`/`0x10u`/`0xFF` literals in the file (the ai.c:630 writer included). On the bound: DOS has none — it writes `indian + nibble + 0x36` (:81488) and `settlement + nibble*2 + 0xa` (:81490) with the raw nibble and would walk off the 4-entry arrays exactly as the finding describes — so the `>= 4 ⇒ skip the mission arm` guard is stated in the comment as port-safety over the 0..3 domain col1_save.h:729 documents, not as DOS behaviour. The threat picker deliberately keeps the *unbounded* nibble for its `== best_nation` compare (an out-of-domain nibble takes the "rival" branch, as in DOS) since nothing is indexed there; a comment says so, so the next sweep does not route it through the bounded helper. Regression test added: `run_152e_mission_nibble_out_of_domain` (tests/unit/test_ai.c) sets mission 0x07 on a village with no colony on the map and no MET bit — the only tick where nothing else writes the accumulator — and asserts `euro_relation_accum[]`, the `euro_diplo[]` bytes that follow it (offset 0x36+7 == 0x3d == `euro_diplo[3]`, the old OOB target) and all four attitudes come back untouched. No in-domain behaviour change, so no existing 152e test moves.

7. **colony.h:339-340 — `COLONIZE_COLONY_FLAG_SMALL_AI` (0x10) and `COLONIZE_COLONY_FLAG_WAGON_TRAIN` (0x20) are write-only with no explanatory note, unlike every other flag in the block.** Sole writers are ai_euro.c:1601/:1604 and :1622/:1625; zero readers anywhere in src/ or tests/. Both are re-derived from live state every AI turn and both *clear* the bit on the negative arm, so a DOS-authored save's value is discarded rather than carried. The neighbouring bits that are legitimately write-only (`AI_WANTS_PIONEER_CLEAR`, `AI_NEEDS_MILITARY`) each carry an explicit "WRITE-ONLY here / documented debt" paragraph; these two carry no comment and no DOS citation at all, so there is nothing to distinguish "save-visible state we keep on purpose" from dead code. Confidence M-L (note, not necessarily a behaviour bug).

   **RESOLVED 2026-09-10 — notes added; both premises partly wrong.** 0x10 is READ-only in the port (readers ai_euro.c:10076-10079; the port writer was deleted by audit C3) and the in-code claim "DOS has no writer" was false: raw 95845-95847 in FUN_5952_035e sets it for pop<10 behind 2a1f_05b4 probability gates — unported, filed as a lead. 0x20's semantics were wrong, not just undocumented: DOS sets it on the colony a type-0x0c wagon is HOMED to (+0x314a origin, raw 93148-93157; raw 93142 clears own colonies), not the colony it stands on — fixed (origin match; zero port readers, save-field-only reach). Style-matching notes added for 0x10/0x20/0x40 with writers/readers.

8. **ai_goals.c:546-560 — `ai_goals_site_nibble_074a` caches its "does this map carry map-gen site nibbles" probe in file-scope statics keyed only on `(map->seen pointer, width*height)`, and `ai_goals_reset` (:21) does not clear it.** A new game or a load that reuses the same `seen` allocation at the same address with the same dimensions (the common case — the pool is re-`calloc`'d per map, and the map size is fixed by the scenario) keeps the previous game's `s_nib_present` verdict, silently switching the whole founding-site extras term between the real nibble and the `unseen ? 4 : 0` fallback. Every other module-level cache in this file is reset (`s_goals`, `s_work`, `s_inv`, `s_plan`), and ai_contact.c got exactly this treatment in the previous audit (#58). Confirmable by calling `ai_init_new_game` twice with different map sources and logging `s_nib_present`. Confidence M-L.

   **RESOLVED 2026-09-10 — cache keyed on (map, seen, w, h) and reset-able.** Hoisted to module scope, cleared by `ai_goals_reset()` (wired at game Load, ai_init_new_game, and per test case), probe exported as `ai_goals_site_nibble`. ai_euro.c carried a byte-identical second copy with the same never-invalidated statics — now forwards to the shared one. Behaviour changes only where the old cache was stale, i.e. only where it was wrong. CRLF preserved.

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

   **RESOLVED 2026-09-10 — the transport arm is gone; transports now run the same ladder as
   land units.** Re-ran the fixture scan independently (all 47 `original_saves/**/*.SAV`, head
   counts at +42/+44/+46 = tribe/unit/colony, units at `390 + colony_count*202`, stride 28):
   **104 of 254** on-map Euro transports with non-goto orders carry a nonzero spent byte, and
   the three cited units decode exactly as claimed — `COLONY00-original` unit 2 (Merchantman,
   nation 3 = human, orders 0) `moves = 18`, `dutch-campaign/COLONY01` unit 35 (Caravel,
   orders 0) `moves = 15`, unit 36 (Wagon Train, orders 0) `moves = 6`. So DOS keeps a
   transport's spent byte like anybody else's, and the blanket refund was wrong. The first arm
   is now just `aboard_ship_id >= 0` (a passenger spends nothing of its own — the one thing
   COLONY00 really does show); the dead `SENTRY || NONE` disjuncts went with it. Everything
   below is unchanged and now reachable for ships and wagons: the AI_MOVE station-keep tip
   (`goto == position`, TURN5 FR 52,43) still exports 0, the #75 exhausted branch exports the
   whole allotment with the same Sentry/Fortified `park_nights` escape land units get, and the
   partial-spend branch exports `max_mp - moves_left`. Left alone deliberately: the Europe
   sentinel lanes (`x >= 200`), where the byte is genuinely mixed in DOS — 405 of 432 sentinel
   units carry 0, but 27 do not (`COLONY00_no-transports` 231,231 Artillery `moves = 1`,
   233,233 `moves = 4`), so there is no rule to write there.

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

   **RESOLVED 2026-09-10 — capture now reconciles the dock against the mirrors and writes the
   orphans out itself.** Confirmed, and the finding undercounts: `europe_spawn_dock_mirror_unit`
   really has only three callers (turn.c:2483 crosses immigrant, units.c:1169 Brewster pick,
   col1_bridge.c's import), while there are **nine** dock-push sites — the three the finding
   names (europe.c:323 `europe_dock_push_front` from `europe_disembark_passengers_to_dock`,
   ai_king.c:4008 mercenaries, ai_diplo.c:2219 sent-home) plus paid Recruit (europe.c:1189),
   free Recruit / Brewster's underlying push (:1234), the 0718 harbor spawn (:1287), Train
   (:1315) and Purchase (:1375). Only the Brewster path is covered, by its caller in units.c.
   Fixed at export, in col1_bridge.c's capture, since the producers live in files this pass
   could not touch — and it is the more robust place anyway: it cannot be bypassed by a tenth
   push site. Before the Europe ship lanes, capture claims one dock row per human mirror unit
   found in the pool (matching `profession` first so an armed or trained immigrant takes its own
   row, then any free row, both restricted to `present` rows — the same unit shape
   `europe_remove_dock_mirror_unit` matches on), and every unclaimed row is written out as its
   own record in the dock lane `236 + n`. Byte shape taken from the French originals' dock
   colonists (COLONY02 units 121-156 at 237,237): `orders 1`, `origin 0xff`, `ai_plan 0x58`,
   `vis 0`, goto 0, spent 0, type from `europe_dock_unit_type_index(dos_type)` and profession
   from the row. `capacity` gained a full `EUROPE_DOCK_MAX` of headroom for them. Import already
   creates both halves, so the next save finds the units in the pool and the loop goes quiet —
   the reconciliation converges rather than duplicating. Residue noticed, not fixed (pre-existing
   and unrelated to the loss): DOS chains the dock lane as a doubly-linked tile stack
   (COLONY02 #121 next=122, #122 prev=121 …), while the port leaves every `x >= 200` record at
   -1/-1 — the chain rebuild deliberately skips Europe-sentinel coords.

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
   **RESOLVED 2026-09-10 — cap raised to 32 and the push's answer honoured.** Save survey
   (units at `x>=200`, French originals): the human dock lane is `236+n` = **237,237** and
   carries 20 units in COLONY02 (19 Colonists + 1 Soldiers), 18 in COLONY03, 13 in
   COLONY04/COLONY09 — the finding's 24 also counted the Bound-lane (233,233) passengers the
   ship pass consumes. So DOS really does queue ~20 immigrants; only 8 of them are ever drawn.
   `EUROPE_DOCK_MAX` 8 → **32** (europe.h:16, with the survey cited there); the 8 is the
   *drawn* count, not the queue depth — `europe_dock_slot_pos` (europe.c:218) still returns
   false past `EUROPE_DOCK_ROW0+ROW1`, and both consumers (`game_loop.c:6533` renderer,
   `europe_hit_test`'s dock arm at europe.c:4180) `break` on that, so rows 8..31 are simply
   unpainted and unclickable — which is exactly what DOS's FUN_38fd_14e2 does with tier 2.
   Nothing serialises the array (grep: only `memset(eu->dock, 0, sizeof(eu->dock))`), every
   other walker is `dock_count`-bounded and dynamic (founding_fathers.c:992, europe.c:2092
   boarding, the 5e52 crosses drain at europe.c:3010 — whose −2-per-waiting-immigrant tick is
   now DOS-faithful past 8, and still clamps at 0), so the widening is fixed-array only.
   Capture needs no change: it exports the (236,236) mirrors by walking the whole pool
   (col1_bridge.c:2310-2323, `236,236 → 236+n`) with no dock-side cap, so all 20 already
   round-tripped — what was lost was the dock *row*, not the unit. Import (col1_bridge.c:1254-1286)
   now takes `europe_dock_push_load`'s bool: the @UNIT type is resolved into a local `dos_type`
   (`src->type` when ≤5, else `europe_dock_type_for(name, profession)` — the same value
   push_load itself computes) and stamped on `dock[dock_count-1]` **only** on a successful
   push, which is the row just appended; the mirror below takes its kit from that local rather
   than re-reading a row that may belong to someone else. An overflow past 32 no longer
   corrupts a neighbouring row: it keeps the unit as an unlisted mirror (so the save
   round-trip stays lossless) and says so through `diag_warn`.

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

   **RESOLVED 2026-09-10 — import now honors the stamp; round-trip closes.** `col1_bridge_apply` gates the first-on-map-unit fallback on `(int16_t)active_unit >= 0`, so 0xffff loads as `selected_id = -1`, and `game_apply_col1_save` derives `view_pieces_mode` from `head.map_mode` ANDed with "nothing selected" instead of hardcoding false. DOS's load is a bulk `fread(0x5380, 0x8e)` (raw 120252) restoring the trio verbatim, so honoring it is DOS-literal. Note `map_mode 1` does NOT imply no selection (french COLONY09 carries map_mode 1 + active 0x50; View Pieces at raw 42112 leaves 0x5392 alone) — `active_unit` is the sole authority. Fixture survey: 7 DOS campaign saves carry active_unit 0xffff (the #78 comment's "4 saves" corrected). Two leads: capture's `no_unit_selected` stamp disagrees with 3 of the 7 DOS saves (nus 0), and a View-Pieces session WITH a selection re-saves as map_mode 0 (needs view_pieces_mode plumbed into capture). (Both closed 2026-09-10 — `view_pieces_mode` is now a `col1_bridge_capture` parameter and `map_mode` stamps from it, while the `no_unit_selected` half is refuted on a 60-save tally; see the batch-of-20 lead 9 stamp. The count there is corrected again: four of the seven carry nus 0, not three.)

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

   **RESOLVED 2026-09-10 — one shared validator, encode/decode symmetric.** DOS validates at neither end (bulk fwrite/fread; sanity comes from FUN_647e_1486's whole-pool clear/renumber on route delete), so the port's guard must agree on both sides. New `col1_bridge_trade_cursor_valid`/`_stop` (col1_bridge.c:105-160) used by all four sites; the map encoder now writes `orders = 0` for an invalid cursor instead of orders 2 + a rejected cursor, and keys off `dst->orders` so sentry-aboard exports keep their profession byte. Same root cause: `game_do_trade_delete_slot` (game_loop.c:1813-1836) now clears/renumbers cursors on Europe-lane ships too — DOS's delete loop covers them as ordinary pool records at the sentinel diagonal.

6. **docs/savegame.md:162-163 contradicts col1_bridge.c:2327-2340 on native `vis_mask`.** The
   doc says "Unit `vis_mask`: euro owner bit only on spawn/`units_set_nation`/capture; natives
   export **0**". Capture zeroes `vis` only for Europe-sentinel records (`x/y >= 200`) and
   otherwise round-trips `col1_vis_mask & 0x0f` for natives. The code is right and the prose is
   wrong: 492 of 3074 on-map native unit records in the fixtures carry a nonzero vis nibble
   (values 1/2/4/8 and combinations), i.e. DOS really does track which Europeans have seen a
   brave. (Checked the neighbouring claim too — "Euro owner bit always present on MAP units" —
   and it holds in the data: 3004/3004 on-map Euro records have their own bit set.)
   Confidence H, severity L (prose only, and it invites someone to "fix" the code to zero it).

   **RESOLVED 2026-09-10 — prose fixed.** savegame.md:162-173 now states the real rule (natives round-trip `col1_vis_mask & 0x0f`; owner bit ORed for on-map Euro units; zero forced only for Europe sentinels x/y >= 200), keeps the 3004/3004 and 492/3074 fixture counts, and carries an explicit do-not-zero-natives note.

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

   **RESOLVED 2026-09-10 — write-temp-then-rename.** `col1_save_write_file` writes `path + ".tmp"` in the same directory, checks `fflush` + `fclose`, `remove(tmp)` on any failure, `rename()` over the target on success (with an `#ifdef _WIN32` `remove(path)` first — MSVCRT rename refuses an existing destination; the tree cross-builds under mingw). FF stash/restore ordering preserved on every exit path; error text unchanged (still names `path`). It is the module's only file writer; slot listing builds paths by index (savegame.c:68), so a stray `.tmp` can never surface as a slot. Verified with a scratch harness: an emit-rejected write leaves the previous file byte-identical, no `.tmp` behind.

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

   **RESOLVED 2026-09-10 — confirmed and fixed: the dump-sell price is `euro_price − 1`.**
   The decisive writer is not 0058's tail but the nation-bind rebuild at
   `viceroy_unpacked.c:6313-6320`, which loops all four nations and all 16 cargoes and does
   nothing but the derivation: `iVar2 = *(char *)(*(int *)0x84fc + local_10 + 0x4c) + -1;
   if (iVar2 < 0) iVar2 = 0; *(local_16 * 0x10 + local_10 + -0x7b44) = (char)iVar2;`. The
   same three lines appear at `:51962-51966` and `:58996-59000`. There is no writer of the
   `-0x7b44` table anywhere that stores the record byte unmodified, so DS:0x84BC is by
   construction a derived per-nation *sell* table, never the record array. Confirmed the
   port's array is the `+0x4c` record side by compiling the header:
   `offsetof(ColonizeCol1Nation, trade.euro_price) == 0x4c`, `sizeof == 0x13c` — exactly the
   audit's claim. `europe.c` now computes `record_price − 1` when the col1 byte is stamped
   (the table's own clamp is unreachable there, since the branch requires `> 0`) and falls
   back to `europe_sell_price(eu, c)` — not the raw `bid` — when it is not, fixing the
   `price <= 0` arm's identical defect. The citation block at :3578 was rewritten: the
   "no `−1`" sentence is gone, the untaxed claim (which is separate and still correct) kept,
   and the derivation spelled out with the three raw cites.
   NOT fixed here (other owner, out of this pass's file scope): the same misread stands in
   `ai_euro.c:14164-14167` and the sibling scoring reads at :14088/:14247, plus
   `ai_diplo.c`, which all use `trade.euro_price[g]` while citing DS:0x84bc. Those are
   scoring weights rather than money, so the impact is a slightly-off ordering, not a
   treasury error — but they should be swept in the same direction.

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

   **RESOLVED 2026-09-10 — confirmed against DOS and plumbed; one caller line left to the
   game_loop owner.** DOS does ledger these trades: `FUN_38fd_1dfa`
   (`viceroy_unpacked.c:60272-60295`) unconditionally adds the amount into nation `+0xbc`
   (tons) and `+0xfc` (tons2) and the `(100 − tax)`-scaled `sell_price·amount` into `+0x7c`
   (gold) on *every* call — there is no arm-row exemption in it — and the three sell rows
   call it bare (38fd:3b4a/3ba0/3bfc), as this file's own #62 refutation established. So the
   NULL was simply a leftover, not a DOS-faithful choice, and the comment at :1639-1640 was
   right while the code was wrong.
   `europe.c`/`europe.h`: added `europe_apply_dock_menu_row_ex` and
   `europe_dock_menu_apply_selection_ex`, both taking the `ColonizeCol1Save*`, and the col1
   is now threaded into the `europe_apply_trade_volume` call at the ledger site. The old
   two names remain as thin wrappers passing NULL, so the unit tests and the units-less
   `europe_menu_confirm` path are unchanged. The :1639-1640 comment now names the call and
   states that it needs a non-NULL col1 to move at all.
   Last mile, NOT done here (game_loop.c is another agent's file this pass):
   `game_loop.c:5641` must become
   `europe_dock_menu_apply_selection_ex(eu, game->units_ok ? &game->units : NULL,
   game->col1_ok ? &game->col1 : NULL, game->human_nation)` — the same guard idiom used at
   game_loop.c:1532 — or the arm rows keep ledgering nothing.

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

   **RESOLVED 2026-09-10 — one treasury per nation, behind an accessor pair; the four ad-hoc pushes and every divergent site now go through it.** Confirmed as filed, and DOS is unambiguous: `FUN_38fd_0000(nation)` (viceroy_unpacked.c 58695-58703) sets only `DS:0x9e12 = nation` and `DS:0x84fc = nation*0x13c + 0x8808` — a *pointer* to that nation's 316-byte record — and the treasury every Europe arm spends is that record's 32-bit `+0x2a/+0x2c` word (`FUN_38fd_2dfe` reads it at 60930 and debits at 60936; the `1dfa` ledger and `38fd:3b4a/3ba0/3bfc` arm rows write the same offset). There is no DOS Europe-side copy, so `europe.gold` is a port invention and the only question is who owns it. Fix: `europe.h` now declares `europe_nation_gold(eu, col1, nation)` (read: the bound nation answers from `eu->gold`, everyone else from the record) and `europe_nation_gold_add(eu, col1, nation, delta)` (write: delta onto `eu->gold` for the bound nation with the record re-stamped from it, delta onto the record for everyone else), plus `europe_set_live_screen` so the writers that legitimately hold only a save — colony-capture plunder, combat ransom/loot — can reach the human's purse without threading an `EuropeScreen` through combat (the same register-once idiom as `colonies_set_col1_context`), and `europe_gold_stamp_record` which *names* the purse→record copy the four game_loop sites open-coded. Inside europe.c a second, deliberately different helper (`europe_purse_move`) serves the Europe channels: the delta always lands on `eu->gold` — whoever is currently borrowing it — and the record is stamped only when the purse really is that nation's. That distinction is the trap this fix turned on: routing the harbor/transport sells through the *outside* accessor instead would have credited an AI seller's record while the borrow wrapper (`units.c:680`, `ai_euro.c:5030/6690`, which park an AI treasury in `eu->gold` and assign it back) then overwrote it with the untouched purse, silently deleting every AI Europe sale. Converted: (a) colony.c's plunder moves both halves by delta; (b) reports.c's F10 treasury and the Score Gold line read the accessor, so they can no longer disagree with the fallback 35 lines below; (c) turn.c's rival `gold/100`, ai_king's tax-event score, and both merc affordability checks read live, and the merc debit is one accessor call instead of a stale read followed by `ctx->europe->gold = nat->gold` clobbering the live purse. Also fixed while surveying: `units.c`'s King-galleon cash-in stamped `europe->gold` from *any* nation's record, so an AI galleon handed the human's purse an AI treasury; ai_contact's incite price and demand tribute debited the record only, so a human incite cost nothing; an Indian `AI_RAID_GOLD` drain on the human drained the stale copy; and ai_diplo's `ai_talk_gold` finished with an absolute record→purse copy that discarded any Europe purchase made since the last stamp. Deliberate residue, documented at the declarations: the five col1-less Europe actions (recruit / train / purchase / treasure cash-in / cheat) can still only move the purse, but all five are reachable from the Europe screen alone and `europe_gold_stamp_record` covers them; and no human gold credit may run inside an AI borrow window (none does — those windows call only `europe_sell_unit_hold` and `europe_cash_treasure`). `europe.c:3484`'s Custom House, the one writer that already bumped both, now uses the accessor and lost its `is_human` special case. gcc -fsyntax-only clean; the europe unit test gained an assert that a harbor sale leaves both stores equal.

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
   **RESOLVED 2026-09-10 — AI Custom House now prices off its own nation's market.** europe.c:3482-3500: the price read is now `europe_sell_price(eu, c)` for the human colony (unchanged, goldens safe) and `max(col1->nation[n].trade.euro_price[c] − 1, 0)` for every other nation, i.e. FUN_38fd_0040's arithmetic applied to that nation's own byte — the same record the tax read at :3446 was already taking, so both halves of the sale now come from one nation record as DOS's `0x84fc` does. The −1 stays on both branches: FUN_38fd_0040 *is* the `−1` (`*(char *)(*(int *)0x84fc + cargo + 0x4c) + -1`, clamped at 0), so the port's documented `euro_price − 1` convention is the DOS accessor itself, and `europe_sell_price` already encodes it for the human path — the AI branch just re-applies it to a different byte. The human branch cannot be folded into the AI one even though col1_bridge stamps the human's byte both ways: the load direction is col1 → `bid` (:1616) but the write-back `bid` → col1 (:2004) runs only on capture/save, so between saves the human's col1 byte is stale while `eu->cargo[].bid` is live — `eu->cargo[].bid` *is* the human nation's live record. Fallback mirrors the dump-sell at :3647 verbatim in mechanism (a game that never came from a DOS save has the AI nations' byte still 0, so fall back to the one Linux market) but at this site the fallback is `europe_sell_price(eu, c)` rather than the raw `eu->cargo[c].bid` the dump-sell uses, because the dump-sell reads a raw DS byte (asm 364b:17d0) with no accessor and so takes no −1, whereas this site goes through FUN_291f_09ea → FUN_38fd_0040 and must. No behaviour change for human colonies; AI Custom House proceeds now move with the AI's own prices.

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

   **RESOLVED 2026-09-10 — tier rolls on the shared DOS stream; Brewster secondary REFUTED.** DOS 46d4 splits two sources: tier rolls are `FUN_281f_04d4(1,15)/(1,10)/(1,8)` on the shared stream (viceroy 64632-64640) — now `dos_rng_range` via `europe_refill_pool_slot_rng`, plumbed from `game->move_rng` through new `_ex` forms (recruit/free/immigrant/menu_confirm, Brewster/FoY popups); the expert value comes off the per-nation 5-bit LFSR on nation+0x44/+0x45 (viceroy 64585-64590) which consumes NO shared draw — the local LCG survives only as that LFSR's documented stand-in, so force-expert refills advance the stream by zero, exactly like DOS. units.c:4852 LCR FoY fallback rewired to `europe_recruit_free_from_pool_ex` (DOS raw 103728-103731 is `FUN_38fd_4884(1,0)`×8, not a random-slot spawn). Brewster 0x13 stays: DOS writes 0x1c but 0x13 is the port's pool-wide Free Colonists byte folded at the store (`europe_set_pool_slot`), the duplicate check compares only `europe_pool_remap` output so it cannot see the difference (the filing's mechanism was wrong), and col1_bridge.c:1701 reads a saved 0x1c as slot-empty — writing the raw byte would reroll a Brewstered pool on load. Comment now says so. Goldens unmoved (62/62).

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

   **RESOLVED 2026-09-10 — the boycott word is `nation+0x20`; `EuropeScreen.boycott_bitmap` is demoted to render chrome.** Confirmed as filed and DOS settles it: the *only* boycott accessor is `FUN_38fd_05e8(cargo)` — viceroy_unpacked.c 59010-59015, `return 1 << (cargo & 0x1f) & *(uint *)(*(int *)0x84fc + 0x20);`, far-called from other segments as `thunk_FUN_291f_0cd8` — reading the bound nation record's `+0x20`, the same word the tea party ORs into (`FUN_38fd_3dc8`, viceroy 64208 / 64306-64307) and the buy-back clears (`FUN_38fd_2dfe`, viceroy 60943). Decisive for the reachability half of the filing: DOS's own trade-route Europe arrival, `FUN_479b_0bd0` — the function this port cites for the EOT unload — calls that accessor itself at viceroy 77260 (also `FUN_479b_0f60` 77424, `FUN_4720_015c` 76413), so the EOT path is *supposed* to see a tea party from the same turn. Fix: `europe_cargo_boycotted_ex(eu, col1, nation, cargo)` reads `col1->nation[nation].boycott_bitmap` and falls back to the mirror only when no save is passed; the old `europe_cargo_boycotted(eu, cargo)` survives as that NULL-col1 form, so chrome and tests are unchanged. Every trade gate now names its nation: `europe_sell_hold` / `_partial` use `seller_nation`, `europe_sell_unit_hold` and `europe_buy_unit_cargo` use the hold owner's `u->nation_id`, `europe_buy_cargo` uses `buyer_nation`, and `europe_buyback_boycott` uses `human_nation` — which also fixes the mirror-image case where a stale mirror made the market-strip click refuse to sell back a boycott that really was active. Per-nation rather than human-only is the same verdict G4 reached for the Custom House price/tax pair: DOS reads both halves of a trade off the one bound record, so an AI's boycott gates that AI's sale. The `@ARMOPTIONS` row-visibility test and the market-strip colour keep reading the mirror on purpose — both are Europe-screen chrome, drawn after the refresh at `render_europe_screen`, and the field's own comment now says so — but three hunks (ai_euro's 5d04 unload fork, and game_loop's strip colour and market click) move them to the accessor as well, which leaves the mirror with no consumer that can be stale. Fugger's elect (`founding_fathers.c:1191`) now clears the nation word *and*, for the human, the mirror — the same pair `europe_buyback_boycott` clears, so the strip stops painting red the moment he is elected instead of one frame later. The unit test was rewritten around the new invariant, and it is the shape of that test that proves the bug direction: mirror-set-with-clear-nation-word must sell, nation-word-set-with-cleared-mirror must refuse. gcc -fsyntax-only clean.

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

   **RESOLVED 2026-09-10 — guard added.** europe.c:2490 scan now requires `> 0 && < 255` with a comment naming the shared sentinel rule and the woodcut consumer; unit test pins sentinel-must-not-fire / real-hold-must-fire.

8. **founding_fathers.c:209 `founding_fathers_intervention_bells` is a read-only API with no
   caller — the counter it exposes is write-only.** Declared in founding_fathers.h:99,
   incremented at :654 from `founding_fathers_consume_woi_bell_pool`, and the field comment
   at :32 says "Score line: +1 per WoI bell-pool spend on foreign intervention" — but
   `reports_compute_score` (reports.c:3943-4040) has no such term and nothing else in
   src/ calls it. Either dead code or an unfinished Score line. Confidence H (dead), L on
   whether DOS wants the term at all.

   **RESOLVED 2026-09-10 — deleted; the counter was invented.** FUN_41f2_0092's final sum (viceroy 71385) has exactly seven terms (early-revolution, congress, villages penalty, treasury, rebel sentiment, bells/100, citizens) — no intervention term anywhere in the chain (0b70/0f56/14a8 all consume that one int). Accessor, header decl, field, increment and the test assertion removed; `founding_fathers_consume_woi_bell_pool` is now just the pool reset.

9. **founding_fathers.c:1212 — stale comment contradicts the unified FF-ownership rule.**
   The de Witt case says "FA detailed strength already peeks head.founding_father[4]
   (reports)", but reports.c:3184 now tests `reports_ff_owned_by_nation(&col1->nation[human],
   REPORTS_FOREIGN_DE_WITT_FF)` — the per-nation bitmask, as the rest of the tree does after
   the #82/#83 unification. Otherwise the bitmask-vs-count sweep is clean: no remaining
   `head.founding_father[]` ownership read anywhere (only the write-once first-claimer stamp
   at :1360 and the Score/name-grid bitmask readers). Confidence H, severity L (comment only).

   **RESOLVED 2026-09-10 — comment rewritten** to state the per-nation-bitmask rule (`reports_ff_owned_by_nation`) with `head.founding_father[]` as first-claimer stamp only.

10. **europe.c:2004 — `europe_harbor_push` hard-codes `cargo_professions[i] = -1` while its
    mirror `europe_enqueue_expected` (:2049-2050) carries the professions through.** Only
    live caller with passengers is ai_diplo.c:2202, so the impact today is a gifted ship's
    passengers losing their @JOB (labels fall back to the base unit name via
    `reports_naval_passenger_label`). Confidence H that the pair diverges, L on impact.

    **RESOLVED 2026-09-10 — professions carried.** New `europe_harbor_push_ex` mirrors `europe_enqueue_expected`; plain form forwards NULL (right for col1_bridge.c:1291, no passengers). New `units_export_cargo_professions` mirrors `units_export_cargo_types`' skip-dangling walk so indices line up; ai_diplo.c:2152 snapshots professions before the despawn.

---

# Sweep 3 — H: map / UI / popup (map.c, map_gen.c, map_menu.c, map_panel.c, popup.c, popup_msg.c, pedia.c, colony_screen.c, woodcut.c, declaration.c)

Deduped against docs/smell_audit_2026-09-09.md §H (#95–#106) by substance: those are all fixed and I re-verified the fixes at their new sites (caret rule, fog ocean-rescan, dest-halving, rumour +1 bias, tribe chrome sentinel, `pedia == 27`, dead PHYS0 bases, dead `case WOODCUT_A_NEW_WORLD`, popup `run[256]`). Nothing below repeats one.

1. **src/core/map_panel.c:1726 and :1527 — the fog-view sentinel that #100 fixed inside `map_panel_draw_tribe_chrome` still leaks into the rest of `map_panel_render`.** The parameter is *named* `human_nation` but game_loop.c:16087 passes `game_fog_nation(game)`, which is −1 under Complete Map and a *foreign* nation under SETVIEW (game_loop.c:3375-3386). Every fog use (:1288/:1302/:1317/:1335/:1436) treats −1 correctly, but two non-fog uses do not: `const bool own_stack = top && top->nation_id == human_nation;` (:1726) makes the player's **own** stack read as foreign — the sidebar collapses it to one "<Nationality> <Type>" row instead of the own-stack listing — and `map_panel_euro_country(..., human_nation)` (:1527, helper at :475) then never returns the player's custom country name for their own colonies. `map_panel_draw_tribe_chrome` (:601-608) resolves the sentinel back through `head.curr_nation_map_view` / `head.human_player` exactly because of #100; the same resolve is missing here. Confirm by opening Complete Map (or SETVIEW to another nation) and clicking one of your own multi-unit stacks. **M-H**

   **RESOLVED 2026-09-10 — sentinel resolved once at the top of `map_panel_render`; parameter renamed `fog_nation`.**
   Confirmed as filed: game_loop.c:3375-3386 returns −1 for Complete Map and the SETVIEW nation
   0..3 otherwise, and :16087 hands that straight to the parameter. Fix: `map_panel_render` now
   computes `resolved_human = col1_save_human_nation(col1)` (the control==0 slot, always 0..3;
   falls back to −1 only when there is no save to probe) and feeds it to the two "is this mine?"
   consumers — the own-stack test and `map_panel_euro_country`. Every fog read (map_tile_seen_by
   at the minimap/colony/tribe/unit passes, the tile_seen probe, both `col1_vis_mask` tests) keeps
   the raw fog value, so Complete Map and SETVIEW still paint the view they are asked for.
   Note the resolve here is deliberately NOT the tribe-chrome one: chrome wants the map-VIEW
   nation (DS:0x5396), so a SETVIEW nation is correct there, while ownership wants the actual
   player — a range check alone would have missed the SETVIEW half of this bug. To stop a third
   regression the parameter is renamed `human_nation` -> `fog_nation` in map_panel.c/.h with the
   constraint spelled out at the declaration and at the resolve. gcc -fsyntax-only clean.

2. **src/core/colony_screen.c:3985-3992 vs :4131 — the Custom House popup hardcodes width 130 in a comment that cites the `@width=190` it is ignoring; its sibling dock-orders popup hardcodes 190.** The comment reads "GAME.TXT @CUSTOM's own @width=190 — the title … is the widest line", then sets `int dialog_w = 130;` and only grows it to the measured title width. COLONIZE/GAME.TXT:2086-2090 does carry `@width=190`, and `popup_msg_fill` already latched it (popup_msg.c:371) — but colony_screen.c is the only popup owner in the tree that never calls `popup_msg_section_width`/`popup_msg_take_pending_width` (ai_popup.c:205, save_load_dialog.c:105, game_loop.c:5982 all do). The dock-orders popup 140 lines later hardcodes exactly 190 for the identically-declared `@COLONYUNIT` (GAME.TXT:1767-1769), so the two siblings disagree about where the number comes from. **M**

   **RESOLVED 2026-09-10 — both widths flow from the GAME.TXT latch.** Custom House and dock-orders popups take `popup_msg_take_pending_width()` right after their `popup_msg_fill` (colony_screen.c:407/:568, stored on the view), draw at the latched width with the old literals surviving only as no-directive fallbacks. Custom House widens 130→190; dock-orders pixel-identical.

3. **src/core/map_menu.c:843-867 — dead duplicate of `map_menu_layout_titles` whose constants have diverged from the live copy.** Both `map_menu_load` and `map_menu_layout_titles` (:1258) compute `title_x`/`title_w`, but the load-time copy uses gap `+4` (`x += menu->title_w + 4`, :852) where the live one uses `+6` (:1271), measures titles as `strlen*6` instead of `map_menu_text_width(font, …)`, and omits the `title_x + title_w > 320` clamp the live copy applies to PEDIA (:1285-1287). Nothing can read the load-time values: `map_menu_handle_input` (:1422) and `map_menu_render` (:1544) both call `map_menu_layout_titles` before touching `title_x`, and `map_menu_hit_title` (:1379) only runs inside handle_input. Either delete it or make it call the same helper — as it stands it is a diverged second answer to the same question. **M-L**

   **RESOLVED 2026-09-10 — load-time duplicate deleted** (map_menu.c:842-850 comment names `map_menu_layout_titles` as the single answer); safe because both readers call the live helper before reading `title_x`/`title_w`.

4. **src/core/map.c:1972-1998 — `map_move_cost_step` is missing the tribe/settlement cap arm that its DOS-cited sibling `map_move_spent_thirds` (:1940-1970) applies.** 465b:00e4 (`original_sources_annotated/ai/move_spent.c:124-136`, accessors.c:324-327) caps the step at 3 thirds when `tile_tribe_owner(dest) >= 0`; `map_move_spent_thirds` ports it, `map_move_cost_step` (whose header at map.h:346-350 claims "same rule at NAMES scale") does not, so a village/colony destination whose terrain class costs 2-3 reports 2-3 there and 1 in the sibling. Currently masked — the only callers of `map_move_cost_step` are tests/unit/test_units.c:4512 — but this is the third round-trip through these two helpers (audit #49, #97), and the header comment now overstates the agreement. **L** (dead outside tests; would matter the moment anything wires it).

   **RESOLVED 2026-09-10 — cap arm added at NAMES scale.** `map_move_cost_step` caps at 1 (= the sibling's 3 thirds) on `map_tile_has_city && map_tile_tribe_or_presence >= 0` (map.c:2056-2064); the two road/river `return 1` arms already sat at the cap. map.h header now states the arm instead of overstating agreement. test_units.c:4512 unaffected (no settlement on its tile).

5. **src/core/popup_msg.c:87-96 vs src/core/pedia.c:270-285 — two implementations of FUN_6f74_0c32's caret rule that disagree on 3+ carets.** popup_msg counts `while (line[caret] == '^') caret++;` and treats `caret >= 2` as centre; `pedia_caret_flags` (the copy that carries the DOS citation) eats exactly two and leaves a third as body text, matching its own header ("Anything further (a third caret, braces, spaces) is body text"). No `^^^` line exists in COLONIZE/GAME.TXT today, so this is latent, but the two parsers of the same DOS routine should not differ. **L**

   **RESOLVED 2026-09-10 — DOS eats exactly two; pedia was right; now shared.** FUN_6f74_0c32 (viceroy 115448) tests byte 0, advances, tests byte 1, stops — no loop; a third caret is body text. New shared `popup_msg_caret_flags()` in popup_msg.h (DOS citation, `POPUP_MSG_CARET_*` flags); popup_msg's collector uses it (centre keyed on the flag, not `caret >= 2`) and `pedia_caret_flags` is a one-line delegate with `PEDIA_CARET_*` aliased. CRLF preserved. Lead: new_game.c:2089/:2259/:2535 carry three more eat-all caret loops (filed below).

6. **src/core/map.h:184-197 — the doc block for `map_tile_tribe_or_presence` has been orphaned onto `map_tile_is_lake`.** The comment describing "Owner … settlement (HAS_CITY) … else an occupying unit (HAS_UNIT) … FUN_1000_88c2 / FUN_137f_0428 … Cite: euro_unit_act.md T1.8 (0015bc hard-reject)" now sits immediately above a *second* comment block (the Lake one) and then `bool map_tile_is_lake(...)`; the function it actually documents is declared bare on the next line. The T1.8 hard-reject citation is the one thing justifying units.c:8213/:8615's use of the helper, so it should not read as documentation of the lake predicate. **L** (prose only; the code is right — FUN_137f_0428 does city-owner-else-unit-owner, viceroy_unpacked_2.c:5559-5569.)

   **RESOLVED 2026-09-10 — doc block moved onto `map_tile_tribe_or_presence`;** Lake comment now sits alone on the lake predicate.

7. **src/core/colony_screen.c:3970-4046 — `@CUSTOM`'s `@smallfont` directive is parsed nowhere.** GAME.TXT:2086-2090 carries `@checkbox` (honoured — the bullet at :3938) and `@smallfont` (not). `@smallfont` is honoured for the map menus (game_loop.c:4423) and the Pick Music dialog (pick_music.c:126), so this is the same dead-directive shape audit #99 filed against pick_music, one screen over. The colony screen has no tiny font loaded, so fixing it is not a one-liner — worth recording rather than silently diverging. **L**

   **RESOLVED 2026-09-10 — `@smallfont` honoured; audit premise corrected.** The colony screen was NOT missing a tiny font — game_loop hands `colony_screen_render` FONTTINY, so the whole screen incl. popups already drew small. Fix makes the directive load-bearing: `colony_screen_section_has_directive` (colony_screen.c:351) reads `@smallfont` from `@CUSTOM`, `colony_screen_popup_font` (:370) picks FONTTINY under the directive and the newly-loaded FONTINTR dialog font (`colony_screen_load`:820, non-fatal fallback) without it. Visually a no-op today; removing the directive from GAME.TXT would now switch the popup to FONTINTR. Other colony popups deliberately left on the screen font (no `@smallfont` of their own; repointing them at FONTINTR is a visual change out of scope).

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

   **RESOLVED 2026-09-10 — DOS gate transcribed literally.** Re-verified: 12d8:000e is an OR chain (`BX || (0x20-bit && [0xa0]) || (0x40-bit && [0xa4])`) — an id with neither class bit is DROPPED (the port used to play it); 2b5a:23ce binds 0xa2/0xa0/0xa4 = Background/Event/Sound Effects in @SOUNDOPTIONS order. New `sound_id_gate_allows` (sound.c:779): signed `(int16_t)id < 0x10` always plays (so 0x8020/0x8024 stings ungated), songs 0x20..0x3f ← event_music, event ids 0x40.. ← sound_effects. New unit_sound_gate test pins the truth table (registered in CMakeLists); smoke_sound's stale gating comment fixed.

2. src/core/assets.c:171-175 — `assets_detect_madspack` reads the MADSPACK section count from header offset 13 while `madspack.c:221` reads it from offset 14; offset 14 is right. Verified on COLONIZE/EUROPE.PIK: bytes `4d…30 1a 00 03 00`, count = 3 at offset 14, whereas offset 13 yields 768. The startup asset probe (game_loop.c:7154) therefore logs `chunks=768` for every 3-section .PIK. The `n < 14` guard is also one short of the `hdr[14]` read, so at exactly n==14 it reads an uninitialized stack byte. Confidence H (empirically confirmed).

   **RESOLVED 2026-09-10 — offset 14, guard widened.** Count read is now `hdr[14] | hdr[15]<<8` matching madspack.c:221, and the short-read guard widened from `n < 14` to `n < 16` — the old guard was already one short of `hdr[14]`, so `n == 14` read uninitialized stack. Verified on shipped assets: EUROPE.PIK reports chunks=3 (was 768), COLONY.PIK chunks=2, header bytes confirm.

3. src/core/sound.c:857-871 + :890-900 + :727-735 — three smaller divergences in the same DS:0x9a/0x9e/0xa0/0xa2 scheduler. (a) The comment at :861 says "DOS only polls this with sound effects enabled or a pending change"; DS:0xa2 (the flag asm 129f:00fa tests) is **Background Music**, not sound effects — the code's `opts.background_music` is correct, the comment is not. (b) `sound_queue_unlocked` arms `pending` unconditionally, but DOS FUN_129f_02cc (asm 129f:02dd-02eb) arms DS:0x9e only when `[0xa0] != 0 && [0xa2] == 0`. (c) `sound_set_options` fades (`gsound_vm_play(vm,1)`) only when background_music went off; the DOS dialog tail (asm 2b5a:2447-2461) calls FUN_281f_04de(1) whenever *any* of the three flags is off. Confidence M-H for (a), M for (b)/(c).

   **RESOLVED 2026-09-10 — all three matched to DOS.** (a) comment corrected (0xa2 = Background Music; code was right). (b) `sound_queue_unlocked` arms pending only when `event_music && !background_music` (129f:02dd-02eb), and the same guard added to `sound_set_bgm` — FUN_129f_0318 carries the identical condition. (c) `sound_set_options` fades whenever ANY of the three flags is off (2b5a:2447-2461, FUN_281f_04de(1) = ungated fade). Lead: `sound_pump_unlocked` still returns before the random pick when `!background_music` where DOS only stops the idle repoll — random-song continuation diverges (filed below).

4. src/main.c:134 vs src/platform/diagnostics.c:107-140 — the diagnostics banner is unreachable. `diag_set_info_enabled(prefs->debug_logs)` runs at main.c:134, but `diag_init` (main.c:95) has already emitted seven `diag_info` lines (log path, executable directory, argv[0], working directory, HOME, XDG_DATA_HOME) and `settings_init` its two ("Settings file: …", "Created default settings file: …"), all of which `diag_info` (diagnostics.c:179-182) drops because `g_info_enabled` is still false. Those are exactly the lines main.c:214 calls out as "for bug reports". Fix is ordering, not logic. Confidence H.

   **RESOLVED 2026-09-10 — early lines buffered, flushed at the first `diag_set_info_enabled`.** Reordering cannot work: `settings_init` needs `diag_exe_dir()`, and settings' own two lines predate knowing `debug_logs`. `diag_info` now buffers (16-slot ring, real timestamps, overflow counted) until the first enable call settles the flag — flush if enabled, discard if not; later runtime toggles unaffected. All nine startup lines reach the log with `debug_logs` on; nothing written with it off. main.c unchanged.

5. src/platform/dos_compat/dos_compat.c:8-35 — the whole tick/port-stub half of dos_compat is dead and its one live constant contradicts the documented DOS clock. `g_tick_rate_hz` is written by `dos_compat_set_tick_rate_hz` (only caller game_loop.c:7319, value 18) and read by nothing; `dos_compat_tick_count` returns a call counter with no relation to time and its only caller discards it (`(void)dos_compat_tick_count();`, game_loop.c:12010); `dos_compat_in_port`, `dos_compat_out_port`, `dos_compat_ptr_from_segment_offset` and `dos_compat_trace_unknown` have zero callers anywhere in src/, tools/ or tests/. The 18 Hz also contradicts the project's own tick documentation (game_loop.c:6756-6800: IRQ0 at PIT divisor 0x7a8 = 608.77 Hz, ÷10 = 60.877 Hz) — anyone who wires this up later inherits the wrong rate. Confidence H (dead), M (the 18 is a trap rather than a live bug).

   **RESOLVED 2026-09-10 — dead half deleted.** Six functions + both statics out of dos_compat.c, six decls out of platform.h, both game_loop call sites gone; replacement comment records the 18 Hz vs 608.77/60.877 Hz contradiction so nobody re-adds it. Module stays (`dos_compat_normalize_asset_path` has ~100 callers; init/shutdown kept as empty lifecycle hooks), so no CMake change. Post-delete grep: zero matches. Lead: dos_types.h is a dead never-included file with a `typedef uint8_t bool` collision trap (filed below).

6. src/core/reports.c:318 (+ colony_screen.c:721/734/747/760, europe.c:1080) — six surviving nearest-colour `remap_sheet_to_palette` call sites contradict the rule the same file states at reports.c:4538 ("the reserved-DAC-block pattern (crown-europe batch: merge, never remap)") and implemented for SCORE<nn>.SS. Worse, `view->icons` (ICONS.SS) is remapped **once at load** onto `backgrounds[COLONIZE_REPORT_RELIGIOUS]` (REPORT2.PIK) and then blitted on other screens: Congress page 1 draws it over REPORT3.PIK (reports.c:1365/1449) and the labor grid over REPORT4.PIK (reports.c:1759), whose palettes differ from REPORT2's in 85 of 256 entries. I checked the actual data: every index the LUT produces resolves to the same RGB under REPORT3/REPORT4, so nothing is visibly wrong *today* — this is a latent trap plus a rule violation, not a live defect. The helper itself exists as three byte-identical private copies (only variable names and blank lines differ) with no shared home in assets.c, which is where every other palette helper lives. Confidence H (duplication + rule contradiction), L (current visual impact — refuted by the data).

   **RESOLVED 2026-09-10 — one shared helper; remap verdict CONFIRMED at all six sites; ICONS trap fixed by per-destination copies.** Palette dumps settle merge-vs-remap: the merge rule needs a sheet owning a DAC block the host leaves black — SCORE<nn>.SS over WOODPAN2.PIK (black 113-251) is the only such pair. PARCH/WOODTILE/BUILDING/ICONS all paint inside 152-251 that WOODPANL also paints (identity or near-identity), and the report photos FILL 152-229+, so merging would blacken them: remap stands everywhere, now documented per site. Helper unified as `assets_sheet_remap_to_palette` (assets.c:218, CRLF preserved), three private copies deleted, six sites repointed. The audit's no-RGB-difference claim partly refuted: ICONS idx 13 diverges on REPORT3/REPORT4 (latent — lives only in undrawn sprite 121), and icon use was far wider than filed (economic/colony/naval/indian/score bars all blit ICONS over their own backgrounds with REPORT2's LUT). Fix: one remapped ICONS instance per destination palette (`icons[COLONIZE_REPORT_ICONS_DEST_COUNT]` + Congress-p1 slot, `reports_icons_for` accessor); bar helpers take the sheet. Before/after render diff of all 9 report screens + Congress p2 from LATE01.SAV: byte-identical, confirming the defect was latent. 62/62 tests.

7. src/core/json_min.c:30-33, :72-73, :129-131, :192, :208, :231-232, :264-265 — the JSON parser checks no allocation for failure, while every sibling parser in the same layer does. `jv_new` dereferences `calloc` unchecked; `parse_raw_string`/`parse_array`/`parse_object` ignore both `malloc` and `realloc` results, and the `realloc` forms also leak the old block on failure. Compare assets.c:255-258 / :331-344 and madspack.c:201-206 / :228-233 / :253-257, which check and unwind every one. It parses settings.json and the sav_json tool's input. Confidence H (the asymmetry is factual); severity low unless allocation actually fails.

   **RESOLVED 2026-09-10 — every allocation checked, sibling-style unwind.** `jv_new` takes the parser and reports "out of memory" via `perr` (so settings.c:235 / sav_json_main.c:95 print a populated error); reallocs go through temps with the old block freed on failure; array/object paths free the not-yet-linked key/item/val and let `json_free` own the linked tree; `json_parse` returns NULL on `!v || ps.failed`. Both public callers already handled NULL.

8. src/core/assets.c:299-311 — dead sentinel guards in the message-catalog parser. The enclosing test requires `line[1] < 'a' || line[1] > 'z'`, so `body` can never equal `"options"` or `"smallfont"`; the two `strcmp` guards at :310-311 can never fire. Lowercase directives already fall through the outer `if` and are stored as ordinary content lines — which is what pick_music.c:118/126 relies on — so the guards are leftovers, not the mechanism the comment at :299-300 describes. Confidence H, severity L.

   **RESOLVED 2026-09-10 — dead guards deleted, comment states the real mechanism** (lowercase directives fail the outer test and are stored as ordinary content lines for screen-side parsers, which pick_music.c relies on).

9. src/core/opening.c:32-35 vs src/core/closing.c:302-307 — mirrored cinematics, one drops a hook. `opening_set_sound_hooks` accepts `set_bgm_fn` and discards it (`(void)set_bgm_fn;`) though game_loop.c:1078 passes `sound_set_bgm`; `closing_open` calls `g_closing_set_bgm(0)` before its cue precisely so `sound_play` takes the "no VICEROY pool" immediate branch (sound.c:917-925) instead of queueing behind the running song. Harmless today because `game_try_start_intro` only runs from main.c:198 with category still 0; it breaks the moment the intro becomes replayable. Confidence M.

   **RESOLVED 2026-09-10 — hook stored and used, opening now mirrors closing.** `opening_set_sound_hooks` keeps `set_bgm_fn` in `g_opening_set_bgm` (opening.c:31/35) and `opening_open` calls `g_opening_set_bgm(0)` immediately before `g_opening_play(OPENING_BGM_ID)` (opening.c:639-647), the same order as `closing_open` (closing.c:302-307). With the category cleared, `sound_play` takes the "No VICEROY pool" immediate branch (sound.c:964-973) instead of `sound_queue_unlocked`, so 0x34 starts now rather than waiting for a running song to go idle — which is exactly why CLOSING.EXE's port needed it. Game wiring already passed the hook (game_loop.c:1078); test_opening.c passes NULL for it and is unaffected.

10. src/core/settings.c — three read/write asymmetries. `"version"` is written (:110) and never read back, so `COLONIZE_SETTINGS_VERSION` cannot actually gate a migration. `settings_init` sets `g_settings_loaded = true` at :332 *before* `settings_load_file` at :348, so `settings_is_loaded()` reports true even on the corrupt-file path that returns false — and `game_try_start_intro` (game_loop.c:11962) keys off exactly that. `--scale` clamps to `>= 1` with no ceiling (main.c:73-75) while the settings-file path clamps 1..8 (settings.c:286), two answers to the same question. Confidence H, severity L.

   **RESOLVED 2026-09-10 — all three closed.** (a) `settings_load_file` now reads `"version"` back (settings.c:254-271): version 1 is the only shape shipped so there is nothing to migrate, but the read is the hook a future `COLONIZE_SETTINGS_VERSION` bump keys on, and a from-the-future file logs a "loading best-effort" warning instead of parsing silently. (b) `settings_init` sets `g_settings_loaded = false` up front and `true` only after `settings_load_file` succeeds (settings.c:362-371, :394) — the corrupt-file path now reports what actually happened, so `game_try_start_intro` (game_loop.c:12091), the new-game `settings_apply_to_head` (game_loop.c:7639) and `game_persist_debug_hud` (game_loop.c:2546, which would otherwise re-save over a file we deliberately left untouched) all take the no-preferences path. settings.h's contract comment updated from "whether settings_init ran at all" to "a preference file is in play"; test_settings.c asserts `!settings_first_run()` on that path (still true) and never asserts `settings_is_loaded()` there. (c) One clamp for both entry points: `settings_clamp_window_scale` (settings.c:30, declared settings.h with `COLONIZE_WINDOW_SCALE_MIN/MAX` 1..8) is now called by the settings-file read (settings.c:316) and by `--scale` (main.c:72-73); README's flag table says "clamped to 1..8".

11. src/core/assets.c:32-38 — `vga6_to8` silently passes through any channel > 63 instead of scaling, with no citation. On a palette where some channels exceed 63 the result is per-channel mixed scaling (that channel ends up ~4× darker relative to its neighbours), which would show as a hue shift rather than an obvious failure. All shipped VICEROY.PAL/COL768 data is 0..63 so nothing triggers it, but the guard is a heuristic guessing at input it never sees rather than a documented DOS rule. Confidence M, severity L.

   **RESOLVED 2026-09-10 — masked, not passed through; DOS cites added.** DOS never scales and never validates: the palette uploaders write the bytes verbatim to the DAC — `out(0x3c8, index)` then a byte-at-a-time `out(v, 0x3c9)` loop in FUN_1ade_0004 (raw 15184-15201, full 0x300-byte upload), FUN_1ae3_0006 (raw 17606-17623, partial range), the retrace-free variant at raw 123768-123783 and the read-back at raw 124222-124239. The VGA DAC latches only the low 6 bits of a 0x3c9 write, so a byte above 63 displays on real DOS as `v & 0x3f`, never as itself; the old `if (v > 63) return v;` was an uncited guess that would have rendered such a channel ~4x brighter than its neighbours. `vga6_to8` now masks first and applies the same `(v << 2) | (v >> 4)` replication to every channel (assets.c:32-53), with the citations and the "all shipped data is 0..63" note in the comment. Confirmed latent: VICEROY.PAL is 1024 bytes with max byte 63 (zero bytes over 63), and the COL768 sections feeding `assets_palette_from_col768` are DAC dumps of the same kind, so no shipped asset changes a single pixel.

12. src/platform/linux_sdl2/sdl_runtime.c:205 vs :529-536 — `platform_present` sizes its index→RGBA loop from `framebuffer->width * framebuffer->height` but `rgba_buffer` is allocated from the hardcoded 320×200 at :205; a larger framebuffer overruns the heap. Only main.c:216-221 supplies one and it is 320×200, so this is latent. Same function family hardcodes `0xFD` at :475 instead of `COLONIZE_SS_TRANSPARENT` (ss.h:10) — documented in platform.h:138, but it is the one place the magic number is re-typed. Confidence H (facts), severity L.

   **RESOLVED 2026-09-10 — loop clamped to the allocation, magic number retyped.** `platform_present` no longer sizes anything from the caller: it copies row-wise over `min(framebuffer, platform)` extents with the destination stride fixed at `platform->width` (sdl_runtime.c:536-573), zeroes the scratch and logs once when the dimensions disagree, and passes `platform->width * 4` as the `SDL_UpdateTexture` pitch (:575) and in the frame-1/120 diag line (:589). The old code walked `framebuffer->width * framebuffer->height` entries of a buffer allocated from the hardcoded 320x200 (:206) — and the streaming texture is created at that same 320x200 (:184-190), so a larger framebuffer was never presentable in the first place, only overrunnable. A null-`pixels` guard was added alongside the existing null-argument check. `0xFDu` at :475 is now `COLONIZE_SS_TRANSPARENT` via a new `#include "core/ss.h"` (:11), the same constant platform.h:138 documents for this parameter. All present-path callers pass 320x200 (main.c:212-218 and the ten `platform_present` sites in game_loop.c), so this is behaviour-preserving today.

13. src/core/closing.c:198-207 — comment/code mismatch in the firework cue. The comment says "port elapsed 1/27/37/42 is DOS frame 1/27/37/42", but the test uses `frame = elapsed % n` (:197), not `elapsed`. With `repeats == -1` (the shipped CLOS-FWK row, :283) the sheet loops forever, so the four cues re-fire on every wrap rather than once as the comment implies. Whether DOS's `_anim_loop` re-triggers per loop is what would settle it; closing.h:47-49 cites the pre-increment counter but not the wrap behaviour. Confidence M.

   **RESOLVED 2026-09-10 — code is DOS-correct (including the per-wrap re-fire); the comment was wrong and is rewritten.** Settled by disassembling COLONIZE/CLOSING.EXE directly (ndisasm -b16 on the image at file offset 0xa00 = load base; `_anim_loop` is image 0x20c, `_do_anims` image 0x102, main loop 0x44c). Each series is a 14-byte record at 0x4b96 (+0 series, +2 start frame, +4 repeats, +6 baseX, +8 delay, +10 active, +12 1-based counter). Three findings: (1) `_anim_loop` tests the counter BEFORE incrementing (0x284-0x2a5: `cmp ax,0x4` for CLOS-FWK, then `cmp ax,0x2a` / `dec al` / `sub al,0x1a` / `sub al,0x0a` = counter 1, 27, 37, 42 -> `mov ax,0x59; call 0x69b:0xe`), then increments (0x2ba) and tail-calls `_do_anims` (0x37b), which draws with the incremented counter — so the tick that plays 0x59 for counter c draws counter c+1, i.e. this port's 0-based `frame == c`. The two off-by-ones cancel and `frame == 1/27/37/42` is literally right. The hat cue is different in kind: `_do_anims` plays 0x5a inline while drawing (0x19b-0x1ae, series 0 and counter == 1), i.e. the drawn frame, which is port `frame == 0` — also already right. (2) The wrap re-fire is DOS-real: with `repeats == -1` the wrap path (0x2c4-0x2e5) skips the decrement (`jng`), sets the counter back to 1 and, with delay 0, leaves the series active, so the counter cycles 1..sprite_count forever with period sprite_count — exactly `elapsed % n`. (3) Reachability confirmed: CLOS-FWK.SS section 1 is 1056 bytes = 66 sprite records and CLOS-HAT.SS 352 = 22, so all four firework counters exist and the cue set repeats ~5.9 times over the 390-tick run. Code unchanged; closing.c:198-223 now carries the counter/frame mapping with these citations and closing.h:45-56 records the wrap behaviour the old note omitted. New lead below: the same disassembly shows CLOSING.TXT's Delay column is an inter-cycle pause in DOS, not a start-tick offset as `closing_start_tick` treats it.


---

# Leads surfaced by the 2026-09-10 fix wave (top-10 severity pass), unfiled above

1. `units_get_const(pool, i)` id-walk over `COLONIZE_UNITS_MAX` treats the loop
   index as a unit ID. IDs are handed out monotonically from 1 and never
   recycled (units.c:328, units_reset next_id=1), so these loops silently drop
   every unit with id >= 256 in a long game AND miss id 0 never/1-off at the
   start. Live at ai_contact.c:9086 (`ai_contact_land_combat_sum`) and
   ai_diplo.c `ai_diplo_153e_exposed_combat_at` (~:1620). The D2 fix's
   `ai_diplo_land_combat_strength_live` slot-walks and passes `u->id`, which is
   the correct idiom (as does col1_stuff_census_tally_units).
   **RESOLVED 2026-09-10 — 19 id-walks converted to slot walks in ai_contact.c/ai_diplo.c; 31 more catalogued elsewhere.** `units_get` / `units_get_const` / `units_is_sea` / `units_clear_orders` / `combat_unit_base_x8` all take a unit **ID**, and ids are handed out monotonically from 1 and never recycled (`units.c:337`, `units_reset` next_id = 1), so `for (i = 0; i < COLONIZE_UNITS_MAX) units_get_const(pool, i)` is not a pool walk: it dropped every unit with id >= 256 in a long game and, because id = slot + 1 on a fresh load, the highest slot even in a short one. DOS's own loop is a slot walk — raw 78159, `for (local_1a = 0; local_1a < *(int *)0x539c; ++local_1a)` indexing `0x3144 + local_1a * 0x1c` — so `&pool->units[i]` with `u->id` handed to the accessors is both the correct C and the more DOS-faithful iteration order. Fixed in `ai_contact.c` at `:2407, 3819, 4762, 5150, 5672, 5900, 9128` (`ai_contact_land_combat_sum`, the named site) and `:9953`, and in `ai_diplo.c` at `:1356, 1468, 1491, 1653` (`ai_diplo_153e_exposed_combat_at`, the named site), `:1683, 1825, 2271, 2802, 3453/3457` — the grep turned up 17 beyond the two named, including four hiding behind a `ui` loop variable rather than `i`. One of them was **not** merely an under-count: `ai_diplo.c:1388` passed the loop index to `units_clear_orders`, so waking a border garrison cleared the orders of whichever unit held `id == slot index` (consistently the unit one slot *below* the intended garrison) and woke nothing once ids passed 255. Both files are now clean under a systematic sweep; the same sweep found 31 surviving instances outside this pass's ownership — `ai_euro.c` ×27 (`:191, 6564, 8737, 8837, 8961, 9008, 9026, 9152, 9341, 9379, 9777, 9853, 9952, 9982, 10006, 10438, 10592, 11801, 11820, 12014, 12037, 12664, 13048, 14678, 14740, 15051, 20786`), `ai_king.c` ×3 (`:4037, 4121, 4171`) and `ai_goals.c:1336` — several of which also pass the index straight to `combat_unit_base_x8`/`units_is_sea`. Both edited files are `gcc -fsyntax-only` clean; the one behavioural edge worth watching in goldens is the extra top slot that now reaches every sum.

2. units.c:6447 carries a second copy of the stale "raw 99190-99196" citation
   (correct range is 99186-99195); the A1 fix corrected only the
   `units_domain_blocker_at` copy.

   **RESOLVED 2026-09-10 — three stale copies in units.c corrected; two more catalogued elsewhere.** Confirmed against the decomp: `FUN_5fef_0000` opens at viceroy_unpacked.c:99111, and the domain gate is **99186-99195** — `if (bVar2) { if (type < 0xd || 0x12 < type) local_16 = 0; else local_16 = 1; local_a._0_2_ = local_16; if (local_c != local_16) goto code_r0x0006ffa9; }`. Lines 99190-99196 are the middle of that block plus the two statements after the `goto` label, i.e. the citation pointed one arm past the comparison it names. units.c had **three** copies, not two: `:2048` (the verbatim transcription block at the head of the defender picker, cited as `99137-99147 + 99190-99196`), `:6539` (the smell #4 berthed-hull note) and `:6618` (`units_domain_blocker_at`'s own gate comment) — all now read 99186-99195. Left for a later pass, same stale range in two other files: `ai_euro.c:16721` and `combat_strength.c:541` (the latter cites 99190 as the *function* address, which is doubly wrong — the function is at 99111). No code changed; `gcc -fsyntax-only` clean.
3. `build/debug/sav_json` still segfaults on
   original_saves/french-campaign/COLONY02.SAV (known from bugs batch
   2026-09-09b; reconfirmed during the F3 fix).
   **RESOLVED 2026-09-10 — stale JSON schema in tools/col1_json.c, crashed on
   every fixture.** col1_save.h had split the old two-byte nation
   `unknown26_pad[2]` into `king_grace_counter` (+0x48) + scalar
   `unknown26_pad` (+0x4a) (smell #52 fallout), but the JSON writer still did
   `W_U8ARR(..., nt->unknown26_pad, 2)` — the scalar's VALUE was passed where
   `wi_arr` expects a data pointer, so the first nation record dereferenced
   e.g. `(const uint8_t*)0x98`. Writer now emits both fields as scalars (and
   `king_grace_counter`, previously dropped, round-trips); reader accepts both
   the new scalars and the legacy `[grace, pad]` array form. All 60+
   original_saves/port_saves fixtures convert cleanly; SAV→JSON→SAV on
   french COLONY02 differs only in 2 bytes of DOS name-tail garbage past a
   colony-name NUL (pre-existing, cosmetic — DOS leaves heap residue after
   the terminator, the JSON path zero-fills). Lead 14 of the seventh wave is
   closed by the same fix.

# Leads surfaced by the 2026-09-10 second fix wave (E6/G3/C2/C3/C7/G6/Lead1/B6/B5/A3), unfiled above

1. Attack MP model vs DOS (from A3 verification): FUN_5fef_1b0e *fully exhausts* the
   attacker after the +3 (`FUN_281f_0934` at viceroy 100376 → 1427_155e writes
   spent = max allotment), and 465b's step cost is skipped on attacks
   (`spent += local_40` inside `if (!bVar4)`, :75639-75640) — the port's
   "ship-slow survives the surcharge" model and "step cost applied
   unconditionally before combat" comment are both questionable. Needs its own
   pass; rewrites the shared model + the ship-slow test.
   **RESOLVED 2026-09-10 — lead confirmed in full; model rewritten in
   units_try_move.** Asm: 1b0e entry snapshots remaining (`uVar15`, raw
   100339-100340), does the attack `spent += 3` (100341-100343), then under
   the SAME flag calls `FUN_281f_0934` (100381-100383) → `FUN_1427_155e`
   `spent = FUN_1427_065a(unit)` = the full max allotment (raw 8880-8888) —
   so the +3 is a dead store and the real charge is a FULL exhaust, before
   the roll, win or lose, ships included; there is no ship-slow. 465b's step
   cost and its shore-crossing exhaust both sit inside `if (!bVar4)`
   (75639-75648), so an attacker pays neither. Port changes (units.c): the
   `combat_attack_mp_surcharge` int became `bool combat_attack_entry`; all
   four outcome sites (loss return, land-win stay-put, native raid stay-put,
   advance/walk-in via the shared charge site) now call `units_mp_exhaust`
   instead of `units_mp_charge(cost + 3)`, and the shared charge site skips
   step cost + shore exhaust for attacks. Charging stays AFTER the resolve
   because the fatigue peel and its Combat Analysis rows read the ENTRY
   remaining, which DOS gets from the pre-mutation snapshot. Also ported the
   1b0e entry gate (100359-100372): with remaining < 3 thirds an attack is
   refused outright for natives/crown (`3 < uVar16`) and for any Euro slot
   with `0x543f[nation] != 0` (port: `col1->player[n].control != 0`); only
   the interactive human reaches the @HALF tired-attack CHOICE, which
   game_loop already asks before the move. Gate is active only when col1 is
   wired so bare unit fixtures keep their attacks. The naval "ship-slow" test
   in test_units.c re-premised to expect 0 MP after a naval win; the stale
   "PARK: ship-slow formula" note in units.h deleted. 62/62 tests pass.
2. 31 remaining id-as-slot walks outside ai_contact/ai_diplo (see Lead 1's
   RESOLVED paragraph for the full line list): ai_euro.c ×27, ai_king.c ×3
   (:4037/:4121/:4171), ai_goals.c:1336.
   **RESOLVED 2026-09-10 — all 30 converted to slot walks, 1 deliberately
   left.** Idiom throughout: iterate `&pool->units[slot]`, drop the now-dead
   `!u ||` NULL arm in favour of `!u->active`, and hand `u->id` to every
   id-taking accessor (`units_get`/`units_get_const`/`units_is_sea`/
   `units_board`/`units_max_mp`/`ai_euro_is_ship_type`/
   `combat_unit_base_x8`) — same as the ai_contact/ai_diplo pass, and the
   same shape as DOS's own record-order walk (raw 78159, `local_1a` indexing
   `0x3144 + local_1a * 0x1c`). Sites, by function:
   **ai_euro.c ×26** — `ai_euro_refresh_continent_stance` (also
   `units_is_sea`/`combat_unit_base_x8` keys); the 5952_035e origin refresh;
   `ai_euro_0a60_weight_seed`'s live census fallback; the continent
   colony/land-unit pair (also `units_is_sea`); `ai_euro_0a60_stack_counts`
   (+ NULL-pool guard, since the walk no longer goes through the accessor);
   `ai_euro_0a60_unit_housekeeping`'s ship-type census, its main loop **and**
   its inner "earlier-indexed own ship in the stack" loop; the 0a60
   goal-consumption loop (also the `ai_euro_is_ship_type` key);
   `ai_euro_0a60_units_on_tile`; the col/land/skilled continent tally (also
   `units_is_sea`); the 20e6 tile-stack scoring walk; the labor-shortage
   `outside` count, its garrison decrement and the `homed_mil` tally; the
   five admission passes; `ai_euro_10ec` land combat strength (also both
   accessor keys) and `ai_euro_10ec_land_units_on`; `ai_euro_20e6_stack_count`
   and `..._stack_combat_0b` (also both keys); the 20e6 foe-stack col9 tally;
   `ai_euro_20e6_nearest_own_unit`; `ai_euro_20e6_clear_stale_board_marks`;
   the 10be transport-assemble loop; the 457e ship-dump mark loop.
   **ai_king.c ×3** — `ai_king_crown_ships_in_europe_lane`, the MoW
   return-home tile-block scan, the crown-presence (`ref_present` re-arm)
   scan. **ai_goals.c ×1** — the water-arm armed-hull stack scan.
   Two extra bug classes fell out of the sweep, both the ai_diplo.c:1388
   `units_clear_orders` class (index used as slot *and* as id in one body):
   (a) the work-slot pioneer/military scan was already a slot walk but still
   passed `ui` to `units_is_sea`, so its "is this a land unit" test read a
   different unit than the one being scored; (b)
   `ai_euro_20e6_nearest_own_unit` compared its `except_id` parameter (the
   caller passes `u->id`) against the loop *index* and returned that index to
   a caller that fed it straight back into `units_get_const` — so the
   treasure-train rendezvous test could exclude the wrong unit and then
   resolve the "mate" to a third one. Both now key on `o->id`.
   **Left as-is on purpose:** the village-errand / explore-fatigue / hop-latch
   hygiene loop in the 20e6 dispatcher (`if (units_get_const(ctx->units, i) !=
   NULL) continue;`). Those latch arrays are keyed by unit ID at every read,
   so that loop walks the addressable ID SPACE and clears the entries no live
   unit owns — `units_get_const(pool, i) == NULL` is exactly "no live unit
   has id i". Converting it would break the arrays' key. Comment added
   in-place so the next sweep does not re-flag it.
   Where a converted loop touches the id-keyed `s_0a60_pilot_state` shadow
   (housekeeping, goal consumption, admission passes, stale-board-mark clear,
   10be, 457e) the shadow keeps its `u->id` key and the file's existing
   `0 <= id < COLONIZE_UNITS_MAX` guard is applied in the loop's skip
   condition — the shadow is 256 wide, so an id past that is skipped exactly
   as the other readers (e.g. the 20e6 unit-state snapshot) already skip it.
   Verified with `gcc -fsyntax-only` at the project's debug flags; all three
   files clean. **Expected test impact:** the extra top slot and any id >= 256
   now reach the sums, so census/strength/stack figures can shift and goldens
   may need a re-run.
3. ai_king.c:3654 KINGFRIGATE gift spawn = third spelling of the voyage fleet
   count (bare `units_count_sea_for_nation`, no Europe adds) and fires for the
   human — same under-count B6 fixed in turn.c.

   **RESOLVED 2026-09-10 — B6's counter exported and reused; human-firing confirmed DOS-real.**
   The count half was real. `ai_king_frigate_spawn` fed
   `europe_voyage_turns_roll` a bare `units_count_sea_for_nation`, but DOS
   58419 is `FUN_291f_0aee(0x281f, iVar5, x, y)` — the same roll the manual
   sail path uses, reading the DS:0x9418[nation] hull tally built by
   FUN_4962_0018 over the WHOLE unit array. DOS parks a crossing ship as a
   live unit on its nation's Europe sentinel diagonal, so harbour, expected
   and bound hulls are all inside that tally; this port hoists the human's
   Europe-side ships into `EuropeScreen`, so the live-pool walk under-counts
   him by exactly those three arrays. A human with his whole fleet in the
   harbour therefore drew the `< 3 hulls` fast crossing for the Crown's gift
   Frigate. B6's `turn_voyage_ship_count` (turn.c:2819) is now non-static and
   declared in turn.h; ai_king.c calls it. game_loop.c's
   `game_voyage_ship_count` stays a separate spelling of the same three adds
   — it works on `ColonizeGameState` and has no `ColonizeTurnContext` in
   hand — and both headers now say so. The counter adds the Europe arrays
   only when `europe->bound_nation == nation`, i.e. only for the human, which
   is correct for AI nations (theirs stay on the diagonal, inside the pool
   walk), so one function serves both.
   Human-firing: **DOS-real, no change.** FUN_3844_00f2's tail
   (viceroy_unpacked.c:58393-58424) runs the whole @KINGFRIGATE block for any
   nation `iVar1`; `if ((iVar1 < 4) && (*(char *)(iVar1*0x34 + 0x543f) == '\0'))`
   plays the 0x3e audience tune and takes the interactive `FUN_281f_03fe`
   CHOICE, `else local_4 = 1` auto-accepts, and the closing
   `FUN_291f_0ae0(0xf01, 10)` (+10% KINGTAX) is behind that same human test.
   `0x543f == 0` is the HUMAN value — the sibling `== '\x01'` arm at 6397 is
   the AI nation turn and `== '\x02'` is absent. So `ai_king_nation_turn`'s
   `ai_king_frigate_offer(ctx, ctx->human_nation)` is the DOS shape and the
   port's existing header already described it correctly.
4. Leave-as dialog residues (both builders): DOS greys short-stock rows
   (`FUN_15eb_3454` returns 0xffff → disabled render) where the port omits
   them; DOS refuses every row but Colonist to an Indian Convert
   (`cur_prof == 0x1b`) — rule already ported for the Europe dock menu.
   **RESOLVED 2026-09-10 — both fixed, in both builders.** FUN_15eb_3454 (raw
   13518-13590) answers three values, and the port now carries all three: 0 =
   row absent, 0xffff = row listed but disabled (raw 50805 `local_e == -1` →
   the greyed draw FUN_291f_01b6), 0xfffe = ordinary. New
   `colonies_list_eject_roles_ex` (colony.c, old signature kept as a wrapper
   so the three test call sites are untouched) and its twin
   `game_colony_list_outside_roles` (game_loop.c) now always list the four
   gear rows and flag each enabled/disabled from colony stock (tools 20,
   muskets/horses 50), and both return the Colonist row alone when the body's
   profession is `COLONIZE_PROF_CONVERT` (raw 13557-13560,
   `0x13 < param_1 && cur_prof == 0x1b -> return 0`) — the same rule
   `europe_arm_row_enabled`'s `convert` flag already enforced on the dock
   menu. Carrier: `ColonyScreenView.eject_role_enabled[]`. Greyed rows draw in
   colour 8 (colony_screen.c, the Europe dock menu's disabled grey), are
   stepped over by the up/down keys, and are inert to Enter and to a click —
   the dialog stays up rather than answering "Cannot equip unit". The
   appliers' own stock re-checks are untouched. **Golden impact:** none —
   dialog rows and input only; no simulation path calls these builders.
   Not ported, filed below: the Missionary row's `iVar2 != 0x18` disjunct
   (DOS offers row 0x18 to an already-Jesuit body in a churchless colony).
5. C2 follow-up: the equip gate keeps DOS-literal `pop > 10` while the
   neighbouring `population > 1` gate carries the absorption +1 compensation —
   two reads of the same DOS byte with different compensation in one block.
   **RESOLVED 2026-09-10 — both reads now carry the same +1.** ai_euro.c:18781
   is `equip_pop > 9` (was `> 10`), matching the `equip_pop > 0` beside it, and
   the block comment states the rule once: both gates read colony +0x1f at the
   same point in FUN_5952_035e — raw 94276 (`+0x1f < 2` → bail) and raw 94290
   (`'\n' < +0x1f`) — which is *after* the absorption arm has added the on-tile
   Pioneer, while the port's compressed step holds the pre-absorption
   population, so every read of that byte here gets the same +1. **Golden
   impact:** the "big settled town" arm now admits pop-10 towns (11
   post-absorption), which is what DOS admits; a golden turn with a pop-10
   colony, stance 0, 50+ muskets, no NEEDS_COLONISTS and a passing 1-in-4 roll
   would gain a Soldier. Not run here (no-build rule).
6. G3 residues in ai_contact.c (human-reachable record-only gold, same class):
   `ai_contact_apply_gift_gold` (CONTACT_GIFT popup), village trade
   (`ai_contact_2820_sell_settle` credit, `ai_contact_2e92_settle` read+debit),
   display/gate reads at ~2302/2542/2828/3438-3459/6293/6556.

   **RESOLVED 2026-09-10 — every record-only gold site in ai_contact.c now goes through `europe_nation_gold` / `europe_nation_gold_add`; `grep '\.gold\|->gold'` on the file returns nothing but comments.** Confirmed as filed and one site wider. Converted, in file order: `ai_contact_apply_gift_gold` (the affordability gate **and** the debit — this is the CONTACT_GIFT amount CHOICE the human drives, so a player who had just sold in Europe could be told "The Xxx refuse gifts" against a purse his sidebar said was fat); `ai_contact_enqueue_gift_amount_choice`'s `< 5` "cannot pay Small" gate; the incite-price read at the head of the 417e Mode-1 menu; `ai_contact_demand_can_pay_gold`'s `>= 50` tribute gate; the low-friction meet band, where one `const uint32_t purse` now feeds all three thresholds that used to read `nat->gold` (`< 10` refuse, `< 20` skip-silent, `>= 0x4b` Generous); the 417e **Mode-2** auto-incite (`< 1500` pre-gate, price gate, and the `gold -= price` debit at the LAB_4d56_4499 tail); `ai_contact_2820_sell_settle`'s sale credit; `ai_contact_2e92_settle`'s buy gate and debit; and the two human-facing display reads — the BUY0/BUY1 accept label's `(of %u$)` and the @NOTENOUGH popup's `%d0` treasury figure, which could name a different number than the gate that had just rejected the purchase. The village-trade pair is the sharpest of them: buying and selling at a village is the human's main non-Europe gold channel, and crediting the record alone meant the sale was overwritten at the next europe→col1 push (the exact G3 failure mode). Mode-2 incite is AI-driven, where the accessor answers from the record as before — no behaviour change today, but it stops being wrong the moment that AI is the nation borrowing `eu->gold` (`units.c:680`, `ai_euro.c:5030/6690`). All calls pass `ctx->europe`, matching the three sites the G3 pass itself had already converted in this file (`:2701/2733`, `:2949/2962`, `:9896`). `gcc -fsyntax-only` clean at the project's debug flags.
7. Equip-tools rounding: DOS `FUN_15eb_35d0` takes `min(stock, 100, req)` tools
   with no 20-step rounding; both port appliers use `colonies_equip_tools_take`
   (whole 20-steps, a bugs.md fix) — possibly deliberate deviation, re-check.
   **REFUTED 2026-09-10 — wrong DOS function; the port is DOS-literal.**
   `FUN_15eb_35d0` is the cargo-hold loader, not the equip path: its `req`
   comes from `FUN_15eb_3208` (raw 13367-13390), which returns *free hold
   slots* and writes `*param_3 = free * 100`, so `min(stock, 100, req)` is
   "load one 100-lot into a ship/wagon hold". The equip path is
   `FUN_15eb_1068`, and raw 11250-11253 is the port's rule verbatim:
   `local_8 = stock[TOOLS] (+0xb6) / 0x14; iVar6 = local_8 * 0x14; if (100 <
   iVar6) iVar6 = 100;`, written to the unit's tools byte +0x3159 on
   profession 0x14 (raw 11274 / 11288) and charged back to the colony in the
   tail (raw 11322-11331). The four dialog builders recompute the same
   `stock/20*20`, clamped to [0x14, 100] (raw 50570-50571, 53455, 62541,
   67774). So bugs.md row 362's "a Pioneer legitimately walks with
   20/40/60/80/100" is DOS behaviour, not a port convenience. Citation added
   to `colonies_equip_tools_take` (colony.c) with a pointer from the
   game_loop.c call site; no behaviour change, no golden impact.

# Leads surfaced by the 2026-09-10 third fix wave (20-smell batch: A4/A6/A9, B3/B4/B8, C1/C5/C6/C7/C8, D6/D7/D9/D10/D11/D13, E3, F7, I2), unfiled above

1. `ai_euro_20e6_colony_sail_pick` (kept as the sole sail scorer by the C6 fix) maps
   DOS `0xa89c` to `head.difficulty`, but the real writer (raw 93110-93115) is the
   count of continents with `-0x6a0e & 8` set; and its `(-0x6a0e[cont] & 7) * 8` war
   term is omitted behind a stale "writer undecoded" comment — all four bit writers
   ARE decoded (raw 78150/78177/78235/78302/78312, now cited in
   `ai_contact_continent_presence_4962`). The term is worth up to +56 against a war
   commit threshold of > 0.

   **RESOLVED 2026-09-10 — both halves ported, bit 8 modelled, no weak symbols.**
   (a) DS:0xa89c is now `ai_contact_continent_war_count_a89c(ctx, nation)`
   (ai_contact.c, declared in ai_contact.h) — raw 93110-93115, verbatim
   `*(undefined1 *)0xa89c = 0; do { if ((*(byte *)(local_12 + -0x6a0e) & 8) != 0)
   *(char *)0xa89c = *(char *)0xa89c + '\x01'; ... } while (local_12 < 0x10);`,
   i.e. the count (0..16) of continents carrying this nation's bit 8, recounted at
   the head of the per-nation AI pass right after FUN_4962_0018 refills DS:0x95f2.
   `head.difficulty` was never that byte; the substitution existed only because the
   writer was unlocated. Consumed at raw 89660-89662 exactly as DOS spells it:
   `if ((*(char *)0xa89c != '\0') && (1 < local_48)) local_28 += (uint)*(byte
   *)0xa89c * local_48 * -8;`.
   (b) Bit 8's writer (raw 78167-78180) is now modelled inside
   `ai_contact_continent_presence_4962` via a shared static predicate
   `ai_contact_4962_unit_sets_bit8` that the tally reuses — no second copy of the
   bit logic, and nothing weak-linked. DOS gate, verbatim: own-nation unit
   (`(bVar5 & 0xf) == param_1`), non-naval (`type < 0xd || 0x12 < type`), combat row
   `1 < *(byte *)(type * 0xe + 0x5235)` (= `ColonizeUnitType.defense`, units.c:533),
   orders `+0x314c == 5 || == 6` (UNITS_ORDER_FORTIFY / FORTIFIED), and
   `FUN_281f_0696(x, y) < 0` — the **Euro-colony** owner probe, not the settlement
   probe `FUN_281f_06be` the exposed-row gate uses, so a unit on a village tile
   still sets the bit. Passengers excluded explicitly (DOS parks them at (−2,−2)
   where the continent lookup returns −1; the port rides them at the carrier tile).
   The presence loop's early-out widened from `(presence & 2) == 0` to
   `(presence & 0xa) != 0xa`. No existing reader changes: both 5952 arms test only
   bits 1/2/4, and the new scorer term masks with `& 7`.
   (c) The scorer's missing war term is live: `score += (
   ai_contact_continent_presence_4962(ctx, nation, cid) & 7) * 8;` — raw
   89654-89656 `local_58 = (*(byte *)(iVar16 + -0x6a0e) & 7) * 8; local_28 +=
   local_58;`, keyed on the **candidate colony's** continent. Bit writers cited:
   raw 78167-78180 (bit 8), 78234-78235 (bit 2), 78301-78302 (bit 4), 78306-78312
   (bit 1).

   **Expected golden impact (AI turns).** Both changes hit only the `mil != 0`
   (war-cargo) arm of `ai_euro_20e6_colony_sail_pick`, whose commit threshold is
   `best > 0`. (c) adds a uniform-per-continent +8/+16/+24 — on a single-continent
   map it is a constant offset that changes no ranking but does push marginal
   scores over the commit threshold, so expect *more* war-cargo sails to commit
   where they previously fizzled; on multi-continent maps it also re-ranks
   candidates toward contested continents. (a) replaces a constant 2..4 penalty
   multiplier with a live 0..16 count that is 0 for a nation with no fortified
   field units — so early-game war sails lose the `difficulty * mil * -8` penalty
   entirely (net +16..+48 for `mil >= 2`) and late-game multi-continent nations
   gain a much larger one. Net: `golden_ai_turns` / `golden_ai_mid01` /
   `golden_ai_late01` war-cargo destinations can move; not re-run here (no-build
   rule). Peace sails (`mil == 0`) are untouched.
2. `FUN_43f7_2244` (peacetime AI twin of the merc hire, raw 75100-75147) fills the
   same `0x9e46` array and also tails into `thunk_FUN_2a1f_010a(1)`; the port's
   counterpart (`ai_king_spawn_landing` at `(hx, hy+1)`, ai_king.c ~:4020) is a
   THIRD copy of the divergent spawner the D6 fix deleted, with the same
   water-spawn exposure. Touches AI-nation goldens; needs its own pass wiring it
   to `ai_king_10f0_land`.

   **RESOLVED 2026-09-10 — 2244 now tails into the shared 10f0 paid arm; the third spawner is deleted.**
   Confirmed the tail: 2244 ends `thunk_FUN_2a1f_010a(0x281f, 1)` at raw 75146,
   2022's accept ends `thunk_FUN_2a1f_010a(0x281f, uVar7)` at 75068, and
   `FUN_2a1f_010a` is a thunk to `FUN_43f7_10f0` (75377-75381) — one landing
   routine, `param_1` the paid flag, for both. `ai_king_ai_peacetime_gift`
   now fills the DS:0x9e46 count array as DOS does — `{regular, 0, -,
   artillery}`, with slot 1 (0x9e48, Cavalry) left zero because 2244 zeroes it
   at entry (75096-75099) and only 2022 ever writes it — stamps
   `head.rival_nation_slot_2` from the rolled nation (DOS 75091
   `*0x53d6 = iVar4`, the slot 10f0's paid arm reads for the @MERCS line),
   debits, and calls `ai_king_10f0_land(ctx, beneficiary, 0, 1, merc_counts)`.
   Debit-before-landing is DOS-literal (75136-75146: the gold goes whether or
   not the roulette finds a port). `ai_king_spawn_landing` had no callers left
   and is deleted, with a do-not-reintroduce note in its place — every King
   landing now runs 10f0's colony roulette, water-tile scoring, Man-O-War
   transport (despawned after unload in paid mode) and the FUN_43f7_0082 type
   map, so the `(hx, hy+1)` water-spawn class (bugs.md 261) is gone from
   ai_king.c entirely.
   Two enabling changes: (a) `ai_king_10f0_land`'s hoisted
   `ai_king_independence_declared` early-return is **removed** — DOS's 10f0
   has no WoI gate (74270-74310 goes straight into the colony walk); WoI state
   is the callers' business (2022 behind `0x5382 & 1` set, 2244 behind it
   clear at 75088), and every free-arm caller already gates itself
   (`ai_king_war_act`, `ai_king_spend_woi_bell_pool`), so the free path is
   unchanged while the peacetime paid path becomes reachable at all;
   (b) 10f0 gained an explicit `target` parameter for DOS's
   `iVar2 = *(int *)0x5398` (74308) — the nation the force spawns for and
   whose colonies the roulette walks. The two pre-existing callers pass
   `ctx->human_nation` (byte-exact); 2244 passes its `beneficiary`, which is
   the port's own premise, not DOS's — see the new lead below.
   Expected impact: AI-nation goldens move. The peacetime gift now lands a
   Man-O-War-borne stack on a scored water tile next to a rolled coastal
   colony (with a 5×5 reveal and an @MERCS status/popup) instead of dropping
   bare Regulars/Artillery on `(hx, hy+1)`, the MoW is despawned after
   unloading, and the gold is debited even when nothing lands. No new RNG
   draws are introduced before the landing, but 10f0's own colony roulette
   (`dos_rng_range(1, total_pop)`) and `units_try_move` consume the shared
   stream where the old spawner consumed none.
3. Build trees: `build/` has no CMakeCache — live configured trees are
   `build/debug` (Debug) and `build-release/`. Several agents burned retries on
   `cmake --build build` "could not load cache".
4. turn.h is LF, not CRLF — the crlf-source-files memory list is stale on that
   entry.

## Leads from the 2026-09-10 fix wave (batch of 20)

1. unit_chrome corner arm 4: DOS is Artillery + damaged bit (raw 2109-2111 `bVar1 == 0xb && +0x3148 & 0x80`, y+2 box at raw 2253-54), not aboard-ship; callers pass `aboard_ship_id >= 0` (map_panel.c:1189). Damaged artillery gets the plain box, artillery aboard gets the damaged offset. Needs a damage flag at the call sites.
   **RESOLVED 2026-09-10 — flag corrected end to end.** `unit_chrome_corner_for_type`'s second argument is now `damaged` (DOS +0x3148 bit7 = Linux `col1_unknown15 & 0x80`, the same bit the damaged-Artillery combat gates read at units.c:11130/11294), and the enum arm is `UNIT_CHROME_CORNER_TOP_CENTER_DAMAGED`. `bool aboard` renamed to `bool damaged` through the whole chrome API (unit_chrome.h/.c: `_draw`, `_blit_unit`, `_blit_unit_colored`, `_blit_unit_for_palette`, `_blit`, and `unit_chrome_draw_impl`). All eight unit-backed call sites now pass the real bit instead of `aboard_ship_id >= 0`: map_panel.c:1189/1478/1770, colony_screen.c:3510, unit_stack.c:408, units.c:11551 (`units_render_on_map`), combat_analysis.c:434 (`CombatAnalysisSideChrome.aboard` → `.damaged`), reports.c colony-garrison row and naval-report passenger (new `NavalRow.pass_damaged`; the naval passenger row used to hardcode `true`, i.e. every passenger drew the damaged-Artillery offset). Unit-less sites (pedia article, Europe dock/harbor rows, Europe-lane cargo in the naval report, colony docked transports) keep `false` — no unit record, no damage bit. Cite: viceroy_unpacked.c raw 2109-2111 (arm) and 2253-2254 (`local_8 = param_3 + 2`).
2. game_loop.c:14855 plain F-key path calls `units_order_fortify` on ships and still prints "Fortifying"; should mirror the Anchor wording when `units_is_sea`.
   **RESOLVED 2026-09-10 — keyboard path now words the ship half like the menu.** game_loop.c's `COLONIZE_KEY_F` handler computes `units_is_sea(&game->units, uid)` and sets the status to "Anchoring in harbor" for hulls, "Fortifying" otherwise — the exact strings `MAP_MENU_ACTION_ANCHOR`/`MAP_MENU_ACTION_FORTIFY` (game_loop.c:11407/11417) already use. Order set unchanged (Fortify/Fortified, @ORDERS letter F): `units_order_anchor` is `units_order_fortify` behind a sea gate (units.c:7872-7883), so only the chrome differed. Cite: GAME.TXT @SHIPOPTIONS line 1782 `Anchor in harbor ("Fortify")`; DOS arms no DS:0x2d54 status string for either row, both strings are Linux chrome.
3. reports.c:2467 compares `units_display_type_index` against the col1 save's DOS type byte — a third consumer of the pool-index==@UNIT-id invariant documented in unit_chrome.c.
   **RESOLVED 2026-09-10 — documented per the A4 pattern, no behavior change.** The local is renamed `dos_unit_type_id` and carries a WARNING block naming all three consumers (chrome corner, europe.c's dock display type, this raw-record match), the invariant it rests on (`units_load_types` appends NAMES.TXT @UNIT rows in file order; the shipped section is exactly Colonists 0 … Mtd. Warriors 0x16), the cosmetic-only blast radius (wrong raw record's orders letter, or wrong badge corner), and the "do NOT copy into a rules path" pointer to units.c:5117. Same treatment as Section A item 4's stamped paragraph.
4. ai.c:5218 raw-writes `relation_by_indian[idx] = 0` — the last assignment to the 15b3 matrix outside `ai_diplo_or_both`/clear pair ops (audit #16 class).

   **RESOLVED 2026-09-10 — routed through `ai_diplo_clear_both` with a full mask; the matrix now has exactly one mutation channel.** Confirmed as the last raw write (`grep -n 'relation_by_indian' src/core/*.c` now returns only the accessor's own quadrant map in ai_diplo.c). The site is inside `col1_kill_indian_nation`, itself a Linux-only whole-nation wipe, and it was one-sided: `nation[e].relation_by_indian[idx]` is only the **Euro→tribe** half of the 15b3 cell, while the other half — `indian[idx].euro_diplo[e]` — happened to be cleared a few lines up by the `memset(ind, 0, sizeof(*ind))`, so the two halves were being zeroed by two unrelated statements and would have drifted the moment either moved. DOS evidence for what the write *is*: DOS never assigns the byte in play, it has exactly two idioms — FUN_43f7_0108's surrender `clear_both(0xb)` + `or_both(0x60)` (raw 73555-73557, the #16 pair) and the new-game reset, which zeroes the whole 12-wide row a column at a time (`for (c = 0; c < 0xc; ++c) *(nation*0x13c + c - 0x77c4) = 0`, raw 121620-121622). A destroyed nation is the second: no relation with anybody, in either direction, so the write is `ai_diplo_clear_both(col1, e, nation_id, 0xff)` per Euro nation. Checked the suspected DOS analogue and it is **not** one: `FUN_4d56_00e0` (raw 81292-81346), the per-village razer this helper stands in for, never touches the 15b3 matrix at all — its extinction tail only ORs bit 0x80 into the indian record's +3 byte and fires @EXTINCT. So this stays a Linux invention, now expressed through the DOS helper. No behavioural change (both halves were already reaching 0 by the two paths); the gain is that `ai_diplo_write`'s dual-mode addressing and its `player.diplomacy` mirror are now the only way into the matrix. `gcc -fsyntax-only` clean.
5. DOS shows exactly ONE `@WARN%d` per turn (same digit-patch selector, last-write-wins, raw 58506-58534); the port fires up to three independently-latched warns. And the DOS lose/warn group gates on `0x5382 & 1 && !(0x5382 & 8)` with no REF-present condition, where the port requires `ref_already`.

   **RESOLVED 2026-09-10 — one warn selector, last-write-wins, and the DOS bit-test gate.** `ai_king_check_revolution_end` now computes `warn_sel` exactly as DOS does (raw 58506-58534): `ports < 3 -> 1`, `if (0x4f < share) -> 3`, `if (colonies < 3) -> 2`, each write overwriting the last, so the precedence is **colonies > pop share > ports** and at most one `@WARN%d` is emitted per turn. Confirmed the digit patch is the same idiom as the lose tag: `FUN_1d1d_07e4(local_58, 0xf39)` loads DS:0xf39, which the VICEROY.EXE string blob (file offset `121248 + addr`) spells **"WARN0"** — `local_54 = local_54 + cVar1` patches the trailing digit, the twin of DS:0xf29 "LOSING0" / `local_52 += cVar8`. The emission moved *below* the lose and win blocks because DOS leaves the group before it (`goto LAB_3844_04ec` at raw 58548, and the win exit at 58500), so a turn that ends the war shows no warn at all; the old `pop_pct < 90` guard on @WARN3 is therefore gone (the @LOSING3 branch takes that turn). `%STRING0` (the human's new-world country, raw 58553) is now set for every warn, not just 1 and 3 — @WARN2 never reads it, but DOS fills it unconditionally. The `ref_already` argument is deleted from the function and `end_checks_armed` from `ai_king_nation_turn`: DOS's only gate is `(*0x5382 & 1) != 0 && (*0x5382 & 8) == 0` (raw 58505), which the head of the function already tests as `ai_king_independence_declared` + `ENDGAME_NONE`, so all three lose branches lost the term too — a WoI whose colonies are all inland now surrenders on @LOSING1 without waiting for a wave to land, as in DOS. The three `unknown46[6]/[7]/[10]` episode latches are kept (port-side; each clears when its own band is left) and are now indexed by the selector. Tests re-premised: the one-coastal-colony fixture asserts @WARN2 **alone** (and that @WARN1 does not join it), and the @WARN3 fixture gained a third human colony so the colonies arm stays quiet. `gcc -fsyntax-only` clean.
6. @MERCENARIES `%STRING1` is DOS's composed unit list ("N Continental Army, Artillery", raw 75029-75041), not the port's single word; @SOONRETIRING0 `%STRING0` has the same hardcoded-"Viceroy" bug fixed at @RETIRING2.

   **RESOLVED 2026-09-10 — composition loop ported; the list is `<qty> Regulars, <extra>`.** The three DS reads in the loop are @UNIT **name pointers**, not literals: `*(type * 0xe + 0x5230)` is the type-table name field (raw 14128 walks it from a unit's +0x06 type byte), so DS:0x5284 = type 6 **Regulars**, DS:0x52a0 = type 8 **Cavalry**, DS:0x52ca = type 11 **Artillery** — the same three the `backup_force` slots 0/1/3 are named after (`k_pool_name`), and `0xe` apart in exactly those steps. The separators resolve out of the string blob too: `FUN_281f_0178 -> FUN_1d1d_07a4(str, 0x50)` appends DS:0x50 `" "` and `FUN_281f_01b4 -> FUN_104b_0032(str, 0x52)` appends DS:0x52 `", "`, with `FUN_281f_0182 -> FUN_104b_012e` the itoa append. So DOS builds `"<qty> Regulars"` then `", Cavalry"` when 0x9e48 is set and `", Artillery"` when 0x9e4c is (raw 75028-75041); 2022's rebel roll sets exactly one of the pair. `ai_king_merc_offer` now composes that string into `%STRING1` (and the fallback body) via `ai_king_merc_unit_name`, which prefers the live NAMES.TXT @UNIT row through `units_find_type`; the port had been naming only the extra, and calling slot 1 "Dragoons" (type 4) instead of Cavalry. Note the offer text and the landing disagree in DOS itself — 43f7_0082 lands Cont. Army / Cont. Cav. for the human at war while the offer names the generic type-table rows — so the wording was not "corrected" to match the spawn. @SOONRETIRING0/1 `%STRING0`: raw 58622 splices `*(0x53a6 * 2 - 0x7c6c)`, the **difficulty title** table (0x53a6 = the difficulty byte), the identical expression @RETIRING2 uses at raw 58643 — both sites now index the Discoverer/Explorer/Conquistador/Governor/Viceroy table, and @SOONRETIRING0's fallback string no longer hardcodes "Viceroy" either (@SOONRETIRING1's body reads only `%STRING1`, but the slot is filled as DOS fills it). `gcc -fsyntax-only` clean.
7. Colony flag 0x10 (small-AI): DOS writer raw 95845-95847 (FUN_5952_035e, pop<10 behind 2a1f_05b4 probability gates) is unported; port readers exist (ai_euro.c:10076).
   **2026-09-10 seventh wave — DECODED, STILL DEFERRED (not a probability gate).**
   `thunk_FUN_2a1f_05b4` is not an RNG roll: it is the RTLink dynalink stub for
   `FUN_5952_0214` (`viceroy_unpacked.c:93686`), the tick's *build-candidate*
   helper, and the decompiler dropped its single register argument.
   `FUN_5952_0214(id)`: if the colony does not already own `id`
   (`FUN_281f_09fc` → `FUN_15eb_038e`) and `FUN_281f_0b8c` → `FUN_15eb_3650(id)`
   says it is buildable, it stores `colony+0x94 = id` and returns **0**; every
   other outcome returns non-zero. `local_4 = 0` is assigned *before* the
   if/else, so the "picked it" path falls through with 0 — which is why every
   call site reads `if (2a6e(...) == 0) goto LAB_5952_274b`: **return 0 means a
   building was chosen and the cascade stops.** A 0 return also clears
   `+0x1c & 0x80`. Sibling stubs, same overlay-thunk table at `ram:1000:a7a2..`
   (each is `PUSH CS; CALLF 110d:0dab; JMPF 0000:<off>`): `2a73` =
   `FUN_5952_0280(chain)` (defence-chain tier probe), `2a7d` = `FUN_5952_02f4(x)`
   → `+0x94 = x + 0x1f`, i.e. the *unit* projects, and `2a82` =
   `FUN_5952_0306(cargo, want)` → the `+0x8d` specialty_cargo writer.
   The `pop < 10 -> +0x1c |= 0x10` store is `LAB_OVL15_L0000__00270d`
   (viceroy_overlays.asm:146147-146150), at the very bottom of a 24-candidate
   build cascade; the ids the decompiler dropped are recovered in the
   seventh-wave lead list below. It is therefore **not** reachable without that
   cascade, and stamping it on any looser condition reproduces smell C3 (the
   flag pinned every turn). Deferred with the cascade, not on its own.
8. Docks-vs-Warehouse first-project pick still keys on `map_tile_is_coastal` (a port invention pinned by test_colonies.c:942 and seed-100 goldens); DOS gates Docks on the +0x1c bit (raw 13688).
   **RESOLVED 2026-09-10 — gate switched to the COASTAL flag.** colony.c:1074 now forks on `slot->colony_flags & COLONIZE_COLONY_FLAG_COASTAL`, the bit stamped 20 lines above from `map_tile_is_open_sea_adjacent`, which is DOS's own Docks buildability predicate (raw 13688: `local_e == 7 && (colony+0x1c & 0x40) == 0 -> reject`). The first-project default itself stays a port heuristic (DOS's found-colony writes +0x8d = 0xff, no project at all), but its Docks/Warehouse fork is no longer a second, looser coastal rule. test_colonies.c:942 re-expressed against `map_tile_is_open_sea_adjacent` so the unit test asks the same question the code does. **Golden impact:** a colony whose only water is a lake, or whose only "water" is off-map at the map edge, now starts on Warehouse instead of Docks — `golden_ai_turns` (New Amsterdam / Quebec / Isabella first projects) and `golden_colony_prod*` are the exposed suites; on a generated map every genuinely ocean-side town keeps the bit and is unchanged. Not run here (no-build rule).
   Residue, filed as a lead below: colony.c:2505's build-list `coastal` (the Docks/Drydock/Shipyard rows) is still the loose probe, i.e. the same DOS test spelled two ways in one file.
9. col1_bridge capture: `no_unit_selected` stamp disagrees with 3 of 7 DOS View-Pieces saves (nus 0, also map_modal_active 0); and a View-Pieces session WITH a unit selected re-saves as map_mode 0 — needs `game->view_pieces_mode` plumbed into `col1_bridge_capture`.

   **RESOLVED 2026-09-10 — `view_pieces_mode` plumbed in and `map_mode` now stamps from it; the `no_unit_selected` half is refuted.** Fixture work first, because both halves of the lead rest on it. `sav_json` still segfaults (top-10 lead 3), so all 60 size-valid saves under `original_saves/` were decoded directly: the head is a packed image and DS:0x5380 lands at **file offset 16**, so `map_mode` (0x5390) is at **+32**, `active_unit` (0x5392) at **+34**, `turn_loop_running` (0x53c2) at **+82**, `map_modal_active` (0x53c4) at **+84** and `no_unit_selected` (0x53c6) at **+86** — cross-checked against the known `+42/+44/+46` tribe/unit/colony counts and the 25-byte `founding_father[]` at DS:0x53a9 (raw 121597 `FUN_1d1d_0dae(0x53a9, 0xffff, 0x19)`).

   **map_mode — real defect, fixed.** 50 saves carry a real active unit; 48 of them are `map_mode 0`, but **two are `map_mode 1`**: `original_saves/COLONY01` (mode 1, active 0x0000) and `french-campaign/COLONY09` (mode 1, active 0x0050). So "View Pieces with a unit still active" is a live DOS state — the View Pieces command at raw 42112 is a bare `0x5390 = 1` that leaves 0x5392 alone, and FUN_2b5a's `0x5390 = 0` (raw 42316) is the rule for the moment a unit is *picked*, not an invariant. Capture's `map_mode = has_active_unit ? 0 : 1` was therefore wrong in both directions (it also claimed View Pieces for any Move-Pieces turn that merely happened to have nothing selected). `col1_bridge_capture` gained a `bool view_pieces_mode` parameter (declared in col1_bridge.h with the two fixtures cited), `game_loop.c` passes `game->view_pieces_mode`, and the stamp is now `map_mode = view_pieces_mode ? 1 : 0`. The round-trip needed its other half too: `game_apply_col1_save` derived `view_pieces_mode = head.map_mode != 0 && selected_id < 0`, and that `&&` was exactly what made COLONY01/COLONY09 lose their mode on a re-save — it is gone, the field is now `head.map_mode != 0` alone. Safe in the port: `view_pieces_mode` has only two readers (`game_end_turn_prompt_active` and the auto-advance in the map pump), both of which also consult `active_awaiting_player`, so mode 1 with a live selection resolves on the first keypress. All 19 test call sites updated to pass `<active id> < 0`, which reproduces the old stamp exactly, so no golden moves; the head-stamp assertions in test_col1_save.c (`map_mode 1` for the `-1` capture, `0` for the `uid` capture) still hold.

   **no_unit_selected — refuted; the stamp stays.** Full tally of the 7 saves with `active_unit 0xffff`: all 7 are `map_mode 1`; `no_unit_selected 1` in three (dutch COLONY01, french COLONY01, french COLONY03) and `0` in **four** (dutch COLONY08/09/10, french COLONY08) — the lead's "3 of 7 carry nus 0" is off by one, and the nus-0 group is actually the majority. But the split is DOS's pump *phase*, not a rule: raw 42309 raises `0x53c6` on the idle pass and conditionally drops `0x53c4`; `FUN_2b5a_3752` (raw 46159) drops `0x53c4` whenever `0x53c6 != 0`; and the input loop's `if (0x829 == '\0') 0x53c6 = 0` (raw 6346) clears it one pass later. That is why every one of the four nus-0 saves *also* carries `map_modal_active 0` — `(nus 1, modal 1)` and `(nus 0, modal 0)` are the same state one pump pass apart. Across all 60 saves the pair `(nus 1, modal 0)` never appears, and neither does `(nus 0, modal 1)` **with** `active_unit 0xffff`. Since this capture must stamp `map_modal_active = 1` for DOS interop (a DOS in-game load restores 0x53c2/0x53c4 into its running main loop — bugs.md port_saves/interop), `(nus 1, modal 1)` is the only internally coherent idle state available, and it is a DOS-observed one. Adopting nus 0 while keeping modal 1 would produce a combination no DOS save contains. Stamp kept as `has_active_unit ? 0 : 1`, with the full tally and the pump-phase reasoning written into the comment at the site.
10. Weak-symbol ban: never add `__attribute__((weak))` fallbacks to a file that is part of colonize_core — in a static-archive link they shadow the real definition whenever nothing else pulls the real .o (bit unit_ai_diplo for `ai_euro_10ec_war_worthy` and the audit-#15 exposure sum). Strong stubs live in ai_contact_link_stubs.c, compiled only by the four slim targets.

# Leads surfaced by the 2026-09-10 fourth fix wave (20-smell batch: F5/F6, G5/G7/G8/G9/G10, H2-H7, I1/I3/I4/I5/I6/I7/I8), unfiled above

1. new_game.c:2089, :2259, :2535 — three more `while (*p == '^')` caret loops with the eat-all bug the popup_msg/pedia unification fixed; should call `popup_msg_caret_flags()`.
   **RESOLVED 2026-09-10 — all three routed through `popup_msg_caret_flags()`.** new_game.c now includes core/popup_msg.h; the two lore collectors take `center = popup_msg_caret_flags(line, &p) != 0` and the build-screen drawer takes `(void)popup_msg_caret_flags(buf, &body)`. Behaviour change is confined to lines with three or more leading carets: the third is now drawn as body text (DOS FUN_6f74_0c32 tests byte 1 then byte 2 and stops), where before the run was swallowed. One or two carets are unchanged, so no shipped GAME.TXT line moves.
2. src/platform/dos_compat/dos_types.h — dead file, zero includes anywhere; carries `typedef uint8_t bool` + `true`/`false` macros that would collide with <stdbool.h> the moment anyone includes it. Delete.
   **RESOLVED 2026-09-10 — deleted.** Pre-delete grep over the whole repo (sources, headers, CMakeLists, *.cmake, compile_commands) found zero references; the only mentions were prose. `src/platform/dos_compat/` keeps `dos_compat.c` alone, so no CMake change. docs/decomp_inventory.md's "typedef stubs live in …" line rewritten to record the deletion and the `typedef uint8_t bool` trap.
3. sound.c `sound_pump_unlocked` returns before the random song pick when `!background_music`; DOS 129f:00fa only stops the idle repoll — with Background Music off + Event Music on, DOS keeps playing randomly-picked songs after a category change, the port goes silent. Explicit tunes now correct either way.
   **RESOLVED 2026-09-10 — background_music re-check removed from the pick path.** asm re-read: 129f:00fa-0106 is the whole gate (`[0xa2] != 0 || [0x9e] != 0`), 129f:010b calls FUN_2059_000a for driver-idle, 129f:011c clears `[0x9e]`, then `CMP [0x94],AX / JL LAB_129f_0134` — the pool pick at 0134 runs with **no** second `[0xa2]` test. So Background Music off + Event Music on still plays one randomly-picked tune per armed `[0x9e]` (armed by FUN_129f_02cc and FUN_129f_0318, which is what `sound_set_bgm` mirrors at sound.c:1050). Port now returns from that branch only for `preview_active`. The category<=0 guard is kept deliberately (documented launch-order reason: no pool until sound_play/sound_set_bgm arms one). No RNG stream involved — the pick uses the sound module's own draw, so goldens are unaffected.
4. units.c Brewster mirror-unit spawn passes `rng = NULL` to `europe_spawn_dock_mirror_unit`, skipping `europe_dock_unit_dos_type`'s Dragoon roll — a DOS roll the port drops on that path.
   **RESOLVED 2026-09-10 — real RNG wired through.** `units_brewster_apply_popup_ex` already receives the shared game stream (game_loop.c:3246 passes `&game->move_rng`, the same stream the G5 pool-refill fix uses); the mirror spawn now forwards that `rng` instead of NULL, matching turn.c:2508's imm==1 sibling path. Draw order follows 4884: `europe_brewster_pick_from_pool_ex` refill roll first, then `europe_dock_unit_dos_type`'s `dos_rng_range(0, difficulty+4)` Dragoon roll. **Expected golden impact:** one extra draw on the shared stream per Brewster pick that lands a Soldiers-profession (0x15) immigrant — only that profession rolls — so any golden that exercises a Brewster event with a Soldier pick shifts downstream RNG. The legacy `units_brewster_apply_popup` wrapper still passes NULL and is now test-only (filed below).
5. Colony popup fonts: dock-orders/eject/message popups draw in the screen font (FONTTINY); DOS-faithful would be FONTINTR for directives without `@smallfont`. Two-line change in colony_screen.c once someone confirms DOS appearance.

# Leads surfaced by the 2026-09-10 seventh fix wave (fourth-wave leads 1-4)

1. new_game.c:1194 — `new_game_draw_markup_line` silently `continue`s on **every** `'^'` it meets mid-line, so a caret that DOS FUN_6f74_0c32 left in the body (the third of a run, or one that is not at column 0) is dropped instead of drawn as a glyph. Same family as the loops fixed this wave, but on the render side; needs a DOS look at whether the renderer ever sees a caret at all before changing.
2. new_game.c:1304 — `while (*p == ' ' || *p == '\t' || *p == '_' || *p == '^')` in the generic line-collector eats an unbounded mixed caret/underscore/space run. DOS consumes at most two carets and treats `_` separately; low impact today (no shipped section hits it) but it is the last eat-all caret loop in the file.
3. units.c:1175 — `units_brewster_apply_popup` (the non-`_ex` wrapper) still hard-codes `rng = NULL`, which now means it skips *both* the pool-refill roll and the mirror-unit Dragoon roll. Only tests/unit/test_turn.c:4698/4716 call it; either give those tests a real stream and delete the wrapper, or document it as a deliberately deterministic test entry point.
4. sound.c:921 — the `category <= 0 && category_applied <= 0` early return in `sound_pump_unlocked` has no DOS counterpart: asm 129f:01a3-020a lets `[0x9a] == 0` fall through `DEC AX / CMP 6 / JA` to LAB_129f_0226 and pick from the default band set at 129f:0147/016e. The guard exists for a real port reason (a map tune starting at launch before the intro queues 0x34), so closing the divergence means finding what DOS actually has in `[0x9a]` at that moment rather than deleting the guard.
5. Europe screens draw every unit badge with `damaged = false`: the dock row (game_loop.c:6608, `europe_dock_display_type_index`), the harbor-lane ship rows (game_loop.c:5596), the lane passenger rows (game_loop.c:5651) and the naval report's Europe-lane cargo (reports.c `NavalRow` fill from `EuropeHarborShip.cargo_types`). None of those paths carries a unit record, so a damaged Artillery waiting on the dock or riding a lane ship draws the undamaged badge corner. Fixing it means plumbing the +0x3148 bit7 alongside `cargo_types`/dock entries — check first whether DOS's Europe screen even runs FUN_112b_01ba's corner logic there.
6. The plain F-key path (game_loop.c) calls `units_order_fortify` directly for ships and only borrows the Anchor *wording*; `units_order_anchor` (units.c:7872) is currently `units_order_fortify` behind a `units_is_sea` gate and ignores its `colonies` argument (`(void)colonies`). If the anchor rule ever gains the harbor requirement that parameter was added for, the keyboard path will silently diverge from the menu path. Either route the keyboard path through `units_order_anchor`, or delete the unused parameter.
7. The naval report never shows a hull's damaged/under-repair state at all (`NavalRow` has no ship damage field; the repair timer lives on the unit). DOS's F7 adviser may mark a damaged ship — worth a golden/DOS check before adding anything.
8. **FUN_43f7_2244 is a HUMAN-turn beat, not an AI one — the port's whole premise for `ai_king_ai_peacetime_gift` is inverted.** DOS's control byte `nation*0x34+0x543f` is **0 for a human-controlled nation**, not for an AI one: FUN_3844_00f2's @KINGFRIGATE takes the interactive `FUN_281f_03fe` CHOICE + the +10% `FUN_291f_0ae0(0xf01, 10)` tax hike on `== '\0'` and auto-accepts (`local_4 = 1`) otherwise (viceroy_unpacked.c:58396-58421), and in the nation loop the `== '\x01'` arm (:6397) is the AI turn (`FUN_281f_0638`) while `== '\x02'` is absent. `FUN_281f_0668` — the only caller of 2244 (:32150-32155) — is invoked from the `== '\0'` arm (:6409-6421). Consequences, all unported: (a) 2244 fires **once per turn on the human's turn**, not once per AI nation; (b) its eligibility test `*0x5398 == iVar4 || FUN_281f_0a38(iVar4, *0x5398) & 0x40` (:75091-75093) reads the **human** nation, and `*0x53d6 = iVar4` names the *seller*, not a beneficiary; (c) the troops land for `*0x5398` (the human) because 10f0 spawns for that global unconditionally (:74308) — there is no beneficiary concept in DOS at all; (d) the payer is `*0x84fc` = the acting player's own record (:75136-75144), i.e. the human's purse; (e) it is a real **@MERCENARIES CHOICE** (`FUN_281f_0652(0x134c, 1)`, accept == 2, :75137) — the peacetime twin of 2022's `0x1340` offer, both mapping to @MERCENARIES per docs/popup_tag_ids.md — not the silent auto-accept the port does. Fixing it moves the call site off `ai.c`'s AI-nation loop into `ai_king_nation_turn`'s peacetime block, changes the payer and recipient, adds a CHOICE popup (whose payload must carry an Artillery count of 0-2, which the shared `ai_king_merc_payload` cannot express — it has only a 1-bit `extra_flag`), and rewrites the seed assumptions in test_ai_king.c's 2244 block. Deliberately left for its own pass; `ai_king_10f0_land`'s `target` parameter exists only to carry the current premise and should collapse back to `ctx->human_nation` when that pass lands.
9. `ai_king_intervention_nation` / `ai_king_intervention_nation_slot` (ai_king.c:412/452) short-circuit their `rival_nation_slot_*` read behind `ai_king_independence_declared`, so on the peacetime 10f0 path (now reachable via 2244) the freshly-stamped slot 2 is ignored and the seller falls back to the colony-count heuristic. DOS's 10f0 reads `*0x53d6` with no WoI condition (:74403). One-line gate removal, but it changes which country the @MERCS line names on the wartime path too if a slot is stale — needs a look at every writer of both slots first.

10. CLOSING.TXT / OPENING.TXT `Delay` column is an **inter-cycle pause** in DOS, not a start-tick
    offset (found while settling I13 by disassembling COLONIZE/CLOSING.EXE; image offsets below,
    load base = file offset 0xa00). The 14-byte series record at 0x4b96 is (+0 series, +2 frame,
    +4 repeats, +6 baseX, +8 delay, +10 active, +12 counter). `_anim_loop` activates a series when
    `[+2] == tick` — the TXT **Frame** alone (0x35a-0x36d) — and uses `[+8]` only at the end of a
    cycle: when the counter passes sprite_count and repeats is still non-zero, a non-zero delay
    deactivates the series and re-arms it at `tick + delay` (0x2f6-0x31e). The port instead folds
    it into the start tick: `closing_start_tick` returns `frame + delay` (closing.c:141-150).
    (OPENING.TXT has no Delay column — four fields only — so opening.c is unaffected.)
    Live on the shipped rows: CLOS-ROC (delay
    100) starts at tick 1 in DOS and 101 in the port, and CLOS-HAT (delay 16) at 1 vs 17; in DOS
    both then *pause* between cycles instead of looping back-to-back, so the whole closing scene's
    rhythm differs. Related detail for whoever ports it: on a delayed re-arm DOS sets the counter
    to 1 *before* deactivating (0x2e0), so the reactivation tick tests counter 1, increments to 2
    and draws 2 — CLOS-HAT's 0x5a cheer (`_do_anims` 0x19b, drawn counter == 1) therefore fires
    only on the *first* cycle of a delayed series, not on later ones.
11. `closing_compose`'s `repeats == 0` row is dropped, where DOS holds the last sprite: the port
    skips the series once `elapsed >= s->repeats * n` (closing.c:173), which for repeats 0 is
    every tick, so such a row would never draw. DOS's wrap path (`_anim_loop` 0x2c4 `jz 0x320`)
    pins the counter at sprite_count and leaves the series active — play once, then hold — which
    is exactly what opening.c:351 already implements for OPENING.TXT. Latent today: every shipped
    @CLOSING row is repeats -1.
12. `platform_create` still hardcodes `width = 320, height = 200` (sdl_runtime.c:107-108) for the
    window, the streaming texture and the RGBA scratch. The I12 fix only stops a larger caller
    framebuffer from overrunning the heap (it now presents the overlap and warns); making the port
    actually present a non-320x200 framebuffer means sizing all three from the framebuffer at
    first present, or recreating the texture when the dimensions change.
13. Side effect of the I10 `settings_is_loaded` fix worth knowing: on the corrupt-settings.json
    path `game_persist_debug_hud` (game_loop.c:2544-2557) no longer writes, so DEBUG HUD toggles
    stop persisting for that session. That is the intended reading — we deliberately leave the bad
    file untouched rather than overwrite the player's edits with defaults — but if we ever want
    those toggles to persist anyway, the fix is a separate "settings file is writable" predicate,
    not re-widening `settings_is_loaded`.

14. `build/debug/sav_json` still segfaults on every `original_saves/**/*.SAV` tried
    (dutch-campaign/COLONY01 as well as the known COLONY02) — it is now useless as
    a fixture-verification tool, which is why this wave's View-Pieces survey decoded
    the head bytes by hand. Head offsets for anyone doing the same: DS:0x5380 sits at
    file offset **16**, so `map_mode` +32, `active_unit` +34, `nation_turn` +36,
    `tribe_count` +42, `unit_count` +44, `colony_count` +46, `turn_loop_running` +82,
    `map_modal_active` +84, `no_unit_selected` +86.
15. Two more copies of the stale `FUN_5fef_0000` citation survive outside units.c:
    `ai_euro.c:16721` ("99190-99196") and `combat_strength.c:541`, which cites 99190
    as the *function* address — `FUN_5fef_0000` opens at viceroy_unpacked.c:99111 and
    its domain gate is 99186-99195. Left alone this wave to avoid touching files other
    agents held.
16. `col1_bridge_capture` now takes `view_pieces_mode`, and `game_apply_col1_save`
    no longer ANDs `head.map_mode` with "nothing selected". That makes
    `view_pieces_mode == true` with `selected_id >= 0` reachable in the port for the
    first time (loading `french-campaign/COLONY09` or `original_saves/COLONY01`).
    Its two readers handle it, but nothing *renders* differently for the combination —
    worth checking against DOS whether View Pieces with a live selection should still
    blink the unit or should show the plain tile cursor.
17. The port has no analogue of DOS's `0x829` input-pump latch, which is what
    decides whether an idle save lands on `(no_unit_selected 1, map_modal_active 1)`
    or the one-pass-later `(0, 0)`. Both appear in the fixtures. If DOS interop ever
    needs the second shape (e.g. a DOS build that reads `0x53c4` before its own pump
    re-arms it), capture would need that latch modelled rather than the constant
    `map_modal_active = 1` it stamps today.
18. colony.c:2505 — the construction list's `coastal` (the Docks / Drydock / Shipyard
    rows, colony.c:2561-2571) is still `map_tile_is_coastal`, while the founding
    first-project fork now uses the DOS predicate, the +0x1c 0x40 bit. Raw 13688 is
    unambiguous that the *list* gate is the bit (building id 7), and Drydock/Shipyard
    inherit it through their Docks prerequisite, so this is a one-line change — but it
    is UI-visible (a lake-side colony would lose the Docks row) and the bit's only
    self-heal is set-only inside `ai_euro_refresh_colony_ai_flags`, which walks AI
    turns; a pre-2026-09-10 save's human colonies may carry a zero bit until that
    runs. Wire the human-side self-heal first, then flip the gate.
19. `colonies_list_eject_roles_ex` / `game_colony_list_outside_roles` still gate the
    Missionary row on Church-or-Cathedral alone. DOS drops row 0x18 only when the
    Church bit is clear AND the body is not already a Missionary (raw 13567-13569,
    `param_1 == 0x18 && FUN_15eb_038e(0x25) == 0 && iVar2 != 0x18 -> return 0`), so a
    Jesuit standing in a churchless colony sees the row in DOS and not in the port.
    Left unported deliberately: both appliers re-test the Church bit and would refuse
    the row, so listing it needs the applier's gate revisited in the same pass.
    Same function's other unported arm: the row 0x13 (Colonist) refusal at raw
    13561-13565 (`cur_prof == 0x18 && *0x8dc6 < 4 && (*0x8dc6 * 0x34 + 0x543f) == 0`)
    — a human-controlled nation cannot un-bless a Missionary from this dialog. Needs
    `0x8dc6` identified before porting.
20. `colony_prod_tick_rebel_accumulators` now applies DOS's non-WoI SoL decay
    (`bells -= sol%/20` when bells < population, audit 2026-09-08 #107). Two knock-ons
    worth a look: the `bells` it decays is the Phase-A stamped word shared with the
    congress tally, so the tally and the dividend now diverge by that term by design
    (DOS diverges the same way — `local_ba` is decayed after `FUN_291f_09f8` has taken
    it, raw 57231 vs 57356) — worth a comment in turn.c beside the tally; and every
    SoL golden should be re-baselined against DOS rather than against the port's
    pre-fix numbers.

# Leads surfaced by the 2026-09-10 seventh fix wave

1. `@WARN%d` has **no latch at all** in DOS: raw 58538-58551 rebuilds and shows the
   selected warn every turn the band holds (the block is reached from the nation
   beat with nothing but the `0x5382` bit test in front of it), and `unknown46[6]`
   `[7]` `[10]` have no writer anywhere in `FUN_3844_0442`. The port keeps them as
   episode latches so the warn nags once per relapse instead of once per turn.
   Deleting them is a one-line change (`ai_king_check_revolution_end`, the
   `warn_byte` guard) but flips a user-visible cadence — wants a DOSBox observation
   before it is made.
2. The peacetime/AI twin `FUN_43f7_2244` (tag 0x134c, raw 75098-75135) composes the
   same `%STRING1` list from a **different head type** — `*0x5268` = type 4
   Dragoons, not type 6 Regulars — rolls its quantity as `rng(1,3)` with a `+1` on
   the second coin flip, can put **two** in the Artillery slot (`if (1 < *0x9e4c)`
   prefixes that slot with its own count, raw 75124-75128) and prices at
   `(difficulty + 4) * 2` against 2022's `+ 3`. `ai_king_ai_peacetime_gift` builds
   no list and does not model the two-Artillery arm.
3. Order of the endgame trio: DOS evaluates **win first** (raw 58473, exiting via
   `goto LAB_3844_0b4a`), then lose, then warn. The port still tests lose, then
   win, then warn. The two conditions are near-disjoint (crown holds no colony vs
   human holds no colony), but `game_options.independence_force` bypasses the win
   gates, so a forced end with the human already colony-less resolves as a LOSS in
   the port and a WIN in DOS.
4. Removing the port-invented `ref_already` arm from the lose group (lead 5 above)
   makes a WoI whose colonies are all inland surrender on the declare beat, which
   is what DOS does but is a state several unit fixtures sit in (all-land test maps
   with `game_options.woi = 1`). `golden_woi_ref01` and the WoI headless driver are
   the suites to re-check first; not run here (no-build rule).
5. **The FUN_5952_035e build cascade, fully decoded — a mechanical port whenever the
   `ai_euro_prefer_*` family is retired.** DOS's candidate order is a chain of
   `FUN_5952_0214(id)` calls whose `id` the decompiler dropped (it arrives in AX);
   every one is recovered from `viceroy_overlays.asm:145593-146167` as the
   `MOV AX,imm` immediately ahead of `CALL FUN_OVL15_L0000__002a6e`. Reading order,
   with the asm label and the guard, `local_*` names per the clean recovery
   (`iStack_138` = max `DS:0x8ea6[job*8] % 4` over expert workers, `uStack_ec` =
   expert head count incl. on-tile units, `iStack_16c` = pop/6, `iStack_142` =
   off-map ring tiles, `uStack_a2` = ring size, `iStack_76` = labor_shortage,
   `iStack_92` = type-0x0b (Artillery) units on the tile, `iStack_2c` = "some
   defence chain is at tier 3"):

   | asm label | guard | id |
   |---|---|---|
   | `22da` | `ring − offmap <= pop \|\| (offmap && local_a0)` | 6 |
   | `22ec` | — | 0 |
   | `22f9` | horses (`+0xaa`) >= 2 | 0x11 |
   | `2312` | `pop < 4` → `goto 2747` (`+0x1d \|= 0x80`) | — |
   | `231f` | `warehouse_level < pop/6 && warehouse_level == 0` | 0x10 |
   | `2347` | `pop >= 6 && ((+0x1b & 3) \|\| armed_ships[me] < armed_ships[human] − 2 \|\| pop > 11)` | 0x12 |
   | `2385` | `!(+0x1c & 0x20) && turn < 0x640 && (continent & 1) && alarm < 0x32 && !ring1_threat` | unit 0xc (`goto 23be`) |
   | `23d0` | `local_138 > 0 && pop + experts > 3` | 0xc |
   | `23f6` | `pop < 6` → `+0x1d \|= 0x80` | — |
   | `2445` | the md:1475-1481 disjunction | 3 |
   | `2453` | `aiStack_68[16] != 0` (a Preacher works here) | 0x25 |
   | `2467` | — | 0x24 |
   | `2475` | — | 1 |
   | `24a9` | md:1482-1484 | 0x28 |
   | `24b7` | `warehouse_level < pop/6` | 0x10 |
   | `24d3` | `DS:0x8dec (bells production) > 0x17` | 0x14 |
   | `24e8` | `DS:0x8dec > 3` | 0x14 |
   | `24fd` | `local_138 > 1 && pop + experts > 9` | 0xd |
   | `2523` | `pop < 8` → `goto 2747` | — |
   | `2530` | `local_138 > 2 && pop + experts > 0xf` | 0xe |
   | `2552` | — | 0x25 |
   | `2560` | `pop > 9` | 2 |
   | `25a3` | `local_2c && (+0x1c & 0x40)` (coastal, maxed defence) | 8 |
   | `2694` | for chain 5..0 where `FUN_5952_0280(chain)` says the tier is short: `FUN_1000_8d90(DS:0x864[chain*4])` then the id it returns | — |
   | `26c4` | — | 0x26 |
   | **`270d`** | **`pop < 10` → `+0x1c \|= 0x10`** (the SMALL_AI writer) | — |
   | `271b` | `local_92 < 3` | 3, then unit 0xb if `DS:0x8de6 (muskets prod) == 0`, else 5 then unit 0xb |
   | `273e` | otherwise `+0x94 = 0xff` | — |
   | `2747` | `+0x1d \|= 0x80` (wants_construction) | — |

   `LAB_23be` is `+0x94 = FUN_5952_02f4(x) = x + 0x1f`, the unit projects. The
   cascade's prologue (md:1394-1440) clears `+0x1d & 0x7f` and sets `+0x94 = 0xff`
   before anything else, so "nothing picked" is a real outcome, not a fallthrough.
6. The four `FUN_15eb_1f72` ledger arrays `colony_craft.c` names as DS bases are also
   read as **scalars** all over the colony tick, and the scalar is always slot 0 or a
   fixed cargo: `DS:0x8dc8` = gross prod[food], `DS:0x8e0a` = demand[food],
   `DS:0x8e32` = shortfall[food] (already cited in `ai_euro.c`), `DS:0x8e5a` =
   unmet[food] (already cited in `turn.c`), and then `DS:0x8dd2` = prod[lumber],
   `DS:0x8dd4` = prod[ore], `DS:0x8de4` = prod[tools], `DS:0x8de6` = prod[muskets],
   `DS:0x8de8` = prod[hammers], `DS:0x8dea` = prod[crosses], `DS:0x8dec` =
   prod[bells], `DS:0x8e64` = unmet[lumber]. The colony record's own aliases fall out
   the same way and are worth a `colony.h` line: `+0x9a` = stock[food], `+0xa4` =
   stock[lumber], `+0xa6` = stock[ore], `+0xaa` = **stock[horses]** (not food — the
   tick's `0x65 < +0xaa` Scout gate and `+0xaa < 0x32` specialty gate are horse
   tests), `+0xb4` = stock[trade goods], `+0xb6` = stock[tools], `+0xb8` =
   stock[muskets].
7. `units_type_has_profession_slot` (units.c:11245) hides the `DS:0x30e` **values**
   behind a bool. Two DOS sites need the value, not the predicate: the 5952 join
   loop (lead above) and anything else that asks "what role does a unit standing on
   a colony tile present as". Worth exposing as `units_type_default_job(type)` with
   the bool as its wrapper.
8. `FUN_5952_0306` (the `2a82` stub) is the real `+0x8d` specialty_cargo writer and
   the tick calls it five times in a row at md:707-741 with
   `(0xf, …), (0xd, …), (8, …), (0xe, …), (0xf, …)`. Its body is three lines:
   clear the want when `colonies_warehouse_capacity() <= stock[cargo]` or when
   `prod[cargo] != 0`, then `+0x8d = cargo` if the want survives, else `+0x8d = 0xff`
   if it currently names this cargo. `ai_euro.c:6610`'s "FUN_5952_0306 thin" note is
   the stand-in; the real thing is small and self-contained, and none of its inputs
   are unresolved.

# FUN_5952_035e building / expert passes (sweep-2 carry-over) — 2026-09-10 seventh wave

**Origin.** Carried over from the sweep-2 audit as "the 5952_035e building/expert
passes are unported"; the standing citations were `colony.h`'s
`COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR` comment ("DOS's own readers are all
inside FUN_5952_035e's later building/expert passes (raw 94422, 94454, 94499,
94751), none of which the port has — so the bit is currently WRITE-ONLY here")
and `COLONIZE_COLONY_AI_NEEDS_MILITARY` ("raw 94247 … that whole loop is
UNPORTED — the port has no 5952 stack-absorption pass at all").

**Read the clean recovery, not the canonical export.** `viceroy_unpacked.c:93790`
is the corrupted Ghidra export (wrong 4-param signature, garbled middle);
`original_sources_annotated/ai/colony_tick_5952_035e.md` lines 128-1706 are the
zero-warning re-disassembly and are canonical. Raw line numbers and md line
numbers drift apart by a growing offset (≈93652 at md:579, ≈94301 at md:1544) —
map each citation individually, never by a fixed delta.

## PORTED this wave — the raw 94751 field-specialist restore pass

`ai_euro.c`, inside `ai_euro_colony_tick_28c8_reassign`, between the two ported
placement passes and the leftovers stand-in (which is where DOS runs it: the
`goto LAB_5952_178f` stop lands on LAB_17a9, and this loop is downstream of it).
DOS body = md:1163-1185. Every still-unplaced slot whose **profession** is one of
the seven raw-goods experts (@JOB 1..7) goes back on the best tile for his own
specialty, unless the colony already holds a warehouse-full of that good:

```
if (is_expert(prof) && prof < 9 && prof != 0 && prof != 8) {
  if (prof == 5) {                        /* Expert Lumberjack */
    if (local_14 == 0) local_14 = 1;      /* the first one is free */
    else if (!(+0x1b & 0x20) && DS:0x8e64 == 0) continue;
  }
  if (colony[0x9a + prof*2] <= local_36) assign(slot, job = prof);
}
```

Resolutions this needed, all new:

- `FUN_1000_8d5e(colony, slot, job)` third argument is not only the −1/−2 modes:
  a real job index confines 28c8's search to that one field job. Ported as a new
  `restrict_job` parameter on the scorer body, now `ai_euro_28c8_score_job`, with
  `ai_euro_28c8_score` kept as the −1 wrapper so no existing call site moved.
- `local_36` = `FUN_1000_8f2a` → `FUN_281f_0d3a` → `FUN_15eb_0a50` =
  `colonies_warehouse_capacity` — one capacity for all goods, not per cargo.
- `is_expert` = `FUN_281f_0c9a` → `FUN_15eb_0002` (viceroy 9298-9307): false for
  @JOB `0x13` (the "Colonist" row), `0x19` Indentured, `0x1a` Criminal, `0x1b`
  Convert, `0x1c` Free Colonist. Added as `ai_euro_5952_job_is_expert`.
- `local_14` is seeded at md:1069 from `(+0x1d & 0x80) == 0`, i.e. a colony that
  wants construction admits its first Lumberjack unconditionally and one that
  does not, does not.
- `DS:0x8e64` = the unmet-after-stock slot for cargo 5 in the `FUN_15eb_1f72`
  ledger array `colony_craft.c` already names (`DS:0x8e5a + 5*2`);
  `FUN_15eb_0b52` records those rows as `stock + production < demand`, and
  `FUN_1000_8df4` refreshes the array after every assign, so it is this colony's
  live number. Re-derived in the port from `colony_prod_colony_hammers`' lumber
  demand vs stock + the ring's Lumberjack yield.

This gives `COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR` (+0x1b bit 0x20) its **first
Linux reader** — `colony.h`'s "WRITE-ONLY here" note is now one quarter stale
(the other three readers, raw 94422/94454/94499, are in the deferred
pioneer-improve pass below).

**Golden impact.** AI colonies only. Raw-goods experts (Master planters, Expert
Fur Trapper / Lumberjack / Ore Miner / Silver Miner) who previously fell through
to the leftovers stand-in and were given the *best-scoring* job now go back on
their own specialty first — so `golden_ai_turns`, `golden_ai_mid01`,
`golden_ai_late01` will move wherever an AI colony holds such an expert who was
not placed by the food or general passes. Nothing human-side changes.

## DEFERRED — the build-decision cascade (raw 94784+, md:1394-1600)

Not blocked on decode any more (see below) but blocked on two decisions above a
single porting pass:

1. It writes `colony+0x94` (the build item) and `+0x1d` bit 0x80. The port
   already picks AI construction through ~15 `ai_euro_prefer_*` heuristics in
   `ai_euro.c`. Wiring the DOS cascade means retiring that family, which is a
   large, golden-moving behaviour swap that cannot be validated under the
   no-build/no-test rule.
2. Several of its guards still need EXE data dumps: `DS:0x864` (6 × 4-byte
   defence-chain rows: id at +0, @JOB at +1, cargo at +2), `DS:0x8ea6` (stride 8,
   indexed by @JOB, `% 4` feeds `local_138`), `DS:0x925b` (stride 0x13 by nation),
   `DS:0x917c`, and the `-0x7b35` / `-0x6bdc` / `-0x6e84` difficulty rows.

Everything else about it is now recovered — see the seventh-wave lead list.

## DEFERRED — raw 94247, the join-colonist (stack absorption) loop, md:579-616

The `+0x1b` 0x04 reader/clear `colony.h` names. Gate is `DS:0x8d72 != 0 &&
(+0x1b & 0x10) != 0`; it walks the virtual slots `pop .. pop + on_tile − 1`
while `pop < 0x20`, repeating until a sweep changes nothing. Newly resolved:

- `FUN_1000_8dfe(slot)` = `FUN_15eb_0e18`: `slot < pop` → `colony[0x20 + slot]`
  (the **job** byte); otherwise `FUN_15eb_0902(unit)` = `DS:0x30e[unit type]`,
  the per-TYPE default @JOB — the port already carries that exact table in
  `units.c:11245` (`{19,21,20,24,23,22,-1,23,-1,21,…}`), reachable only through
  the bool `units_type_has_profession_slot`; it needs a value accessor.
- `FUN_1000_8e44(slot)` = `FUN_15eb_0e52`: `colony[0x40 + slot]` (the
  **profession** byte) or `unit+0x315b`.
- So colony `+0x20..0x33` = job per colonist, `+0x40..0x53` = profession per
  colonist, `+0x70..0x83` = tile → slot.
- `FUN_1000_8e26(slot, job)` = `FUN_15eb_0e8c`, which normalises `0x17 → 0x15`
  on the way in; `0x12` is the tick's "idle / unassigned" job sentinel (also
  what the md:962-968 reset loop writes), not @JOB 18 Teacher.
- `local_42` / `local_3e` are not real locals: `aiStack_68` is memset for 0x32
  bytes (md:568), i.e. 25 ints, and those two "locals" are `aiStack_68[19]` and
  `aiStack_68[21]` — the per-@JOB head-count buckets filled at md:573-577, where
  a non-expert profession is bucketed as `0x13`. So `local_42` = count of
  non-expert colonists and `local_3e` = count of @JOB 0x15 (Soldier) colonists.
  Same aliasing gives `local_4e = aiStack_68[13]` (Carpenter),
  `local_4a = aiStack_68[15]` (Gunsmith), `local_48 = aiStack_68[16]` (Preacher).

The port has `colonies_admit_unit`, so the machinery exists; what stops it is
that the port already admits units into colonies from the unit-act side
(`ai_euro_try_farmer_field_assign` and friends), and a colony-tick admit pass
has to be reconciled with those rather than stacked on top.

## DEFERRED — raw 94330-94560, the colony-tick pioneer-improve pass (md:782-960)

Holds the other three `+0x1b & 0x20` readers (raw 94422, 94454, 94499). This is
**not** the port's `ai_euro_try_pioneer_improve`: DOS scores every ring tile from
the colony record, then *spawns* a unit of type `DS:0x524e` at the winner
(`func_0x00019c10`), sets its `+0x315a = 99`, applies road or plow
(`func_0x000193b2` / `FUN_1000_9406`) and disposes of it (`func_0x00019bf6`) —
a phantom-worker mechanism with no Linux counterpart. Porting it alongside the
port's real walking pioneers would double-improve.

9. RESOLVED as operator error, kept as a warning. In-tree writes to
   `port_saves/campaign3/COLONY08.SAV`/`COLONY09.SAV` (the decade/turn autosave
   slots) appeared twice on 2026-09-10 and were first blamed on the test suite —
   but no test or source names that directory, four clean suite runs (plus three
   with the files chmod-444) never reproduced it, and the diffs were
   autosave-shaped (turn counters advancing). The directory is a live campaign's
   save_dir: the writes were almost certainly a concurrent play session's own
   autosaves, uncorrelated with `ctest`. The session's `git checkout --
   port_saves/` "restores" therefore DISCARDED two real autosaves (turn/decade
   slots; the next end-of-turn re-creates them, so the loss is one autosave
   state). Rule for future sessions: `port_saves/**` is player data, never test
   fixtures — do not restore it from git without asking, and treat unexplained
   diffs there as the player's own saves first.
