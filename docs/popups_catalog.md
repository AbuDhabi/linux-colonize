# Popup Catalog

Appendices and exhaustive `@SECTION` reference for [popups.md](popups.md).

## Contents

- [Appendix A — Full `GAME.TXT` `@SECTION` checklist](#appendix-a--full-gametxt-section-checklist)
- [Appendix B — `AiPopupTag` map](#appendix-b--aipouptag-map)
- [MAPEDIT](#mapedit-out-of-scope)
- [See also](#see-also)

---

## Appendix A — Full `GAME.TXT` `@SECTION` checklist

| `@SECTION` | Status | Port note |
|------------|--------|-----------|
| `@DOS` | Done | quit confirm (`AI_POPUP_TAG_MAP_CONFIRM`) |
| `@DOSYES` | Done | quit confirm |
| `@RETIRE` | Done | confirm then F10 score |
| `@BEGINMENU` | Done | title menu (`game_loop.c`) |
| `@AMERICA` | Done | new-game America / map pick (`new_game.c`) |
| `@MAPTOLOAD` | Done | new-game America / map pick (`new_game.c`) |
| `@MULTI` | Missing | no multiplayer UI |
| `@MULTINEXT` | Missing | no multiplayer UI |
| `@MULTIREV` | Missing | no multiplayer UI |
| `@GAMEOPTIONS` | Done | options_dialog |
| `@COLONYOPTIONS` | Done | options_dialog |
| `@SOUNDOPTIONS` | Done | options_dialog |
| `@SAVEGAME` | Done | slot / music dialogs |
| `@SAVEGOOD` | Done | real popup on save confirm (game_apply_save_load_result) |
| `@SAVEERROR` | Done | real popup on save I/O failure |
| `@LOADGAME` | Done | slot / music dialogs |
| `@LOADGOOD` | Done | real popup on load confirm |
| `@LOADNOT` | Done | real popup: bad signature/EOF/version-mismatch-high |
| `@LOADOLD` | Done | real popup: save_version < current |
| `@LOADSIZE` | Done | real popup: map W×H mismatch on mid-game Load |
| `@LOADERROR` | Done | real popup: any other read failure (missing/corrupt file) |
| `@PICKNATION` | Done | new-game wizard |
| `@DIFFICULTY` | Done | new-game wizard |
| `@LEADERNAME` | Done | new-game wizard |
| `@FINDCITY` | Done | colony list picker |
| `@NOCITY` | Done | OK when no colonies |
| `@VICEROY` | Done | new-game wizard |
| `@VICEROY2` | Done | new-game wizard |
| `@LANDHO` | Done | first land sight → name New World |
| `@COLONY` | Done | name entry after found |
| `@RENAMECOLONY` | Done | colony **R** rename |
| `@LANDFALL` | Done | AI_POPUP_TAG_LANDFALL |
| `@LANDFALL2` | Done | river variant: `game_loop.c` picks LANDFALL vs LANDFALL2 by `map_tile_has_river` on the dest tile (DOS FUN_4720_015c terrain-flag bit 0x40) |
| `@ONLYPIO` | Dead text | literal absent from VICEROY.EXE; DOS greys the menu row (`0b34` raw 42211-42215). Popup deleted 2026-09-22, bugs.md #621 |
| `@ONLYCOL` | n/a | dead text: no `ONLYCOL` DS string in VICEROY.EXE (absent from popup_tag_ids.md), so nothing can push it |
| `@SHIPCOMBAT` | Done | `game_report_enter_reason` (`game_loop.c`) — real OK popup on `COLONIZE_ENTER_BOUNCE_FOREIGN` when the mover is sea (non-combat ship attacking a ship) |
| `@SHIPLAKE` | Done | new gate: `units_enter_probe` denies a ship entering an enclosed water region with `COLONIZE_ENTER_LAKE_BLOCKED`; `game_report_enter_reason` shows the real OK popup. Uses `layer3` region nibble `> 1` (stricter than the shared `map_tile_is_lake` `!= 1`) since a zeroed/uncomputed region nibble (many synthetic test/AI fixtures) must not misread as a lake — real generated/loaded maps never legitimately carry region 0 on water |
| `@LANDFIRST` | Done | new `COLONIZE_ENTER_LANDFIRST` reason (sea mover meeting a land foe on the dest tile); `game_report_enter_reason` shows the real OK popup |
| `@SEACOLONY` | Done thin | Build Colony on water → ai_popup OK |
| `@NOPORT` | Done thin | inland Build Colony → CHOICE cancel/proceed |
| `@TOOMOUNTAIN` | Done thin | Build Colony on mountains → ai_popup OK |
| `@BUILT` | Done thin | EOT building complete ai_popup OK (`@BUILT`); VGA PARKED |
| `@FULL` | Done thin | Join Colony at POP_MAX → ai_popup OK |
| `@NOTEACHER` | Done thin | unskilled → school assign refuse + ai_popup OK |
| `@NEEDCOLLEGE` | Done thin | school assign when job tier 2 > building → ai_popup OK |
| `@NEEDUNIVERSITY` | Done thin | school assign when job tier 3 > building → ai_popup OK |
| `@TRAINFAIL` | Done thin | EOT Phase G ready teacher + no students ai_popup OK |
| `@TRAINCRIMINAL` | Done thin | EOT Phase G Criminal→Indentured ai_popup OK |
| `@TRAININDENTURED` | Done thin | EOT Phase G Indentured→Free ai_popup OK |
| `@TRAINPROFESSION` | Done thin | EOT Phase G graduate + Phase H field-skill discover ai_popup OK |
| `@SIEGE` | Missing | no colony modal (FULL/SIEGE may status) |
| `@ABANDON` | Done | colony abandon confirm; `ai_popup` `AI_POPUP_TAG_COLONY_ABANDON` (2026-09-03 — was a colony-screen-local box with no wrap/width/figure) |
| `@ABANDON2` | Done | same, picked when the owner has < 2 colonies **and** year > 1575 (DOS 2f2b `caseD_a` `CMP [0x538a],0x627`; the copy's "after 1600" is flavour text only) |
| `@SAILHOME` | Done | `AI_POPUP_TAG_SAILHOME` CHOICE (FUN_4720_049e reason 5) |
| `@SAILAWAY` | Done | confirmed 2026-09-16: `game_europe_request_sail` (`game_loop.c`) already renders the real GAME.TXT body via `popup_msg_fill`, CHOICE Yes/No wired to `GAME_MAP_CONFIRM_EUROPE_SAIL`; every sail entry point routes through it |
| `@SAILPORT` | Done | 2026-09-16: real title, `game_trade_open_stop_picker` (Begin Trade Route stop picker, `game_loop.c`, `r->sea` selects `@SAILPORT`/`@TRAVELPLACE` — DOS `FUN_647e_090a` asm 647e:0925-093a) and the new `game_open_goto_port_picker` (unit ORDERS "Go to Port" ship destination list, DOS `FUN_647e_01c6`/`FUN_2b5a_1dfc`, unit type 0xd..0x12 → `@SAILPORT`); both consume `popup_msg`'s `@default` side channel so it does not leak onto the next `ai_popup` |
| `@TRAVELPLACE` | Done | same site as `@SAILPORT`, non-sea route branch of `game_trade_open_stop_picker`; the land-unit "Go to Place" branch of `FUN_647e_01c6`/`FUN_2b5a_1dfc` is not built — the port's Go to Place stays the existing click-to-destination mode (docs/unit_orders.md), not a list picker |
| `@UNREST` | Done | Dock immigrant arrive ai_popup OK (`@UNREST`); no auto-open Europe |
| `@RECRUIT` | Done | Europe recruit wood menu (structural) |
| `@RECRUITCHOOSE` | Done | Europe recruit wood menu (structural) |
| `@RECRUIT2` | Done | Europe recruit wood menu (structural) |
| `@KINGRECRUIT` | Done | Europe recruit wood menu (structural) |
| `@PURCHASE` | Done | purchase / train menus |
| `@SCHOOL1` | Done | purchase / train menus |
| `@COLLEGE2` | Done | purchase / train menus |
| `@UNIV3` | Done | purchase / train menus |
| `@NODOCKS` | Done | DOS site confirmed 2026-09-16: `viceroy_overlays.asm` OVL08_L0040 raw :0x13e2 LEA (`LAB_OVL08_L0040__0013e0+2`) OK popup; port shows it at Fisherman assignment without Docks — `game_loop.c` `"NODOCKS"` `popup_msg_fill` sites (colony job pick + keyboard path) |
| `@CARGOREADY0` | Done thin | Phase P century tip ai_popup OK; ship-ready also uses this section |
| `@CARGOREADY1` | Done thin | Century tip at warehouse cap (basic); ship PARKED |
| `@CARGOREADY2` | Done thin | Century tip at warehouse cap (expanded); ship PARKED |
| `@LUMBER` | Done thin | EOT Phase K empty lumber + Carpenter ai_popup OK |
| `@COTTON` | Done thin | EOT Phase K empty cotton + Weaver ai_popup OK |
| `@TOBACCO` | Done thin | EOT Phase K empty tobacco + Tobacconist ai_popup OK |
| `@CANESUGAR` | Done thin | EOT Phase K empty sugar + Distiller ai_popup OK |
| `@FURS` | Done thin | EOT Phase K empty furs + Fur Trader ai_popup OK |
| `@ORE` | Done thin | EOT Phase K empty ore + Blacksmith ai_popup OK |
| `@TOOLS` | Done thin | EOT Phase K empty tools+muskets + Armory ai_popup OK |
| `@FOOD1` | Done thin | EOT first starvation latch ai_popup OK (`@FOOD1`); VGA PARKED |
| `@FOOD2` | Done thin | EOT first latch + autumn → winter-soon (`@FOOD2`); VGA PARKED |
| `@VANISH` | Done thin | EOT last-colonist starve-kill → ai_popup OK + abandon; VGA PARKED |
| `@STARVE1` | Done thin | EOT starve-kill ai_popup OK; spring/default |
| `@STARVE2` | Done thin | EOT starve-kill + autumn → winter-coming (`@STARVE2`) |
| `@FOODLOW` | Done thin | EOT production shortfall (`8e32`) and stock &lt; shortfall×4 (`@FOODLOW`); no warn on surplus harvest; VGA PARKED |
| `@SPOIL1` | Done thin | EOT ai_popup OK; tip warehouse, single cargo |
| `@SPOIL2` | Done thin | EOT multi-cargo tip warehouse (`spoil_types>1`) |
| `@SPOIL3` | Done thin | EOT expanded warehouse single (`warehouse_level>1`) |
| `@SPOIL4` | Done thin | EOT expanded warehouse multi |
| `@BUYME0` | Missing | info-only twin unused; cost lines live in `@BUYME1` confirm |
| `@BUYME1` | Done thin | colony buy-construction CHOICE Never mind / Complete it |
| `@DEFOREST` | Done thin | pioneer clear-forest near owned colony ai_popup OK |
| `@DEPLETION` | Done thin | EOT ore/silver wrap ai_popup OK (`@DEPLETION`); VGA PARKED |
| `@UNITFLAG` | n/a | Col1 flag bits; not a dialog (no id in `popup_tag_ids.md`), re-verified 2026-09-16 |
| `@COLONYFLAG` | n/a | Col1 flag bits; not a dialog (no id in `popup_tag_ids.md`), re-verified 2026-09-16 |
| `@LOSTCITY0` | n/a | Not an LCR outcome — reused recruit-menu text ("Which of the following individuals shall we recruit?"), unrelated section number |
| `@LOSTCITY1` | Done thin | Fountain of Youth — 8 dock immigrants (human only; AI has no EuropeScreen pool) |
| `@LOSTCITY2` | Done thin | Seven Cities of Cibola — big treasure train (needs Galleon home) |
| `@LOSTCITY3` | Done thin | Ruins gold, credited direct to nation |
| `@LOSTCITY4` | Done thin | Burial mounds — auto-resolves as Search (Stay-clear CHOICE PARKED) → `@BURIAL1`/`2`/`3`/`@SCREWED` |
| `@SCREWED` | Done thin | Hostile burial-ground natives — relation malus; 50/50 expedition lost (full combat resolve PARKED) |
| `@BURIAL1` | Done thin | Burial search — nothing found |
| `@BURIAL2` | Done thin | Burial search — trinkets (small gold) |
| `@BURIAL3` | Done thin | Burial search — incredible treasure train (needs Galleon home) |
| `@LOSTCITY5` | Done thin | Expedition vanishes — Scout despawns |
| `@LOSTCITY6` | Done thin | Nothing but rumors |
| `@LOSTCITY7` | Done thin | Friendly tribe's chief gift (small gold) |
| `@LOSTCITY8` | Done thin | Trespassing near shrines — native relation malus, no combat |
| `@LOSTCITY9` | Done thin | Survivors of a former colony join (free Colonist spawns) |
| `@SNEAK` | Done | User-confirmed real (2026-08-14): AI Euro attacks outright at peace, war declared as a side effect of the attack (not a prerequisite) — already correctly implemented in `ai_euro_try_attack` (`src/core/ai_euro.c`). Fixed: was silently calling bare `ai_diplo_declare_war` (no notification); now `ai_diplo_declare_war_ctx` + real GAME.TXT "Sneak attack by the treacherous %STRING0!" status when human is a party. Covered by `unit_ai_diplo` (`test_ai_diplo.c`, direct `popup_msg_fill` check against real GAME.TXT — end-to-end dispatcher assertion too flaky, `ai_diplo_euro_balance`'s own opportunistic declare can win the race within the same turn). |
| `@CANCELPEACE` | Done | DIPLO_* CHOICE structural; 10ec AI→human war-declare CHOICE prompt body now the real GAME.TXT line via `popup_msg_fill` |
| `@SIGNTREATY` | Done | DIPLO_* CHOICE structural; peace-concluded OK popup body now the real GAME.TXT line via `popup_msg_fill` (Tools-embargo-lift chrome may override) |
| `@DECLAREWAR` | Done | DIPLO_* CHOICE structural; war-declared OK popup body now the real GAME.TXT line via `popup_msg_fill` (boycott/hostility chrome may override); also `13b0` treaty-cancel-without-peace notice (2026-08-27) |
| `@CANCELTREATY` | n/a — DOS shows nothing | `FUN_5bfb_13b0` cancel branch pushes tag `0x1898` = `CANCELTREATY`, but **no `.TXT` in `COLONIZE/` has that section**. `FUN_7314_001a` (open resource; scan for `@TAG`) hits EOF → returns 1 → `FUN_6f74_32a4` skips the whole parse and returns a NULL box → `FUN_6f74_36ca` returns 0 without ever opening a dialog. So real DOS cancels the treaty **silently** (bits + timers only). Fixed 2026-09-03: the port's invented "The X cancel their treaty with the Y." notice is gone; the effects stay, and the popup only fires if some GAME.TXT ever ships the section |
| `@HAVETREATY` | Done | euro-attack confirm gate in `ai_contact.c` (real GAME.TXT body via `popup_msg_fill`, 2026-09-01) |
| `@WHACKINDIANS` | Done (structural) | `FUN_465b_0000` → `ai_contact_try_whack_confirm` (2026-08-27): Yes/No before the first attack on a calm tribe, asked once (bit 0x04); real GAME.TXT body via `popup_msg_fill` |
| `@VILLAGEHAPPY` | Done | Village-meet body picked by alarm band (`ai_contact_enqueue_village_meet`, `ai_contact.c`) — real GAME.TXT via `popup_msg_fill`; the tag itself is built at runtime from the DS `"VILLAGE"` stem (0x1710) + band suffix, as in DOS. Confirmed 2026-09-16 |
| `@VILLAGESAVAGE` | Done | as `@VILLAGEHAPPY` (band = tribe 6, calm) |
| `@VILLAGEMEDIUM` | Done | as `@VILLAGEHAPPY` (alarm ≥ 25 or friction word ≥ 0x80) |
| `@VILLAGEBAD` | Done | as `@VILLAGEHAPPY` (alarm ≥ 50) |
| `@VILLAGEWAR` | Done | as `@VILLAGEHAPPY` (alarm ≥ 75) |
| `@INDIANWELCOME` | Done | CONTACT_WELCOME + follow-ups |
| `@INDIANBOW` | Done | `units_try_native_settlement_fallout` (`units.c`): capital razed → tribe bows and cedes its land. DOS `FUN_5fef_1b0e` tail (viceroy_unpacked.c 101300-101306, tag 0x1cd7), human conqueror only. Popup wired 2026-09-16 (the alarm/attitude discharge was already ported) |
| `@INDIANTREATY` | n/a — dead GAME.TXT | no NUL-terminated `INDIANTREATY` tag string exists in `VICEROY.EXE` DS (checked 2026-09-16 with the 121248+addr method), and nothing builds it by `strcat`, so DOS can never request the section |
| `@INDIANHELLO1` | n/a — dead GAME.TXT | no `INDIANHELLO` DS string at all (2026-09-16); the greeting DOS really shows on entering a village is the `@VILLAGE*` band body |
| `@INDIANHELLO2` | n/a — dead GAME.TXT | as `@INDIANHELLO1` |
| `@INDIANPEACE` | Done | CONTACT_WELCOME + follow-ups |
| `@INDIANCOME` | Done | CONTACT_WELCOME + follow-ups |
| `@INDIANSHUN` | Done | CONTACT_WELCOME + follow-ups |
| `@INDIANWAGONS` | Done | reparations demand flavor 2 (`ai_contact.c` `AI_CONTACT_REPARATIONS_WAGONS`) — real GAME.TXT body + rows via `popup_msg_fill`; also the `521d_20e6` wagon arm. Confirmed 2026-09-16 |
| `@INDIANCITY` | Done | reparations demand flavor 1 (`AI_CONTACT_REPARATIONS_CITY`) — real GAME.TXT body + rows. Confirmed 2026-09-16 |
| `@INDIANGOLD` | n/a — dead GAME.TXT | no `INDIANGOLD` DS string (2026-09-16); the reparations demands DOS can actually show are `@INDIANCITY` / `@INDIANWAGONS` |
| `@INDIANSLAVES` | Done | `units_try_native_settlement_fallout`: razing a village that hosts YOUR mission may convert its people (already ported); the `@INDIANSLAVES` announcement (DOS `5fef_1b0e`, viceroy_unpacked.c 101176-101181, tag 0x1cbf, human conqueror only) wired 2026-09-16 |
| `@INDIANSCONVERT` | Done | `ai_contact.c` 022e adjacency convert arm — real GAME.TXT body via `popup_msg_fill`. Confirmed 2026-09-16 |
| `@INDIANGIVEFOOD` | Done | 022e adjacency gift arm + `4d56_4528` village arm — real GAME.TXT body. Confirmed 2026-09-16 |
| `@INDIANGIVESTUFF` | Done | as `@INDIANGIVEFOOD` (goods flavor). Confirmed 2026-09-16 |
| `@INDIANCOMMENT` | Done | Colony encroachment OK via `popup_msg_fill` |
| `@INDIANBEGFOOD` | Done | `ai_contact_try_village_beg_food`/`ai_contact_apply_beg_food` — real accept/decline CHOICE, `AI_POPUP_TAG_CONTACT_BEGFOOD` (2026-08-14, was unwired; sign convention resolved via live user gameplay testimony, see `settlement_record_8d4a.md`) |
| `@INDIANWAR` | n/a — dead GAME.TXT | no `INDIANWAR` DS string (only `INDIANWARPATH`/`INDIANWARPATH2`/`INDIANWARFARE`), 2026-09-16. The port's raid-time "declare war" sentence stays as Linux chrome and no longer claims this section |
| `@INDIANGRUDGE` | Done | WoI tribe defection (`ai_contact_indian_woi_defect`): DOS `FUN_4d56_1816` item 2 flushes tag 0x14f6 with both tribe name forms right before the ±100 alarm pair (viceroy_unpacked.asm 136388). Popup wired 2026-09-16 (was a status line only, id unresolved) |
| `@INDIANLAND` | Done | colony encroachment CHOICE (`game_loop.c` `AI_POPUP_TAG_INDIAN_LAND`) — real GAME.TXT body + rows. Confirmed 2026-09-16 |
| `@INDIANROAD` | Done | as `@INDIANLAND` (road flavor) |
| `@INDIANFOREST` | Done | as `@INDIANLAND` (forest flavor); DOS `2b5a` LEAs 0x944 directly |
| `@INDIANFOREST2` | n/a — dead GAME.TXT | no `INDIANFOREST2` DS string, and the one `@INDIANFOREST` site (`2b5a:1384`, `LEA AX,[0x944]`) takes the tag as a plain literal with no digit patch (2026-09-16) |
| `@INDIANBRIBE` | Done | encroachment CHOICE row set. Confirmed 2026-09-16 |
| `@NOPLOW` | Done thin | plow on already-plowed ai_popup OK |
| `@NOROAD` | Done thin | road where road exists ai_popup OK |
| `@VIOLATE` | Dead (DOS) | 2026-08-27: no `VIOLATE` tag-name string exists in `VICEROY.EXE` DS (`docs/popup_tag_ids.md` method), so DOS never displays this text — orphaned GAME.TXT entry. The earlier `FUN_4720_049e` lead resolves to `@HAVETREATY`/`@SNEAK`/`@CANCELPEACE`/`@DECLAREWAR` (encounter → war-declare flow, covered by `DIPLO_WAR`). Nothing to port. |
| `@HALF` | Done | `AI_POPUP_TAG_COMBAT_HALF` CHOICE (FUN_5fef_1b0e) |
| `@NOLOOT` | n/a | Alias unused, no DS id (no entry in `popup_tag_ids.md`); village burn uses `@LOOT2` (re-verified 2026-09-16) |
| `@LOOT` | Done | Cortes/conquest treasure fallout |
| `@LOOT2` | Done | village burn without treasure |
| `@LOOTCASH` | Done thin | `europe_cash_treasure` status now the real GAME.TXT line (was invented "Treasure cash-in +$"); not combat — combat uses `@LOOTCAPTURE` capture |
| `@LOOTFOREIGN` | Missing | Bystander spy-report sibling of `@LOOTCASH` (mirrors `@BURNED3`); not wired — `ai_euro_cash_one_treasure` reuses one shared `EuropeScreen` scratch struct per nation and it is unclear whether/when it runs for AI (non-human) nations, so gating "human is not the cashing nation" needs that traced first |
| `@LOOTCAPTURE` | Done | treasure nation-flip capture (`FUN_5fef_0352` raw 99392-99413); NUMBER0 = value, display only |
| `@WAGONCAPTURE` | Done | wagon nation-flip capture |
| `@COLONISTCAPTURE` | Done | Euro winner captures Colonists only (not Pioneer) |
| `@COLONISTCAPTURE2` | Done | capture + strip Veteran specialty |
| `@CARGOCAPTURE` | Done | wagon holds present |
| `@DEMOTE` | Done | type demote (Soldier→Colonist, …) nation + unit + status |
| `@SEIZURE` | Done | `AI_POPUP_TAG_COMBAT_SEIZURE` privateer body |
| `@SEIZURESEA` | Done | Crown naval → Royal Navy; Privateer uses custom body |
| `@SEIZURELAND` | Done | Crown land win vs human → Royal Army text |
| `@SHIPDAMAGE` | Done | naval/arty damage-not-sink path |
| `@SHIPSUNK` | Done | naval sink after plunder |
| `@RAIDNOTHING` | Done | `ai_contact_apply_raid_loot`/`ai_contact_raid_chrome_row` real GAME.TXT body via `popup_msg_fill` (re-verified 2026-09-16) |
| `@RAIDWREAK` | Partial | Deliberately kept thin: DOS's `FUN_281f_0652(0x1b8a, mode=3)` call (raw `5fef:126a`) is a different mode/shape than the other 5 kinds' `mode=5` victim calls, and its GAME.TXT text is a third-person "Spies report…" frame — see `indian_raid_outcomes.md` |
| `@RAIDSTORES` | Done | real GAME.TXT body incl. drained cargo name via `popup_msg_fill` (re-verified 2026-09-16) |
| `@RAIDBURN` | Done | real GAME.TXT body when a building was actually destroyed (named via `s_last_burn_building`); construction-cleared/lumber-drained sub-cases keep the thin paraphrase (no object to name) |
| `@RAIDSCALP` | Done | real GAME.TXT body via `popup_msg_fill` (re-verified 2026-09-16) |
| `@RAIDSHIP` | Done | real GAME.TXT body incl. `units_display_name` via `popup_msg_fill` (re-verified 2026-09-16) |
| `@RAIDGOLD` | Done | real GAME.TXT body incl. drained amount (`%NUMBER0`) via `popup_msg_fill` (re-verified 2026-09-16) |
| `@MISSION0` | Done | `ai_contact_establish_mission` picks the band (`MISSION` + 0-3, exactly DOS's digit patch of the DS `"MISSION0"` stem) and fills the real GAME.TXT body — nation / colony / season / year / tribe tokens. Confirmed 2026-09-16 |
| `@MISSION1` | Done | `ai_contact_establish_mission` picks the band (`MISSION` + 0-3, exactly DOS's digit patch of the DS `"MISSION0"` stem) and fills the real GAME.TXT body — nation / colony / season / year / tribe tokens. Confirmed 2026-09-16 |
| `@MISSION2` | Done | `ai_contact_establish_mission` picks the band (`MISSION` + 0-3, exactly DOS's digit patch of the DS `"MISSION0"` stem) and fills the real GAME.TXT body — nation / colony / season / year / tribe tokens. Confirmed 2026-09-16 |
| `@MISSION3` | Done | `ai_contact_establish_mission` picks the band (`MISSION` + 0-3, exactly DOS's digit patch of the DS `"MISSION0"` stem) and fills the real GAME.TXT body — nation / colony / season / year / tribe tokens. Confirmed 2026-09-16 |
| `@HERESY0` | Done | `ai_contact_denounce_heresy` — real GAME.TXT body via `popup_msg_fill`, both outcomes (mission taken over / missionary burned). Confirmed 2026-09-16 |
| `@HERESY1` | Done | `ai_contact_denounce_heresy` — real GAME.TXT body via `popup_msg_fill`, both outcomes (mission taken over / missionary burned). Confirmed 2026-09-16 |
| `@INDIANBURN` | Done | `FUN_4cc6_0000` mission clear (`ai_contact.c`) — real GAME.TXT body via `popup_msg_fill` |
| `@INDIANWIN0` | Done | Native attacker beats a human land defender: ai_contact ambush arm **and** the generic `units_combat_outcome_popups` path (braves stepping onto defended tiles); `units_set_native_combat_chrome_owned` keeps the two from doubling |
| `@INDIANWIN1` | Done | Ambush arm only (muskets seized); generic path has no native seizure, like DOS |
| `@INDIANWIN2` | Done | Ambush arm only (horses seized) |
| `@INDIANLOSE` | Done | Native attack repulsed; LABELS defeat/defeats by defender type (`<7`) |
| `@INDIANWINCOLONY` | Done | DOS `5fef_1b0e` colony arm (`bVar28` = undefended colony tile): native winner kills one colonist while pop>1 (`units_try_capture_foreign_colony`) |
| `@INDIANWINCOLONY2` | Done | Human-bystander "Spies report…" twin of the above |
| `@INDIANBURNCOLONY` | Done | Last colonist falls → colony burned (`units_combat_notify_colony_burned`, + woodcut 11). The `@BURNED` family is the both-Euro arm only |
| `@INDIANBURNCOLONY2` | Done | Human-bystander twin (`units_combat_notify_colony_burned_foreign`) |
| `@INDIANSURPRISE` | Done | brave raid on a colony while NOT at war (`ai_contact.c` raid chrome) — real GAME.TXT body with DOS's three slots (tribe / colony / tribe), wired 2026-09-16; was an invented one-liner |
| `@CAPTURED` | Done | Euro colony conquest with plunder |
| `@CAPTURED2` | Done | spies report (AI capturer) |
| `@CAPTURED3` | Done | conquest without plunder amount |
| `@BURNED` | Missing | Euro-vs-Euro colony burn only (DOS `5fef_1b0e` `uVar16<4 && uVar15<4` arm); the port has no Euro burn path yet — native burns use `@INDIANBURNCOLONY` |
| `@BURNED2` | Missing | Ambiguous vs `@BURNED3` (no distinct trigger identified); left unwired |
| `@BURNED3` | Missing | Euro-vs-Euro bystander twin of `@BURNED` (DOS tag `0x1c37` in the both-Euro arm); the native bystander uses `@INDIANBURNCOLONY2` instead |
| `@EUROPEWIN` | Done | `{atk_nation} defeat {def_nation def_unit} near {place}!` |
| `@EUROPELOSE` | Done | `{def_nation def_unit} defeat(s) {atk_nation} near {place}!` |
| `@WAREHOUSEFULL` | Done thin | ship→colony unload when no room; spoilage remains `@SPOIL*` |
| `@EXTORTSTUFF` | Missing | extort/ship anger dialogs missing |
| `@EXTORTPOOR` | Missing | extort/ship anger dialogs missing |
| `@EXTORTLAUGH` | Missing | extort/ship anger dialogs missing |
| `@EXTORTNO` | Missing | extort/ship anger dialogs missing |
| `@TOONEAR` | Done | already real (`game_loop.c` Build Colony adjacency-to-existing-colony scan) — verified against DOS |
| `@TOONEARBUILD` | Done | new 9-tile neighbor scan for a unit with `UNITS_ORDER_BUILD_COLONY` pending (`game_try_found_colony_at_cursor`, `game_loop.c`) — was entirely unported, fell through to generic "Cannot found colony here" |
| `@DONTKNOWSHIPS` | Done | Ship→unmet village: `ai_contact_try_ship_village` OK |
| `@MADATSHIPS` | Done | Ship→met village (rel≥75 / friction≥64): same |
| `@MADATWAGONS` | Missing | extort/ship anger dialogs missing |
| `@GRUDGEWAGONS` | Missing | extort/ship anger dialogs missing |
| `@CONFISCATE` | Missing | extort/ship anger dialogs missing |
| `@CHIEFHOWDY` | Missing | chief portrait dialogs missing |
| `@CHIEFGUIDES` | Missing | chief portrait dialogs missing |
| `@CHIEFAREA` | Missing | chief portrait dialogs missing |
| `@CHIEFGIFT` | Missing | chief portrait dialogs missing |
| `@CHIEFBORED` | Missing | chief portrait dialogs missing |
| `@CHIEFKILL` | Missing | chief portrait dialogs missing |
| `@KILLWAGONS` | Missing | chief portrait dialogs missing |
| `@TRADE0` | Done (structural) | `2820` sell offer (any cargo, whole hold, NAMES `@VALUES` adjective): Accept / fairer price / gift / Never mind → `ai_contact_enqueue_trade_offer_round` (2026-08-29 rewrite) |
| `@TRADE1` | Done (structural) | `2820` sell counter-offer re-ask after a successful haggle (3 options) (2026-08-27) |
| `@BADCARGO` | Done (structural) | `2820` human gate: `last_bought == cargo` / `last_sold == cargo` / `ask[cargo] == 0` → refuse, names the three highest-ask goods (2026-08-29) |
| `@BADHAGGLE0` | Done (structural) | `2820` sell-side patience exhausted → `sticky_trade_good = cargo`, alarm +tier/2+1 (2026-08-27) |
| `@BADHAGGLE1` | Done (structural) | `2820` revisit gate `tribe+7 == cargo` → refuse line, no trade (2026-08-27); `0xfe` skips the buy offer silently |
| `@BADHAGGLE2` | Done (structural) | `2820` buy-side haggle refusal → `ai_contact_2e92_haggle` (2026-08-27) |
| `@BADHAGGLE3` | Missing | deep village trade 2820 PARKED |
| `@BRING` | Done (structural) | `2820` `LAB_002e92` entry: sold good not in the top-2 asks (or empty-handed unit) → "we are in need of X and Y" (2026-08-29) |
| `@DEFICIT` | Missing | deep village trade 2820 PARKED |
| `@BUYWHICH` | Done (structural) | `2820` `LAB_002e92` human pick of 3 tribe goods → `ai_contact_enqueue_buywhich`; only after a completed sale, qty = the sold hold's amount (2026-08-29) |
| `@TRADEWHICH` | Done | two DS ids share the name: `0x1556` (village trade 2820, PARKED) and `0x1ad2`, the foreign-colony hold pick of `FUN_5f7a_020e` — `AI_POPUP_TAG_FOREIGN_TRADE_WHICH` (game_loop.c), docs/foreign_colony_trade.md |
| `@BUY0` | Done (structural) | `2820` `LAB_002e92` Accept / fairer / Never mind → `ai_contact_apply_buywhich`/`apply_buy0`; treasury shown in the accept row (2026-08-29) |
| `@BUY1` | Done (structural) | `2820` buy-side haggle re-ask (tag built at runtime `"BUY"+digit`) (2026-08-27) |
| `@NOTENOUGH` | Done (structural) | `2820` `LAB_002e92` can't-afford line on Accept, alarm +1 (2026-08-29) |
| `@LEARNMASTER` | Done thin | Already-expert learner refuse; `popup_msg_fill`; does not consume village one-shot |
| `@LEARNCRIMINAL` | Done | `ai_contact_live_among_natives` real body via `popup_msg_fill`; Petty Criminal refused outright, one-shot not consumed (re-verified 2026-09-16) |
| `@LEARNALREADY` | Done | `ai_contact_live_among_natives` real body via `popup_msg_fill`; village already taught && !capital (re-verified 2026-09-16) |
| `@LEARNMAD` | Done thin | Alarm/friction ≥40 refuse (both mid and hostile bands); `popup_msg_fill` |
| `@LEARNSLOW` | Done | `ai_contact_live_among_natives` real body via `popup_msg_fill`; quartile-1 slow-learner roll (re-verified 2026-09-16) |
| `@LEARNSTAY` | Done | Yes/No CHOICE `AI_POPUP_TAG_CONTACT_LEARNSTAY` (2026-08-28) |
| `@LEARNLATER` | Done | `ai_contact_live_among_natives` real body via `popup_msg_fill`; `@LEARNSTAY` No branch (re-verified 2026-09-16) |
| `@LEARNDONE` | Done | `ai_contact_live_among_natives` real body via `popup_msg_fill`; profession granted, village-taught bit set (re-verified 2026-09-16). Menu-path only — the AI-only auto pulse still skips this chain silently (design tension, not a regression: see `indian_contact.md` "preserve gift/trade chrome") |
| `@TRADEMANY` | Done | create wizard cap popup (%NUMBER0=12) |
| `@TRADESTART` | Done | destination pickers (create wizard + editor) |
| `@TRADETYPE` | Done | sea/land CHOICE (AI_POPUP_TAG_TRADE_TYPE) |
| `@TRADENAMES` | Done | default-name word pool (create wizard) |
| `@TRADENAME` | Done | name entry (create + editor rename) |
| `@TRADENONE` | Done | Begin/Edit with no routes |
| `@TRADENONE2` | Done | no routes of unit's sea/land type |
| `@TRADESELECT` | Done | route picker (`cheat_list`) |
| `@TRADEDELETE` | Done | route picker (DOS unit fixup + compaction) |
| `@SUREDELETE` | Done | delete Yes/No |
| `@CARGOLOAD` | Done | editor load-list append picker |
| `@CARGOUNLOAD` | Done | editor unload-list append picker |
| `@ROUTELOOP` | Done | single-port route warning at stop service |
| `@PISS0` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@PISS1` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@PISS2` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@PISS3` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@PISS4` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@PISS5` | n/a | dead text: no `PISS` string in VICEROY.EXE (`strings` scan + popup_tag_ids.md), no stem for a digit patch — nothing can push it |
| `@KINGNO` | n/a | Dead text: audience-chamber "we do not deign" refusal (`38fd:44a4`, tax >= 60). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGFUND` | n/a | Dead text: audience-chamber grant CHOICE (`38fd:44a4`, accept / never mind). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGLOWER` | n/a | Dead text: audience-chamber tax-cut grant (`38fd:4590`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGNOTHING` | n/a | Dead text: audience-chamber "we shall not change your tax rate" (`38fd:4590`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGRAISE` | n/a | Dead text: audience-chamber punitive hike (`38fd:4590`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGTAX` | Done | king tax body via `popup_msg_fill` |
| `@KINGBLESS` | n/a | Dead text: audience-chamber menu row 1 (`38fd:469e`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGLAUGH` | n/a | Dead text: audience-chamber menu row 5, "Independence! Ha ha ha" (`38fd:46ba`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@KINGWELCOME0` | n/a | Dead text: audience-chamber greeting (`38fd:465c`). The only reader is `FUN_38fd_462e`, the King's audience chamber, and no `CALLF`/`JMPF` anywhere in `viceroy_unpacked.asm` or `viceroy_overlays.asm` reaches it; the chamber's own menu (`@AUDIENCE` `0x10d4`) and brush-off (`@KINGGOAWAY` `0x10c9`) are not in the shipped `GAME.TXT`. Feature cut before release — do not port (2026-09-16) |
| `@MERCANTILISM` | n/a | Lead closed 2026-09-16: VICEROY.EXE has no DS string `MERCANTILISM` at all (`docs/popup_tag_ids.md` is the full dump of that table), so no code can name the section — DOS cannot reach it. The tax-raise triggers that exist all name their own section (`@KINGTAX` and the five `FUN_38fd_5be8` rungs). Dead text |
| `@PURCHASETAX` | n/a | Same evidence as `@MERCANTILISM`: no `PURCHASETAX` DS string in VICEROY.EXE, hence no call site. Dead text |
| `@TAXOPTIONS` | Done | ai_popup CHOICE structural |
| `@TEAPARTY` | Done thin | king refuse/dump follow-up OK via `popup_msg_fill`; thin `3dc8` stock dump; VGA PARKED |
| `@KISSUP` | Done | `FUN_38fd_2dfe` two-row CHOICE raised from the Europe market strip click (`game_europe_ask_boycott_buyback`, `src/core/game_loop.c`), tag `AI_POPUP_TAG_EUROPE_KISSUP`; `%STRING0` = cargo, `%STRING1` = the player's country, `%NUMBER0` = back taxes. DOS's row 2 ("Pay") is the paying answer. 2026-09-16 |
| `@KISSSORRY` | Done | Shown only after "Pay", when the purse falls short — the order `38fd:2e6e` uses; `%NUMBER0` = gold on hand, no state change. 2026-09-16 |
| `@PRICEUP` | Done | market price OK popup (EOT + buy/sell) |
| `@PRICEDOWN` | Done | market price OK popup (EOT + buy/sell) |
| `@WHICHFREEDOM` | Done | FF debate body via `popup_msg_fill`; choices = FF names |
| `@FREEDOM` | Done | FF elect announce via `popup_msg_fill` |
| `@CLAND` | Done | customize wizard |
| `@CCONT` | Done | customize wizard |
| `@CTEMP` | Done | customize wizard |
| `@CCLIM` | Done | customize wizard |
| `@SHIPSLOW` | Done | `units_ship_slow_scan` (FUN_5bfb_3180 naval half): human mover slowed by adjacent foreign warship (0x1a51) or Fort/Fortress (0x1a5a) |
| `@SHIPRUN` | Done | `units_ship_slow_scan`: roll beat the adjacent warship, either side human |
| `@FORTFIRE` | Done | `units_coastal_fort_fire_pulse` (`units.c`) now enqueues the real OK popup (fort/fortress, colony, ship nation+type tokens) before resolving the fort-vs-ship roll, gated to human involvement |
| `@EUROPEARM` | Done | Europe dock / arm chrome |
| `@EUROPESHIPCLICK` | Done | Europe dock / arm chrome |
| `@ARMOPTIONS` | Done | Europe dock immigrant click — 12 GAME.TXT rows, DOS `FUN_38fd_37xx` |
| `@COLONYUNIT` | Done | colony dock-orders popup title (`colony_screen_open_dock_orders`) |
| `@UNITOPTIONS` | Done thin | colony dock-orders popup, land transport; chrome thin |
| `@SHIPOPTIONS` | Done thin | colony dock-orders popup, sea transport; chrome thin |
| `@EUROPESHIPOPTIONS` | Done | Europe dock / arm chrome |
| `@KINGFRIGATE` | Done | `ai_king_frigate_offer` Yes/No (FUN_3844_00f2 tail, 2026-08-29) |
| `@KINGGALLEON2` | Done (structural) | `FUN_5fef_1908` CHOICE (tag built at runtime); VGA PARKED |
| `@KINGGALLEON3` | Done (structural) | Cortes free-transport arm of the same function; VGA PARKED |
| `@CASHTREASURE` | Done | `units_king_galleon_cash_in` (FUN_5fef_1908 else-branch, no King) — OK via `ai_popup_enqueue_ok` |
| `@USEDUPTOOLS` | Done thin | pioneer tools demotion ai_popup OK; VGA PARKED |
| `@EVASIVE` | Done | already real (`units.c:~5837` naval evasion via `units_combat_enqueue_tok`) — verified against DOS FUN_5fef_1b0e raw ~100629 |
| `@KINGMERCY` | n/a | dead text: no `KINGMERCY` DS string in VICEROY.EXE — the tax-cut-on-REF-loss audience is cut content |
| `@KINGNEWWAR` | Done (structural) | `FUN_38fd_5930` → `ai_king_new_war_event` (2026-08-27): real gate/roll/grant formula, OK popup with title/name/peer/gold/count tokens; VGA PARKED |
| `@KINGVICTORY` | Done | `FUN_38fd_5be8` score<100 rung (38fd:5d2d): the tax CUT, `%STRING2` = `@COUNTRIES[king_audience_last_pick-1]` (the crown's current war peer). Rendered through `popup_msg_fill` from `ai_king_tax_hike_apply` (`src/core/ai_king.c`) with `%STRING0`/`%STRING1` = difficulty title + player name, `%NUMBER0` = applied delta, `%NUMBER1` = resulting rate; the raise arm keeps the `@TAXOPTIONS` Kiss/Party rows. 2026-09-16 |
| `@KINGWIFE` | Done | `FUN_38fd_5be8` score<650 rung (38fd:5d64): +1%, bumps the King's wife counter `DS:0x53a7` and names it via `@ORDINAL`. Rendered through `popup_msg_fill` from `ai_king_tax_hike_apply` (`src/core/ai_king.c`) with `%STRING0`/`%STRING1` = difficulty title + player name, `%NUMBER0` = applied delta, `%NUMBER1` = resulting rate; the raise arm keeps the `@TAXOPTIONS` Kiss/Party rows. 2026-09-16 |
| `@KINGWAR` | Done | `FUN_38fd_5be8` score<950 rung (38fd:5d9e): +2%, rerolls the war peer 1..8 until it differs and names it via `@COUNTRIES`. Rendered through `popup_msg_fill` from `ai_king_tax_hike_apply` (`src/core/ai_king.c`) with `%STRING0`/`%STRING1` = difficulty title + player name, `%NUMBER0` = applied delta, `%NUMBER1` = resulting rate; the raise arm keeps the `@TAXOPTIONS` Kiss/Party rows. 2026-09-16 |
| `@KINGNAVACT` | Done | `FUN_38fd_5be8` score<1100 rung (38fd:5de2): +3..4%, no `%STRING2`. Rendered through `popup_msg_fill` from `ai_king_tax_hike_apply` (`src/core/ai_king.c`) with `%STRING0`/`%STRING1` = difficulty title + player name, `%NUMBER0` = applied delta, `%NUMBER1` = resulting rate; the raise arm keeps the `@TAXOPTIONS` Kiss/Party rows. 2026-09-16 |
| `@KINGSTAMPACT` | Done | `FUN_38fd_5be8` top rung (38fd:5dfe): +5..8%, `%STRING2` = the player's New World name (`player.country_name`). Rendered through `popup_msg_fill` from `ai_king_tax_hike_apply` (`src/core/ai_king.c`) with `%STRING0`/`%STRING1` = difficulty title + player name, `%NUMBER0` = applied delta, `%NUMBER1` = resulting rate; the raise arm keeps the `@TAXOPTIONS` Kiss/Party rows. 2026-09-16 |
| `@COUNTRIES` | Done | Not a popup: the 8-entry list `FUN_38fd_5be8` indexes for the tax audience's `%STRING2` (`@KINGVICTORY` / `@KINGWAR`). Read by `ai_king_msg_list_entry` (`src/core/ai_king.c`), 2026-09-16 |
| `@ORDINAL` | Done | Not a popup: the 30-entry ordinal list the audience's `@KINGWIFE` rung indexes with the King's wife count. Same reader, 2026-09-16 |
| `@NEEDTOOLS` | Done | EOT Phase L (raw 57737-57771) `turn_emit_needtools_notice` in `turn.c`: hammers ready, tools short but >0. Runs every EOT (not only on hammer-producing ticks) and covers unit projects too — bugs.md #537, 2026-09-20 |
| `@NEEDTOOLS0` | Done | Same emitter, tools == 0 (DOS appends DS:0xeab "0" to DS:0xea1), 2026-09-20 |
| `@ALREADYHAVE` | Done thin | construction set refused when already owned → ai_popup OK |
| `@LOBOTOMIZE` | Done | Clear Specialty confirm `AI_POPUP_TAG_COLONY_CLEARSPEC` (bugs.md #431) |
| `@NATION0A` | Done | nation lore pages |
| `@NATION0B` | Done | nation lore pages |
| `@NATION1A` | Done | nation lore pages |
| `@NATION1B` | Done | nation lore pages |
| `@NATION2A` | Done | nation lore pages |
| `@NATION2B` | Done | nation lore pages |
| `@NATION3A` | Done | nation lore pages |
| `@NATION3B` | Done | nation lore pages |
| `@PICKACARGO` | Missing | trade Edit uses thin multi-select; not full PICKACARGO |
| `@CUSTOM` | Done | customize wizard |
| `@CONTINENTAL` | Done | combat promotion ladder (`units_promote_on_win`, FUN_5fef_172c) — Veteran + WoI mobilization → Cont. Army/Cav, `ai_popup_enqueue_ok` |
| `@VETERAN` | Done | combat promotion ladder (`units_promote_on_win`) — Free Colonist w/ musket body → Veteran Soldier |
| `@VALOR` | Done | combat promotion ladder (`units_promote_on_win`) — Criminal/Servant ladder step, %STRING1/2 = old/new @JOB label (bugs.md #265) |
| `@SCOUTCOLONY` | Done | `AI_POPUP_TAG_SCOUT_COLONY` (FUN_5f7a_000e, bugs.md #438) |
| `@LOSTOURSCOUTS` | Done | FUN_5f7a_000e infiltrate-fail, human-scout branch — `game_loop.c` AI_POPUP_TAG_SCOUT_COLONY choice 2 |
| `@LOSTTHEIRSCOUTS` | Partial | FUN_5f7a_000e infiltrate-fail, AI-scout-vs-human-colony branch (raw :98863-98873); unreachable in port — AI unit movement never calls the scout-colony menu (only the human's own `game_try_unit_move` does), so an AI scout can never infiltrate a human colony to trigger it |
| `@HELLOFIRST` | Done | Euro first-contact land (`ai_diplo_153e_encounter`) |
| `@HELLOUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@HELLOAHOY` | Done | Euro first-contact sea |
| `@HELLOMEEK` | Done | Euro subsequent meek greeting |
| `@HELLOMANLY` | Done | Euro subsequent manly greeting |
| `@GREATKINGS` | Not wired | `FUN_2a1f_0618(2,"KINGS")` name-prep table (%STRING2 of the @HELLO* greetings), not a popup; folded to the nation name in the port |
| `@GREATDEEDS` | Not wired | `FUN_2a1f_0618(3,"DEEDS")` name-prep table (%STRING3 of the @HELLO* greetings), not a popup; folded in the port |
| `@GREATLEADER` | Not wired | `0618(…,"LEADER")` name-prep table, not a popup; no 153e site reads it |
| `@GREATLEADER2` | Done | `0618(0,"LEADER2")` name-prep table = %STRING0 of @RID / @WARMANLY / @WAR+tone; read by `ai_talk_great_line` (2026-09-08) |
| `@MYLEADER` | Not wired | leader-title name table, not a popup and not a `3f41` tag |
| `@PIRACY` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@PIRACYUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@SIEGES` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@SIEGESUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@MEEKNESS` | Done | %STRING3 "demand"/"request" word, supplied by the 153e talk tokens |
| `@HEATHEN` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@HEATHENUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@APOSTATES` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@APOSTATESUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@TRIBUTE` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@TRIBUTEUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@WANTSTUFF` | Done | 153e demand phase incl. the byte-verified DOS Furs stale-index bug (2026-09-06) |
| `@WANTSTUFFUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@RID` | Done | 153e worthy cascade, 3rd leg (raw :97966): ultimatum OK popup, no war, %STRING1 = target `player.country_name` — ported 2026-09-08 |
| `@RIDUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@WORTHY` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@GIVECASH` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@PEACEMANLY` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@PEACEMEEK` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@OLDPEACEMEEK` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@OLDPEACEMANLY` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@PEACEUSA` | Not wired | USA text variant — deliberate delta (`iStack_9c`); 153e bails on `woi`, so it is unreachable |
| `@NOTWITHDRAW` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@WITHDRAW` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) (teleport-vs-walk delta documented) |
| `@NOTHINGWITHDRAW` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@MAYBEWITHDRAW` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@PROVOKE` | Done | 153e worthy cascade, 1st leg (`worthy && at_peace && score >= 0x65`) → war; also the PEACEMENU threat arm |
| `@WARMEEK` | Done | 153e WAR+tone tail; showing @GIVECASH forces the MEEK tone (raw :98017, 2026-09-08) |
| `@WARMANLY` | Done | 153e WAR+tone tail **and** the post-tribute war declaration (raw :97960, `score == 999`) — 2nd leg ported 2026-09-08 |
| `@THREATS` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@GIFTS` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@MILITARY` | Done | PEACEMENU choice 4 (Military Assistance), raw :98330-98333 — `ai_diplo.c` talk machine (2026-09-16) |
| `@NOCONTACT` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@ALREADYSMITE` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@SMITEINDIANS` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@SMITEEUROPE` | Done | `FUN_5bfb_153e` encounter dialog — live in the `ai_diplo.c` talk machine (2026-09-06) |
| `@UNFORTUNATE` | Done | FUN_5bfb_13b0-family ally-hire affordability check (raw :98368) — `ai_diplo.c` AI_TALK_ST_ALLY_PAY (2026-09-16 verified) |
| `@MERCENARY` | Done | FUN_5bfb_13b0-family ally-hire success (raw :98386), war-bit set both directions incl. Indian side — `ai_diplo.c` AI_TALK_ST_ALLY_PAY (2026-09-16 verified) |
| `@SUCCESSION` | Done thin | `FUN_43f7_0218` Treaty of Utrecht announce — `ai_king.c:1609` `popup_msg_fill("SUCCESSION", …)`; VGA PARKED |
| `@REBELMAJORITY` | Done thin | EOT Phase D ai_popup OK (`turn_emit_sol_phase_d_chrome`); VGA PARKED |
| `@REBELUNANIMOUS` | Done thin | EOT Phase D ai_popup OK; VGA PARKED |
| `@TORYMINORITY` | Done thin | EOT Phase D ai_popup OK; VGA PARKED |
| `@TORYMAJORITY` | Done thin | EOT Phase D ai_popup OK; VGA PARKED |
| `@SONSUP` | Done thin | EOT Phase D decade chrome ai_popup OK; VGA PARKED |
| `@SONSDOWN` | Done thin | EOT Phase D decade chrome ai_popup OK; VGA PARKED |
| `@REBELUP` | Done | `ai_king_beat` decile SoL notify (FUN_43f7_2424 tail, 0x53d8 dedup) — rising, SoL<50, `ai_popup_enqueue_ok_ctx` |
| `@REBELUP50` | Done | same 2424 tail, rising branch with SoL≥50 |
| `@REBELDOWN` | Done | same 2424 tail, falling branch (report > (last+4)/10 hysteresis) |
| `@REFIT` | Done thin | Drydock repair ai_popup OK (`@REFIT`); VGA PARKED |
| `@WELLSEASONED` | Done thin | Indian teach Scout→Seasoned `CONTACT_TEACH` + `@WELLSEASONED` |
| `@KINGBUY` | Done thin | `FUN_43f7_1d42` peacetime royal-purse buy — `ai_king.c:788` `popup_msg_fill("KINGBUY", …)`, %STRING0 pool name; VGA PARKED |
| `@SEIZURE` | Done | `AI_POPUP_TAG_COMBAT_SEIZURE` privateer body |
| `@SEIZURESEA` | Done | Crown naval → Royal Navy text; Privateer uses custom body |
| `@SEIZURELAND` | Done | Crown land win vs human → Royal Army text |
| `@INDEPENDENCE` | Done | `KING_LETTER` + signing cinematic (`declaration.c`) |
| `@INVASION` | Done thin | REF `1528` wave OK/status via `popup_msg_fill`; VGA PARKED |
| `@INTERVENTION` / `@INTERVENE` | Done thin | `10f0` ally declare + landing ARRIVAL |
| `@TOOTORY` | Done | `ai_king_menu_declare_independence` (FUN_43f7_2564 tail, sol<50 branch) — OK notice via `ai_popup_enqueue_ok_ctx` |
| `@DECLARE` | Done thin | ai_popup CHOICE body+labels via `popup_msg_*`; VGA PARKED |
| `@DEADCONVERTS` | Missing | real DOS site: the per-unit tick `FUN_3844_0002` case 0x1a (viceroy_unpacked.c 58287-58299, tag 0xee2) — a Convert standing on open map outside a colony ages a counter at unit +0x16 and is removed on the 9th turn. Neither the expiry nor the popup is ported; the port's `col1_counter16` byte is already multiplexed for voyages / trade-route stops, so this needs its own field decision |
| `@TOOMANYUNITS` | n/a | dead text: no DS string in VICEROY.EXE (absent from popup_tag_ids.md) |
| `@TOOMANYCOLONIES` | n/a | dead text: no DS string in VICEROY.EXE (absent from popup_tag_ids.md) |
| `@PICKMUSIC` | Done | slot / music dialogs |
| `@PICKINDEPENDENCE` | Done | slot / music dialogs |
| `@PICKMILITARY` | Done | slot / music dialogs |
| `@PICKINDIAN` | Done | slot / music dialogs |
| `@UPKEEP` | Missing | production/EOT messages — status or silent; no modal |
| `@MOBILIZE` | Done thin | `FUN_43f7_1eca` single-unit promote — `ai_king.c:4624` `popup_msg_fill`; VGA PARKED |
| `@MOBILIZE2` | Done thin | `FUN_43f7_1eca` multi-unit promote — `ai_king.c:4633` `popup_msg_fill`; VGA PARKED |
| `@CANTMOBILIZE` | n/a | `FUN_43f7_1eca` (viceroy_unpacked.c 74910-74968) never emits this tag: the only messages it sends are 0x132d/0x1336 (`@MOBILIZE`/`@MOBILIZE2`) when `local_a != 0`; the `local_a == 0` (no muskets) fall-through is silent in DOS, and no other decompiled function references this section — dead GAME.TXT text, not ported |
| `@KINGMOBILIZE` | n/a | `FUN_43f7_1d42` wartime arm (0x1320, OVL07:2d8a) is DEAD CODE in the shipped binary — the function's first instruction (`TEST [0x5382],1` / early RETF) makes the whole routine a peacetime-only no-op, so the branch guarding this tag is unreachable; see `ai_king.c:693` comment. Correctly unported |
| `@EUROPENOTAVAIL` | Done | 2026-09-16: `game_try_enter_europe` (`game_loop.c`) now renders the real GAME.TXT body + `AI_POPUP_TAG_INFO` OK popup on WoI-blocked entry, same pattern as `@FOREIGNNOTAVAIL`/`game_open_report`; previously only set a bare status string |
| `@FOREIGNNOTAVAIL` | Done | F8 Foreign Affairs is withdrawn once the WoI has begun — `FUN_3f41_2548` raw :70792 (`0x5382 & 1`); `reports_is_available` + the `game_open_report` popup, 2026-09-08 |
| `@EUROPENOTLEAVE` | Done | DOS site confirmed 2026-09-16: `viceroy_overlays.asm` OVL08_L0040 raw :0x13fd LEA gated by `TEST byte[0x5382],1` (WoI bit) — same bit the port tests in `game_ship_sail_to_europe`/lane-entry gates (`game_loop.c` ~10263-10389), `"EUROPENOTLEAVE"` `popup_msg_fill` sites |
| `@NOWARSDURINGREV` | Done | `FUN_5f7a_0662` tail (raw 99069-99080, asm 5f7a:06c8): during the WoI a human-controlled Euro unit stepping onto the colony of a Euro power that is neither human-controlled nor the Crown is refused, abort + full allotment spent. Wired in `game_move_native_prompts` (game_loop.c) alongside the foreign-trade dispatch, docs/foreign_colony_trade.md |
| `@NOCOLONIESEITHER` | n/a | no asm PUSH/LEA site found for this id (absent from `docs/popup_tag_ids.md`'s asm-scanned table, unlike its GAME.TXT neighbor `@NOWARSDURINGREV` at 0x1af3); dead text in GAME.TXT |
| `@NOMAYORSDURINGREV` | Done | FUN_5f7a_000e Meet-With-Mayor WoI refusal (raw :98838) — `game_loop.c` AI_POPUP_TAG_SCOUT_COLONY choice 1 |
| `@HOWMUCH1` | Done | howmuch colony load |
| `@HOWMUCH2` | Done | confirmed 2026-09-16: shift+drag colony-cargo unload already opens the real amount prompt (`game_loop.c` ~9548, `HOWMUCH_KIND_UNLOAD`) alongside the DOS-matching whole-hold plain drag |
| `@HOWMUCH3` | Done | 2026-09-16: added shift+drag amount prompt for ship-to-ship cargo transfer (`game_loop.c` `UI_DRAG_COLONY_HOLD` handler, new `HOWMUCH_KIND_MOVE` path + `howmuch_move_dst_unit_id`); `game_apply_howmuch_result` now does a real hold-to-hold transfer instead of mis-routing through `game_colony_load_hold` (warehouse load) |
| `@HOWMUCH4` | Done | Europe buy amount |
| `@HOWMUCH5` | Done | Europe sell amount |
| `@AMBUSHHINT` | Done | already real (`turn.c:2583` `popup_chrome_ok("AMBUSHHINT", …)`, paired with `@CONSIDER`) — verified against DOS FUN_4345_0a22; VGA PARKED |
| `@CONSIDER` | Done thin | `FUN_4345_0a22` wartime bell-pool ambush hint — `turn.c:2593` `popup_chrome_ok("CONSIDER", …)`, one-shot per `woi_crosses_event` latch; VGA PARKED |
| `@INTERVENTION` | Done thin | `FUN_43f7_1528` ally-declare arm — `ai_king.c:3368` `popup_msg_fill`; VGA PARKED |
| `@FRIEND` | Done thin | ally-name splice into `@INTERVENTION`/`@INVASION` %STRING2 — `ai_king.c:3306` `assets_msg_find("FRIEND")`; VGA PARKED |
| `@INTERVENE` | Done thin | `FUN_43f7_1528` ally-landing arm — `ai_king.c:3387` `popup_msg_fill`; VGA PARKED |
| `@EXPLOITS` | Done thin | `FUN_41f2_0b70` retire Colonization Rating screen — `game_build_exploits` renders the real header (`%NUMBER0`=rating, `%STRING0`=nation) at `game_loop.c:5289`; VGA/SS art shown, HoF-plate chrome thin |
| `@SCORE` | Done thin | same chain — first `tier+1` `@SCORE` rows split at the comma (category / `%STRING0`=leader surname), `game_loop.c:5324` |
| `@LOSING1` | Done thin | WoI lose all ports — `ai_king_check_revolution_end` |
| `@WARN1` | Done | WoI ports<3, lowest-priority selector arm — `ai_king_check_revolution_end` (`market_demand_pool_raw[6]`) |
| `@LOSING2` | Done thin | WoI lose all colonies — `ai_king_check_revolution_end` |
| `@WARN2` | Done | WoI colonies<3, wins the selector — `ai_king_check_revolution_end` (`market_demand_pool_raw[7]`) |
| `@LOSING3` | Done thin | WoI crown pop share ≥90% — `ai_king_check_revolution_end` |
| `@WARN3` | Done | WoI crown pop share ≥80% (below the 90% `@LOSING3`), outranked by colonies<3 — `market_demand_pool_raw[10]` episode |
| `@WINNING` | Done | WoI win — `ai_king_check_revolution_end`; precedes the `@KINGLOSE` throne audience then the CLOSING.EXE cinematic; win tune pool 3 |
| `@OTHERGRANTED` | Done thin | `FUN_3844_0442` §D rival-nation independence (raw 58596-58611, ids `0xf4b`/`0xf3f` NAMES/INDEPENDENT dual-load resolve to the republic rename) — `turn.c:3340` `turn_year_end_rival_popup`; renames the nation and clears war/ally bits toward every other power; VGA PARKED |
| `@OTHERMIGHT` | Done thin | `FUN_3844_0442` §D rising SoL pressure on a rival (raw 58570-58582, id `0xf5e`) — `turn.c:3322`; hysteresis band ported (`thresh-20`/cache) |
| `@OTHERLESS` | Done thin | `FUN_3844_0442` §D falling SoL pressure on a rival (raw 58583-58593, id `0xf69`) — `turn.c:3336` |
| `@SCORED` | Done thin | peacetime year≥1800 — `AI_POPUP_TAG_KING_SCORED`; That's all opens retire score |
| `@TORYUPRISING` | Done thin | `FUN_43f7_06a6` — `ai_king.c:2597` `popup_msg_fill("TORYUPRISING", …)` with colony name; VGA PARKED |
| `@CANNOTATTACK` | Done | `game_report_enter_reason` (`game_loop.c`) — real OK popup on `COLONIZE_ENTER_BOUNCE_FOREIGN` when the mover is land (non-combat land unit attacking) |
| `@TRADEMERCANTILISM` | Done | `FUN_5f7a_020e` raw 98928-98934 — no Jan de Witt (FF 4); %STRING0 = @GREATLEADER2[owner]. docs/foreign_colony_trade.md |
| `@TRADEATWAR` | Done | `FUN_5f7a_020e` raw 98924-98927 — no peace treaty with the colony's owner (`FUN_281f_0a38 & 0x40`). docs/foreign_colony_trade.md |
| `@TRADENOCARGO` | Done | `FUN_5f7a_020e` raw 98935-98936 — transport with no goods holds occupied. docs/foreign_colony_trade.md |
| `@TRADENOWANT` | Done | `FUN_5f7a_020e` raw 99042-99048 — the colony's warehouse has no affordable counter-offer. docs/foreign_colony_trade.md |
| `@TRADEWITH` | Done | `FUN_5f7a_020e` raw 99013-99018 — the counter-offer CHOICE (goods / gold / refuse); `AI_POPUP_TAG_FOREIGN_TRADE_OFFER`. docs/foreign_colony_trade.md |
| `@EXTINCT` | Done | last village razed → `units.c` `col1_destroy_tribe_at` tail (`FUN_4d56_00e0`, tag 0x14d4) — real GAME.TXT body. Confirmed 2026-09-16 |
| `@MERCENARIES` | Done | ai_popup CHOICE structural |
| `@MERCS` | Done | ai_popup CHOICE structural |
| `@OVERBOARD` | Done | dump Yes/No |
| `@ALREADYREVOLUTION` | Done | ai_popup CHOICE structural |
| `@SUREDISBAND` | Done | disband Yes/No |
| `@NEWCOLONIST` | Done thin | EOT Phase I birth ai_popup OK (`@NEWCOLONIST`); VGA PARKED |
| `@INEFFICIENT` | Done thin | EOT Tory-pressure ai_popup OK (`turn_emit_inefficient_gov_chrome`); VGA PARKED |
| `@EFFICIENT` | Done thin | EOT Tory-pressure clear ai_popup OK; VGA PARKED |
| `@CLEARCUT` | Done thin | pioneer clear-forest → lumber to nearest colony + ai_popup OK; also `@DEFOREST`; Hardy×2 / terrain×20 PARKED |
| `@REALLYBUY` | Done | purchase / train menus |
| `@INDIANWARPATH` | Done | Incite Indians target menu (`ai_contact_enqueue_incite_target_choice`) — real GAME.TXT body 2026-09-16; rows are now plain nation names and are no longer filtered by affordability, as in DOS (viceroy_unpacked.c 83593-83607) |
| `@INDIANWARPATH2` | Done | the pay confirm DOS shows after the target pick (`ai_contact_enqueue_incite_confirm`, tag 0x16c1): real body + `Pay`/`Never mind.` rows; `@NOCONTACT` before the quote, `@UNFORTUNATE` / `@ALREADYSMITE` after it 2026-09-16. Was merged into the menu labels |
| `@INDIANWARFARE` | Done | Incite announcement at `FUN_4d56_417e` LAB_4499 (tag 0x16e9) — real GAME.TXT body via `popup_msg_fill`, fires for both the human incite and the AI missionary auto-incite. Confirmed 2026-09-16 |
| `@LOSENOCOLONIES` | Done thin | Section B zero-colony defeat OK + `ENDGAME_LOST` latch (`turn_run_year_end_chrome`) |
| `@SOONRETIRING0` | Done thin | peacetime Spring 1790 — `ai_king_nation_turn` (`market_demand_pool_raw[8]`) |
| `@SOONRETIRING1` | Done thin | wartime 1840 — `ai_king_nation_turn` (`market_demand_pool_raw[9]`) |
| `@RETIRING` | Done thin | peacetime `@SCORED` That's all → `ai_king_apply_popup_result` |
| `@RETIRING2` | Done thin | WoI year≥1850 + crown alive — `ai_king_check_revolution_end` |
| `@HOWTOWIN` | Done thin | after declare — `ai_king_do_declare` INFO (invent WoI-begins demoted) |
| `@ARTILLERY` | Done | already real (`units.c:3192` `units_combat_enqueue_tok`) — verified against DOS FUN_5fef_016c raw 99475 |
| `@ARTILLERY2` | Done | already real (`units.c:3205` `units_combat_enqueue_tok`) — verified against DOS FUN_5fef_016c raw 99486 |
| `@TIMECHANGE` | Done thin | `FUN_130d_0290` calendar-help — `LEA BX,[0x141]` (Ghidra-dropped tag arg, asm-confirmed) then `CALLF FUN_281f_03fe`, fired once at year==1600 && season==0 (the exact turn the calendar splits into Spring/Autumn) — `turn.c` `turn_processor_advance` TURN_PROC_SETUP, `popup_chrome_ok("TIMECHANGE", …)`; no tutorial-hints gate in DOS |
| `@TEACHCONVERT` | Done | "Live among the natives" with a Convert (`ai_contact.c`) — real GAME.TXT body. Confirmed 2026-09-16 |
| `@SOMEBOYCOTT` | Done | boycotted market-cell click → `europe_buyback_boycott` (FUN_38fd_2dfe) |
| `@KEEPSTOCKADE` | Done | stockade min-pop OK message |
| `@MORETHANTHREE` | Done | building-slot-full OK (§4; not stockade min-pop — that's `@KEEPSTOCKADE`) |
| `@LOOTWAGONS` | Missing | combat/loot modals missing (effects may apply silently) |
| `@TUTORIAL1` | Missing | tutorial hints missing |
| `@TUTORIAL2` | Missing | tutorial hints missing |
| `@TUTORIAL3` | Missing | tutorial hints missing |
| `@TUTORIAL4` | Missing | tutorial hints missing |
| `@TUTORIAL5` | Missing | tutorial hints missing |
| `@TUTORIAL6` | Missing | tutorial hints missing |
| `@TUTORIAL7` | Missing | tutorial hints missing |
| `@TUTORIAL8` | Missing | tutorial hints missing |
| `@TUTORIAL9` | Missing | tutorial hints missing |
| `@TUTORIAL10` | Missing | tutorial hints missing |
| `@TUTORIAL11` | Missing | tutorial hints missing |
| `@TUTORIAL12` | Missing | tutorial hints missing |
| `@TUTORIAL13` | Missing | tutorial hints missing |
| `@TUTORIAL14` | Missing | tutorial hints missing |
| `@TUTORIAL15` | Missing | tutorial hints missing |
| `@TUTORIAL16` | Missing | tutorial hints missing |
| `@TUTORIAL17` | Missing | tutorial hints missing |
| `@TUTORIAL18` | Missing | tutorial hints missing |
| `@TUTORIAL19` | Missing | tutorial hints missing |
| `@TUTNOLUMBER` | Missing | tutorial hints missing |
| `@TUTNOSPACES` | Missing | tutorial hints missing |
| `@KINGLOSE` | Done | WoI won — full-screen throne audience (KINGLSS1 + KINGLOSE.SS, FUN_75c2_20e2); dismissal plays the CLOSING.EXE cinematic (`closing.c`) then the retire score |
| `@KINGWIN` | Done | WoI lost (@LOSING1-3) — full-screen throne audience (KINGWIN.SS); dismissal opens the retire score |
| `@DISBANDSHIP` | Done | ship-with-cargo error OK (not Yes/No) |
| `@NOMOREWAREHOUSE` | Done thin | Warehouse Expansion already owned → ai_popup OK |
| `@NOMOREWAGONS` | Missing | no colony modal (FULL/SIEGE may status) |
| `@BUILD1` | Done | sail captions |
| `@BUILD2` | Done | sail captions |
| `@BUILD3` | Done | sail captions |
| `@BUILD4` | Done | sail captions |
| `@BUILD5` | Done | sail captions |
| `@BUILD6` | Done | sail captions |
| `@BUILD7` | Done | sail captions |
| `@BUILD8` | Done | sail captions |
| `@BUILD9` | Done | sail captions |
| `@BUILD10` | Done | sail captions |
| `@END` | n/a | catalog sentinel |

---

## Appendix B — `AiPopupTag` map

From [`ai_popup.h`](../src/core/ai_popup.h). **Kind OK** means body-only wood
(dismiss Enter/Space/Esc/any click; no invent “OK” label — GAME.TXT info
sections have no response lines). **Kind CHOICE** lists authentic labels.

| Tag | Kind | Typical `@` / FUN | Status |
|-----|------|-------------------|--------|
| `INFO` | OK | Generic notices (revolution, 1800, …) | Done (port queue tag; body always from a GAME.TXT section at the call site) |
| `KING_AUDIENCE` | CHOICE | `@TAXOPTIONS` / `38fd_5be8` | Done |
| `KING_DUMP_GOODS` | CHOICE | Refuse dump cargo / `38fd_3dc8` | Done |
| `KING_TAX` | CHOICE / OK | `@KINGTAX` + `@TAXOPTIONS`; `@TEAPARTY` Done thin | Done |
| `KING_MERC` | CHOICE / OK | `@MERCENARIES` / `@MERCS` | Done |
| `KING_CONGRESS` | CHOICE | `@DECLARE` body+choices / `43f7_2564` | Done thin |
| `KING_LETTER` | OK | `@INDEPENDENCE` / `43f7_160a` | Done — the popup is preceded by the `160a` signing cinematic (`src/core/declaration.c`, 2026-08-30) |
| `KING_ARRIVAL` | OK | `@INVASION` REF `43f7_1528`; `@INTERVENTION`/`@INTERVENE` `10f0` | Done thin |
| `KING_CAPTURE` | OK | `@CAPTURED3` REF take | Done thin |
| `FF_CONGRESS` | CHOICE / OK | `@CONTINENTAL` / `4345_024a` | Done |
| `CONTACT_WELCOME` | CHOICE | `@INDIANWELCOME` / `5bfb_022e` | Done |
| `CONTACT_MEET` | CHOICE | Meet Trade/Gift/Demand/Teach/Leave | Done |
| `CONTACT_GIFT` | CHOICE / OK | Gift amounts | Done |
| `CONTACT_DEMAND` | CHOICE / OK | Tribute demand | Done |
| `CONTACT_TEACH` | OK | `@LEARN*` | Done |
| `CONTACT_CONVERT` | OK | `@MISSION*` / convert | Done |
| `CONTACT_RAID` | OK | `@RAID*` | Done (6/7; `@RAIDWREAK` thin by design) |
| `CONTACT_REFUSE` | OK | `@INDIANSHUN` / refuse | Done |
| `DIPLO_WAR` | CHOICE | `@DECLAREWAR` / `5bfb_153e` | Done |
| `DIPLO_PEACE` | CHOICE | Peace accept/refuse | Done |
| `DIPLO_ALLIANCE` | CHOICE | Alliance | Done |
| `DIPLO_BREAK` | CHOICE | Break alliance | Done |
| `DIPLO_BOYCOTT` | OK | Embargo / Tools lift | n/a — no producer since 2026-09-16 (invented bodies retired) |
| `DIPLO_FA` | OK | Thin FA leftovers (the 153e negotiation uses `DIPLO_TALK`) | n/a — no producer since T2.4 |
| `LANDFALL` | CHOICE | `@LANDFALL` | Done |
| `MAP_CONFIRM` | CHOICE | Disband / overboard / quit / retire / trade-delete | Done |
| `COLONY_EVENT` | CHOICE | Colony EOT messages / `364b_0000`: "Continue turn." / "Zoom to colony." (LABELS `@MISC` 34/35); optionless once zoom elected, colony screen opens after the batch | Done |

---

## MAPEDIT (out of scope)

[`MAPEDIT.TXT`](../COLONIZE/MAPEDIT.TXT) has 19 `@SECTION`s (`MAPTOLOAD`,
`MAPTOEDIT`, `SAVE`/`LOAD`/`ERROR`/`EXIT`/`SAVEAS`, size/continent prompts,
`HELP1`…`5`, `ABOUT`). Presented by MAPEDIT.EXE (`FUN_133d_*`), not the main
game compositor. Not inventoried here.

---

## See also

- [manual_gap.md](manual_gap.md) — feature checklist vs manual
- [port_plan.md](port_plan.md) — AI FUN inventory; structural vs VGA PARKED
- [assets.md](assets.md) — `popup_draw` chrome, colony/map popup UX
- [move_enter.md](move_enter.md) — landfall / meet enter rules
- [turn_between_players.md](turn_between_players.md) — ship-ready / HoF PARKED
- [sons_of_liberty.md](sons_of_liberty.md) — independence / year-end SoL chrome
- [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md) — year-end string ids
