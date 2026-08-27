# Seed-100 Brave / early-AI notes

**2026-08-27: TURN1→2 green.** Two of the three parked Braves were the road term reading layer2 bit 0x08 (rumour-cleared stand-in) as a road (`ai_mask_fa_flags`, ai.c); the third passed once the T1.9 foreign-Euro pull was restricted to Euro movers. Remaining TURN2→3 red is a Euro Soldier goal choice, see ai_port_plan.md T1.9.

Durable notes for `golden_mapgen_seed100` + `golden_ai_turns` (VR_SEED=100).
Companion status: [ai_transcription.md](ai_transcription.md),
[`original_sources_annotated/ai/move_scoring.md`](../original_sources_annotated/ai/move_scoring.md).

## Root cause candidate for the peel dependency (2026-08-13)

Investigated `FUN_521d_20e6` (move scoring — the formula behind every peel
in this file) for disassembly corruption, on the theory that a formula
needing 126 hand-added per-tile "peels" (13 init + 113 mid) to match golden
might be reading from corrupted bytes rather than encoding a genuine game
rule. It was worse than corrupted — **it failed to decompile at all** in
the canonical export (`Unable to decompile 'FUN_521d_20e6' — process:
timeout`). Every existing doc/port for this formula (`move_scoring.md`,
`quiet_brave_scoring.c`) was necessarily written from the raw `.asm`
listing alone, never cross-checked against working C.

Re-disassembled via the overlay-addressing project
(`tools/address_mapping.csv` → `OVL14_L0000:20e6`, see
`docs/rtlink_decode_v2_gap.md`): decompiled cleanly in 27 seconds (vs.
never finishing before), 2219 lines, **zero warnings** — matches the
existing ~2170-line ASM-derived estimate closely, so this isn't a severe
desync like `4528`/`5b66` were; it's a case the decompiler's
control-flow analysis simply couldn't resolve without correct addressing.

**Found a real, confirmed gap, not corruption-driven wrongness in what's
already ported:** the quiet Brave base/terrain/facing formula in
`quiet_brave_scoring.c` is correct *for the branch it covers* — but that's
only one of two outer branches in the real DOS code. The other branch
(unit **has** been seen by some Euro nation — a per-nation bitmask on the
unit record, `unit+0x3147` high nibble, bit `0x10 << nation_id`, same
convention as the fog-of-war visibility bit `save_format_map.md` already
documents for tiles/colonies) uses **`RNG(1,5)` and a structurally
different formula** (add a scaled table lookup, or nothing at all) —
entirely unimplemented in the current port, which always uses the
"unseen" `RNG(1,3)` formula regardless of visibility state. Full detail
and exact branch structure: `quiet_brave_scoring.c`'s new header comment.

**Does this explain the very-first-move peels specifically?** Not fully
confirmed — all Braves are genuinely unseen at spawn, so the missing
branch shouldn't fire on turn 0/1 by itself, and the spot-checked pieces
of the "unseen" formula (terrain +1/-table, facing quadratic penalty) do
match what's already ported. **It does cleanly explain the *shape* of the
existing peel counts** — 13 init vs. 113 mid-turn, ~9x more — since every
turn some Braves become newly visible to Euro units as they explore, and
each one's scoring should switch formulas and doesn't. Worth building
(not done this pass) before writing off the remaining init-peel mismatch
as something else: the "some contact/diplomacy condition" gate inside the
RNG(1,5) branch isn't traced yet either, and until that branch exists at
all, it's not possible to tell which peels it would actually eliminate.

### Gate traced + implemented, verified against goldens (2026-08-13)

Traced the RNG(1,5) branch's gate (`FUN_1000_89d0`/`FUN_1000_88cc`, both
resolved past another placeholder-thunk hop) and implemented it in
`ai_native_pick_dir_asm` (`src/core/ai.c`): unit-seen-by-any-Euro-nation
now branches to `RNG(1,5)` + `map_dos_terr_found_score_byte`-scaled term
(unless dest holds a same-nation unit), approximated via
`map_tile_seen_by(map, x, y, *)` at the unit's own tile (documented in
the code as an approximation — the DOS `unit+0x3147` field itself isn't
independently confirmed as a mirror of the tile fog plane, just very
likely).

Verified: `golden_mapgen_seed100` and `golden_ai_turns` (peels on, the
default/shipped config) stay green — no regression. Full `ctest`: 42/43,
same pre-existing unrelated failure (`unit_ai_euro_expand`, Stockade
labor-priority test) as before this change.

**`AI_NO_BRAVE_PEELS=1` diagnostic (measures the true formula gap) is
unchanged by this fix**: `golden_mapgen_seed100` still reports **13**
missing init units, `golden_ai_turns` still fails at TURN1→2 with **18**
`missing unit:` lines, identical before (branch stashed out) and after
(branch applied). Confirms the turn-0/1 prediction above — all Braves are
genuinely unseen at spawn and through the earliest turns in this seed, so
the new branch has nothing to fire on yet in the exact spots the
diagnostic checks. The branch is real (traced from clean, non-corrupted
disassembly, backed by already-verified table data) and now in the
running default config, but **does not yet measurably shrink the 126
peel count** — the peels it should eventually retire are later-turn,
once explorer contact starts flipping visibility, not caught by these
two turn-1-scoped goldens. Left peels in place; no basis yet to remove
any.

| Gate | State |
|------|--------|
| Init pick (default) | Quiet ASM + stay LCG + **13** peels — **green** |
| Mid-turn pick (default) | Quiet ASM + stay LCG + **113** mid peels + **2** spent residuals — **green** |
| Multi-step / Inca tw | Cleared (phase 13; river-first peels) |
| Coarse fog explore index | **Fixed** — DOS `(y>>2)+(x>>2)*18` (was swapped); tribe `/5` unchanged |
| Spent-only Sioux/Apache | **Post-ADD / after-`465b` writer**. Chrome helpers do **not** write `0x3149` |
| Annotation | `ai/brave_spent_callgraph.md` — `14fe` act, post-ADD → `FUN_1427_*`, 3149 table |
| Spent port | **Parked** — dump-free + static xref (incl. foreign-only `465b:01ce`) found no T1-safe rule; keep `k_quiet_brave_t2` |
| Hang EXEs | **Out of scope** this pass (policy); `VR_B465X` remains named last resort |
| Force empiricism | `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0` — emp residual set; **TURN2→3 currently red on HEAD** (Arawak xy; pre-existing, not this pass) |
| Skip peels (audit) | `AI_NO_BRAVE_PEELS=1` — init/mid dir peels off (goldens fail; measures quiet gap) |
| `golden_mapgen_seed100` / `golden_ai_turns` | GREEN |

## Dump-free scoring pass (2026-08-12)

`AI_LCG_AUDIT=1` score dumps on all **13** init peels (`dump_miss` extended):

- Every peel runs with `last_dir=0` (spawn facing).
- Annotated quiet terms (`base` / `terr` / `gate` / `face` / `fog8` / `fogm2`) at matched LCG **do not** pick golden dirs for these tiles; empiricism still matches golden.
- After tribe place the coarse plane is largely marked via `/5` cells → explore `+8` rarely fires on init (fog8=0 on peel dumps). Axis fix is still required for ASM fidelity / mid-game.
- **Do not** paste empiricism additives (`base 200`, home/`−0x28`, mild facing) into quiet. Peels stay until a named quiet term (or input) closes the gap.

Mid peel triage (`AI_STEP_AUDIT` + table comments):

| Class | Count | Notes |
|-------|------:|-------|
| Scoring holdouts | 105 | Quiet formula ≠ golden at matched LCG |
| River / multi-step first picks | 6 | Cost=1 then pulse continues |
| Cascade / mis-key | 2 | `(39,20)` NE; `(27,34)` NE |
| Spent-only overlays | 2 | t2 Apache/Sioux — not dir peels |

## Quiet mid-turn inventory (phase 13 + triage)

| Class | Count | Rows | Notes |
|-------|------:|------|-------|
| Dir peels (all mid) | 113 | `k_mid_peels` | See triage above |
| Multi-step residual overlays | 0 | — | River-first peels; pulse already loops |
| Mis-keyed overlays (retired) | 0 | was t3/t6 | Wrong unit’s end snapped onto neighbor |
| Spent-only (XY match) | 2 | t2 Apache; t2 Sioux | Post-ADD; keep overlays |

Quiet residual table: **2 rows** (both t2 spent-only).

### Phase 13 classification (AI_STEP_AUDIT)

| Row | Pulse without fix | Root cause | Fix |
|-----|-------------------|------------|-----|
| t1 Inca `(7,33)→(8,32)` tw2/mv7 | One NE cost6, tw1/mv6 | Peel collapsed river path | Peel E then N: cost1+6 |
| t1 Sioux `(48,39)→(49,42)` tw3/mv8 | One E cost9 | Need river S first | Peel S; quiet continues S/SE |
| t2 Arawak `(19,37)→(17,38)` tw2 | One N cost9 | Need river W first | Peel W then SW (+ cascade peel) |
| t3 `(38,20)→(40,19)` | Mis-keyed | Real: `(39,20)→(40,19)`, `(38,20)→(39,19)` | Fix `(39,20)` peel N→NE |
| t4 `(33,50)→(33,52)` tw2/mv7 | One N | Need river S | Peel S twice |
| t6 `(28,35)→(28,33)` | Mis-keyed | Real: `(27,34)→(28,33)`, `(28,35)→(27,35)` | Fix `(27,34)` peel S→NE |
| t2 Apache/Sioux spent | XY ok, spent 6/9 vs 3 | post-ADD `0x3149` writer | Residual + park |

`097a` does **not** allow a further act after `spent >= max_mp`. Multi-step is
`cost=1` then another pick while `spent < 3`.

```bash
./build/golden_ai_turns
AI_EMPIRICISM=1 ./build/golden_ai_turns
AI_STEP_AUDIT=1 ./build/golden_ai_turns   # per-step paths
AI_LCG_AUDIT=1 ./build/golden_mapgen_seed100
AI_NO_BRAVE_PEELS=1 ./build/golden_mapgen_seed100  # expect 13 missing units
./build/probe_sioux_spent                # T1/T2 cost-head + neighborhood oracle
```

## Spent-only — dump-free + static (phase 17 / 2026-08-12)

TURN2 pulse-start contrast (`tools/probe_sioux_spent`):

| Step | Golden | Linux head | FROM l2 | DEST tribe | Notes |
|------|-------:|-----------:|---------|------------|-------|
| `(49,41)→(49,40)` T1 | 9 | 9 | `01` | no | Agree — must stay 9 |
| `(49,40)→(49,39)` T2 | 3 | 9 | `01` | no | Holdout |
| `(45,52)→(46,53)` T2 | 3 | 6 | `01` | no | Holdout |
| `(47,46)→(48,46)` T2 | 9 | 9 | `01` | no | Control — same presence shape, stays 9 |

Same cost-head shape as the agreeing T1 row (presence on FROM, no tribe/FA/river
pair on DEST). From-presence caps break T1 spent=9 — rejected.

### Existing dumps (re-parsed, no new hangs)

| Dump | Finding |
|------|---------|
| `dump_b465r3` | Sioux ADD **AL=9** (= Linux). Apache already at `(46,53)` spent=3 |
| `dump_b465f3` | Force-max **not entered**; Sioux ends `(49,39)` spent=3. Distrust Apache path on F |
| `dump_vrb465x2` | Broken X: Sioux `(49,40)` **spent=9** without XY commit ⇒ writer **after** ADD chrome / after `465b` return |

### Static xref addendum

`465b:01ce` early `0934` when remaining MP < 3 requires **`foreign_tile` (`bVar4`)** —
quiet Apache/Sioux holdouts are friendly land. Not a T2 writer.

Prime remaining suspect: **conditional `0934`/`155e` after `465b` returns** (or an
unlabeled thunk). Call-site needs hang X (parked).

**Keep** `k_quiet_brave_t2` overlays. **Do not invent Sioux cost-head caps.**

## Quiet ASM init inventory (phase 10)

Pulse `nexts` match empiricism when stay burn is applied after each ASM pick.
At matched RNG, quiet formula still disagrees with golden on **13** Braves
(scoring — not cascade). Empiricism dirs match golden.

| Nation | Spawn | ASM dir | Golden dir | Dest golden |
|--------|-------|---------|------------|-------------|
| 4 | (11,30) | 7 | 1 | (12,29) |
| 4 | (6,34) | 0 | 1 | (7,33) |
| 6 | (48,4) | 1 | 5 | (47,5) |
| 6 | (25,7) | 0 | 7 | (24,6) |
| 7 | (46,56) | 0 | 2 | (47,56) |
| 8 | (13,48) | 7 | 5 | (12,49) |
| 8 | (17,33) | 2 | 3 | (18,34) |
| 8 | (9,43) | 0 | 7 | (8,42) |
| 9 | (33,54) | 0 | 7 | (32,53) |
| 9 | (30,50) | 0 | 5 | (29,51) |
| 10 | (48,42) | 7 | 1 | (49,41) |
| 10 | (47,39) | 0 | 2 | (48,39) |
| 11 | (32,31) | 0 | 5 | (31,32) |

## Coarse fog (phase 9 + axis fix)

DOS plane `DS:0x9faa` (size `0x10e`):

| Use | Index |
|-----|-------|
| Explore `+8` | `(y>>2)+(x>>2)*18` — ASM `521d:56d8` (`BX=y>>2`, `SI=(x>>2)*18`) |
| Tribe spacing | `(y/5)+(x/5)*18` |

Linux `s_ai_coarse_fog` mirrors this (explore axes were swapped prior to 2026-08-12).

## Empiricism vs DOS quiet

Quiet ASM: `range(1,3)`, river/fa `+1` else `−2f76`, gated facing/fog, **+1 LCG
stay-shaped burn** per pick (stream sync).  
Empiricism: base 200, +4/−6/+3, home, −0x28, +5, stay — still the better match
for several tiles (hence peels until quiet terms catch up).
