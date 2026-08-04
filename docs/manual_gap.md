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
| Fog of war / unexplored blackness | Missing | Intentional MAPEDIT gap; game still needs it |
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
| Terrain move costs (forest >1 MP, roads, rivers) | Done | Phase 7: pedia stub costs; road/river halves |
| Fortify (F), Sentry (S), Disband, Goto (G) | Missing | Sentry used only for passengers aboard |
| Orders box letters on units | Missing | |
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
| Assign jobs / field work / production numbers | Partial | Workplace strips show **output-type badge** (no per-worker count); Production tab via `colony_preview.c` — see [building_production.md](building_production.md) |
| Construction queue + buy with gold | Partial | Construction tab BUY/CHANGE; Change list uses min-pop / upgrade / FF gates; hammers = accumulated progress; `NAMES` tools×10; no settlement banner |
| Warehouse drag load/unload to ships / wagons | Partial | Click cargo↔hold + **L**/**U**/**=**/**+**; empty holds use `ICONS` **#122**; full drag later |
| SoL / Tory display | Partial | Col1 rebel_dividend/divisor when save-bridged; else stub 0%/100%; Tory right-aligned; people row includes fence units |
| Leave colony / abandon | Partial | Leave-as popup; Stockade+ keeps ≥2; last colonist confirms abandon (cargo lost) |
| Fortification defense bonuses | Missing | Fence / docks art only; no combat |

### Europe

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Open Europe (**E**), sail **H** / **S** | Done | Passengers persist in harbor |
| Docks immigrants from crosses | Partial | Crosses → dock; recruit |
| Market bid / ask display | Done | From `NAMES.TXT` `@CARGO` |
| Buy / sell goods (drag / L / = / + / U) | Partial | Phase 5: click hold/market + **L**/**U**/**=**/**+**; tax on sales; goods persist H/S; drag later |
| Buy ships / artillery | Missing | |
| Hire Royal University / Train | Missing | Train stub |
| Equip muskets / horses / tools; bless missionary | Partial | Tools/muskets/horses on units; map/fence icons; colony admit dumps gear; eject popup spends stock |
| Tax rate / boycotts / king tax events | Missing | King phase stub |

### Economy and turn sequence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Calendar (`@TIMECHANGE`) | Done | Year then Spring/Autumn from 1600 |
| Food / production / hammers | Partial | Simplified stubs in `turn.c` |
| Liberty bells / crosses counters | Partial | Accumulators; FF election stub |
| Full production formulas, spoilage, boycotts | Missing | |
| Market prices driven by trade volume | Missing | Static bid/ask display only |
| Turn order: natives first, then EN→FR→SP→DU | Partial | Human-centric; AI / Indian / King stubs |

### Indians

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Villages on map + Braves | Partial | Map/minimap icons (`ICONS` #10–13 by tech) + placement + light wander / growth |
| Meet menus, trade, teach skills | Missing | Parked |
| Missions / convert / incite | Missing | |
| Alarm, raid, Indian wars | Missing | `@RAID*` deferred |

### Combat and diplomacy

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Land / naval attack | Missing | Foreign occupant only blocks enter |
| Capture colony | Missing | |
| Stockade / fort / fortress defense % | Missing | |
| Rival war / peace / privateers | Missing | |

### Founding Fathers and independence

| Manual feature | Status | Notes |
|----------------|--------|-------|
| FF election from liberty bells | Missing | Stub in turn pipeline |
| Pedia / F3 Congress report | Partial | Data / articles; no election |
| Sons of Liberty %, declare independence | Missing | |
| REF invasion / revolution combat | Missing | |
| F10 Colonization Score | Partial | Schedule exists; win loop incomplete |

### AI Europeans

| Manual feature | Status | Notes |
|----------------|--------|-------|
| Rival starter fleets + sail to landfall | Partial | `ai.c` Phase 1 |
| Unload, found colonies, combat, colony AI | Missing | Parked Phase 2+ |

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
2. **Europe commodity trade** — phase 5 buy/sell + tax on sales done; boycotts / volume prices later
3. **Pioneer terrain work + roads** and real movement costs — phase 7 done
4. **Unit orders** — fortify, sentry, goto, disband
5. **Fog of war / exploration**
6. **Combat** (land first; colony defense)
7. **Indian contact** — meet / trade / alarm
8. **Founding Fathers → independence / REF**
9. **Trade routes** (manual notes mouse; lower priority for core loop)
10. **Full Euro / Indian AI** + Hall of Fame / end sequences

---

## Takeaway

The port is strong on **shell, map art, navigation, reports / pedia, save, basic
units / naval passengers, and founding a colony**. It is still thin on almost
everything the manual treats as the **game**: colony labor / production, Europe
trade, combat, natives as interactive powers, founding fathers, and independence.

## See also

- [original_index.md](original_index.md) — decomp / data navigation
- [decomp_inventory.md](decomp_inventory.md) — bring-up and parked RE
- [assets.md](assets.md) — formats and UI wiring
- [savegame.md](savegame.md) — `COLONY##.SAV` layout
