# DOS-fidelity audit of surviving Aug/Sep 2026 code (2026-11-06)

STATUS: OPEN. Findings not yet verified by a second pass and not filed as port_bugs.md rows.
This is an audit dump, not a spec (docs/conventions.md "Don't read archives willy-nilly").

## Method

- `git blame -w -M -C` over src/ (third_party excluded): 124.6k of 183.8k lines still date to
  2026-08/09 (Jul 13.6k, Aug 40.7k, Sep 83.9k, Oct 35.7k, Nov 9.8k). 3240 functions carry Aug/Sep
  lines (85.2k function lines).
- 16 read-only agents, one per gameplay slice (~82k Aug/Sep function lines covered), compared
  functions against the raw Ghidra decomp / ndisasm. Each agent compared ~30-45 functions; the rest
  were skipped (see each slice's header line). Out of scope: mod-gated code, platform/SDL, port-only
  plumbing, documented DB-/TB-/known divergences.
- Confidence: CONFIRMED = bytes/decomp read; LIKELY = decomp suggests, asm unread; QUESTION = needs a
  DOSBox trace. "U" = port logic with gameplay effect and no DOS source found.
- Main-thread check: the col1->colony[] staleness claim (King H1/H2/H7, K8) holds:
  game_capture_col1 (game_loop_saveload.c:439) runs only at save/autosave/hotseat/F10.

## Ranked shortlist (high/medium)

| Slice#item | Where | Claim |
|---|---|---|
| C1 | ai_euro_land.c:957/965 | nearest-colony miss -> 0 not 9999 (FUN_15eb_0142); crown slot w/o colonies halves REF attack odds |
| I1 | game_loop_eot.c:2004-2017 | map ORDERS Load/Unload Cargo invented; DOS loads/unloads goods (FUN_479b_11a4 / 0f60) |
| H1 | ai_king_ref.c:59-120 | intervention/merc landing colony from stale col1 records + invented fallback |
| J1 | turn_production.c:1833 | ran-out-of-X popups suppressed by status_written (horse breeding) |
| A1 | units_combat.c:2564-2604 | extra INDIANLOSE/INDIANWIN0 on colony fights |
| A2 | units_combat.c:2530-2563 | EUROPEWIN/LOSE gate: DOS needs human defender |
| B1 | units_pioneer.c:375,:610 | work body refuses Hills/Mountains plow; DOS plows (veto menu-only) |
| C2 | ai_euro_land.c:1937-2045 | attack_term zeroes foe-stack a/d when top unit non-combat |
| D2 | ai_euro_europe.c:1466,:1527 | turn>99 difficulty discount 16-bit wrap missing |
| E2 | ai_contact.c:1523 | village haggle on invented private LCG, not 0x83a6 reseed of global LCG |
| E4 | ai_diplo.c:1889 | PEACE vs OLDPEACE picked by war bit; blank labels |
| F1 | ai_contact_demand.c:1635 | AI incite rand/mission gate inverted |
| F2 | ai_contact_demand.c:1650 | AI can't afford incite -> port establishes/denounces |
| F5 | ai_contact_actions.c:77-95 | village action resolves first adjacent village of tribe |
| G1 | ai_native_021a.c:550 | lone-treasure rng path runs grudge latch |
| H2-H5 | ai_king_war.c:436, ai_king_ref.c:939, ai_king_audience.c:1355, ai_king_war.c:1142 | stale mobilization pop; lake landing; live SoL declare gate; KINGNEWWAR after clamped audience |
| I2,I4 | game_loop_update.c:970, :3657 | trade stop serviced at 0 MP; B founds colony with row hidden |
| J2 | turn_production.c:1712-1760 | completion only when hammers produced (spoil before Warehouse) |
| K1 (=E1) | colony_build.c:690-709 | raid-burned root crew not made Carpenters |
| K3 | turn_colony.c:1114-1224 | per-turn damaged-ship teleport |
| L1,L2 | europe_dock.c:249, turn_colony.c:761 | immigrant dock order; double Soldier->Dragoon roll |
| M1 | game_loop_render.c:2303, map_gen.c:1590 | Americas .MP new game: no Pacific walk (no Pacific woodcut) |
| N2,N5 | options_dialog.c:370, game_dialogs.c:1690 | Esc drops option toggles; OVERBOARD cancel row order |
| O1,O2 | map_panel.c:1787, :1652 | sidebar colony goods strip missing; tile owner from nibble |

Doc fixes flagged: docs/popups_catalog.md:211 + manual_gap.md (INDIANSURPRISE "Done"), docs/cheats.md
(treasure 2800, Test Routine channel), docs/save_format_map.md:382 (Pacific col 41), known_divergences.md
#1081 (jobs menu leave-as rows), closed #737 (cheat treasure) and #1076 (Warehouse Expansion) reopened by
new evidence.

## Per-slice findings

### A — combat

```
A_combat (~30 fns; enter_probe, LCR, burial, plunder skipped)
1 CONFIRMED units_combat.c:2564-2604 outcome_popups extra @INDIANLOSE / @INDIANWIN0 on colony fights; DOS 1b0e raw 101036-101143 gate local_10<0||0x8db8||bVar8, bVar28 colony rows. called unconditionally units_combat_resolve.c:357,:479. medium UI frequent.
2 CONFIRMED units_combat.c:2530-2563 EUROPEWIN/LOSE gate: DOS raw 100811-100836 asm 0x5D974 needs human DEFENDER, local_c==0, !bVar12; STRING2 militia @JOB name. Port pops on human attacks + AI capture of empty colony. medium UI. VERIFY (user plays live; #242 closed).
3 CONFIRMED units_combat_resolve.c:1618-1627+1112 coastal fort fire rolls vs first ship not best defender (364b_03f6 raw 57067-57109 -> 1b0e -> 5fef_0000). med-low.
4 CONFIRMED units_capture.c:1045-1078 + combat_strength.c:192 Revere phantom spawned type 1 gets Veteran +50%; DOS type 0x17 (478c_002c raw 76545-76564), vet test type 1/4 (raw 8942). low-med.
5 CONFIRMED units_combat.c:2265 raid_damage_ship repair bill not doubled; DOS 0352 raw 99622-99626 reads DS:0x312a=24 (TB-20) -> always doubled. low.
6 LIKELY units_combat.c:1404-1409 veteran strip on capture ignores show flag (raw 99400-99413 inside param_3). low.
7 CONFIRMED units_combat.c:839-847 @CONTINENTAL STRING0 old type name; DOS new (asm 0x5C7B8). low wording.
8 LIKELY combat_analysis.c:199-203 Cargo row label lacks ": N" (asm 0x5ED11). low.
9 CONFIRMED units_combat.c:2986 @INDIANSLAVES singular tribe name; DOS plural 0a1a raw 101176. low.
U units_sweep_stack_after_loss attack==0 filter :1740; mounted_attack_spend_all no-op; revere_should_auto_arm soldier veto units_capture.c:274 (DOS raw 100424 FF12 + muskets>0x31 only).
```

### B — units

```
B_units (~30 fns; render/sprites/cargo internals/flood skipped)
1 CONFIRMED units_pioneer.c:375,:610 work tick/plow_w refuse Hills/Mountains plow; DOS work body FUN_479b_01a6 raw 76745-76755 plows them (veto only in ORDERS menu FUN_2b5a_0b34 raw 42224). AI phantom-pioneer arm ai_euro_land.c:478. medium.
2 CONFIRMED units_pioneer.c:182 native_land_tail road arm: DOS FUN_479b_0526 raw 76935-76941 calls 00ca purchase BEFORE Minuit test -> pays 0, lands_bought++, mask 0x10. Port returns at price<=0. low.
3 LIKELY units.c:1392 KINGGALLEON2/3 %STRING2 = port_city; DOS FUN_5fef_1908 raw 100166 = country phrase form 0 (FUN_281f_042e). low wording.
4 LIKELY units.c:1488 FoY cancel recruits slot 0; DOS FUN_38fd_4884 raw 64747-64751 local_58<0 -> skip. RNG drift. low.
5 QUESTION units_move.c:1613 Sentry plays sound 0x58; DOS stub 2b5a:1106 no sound (dispatcher unresolved). low.
U units_cargo.c:1832 units_top_on_map_tile highest id uncited vs DOS chain head; units_move.c:1563 set_orders clears goto for Sentry (DOS stub writes only +0x314c).
```

### C — euro_land_goals

```
C_euro_land_goals (~30 fns; 5952 improve/absorb/equip, expand.c, ship_dos, dispatcher skipped)
1 CONFIRMED HIGH ai_euro_land.c:957/965 20e6_prologue nearest-colony miss sets home_dist/any_colony_dist 0; DOS FUN_15eb_0142 raw 9380-9421 writes 9999 to DS:0x8db8. Consumers: crown-halving :2066 (raw 88914), 'B' park :2907 (raw 88990), scout explorer :1282 (raw 88524). WoI crown slot w/o colonies -> REF halves odds, declines attacks.
2 CONFIRMED ai_euro_land.c:1937-1947+2024-2045 attack_term foe_id=-1 reset zeroes a/d foe-stack factors; DOS LAB_52aa raw 88888-88897 uses real tile unit local_5c. medium (AI vs human colonies).
3 LIKELY tile-stack walks exclude ship riders (DOS FUN_1427_0d38 chain includes): park_arms_4d2e :2897, 5952_labor_demand goals.c:1297-1340 (raw 94040), stack_count labor :1566, 0a60 defender counts goals.c:840/1006, stack_combat_0b :1210. low-med.
4 LIKELY ai_euro_land.c:1272 explorer_flag Dragoon clause live foreign-colony count vs frozen DS:0x95f2 bit4 (raw 88508); same ai_euro_goals.c:1429. low. (pairs with D#4)
U ai_euro_s_20e6_village_visited latch (land.c:1438,1466-1506, RTST) no DOS cite; wander_step :2230 water owner = hu->nation_id patch.
```

### D — euro_colony_ship

```
D_euro_colony_ship (~30 fns vs decomp; 5952 indoor/ledgers/tick, berth/europe lanes skipped)
1 CONFIRMED ai_euro_europe.c:1491 hire_tail_colony_demand: Pioneer tools rebate uses BUY price; DOS FUN_291f_09ea = SELL (raw 92916-92920, asm 521d:697a-6998). low-med.
2 CONFIRMED ai_euro_europe.c:1466 (+horse :1527): turn>99 difficulty discount int32; DOS 16-bit IMUL wraps (asm 521d:68d3-68f3, raw 92872/92945). Viceroy price>819 diverges. medium late.
3 QUESTION ai_euro_europe.c:914-923 cb_sell_hold0 boycott skip no DOS counterpart (FUN_38fd_1f0c raw 60320-60337 has no +0x20 test); TB-25 comment :1675 hints DOS AI boycott bits. low.
4 LIKELY ai_euro_ship.c:2334 unload_mask_snapshot foreign-colony term live scan; DOS reads frozen DS:0x95f2 bit4 (raw 89535-89537; writer FUN_4962_0018 raw 78300). low.
```

### E — diplo_contact

```
E_diplo_contact (~45 fns; name/chrome helpers, test seams, mods, village_meet row gating (#501/#726), clamp_alarms, leader_trait skipped)
1 CONFIRMED = K#1 colony_build.c:707 raid-burned root crew not Carpenters (FUN_5fef_0f14 raw 99975-99983 -> 15eb_1068 job 0xd). medium.
2 CONFIRMED ai_contact.c:1523 ai_contact_local_rng invented private LCG for 2820/2e92 haggle (trade.c:2030,:2090,:2289); DOS FUN_4d56_2820 raw 82130 reseeds global LCG from DS:0x83a6 (dos_rng_reseed_83a6 exists). every human village trade; global stream drift. medium.
3 CONFIRMED ai_diplo.c:2597-2600 153e silent-dominance exit skips treaty-timer re-arm (raw 98418-98428 LAB_5bfb_30de (6-diff)*2, Franklin half) and returns 0 vs 1 (asm 5bfb:1664). war-intent roll earlier. low-med.
4 CONFIRMED ai_diplo.c:1889-1891 peacemenu PEACE vs OLDPEACE chosen by war bit; DOS raw 97995-97997 / 98055-98057 by treaty-already-existed; 4-row ask on 3-row PEACE -> blank labels (ai_diplo.c:1563). test_ai_diplo #474 case enshrines. medium.
5 LIKELY ai_diplo.c:2782 + :550 AI-vs-AI SIGNTREATY/DECLAREWAR never shown (13b0 raw 97286/97304, 465b raw 75576-75585 no human gate). trace. low-med.
6 CONFIRMED ai_contact.c:1492-1494 ship visit reads friction byte not attitude word (OVL13 asm 130150/130208 CMP word 0x40) + uncited at_war gate. low-med.
7 CONFIRMED ai_diplo.c:2010 PIRACY/SIEGES request/demand inverted (asm 164188/164329; APOSTATES 164056 opposite). low.
8 CONFIRMED ai_diplo.c:2304 withdraw refusal always WARMANLY + wrong tokens (raw 98159-98166). low.
9 CONFIRMED plural tribe names (0a1a) missed by #1083: ai_contact_raid.c:474,:938; ai_contact.c:1244 WHACKINDIANS; ai_contact.c:633 INDIANSHUN; ai_diplo.c:1647 HEATHEN. low. (pairs A#9)
10 CONFIRMED ai_diplo.c:1188 worthiness treasury clamp <= vs DOS strict (asm 5bfb:1a3c-1a52 JBE). low.
11 CONFIRMED ai_diplo.c:1625 USA-flagged target still shows WORTHY; DOS raw 97989-97994 auto-sign. low.
12 QUESTION ai_contact_enqueue_welcome INDIANWELCOME count live settlements + 0->1 floor; DOS raw 96626 byte DS:0x962a+tribe. low.
U capital_surrender capital=0 "Fandom" + MET|PEACE encoding; ship visit at_war gate; 13b0 redundant adjacency rescan.
```

### F — contact_trade_demand

```
F_contact_trade_demand (35 fns line-by-line; mood_report, labels, TB/DB branches skipped)
1 CONFIRMED ai_contact_demand.c:1635 ai_contact_ai_incite_human rand(0,4)/mission gate inverted; DOS OVL13 0x46ac skip only when rand==0 AND no mission. medium.
2 CONFIRMED ai_contact_demand.c:1650 + ai_contact_actions.c:1574-1587 AI can't afford incite -> port falls through to establish/denounce (consumes unit); DOS case 7 a5b8 returns, nothing (overlays.c 76230-76242). medium.
3 CONFIRMED ai_contact_actions.c:795 live_among_natives_body extra alarm>=0x4b return (commit 6a6c8ce6); DOS a618 overlays.c 77786-77835 none -> +3 alarm + @LEARNMAD. human only. low-med.
4 CONFIRMED ai_contact_actions.c:1408-1424 denounce_heresy HERESY0/1 popup+sound gated human actor; DOS a594 overlays.c 75862-75874 unconditional. low-med.
5 LIKELY ai_contact_actions.c:77-95/1820 menu_village acts on first adjacent tribe village, not entered (DOS DS:0x8d4a bound by 4528); trade.c:95 same. medium where clustered.
6 CONFIRMED ai_contact_actions.c:1017-1043 speak_with_chief CHIEFHOWDY wanted goods skip 2820(-1) prep + stable sort vs a0c0 (overlays.c 78251-78269). low.
U menu_village fallback tribe's first village; speak_with_chief default goods (unreachable); a618 skill_ex skips rand(1,sum) when sum 0; stale header trade.c:612-618.
```

### G — natives

```
G_natives (021a instr-by-instr vs OVL13 asm; 152e, 03f8, 465b brave, 3180, 1b3a, 6a09 placement, goals ops; Euro goal producers/fleet skipped)
1 CONFIRMED ai_native_021a.c:550 dir_occupant lone-treasure rng(0,7)==0 path runs grudge latch; DOS OVL13 0xb0e-0xb54 JZ 0xb54 skips 0xb42 (overlays.c ~76631). Braves overscore treasure trains ~+50. medium rare.
2 LIKELY ai_native_021a.c:1141/1171 tail @INDIANSURPRISE (0x14dc) never shown; DOS OVL13 0x1316-0x13ae (overlays.c 76958-76970) on attack pick vs human with colony. popups_catalog.md:211 + manual_gap.md stale "Done". conflicts user obs #132 -> trace first. low-med UI.
```

### H — king

```
H_king (23 DOS bodies; audience mod, 20e6 MoW ship band skipped)
1 CONFIRMED ai_king_ref.c:59-120 10f0_pick_colony walks col1->colony[] records (refreshed only at save capture?) not live colonies; new colony coastal=0 excluded, destroyed kept; invented weakest-coastal fallback :105-121 (DOS lands nothing, #873a). DOS FUN_43f7_10f0 raw 74312-74331. med-high. VERIFY staleness claim.
2 CONFIRMED ai_king_war.c:436 1eca mobilization cap uses record population (stale) + name blank; DOS raw 74910-74940 live +0x1f. medium. VERIFY.
3 CONFIRMED ai_king_ref.c:939 0982_invasion water tile missing layer3 low nibble==1 (lake) test; DOS raw 74104. medium.
4 CONFIRMED ai_king_audience.c:1355 declare SoL gate live vs cached DS:0x53d0 (raw 75227; cheat sets 75). medium.
5 CONFIRMED ai_king_war.c:1142 human_5e52_tail runs KINGNEWWAR (5930) after clamped-away audience (tax 75%); DOS raw 68617-68619 skips when 5be8 returned 1. medium RNG.
6 CONFIRMED Dutch article dropped (country_name vs country_phrase): audience.c:1245, :868/889, ref.c:1389-1390, war.c:925, war.c:1269, offers.c:616. low wording.
7 LIKELY ai_king_ref.c:1244-1264 1528_announce_colony stale records. low.
U ai_king.c:358-396 intervention_nation fallback; offers.c:181-184 merc_offer returns before 1-in-3 draw while popup pending; REF latches inert.
VERIFIED (main): col1->colony[] refreshed only by game_capture_col1 (game_loop_saveload.c:439): saves, per-turn autosave (game_loop_eot.c:105, when request_autosave_turn), hotseat handoff, F10 score. Stale within a turn and when autosave off -> H#1/#2/#7, K#8 real (severity depends on autosave setting).
```

### I — game_loop_sim

```
I_game_loop_sim (~30 fns; render, HoF, debug/cheat, colony/Europe key handlers skipped)
1 CONFIRMED HIGH game_loop_eot.c:2004-2017 (game_order_board :1461, game_order_unload :1489) map ORDERS Load/Unload Cargo invented (board cursor unit / land passenger). DOS 2b5a:1908 -> FUN_479b_11a4 raw 77435-77509 (load best cargo <=100 from colony), 2b5a:1952 -> FUN_479b_0f60 raw 77343-77431 (unload lowest-key hold, WAREHOUSEFULL). Bodies exist game_dialogs.c:2650-2700 (colony_screen-keyed).
2 LIKELY game_loop_update.c:970 -> trade_route_retarget :1253 stop serviced on arrival step w/o MP; DOS 0bd0 only from Begin Trade Route + 3ae6 order-2 case (active unit, needs MP). load before production. medium.
3 CONFIRMED game_loop_update.c:3581-3616 Enter selects/moves units; DOS 303c no Enter, 32ee/3344 (raw 45870, 45894-45923) open colony / end turn. low-med.
4 CONFIRMED game_loop_update.c:3657 plain B fallback founds colony when ORDERS row hidden/disabled; DOS 4b58_14de needs enabled item. View Pieces B on water -> @SEACOLONY. medium.
5 CONFIRMED game_loop_update.c:3504+3625 H with ship = sail to Europe; DOS H = Show Hidden Terrain only. low.
6 CONFIRMED game_loop_update.c:3630 plain U fallback unloads passenger when row disabled. low-med.
7 LIKELY game_loop_update.c:960 -> game_loop_eot.c:359 extra @EUROPENOTLEAVE on Go To reaching high seas during WoI; DOS 479b_0972 raw 77092-77101 silent. low.
U game_after_unit_tail exact-tile Meet arm (orders.c:1335) unreachable; eot.c:1335 trade stop only own colony.
```

### J — production

```
J_production (~35 fns; 1d4c worker bodies, display, upkeep mod skipped)
1 CONFIRMED turn_production.c:1833 turn_produce_crumbs ran-out-of-X popups gated on !status_written; DOS FUN_364b_0688 raw 57695-57728 only gate DS:0x5384&0x20. horse breed sets status_written -> @COTTON etc almost never fire. medium. (cites #397, real history #399)
2 CONFIRMED turn_production.c:1712-1760 turn_produce_hammers: completion only if hammers_add>0; DOS raw 57730-57775 always resolves + re-reads warehouse cap before spoilage (57809). BUY Warehouse no carpenter -> port spoils at old cap. med-low.
3 CONFIRMED turn_production.c:1113 @TUTORIAL6 missing COASTAL gate (raw 57911-57915). low.
4 CONFIRMED turn_production.c:1435 birth gated colonist_count<32; DOS raw 57615-57622 none (newborn map unit). low.
5 CONFIRMED cosmetic turn_production.c:1081 custom house tax tail gated tax_percent>0; DOS raw 57311-57319 gates (DS:0x5382&1)==0. low.
U was_starving latch suppressing FOOD1/2 (:1421/1644, inside closed #5/#292); colonies_auto_assign_idle #256; colony_goods.c:400/454 cargo_idle_turns zeroed every unload (DOS only raw 90249).
```

### K — colony_turn_ff

```
K_colony_turn_ff (~45 fns; election core, land price, eject rows, year-end chrome match; chrome helpers, build queue, loaders, slot dispatchers skipped)
1 CONFIRMED colony_build.c:690-709 destroy_building burned chain-root building crew not reseated as Carpenter (job 0xd); DOS FUN_5fef_0f14 raw 99975-99982. auto_assign_idle then Statesmen. medium.
2 LIKELY colony_workers.c:706-715 admit_unit_seat_w clears 0-hammer Stockade project on every join; DOS no join writer of +0x94 (only 5952_035e raw 95711/95851); cites save observation. low-med.
3 CONFIRMED turn_colony.c:1114-1224 turn_route_damaged_ships per-turn teleport to nearest drydock; DOS relocates once at combat FUN_5fef_0352 raw 99583-99645 (dist!=0, <=), 3844_00f2 raw 58331-58372 only counts timer. med-low.
4 CONFIRMED colony_build.c:818-821 (+turn_colony.c:138-147, turn_production.c:348) completed Warehouse Expansion left selected: DOS Phase L raw 57741-57771 deducts 20 tools / NEEDTOOLS each time hammers>=80 then NOMOREWAREHOUSE; port refuses before tools. low. (#1076 CLOSED misread)
5 CONFIRMED colony_workers.c:477 assign_field_land_reaction alarm base keyed on colony owner; DOS raw 9893-9899 DS:0x5394 nation-to-move. low rare.
6 LIKELY declaration.c:287-306 one key skips whole signature; DOS 43f7_160a raw 74640-74667 per glyph. low UI.
7 LIKELY colony_build.c:834 build reveal fires for 0x10/0x1f; DOS raw 56966 excludes. low.
8 QUESTION colony_build.c:245-261 capture plunder divisor sums col1->colony[].population (stale?) vs DOS live raw 100970-100976. (pairs H#1/#2 staleness)
U colony_workers.c:872-889 auto_assign_idle Statesman parking (not in known_divergences); build completion crew rebind harmless.
```

### L — europe

```
L_europe (~45 fns; art/trade_screen/howmuch/icon flow skipped)
1 CONFIRMED europe_dock.c:249 europe_immigrant_from_pool human crosses immigrant inserted dock front (and turn_colony.c:754-758 again); DOS FUN_38fd_5e52 raw 68585-68588 then FUN_1427_03a0 relink oldest end (AI twin harbor.c:1896 does it). boarding newest-first differs. low-med. #999 list wrong.
2 CONFIRMED turn_colony.c:761-768 + units.c:1600 Soldier->Dragoon roll drawn twice for human vet soldier immigrant (europe_dock.c:255 + europe_spawn_dock_mirror_unit); DOS one FUN_281f_04d4 in FUN_38fd_0718 raw 59117-59127. Brewster cite wrong (4884 spawns before refill raw 64768-64775). mirror type can disagree, save reload flips. medium.
U europe_menu_open pool_ensure_filled LCG refill (inert); autosell/dump_sell fallback to human price when AI euro_price 0; 5e52 human DS:0x14c=1 auto-open Europe dropped (#223) needs trace.
```

### M — save_map_newgame

```
M_save_map_newgame (~30 fns; map_gen vs 684c_08c0 asm, census, post_map, map reveal, validate_head; most col1_bridge apply/capture skipped)
1 CONFIRMED game_loop_render.c:2303-2319 + map_gen.c:1590-1602 loaded-map (Americas .MP) new game never runs 08c0 tail Pacific walk (cutoff W-16 when param_1) + suppress pass (asm 684c:1950-1a9d). DOS 000001.SAV 1309 pacific bits max x 41. Port: no DISCOVERY OF THE PACIFIC woodcut; first save writes W/2 rule. save_format_map.md:382 wrong. medium.
2 CONFIRMED game_loop_render.c:2303 AMER2 two terrain stamps |=0xa0 at (21,1),(43,68) missing (asm 684c:1aa0-1ad3; DS:0x2174==0). low.
3 CONFIRMED map_gen.c:612-614 set_type_keep_feat + finalize :806-817 keep feature bits 0xe0; DOS writers FUN_1bb9_000a/1b9e_000a store bare byte. AMER2 (1,1) port 0x3a vs DOS 0x1a. low.
4 LIKELY map_gen.c:1607-1630 suppress on every offshore water tile; DOS needs prime resource (684c:1a52-1a7b). save bytes only. low.
5 LIKELY col1_post_map.c:51-80 post_find_rep all-lake cell -1; DOS returns lake id (raw 104967-105006) w/ stale coords. low.
6 QUESTION map_gen.c:584-596 rivers_pass ring-20 out-of-map cells skip rand; DOS FUN_684c_04a6 raw 105562-105577 no bounds -> possible extra draw. trace needed. medium if hit.
U map_gen_pick_start heuristic fallback; new_game_scenario.c:35-36 hard-coded AMER2 coords; col1_bridge.c:2857 crown default.
```

### N — ui_dialogs_popups

```
N_ui_dialogs_popups (~45 fns; ai_popup queue, asm-audited geometry, save/load plumbing, mods skipped)
1 CONFIRMED game_loop_menus.c:563 cheat Test Routine: DOS 2b5a_08cc (0x217AC) clears DS:0x5382 &=0xf4 + DEBUG @TEST popup; port status line, no clear. cheats.md:37 wrong. medium cheat.
2 CONFIRMED options_dialog.c:370 + game_dialogs.c:2128 Esc discards option toggles; DOS 2b5a_20f6/223a/23ce read rows back regardless (raw 42994-43054). medium.
3 CONFIRMED game_loop_menus.c:431-438 cheat Treasure 2800 vs DOS 100 (asm 0x23A7D byte store +0x315b=1). new evidence vs CLOSED #737. cheats.md wrong. low.
4 CONFIRMED game_loop_menus.c cheat Wagon no home colony +0x314a (asm 0x23A2A-0x23A5B FUN_281f_0614). low.
5 CONFIRMED game_dialogs.c:1690-1741 OVERBOARD "nothing" row (id 99) first in DOS (0x21EDE-0x21FDE); port last -> Enter dumps cargo. medium.
6 CONFIRMED popup_msg.c/assets.c GAME.TXT '_' not -> space (FUN_7314_0106 raw 117904-117925 every line). @LEARNMAD. low. (NB conventions say '_' = leading space only; DOS replaces all)
7 CONFIRMED game_loop_saveload.c:804 game_build_exploits surname strrchr incl space (FUN_1d1d_0d1a raw 20457) vs port strchr+1. low.
8 CONFIRMED game_loop_menus.c:657 cheat sound test prompt body missing + port status line (asm 0x23D86). low.
9 CONFIRMED game_dialogs.c:127/217 EUROPENOTAVAIL missing DS:0x53a2==0 term (asm 0x236B2). low cheat.
10 CONFIRMED game_dialogs.c:2010-2014 LANDHO empty name fallback; DOS strcpy unconditional (raw 40197). low.
11 LIKELY game_loop_menus.c:1007 trade_default_name trailing space w/o TRADENAMES. low.
12 LIKELY popup_msg.c:189-246 '%' gaps (lone %, %%, %HEX, %YEAR). low.
U cheat memory check status line vs @MEMORY popup; port status strings; name_entry outside-click cancel / options outside-click commit.
```

### O — ui_map_colony_screen

```
O_ui_map_colony_screen (~30 fns; production tab player-requested, preview, minimap, Europe render skipped)
1 CONFIRMED map_panel.c:1787-1831 colony_row: own-colony "With:" goods strip (5 icons, sort min(stock,100)*(0x84bc<<4)) missing + geometry (name x+0x14,y+10; advance font_h+0x11). FUN_49dd_0424 raw 79022-79112. medium.
2 CONFIRMED map_panel.c:1652-1668 cursor_block owner line: DOS layer3 high nibble (raw 78790-78811) Wilderness/<Tribe> Land/country for every land tile; port only settlement on tile. medium.
3 CONFIRMED map_panel.c:1704-1739 tile_details: minor river label row 3 "(Minor River)" (port row1), order terrain,river,road,plowed,resource,rumour; road test layer2&0x0a. raw 78943-79017. low.
4 CONFIRMED map_panel.c:1834-1911 village_row two lines (asm 126556-126623). low.
5 CONFIRMED map_panel.c:1992-2027 foreign stack row two lines (asm 126995-127052). low.
6 CONFIRMED colony_screen_draw.c:105 top bar gold = viewer not colony owner (FUN_2f2b_0fce raw 47821-47879, 15eb_05b2). spy view. low.
7 CONFIRMED map_panel.c:2087 EOT clamp 0xc6-font_h (port -line_h, 1px). low.
8 LIKELY map_panel.c:1925 stack_full extra stop y+18>h-2; DOS raw 79210 only 0xb7<y. low.
U game_loop_render.c:1243-1258 Europe passengers drawn in hold slots past capacity, covered by #122 covers; known_divergences #1081 stale (jobs menu now lists leave-as rows #1224).
```

### P — ui_reports_cinematics_sound

```
P_ui_reports_cinematics_sound (~30 fns; gsound_vm, sound backend, pedia (audited 10-30), pick_music skipped)
1 CONFIRMED closing.h:42 opening.h:69 cinematics 55ms/frame; DOS 60.877Hz clock, anim_timing 6 (closing, logo), pan_timing 8 (opening sail). closing 21s vs 38.4s; opening 49s vs 117s. low.
2 CONFIRMED woodcut.c:96-114 woodcut_play_tune ids 1/2/9 all sound_set_bgm(2); DOS 12fd_006c table: 0/1 gated EventMusic&&!BGM, 2 -> 129f_034c pool only, 9 -> 0318. low audio.
3 CONFIRMED game_loop_update.c:1236-1313 F2-F9 reports close only Esc/Enter/OK box, F-keys switch; DOS any key/click closes, no re-dispatch (#1227/#1231 fixed F10 only). Labor miss-click closes. low UI.
4 CONFIRMED opening.c:248-268 OPENSUN sprite from absolute anim_frame thresholds 0x87/0x99/0xad/0xc3/0xdc/0xec, back to 6 at 0xfc (OPENING.EXE _do_anims 0x454-0x4c5). low.
5 CONFIRMED reports_military.c:1008-1009,1139-1146 indian report muskets/horses unsigned; DOS CBW signed (asm 107217, 107355-107359). low (DOS quirk).
6 LIKELY woodcut_present.c:111-186 woodcut picture not fizzled in; DOS 6f30_0062 FUN_281f_03ea(8). low.
U reports_foreign crown==human -> -1 guard; sound_set_bgm(<=0) clears extra state.
Scratch disasm of OPENING/CLOSING.EXE in scratchpad/P_ui/.
```

