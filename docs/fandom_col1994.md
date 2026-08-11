# Fandom wiki digest — Sid Meier's Colonization (1994)

Community digest of [civilization.fandom.com](https://civilization.fandom.com/wiki/Sid_Meier%27s_Colonization) pages tagged for the **original 1994 game** (`(Col)` / “in Colonization (1994)”). Use this for checklists and effect prose while porting — **not** as a DOS oracle.

**Crawl date:** 2026-08-04 (MediaWiki `api.php` wikitext).

---

## Authority (read first)

| Rank | Source | Role |
|------|--------|------|
| **1** | **Original code** — `VICEROY.EXE` / `MAPEDIT.EXE`, [`original_sources_decompiled/`](../original_sources_decompiled/), save-diff / runtime observation. Shipped catalogs the code loads (`NAMES.TXT`, etc.) are part of this artifact set; **code wins** if catalog text and observed behavior disagree. | Binding for implementation |
| **2** | **Original manual** — [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Qualitative rules, charts the catalogs omit |
| **3** | **Other sources** — this digest, StrategyWiki, NamuWiki, tips pages | Unverified until reconciled with (1) or (2) |

Every table below is **Unverified vs DOS** unless a “Port / original” note says otherwise. When wiki conflicts with (1) or (2), see [Conflicts](#conflicts--open-questions) — do not silently “fix” [`building_production.md`](building_production.md) or [`terrain_yields.md`](terrain_yields.md).

**Exclude:** Civ4Col, FreeCol, and generic Civ pages (except one-line contrast). FreeCol notes on wiki pages are ignored for this port.

Gap map: [`manual_gap.md`](manual_gap.md). Navigation: [`original_index.md`](original_index.md).

---

## Founding Fathers

**Election (wiki):** Liberty Bells attract Fathers. After the first colony produces bells, the player picks from a random set of one not-yet-had Father **per category** (five choices). Non-exclusive (every nation can recruit the same Father). Viewed on the Continental Congress screen.

**Port:** Choose-then-accumulate wired (`next_founding_father` / Congress debate CHOICE → lock candidate → elect at `FUN_4345_0982` threshold; after elect `next=-1`). Century-weighted random pick still a stand-in (first unclaimed per type). DOS zero-bells-on-elect still PARKED (Linux keeps cumulative).

### Trade

| Father | Wiki effect (paraphrase) | Source |
|--------|--------------------------|--------|
| Adam Smith | Unlock factory-tier buildings; factories produce **1.5** manufactured goods per unit of raw material. Factories need colony pop **8**. | [Adam Smith (Col)](https://civilization.fandom.com/wiki/Adam_Smith_(Col)) |
| Jakob Fugger | All current Europe boycotts forgiven; resume trade at no cost (no back taxes). | [Jakob Fugger (Col)](https://civilization.fandom.com/wiki/Jakob_Fugger_(Col)) |
| Jan de Witt | Trade with foreign colonies allowed; Foreign Affairs report more revealing. | [Jan de Witt (Col)](https://civilization.fandom.com/wiki/Jan_de_Witt_(Col)) |
| Peter Minuit | Indians no longer demand payment for land. | [Peter Minuit (Col)](https://civilization.fandom.com/wiki/Peter_Minuit_(Col)) |
| Peter Stuyvesant | Unlock **Custom House** (auto Europe trade; continues in Revolution). | [Peter Stuyvesant (Col)](https://civilization.fandom.com/wiki/Peter_Stuyvesant_(Col)) |

Adam Smith factory list (wiki): Iron Works, Cigar Factory, Textile Mill, Rum Factory, Fur Factory, Arsenal. Port already gates factories / Custom House in construction Change list — see [`building_production.md`](building_production.md).

### Exploration

| Father | Wiki effect (paraphrase) | Source |
|--------|--------------------------|--------|
| Ferdinand Magellan | All naval vessels +1 movement; west-map-edge → Europe sail time shortened considerably. | [Ferdinand Magellan (Col)](https://civilization.fandom.com/wiki/Ferdinand_Magellan_(Col)) |
| Francisco de Coronado | All existing colonies and surrounding area become visible on the map. | [Francisco de Coronado (Col)](https://civilization.fandom.com/wiki/Francisco_de_Coronado_(Col)) |
| Henry Hudson | Fur trapper output +100%. | [Henry Hudson (Col)](https://civilization.fandom.com/wiki/Henry_Hudson_(Col)) |
| Hernando de Soto | Lost City Rumors always positive; all units extended sight radius. | [Hernando de Soto (Col)](https://civilization.fandom.com/wiki/Hernando_de_Soto_(Col)) |
| Sieur de La Salle | Existing and future colonies get a **Stockade** when population reaches **3**. | [Sieur de La Salle (Col)](https://civilization.fandom.com/wiki/Sieur_de_La_Salle_(Col)) |

### Military

| Father | Wiki effect (paraphrase) | Source |
|--------|--------------------------|--------|
| John Paul Jones | Free **Frigate** added to navy. | [John Paul Jones (Col)](https://civilization.fandom.com/wiki/John_Paul_Jones_(Col)) |
| Francis Drake | Privateer combat strength +50%. | [Francis Drake (Col)](https://civilization.fandom.com/wiki/Francis_Drake_(Col)) |
| Paul Revere | If a colony with no standing soldiers is attacked, a colonist auto-takes stockpiled muskets to defend. | [Paul Revere (Col)](https://civilization.fandom.com/wiki/Paul_Revere_(Col)) |
| George Washington | Non-veteran soldiers/dragoons who win combat always upgrade (normally chance-based). | [George Washington (Col)](https://civilization.fandom.com/wiki/George_Washington_(Col)) |
| Hernan Cortes | Conquered native settlements always yield more treasure; king’s galleons transport treasure free. | [Hernan Cortes (Col)](https://civilization.fandom.com/wiki/Hernan_Cortes_(Col)) |

### Political

| Father | Wiki effect (paraphrase) | Source |
|--------|--------------------------|--------|
| Benjamin Franklin | King’s European wars no longer affect New World relations; Europeans in the New World always offer peace in negotiations. | [Benjamin Franklin (Col)](https://civilization.fandom.com/wiki/Benjamin_Franklin_(Col)) |
| Simon Bolivar | Sons of Liberty membership in all colonies +20%. | [Simon Bolivar (Col)](https://civilization.fandom.com/wiki/Simon_Bolivar_(Col)) |
| Thomas Paine | Liberty Bell production in all colonies increased by the **current tax rate**. | [Thomas Paine (Col)](https://civilization.fandom.com/wiki/Thomas_Paine_(Col)) |
| Pocahontas | All native tension → content; Indian alarm generated half as fast. | [Pocahontas (Col)](https://civilization.fandom.com/wiki/Pocahontas_(Col)) |
| Thomas Jefferson | Liberty Bell production of statesmen +50%. | [Thomas Jefferson (Col)](https://civilization.fandom.com/wiki/Thomas_Jefferson_(Col)) |

### Religious

| Father | Wiki effect (paraphrase) | Source |
|--------|--------------------------|--------|
| Bartolome de las Casas | Existing Indian converts assimilate as free colonists. | [Bartolome de las Casas (Col)](https://civilization.fandom.com/wiki/Bartolome_de_las_Casas_(Col)) |
| Juan de Sepulveda | Higher chance subjugated Indians “convert” and join a colony. | [Juan de Sepulveda (Col)](https://civilization.fandom.com/wiki/Juan_de_Sepulveda_(Col)) |
| Father Jean de Brebeuf | All missionaries function as experts. | [Father Jean de Brebeuf (Col)](https://civilization.fandom.com/wiki/Father_Jean_de_Brebeuf_(Col)) |
| William Penn | Cross production in all colonies +50%. | [William Penn (Col)](https://civilization.fandom.com/wiki/William_Penn_(Col)) |
| William Brewster | No more criminals/servants on docks; player picks which recruit-pool immigrant moves to docks. | [William Brewster (Col)](https://civilization.fandom.com/wiki/William_Brewster_(Col)) |

Hub: [Founding Fathers (Col)](https://civilization.fandom.com/wiki/Founding_Fathers_(Col)).

---

## Buildings

Wiki upgrade chains and workplace table: [Building (Col)](https://civilization.fandom.com/wiki/Building_(Col)), [List of buildings in Col](https://civilization.fandom.com/wiki/List_of_buildings_in_Col) (list page uses wiki data templates — prefer `NAMES.TXT` `@BUILDING` for hammers/tools/min-pop).

| Chain | Tiers (wiki) | Notes |
|-------|--------------|-------|
| Hammers | Carpenter’s Shop → Lumber Mill | Mill is shop tier (not factory) |
| Tools | Blacksmith’s House → Shop → Iron Works | Iron Works = Adam Smith |
| Cloth | Weaver’s House → Shop → Textile Mill | Factory |
| Cigars | Tobacconist’s House → Shop → Cigar Factory | Factory |
| Rum | Distiller’s House → Distillery → Rum Factory | Factory |
| Coats | Fur Trader’s House → Trading Post → Fur Factory | Factory |
| Muskets | Armory → Magazine → Arsenal | Arsenal = Adam Smith |
| Defense | Stockade → Fort → Fortress | See [Colonies / defense](#colonies--defense) |
| Naval | Docks → Drydock → Shipyard | Coastal |
| Education | Schoolhouse → College → University | |
| Crosses | Church → Cathedral | Wiki also mentions Chapel as early religious site |
| Media | Printing Press → Newspaper | +50% / +100% bells (wiki strategy) |
| Storage | Warehouse → Warehouse Expansion | |
| Other | Town Hall, Stable, Custom House | Custom House needs Stuyvesant |

**Custom House** ([Custom House (Col)](https://civilization.fandom.com/wiki/Custom_House_(Col))): After Stuyvesant, auto-sells to Europe (wiki strategy: sells excess above 50 when ≥100 of a configured cargo type, before spoilage). Sells boycotted goods; during Revolution continues trade (bids frozen at declaration; sales untaxed per wiki). Port: gate Partial in construction list; auto-sell **Done** structural (`europe_custom_house_autosell` / `FUN_364b_0688` stock>99 leave 50 + `FUN_364b_0636` denylist; per-cargo UI chrome PARKED).

**Docks** ([Docks (Col)](https://civilization.fandom.com/wiki/Docks_(Col))): Unlock fishing on ocean/lake tiles in catchment; shore / river / fishery bonuses (qualitative). Port: coastal gate Partial; full fishing rules Missing.

Costs / min-pop: already catalogued in `NAMES.TXT` and summarized in [`building_production.md`](building_production.md) — do not trust wiki list templates over the catalog.

---

## Production / SoL

Full catalog: [`sons_of_liberty.md`](sons_of_liberty.md).

| Claim (wiki) | Porting note |
|--------------|--------------|
| Factory ≈ **1.5×** goods per raw (Adam Smith) | Aligns with port framing **6→9** throughput in [`building_production.md`](building_production.md); verify in decomp |
| SoL **50%** / **100%** productivity bumps; can exceed 100% internally but display caps at 100% | [Sons of Liberty (Col)](https://civilization.fandom.com/wiki/Sons_of_Liberty_(Col)); port: `colony_prod_sol_bonus`; DOS caps 100 after Bolivar |
| “Inefficient government”: too many Tory colonists (wiki: ≥10 easiest / ≥6 hardest) → **−1** on all production | Thresh `10-diff` correct; magnitude is DOS `−⌊tories/thresh⌋` + sol latches — not a flat −1 ([sons_of_liberty.md](sons_of_liberty.md)); effect PARK |
| Printing Press +50% / Newspaper +100% liberty bells from town-hall workers | Partial construction; multipliers Missing |
| Jefferson ×1.5 on statesmen; Paine adds tax-rate factor (wiki: multiplicative with press/newspaper) | FF Missing |
| Church raises passive crosses; Penn +50% crosses | Crosses → dock Partial; full formulas Missing |
| Declare independence needs **overall** SoL ≥ **50%** | Structural Partial (`ai_king`); see [sons_of_liberty.md](sons_of_liberty.md) |

Liberty bells also feed FF election, then after independence feed [foreign intervention](#independence--win). Details: [Liberty Bell (Col)](https://civilization.fandom.com/wiki/Liberty_bell_(Col)) (long strategy page).

Simon Bolivar wiki “+20% SoL”: DOS adds +20 on every SoL **read** while elected; port applies a one-shot dividend bump — [sons_of_liberty.md](sons_of_liberty.md).

Terrain / field yields: see [`terrain_yields.md`](terrain_yields.md) — do not re-dump from wiki.

---

## Units / goods / skills

Wiki hubs: [List of units in Col](https://civilization.fandom.com/wiki/List_of_units_in_Col), [Goods (Col)](https://civilization.fandom.com/wiki/Goods_(Col)), [Market system (Col)](https://civilization.fandom.com/wiki/Market_system_(Col)).

Prefer `@UNIT` / `@CARGO` / `@JOB` in `NAMES.TXT` for numbers. Wiki adds qualitative rules worth verifying:

- Equip soldiers/dragoons from muskets (+ horses); pioneers from tools; missionaries bless (church) / expert via education or Brebeuf.
- Treasure from natives / LCR; Cortes / king transport interactions.
- Wagon / ship cargo holds; privateers (Drake).
- Recruit pool of three on Europe docks; crosses speed free emigration ([Europe (Col)](https://civilization.fandom.com/wiki/Europe_(Col))).

Port: combat Missing; Europe buy/sell Partial; equip Partial.

---

## Colonies / defense

| Claim (wiki) | Source | Port / original |
|--------------|--------|-----------------|
| Stockade: defender strength **+100%**; replaces Fortify benefit inside | [Stockade (Col)](https://civilization.fandom.com/wiki/Stockade_(Col)) | Matches [`building_production.md`](building_production.md); combat wired `colonies_fortification_defense_bonus_percent` |
| Fort: **+150%**; coastal fort slows / fires on adjacent enemy ships (wiki: attack strength 4 + 4 per artillery) | [Fort (Col)](https://civilization.fandom.com/wiki/Fort_(Col)) | Chart +150% in land combat; naval fire **Done** (`units_coastal_fort_fire_pulse`); ship-slow PARK |
| Fortress: **+200%**; stronger coastal fire (wiki: 8 + 8 per artillery) | [Fortress (Col)](https://civilization.fandom.com/wiki/Fortress_(Col)) | Chart +200% in land combat; naval fire **Done** (tier×8 formula) |
| With Stockade/Fort/Fortress, cannot **voluntarily** reduce population below **3** | Stockade / [Colony (Col)](https://civilization.fandom.com/wiki/Colony_(Col)) | Port eject keeps ≥**3** (aligned) |
| Last colonist leave → abandon confirm | Colony (Col) | Port Partial (abandon confirm) |
| Center tile always feeds at least one colonist (cannot starve out completely except edge cases wiki notes for La Salle) | Colony (Col) | Verify vs code |

---

## Natives

Port / decomp hub: [indians.md](indians.md).

[Indians (Col)](https://civilization.fandom.com/wiki/Indians_(Col)) — qualitative checklist (all Unverified vs DOS):

| Topic | Wiki summary |
|-------|----------------|
| Tribes | Semi-nomadic camps: Apache, Sioux, Tupi. Agrarian villages: Arawaks, Cherokee, Iroquois. Empires: Aztec (warlike), Inca (peaceful). Capital marked with starburst. |
| First contact | Leader offers peace + small land grant; reject → war. Peace → visit, trade, learn skills, gifts. |
| Alarm | Encroachment / military presence raise alarm (`!` green → red). Alarmed units may refuse trade, attack, kill scouts. French national bonus slows hostility; missions slow it; Pocahontas resets + halves future growth. |
| Capital destroy | Hostile tribe surrenders once; hostility reset; no new capital. |
| Teach / trade / missions | Elder skills; sea/land trade; missionaries (expert via Brebeuf); denounce foreign mission 50/50. |
| Nation bias | Spanish pushed toward conquest (Aztec/Inca treasure); French toward cooperation. |

Port: villages + light AI Partial; first contact **Done** structural
(`FUN_5bfb_022e` `@INDIANWELCOME` → `0182` peace / `@INDIANSHUN` war; thin land
grant stamps purchased+owner on occupied tile); capital destroy surrender
**Done** thin; sea/wagon trade-goods drain **Done** thin; foreign-mission
heresy 50/50 **Done** thin; colony encroachment **Done** thin; meet/trade/teach/
missions/alarm Partial
(`ai_popup`; deep VGA / WARPATH gold PARKED). Tribe tech colors:
`@TRIBES` in `NAMES.TXT`.

---

## Europe / tax / REF funding

| Claim (wiki) | Source |
|--------------|--------|
| King raises tax periodically; difficulty sets first tax year + interval (Discoverer 1536/22 … Viceroy **1532**/14 — wiki’s 1534 is wrong; see [difficulty.md](difficulty.md)) | [Tax rate (Col)](https://civilization.fandom.com/wiki/Tax_rate_(Col)) |
| On hike: accept; or boycott (throw named goods) — rate does not rise this time, but goods blocked in Europe until penalty paid or Fugger | [Boycott (Col)](https://civilization.fandom.com/wiki/Boycott_(Col)), Tax rate |
| Boycott buy-back ≈ cost of **500 tons** of that good at current price | Tax rate (cites StrategyWiki) |
| Max tax **75%** | Tax rate |
| Taxes paid accumulate toward expanding the REF | Tax rate / REF |
| Custom House bypasses boycotts; during Revolution can bypass tax (wiki) | Tax rate, Custom House |

Port: tax on sales Partial; king events / boycotts / volume prices Missing.

Europe screen: recruit (3-slot pool), train, buy ships/artillery, buy/sell goods — [Europe (Col)](https://civilization.fandom.com/wiki/Europe_(Col)).

---

## Independence / win

[Independence (Col)](https://civilization.fandom.com/wiki/Independence_(Col)), [Royal Expeditionary Force (Col)](https://civilization.fandom.com/wiki/Royal_Expeditionary_Force_(Col)), [Foreign intervention (Col)](https://civilization.fandom.com/wiki/Foreign_intervention_(Col)):

- Declare when **total** SoL ≥ 50%.
- King sends REF (Men-O-War, Regulars, Cavalry, Artillery); initial size by difficulty; grows with taxed gold.
- Europe screen closed; Custom House can still trade.
- Liberty Bells shift from FF election to foreign intervention (free military units + naval bombard bonus vs Tory cities when threshold met).
- Veteran Soldiers → Continental Army / Cavalry.
- Lose all **port** colonies → lose war. Defeat REF + recapture lost colonies by **1850** → win.
- REF AI (wiki strategy): prioritizes weakest-defended ports; man-o-war with 6 units; seizes occupied landing tiles; always attacks adjacent uncaptured colony rather than marching past.

Port: all Missing except thin score schedule.

---

## Conflicts / open questions

| Topic | Wiki | Port / original notes | Action |
|-------|------|----------------------|--------|
| Stockade+ min voluntary population | Below **3** | Port eject keeps ≥**3** | Aligned with wiki |
| Factory efficiency | “1.5 goods per raw” | Port / EXE tier table framed as **6→9** hammers-style throughput | Same intent; keep decomp numbers |
| Fur Factory min pop | Adam Smith page says factories need pop **8** | [`building_production.md`](building_production.md) lists Fur Factory min **6** from `NAMES` | Prefer `NAMES` / code |
| Tax first-year table (Viceroy 1534) | Listed on Tax rate page | Decomp/port `1536-diff` → Viceroy **1532** | **Resolved** — [difficulty.md](difficulty.md) |
| Custom House sell thresholds (100/50) | Strategy section on Custom House page | Decomp `FUN_364b_0688` (`stock>99` → leave 50) | **Done** structural autosell |
| SoL inefficient-government Tory caps (10 / 6) | Sons of Liberty page | Decomp `10-diff` + `−⌊tories/thresh⌋` | **Resolved** — [sons_of_liberty.md](sons_of_liberty.md); production floor still PARK |
| Fort/Fortress coastal bombardment strengths | Strategy sections (4+4 art / 8+8 art) | Not in `NAMES` building rows | Combat RE |

---

## Crawl index

Fetched 2026-08-04 via `https://civilization.fandom.com/api.php?action=parse&page=…&prop=wikitext` (HTML `WebFetch` blocked by Cloudflare).

### Hubs

- https://civilization.fandom.com/wiki/Sid_Meier%27s_Colonization
- https://civilization.fandom.com/wiki/Founding_Fathers_(Col)
- https://civilization.fandom.com/wiki/Building_(Col)
- https://civilization.fandom.com/wiki/List_of_buildings_in_Col
- https://civilization.fandom.com/wiki/List_of_units_in_Col
- https://civilization.fandom.com/wiki/Goods_(Col)
- https://civilization.fandom.com/wiki/Indians_(Col)
- https://civilization.fandom.com/wiki/Europe_(Col)
- https://civilization.fandom.com/wiki/Colony_(Col)
- https://civilization.fandom.com/wiki/Continental_Congress_(Col)

### Founding Fathers (25)

- https://civilization.fandom.com/wiki/Adam_Smith_(Col)
- https://civilization.fandom.com/wiki/Jakob_Fugger_(Col)
- https://civilization.fandom.com/wiki/Jan_de_Witt_(Col)
- https://civilization.fandom.com/wiki/Peter_Minuit_(Col)
- https://civilization.fandom.com/wiki/Peter_Stuyvesant_(Col)
- https://civilization.fandom.com/wiki/Ferdinand_Magellan_(Col)
- https://civilization.fandom.com/wiki/Francisco_de_Coronado_(Col)
- https://civilization.fandom.com/wiki/Henry_Hudson_(Col)
- https://civilization.fandom.com/wiki/Hernando_de_Soto_(Col)
- https://civilization.fandom.com/wiki/Sieur_de_La_Salle_(Col)
- https://civilization.fandom.com/wiki/John_Paul_Jones_(Col)
- https://civilization.fandom.com/wiki/Francis_Drake_(Col)
- https://civilization.fandom.com/wiki/Paul_Revere_(Col)
- https://civilization.fandom.com/wiki/George_Washington_(Col)
- https://civilization.fandom.com/wiki/Hernan_Cortes_(Col)
- https://civilization.fandom.com/wiki/Benjamin_Franklin_(Col)
- https://civilization.fandom.com/wiki/Simon_Bolivar_(Col)
- https://civilization.fandom.com/wiki/Thomas_Paine_(Col)
- https://civilization.fandom.com/wiki/Pocahontas_(Col)
- https://civilization.fandom.com/wiki/Thomas_Jefferson_(Col)
- https://civilization.fandom.com/wiki/Bartolome_de_las_Casas_(Col)
- https://civilization.fandom.com/wiki/Juan_de_Sepulveda_(Col)
- https://civilization.fandom.com/wiki/Father_Jean_de_Brebeuf_(Col)
- https://civilization.fandom.com/wiki/William_Penn_(Col)
- https://civilization.fandom.com/wiki/William_Brewster_(Col)

### Buildings / colony

- https://civilization.fandom.com/wiki/Custom_House_(Col)
- https://civilization.fandom.com/wiki/Stockade_(Col)
- https://civilization.fandom.com/wiki/Fort_(Col)
- https://civilization.fandom.com/wiki/Fortress_(Col)
- https://civilization.fandom.com/wiki/Docks_(Col)
- https://civilization.fandom.com/wiki/Church_(Col)
- https://civilization.fandom.com/wiki/Printing_Press_(Col)
- https://civilization.fandom.com/wiki/Newspaper_(Col)
- https://civilization.fandom.com/wiki/Warehouse_(Col)

### Economy / independence

- https://civilization.fandom.com/wiki/Liberty_bell_(Col) (and Liberty_Bell_(Col))
- https://civilization.fandom.com/wiki/Sons_of_Liberty_(Col)
- https://civilization.fandom.com/wiki/Tax_rate_(Col)
- https://civilization.fandom.com/wiki/Boycott_(Col)
- https://civilization.fandom.com/wiki/Market_system_(Col)
- https://civilization.fandom.com/wiki/Independence_(Col)
- https://civilization.fandom.com/wiki/Royal_Expeditionary_Force_(Col)
- https://civilization.fandom.com/wiki/Foreign_intervention_(Col)
- https://civilization.fandom.com/wiki/LCR
- https://civilization.fandom.com/wiki/Terrain_(Col)

### Skipped / thin

- `Civil_disorder_(Col)` — empty / missing page
- Civ4Col / FreeCol counterparts — out of scope
- Individual building leaf pages beyond those listed (costs → `NAMES.TXT`)
