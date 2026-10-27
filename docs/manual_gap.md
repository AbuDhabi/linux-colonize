# Manual vs OpenCol Gap Analysis

Digest of [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) (MicroProse
Instruction Manual + Technical Supplement, ~135 pages) compared to
OpenCol. Deep implementation notes live in the owning feature docs
([Authority table](architecture.md#authority)); this file is the **feature checklist**. Player-facing
modals (GAME.TXT `@SECTION`s vs port Done/Partial/Missing): [popups.md](popups.md).

**Legend**

| Status | Meaning |
|--------|---------|
| Done | Playable roughly as the manual describes |
| Done thin | Behaviour is there and correct, but the presentation is port-drawn rather than DOS-exact |
| Partial | Shell, stub, or simplified behavior |
| Missing | Not meaningfully implemented |

---

## Manual structure (digest)

| Block | Manual pages (approx.) | Topics |
|-------|------------------------|--------|
| Tech Supplement | Front matter | Hardware; keyboard for map / colony / Europe; F2–F10 advisers |
| Intro / setup | 1–9 | Background, difficulty, nationality powers, pre-game options |
| Turn / win | 10–12 | Turn order (natives → EN → FR → SP → DU); quit / retire / revolution / 1800–1850 end |
| New World / map | 13–22 | Terrain, fog, sidebar, menus, save/load |
| Units / orders | 23–29 | Move costs, fortify / sentry / goto, naval transport, trade routes |
| Combat | 30–32 | Meeting engagements; colony defense (stockade / fort / fortress) |
| People / skills | 33–36 | Criminals / servants, skills, immigration, Royal College |
| Colonies | 37–58 | Place colony; people / warehouse / area / buildings; production |
| Europe | 59–66 | Sail, harbor, buy/sell, tax, docks, recruit / hire / train, equip |
| Natives | 67–78 | Tribes, anger, trade, missions, wars |
| Diplomacy / navy | 79–82 | Rival powers, warships, peace |
| Independence / FF | 83–94 | Liberty, REF, Congress, founding fathers by category |
| Lore / charts | 95–end | History essays, terrain / production charts |

---

## Status by subsystem

### Shell and presentation

| Manual feature | Status | Notes |
|----------------|--------|-------|
| NEW WORLD / AMERICA / CUSTOMIZE | Done | `src/core/new_game.c`, `map_gen.c` |
| Difficulty, nation, leader, king audience, sail | Done | Wizard flow functional |
| Hall of Fame | Done | DOS `FUN_41f2_0f56` presenter ported 2026-08-29 (`reports_render_hall_of_fame`): WOODPANL, title `@MISC` #192 centered, 5 shown entries × 3 centered lines ("n. Difficulty Leader of the [Free ]Nation" / "President, <@INDEPENDENT> | General, Continental Army | Leader, Nation Colonies to A.D. year. Score: n" / "--- Colonization_Rating: n% ---"), ranked by Colonization Rating like HALLFAME.DAT word 19. Storage is DOS `HALLFAME.DAT` (5 × 42-byte records, DOS insert rule; read/written since 2026-10-26, `HOF.TXT` retired) |
| Map compositor (terrain, coasts, rivers, forest/hill, resources) | Done | MAPEDIT-faithful; see inventory / assets |
| Fog of war / unexplored blackness | Done | `map.seen` plane; black unseen; PHYS0 **104–107** edge fringe; cheat Reveal. Full DOS reveal model (2026-08-29): `FUN_13f1_0158` sight walk (inset-only, domain-gated outer ring) with the `13f1_000a` per-tile effects — owner-nibble stamp on unowned non-rumour tiles, unit vis bits (`FUN_1427_09ac`, natives only in the core ring), colony pop/fort snapshots (`FUN_364b_1b4c`, `ColonizeColony.pop_on_map/fort_on_map`, round-trips Col1 +0xba/+0xbe); units draw only while the viewer's vis bit is set (`FUN_2f2b_6372`), reset on every move to tile-owner + watching nations (`FUN_1427_0c9a/0968`); ±5 reveal on founding (`13f1_00a6`); reveal on piece select (`2b5a_0e52`); AI Euro EOT reveal on (`00f2`); Pacific discovery fires the real woodcut screen (2026-08-30, `woodcut.c`; `FUN_13f1` → `FUN_12fd_006c(6)`), not a popup. Scenario `.MP` starts fogged (seen cleared on load) |
| Zoom VIEW modes (In/Out/15×12/30×24/60×48/120×96) | Done | `game_map_zoom_*` (`game_loop.c`); FUN_6ba1_000c-equivalent viewport sizing (15<<zoom × 12<<zoom tiles, 16>>zoom on-screen px), but composited by rendering the wider tile grid at native 16px into an offscreen buffer then nearest-neighbor-decimating into the fixed 240×192 viewport — DOS instead redraws with pre-scaled sprite blits per zoom tier |
| Hidden terrain VIEW mode | Done thin | VIEW ~Hidden Terrain / **H** hotkey (`MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN`, `game_loop.c`); equivalent-information peel (units/settlements → non-exempt land PHYS → hills+forest, scrub→Desert), not pixel-1:1 with DOS's three-pass timing. H is contextual: sails a selected ship to Europe when one's selected, else opens the reveal |
| Roads on map | Done | PHYS0 **80** isolated / **81–88** multi-blit stubs via `map_phys0_road_layer_*` (`FUN_6ba1_0938`) |
| Plowed fields on map | Done | PHYS0 **149** via `map_phys0_plow_sprite_at` (main map + colony area) |
| Menu bar, right panel, minimap | Done | `map_menu.c`, `map_panel.c` |
| Colonizopedia | Done | `pedia.c` |
| Reports F1–F10 | Done | `reports.c` |
| Pick Music + BGM | Done | `gsound_vm.c` (literal `GSOUND.COL` driver emulator) + `sound.c`, `pick_music.c`; Sound Options popup (Background/Event/Sound Effects) **Done**, `options_dialog.c`. Song-id table corrected 2026-08-27 (Pick Music *n* is not `0x20+n`) — see [assets_sound.md](assets_sound.md) |
| Situational "Military" BGM cue on combat | Done thin | `units_combat_music_sting` (`units_combat.c`) — DOS-evidenced (segment `5fef`), see [assets_sound.md](assets_sound.md) |
| Digital SFX (`COLDIG.BIN`) | Done | Event ids `0x40..0x5c` are pushed in **AX**, which the decompile often drops. Sample decode, queueing, and mixing are done (`sound.c`, `gsound_vm.c`). The static DOS launch expressions and port contexts have an exact per-ID ledger ([archive/assets_sound_trigger_ledger_2026-10-24.md](archive/assets_sound_trigger_ledger_2026-10-24.md)), including computed and normally unreachable branches; every ID is launched by both programs and the normalized context counts match. See [assets_sound.md](assets_sound.md#trigger-parity-audit-2026-09-24). |
| Col1 save / load | Done | Playable I/O: `col1_save.c`, `col1_bridge.c`. **Not** a complete field map — see [save_format_map.md](save_format_map.md) |

### Units and map orders

Deep mechanics (expected vs OpenCol by context): [unit_orders.md](unit_orders.md).

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Move / wait / skip turn | Done | Arrows, Wait, Space |
| Terrain move costs (forest >1 MP, roads, rivers) | Done | DOS `terr_cost` table + full-MP / partial overspend; road-pair + cardinal-river pair via `map_move_cost_step` — [move_enter.md](move_enter.md). Unit MP is DOS thirds for every unit (`UNITS_MP_PER_TILE = 3`, `movement*3`), the old `*3`-scale PARK closed 2026-08-29 with `port_plan.md` T1.8 |
| Fortify (F), Sentry (S), Disband, Goto (G) | Done | One **Fortify** (land or ship-in-harbor); **Go-To** drag / **G** Place (land) or Port (ship); **Sentry** / **Disband** (Shift+D with Yes/No). ORDERS items enable/hide from selected unit (Clear↔Plow, Port↔Place). Plain letter hotkeys match menu `~` markers; Alt+letter opens bar menus |
| Orders box letters on units | Done | `unit_chrome.c` (FUN_112b_01ba): black silhouette (−2px) + nation fill + order letter + stack under-rect; map, sidebar, Europe, colony Units/transport, Colonizopedia. England fill palette 112. F6/F7 icon rows deferred |
| Pioneer clear / plow / road (P / R) | Done | Multi-turn FUN_479b: clear/plow = `terr_cost+2`, road = `terr_cost`, Hardy halves; −20 tools on complete; clear and plow are separate jobs |
| Board / unload passengers | Done | **O** / **U**; hold icons; land→ocean with room auto-boards (`BOARD`); a zero amount means unload the whole selected hold |
| Dump cargo overboard | Done | ORDERS Dump Cargo Overboard → first goods hold |
| Pillage | Done | DOS hides ORDERS Pillage (`0x317`) unconditionally (`FUN_2b5a_0b34`) and the port does too; there is no improvement-destroying pillage in DOS (port_bugs.md #631). The manual describes a feature the shipped game never exposes. The port's invented, unreachable colony-loot arm was deleted 2026-10-26 |
| Colony auto-disembark when ship enters settlement | Done | Dock + `units_disembark_all` |
| Sentry auto-board when ship leaves tile | Done | Same-tile Sentry land → departing ship to capacity (colony + ocean stack) |
| Wagon Train cannot board a ship | Done | port_bugs.md #482, 2026-09-17: DOS gates boarding on the `@UNIT` **size** column (`DS:0x5238`) being `< 99` — `FUN_4720_00e0` (raw 74644-74648) and the `FUN_4720_015c` candidate walk (raw 74739). Wagon Train's size is the 99 sentinel, so it is skipped; ported in `units_board_stacked` (covers `units_board` and every move-onto-ship path) |
| Treasure Train needs a Galleon | Done | port_bugs.md #484, 2026-09-17: emergent, not a type rule — a passenger is charged its `@UNIT` size column (Treasure 6) against `cargo capacity − goods holds` (`FUN_4720_00e0` raw 74628-74665), and only Galleon / Man-O-War have 6 holds. One Treasure fills a Galleon; six no longer fit — [move_enter.md](move_enter.md) |
| No amphibious assault | Done | port_bugs.md #485, 2026-09-17: a land unit aboard a ship cannot enter any square occupied by another nation (unit stack, village, or empty enemy colony) — `FUN_4720_015c` reason 9 → `@LANDFIRST` (`FUN_4720_049e` table at `4720:060a`, tag `0x1429`). Must unload onto an empty or friendly square first |
| Landfall confirm (one unit ashore) | Done | Ship→bare land: `@LANDFALL` Stay / Make Landfall; passenger pays dest terrain MP + ship −1 MP; ship stays at sea — [move_enter.md](move_enter.md) |
| Ship→native village | Done | `@DONTKNOWSHIPS` / `@MADATSHIPS` (not landfall); [move_enter.md](move_enter.md) |
| Stack picker for partial unload | Done | `unit_stack.c` (wake sentry → select) |
| Trade routes (TRADE menu) | Done | Full DOS flows: create wizard (@TRADEMANY cap, @TRADESTART pickers, @TRADETYPE, @TRADENAMES default + @TRADENAME entry), EDIT TRADE ROUTE screen (`trade_screen.c`, 647e_09da layout: `LABELS @ROUTE` headers, rename band, per-stop destination / unload / load columns, icon click-to-remove, (Delete Destination)), Begin filters sea/land + starting-stop picker, DOS stop service (strict nibble lists; Europe = sell unload list + auto-buy load list; @ROUTELOOP), delete with unit fixup + compaction |

### Colonies

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Found colony (**B**) | Done | Disband → Town Hall + starters + stock dump; name-entry `@COLONY`. `@NOPORT` is suppressed only by adjacency to region-1 open ocean (a lake is not a port). `colonies_can_found` (`colony.c`) rejects arctic/mountain terrain, an occupied tile, an Indian city tile, and any tile Chebyshev-adjacent to an active colony of any nation: the DOS Build Colony handler (`FUN_2b5a_1662`/`16ce` — not `FUN_2b5a_3252`, the arrow-key dispatcher) bounces when the `FUN_0000_2500` metric `max(|dx|,|dy|) + min(|dx|,|dy|)/2` is 1 (re-derived 2026-08-24 with `tools/GhidraDecompileAt.java` against the overlay project; the flattened export is corrupt at that range). AI founding runs through the same gate (`ai_euro_found_with_unit`, `ai_goals_pick_founding_tile_ex_w`). Regression in `unit_colonies` |
| Join colony | Done | ORDERS Join Colony admits selected land unit on owned colony tile; else opens colony screen |
| Colony display chrome | Done | Area 1.5× (24px) tiles; people/transport (+30px) bands; multifunction; Note 1 resource-count strips; sprite-bound building hits (`colony_screen.c`). Building positions DOS-exact (recovered tables); deliberate player-requested divergences: the Production pane's surplus split and 4-row hammer bar. Golden differences #1274-#1286 fixed 2026-10-26 — [colony_screen.md](colony_screen.md) |
| Assign jobs / field work / production numbers | Done | Drag or select-then-click colonists to buildings/area/fence; workplace strips show **output-type badge**; Production tab via `colony_preview.c` — see [building_production.md](building_production.md) |
| Construction queue + buy with gold | Done | Construction tab BUY/CHANGE; Change list uses min-pop / upgrade / FF gates; hammers = accumulated progress; `NAMES` tools×10; settlement banner (name + hammers; click → Change) **Done** thin; buy confirm `@BUYME1` Done thin |
| Build units in a colony (Artillery / Wagon Train / ships) | Done | port_bugs.md #481, 2026-09-17: all **seven** DOS unit projects, raw codes 42–48 = `@UNIT` rows 11–17 (`FUN_15eb_32f8`). Caravel/Merchantman/Galleon/Privateer/Frigate need a **Shipyard** and nothing else (`FUN_15eb_3650`); Man-O-War is out of the decode range. Hammers/tools now derive from the `@UNIT` cost/tools columns via `FUN_15eb_33aa` (`cost×0x20` with a 0x28 floor, `tools×10`) instead of two hardcoded pairs. Completion spawns on the colony tile (`FUN_364b_0114`) — chart in [building_production.md](building_production.md). 2026-09-23 (port_bugs.md #817): the two items previously listed as open both ship. **Wagon Train cap / `@NOMOREWAGONS`** — `colonies_wagon_cap_reached` (`colony_build.c`, DOS-LITERAL `FUN_15eb_3650` raw 13736-13740: `colony_counts[n] <= unit_type_counts[n][12]`, i.e. wagons < colonies) gates both the build-list availability and the spawn arm in `turn_colony.c` (DOS-LITERAL `FUN_364b_0114` raw 56926-56933: `@NOMOREWAGONS` with %NUMBER0 = colony count, `+0x1c |= 0x80`, project and hammers left intact so it re-fires each turn; popup is human-colony only, see port_bugs.md #789). **AI unit projects** — `ai_euro_colony_jobs.c` `ai_euro_5952_set_unit_project` picks Wagon Train `0x0c` (2385 arm: no WAGON_TRAIN flag, year < 0x640, continent presence & 1, ring1 == 0, nearest-village alarm < 0x32) plus the `0x0b`/`0x0f`/`0x10`/`0x11` Artillery/Galleon/Privateer/Frigate arms. |
| Warehouse drag load/unload to ships / wagons | Done | Drag cargo↔hold (icon cursor); **L**/**U**/**=**/**+**; empty holds use `ICONS` **#122**; full unload `@WAREHOUSEFULL` Done thin. **U** = `FUN_479b_0f60` mode 0: the lowest `(price<<4)*qty` hold (the Europe `U` pick), red bar @MISC 22 when empty, the @WAREHOUSEFULL confirm, status "Unloading ... in ..." on the title strip (`game_colony_unload_best_hold`, #1182); **L** = `FUN_479b_11a4`: gate @MISC 21 (no free hold), then the HIGHEST `(price<<4)*min(stock,100)` cargo (Muskets/Tools weight 1, Horses 0 under 102 stock), red bar @MISC 20 when none, status "Loading ... onto <ship type>" (`game_colony_load_best_cargo`, #1184) |
| SoL / Tory display | Done | Col1 rebel_dividend/divisor when present, else 0 (`colony_prod_sol_percent`; the old nation `liberty_bells/4` stand-in is deleted; Bolivar-bonus divergence is port_bugs.md #1089); Tory right-aligned; people row includes fence units — [sons_of_liberty.md](sons_of_liberty.md) |
| Leave colony / abandon | Done | Leave-as popup; Stockade+ keeps ≥3; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Done | Live land via `157e` `local_1a` (Stockade/Fort ×2, Fortress ×3); coastal Fort/Fortress fire EOT. Fence/docks art separate — [combat.md](combat.md) |

### Europe

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Open Europe (**E**), sail via the ship menu | Done | A mouse release over a harbor ship icon, or **S** / **Space** / **Enter** with a ship selected, opens the ship menu (`game_europe_open_ship_menu`, `@EUROPESHIPCLICK` + `@EUROPESHIPOPTIONS`, `FUN_38fd_2bfe`): Move to front / Set sail for the New World (`@SAILAWAY`) / Unload all cargo / No changes. Multi-turn Expected Soon / Bound For *region*; passengers + holds persist; **blocked after declare** (`game_options.woi`). The Set sail row is always offered (DOS hides it for a damaged ship; harbor ships carry no damage flag) |
| Docks immigrants from crosses | Done | Crosses pull from 3-slot recruit pool; Brewster filter + pick-among-pool **Done**; pool slot is DOS `RNG(0,2)` (`FUN_281f_04d4`) not player choice; **R** Recruit UI Done. The `5e52` tail (tax audience, then `@KINGNEWWAR`) is `ai_king_human_5e52_tail` (`ai_king_war.c`) |
| Market bid / ask display | Done | Bottom strip from `NAMES.TXT` `@CARGO` |
| Buy / sell goods (drag / L / = / + / U) | Done | Drag market↔hold; **L**/**=**/**+** buy, **U** sells one hold per press, lowest `(price<<4)*qty` key first (`europe_dos_0f60_pick`, `FUN_479b_0f60`), **-** sells the whole first hold of the selected cargo, **_** asks the amount first (`FUN_38fd_23c4`); unload-everything is the ship menu's row 3 (port_bugs.md #1176) |
| Buy ships / artillery | Done | **P** purchase menu (screenshot gold: Artillery 500 … Frigate 5000) |
| Hire Royal University / Train | Done | **T** / `@JOB` hire costs; expert → docks |
| Recruit pool (3) + passage | Done | **R** dialog; passage = real DOS `FUN_38fd_4884` formula (2026-08-15, was linear start-100/+16 placeholder) — `europe_compute_recruit_passage`, see `europe.h` |
| Dock sentry / board on sail | Done | Default sentry; dock click is GAME.TXT `@ARMOPTIONS` (12 rows, DOS `FUN_38fd_37xx`) — not the old invented four-row Don't/Board/Move-front menu |
| Equip muskets / horses / tools; bless missionary | Done | Europe dock `@ARMOPTIONS` **Done** (2026-08-31): omit disabled rows, grey unaffordable, boycott test, Convert cannot arm/bless; type transitions DOS-exact. Colony fence Tools/muskets/horses + Leave-as Missionary with Church/Cathedral **Done**; colony admit dumps gear |
| Tax rate / boycotts / king tax events | Done | Structural tax→REF + refuse/boycott flag (`ai_king`); audience UI **Done** structural (`ai_popup`); Europe screen enforces the boycott itself: `europe_cargo_boycotted` gates `europe_buy_cargo`/`europe_sell_hold`/`europe_sell_unit_hold`, boycotted market cell price drawn in red — `EuropeScreen.boycott_bitmap` mirrors `nation.boycott_bitmap` live each Europe-screen render (`game_loop.c`); Custom House intentionally still bypasses it (fandom). Boycott buy-back **Done** (2026-08-24): `europe_buyback_boycott` (`FUN_38fd_2dfe`) — clicking a boycotted market cell (real DOS trigger, GAME.TXT `@SOMEBOYCOTT`) pays `ask_price × 500` gold back taxes (fandom "500 tons of that good", confirmed by the traced decompile constant), crediting the same amount to `nation.royal_money` (Crown REF budget — the DOS write really lands on that field) and clearing the boycott bit; insufficient funds is a no-op. Wired at `game_loop.c` `EUROPE_HIT_MARKET`; every other human buy/sell entry on a boycotted cargo (L/=/+/-/_, hold drag onto the market) raises `@KISSUP` through `game_europe_boycott_gate` and aborts, and `U` raises `@SOMEBOYCOTT` only when its last-examined hold was boycotted (port_bugs.md #1167). `@KISSUP`/`@KISSSORRY` chrome **Done** (2026-09-16): the click now raises DOS's own two-row `@KISSUP` CHOICE (`AI_POPUP_TAG_EUROPE_KISSUP`) quoting the back taxes, and only its "Pay" row tests the purse — short funds get the `@KISSSORRY` OK instead of a silent no-op, the order `FUN_38fd_2dfe` uses. The King's tax audience also shows its real per-rung text now (`@KINGVICTORY` / `@KINGWIFE` / `@KINGWAR` / `@KINGNAVACT` / `@KINGSTAMPACT`) rather than `@KINGTAX` for every trigger — [port_plan.md](port_plan.md) |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Done | Full EOT chain in `turn.c` (`turn_produce_one_colony`): field yield, food consume/starve/birth, horse breeding, manufacturing (`colony_craft_one_colony`), hammers; tier rates + class scale DOS-confirmed — [building_production.md](building_production.md) |
| Liberty bells / crosses counters | Done | Bell order `FUN_15eb_1f72`, nation `+0xc` FF pool, per-colony elect (statesman audit 2026-09-24); crosses `5e52` tick before the colony add (preacher audit); FF election via `founding_fathers_tick` |
| Full production formulas, spoilage, boycotts | Done | SoL/Tory net mod (`colony_prod_sol_bonus`); warehouse spoilage clamp EOT (`colonies_apply_warehouse_spoilage` / FUN_15eb_0a50); boycotts structural diplo |
| Market prices driven by trade volume | Done | Every buy/sell runs `europe_apply_trade_volume` (`38fd_1dfa`/`1d80`, Done exact) + `FUN_38fd_0058` rise/fall; the EOT tick runs per nation (human `europe_tick_market_prices_w`, others `europe_nation_tick_market_prices_w`, #1012) incl. the AI-only price ceilings, byte-exact vs two DOS turn pairs (`golden_market_prices01`, P6.1); `@PRICEUP`/`@PRICEDOWN` are real OK popups — [europe_nation_eot.md](../original_sources_annotated/turn/europe_nation_eot.md) |
| Turn order: natives first, then EN→FR→SP→DU | Done | The manual's "natives first" is **not** what DOS `130d` runs: relative to the human's end of turn it is Euro slots above the human → Indian mid-pass → Euro slots below, then the king. `turn_processor_advance` splits `TURN_PROC_EURO` around `TURN_PROC_INDIAN` the same way, with `TURN_PROC_KING` after FINISH — [turn_between_players.md](turn_between_players.md) |

### Indians

Topic hub (graphics, units, settlements, contact): [indians.md](indians.md); alarm model: [indian_alarm.md](indian_alarm.md).

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Done | Map/minimap icons + seed-100 placement + `152e` growth and the `021a` Brave picker, all live gates (`golden_mapgen_seed100`, `golden_ai_turns`) with no fitted rows since 2026-10-04 |
| Meet menus, trade, teach skills | Done thin | Real DOS `@ACTIONS` meet menu with per-unit gating (`FUN_4d56_4528` human arm, `port_plan.md` P8.8, 2026-08-28) — Trade / hostile-village / Speak With Chief / Establish Mission / Denounce / Live Among / Demand Tribute / Attack; village trade rewritten against the clean `2820` recovery 2026-08-29 (real quantities and buy/sell ordering). The `2820`/`2e92` haggle sub-loops are ported (`ai_contact_trade.c`); VGA chrome is D4 — [port_plan.md](port_plan.md) |
| Missions / convert / incite | Done | Missions are established only at a village: the human @ACTIONS rows 3/4 (`thunk_FUN_1000_a5dc` → `ai_contact_establish_mission`, `a594` → `ai_contact_denounce_heresy` with DOS's real weighted roll, `@HERESY0/1`, `MISSION0-3`), and the AI through `FUN_4d56_4528`'s non-human unit-type switch case 3 (`ai_contact_ai_missionary_village`: incite case 7 → establish case 3 → denounce case 4). The invented AI adjacency convert pulse, its 50/50 heresy, crosses bump, alarm decay and the missionary flee arm were deleted 2026-09-22 (port_bugs.md #557). Converts reaching a colony are the separate `FUN_5bfb_022e` `@INDIANSCONVERT` arm. HELLO1/2 greet **Done** thin; raid surprise/war chrome **Done** thin; incite/WARPATH gold **Done** for both arms (`FUN_4d56_417e` → `ai_contact_apply_incite`, 6th village-meet CHOICE; AI Mode 2 `ai_contact_ai_incite_human`, `port_plan.md` T4.5; [`indian_incite_417e.md`](../original_sources_annotated/ai/indian_incite_417e.md)) |
| Alarm, raid, Indian wars | Done | Structural contact/raids (`ai_contact_*`, `@RAID*` tribe+colony status + ambush WIN1/2 / surprise / war); colony encroachment **Done** thin; player dialog **Done** structural (`ai_popup`); `4528` **Done** 2026-08-27/28 (both arms); `2820` haggle ported |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Done | Playable bar + fort bit7/Drydock + the `20e6` attack term (LAB_52aa) and `−0x6790` stance table; the village arm is now the full `4528` action dispatch (P8.8 human menu incl. Attack Village, AI attack via the `20e6` village arm, `ai_euro_land.c`), not the old invented warn CHOICE. VGA chrome D4 — [combat.md](combat.md) |
| Capture colony | Done | `colonies_capture` on player enter (empty/cleared foreign Euro) + AI / combat paths; Indian raid abandons — [combat.md](combat.md) |
| Stockade / fort / fortress defense % | Done | Live land combat via `157e` `local_1a`; coastal Fort/Fortress fire `units_coastal_fort_fire_pulse_w`. Attack MP (full exhaust, 2026-09-10) + close-hit bit7 + Drydock repair Done — [combat.md](combat.md) |
| Rival war / peace / privateers | Done | Euro bilateral war/ally/peace + Furs embargo + Privateer spawn (`ai_diplo_*`); Indian×Euro matrix + `153e` talk machine; the invented alliance machinery (ally-aid, FA gift, break trust) was retired 2026-09-06. The ally list carries GAME.TXT `@MILITARY` as its prompt (5bfb:2d59, fixed 2026-10-26, #1287). Left: the `3f41` F8 report chrome is D4 |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Done | Side-table bell pool (DOS +0xc); peacetime threshold elect + WoI pool→intervention (**Done** 2026-08-22); century-weighted debate pick (`4345_06d2`/`015a` **Done**); all 25 Fathers wired (2026-08-28); KINGGALLEON2 **Done** 2026-08-27 (`FUN_5fef_1908`); F3 portrait grid **Done**; FF pool bridge load **Done** |
| Pedia / F3 Congress report | Done | F3 page 1+2 match goldens (2026-08-25); debating highlight; leftover polish: fatter-than-DOS bell glyph, not a missing plate — [reports.md](reports.md) |
| Sons of Liberty %, declare independence | Done | SoL + auto-declare structural (`ai_king`); player confirm UI **Done** structural (`ai_popup`); `@INDEPENDENCE` letter OK **Done** thin; signing cinematic **Done** 2026-08-30 (`FUN_43f7_160a` → `src/core/declaration.c`: DECOIND.PIK + DEC-UPP/LOW/SQIG.SS quill animation; `DECLARAT.PIK` turned out to be an unreferenced leftover) — [sons_of_liberty.md](sons_of_liberty.md) |
| REF invasion / revolution combat | Done | REF wave / war act structural; merc hire dialog **Done** structural (`ai_popup`); win/lose latches **Done** thin; `10f0` landing scorer + caps + Veteran 0x15 **Done** Phase 5; `backup_force[2]`/`[3]` index-swap bug fixed 2026-08-24 (was feeding the wrong pool formula into both the Man-O-War/`2022` merc-gate check and the Artillery land-troop drain — `ai_king_seed_backup_force_1a26` / `ai_king_merc_offer`, verified byte-for-byte against `FUN_43f7_10f0`/`FUN_43f7_0082`/`FUN_43f7_2022`); foreign MoW ship **Done** 2026-08-28 static (`port_plan.md` P5.5: the whole force is spawned player-controlled via `281f_095c(type, DS:0x5398, …)`; the Man-O-War takes the best water 8-neighbour of the colony — no foreign unit, −999 for a REF MoW, score = 1 + its land neighbours on the colony's continent without a colony — then Cont. Cav. ≤2 / Artillery ≤2 / Cont. Army = 6 − those, `+0x15` Veteran, unloaded at the colony, 5×5 reveal); signing cinematic **Done** 2026-08-30 (see the SoL row — DECOIND.PIK, not DECLARAT.PIK) |
| F10 Colonization Score | Done | `FUN_41f2_0092` byte-faithful 2026-08-29 (`reports_compute_score`): every component + gate (gold ≥1000, villages, 0x53d0 rebel, Early Revolution from the 0x53a7/8 declare year ×2, REF-present bells/100 cap 100, Independence Achieved 100>>prior with ×(8+(8>>prior))/8 total, SCORING COMPLETE on 0x5382&0x10); conditional lines rendered in DOS order, `score.png` golden unchanged |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Done | Path-independent `FUN_75c2_235c`: all four fleets at sentinel `228+n`, counter 0, goto = landfall — `FUN_684c` HS-rim on generated maps, `@SCENARIO` on AMERICA/TRIBE.TXT (port_bugs.md #490) — then Europe exit via `48d3_048e` / `ai_euro_act_*` dispatcher; seed-100 early fixture still gated — [port_plan.md](port_plan.md) |
| Unload, found colonies, combat, colony AI | Done | Not claimed 1:1. Rival-Euro AI closed at logic level 2026-09-07g (D1): `0a60` goals, `5d04` hire ladder (the invented OpenCol matrix is deleted), `20e6` land arms with the `3558` cargo matrices, colony tick and Col1 colony flags/SoL latches, BUY `hammers_purchased`, `depletion_counter` wrap, `warehouse_level`/`capitol_level`. Gates: the four DOSBox idle campaigns ([idle_campaign.md](idle_campaign.md)) plus `golden_ai_turns`, `smoke_ai_mid01`/`late01`. The Treasure/Missionary/Wagon band tail closed 2026-10-26 (port_bugs.md #1291) — [ai_euro_logic_map.yaml](ai_euro_logic_map.yaml) |

### Win / end sequences

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Quit / Exit | Done | |
| Retire → score / HoF | Done | `FUN_41f2_14a8` chain 2026-08-29: `@RETIRE` confirm → F10 → exploits screen (`FUN_41f2_0b70`: WOODPAN2, `@EXPLOITS` + `@SCORE` build-up list, `SCORE<tier+1>.SS` picture, surname-substituted name; skipped when no tier / scoring complete) → Hall of Fame → title; the entry persists in `HALLFAME.DAT` |
| Revolution victory / failure | Done thin | `@WINNING` / `@LOSING1`–`3` / `@RETIRING2` + latches; `@WARN1`–`3` Done thin |
| Auto-end 1800 / 1850 | Done thin | Peacetime year 1800 `@RETIRING` / WoI 1850 `@RETIRING2` → retire score → Hall of Fame → `@SCORED` (port_bugs.md #1231/#1232; Keep playing sets `calendar_latch` and the war goes on); `@SOONRETIRING0`/`1`; WoI `@WINNING` → `@KINGLOSE` → CLOSING.EXE cinematic (`closing.c`) → score |

---

## Takeaway

Every manual feature that exists in the shipped DOS game is ported (re-rated
2026-10-26; no row is Partial or Missing). "Done thin" rows are behaviourally
correct with port-drawn presentation, which is deferred phase D4. Gaps below
this table's granularity are listed in [port_plan.md](port_plan.md)
"Remaining work" (colony-screen Tab / cursor-key focus). The hidden demo and
autoplay (argv `-D`, CHEAT Set Human "None") are ported ([demo.md](demo.md)). The hidden DOS hot-seat
mode is ported ([hotseat.md](hotseat.md)).

## See also

- [port_plan.md](port_plan.md) — whole-project phases / what’s next
- [original_index.md](original_index.md) — decomp / data navigation
- [unit_orders.md](unit_orders.md) — unit order mechanics + port status
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout / interop
- [save_format_map.md](save_format_map.md) — opaque field atlas
