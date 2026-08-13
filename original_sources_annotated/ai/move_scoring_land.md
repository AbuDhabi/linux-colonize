# Euro land / combat / explore scoring (`FUN_521d_20e6` OPEN)

Section map for decomp **88975–89375** (`LAB_521d_5183` … mid-gate
`304c`). Quiet Brave / `54f5` bands are Done elsewhere — this file owns the
**Euro land** arms that remain **OPEN** for unpark #4.

Parent: [`move_scoring.md`](move_scoring.md). Act entry: [`euro_unit_act.md`](euro_unit_act.md)
(`5b66` → `2a1f_04f4` → `20e6`). Ship band: [`move_scoring_ship.md`](move_scoring_ship.md).

**Port status:** mapped; Linux `ai_euro_score_move` settlement/siege peels +
`ai_euro_land_best_adjacent_foe` Done thin. Deep −0x6790 / full explore ring
**PARKED**.

## Entry into this band

Reached after quiet/`54f5` fall-through when the unit is **not** on the quiet
Brave-only path. Debug chrome `281f_077e` string ids `0x17b5` / `0x1746` /
`0x174a` / `0x174e` / `0x1752` fire when `local_ae` (AI debug) is set.

Key locals (from prologue / prior bands):

| Local | Role (inferred) |
|-------|-----------------|
| `param_1` | Unit index |
| `uVar11` / nation | Active Euro nation |
| `local_88` / `local_94` | Unit x / y |
| `local_34` | Ship flag (type ∈ [0x0d,0x12]) — usually 0 here |
| `local_62` | Bound colony index (−1 if none) |
| `local_2e` | Dist / “away from colony” score |
| `local_38` / `local_2c` | Continent ids (unit vs colony) |
| `local_76` | Chosen dir 0..7 or 8=stay |
| `0x314b` | Orders / act-opcode byte written by arms below |

## Section flow

```
LAB_521d_5183
  ├─ debug 0x17b5
  ├─ if local_ce==0 (main land arm):
  │    ├─ colony-bound → bind colony (09e6) → LAB_521d_5888 (quiet-ish)
  │    ├─ type-table 0x523d bit0 + goal probe 0584(…,0) <5 + cargo_query<2
  │    │     → orders = 0x42  → LAB_521d_5899   (FOUND / settle-ish)
  │    ├─ type-table 0x523d bit2 + probe 0584(…,2) + cargo_query<2
  │    │     → orders = 0x65  → LAB_521d_5899   (CONTACT / claim-ish)
  │    ├─ combat-capable (0x5236>1) + not ship + local_ea:
  │    │     8-dir euro_settlement_owner scan → orders = 0x46 → 5899 (ATTACK)
  │    └─ fall: orders = 0x39  (default land wander / explore opcode)
  ├─ else (local_ce!=0): low remaining MP (<3) → local_76=8 stay
  └─ else branch LAB_521d_277a (colony / mission / explore matrix)
       ├─ Scout-like (type 2 or combat>1) same continent → orders 0x56 or colony goto
       ├─ Missionary (type 5) + tribe gate → 2a1f_059c dir; orders 0x4c
       └─ LAB_521d_2912 … LAB_521d_2a59: explore tile score ring
            continent match, explore nibble, LCR 0x1b skip, colony pull,
            score local_28 → pick best dir / goto
```

## Orders byte writes (`unit+0x314b`)

| Value | Gate (summary) | Meaning (port name) |
|-------|----------------|---------------------|
| `0x42` | `0x523d&1`, probe mode 0, cargo slots free | Found / settle impulse |
| `0x65` | `0x523d&4`, probe mode 2 | Contact / claim impulse |
| `0x46` | Adjacent foreign Euro settlement | Attack / siege approach |
| `0x39` | Default fall-through | Land explore / move opcode |
| `0x56` | Type 2 / combat unit, no colony bind | Scout / patrol |
| `0x4c` | Missionary + tribe mission bit clear | Mission plant |

Exact opcode names in Linux ORDERS menu may differ; treat values as DOS act codes
consumed by `5b66` / move apply — **do not invent** menu strings here.

## Explore ring (`2912` → `2a59`)

When `281f_0b28(unit)==0` (not already tasked):

1. Goal probe `2a1f_0584(nation, x, y, 6)` (explore mode).
2. Optional `2a1f_04b8` stamp when `local_6a`.
3. Score window radius `local_ca` from nation power table `−0x6ba2` and
   `local_12` (fog/explore budget).
4. Per tile: in-bounds, tribe_or_presence own/empty, continent match
   (`0722==local_38`), explore nibble (`074a&0xf`), skip terrain class `0x1b`
   (LCR), resource/LCR chrome via `0d12` / continent `06b4`.
5. Colony-at-tile (`0614`) adjusts score with `0x8db8` stack size and
   `−0x6b1a` / nation friction tables.
6. Best score → commit path `LAB_521d_27f5` (goto stamp via `20c6` family) or
   dir pick → `5899`.

## Thunks / helpers

| Call | Real | Role |
|------|------|------|
| `2a1f_0584` | goal probe | Mode 0/2/6/7 — founding/contact/explore/war |
| `2a1f_04ac` | `521d_06ae` | Founding tile (ship band only; not this arm) |
| `281f_08bc` | `1427_0d38` | Cargo / combat query |
| `281f_0696` | `137f_0358` | Euro settlement owner |
| `281f_06d2` | tribe/presence | Tile nation |
| `281f_074a` | explore mask | Coarse explore nibble |
| `281f_078c` | terrain class | Skip LCR `0x1b` |
| `281f_09e6` | `15eb_002c` | Bind active colony |
| `281f_090c` | max MP | Stay when spent near max |

## Linux thin vs OPEN

| Behavior | Linux today | OPEN (this map) |
|----------|-------------|-----------------|
| Adjacent foe pick | `ai_euro_land_best_adjacent_foe` (+ settlement prefer) | Defended case Done |
| `0x46` undefended colony | `ai_euro_land_try_adjacent_colony_seize` — **Done** full port: combat-capable land unit (attack>1) adjacent to a foreign, at-war Euro colony tile with **no defending unit** walks in and captures it outright (Colonization capture-by-move), then fortifies to hold the prize (own addition — prevents the unrelated "on own colony, no quota → admit as LABOR" beachhead gate from absorbing the conqueror into the workforce next outer-wave pass). Decomp scans all 8 neighbors via `euro_settlement_owner`; Linux additionally gates on war state (decomp has no live peacetime-seize case). Covered by `unit_land_adjacent_colony_seize` in `test_ai_euro_war.c`. |
| Step toward goal | `ai_euro_score_move` + continent/FoW/LCR/rumour thin | Full explore ring `2912` score matrix |
| Found / contact opcodes | Goals via `0a60` / peels | Live `0x42`/`0x65` writes inside `20e6` |
| Missionary `0x4c` | Thin mission contact | Full `2a1f_059c` dir + tribe gate |
| Debug `077e` | — | Ignore (AI debug overlay) |

## Related LAB exits (after this band)

| LAB | Lines (approx) | Next |
|-----|----------------|------|
| `304c` | 89376 | Mid gate → ship `3558` or continue |
| `5899` / `589e` | ~90225+ | Commit dir / `20c6` goto stamp |
| `27f5` | ~90037 path | Colony/xy goto commit |
| `5a78` | epilogue | Clear / return 0 |
