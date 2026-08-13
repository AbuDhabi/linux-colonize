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
| `0x9e58`…`0x9e76` | Ask table (16×int16) — Ghidra `-25000` |
| `0x9e78`…`0x9e96` | Bid table (16×int16) — Ghidra `-0x6188` |

## Linux — **Done** (scorer + consumers)

Port: `ai_contact_meet_economics_2154` in [`ai_contact.c`](../../src/core/ai_contact.c).

| Phase | Linux |
|-------|-------|
| 1 Cover | Tribe-local 25-cell mask from colony↔tribe relative overlap; `281f_0ce0`→`15eb_06a6`→`15eb_05e2` work-slot gate **Done** — colony's own tile always covered, else one of the 8 immediate (N/NE/E/SE/S/SW/W/NW) ring tiles covered only when actively worked (`colony->tiles[dir] >= 0`); outer distance-2 ring cells never worker-assignable (`col1_bridge.c` `tiles[8..19]` stay `-1`) so are never covered, matching DOS byte-for-byte |
| 2–3 Buckets | `map_dos_terr_class_at` class arms (`0x1b`/`0x1c`/`0x18`/plains/forest) |
| 4 Write | Decomp formulas → `ask[16]` / `bid[16]` from `tech`, `population+1`, buckets, `muskets` / `horse_herds` / `horse_breeding` / `tons[]` |
| 5 Clamp/mix | Ask clamp 0..0x32; capital doubles ask[0..7] + bumps; tons mix; bid/ask half-cross |
| Divisor | `*(0x8d52 + −0x69d6)` → `head.difficulty` (0..4; min 1) |
| Debug | `0x894&4` dump **PARKED** (VGA) |

**Consumers (`ai_contact_gift_or_demand`):**

- Gift Generous (−20/−3) when `ask[0]−bid[0] ≥ 1`, nation gold ≥ `0x4b`, and delta ≥ thin RNG (`281f_04d4` stand-in 1..100); else Large (−10/−2) when gold ≥ 20.
- Demand: `ask[0] < bid[0]` → gold-first; else tools-first (same drains).

Series P/S scalar `S` floors **retired**. Unit: `unit_ai_contact`. Not blanket Indian T3 (`2820`/`4528` remain partial).
