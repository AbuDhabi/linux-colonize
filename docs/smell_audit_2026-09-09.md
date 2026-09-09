# Mechanics smell audit — 2026-09-09 (second sweep)

Eight-area Opus sweep (ai_euro, game_loop/turn, units/combat, native/king AI, colony production, save/bridge, Europe/reports, UI/popup/map). Successor to docs/smell_audit_2026-09-08.md (closed); nothing here duplicates that audit. Findings pending user review. Confidence H/M/L. Line numbers as of commit 313e0c4.

## A. Units / combat (units.c, combat_strength.c, combat_analysis.c, unit_stack.c, unit_chrome.c)

1. units.c:5036 (and :5386) — the defender half of the ported 1b0e resolve-handicap group is computed then thrown away. Both resolvers copy back only `er.atk_strength`; `er.def_strength` (the "half-the-nation colony" +`(4−diff)*4` from prior-audit #1 clause (b), combat_strength.c:925) never reaches `total`. test_units.c:2382 calls the helper directly so passes while the live path is dead. H

2. units.c:6730 + :2833 — militia / Paul Revere phantom spawns as a real Free Colonist/Soldiers type and runs the full FUN_5fef_0352 capture/demote/promote path: @COLONISTCAPTURE / @DEMOTE popups DOS never shows, plus an RNG draw for promotion. File's own comment at :6621 says the DOS phantom is scratch type 0x17, despawned silently. M-H

3. combat_strength.c:384 — `combat_engagement_strength` defaults a missing foe to nation 0xf; with `foe_id = -1` the gate at :391 passes only for natives, so European defenders lose the open-terrain bonus in AI scoring. `ai_euro_land_foe_toughness` (ai_euro.c:15924) calls with exactly −1. M

4. units.c:6219 and :3021 — entry-seizure and stack-sweep gates use bare `t->attack == 0`, which :5160 in the same file documents as unfaithful ("attack 0 AND no carried kit"); armed colonist-body (muskets > 0) is seized without a fight though `units_best_defender_at` ranks it armed. Canonical predicate `units_is_combat_role` (:5833) unused at both sites. M

5. units.c:7044-7050 — village-raid arm rolls the 465b MP-overspend gamble that its sibling at :7130-7140 says DOS never applies to an attack ("attack flag ⇒ never denied"), and the denial fires after the fight already drained the dwelling; also burns an extra RNG draw. M

6. units.c:7666 and :7422 — `units_wake` / `units_set_orders` write `moves_left` raw (restore = max, park = 0), inverted for natives under spent-byte semantics. Latent (reached from human/Euro paths only) but no restore-side spent-aware helper exists. Same class: unit_stack.c:177 reads raw `moves_left <= 0`. M-L

7. units.c:2098 — `units_spawn_village_temp_defender` writes `moves_left = 0` on a unit just given a native nation_id = full movement under spent semantics; intended write is `units_mp_exhaust`. Inert today (nothing reads phantom MP). L

8. units.c:4944 — `units_combat_brave_vs_human_arty` matches raw `type_index == 0x13/0x0b`, while combat_strength.c:78-81 says name-match is the family rule because pool index ≠ DOS @UNIT id in synthetic fixtures; the auto-loss silently no-ops on non-stock rosters. M-L

9. units.c:5691 vs :3282 — repair-timer "winner strength" fed from two incompatible scales: naval loss uses `wt->defense` (1-12), fort fire uses `attack_str << 1` (≥8, typically 16-32), so `worked = thresh − wstr` is always 0 on the fort branch — every fort-damaged ship gets maximum repair time and the `<< 1` is dead. M-L

10. combat_strength.c:588-598 — uncited fallback SoL `liberty_bells_total / 4` in `combat_colony_sol_at` when no colony record matches the tile, feeding the WoI Tory/Rebel peel at :749; divider 4 appears nowhere else in SoL machinery. M-L

11. combat_strength.c:1012 — `combat_naval_engage` runs the shared 1b0e peels, so attacker-fatigue `* rem / 3` applies at sea (:663-671, no domain guard) while combat.md's naval row lists no fatigue; ships have 12-18 thirds so `rem < 3` is reachable. Either doc or missing gate wrong. M-L

12. combat_strength.c:506 vs units.c:5833 — two near-identically named "is combat unit" predicates with different answers (`attack > 0` vs `muskets||horses||attack`), plus a third open-coded copy at units.c:1981; callers pick by name similarity with no doc saying which question each answers. L-M

13. units.c:1978 — best-defender domain gate tests attacker's own ship-ness where its own comment (and FUN_281f_0768) specify the attacker *tile's* water test; diverges exactly when a ship attacks from a colony berth. L

14. unit_chrome.c:49 and :238 — `g_chrome_crown_nation` declared twice at file scope (tentative + real definition); read at :107 before the `= -1` initializer appears in the text. Also :315 derives crown Sentry/Fortified letter as `15 − 8 = 7`, not any documented REF shade. L

## B. game_loop.c / turn.c

15. game_loop.c:12291 (with :16157, :11838) — new-game wizard is in no "screen owns the display" test: `game_modal_open`, `screen_over_map`, `game_screen_name` all omit it and `in_menu` is false, so the previous campaign keeps simulating underneath (goto pacer, Land Ho/LCR/combat watches, Europe auto-open). Reachable via Esc-to-title from a live game → "New World". Violates the popup-blocking invariant. H

16. game_loop.c:9908 — `game_ship_sail_to_europe` has no WoI gate and 4 of 5 callers supply none (only MAP_MENU_ACTION_RETURN_EUROPE checks at :11353; `H` handler :14360 and trade-route sail :10759 are bare). Ship lands in harbor; `game_finish_end_turn:10196` refuses to open Europe — ship permanently lost. DOS shows @EUROPENOTLEAVE under DS:0x5382 bit0 (move_enter.md:151-154; :7828 already ports it for @SAILHOME). H

17. turn.c:3053-3082 — Section D rival-SoL chrome drops both DOS hysteresis bands (rising `last < v && v >= local_6−0x14`, falling `v < last−5`, viceroy 58571/58584) and the once-only latch (`0x84fc & 4`), so "Rival declares war." + `ai_diplo_declare_war` re-fire every year on a 1-point wobble; also visits only 2 cached rival slots where DOS loops all 4. M-H

18. game_loop.c:11638 — `game_service_woodcut` parks only for `in_menu`; a woodcut queued mid-EOT (turn.c:2343, ai_contact.c:8429/8439) seizes the screen on top of colony/Europe/report/pedia (sibling `game_service_bar_message` parks for the full screen set at :11682). Also EOT services woodcut before bar line, idle path the reverse. M-H

19. game_loop.c:10766 — Europe lane-full fallback rebuilds the ship via `units_spawn_ship_with_cargo` (fresh unit, no orders/follow/goto), so `game_trade_route_retarget` gets a dead or recycled id and the trade route dies permanently with only "Europe lane is full" as feedback. M-H

20. game_loop.c:12498-12522 — colony-screen Escape cascade closes five sub-panels but forgets `custom_house_open`; missing from the Enter cascade too. Self-heals on re-entry. M-H

21. turn.c:3030 — Section C2 gated on `crown_colonies > 0`, invented; DOS runs C2 whenever `(0x5382&1) && !(0x5382&8)` (rebel-colony loop sits outside the C1 guard, viceroy 58507). Crown wiped on land but fat at sea ⇒ port emits no peace-offer/pressure chrome. M

22. turn.c:126-133 — `turn_refresh_moves_for_nation` re-derives human nation with a first-`control==0` scan (the exact idiom `col1_save_human_nation` replaced after bugs.md 288) and overwrites the authoritative `units_set_combat_human_nation(ctx->human_nation)` set at :3283. M

23. game_loop.c:10678-10699 vs :10839-10851 — two copies of the same cited FUN_479b_0bd0 sell-then-buy diverged; the in-place copy fires on a high-seas map tile (behavior move_enter.md:157 says was retired), stops at first failed sale where the harbor copy sweeps all slots, and gates on a bare `x >= 200 || y >= 200` sentinel. M

24. turn.c:3694 — KING slice hands control over via bare `turn_select_next_unit` (no standing-order skip), the idiom prior-audit #34 factored away; `game_select_next_unit_awaiting_orders` (:10323) exists and is used at all four game_loop sites. Stale comment at :10166/:10170 says FINISH does this — it moved to KING. M

25. game_loop.c:9948 — voyage-length roll taken after the departing ship is despawned, over `units_count_sea_for_nation` which walks only the live pool; DOS counts harbor/expected/bound ships at sentinel coords 228/232/244+n, so a three-ship player can never hit the `ship_count > 2` slow-voyage gate. Third spelling at turn.c:2714 hardcodes count 1 while still burning the draw. M

26. game_loop.c:10862 — automatic trade-route departure calls `europe_set_sail_from_harbor`, which unconditionally boards sentried dock immigrants (`europe_board_sentry_dockers`, europe.c:2127); no prompt, no DOS citation (site cites 0bd0 for cargo only), runs mid-EOT. M

27. game_loop.c:11892 — popup-presentation gate excludes pedia/reports but not exploits, Hall of Fame, or debug atlas; those screens skip `ai_popup_render` (:15048/:15056) while `game_handle_modal_input` (:12337) still runs first — same invisible-popup-swallows-keys shape, live during the retire chain. M

28. game_loop.c:13642 — dead duplicate `S` handler in the Europe branch (unreachable after :13475's unconditional return; would sail harbor ship 0 on no selection). Same family: map-only `S`→Save (:14545) fires whenever ORDERS Sentry is disabled, and :14537 excludes sea units from Sentry, so S with a ship selected opens Save dialog. M

29. turn.c:355-368 — inefficient-government latch written only for human colonies (DOS FUN at viceroy 57470 ORs/clears bit3 for every colony of the ticked nation), contains a dead re-test of `control == 0`, and `COLONIZE_COLONY_FLAG_INEFFICIENT_GOV` is read by nothing. M-L

30. Minor trio: turn.c:3448 `want_eu && n == human` unreachable (EURO never runs for human); turn.c:3556/3584 FINISH `show_indicator` set-then-cleared dead assignment while turn.h:107 and turn_between_players.md three-way disagree; original_sources_annotated/turn/year_end_chrome.md "Victory fleet (C1)" prose inverts the quoted expression (code right, doc wrong; doc's `warships` also counts land units). L

## C. ai_euro.c (+ ai_goals.c)

31. ai_euro.c:1383-1388 — NEEDS_COLONISTS (+0x1b bit 0x10) latched from uncited `population < 3`; DOS (colony_tick_5952_035e.md:557-563) latches from pop < the 8/12/32 fortification capacity the same file already ports twice (:11648, :16803). Thin definition gates the DOS-ported consumers (labor arm :11644, colony-sail :16818, 5d04 cb :7601). H

32. ai_euro.c:7194-7208 — `ai_euro_5d04_dos_type_of` tests "Cav" before "Cont": Cont. Cav.→8, Cont. Army→7; NAMES.TXT @UNIT order is 6 Regulars / 7 Cont. Cav. / 8 Cavalry / 9 Cont. Army, and sibling `ai_euro_20e6_dos_type` (:10929) encodes that correctly. Two @UNIT tables in one file disagree on WoI rows. H

33. ai_euro.c:8118-8120 — DOS @UNIT code from `cb_unit_dispatch_byte` passed to `units_type()` as a Linux pool index (every other consumer translates via `ai_euro_5d04_linux_type_for`); wrong `space` subtracted from the recruit-buy hull budget. M-H

34. ai_euro.c:9832 vs :10217 — 0a60 work-queue "+1500 exposed combat unit" arm reads `ai_euro_continent_stance_at` ~380 lines before the only planning-phase refresh; sees last turn's table, all-zero on turn 1 (arm fires for every armed unit in every colony). M-H

35. ai_euro.c:6325-6327 — `ai_euro_unit_inventory` decrements `muskets_short` per Pioneer, uncited; the DOS mechanic of this shape (raw 93168-93170, ported at :7576-7586) decrements the TOOLS demand counter 0xa0da. M

36. ai_euro.c:5107 and :8658 — two hold counters treat the 255 empty-hold sentinel as occupied (`> 0` without `< 255`), unlike 8 sibling sites; under-counted free holds feed the 4393 queue-decrement tail and ship-hold budget. `0a60_holds_occupied` also scans COLONIZE_UNIT_CARGO_MAX not `units_goods_hold_count`. M

37. ai_euro.c:19919-19936 — tail "sticky CONTACT re-hunt" lacks the at-war and hunter gates its comment claims and its sibling at :19904 has; with #106's peace-permissive foe picker this opens @SNEAK wars from any land unit at end of act. M

38. ai_euro.c:1389-1393 — NEEDS_GARRISON (bit 0x40) derived from `garrison_quota` while DOS sets it from the labor_shortage formula :9414 admits is unported; silent substitution drives colony-sail +0x3c and wander +10. M

39. ai_euro.c:6592-6600 — AI conjures 10 TOOLS per act into any own colony a Pioneer stands in (cap 100); "stand-in", no DOS citation, fires every dispatcher pass (:19781). M

40. ai_euro.c:18072-18087 — generic Pioneer→Soldier conversion cited only to a golden save, unguarded (neighbours in the same loop carry literal seed-100 offsets); every AI Pioneer entering a colony with ≥50 muskets is re-typed. Only site parking a goto at (0,0). M

41. colony.h:239-241 + ai_euro.c:12179/16835 — +0x1b bits 0x04/0x08 read but never written or cleared in the port; DOS clears the byte each colony tick (`&= 7`) before recompute, port clears only 0x80/0x10/0x40 — DOS-imported values frozen for the whole game while feeding live scoring. M

42. ai_euro.c:4002-4014 — 28c8 "Leftovers" pass contradicts its own comment ("keep what they had") by assigning the newly scored tile/job, and skips the `yield < 3` stop both real passes enforce. M-L

43. Stale comments: ai_euro.c:1187-1195 doc block describes `colony_wants_construction_labor` but sits above `type_is_man_o_war_name`; :16756 header still says +0x19/−0x25 (code is −0x19 after prior-audit #95); :3816 warehouse `pop_cap` special case arithmetically identical to general case (dead branch implying an exception). L

## D. Native / King AI (ai_contact.c, ai.c, ai_king.c, ai_diplo.c, ai_popup.c)

44. COL1_INDIAN_WAR_BIT (0x02) — six production readers (ai_contact.c:7840, ai_diplo.c:545/3121/3129, ai_euro.c:254/9298, units.c:3650), zero production writers (only clears at ai_diplo.c:3773); every gate permanently false in a Linux-started game — e.g. raid vent's at-war no-discharge exception never fires, so raids always cool alarm 4-16. col1_save.h:616 admits the bit is never set. H

45. ai.c:3115-3119 vs :3131-3138 — 152e mission arm decrements tribe attitude as an 8-bit byte (`friction` only) while the adjacent threat arm maintains the DOS int16 word (friction+attacks, clamps at 0x7fff); once the word passes 255 a mission can never reduce it (friction bottoms at 0, high byte untouched). DOS writes the full int both times (viceroy 81490-81496). H

46. ai_contact.c:9545/9792/9793/9876 + ai.c:3143/3149/3157 — DOS FUN_4cc6_00f2 ported as two functions and the cited sites call the wrong one: `ai_diplo_indian_alarm_delta` = first half only, `ai_contact_alarm_delta_00f2` = + escalation tail (alarm 100 at peace → RNG → mission expel + @INDIANBURN). The 152e accumulator (DOS's sole alarm-growth channel, whose raw thunks straight to 00f2) calls the bare writer — no mission burn at 100, RNG stream shift. :4602 asserts the opposite invariant. H

47. ai_diplo.c:977-983 — `ai_diplo_declare_war` charges uncited −100 gold and +1 `tax_rate` to both sides (human included) on first declare; writes `tax_rate` directly, bypassing `ai_king_audience_apply_delta` and the `europe->tax_percent` mirror (stale Europe screen until next audience). Sibling in the same block already retired as fandom invention. M-H

48. ai_contact.c:4589-4594 and :4367-4378 — two surviving negative alarm drips of the class retired around them (per-village −1 per Indian turn under 40; −2 on mission meet cited "Source: fandom"), double-counting DOS's mission term inside 152e; both write `alarm_by_player` raw, skipping the delta helper's war-clear and DS:0x54f6 tier update. M-H

49. ai_diplo.c:738-752 — invented −2 gold/turn "harassment" drain on any Euro at war with any tribe (human included), in a function whose other two arms are now empty stubs marked "no DOS counterpart". M

50. ai_contact.c:1251-1252 — alarm floor of 80 after the +100 delta stomps the France/Pocahontas halving (ai_diplo.c:3746-3752, DOS 80844-80850); the sibling it claims to copy floors tribe friction, not `alarm_by_player`. M

51. ai_diplo.c:3236-3279 — `ai_diplo_military_score` is an invented blend (attack+defense, +3 naval, pop*2, +5 fort, gold/50, +(3−rank)*2) driving Euro war/peace in `ai_diplo_euro_balance`, while every sibling reads the real DS:0x941c mirror `stuff.land_combat_strength[]`; also double-counts power via rank. M

52. ai_diplo.c:603 — `indian_hostility_sticky` stand-in written every turn into real DOS save byte nation+0x48, which col1_save.h:634-642 maps as the King's grace/waiver counter; ~10 ai_euro.c gates depend on it; on a DOS-authored save the first read is the wrong quantity. Same repurposing pattern prior-audit #72 retired. M

53. ai_contact.c:1710-1723 — `ai_contact_clamp_alarms` bands the mirror at 200 while both accessors and every DOS site clamp 0..100; raw readers (:2112 pair_friction seed, :7447 raid gate) see 101..200 while accessor paths see 100 — band tests in one file disagree about the same pair. M-L

54. ai_contact.c:2147 (also :2751, :2824) — dead disjunct: `pair_friction` already seeds from `alarm_by_player`, so `|| alarm_by_player >= 55` can never decide; gift gate's `friction>=55 || alarm>=55 || friction>=40` reduces to the last term. L

55. ai.c:2673 — comment on fresh Brave spawn inverts the port's native MP polarity ("created spent, acts next turn" — 0 means fresh/acts now per turn.c:186). Behavior matches DOS; comment is a trap on the inversion axis. L

56. ai_diplo.c:3482 — `uint16_t gold_before_upkeep` truncates uint32_t gold; 65536/131072 read 0, suppressing the war-upkeep status line. L

57. ai_diplo.h:256/259 + ai_contact.c:628 + ai_diplo.c:501 — stale at-war band comments surviving prior-audit #79 (header still says relation < 50; live constant is 26). L

58. ai_contact.c:5385 — `static AiContactReparations s_reparations[4]` has no reset hook (siblings have `ai_goals_reset` etc.); a pending offer survives new-game/load and its resolve indexes `col1->tribe[]` of a different game. L

59. Noted, no divergence yet: two different Jesuit predicates (:1760 vs :9872) set the same mission bit; FUN_15dc_00a2 quartile bucketer exists in four identical copies (ai.c:2515, ai_contact.c:8808, ai_diplo.c:3616, ai_popup.c:951). Refuted on inspection: 0x17/0x18 FF doubles split is DOS-real; mission-establish double-halving is DOS's own. DOC

## E. Colony production cluster

60. turn.c:705 (+ colony_preview.c:139, colony_screen.c:1635, :3901) — Hudson fur ×2 applied outside `colony_yield_pipeline`, i.e. after Convert +1 and after negative-SoL subtraction; DOS (viceroy 11970-11973) doubles between the improvement stack and both mods: DOS = `2·base + 1` / `2·base − 2`, port = `2·(base+1)` / `2·(base−2)`. Same design flaw: AI work-plot scorer (ai_euro.c:3843) omits Hudson entirely because the doubling lives at four call sites, not the shared pipeline. H

61. colony_yield.c:363-443 — DOS Silver-Miner-without-deposit collapse branch (viceroy 11925-11940: no resource + suppress bit clear ⇒ yield forced to 0/1 and improvement stack suppressed) unported; port pays full base + expert ×2 + road/river stack on bare mountain. MAP_LAYER2_SUPPRESS already exists as input. H

62. turn.c:1321/:1364-1365 (and :2196 via nation ticks) — craft, hammers and bells/crosses tally read `colony_prod_sol_bonus()` after the same tick's Phase C/D SoL update at :1025-1027, while field yields (:672) and the whole preview read before; violates the "one number, two consumers" invariant asserted at colony_production.c:419-433 and makes preview structurally unable to match the tick on a latch-crossing turn. DOS composes all yields in Phase A before the accumulators. H

63. turn.c:1217/:1295/:919 vs :1321/:1365 — education/skill-discovery and starve-kill mutate professions/colonist_count before craft+hammers run: one tick uses two profession snapshots (graduate produces at new rate same tick; starved Blacksmith's tools vanish retroactively). DOS Phase A composes before F/G/H/J. M-H

64. colony_production.c:369-379 — `colony_prod_crown_nation` re-derives crown as "peer of human slot" (only ever 0/1), ignoring `head.crown_nation_id` (real values 0..3; dutch-reports.SAV has 2); the WoI bells-negation at :441-443 fires on the wrong nation's colonies whenever the crown slot is 2/3. combat_strength.c:277 carries the same stand-in. M-H

65. colony_screen.c:1604 — settlement-view badge loop lacks the `worked_colonist[32]` dedupe both turn.c:655-662 and colony_preview.c:106-113 apply; a colonist on two `tiles[]` slots renders two badges. M

66. colony_yield.c:668 — town-commons plow term gated `pedia >= 0 && <= 7`; DOS FUN_15eb_1f72 (viceroy 12525-12529) applies unconditionally. Currently unreachable divergence; uncited local invention. L-M

67. colony_yield.c:639/:684 — invented and dead `timber` exclusion on commons food resource bonus (res 10/11 can't satisfy the inner test anyway; trailing `(void)timber` at :726 confirms half-retired). L

68. colony_preview.c:12/:25 — `colony_preview_best_job` / `second_job` have zero callers in src or tests; encode a job ranking nothing validates. L

69. colony_yield.c:620/:708 — `sol_bonus` parameter fully dead (`(void)sol_bonus`) yet all three call sites compute `colony_prod_sol_bonus_field()` to pass it; live-looking parameter invites re-wiring the exact bug prior-audit #18 removed. L

70. colony.h:266 — COLONIZE_COLONY_FLAG_BUILD_COMPLETE 0x80 defined, never read/written; DOS sets it at all three completion arms of FUN_364b_0114 and reads it in colony-screen chrome; Col1 export currently loses the bit. L

71. colony_craft.c:331 — `colony_craft_preview` writes stock unclamped where the live tick uses `colony_craft_clamp`. L

## F. Save / bridge (savegame.c, col1_save.c, col1_bridge.c, col1_post_map.c, col1_stuff_census.c)

72. col1_bridge.c:1316 — OOB read of `trade_route[12]`: route slot from a 4-bit nibble (0..15) indexes a 12-element array unchecked. Reachable: capture writes profession UNITS_JOB_NONE (0x1c, low nibble 12) for a trade-route unit with invalid follow id (:2351 vs guarded :2365). Sibling Europe-lane decoder bounds-checks (:126). H

73. col1_bridge.c:1184 — human ship docked in Europe imports all 6 cargo slots ignoring `holds_occupied` (Bound/Expected path :1101 and map-unit path :1377 both honor it); reintroduces the phantom-goods stale-slot bug savegame.md:164-172 documents as fixed. H

74. col1_stuff_census.c:93-105 — three census counters computed by invented rules feeding live King/diplomacy math: port counts units / sums attack+defense / gates on aboard-ship, atlas says 0x9180 = Σ FUN_281f_09c8(mode 0) over every non-ship unit (value sum), 0x941c = Σ mode-1 value (base×8 + vet/Drake/damage), 0x942c = land units not in colony and not orders A/G. Consumers: ai_king.c:3887 (`14L * strength`), :3913, ai_diplo.c:1558 — ×8 scale difference is live. M-H

75. col1_bridge.c:2299-2304 — exhausted Euro land units export `moves_spent = 0` (full refund on save+reload); branch above exports real spent for partly-moved units. Golden evidence came from AI-turn fixtures where the day had ended; human saves are mid-Move-Pieces. M-H

76. col1_save.c:77 — `col1_save_human_nation` failure path returns `head.human_player` unvalidated (uint16, up to 65535); col1_bridge.c:1484 indexes `nation[]` with it unclamped. The near-identical probe in savegame.c:154-164 never returns unchecked hp — two copies diverge exactly on the failure path. M

77. col1_bridge.c:726-741 — comments (and save_format_map.md:292) call density bit 0x04 "village/capital occupancy" while struct, writer and constant all treat it as suppress (col1_save.h:963, MAP_LAYER2_SUPPRESS, :592-596); village occupancy actually 0x02 (:567). Code self-consistent, prose invites a corrupting "fix". M

78. col1_bridge.c:1847-1848 + :2747 — head-stamp coverage partial: capture forces turn_loop_running/map_modal_active and `active_unit = 0xffff`, never stamps `map_mode` (DS:0x5390) or `no_unit_selected` (DS:0x53c6), so port saves can carry contradictory idle-state fields. M-L

79. col1_bridge.c:1799-1802 — capture writes cursor tile into both focus pair (stuff.x/y) and camera pair (viewport_x/y), conflating two DS words; import prefers viewport as cursor; runtime keeps them separate — camera position silently discarded on every save. M-L

80. col1_bridge.c:376 — buildings encoder unconditionally zeroes `capitol` (+ unused05 via memset) while the decoder honours two capitol bits (:298); lossy RMW for any save violating the "0 in every real save" assumption. L-M

81. col1_bridge.c:279-282 vs :362 — `town_hall` decodes as any-bit-set but encodes only bit 0; upper bits round-trip to 1, same lossy shape as the old popcount-tiers bug. L

82. savegame.c:142 / save_load_dialog.c:44 — slot probe validates only the 8-byte signature (no sig_eof / save_version, unlike `col1_save_validate_head` and DOS FUN_75c2_0840 it claims to mirror), so unloadable files list as selectable slots; slots 8/9 never labeled as autosaves though savegame.md:136 says they are. L

## G. Europe / trade / reports / FF

83. europe.c:3746-3783 — `europe_buy_cargo` ignores ship hold capacity entirely (no `europe_ship_cargo_cap`, unlike passenger path :2126); every harbor ship treated as 6 holds — Caravel loads 600 tons. All four call sites pass no cap. H

84. europe.c:3615 — AI colony dump-sell applies tax DOS never charges and drops the withheld amount (DOS non-human arm of FUN_364b_0688, viceroy ~57816-57848: gross×price to gold, no tax, no royal_money write; the Custom House arm 500 lines earlier does tax and the port mirrors that one correctly). Secondary: gross uses `euro_price − 1` where DOS uses the raw byte. M-H

85. reports.c:3440 — Indian Adviser gate `euro_diplo != 0` where DOS FUN_3f41_010a tests bit 0x20 OR extinct (0x80): port lists never-met tribes carrying stray bits and drops extinct tribes DOS unconditionally lists (the same function reads `ind->extinct` 60 lines later). File's own header quotes the `& 0x20` form. M-H

86. reports.c:3646-3676 vs :4189/:1462 — two FF-ownership tests on one screen: Score's Congress points fall back to `founding_father_count` then head-equality (the reading prior-audit #83 removed elsewhere), while every FF name list is bitmask-only; plate can disagree with its own grid. M

87. reports.c:1235 — Congress page 1 draws "<Ally> Intervention:" whenever WoI is on; DOS draws it only before REF arrival (`0x5382&2` clear) and an empty header after (viceroy 69672-69692). Port also invents an ally fallback search where DOS reads DS:0x53d4 raw. M

88. europe.c:3864-3866 — uncited `k_value[]` table zeroes Lumber/Horses/Tools/Muskets, so the "U = unload whole cargo" key silently refuses them and game_loop.c:13454's loop (whose comment says "unloads the WHOLE cargo") breaks early. M

89. reports.c:3375-3381/:3422/:3540 — Indian chief-portrait comments contradict code and reports.md (say "always #113 pending golden"; code does 113+quartile, resolved in T5.3); leftover base-114 guard makes q==0 bypass the sprite-count bound. L-M

90. reports.c:1643-1645/:1859-1861 — labor report falls back from bad profession byte to `u->type` — a different id space (Scout → Expert Lumberjacks); sibling Score collector skips instead and a real translation table exists at :3587. L-M

91. reports.c:2751/:2813 — Naval report goods scan omits the 255 empty-hold sentinel guard every europe.c consumer applies; sentinel hold paints a phantom cargo icon on F7. L

92. trade_screen.c:107-124 — click in the dead gap left of a cargo column's first icon (mx 115-124 / 198-207) returns slot 0 → CARGO_REMOVE: stray click deletes cargo slot 0. L

93. europe.c:583-626 — recruit-pool seed comments off by one difficulty rank vs code and DOS (expert force is `< 3` = below Governor; easy opener Discoverer/Explorer, nothing at Conquistador). Code right, prose wrong. L

94. declaration.c:163-166 — comment says "DOS aborts the whole animation on failed load" above a `continue` that does the opposite and leaves x/y un-advanced (missing DEC-*.SS closes up the signature). L

## H. UI / popup / map / sound

95. pedia.c:1255-1264 (+ :1016, :1157) — caret rule inverted vs DOS: FUN_6f74_0c32 sets flag 1 for `^^` (centered) and 2 for `^` (own-line, LEFT-aligned); pedia consumes all carets and substitutes a brace test. PEDIA.TXT has zero `^^` and 164 `^{…}` lines ⇒ 164 headings centered that DOS draws left. popup_msg.h:32-38 states the rule correctly; assets.md:344 repeats the wrong one. H

96. map.c:963-988 — `map_fog_edge_fill_sprite_at` omits the ocean-neighbour rescan/skip its mirror `map_fog_reveal_fill_for` (:1001-1031) applies; DOS gate is `param_2 == 0` and the skip fires regardless of visibility, so seen→fog and fog→seen edges disagree; `map_fog_edge_count`/`_mask_sprite_at` never apply the skip either. M-H

97. map.c:1894-1899 and :1968-1973 — uncited "destination road/river halves the cost" rule absent from the DOS-cited sibling `map_move_spent_thirds`; `map_move_cost_step` also mixes `map_tile_has_road_art` (pair test) with `map_tile_has_road` (halving). test_units.c:3747 bakes the halved value in. M

98. map.c:533-534 vs :605-606 — `map_resource_type_at_ex` dropped the +1 coord bias (player-confirmed wrong) but `map_procedural_rumour_at`, hashing the same seed 70 lines later, still adds +1/+1 with no note; rumours off by one tile if the same conclusion applies. M

99. pick_music.c:117-119 — `@smallfont` parsed and stored, never read; Pick Music dialog always renders big font though all four GAME.TXT menus carry @smallfont and game_loop.c:14663 honours it elsewhere. H dead-flag / M visual.

100. map_panel.c:584 — fog-view sentinel leaks: `game_fog_nation()` returns −1 under Complete Map, so `map_panel_draw_tribe_chrome` bails and village alarm/mission chrome vanishes while villages still draw; DOS keys the marks off DS:0x5398, not the view nation. M

101. map_menu.c:994 — `pedia == 27` duplicates `pedia == 0x1b` (mixed-radix confusion; comment treats hex and decimal as different numberings); Arctic (24) hide-Clear/Plow clause uncited. M

102. sound.c:794-807 — 1-in-8 pool override dead for every real BGM category (unconditionally overwritten by the category switch); either belongs after the switch or is pure RNG burn. `sound_pick_rand` uses glibc LCG, not dos_rng. M/L

103. map.c:41-44 — four dead sprite-base constants (PHYS0_COAST_FRAG_BASE etc.); collectors hardcode the equivalent literals — exactly the −1 bookkeeping class the fog PHYS0#148 trap lives in. L

104. map_gen.c:1913-1929 vs map.c:443 — two coastal predicates, 4-neighbour vs 8-neighbour water test; mitigated (fallback-only path) but undocumented at the divergent site. L

105. popup.c:335/:290 — latent markup-helper limits: `char run[256]` truncation vs 512-byte bodies; `popup_markup_text_width` strips braces but not LINE/CENTER marks (safe only because ai_popup strips first). L

106. woodcut.c:105 — dead `case WOODCUT_A_NEW_WORLD` (id 0 unreachable: `woodcut_fire` rejects id ≤ 0; once-only bit is 1-based). L
