# Meet economics scorer (`FUN_4d56_2154`)

| Item | Value |
|------|-------|
| Lines | **81743–82057** (~315) |
| Thunk | `FUN_2a1f_0434` |
| Callers | `FUN_5bfb_022e` meet/contact (only live path; duplicate blob ~96088) |
| **Not** | Mid-pass `1b3a`; not combat/raid (`4528` / `5fef_*`) |

Bridge: [`indian_contact.md`](indian_contact.md) ·
[`indian_raid_outcomes.md`](indian_raid_outcomes.md) (sibling cluster only).

**Correction vs older catalog gloss:** this is **tribe-neighborhood valuation**
for gift/demand price tables (`DS:0x9e*`), not a raid action body.

## Preconditions (caller)

1. `281f_0a4c` binds tribe → `DS:0x8d4a` from unit tribe byte
2. Relation / friction RNG gates in `5bfb_022e`
3. Then `2a1f_0434` → `2154`
4. After return, meet path uses `0x9e58`…`0x9e78` vs colony gold for gifts

## Phases

| # | Role |
|---|------|
| 1 | Zero 25-cell cover mask; for each colony `281f_09e6`, mark 5×5 work-ring cells that colony can see/work |
| 2 | Scan tribe `±2` tiles; skip cells already covered by colony rings |
| 3 | Terrain / resource id via `281f_078c` → bucket into locals (`local_a0` silverish, `local_9c`, food/ore/lumber/… counters) |
| 4 | Write score words at `DS:0x9e58`…`0x9e96` from tribe level (`0x8d4e+2`), counters, building bits |
| 5 | Clamp / mix pairs at `-25000` and `-0x6188` (goods bid/ask scratch); debug dump if `0x894&4` |
| 6 | Return — **no unit spawn, no combat, no loot** |

## Key DS

| Addr | Use |
|------|-----|
| `0x8d4a` | Bound tribe |
| `0x8d4e` / `0x8d52` | Tribe / Indian-nation side data |
| `0x8542` | Bound colony (inside colony walk) |
| `0x9e58`…`0x9e96` | Meet economics outputs |

## Linux

Meet gift/demand uses thin relation/alarm stand-ins in `ai_contact` /
`ai_diplo`. **Partial `2154`:** colony 5×5 cover mask + tribe±2 forest/coast/food/ore
buckets modulate Generous gift floor (gold≥30 when rich; cover suppresses). Full
`0x9e*` table still **OPEN**.
