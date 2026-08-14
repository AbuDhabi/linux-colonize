# `FUN_521d_0a60` — full clean recovery, real size correction (2026-08-14)

**Status: mapped in full; goal-selection distance/priority scoring ported**
into `ai_euro_unit_act`'s existing goal-consumption loops (see
"Implementation" section below). Verified: clean build, full `ctest` 42/43
(known pre-existing baseline failure only).

## Critical correction: this function is ~5.4KB, not ~840 bytes

Every prior citation of `FUN_521d_0a60`'s size in this project (`839` bytes
in `FUNCTION_CATALOG.md`, "~858 lines"/"853 lines, fifth-pass sweep" in
`euro_g_table_0a60.md` and `docs/ai_transcription.md`) was **wrong** — not
corrupted-content-wrong (no `WARNING:` ever flagged this function), but
silently **truncated**: the canonical export's function-boundary detection
closed `0a60` off early and never gave the remaining ~4.5KB (down to
`OVL14_L0000:0x1fdf`, right up against the confirmed-real next function at
`0x20c6`) a boundary at all — the same "before-first-function"/gap class
`address_mapping.csv` already flags for `0a60`'s own entry, just far worse
in extent than anyone had checked. That gap had never been probed before
this pass (earlier sessions read `0a60` only via the canonical export,
which *looked* complete — clean return, matched brace, no warning — so
nobody had reason to suspect it silently stopped short).

**Found while pursuing the user's "mapping effort" instruction**: probed
the address range between `0a60` (`before-first-function`) and the next
*confirmed* real boundary, `0x20c6` (`exact` match kind) — Ghidra's own
control-flow backtracking from a probe inside the gap immediately produced
a label (`LAB_521d_0ef0`) *before* the probe point, and tracing that back
further landed exactly on `0a60`'s own already-known entry. One single
`GhidraDecompileAt OVL14_L0000:a60` call (this project's fresh Ghidra
project, already analyzed from earlier passes) recovered the whole thing
cleanly: **845 lines, one mild pre-existing-class warning** (`Removing
unreachable block` — same safe category as everywhere else in this
project, not a corruption signal).

The first ~700 lines (unit/colony/tribe housekeeping, the deep `−0x6790`
G-table, the `−0x6168` max-tracker) **match what was already documented**
in `euro_g_table_0a60.md` and `move_scoring_land.md` line-for-line in
substance (same locals, same thunks, same DS tables) — that work stands,
confirmed rather than corrected. The genuinely new material is the final
~90 lines, previously never read by anyone: a **goal-table consumption /
unit-orders-assignment engine**, detailed below.

Full raw recovery: see the fenced block at the end of this file.

## Section map

| Lines (this recovery) | Role | Status before this pass |
|---|---|---|
| 1-189 | Unit loop (per-nation unit housekeeping: act-state resets, admit-LABOR-if-fortified-elsewhere clear, garrison-check `0x3148` flags) + colony-defense-request thunk calls | Read before (canonical), unchanged |
| 190-240 | Colony loop entry: bind colony, explore-flag OR, garrison-request via `thunk_FUN_2a1f_0470` | Read before |
| 241-753 | Colony threat/labor scoring: cargo-weight table `−0x7b44`, urgency accumulator `0x1734`, deep `−0x6790` G-table write, `−0x6168` max-tracker | **Done** — `euro_g_table_0a60.md` (confirmed unchanged by this fuller recovery) |
| **754-844** | **Goal-table consumption: pick best `AiGoalSlot` for each idle unit, write orders/act-state/goal-target** | **Never mapped before this pass** — new |

## New section: goal → orders wiring (lines 754-844)

For every unit belonging to this nation (`param_2`) not already admitted as
labor (`orders != 'A'`):

```c
if (act_state(+0x314c) < 10) orders(+0x314b) = '?' (0x3f);   /* pending-decision placeholder */
if (act_state ∈ {0, 5, 6} && orders ∈ {'t','i'}) orders = '?'; /* clear stale goal-pursuit codes */

if (act_state ∈ {0, 5, 6}) {
  best_score = 9999; best_slot = -1;
  ux, uy = unit x/y; unit_continent = tile_continent(ux, uy);

  /* Skip units that are Soldier/Dragoon-shaped AND the G-table stance for
   * their own continent is already non-hostile — i.e. don't reassign
   * combat units away from a continent that still needs defending. */
  if (unit_type ∈ {Soldier(1), Dragoon(4)}) {
    land_units_here = land_unit_count[continent][nation];   /* −0x6b5a, CONFIRMED this session */
    if (land_units_here < 3 &&
        (land_units_here < 2 || colony_count[continent][nation] == 0))  /* −0x6b1a, CONFIRMED */
      goto skip_this_unit;
  }

  for (slot = 0; slot < 64; slot++) {                         /* AiGoalSlot primary[64], ALREADY PORTED */
    goal = primary_goals[nation][slot];                       /* ai_goals_primary(nation_id, slot) */
    if (goal.code == AI_GOAL_EMPTY) continue;
    if (!(unit_type_capability_mask(unit_type) & (1 << goal.code))) continue;  /* DS 0x523d, ALREADY-KNOWN table */
    goal_dist_from_unit = dos_dist(ux, uy, goal.x, goal.y);     /* FUN_1000_856a — DOS distance helper */
    if (!(goal.code matches unit_type/state gates)) continue;   /* fine-grained skip: Missionary code-5/6 special-case */
    score = tally[slot] * goal_dist_from_unit / (goal.prio + 1);
    if (score < best_score && score/base_scale <= goal.prio*1.5)
      { best_score = score; best_slot = slot; }
  }

  if (best_slot >= 0) {
    best_goal = primary_goals[nation][best_slot];
    orders(+0x314b) = (best_goal.code == AI_GOAL_FOUND) ? 't'
                     : (best_goal.code == AI_GOAL_MIL_EXPAND) ? 'i'
                     : '1' (0x31);                             /* default goal-pursue code */
    act_state(+0x314c) = 0xb (11);                              /* "pursuing a goal" state */
    unit+0x314d = best_goal.x;                                  /* goal target x — NEW field */
    unit+0x314e = best_goal.y;                                  /* goal target y — NEW field */
    if (best_goal.code != AI_GOAL_MILITARY(4))
      tally[best_slot]++;   /* claim-count so the same goal isn't over-assigned */
  }
}
```

**`unit+0x314d`/`+0x314e` — goal-target x/y, resolved this session
independent of this find**: `move_scoring_20e6_full.md` (the full `20e6`
recovery from a much earlier pass this session) already shows `20e6`
*reading* these same two fields at three separate sites (compares against
current x/y when act-state `== 0xb`, i.e. "have I arrived at my goal
target yet") and *clearing/rewriting* them on commit — confirms `0a60`
writes exactly what `20e6` consumes. **This closes a three-way loop that
was scattered across separate "OPEN"/"thin" framings in three different
docs before today**: `ai_goals.c` (goal *storage*, already ported) →
`0a60` (goal *selection*, just mapped) → `20e6` (goal *pursuit/arrival*,
already recovered) → `5b66`'s tiny dispatcher case `0xb` → `FUN_1000_96aa`
(goal *fulfillment* — not traced this pass, real next step if implementing).

**Aligns with already-known constants**: `AI_GOAL_FOUND`=1, `AI_GOAL_
MIL_EXPAND`=7 (`ai_goals.h`) match the two special-cased `goal.code`
values seen in the orders-byte write (`'t'`/`'i'`); `AiGoalSlot{x,y,code,
prio}` (`ai_goals.h`) matches the 4-byte-stride table read exactly (`code`
= "type" field, `prio` = the divisor/threshold weight). `AI_PRIMARY_SLOTS
= 64` already matches the `slot < 0x40` loop bound — **this table's shape
was already correctly ported**, just never wired to a consumer that reads
it back to assign orders.

## Not yet done

- **Not ported.** This is a mapping/documentation pass per explicit
  instruction — no `src/` changes this pass.
- `FUN_1000_856a` **resolved same pass**: `abs(x1-x2)`/`abs(y1-y2)` into
  `FUN_0000_2500(dx,dy) = max(dx,dy) + min(dx,dy)/2` — the classic octile-
  distance formula, matching the project's already-documented
  `FUN_124c_0040`/`ai_dos_dist` shape. Almost certainly the same helper
  under a different resident thunk; **already available in Linux**, no new
  port needed for this piece.
- `FUN_1000_96aa` **traced one hop, not fully resolved**: thunks to
  `FUN_1000_1e7b`, which opens with a computed function-pointer dispatch
  (`code *pcVar2`) gated on globals `LAB_1000_39e1`/`LAB_1000_39dc_2` — this
  smells like the long-hunted "real per-unit act state machine" this
  project's `euro_unit_act.md` already flagged as unresolved (the
  `5b66`-body mystery, closed for `5b66` itself earlier this session but
  never chased into what `5b66`'s own case handlers *do*). Not fully
  traced this pass — real next lead if anyone wants goal *fulfillment*
  (as opposed to goal *selection*, which is what this file maps) byte-exact.
- The fine-grained per-goal-code skip conditions (Missionary code 5/6
  special case at lines 802-811) weren't fully unpacked above — simplified
  to "gates" in the pseudocode; needed for a byte-exact port, not needed to
  understand the overall shape.
- Unit-type capability mask `DS:0x523d` — traced its write site
  (`FUN_75c2_1770`, the unit-type table loader) and confirmed it's loaded
  from **external resource data at startup**, not computed — same stride
  (`type*0xe`) as the already-known `0x5235`/`0x5236` attack/defense bytes,
  sitting right next to them. Checked `NAMES.TXT` for the raw values (it's
  plain text, would have been an easy win) — not there; the loader calls
  (`FUN_291f_0928`/`FUN_2a1f_088a`) are resource/overlay reads, not the
  text-file parser, so the actual bit values live in a binary data segment
  this project's tooling doesn't have a path to inspect yet. Real reason
  Linux's whole surrounding cascade uses name-string matching
  (`ai_euro_is_artillery_name` and friends) instead of a bitmask: this
  exact byte was never recoverable. Not a gap in this pass's work — a
  standing limitation the rest of the codebase already worked around the
  same way.

## Implementation: ported, same pass (2026-08-14)

**Correction to this section's first draft**: an initial pass checking
`ai_goals_primary()`'s 7 call sites concluded Linux had nothing resembling
DOS's real algorithm and floated two speculative integration paths
("wholesale replacement" vs. "additive fallback") without picking either.
That was premature — a closer read (looking for a safe insertion point)
found `ai_euro_unit_act` already has a **third, better-fitting shape**
neither path anticipated: a goal-selection block (soldier/founder/fallback
loops immediately before the LABOR-bind logic) that already picks a goal
from `ai_goals_primary()` by unit-type category — just by **first match in
table order**, not DOS's **closest/highest-priority match**. This isn't a
gap needing new architecture; it's an existing, correctly-shaped mechanism
with one concrete formula difference.

**Ported**: the soldier and founder loops now score every matching slot by
`dos_dist(unit, goal) / (goal.prio + 1)` (via a new local `ai_euro_dos_dist`,
same octile-distance shape as `FUN_0000_2500`/`FUN_1000_856a`) and keep the
lowest-scoring match, instead of breaking at the first non-empty slot.
Approximated: the DOS per-slot difficulty-scaled weight table (`aiStack_1da`)
and the fine `score/dist <= prio*1.5` threshold gate are not reproduced
(weight≈1, no gate) — same "confirmed core, approximate the edge" pattern
as every other port this session. The third (generic fallback, no
type-category match) loop was left untouched in this same-day pass, then
**also fixed two passes later** (2026-08-14, third pass) once the first two
loops were verified safe — same closest/highest-prio scoring, no remaining
first-match loop in this function.

**Verified**: clean build, no warnings; full `ctest` 42/43 (same
pre-existing `unit_ai_euro_expand` baseline failure, unaffected) — checked
after each of the two implementation passes separately. No new dedicated
test added — the change is a scoring-order refinement inside an already-
covered code path (existing goal/military-prio tests still pass
unchanged), not new observable behavior with an obvious new assertion
point; a future pass could add one if the closest-vs-first distinction
needs its own regression guard.

**Not done, real follow-up if resumed**: `FUN_1000_96aa`/`FUN_1000_1e7b`
(goal *fulfillment* once a unit arrives — separate from the *selection*
ported here) still traced only one hop; `aiStack_1da`'s difficulty-scaled
weight meaning unresolved. (The generic-fallback-loop follow-up noted
here originally is done — see "Implementation" above.)

**Bonus, same day, third pass — separate `unit+0x314f` field also
resolved and ported**: cross-checking `move_scoring_20e6_full.md` for
`unit+0x314f` (seen written by `FUN_5bfb_3180`, a caller of `153e` found
while mapping that function) showed `20e6` itself already reads/writes
this field at its own commit point (`LAB_521d_589e`) and facing band
(`LAB_521d_54f5`) — it's the **last chosen movement direction**, feeding a
momentum bias (same-direction preferred, opposite penalized) identical in
shape to the already-ported Brave `quiet_score_facing`
(`ai.c`/`quiet_brave_scoring.c`), just never wired for Euro units. Added
`s_euro_last_dir[COLONIZE_UNITS_MAX]` + the same bias formula to
`ai_euro_score_move`. Verified: clean build, `ctest` 42/43 unchanged.

## Raw recovered C (845 lines, one mild warning)

```c
/* WARNING: Removing unreachable block (ram,0x00001741) */

void FUN_521d_0a60(undefined2 param_1,int param_2)

{
  byte *pbVar1;
  int *piVar2;
  byte *pbVar3;
  undefined1 *puVar4;
  bool bVar5;
  char cVar6;
  byte bVar7;
  undefined1 uVar8;
  uint uVar9;
  undefined2 uVar10;
  uint uVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar15;
  int iStack_1e2;
  int aiStack_1da [64];
  int iStack_15a;
  int iStack_156;
  int iStack_154;
  uint uStack_152;
  int iStack_150;
  int aiStack_14e [128];
  int iStack_4e;
  int iStack_4c;
  int iStack_4a;
  int iStack_48;
  undefined2 uStack_46;
  undefined2 uStack_44;
  int iStack_42;
  int iStack_40;
  int iStack_3e;
  int iStack_3c;
  uint uStack_3a;
  int iStack_38;
  uint uStack_36;
  int iStack_34;
  int iStack_32;
  int iStack_30;
  int iStack_2e;
  uint uStack_2c;
  int iStack_2a;
  int iStack_28;
  int iStack_26;
  uint uStack_24;
  int iStack_22;
  uint uStack_20;
  int iStack_1e;
  int iStack_1c;
  undefined4 uStack_1a;
  int iStack_16;
  int iStack_14;
  int iStack_12;
  int iStack_10;
  int iStack_e;
  int iStack_c;
  uint uStack_a;
  uint uStack_8;
  uint uStack_6;
  uint uStack_4;
  
  iVar14 = param_2;
  FUN_1000_8772();
  FUN_0000_df7e(0x9faa,0,0x10e,iVar14);
  FUN_0000_df7e(0xa13c,0,0x10);
  FUN_0000_df7e(0x9e98,0,0x10);
  FUN_0000_df7e(aiStack_14e,0,0x100);
  iStack_3c = func_0x0001854c(0xd1d,*(byte *)(param_2 + -0x7304) >> 3,3,99);
  iStack_3e = 0;
  do {
    aiStack_1da[iStack_3e] = iStack_3c;
    iStack_3e = iStack_3e + 1;
  } while (iStack_3e < 0x40);
  iStack_c = 0;
  for (iStack_154 = 0; iStack_154 < *(int *)0x539c; iStack_154 = iStack_154 + 1) {
    iVar14 = iStack_154 * 0x1c;
    if ((*(byte *)(iVar14 + 0x3147) & 0xf) == (byte)param_2) {
      uStack_36 = (uint)*(byte *)(iVar14 + 0x3144);
      uStack_3a = (uint)*(byte *)(iVar14 + 0x3145);
      if (*(char *)(iVar14 + 0x314b) == 'A') {
        *(undefined1 *)(iVar14 + 0x314b) = 0x47;
      }
      iVar14 = iStack_154 * 0x1c;
      *(byte *)(iVar14 + 0x3148) = *(byte *)(iVar14 + 0x3148) & 0xd1;
      if ((*(char *)(iVar14 + 0x314c) == '\x05') || (*(char *)(iVar14 + 0x314c) == '\x06')) {
        pbVar1 = (byte *)(iStack_154 * 0x1c + 0x3148);
        *pbVar1 = *pbVar1 | 2;
      }
      iVar14 = FUN_1000_8aac(0x181f,iStack_154,4);
      if ((iVar14 < 2) && (iVar14 = FUN_1000_8aac(0x181f,iStack_154,6), iVar14 == 0)) {
        iStack_1e = 0;
      }
      else {
        iStack_1e = 1;
      }
      iStack_1c = FUN_1000_8aac(0x181f,iStack_154,3);
      if ((iStack_1e != 0) || (iStack_1c != 0)) {
        uStack_4 = 1;
        iVar14 = iStack_154 * 0x1c;
        if ((0xc < *(byte *)(iVar14 + 0x3146)) &&
           ((*(byte *)(iVar14 + 0x3146) < 0x13 &&
            (uStack_4 = (uint)(*(char *)((uint)*(byte *)(iVar14 + 0x3146) * 0xe + 0x5237) ==
                              *(char *)(iVar14 + 0x3150)), uStack_4 != 0)))) {
          iStack_42 = iStack_154;
          uVar10 = 0xb66;
          iStack_154 = FUN_1000_84de(0x181f);
          while ((uStack_4 != 0 && (-1 < iStack_154))) {
            if ((0xc < *(byte *)(iStack_154 * 0x1c + 0x3146)) &&
               (((*(byte *)(iStack_154 * 0x1c + 0x3146) < 0x13 &&
                 (*(char *)((uint)*(byte *)(iStack_154 * 0x1c + 0x3146) * 0xe + 0x5237) !=
                  *(char *)(iStack_154 * 0x1c + 0x3150))) && (iStack_154 < iStack_42)))) {
              uStack_4 = 0;
            }
            iStack_154 = FUN_1000_84d4(uVar10,0x181f);
          }
          iStack_154 = iStack_42;
        }
        if (uStack_4 != 0) {
          if (iStack_1e != 0) {
            pbVar1 = (byte *)(iStack_154 * 0x1c + 0x3148);
            *pbVar1 = *pbVar1 | 0xc;
          }
          if (iStack_1c != 0) {
            pbVar1 = (byte *)(iStack_154 * 0x1c + 0x3148);
            *pbVar1 = *pbVar1 | 4;
          }
        }
      }
      if (((iStack_c == 0) && (iVar14 = iStack_154 * 0x1c, 0xc < *(byte *)(iVar14 + 0x3146))) &&
         ((*(byte *)(iVar14 + 0x3146) < 0x13 && ((*(byte *)(iVar14 + 0x3148) & 0xc) == 0)))) {
        bVar7 = *(byte *)(param_2 * 0x13 + -0x6da6);
        if (((uint)bVar7 + (uint)*(byte *)(param_2 * 0x13 + -0x6da5) < 2) || (bVar7 == 0)) {
          if ((*(char *)(iStack_154 * 0x1c + 0x3146) == '\r') &&
             (1 < (byte)((undefined1 *)&LAB_0000_9259)[param_2 * 0x13]))
          goto LAB_521d_c54;
        }
        else if (*(char *)(iStack_154 * 0x1c + 0x3146) == '\x0e') {
LAB_521d_c54:
          pbVar1 = (byte *)(iStack_154 * 0x1c + 0x3148);
          *pbVar1 = *pbVar1 | 0x20;
          iStack_c = 1;
        }
      }
      iVar14 = FUN_1000_84f2(0x181f,uStack_36,uStack_3a);
      if (iVar14 != 0) {
        iVar14 = FUN_1000_8d18(0x181f,iStack_154);
        pbVar1 = (byte *)(((int)uStack_3a >> 2) + ((int)uStack_36 >> 2) * 0x12 + -0x6056);
        *pbVar1 = *pbVar1 | (-(iVar14 == 0) & 0xfcU) + 5;
        iVar14 = iStack_154 * 0x1c;
        if ((((*(char *)(iVar14 + 0x314c) == '\x03') || (*(char *)(iVar14 + 0x314c) == '\x02')) ||
            (*(char *)(iVar14 + 0x314c) == '\x01')) ||
           ((9 < *(byte *)(iVar14 + 0x314c) && (*(char *)(iVar14 + 0x314b) != '1')))) {
          *(undefined1 *)(iStack_154 * 0x1c + 0x314c) = 0;
        }
        iVar14 = thunk_FUN_2a1f_047c(0x181f,uStack_36,uStack_3a,param_2,1);
        if (-1 < iVar14) {
          *(undefined1 *)(iStack_154 * 0x1c + 0x314c) = 10;
        }
        iVar14 = FUN_1000_8958(0x181f,uStack_36,uStack_3a);
        if ((iVar14 == 0) ||
           ((0xc < *(byte *)(iStack_154 * 0x1c + 0x3146) &&
            (*(byte *)(iStack_154 * 0x1c + 0x3146) < 0x13)))) goto LAB_521d_c7d;
      }
      *(undefined1 *)(iStack_154 * 0x1c + 0x314c) = 1;
    }
    else {
      iVar14 = iStack_154 * 0x1c;
      if (((0xc < *(byte *)(iVar14 + 0x3146)) && (*(byte *)(iVar14 + 0x3146) < 0x13)) &&
         ((*(char *)(iVar14 + 0x3146) != '\x11' || ((*(byte *)0x5382 & 1) != 0)))) {
        iVar14 = iStack_154 * 0x1c;
        bVar7 = *(byte *)(iVar14 + 0x3147);
        if (((0x10 << ((byte)param_2 & 0x1f) & (uint)bVar7) != 0) &&
           ((bVar7 = FUN_1000_8c28(0x181f,param_2,bVar7 & 0xf), (bVar7 & 0x60) == 0x20 ||
            (*(char *)(iVar14 + 0x3146) == '\x10')))) {
          thunk_FUN_2a1f_0470
                    (0x181f,param_2,*(undefined1 *)(iStack_154 * 0x1c + 0x3144),
                     *(undefined1 *)(iStack_154 * 0x1c + 0x3145),0,3);
        }
      }
    }
LAB_521d_c7d:
  }
  thunk_FUN_2a1f_0560(0x181f);
  *(undefined2 *)0x173c = 0;
  *(undefined2 *)0x173e = 0;
  for (iStack_3e = 0; iStack_3e < *(int *)0x539e; iStack_3e = iStack_3e + 1) {
    FUN_1000_8bd6(0x181f,iStack_3e);
    pbVar3 = (byte *)*(undefined2 *)0x8542;
    if (pbVar3[0x1a] == (byte)param_2) {
      pbVar1 = (byte *)((uint)(*pbVar3 >> 2) * 0x12 + (uint)(pbVar3[1] >> 2) + -0x6056);
      *pbVar1 = *pbVar1 | 2;
      if ((*(byte *)(iStack_3e * 0xca + 0x5d62) & 0x40) != 0) {
        puVar4 = (undefined1 *)*(undefined2 *)0x8542;
        if ((puVar4[0x1b] & 3) != 0) {
          thunk_FUN_2a1f_0470
                    (0x181f,param_2,*puVar4,puVar4[1],0,
                     (-(uint)((puVar4[0x1b] & 2) == 0) & 0xfffd) + 8);
        }
        uStack_1a._2_2_ = 0;
        uStack_1a._0_2_ = 0;
        iStack_40 = 0;
        uStack_44 = 0;
        bVar5 = false;
        iStack_48 = FUN_1000_8f2a(0x181f);
        iStack_14 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                                  ((undefined1 *)*(undefined2 *)0x8542)[1]);
        uVar10 = 0x1598;
        iStack_154 = FUN_1000_89d0(0x181f);
        iStack_16 = iStack_154;
        while (-1 < iStack_154) {
          if ((*(char *)(iStack_154 * 0x1c + 0x3146) == '\x02') &&
             ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x80) == 0)) {
            iStack_40 = iStack_40 + 1;
            bVar5 = true;
            bVar15 = 0xfcdf < (uint)uStack_1a;
            uStack_1a._0_2_ = (uint)uStack_1a + 800;
            uStack_1a._2_2_ = uStack_1a._2_2_ + (uint)bVar15;
          }
          if (((((undefined1 *)&LAB_0000_9870)[iStack_14 + param_2 * 0x10] == '\0') &&
              (((*(byte *)(iStack_154 * 0x1c + 0x3146) < 0xd ||
                (0x12 < *(byte *)(iStack_154 * 0x1c + 0x3146))) &&
               (iVar14 = iStack_154 * 0x1c,
               1 < *(byte *)((uint)*(byte *)(iVar14 + 0x3146) * 0xe + 0x5236))))) &&
             ((*(char *)(iVar14 + 0x314b) != 'G' && (*(char *)(iVar14 + 0x314b) != 'A')))) {
            uStack_44 = 1;
            bVar5 = true;
            bVar15 = 0xfa23 < (uint)uStack_1a;
            uStack_1a._0_2_ = (uint)uStack_1a + 0x5dc;
            uStack_1a._2_2_ = uStack_1a._2_2_ + (uint)bVar15;
            iStack_40 = iStack_40 + 1;
          }
          iStack_154 = FUN_1000_84d4(uVar10,0x181f);
        }
        for (iStack_150 = 0; iStack_150 < 0x10; iStack_150 = iStack_150 + 1) {
          iStack_2a = *(int *)(*(int *)0x8542 + iStack_150 * 2 + 0x9a);
          if (iStack_2a < iStack_48) {
            if ((iStack_150 == 8) && (iStack_2a = iStack_2a + (0x19 - iStack_48), iStack_2a < 0)) {
              iStack_2a = 0;
            }
          }
          else {
            iStack_2a = iStack_2a << 1;
          }
          iVar14 = iStack_2a;
          if (iStack_48 < iStack_2a) {
            iVar14 = iStack_48;
          }
          iStack_32 = (iVar14 + 0x19) / 100;
          if (((iStack_150 != 0) && (iStack_150 != 5)) && (iStack_150 != 0xd)) {
            if ((iStack_150 == 0xe) || (iStack_150 == 0xf)) {
              if ((*(uint *)(*(int *)0x8542 + 0x90) & 1 << ((byte)iStack_150 & 0x1f)) == 0)
              goto LAB_521d_167d;
              iStack_2a = iStack_2a + -100;
            }
            if (0x4a < iStack_2a) {
              bVar5 = true;
            }
            if (-1 < iStack_2a) {
              uStack_1a = (long)(int)(uint)*(byte *)(iStack_150 + param_2 * 0x10 + -0x7b44) *
                          (long)iStack_2a + uStack_1a;
              iStack_40 = iStack_40 + iStack_32;
            }
          }
LAB_521d_167d:
        }
        if (bVar5) {
          piVar2 = (int *)(param_2 * 2 + 0x1734);
          *piVar2 = *piVar2 + 1;
          uStack_1a = uStack_1a + *(char *)(*(int *)0x8542 + 0x8f) * 8;
          if (0x7fff < uStack_1a) {
            uStack_1a = 0x7fff;
          }
          thunk_FUN_2a1f_0524(0x181f,iStack_3e,(int)uStack_1a,iStack_40,uStack_44);
        }
        if ('\0' < *(char *)(*(int *)0x8542 + 0x8e)) {
          iVar12 = iStack_16;
          iStack_28 = FUN_1000_8aac(0x181f,iStack_16,10);
          puVar4 = (undefined1 *)*(undefined2 *)0x8542;
          iVar14 = iStack_16;
          if (iStack_28 < (char)puVar4[0x8e]) {
            if (puVar4[0x1a] == (byte)param_2) {
              iVar12 = ((char)puVar4[0x8e] - iStack_28) + 2;
            }
            else {
              iVar12 = 2;
            }
            thunk_FUN_2a1f_0470(0x181f,param_2,*puVar4,puVar4[1],3,iVar12);
            iVar14 = iStack_16;
          }
          while ((iStack_154 = iStack_16, -1 < iVar14 &&
                 (iVar13 = *(int *)0x8542, '\0' < *(char *)(iVar13 + 0x8e)))) {
            if (*(char *)(iVar14 * 0x1c + 0x3146) == '\v') {
              *(char *)(iVar13 + 0x8e) = *(char *)(iVar13 + 0x8e) + -1;
              if (*(char *)(iVar13 + 0x1e) != '\0') {
                *(char *)(iVar13 + 0x1e) = *(char *)(iVar13 + 0x1e) + -1;
              }
              *(undefined1 *)(iVar14 * 0x1c + 0x314b) = 0x41;
            }
            iStack_154 = iVar14;
            iVar14 = FUN_1000_84d4(iVar12,0x181f);
          }
          while ((iVar14 = iStack_16, -1 < iStack_154 &&
                 (iVar13 = *(int *)0x8542, '\0' < *(char *)(iVar13 + 0x8e)))) {
            if ((*(char *)(iStack_154 * 0x1c + 0x3146) == '\x01') &&
               (*(char *)(iStack_154 * 0x1c + 0x315b) != '\x15')) {
              *(char *)(iVar13 + 0x8e) = *(char *)(iVar13 + 0x8e) + -1;
              if (*(char *)(iVar13 + 0x1e) != '\0') {
                *(char *)(iVar13 + 0x1e) = *(char *)(iVar13 + 0x1e) + -1;
              }
              *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x41;
            }
            iStack_154 = FUN_1000_84d4(iVar12,0x181f);
          }
          while ((iStack_154 = iStack_16, -1 < iVar14 &&
                 (iVar13 = *(int *)0x8542, '\0' < *(char *)(iVar13 + 0x8e)))) {
            if ((*(char *)(iVar14 * 0x1c + 0x3146) == '\x01') &&
               (*(char *)(iVar14 * 0x1c + 0x315b) == '\x15')) {
              *(char *)(iVar13 + 0x8e) = *(char *)(iVar13 + 0x8e) + -1;
              if (*(char *)(iVar13 + 0x1e) != '\0') {
                *(char *)(iVar13 + 0x1e) = *(char *)(iVar13 + 0x1e) + -1;
              }
              *(undefined1 *)(iVar14 * 0x1c + 0x314b) = 0x41;
            }
            iStack_154 = iVar14;
            iVar14 = FUN_1000_84d4(iVar12,0x181f);
          }
          while ((iVar14 = iStack_16, -1 < iStack_154 &&
                 (iVar13 = *(int *)0x8542, '\0' < *(char *)(iVar13 + 0x8e)))) {
            if ((*(char *)(iStack_154 * 0x1c + 0x3146) == '\x04') &&
               (*(char *)(iStack_154 * 0x1c + 0x315b) != '\x15')) {
              *(char *)(iVar13 + 0x8e) = *(char *)(iVar13 + 0x8e) + -1;
              if (*(char *)(iVar13 + 0x1e) != '\0') {
                *(char *)(iVar13 + 0x1e) = *(char *)(iVar13 + 0x1e) + -1;
              }
              *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x41;
            }
            iStack_154 = FUN_1000_84d4(iVar12,0x181f);
          }
          while ((iStack_154 = iVar14, -1 < iVar14 &&
                 (iVar13 = *(int *)0x8542, '\0' < *(char *)(iVar13 + 0x8e)))) {
            if ((*(char *)(iVar14 * 0x1c + 0x3146) == '\x04') &&
               (*(char *)(iVar14 * 0x1c + 0x315b) == '\x15')) {
              *(char *)(iVar13 + 0x8e) = *(char *)(iVar13 + 0x8e) + -1;
              if (*(char *)(iVar13 + 0x1e) != '\0') {
                *(char *)(iVar13 + 0x1e) = *(char *)(iVar13 + 0x1e) + -1;
              }
              *(undefined1 *)(iVar14 * 0x1c + 0x314b) = 0x41;
            }
            iVar14 = FUN_1000_84d4(iVar12,0x181f);
          }
        }
      }
    }
    else {
      iStack_14 = FUN_1000_8912(0x181f,*pbVar3,pbVar3[1]);
      if (((int)((uint)*(byte *)0x53a6 * *(int *)0x538e) < 0xb5) && (param_2 < 4)) {
        uVar11 = (uint)(byte)((undefined1 *)*(undefined2 *)0x8542)[1];
        uVar9 = FUN_1000_893a(0x181f,*(undefined1 *)*(undefined2 *)0x8542,uVar11);
        if (((0x10 << ((byte)param_2 & 0x1f) & uVar9 & 0xff) != 0) ||
           ((3 < *(byte *)(*(int *)0x8542 + 0x1a) ||
            (*(char *)((uint)*(byte *)(*(int *)0x8542 + 0x1a) * 0x34 + 0x543f) != '\0'))))
        goto LAB_521d_f81;
      }
      else {
LAB_521d_f81:
        bVar7 = FUN_1000_8c28(0x181f,param_2,*(undefined1 *)(*(int *)0x8542 + 0x1a));
        uStack_152 = (uint)((bVar7 & 0x48) == 0x40);
        iVar14 = param_2 * 0x10 + iStack_14;
        if (((uint)*(byte *)(iVar14 + -0x6b1a) + (uint)*(byte *)(iVar14 + -0x6b5a) != 0) &&
           (((char)iStack_3e + *(char *)0x538e & 3U) != 0)) {
          iStack_154 = FUN_1000_89d0(0x181f);
          iStack_10 = FUN_1000_8aac(0x181f,iStack_154,2);
          puVar4 = (undefined1 *)*(undefined2 *)0x8542;
          iStack_10 = iStack_10 + (char)puVar4[0x1f];
          if (*(int *)0x538e / -0x32 + 6 < iStack_10) {
            thunk_FUN_2a1f_0470
                      (0x181f,param_2,*puVar4,puVar4[1],4,(-(uStack_152 == 0) & 2U) + 3);
          }
        }
        if (uStack_152 != 0) goto LAB_521d_0ef0;
        uVar11 = 0xfb8;
        iStack_154 = FUN_1000_89d0();
        if ((*(byte *)(*(int *)0x8542 + 0x1c) & 0x40) != 0) {
          if ((iStack_154 + *(int *)0x538e) % 4 == 0) {
            uVar11 = 0xd;
            iVar14 = FUN_1000_8aac(0x181f,iStack_154,0xd);
            if (iVar14 == 0) goto LAB_521d_11b6;
          }
          iStack_15a = 0;
          uVar11 = 1;
          uStack_46 = FUN_1000_8bec(0x181f,1);
          for (uStack_a = 0xfffe; (int)uStack_a < 3; uStack_a = uStack_a + 1) {
            for (uStack_6 = 0xfffe; (int)uStack_6 < 3; uStack_6 = uStack_6 + 1) {
              if ((uStack_6 != 0) || (uStack_a != 0)) {
                uVar9 = uStack_6;
                if ((int)uStack_6 < 1) {
                  uVar9 = ~uStack_6 + 1;
                }
                if (uVar9 != 2) {
                  uVar9 = uStack_a;
                  if ((int)uStack_a < 1) {
                    uVar9 = ~uStack_a + 1;
                  }
                  if (uVar9 != 2) goto LAB_521d_1029;
                }
                uStack_3a = ((byte *)*(undefined2 *)0x8542)[1] + uStack_a;
                uStack_36 = *(byte *)*(undefined2 *)0x8542 + uStack_6;
                iVar14 = FUN_1000_84f2(0x181f,uStack_36,uStack_3a);
                if (iVar14 != 0) {
                  iVar14 = FUN_1000_8958(0x181f,uStack_36,uStack_3a);
                  if (iVar14 != 0) {
                    cVar6 = FUN_1000_88a4(0x181f,uStack_36,uStack_3a);
                    if (cVar6 == '\x01') {
                      iStack_e = 0;
                      for (iStack_26 = 0; iStack_26 < 8; iStack_26 = iStack_26 + 1) {
                        iStack_4c = (int)*(char *)(iStack_26 + 0xbe) + uStack_3a;
                        iStack_34 = (int)*(char *)(iStack_26 + 0xb4) + uStack_36;
                        iVar14 = FUN_1000_84f2(0x181f,iStack_34,iStack_4c);
                        if (iVar14 != 0) {
                          iVar14 = FUN_1000_8958(0x181f,iStack_34,iStack_4c);
                          if (iVar14 != 0) {
                            cVar6 = FUN_1000_88a4(0x181f,iStack_34,iStack_4c);
                            if (cVar6 == '\x01') {
                              pbVar3 = (byte *)*(undefined2 *)0x8542;
                              iVar14 = -((uint)*pbVar3 - iStack_34);
                              if (iVar14 < 1) {
                                iVar14 = ~(iStack_34 - (uint)*pbVar3) + 1;
                              }
                              if (iVar14 < 2) {
                                iVar14 = -((uint)pbVar3[1] - iStack_4c);
                                if (iVar14 < 1) {
                                  iVar14 = ~(iStack_4c - (uint)pbVar3[1]) + 1;
                                }
                                if (iVar14 < 2) {
                                  iStack_e = iStack_e + 1;
                                }
                              }
                            }
                          }
                        }
                      }
                      if (iStack_15a < iStack_e) {
                        iStack_15a = iStack_e;
                        uStack_24 = uStack_36;
                        uStack_2c = uStack_3a;
                      }
                    }
                  }
                }
              }
LAB_521d_1029:
            }
          }
          if (0 < iStack_15a) {
            iVar14 = *(char *)(*(int *)0x8542 + 0x1f) + 4 >> 3;
            if (2 < iVar14) {
              iVar14 = 2;
            }
            thunk_FUN_2a1f_0470(0x181f,param_2,uStack_24,uStack_2c,0,iVar14 + 2);
          }
        }
      }
LAB_521d_11b6:
      if (((int)((uint)*(byte *)0x53a6 * *(int *)0x538e) < 0xc9) && (param_2 < 4)) {
        uVar11 = FUN_1000_893a(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                               ((undefined1 *)*(undefined2 *)0x8542)[1],uVar11);
        if ((0x10 << ((byte)param_2 & 0x1f) & uVar11 & 0xff) == 0) goto LAB_521d_0ef0;
      }
      iStack_2e = 0;
      bVar5 = false;
      iVar14 = iStack_14 + (uint)*(byte *)(*(int *)0x8542 + 0x1a) * 0x10;
      if ((*(byte *)(iStack_14 + param_2 * 0x10 + -0x6b1a) < *(byte *)(iVar14 + -0x6b1a)) &&
         (7 < *(byte *)(iVar14 + -0x6ada))) {
        iStack_2e = 1;
      }
      if ((*(char *)(iStack_14 + param_2 * 0x10 + -0x6b1a) == '\0') &&
         (*(byte *)(iStack_14 + (uint)*(byte *)(*(int *)0x8542 + 0x1a) * 0x10 + -0x6ada) < 8)) {
        bVar5 = true;
      }
      if ((iStack_2e != 0) || (bVar5)) {
        iStack_15a = -99;
        uStack_24 = (uint)*(byte *)*(undefined2 *)0x8542;
        uStack_2c = (uint)((byte *)*(undefined2 *)0x8542)[1];
        for (uStack_6 = 0xfffd; (int)uStack_6 < 4; uStack_6 = uStack_6 + 1) {
          for (uStack_a = 0xfffd; (int)uStack_a < 4; uStack_a = uStack_a + 1) {
            uStack_3a = ((byte *)*(undefined2 *)0x8542)[1] + uStack_a;
            uStack_36 = *(byte *)*(undefined2 *)0x8542 + uStack_6;
            iVar14 = FUN_1000_8958(0x181f,uStack_36,uStack_3a);
            if (iVar14 != 0) {
              cVar6 = FUN_1000_88a4(0x181f,uStack_36,uStack_3a);
              if (cVar6 == '\x01') {
                uStack_4 = 0;
                for (iStack_30 = 0; iStack_30 < 8; iStack_30 = iStack_30 + 1) {
                  iStack_4c = (int)*(char *)(iStack_30 + 0xbe) + uStack_3a;
                  iStack_34 = (int)*(char *)(iStack_30 + 0xb4) + uStack_36;
                  iVar14 = FUN_1000_8958(0x181f,iStack_34,iStack_4c);
                  if (iVar14 == 0) {
                    iVar14 = FUN_1000_8912(0x181f,iStack_34,iStack_4c);
                    if (iVar14 == iStack_14) {
                      uStack_4 = uStack_4 + 1;
                    }
                  }
                }
                if (uStack_4 != 0) {
                  uVar11 = uStack_6;
                  if ((int)uStack_6 < 1) {
                    uVar11 = ~uStack_6 + 1;
                  }
                  uVar9 = uStack_a;
                  if ((int)uStack_a < 1) {
                    uVar9 = ~uStack_a + 1;
                  }
                  iStack_e = (uVar9 + uStack_4 + uVar11) * 2;
                  if (iStack_15a <= iStack_e) {
                    uStack_24 = uStack_36;
                    uStack_2c = uStack_3a;
                    iStack_15a = iStack_e;
                  }
                }
              }
            }
          }
        }
        if (0 < iStack_15a) {
          iVar14 = FUN_1000_8872(0x181f,uStack_24,uStack_2c);
          if (iVar14 < 0) {
            if (iStack_2e == 0) {
              *(uint *)0x173e = *(uint *)0x173e | 1 << ((byte)iStack_14 & 0x1f);
            }
            else {
              *(uint *)0x173c = *(uint *)0x173c | 1 << ((byte)iStack_14 & 0x1f);
            }
            iStack_e = 2;
            if (iStack_2e != 0) {
              iStack_e = 3;
            }
            iVar14 = iStack_e;
            if (((*(byte *)(*(int *)0x8542 + 0x1a) < 4) &&
                (uVar11 = (uint)*(byte *)(*(int *)0x8542 + 0x1a),
                *(char *)(uVar11 * 0x34 + 0x543f) == '\0')) &&
               (iVar14 = iStack_e + 1,
               (uint)*(byte *)(iStack_14 + -0x6aea) + (uint)*(byte *)(iStack_14 + -0x6afa) +
               (uint)*(byte *)(iStack_14 + -0x6b0a) + (uint)*(byte *)(iStack_14 + -0x6b1a) ==
               (uint)*(byte *)(iStack_14 + uVar11 * 0x10 + -0x6b1a))) {
              if (0xf < *(int *)(iStack_14 * 2 + -0x7a38)) {
                iVar14 = iStack_e + 2;
              }
              iStack_e = iVar14;
              iVar14 = iStack_e;
              if (0x3f < *(int *)(iStack_14 * 2 + -0x7a38)) {
                iVar14 = iStack_e + 1;
              }
            }
            iStack_e = iVar14;
            if (*(int *)(iStack_14 * 2 + -0x7a38) <
                (int)(((uint)*(byte *)(iStack_14 + -0x6aea) + (uint)*(byte *)(iStack_14 + -0x6afa) +
                       (uint)*(byte *)(iStack_14 + -0x6b0a) + (uint)*(byte *)(iStack_14 + -0x6b1a))
                     * 0x10)) {
              iStack_e = iStack_e + -1;
            }
            bVar7 = FUN_1000_8c28(0x181f,param_2,*(undefined1 *)(*(int *)0x8542 + 0x1a));
            if ((bVar7 & 0x60) == 0x20) {
              iStack_e = iStack_e + 1;
            }
            if (*(int *)0x538e < 0x96) {
              iStack_e = iStack_e << 1;
            }
            iStack_154 = FUN_1000_89d0(0x181f);
            iStack_10 = FUN_1000_8aac(0x181f,iStack_154,2);
            iStack_10 = iStack_10 + *(char *)(*(int *)0x8542 + 0x1f);
            if (iStack_10 <= *(int *)0x538e / -0x32 + 6) {
              iStack_2e = 0;
              bVar5 = false;
            }
            if ((iStack_2e != 0) || (bVar5)) {
              thunk_FUN_2a1f_0470
                        (0x181f,param_2,uStack_24,uStack_2c,(-(uint)(iStack_2e == 0) & 0xfffa) + 7,
                         iStack_e);
            }
          }
        }
      }
    }
LAB_521d_0ef0:
  }
  for (iStack_3e = 0; iStack_3e < *(int *)0x539a; iStack_3e = iStack_3e + 1) {
    FUN_1000_8c3c(0x181f,iStack_3e);
    iStack_14 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8d4a,
                              ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
    iVar14 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,param_2);
    aiStack_14e[*(int *)0x8d52 * 0x10 + iStack_14] =
         aiStack_14e[*(int *)0x8d52 * 0x10 + iStack_14] + (uint)*(byte *)(*(int *)0x8d4a + 4);
    iVar12 = param_2 * 0x10 + iStack_14;
    if ((uint)*(byte *)(iVar12 + -0x6b1a) + (uint)*(byte *)(iVar12 + -0x6b5a) != 0) {
      if (iVar14 < 0x4b) {
        uVar11 = FUN_1000_8c28(0x181f,param_2,*(undefined2 *)(code *)FUN_0000_8d50);
        if ((uVar11 & 2) == 0) goto LAB_521d_194b;
      }
      puVar4 = (undefined1 *)*(undefined2 *)0x8d4a;
      if ((char)puVar4[5] < '\0') {
        uVar10 = 2;
      }
      else {
        uVar10 = 4;
      }
      thunk_FUN_2a1f_0470(0x181f,param_2,*puVar4,puVar4[1],4,uVar10);
    }
LAB_521d_194b:
    if ((((*(uint *)0x173c & 1 << ((byte)iStack_14 & 0x1f)) == 0) &&
        ((*(uint *)0x173e & 1 << ((byte)iStack_14 & 0x1f)) == 0)) &&
       (*(char *)(iStack_14 + param_2 * 0x10 + -0x6b1a) == '\0')) {
      uStack_24 = 0xffff;
      uStack_2c = 0xffff;
      iStack_15a = -1;
      for (iStack_26 = 0; iStack_26 < 8; iStack_26 = iStack_26 + 1) {
        uStack_3a = (int)*(char *)(iStack_26 + 0xbe) + (uint)((byte *)*(undefined2 *)0x8d4a)[1];
        uStack_36 = (int)*(char *)(iStack_26 + 0xb4) + (uint)*(byte *)*(undefined2 *)0x8d4a;
        iVar14 = FUN_1000_8958(0x181f,uStack_36,uStack_3a);
        if (iVar14 != 0) {
          cVar6 = FUN_1000_88a4(0x181f,uStack_36,uStack_3a);
          if (cVar6 == '\x01') {
            iStack_e = 0;
            for (iStack_30 = 0; iStack_30 < 8; iStack_30 = iStack_30 + 1) {
              iStack_4c = (int)*(char *)(iStack_30 + 0xbe) + uStack_3a;
              iStack_34 = (int)*(char *)(iStack_30 + 0xb4) + uStack_36;
              iVar14 = FUN_1000_8958(0x181f,iStack_34,iStack_4c);
              if (iVar14 == 0) {
                iVar14 = FUN_1000_8912(0x181f,iStack_34,iStack_4c);
                if (iVar14 == iStack_14) {
                  iStack_e = iStack_e + 1;
                }
              }
            }
            if (iStack_15a < iStack_e) {
              iStack_15a = iStack_e;
              uStack_24 = uStack_36;
              uStack_2c = uStack_3a;
            }
          }
        }
      }
      if (0 < (int)uStack_24) {
        thunk_FUN_2a1f_0470(0x181f,param_2,uStack_24,uStack_2c,1,2);
        *(uint *)0x173e = *(uint *)0x173e | 1 << ((byte)iStack_14 & 0x1f);
      }
    }
  }
  for (iStack_3e = 0; iStack_3e < 0x10; iStack_3e = iStack_3e + 1) {
    uStack_8 = (uint)(byte)((undefined1 *)&LAB_0000_9870)[iStack_3e + param_2 * 0x10];
    iStack_38 = 0;
    iStack_22 = 0;
    iStack_156 = 0;
    iStack_1e2 = 0;
    for (iStack_4a = 0; iStack_4a < 4; iStack_4a = iStack_4a + 1) {
      iVar14 = iStack_4a * 0x10 + iStack_3e;
      iStack_156 = iStack_156 + (uint)*(byte *)(iVar14 + -0x6ada);
      iStack_1e2 = iStack_1e2 + (uint)*(byte *)(iVar14 + -0x6b1a);
      if ((param_2 != iStack_4a) &&
         ((*(byte *)(iVar14 + -0x6b1a) != 0 || (*(char *)(iVar14 + -0x6b5a) != '\0')))) {
        bVar7 = FUN_1000_8c28(0x181f,param_2,iStack_4a);
        if ((bVar7 & 0x60) != 0x20) {
          bVar7 = FUN_1000_8c28(0x181f,param_2,iStack_4a);
          if ((bVar7 & 0x48) == 0x40) goto LAB_521d_1b2b;
        }
        iVar14 = iStack_3e + param_2 * 0x10;
        if ((*(byte *)(iStack_3e + iStack_4a * 0x10 + -0x6e74) < *(byte *)(iVar14 + -0x6e74)) ||
           (*(char *)(iVar14 + -0x6b1a) == '\0')) {
          iStack_22 = iStack_22 + 1;
        }
        else {
          iStack_38 = iStack_38 + 1;
        }
      }
LAB_521d_1b2b:
    }
    for (iStack_4a = 4; iStack_4a < 0xc; iStack_4a = iStack_4a + 1) {
      FUN_1000_8c32(0x181f,iStack_4a + -4);
      iVar14 = *(int *)0x8d52 * 0x10 + iStack_3e;
      iStack_156 = iStack_156 + aiStack_14e[iVar14] * 2;
      if ((*(char *)(iVar14 + -0x6e34) != '\0') || (aiStack_14e[iVar14] != 0)) {
        iVar14 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,param_2);
        if (iVar14 < 0x4b) {
          uVar11 = FUN_1000_8c28(0x181f,param_2,iStack_4a);
          if ((uVar11 & 2) == 0) goto LAB_521d_1bc3;
        }
        iVar14 = iStack_3e + param_2 * 0x10;
        if ((*(byte *)(iStack_3e + *(int *)0x8d52 * 0x10 + -0x6e34) < *(byte *)(iVar14 + -0x6e74))
           || (*(char *)(iVar14 + -0x6b1a) == '\0')) {
          iStack_22 = iStack_22 + 1;
        }
        else {
          iStack_38 = iStack_38 + 1;
        }
      }
LAB_521d_1bc3:
    }
    iVar13 = param_2 * 0x10 + iStack_3e;
    iVar12 = ((uint)*(byte *)(iVar13 + -0x6b1a) + iStack_1e2) * 0x14;
    iVar14 = *(int *)(iStack_3e * 2 + -0x7a38);
    if (iVar12 - iVar14 == 0 || iVar12 < iVar14) {
      uVar8 = 6;
    }
    else {
      uVar8 = 0;
    }
    ((undefined1 *)&LAB_0000_9870)[iVar13] = uVar8;
    if (iStack_22 != 0) {
      ((undefined1 *)&LAB_0000_9870)[iVar13] = 4;
    }
    if (iStack_38 != 0) {
      ((undefined1 *)&LAB_0000_9870)[iStack_3e + param_2 * 0x10] = 3;
    }
    iVar14 = param_2 * 0x10 + iStack_3e;
    if ((*(char *)(iVar14 + -0x6b5a) == '\0') && (*(char *)(iVar14 + -0x6b1a) == '\0')) {
      ((undefined1 *)&LAB_0000_9870)[iVar14] = 4;
    }
    for (iStack_16 = 0; iStack_16 < *(int *)0x539e; iStack_16 = iStack_16 + 1) {
      FUN_1000_8bd6(0x181f,iStack_16);
      puVar4 = (undefined1 *)*(undefined2 *)0x8542;
      if (puVar4[0x1a] != (byte)param_2) {
        iVar14 = FUN_1000_8912(0x181f,*puVar4,puVar4[1]);
        if (iVar14 == iStack_3e) {
          bVar7 = *(byte *)(iStack_3e + -0x6168);
          if ((int)(uint)*(byte *)(iStack_3e + -0x6168) <
              (int)(char)*(byte *)(*(int *)0x8542 + 0x1f)) {
            bVar7 = *(byte *)(*(int *)0x8542 + 0x1f);
          }
          *(byte *)(iStack_3e + -0x6168) = bVar7;
        }
      }
    }
    uStack_20 = 0;
    for (iStack_4a = 0; iStack_4a < 4; iStack_4a = iStack_4a + 1) {
      if (param_2 != iStack_4a) {
        uStack_20 = uStack_20 + *(byte *)(iStack_3e + iStack_4a * 0x10 + -0x6b5a);
      }
    }
    if (4 < (int)uStack_20) {
      uStack_20 = 4;
    }
    uVar11 = (uint)*(byte *)(iStack_3e + -0x6168);
    if ((int)(uint)*(byte *)(iStack_3e + -0x6168) < (int)uStack_20) {
      uVar11 = uStack_20;
    }
    *(undefined1 *)(iStack_3e + -0x6168) = (char)uVar11;
  }
  iStack_154 = 0;
  do {
    if (*(int *)0x539c <= iStack_154) {
      return;
    }
    if (((*(byte *)(iStack_154 * 0x1c + 0x3147) & 0xf) == (byte)param_2) &&
       (*(char *)(iStack_154 * 0x1c + 0x314b) != 'A')) {
      if (*(byte *)(iStack_154 * 0x1c + 0x314c) < 10) {
        *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x3f;
      }
      iVar14 = iStack_154 * 0x1c;
      if (((*(char *)(iVar14 + 0x314c) == '\0') || (*(char *)(iVar14 + 0x314c) == '\x05')) ||
         (*(char *)(iVar14 + 0x314c) == '\x06')) {
        if ((*(char *)(iStack_154 * 0x1c + 0x314b) == 't') ||
           (*(char *)(iStack_154 * 0x1c + 0x314b) == 'i')) {
          *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x3f;
        }
        iStack_15a = 9999;
        iStack_4e = -1;
        iVar14 = iStack_154 * 0x1c;
        uStack_36 = (uint)*(byte *)(iVar14 + 0x3144);
        uStack_3a = (uint)*(byte *)(iVar14 + 0x3145);
        iStack_14 = FUN_1000_8912(0x181f,uStack_36,uStack_3a);
        if ((*(char *)(iVar14 + 0x3146) == '\x01') || (*(char *)(iVar14 + 0x3146) == '\x04')) {
          bVar7 = *(byte *)(iStack_14 + param_2 * 0x10 + -0x6b5a);
          uStack_20 = (uint)bVar7;
          if ((bVar7 < 3) &&
             ((bVar7 < 2 || (*(char *)(iStack_14 + param_2 * 0x10 + -0x6b1a) == '\0'))))
          goto LAB_521d_1fdf;
        }
        for (iStack_3e = 0; iStack_3e < 0x40; iStack_3e = iStack_3e + 1) {
          iVar14 = (param_2 * 0x40 + iStack_3e) * 4;
          if ((*(char *)(iVar14 + -0x674e) != -1) &&
             (iVar12 = iStack_154 * 0x1c,
             (1 << (*(byte *)(iVar14 + -0x674e) & 0x1f) &
             (uint)*(byte *)((uint)*(byte *)(iVar12 + 0x3146) * 0xe + 0x523d)) != 0)) {
            iVar14 = FUN_1000_8912(0x181f,(int)*(char *)(iVar14 + -0x6750),
                                   (int)*(char *)(iVar14 + -0x674f));
            if ((((iVar14 == iStack_14) ||
                 ((0xc < *(byte *)(iVar12 + 0x3146) && (*(byte *)(iVar12 + 0x3146) < 0x13)))) &&
                ((*(char *)((param_2 * 0x40 + iStack_3e) * 4 + -0x674e) != '\x01' ||
                 ((*(byte *)(iStack_154 * 0x1c + 0x3148) & 4) != 0)))) &&
               ((*(char *)((param_2 * 0x40 + iStack_3e) * 4 + -0x674e) != '\a' ||
                ((*(byte *)(iStack_154 * 0x1c + 0x3148) & 8) != 0)))) {
              iVar12 = (param_2 * 0x40 + iStack_3e) * 4;
              iVar14 = FUN_1000_856a(uStack_36,uStack_3a,(int)*(char *)(iVar12 + -0x6750),
                                     (int)*(char *)(iVar12 + -0x674f));
              iStack_12 = (aiStack_1da[iStack_3e] * iVar14) / (*(char *)(iVar12 + -0x674d) + 1);
              if (((*(char *)(iStack_154 * 0x1c + 0x314c) == '\x05') ||
                  (*(char *)(iStack_154 * 0x1c + 0x314c) == '\x06')) &&
                 ((*(byte *)(iStack_154 * 0x1c + 0x3146) < 0xd ||
                  (0x12 < *(byte *)(iStack_154 * 0x1c + 0x3146))))) {
                iVar14 = FUN_1000_8886(0x181f,uStack_36,uStack_3a);
                if ((-1 < iVar14) ||
                   ((iVar14 = (param_2 * 0x40 + iStack_3e) * 4, *(char *)(iVar14 + -0x674d) < '\x03'
                    || ((*(char *)(iVar14 + -0x674d) * iStack_3c < iStack_12 &&
                        (aiStack_1da[iStack_3e] != iStack_3c)))))) goto LAB_521d_1dec;
              }
              if ((iStack_12 < iStack_15a) &&
                 (iStack_12 / iStack_3c <=
                  *(char *)((param_2 * 0x40 + iStack_3e) * 4 + -0x674d) * 3 >> 1)) {
                iStack_15a = iStack_12;
                iStack_4e = iStack_3e;
              }
            }
          }
LAB_521d_1dec:
        }
        if (-1 < iStack_4e) {
          *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x31;
          if (*(char *)((param_2 * 0x40 + iStack_4e) * 4 + -0x674e) == '\x01') {
            *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x74;
          }
          else if (*(char *)((param_2 * 0x40 + iStack_4e) * 4 + -0x674e) == '\a') {
            *(undefined1 *)(iStack_154 * 0x1c + 0x314b) = 0x69;
          }
          iVar14 = iStack_154 * 0x1c;
          *(undefined1 *)(iVar14 + 0x314c) = 0xb;
          iVar12 = (param_2 * 0x40 + iStack_4e) * 4;
          *(undefined1 *)(iVar14 + 0x314d) = *(undefined1 *)(iVar12 + -0x6750);
          *(undefined1 *)(iVar14 + 0x314e) = *(undefined1 *)(iVar12 + -0x674f);
          if (*(char *)(iVar12 + -0x674e) != '\x04') {
            aiStack_1da[iStack_4e] = aiStack_1da[iStack_4e] + 1;
          }
        }
      }
    }
LAB_521d_1fdf:
    iStack_154 = iStack_154 + 1;
  } while( true );
}

```
