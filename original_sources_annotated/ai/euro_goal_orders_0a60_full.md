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

## Structural port, now live (2026-08-18)

User asked to "structura-port" `FUN_521d_0a60` — the whole function's own
control flow ported faithfully, callees allowed to stay placeholder. Given
the honest scoping constraint below, this landed as a standalone function
first (pilot, gated behind an env var), then — on explicit instruction
("replace the old ported function, it wasn't even fully ported") — was
made the live path the same day, replacing the old three-loop
soldier/founder/fallback approximation described in "Implementation"
above.

**Ported at structural (line-for-line control-flow) fidelity**:
`ai_euro_0a60_goal_orders_structural` in `ai_euro.c`, next to
`ai_euro_colony_goals` — the goal-consumption tail only (raw lines
974-1063, this file's own "New section: goal -> orders wiring" pseudocode
above). Mirrors the decomp's branches/arithmetic 1:1: act_state/order_code
byte machine, Soldier/Dragoon continent-defense skip gate (recomputed
fresh via a small local colony/land-unit-count-by-continent helper,
same tables `euro_g_table_0a60.md` resolved), the 64-slot scored scan with
the act-state 5/6 re-evaluation skip clause, and the best-slot commit
(order_code/act_state/goal_x/goal_y write + non-MILITARY tally bump).
DOS-only per-unit scratch bytes (`+0x314b/c/d/e`) live in a file-local
`s_0a60_pilot_state[]` shadow array (name kept from the pilot pass) —
Linux has no persisted struct field for them. **Live**: called once per
nation per turn from `ai_euro_dispatcher_turn`, right after
`ai_euro_colony_goals`; `ai_euro_unit_act`'s old three-loop scan was
deleted and replaced with a read-back of the shadow state
(`act_state==0xb`) plus the one Linux-only override that had no mapped
0a60 equivalent (threatened-Stockade LABOR), kept as a post-override.
The shadow array is memset at the top of every `ai_euro_dispatcher_turn`
call (alongside the pre-existing `s_deferred_found`/`s_founded_colony_turn`
resets) rather than kept across turns like DOS's real bytes — same
"recompute fresh within the turn" simplification those two already use;
avoids stale cross-scenario state (distinct unit pools reusing small unit
ids, as several unit tests do) driving a unit into a found/labor-bind
action from leftover garbage. Real games only ever have one live unit
pool, so this is a test-hygiene fix, not an in-game behavior change.

**Deliberately not attempted this pass**: the unit-loop threat-flag section
(raw lines 1-189) and the deep G-table/colony-loop section (raw lines
190-973). Both still lean on genuinely unresolved DOS accessor semantics —
`FUN_1000_8aac`'s field-id meaning across its several call sites, what
`thunk_FUN_2a1f_0470`/`047c`/`0524`/`0560` actually do, and what
individual bits of `unit+0x3148` mean beyond the two already inferred here
(FOUND/MIL_EXPAND eligibility, folded into
`ai_euro_0a60_unit_can_pursue_goal` as a name-match approximation rather
than modeled as a separate always-true gate). A literal transliteration of
those sections right now would be unverifiable guesswork — this project's
own method notes (`ai-transcription-fulldraft` memory) explicitly warn
against that. `ai_euro_refresh_continent_stance` already covers the
G-table's *effect* via a from-scratch recompute; it just isn't
`FUN_521d_0a60`'s own literal write path.

## Second pass, "keep picking at it" (2026-08-18, same day)

User asked to keep pushing toward this being structurally done. Used
Ghidra headless directly (`analyzeHeadless <projects> decompiled-colonize
-process VICEROY_OUT_2.EXE -postScript GhidraDecompileAt.java <space:off>`,
`space:off` = `0000:<segment*0x10+offset hex>` for a resident-space
target) to decompile three of the four previously-unresolved thunks
`0a60` itself calls — trivial once decompiled, each is a two-line overlay-
load-then-call stub:

- `thunk_FUN_2a1f_047c` → `FUN_521d_0906` = `probe_adjacent_contact_claim`
  (already in `euro_goals.c`, catalogued "structure only" — not itself
  further resolved this pass, but confirms the call site's shape: `0a60`
  asks "is there an adjacent-tile contact claim available" and sets
  `act_state=10` if so, i.e. a *third* act_state distinct from the goal-
  pursuit `0xb` this port already models — units in that state are
  implicitly excluded from goal-consumption since it only reacts to
  `act_state ∈ {0,5,6}`. Not wired — this port has no upstream write of
  `act_state=10` yet, so it would be permanently unreachable dead state.)
- `thunk_FUN_2a1f_0524` → `FUN_521d_02be` = `upsert_work_queue` (already
  in `euro_goals.c`, fully resolved) — **confirms `ai_euro_colony_goals`'s
  existing haul work-queue registration is the structurally correct call
  shape**, not a Linux invention.
- `thunk_FUN_2a1f_0560` → `FUN_521d_031c` = `clear_work_queue` (already in
  `euro_goals.c`, fully resolved) — DOS calls this once, between the unit
  loop and the colony loop; Linux's `ai_euro_colony_goals` already calls
  the equivalent `ai_goals_clear_work_queue()` once, at the top of the
  function — functionally identical (nothing between "top of function"
  and "end of unit loop" touches the work queue either way), just not at
  the textually-identical spot.
- `thunk_FUN_2a1f_0470` (the fourth) was already resolved earlier the same
  day (`upsert_primary_goal`, see the CONTACT-gate fix above) — all four
  `0a60`-called thunks are now named.

**Real formula recovered and ported**: the colony-loop's haul-urgency
work-queue score (raw lines ~528-604, the call into `upsert_work_queue`
just confirmed above) is not the "16×6 matrix OPEN" mystery it was filed
as — it's `Σ over 16 cargo slots (skip FOOD/LUMBER/TRADE_GOODS) of
euro_price[cargo][nation] * clamp(f(stock,target), 0, target)`, both
tables already fully live in Linux: `-0x7b44` is `col1->nation[n].trade.
euro_price[]` (cross-referenced via `indian_raid_loot.md`/
`indian_trade_2820.md`/`euro_diplo_153e_full.md`, all already citing this
same table as "euro price"), and `colony_ptr+0x9a` is `c->stock[]` — same
16-slot order, confirmed field-for-field against `col1_save.h`'s Col1
colony struct (`+0x8c improve_timer` … `+0x98 hammers_purchased` then
`stock[16]` immediately after, landing exactly on `+0x9a`). TOOLS(14)/
MUSKETS(15) only contribute (at a flat −100 discount) when
`cargo_produced_mask` has that bit set; HORSES(8) gets a floor-adjust
below target instead of the plain clamp; at/above target, `have` doubles
before re-clamping. Ported into `ai_euro_colony_goals`'s haul work-queue
block, replacing the old `idle_turns*8 + specialty_bump` thin stand-in —
the score is now the real formula (still `+ cargo_idle_turns*8` as its own
tail term, matching DOS). `target` (`FUN_1000_8f2a()`, a single scalar,
three other unrelated call sites across this project, never named
anywhere) is approximated as a fixed 100 (base Warehouse capacity, the
only DOS-documented "target stock level" constant already in this
codebase) — flagged in-code as a placeholder.

**Registration gate deliberately NOT switched to the real formula**:
tried using the formula's own "any slot's post-adjustment value > 0x4a"
signal as the register-or-not gate (matching DOS literally) — regressed
`unit_specialty_flag_a_haul_match` (a deliberately-symmetric two-colony
tie-break test) because the `target=100` placeholder never trips for that
test's modest stock quantities (50 lumber vs. a 100 target), so neither
colony ever got a work-queue entry and the wagon never moved. Reverted to
the existing, already-tested `ai_euro_colony_haul_cargo_short`-based
boolean as the gate, keeping only the *score* as the new real formula —
a deliberate, documented "real value, pragmatic gate" split, not an
oversight.

**Still deliberately not ported** (same class of blocker as before, now
narrower and more precisely stated): a real DOS pre-loop over this
nation's units, run once per colony before the cargo-weight loop, adds a
further saturating +800 (idle Missionary, globally) / +1500 (exposed
combat-capable land unit on a `stance==0` continent — `LAB_0000_9870`,
confirmed to be this file's own G-table output,
`ai_euro_continent_stance_at()==0`, i.e. no assigned stance) into the same
score, and its own boolean (whether the +1500 arm fired at least once)
becomes `upsert_work_queue`'s real `flag_b` (this port keeps `flag_b` as
its own already-load-bearing haul/CONTACT discriminator instead, see
in-code comment). Not ported: its unit-iterator (`FUN_1000_89d0`, no
explicit x/y args) was already traced in `move_scoring_20e6_full.md`'s
`FUN_1000_8aac` investigation to depend on caller-context registers this
project has no cheap way to read — a genuinely open question, not a
convenience skip.

**Verified**: clean `colonize_core` rebuild, zero new warnings. Full
`ctest`: identical failure set before/after (`unit_ai_euro_expand`'s
pre-existing unrelated ship-buy bug, `golden_ai_turns`/`golden_ai_joint`'s
pre-existing TURN4→5) — confirmed via `git stash`/`git stash pop` on top
of the CONTACT-gate-fixed baseline. No regressions from either the thunk
resolution or the real haul-score formula.

**Still open for a future "structurally done" pass** — the unit-loop
(raw lines 1-189, per-unit garrison-request/threat-flag housekeeping that
writes `unit+0x3148`'s bits before this section's own goal-consumption
tail ever runs) and the missionary/exposed-unit haul-score bonus above are
the two largest remaining pieces; both need the same kind of dedicated,
possibly multi-pass raw-disassembly investigation `FUN_1000_8aac` needed
(6 passes, 2 of 15 cases actually resolved) before a literal port is
honest rather than guesswork. `unit+0x3148`'s individual bits are
scattered across at least three other functions (`euro_unit_act.md`:
`0x80`=ship damaged/under-construction, `nation_eot_ship_spawn.md`:
`0x40`=in-transit, `move_scoring_20e6_full.md`: `0x20`/`0x10`/bit1 seen
but not yet named) — collecting and reconciling all of them into one
coherent bitfield is real, bounded follow-up work, not a dead end.

**Verified**: clean `colonize_core` rebuild, zero new warnings under
`-Wall`. `golden_ai_turns`/`golden_ai_joint` fail at the same pre-existing
TURN4→5 spot (confirmed via `git stash` before this change too) — specific
goto numbers shifted (expected, new formula) but it was already failing
there, not a newly-broken golden.

**`unit_ai_euro_expand` regression, root-caused and fixed (2026-08-18,
same day, per "if it blocks getting the structure right, it's in
scope")**: DOS's real `score = weight*dist/(prio+1)` formula (lower score
wins) means a *higher* prio number pulls harder — this made the still-
condensed goal *writer* (`ai_euro_colony_goals`)'s **unconditional**
"else register a COLONY/COLONY_ALT visit goal at every owned colony,
prio 5 or 8" branch out-compete a nearby FOUND goal (prio 2) whenever a
colony had nothing else to report. Traced the real DOS write site: this is
`thunk_FUN_2a1f_0470` call #2 in the raw colony loop (`*puVar4,puVar4[1],
0,(-(uint)((puVar4[0x1b]&2)==0)&0xfffd)+8` — code is actually `CONTACT`
(0), prio computed as exactly 8 when colony `ai_flags` bit1 (Man-O-War
nearby) is set, else 5), gated by `(puVar4[0x1b] & 3) != 0` — i.e. it only
fires when ai_flags bit0 (`COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP`) or bit1
(`COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR`) is actually set (naval threat near
the colony), never unconditionally. Fixed in `ai_euro_colony_goals`: the
prio values (5/8) were already right, only the gate was invented; kept
Linux's own `COLONY`/`COLONY_ALT` codes rather than switching to literal
`CONTACT` (downstream `ai_euro_unit_act` already branches on them for
"go work/garrison this colony", a behavior `CONTACT`'s own downstream
handling — move-and-attack — doesn't have; DOS's own code being literally
`CONTACT` at this site likely means its 5b66/20e6 tail treats an
own-colony `CONTACT` target specially in a way not worth reverse-
engineering just to rename an already-correctly-behaving Linux code).
**Also confirmed unrelated**: a second failure surfaced once this one was
fixed (`unit_5d04_buy_caravel_colonies_ge6`, further into the same test
binary) — isolated via `git stash`/checkout bisection and found to
already fail on committed `HEAD` with **zero** of this session's changes
applied (a concurrent, unrelated "AI rewrite"/"AI structural rewrite"
commit set rewrote the Europe ship-buy/treasury code separately and
introduced its own bug there); left alone as out of scope for 0a60.

**Verified after fix**: `unit_second_wave` (the pioneer-founds-second-
colony scenario) now passes; overall `unit_ai_euro_expand` still fails
(the unrelated pre-existing ship-buy bug above), `golden_ai_turns`/
`golden_ai_joint` unchanged at the same pre-existing spot; clean rebuild,
no new warnings.

**Follow-up if resumed**: the two out-of-scope sections above (unit-loop
threat-flags, deep G-table/colony-loop) still need their own mapping
passes (same method as `euro_g_table_0a60.md`'s multi-pass `.asm` register
tracing) before a literal port of them would be honest — the CONTACT-gate
fix above was a narrow, well-evidenced extraction from that same raw
block, not a full mapping pass of it.

## Third pass — both "genuinely open" targets closed, no live session needed (2026-08-18, same day)

User asked whether they could help via a live DOSBox-X session, then
corrected two things about the plan: don't assume a `FUN_1000_XXXX` name's
suffix is its runtime address (real-mode segment relocation makes that
unsafe), and prefer `BPM` (memory breakpoint) over a code breakpoint —
matching this project's own established track record
(`decomp_inventory.md`: `popup_string_resolver.md`'s `BPM` capture, "has
repeatedly been the actual unblock, not a fallback of last resort").

Before asking for a live session at all, re-checked whether
`tools/address_mapping.csv` (already built, already used by the CONTACT-
gate fix's thunk resolutions) had these two targets — it did, for both:

- **`FUN_1000_8f2a` resolved**: canonical `FUN_281f_0d3a` (`281f:0d3a`),
  which is itself a thunk to `FUN_15eb_0a50` — already fully documented
  elsewhere in this project (`save_format_map.md`, `FUNCTION_CATALOG.md`
  ×2) as the **warehouse capacity formula**, `100×(1+warehouse_level)`,
  already live in Linux as `colonies_warehouse_capacity`. The "target"
  scalar the haul-score clamp uses (previously a flat `100` placeholder)
  is now the real per-colony value. Confirmed via `GhidraDecompileAt` on
  `0000:28f2a` (`= 0x281f*0x10 + 0x0d3a`, i.e. the *canonical* thunk
  address from the CSV — not the `FUN_1000_8f2a`-suffix-implied
  `0000:18f2a`, which failed `createFunction` outright, exactly the
  failure mode the user's correction predicted).
- **`FUN_1000_89d0`/`FUN_1000_84d4` resolved**: canonical
  `FUN_281f_07e0`/`FUN_281f_02e4`, both **already fully known** in
  `ai/accessors.c` — `unit_index_on_tile(x,y)` and the transport-chain
  prev-link walker (`FUN_1427_005c`/`FUN_1427_004a`, cross-referenced
  against `euro_goals.c`'s already-documented `walk_unit_stack_to_end`).
  This confirms the unit-scan pre-loop this file's "Second pass" section
  filed as blocked is: walk the stack of units **at the colony's own
  tile** (DOS via the transport-chain linked list; the call site's own
  immediately-preceding code loads the colony's x/y right before, which
  is why no explicit args are visible at the call — the chain-walk starts
  from whatever's already at that tile). `move_scoring_20e6_full.md`'s
  open "caller-context registers" question was about a *different* call
  site (`20e6`'s own field-2 use of `FUN_1000_8aac`, a separate,
  still-genuinely-open function) — not this one; the two were
  conflated in the prior pass's writeup.

**Ported, same pass**: the missionary/exposed-combat-unit haul-score
bonus this file's "Second pass" section deferred. Linux has no live
per-tile unit stack to walk, so iterates + filters `x/y` instead (same
substitution this file already uses for `garrison_quota`). +800 per
Missionary/Jesuit at the colony tile (DOS's "colony ai_flags bit7 clear"
gate approximated as always-clear — unnamed bit, no Linux field, a
defensible superset since the real gate would only narrow this); +1500
per exposed combat-capable land unit (`attack>1`, real `ColonizeUnitType`
field, not a name-match this time) when `ai_euro_continent_stance_at()`
reads 0 for this colony's continent and the unit isn't garrisoned/
admitted (checked against this port's own `s_0a60_pilot_state`, which is
always fresh-zeroed at this point in the turn — `ai_euro_colony_goals`
runs before the shadow state gets populated — so this arm is always
satisfied here, correctly, not a shortcut).

**Not changed**: `flag_b`'s real DOS meaning (whether the +1500 arm fired)
still isn't wired — kept as Linux's own haul/CONTACT work-queue
discriminator, same reasoning as the "Second pass" section already gave
for `flag_a`.

**Verified**: clean `colonize_core` rebuild, zero new warnings. Full
`ctest`: identical failure set throughout, no regressions (`git stash`
confirmed both before this pass's two edits individually).

**Now actually closed** (not just "not attempted"): both concrete
targets from the "Second pass" section's `**Still deliberately not
ported**`/`**Still open**` notes. What remains genuinely open for a full
structural pass is narrower now: the unit-loop's own per-unit garrison-
request/`0x3148`-bit housekeeping (raw lines 1-189) and the deep G-table
write path itself (raw lines 190-500ish, `ai_euro_refresh_continent_stance`
covers its *effect* via recompute, not this literal path) — both still
same difficulty class as `FUN_1000_8aac`'s field-id puzzle, not this
pass's scope.

## Fourth pass — a real bug fixed, one lead chased to ground (2026-08-18, same day)

User said "Proceed." Applied the `address_mapping.csv`-first method to
the remaining unresolved unit-loop/goal-consumption callees.

**Real bug fixed, not just a refinement**: `FUN_1000_8886`, used in the
live goal-consumption tail's act-state-5/6 re-evaluation gate
(`ai_euro_0a60_goal_orders_structural`), was ported as "is a unit
standing on the *goal* tile" (`units_id_at(g->x, g->y)`). Checked its
canonical mapping (`FUN_281f_0696` → `FUN_137f_0358`) — it's
`euro_settlement_owner`, **already fully resolved** in `accessors.c`
("Tribe bit + Euro owner (0..3); Indians/empty → −1"), and the raw
decomp's own args at this call site are `uStack_36`/`uStack_3a` — the
**unit's own** x/y (set from `unit+0x3144/0x3145` earlier the same
block), not the goal's. Real check: "is the re-evaluating unit currently
sitting in any Euro colony (any nation)", not "is anyone standing on the
goal tile" — a different tile *and* a different question. Fixed to
`colonies_id_at(ctx->colonies, u->x, u->y) >= 0` (Linux's colony pool
holds only Euro colonies, matching DOS's own tribe-owner exclusion).
Verified: clean rebuild, identical `ctest` failure set, no regressions.

**Lead chased, genuinely a dead end (not a new one)**: `FUN_1000_8aac`
(used repeatedly in the unit-loop, still out of scope) — checked whether
its canonical chain via `address_mapping.csv` pointed somewhere new.
It doesn't: `FUN_1000_8aac` → `FUN_281f_08bc` → `FUN_1427_0d38` →
resident `ram:4fa8` = **the same `FUN_0000_4fa8`** `move_scoring_20e6_
full.md` already spent 6 passes on (15-case low-level dispatcher, only
case 2 disassembly-confirmed — a transport-chain node swap, not a
count). Not a wrong-address dead end this time; genuinely the same wall.

**One real catch worth recording**: `euro_ocean_scoring.c`'s own comment
block gives *caller-side* names for this same function's modes 2-6/0xc
("mode 2 → free-ish/pax capacity", "mode 3 → founders", "mode 4 →
military", …) — inferred from a *different* caller's local-variable
names, not from disassembling the callee. This **directly contradicts**
the disassembly-confirmed case-2 finding above (a chain-link/unit-id
value, not a countable quantity) for the same mode number. The
caller-variable-naming guess is lower-confidence evidence than the actual
disassembly and should not be trusted over it — flagging here so a future
pass doesn't quietly adopt `euro_ocean_scoring.c`'s mode table as ground
truth for `0a60`'s own mode 2/3/4/6 calls without re-checking this
conflict first. Not resolved this pass; the unit-loop's garrison-request
logic that depends on these modes stays unported.

## Fifth pass — a real capability gap closed (2026-08-18, same day)

User said "Continue." Went further into the unit-loop (raw lines
~645-711) applying the same method, resolved several more supporting
symbols along the way:

- `FUN_1000_84de` → canonical `FUN_281f_02ee` → `FUN_1427_0002`
  ("walk transport_next to stack head", per `FUNCTION_CATALOG.md`) —
  confirms the unit-loop's `FUN_1000_84de`/`84d4` pair are the same
  transport-chain-stack walk (to-head / one-step-down) already used
  elsewhere (haul-score bonus, "Third pass").
- `unit+0x3150` — already known project-wide as `holds_occupied` (cargo
  count), cross-referenced via `euro_diplo_3180_full.md`. Combined with
  the type-table `0x5237` (already known: "sail capacity" —
  `move_scoring_ship.md`), the raw comparison
  `type_table_5237[unit.type] == unit+0x3150` resolves cleanly as **"is
  this ship's cargo hold completely full"** (`u->cargo_count ==
  units_ship_capacity(...)` in Linux terms) — not the "type ==
  personality" non-sequitur an unlabeled read would suggest.
- `func_0x0001854c` (the `aiStack_1da` weight-seed initializer) —
  attempted via `GhidraDecompileAt`; `createFunction` failed the same way
  `FUN_1000_8f2a`'s wrong-guessed address did before the CSV lookup fixed
  it, but this symbol has no `address_mapping.csv` row at all (unlike
  every `FUN_1000_*`/`FUN_281f_*` name so far) — genuinely unresolved,
  not a repeat of the same mistake. Left as the existing fixed-50
  placeholder; low-impact since it seeds *all* 64 slots uniformly (the
  claim-count increments that follow matter more than the seed's exact
  magnitude).

**Real capability gap closed**: `unit+0x3148` bit2/bit3 (the FOUND/
MIL_EXPAND eligibility bits `ai_euro_0a60_unit_can_pursue_goal` folds in)
turned out to be about a **ship's cargo composition**, not a land unit's
own type — the raw code's outer gate (`FUN_1000_8aac` modes 3/4/6, still
unresolved at the disassembly level — same `FUN_0000_4fa8` wall as
before) is nested inside `if (unit is ship-type && ship_is_full)`, and
sets bit2 when the ship carries founders-or-military, bit3 specifically
for military. This means **ships were never eligible for FOUND/
MIL_EXPAND at all** in this port before now — their names never match the
land-unit name checks (`ai_euro_name_is_pioneer`, `ai_euro_is_military_
name`), so a Caravel loaded with colonists ready to found a new town had
no way to be assigned that goal via this mechanism. Fixed: since DOS's
own query mechanism (modes 3/4/6) isn't safely portable, computed the
same *information* directly from Linux's real ship cargo hold
(`u->cargo_ids[]`, scanning each passenger's name) instead — ships with a
military passenger now qualify for MIL_EXPAND, ships with a founder or
military passenger now qualify for FOUND. Not modeled: DOS's "if this
ship is full, also require every earlier-indexed ship in the same
stack/fleet to be full" coordination downgrade — a defensible superset
(may let a not-yet-fully-loaded fleet's already-full ship pursue slightly
early).

**Verified**: clean rebuild, zero new warnings. Full `ctest`: identical
failure set before/after (confirmed via `git stash`/`git stash pop` —
also confirmed HEAD had moved to a newer concurrent "AI rewrite" commit
since the last pass, unrelated, no interaction).

**Still open, unchanged from the "Fourth pass" note**: the unit-loop's
remaining garrison-request bits (bit0/1/4/5/6/7, the `-0x6da6`/`-0x6da5`
per-nation table, `LAB_0000_9259`) and the deep G-table's literal write
path — same difficulty class as `FUN_1000_8aac`'s field-id puzzle for the
parts that depend on it.

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
