# Euro ship / cargo scoring (`FUN_521d_20e6` OPEN)

Section map for ship band **`LAB_521d_3558`** (89384–89870) and follow-on
cargo / wagon arms **`4393`…`47b9`** (89871–90224).

Parent: [`move_scoring.md`](move_scoring.md). Annotated stub:
[`euro_ocean_scoring.c`](euro_ocean_scoring.c). Land OPEN arms:
[`move_scoring_land.md`](move_scoring_land.md).

**Port status:** mapped; Linux `ai_euro_ocean_score_step` thin. Cargo matrix /
`−0x6790` deep tables **PARKED**.

## Gate into ship band

`LAB_521d_304c` → `3558` when:

- type-table `0x5237` sail capacity byte is 0, **or**
- `local_2e != 0`, **or**
- non-ship type with colony-bind mismatch

Inside `3558`, body runs only if `local_34 != 0` (type ∈ **[0x0d, 0x12]**).

Naval tests later use open upper **(0x0c, 0x13)** — wider than dispatcher
`SHIP_A..C`. Do not conflate.

## `LAB_521d_3558` phases

```
3558 ship entry (local_34)
  1. Hold scan
       for slot in 0..cargo_count(0x3150):
         type = 0be6(unit,slot); amt = 0c68(unit,slot)
         track max cargo type local_44; accumulate local_c8[type]
  2. Stack queries (08bc modes 2/3/4/5/6) → local_a8/4a/48/46/16
       free_slots local_82 = −local_48 − (local_16 − local_a8) − local_4a
       wartime (0x5382 bit0): add mode-0xc query into local_48
  3. stack_set_xy(unit) refresh
  4. Goal probes
       local_66 = 0584(nation,xy,7)   war/military
       iVar16   = 0584(nation,xy,1)   settle/found
       local_68 = 0464(unit)          work/primary goal id
       if goal: 053c + 0494 → local_58 urgency; may fold free into local_4a
  5. Build land-adj flag word local_9c (8 dirs) when free holds + cargo kinds
       bits (OR):
         0x20  trade/specialty continent (−0x6790 / −0x6b5a / −0x7a38)
         0x40  founder / empty colony continent / war probes / bitmaps 0x173e
         0x10  wartime / war human continent / flag 0x6a0e / probes
         0xffff special: already sailing to continent matching goto
       Cleared when DS:0x1740 != 0
  6. If local_9c: walk stack units; for land units matching (0x523d & local_9c)
       dir = 06ae via 2a1f_04ac (~89587) — sole 20e6 founding call
       step_unit + exhaust; may leave ship
  7. Colony sail pick (~89614–89711) when free cargo or empty local_9c
       score own colonies → best (x,y) → LAB_521d_27f5 goto
  8. Treasure / empty-hold / capacity epilogues → maybe 3fa6 or 4393
LAB_521d_3fa6 → 291f_02ea → 48d3_015e spiral HS / set sail → 5a78
```

### `local_9c` bit cheat sheet

| Bit | Set when (summary) |
|-----|--------------------|
| `0x40` | Founder unload wanted: empty colony count on continent, war probe 7/1, bitmap `0x173e`, or `local_66`/`iVar16` |
| `0x20` | Specialty/trade pressure vs `−0x7a38` / `−0x6b5a` tables |
| `0x10` | Military unload: wartime human colonies, peacetime flag `−0x6790==4`, `0x6a0e` bits, probes |
| `0xffff` | Already committed goto on that continent (suppress others) |

Continent tables live at nation×16 + continent offsets **`−0x6790`**,
**`−0x6b1a`**, **`−0x6b5a`**, **`−0x6a0e`**. Linux has no full port of these
nibbles yet (PORT DEBT / unpark #4).

### Colony sail score (peace vs war cargo)

- **Peace (`local_48==0`):** RNG + pop-distance terms + dock flag `0x1b&0x10` +
  human military presence `+0x14`.
- **War cargo:** requires `−0x6790 != 0`; add fort bits `0x6a0e&7`, difficulty
  `a89c`, human presence `+0x32`, building flags `0x1b` (±`0x3c`/`0x2d`/`0xf`),
  idle timer `+0x8f`, ship-type docks.
- Distance penalty via `037a` Chebyshev-ish; keep best `local_e2`.

## Follow-on bands (`4393` … `47b9`)

| LAB | Lines | Gate | Role |
|-----|-------|------|------|
| `4393` | 89871+ | type ∈ (0x0c,0x13), holds empty-ish | Score **work queue** `−0x5f24` (16×6) → best colony haul target → `4567`/`27f5` |
| `4567` | 89926 | — | Bind colony y → `27f5` goto |
| `457e` | 89930 | — | Empty ship + sail bit / turn cadence → `3fa6` HS spiral |
| `4701` | 89964 | Wagon/type path | Bind colony → `4567` |
| `47b9` | 90037 | type `0x0c` / fail paths | **Destroy unit** (`0808`) — wagon/treasure dead ends |
| `48ab`+ | 90047+ | type `0x03` Pioneer-ish | Nearest tribe / tile score (land follow-on; still OPEN) |

Work-queue layout (AI goals): id @ `−0x5f24`, score @ `−0x5f22`, count byte
`−0x5f20`, flag `−0x5f1f` — same family as `euro_goals.c` work queue.

## Linux thin vs OPEN

| Behavior | Linux | OPEN |
|----------|-------|------|
| Ocean step toward goto | `ai_euro_ocean_score_step` (HS west/east bias, fort avoid, thin war) | Full `local_9c` unload + colony sail matrix |
| HS place | `units_spiral_place_hs_near` / `48d3_0434` | Matches `3fa6` intent |
| `06ae` unload | Founding peels / landfall table | Live call inside `3558` with `local_9c` mask |
| Work-queue haul | Thin specialty_cargo / haul scores in planning | Full `4393` distance-normalized pick |
| Atlantic / cruise XY | PORT DEBT peels | Retire when ocean `20e6` matches TURN1→4 |

## Related

- Founding tile: [`euro_goals.c`](euro_goals.c) `06ae` + [`move_scoring.md`](move_scoring.md)
- HS helpers: `FUN_48d3_015e` / `0434` / `048e`
- Commit: `FUN_521d_20c6` orders=`0x0B` goto
