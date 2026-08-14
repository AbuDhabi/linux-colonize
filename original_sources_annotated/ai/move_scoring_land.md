# Euro land / combat / explore scoring (`FUN_521d_20e6` OPEN)

Section map for decomp **88975–89375** (`LAB_521d_5183` … mid-gate
`304c`). Quiet Brave / `54f5` bands are Done elsewhere — this file owns the
**Euro land** arms that remain **OPEN** for unpark #4.

Parent: [`move_scoring.md`](move_scoring.md). Act entry: [`euro_unit_act.md`](euro_unit_act.md)
(`5b66` → `2a1f_04f4` → `20e6`). Ship band: [`move_scoring_ship.md`](move_scoring_ship.md).

**Port status:** mapped; Linux `ai_euro_score_move` settlement/siege peels +
`ai_euro_land_best_adjacent_foe` Done thin. Deep −0x6790 / full explore ring
**PARKED**. Full clean whole-function recovery (2215 lines, zero warnings,
2026-08-14, supersedes the canonical export's tail — see its own header for
what changed): [`move_scoring_20e6_full.md`](move_scoring_20e6_full.md).

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

## `0x8db8` identified (2026-08-14) — dist to bound/home colony, caller-supplied

`local_2e` (`Dist / away from colony score` in the Key locals table above) and
three sibling locals re-read at this band's own prologue (`local_74`/`local_a0`/
`local_3c`, all `= *(int *)0x8db8` with different casts — same DOS cell,
re-dereferenced per Ghidra's usual non-cached-global behavior, not 4 distinct
values) are now identified: `0x8db8` is written in exactly one place,
`FUN_15eb_0142(x, y, filter_nation, filter_continent)` — a nearest-colony
search over the `*(int*)0x539e`-count colony table (stride `0xca`, base
`0x5d60` for the nation/type filter field, `0x5d46`/`0x5d47` for colony x/y):
for each colony matching `filter_nation` (or any if `<0`) and continent-gated
by `FUN_137f_02a0`, computes `FUN_124c_0040(dx, dy)` (the already-named DOS
Chebyshev distance helper, `ai_dos_dist`) to `(x,y)`, keeps the closest, binds
the winner via `FUN_15eb_002c` (`= 15eb_002c`, the same "bind active colony"
target `281f_09e6` thunks to elsewhere in this file), and stores that winning
distance to `0x8db8` before returning. **`FUN_521d_20e6` never calls
`FUN_15eb_0142` itself** — it only rereads `0x8db8` as a snapshot the caller
(`5b66`/its own caller) already computed before invoking `20e6`, so within one
`20e6` call it behaves as a constant: **DOS distance from the unit's query
point to its own nearest/bound colony**, 0 meaning "standing on it."

This resolves the SCOUT/PATROL gate's condition (`local_2e==0` → orders
`0x56`; `local_2e!=0` → `goto LAB_521d_27f5` walk-to-colony) semantically:
*combat-capable/Scout unit, same continent as its colony, standing on the
colony tile → enter Scout/patrol; away from it → head back first.*

**Update, same day: the "no consumer found" blocker below is resolved —
retracted, not just narrowed.** The original text here searched the
canonical export for a `0x314b == 'V'` compare and, finding none, guessed
the consumer must be the still-unfound real `5b66` body. A full clean
re-recovery of `20e6` itself
([`move_scoring_20e6_full.md`](move_scoring_20e6_full.md), 2026-08-14) shows
**`20e6` reads `unit+0x314b` at its own entry** (checks several of its own
previously-written values, `0x56` included in spirit — the same field is
read-and-written throughout the function) — it's a persistent per-unit
decision cache `20e6` consults on its next call, not a value some other
function dispatches on. There never was a missing consumer to find. This
specific sub-gate (`local_de` continent-tier band) is now semantically
understood end-to-end; it's just the still-missing `local_12` term
(`−0x6168`, unit `+0x3154`) that keeps it from being ported, per above —
not an unfound caller anymore.

**Also gates part of the deep explore-ring colony-pull adjustment** (the
`local_de`/`local_ca` score-radius bands keyed off nation-power table
`−0x6ba2`, and the `local_a2`/`iVar13`/`iVar16` colony-pull terms keyed off
`local_32 = *(int*)0x8db8` compared against tiers 2/9 and `−0x6b1a` friction —
those adjacent tables are still their own unlabeled globals, `0x8db8`'s ID
alone doesn't unblock that arithmetic). Useful next step if anyone resumes:
`−0x6ba2` (nation power, indexed `local_38*2` = continent×2) and `−0x6b1a`
(indexed `continent + nation*0x10`, byte, "friction"-shaped) are the two
remaining unnamed tables directly gating this band — same family as
`move_scoring_ship.md`'s already-flagged `−0x6b1a`/`−0x6b5a`/`−0x6a0e` trio,
worth naming together if picked up (`FUN_15eb_0142`'s call sites at line
6380/6487/32084 plus this file's own `09e6`/`002c` binds are the cleanest
callers to trace first — none touch `−0x6ba2`/`−0x6b1a` directly, so those
still need their own write-site hunt).

**Update (same day, next pass): `−0x6ba2` resolved — it's already-named,
already-wired `continent_tally_a[16]`.** `−0x6ba2` mod 0x10000 = `0x945e`,
which `save_format_map.md` row 304 already identifies as
`continent_tally_a[16]` (`uint16_t`, "land terrain-class filter; rebuilt",
status `mapped`) — live in Linux as
`col1->post_map.continent_tally_a[continent_id]`
([`col1_post_map.c`](../../src/core/col1_post_map.c), filled per-continent
land-tile count). So the `local_de` tier-select (`<9`→0, `<0x19`→1, `<0x31`→2,
else 3) reads a value this port already computes correctly — this specific
sub-term is a real, safe, ready-to-wire target, no guessing required.

**Not wired in this pass, though — the term it feeds is not fully
resolvable yet.** DOS uses `local_de` only as a shift on `local_12`
(`local_28 += local_12 >> local_de`, plus a `local_ca` radius picked from
`local_12` directly, `0x1f`/`0x3f` tiers). `local_12` itself
(prologue: `local_12 = *(byte*)(local_38 + -0x6168)*8 +
*(byte*)(param_1*0x1c + 0x3154)`) depends on **two more unnamed values** —
DS table `−0x6168` (continent-indexed byte, no cross-reference found yet)
and unit-record byte `+0x3154` (not in any doc's unit-offset table checked
so far) — so the bonus this would feed is still not faithfully
reproducible; wiring `continent_tally_a` alone with an invented substitute
for `local_12` would be exactly the "guess at the DS globals" mistake this
file exists to avoid, not a real port.

**Also structurally (not semantically) placed `−0x6b1a` and `−0x6a8e`
this pass**, via the same negative-offset→absolute-address trick
(`mod 0x10000`): `−0x6b1a` = `0x94e6`, `−0x6a8e` = `0x9572` — both already
rows in `save_format_map.md` (255 `unknown_ds_94e6`, 259 `unknown_ds_9572`).
Neither is safe to port on this evidence alone.

**Self-correction, same day, next pass:** the paragraph originally here
claimed a second read site for `0x94e6` "inside `FUN_5952_035e` itself
(~line 95043-95062)" — a nation×continent grid used for a sole-occupant
check. **That was wrong**, caught before it went further: re-disassembled
`FUN_5952_035e` cleanly via the overlay project (`OVL15_L0000:35e`) — same
method as `4528`/`1816`/etc — and the real function is a **complete,
zero-warning, 1577-line body that never references `0x94e6`, `0x9572`,
`−0x6b1a`, or `−0x6a8e` anywhere**. The canonical export's `FUN_5952_035e`
(`viceroy_unpacked.c:93790`, 4 params) carries the exact disassembly-fault
warning class (`overlaps instruction` + 2× unreachable block) already known
to cause content misattribution, and its param count (4) doesn't even match
the clean recovery (2) — the line-94990-95070 block I read and attributed to
it was corrupted-decompile content that doesn't actually belong to this
function, same failure mode as `4528`'s old "8 raid actions" and
`15eb_1d4c`'s old "19KB corrupted function" false leads (`decomp_inventory.md`
"Lesson" notes). **Corrected finding: `0x94e6`/`0x9572`'s real reader/writer
is still wholly unidentified** — not `5952_035e`, not chased further this
pass. If resumed: the honest next step is finding which function truly
follows `5952_035e` at `OVL15_L0000` past its real ~1577-line end (not
assumed from the corrupted export's line numbers), then checking that.

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
