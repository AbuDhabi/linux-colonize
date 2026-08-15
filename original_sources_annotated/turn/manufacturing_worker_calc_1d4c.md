# `FUN_15eb_1d4c` — per-worker manufacturing/production value (deep peel)

Deep annotation per `original_sources_annotated/README.md` Layer D. Covers the
function `docs/building_production.md` cites as the "DOS manufacturing
composer" and had flagged as blocked by decompiler corruption.

**2026-08-15: corruption resolved.** There was no real file/data corruption.
The `.asm` text export's recursive-descent disassembler never followed the
function's own indirect jump table (`switchD` at `15eb:1f34`, table data at
`15eb:1f44..1f55`) because the table is inline data sitting *after* the `JMP`
instruction — a completely ordinary, resolvable case, not damage. The exporter
just dumped that stretch (`15eb:1e4f`..`15eb:1f33`, ~230 bytes) as raw
undecoded bytes (`?? XXh` rows) instead of instructions.

Fixed by hand: extracted the raw bytes straight from the `??` rows (which
already give the correct byte value per row) plus the interleaved clean rows
around them, reassembled a contiguous blob from `15eb:1d4c` (the function's
`ENTER`) through the shared epilogue, and ran it through `ndisasm -b 16 -o
0x1d4c`. The result's addresses land exactly on the tool's own labels
(`LAB_15eb_1dba`, `LAB_15eb_1dd7`, `LAB_15eb_1ddc`, …) confirming byte-perfect
reconstruction. Repro: see the extraction script referenced at the bottom.

**The `.c` pseudocode export for this symbol is unreliable in both copies —
do not use it.** This function is exported *twice* under two different names
using two different addressing conventions for the exact same bytes:
`FUN_15eb_1d4c` (segment:offset, in `viceroy_unpacked_2.*`) and
`FUN_0000_7bfc` (linear/wrapped: `0x15eb*0x10 + 0x1d4c = 0x17bfc`, truncated
to 16 bits `= 0x7bfc`, in `viceroy_overlays.*`). Confirmed identical: same
signature, same call-site shape (`FUN_..._7bfc(local_1e,&local_28)` /
`FUN_15eb_1d4c(local_1e,&local_28)`), same three prologue sub-calls under
each file's own naming. **Both files'** `.c` pseudocode independently render
this function's `switch` cases `0x9`/`0xb`/`0xc` as unrelated sound/timer
driver code (`FUN_1b9e_000a`/`FUN_0000_b9ea`, `FUN_1c0c_0006`/`FUN_0000_c0c6`,
raw pokes at ports `0x4a`/`0x2d54`) that doesn't match any code actually
reachable from the real jump table (verified below, byte-exact from the
table's own data bytes). Since two independent exports of identical bytes
produce the identical wrong case bodies, this looks like a deterministic bug
in whatever switch-statement-recovery step both exports share — not random
noise, and not evidence the real jump table (which I read directly as data,
not through that recovery step) is wrong. **The raw `.asm` is trustworthy**
once its own gap (above) is manually resolved; the `.c` for this symbol,
in either file, is not.

**The 2026-08-13 doc pass's `0x7bfc`-`0x7e21` citation was correct** — it's
this function's `FUN_0000_7bfc` alias in `viceroy_overlays.*`, verified above
by the linear-address arithmetic (and the tail address checks out too:
`0x15eb0 + 0x1f71 → 0x7e21`, matching this function's real end almost to the
byte). An earlier pass here had flagged that citation as unverifiable and
retracted it; that retraction was wrong — the address was real, just phrased
under a naming convention (`FUN_0000_*`) this file hadn't cross-checked
against the sibling export. What *was* off in the 2026-08-13 text: it says
"497 bytes" but `0x7e21 - 0x7bfc = 549` bytes — a same-pass arithmetic slip,
not an address problem. Real extent: `15eb:1d4c` (`ENTER`) through
`15eb:1f71`-ish (`RETF`), **549 bytes**, immediately followed by a separate
function (`FUN_15eb_1f72` / `FUN_0000_7e22` — same identical-alias pattern,
confirmed: fresh `;FUNCTION` banner + local-var table starts right after the
`RETF` in both exports).

## Signature and call sites

```c
uint __cdecl16far FUN_15eb_1d4c(int param_1 /*worker index*/, undefined2 *param_2 /*out: tag*/, int param_3);
```

Two live call sites in `viceroy_unpacked_2.c` (a third, line 32489, is a bare
`FUN_15eb_1d4c();` with no visible args — likely another `.c`-export
omission, not investigated this pass):

```c
// line 11299 (best-preserved decompile — this one's C IS trustworthy)
for (local_1e = 0; local_1e < *(char *)(*(int *)0x8542 + 0x1f); local_1e++) {
  local_c = FUN_15eb_1d4c(local_1e, &local_28);
  if (-1 < local_28) {
    *(int*)(local_28 * 2 - 0x7238) += local_c;   // sum into a 20-slot totals table
  }
}
```

So: for each of `byte[colony_ptr+0x1f]` workers (loop bound == the same field
the function itself reads at `[bx+0x1f]` — almost certainly **colonist
count for the building/colony**, `bx = word[0x8542]` = current
building/colony pointer, a DS global), call once with the worker's index,
and accumulate the returned amount into `totals[out_tag]`. This is exactly
the shape of "per-worker manufacturing output, bucketed by cargo/output
type" — consistent with `building_production.md`'s framing.

## Confirmed formula (byte-exact against the asm, no longer provisional)

```
call1 = FUN_15eb_0e18(param_1)   ; = colonist profession (catalog: "Colonist profession: slot+0x20…")
call2 = FUN_15eb_0e52(param_1)   ; = colonist workplace/job (catalog: "slot+0x40…")
call3 = FUN_15eb_0274()          ; = colony SoL% (catalog: "Colony SoL % from bells/pop (0..100; +20 FF bonus)")

CX = 100 - call3                  ; = Tory% (100 - SoL%)
AX = (byte[bx+0x1f] * CX + 50) / 100     ; rounding percentage scale of the colonist-count field by Tory%
```

then the class-gate:

```
if (byte[bx+0x1a] < 4) {
  BX2 = byte[bx+0x1a] * 0x34            ; 52-byte stride table row
  if (byte[BX2+0x543f] == 0) {
    v = 10 - byte[0x53a6]
  } else {
    v = 10                               ; default
  }
} else {
  v = 10
}
```

`byte[bx+0x1f]`, `byte[bx+0x1a]`, `byte[0x53a6]`, and the `0x543f` table are
**still not semantically decoded** — see Open questions below. The
arithmetic shape itself (confirmed byte-exact) is not in dispute any more.

## The switch: selector is the worker's `@JOB` profession (9-17)

`call1` (`FUN_15eb_0e18`'s return, i.e. the worker's profession code) is
reloaded right before the dispatch and used as the switch selector:

```
AX = call1_result           ; profession code
AX -= 9
if (AX > 8) goto default/epilogue (15eb:1f56)
BX = AX * 2
goto word[CS:BX + 0x1f44]   ; 9-entry jump table
```

Table contents, read straight off the data bytes at `15eb:1f44`-`15eb:1f55`
(`D8 1E, D8 1E, D8 1E, D8 1E, 50 1E, D8 1E, D8 1E, 82 1E, 18 1F`, little-endian
words):

| profession (`@JOB`) | selector | target | body |
|---|---:|---|---|
| 9 Distiller | 0 | `15eb:1ed8` | shared craft body |
| 10 Tobacconist | 1 | `15eb:1ed8` | shared craft body |
| 11 Weaver | 2 | `15eb:1ed8` | shared craft body |
| 12 Fur Trader | 3 | `15eb:1ed8` | shared craft body |
| **13 Carpenter** | 4 | `15eb:1e50` | distinct body |
| 14 Blacksmith | 5 | `15eb:1ed8` | shared craft body |
| 15 Gunsmith | 6 | `15eb:1ed8` | shared craft body |
| **16 Preacher** | 7 | `15eb:1e82` | distinct body |
| **17 Statesman** | 8 | `15eb:1f18` | distinct body |

This is an exact match, profession-for-profession, to
`building_production.md`'s own "Skills chart" split: the six raw-good
craftsmen (Distiller/Tobacconist/Weaver/Fur Trader/Blacksmith/Gunsmith) share
one generic formula, while Carpenter (hammers, lumber-only input), Preacher
(crosses, colony passive + per-worker), and Statesman (bells, colony passive +
per-worker) are each special-cased — precisely how the port already splits
these in `colony_production.c` (`colony_prod_hammers_worker`,
`colony_prod_crosses_worker`, `colony_prod_bells_worker` vs.
`colony_prod_manufacturing_output`). Strong evidence the port's *shape* is
right; this doesn't yet confirm the port's exact *rates*.

### Statesman body (`15eb:1f18`) — skilled ×2 confirmed exact

```
v = local_12 + local_e            ; base
tag = 0x12
if (skill_matches_flag != 0) v <<= 1   ; ×2
```

`skill_matches_flag` (raw `[bp-0x1c]`) — matches `building_production.md`
"Elder Statesman ×2". The port's `colony_prod_bells_worker` already does
exactly this (`base *= 2` on `COLONIZE_PROF_STATESMAN` match).

### Preacher body (`15eb:1e82`) and Carpenter body (`15eb:1e50`)

Same shape as Statesman's: base value, a far call (`0x38e`, arg `0x26` for
Preacher / `0x24` for Carpenter — not yet identified), then `if (result != 0)
v <<= 1`. Consistent with "Firebrand Preacher ×2" and implied Master
Carpenter ×2 already documented.

### `local_12` — the class-scale tag, fully traced and confirmed

Set right before the switch by a 3-way branch on `call2` (`FUN_15eb_0e52`,
catalog: "colonist workplace/job", but the actual values compared are
`0x19`/`0x1a`/`0x1b` = **25/26/27 — exactly `COLONIZE_PROF_INDENTURED` /
`_CRIMINAL` / `_CONVERT`**, i.e. this reads the colonist's *class*, not their
workplace; the catalog one-liner is imprecise here):

```
class == 0x19 (Indentured)          -> local_12 = 2
class == 0x1a or 0x1b (Crim/Convert)-> local_12 = 1
anything else (free colonist, …)    -> local_12 = 3
```

`local_12` is literally the **numerator over 3** of the port's own
`class_factor` (criminal/convert → `1/3`, indentured → `2/3`, free → `3/3`).
Exact match, no gap.

### Shared craft body (`15eb:1ed8`) — now fully confirmed against the port's 3/6/9 table

```
v = local_12 + local_e                       ; class tag + SoL/Tory term (below)
tier = FUN_15eb_039e(chain_id)               ; catalog: "count owned buildings along
                                                parent chain" — i.e. 1/2/3 for
                                                house/shop/factory owned so far
if (tier > 1) v += local_12                  ; shop: += class tag again
if (tier > 2) v += v >> 1                    ; factory: whole thing ×1.5
[shared tail, see below] if (skill_match) v <<= 1   ; ×2
```

With `local_e ≈ 0` (no active SoL/Tory swing) and `local_12 = 3` (free
colonist): house `v=3`, shop `v=3+3=6`, factory `v=(3+3)*1.5=9`. **Exactly**
the port's `colony_prod_tier_free_output` (3/6/9). For criminal/convert
(`local_12=1`): 1, 2, 3 — matches `colony_prod_tier_free_output(tier)/3`
exactly at every tier. For indentured (`local_12=2`): 2, 4, 6 — matches
`*2/3` exactly at every tier. **The port's tier-rate table and class-scale
formula are now DOS-confirmed, not provisional**, for the six shared craft
professions (Distiller/Tobacconist/Weaver/Fur Trader/Blacksmith/Gunsmith).

The `×2` skill-match doubling is real too, found via a compiler tail-merge:
the craft body sets its own `CMP [bp-0x1c],0` (the skill-match flag — see
below) then `JMP 0x1e77`, landing mid-Carpenter-body on a `JNZ` that was
already there for Carpenter's own doubling, reusing one shared "if flag: `SHL
v,1`" tail instead of duplicating it. Confirms the port's
`craft_skill_matches(...) → out *= 2`, applied *after* the tier math — same
order the port uses.

### `local_e` — the SoL/Tory production modifier, fully traced and confirmed (same shape as the port, folded in differently)

```
v_08 = (byte[bx+0x1f] * (100 - SoL%) + 50) / 100    ; = port's `tories` formula exactly
local_e = -(v_08 / class_gate_divisor)
        + (colony_flags bit4 ? 1 : 0)                ; matches COLONIZE_COLONY_FLAG_SOL_50
        + (colony_flags bit2 ? 1 : 0)                ; matches COLONIZE_COLONY_FLAG_SOL_100
```

`class_gate_divisor` is `10`, or `10 - byte[0x53a6]` when a per-nation
`0x543f`-table row reads 0 — matching the port's `thresh = 10 - difficulty`
(human-controlled) vs. fixed `10` (AI) split exactly; `byte[0x53a6]` is
almost certainly the difficulty setting. **This is byte-for-byte the same
formula as `colony_prod_sol_bonus()`** — confirmed, not provisional.

**Fixed 2026-08-15.** DOS folds `local_e` into `v` *before* the tier-bonus
arithmetic and the skill-doubling (so at shop tier `local_e` is *not*
re-added like `local_12` is, but at factory tier the *whole* `v` including
`local_e` gets ×1.5'd, and a skilled match doubles the whole thing including
`local_e` too). The port used to add `sol_bonus` as a flat, unscaled,
**positive-only** term after tier+skill math
(`colony_craft_pair_totals`: `out = colony_prod_manufacturing_output(...); if
(sol>0) out += sol;` — the `sol>0` guard also silently dropped every Tory
*penalty*, since `colony_prod_sol_bonus` can be negative). Both fixed:
`colony_prod_manufacturing_output` now takes `sol_bonus` as a signed
parameter and folds it in exactly where DOS does — house/shop tier were
already numerically identical to the old code when unskilled (so no change
there), the real deltas were factory-tier rounding and any case where skill
matches or the SoL term is negative. Regression:
`test_turn.c` "FUN_15eb_1d4c: sol_bonus folds in..." block (factory
unskilled/skilled sol-fold values, plus Tory-penalty clamp-to-0 and
Tory-penalty-that-doesn't-clamp cases, all hand-derived from the formula
above and confirmed against the actual code on first run).

### Carpenter/Preacher/Statesman doubling — a different, colony-wide mechanism, converges to the same numbers in normal play

Carpenter's own body (`15eb:1e50`) doesn't use the `local_12`+tier-lookup
path at all: `v = (skill_match ? 6 : local_12) + local_e`, then **`v <<= 1` if
`FUN_15eb_038e(0x24)` is true** — catalog: "test building bit for active
colony **index**" (colony-wide, not per-worker) — i.e. this doubling is gated
by *whether the colony owns a specific building* (almost certainly Lumber
Mill for arg `0x24`; Preacher's body uses the same `FUN_15eb_038e` shape with
arg `0x26`, almost certainly Cathedral), not by whether *this worker's*
assigned building is the upgraded one. Statesman's body (`15eb:1f18`) instead
doubles on `[bp-0x1c]` (skill match) directly, no building check — matches
`colony_prod_bells_worker`'s "Elder Statesman ×2" exactly, no divergence
there.

For Carpenter/Preacher this is a **different mechanism** than the port's
(port ties the tier/rate to the worker's own assigned building name via
`colony_prod_building_tier`; DOS ties it to a colony-wide "owns building X"
flag plus a separate worker-level skill-match flag for the *baseline*). They
produce the **same numbers** in the normal case (a colony has either
Carpenter's Shop *or* Lumber Mill, never both, so "worker's building is the
upgraded one" and "colony owns the upgraded building" coincide) — verified by
hand for all four skilled/unskilled × has/hasn't-Lumber-Mill combinations,
all match the port's `tier_free_output(tier) × class_scale × skill_×2`
output exactly. They'd only disagree in the edge case of a colony somehow
owning both tiers simultaneously with a worker parked in the lower one — not
reachable through normal construction (upgrades replace, not stack), so
**no port change made or needed here**, just documented.

## Open questions (next layer — normal RE backlog, not a blocker)

- **Bells fixed 2026-08-15 (two passes).** First pass fixed the `sol_b > 0`
  sign-drop bug shared with hammers/crosses. Second pass fixed it properly:
  `colony_prod_bells_worker` now takes `sol_bonus`, folds it into the class
  tag *before* the Statesman ×2 (`(tag+sol)*2` when skilled), and
  `colony_prod_colony_bells_ff` threads it per-worker — the old external
  "count workers, multiply, add after `colony_prod_colony_bells_ff` returns"
  mechanism in `turn.c`/`colony_preview.c` is gone for bells. Statesman's DOS
  body has no colony-wide building-flag complication (unlike Carpenter/
  Preacher, below), so this one is a clean, direct match: `v = local_12 +
  local_e`, then `if (skill) v <<= 1` — exactly what the new
  `colony_prod_bells_worker` does.

- **Hammers/crosses: fixed 2026-08-15 (three passes).** First pass fixed the
  `sol_b > 0` sign-drop. Second pass identified — but deliberately didn't
  rush — that copying the bells fix wouldn't transfer cleanly, because
  Carpenter/Preacher's DOS bodies don't work like Statesman's:

  ```
  v = (skill_match ? 6 : local_12) + local_e     ; skill picks the BASE, not a ×2 flag
  if (FUN_15eb_038e(building_index)) v <<= 1     ; COLONY-WIDE "owns the upgraded
                                                    building" flag — Lumber Mill (0x24)
                                                    for Carpenter, Cathedral (0x26) for
                                                    Preacher — not per-worker skill match
  ```

  the port's old `church_rate=3`/`cathedral_rate=6` (and the equivalent
  house/shop `colony_prod_tier_free_output` lookup for hammers) captured that
  colony-wide doubling *implicitly* via a bigger rate constant, which is only
  algebraically identical to DOS's explicit `×2` when nothing's added in
  between (`scale_by_class(p, 6) + sol` ≠ `(scale_by_class(p, 3) + sol) × 2`
  once `sol != 0`).

  Third pass implemented the fix properly: `colony_prod_crosses_worker`/
  `colony_prod_hammers_worker` now take `(sol_bonus, colony_has_upgrade)` and
  compute the exact shape above — `base = skilled ? 6 : class_tag`, `+
  sol_bonus`, then doubled only if the caller-supplied colony-wide flag says
  so (computed once per colony via the existing `colony_prod_building_built`
  helper, in `colony_prod_colony_crosses_ff`/`colony_prod_colony_hammers`,
  not derived from which building the worker happens to occupy).
  `colony_prod_colony_hammers` also gained a `sol_bonus` parameter and now
  tracks two totals internally: `lumber_use` (sol-free — consumption
  shouldn't scale with SoL, same reasoning as manufacturing's input side) and
  the returned hammer count (sol-adjusted). The old external "count matching
  workers, multiply, add after" mechanism in `turn.c`/`colony_preview.c` is
  gone for both. Regression: 4 direct unit checks in `test_turn.c` targeting
  exactly the cases that differ from the pre-fix numbers (unskilled+upgrade,
  skilled+upgrade, both crosses and hammers) — all matched the hand-derived
  values on first run.

  Hammers/crosses/bells are now all DOS-confirmed for the sol-fold shape.

  **Building-index args confirmed** (2026-08-15, same pass): counted
  `COLONIZE/NAMES.TXT`'s `@BUILDING` rows 0-based in file order — index
  `0x24` (36) is `Lumber Mill`, index `0x26` (38) is `Cathedral`, exactly
  matching the Carpenter/Preacher bodies' `FUN_15eb_038e` args inferred from
  context above. No longer just a converges-numerically hypothesis.
  (Adjacent: index `0x25` (37) is `Church` — shows up as a *separate*
  `FUN_15eb_038e(0x25)` check in the bells/crosses nation-aggregate caller
  around `viceroy_unpacked_2.c:11307`, `0x8dea`/`0x8dec` accumulators, a
  different function from `FUN_15eb_1d4c` entirely and not chased further
  this pass — flagged below as a new open item since it looks like where
  DOS's *real* Church/Cathedral passive-cross and Printing-Press/Newspaper
  bell-bonus constants might actually live, which the port currently
  sources from the manual instead of decomp.)
- `byte[bx+0x1a]` / `0x543f` table / `byte[0x53a6]` — strong-hypothesis (per
  above: nation-control-type gate, per-nation table, difficulty setting).
  **2026-08-15: cross-checked against a third independent call site**
  (`FUN_15eb_18ec`'s field-yield SoL zero-out — see
  `docs/terrain_yields.md`), same shape again. Acted on: shipped
  `colony_prod_sol_bonus_field`, a field-yield-only variant that zeroes the
  SoL/Tory mod for AI colonies (matching `18ec`), while the shared
  `colony_prod_sol_bonus` (matching `1d4c`'s divisor-only behavior) stays
  unchanged for manufacturing/bells/crosses/hammers. Still not proven from a
  source that states the table byte's meaning directly — three consistent
  independent observations is strong, not certain.
- The far call at `15eb:1eaf` (`CALL 0x1981:0x0000`, arg = `byte[bx+0x1a]`) —
  crosses into a completely different overlay segment; not investigated.
- `FUN_15eb_0aec` / `FUN_15eb_0434` chain-id resolution feeding
  `FUN_15eb_039e`'s tier count — catalog-level only, not traced byte-level.

## Repro

Byte extraction + `ndisasm` cross-check used to resolve the `.asm` export gap
(no files changed by this — read-only verification):

```
awk lines 16081-16450 of original_sources_decompiled/viceroy_unpacked_2.asm,
regex out the hex column of every row (both clean `MOV …` rows and `?? XXh`
rows carry the raw byte(s) in the same column position), concatenate in file
order, then:
  ndisasm -b 16 -o 0x1d4c <blob>
```

Every address ndisasm reports for the "clean" stretches matches the tool's
own `LAB_15eb_XXXX` labels exactly, which is the cross-check that the
reconstruction is byte-accurate.
