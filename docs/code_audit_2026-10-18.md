# General code audit — 2026-10-18

> **STATUS: open audit dump (findings filed as bugs.md #1086-#1122); not an authoritative
> spec; each row is a lead to investigate, not a verified fact.** Line numbers as of commit 9fcf9dec.

Eight parallel Opus 5.5 sweeps over the whole tree: A compiler warnings + cppcheck, B Euro AI,
C native/contact/king/diplo AI, D units/combat/turn/colony, E game loop/europe/col1/map/json,
F UI/assets/platform, G tests/tools/build/docs, H cross-cutting greps (scripts reproduced the
grep sweeps; they live in the session scratchpad, not the repo). Each bugs.md row cites
`code_audit_2026-10-18 <slice>` for the full evidence table below. Everything an agent
checked and found clean is listed at the end of each slice; do not re-audit those.


## Slice A

gcc 13.3; default build (project -Wall -Wextra -Wpedantic) = 6 unique warnings; extended flags 1331; release 47; cppcheck 2.13 29 lines. Raw logs were in the session scratchpad (warnings-default.txt, warnings.txt, warnings-rel.txt, cppcheck.txt).
| Sev | Area | file:line | Finding |
|---|---|---|---|
| HIGH | default warning | ai_euro_act.c:103 | unused local ctx since treasure cash-in deleted (-Wunused-variable) |
| HIGH | default warning | ai_euro_expand.c:343,380 | best_colony set never read: dead or dropped DOS write (check raw ~2226-2242) |
| HIGH | default warning | colony_plots.c:300 | col1->player && tests array member, always true (-Waddress) |
| HIGH | default warning | units_combat_resolve.c:1403 | fort_label[40] "%s %s" colony+building may truncate (-Wformat-truncation; 11x because file in 22 targets) |
| HIGH | default warning | colony_screen_popups.c:179 | queue_prefix[8] "%d " truncation warning |
| HIGH | default warning | units_move.c:9 | banner pasted inside Sections: comment (-Wcomment) |
| MED | UB signed shift | ai_contact_trade.c:1692 (decoder :1952) | (1 << 31) packed into int; >>31 on negative implementation-defined |
| MED | UB signed shift | ai_king_offers.c:38 | (hx & 0x3f) << 26 int overflow for hx>=32 |
| MED | OOB | europe_dock.c:376 | &eu->harbor[eu->harbor_ships++] no EUROPE_HARBOR_MAX check (europe_harbor.c:72, ai_diplo.c:1629 check); release -Wrestrict |
| MED | gold | europe_dock.c:145,299,376,386 | eu->gold -= bypass europe_purse_move |
| MED | null after use | game_loop_eot.c:1529-1531 | game->goto_port_pending_unit used before if (!game) |
| MED | build hygiene | CMakeLists.txt:227-236 | -Wall/-Wextra only on colonize_sim/colonize_ui; 113 files (tests/, tools/, main.c, sdl_runtime.c, ai_contact_link_stubs.c) no warnings; slim targets recompile src/core 731 times (ai_diplo.c in 12 targets) |
| MED | test UB | tests/unit/test_units_combat.c:1765 | seed*9973 int overflow (seed up to 2,031,690) |
| LOW | release FP | ai_euro_europe.c:1580 | -Warray-bounds departed[idx2] false positive |
| LOW | release truncation | assets.c:369, founding_fathers.c:567, game_loop.c:2469, game_loop_menus.c:671, ss.c:120, unit_stack.c:145,151 | ss.c:120 truncates path 255->237 (would open wrong file) |
| LOW | dead branch | ai_indian.c:960 | unreachable after :951 |
| LOW | dup branches | ai_euro_land.c:2437-2447 | ship/land far-probe arms identical; DOS raw 88834-88841 single arm |
| LOW | dup call | ai_euro_europe.c ai_euro_5d04_port_type_for | fallback repeats same lookup |
| LOW | dead null checks | ai_king.c:384, map_menu.c:38-40, cheat_list_dialog.c:468, map_panel.c:828 | deref near NULL check |
| LOW | should be static | ai_native_021a.c:115-203 (7 fns), ai_contact_raid.c:1196,1466,1619 | COLONIZE_INTERNAL, no *_internal.h decl, no test caller |
| LOW | nonliteral format | ai_contact_actions.c:937, ai_contact_raid.c:1531,1533, ai_diplo.c:671, test_idle_campaign.c:219 | catalog text as format; only one guards strstr "%s" |
| LOW | const cast | world.h:80-86 world_make + 110 inline literals | 165 -Wcast-qual |
| LOW | goto skips init | ai_contact_trade.c:361 | benign |
| LOW | shadow | ai_contact_raid.c:1338, colony_preview.c:63, reports_economy.c:683 (+74 tests) | no misuse |
| LOW | test uninit | test_colonies.c:3323, test_units_core.c:1454 | ids[1] read without n==3; pmsg printed unfilled |
| LOW | test warnings | test_turn_core.c:1049-1053, test_ai_contact_core.c:2357, test_units_core.c:3718-4161, test_regress_units.c:58, test_popup_msg.c:4,34, test_col1_save.c:115,695,1508, test_settings.c:328, test_stage_seams.c:312 | unused locals, sign-compare, ignored fread/system/fgets, strncat truncation |
| LOW | English fallback | turn_production.c:1037 | : "goods" |
| NOISE | -Wswitch-enum 1007, -Wlogical-op 11, cppcheck FPs units.c:205, units_combat_resolve.c:1276, europe_market.c:449, ai_diplo.c:3211, units_combat.c:3549, game_loop.c:187,2050, pedia.c:508 | |
Clean: no strcpy/strcat/sprintf/gets/alloca; allocs checked; fopen/fclose balanced; no leaks (cppcheck); % indices non-negative; snprintf accumulations bounded.

## Slice B

| Sev | Area | file:line | Finding |
|---|---|---|---|
| MED | id-as-slot | ai_euro.c:59,66,93,104,1644; ai_euro_expand.c:421 | 6 per-unit side arrays sized COLONIZE_UNITS_MAX(4096) indexed by unit id; ids monotonic never recycled (units.c:640) so >4096 spawns silently disables latches; ai_euro_s_20e6_wagon_errand is save-persistent +0x3158 (col1_bridge.c:1785/3057) — lost on save. Guards at expand.c:450, ship.c:524,729,776,794,1020,1161, land.c:954,2600,2772,2984,3106,3346,3484,3609,4068,4119 |
| MED | dead state | ai_euro.c:59,66,68,104 | write-only latches: ai_euro_s_unloaded_this_turn (ai_euro.c:541), ai_euro_s_euro_roam_wander (expand.c:85, land.c:3610), ai_euro_s_founded_colony_turn (expand.c:693); ai_euro_s_deferred_found never written |
| MED | dead code | ai_euro.c:624,1104; ai_euro_expand.c:95; ai.c:240 | zero callers: ai_euro_colony_food_short (invented pop*2), ai_euro_colony_wants_construction_labor, ai_euro_tiles_near, ai_coarse_fog_explore_byte; ship.c:701,1351 comments still cite ai_euro_tiles_near |
| MED | dead code | ai_internal.h:123,153,158,209 -> ai_indian.c:847,937,975,868 | ai_lab_54f5_gate, ai_native_foreign_euro_pull, ai_native_foreign_euro_pull_open, ai_quiet_fog_explore_ex no callers |
| MED | stand-in kept | ai_euro_colony_jobs.c:3399-3459 | invented "leftovers" field arm (best.yield>=2) still runs under AI_5952_INDOOR=0; comment says building pass not ported, header :1231 says it retires this arm |
| MED | DOS text | ai_euro_land.c:4106 | @VIOLATE fallback "the frontier" typed English |
| MED | invented constant | ai_euro_land.c:4068-4070; ai_euro.c:88-93 | @VIOLATE 10-turn per-unit cooldown + nearest colony of any nation, "own addition"; no known_divergences entry |
| MED | gold rule | ai_euro_europe.c:185,198,918,1011,1268,1296,1330,1431,1487,1510,1751; ai_euro_ship.c:386 | direct col1->nation[n].gold read/write incl. nat->gold = floor (:198), clamp :1011 |
| MED | length | ai_euro_colony_jobs.c:2983-3462, 2161-2573; ai_popup_render.c:379-783 | ai_euro_colony_tick_run 480, ai_euro_5952_build_cascade_for_owner 413, ai_popup_render 405; ai_euro_move_scoring_gate land.c:3643 391 |
| MED | reset hygiene | ai.c:150 | s_ai_coarse_fog (DS:0x9faa) not reset by any lifecycle hook; carries across games in session |
| LOW | reset | ai_euro_colony_jobs.c:2100 | s_5952_arty_plot not in ai_euro_reset (written :3041, consumed :2263) |
| LOW | stale/wasted | ai_euro.c:1560-1571 | loop over 4096 ids clearing wagon-errand "for reused id"; ids never reused |
| LOW | naming | ai_goals.c:1266-1285, ai_goals.h:333 | ai_goals_filter_profession_by_distance_wealth: param unit_index receives id; profession/has_context receive owner/profession |
| LOW | dup drift | ai_euro_land.c:1221,1242,1285; ai_euro_ship.c:1152,2156; ai_goals.c:775,1161 +10 | 17 hand-rolled (ColonizeWorld){} literals vs world_from_turn_ctx; disagree on col1_ok gating |
| LOW | dup | ai_euro_goals.c:1552,2596; ai_euro_ship.c:1654,2280; ai_euro_europe.c:947 | 5 hand insertion sorts by tile_stack_order; units_tile_stack_sort_at exists |
| LOW | dup helpers | ai_euro_land.c:790-805 vs ai_euro_europe.c:590 | two DOS @UNIT code resolvers, miss -1 vs 0xff |
| LOW | redundant | ai_euro_europe.c:601-604 | fallback repeats identical call |
| LOW | near-dup | ai_euro_land.c:3343-3356 vs 3481-3494 | same 12-line prologue |
| LOW | test-only exports | ai_euro.h:120 ai_euro_5d04_dos_type_code; ai_euro_28c8_colonist_job_score_structural; colony_jobs.c:2155 ai_euro_5952_build_cascade; internal.h:212 ai_euro_colony_tick_28c8_reassign; land.c:3332 ai_euro_20e6_457e_hs_cadence; ai_goals_upsert_secondary/_plan_scratch/_stack_settler_pick | not behind COLONIZE_INTERNAL |
| LOW | dup decls | ai_euro_internal.h:93/386,198/490,212/515,462/530,463/557 | declared twice |
| LOW | hygiene | ai.c:1-27 vs 39-62; 64-82 | include block + Sections index duplicated; 2nd index stale (cites ~4640, file 1523 lines) |
| LOW | dup API | ai.c:218 vs 227 | ai_coarse_fog_unseen == ai_coarse_fog_explore_unseen |
| LOW | vestigial | ai.c:121-126 | k_new_country, k_default_leaders all "" |
| LOW | bypass setter | ai.c:1273-1276 | nation_id = 0 raw vs units_set_nation sibling |
| LOW | stale comments | ai_euro_expand.c:430-444; ai_euro_europe.c:577 | "file-local" but extern; "by name" lookup now by kind |
| LOW | doc drift | docs/debug_env_vars.md | 15 env vars cite pre-split ai_euro.c lines; AI_5952_INDOOR/AI_5952_BUILD_TRACE/AI_4D2E_TRACE wrong file; undocumented AI_20E6_SHIP_DUMP (ship.c:88), AI_20E6_TREASURE_CASH (land.c:2758), AI_20E6_HS_CADENCE (land.c:3217) |
| LOW | pointer reuse | ai_euro_land.c:4247-4256 | u used after combat+move without refetch (sea branch refetches :4243) |
| LOW | popup patch | ai_popup.c:864-899 | set_last_portrait/_graphic patch queue[count-1] even when enqueue dropped (queue full :83); callers ignore result (ai_contact.c:213, ai_king_audience.c:621, ai_diplo.c:1704) |
INFO: logic_map check 0 errors; no id-walk loops; no shims; ctx write-back clean.

## Slice C

| Sev | Area | file:line | Finding |
|---|---|---|---|
| HIGH | invented AI live | ai_contact_raid.c:1773-1872 (called ai_brave.c:1005 every Indian turn) | ai_contact_indian_raids "thin stand-in" after real 021a/465b pulse: ambush alarm>=55, march>=70, escort-follow MD 3/4/5 weights 1/2/3; no decomp citation; :584-589 band values 40/55/70/80; port_plan.md:450 says 2820/021a Done; not in known_divergences |
| HIGH | id/slot mix | ai_indian.c:955-958,979-1000 | ai_unit_index_on_tile returns slot (:841 return i) passed as unit id to combat_land_engage/combat_unit_base_x8 (combat_strength.c:162 units_get_const) — latent, functions dead |
| MED | gold read | ai_diplo.c:1431,1031 | 153e clamp reads nat_self->gold (human record, lags eu->gold); 00f8 rank reads nat->gold; :2119 uses accessor correctly |
| MED | dead destructive | ai_diplo.c:151-168,782-842,3312-3314 | ai_diplo_make_peace -> war_embargo_lift_if_peace clears all 16 boycott_bitmap bits (tea-party ai_king_audience.c:414 same field); only reachable from tests (DIPLO_PEACE popups are OK-only) |
| MED | DOS text | ai_contact.c:663,284,298,305,127,922; ai_king_audience.c:226,864,884,1166; ai_king_war.c:697,714,1004,1131; ai_diplo.c:590,599; ai_contact_link_stubs.c:33 | ~17 typed English tokens/fallbacks: "settlement(s)" in @INDIANWELCOME, "Washington", "the colonies", "its colonies", "rival", "foreign", "Europeans", "natives", "goods" |
| MED | logic on English | ai_king_war.c:1393-1396 | strstr(ctx->status,"refuse"/"Audience"/"raises taxes"/"Tax stays"/"Congress notes") decides overwrite |
| MED | id-as-index 4096 | ai_brave.c:186-188,334-353; ai_contact_demand.c:1717 | Brave origin arrays [COLONIZE_UNITS_MAX] by unit id; ids monotonic; >4096 disables 022e visit/gift/reparations; ai_euro.c:1789 stalled[u->id] same |
| MED | native MP | ai_brave.c:642,689 vs units_capture.c:788-792 | stepper writes u->moves = spent+cost unclamped; units_mp_charge clamps at max; raw = max_mp exhaust writes at :399,523,705,710; ai_contact_raid.c:132 |
| MED | dead code | ai_contact_raid.c:57-191; ai_indian.c:847-866,868-906,937-1030; ai_diplo.c:194,216,2873; ai_contact.c:120,1167,1437; ai_king_war.c:1219 | 13 externs no production caller: ai_contact_indian_meet_trade (134 lines, uncited friction-- drip; indians.md:363 misattributes), none-at-all: ai_lab_54f5_gate, ai_quiet_fog_explore_ex, ai_native_foreign_euro_pull(_open); test-only: ai_diplo_indian_any_at_war, ai_diplo_indian_hostility_set, ai_diplo_military_score, ai_contact_last_raid_kind, ai_contact_try_village_meet, ai_contact_try_ship_village, ai_king_nation_turn, ai_contact_auto_buy_2e92; _pull_open:950 makes :959 unreachable |
| MED | invented chrome | ai_diplo.c:351-382,207-257 | indian_matrix_tick INFO popups "Natives grow hostile." etc from fandom; DEEP band AI_DIPLO_INDIAN_VERY_LOW_REL 16 "(OpenCol)"; likely dead (caller AI-only); s_indian_hostility_sticky never reset |
| LOW | stale stub | ai_contact_link_stubs.c:28 | turn_rank_loaded stub: no slim caller |
| LOW | stale header | ai_contact_link_stubs.c:2-4 | "four slim targets" vs 10 |
| LOW | stale comments | ai_contact_raid.c:662,1843,72; ai_contact_demand.c:2002 | 021a/2820 "PARKED" |
| LOW | stale comment | ai_diplo.c:2856-2863 | land_combat_strength "no live writer" — col1_stuff_census_4962_w refreshes (turn.c:689,846) |
| LOW | human test | ai_contact.c:1239; ai_contact_actions.c:217; ai_contact.c:418 vs :133-144 | control==0 direct vs ai_contact_euro_is_human honoring ctx->human_nation override; alarm_delta_00f2 vs _w differ on @INDIANBURN |
| LOW | dup blocks | ai_king_offers.c:268-280 vs 657-668; ai_king_war.c:1261-1284 vs 1418-1440 | merc dialog fill x2; SOONRETIRING0/1 emitters ~20 lines each |
| LOW | dead store/RNG | ai_contact_raid.c:1785-1787,1849-1854 | rng = ctx->rng?..:&local then rng=&local; follow-step worlds pass ctx->rng hand-built |
| LOW | unread ctx | ai_king_internal.h:19 / ai_king_ref.c:1045 | ai_king_0982_ctx.total written never read |
| LOW | seam | ai_contact_internal.h:294 / ai_contact_demand.c:1410 | ai_contact_visit_mood_beg_conceded only used in defining file |
| LOW | misplaced | ai_brave.c:1026 | col1_kill_indian_nation_w in ai_brave.c, derefs w before NULL check; caller game_loop_menus.c:138 |
| LOW | orphan comments | ai_contact.c:289-291; ai_king.h:16; ai_indian.c:906-932 | describe absent functions / wrong location |
| LOW | size | ai_contact_trade.c:259 (340), ai_diplo.c:2196 (292), ai_king_audience.c:904 (289) | >300 but <400 |
Clean: no id-walk, gold writes via accessor, EuropeScreen borrows gated, alarm drips only 152e, 021a write-back ok, layering ok, VTIN bounds ok.

## Slice D

| Sev | Area | file:line | Finding |
|---|---|---|---|
| HIGH | size | turn_production.c:516-2072 | turn_produce_one_colony 1557 lines, no stage split |
| HIGH | size | units_move.c:99-966 | units_try_move_w 868 lines |
| LOW | size | units_capture.c:346 units_enter_probe_w 387; units_combat_resolve.c:171 372; colony_yield.c:260 342; units_combat.c:1173 335 | >300 |
| MED | dup drift SoL | colony_production.c:281-283,300-302 vs combat_strength.c:648-668 | rebel_divisor==0: colony_prod_sol_percent returns 0 w/o Bolivar; combat copy returns 0+Bolivar; DOS FUN_15eb_0274 (raw 9471-9501) adds 0x14 — production copy wrong; inline justification stale (colony_production.c already in SLIM) |
| LOW | misleading | colony_production.h:129-133 | says liberty_bells_total/4 stand-in fallback; deleted in .c (:289-299) |
| MED | dup helper | unit_stack.c:72-91 vs units_cargo.c:1425-1448 | unit_stack_job_field0 copy of units_job_field(names,prof,0); both reimplement assets_msg_row_field (assets.h:97); col always 0 |
| MED | global vs ctx | units_map.c:884; units_move.c:165-348,394,442-444,485,819; units_capture.c:113-219,256 | g_units_ff_col1 92 refs; _w functions ignore w->col1; ~14 const-cast writes through it |
| MED | world plumbing | units_move.c:175,651,654,941,1429; units_pioneer.c:633,680; units_cargo.c:709,894,954; units_capture.c:747 | _w functions rebuild partial (ColonizeWorld){units,colonies,map} dropping col1/rng/europe; architecture.md:186-188 claims pass-through |
| MED | reset coverage | units_combat.c:4045-4065 | units_reset_hooks misses s_spawn_gate_fn/_user, s_spawn_refused_fn (units.c:35-37); g_units_combat_popups/_europe/_game_txt/_colonies/_human_nation (units_map.c:888-892); g_units_fallout_col1/_map (:885-886); g_combat_analysis_presenter (combat_analysis.c:13) |
| LOW | reset | turn_production.c:247; turn.c:1010,1089-1097 | turn_labels never cleared; g_turn_1528_last_walked_colony (turn_colony.c:31) not in turn_reset |
| LOW | dead exports | units_cargo.c:855 units_pick_landfall_tile_w, :922 units_landfall_unload_all_w; units_combat.c:2613 units_drake_scale_strength; units_capture.c:901 units_can_afford_move_cost; colony_build.c:930 colonies_has_building_name_contains, :1058 colonies_building_chain_length; colony_craft.c:90 colony_craft_recipe_at; units.c:44 units_unlimited, :523/:531 units_type_is_military/_native | 11 dead; founding_fathers.md:136, move_enter.md:82, port_plan.md:267 cite two as live |
| LOW | diag-only | units_move.c:1980-2090,2776-2784 | units_bfs_next_step only UNITS_FAR_BFS=1; debug_env_vars.md:22 cites units.c:9804 stale |
| MED | name-keyed | colony_build.c:1092-1269,1073-1089,918-928 | colonies_building_is_buildable 40x colonies_building_name_row strcmp; colonies_building_type_row/colonies_has_building_row exist (colony.c:367,357); miss "" feeds has_building_named("") |
| LOW | name-keyed | colony_production.c:645,671 (+crosses, manufacturing_output/_input) | name->row wrappers; only caller ai_euro_colony_jobs.c:793-817,1066-1068 |
| LOW | name-keyed | colony.c:475 colonies_find_building | strcmp; callers game_dialogs.c:2649, ai_euro_goals.c:2115 convert row->name->row |
| LOW | typed fallback | turn_production.c:497-507,1697-1704 | popup_msg_fill fallbacks "Mine depleted near %s.", "%s already built." |
| LOW | shims test-only | colony_workers.c:563 colonies_assign_field; units_combat_resolve.c:582 units_resolve_naval_combat; units.c:1573 units_brewster_apply_popup | no production callers; Brewster shim .rng=NULL |
| LOW | MP bypass | turn.c:129,345; turn_colony.c:1075; colony_workers.c:1309; units_cargo.c:368,1214 | inline mp_restore; moves = 0 direct (Euro paths) |
| LOW | dup lookup | colony.c:1146,1184; colony_build.c:150; colony_production.c:276; combat_strength.c:650 | 5 hand "col1 colony at xy" scans; col1_colony_at_xy/col1_save_colony_at exist |
| LOW | dup idiom | turn_production.c x3, colony_production.c x3, colony_preview.c, turn_colony.c +5 | colonist_count>0?colonist_count:population 13x |
| LOW | parallel pipeline | colony_preview.c:12-215 | preview re-runs production sequence; "must match turn.c exactly" |
| LOW | const casts | units_move.c(9), units_combat_resolve.c(5), units_capture.c(4), units_combat.c(4), turn_production.c:496, colony.c:1059 | ~31 const-stripping casts |
| LOW | port clamp | turn_colony.c:106-111 vs units_combat.c:2144 | unit_type_counts ++ saturates 0xff (port add); -- unguarded |
| LOW | naming | ff.c/ff.h | font loader prefix collides with FF_* Founding Father enum |
| LOW | stale comments | founding_fathers.c:19,27 | "stand-ins" but DOS-LITERAL; orphan comment :27 |
| LOW | nav | founding_fathers.c(1553), combat_strength.c(1126), colony_production.c(1026), colony_yield.c(939) | no Sections: index |
Clean: id-walk, _id/_index, static collisions, job tables bounds, declared-undefined.

## Slice E

| Sev | Area | file:line | Finding |
|---|---|---|---|
| HIGH | json/settings | json_min.c:81-135 (writer :512-528) | parser re-encodes raw bytes >=0x80 as 2-byte UTF-8, writer emits raw: non-ASCII strings double-encoded each load/save cycle; settings data_dir/save_dir/soundfont with non-ASCII path corrupted; sav_json CP437 names too; settings.c:250,253 / 410-416 |
| MED | buffer overflow | col1_bridge.c:2544 | str_copy_trunc(dst->name, sizeof(src->name), ...) — dst COL1 name[24] (col1_save_layout.h:398), src runtime name[28] (colony.h:22): up to 4 bytes spill into nation_id/ai_flags/flags(+0x1c coastal 0x40)/+0x1d; latent (name entry caps 23) |
| MED | portability | dos_rng.c:1,:103 | <execinfo.h> + backtrace() unguarded in sim file (mingw breaks); getenv("RANGEDBG") uncached per draw; dup includes; af9b7615 |
| MED | size | col1_bridge.c:905-2141, 2304-3562 | col1_bridge_apply_w 1236, col1_bridge_capture_w 1258 lines |
| MED | helper drift | europe.c:756-778 vs europe_market.c:475, europe_harbor.c:552, units.c:1208 | europe_credit_sale_tax returns early tax_paid<=0 before #1021 nation+0x26 net-income add; 3 inline copies always add |
| MED | modal gate | game_loop.c:148-162,:3064; game_loop_update.c:960 | map pull-down (map_menu.open_index>=0) click-toggled, not in GAME_MODAL_LIST/game_screen_owns_display/goto pacer map_visible; goto pacing + turn hand-off run behind dropdown |
| LOW | gold | europe_dock.c:145,299,376,386; europe_market.c:1457,652 | eu->gold -=/+= skip europe_purse_move (no record restamp); :652 nat->gold += gained unclamped |
| LOW | dup | europe.c:768, europe_market.c:475, europe_harbor.c:552, units.c:1208 | int32 LE read-add-write of unknown24_pad x4; col1_nation_bank_4a pattern exists |
| LOW | sentinel | europe_dock.c:626,664,688; col1_bridge.c:1595,2764,3267 vs europe_harbor.c:1601,1643; col1_bridge.c:3304,3340 | dock sentinel (236,236) vs 236+n, no named constant |
| LOW | layer3 | col1_bridge.c:751,790; map.c:671,1246,1258; ai_king_ref.c:218; ai_euro_goals.c:769 | hand nibble encode/decode though MAP_L3_*/map_set_owner_nibble/map_tile_is_lake exist |
| LOW | layering hygiene | europe.c, europe_pool.c, europe_dock.c, europe_harbor.c, europe_market.c | sim files include europe_art.h, ui_button.h, reports.h (stale, unused); europe.c includes platform.h twice (:31,:67), re-declares europe_purse_nation/_move (:62-66 vs europe_internal.h:69-70) |
| LOW | dead decls | game_loop.c:1171-1216 | ~45 lines prototypes already in game_dialogs.h |
| LOW | dead API | game_loop.c:3107 / game_loop.h:129 | game_colony_pos no caller |
| LOW | misleading/dead tables | game_loop.c:172; new_game.c:56-73,865,890 | "built-in English" comment, callers pass ""; k_difficul_levels, k_nation_bonuses, k_finished_label, k_customiz_* all "" |
| LOW | dead code | map_gen.c:11-12,426-1480 | 12 #if MAPGEN_STAGE_DIAG blocks (never set); histogram fprintf copy-pasted ~5x (1402/1427/1465) |
| LOW | dup | game_loop*.c, game_dialogs.c 52 sites (game_loop_orders.c:244, game_loop_eot.c:1211, game_dialogs.c:421) | inline &(ColonizeWorld){...} from game 52x; no game_world(game) helper; 7 hard-code .col1_ok=true |
| LOW | dup drift | game_loop.c (10x e.g. 1591,1665,2302), game_loop_menus.c, game_dialogs.c, game_loop_update.c | popup font fallback chain 13x; final fallback colony_font vs hud_font differs |
| LOW | dup drift | game_loop_colony.c:843-856 vs 999-1010 | @NODOCKS Fisherman block dup; only first calls colony_screen_set_status; colonies_profession_name vs colony_yield_job_name |
| LOW | const lies | game_loop.c:1479,1534,1599,1676,1704,1799,2307; game_loop_render.c:1468 | render paths cast const away to mutate ai_popups/new_game/europe (11) |
| LOW | parser | json_min.c:330-420,441-455 | unbounded recursion; json_get_i64/u64 cast out-of-range double (UB); \u0000 truncates; surrogates CESU-8 |
| LOW | file IO | game_loop_eot.c:1028-1049,989-1003 | game_hof_save truncates in place, unchecked fprintf/fclose; sscanf widths hard-coded; empty leader field drops row |
| LOW | silent loss | col1_save.c:959-969; col1_bridge.c:3547,3554 | UNXT tail malloc/fread failure drops units >300 silently; col1_save_ext_put results for VTIN/BQUE ignored |
| LOW | redundant | game_loop_update.c:156; game_loop.c:1433 | woodcut.open/ai_popups.open re-tested alongside game_modal_open |
| LOW | no-op | savegame.c:206-208 | if (leader_name[0]=='\0') leader_name[0]='\0' |
Clean: modal list (14 dialogs), no rand(), no head.unknown* writes, save round trip symmetric, no id-walk.

## Slice F

| Severity | Area | file:line | Finding | Evidence | Confidence |
|---|---|---|---|---|---|
| HIGH | Big-function | src/core/map_panel.c:1220-1891 | map_panel_render_w 672 lines | closing } at 1891 | HIGH |
| HIGH | Big-function | src/core/colony_screen_popups.c:645-1166 | colony_screen_hit_test 522 lines | | HIGH |
| MED | Big-function | colony_screen_panels.c:469-822; reports_congress.c:156-469 | colony_screen_draw_multifunction 354, reports_render_congress_page1 314 (>300 claim) | | HIGH |
| MED | Audio race at exit | src/main.c:323-324, sound.c:643-675 | sound_shutdown() destroys g_sound.lock while SDL audio device still open; callback may lock destroyed mutex | platform_destroy (sdl_runtime.c:316) holds only SDL_CloseAudioDevice; sound_render_s16 checks inited before lock (sound.c:1166-1170) | HIGH |
| MED | DOS text | reports_score.c:514 | "of the" hard-coded; same file reads LABELS @MISC 19 at :820 | "%s %s of the %s:  %s %d" | HIGH |
| MED | DOS text | reports_score.c:686 | "Liberty Bells:  +%d" compiled in; NAMES.TXT:290 has it | | HIGH |
| MED | DOS text | save_load_dialog.c:57 | "(EMPTY)" compiled in; DS:0x20ee; reports_ds_string exists (reports_names.c:589) | | HIGH |
| MED | Invented constants | colony_screen_panels.c:680-681 | BUY/CHANGE button RGB (20,40,120)/(180,200,255) no citation | | MED |
| MED | OOB malformed asset | gsound_vm.c:80,102-107,175-200,858 | ds[0x8300] indexed by arbitrary u16 from image | | MED |
| LOW | Input loss | sdl_runtime.c:515-517 | each KEYDOWN overwrites last_key; unmapped key erases mapped one same frame | | MED |
| LOW | UTF-8 | sdl_runtime.c:533-537 | only text[0] of TEXTINPUT kept; stray lead byte | | HIGH |
| LOW | Leak | sdl_runtime.c:569-573 | SDL_CreateSystemCursor leaked when default_cursor NULL (comment admits) | | HIGH |
| LOW | Duplicate | popup.c:25-39 | popup_nearest_rgb copy of assets_palette_nearest_rgb (assets.c:244) | | HIGH |
| LOW | Duplicate primitives | colony_screen_draw.c:82-118,154-169; unit_chrome_draw.c:392-401; colony_chrome.c:168-178; reports.c:584-593; map_panel.c:1283-1287 | hand-rolled clipped line/rect/outline loops duplicating fb.c; selection_box == fb_rect_outline | | HIGH |
| LOW | Copy drift | cheat_list_dialog.c:118-153 vs 340-365 | SETVIEW/SETHUMAN parsers same loop, overflow id -2 vs -1, @width= handled in one only | | MED |
| LOW | Dead | cheat_list_dialog.c:96-117 | SETVIEW section-missing fallback unreachable (DEBUG.TXT required) | | HIGH |
| LOW | Dead | name_entry_dialog.c:157 | draws empty string | | HIGH |
| LOW | Dead param | pedia.c:848, pedia.h:57-58 | PEDIA_LIST_* macros both "" | | HIGH |
| LOW | Fallback text | pedia.c:560,898,906,943 | miss prints "#%d"/"Skill %d" not empty | | MED |
| LOW | Misleading | reports_score.c:495,522 | (void)turn_number yet used at :522 | | HIGH |
| LOW | Copy drift | ui_drag.c:66-74 vs 109-114 | arrow cursor clears every 0x09 pixel, sprite cursor only two corners | | MED |
| LOW | Dead exports | sound.c:686,1226; options_dialog.c:20 | sound_backend_name, sound_sfx_sample no callers; options_dialog_close could be static | | HIGH |
| LOW | Unused param | unit_chrome_draw.c:216-220 | shadow_color always 0 | | MED |
| LOW | Built-in fallback data | closing.c:207-220; opening.c:515-529 | compiled-in copies of CLOSING/OPENING.TXT timelines | | MED |
| LOW | Stale cache | sound.c:543-548 | song->id set before gsound_vm_create; failure caches empty song | | MED |
| LOW | Dead branch | ss.c:95-97 | zero-size memset unreachable (ss_load skips w/h==0 at :291) | | HIGH |
| LOW | Uncapped alloc | madspack.c:227 | malloc(uncompressed_size) u32 from header, no cap; size 0 reported as oom | | MED |
| LOW | Redundant gate | game_loop.c:1433-1434 | woodcut.open||closing.open repeats game_modal_open | | HIGH |
INFO: modal coverage complete; parsers/fb clipping clean; tsf.h only CRLF diff; no static-name collisions in src/core.

## Slice G

| Sev | Area | file:line | Finding | Evidence |
|---|---|---|---|---|
| HIGH | docs claim | docs/conventions.md:131-132 | ">300 lines split" claim false; 8 functions >400: turn_produce_one_colony turn_production.c:516 (~1550), col1_bridge_capture_w col1_bridge.c:2305 (~1250), col1_bridge_apply_w col1_bridge.c:905 (~1230), units_try_move_w units_move.c:~99 (~860), map_panel.c:~1230 (~650), colony_screen_hit_test (~515), ai_euro_colony_tick_run ai_euro_colony_jobs.c:2983 (~480), ai_euro_5952_build_cascade_for_owner :2163 (~410); 9 more 300-400 | brace scan |
| HIGH | fixture id-vs-slot | tests/unit/test_ai_diplo.c:745-746 | units.units[sid].nation_id = 0; units.units[ship] — slot write by id | |
| HIGH | CRLF | CMakePresets.json; tools/address_mapping.csv; data/soundfonts/COPYRIGHT.Roland_SC-55 | i/crlf in index | git ls-files --eol |
| MED | build | CMakePresets.json:2-6 | schema version 6 needs CMake 3.25; min declared 3.20 | |
| MED | fixture uninit pool | test_turn_core.c:3974; test_units_core.c:7377; test_ai_euro_5d04_hire.c:878,913 | stack ColonizeUnitPool only units_reset, no memset; types garbage; test_turn_core never sets type_count | units.c:318-330 |
| MED | fixture hand-filled | test_colonies.c:3319-3324 | slot 0 active, id 0, slot_end=1 by hand, no units_pool_sync | |
| MED | id-walk | test_col1_save.c:3449,3607,3864,4316,4553 | for i<COLONIZE_UNITS_MAX units_get_const(&units2,i) | 5 loops |
| MED | docs claim | conventions.md:312; CLAUDE.md:21,77 | COLONIZE_TEST_ONLY "any test binary" false: 19 test sources own main(), ignore CASE= (test_units_{combat,movement,pioneer,ships,types}, test_col1_save, test_founding_fathers, test_ai_indian_census, test_settings, smoke_{ai_mid01,ai_late01,game_flow,pick_music,play,sound}, golden_{colony_preview01,idle_campaign,mapgen_seed100,woi_ref01}) | no TEST_MAIN |
| MED | Makefile | Makefile:18-21 | make test T= runs binary without COLONIZE_UNITS_STRICT=1 (ctest sets, CMakeLists:430); golden_ai_joint same gap | |
| MED | tracked test output | test-assets-hof/, test-assets-msg/, test-assets-newgame/ (41 files) | regenerated every run but committed; mixed CRLF; .gitignore only test-assets/ | test_hall_of_fame.c:28, test_new_game.c:12, smoke/test_assets.c:29 |
| MED | tool writes tracked | tools/trcmp.py:63 | sav_json with no output arg writes .json beside input in tracked original_saves/independence-seed-100/tr9/ (17 files) | |
| MED | duplication | tests/unit 15 files | 15 copies of static int fail() identical to tests/common/test_fail.h | test_ai_euro_20e6.c:27, test_regress_trade_raid.c:20, test_ai_diplo.c:84, test_stage_seams.c:29, test_ai_king_common.h:75, test_ai_contact_common.h:27 … |
| MED | duplication | tests 13 files ~105 sites | hand-rolled calloc/free of terrain/layer2/layer3 instead of fx_map_alloc/free; fx_map_free copy of map_free (map.c:793) | test_ai_king_revolution.c 56, test_units_combat.c 15, test_ai_diplo.c 12 |
| MED | dup with drift | tools/render_common.c:47-81; tools/ai_replay_main.c:~145-180; tests/common/golden_fixture.c:20-70 | three "load SAV into pools" sequences shadow game_apply_col1_save and differ (render_common skips units_reset/ai_euro_reset/turn_reset, uses europe_load; golden_fixture europe_load_tables + resets) | |
| MED | defines inconsistent | CMakeLists.txt SLIM ~609-690; smoke_sound/smoke_pick_music | direct-compiled src files miss colonize_sim PUBLIC defines: map_menu.c builds COLONIZE_DEBUG_MENU=0 in slim vs 1 elsewhere; sound.c without COLONIZE_HAS_TSF=0 when TSF off | map_menu.c:532,567 |
| LOW | fixture | test_ai_goals.c:284-287 | hand-filled unit id=0 | |
| LOW | dup | tools/render_common.c:10-22 | render_load_sheet copies cinematic_load_sheet (cinematic.h:176); 5 copies total (woodcut_present.c:33, declaration.c:54, new_game.c:213) | |
| LOW | CMake | CMakeLists.txt:687 | unit_reports lists col1_save.c twice | |
| LOW | stale comments | CMakeLists.txt:604-609; ai_contact_link_stubs.c:2; conventions.md:144,147-148 | "four slim targets"/"weak stubs": 10 SLIM targets, stubs strong; list omits unit_colony_yield | |
| LOW | stale comments | CMakeLists.txt:~1088,~1003-1008; tests/README.md:96 | "six COMMANDs" but golden_ai_joint runs 8; "no set_tests_properties/DISABLED" wrong | |
| LOW | docs | conventions.md:100-101,155-157 | stale counts ~2900 FUN_/~470 raw vs 5830/1874; "ai_euro.c" split into 8 | |
| LOW | docs | conventions.md:115-118,137-140 | game_update_ctx doesn't exist; COLONIZE_TESTING on colonize_sim not colonize_core (CMakeLists:251) | |
| LOW | doc length | docs/indians.md | 1018 lines | |
| LOW | doc placement | docs/smell_audit_2026-09-08.md, 09-09 | STATUS closed but not archived | |
| LOW | arch inventory | docs/architecture.md | never names cheat_list_dialog, howmuch_dialog, name_entry_dialog, cinematic.h, bytes.h, pick_music_ids.h | |
| LOW | stale refs | tests/README.md:203; docs/manual_gap.md:91 | cite pre-split test_ai_euro_expand.c; now test_ai_euro_expand_labor.c:361 | |
| LOW | bugs.md header | bugs.md:3 | "archive holds #1-#952" stale (up to #1085); next free 1086 correct | |
| LOW | tools README | tools/README.md:81-84 | "stdlib only" false (dosbox_trace.py pexpect, PIL); make_vr_sound.py, viceroy_v2_output_layout.json undocumented; check_save_interop.sh build/ fallback omitted | |
| LOW | stale tree | tools/check_save_interop.sh:22 | falls back to bare build/ | |
| LOW | hard paths | tools/trcmp.py:63-64; tests/unit/test_reports.c:1023 | /usr/bin/python3; /tmp/... | |
| LOW | silent pass | smoke/test_play_smoke.c:235; unit/test_stage_seams.c:236; unit/test_map_panel.c:681 | return 0 when COLONIZE/ absent | |
| LOW | fixture helper | tests/common/ai_fixture.h:24-44 | fx_map_alloc callocs layer3 (all-lake trap); leaks on partial failure | |
| LOW | disabled test | CMakeLists.txt:964-969 | golden_idle_independence DISABLED, no bugs.md row | |
Clean: no orphan tests, no missing sources, no #if 0, no broken doc links, Makefile targets exist, python paths exist, bugs ids consistent.

## Slice H


Scripts sit next to this file. Run them with `python3 sNN_*.py` from this dir; `cparse.py` is the shared comment/string stripper and function finder.
Every row is grep- or script-proven. Rows marked "verified" were also read in context.

| Severity | Area | file:line | Finding | Evidence | Confidence |
|---|---|---|---|---|---|
| HIGH | DOS text (S8) | src/core/turn_production.c:501,507 | GAME.TXT @DEPLETION wording is copied verbatim into a status line and used as the popup_msg_fill fallback. | `"Mine depleted near %s."` = GAME.TXT:626 `Mine depleted near %STRING0.`; present in `strings build/debug/opencol`. | HIGH (verified) |
| HIGH | DOS text (S8) | src/core/ai_king_war.c:131-141 | A typed English fallback paraphrases @KINGNEWWAR ("Veteran Soldier units") and goes into popup_msg_fill. The rule is that a catalog miss gives the empty string. | `"The Crown has declared war on the %s ... %d Veteran Soldier units."` is in the binary. | HIGH (verified) |
| HIGH | DOS text (S8) | src/core/europe_dock.c:363 | Status text "Purchase %s for %d$?" is the guarded needle `Purchase %STRING0 for %NUMBER0$?` with %-tokens swapped, so tests/no_catalog_text.cmake does not catch it. | s08c_catalog_copies.py matches GAME.TXT; the string is in the binary. | HIGH |
| HIGH | DOS text (S8) | src/core/units_combat_resolve.c:679; units_combat.c:2181; ai_contact_raid.c:1237; game_loop_saveload.c:538 | Four more GAME.TXT templates are compiled in as typed fallbacks or headers. | `"%s %s evades %s %s."`, `"%s %s sunk by %s %s!"`, `"%s %s %s %s near %s!"`, `"COLONIZATION RATING: %d%%"`: each matches a normalised GAME.TXT line and is in the binary. | HIGH |
| MED | DOS text guard (S8) | tests/no_catalog_text.cmake | The guard is a fixed list of 17 needles written in catalog spelling (%STRING0). It never normalises to printf form and never diffs the binary against COLONIZE/*.TXT, so all 6 copies above pass. | s08c_catalog_copies.py does that diff in about 20 lines and could serve as the guard. | HIGH |
| MED | DOS text (S8) | ai_contact.c:260; game_dialogs.c:1273; turn_production.c:1704; game_loop_eot.c:1753 | Other non-empty English fallbacks are fed to popup_msg_fill or used as name fallbacks. They are paraphrases, not exact copies. | `"The %s refuse %s."`, `"%s has already built a %s!"` (GAME.TXT: "it has already built one!"), `"%s already built."`, `ut->name : "unit"`. 8 var-fallback sites in s08b.out. | MED |
| LOW | DOS text (S8) | turn_production.c:845-1571 (8 lines); colony_screen_panels.c:805; reports_score.c:686; units_pioneer.c:289,576; ai_diplo.c:837; europe_market.c:510-515,1254; ai_king_war.c | 18 port-authored player-visible status strings contain catalog nouns ("Food shortage in %s.", "(Requires %d Tools)", "Liberty Bells:  +%d", "Select a Pioneer"). They are allowed as port notices but are the next candidates for catalog sourcing. | s08.out lists them. pedia.c:51-100 has 4 port-authored blurbs (documented as port-authored). | MED |
| HIGH | Function size (S1) | turn_production.c:516-2072 | turn_produce_one_colony is 1557 lines, about 4x the 400-line rule. conventions.md:115 still says every function over 300 lines was split. | s01_long_funcs.py; start and end lines checked. | HIGH (verified) |
| HIGH | Function size (S1) | col1_bridge.c:905-2141, 2304-3562 | col1_bridge_apply_w is 1237 lines and col1_bridge_capture_w is 1259. | s01_long_funcs.py | HIGH (verified) |
| MED | Function size (S1) | units_move.c:99 (868); map_panel.c:1220 (672); colony_screen_popups.c:645 (522); ai_euro_colony_jobs.c:2983 (480), :2161 (413); ai_popup_render.c:379 (405) | 6 more functions are over 400 lines. 12 functions sit at 300-400 (e.g. ai_euro_land.c:3643 392, units_capture.c:346 387, ai_brave.c:380 359). Nothing outside src/core is over 300. | s01_long_funcs.py. gsound_vm.c voice_tick (390) is exempt. | HIGH |
| LOW | Doc drift (S1) | docs/conventions.md:115 | The claim "Every function over 300 lines was split 2026-09-16" is false: 21 functions exceed 300 lines. | See the two S1 rows above. | HIGH |
| MED | Dead state (S2) | src/core/units_map.c:887,1230 | g_units_conquest_gold is written in units_set_native_fallout_context and never read. The conquest_gold parameter is dead, and test_units_core.c:2651 passes 500 to no effect. | `grep -n g_units_conquest_gold` gives 2 hits: the definition and one write. | HIGH (verified) |
| LOW | Dead statics (S2) | src/** | None found: 0 static functions and 0 static file-scope vars are referenced only by their definition (1354 static fns scanned). 66 static tables have exactly one use, which is normal. | s02_dead_statics.py prints nothing (consistent with -Wall). | HIGH |
| LOW | Decl/def (S3) | src/core/*.c, *.h | No real orphans. Every non-static function in src/core has a declaring header, and every src/core/*.h prototype has a definition. The 20 apparent misses are macro-generated (UNITS_TYPE_PREDICATE, units.c:463) or parser artefacts after macros. No `extern` in any .c file. All 112 header externs are defined in src/. | s03_decl_def.py, s15_extern_vars.py | HIGH |
| MED | Compat shims (S4) | colony_workers.c:563 colonies_assign_field; units.c:1573 units_brewster_apply_popup; units_combat_resolve.c:583 units_resolve_naval_combat | Three pool-signature shims that forward to *_w have zero production callers; only tests call them (10, 2, 2 call sites). Under the shim rule, move the tests to *_w and delete the shims. | s04c_worldmake_shims.py: `prod=0`. | HIGH |
| LOW | Compat shims (S4) | game_loop.c:702 game_move_watch; units_combat_resolve.c:35 units_resolve_land_combat (3 prod callers: ai_contact_raid.c:1355, ai_euro_land.c:4247, ai_brave.c:584) | These shims still have production callers. game_move_watch is documented as kept (callback typedef). | s04c output | HIGH |
| MED | Id-keyed array OOB (S5) | src/core/ai_euro_act.c:917 | `ai_euro_s_20e6_wagon_errand[id] = 0` is written with no `id < COLONIZE_UNITS_MAX` check. The array is [4096]. Unit ids come from `pool->next_id++` (units.c:640), are never recycled within a session, and are hashed (id_slot), so they can exceed 4096. The 15 sibling sites bound-check (ai_euro_ship.c:524, ai_euro_land.c:3106, ai_euro_expand.c:530, getters at ai_euro_expand.c:449/456). | s05b_id_keyed_arrays.py; `const int id = u->id;` at ai_euro_act.c:911. | MED (whether a session reaches 4096 ids is unproven) |
| LOW | id-walk (S5) | ai_euro.c:1567; ai_euro_expand.c:743; ai_euro_europe.c:553,575 | No id-walk bugs: the only `units_get*(…, i)` loop is a documented id-space walk. ai_euro_expand.c:743 is a slot walk but names its index `uid`, which breaks the _id/_index naming rule. ai_euro_europe.c `idx` is a slot. | s05_idwalk.py | HIGH |
| MED | Gold writes (S6) | ai_euro_europe.c:185,198,918,1011,1268,1296,1330,1431,1487,1510,1751; ai_euro_ship.c:386; europe_market.c:652 | 13 direct `nat->gold ±=` writes to the col1 nation record bypass europe_nation_gold_add. This is only equivalent while the nation is never the human (the human's purse is eu->gold). | s06_gold_writes.py | HIGH (that they bypass); MED (that any is reachable for the human) |
| LOW | Gold writes (S6) | europe_dock.c:145,299,376,386; europe_market.c:1457; europe_pool.c:899; ai.c:376,517; col1_bridge.c:2003,2457; game_dialogs.c:2382 | 11 more direct purse writes in the purse-owning module, new-game setup, the save bridge, and one documented stamp/pay/pull (game_dialogs.c:2376 comment). Listed for completeness. | s06_gold_writes.py | MED |
| MED | MP writes (S7) | ai_brave.c:399,523,642,689,705,710 | Brave MP is hand-written as thirds spent (`u->moves = spent + cost`, `= max_mp`), duplicating units_mp_charge/units_mp_exhaust logic. The invariant says natives route through those helpers. | s07_moves_writes.py | HIGH |
| LOW | MP writes (S7) | 78 writes in 28 files outside units.c (ai_euro_act.c 8, units_cargo.c 6, ai_king_ref.c 5, col1_bridge.c 5, game_loop_update.c 4, …) | Most are `= 0`, which is exhaust only for Euro units. The sites were not checked for whether the unit can be native. Examples: game_dialogs.c:2726, ai_contact_actions.c:1972, game_loop_orders.c:888. | s07.out | MED |
| LOW | Doc drift (S7) | docs/conventions.md (MP invariant row) | The doc says units_mp_charge/units_mp_exhaust live in units.c. They are in units_capture.c:789-832. | s07.out `[units_mp_charge] src/core/units_capture.c:789` | HIGH |
| LOW | DOS-LITERAL tag (S9) | ai_euro_colony_jobs.c:59,2715; ai_euro.c:1181,1202; europe_harbor.c:1714; map_panel.h:25 (+ combat_strength.c:1114 "DOS-LITERALLY") | 358 canonical tags. 6 src variants (`DOS-literal` ×5, `DOS literal` ×1) escape the documented exact-case grep, plus 4 more in tests/tools. Every canonical tag has a FUN_/raw/asm/md citation within the function (ai_euro_colony_jobs.c:2813 cites only a few lines up). | `grep -rnoiE 'dos[ _-]literal' \| grep -v DOS-LITERAL`; s09_doslit_nocite.py | HIGH |
| MED | Line endings (S10) | tools/address_mapping.csv; CMakePresets.json | Both are committed with CRLF in the index (`i/crlf`) despite `* text=auto eol=lf`. | `git ls-files --eol \| grep i/crlf` | HIGH |
| LOW | Line endings (S10) | test-assets-hof/* (5), test-assets-newgame/* (5), test-assets-msg/GAME.TXT, data/soundfonts/COPYRIGHT.Roland_SC-55 | 12 DOS fixture files are i/crlf. This is probably intended (DOS assets) but they are not marked `-text`, so a renormalise would rewrite them. | `git ls-files --eol` | MED |
| LOW | Encoding (S10) | src tests tools docs | 0 UTF-8 BOMs and 0 tab-indented lines in src/. Trailing whitespace is in 5 files: tools/address_mapping.csv (2813 lines, all of them CR), docs/turn_between_players.md 3, docs/archive/bugs_closed.md 3, docs/assets.md 1, docs/ai_euro_logic_map.yaml 1. | grep -c | HIGH |
| LOW | Layering (S11) | colonize_sim files | No sim .c calls or takes the address of a function defined only in a colonize_ui .c. Sim files do include UI-named headers for types: font.h 6, ss.h 10, ui_button.h 6, europe_art.h 5, new_game.h 6, fb.h 1 (unit_chrome.c), unit_chrome_draw.h 1, woodcut_present.h 1, ai_popup_render.h + map_menu.h (ai_popup.c), reports.h 46. That is compile-time coupling only. | s11_layering.py | HIGH |
| LOW | Weak / ctor (S12) | tests/common/test_name_kinds.c:141-149; ai_contact_raid.c:1032; units_map.c:170 | No weak definitions in src/. tests/common has 4 weak extern references, which become NULL if the archive member is not pulled (the same trap class as the old weak stubs). ai_contact_raid.c registers its raid-repelled hook from `__attribute__((constructor))`, so in any link that does not pull ai_contact_raid.o the hook silently stays unset. | grep `__attribute__((weak\|constructor` | HIGH (that they exist); LOW (impact) |
| LOW | TODO inventory (S13) | src/, tests/, tools/ | 0 TODO/FIXME/XXX/HACK/TBD markers. src/ cites 530 distinct `bugs.md #NNN` ids: 529 are CLOSED and #974 (reports_military.c:125) appears in none of bugs.md, bugs_fixed_pending.md or bugs_closed.md (it is only mentioned inside #1071's text). | s13_bugrefs.sh | HIGH |
| LOW | Magic numbers (S14) | gsound_vm.c 32; ai_euro_colony_jobs.c 20; ai_euro_europe.c 19; map_gen.c 7; col1_save.c 6 | 161 hex literals ≥0x100 have no FUN_/raw/DS:/hex/asm citation on the same line or the 2 lines above. Most sit inside cited functions (e.g. ai_contact_trade.c:369 `dos_rng_range(…,1,0x148)`, ai_euro_colony_jobs.c:899 `year < 0x604`, :2127 `addr >= 0x924c`). Informational. | s14_magic.py | LOW |
| LOW | Header hygiene (S15) | src/**/*.h | All headers have guards and guard names are unique. No .c is included. No struct is defined twice; the 10 repeated `typedef struct X X;` are legal C11 forward declarations. | s15 greps | HIGH |
| MED | Globals (S16) | units_map.c (23); ai_contact.c 7; ai.c 6; ai_euro.c 6; colony.c 5; turn_production.c 5 | There are 89 file-scope non-static, non-const variables in 26 src/core files. units_map.c holds the combat/fallout context (g_units_combat_colonies, _human_nation, _popups, _game_txt, _europe, g_units_fallout_*), which setters inject instead of ctx. One of these is already dead (g_units_conquest_gold). | s16_globals.py | HIGH (count); MED (should live in ctx) |

## Summary
1. Catalog wording in the binary: 6 GAME.TXT templates are compiled into opencol verbatim (Mine depleted, Purchase…?, evades, sunk by, near, COLONIZATION RATING) plus a @KINGNEWWAR paraphrase. The cmake guard misses all of them because it checks fixed needles in %STRING spelling.
2. A catalog-vs-binary diff (s08c_catalog_copies.py) would make a far stronger guard than the needle list.
3. Big-function rule: 9 functions over 400 lines (turn_produce_one_colony 1557, col1_bridge apply/capture about 1250 each, units_try_move_w 868), and conventions.md still claims all were split.
4. One unchecked id-indexed array write (ai_euro_act.c:917). Unit ids are never recycled, so a long session can pass COLONIZE_UNITS_MAX.
5. 13 AI-side direct `nat->gold` writes bypass europe_nation_gold_add.
6. ai_brave.c hand-writes Brave MP in 6 places instead of using units_mp_charge/exhaust. The doc names the wrong file for the MP helpers.
7. 3 compat shims are test-only (colonies_assign_field, units_brewster_apply_popup, units_resolve_naval_combat).
8. 1 dead global (g_units_conquest_gold) along with its dead parameter.
9. tools/address_mapping.csv and CMakePresets.json are committed CRLF despite eol=lf. There are 6 DOS-LITERAL case variants.
10. Clean sweeps: dead statics, decl/def orphans, id-walks, weak definitions, TODOs, header guards, sim→UI calls.
