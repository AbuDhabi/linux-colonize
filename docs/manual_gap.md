# Manual vs Linux Port Gap Analysis

Digest of [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) (MicroProse
Instruction Manual + Technical Supplement, ~135 pages) compared to the current
Linux bring-up. Deep implementation notes live in [decomp_inventory.md](decomp_inventory.md)
and [assets.md](assets.md); this file is the **feature checklist**.

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
| Hall of Fame | Partial | Stub / not entered on retire |
| Map compositor (terrain, coasts, rivers, forest/hill, resources) | Done | MAPEDIT-faithful; see inventory / assets |
| Fog of war / unexplored blackness | Partial | `map.seen` plane; black unseen; PHYS0 **104–107** edge fringe; units/tribes/colonies hidden in fog; Go-To reveals; cheat Reveal. Scenario `.MP` starts fully seen |
| Zoom / hidden terrain VIEW modes | Missing | Menu stubs |
| Roads on map | Missing | Not drawn |
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
| Terrain move costs (forest >1 MP, roads, rivers) | Done | Phase 7 costs; full-MP enter; partial overspend via DOS `range(1,cost)` (charges MP even on fail) |
| Fortify (F), Sentry (S), Disband, Goto (G) | Partial | **Go-To** via map drag; **Fortify** (F / ORDERS; overnight → Fortified); **Sentry** (S when unit selected; menu Save still via menu / S with no unit); **Disband** (Shift+D / ORDERS). Activate wakes sentry/fortified |
| Orders box letters on units | Done | `unit_chrome.c` (FUN_112b_01ba): black silhouette (−2px) + nation fill + order letter + stack under-rect; map, sidebar, Europe, colony Units/transport, Colonizopedia. England fill palette 112. F6/F7 icon rows deferred |
| Pioneer clear / plow / road (P / R) | Done | Phase 7: context P/R when Pioneer selected; tools |
| Board / unload passengers | Done | **O** / **U**; hold icons |
| Colony auto-disembark when ship enters settlement | Done | Dock + `units_disembark_all` |
| Landfall confirm + activate-all ashore | Partial | Simplified: unload ready cargo; full dialog deferred |
| Stack picker for partial unload | Done | `unit_stack.c` (wake sentry → select) |
| Trade routes (TRADE menu) | Missing | Menu stubbed |

### Colonies

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Found colony (**B**) | Done | Disband → Town Hall + starters + stock dump |
| Join colony | Partial | Opens colony screen; not true join-into-population |
| Colony display chrome | Partial | Area 1.5× (24px) tiles; people/transport (+30px) bands; multifunction; Note 1 resource-count strips; sprite-bound building hits (`colony_screen.c`) |
| Assign jobs / field work / production numbers | Partial | Drag or select-then-click colonists to buildings/area/fence; workplace strips show **output-type badge**; Production tab via `colony_preview.c` — see [building_production.md](building_production.md) |
| Construction queue + buy with gold | Partial | Construction tab BUY/CHANGE; Change list uses min-pop / upgrade / FF gates; hammers = accumulated progress; `NAMES` tools×10; no settlement banner |
| Warehouse drag load/unload to ships / wagons | Done | Drag cargo↔hold (icon cursor); **L**/**U**/**=**/**+**; empty holds use `ICONS` **#122** |
| SoL / Tory display | Partial | Col1 rebel_dividend/divisor when save-bridged; else stub 0%/100%; Tory right-aligned; people row includes fence units |
| Leave colony / abandon | Partial | Leave-as popup; Stockade+ keeps ≥3; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Done | Land +100/150/200%; coastal Fort/Fortress fire EOT (`units_coastal_fort_fire_pulse`). Fence/docks art separate |

### Europe

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Open Europe (**E**), sail **H** / **S** | Done | Multi-turn Expected Soon / Bound For *region*; passengers + holds persist |
| Docks immigrants from crosses | Partial | Crosses pull from 3-slot recruit pool; Brewster filter later |
| Market bid / ask display | Done | Bottom strip from `NAMES.TXT` `@CARGO` |
| Buy / sell goods (drag / L / = / + / U) | Done | Drag market↔hold; **L**/**=**/**+** buy, **U**/**-**/**_** sell |
| Buy ships / artillery | Done | **P** purchase menu (screenshot gold: Artillery 500 … Frigate 5000) |
| Hire Royal University / Train | Done | **T** / `@JOB` hire costs; expert → docks |
| Recruit pool (3) + passage | Done | **R** dialog; passage starts 100, +16 per recruit (Unverified formula) |
| Dock sentry / board on sail | Partial | Default sentry; Don’t/Board/Move-front menu; full equip/bless later |
| Equip muskets / horses / tools; bless missionary | Partial | Tools/muskets/horses on units; map/fence icons; colony admit dumps gear; eject popup spends stock |
| Tax rate / boycotts / king tax events | Partial | Structural tax→REF + refuse/boycott flag (`ai_king`); audience UI **Done** structural (`ai_popup`); VGA chrome PARKED — [ai_transcription.md](ai_transcription.md) |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Partial | Simplified stubs in `turn.c` |
| Liberty bells / crosses counters | Partial | Accumulators; FF election via `founding_fathers_tick` |
| Full production formulas, spoilage, boycotts | Partial | SoL field/craft/hammers/bells; warehouse spoilage clamp EOT (`colonies_apply_warehouse_spoilage` / FUN_15eb_0a50); boycotts structural diplo |
| Market prices driven by trade volume | Missing | Static bid/ask display only |
| Turn order: natives first, then EN→FR→SP→DU | Partial | Human-centric; Euro sail + Indian growth/pulse; King/REF structural — [ai_transcription.md](ai_transcription.md) |

### Indians

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Partial | Map/minimap icons + placement + quiet pulse / growth / residual overlays (R0 partial: t1 empty, ~50 on t2–t6); see [ai_transcription.md](ai_transcription.md) |
| Meet menus, trade, teach skills | Partial | Structural auto-meet/trade/teach (`ai_contact_*`); player dialogs **Done** structural (`ai_popup`); deep `2820` / VGA PARKED — [ai_transcription.md](ai_transcription.md) |
| Missions / convert / incite | Partial | Adjacent Missionary → `tribe.mission` + crosses; convert UI **Done** structural with unpark #1; incite still thin |
| Alarm, raid, Indian wars | Partial | Structural contact/raids (`ai_contact_*`, `@RAID*` kinds); player dialog **Done** structural (`ai_popup`); deep `2820`/`4528` bodies still PARKED |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Partial | Land T0: move into foreign unit → attack/defense (+ fortified ×2); loser despawned. Naval T0 via `units_resolve_naval_combat`; AI hunt thin |
| Capture colony | Partial | T0 `colonies_capture` (Euro owner swap; Indian raid abandons) — AI / combat paths |
| Stockade / fort / fortress defense % | Done | Land combat: +100%/+150%/+200% via `colonies_fortification_defense_bonus_percent`; coastal Fort/Fortress naval fire `units_coastal_fort_fire_pulse` (FUN_364b_03f6). PARK: ship-slow formula |
| Rival war / peace / privateers | Partial | Euro bilateral war/ally/peace + Furs embargo + Privateer spawn (`ai_diplo_*`); Indian×Euro matrix + fuller `153e` **Done** structural (unpark #5); FA `3f41` / 8g prize PARKED |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Partial | Rough threshold elect + manual-aligned effects (`founding_fathers`); Minuit+Franklin+Brebeuf+Las Casas+Cortes coastal cash+de Witt **Done**; Sepulveda join% / KINGGALLEON2 PARK; Congress UI PARKED |
| Pedia / F3 Congress report | Partial | Data / articles; no election |
| Sons of Liberty %, declare independence | Partial | SoL + auto-declare structural (`ai_king`); player confirm UI **Done** structural (`ai_popup`); VGA PARKED |
| REF invasion / revolution combat | Partial | REF wave / war act structural; merc hire dialog **Done** structural (`ai_popup`); deep `10f0` / arrival / letter chrome PARKED |
| F10 Colonization Score | Partial | Schedule exists; win loop incomplete |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Done | NEW WORLD: `FUN_684c` HS-rim landfalls + Europe exit via landfall goto (`48d3_048e` / `ai_euro_unit_act`); seed-100 early fixture still gated — [ai_transcription.md](ai_transcription.md) |
| Unload, found colonies, combat, colony AI | Partial | **T2 early:** unload/found (`smoke_ai_turns`); full-dispatch expand/war/scout/tools/fields thin; deep land `20e6` **OPEN** (unpark #4) |

### Win / end sequences

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Quit / Exit | Done | |
| Retire → score / HoF | Partial | Score path thin; HoF stub |
| Revolution victory / failure | Missing | |
| Auto-end 1800 / 1850 | Missing | |

---

## Suggested implementation order

Aligned with early manual chapters (short playable loop first), then the
**unparked queue** in [ai_transcription.md](ai_transcription.md) (prereqs met):

1. **Colony economy UI** — phases 1–4 (workplaces, fields, craft, warehouse↔ship) done
2. **Europe commodity trade** — recruit/train/purchase + multi-turn sail + buy/sell done; boycotts / volume prices later
3. **Pioneer terrain work + roads** and real movement costs — phase 7 done
4. **Unit orders** — fortify, sentry, disband (goto drag done)
5. **Fog of war / exploration**
6. **Combat** (land first; colony defense) — T0 land/naval/capture + fort % + coastal fort fire in; ship-slow / deep `20e6` still PARKED
7. **Indian contact UI** — first contact `@INDIANWELCOME` Yes/No →
   `@INDIANPEACE`/`@INDIANCOME` or `@INDIANSHUN`+war (**Done** structural;
   `FUN_5bfb_022e` / `0182`); later meet / trade / teach / gift (**Done**
   structural `ai_popup`; deep/VGA PARKED)
8. **King audience / declare / merc UI** (**Done** structural) + **FF effect depth** (Sepulveda join% / KINGGALLEON2 PARK; Cortes/de Witt Done; Congress UI later)
9. **Euro mid-planner** (deep `20e6` **OPEN** unpark #4) + **Indian×Euro diplo** (**Done** structural; FA UI PARKED)
10. **Trade routes** (manual notes mouse; lower priority for core loop)
11. **Deep PARKED bodies** (full `2820`/`4528`, VGA dialog chrome, T3 goldens, letter cinematic) + HoF / end sequences — [ai_transcription.md](ai_transcription.md)

---

## Takeaway

The port is strong on **shell, map art, navigation, reports / pedia, save, basic
units / naval passengers, founding a colony, and Europe buy/sell/recruit/hire**.
**Structural** Indian contact (incl. player dialogs), Euro/Indian diplomacy,
king/REF (incl. audience/confirm/merc), FF elect, and early Euro AI (seed-100 T2
+ thin expand/war) are in; next playability work is leftover **FF hooks**
(Sepulveda/Cortes), deep mid-planner `20e6`, and VGA / deep AI bodies — not
waiting on missing combat/capture prerequisites. Still thin on fort defense %,
trade routes, Congress UI, and full 1:1 AI bodies.

## See also

- [original_index.md](original_index.md) — decomp / data navigation
- [decomp_inventory.md](decomp_inventory.md) — bring-up and parked RE
- [ai_transcription.md](ai_transcription.md) — AI FUN_* inventory and 1:1 roadmap
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout / interop
- [save_format_map.md](save_format_map.md) — opaque field atlas + RE roadmap (P0–P4)
