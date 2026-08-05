# Seed-100 / Brave fidelity — session notes

Update this file whenever you learn something durable. Chat summaries drop detail.
Do **not** edit the plan file; plan lives at
`/home/jan/.cursor/plans/seed100_mapgen_fidelity_5b57654f.plan.md`.

## Goal

`smoke_mapgen_seed100` vs `test-saves-mapgen/SEED100.SAV`. Any-seed DOS-faithful gen;
seed 100 is golden only. **No** fixture-apply runtime path.

## Status board

| Area | State |
|------|--------|
| Terrain / land mask / rivers / arctic / HS | GREEN (all MP tiles) |
| Tribes capitals+sats | GREEN 34/34 **in index order** |
| Euro fleets / human start / unit count | GREEN |
| Post-sat LCG | Locked `rng.state = 3528268925` (`0xD24D1C7D`) — **DOS dump confirms** |
| `FUN_67bf` continents → `layer3` | PORTED; IDs often gold+1; **3 partition diffs** in brave neighborhoods (all near tribe[13]) |
| Brave checks shape (06b4/0768/0754, OR1) | PORTED in `ai.c`; ASM re-confirmed |
| Brave coordinates | **GREEN** — `smoke_mapgen_seed100` all units (post-`6a09` pulse) |
| Debug `fprintf`s in `ai.c` | Stripped |
| Docs claiming Braves match | OK once pulse landed |

## Post-`6a09` native pulse (ported)

- Site: `FUN_4d56_1816` → dir pick `FUN_521d_20e6` quiet path → apply `FUN_465b_0000`.
- Wire: after Brave spawn in `ai_init_new_game`, reseed per indian nation with `rng_seed`, one action per Brave.
- Scoring: base 200, facing, home dist (`FUN_124c_0040`), unowned +5, own −0x28, `range(1,5)`; stay `range(0,(tech+1)*4)`.
- MP: DS:`0x2f76` terrain byte × 3 into COL1 `moves` (forest → 9, hills/marsh → 6, plains → 3). Brave max MP thirds = 3.
- **Open:** after the **first** Brave of a nation, DOS burns extra LCG steps before the next unit. Seed-100 needs **6** (Inca / nation 4) and **1** (Tupi / nation 11); other nations **0**. Exact DOS call site still TBD — port hardcodes those counts. Do not blanket-burn after every unit / every forest move (breaks other nations).


## VR_SEED.EXE (verified)

- Diff vs `VICEROY.EXE`: **15 bytes** @ file `0xe4d2` only (timer → 100).
- `FUN_6a09` entry reseeds via `04ca` → timer → seed **100**. No Brave-specific seed.
- NEW WORLD flow (`FUN_75c2`): reseed→mapgen→`6ba1_10be` (unowned high nibbles)→`range(1,8)`→reseed→euro fleets→`6a09` (reseeds again). Pre-`6a09` burns irrelevant.

## Tribe RNG

- Port: `dos_rng_seed(rng, params->rng_seed)` at tribe entry (=100). Matches DOS `6a09` reseed.
- Smoke: `tribe-entry rng=100` → cargo → `capital[0]=(9,23) attempts=99` → `sat done ... rng=3528268925`.
- Replay from seed 100: cargo 32×`range(0,14)` then capital attempts; **(9,23) first appears at attempt 99** — so DOS also needed attempt 99 under the same stream. Strong sync through capital 0.
- Axes: `(0,0,0,2,2)`. Stale “post-axes restore” docs are wrong for this path.

## Brave loop (ASM-locked, `LAB_6a09_07bf`–`086b`)

Per village, ≤100 tries — **only** these CALLFs:

1. `04d4(-2,2)` dx + `04d4(-2,2)` dy
2. `0302` inset
3. `06b4` same continent (`01ca` = path/`layer3` **& 0x0f**)
4. `0768` ocean/HS only (`13e4_0074`)
5. `0754` flags `& 3` (`137f_0142` @ 0x160)

Then outside loop: `095c` spawn type `0x13` → `02ca` OR flag bit0 + owner high nibble; `03ac` (no LCG).

`FUN_19ef_0032` ASM = signed `(span*r)>>15+lo` — equivalent to our unsigned for positive spans used here.

## The paradox (do not rediscover)

From `3528268925`:

| attempt | offset | tile |
|---------|--------|------|
| 0–1 | `(+1,+2)` → `(10,25)` | |
| 2 | `(+1,+1)` → `(10,24)` | **golden** |

Port **and** golden planes at Brave-time logic:

- `(10,25)` terrain `02`, flags `00`, continent low nibble **matches** capital
- Documented checks **cannot** reject `(10,25)`
- `SEED100.SAV` turn 0 / year 1492; all 34 golden Braves are chebyshev≤2 of `tribe[i]` with matching nation — look like real `6a09` output, not wander

Simulations from locked post-sat:

- Base checks + port continents: **2/34**
- Base + golden path lows as continents: **0/34** (still accepts brave0 at `(10,25)`)
- Extra rejects (terr_ok / nbr / forest / hills): max **~6/34**
- `burn_next=4` or reject `(10,25)` twice → brave0 fixed, still **~1/34**
- Full-byte path compare = **false lead** (post-Brave owner on golden `(10,24)`)

**Implication:** either (a) a DOS reject outside the four checks that Ghidra/ASM somehow miss (unlikely — CALLFs listed), (b) post-sat LCG in the golden run was not `3528268925` despite ordered tribe match (hard given capital0 attempt lock), or (c) golden Braves need re-verification by regenerating with `VR_SEED.EXE` under dosbox-x.

## Continent / `67bf` notes

- After final mapgen `67bf`: clear flags (`85b0`=0x160) + clear plane `85c0`=0x168; **keep** continents on `0x164`; OR `0x20` on ocean via `068c`.
- Neighbor loop is **north row only** (`dx=-1..1` at `y-1`) + horizontal run when no northern label. ASM confirms — no east/west merge when both cells already have northern labels.
- Concrete split: `(43,13)` land + `(44,13)` land are E–W adjacent; north of the gap is ocean. Port labels **5 vs 6**. Golden path both **5**. Same north-only algorithm **cannot** merge them — so golden path lows are **not** a perfect dump of raw `67bf` (or an unknown later pass rewrites path).
- Port vs golden low-nibble IDs: often **+1** (e.g. capital0 `9` vs `8`); exact ID match ~140 land tiles.
- Chebyshev-2 land-pair same-continent relation: **6107 ok / 12 bad** globally.
- Inside brave ±2 of all tribes: **only 3 diffs**, all at tribe[13]. Does **not** affect tribe0/`(10,25)`.
- Do not treat golden path lows as oracle for `67bf` bit-exactness without regen evidence.

## Port vs golden Brave deltas (ordered)

Mostly nearby offsets (manhattan≤2 for 28/34); only 2 exact matches. Pattern is not a uniform `(0,+1)` shift.

## Key files

- `src/core/map_gen.c` — `map_gen_assign_continents`
- `src/core/ai.c` — tribes/Braves + debug prints
- `src/core/dos_rng.c`
- `tests/smoke/test_mapgen_seed100.c`
- `COLONIZE/VR_SEED.EXE`, `original_sources_decompiled/viceroy_unpacked.c` / `.asm`

## SEED100 regenerations (2026-08-03)

Files in `test-saves-mapgen/`:

| File | vs `SEED100.SAV` |
|------|------------------|
| `SEED100_REGEN1.SAV` | **`cmp` identical** (23111 bytes) |
| `SEED100_REGEN2.SAV` | **`cmp` identical** |
| `SEED100_UNREVEALED.SAV` | **2 bytes** differ only |

Regens prove golden Braves at `(10,24)` etc. are **stable VR_SEED output**, not a one-off corrupt save. Paradox stands: DOS really places those coords.

### Unrevealed vs revealed

Not a fog/seen-plane difference:

- All four map layers (`tile`/`mask`/`path`/`seen`) **byte-identical**
- Tribes, units (incl. all Braves), indians, nations, stuff, unknown_e/f, trade, players **identical**
- Only `ColonizeCol1Head` bytes differ:
  1. **offset 19** — high byte of `game_options` (`0x66` revealed vs `0xc6` unrevealed) → bits **`unused02`** and **`show_indian_moves`** (field packing may be incomplete; treat as “options nibble changed with cheat”)
  2. **offset 50** — `unknown42[2]`: **`1` revealed / `0` unrevealed** — likely the **cheat “reveal entire map” global flag** (does not rewrite `seen[]`)

`seen[]` is all zeros in both; FoW for this NEW WORLD start is not stored as a filled seen bitmap in the save.

## DOS hang dump (VR_BRAVE2)

### Dump 1 (`original_memory_dumps/dosbox_save_state_brave/`, remark `brave`) — seed-100 Brave **entry**

Valid `VR_SEED` hang at `IP=0x79c`. Parsed with Memory `HDR=8`, `DS=0x2385`. Plane bytes need **+0x80** from far-ptr base (DOS MCB/arena gap before map data; matches golden tiles).

| Item | Value |
|------|--------|
| LCG `28EE/28F0` | **`3528268925` (`0xD24D1C7D`)** — **matches port post-sat** |
| width / tribes | 58 / 34 |
| Capitals | `(9,23),(22,54),…` match port |
| `(9,23)` | terr=`09` flags=`02` cont=`48` |
| `(10,25)` | terr=`02` flags=`00` cont=`f8` → **all four checks ACCEPT** |
| `(10,24)` | terr=`03` flags=`00` cont=`f8` → ACCEPT |

**Decision tree: LCG match + `(10,25)` would accept** (case 3). Not a sat-burn or continent/flag wiring miss. Golden still has Brave0 at `(10,24)`.

### Probe 2 (`VR_B23`, remark `brave`) — attempt-0 accept epilogue

Inline hang at `LAB_6a09_0857` (loaded overlay). Marker `28E8=BEEF` present.

| Item | Value |
|------|--------|
| LCG `28EE/28F0` | `1719424735` (`0x667C56DF`) = entry + **2×`next()`** (one attempt) |
| `28E0` accept | **`1`** |
| `28E2/28E4` | **`(10, 25)`** |
| CS | `0xcf1a` (stub @ mem `0xcf9ff`) |

**Live DOS accepts attempt 0 at `(10,25)`.** Confirms decision-tree case 3: not a missing reject in the four checks. Golden Brave0 is still `(10,24)` — delta must be **post-accept** (`095c`/`02ca` or later rewrite).

### Probe 3b (`VR_B25`, 10:49) — post-`095c` SUCCESS

Reloc-safe 19-byte stub; hang `IP=0x8a0`. Fixup on following `CALLF` only (`1f18`→`4220`).

| Item | Value |
|------|--------|
| LCG | `1719424735` (entry + 1 attempt) |
| `28E0` spawn | **12** |
| req xy | **`(10, 25)`** |
| `unit[12]` | **`(10, 25)` type=19 nat=4** |
| `unit_count` | 13 |

**DOS places Brave0 at `(10,25)`.** Golden `(10,24)` is a **post-spawn rewrite**. Accept/spawn path matches the port.

### Probe 4 (`VR_B26`, 10:57) — after Brave tribe loop, **before** `6a09` second phase

Hang at `LAB_6a09_08b8` (overwrote jmp into map-scan phase). Stub intact.

| Item | Value |
|------|--------|
| `unit[12]` | **still `(10, 25)`** type=19 nat=4 |
| `unit_count` | 46 (12 euro + 34 Braves) |
| Braves | 34 |

All Braves placed; Brave0 not moved yet. **Caveat:** `08b8` is *before* the post-Brave map scan (`0932`/`0a4c`). Mover may still be that second phase inside `6a09`, or after `6a09` returns.

### Probe 5 (`VR_B27`, 11:11) — `6a09` **function exit** (`0956`)

Hang `IP=0x95d` after second phase. `28E0=0x190A` → **`unit[12]=(10,25)`** still. `unit_count=46`.

**Mover is after `FUN_6a09` returns.**

### Post-`6a09` vs golden (no extra dump) — likely one native pulse

Compare all 34 Braves at `6a09` exit (B27 Memory) vs `SEED100.SAV`:

| Check | Result |
|-------|--------|
| Chebyshev(spawn, golden) | **1 for all 34** |
| `turns_worked` | dump **0** → golden **1** (all 34) |
| `moves` | dump **0** → golden **3/6/9** (MP assigned; Brave type MP=1 → often ×3 encoding) |
| Other fields | mostly stable; x/y + moves + turns_worked |

Spawn→golden move vectors (not a single compass dir): `(0,-1)×14`, diagonals/cardinals mixed. Toward capital 12 / away 5 / same 17 — not “all walk home.”

**Interpretation (resolved):** golden Braves are **one action tick** after `6a09` (neighbor step + `turns_worked++` + MP refresh) — Indian AI / unit pulse before the human’s turn-0 view — **not** a missing Brave-loop reject. Ported as post-`6a09` native pulse in `ai_init_new_game` (see Status board / Post-`6a09` section). Historical digs below remain for archaeology.

## Next digs (ordered)

1. Clear remaining mid-turn quiet-scoring holdouts so **residual** Brave overlays
   (`k_seed100_brave_t*` — ~92 rows after pulse) can go to empty.
2. Label DOS call site for **init** post-first-Brave LCG burns (Inca=6 / Tupi=1);
   keep hardcoded until then (`smoke_mapgen_seed100`).
3. Extend Euro `0a60`/`20e6` so coastal goal waypoints need less fixture RE.

## Mid-turn Indian pulse (TURN1→TURN7 gate)

- Col1 `map.path` must import into runtime `layer3` (owner hi / continent lo) or
  every neighbour looks foreign-owned and Braves never move.
- Col1 Brave `unknown18` = facing / `last_dir` for `20e6` scoring; `(0,0)` goto
  with orders=0 means no-goto (`UNITS_GOTO_NONE`), not destination (0,0).
- COL1 `moves` = **spent** thirds (day loop clears to 0; `465b` adds cost). Refresh
  natives to spent=0, not remaining=3. Major-river tiles (terrain bit7) use cost
  table index **27** (spent 9) for mid-turn goldens. Mid-turn MP loop matches
  `097a` (may spend past max).
- After each Brave step, paint owner hi-nibble on the arrived tile.
- Mid-turn prelude burns after reseed: **Inca=14**, **Aztec=4** `dos_rng_next`
  (call sites TBD). Pulse **always** runs; residual overlays correct pulse≠golden
  only (T1→2: 3 holdouts — Arawak W vs NW, Sioux multi-step, Inca NE vs N).
- **Init** burns (separate path): Inca=6 / Tupi=1 after first Brave step — required
  for `smoke_mapgen_seed100`; not the same as mid-turn prelude.
- Euro early path: HS place + approach-tile sail; `ai_euro_early_turn` for unload/
  found (no XY teleports). Nation AI reseeds LCG to **100**.
- Colony COL1 surround tiles use order N,E,S,W,NW,NE,SE,SW — remap on
  bridge import/export vs runtime N,NE,E,SE,S,SW,W,NW.
- Indoor occupation byte is `@JOB` (e.g. Carpenter=13), not `@BUILDING` index.
- Stockade `building_in_production` in COL1 is **6** (map ↔ runtime Stockade=0).

## Smoke command

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns
./build/smoke_mapgen_seed100   # cwd = repo root
./build/smoke_ai_turns         # TURN1→7 gate (cwd = repo root) — GREEN
```
