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
| Hall of Fame | Partial | Title shows last retired score; persists to `HOF.TXT` (thin table stub) |
| Map compositor (terrain, coasts, rivers, forest/hill, resources) | Done | MAPEDIT-faithful; see inventory / assets |
| Fog of war / unexplored blackness | Partial | `map.seen` plane; black unseen; PHYS0 **104–107** edge fringe; units/tribes/colonies hidden in fog; Go-To reveals; cheat Reveal. Scenario `.MP` starts fully seen |
| Zoom / hidden terrain VIEW modes | Missing | Menu stubs |
| Roads on map | Done | PHYS0 **80** isolated / **81–88** multi-blit stubs via `map_phys0_road_layer_*` (`FUN_6ba1_0938`) |
| Plowed fields on map | Done | PHYS0 **149** via `map_phys0_plow_sprite_at` (main map + colony area) |
| Menu bar, right panel, minimap | Done | `map_menu.c`, `map_panel.c` |
| Colonizopedia | Done | `pedia.c` |
| Reports F1–F10 | Done | `reports.c` |
| Pick Music + BGM | Done | `sound.c`, `pick_music.c` |
| Digital SFX (`COLDIG.BIN`) | Missing | Deferred |
| Col1 save / load | Done | Playable I/O: `col1_save.c`, `col1_bridge.c`. **Not** a complete field map — see [save_format_map.md](save_format_map.md) |

### Units and map orders

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Move / wait / skip turn | Done | Arrows, Wait, Space |
| Terrain move costs (forest >1 MP, roads, rivers) | Partial | DOS `terr_cost` table + full-MP / partial overspend; road-pair + cardinal-river pair via `map_move_cost_step` — [move_enter.md](move_enter.md). PARK: unit MP `*3` scale (Brave already `table*3`) |
| Fortify (F), Sentry (S), Disband, Goto (G) | Done | One **Fortify** (land or ship-in-harbor); **Go-To** drag / **G** Place (land) or Port (ship); **Sentry** / **Disband** (Shift+D with Yes/No). ORDERS items enable/hide from selected unit (Clear↔Plow, Port↔Place). Plain letter hotkeys match menu `~` markers; Alt+letter opens bar menus |
| Orders box letters on units | Done | `unit_chrome.c` (FUN_112b_01ba): black silhouette (−2px) + nation fill + order letter + stack under-rect; map, sidebar, Europe, colony Units/transport, Colonizopedia. England fill palette 112. F6/F7 icon rows deferred |
| Pioneer clear / plow / road (P / R) | Done | Multi-turn FUN_479b: clear/plow = `terr_cost+2`, road = `terr_cost`, Hardy halves; −20 tools on complete; clear and plow are separate jobs |
| Board / unload passengers | Done | **O** / **U**; hold icons; land→ocean with room auto-boards (`BOARD`) |
| Dump cargo overboard | Done | ORDERS Dump Cargo Overboard → first goods hold |
| Pillage | Partial | ORDERS: military loots foreign Euro colony stock or clears plow/road; thin vs full `2b5a` body |
| Colony auto-disembark when ship enters settlement | Done | Dock + `units_disembark_all` |
| Sentry auto-board when ship leaves tile | Done | Same-tile Sentry land → departing ship to capacity (colony + ocean stack) |
| Landfall confirm + activate-all ashore | Done | Ship→bare land: `@LANDFALL` Stay With Ships / Make Landfall (`AI_POPUP_TAG_LANDFALL`); Make Landfall unloads all; ship stays at sea — [move_enter.md](move_enter.md) |
| Ship→native village | Done | `@DONTKNOWSHIPS` / `@MADATSHIPS` (not landfall); [move_enter.md](move_enter.md) |
| Stack picker for partial unload | Done | `unit_stack.c` (wake sentry → select) |
| Trade routes (TRADE menu) | Partial | Create/Edit/Delete; Begin aims+cycles stops; stop service honors Col1 load/unload nibbles when counts>0 (else unload-all / surplus ladder); Edit autofill + thin cargo picker (unload→load multi-select); Europe sell on 999; VGA TRADE chrome PARKED |

### Colonies

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Found colony (**B**) | Done | Disband → Town Hall + starters + stock dump; name-entry `@COLONY` |
| Join colony | Done | ORDERS Join Colony admits selected land unit on owned colony tile; else opens colony screen |
| Colony display chrome | Partial | Area 1.5× (24px) tiles; people/transport (+30px) bands; multifunction; Note 1 resource-count strips; sprite-bound building hits (`colony_screen.c`) |
| Assign jobs / field work / production numbers | Partial | Drag or select-then-click colonists to buildings/area/fence; workplace strips show **output-type badge**; Production tab via `colony_preview.c` — see [building_production.md](building_production.md) |
| Construction queue + buy with gold | Partial | Construction tab BUY/CHANGE; Change list uses min-pop / upgrade / FF gates; hammers = accumulated progress; `NAMES` tools×10; settlement banner (name + hammers; click → Change) **Done** thin |
| Warehouse drag load/unload to ships / wagons | Done | Drag cargo↔hold (icon cursor); **L**/**U**/**=**/**+**; empty holds use `ICONS` **#122** |
| SoL / Tory display | Partial | Col1 rebel_dividend/divisor when present; else nation liberty_bells/4 stand-in (`colony_prod_sol_percent`); Tory right-aligned; people row includes fence units — [sons_of_liberty.md](sons_of_liberty.md) |
| Leave colony / abandon | Partial | Leave-as popup; Stockade+ keeps ≥3; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Done | Land +100/150/200%; coastal Fort/Fortress fire EOT (`units_coastal_fort_fire_pulse`). Fence/docks art separate |

### Europe

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Open Europe (**E**), sail **H** / **S** | Done | Multi-turn Expected Soon / Bound For *region*; passengers + holds persist |
| Docks immigrants from crosses | Partial | Crosses pull from 3-slot recruit pool; Brewster filter Done; pick-among-pool UI PARKED |
| Market bid / ask display | Done | Bottom strip from `NAMES.TXT` `@CARGO` |
| Buy / sell goods (drag / L / = / + / U) | Done | Drag market↔hold; **L**/**=**/**+** buy, **U**/**-**/**_** sell |
| Buy ships / artillery | Done | **P** purchase menu (screenshot gold: Artillery 500 … Frigate 5000) |
| Hire Royal University / Train | Done | **T** / `@JOB` hire costs; expert → docks |
| Recruit pool (3) + passage | Done | **R** dialog; passage starts 100, +16 per recruit (Unverified formula) |
| Dock sentry / board on sail | Partial | Default sentry; Don’t/Board/Move-front menu; full equip/bless later |
| Equip muskets / horses / tools; bless missionary | Partial | Tools/muskets/horses on units; Leave-as Missionary with Church/Cathedral **Done**; fence icons; colony admit dumps gear |
| Tax rate / boycotts / king tax events | Partial | Structural tax→REF + refuse/boycott flag (`ai_king`); audience UI **Done** structural (`ai_popup`); VGA chrome PARKED — [ai_transcription.md](ai_transcription.md) |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Partial | Simplified stubs in `turn.c` |
| Liberty bells / crosses counters | Partial | Accumulators; FF election via `founding_fathers_tick` |
| Full production formulas, spoilage, boycotts | Partial | SoL field/craft/hammers/bells; warehouse spoilage clamp EOT (`colonies_apply_warehouse_spoilage` / FUN_15eb_0a50); boycotts structural diplo |
| Market prices driven by trade volume | Partial | T0: buy/sell update `trade_nr` + FUN_38fd_0058 rise/fall ±1 bid (`europe_apply_volume_price`); EOT attrition + colony→`price_group_state` half **Done** thin; pressure/bid chrome PARKED |
| Turn order: natives first, then EN→FR→SP→DU | Partial | Human-centric; Euro sail + Indian growth/pulse; King/REF structural — [ai_transcription.md](ai_transcription.md) |

### Indians

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Partial | Map/minimap icons + placement + quiet pulse / growth / residual overlays (R0 partial: t1 empty, ~50 on t2–t6); see [ai_transcription.md](ai_transcription.md) |
| Meet menus, trade, teach skills | Partial | Structural auto-meet/trade/teach (`ai_contact_*`); sea/wagon hold trade + gift/demand widgets **Done** thin; deep `2820` / VGA PARKED — [ai_transcription.md](ai_transcription.md) |
| Missions / convert / incite | Partial | Adjacent Missionary → `tribe.mission` + crosses; convert UI **Done** structural (`@INDIANSCONVERT` colony name); foreign-mission heresy 50/50 **Done** thin; HELLO1/2 greet **Done** thin; raid surprise/war chrome **Done** thin; incite/WARPATH gold still PARKED |
| Alarm, raid, Indian wars | Partial | Structural contact/raids (`ai_contact_*`, `@RAID*` tribe+colony status + ambush WIN1/2 / surprise / war); colony encroachment **Done** thin; player dialog **Done** structural (`ai_popup`); deep `2820`/`4528` still PARKED |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Partial | Land: combat role (`attack>0`) fights; non-combat / peace bounce. Naval: human/AI move-into ship combat T0. Deep `5fef` PARKED — [move_enter.md](move_enter.md) |
| Capture colony | Partial | `colonies_capture` on player enter (empty/cleared foreign Euro) + AI / combat paths; Indian raid abandons |
| Stockade / fort / fortress defense % | Done | Land combat: +100%/+150%/+200% via `colonies_fortification_defense_bonus_percent`; coastal Fort/Fortress naval fire `units_coastal_fort_fire_pulse` (FUN_364b_03f6). PARK: ship-slow formula |
| Rival war / peace / privateers | Partial | Euro bilateral war/ally/peace + Furs embargo + Privateer spawn (`ai_diplo_*`); Indian×Euro matrix + fuller `153e` **Done** structural (unpark #5); FA `3f41` / 8g prize PARKED |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Partial | Rough threshold elect + manual-aligned effects (`founding_fathers`); Minuit+Franklin+Brebeuf+Las Casas+Cortes coastal cash+de Witt+Sepulveda convert-join **Done**; human debate CHOICE one-per-@FATHERS-type **Done** structural; KINGGALLEON2 PARK; F3 portrait grid PARKED |
| Pedia / F3 Congress report | Partial | Data / articles; debate elect via popup; no election on F3 plate |
| Sons of Liberty %, declare independence | Partial | SoL + auto-declare structural (`ai_king`); player confirm UI **Done** structural (`ai_popup`); `@INDEPENDENCE` letter OK **Done** thin; VGA / DECLARAT.PIK anim PARKED — [sons_of_liberty.md](sons_of_liberty.md) |
| REF invasion / revolution combat | Partial | REF wave / war act structural; merc hire dialog **Done** structural (`ai_popup`); win (1850+no crown) / lose (no ports after REF) latches **Done** thin; deep `10f0` / DECLARAT anim PARKED |
| F10 Colonization Score | Partial | Schedule exists; WoI → declared flag; achieve from endgame latch; retire opens score |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Done | NEW WORLD: `FUN_684c` HS-rim landfalls + Europe exit via landfall goto (`48d3_048e` / `ai_euro_unit_act`); seed-100 early fixture still gated — [ai_transcription.md](ai_transcription.md) |
| Unload, found colonies, combat, colony AI | Partial | **T2 early:** unload/found (`smoke_ai_turns`); full-dispatch expand/war/scout/tools/fields thin; mid-game `5d04` ship-buy+war/peace shortage hire past colonies≥6 **Done**; Col1 colony AI/flags (incl. SoL latches) + BUY `hammers_purchased` + `depletion_counter` wrap + `warehouse_level`/`capitol_level` **Done**; deep land `20e6` **OPEN** (unpark #4) |

### Win / end sequences

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Quit / Exit | Done | |
| Retire → score / HoF | Partial | `@RETIRE` confirm → F10 score then title; last score persists via `HOF.TXT` |
| Revolution victory / failure | Partial | Thin: lose all coastal ports (after REF present); win year≥1850 + no crown units (`unknown46[4]`) |
| Auto-end 1800 / 1850 | Partial | Peacetime year≥1800 latch + popup; WoI win at 1850+no crown; forced retire UI still thin |

---

## Suggested implementation order

Aligned with early manual chapters (short playable loop first), then the
**unparked queue** in [ai_transcription.md](ai_transcription.md) (prereqs met):

1. **Colony economy UI** — phases 1–4 (workplaces, fields, craft, warehouse↔ship) done
2. **Europe commodity trade** — recruit/train/purchase + multi-turn sail + buy/sell done; boycotts / volume prices later
3. **Pioneer terrain work + roads** and real movement costs — phase 7 done
4. **Unit orders** — ORDERS pulldown Done (fortify/anchor/sentry/disband/goto place+port/pioneer/pillage/dump; trade-route aim+cycle); TRADE stop nibble honor Done (Edit UI thin)
5. **Fog of war / exploration**
6. **Combat** (land first; colony defense) — T0 land/naval/capture + fort % + coastal fort fire in; ship-slow / deep `20e6` still PARKED
7. **Indian contact UI** — first contact `@INDIANWELCOME` Yes/No →
   `@INDIANPEACE`/`@INDIANCOME` or `@INDIANSHUN`+war (**Done** structural;
   `FUN_5bfb_022e` / `0182`; thin land-grant purchased+owner on occupied tile);
   later meet / trade / teach / gift (**Done** structural `ai_popup`; deep/VGA
   PARKED)
8. **King audience / declare / merc UI** (**Done** structural) + **FF effect depth** (Sepulveda convert-join + Cortes/de Witt Done; KINGGALLEON2 PARK; Congress UI later)
9. **Euro mid-planner** (deep `20e6` **OPEN** unpark #4) + **Indian×Euro diplo** (**Done** structural; FA UI PARKED)
10. **Trade routes** — Create/Edit/Delete + Begin aim/cycle + stop nibble honor + Edit autofill + cargo picker + route select/delete confirms **Done**; VGA TRADE chrome PARKED
11. **Deep PARKED bodies** (full `2820`/`4528`, VGA dialog chrome, T3 goldens, letter cinematic) + HoF / end sequences — [ai_transcription.md](ai_transcription.md)

---

## Takeaway

The port is strong on **shell, map art, navigation, reports / pedia, save, basic
units / naval passengers, founding a colony, and Europe buy/sell/recruit/hire**.
**Structural** Indian contact (incl. player dialogs), Euro/Indian diplomacy,
king/REF (incl. audience/confirm/merc), FF elect, and early Euro AI (seed-100 T2
+ thin expand/war) are in; next playability work is leftover **FF hooks**
(Sepulveda/Cortes), deep mid-planner `20e6`, and VGA / deep AI bodies — not
waiting on missing combat/capture prerequisites. TRADE Create/Edit/Begin
aim+cycle + stop nibble honor + Edit autofill + thin cargo picker are in; VGA TRADE
chrome, Congress UI, and full 1:1 AI bodies remain.

## See also

- [original_index.md](original_index.md) — decomp / data navigation
- [decomp_inventory.md](decomp_inventory.md) — bring-up and parked RE
- [ai_transcription.md](ai_transcription.md) — AI FUN_* inventory and 1:1 roadmap
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout / interop
- [save_format_map.md](save_format_map.md) — opaque field atlas + RE roadmap (P0–P4)
