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
| Col1 save / load | Done | `col1_save.c`, `col1_bridge.c` |

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
| Leave colony / abandon | Partial | Leave-as popup; Stockade+ keeps ≥2; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Missing | Fence / docks art only; no combat |

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
| Tax rate / boycotts / king tax events | Missing | King phase stub; wiki checklist in [fandom_col1994.md](fandom_col1994.md) (unverified) |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Partial | Simplified stubs in `turn.c` |
| Liberty bells / crosses counters | Partial | Accumulators; FF election stub |
| Full production formulas, spoilage, boycotts | Missing | |
| Market prices driven by trade volume | Missing | Static bid/ask display only |
| Turn order: natives first, then EN→FR→SP→DU | Partial | Human-centric; Euro sail + Indian growth/pulse; King stub — [ai_transcription.md](ai_transcription.md) |

### Indians

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Partial | Map/minimap icons (`ICONS` #10–13 by tech) + placement + quiet pulse / growth; see [ai_transcription.md](ai_transcription.md) |
| Meet menus, trade, teach skills | Missing | Parked; adjacent contact bumps alarm + status line only ([ai_transcription.md](ai_transcription.md)) |
| Missions / convert / incite | Missing | |
| Alarm, raid, Indian wars | Partial | Runtime alarm friction bumps on adjacent contact; `@RAID*` / wars still deferred |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Partial | Land T0: move into foreign unit → attack/defense (+ fortified ×2); loser despawned. Naval / colony capture still Missing |
| Capture colony | Missing | |
| Stockade / fort / fortress defense % | Missing | Wiki +100%/+150%/+200% in [fandom_col1994.md](fandom_col1994.md) / [building_production.md](building_production.md); unverified in combat |
| Rival war / peace / privateers | Missing | |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Missing | Stub in turn pipeline; per-Father wiki effects in [fandom_col1994.md](fandom_col1994.md) (unverified) |
| Pedia / F3 Congress report | Partial | Data / articles; no election |
| Sons of Liberty %, declare independence | Missing | Wiki SoL / independence outline in [fandom_col1994.md](fandom_col1994.md) (unverified) |
| REF invasion / revolution combat | Missing | Wiki REF checklist in [fandom_col1994.md](fandom_col1994.md) (unverified) |
| F10 Colonization Score | Partial | Schedule exists; win loop incomplete |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Partial | `ai.c`; T2 early path on VR_SEED=100 — [ai_transcription.md](ai_transcription.md) |
| Unload, found colonies, combat, colony AI | Partial | **T2 early:** unload/found New Amsterdam/Quebec/Isabella (`smoke_ai_turns`); combat / mid-game planner still missing |

### Win / end sequences

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Quit / Exit | Done | |
| Retire → score / HoF | Partial | Score path thin; HoF stub |
| Revolution victory / failure | Missing | |
| Auto-end 1800 / 1850 | Missing | |

---

## Suggested implementation order

Aligned with early manual chapters (short playable loop first):

1. **Colony economy UI** — phases 1–4 (workplaces, fields, craft, warehouse↔ship) done
2. **Europe commodity trade** — recruit/train/purchase + multi-turn sail + buy/sell done; boycotts / volume prices later
3. **Pioneer terrain work + roads** and real movement costs — phase 7 done
4. **Unit orders** — fortify, sentry, disband (goto drag done)
5. **Fog of war / exploration**
6. **Combat** (land first; colony defense)
7. **Indian contact** — meet / trade / alarm
8. **Founding Fathers → independence / REF**
9. **Trade routes** (manual notes mouse; lower priority for core loop)
10. **Full Euro / Indian AI** + Hall of Fame / end sequences — see [ai_transcription.md](ai_transcription.md)

---

## Takeaway

The port is strong on **shell, map art, navigation, reports / pedia, save, basic
units / naval passengers, founding a colony, and Europe buy/sell/recruit/hire**.
It is still thin on combat, natives as interactive powers, founding fathers,
independence, trade routes, and full Euro/Indian AI beyond the early seed-100
T2 gate.

## See also

- [original_index.md](original_index.md) — decomp / data navigation
- [decomp_inventory.md](decomp_inventory.md) — bring-up and parked RE
- [ai_transcription.md](ai_transcription.md) — AI FUN_* inventory and 1:1 roadmap
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout
