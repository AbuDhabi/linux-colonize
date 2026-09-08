# Mechanics smell audit — 2026-09-08

Eight-subsystem sweep for bug-smells (counter-intuitive logic, doc/DOS contradictions, invented mechanics, asymmetries). Findings pending user review; items marked FIXED are done, rest open. Confidence: H/M/L. Items marked DOC are documentation-only.

**FIXED 2026-09-08 (batch 1):** cross-cutting inversion, combat #1, #2, #7, Europe #51, FF #82. Details per item below. ctest 60/60 + goldens green after.

## Cross-cutting: native moves_left semantics inversion (H) — FIXED: `units_mp_charge`/`units_mp_exhaust` spent-aware writers in units.c; all listed sites converted to `units_remaining_mp`/helpers (try_move gate+charge+shore, village-raid branch, win/loss drains, mounted spend-all, afford gate, goto gates, ai_contact raid+escort gates). ~40 test fixtures in test_ai_contact.c + 2 in test_units.c flipped to spent semantics (they were written against the inverted gates).

For native nations `moves_left` holds the DOS **spent** byte (turn.c:179 refreshes natives to 0 = unspent; `units_remaining_mp` units.c:6292 is the safe accessor). Multiple sites read/write it as "remaining":

- units.c:6589 — `units_try_move` rejects `moves_left <= 0`: fresh Brave can never move via this path, exhausted one can.
- units.c:6963 — charges `remaining - cost`, i.e. *refunds* a native's spent count.
- units.c:4738 — `units_mounted_attack_spend_all` writes `moves_left = 0` for Mtd. Braves/Warriors = full refresh instead of drain. Same inversion in post-combat drains units.c:6833, 6883.
- units.c:6353, 8654, 8702 — affordability/goto gates read raw `moves_left`, inverted for natives.
- ai_contact.c:8066, 8086 — raid loop skips Braves `moves_left <= 0`, escorts `> 0`: both inverted (unmoved excluded, exhausted admitted).
- ai_contact.c:8712, 8207 — alarm≥70 approach march + post-ambush advance go through `units_try_move`, dead at turn start under spent semantics.

## Combat

1. combat_strength.c:817-819 — two DOS colony-tile resolve peels unported (raw 100557-100564): (a) last-colony shield — native attacker strength zeroed vs a European's only colony; (b) human defender +`(4−difficulty)*4` when attacked colony holds ≥ half nation's colony population. Largest live combat gap. H — FIXED: both clauses at combat_apply_1b0e_resolve_handicaps tail; counts from `stuff.colony_counts`/`colony_pop_totals` (DS:0x9298/0x940c mirrors, DOS per-turn staleness kept).
2. combat_strength.c:184 — veteran +50% gated on JOB_SOLDIER only; **Veteran Dragoons fight at plain Dragoon strength**. Rest of port treats 0x15 and 0x17 as veteran. H — FIXED: accepts UNITS_JOB_SOLDIER or UNITS_JOB_DRAGOON.
3. combat_strength.c:58-59 — name match includes "Cont. Army" but not "Cont. Cav." (asymmetric); DOS gates on type 1/4 only, so whole Continental arm uncited. M-H
4. combat_strength.c:170 — damaged −2 peel applied to Privateers too; DOS applies to Artillery (type 0x0b) only. docs/combat.md:82 documents the wrong form. M-H
5. combat_strength.c:415, 388 — Fortify(5) accepted for the +2 defense and terrain-denial; DOS `FUN_157e_015e` requires Fortified(6) only. Digging-in unit gets bonus a turn early. Sibling 1b0e clauses correctly accept 5 or 6. M-H
6. combat_strength.c:723-726 — Scout-vs-Artillery force-defender-wins invented; DOS's only auto-loss is Brave vs human-controlled Artillery (ported separately 2026-09-08 at units.c:4774). Also units.c:4885 fakes roll without consuming RNG. Stale stand-in to retire. M-H
7. combat_strength.h:38-39 — `COMBAT_FLAG_FATIGUE_66` and `COMBAT_FLAG_VILLAGE_CAPITAL` both 0x0008; attacking a capital village prints phantom "Fatigue −66%" row (combat_analysis.c:184). H — FIXED: VILLAGE_CAPITAL relocated to flags2 0x0010 (runtime-only, nothing serializes flags2).
8. combat_strength.c:41-52 vs units.c:3114-3126 — occupied holds computed two ways (goods-only vs goods+passengers); troop-laden ship gets evasion penalty but no strength penalty. M
9. combat_strength.c:287-294 — colony fortification multiplier requires owner match + nation ≤3; DOS colony probe has no owner test. Internally inconsistent with `combat_unit_on_colony` (255-263). M-L
10. combat_strength.c:363-390 — "village on either tile → defender gets terrain" invented; DOS's clause is colony-probe on attacker's tile. Port also adds a `combat_woi_active` guard DOS lacks. M
11. combat_analysis.c:451-453 vs combat_strength.c:879 — naval ×3/2 attack factor applied but Attack Bonus row suppressed for naval; displayed mods no longer sum to strengths. docs/combat.md omits ×3/2. M
12. units.c:5525 vs units.c:3206 — two copies of damage-vs-sink no-RNG fallback break ties opposite ways. L
13. units.c:5520 — dead gate (`attack_str <= 0` already returned at 5464). L
14. combat_analysis.c:168 — Cargo row prints `holds*100>>3` (12.5%/hold) but real penalty is −holds after ×8; only equal at base 1. L
15. docs/combat.md:577 — coastal-fort Resolve row stale vs rebuilt guns/hull roll (bugs 255). DOC

## Colony economy

16. colony_yield.c:23 — Prairie Farmer base **3**, NAMES.TXT and terrain_yields.md say **2**; lone outlier in otherwise cell-exact tables; pedia (reads NAMES) disagrees with production in-game. H
17. turn.c:629 — EOT Fisherman gate short-circuits `has_docks` with "coastal ⇒ true" before checking Docks building; docs say Docks required; the 4 sibling sites (colony_preview.c:91, colony_screen.c:1589/3853, game_loop.c:9025) have no shortcut, so UI shows 0 fish while tick banks it. H
18. colony_production.c:299-305 — SoL production bonus adds `max(latch, live%)`; docs (both) say latch-bits-only; live term is an invented "stand-in" that defeats the one-step latch ramp; town-commons sibling correctly latch-only. M-H
19. colony_production.c:657-662 — unworked Town Hall still emits raw `sol_bonus` bells; the sibling crosses path deleted exactly this as invented (584-589). Unworked Town Hall at 100% SoL yields 3 bells not 1. M-H
20. colony_yield.c:356-357, 427 — Silver Miner special-resource deferred to `post_resource`, added after expert multiplier without expert doubling; doc rule doubles additive bonus for matching expert. Surviving uncited curve-fit from commit 3a9f688. M
21. game_loop.c:8554-8561 — ORDERS "Join Colony" auto-assign has no school filter (can seat specialist as teacher); bugs 414 fix landed only on the colony.c:1576 copy. M
22. colony_craft.c:24-28 — recipe array order lets ore→tools→muskets cascade in one tick; NAMES @BUILDING order (natural DOS iteration) would make muskets consume last turn's tools. Uncited ordering. M (rests on catalog-order inference)
23. colony_yield.c:518-545 — stale comment claims flat +2 commons food; code below implements the 4-way class split docs agree with. L
24. colony_production.c:715/723 + turn.c:1356/1372 — sol-free `lumber_total` contract (header + doc) vs live sol-adjusted debit; code likely right (bugs 169), doc/header stale; dead path invites wrong "fix". L
25. colony.c:2605-2607 — food warehouse cap hardcoded 199, uncited, ignores warehouse level; drives permanent "warehouse full" chrome in a growing colony. L
26. colony_yield.c:471 — improvement-stack road term uses road-bit only; DOS mask 0x0a = road-or-settlement per map.c:1674; three consumers of same mask disagree. Practically unreachable. L
27. colony_production.c:630 vs :238 (+ turn.c:334 vs :577) — opposite precedence for `population` vs `colonist_count` fallback. Latent. L

## Turn flow

28. turn.c:2854 — 1790/1840 anniversary chrome missing DOS gates `autumn==0` and `!at_war`: @WARN1 fires **twice in 1790** (Spring+Autumn) and during WoI. Sibling @SOONRETIRING0 (ai_king.c:4950) has the gates. H
29. turn.c:2892 + :2949 — 1800 game-over year missing `!at_war`, then sets `calendar_latch` unconditionally; a WoI win on/after 1800 latches WON but never runs win sequence (game_loop.c:11880 gate). Also Section E runs before woi computed. M
30. turn.c:3670-3676 + game_loop.c:9702-9715 — decade autosave writes slots 8 **and** 9 (DOS: exactly one) and omits `turn > 2` guard. M-H
31. turn.c:3484-3520 — INDIAN phase runs all 8 slots; DOS skips extinct tribes (bit 7 of tribe stance, transcribed at ai.c:4786 but never tested). Extinct tribe keeps rolling, drifts RNG stream. M-H
32. game_loop.c:8371 — auto-end-turn (EOT option OFF) still uses `turn_human_units_exhausted`, the helper the file documents as wrong; all other sites migrated to `game_units_pending_orders`. M
33. game_loop.c:10146-10148 — turn autosave written before Europe ship arrivals applied; reloading slot 9 delays every arriving ship one turn. M
34. game_loop.c:8358 — post-move handoff calls `turn_select_next_unit` without the standing-order skip loop siblings use; frame-flicker of wrong selection. L-M
35. turn.c:3534 + 1963-1971 — negative `human_nation` (headless) makes SETUP skip nothing and FINISH filter nothing: colonies produce **twice** per turn in headless sims. L-M
36. turn.c:3306 — SETUP raises turn-owner indicator in human color; turn.h and docs say no indicator in SETUP. L
37. turn.c:2728 — `turn_run_european_ai_stubs` dead code with superseded (wrong) phase order, still exported. L
38. docs/turn_between_players.md:66-83 — stale: TURN_PROC_KING split absent, King/REF etc. still shown inside FINISH. DOC

## Units / movement (beyond MP cluster)

39. turns_worked multi-role collisions: units.c:478 + turn.c:166 — Sentried/Fortified Treasure Train outside colony double-incremented, despawns ~4 turns not 8; units.c:554 + turn.c:166 — anchored damaged/building ship repairs ~2× fast; units.c:7474 — `units_wake` zeroes it, wiping repair progress / treasure clock / **trade-route stop index**; units.c:7284 vs 7255 — sentry doesn't reset it but fortify does (stale value = same-turn MP refund). M
40. units.c:7467 — wake `parked` set excludes CLEAR_PLOW/BUILD_ROAD: woken pioneer left at 0 MP though turn.c:158 zeroed its allotment for the same reason as Fortified. M
41. units.c:7813 — flood neighbour pick `best_score = 99` but costs are thirds (up to 39/tile); mountainous paths exceed 99 and flood falsely fails. M
42. units.c:7641 — `units_flood_edge` omits the `>100 → 1` sentinel clamp both map.c cost helpers apply; classes 30/31 cost 765 in flood vs 3 in real move. M
43. units.c:8013 — greedy fallback tier has no owner term (flood tier has); goto can route through foreign tiles / into peer colonies. M
44. units.c:6254-6259 — enter probe: empty foreign colony bypasses war/treaty gate entirely for AI and goto stepper; peer colony captured while formally at peace (human protected only by game_loop @HAVETREATY). M
45. units.c:9652 vs 9566 — `units_find_boardable_ship` ignores goods holds; probe says BOARD, board fails as bogus BLOCKED_DOMAIN. M
46. units.c:5759 — `strstr(name, "Missionary")` but type name is "Missionaries"; squat test never fires, missionary classified illegal squatter in `units_can_enter`/pathfinding. Compare units.c:8722 "Missionar". M
47. pedia.c:72-73 — claims sentried units wake when enemies approach; no such mechanic exists anywhere. M
48. game_loop.c:12054-12066 — goto to a farther lane tile sails to Europe from first high-seas tile touched, not ordered destination. L
49. map.c:1941 — `map_move_cost_step` road-only pair rule vs `map_move_spent_thirds` road-or-settlement (bugs 352 fix); helpers now disagree (dead outside tests). L

## Europe / market / trade routes

50. europe.c:2478-2482 — human harbor buys/sells pass `col1=NULL`: nation `trade.tons/tons2/gold` ledger never written, so player's own trading is invisible to the long-run price pool (Custom House + AI sales do count). H
51. europe.c:3042-3058 — sell credits gold but never credits tax to `royal_money` (DOS `nation+0x22 += tax`); all Europe tax revenue vanishes, REF systematically underfunded. Custom House arm does credit it. H — FIXED: europe_credit_sale_tax helper; europe_sell_hold + europe_sell_unit_hold credit gross−net to seller nation royal_money (trade-route auto-unload fixed for free; #52/#53 direct paths still leak, see below).
52. game_loop.c:2861-2874 — shift+drag partial sell: no boycott check, no price movement. Boycott exploit + zero-market-impact channel. H
53. game_loop.c:13429-13435 — `-` key single-unit sell: same two omissions; 99 presses liquidate a hold with zero price movement. H
54. game_loop.c:9418, 13337, 13399 — all three @HOWMUCH4 purchase prompts quote `europe_sell_price()` (bid) instead of ask; off by burden+1 (Food quoted 0 not 8). H
55. europe.c:725/729 — @CARGO `start_hi` discarded; DOS rolls RNG(lo..hi) per cargo per campaign; every game opens at bottom of every price band. H
56. europe.c:589-631 — missing DOS Spain override: nation 2 recruit-pool slot 0 forced to Jesuit Missionaries after seeding. M
57. europe.c:2392-2394 — Dutch (term*2)/3 damping keyed on `human_nation == 3` which the caller can never satisfy (passes −1), and applied to buys too (DOS: sells only). Dutch player gets no damping on own harbor trades. M
58. europe.c:2387/3392 + ai_euro.c:6405 — `seller_is_human` hardcoded true for AI dump-sells; at difficulty >2 volume-term sign flips, AI sales push market wrong direction. M
59. col1_bridge.c:1444 vs 1821-1853 + game_loop.c:6287 — boycott `0xFFFF` heal applied only to Europe mirror; nation copy stays poisoned and the render mirror copies it back over the healed value first frame. M
60. europe.h:222-223 + col1_bridge.c:2512-2600 — `trade_route_plus1`/`trade_stop` not exported for Europe-lane ships; saving mid-Atlantic silently drops route automation. M
61. ai_euro.c:7421-7422 — 5d04 AI Europe sale writes tons/gold but not tons2 (price-pool input) nor royal_money. M
62. europe.c:1521 — `europe_arm_sell_gain` untaxed while every other sale routes through tax; no DOS arm located either way. L
63. reports.c:2038-2039 — Economic report fallback ask missing `+burden`. L
64. game_loop.c:2870 vs europe.c:3045 — empty-hold sentinel 255 vs 0. L

## Indian / King AI / diplomacy

65. ai_contact.c:8292-8305 — post-ambush raid pulse adds +2 alarm and `attacks++` on **every** tribe of nation, immediately after DOS's negative vent + attitude zero in `units_resolve_land_combat`; last surviving retired-drip-class caller (`ai_contact_alarm_bump_amount`); attacks word now live state (hostility gate + tier-3 map alarm). H
66. ai_contact.c:642, 1259 — @INDIANSHUN reject and open-hostilities also `attacks++` across all tribes; DOS's only attacks-writer is the per-settlement trespass bump. M
67. ai_contact.c:7718-7748 — SHIP raid kind: −16 alarm charged for zeroing MP + dumping 1 ton; DOS row is "unit at the colony killed". Raider pays alarm for nothing. M
68. ai_contact.c:4595-4604 — WoI defection status line says tribe "declares for the rebel cause" while the mechanic (per its own comment) makes it Tory (+100 rebel alarm, −100 Crown). H
69. docs/difficulty.md:130-146 — King tax table (`first_year = 1536 - diff` etc.) matches nothing in ai_king.c; real gate is `ai_king_audience_roll` (turn ≥30, band 18/15/12/9, −2*(diff−2)). DOC-H
70. docs/difficulty.md:151-166 — REF seed/intervention table stale vs `8*diff+15 / 5*(diff+1) / 3*diff+2 / 6*diff+2` and census-derived backups. DOC-H
71. ai_king.c:3013-3015, 2372-2374 — comments reference retired constant/stand-ins. L
72. ai_contact.c:4624-4657 — alarm-prelude escalation latches `unknown31_flags |= 0x20` forever (fires once per nation per game), plus undocumented `alarm < 30` gate; makes difficulty escalation table nearly meaningless; repurposes unmapped DOS save byte. M
73. ai_contact.c:3686 vs 5168 — beg gate `alarm >= 55 skip` vs gift gate `> 74`; DOS has one gate (≤ 0x4a) for the single 022e encounter; 55-74 window produces no visit at all. M
74. ai_contact.c:3528-3537 — beg-food outcome binds *first tribe of nation*, not the visiting Brave's home settlement (gift half binds correctly); wrong village takes attitude zero + capital doubling. M
75. ai_contact.c:5242-5249 + 3962-4058 — convert rolled twice per nation-turn (mission_convert_visit with own RNG, then gift arm re-rolls and `continue`s without sending); visit also fires on standing adjacency (invented-pulse shape). M
76. ai_contact.c:7482-7487 — raid threshold 35-not-40 for euro nation 2 "Spain conquest bias": invented, and backwards (e is the *target* — natives raid Spain earlier). M
77. ai_diplo.c:633-639 — capital surrender forces PEACE bit + `relation_by_indian = 0x60` uncited (only the alarm clamp has DOS citation); razing capital rewards attacker with enforced peace. M
78. ai_contact.c:3689-3699 — stale comment: contact_state is annually wiped, not a forever latch. L
79. ai_diplo.c:572-575 — stale comment bands (50/40 vs live 26/16). L
80. docs/indians.md:179-180 — pop cap 15 stale vs `3*tech+4` / `2*tech+3`, satellites don't grow. DOC
81. col1_save.h:778-781 — `woi_defect_resolved` comment wrong: set only on success (matches DOS); eligible tribes genuinely re-roll every turn. L

## Founding Fathers / SoL / scoring / save

82. founding_fathers.c:1352 — `elect_commit` writes `head.founding_father[idx]` unconditionally; DOS writes only when still unclaimed (write-once, verified against dutch-reports.SAV). Turns first-claimer record into last-claimer; corrupts the reports fallback. H — FIXED: head write guarded on `< 0` (matches FUN_4345_0342).
83. founding_fathers.c:289 — `nation_has` reads head equality, contradicting reports.c:717 and inconsistent with `ff_available_to` (bitmask-only); head/bitmask desync (zero-filled heads existed per bugs 288) grants false effects AND leaves father electable again → double `apply_effect` (Jones second Frigate, Magellan re-bump, Coronado re-reveal). M
84. founding_fathers.c:171 — pool sync only adopts stash when `> 0`; legit zero pool (right after election) never round-trips — reload refunds a large `total−spent` estimate, can fund instant second election. M
85. col1_save.c:685/769 + col1_bridge.c:1457-1458 — pool stash permanently destroys real `liberty_bells_last_turn` for all 4 nations and leaks the FF pool into the Europe screen's "bells last turn". M
86. founding_fathers.c:225 — `FF_DESOTO_REVEAL_RADIUS` 1 vs doc's radius 2 (elect-time sweep tighter than ongoing effect; Coronado sibling correct). M-L
87. founding_fathers.c:870/945/1099 (+ game_loop.c:564, reports.c:2618) — `i < colony_count` iteration but `colonies_abandon` leaves in-place holes and shrinks count: loops visit dead slot, miss live tail colony (La Salle sweep, Coronado reveal). M
88. reports.c:2335/2499 — Colony report pairs col1 and runtime colony arrays 1:1 by index; offset after any abandon/raze → SoL/bells attributed to wrong colony. M
89. colony_production.c:416-418 vs turn.c:2170-2175 — rebel accumulator recomputes bells with sol_bonus=0 while EOT nation tally uses real bonus; two different "bells this turn" for same colony, zeroing rationale invented. M-L
90. col1_bridge.c:2734-2736/2747-2749 — `col1_contact_adjacent_tribe` on every human land action drips friction + alarm (fourth drip of the class bugs 297 removed), and increments low byte of a documented signed int16 attitude word with `< 11` on that byte (incoherent at values ≥256). M
91. col1_bridge.c:1764 — `head.show_entire_map = 0` every capture; read as de Witt report gate; cheat/post-win state can't round-trip. M-L
92. col1_bridge.c:2198 vs 1223-1227 — spent-MP export via `units_max_mp` (Magellan +3 only if module global bound) vs import adding +3 unconditionally: +3 MP per round trip when capture runs unbound (tools/tests). M-L
93. colony_production.c:194-206 + col1_bridge.c:1880-1894 — colony matched by (x,y) only; re-found colony on same tile inherits previous owner's SoL history. L
94. founding_fathers.c:1416 — election gate reads the stash-overloaded field as genuine EOT bells. L

## European AI

95. ai_euro.c:16728 — 20e6 sail-pick "doesn't need colonists" arm `−0x25` (−37); DOS SBB idiom gives −25 (`-0x19`). Hex/decimal slip; ferries over-avoid staffed colonies. H
96. ai_euro.c:16744 — reads ai_flags bit 0x04 (`NEEDS_MILITARY`, which has **no writer** in the port) where DOS raw tests bit 0x08; war-cargo destination score always takes the −0xf arm on DOS-loaded games. M-H
97. ai_euro.c:16749 — missing `(int8_t)` cast on `cargo_idle_turns`; sibling at 13452 casts; DOS reads signed. M
98. ai_euro.c:1592 vs 1546 — `clear_pre_stockade_build_queue` runs last in dispatcher and wipes exactly the young-colony Docks/Warehouse picks `prefer_young` just made (and the `colonies_found` Docks default). Comment/code disagree (Pop≥2 vs `< 3`). M
99. ai_euro.c:248 — 0x08 relation bit double-booked: 0a60 continent-stance gate reads it as DOS amicable-negotiation latch, but Linux-only `AI_DIPLO_TREASURE_STRONGER` (ai_euro.c:3254, ai_king.c:1724) writes the same byte; treasure train passing a stronger peaceful rival can flip continent tier. ai_diplo.h:79 documents collision as unreconciled; this consumer post-dates it. M
100. ai_euro.c:12003 — DOS's non-ship guard and `odds<1` clamp on the −999 attack penalty dropped; latent (sea units gated out upstream) but function branches on `is_ship` as if reachable. L
101. ai_euro.c:12098 — reads destination-tile colony where DOS reads mover's nearest own colony; also widens DOS's single owner gate. L
102. ai_euro.c:10855 — `s_20e6_hop_slot`/`hop_steps` get no despawned-id cleanup unlike siblings; reused unit id inherits foreign ring-hop latch. L
