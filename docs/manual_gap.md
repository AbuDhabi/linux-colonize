# Manual vs Linux Port Gap Analysis

Digest of [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) (MicroProse
Instruction Manual + Technical Supplement, ~135 pages) compared to the current
Linux bring-up. Deep implementation notes live in [decomp_inventory.md](decomp_inventory.md)
and [assets.md](assets.md); this file is the **feature checklist**. Player-facing
modals (GAME.TXT `@SECTION`s vs port Done/Partial/Missing): [popups.md](popups.md).

**Legend**

| Status | Meaning |
|--------|---------|
| Done | Playable roughly as the manual describes |
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
| Hall of Fame | Done | DOS `FUN_41f2_0f56` presenter ported 2026-08-29 (`reports_render_hall_of_fame`): WOODPANL, title `@MISC` #192 centered, 5 shown entries × 3 centered lines ("n. Difficulty Leader of the [Free ]Nation" / "President, <@INDEPENDENT> | General, Continental Army | Leader, Nation Colonies to A.D. year. Score: n" / "--- Colonization_Rating: n% ---"), ranked by Colonization Rating like HALLFAME.DAT word 19. Storage stays the text `HOF.TXT` (now `score|leader|nation|year|difficulty|rating|declared|achieved|nation_id`, older rows still load; cap 10 stored, 5 shown) — DOS binary HALLFAME.DAT interop not attempted |
| Map compositor (terrain, coasts, rivers, forest/hill, resources) | Done | MAPEDIT-faithful; see inventory / assets |
| Fog of war / unexplored blackness | Done | `map.seen` plane; black unseen; PHYS0 **104–107** edge fringe; cheat Reveal. Full DOS reveal model (2026-08-29): `FUN_13f1_0158` sight walk (inset-only, domain-gated outer ring) with the `13f1_000a` per-tile effects — owner-nibble stamp on unowned non-rumour tiles, unit vis bits (`FUN_1427_09ac`, natives only in the core ring), colony pop/fort snapshots (`FUN_364b_1b4c`, `ColonizeColony.pop_on_map/fort_on_map`, round-trips Col1 +0xba/+0xbe); units draw only while the viewer's vis bit is set (`FUN_2f2b_6372`), reset on every move to tile-owner + watching nations (`FUN_1427_0c9a/0968`); ±5 reveal on founding (`13f1_00a6`); reveal on piece select (`2b5a_0e52`); AI Euro EOT reveal on (`00f2`); Pacific discovery fires the real woodcut screen (2026-08-30, `woodcut.c`; `FUN_13f1` → `FUN_12fd_006c(6)`), not a popup. Scenario `.MP` starts fogged (seen cleared on load) |
| Zoom VIEW modes (In/Out/15×12/30×24/60×48/120×96) | Done | `game_map_zoom_*` (`game_loop.c`); FUN_6ba1_000c-equivalent viewport sizing (15<<zoom × 12<<zoom tiles, 16>>zoom on-screen px), but composited by rendering the wider tile grid at native 16px into an offscreen buffer then nearest-neighbor-decimating into the fixed 240×192 viewport — DOS instead redraws with pre-scaled sprite blits per zoom tier |
| Hidden terrain VIEW mode | Done thin | VIEW ~Hidden Terrain / **H** hotkey (`MAP_MENU_ACTION_VIEW_HIDDEN_TERRAIN`, `game_loop.c`); equivalent-information peel (units/settlements → non-exempt land PHYS → hills+forest, scrub→Desert), not pixel-1:1 with DOS's three-pass timing. H is contextual: sails a selected ship to Europe when one's selected, else opens the reveal |
| Roads on map | Done | PHYS0 **80** isolated / **81–88** multi-blit stubs via `map_phys0_road_layer_*` (`FUN_6ba1_0938`) |
| Plowed fields on map | Done | PHYS0 **149** via `map_phys0_plow_sprite_at` (main map + colony area) |
| Menu bar, right panel, minimap | Done | `map_menu.c`, `map_panel.c` |
| Colonizopedia | Done | `pedia.c` |
| Reports F1–F10 | Done | `reports.c` |
| Pick Music + BGM | Done | `gsound_vm.c` (literal `GSOUND.COL` driver emulator) + `sound.c`, `pick_music.c`; Sound Options popup (Background/Event/Sound Effects) **Done**, `options_dialog.c`. Song-id table corrected 2026-08-27 (Pick Music *n* is not `0x20+n`) — see [assets.md](assets.md) Music / sound |
| Situational "Military" BGM cue on combat | Done thin | `units_combat_music_sting` (`units.c`) — DOS-evidenced (segment `5fef`), see [assets.md](assets.md) Music / sound |
| Digital SFX (`COLDIG.BIN`) | Done | **Wired 2026-08-27, completed 2026-08-29** (`port_plan.md` P3.2 / P3.7 both closed) — the earlier "no reachable trigger" verdict was wrong: event ids `0x40..0x5c` are pushed in **AX**, which the decompile drops. Sample table, decode, queueing and mixing are done (`sound.c`, `gsound_vm.c`), and every reachable push site is wired: attack fire + the `5fef_1b0e` typed unit-class variants, combat won, `0x44`/`0x45` colony-defender tail, `0x4d` naval win, raid outcomes (`0x4d`/`0x4e`/`0x4f`/`0x5b`), ship sunk, wagon move, tax raise / tea party, colony burned, found colony / colony enter, fortify, sentry, King's Galleon, and the `0x8020`/`0x8024` chord stings (driver table `0x2AB6`). Ids `0x4c`/`0x50`/`0x51`/`0x55`/`0x5c` have no reachable DOS push site (typed-rule dead ends). Only the Retire tune's coin tier stays PARKed ([difficulty.md](difficulty.md)). See [assets.md](assets.md) "COLDIG.BIN" |
| Col1 save / load | Done | Playable I/O: `col1_save.c`, `col1_bridge.c`. **Not** a complete field map — see [save_format_map.md](save_format_map.md) |

### Units and map orders

Deep mechanics (expected vs Linux by context): [unit_orders.md](unit_orders.md).

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Move / wait / skip turn | Done | Arrows, Wait, Space |
| Terrain move costs (forest >1 MP, roads, rivers) | Done | DOS `terr_cost` table + full-MP / partial overspend; road-pair + cardinal-river pair via `map_move_cost_step` — [move_enter.md](move_enter.md). Unit MP is DOS thirds for every unit (`UNITS_MP_PER_TILE = 3`, `movement*3`), the old `*3`-scale PARK closed 2026-08-29 with `ai_port_plan.md` T1.8 |
| Fortify (F), Sentry (S), Disband, Goto (G) | Done | One **Fortify** (land or ship-in-harbor); **Go-To** drag / **G** Place (land) or Port (ship); **Sentry** / **Disband** (Shift+D with Yes/No). ORDERS items enable/hide from selected unit (Clear↔Plow, Port↔Place). Plain letter hotkeys match menu `~` markers; Alt+letter opens bar menus |
| Orders box letters on units | Done | `unit_chrome.c` (FUN_112b_01ba): black silhouette (−2px) + nation fill + order letter + stack under-rect; map, sidebar, Europe, colony Units/transport, Colonizopedia. England fill palette 112. F6/F7 icon rows deferred |
| Pioneer clear / plow / road (P / R) | Done | Multi-turn FUN_479b: clear/plow = `terr_cost+2`, road = `terr_cost`, Hardy halves; −20 tools on complete; clear and plow are separate jobs |
| Board / unload passengers | Done | **O** / **U**; hold icons; land→ocean with room auto-boards (`BOARD`) |
| Dump cargo overboard | Done | ORDERS Dump Cargo Overboard → first goods hold |
| Pillage | Partial | ORDERS: military loots foreign Euro colony stock or clears plow/road; thin vs full `2b5a` body |
| Colony auto-disembark when ship enters settlement | Done | Dock + `units_disembark_all` |
| Sentry auto-board when ship leaves tile | Done | Same-tile Sentry land → departing ship to capacity (colony + ocean stack) |
| Landfall confirm (one unit ashore) | Done | Ship→bare land: `@LANDFALL` Stay / Make Landfall; passenger pays dest terrain MP + ship −1 MP; ship stays at sea — [move_enter.md](move_enter.md) |
| Ship→native village | Done | `@DONTKNOWSHIPS` / `@MADATSHIPS` (not landfall); [move_enter.md](move_enter.md) |
| Stack picker for partial unload | Done | `unit_stack.c` (wake sentry → select) |
| Trade routes (TRADE menu) | Partial | Create/Edit/Delete; Begin aims+cycles stops; stop service honors Col1 load/unload nibbles when counts>0 (else unload-all / surplus ladder); Edit autofill + thin cargo picker (unload→load multi-select); Europe sell on 999; VGA TRADE chrome PARKED |

### Colonies

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Found colony (**B**) | Done | Disband → Town Hall + starters + stock dump; name-entry `@COLONY`. **Distance gate confirmed 2026-08-24** (static decomp trace, `colony.c`'s `colonies_can_found`): `colonies_can_found` rejects arctic/mountain terrain, an occupied tile, an Indian city tile, and — the once-open item — any tile Chebyshev-adjacent (`dx<=1 && dy<=1`) to an existing active colony (own or foreign). That `dx<=1&&dy<=1` rule shipped 2026-08-14 (`3abe4c4`) with a since-corrected wrong citation; this pass re-derived it independently via `tools/GhidraDecompileAt.java`/`GhidraListXRefs.java` against the `OvlWork/Ovl` overlay Ghidra project (the canonical `viceroy_unpacked.c` export of this address range is corrupted — jumptable/EMS-mapping garbage, a false alarm the way `ai_port_plan.md`'s Method notes describe) and confirmed it byte-faithful: the real Build Colony order handler (`FUN_2b5a_1662`/`16ce`, an undocumented gap in `FUNCTION_CATALOG.md` between `FUN_2b5a_1454` and `FUN_2b5a_199e` — NOT `FUN_2b5a_3252`, which is the numpad/arrow-key movement dispatcher and has no connection to founding at all) calls `FUN_1000_8804` → `FUN_15eb_0142`/`FUN_0000_5ff2` ("nearest colony" utility, any nation/any type) and bounces when the `FUN_0000_2500` distance metric (`max(|dx|,|dy|) + min(|dx|,|dy|)/2`) equals 1 — which evaluates to exactly 1 for all 8 Chebyshev-adjacent tiles and 0 only for the same tile (caught separately by the occupied-tile case). The bounce dialog formats the winning colony's name into a substitution slot before showing an opaque numeric GAME.TXT id (`0x9a5`, one of the DOS "unrecoverable binary popup string ids" `popup_string_resolver.md` already documents as needing a live capture to resolve to its `@TAG` — not chased further here since the gate condition + name substitution alone already match `@TOONEAR`'s text and structure unambiguously, and the sibling ids in the same function line up with `@TOOMOUNTAIN`/`@TOONEARBUILD`). AI founding already inherits this for free — `ai_euro_found_with_unit` and every AI found-tile call site gate through the same `colonies_can_found`, and `ai_goals_pick_founding_tile_ex` (the "second-wave" scorer) already filters candidate tiles through `colonies_can_found` too — so no separate AI-side wiring was needed. Regression-locked: new `unit_colonies` assertion isolates a Chebyshev-adjacent-but-otherwise-valid neighbor tile and confirms `colonies_can_found` rejects it. Bonus: the previously-cited `unit_ai_euro_expand`'s `unit_construction_labor_stockade` first-failure-blocks-suite bug is confirmed fixed by the same 2026-08-14 gate (verified passing on this pass) — `port_plan.md` W2.2's "(a)" half is done; "(b)" (auditing other single-`main()` binaries for the same pattern) is still open. Queue: [port_plan.md](port_plan.md) W1.2 (closed) |
| Join colony | Done | ORDERS Join Colony admits selected land unit on owned colony tile; else opens colony screen |
| Colony display chrome | Partial | Area 1.5× (24px) tiles; people/transport (+30px) bands; multifunction; Note 1 resource-count strips; sprite-bound building hits (`colony_screen.c`) |
| Assign jobs / field work / production numbers | Partial | Drag or select-then-click colonists to buildings/area/fence; workplace strips show **output-type badge**; Production tab via `colony_preview.c` — see [building_production.md](building_production.md) |
| Construction queue + buy with gold | Partial | Construction tab BUY/CHANGE; Change list uses min-pop / upgrade / FF gates; hammers = accumulated progress; `NAMES` tools×10; settlement banner (name + hammers; click → Change) **Done** thin; buy confirm `@BUYME1` Done thin |
| Warehouse drag load/unload to ships / wagons | Done | Drag cargo↔hold (icon cursor); **L**/**U**/**=**/**+**; empty holds use `ICONS` **#122**; full unload `@WAREHOUSEFULL` Done thin |
| SoL / Tory display | Partial | Col1 rebel_dividend/divisor when present; else nation liberty_bells/4 stand-in (`colony_prod_sol_percent`); Tory right-aligned; people row includes fence units — [sons_of_liberty.md](sons_of_liberty.md) |
| Leave colony / abandon | Partial | Leave-as popup; Stockade+ keeps ≥3; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Done | Live land via `157e` `local_1a` (Stockade/Fort ×2, Fortress ×3); coastal Fort/Fortress fire EOT. Fence/docks art separate — [combat.md](combat.md) |

### Europe

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Open Europe (**E**), sail **H** / **S** | Done | Multi-turn Expected Soon / Bound For *region*; passengers + holds persist; **blocked after declare** (`game_options.woi`) |
| Docks immigrants from crosses | Partial | Crosses pull from 3-slot recruit pool; Brewster filter Done; pool slot is DOS `RNG(0,2)` (`FUN_281f_04d4`) not player choice — **Done** (2026-08-15, was mislabeled "pick-among-pool UI PARKED"). Interactive **R** Recruit UI (3-slot picker, passage price, selection highlight → `europe_recruit_from_pool`) confirmed **Done** (2026-08-24 re-check, stale "Recruit UI PARKED" wording removed — `game_loop.c` `EUROPE_MENU_RECRUIT`). Atomic `5e52` phase-6 tax-audience/FF-gift tail (raw decompile 68617-68620: `5be8` king tax then `5930` FF grant) still **PARKED** — not a Europe-screen item, belongs to `ai_king.c`/`founding_fathers.c` (out of this file's domain) |
| Market bid / ask display | Done | Bottom strip from `NAMES.TXT` `@CARGO` |
| Buy / sell goods (drag / L / = / + / U) | Done | Drag market↔hold; **L**/**=**/**+** buy, **U**/**-**/**_** sell |
| Buy ships / artillery | Done | **P** purchase menu (screenshot gold: Artillery 500 … Frigate 5000) |
| Hire Royal University / Train | Done | **T** / `@JOB` hire costs; expert → docks |
| Recruit pool (3) + passage | Done | **R** dialog; passage = real DOS `FUN_38fd_4884` formula (2026-08-15, was linear start-100/+16 placeholder) — `europe_compute_recruit_passage`, see `europe.h` |
| Dock sentry / board on sail | Partial | Default sentry; Don’t/Board/Move-front menu; full equip/bless later |
| Equip muskets / horses / tools; bless missionary | Partial | Tools/muskets/horses on units; Leave-as Missionary with Church/Cathedral **Done**; fence icons; colony admit dumps gear |
| Tax rate / boycotts / king tax events | Partial | Structural tax→REF + refuse/boycott flag (`ai_king`); audience UI **Done** structural (`ai_popup`); Europe screen enforces the boycott itself: `europe_cargo_boycotted` gates `europe_buy_cargo`/`europe_sell_hold`/`europe_sell_unit_hold`, boycotted market cell price drawn in red — `EuropeScreen.boycott_bitmap` mirrors `nation.boycott_bitmap` live each Europe-screen render (`game_loop.c`); Custom House intentionally still bypasses it (fandom). Boycott buy-back **Done** (2026-08-24): `europe_buyback_boycott` (`FUN_38fd_2dfe`) — clicking a boycotted market cell (real DOS trigger, GAME.TXT `@SOMEBOYCOTT`) pays `ask_price × 500` gold back taxes (fandom "500 tons of that good", confirmed by the traced decompile constant), crediting the same amount to `nation.royal_money` (Crown REF budget — the DOS write really lands on that field) and clearing the boycott bit; insufficient funds is a no-op. Wired at `game_loop.c` `EUROPE_HIT_MARKET`. `@KISSUP`/`@KISSSORRY` Pay/Cancel CHOICE dialog chrome still PARKED — ported as immediate action + status line, matching this screen's existing chrome-PARKED precedent — [ai_transcription.md](ai_transcription.md) |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Partial | Full EOT chain in `turn.c` (`turn_produce_one_colony`): field yield, food consume/starve/birth, horse breeding, manufacturing (`colony_craft_one_colony`), hammers; tier rates + class scale still DOS-unconfirmed — [building_production.md](building_production.md) |
| Liberty bells / crosses counters | Partial | Accumulators; FF election via `founding_fathers_tick` |
| Full production formulas, spoilage, boycotts | Partial | SoL/Tory net mod (`colony_prod_sol_bonus`); warehouse spoilage clamp EOT (`colonies_apply_warehouse_spoilage` / FUN_15eb_0a50); boycotts structural diplo |
| Market prices driven by trade volume | Partial | T0: buy/sell update `trade_nr` + FUN_38fd_0058 rise/fall ±1 bid (`europe_apply_volume_price`); EOT attrition + colony→`price_group_state` half **Done** thin; rise/fall status line now real GAME.TXT `@PRICEUP`/`@PRICEDOWN` wording (2026-08-24, was a generic placeholder) — [europe_nation_eot.md](../original_sources_annotated/turn/europe_nation_eot.md); modal CHOICE dialog chrome (VGA box, not just the text) still PARKED |
| Turn order: natives first, then EN→FR→SP→DU | Partial | Human-centric; Euro sail + Indian growth/pulse; King/REF structural — [ai_transcription.md](ai_transcription.md) |

### Indians

Topic hub (graphics, units, settlements, alarm, contact): [indians.md](indians.md).

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Partial | Map/minimap icons + placement + quiet pulse / growth / residual overlays (R0 partial: t1 empty, ~50 on t2–t6); see [ai_transcription.md](ai_transcription.md) |
| Meet menus, trade, teach skills | Partial | Real DOS `@ACTIONS` meet menu with per-unit gating (`FUN_4d56_4528` human arm, `port_plan.md` P8.8, 2026-08-28) — Trade / hostile-village / Speak With Chief / Establish Mission / Denounce / Live Among / Demand Tribute / Attack; village trade rewritten against the clean `2820` recovery 2026-08-29 (real quantities and buy/sell ordering). Deep `2820` haggle / hard-bargain sub-loops PARKED on the T4.4 live capture; VGA chrome PARKED — [ai_transcription.md](ai_transcription.md) |
| Missions / convert / incite | Partial | Adjacent Missionary → `tribe.mission` + crosses; convert UI **Done** structural (`@INDIANSCONVERT` colony name); foreign-mission heresy: the human Denounce menu action uses DOS's real weighted roll (`a594` → `ai_contact_denounce_heresy`, `@HERESY0/1`, 2026-08-28), the AI-side auto pulse is still the invented 50/50 (`ai_contact_missionary_convert`); HELLO1/2 greet **Done** thin; raid surprise/war chrome **Done** thin; incite/WARPATH gold **Done** for both arms (`FUN_4d56_417e` → `ai_contact_apply_incite`, 6th village-meet CHOICE; AI Missionary auto-incite `ai_contact_ai_incite_human` 2026-08-27, `ai_port_plan.md` T4.5; [`indian_incite_417e.md`](../original_sources_annotated/ai/indian_incite_417e.md)) |
| Alarm, raid, Indian wars | Partial | Structural contact/raids (`ai_contact_*`, `@RAID*` tribe+colony status + ambush WIN1/2 / surprise / war); colony encroachment **Done** thin; player dialog **Done** structural (`ai_popup`); `4528` **Done** 2026-08-27/28 (both arms); deep `2820` haggle still PARKED |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Done | Playable bar + fort bit7/Drydock + `20e6` combat peels Done thin; the village arm is now the full `4528` action dispatch (P8.8 human menu incl. Attack Village, AI attack via `ai_euro_land_try_adjacent_village_seize`), not the old invented warn CHOICE. Deep −0x6790/VGA/`2820` haggle PARKED — [combat.md](combat.md) |
| Capture colony | Partial | `colonies_capture` on player enter (empty/cleared foreign Euro) + AI / combat paths; Indian raid abandons — [combat.md](combat.md) |
| Stockade / fort / fortress defense % | Done | Live land combat via `157e` `local_1a`; coastal Fort/Fortress fire `units_coastal_fort_fire_pulse`. MP ship-slow + close-hit bit7 + Drydock repair Done — [combat.md](combat.md) |
| Rival war / peace / privateers | Partial | Euro bilateral war/ally/peace + Furs embargo + Privateer spawn (`ai_diplo_*`); Indian×Euro matrix + fuller `153e` **Done** structural (unpark #5); FA `3f41` / 8g prize PARKED |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Partial | Side-table bell pool (DOS +0xc); peacetime threshold elect + WoI pool→intervention (**Done** 2026-08-22); century-weighted debate pick (`4345_06d2`/`015a` **Done**); manual-aligned effects; KINGGALLEON2 **Done** 2026-08-27 (`FUN_5fef_1908`; the old "38fd CHOICE search negative" PARK was a wrong-segment search); F3 portrait grid **Done** structural; FF pool bridge load smoke **Done** |
| Pedia / F3 Congress report | Partial | F3 blits joined FF portraits (center-cropped to grid cells) + debating highlight; stats column; debate elect via popup; VGA-identical chrome PARKED |
| Sons of Liberty %, declare independence | Partial | SoL + auto-declare structural (`ai_king`); player confirm UI **Done** structural (`ai_popup`); `@INDEPENDENCE` letter OK **Done** thin; signing cinematic **Done** 2026-08-30 (`FUN_43f7_160a` → `src/core/declaration.c`: DECOIND.PIK + DEC-UPP/LOW/SQIG.SS quill animation; `DECLARAT.PIK` turned out to be an unreferenced leftover) — [sons_of_liberty.md](sons_of_liberty.md) |
| REF invasion / revolution combat | Partial | REF wave / war act structural; merc hire dialog **Done** structural (`ai_popup`); win/lose latches **Done** thin; `10f0` landing scorer + caps + Veteran 0x15 **Done** Phase 5; `backup_force[2]`/`[3]` index-swap bug fixed 2026-08-24 (was feeding the wrong pool formula into both the Man-O-War/`2022` merc-gate check and the Artillery land-troop drain — `ai_king_seed_backup_force_1a26` / `ai_king_merc_offer`, verified byte-for-byte against `FUN_43f7_10f0`/`FUN_43f7_0082`/`FUN_43f7_2022`); foreign MoW ship **Done** 2026-08-28 static (`port_plan.md` P5.5: the whole force is spawned player-controlled via `281f_095c(type, DS:0x5398, …)`; the Man-O-War takes the best water 8-neighbour of the colony — no foreign unit, −999 for a REF MoW, score = 1 + its land neighbours on the colony's continent without a colony — then Cont. Cav. ≤2 / Artillery ≤2 / Cont. Army = 6 − those, `+0x15` Veteran, unloaded at the colony, 5×5 reveal); signing cinematic **Done** 2026-08-30 (see the SoL row — DECOIND.PIK, not DECLARAT.PIK) |
| F10 Colonization Score | Done | `FUN_41f2_0092` byte-faithful 2026-08-29 (`reports_compute_score`): every component + gate (gold ≥1000, villages, 0x53d0 rebel, Early Revolution from the 0x53a7/8 declare year ×2, REF-present bells/100 cap 100, Independence Achieved 100>>prior with ×(8+(8>>prior))/8 total, SCORING COMPLETE on 0x5382&0x10); conditional lines rendered in DOS order, `score.png` golden unchanged |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Done | NEW WORLD: `FUN_684c` HS-rim landfalls + Europe exit via landfall goto (`48d3_048e` / `ai_euro_unit_act`); seed-100 early fixture still gated — [ai_transcription.md](ai_transcription.md) |
| Unload, found colonies, combat, colony AI | Partial | **T2 early:** unload/found (`golden_ai_turns`); full-dispatch expand/war/scout/tools/fields thin; mid-game `5d04` ship-buy+war/peace shortage hire past colonies≥6 **Done**; Col1 colony AI/flags (incl. SoL latches) + BUY `hammers_purchased` + `depletion_counter` wrap + `warehouse_level`/`capitol_level` **Done**; land `20e6` arms structurally ported 2026-08-27 (`ai_port_plan.md` T1.18: explorer flag, SCOUT/PATROL, explore-ring scoring, 8-direction wander, epilogue commit — the old "unpark #4" is closed); thin spots left in code: LAB_52aa attack-odds core, explore-plane seen nibble, `−0x6168` rival strength, `0x4c` village arms; deep −0x6790 PARKED |

### Win / end sequences

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Quit / Exit | Done | |
| Retire → score / HoF | Done | `FUN_41f2_14a8` chain 2026-08-29: `@RETIRE` confirm → F10 → exploits screen (`FUN_41f2_0b70`: WOODPAN2, `@EXPLOITS` + `@SCORE` build-up list, `SCORE<tier+1>.SS` picture, surname-substituted name; skipped when no tier / scoring complete) → Hall of Fame → title; rating + flags persist via `HOF.TXT` |
| Revolution victory / failure | Done thin | `@WINNING` / `@LOSING1`–`3` / `@RETIRING2` + latches; `@WARN1`–`3` Done thin |
| Auto-end 1800 / 1850 | Done thin | Peacetime year≥1800 `@SCORED` → `@RETIRING` + retire score; `@SOONRETIRING0`/`1`; WoI `@WINNING` / `@RETIRING2` at 1850 |

---

## Suggested implementation order

Whole-project **phase priority** lives in [roadmap.md](roadmap.md). This list
is the historical bring-up order (early manual chapters first), then the
**unparked queue** in [ai_transcription.md](ai_transcription.md) (prereqs met):

1. **Colony economy UI** — phases 1–4 (workplaces, fields, craft, warehouse↔ship) done
2. **Europe commodity trade** — recruit/train/purchase + multi-turn sail + buy/sell done; volume-price T0 **Done thin**; boycotts **Partial** structural; pressure/bid chrome PARKED
3. **Pioneer terrain work + roads** and real movement costs — phase 7 done
4. **Unit orders** — ORDERS pulldown Done (fortify/anchor/sentry/disband/goto place+port/pioneer/pillage/dump; trade-route aim+cycle); TRADE stop nibble honor Done (Edit UI thin)
5. **Fog of war / exploration**
6. **Combat** (land first; colony defense) — **Playable bar Done**; fort bit7/Drydock + `20e6` peels Done thin; village arm is the full `4528` dispatch (P8.8); deep −0x6790 / VGA / `2820` haggle still PARKED — [combat.md](combat.md)
7. **Indian contact UI** — first contact `@INDIANWELCOME` Yes/No →
   `@INDIANPEACE`/`@INDIANCOME` or `@INDIANSHUN`+war (**Done** structural;
   `FUN_5bfb_022e` / `0182`; thin land-grant purchased+owner on occupied tile);
   later meet / trade / teach / gift (**Done** structural `ai_popup`; deep/VGA
   PARKED)
8. **King audience / declare / merc UI** (**Done** structural) + **FF effect depth** (Sepulveda convert-join + Cortes/de Witt Done; KINGGALLEON2 **Done** 2026-08-27; F3 Congress **Done** structural)
9. **Euro mid-planner** (`20e6` land arms + `5d04` **Done** 2026-08-27, T1.18 / T3.1; deep −0x6790 still open) + **Indian×Euro diplo** (**Done** structural; FA UI PARKED)
10. **Trade routes** — Create/Edit/Delete + Begin aim/cycle + stop nibble honor + Edit autofill + cargo picker + route select/delete confirms **Done**; VGA TRADE chrome PARKED
11. **Deep PARKED bodies** (full `2820`/`4528`, VGA dialog chrome, T3 goldens, letter cinematic) + HoF / end sequences — [ai_transcription.md](ai_transcription.md)

---

## Takeaway

The port is strong on **shell, map art, navigation, reports / pedia, save, basic
units / naval passengers, founding a colony, and Europe buy/sell/recruit/hire**.
**Structural** Indian contact (incl. player dialogs), Euro/Indian diplomacy,
king/REF (incl. audience/confirm/merc), FF elect (Sepulveda/Cortes/de Witt
effects **Done**), and early Euro AI (seed-100 T2 + thin expand/war) are in;
next playability work is leftover **FF** KINGGALLEON2, deep mid-planner `20e6`,
production / combat depth, and VGA / deep AI bodies — not waiting on missing
combat/capture prerequisites. TRADE Create/Edit/Begin aim+cycle + stop nibble
honor + Edit autofill + thin cargo picker are in; VGA TRADE chrome,
KINGGALLEON2 (Done 2026-08-27) and the `20e6` land arms (Done 2026-08-27,
T1.18) are closed — deep −0x6790 remains; `10f0` landing scorer / caps /
Veteran `0x15` and the foreign MoW ship spawn all landed (Phase 5 / P5.5
2026-08-28); full 1:1 AI bodies
remain.

## See also

- [roadmap.md](roadmap.md) — whole-project phases / what’s next
- [original_index.md](original_index.md) — decomp / data navigation
- [unit_orders.md](unit_orders.md) — unit order mechanics + port status
- [decomp_inventory.md](decomp_inventory.md) — bring-up and parked RE
- [ai_transcription.md](ai_transcription.md) — AI FUN_* inventory and 1:1 roadmap
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout / interop
- [save_format_map.md](save_format_map.md) — opaque field atlas + RE roadmap (P0–P6 Done)
