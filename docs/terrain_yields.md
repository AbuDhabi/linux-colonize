# Terrain field yields (colony area / town commons)

Reference for what a map square can produce when worked as a field job (or, for the colony center, as the automatic **town commons** harvest). Sentiment / Sons of Liberty bonuses are **out of scope** here.

## Sources

| Source | Role |
|--------|------|
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@UNFORESTED` / `@FORESTED` / `@OTHER` / `@RESOURCE` / `@JOB` | **Authoritative game data** loaded by the DOS binary and by [`src/core/colony_yield.c`](../src/core/colony_yield.c) |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Manual prose (Town Commons, Special Resources, Clear/Plow/Road) and the printed **Terrain Chart** (player-aid style). Numbers in that chart **do not always match** `NAMES.TXT` — prefer `NAMES.TXT` when they disagree |
| MAPEDIT resource class table (`mapedit_resource_type_by_terrain` in [`src/core/map.c`](../src/core/map.c)) | Which special resource **type** a terrain class is allowed to roll |
| Community town-square notes (e.g. peyre ForestTypes) | Useful cross-check for **center-tile dual production**; not shipped data |

Pedia / map indices: cleared land **0–7**, forests **8–23** (type = `index & 7`), arctic / ocean / sea lane **24–26**, mountains / hills treated as resource classes **27 / 28** (hill/mountain bits on the terrain byte).

---

## Field jobs → cargo

Order matches `NAMES.TXT` yield columns and `@JOB`:

| Col | Job | Cargo |
|----:|-----|-------|
| 0 | Farmer | Food |
| 1 | Sugar Planter | Sugar |
| 2 | Tobacco Planter | Tobacco |
| 3 | Cotton Planter | Cotton |
| 4 | Fur Trapper | Furs |
| 5 | Lumberjack | Lumber |
| 6 | Ore Miner | Ore |
| 7 | Silver Miner | Silver |
| 8 | Fisherman | Food (fish) |

A **0** in a yield cell means that job produces nothing on that terrain (worker should not be assigned that job).

---

## Base yields — unforested land (`@UNFORESTED`)

Cleared / never-forested tiles. Columns: Farmer … Fisherman.

| Terrain | Idx | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|---------|----:|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Tundra | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |
| Desert | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 2 | 0 | 0 |
| Plains | 2 | 4 | 0 | 0 | 2 | 0 | 0 | 1 | 0 | 0 |
| Prairie | 3 | 2 | 0 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| Grassland | 4 | 2 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| Savannah | 5 | 3 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Marsh | 6 | 2 | 0 | 2 | 0 | 0 | 0 | 2 | 0 | 0 |
| Swamp | 7 | 2 | 2 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |

---

## Base yields — forested (`@FORESTED`)

Forest type *N* clears permanently to unforested type *N* (Boreal→Tundra, …, Rain→Swamp). Clearing removes lumber/fur potential and cannot be undone (manual).

| Forest | Cleared becomes | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|--------|-----------------|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Boreal | Tundra | 1 | 0 | 0 | 0 | 3 | 2 | 1 | 0 | 0 |
| Scrub | Desert | 1 | 0 | 0 | 1 | 2 | 1 | 1 | 0 | 0 |
| Mixed | Plains | 2 | 0 | 0 | 1 | 3 | 3 | 0 | 0 | 0 |
| Broadleaf | Prairie | 1 | 0 | 0 | 1 | 2 | 2 | 0 | 0 | 0 |
| Conifer | Grassland | 1 | 0 | 1 | 0 | 2 | 3 | 0 | 0 | 0 |
| Tropical | Savannah | 2 | 1 | 0 | 0 | 2 | 2 | 0 | 0 | 0 |
| Wetland | Marsh | 1 | 0 | 1 | 0 | 2 | 2 | 1 | 0 | 0 |
| Rain | Swamp | 1 | 1 | 0 | 0 | 1 | 2 | 1 | 0 | 0 |

---

## Base yields — other (`@OTHER`)

These cannot be forested (except hills/mountains as elevation features on land).

| Terrain | Pedia-ish | Food | Sugar | Tob. | Cotton | Furs | Lumber | Ore | Silver | Fish |
|---------|-----------|-----:|------:|-----:|-------:|-----:|-------:|----:|-------:|-----:|
| Arctic | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Ocean | 25 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Sea Lane | 26 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Mountains | — | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 1 | 0 |
| Hills | — | 1 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |

Colonies cannot be founded on mountains (manual). Fishing on ocean/sea lane needs a colony **Dock** (and coastal access) before fishermen can work those surrounds — that gate is building logic, not a terrain yield cell.

---

## Plow (unforested only)

Manual: pioneers **clear** forests or **plow** non-forest; clearing raises crop potential and ends timber/fur; plowing improves fields. Player-aid note: plowing / road / river increase output.

Engine convention (also used in [`colony_yield.c`](../src/core/colony_yield.c)):

- **+1** to Farmer, Sugar Planter, Tobacco Planter, Cotton Planter when the tile is **plowed**.
- Forests cannot be plowed until cleared.
- Hills / mountains / ocean are not plow targets in normal play.

Example: Plains food 4 → **5** when plowed; Prairie cotton 3 → **4** when plowed.

---

## Roads (not requested in the breakdown, brief)

Manual: roads raise productivity of **ore, fur, and timber**; on mountains, road does **not** raise silver unless a silver deposit is present.

Engine convention: **+1** to Fur Trapper, Lumberjack, Ore Miner, Silver Miner when a road is present (silver-on-mountain nuance may still be incomplete).

Rivers also enhance production (manual); river bonuses are a separate modifier and are not tabulated here.

---

## Special resources

### Values (`NAMES.TXT` `@RESOURCE`)

Index is the MAPEDIT / PHYS0 resource type (`PHYS0` sprite ≈ `89 + type`).

| Type | Name | Value |
|-----:|------|------:|
| 0 | Depleted Mine | 6 |
| 1 | Oasis | 3 |
| 2 | Wheat (Prime Food) | 4 |
| 3 | Prime Cotton | 6 |
| 4 | Prime Tobacco | 6 |
| 5 | Prime Sugar | 7 |
| 6 | Minerals | 4 |
| 7 | Fishery | 5 |
| 8 | Beaver | 6 |
| 9 | Game | 6 |
| 10 | Prime Timber | 6 |
| 11 | Prime Timber (duplicate row) | 6 |
| 12 | Silver Deposit | 12 |
| 13 | Ore Deposit | 6 |

### Allowed terrain class → resource type (MAPEDIT)

One preferred resource type per terrain class (procedural roll may still yield none). Class = `terrain & 0x1f`, except mountain → **27**, hill → **28**. Table value `0` remaps to type **6** (Minerals); `-1` = never.

| Class | Terrain | Allowed resource |
|------:|---------|------------------|
| 0 | Tundra | Minerals (6) |
| 1 | Desert | Oasis (1) |
| 2 | Plains | Wheat (2) |
| 3 | Prairie | Prime Cotton (3) |
| 4 | Grassland | Prime Tobacco (4) |
| 5 | Savannah | Prime Sugar (5) |
| 6 | Marsh | Minerals (6) |
| 7 | Swamp | Minerals (6) |
| 8 | Boreal | Game (9) |
| 9 | Scrub | Oasis (1) |
| 10 | Mixed | Beaver (8) |
| 11 | Broadleaf | Game (9) |
| 12 | Conifer | Prime Timber (10) |
| 13 | Tropical | Prime Timber (10) |
| 14 | Wetland | Minerals (6) |
| 15 | Rain | Minerals (6) |
| 16–23 | (forest alt indices) | same as `index & 7` rows above |
| 24 | Arctic | none |
| 25 | Ocean | Fishery (7) |
| 26 | Sea Lane | none |
| 27 | Mountains | Silver Deposit (12) |
| 28 | Hills | Ore Deposit (13) |

### Effect on a matching job

Manual: special icons mark an **especially abundant** source of the related produce. Town commons: specials apply **except Prime Timber**.

Exact arithmetic is not spelled out as a formula in the manual. Current port logic in `colony_yield_for_tile`:

1. Start from base terrain yield for the job.
2. If the tile’s resource prefers that job:
   - if `@RESOURCE` value **>** base yield → use the resource value;
   - else → **base + 2**.
3. Then apply plow / road modifiers.

Resource → preferred job mapping in the port:

| Resource | Preferred job |
|----------|---------------|
| Oasis, Wheat | Farmer |
| Prime Cotton | Cotton Planter |
| Prime Tobacco | Tobacco Planter |
| Prime Sugar | Sugar Planter |
| Minerals, Ore Deposit | Ore Miner |
| Fishery | Fisherman |
| Beaver, Game | Fur Trapper |
| Prime Timber | Lumberjack |
| Silver Deposit | Silver Miner |

Game (manual) also implies abundant **food**; the port currently routes Game only through the fur job — call that out when fixing center/surround harvest.

---

## Town commons (colony center tile)

Manual (*Colonies* / Town Commons): the settlement square **always produces some food and one other commodity**, depending on terrain; special resources apply **except Prime Timber**. Surround tiles need assigned workers; the center does not take a field colonist in the usual way.

That is **not** the same as “best two jobs from the yield table”:

- Forested commons typically show **food + furs** (and food/sugar on rain forest), not lumber as the secondary (Prime Timber is excluded on the center).
- Cleared commons show **food + the land’s cash crop / ore**, and plowing raises the food (and crop) line.

Community “town square” cross-check (forested → cleared → plowed food + secondary):

| Forest | Forested | Cleared | Cleared+plowed |
|--------|----------|---------|----------------|
| Boreal | 3 food, 3 furs | 4 food, 2 ore | 5 food, 2 ore |
| Broadleaf | 3 food, 2 furs | 4 food, 3 cotton | 5 food, 3 cotton |
| Conifer | 3 food, 2 furs | 4 food, 3 tobacco | 5 food, 3 tobacco |
| Mixed | 3 food, 3 furs | 4 food, 2 cotton | 5 food, 2 cotton |
| Rain | 3 food, 1 sugar | 4 food, 2 sugar | 5 food, 2 sugar |
| Scrub | 2 food, 3 furs | 2 food, 3 ore | 3 food, 3 ore |
| Tropical | 3 food, 2 furs | 4 food, 3 sugar | 5 food, 3 sugar |
| Wetland | 3 food, 2 furs | 4 food, 2 tobacco | 5 food, 2 tobacco |

These numbers are **higher food** than raw `@FORESTED` Farmer columns (often 1–2). The port implements town-commons food as **scrub 2 / other forests 3**, cleared = `@UNFORESTED` Farmer (+1 if plowed). Secondary is terrain-fixed (forests → furs, rain → sugar; cleared → cotton/tobacco/sugar/ore as above), not “best two jobs”. Specials apply except Prime Timber; plow/road do not boost the secondary.

---

## Manual Terrain Chart vs `NAMES.TXT`

The printed chart uses forested/non-forested pairs per land name (e.g. Plains FD `3/4`, lumber `6/0`). Examples of mismatch with `@FORESTED` Mixed / `@UNFORESTED` Plains:

| | Manual Plains forested | `NAMES` Mixed | Manual Plains clear | `NAMES` Plains |
|--|------------------------|---------------|---------------------|----------------|
| Food | 3 | 2 | 4 | 4 |
| Lumber | 6 | 3 | 0 | 0 |
| Cotton | 1 | 1 | 2 | 2 |

**Implementation rule:** drive yields from `NAMES.TXT` (and MAPEDIT resource classes). Use the manual for qualitative rules (town commons dual produce, plow/road intent, which specials exist, Prime Timber exception on the center).

---

## Explicitly excluded here

- Sons of Liberty / Tory production ±1 (manual production bonus/penalty)
- Expert / Master job multipliers
- Building gates (Dock for fishing, etc.) beyond noting them
- River major/minor numeric bonuses (mentioned only)
- Combat / movement columns from the Terrain Chart
