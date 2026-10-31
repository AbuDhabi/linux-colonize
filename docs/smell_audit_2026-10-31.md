STATUS: OPEN (2026-10-31). Read-only smell audit of src/ (8 slices, Opus agents) plus a cppcheck pass. High/Medium items and the recurring patterns were filed and fixed as port_bugs.md #1333-#1350 (remainder #1351); the Low list below is still open. None of the findings below had an OPEN row in docs/port_bugs.md when the audit ran. File a finding as a port_bugs.md row before you work on it. Severity is the reviewing agent's call. Only the [V] rows were spot-checked by hand.

Format: `path:line` — category — problem — fix.

## High

- [V] src/core/game_dialogs.c:1378 — gold write — `colonies_buy_construction_ex(..., &game->europe.gold)` makes colony_build.c:984 run `*gold -= cost` on eu->gold directly, which skips europe_nation_gold_add and leaves the col1 record unstamped. — Pass a scratch value, then call `europe_nation_gold_add(..., -cost)`.
- [V] src/core/ai_contact_trade.c:923-948 — the comment says the code does the opposite — `ai_contact_reparations_home_tribe`: the comment says "no fallback, DOS aborts (raw 96705)", but the loop at :942-946 still falls back to the nation's first tribe. — Return -1; both callers already handle it.
- [V] src/core/units_combat.c:2587 + units_map.c:1099-1117 — DOS behaviour never reached / dead code — The native win chrome always queues INDIANWIN0. g_units_native_gear_armed/_mounted (set at :1210-1301) is never read, and nothing in src queues INDIANWIN1/2, though DOS raw 101088-101095 appends the '1'/'2' suffix. units_last_native_gear_step, units_clear_native_gear_step and units_set_native_combat_chrome_owned have no callers, so the `!g_units_native_chrome_owned` gate is always true. docs/units.md:64-79 is stale. — Build the tag as "INDIANWIN%d", delete the dead owner API, fix the doc.
- src/data/viceroy_tables.c:5 + src/core/pedia.c:183,208 — wrong data — viceroy_terrain_meta was extracted at the wrong DS offset: its rows contain DOS text bytes, so there is MicroProse text in the binary. Row 6 byte 0 is 0xff, which adds mountain sprite 36 to the Pedia preview of Marsh. — Delete class_flag, the table, its test and scripts/extract_viceroy_tables.py. Keep the index-27/28 tests and add a Marsh case to test_pedia.
- src/core/save_load_dialog.c:78 — English literal — "of the" is compiled into the slot row format. It is LABELS.TXT @MISC 19 (see reports_score.c:522). — Use `reports_misc_word(19, ...)`.
- src/core/settings.c:281 — Windows data loss — tmp+rename() has no `_WIN32 remove(path)` first, so every settings save on mingw after the first fails. col1_save.c:1072 and game_loop_eot.c:1121 already handle this. — Write one shared atomic-replace helper and use it at all 3 sites.
- src/core/ai_contact_raid.c:61 — dead in production — ai_contact_indian_meet_trade, and ai_contact_auto_trade (ai_contact_trade.c:2385) which only it reaches, are called only from tests (test_ai_contact_core.c). — Delete them and their tests.
- src/core/colony_craft.c:350 — dead — colony_craft_demand_mask has no caller. — Delete it.
- src/core/colony.c:686 — duplicate — colonies_nation_settlement_count has the same body as colonies_count_for_nation (:81). — Keep one.
- src/core/colony_screen.h:317 + colony_screen.c:45 — write-only — ColonyScreenView.status is never drawn; only a test reads it. About 46 colony_screen_set_status calls are no-ops. — Draw it, or delete the field and the calls.
- src/core/game_loop.c:3512 — stale/dead pipeline — "Status feeds the window title" is false since 5519784e. game->status only reaches the play_driver log, so about 100 English set_status literals are debug-only. — Fix the comment and document the field as debug text, or drop the literals.
- src/core/game_dialogs.c:249 game_emit_warehouse_full, :758 game_request_noport_found_confirm — dead — no callers. — Delete them and their game_dialogs.h prototypes.
- src/core/cheat_list_dialog.c:514 — English literal — appends " (Space toggle, Enter OK)" to the DEBUG prompt. — Remove it.

## Medium

### AI
- src/core/ai_contact_trade.c:337-347, ai_contact_demand.c:1553,1866, ai_contact_actions.c:1840,1956 — duplicate — "home_tribe else the nation's first village" is spelled out by hand at each site, and each copy may carry the fallback refuted at raw 96705. — Write one helper and check it against 96705.
- src/core/ai_diplo.c:169-223 — write-only / invented — s_indian_hostility_sticky is read only by smoke tests. AI_DIPLO_INDIAN_VERY_LOW_REL is "(OpenCol)" with no FUN cite. — Delete the cache, the sync calls and the assertions.
- src/core/ai_native_021a.c:221, ai_popup.c:894 — duplicate — both copy ai_relation_quartile (ai_diplo.h:28). — Call the shared one.
- src/core/ai_brave.c:228 — wrong unit — `units_id_at` returns the lowest slot, not the stack head. That unit then decides the euro nation and the Wagon Train head test at :312. :244 correctly uses units_tile_head_id_at. — Use the head here too.
- src/core/ai_contact_actions.c:2025-2047 — invented / stale / empty branch — Demand with no menu_unit takes 10/20 gold as a *gift*, which has no DOS body (demand.c:73-75). The refuse OK promised in the comment never appears, and `else if (alarm >= 55) {}` is empty. — Check whether any caller lacks menu_unit; if none does, delete the path.
- src/core/ai_contact_demand.c:776-813 — invented, cite contradicts its own file, English literals — ai_contact_enqueue_demand_amount_choice ("Pay tools (10)" / "Pay gold (15)"). — Find the real DOS widget or delete it.
- src/core/ai_indian.c:156-169 — catalog-row rule — enum values are used directly as the pool type_index. — Map them through units_kind_type_index.
- src/core/ai_king.h:69 — trust of a save field — ai_king_latch_pad uses raw head.human_player (clamped only). Old port saves can hold a stale 0, so the latches land on England. — Resolve via col1_save_human_nation / col1_fixed_human.
- src/core/ai_king_audience.c:948, ai.c:378, game_loop_saveload.c:265,455 — duplicate ×4 — the REF seed formula (75c2:360b). — Write one ai_king_ref_seed_floor helper.
- src/core/ai_king_offers.c:150,544 — duplicate — two copies of ai_popup_pending. — Call ai_popup_pending.
- src/core/ai_euro_act.c:102 — dead path / stale header — ai_euro_act_ship_europe_exit is only ever called in Europe, so only `moves=0; RETURN` runs. — Inline it at :867.
- src/core/ai_euro_land.c:141 — duplicate table — ai_euro_k_dos_ring20_dx/dy equals MAP_RING20_DX/DY and colony_plots.c:153. — Use the map.h table.
- src/core/ai_euro_land.c:130, ai_euro_ship.c:161 — mislabelled table — ai_euro_k_20e6_ring20 is labelled DS:0xc8 but holds the DS:0xb4 dir8 plus a ring the port invented. It is harmless today only because its users don't depend on order. — Point the users at MAP_DIR8 / MAP_RING20 and delete it.
- src/core/ai_goals.c:1435 vs map.c:1296,1308 — duplicate, bounds differ — the map.c owner helpers use map_in_bounds where the cited FUN_137f_0428 means map_coords_inset, so ai_euro_land.c:406 reads the border row where DOS returns -1. — Switch map.c to inset, then have ai_goals call it.
- src/core/ai_euro_expand.c:824, ai_euro_europe.c:1889, ai_euro_goals.c:2767, ai_euro_ship.c:2561 — split leftovers — banners orphaned at EOF describe code that is now elsewhere or has since been ported. — Delete them or move them.

### Units / combat
- src/core/units.c:465-478, units_move.h:185 — dead — units_type_is_soldier/_dragoon/_regular/_cont_cav/_cavalry/_cont_army/_caravel/_merchantman and units_orders_is_follow have no callers. — Delete them.
- src/core/units_combat.c:3281 — duplicate — units_lcr_nearest_tribe_dist re-implements colonies_nearest_tribe_on (colony.c:776). — Call it.
- src/core/units_combat.c:2635,2760,3069, units_map.c:428,516, combat_strength.c:108, explore_auto.c:114 — duplicate ×7 — hand-rolled copies of col1_save_tribe_index_at. The destroy path scans the same tile 3 times. — Call col1_save_tribe_index_at.
- src/core/units.c:111-161 — duplicate caches — the NATIONALITY/HOMEPORT/LEVELS caches duplicate the reports_names accessors, and their fallback parameter is always all "". — Use the reports_* accessors.
- src/core/combat_strength.c:145 vs units_combat_resolve.c:944-962 — contradiction — combat counts the fort flags independently, while coastal fire treats Fortress as implying Fort, so a Fortress-only colony is a Stockade in combat. Fort probes are also copied at resolve.c:1389,1611,1670, colony_build.c:93 and colony.c:43,68. — Settle the invariant and write one helper.
- src/core/pioneer_plan.c:57-66 — duplicate — re-spells units_pioneer_tile_can_clear_or_plow. — Export that function and call it.
- src/core/units_combat.c:500,555,561, units_cargo.c:1346 — English on a catalog miss — "enemy" / "unit" / "Unit". — Return "".

### Colony / turn
- src/core/colony_build.c:1015,744 — mixed-up parameter — `difficulty` is passed into the fallback_tools_price slot. The shims are used only by tests. — Delete the shims, or rename the parameter.
- src/core/colony_goods.c:754-811 — invented, test-only — trade_stop_autofill and the surplus helpers use constants with no cite. — Delete them or cite DOS.
- src/core/colony_production.c:400 — duplicate — colony_prod_crown_nation re-implements ai_king_crown_nation_col1 and picks the first control==0 slot, which is wrong in hotseat. — Call the canonical function.
- src/core/turn_colony.c:153, colony_goods.c:231, game_loop_update.c:1601 — identifies buildings by English name — round-trips through the display name although row_plus1 exists. — Compare rows.
- src/core/colony_screen_buildings.c:291 — identifies buildings by name — the slot chain goes row → name → strcmp → index. — Use colony_screen_find_built_row with the chain rows.
- src/core/turn_production.c:1817 — fragile marker — "message written" is detected with strcmp on europe->status, and status texts with no colony name collide. — Use a flag in TurnProduceCtx.
- src/core/founding_fathers.c:1037 — stale stand-in — the AI Jones frigate is placed next to a colony, not on the 232+n lane with goto landfall that FUN_4345_0342 case 0xe uses. The lane model now exists. — Use the lane for every nation.
- src/core/reports_economy.c:741-752 vs colony_production.c:447 and turn_production.c:253 — triplicated bells inputs — the report ignores prod_compose_stamp, so its bells can differ from what the turn credited. — Write one inputs helper and have the report use the stamp.

### Europe / map / UI
- src/core/europe_dock.c, europe_market.c, europe_pool.c, europe.c, europe_harbor.c (about 20 sites) — English write-only field — eu->status gets English sentences but is never drawn. Its only readers pass it as a miss-fallback to popup_msg_fill (game_dialogs.c:152, turn_production.c:1461). — Write "" there and pass "" as the fallback.
- src/core/europe.c:234, europe_dock.c:380,589 — identity by display name — artillery and purchased ships are matched by strcmp. — Store the kind/row at purchase.
- src/core/ui_button.c:7-29 — duplicate — re-implements fb_rect_outline. — Call it.
- src/core/text_edit.c:404-466 — dead — text_edit_field_width, frame_rect, draw_frame and font_ink_rows. — Delete them.
- src/core/pick_music.c:45, new_game.c:274, cheat_list_dialog.c:37 — duplicate ×3 — copies of popup_msg_is_directive. — Call the shared one.
- src/core/map_menu.c:229-242 — dead, matches by English name — the DEBUG classify branch is unreachable. — Delete it.
- src/core/trade_screen.c:97 — English — "(colony)". — Return "".
- src/core/game_loop_eot.c:1154-1177 — invented — the uncited "nearest coastal colony" stand-in for Europe. Ships fall into it when the high-seas lookup fails. — Return 0.
- src/core/game_loop_eot.c:1207,1239, units.h:141 — stale comments / overloaded field — "nibbles still thin" is stale, and follow_unit_id also holds a trade-route slot. — Document that use or add a field.
- src/core/game_dialogs.c:2080 — _id/_index — name_entry.result_colony_id carries a trade-route slot. — Rename it.
- src/core/cheat_list_dialog.c:501-506 — fallback — when 1..6 rows load, all live labels are dropped for 7 blanks. — Pad only the missing rows.

## Low (grouped)

- Dead code / write-only state: col1_stuff_census_refresh_colony_counts_w (test-only); europe_pop_dock_immigrant + _ex; map_tile_is_coast_water; PediaPage.category_label; ai_euro_unit_hold_has_cargo_type; ai_euro_act_ctx.is_ship/exited_europe/wagon_hauled; ai_s_lcg_in_pick / pick_burns / ai_s_seed100_init_pulse (ai.c:255-266); TurnProduceCtx.sol_b_phase_a; cross_workers/bell_workers (colony_production.c:661,718); sound.c:106 sfx_last_index; dos_compat_init/shutdown (empty); ai_popup_render.c:216 `(void)0`; ai_contact_demand.c:1266 empty block and the redundant `purse < 10` check at :1212; name_entry_dialog.c:37-44 no-op block; opening.c:117 second sscanf; ss.c:132-140 duplicate check; game_colony_count; map_menu_refresh_orders alias; founding_fathers_franklin_keeps_nw_peace (test-only); advance_next_candidate (always -1); ai_diplo_at_war_with_any / ai_popup_queue_pending (test-only); turn_idle_human_rotation_w (test-only); AI_6D8E_DOS_LOOP=0 legacy branch (ai_euro.c:1518); ai_king_offers merc hx/hy payload (unused, wraps at row 64); europe_harbor.c:582 unreachable `>100` clamp; pedia/reports "" fallback tables; options_dialog fallback params (always "").
- Unused parameters: ai_contact_human_chrome `title` (about 50 English literals passed in), ai_talk_ok/choice `fallback`, ai_popup_set_last_graphic `st`, units_lcr_credit_gold `human_nation`, colonies_indian_land_pay `nation_id`, colonies_cheat_add_colonist `pool`, colony_yield_pipeline `colony_flags`, colony_screen_buildings.c:538 / fence_rect / assign_slot_positions_ex, turn_refresh_moves_for_nation_w `ai_popups`/`messages`, units_pioneer.c:376 `clearing`.
- Duplicate helpers: europe_label / pedia_label / reports_labels_field vs assets_msg_line_or; Europe sentinel test (reports.c:393, col1_bridge.c:144, ai.c:151, ai_diplo.c:1570,2895, ai_euro_land.c:921); map.c:666/690 rumour hash; 5 copies of the ss asset loader (mapedit.c:127, new_game.c:210, game_loop_render.c:1560, declaration.c:54, woodcut_present.c:33); opening_load_pik_rect vs cinematic_load_background; text_edit char_width vs font_face_advance; clipboard copies vs str_copy_trunc; LE u16 reads in madspack.c:26 / assets.c:208 vs rd_u16_le; debug_atlas_free; wall-clock ms block (dos_rng.c:96, sdl_runtime.c:456); mapedit --scale clamp; font_glyph_ink_bounds decode loop; popup_msg_apply_tokens ten strncmp blocks; dir8 tables (ai_goals.c:25, ai_euro.c:1317, ai_euro_land.c:2153); first-goods-hold loop (ai_euro_ship.c:1072, ai_euro_expand.c:529) vs units_first_goods_hold; holds-used ×4; census fallback ×2 (saturation differs); visit-mood lookup ×7; pedia open/restore ×5 (game_loop_update.c); settings persist ×3 (game_dialogs.c:2186-2253); outside-rect test vs ui_rect_hit (howmuch, name_entry); trade_screen icon width; explore_auto_cancel vs pioneer_plan_cancel; owner-nibble spelled inline vs units_tile_owner_nibble; reports_garrison rank ×2; col1_stuff_census_tally_units vs 4962_w (they disagree on ships, saturation and continents); reports_*_display_name forwarders.
- Stale comments: settings.c:466 line cites; col1_bridge.c:31 Sections index; ai_king_internal.h:4; ai_king_offers.c:116 (misplaced "invented stand-in"), :131; col1_save.c:1066 "fsynced-by-fclose"; ai_popup.c:200,333; ai_contact.c:1182; ai_goals.c:560,954, ai_euro_goals.c:2443 (pre-split line numbers); ai_goals.c:1527 "ids never recycled" (#1094 says otherwise); ai_euro_goals.c:2338 "fixed 100"; ai_euro_goals.c:2723 28c8 "reference-only"; reports_economy.c:586 sort direction; europe_market.c:669 tax formula; reports_score.c:393; game_loop.h:62-72; game_dialogs.h:5; cheat_list_dialog.h:68, .c:44,190; turn.h:316 misplaced line; units_map.c:917 local prototypes.
- Misleading names: col1_save_layout.h:76 `ref_present` (it means intervention announced, #865); ai_euro_is_ship_type and the `*_name` predicates that actually compare kinds; units.h:146 `home_tribe_id` is an index.
- Robustness: col1_save.c:973 uncapped tail malloc; col1_save.c:1083 rebuilds the ext buffer just to log its size; settings.c:323 slurps the file before the size check; json_min.c:183 accepts "-", "1e", "01" and \u0000; json_min.c:533 buf_len==0 wrap; new_game_scenario.c:44 uninitialised stem; sound.c:140 weak size precheck, :160 vm kept alive with 0 songs, :1370 offline render races the live VM (test-only); gsound_vm.c:339 chord count sign-extended (verify against GSOUND asm); sdl_runtime.c:492 "wbx" on msvcrt; mapedit_main.c:58,72 missing diag_shutdown; ai_euro_expand.c:571 + ship.c:536,565,580 unchecked wagon_errand[id] (id can equal UNITS_MAX); explore_auto.c:69 sees only stack heads; ai_king_offers.c:109,285,574 U+2212 minus in status text; reports_names.c:263 FF name single static buffer.
- Layering hygiene: unused core/font.h + unit_chrome.h includes in colony*.c/turn*.c; unit_chrome.c:8 fb.h; units_enter_reason_status and other English status text kept in sim (units_map.c:1303, units_pioneer.c, units_combat_resolve.c:1700); turn_production.c status literals vs popup_msg_fill_status.
- Over-exported: ai_popup_option_at_y, units_combat_nation_label_plural, g_woodcut_play/set_bgm (woodcut_present.h exists only for them).
- Magic numbers: col1_post_map.c:283 `18`; village_trade_intel.c:56,199 `16`.

## cppcheck (2026-10-31, --enable=warning,style)

These remain after triage. The listed nullPointer/uninitvar errors at game_loop.c:194, diagnostics.c:51 and ai_euro_ship.c:1928 are false positives (macro guard, out-param, pointer comparison).
- game_loop_update.c:2476 — the loop guard `i < 32` exceeds dock_menu_greyed[EUROPE_DOCK_MENU_MAX=12]; it is safe only while dock_menu_count stays at or below 12. Guard with the MAX.
- ai_diplo.c:3146 — alarm_by_player[euro_nation] is indexed without the 0..3 guard that the line above applies.
- units_combat_resolve.c:1480, units.c:202 — negativeIndex: check the guards.
- unsignedLessThanZero: game_dialogs.c:1092, game_loop_eot.c:1219,1331,1424 (dest_count), map_panel.c:262.
- unreadVariable: ai_contact_actions.c:970 kill, ai_euro_europe.c:1450 bought_this_iteration, ai_native_021a.c:1163 owner, map.c:1652 feature_layer, pedia.c:1190 used, turn_production.c:1692 hammers, units_cargo.c:148 amount. redundantAssignment: ai_euro_europe.c:1682, ai_king_audience.c:196, units.c:721.
- duplicateCondition: units_combat.c:3179,3862 and units_move.c:981 test the same thing back to back (likely leftovers from a split; merge them).
- mapedit.c:635 nid is used before its limit check.
