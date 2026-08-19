# `FUN_5bfb_153e` — full clean recovery (2026-08-14)

## Status: the "5 local helpers" from the earlier pass were a false lead — RETRACTED below. The `003bc6-003bf8` region is a resident-thunk jump table dispatching to 10 ALREADY-KNOWN `FUN_5bfb_*` functions, not new flavor-text/attitude code. Real structural finding: 153e's outcome dispatch reuses existing, mostly-already-ported machinery (102a/1092/0182 dialogs, 312e/0000 score, 13b0 alliance, 10ec war/ally eligibility, 022e Indian contact) plus one still-unresolved branch (`FUN_5bfb_12d0`, already tracked elsewhere as "Order clear `12d0` deep"). Worthiness-score phase and the exact war-declare state flip remain genuinely open.

**2026-08-19 — both remaining open items resolved/ported.** The selector
"mystery" turned out to be a false premise (no runtime index — every call
site is compile-time-fixed to one jump-table slot, confirmed directly off
`address_mapping.csv`), and the worthiness-score phase is now a
structural reference port (`ai_diplo_153e_worthiness_score_structural` in
`src/core/ai_diplo.c`, real where resolvable, honestly stubbed where not,
not wired live). Full section below: "Selector logic + worthiness-score
phase — resolved/ported 2026-08-19."

**2026-08-15 — `FUN_1000_8c28` (the accessor most `& 0x..` reads in this
doc go through) confirmed to be a pure raw-byte accessor with a
nation-range branch, not a computed/derived summary.** Decompiled it
directly (`FUN_0000_5b34`): `param_1<4` (Euro nation) reads exactly
`euro_relation` (`nation*0x13c-0x77c4`, same field `col1_save.h` already
maps); `param_1>=4` (Indian tribe) reads a **wholly separate** table at
absolute `23000`, stride `0x4e` — not `relation_by_indian[8]`, a
different, previously-unrecorded per-tribe-per-Euro-nation flags byte.
Matters for reading this doc: several `& 2`/`& 0x60` checks below (e.g.
the "worthiness score" phase, `iStack_a4 + 4` params) are Indian-range
calls into that *other* table, not `euro_relation` — don't read their bit
values as `euro_relation` semantics. The direct (non-accessor) `-0x77c4`
reads in this doc (bit 2 set/checked around contact-establishment and
worthiness discount) are consistent with plain `AI_DIPLO_PEACE`, cross-
used to fix `euro_unit_act.md`'s `FUN_4720_049e` treasure-tension bit
choice same day. Full writeup: `euro_unit_act.md`'s "`153e` bit-semantics
pass" section. The 23000-stride-0x4e Indian table itself is not named or
ported — real open lead if anyone wants it.

**2026-08-14, later same day — RETRACTION of this doc's own immediately-
preceding "5 helpers confirmed blocked" claim.** That claim was built on
decompiling `OVL16_L0040:003bd0`/`003bd5`/`003bdf`/`003be4`/`003bee` as if
they were 5 separate function bodies. Dumping the **raw instructions**
(not the decompiler's C) at those same addresses immediately after
publishing that claim showed they are NOT function entries at all —
they're **consecutive `JMPF <seg>:<off>` thunk instructions**, one
5-byte jump each, part of a 10-entry (`003bcb`-`003bf8`) resident jump
table (an 11th entry at `003bc6` precedes it, and garbage bytes follow at
`003bfd`, confirming the table's real bounds). Ghidra's decompiler had
**stale, wrong function boundaries cached at those addresses** from an
earlier, unrelated analysis pass, and happily produced plausible-looking
(but meaningless) C for garbage/misaligned bytes — the exact "silent
wrong content, not a crash" failure mode this project's own memory has
flagged before (`684c_08c0`, `15eb_1d4c`) as needing boundary
verification, not just a successful decompile. Every claim in the
immediately-preceding version of this section (shared attitude-adjustment
shape, the `0x1866` message-id match, "confirmed blocked same wall as
`022e`") is **void** — built on decompiling the wrong bytes. Full
retraction; `settlement_record_8d4a.md`'s corresponding "second
confirmation" addition is being reverted too.

**What the jump table actually is, verified via `address_mapping.csv`
cross-reference (not guessed)**: `address_mapping.csv` already had ONE
entry for this exact table (`thunk_FUN_2a1f_05fc` at `5bfb:37cb` =
`OVL16_L0040:3bcb` → canonical `FUN_1000_a7ec`/`FUN_2a1f_05fc`) — the
missing piece was recognizing the other 9 addresses as the *same* table,
continuing. `FUN_2a1f_05fc` and its 9 neighbors (offset +0xe each,
`viceroy_unpacked.c:38150-38280`) are trivial 2-line resident thunks
(`FUN_210d_0dab(0x2a1f); FUN_5bfb_XXXX(); return;` — an overlay-loader
call then a direct forward), and **every one of the 10 targets is already
a known, named, mostly-already-documented function**:

| Table idx | Thunk offset | Target | Linux status |
|---|---|---|---|
| 0 | `2a1f:05fc` | `FUN_5bfb_153e` (self) | this function |
| 1 | `2a1f:060a` | `FUN_5bfb_12d0` | **resolved 2026-08-19** — see below |
| 2 | `2a1f:0618` | `FUN_5bfb_102a` | dialogs, thin `ctx->status` **Done** |
| 3 | `2a1f:0626` | `FUN_5bfb_312e` | census/rank/combat factor, score stand-in **Done** |
| 4 | `2a1f:0634` | `FUN_5bfb_0000` | census/rank/combat factor, score stand-in — this is the ONE selector call `153e` fires from inside its own worthiness-score phase (raw 485); still not independently ported byte-exact (honest stub, `ai_diplo_153e_unit_score_stub`, 2026-08-19) |
| 5 | `2a1f:0642` | `FUN_5bfb_1092` | dialogs, thin `ctx->status` **Done** |
| 6 | `2a1f:0650` | `FUN_5bfb_0182` | dialogs, thin `ctx->status` **Done** |
| 7 | `2a1f:065e` | `FUN_5bfb_13b0` | form/break alliance **Done** |
| 8 | `2a1f:066c` | `FUN_5bfb_022e` | Indian unit meet/contact — partial, deep body PARKED (`settlement_record_8d4a.md`) |
| 9 | `2a1f:067a` | `FUN_5bfb_10ec` | war/ally eligibility by military balance **Done** |

**Real implication, and it's a good one**: `153e`'s outcome dispatch —
whatever selects an index 0-9 into this table — routes into machinery
this project has **already mostly ported**, not a wall of new blocked
code. Index 1 (`12d0`) is now resolved too (below), so **every** table
target is understood. This makes `153e` look considerably *more*
tractable than the retracted claim suggested.

**2026-08-19: both remaining items resolved.** "Which selector value
picks the table index" turned out to be a false premise — see "Selector
logic + worthiness-score phase — resolved/ported 2026-08-19" below for
the mechanical offset→target table (built directly off
`address_mapping.csv`, not re-derived) and the worthiness-score phase's
structural port. The below paragraph is kept for its historical framing
(what looked open before this pass), not as a still-current TODO.

## `FUN_5bfb_12d0` — resolved 2026-08-19, previously "body unexamined"

Full body (`viceroy_unpacked.c:97214-97256`) is clean, 43 lines, no
corruption. `FUN_5bfb_12d0(nation_a, nation_b)`:

```c
for (each unit i in the unit table) {
  if (unit[i].owner_nation == nation_b            // +0x3147 & 0xf
      && !(0xd <= unit[i].type <= 0x12)            // not a ship (+0x3146)
      && unit_type_combat_byte[unit[i].type] > 1)   // combat-capable land unit (0x5236)
  {
    x, y = unit[i].x, unit[i].y;                    // +0x3144 / +0x3145
    if (map_tile_in_bounds(x, y)) {                  // FUN_281f_0302
      for (each of the 8 neighbor offsets) {
        if (euro_settlement_owner(nx, ny) == nation_a) {   // FUN_281f_0696
          if (unit[i].orders == 5 || unit[i].orders == 6) {  // +0x314c
            unit[i].orders = 0;
          }
          break;
        }
      }
    }
  }
}
```

Both callees are already-known, already-catalogued accessors
(`FUNCTION_CATALOG.md`: `FUN_281f_0302`=`map_tile_in_bounds`,
`FUN_281f_0696`=`euro_settlement_owner`), and the combat-capable/land-only
gate (`0x5236>1`, ship range `0xd..0x12` excluded) is the same idiom
already glossed elsewhere in this project (`move_scoring_land.md`:
"combat-capable (`0x5236>1`)"). So the whole function reads cleanly with
zero unresolved pieces: **for every combat-capable land unit belonging to
`nation_b` that sits adjacent to a settlement owned by `nation_a`, cancel
any pending roam/reevaluate order** (order-state 5 or 6 → 0; per
`docs/mysteries_catalog.md`'s `unknown15_lo` entry, order-state∈{5,6} is
exactly what `unit+0x3148` bit1 `roam_reeval_pending` mirrors). Called
`(A,B)` then `(B,A)` from `FUN_5bfb_13b0`'s alliance-form path (both
sides' border garrisons refreshed) and once from this table's index 1 —
i.e. **"wake up border-garrison units near the other nation's territory
so they re-plan, because the diplomatic relationship with that neighbor
just changed."** `euro_diplo.md`'s "Order clear `12d0` deep" line item is
now fully resolved; not ported to Linux yet — `ai_diplo_form_alliance_ctx`
doesn't currently touch unit orders, and this is small enough to be a
quick follow-up (loop units of the two nations, same adjacency/order-state
test, no new struct fields needed) rather than a semantic gap.

**Ported 2026-08-19** as `ai_diplo_wake_border_garrisons` (`ai_diplo.c`,
runtime layer): DOS order-state 5/6 lines up numerically with Linux's own
`UNITS_ORDER_FORTIFY`/`FORTIFIED`, so the port clears a Fortify/Fortified
order (`units_clear_orders`) on any combat-capable (`attack>1`) land unit
of `nation_b` sitting adjacent to a `nation_a` colony; wired into
`ai_diplo_form_alliance_ctx` called both directions (A,B)/(B,A), matching
`13b0`'s dual call. `unit_ai_diplo`/`golden_ai_joint` green (pre-existing
unrelated `TURN1→2 tribe[8]` fixture failure confirmed via `git stash`).
rushing it after just having to retract one wrong finding.

**Lesson, the important one**: a decompile succeeding (Ghidra prints
plausible C, no error) is not evidence the address it started from was a
real function entry — cross-check against `address_mapping.csv`'s own
already-registered entries for the SAME region *before* trusting a fresh
decompile, and when something looks like a cluster of near-identical
small "helper functions" sharing suspicious `unaff_BP`/`unaff_SS`
register origins, dump raw instructions first. This is a sharper version
of a lesson this project has logged at least twice before (`684c_08c0`,
`15eb_1d4c`) — logging it a third time because it just cost a real
retraction, not just a close call.

## Selector logic + worthiness-score phase — resolved/ported 2026-08-19

**Selector logic: resolved, no runtime index exists.** The "which selector
value picks the table index" question above is answered by rereading
`tools/address_mapping.csv` directly instead of re-deriving anything: rows
2360-2369 already list the exact `OVL16_L0040` byte offset of every one of
the 10 jump-table slots (`3bcb, 3bd0, 3bd5, 3bda, 3bdf, 3be4, 3be9, 3bee,
3bf3, 3bf8` — uniform 5-byte `JMPF` stride, matching the earlier retraction's
own raw-instruction dump), and each offset's own row already carries the
`FUN_OVL16_L0040__003bXX` symbol Ghidra printed at that address inside
`153e`'s body. Cross-referencing those against the 10 trivial 2-line
resident thunks at `viceroy_unpacked.c:38150-38246`
(`FUN_210d_0dab(0x2a1f); FUN_5bfb_XXXX(); return;`, read directly, not
inferred) gives a complete, mechanical offset→target table:

| `153e` call-site symbol | table idx | target | raw line(s) |
|---|---|---|---|
| `FUN_OVL16_L0040__003bee` | 7 | `FUN_5bfb_13b0` | 406 (entry gate) |
| `FUN_OVL16_L0040__003bdf` | 4 | `FUN_5bfb_0000` | 485 (worthiness-score unit loop) |
| `FUN_OVL16_L0040__003bd5` | 2 | `FUN_5bfb_102a` | 704, 705, 780, 837, 900, 933, 959, 966, 991 (commit/flavor text) |
| `FUN_OVL16_L0040__003be4` | 5 | `FUN_5bfb_1092` | 734, 785, 842 (commit/flavor text) |
| `FUN_OVL16_L0040__003bd0` | 1 | `FUN_5bfb_12d0` | 1065-1066 (commit phase, "at war" bit set — both directions) |

So there is **no runtime selector variable at all** — the earlier framing
("which selector value... picks the table index") was itself a false
premise, built on the now-retracted "`003bd0` is a real function call with
real arguments" reading. Each named call site in the raw decompile is a
**compile-time-fixed** call through one specific jump-table slot — this is
just DOS's inter-overlay call linkage (every far call from one overlay
into a different resident overlay routes through a per-call-site thunk
slot), functionally identical to a direct call. `153e` reaches only 5 of
the 10 possible targets; it never reaches `312e`, `0182`, or `022e` (those
are reached by other callers elsewhere).

Confirms the earlier "declare-war state flip" guess (`003bd0(a,b)` then
`(b,a)`) was on the right track after all, just for the wrong reason: it's
not a state flip, it's the border-garrison wake (`12d0`, already fully
ported as `ai_diplo_wake_border_garrisons`) fired symmetrically once the
"at war" bit reads set post-commit.

**Worthiness-score phase: structurally ported (reference, not live)** as
`ai_diplo_153e_worthiness_score_structural` in `src/core/ai_diplo.c`
(alongside the selector table above, wired in as
`ai_diplo_153e_selector_table`). Mirrors raw lines ~405-594 (the doc's
"phase 1" description) end to end: the human-nation/invalid-nation entry
gate (real — DOS `param_2*0x34+0x543f`, the same control-status byte
`ai_king.c`'s `FUN_43f7_2244` header already ties to
`turn_run_european_ai_stubs`'s human-skip check), the 14-continent
dominance/delta-accumulator loop (real for the already-resolved colony
(`-0x6b1a`) and land-unit (`-0x6b5a`) counts, recomputed locally the cheap
way `ai_euro_refresh_continent_stance` already does), the per-unit
ownership loop (real control flow; its one selector call, idx4/`FUN_5bfb_
0000`, is an honest stub — see `ai_diplo_153e_unit_score_stub`), the
euro_relation peace-bit check (real, `-0x77c4`), and the final clamp/scale
arithmetic (real — the two `func_0x0001854c` calls are ported as plain
value/lo/hi clamps, since the raw literals are given, not invented; the
`FUN_0000_e096` treasury-ratio call is NOT ported, see below).

Follows the `ai_euro_5d04_nation_planning_structural` precedent exactly:
finishing a structural port with this many honest stubs does not by
itself make it safe to wire live. **Newly identified, genuinely
unresolved DS globals this pass** (none guessed, all left as documented
neutral stubs in the code):

- `-0x77f8` — a per-nation flags byte, bit 2 (raw 432). **Not** `-0x77c4`/
  `euro_relation` — a distinct field this project hasn't named. Only feeds
  phase 4's flavor text, inert within phase 1.
- `-0x6d68` — per-nation "tension" byte (raw 441, 509-517, 556, 590),
  compared against a ×3 threshold in several places. Central to the
  forced-conflict override and final worthy-flag gate; stubbed to 0.
- `-0x6a4e` — per-continent **exposed** combat value (not fortified,
  orders ≠ `A`/`G`, plus the `0x543f` class gate). Distinct from the
  already-resolved unrestricted sums (`-0x6e74`/`-0x6a8e`) this project's
  G-table already exposes in `ai_euro.c` — a real, separate table this
  port does not compute. Central to the continent loop's dominance/delta
  math; stubbed to 0, which makes the "self is dominant here" branch
  never fire.
- `-0x6ada` — per-continent skilled-unit count. Stubbed to 0.
- `0x53c8[]` — a per-nation declare-war cooldown timer (raw 421-424,
  436-437, 548, 556). No persistent Linux equivalent tracked (reference
  port, not live); stubbed as "always eligible."
- `0xa153` — a single byte (raw 509), compared directly to `param_2`. No
  match anywhere else in this project's docs or `address_mapping.csv`.
  Stubbed to "never matches."
- `0x84fc` — an indirect royal/crown treasury record (`+0x2a`/`+0x2c`
  32-bit halves, raw 570-584), gating an affordability clamp via
  `FUN_0000_e096`. Not the same as the already-known per-nation
  `royal_money` field (`nation+0x22`, single `int32_t`) — this is a
  separate, likely-global pointer. The clamp is stubbed to never trigger.
- `FUN_1000_89a4` (table `-0x77f1`, arg `0x13`) — per-nation FF/feature-bit
  test (`FUNCTION_CATALOG.md`: "nation feature/FF bit test"). **Not** an
  is-human-nation check (that's the separate, already-resolved `0x543f`
  table used at the entry gate) — which specific Founding Father or
  feature bit 0x13 selects isn't identified. Stubbed to "absent."

`FUN_5bfb_0000` (selector idx4, the worthiness-score unit loop's own
per-unit score callee) is a genuinely separate open item from all of the
above — it's not a DS global but an unresolved DOS *function body*
(`euro_diplo.md`: "census/rank/combat factor, score stand-in", never
independently ported byte-exact anywhere in this project; the existing
`ai_diplo_military_score` is a different, already-live generic
approximation of the same role, not a port of this specific callee).
Stubbed via `ai_diplo_153e_unit_score_stub` — real per-unit iteration and
ownership matching, inert per-unit score contribution.

Full `ctest`: 41/41 runnable tests pass (2026-08-19; `golden_ai_turns`/
`_mid01`/`_late01`/`golden_ai_joint` remain PARKED/Disabled per the top of
`docs/ai_transcription.md`, unaffected either way since nothing here is
wired live).

## Status (original, 2026-08-14 earlier same day): recovered clean, characterized at moderate depth, not fully section-mapped or ported

Found while sweeping this project's `address_mapping.csv` for other large
`before-first-function`/gap entries after `FUN_521d_0a60` turned out to be
one (see `euro_goal_orders_0a60_full.md`). `euro_diplo.md` already flagged
`153e` as "Large war-declare body... thin sting + structural deepen (Done
unpark #5); deep body PARKED" — this is that deep body, recovered for the
first time.

**Recovery**: one `GhidraDecompileAt OVL16_L0040:193e` call. **1117 lines,
zero warnings, clean single return** — no truncation/mid-flow red flags
like `0a60` had; this one looks genuinely complete on the first attempt.
6 params (`undefined2 param_1` + 5 `int` params) — a far richer signature
than any thin stand-in currently documents. Confirms the ~7KB gap
`address_mapping.csv` showed between `153e` and the next real symbol
(`FUN_5bfb_312e`) is genuinely this one function's real body, not
several smaller undeclared functions (unlike the `5d04`→`6d8e` gap probed
the same pass — see below).

**Correction, checked after the fact: the canonical export already had
this clean too** — `viceroy_unpacked.c:97321` declares the identical
6-param `FUN_5bfb_153e`, no `WARNING:` above it, and its body runs exactly
to `FUN_5bfb_312e`'s declaration at line 98433 — **1112 lines**, matching
both this recovery and `euro_diplo.md`'s original size citation. Unlike
`0a60`, this function was **never actually truncated or missing** — its
`address_mapping.csv` "before-first-function" tag just meant that specific
tool's own database hadn't cross-registered a boundary for it, not that
the content was wrong or absent. **Lesson for the gap-sweep method**: check
the canonical export directly first (cheap) before reaching for the
overlay/Ghidra tooling (expensive) — "before-first-function" in
`address_mapping.csv` is a weaker signal than "silently truncated," and
conflating them wastes a round-trip. `0a60`'s real truncation was a
genuine, specific bug; it doesn't generalize to every gap-flagged symbol.

## What it touches (from a field-frequency scan, not a full read)

Real, load-bearing overlap with tables **already resolved earlier this
session**:
- `−0x6a4e` (`field_combat_strength_by_continent`) and `−0x6b1a`
  (`colony_counts_by_continent`) — both from the deep G-table work
  (`euro_g_table_0a60.md`) — appear repeatedly. War-declare logic reads
  the same per-nation-per-continent presence/strength data the G-table
  formula does.
- `unit+0x314d`/`+0x314e` (goal-target x/y, from the `0a60` goal-consumption
  engine) — appear 3 times each, suggesting this function also reads or
  sets active unit goals as part of its war-declare side effects.
- `−0x77ce`/`−0x77cc` (nation record, `nation*0x13c` base) paired and read
  via `FUN_0000_e096` with a `100`/`0x32` scale argument — shape strongly
  suggests a treasury/gold ratio check (percentage-of-treasury threshold),
  though not confirmed against the already-named `royal_money` field
  (different addressing convention, not cross-checked this pass).
- `0x84fc`, `0x8542`, `0x3144`-`0x3147`, `0x539c` — all already-named
  globals/unit-struct fields used throughout this project.
- `−0x6be4`, `−0x6bd4`, `−0x6d68` — new, not resolved this pass.

## Structural shape, second pass same day (sampled, not exhaustive)

Read further (roughly the first third closely, the rest by scanning call/
label markers) to confirm the overall shape without doing a full line-by-
line phase map:

1. **Worthiness score** (lines ~78-390): loops all 14 continents comparing
   `param_2` (self) vs `param_3` (target) via the G-table tables
   (`−0x6a4e` combat value, `−0x6b1a` colonies, `−0x6b5a` land units,
   `−0x6ada` skilled units — all confirmed this session), scaled by
   difficulty (`0x53a6`) and a per-nation "tension" byte (`−0x6bd4`,
   compared at a ×3 threshold — not resolved). Folds in a unit-loop military
   balance (`FUN_...__003bdf`, a local helper, not traced) and a treasury-
   capacity gate via the already-known royal-money-shaped fields
   (`FUN_0000_e096` ratio call, matches the pattern used elsewhere for
   "can we afford this" checks).
2. **Target colony pick** (lines ~281-335): scores up to 16 candidate
   colonies near a reference unit (`param_4`) by production-shortfall ×
   the already-known `−0x7b44` urgency-weight table (same table `0a60`'s
   colony-threat scoring uses), keeps the best.
3. **Indian-relation modifier** (lines ~310-360): loops all 8 tribes,
   comparing relation-to-target vs relation-to-self (`FUN_1000_84fc`,
   already-known `ai_diplo_indian_relation` shape) and the already-resolved
   `−0x6e7c`/`−0x6be4` tables (Brave combat value / nation accumulator,
   from the `417e` price-formula work) to adjust the aggression count.
4. **Commit + flavor text** (lines ~390-1106): calls a local mutator twice
   with swapped args (`FUN_...__003bd0(param_2,param_3)` then
   `(param_3,param_2)` — almost certainly the real declare-war state
   flip, both directions) gated behind the accumulated score, then picks
   one of **~11 distinct message-id branches** (`FUN_...__003bd5(mode,
   msg_id, param_3)` with IDs `0x18bb/0x18c1/0x18f3/0x1901/0x190f/0x191e/
   0x1938/0x1949/0x1955/0x197c/0x19dc`) — these read as raw compiled string-
   table handles, not directly matchable to `GAME.TXT` `@` tags without
   knowing this build's string-index scheme (not resolved this pass; the
   IDs are real data if anyone wants to chase a byte-exact port later).

**Confirms this is the real war-declare *trigger* Linux is missing** —
`ai_diplo.c`'s existing "thin 153e" citations are all about *side effects*
once a war starts (gold sting, tax bump, embargo), sourced from a
*different*, already-ported, simpler function (`FUN_5bfb_10ec`-shaped
`ai_diplo_euro_balance`). This function is the deeper, G-table-aware
decision `10ec` apparently doesn't replace, just coexists with — not
cross-checked which one actually fires when in the real DOS turn order.

## Not done this pass

- Still no line-by-line phase map with exact formulas (`0a60`-level rigor)
  — this is a structural read, confirmed accurate where checked but not
  exhaustive.
- `FUN_...__003bdf`/`003bd0`/`003bd5`/`003be4`/`003bee` were never real
  local helper functions at all — resolved (and a first, wrong "traced"
  attempt retracted) same day, later pass: these addresses are entries in
  a resident jump table (`003bc6`-`003bf8`) whose real targets are 10
  already-known `FUN_5bfb_*` functions. See the status section at the top
  of this doc for the full table and what it means.
- Message-id → `GAME.TXT` tag mapping not resolved.
- Checked whether `153e` calls `10ec` or vice versa: **neither** —
  confirmed via direct canonical-export read, no cross-call in either
  body. **Found their real callers on a second, better-targeted grep**
  (first attempt searched for the thunk names with empty parens and missed
  the real argumented calls):
  - `FUN_5bfb_10ec` is called from `FUN_5bfb_13b0` — **already documented**
    as `form_alliance`/`break_alliance` in `euro_diplo.md`. So `10ec`
    genuinely is alliance-decision-scoped, not a general war trigger —
    confirms the header comment's "Opportunistic war/ally by military
    balance (5bfb_10ec/13b0)" framing was accurate all along.
  - `FUN_5bfb_153e` is called from `FUN_5bfb_3180` — a **genuinely
    unmapped** function (not in `euro_diplo.md` at all), itself also
    flagged in this pass's gap sweep (a further 1.6KB gap right after it).
    Its call site writes `unit+0x314f` right before invoking `153e`.
    **Fully mapped in a later same-day pass**: `3180` is the adjacent-unit
    encounter resolver — naval ambush roll + Euro/Euro (`153e`) vs
    Euro/Indian (already-ported `022e`) vs Indian/Indian diplomatic
    dispatch by nation-pair type. See
    [`euro_diplo_3180_full.md`](euro_diplo_3180_full.md). `153e` really is
    the deep war-declare trigger, fired on physical adjacency — not a
    duplicate/alternate path to what `10ec` already covers, and not a
    periodic turn-based check either (that assumption would have been
    wrong if left unchecked).
  
  Chased one level further than originally planned that same pass (a grep
  pattern miss, not a real investigation) — a later pass then went ahead
  and mapped `3180` in full anyway, since at 239 lines it turned out much
  smaller than `153e`/`0a60`, not the fourth big thread it looked like at
  first glance.
- **Not ported.**

**Real next step if resumed**: this deserves the same phase-by-phase
treatment `0a60` got, given it clearly shares infrastructure with the
G-table/goal-engine work. Given time spent this session, deferred rather
than rushed — flagged here with real, verified content (not a guess) so
the next pass starts from clean ground truth instead of re-discovering it.

## Sibling finding: `5d04`→`6d8e` gap is NOT one truncated function

While probing gaps, also checked the ~3.5KB unexplained span between
`FUN_521d_5d04`'s documented end and `FUN_521d_6d8e`'s confirmed start.
Unlike `0a60`, this one bisects into **several distinct small real
functions** (clean creates at `OVL14_L0000:6300`/`6500`/`6600`/`6900`,
each with a plausible register-passed near-call shape; failed creates at
`6000`/`6100`/`6200`/`6700`/`6a00`/`6b00` are most likely body interiors of
those, not evidence of anything hidden) rather than one big missed body.
Lower priority than `153e` — not named or mapped further this pass.

## Raw recovered C (1117 lines, zero warnings)

```c
undefined2
FUN_5bfb_153e
          (undefined2 param_1,int param_2,int param_3,int param_4,int param_5,int param_6)

{
  uint *puVar1;
  int *piVar2;
  byte *pbVar3;
  byte bVar4;
  int iVar5;
  uint uVar6;
  int iVar7;
  int iVar8;
  uint uVar9;
  undefined2 unaff_CS;
  undefined2 uVar10;
  undefined2 uVar11;
  undefined2 unaff_DS;
  bool bVar12;
  undefined4 uVar13;
  undefined1 *puVar14;
  undefined2 uVar15;
  undefined2 uVar16;
  undefined2 uVar17;
  int iStack_ce;
  int iStack_cc;
  int iStack_ca;
  int iStack_c6;
  int iStack_c4;
  int iStack_c2;
  int iStack_c0;
  int iStack_be;
  int iStack_bc;
  int iStack_ba;
  int iStack_b8;
  int iStack_b6;
  int iStack_b4;
  int iStack_b2;
  int iStack_b0;
  uint uStack_ae;
  int iStack_ac;
  int iStack_aa;
  int iStack_a8;
  int iStack_a6;
  int iStack_a4;
  uint uStack_a2;
  uint uStack_a0;
  uint uStack_9e;
  int iStack_9c;
  uint uStack_9a;
  int iStack_98;
  uint uStack_96;
  int iStack_94;
  int iStack_92;
  int iStack_90;
  undefined2 uStack_8e;
  int iStack_8c;
  int iStack_8a;
  int iStack_88;
  int iStack_86;
  int iStack_84;
  undefined1 auStack_80 [20];
  undefined4 uStack_6c;
  uint uStack_68;
  int iStack_66;
  int iStack_64;
  int iStack_62;
  int iStack_60;
  int iStack_5e;
  undefined1 auStack_5c [80];
  int iStack_c;
  int iStack_a;
  int iStack_8;
  int iStack_6;
  int iStack_4;
  
  iStack_b4 = -1;
  uStack_8e = 0;
  iStack_ce = 0;
  uStack_68 = 0;
  iStack_a8 = 0;
  iStack_c = 0;
  iStack_bc = 0;
  iStack_b0 = 0;
  iStack_be = 0;
  iStack_9c = 0;
  uStack_6c._2_2_ = 0;
  uStack_6c._0_2_ = 0;
  thunk_FUN_1000_8740();
  if ((3 < param_2) || (*(char *)(param_2 * 0x34 + 0x543f) != '\0')) {
    FUN_OVL16_L0040__003bee();
    uStack_8e = 1;
    uStack_6c = CONCAT22(uStack_6c._2_2_,(undefined2)uStack_6c);
    goto LAB_OVL16_L0040__0034de;
  }
  if ((*(byte *)0x5382 & 1) != 0) goto LAB_OVL16_L0040__0034de;
  iStack_bc = param_6;
  iVar7 = param_2;
  iVar8 = param_3;
  uVar6 = FUN_1000_8c28();
  if ((uVar6 & 0x20) == 0) {
    iStack_bc = 1;
    iVar7 = 10;
    FUN_1000_8714(0x181f,10,iVar8);
  }
  if (*(int *)(param_3 * 2 + 0x53c8) + 0x10 <= *(int *)0x538e) {
    iStack_bc = 1;
    FUN_1000_8c00(param_2,param_3,0x10,iVar7);
  }
  unaff_CS = 0x181f;
  uStack_9e = FUN_1000_8c28(0x181f,param_2,param_3);
  uStack_9e = uStack_9e & 0x10;
  if (iStack_bc == 0) goto LAB_OVL16_L0040__0034de;
  if ((((param_3 == 0) || (param_3 == 1)) || (param_3 == 2)) || (param_3 == 3)) {
    FUN_1000_86b0(0x181f);
  }
  if ((*(byte *)(param_3 * 0x13c + -0x77f8) & 4) != 0) {
    iStack_9c = 1;
  }
  FUN_1000_8772(0x181f,param_2);
  iStack_8c = *(int *)(param_3 * 2 + 0x53c8);
  *(undefined2 *)(param_3 * 2 + 0x53c8) = *(undefined2 *)0x538e;
  uStack_8e = 1;
  for (iStack_cc = 1; iStack_cc < 0xf; iStack_cc = iStack_cc + 1) {
    iVar7 = param_3 * 0x10 + iStack_cc;
    if (((1 < *(byte *)(param_3 + -0x6d68)) < *(byte *)(iVar7 + -0x6b1a)) &&
       (bVar4 = *(byte *)(iStack_cc + param_2 * 0x10 + -0x6a4e), *(byte *)(iVar7 + -0x6b5a) < bVar4)
       ) {
      iStack_ce = iStack_ce +
                  ((uint)bVar4 / (*(byte *)(iVar7 + -0x6b5a) + 1) <<
                  (-(*(char *)0x53a6 == '\0') & 1U) + 1);
    }
    else {
      if ((*(char *)(iStack_cc + param_2 * 0x10 + -0x6a4e) != '\0') &&
         (*(char *)(iStack_cc + param_3 * 0x10 + -0x6a4e) != '\0')) {
        iStack_a8 = 1;
      }
      if ((*(char *)(iStack_cc + param_3 * 0x10 + -0x6a4e) != '\0') &&
         (4 < *(byte *)(iStack_cc + param_2 * 0x10 + -0x6ada))) {
        iStack_a8 = 1;
      }
      iVar7 = param_2 * 0x10 + iStack_cc;
      if (*(char *)(iVar7 + -0x6b1a) == '\0') {
        iVar7 = param_3 * 0x10 + iStack_cc;
        uStack_96 = (uint)*(byte *)(iVar7 + -0x6a4e) -
                    (uint)*(byte *)(iStack_cc + param_2 * 0x10 + -0x6a4e);
        uVar6 = (int)uStack_96 >> (-(*(char *)(iVar7 + -0x6b1a) == '\0') & 1U) + 1;
      }
      else {
        iVar8 = param_3 * 0x10 + iStack_cc;
        if (*(byte *)(iVar8 + -0x6b1a) < 2) {
          uVar6 = (uint)*(byte *)(iStack_cc + param_3 * 0x10 + -0x6a4e);
        }
        else {
          uVar6 = (uint)*(byte *)(iVar8 + -0x6a4e) - (uint)*(byte *)(iVar7 + -0x6a4e);
        }
      }
      uStack_68 = uStack_68 + uVar6;
    }
  }
  iStack_8 = 0;
  iStack_b2 = 0;
  iStack_62 = 0;
  iStack_c6 = 0;
  while( true ) {
    if (*(int *)0x539e <= iStack_c6) break;
    FUN_1000_8bd6(0x181f,iStack_c6);
    if ((*(byte *)(*(int *)0x8542 + 0x1a) == (byte)param_2) ||
       (*(byte *)(*(int *)0x8542 + 0x1a) == (byte)param_3)) {
      iStack_b8 = FUN_OVL16_L0040__003bdf(0x181f,&iStack_92,&iStack_6,&iStack_c4,param_3);
      if (iStack_92 == param_3) {
        uStack_68 = uStack_68 + iStack_b8 * 2;
        if ((iStack_6 == 0) || (1 < iStack_c4)) {
          if (iStack_ce != 0) {
            iStack_ce = iStack_ce + -1;
          }
          iStack_a8 = 1;
        }
        iStack_86 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                                  ((undefined1 *)*(undefined2 *)0x8542)[1]);
        if (*(char *)(param_3 * 0x10 + iStack_86 + -0x6b1a) == '\0') {
          iStack_b8 = iStack_b8 << 1;
        }
        iStack_b2 = iStack_b2 + iStack_b8;
        iStack_62 = 1;
      }
      if (iStack_92 == param_2) {
        iStack_8 = iStack_8 + iStack_b8;
        uStack_68 = uStack_68 + iStack_c4 * -2;
      }
    }
    iStack_c6 = iStack_c6 + 1;
  }
  if ((((*(byte *)0xa153 == (byte)param_2) && (0x4f < *(int *)0x538e)) &&
      (3 < *(byte *)(param_2 + -0x6d68))) && (1 < *(byte *)(param_3 + -0x6d68))) {
    iStack_c = 1;
  }
  bVar4 = *(byte *)(param_2 + param_3 * 0x13c + -0x77c4);
  uStack_ae = bVar4 & 2;
  if ((iStack_c != 0) ||
     (((bVar4 & 2) != 0 &&
      (*(byte *)(param_2 + -0x6bd4) < (byte)(*(char *)(param_3 + -0x6bd4) * '\x03'))))) {
    iStack_a8 = 1;
    iStack_ce = 0;
    uStack_68 = func_0x0001854c(0x181f,uStack_68,(uint)*(byte *)0x53a6 * 200 + 100,0x26ac);
  }
  if (uStack_9e != 0) {
    iStack_a8 = 1;
  }
  if (iStack_ce != 0) {
    iStack_a8 = 0;
  }
  iVar7 = FUN_1000_89a4(0x181f,param_2,0x13);
  if (iVar7 != 0) {
    iStack_c = 0;
    iStack_a8 = 0;
    if (iStack_8c < 0) {
      iStack_8c = 0;
    }
  }
  if ((uStack_ae == 0) &&
     (iVar7 = (*(byte *)0x53a6 - 10) * -10, iVar7 - *(int *)0x538e != 0 && *(int *)0x538e <= iVar7))
  {
    iStack_c = 0;
    iStack_a8 = 0;
    if (iStack_8c < 0) {
      iStack_8c = 0;
    }
  }
  uVar6 = (int)((*(byte *)0x53a6 + 8) * uStack_68 * 10) / 100;
  if (uStack_ae == 0) {
    uStack_68 = (int)uVar6 >> 2;
    if (-1 < iStack_8c) {
      if (*(int *)0x538e < 0x32) {
        uVar6 = (int)uVar6 >> 1;
      }
      else if (*(int *)0x538e < 100) {
        uVar6 = uVar6 - uStack_68;
      }
      uStack_68 = uVar6;
      if ((*(byte *)(param_2 + -0x6d68) < 3) && (*(byte *)(param_2 + -0x6bf0) < 8)) {
        uStack_68 = (int)uStack_68 >> 1;
      }
    }
  }
  else {
    uStack_68 = uVar6 << 1;
  }
  uVar10 = 0x181f;
  iVar7 = func_0x0001854c(0x181f,(int)((*(byte *)0x53a6 + 1) * uStack_68) >> 3,0,400);
  uStack_68 = iVar7 * 0x32;
  if (uStack_9e != 0) {
    uStack_68 = uStack_68 + (*(byte *)0x53a6 + 1) * 500;
  }
  iVar8 = (int)uStack_68 >> 0xf;
  iVar7 = *(int *)0x84fc;
  if ((*(int *)(iVar7 + 0x2c) <= iVar8) &&
     ((*(int *)(iVar7 + 0x2c) < iVar8 || (*(uint *)(iVar7 + 0x2a) < uStack_68)))) {
    uVar6 = *(uint *)(iVar7 + 0x2a);
    iVar7 = *(int *)(iVar7 + 0x2c);
    uVar9 = iVar7 << 1 | (uint)((int)uVar6 < 0);
    if (((iVar8 <= (int)uVar9) &&
        (((iVar8 < (int)uVar9 || (uStack_68 <= uVar6 * 2 && uVar6 * 2 - uStack_68 != 0)) &&
         (-1 < iVar7)))) && ((0 < iVar7 || (299 < uVar6)))) {
      uVar10 = 0xd1d;
      iVar7 = FUN_0000_e096(uVar6,iVar7,0x32,0);
      uStack_68 = iVar7 * 0x32;
    }
  }
  iVar7 = FUN_1000_89a4(uVar10,param_2,0x13);
  if (iVar7 != 0) {
    uStack_68 = (int)uStack_68 >> 1;
  }
  if ((uStack_68 == 0) ||
     ((byte)(*(char *)(param_3 + -0x6bd4) * '\x03') < *(byte *)(param_2 + -0x6bd4))) {
    iStack_a8 = 0;
  }
  bVar12 = iStack_a8 != 0;
  iStack_94 = iStack_a8;
  iVar7 = param_4 * 0x1c;
  if ((*(byte *)(iVar7 + 0x3147) & 0xf) == (byte)param_3) {
    iStack_64 = FUN_1000_8804(0x181f,*(undefined1 *)(iVar7 + 0x3144),*(undefined1 *)(iVar7 + 0x3145)
                              ,param_3,0xffff);
    iStack_a6 = (uint)*(byte *)(iVar7 + 0x3144) + (int)*(char *)(param_5 + 0xb4);
    iStack_ac = (uint)*(byte *)(iVar7 + 0x3145) + (int)*(char *)(param_5 + 0xbe);
    iVar7 = FUN_1000_89ae(0x181f,iStack_a6,iStack_ac);
    if ((-1 < iVar7) && (-1 < iStack_64)) {
      FUN_1000_8bd6(0x181f,iStack_64);
      iVar8 = FUN_1000_8f2a(0x181f);
      iStack_88 = iVar7;
      FUN_1000_8bd6(0x181f,iVar7);
      uStack_96 = (int)uStack_68 / 10;
      iStack_c0 = uStack_96 * (*(byte *)0x53a6 - 2) + ((int)uStack_68 >> 1);
      iStack_c2 = 0;
      for (iStack_a4 = 0; iStack_a4 < 0x10; iStack_a4 = iStack_a4 + 1) {
        iStack_b6 = iVar8 - *(int *)((iStack_64 * 0x65 + iStack_a4) * 2 + 0x5de0);
        if (*(int *)(*(int *)0x8542 + iStack_a4 * 2 + 0x9a) < iStack_b6) {
          iStack_b6 = *(int *)(*(int *)0x8542 + iStack_a4 * 2 + 0x9a);
        }
        iStack_66 = iStack_b6 * (uint)*(byte *)(iStack_a4 + param_3 * 0x10 + -0x7b44);
        if ((iStack_c0 <= iStack_66) && (iStack_c2 <= iStack_66)) {
          iStack_b4 = iStack_a4;
          iStack_c2 = iStack_66;
          iStack_8a = iStack_b6;
        }
      }
    }
  }
  iStack_ba = 0;
  if (uStack_ae != 0) {
    iStack_ba = -2;
  }
  iStack_ca = -1;
  iStack_a4 = 0;
  do {
    FUN_1000_8c32(0x181f,iStack_a4);
    if (((*(byte *)(*(int *)0x8d4e + 3) & 0x80) == 0) &&
       ((iVar7 = FUN_1000_84fc(0x181f,iStack_a4,param_3), 0x4a < iVar7 ||
        (uVar6 = FUN_1000_8c28(0x181f,param_2,iStack_a4 + 4), (uVar6 & 2) != 0)))) {
      if (*(int *)(param_3 * 2 + -0x6be4) < (int)(uint)*(byte *)(iStack_a4 + -0x6e7c)) {
        iStack_ba = iStack_ba + 1;
      }
      iStack_ba = iStack_ba + 1;
      iVar7 = FUN_1000_84fc(0x181f,iStack_a4,param_2);
      if (iVar7 < 0x4b) {
        iVar7 = iStack_a4 + 4;
        uVar6 = FUN_1000_8c28(0x181f,param_2,iVar7);
        if ((uVar6 & 2) == 0) {
          iStack_ca = iVar7;
        }
      }
    }
    iStack_a4 = iStack_a4 + 1;
  } while (iStack_a4 < 8);
  iStack_a4 = 0;
  do {
    if ((((iStack_a4 != param_2) && (iStack_a4 != param_3)) && (iStack_a4 != *(int *)0x53d2)) &&
       (bVar4 = FUN_1000_8c28(0x181f,param_3,iStack_a4), (bVar4 & 0x60) == 0x20)) {
      if (*(int *)(param_3 * 2 + -0x6be4) < *(int *)(iStack_a4 * 2 + -0x6be4) * 4) {
        iStack_ba = iStack_ba + 1;
      }
      if (*(uint *)(param_3 * 2 + -0x6be4) < *(uint *)(iStack_a4 * 2 + -0x6be4)) {
        iStack_ba = iStack_ba + 1;
      }
      uVar6 = FUN_1000_8c28(0x181f,param_2,iStack_a4);
      if ((uVar6 & 0x40) != 0) {
        iStack_ca = iStack_a4;
      }
    }
    iStack_a4 = iStack_a4 + 1;
  } while (iStack_a4 < 4);
  iStack_ba = iStack_ba - *(char *)(param_3 * 3 + -0x6a9a);
  if (*(uint *)(param_2 * 2 + -0x6be4) < *(uint *)(param_3 * 2 + -0x6be4)) {
    iStack_ba = iStack_ba + -1;
  }
  if ((iStack_c == 0) &&
     (uVar6 = FUN_1000_8c28(0x181f,param_2,param_3), (int)(uint)((uVar6 & 0x40) == 0) < iStack_ba))
  {
    iStack_a8 = 0;
    iVar7 = FUN_1000_86c4(0x181f,0,1);
    if (iVar7 != 0) {
      uStack_68 = 0;
    }
  }
  if (((param_6 == 0) && (iStack_a8 == 0)) && (iStack_ce != 0)) {
    unaff_CS = 0x181f;
    uVar6 = FUN_1000_8c28(0x181f,param_2,param_3);
    if ((uVar6 & 0x40) != 0) goto LAB_OVL16_L0040__0034de;
  }
  if (((iStack_a8 != 0) && (uVar6 = FUN_1000_8c28(0x181f,param_2,param_3), (uVar6 & 0x40) != 0)) &&
     ((uStack_ae != 0 || (*(uint *)(param_3 * 2 + -0x6be4) < *(uint *)(param_2 * 2 + -0x6be4))))) {
    iStack_b0 = 1;
    iStack_a8 = 0;
    bVar12 = false;
  }
  if (bVar12 == false) {
    uVar10 = 0x18b0;
  }
  else {
    uVar10 = 0x18b5;
  }
  FUN_0000_d9b4(auStack_80,uVar10);
  auStack_5c[0] = 0;
  FUN_1000_835e(0xd1d,auStack_5c,*(undefined2 *)((uint)*(byte *)0x53a6 * 2 + -0x7c6c));
  FUN_1000_8368(0x181f,auStack_5c);
  FUN_0000_d974(auStack_5c,param_2 * 0x34 + 0x540e);
  FUN_1000_8606(0,auStack_5c);
  FUN_1000_8606(1,param_3 * 0x34 + 0x5426);
  FUN_OVL16_L0040__003bd5(0x181f,2,0x18bb,param_3);
  FUN_OVL16_L0040__003bd5(0x181f,3,0x18c1,param_3);
  FUN_0000_d9b4(auStack_5c,0x18c7);
  uVar6 = FUN_1000_8c28(0xd1d,param_2,param_3);
  if ((uVar6 & 0x20) == 0) {
    if ((*(byte *)(param_4 * 0x1c + 0x3146) < 0xd) || (0x12 < *(byte *)(param_4 * 0x1c + 0x3146))) {
      puVar14 = (undefined1 *)0x18d2;
    }
    else {
      puVar14 = (undefined1 *)0x18cd;
    }
  }
  else {
    puVar14 = auStack_80;
  }
  FUN_0000_d974(auStack_5c,puVar14);
  if (iStack_9c != 0) {
    FUN_0000_d9b4(auStack_5c,0x18d8);
  }
  uVar10 = 0x1a1f;
  func_0x0001a878(0xd1d,auStack_5c,param_3);
  if ((-1 < iStack_ca) && (uStack_9e == 0)) {
    uVar10 = 0x181f;
    uVar6 = FUN_1000_8c28(0x1a1f,param_2,iStack_ca);
    if ((uVar6 & 0x20) != 0) {
      uVar10 = func_0x00018b94(0x181f,iStack_ca);
      FUN_1000_8628(0,uVar10);
      if (iStack_ca < 4) {
        uVar11 = 0xd1d;
        FUN_0000_d9b4(auStack_5c,0x18e1);
        FUN_OVL16_L0040__003be4(0xd1d,1,bVar12 == false);
        if (iStack_9c != 0) {
          uVar11 = 0xd1d;
          FUN_0000_d974(auStack_5c,0x17e8);
        }
      }
      else {
        FUN_0000_d9b4(auStack_5c,0x18eb);
        uVar10 = func_0x00018b94(0xd1d,iStack_ca);
        FUN_1000_8628(1,uVar10);
        if (iStack_9c == 0) {
          uVar10 = FUN_1000_8c0a(iStack_ca);
          uVar11 = 0x181f;
          FUN_1000_8628(0,uVar10);
        }
        else {
          FUN_0000_d974(auStack_5c,0x17e8);
          uVar11 = 0x181f;
          FUN_1000_8606(0,param_3 * 0x34 + 0x5426);
        }
      }
      uVar10 = 0x1a1f;
      iStack_a = func_0x0001a878(uVar11,auStack_5c,param_3);
      if (iStack_a == 2) {
        iStack_a8 = 0;
        iVar7 = FUN_1000_86c4(0x1a1f,0,1);
        if (iVar7 != 0) {
          uStack_68 = 0;
        }
        if (iStack_ca < 4) {
          uVar10 = 0x181f;
          FUN_1000_8c00(param_2,iStack_ca,0x40);
          pbVar3 = (byte *)(param_2 + iStack_ca * 0x13c + -0x77c4);
          *pbVar3 = *pbVar3 | 2;
        }
        else {
          uVar10 = 0x181f;
          FUN_1000_8f5c(iStack_ca + -4,param_2,100,0);
        }
      }
    }
  }
  uVar11 = 0x181f;
  uVar6 = FUN_1000_8c28(uVar10,param_3,param_2);
  if ((((uVar6 & 0x80) != 0) && (uStack_9e == 0)) && (*(char *)(param_2 * 0x13 + -0x6da4) != '\0'))
  {
    FUN_OVL16_L0040__003bd5(0x181f,0,0x18f3,param_3);
    uVar10 = func_0x00018b94(0x181f,param_2);
    FUN_1000_8628(1,uVar10);
    iVar7 = param_3 * 0x34 + 0x5426;
    FUN_1000_8606(2,iVar7);
    FUN_OVL16_L0040__003be4(0x181f,3,bVar12);
    uVar10 = 0xd1d;
    FUN_0000_d9b4(auStack_5c,0x18fa);
    if (iStack_9c != 0) {
      FUN_0000_d974(auStack_5c,0x17e8);
      uVar10 = 0x181f;
      FUN_1000_8606(0,iVar7);
    }
    uVar11 = 0x1a1f;
    iStack_a = func_0x0001a878(uVar10,auStack_5c,param_3);
    if (iStack_a == 2) {
      iStack_aa = param_4;
      iStack_98 = 0;
      for (param_4 = 0; param_4 < *(int *)0x539c; param_4 = param_4 + 1) {
        if (((*(byte *)(param_4 * 0x1c + 0x3147) & 0xf) == (byte)param_2) &&
           (*(char *)(param_4 * 0x1c + 0x3146) == '\x10')) {
          if ((byte)(*(char *)(param_4 * 0x1c + 0x3144) - (byte)param_2) != -0x14) {
            iStack_98 = iStack_98 + 1;
          }
          FUN_1000_8b10(uVar11,param_4);
          uVar11 = 0x181f;
          FUN_1000_8b38(0x181f,param_4,param_2 + -0x14,param_2 + -0x14);
          *(undefined1 *)(param_4 * 0x1c + 0x314d) = *(undefined1 *)(param_2 * 0x13c + -0x77c6);
          *(undefined1 *)(param_4 * 0x1c + 0x314e) = *(undefined1 *)(param_2 * 0x13c + -0x77c5);
        }
      }
      pbVar3 = (byte *)(param_2 + param_3 * 0x13c + -0x77c4);
      *pbVar3 = *pbVar3 & 0x7f;
      uVar10 = uVar11;
      if (iStack_98 != 0) {
        uVar10 = 0x181f;
        iVar7 = FUN_1000_86c4(uVar11,0,iStack_98);
        if (iVar7 != 0) {
          iStack_a8 = 0;
        }
      }
      uStack_68 = (int)uStack_68 / (iStack_98 + 1);
      uVar11 = uVar10;
    }
  }
  if (uStack_9e == 0) {
    uVar10 = uVar11;
    if (iStack_8 < (int)(uint)(*(byte *)(param_3 + -0x6bf4) >> 2)) {
      if (0xc < iStack_8) {
        uVar10 = 0x181f;
        iVar7 = FUN_1000_86c4(uVar11,0,4);
        uVar11 = uVar10;
        if (iVar7 == 0) goto LAB_OVL16_L0040__002641;
      }
    }
    else {
LAB_OVL16_L0040__002641:
      FUN_OVL16_L0040__003bd5(uVar10,0,0x1901,param_3);
      uVar10 = func_0x00018b94(uVar10,param_2);
      FUN_1000_8628(1,uVar10);
      uVar10 = func_0x00018b94(0x181f,param_3);
      FUN_1000_8628(2,uVar10);
      FUN_OVL16_L0040__003be4(0x181f,3,bVar12);
      uVar10 = 0xd1d;
      FUN_0000_d9b4(auStack_5c,0x1908);
      if (iStack_9c != 0) {
        FUN_0000_d974(auStack_5c,0x17e8);
        uVar10 = 0x181f;
        FUN_1000_8606(0,param_3 * 0x34 + 0x5426);
      }
      iStack_a = func_0x0001a878(uVar10,auStack_5c,param_3);
      uVar11 = 0x1a1f;
      if (iStack_a == 2) {
        uStack_68 = uStack_68 + iStack_8 * -100;
        if ((int)uStack_68 < 0) {
          uStack_68 = 0;
        }
        iStack_be = 1;
        iStack_a8 = 0;
        for (param_4 = 0; param_4 < *(int *)0x539c; param_4 = param_4 + 1) {
          uVar10 = uVar11;
          if ((((*(byte *)(param_4 * 0x1c + 0x3147) & 0xf) == (byte)param_2) &&
              ((*(byte *)(param_4 * 0x1c + 0x3146) < 0xd ||
               (0x12 < *(byte *)(param_4 * 0x1c + 0x3146))))) &&
             (1 < *(byte *)((uint)*(byte *)(param_4 * 0x1c + 0x3146) * 0xe + 0x5236))) {
            uStack_9a = (uint)*(byte *)(param_4 * 0x1c + 0x3144);
            uStack_a2 = (uint)*(byte *)(param_4 * 0x1c + 0x3145);
            iStack_84 = 0;
            uVar10 = 0x181f;
            iVar7 = FUN_1000_84f2(uVar11,uStack_9a,uStack_a2);
            if (iVar7 != 0) {
              for (iStack_90 = 0; (uVar10 = 0x181f, iStack_84 == 0 && (iStack_90 < 8));
                  iStack_90 = iStack_90 + 1) {
                iStack_60 = (int)*(char *)(iStack_90 + 0xbe) + uStack_a2;
                iStack_4 = (int)*(char *)(iStack_90 + 0xb4) + uStack_9a;
                iVar7 = FUN_1000_8886(0x181f,iStack_4,iStack_60);
                if (iVar7 == param_3) {
                  iStack_84 = 1;
                }
              }
              if (iStack_84 != 0) {
                uVar10 = 0x181f;
                FUN_1000_8a70(0x181f,param_4,param_2 + -0x14,param_2 + -0x14);
                *(undefined1 *)(param_4 * 0x1c + 0x314d) =
                     *(undefined1 *)(param_2 * 0x13c + -0x77c6);
                *(undefined1 *)(param_4 * 0x1c + 0x314e) =
                     *(undefined1 *)(param_2 * 0x13c + -0x77c5);
              }
            }
          }
          uVar11 = uVar10;
        }
      }
    }
  }
  uVar6 = uStack_68;
  if ((uStack_68 != 0) && (bVar12 == true)) {
    iVar8 = (int)uStack_68 >> 0xf;
    iVar7 = *(int *)(*(int *)0x84fc + 0x2c);
    if ((iVar8 <= iVar7) && ((iVar8 < iVar7 || (uStack_68 < *(uint *)(*(int *)0x84fc + 0x2a))))) {
      FUN_OVL16_L0040__003bd5(uVar11,0,0x190f,param_3);
      uVar10 = func_0x00018b94(uVar11,param_2);
      FUN_1000_8628(1,uVar10);
      iVar7 = param_3 * 0x34 + 0x5426;
      FUN_1000_8606(2,iVar7);
      FUN_1000_8b9e(0,uVar6,iVar8);
      uVar10 = 0xd1d;
      FUN_0000_d9b4(auStack_5c,0x1916);
      if (iStack_9c != 0) {
        FUN_0000_d974(auStack_5c,0x17e8);
        uVar10 = 0x181f;
        FUN_1000_8606(0,iVar7);
      }
      uVar11 = 0x1a1f;
      iStack_a = func_0x0001a878(uVar10,auStack_5c,param_3);
      if (iStack_a == 2) {
        iVar7 = *(int *)0x84fc;
        puVar1 = (uint *)(iVar7 + 0x2a);
        uVar6 = *puVar1;
        *puVar1 = *puVar1 - uStack_68;
        piVar2 = (int *)(iVar7 + 0x2c);
        *piVar2 = (*piVar2 - ((int)uStack_68 >> 0xf)) - (uint)(uVar6 < uStack_68);
        puVar1 = (uint *)(param_3 * 0x13c + -0x77ce);
        uVar6 = *puVar1;
        *puVar1 = *puVar1 + uStack_68;
        piVar2 = (int *)(param_3 * 0x13c + -0x77cc);
        *piVar2 = *piVar2 + ((int)uStack_68 >> 0xf) + (uint)CARRY2(uVar6,uStack_68);
        iStack_a8 = 0;
      }
      uStack_68 = 999;
    }
  }
  if (((bVar12 != false) && (uStack_68 != 999)) && (-1 < iStack_b4)) {
    FUN_OVL16_L0040__003bd5(uVar11,0,0x191e,param_3);
    FUN_1000_8b9e(0,iStack_8a,iStack_8a >> 0xf);
    FUN_1000_8628(1,*(undefined2 *)(iStack_b4 * 2 + -0x6840));
    uVar10 = func_0x00018b94(0x181f,param_3);
    FUN_1000_8628(2,uVar10);
    uVar10 = 0xd1d;
    FUN_0000_d9b4(auStack_5c,0x1926);
    if (iStack_9c != 0) {
      FUN_0000_d974(auStack_5c,0x17e8);
      uVar10 = 0x181f;
      FUN_1000_8606(0,param_3 * 0x34 + 0x5426);
    }
    uVar11 = 0x1a1f;
    iStack_a = func_0x0001a878(uVar10,auStack_5c,param_3);
    if (iStack_a == 2) {
      piVar2 = (int *)((iStack_88 * 0x65 + iStack_a4) * 2 + 0x5de0);
      *piVar2 = *piVar2 - iStack_8a;
      piVar2 = (int *)((iStack_64 * 0x65 + iStack_a4) * 2 + 0x5de0);
      *piVar2 = *piVar2 + iStack_8a;
      iStack_a8 = 0;
    }
  }
  uVar10 = uVar11;
  if (iStack_a8 == 0) {
LAB_OVL16_L0040__0029fc:
    if ((iStack_a8 != 0) && (uStack_68 == 999)) {
      FUN_OVL16_L0040__003bd5(uVar10,0,0x1938,param_3);
      FUN_1000_8606(1,param_3 * 0x34 + 0x5426);
      FUN_1000_869c(0x181f,4);
      uVar10 = 0x1940;
      goto LAB_OVL16_L0040__0029e1;
    }
    if (iStack_a8 != 0) {
      FUN_OVL16_L0040__003bd5(uVar10,0,0x1949,param_3);
      FUN_1000_8606(1,param_3 * 0x34 + 0x5426);
      FUN_0000_d9b4(auStack_5c,0x1951);
      if (iStack_9c != 0) {
        FUN_0000_d974(auStack_5c,0x17e8);
      }
      uVar10 = 0x1a1f;
      func_0x0001a878(0xd1d,auStack_5c,param_3);
    }
  }
  else {
    uVar10 = 0x181f;
    uVar6 = FUN_1000_8c28(uVar11,param_2,param_3);
    if (((uVar6 & 0x40) == 0) || ((int)uStack_68 < 0x65)) goto LAB_OVL16_L0040__0029fc;
    uVar10 = 0x1930;
LAB_OVL16_L0040__0029e1:
    func_0x0001a878(0x181f,uVar10,param_3);
    uVar10 = 0x181f;
    FUN_1000_8c00(param_2,param_3,0x40);
  }
  bVar12 = false;
  if (iStack_a8 == 0) {
    uVar6 = FUN_1000_8c28(uVar10,param_2,param_3);
    uVar10 = 0x181f;
    if ((uVar6 & 0x40) != 0) goto LAB_OVL16_L0040__002c6c;
    FUN_OVL16_L0040__003bd5(0x181f,0,0x1955,param_3);
    uVar10 = func_0x00018b94(0x181f,param_2);
    FUN_1000_8628(1,uVar10);
    uVar10 = func_0x00018b94(0x181f,param_3);
    uVar11 = 0x181f;
    FUN_1000_8628(2,uVar10);
    if (iStack_9c == 0) {
      uVar11 = 0x1a1f;
      iStack_a = func_0x0001a878(0x181f,0x195d,param_3);
    }
    else {
      iStack_a = 1;
    }
    if (iStack_a == 1) {
      FUN_1000_8bf6(param_2,param_3,0x40);
      *(int *)(param_3 * 2 + 0x53c8) = *(int *)0x538e + 0x10;
      uVar10 = 0xd1d;
      FUN_0000_d9b4(auStack_5c,0x1964);
    }
    else {
      if (uStack_ae == 0) {
        iVar8 = param_3 * 0x13c;
        uVar10 = FUN_0000_e096(*(undefined2 *)(iVar8 + -0x77ce),*(undefined2 *)(iVar8 + -0x77cc),100
                               ,0);
        uVar11 = 0x181f;
        iVar7 = func_0x0001854c(0xd1d,(iStack_ce + -2) * 2,0,uVar10);
        uStack_96 = iVar7 * 100;
        if (uStack_96 != 0) {
          FUN_0000_d9b4(auStack_80,0x196a);
          uVar6 = uStack_96;
          iVar7 = (int)uStack_96 >> 0xf;
          FUN_1000_8b9e(0,uStack_96,iVar7);
          uVar11 = 0x1a1f;
          iStack_a = func_0x0001a878(0x181f,0x196f,param_3);
          if (iStack_a == 1) {
            uVar11 = 0x181f;
            FUN_1000_8bf6(param_2,param_3,0x40);
            puVar1 = (uint *)(iVar8 + -0x77ce);
            uVar9 = *puVar1;
            *puVar1 = *puVar1 - uVar6;
            *(int *)(iVar8 + -0x77cc) = (*(int *)(iVar8 + -0x77cc) - iVar7) - (uint)(uVar9 < uVar6);
            iVar8 = *(int *)0x84fc;
            puVar1 = (uint *)(iVar8 + 0x2a);
            uVar9 = *puVar1;
            *puVar1 = *puVar1 + uVar6;
            piVar2 = (int *)(iVar8 + 0x2c);
            *piVar2 = *piVar2 + iVar7 + (uint)CARRY2(uVar9,uVar6);
          }
        }
      }
      uVar10 = 0x181f;
      uVar6 = FUN_1000_8c28(uVar11,param_2,param_3);
      if ((uVar6 & 0x40) == 0) {
        FUN_0000_d9b4(auStack_5c,0x1978);
        FUN_0000_d974(auStack_5c,auStack_80);
        FUN_OVL16_L0040__003bd5(0xd1d,0,0x197c,param_3);
        FUN_1000_8606(1,param_3 * 0x34 + 0x5426);
        FUN_1000_869c(0x181f,4);
        uVar10 = 0x1a1f;
        func_0x0001a878(0x181f,auStack_5c,param_3);
      }
    }
  }
  else {
LAB_OVL16_L0040__002c6c:
    if (iStack_a8 == 0) {
      uVar10 = 0xd1d;
      FUN_0000_d9b4(auStack_5c,0x1984);
      bVar12 = true;
    }
  }
  uVar11 = 0x181f;
  uVar6 = FUN_1000_8c28(uVar10,param_2,param_3);
  if ((uVar6 & 0x40) != 0) {
    FUN_OVL16_L0040__003bd0(0x181f,param_2,param_3);
    FUN_OVL16_L0040__003bd0(0x181f,param_3,param_2);
  }
  if (iStack_a8 == 0) {
    uVar11 = 0x181f;
    uVar6 = FUN_1000_8c28(0x181f,param_2,param_3);
    if ((uVar6 & 0x40) != 0) {
      uVar10 = func_0x00018b94(0x181f,param_2);
      FUN_1000_8628(0,uVar10);
      uVar10 = func_0x00018b94(0x181f,param_3);
      FUN_1000_8628(1,uVar10);
      uVar10 = 0xd1d;
      FUN_0000_d974(auStack_5c,auStack_80);
      if (bVar12) {
        FUN_1000_8628(2,*(undefined2 *)((uint)*(byte *)0x53a6 * 2 + -0x7c6c));
        uVar10 = 0x181f;
        FUN_1000_8606(3,param_2 * 0x34 + 0x540e);
      }
      if (iStack_9c != 0) {
        FUN_0000_d9b4(auStack_5c,0x198d);
        FUN_1000_8606(0,param_3 * 0x34 + 0x5426);
        uVar11 = FUN_1000_8c0a(param_2);
        uVar10 = 0x181f;
        FUN_1000_8628(1,uVar11);
      }
      uVar11 = 0x1a1f;
      iStack_a = func_0x0001a878(uVar10,auStack_5c,param_3);
      if (iStack_a == 2) {
        iStack_84 = 0;
        if (iStack_62 == 0) {
          uVar10 = 0x1996;
LAB_OVL16_L0040__002daa:
          uVar17 = 0x1a1f;
          func_0x0001a878(uVar11,uVar10,param_3);
        }
        else if ((iStack_ce == 0) || (iStack_b0 != 0)) {
          iVar7 = (*(byte *)0x53a6 + 2) * iStack_b2;
          uStack_a0 = iVar7 * 0x19;
          if (uStack_ae != 0) {
            uStack_a0 = iVar7 * 0x32;
          }
          if (iStack_94 != 0) {
            uStack_a0 = uStack_a0 + ((int)uStack_a0 >> 1);
          }
          if (iStack_be != 0) {
            uStack_a0 = uStack_a0 + iStack_8 * -0x32;
          }
          iVar7 = FUN_1000_89a4(0x1a1f,param_2,0x13);
          if (iVar7 != 0) {
            uStack_a0 = (int)uStack_a0 >> 1;
          }
          uVar10 = func_0x00018b94(0x181f,param_3);
          uVar11 = 0x181f;
          FUN_1000_8628(0,uVar10);
          uVar6 = uStack_a0;
          if ((int)uStack_a0 < 100) {
            uVar6 = 100;
          }
          iVar8 = (int)uVar6 >> 0xf;
          iVar7 = *(int *)(*(int *)0x84fc + 0x2c);
          uStack_a0 = uVar6;
          if ((iVar7 < iVar8) ||
             (((iVar7 <= iVar8 && (*(uint *)(*(int *)0x84fc + 0x2a) < uVar6)) || (iStack_b0 != 0))))
          {
            uVar10 = 0x19af;
            goto LAB_OVL16_L0040__002daa;
          }
          FUN_1000_8b9e(0,uVar6,iVar8);
          uVar17 = 0x1a1f;
          iVar7 = func_0x0001a878(0x181f,0x19bb,param_3);
          if (iVar7 == 1) {
            iVar5 = *(int *)0x84fc;
            puVar1 = (uint *)(iVar5 + 0x2a);
            uVar9 = *puVar1;
            *puVar1 = *puVar1 - uVar6;
            piVar2 = (int *)(iVar5 + 0x2c);
            *piVar2 = (*piVar2 - iVar8) - (uint)(uVar9 < uVar6);
            puVar1 = (uint *)(param_3 * 0x13c + -0x77ce);
            uVar9 = *puVar1;
            *puVar1 = *puVar1 + uVar6;
            piVar2 = (int *)(param_3 * 0x13c + -0x77cc);
            *piVar2 = *piVar2 + iVar8 + (uint)CARRY2(uVar9,uVar6);
            iStack_84 = 1;
          }
          if (iVar7 == 2) {
            iStack_5e = (uint)*(byte *)(param_3 + -0x6bd4) + (uint)*(byte *)(param_2 + -0x6bd4);
            if (uStack_ae != 0) {
              iStack_5e = iStack_5e * 2;
            }
            uVar11 = 0x181f;
            iVar7 = FUN_1000_86c4(0x1a1f,0,iStack_5e);
            if (iVar7 <= (int)(uint)*(byte *)(param_2 + -0x6bd4)) {
              uVar10 = 0x19c9;
              goto LAB_OVL16_L0040__002dca;
            }
            FUN_0000_d9b4(auStack_5c,0x19d2);
            if (iStack_9c != 0) {
              FUN_0000_d9b4(auStack_80,0x19d6);
            }
            FUN_0000_d974(auStack_5c,auStack_80);
            FUN_OVL16_L0040__003bd5(0xd1d,0,0x19dc,param_3);
            FUN_1000_8606(1,param_3 * 0x34 + 0x5426);
            FUN_1000_869c(0x181f,4);
            func_0x0001a878(0x181f,auStack_5c,param_3);
            uVar17 = 0x181f;
            FUN_1000_8c00(param_2,param_3,0x40);
          }
        }
        else {
          uVar10 = 0x19a6;
LAB_OVL16_L0040__002dca:
          uVar17 = 0x1a1f;
          func_0x0001a878(uVar11,uVar10,param_3);
          iStack_84 = 1;
        }
        uVar11 = uVar17;
        if (iStack_84 != 0) {
          *(undefined2 *)0x1740 = 1;
          for (param_4 = 0; uVar11 = uVar17, param_4 < *(int *)0x539c; param_4 = param_4 + 1) {
            uVar10 = uVar17;
            if (((*(byte *)(param_4 * 0x1c + 0x3147) & 0xf) == (byte)param_3) &&
               ((*(byte *)(param_4 * 0x1c + 0x3146) < 0xd ||
                (0x12 < *(byte *)(param_4 * 0x1c + 0x3146))))) {
              uStack_9a = (uint)*(byte *)(param_4 * 0x1c + 0x3144);
              uStack_a2 = (uint)*(byte *)(param_4 * 0x1c + 0x3145);
              iStack_84 = 0;
              uVar10 = 0x181f;
              iVar7 = FUN_1000_84f2(uVar17,uStack_9a,uStack_a2);
              if (iVar7 != 0) {
                for (iStack_90 = 0; (uVar10 = 0x181f, iStack_84 == 0 && (iStack_90 < 8));
                    iStack_90 = iStack_90 + 1) {
                  iStack_60 = (int)*(char *)(iStack_90 + 0xbe) + uStack_a2;
                  iStack_4 = (int)*(char *)(iStack_90 + 0xb4) + uStack_9a;
                  iVar7 = FUN_1000_8886(0x181f,iStack_4,iStack_60);
                  if (iVar7 == param_2) {
                    iStack_84 = 1;
                  }
                }
                if (iStack_84 != 0) {
                  uVar10 = 0x181f;
                  FUN_1000_8a70(0x181f,param_4,param_3 + -0x14,param_3 + -0x14);
                  *(undefined1 *)(param_4 * 0x1c + 0x314d) =
                       *(undefined1 *)(param_3 * 0x13c + -0x77c6);
                  *(undefined1 *)(param_4 * 0x1c + 0x314e) =
                       *(undefined1 *)(param_3 * 0x13c + -0x77c5);
                }
              }
            }
            uVar17 = uVar10;
          }
        }
      }
      if (iStack_a == 3) {
        iVar7 = FUN_1000_89a4(uVar11,param_2,0x13);
        if ((iVar7 != 0) && (iVar7 = FUN_1000_86c4(0x181f,0,2), iVar7 == 0)) {
          iStack_ce = iStack_ce + 1;
        }
        iVar8 = param_3 * 0x13c;
        uVar10 = FUN_0000_e096(*(undefined2 *)(iVar8 + -0x77ce),*(undefined2 *)(iVar8 + -0x77cc),100
                               ,0);
        iVar7 = func_0x0001854c(0xd1d,iStack_ce,0,uVar10);
        uVar6 = iVar7 * 100;
        uStack_96 = uVar6;
        if ((int)uVar6 < 1) {
          if (iStack_94 == 0) {
            iVar7 = 0x19f2;
            iVar8 = param_3;
          }
          else {
            iVar8 = 4;
            FUN_1000_869c(0x181f,4);
            FUN_1000_8c00(param_2,param_3,0x40);
            iStack_ce = 0x19ea;
            iVar7 = param_3;
          }
          uVar11 = 0x1a1f;
          func_0x0001a878(0x181f,iStack_ce,iVar7,iVar8);
        }
        else {
          iVar5 = (int)uVar6 >> 0xf;
          FUN_1000_8b9e(0,uVar6,iVar5);
          uVar11 = 0x1a1f;
          func_0x0001a878(0x181f,0x19e4,param_3);
          puVar1 = (uint *)(iVar8 + -0x77ce);
          uVar9 = *puVar1;
          *puVar1 = *puVar1 + iVar7 * -100;
          *(int *)(iVar8 + -0x77cc) = (*(int *)(iVar8 + -0x77cc) - iVar5) - (uint)(uVar9 < uVar6);
          iVar7 = *(int *)0x84fc;
          puVar1 = (uint *)(iVar7 + 0x2a);
          uVar9 = *puVar1;
          *puVar1 = *puVar1 + uVar6;
          piVar2 = (int *)(iVar7 + 0x2c);
          *piVar2 = *piVar2 + iVar5 + (uint)CARRY2(uVar9,uVar6);
        }
      }
      if (iStack_a == 4) {
        unaff_CS = 0x191f;
        uStack_6c = func_0x00019372(uVar11);
        if (uStack_6c == 0) goto LAB_OVL16_L0040__0034de;
        uStack_96 = 0;
        uVar10 = unaff_CS;
        for (iStack_a4 = 0; iStack_a4 < 0xc; iStack_a4 = iStack_a4 + 1) {
          if (iStack_a4 < 4) {
            if ((iStack_a4 != param_2) && (iStack_a4 != param_3)) {
              uVar6 = FUN_1000_8c28(uVar10,param_2,iStack_a4);
              goto joined_r0x000031e6;
            }
          }
          else {
            FUN_1000_8c32(uVar10,iStack_a4);
            uVar10 = 0x181f;
            if ((*(byte *)(*(int *)0x8d4e + 3) & 0x80) == 0) {
              uVar6 = FUN_1000_8c28(0x181f,param_2,iStack_a4);
joined_r0x000031e6:
              uVar10 = 0x181f;
              if ((uVar6 & 0x20) != 0) {
                uVar10 = FUN_1000_8c0a(iStack_a4,iStack_a4 + 1);
                uVar13 = FUN_1000_8212(0x181f,uVar10);
                uVar10 = 0x191f;
                func_0x00019366(0x181f,uStack_6c,uVar13);
                uStack_96 = 1;
              }
            }
          }
        }
        if (uStack_96 == 0) {
          unaff_CS = 0x191f;
          func_0x00019398(uVar10,uStack_6c);
          goto LAB_OVL16_L0040__0034de;
        }
        iStack_a = FUN_1000_935a(uStack_6c);
        uVar11 = 0x191f;
        func_0x00019398(0x191f,uStack_6c);
        iVar7 = iStack_a;
        uStack_6c._2_2_ = 0;
        uStack_6c._0_2_ = 0;
        if (iStack_a < 1) goto LAB_OVL16_L0040__0034ca;
        iVar5 = iStack_a + -1;
        uVar10 = FUN_1000_8c0a(iVar5);
        FUN_1000_8628(0,uVar10);
        iVar8 = iVar5;
        uVar6 = FUN_1000_8c28(0x181f,param_3,iVar5);
        if ((uVar6 & 0x20) == 0) {
          uVar17 = 0x1a03;
          uVar10 = 0x181f;
        }
        else {
          uVar6 = FUN_1000_8c28(0x181f,param_3,iVar5);
          if ((uVar6 & 0x40) != 0) {
            if (iVar5 < 4) {
              uVar16 = 200;
              uVar17 = 10;
              uVar11 = 0;
              uVar10 = 0x32;
              uVar13 = FUN_0000_e096(*(undefined2 *)(param_2 * 0x13c + -0x77ce),
                                     *(undefined2 *)(param_2 * 0x13c + -0x77cc),0x32,0);
              uVar13 = FUN_0000_e130((uint)*(byte *)(iVar7 + -0x6bd5) +
                                     *(int *)(iVar5 * 2 + -0x6be4),0,uVar13);
              uVar10 = FUN_0000_e096(uVar13,uVar10,uVar11);
              iVar7 = func_0x0001854c(0xd1d,uVar10,uVar17,uVar16);
              uStack_96 = iVar7 * 0x32;
              iVar7 = FUN_1000_89a4(0x181f,param_2,0x13);
              if (iVar7 != 0) {
                uStack_96 = (int)uStack_96 >> 1;
              }
              uVar10 = func_0x00018b94(0x181f,iVar5);
              FUN_1000_8628(0,uVar10);
              FUN_1000_8b9e(0,uStack_96,(int)uStack_96 >> 0xf);
              uVar10 = 0x1a27;
            }
            else {
              FUN_1000_8c32(0x181f,iVar5);
              uVar15 = 200;
              uVar16 = 10;
              uVar17 = 0;
              uVar11 = 0x32;
              uVar6 = (uint)*(byte *)(*(int *)0x8d52 + -0x6e7c);
              uVar10 = 0;
              uVar13 = FUN_0000_e096(*(undefined2 *)(param_2 * 0x13c + -0x77ce),
                                     *(undefined2 *)(param_2 * 0x13c + -0x77cc),0x32,0);
              uVar13 = FUN_0000_e130(uVar13,uVar6,uVar10);
              iVar7 = (int)((ulong)uVar13 >> 0x10);
              uVar6 = (uint)uVar13;
              uVar10 = FUN_0000_e096(uVar6 * 3,
                                     (iVar7 << 1 | (uint)((int)uVar6 < 0)) + iVar7 +
                                     (uint)CARRY2(uVar6 * 2,uVar6),uVar11,uVar17);
              iVar7 = func_0x0001854c(0xd1d,uVar10,uVar16,uVar15);
              uStack_96 = iVar7 * 0x32;
              iVar7 = FUN_1000_89a4(0x181f,param_2,0x13);
              if (iVar7 != 0) {
                uStack_96 = (int)uStack_96 >> 1;
              }
              uVar10 = func_0x00018b94(0x181f,iVar5);
              FUN_1000_8628(0,uVar10);
              FUN_1000_8b9e(0,uStack_96,(int)uStack_96 >> 0xf);
              uVar10 = 0x1a1a;
            }
            uVar11 = 0x1a1f;
            iStack_a = func_0x0001a878(0x181f,uVar10,param_3);
            if (iStack_a == 1) {
              iVar7 = *(int *)(*(int *)0x84fc + 0x2c);
              if ((iVar7 <= (int)uStack_96 >> 0xf) &&
                 ((iVar7 < (int)uStack_96 >> 0xf || (*(uint *)(*(int *)0x84fc + 0x2a) < uStack_96)))
                 ) {
                uVar17 = 0x1a33;
                uVar10 = uVar11;
                iVar8 = param_3;
                goto LAB_OVL16_L0040__003290;
              }
              FUN_1000_8c00(param_3,iVar5,0x40);
              if (iVar5 < 4) {
                pbVar3 = (byte *)(param_3 + iVar5 * 0x13c + -0x77c4);
                *pbVar3 = *pbVar3 | 2;
              }
              else {
                FUN_1000_8bf6(param_3,iVar5,2);
              }
              uVar10 = FUN_1000_8c0a(param_3);
              FUN_1000_8628(0,uVar10);
              uVar10 = FUN_1000_8c0a(iVar5);
              FUN_1000_8628(1,uVar10);
              FUN_1000_869c(0x181f,4);
              uVar11 = 0x1a1f;
              func_0x0001a878(0x181f,0x1a3f,param_3);
              iVar7 = *(int *)0x84fc;
              puVar1 = (uint *)(iVar7 + 0x2a);
              uVar6 = *puVar1;
              *puVar1 = *puVar1 - uStack_96;
              piVar2 = (int *)(iVar7 + 0x2c);
              *piVar2 = (*piVar2 - ((int)uStack_96 >> 0xf)) - (uint)(uVar6 < uStack_96);
              puVar1 = (uint *)(param_3 * 0x13c + -0x77ce);
              uVar6 = *puVar1;
              *puVar1 = *puVar1 + uStack_96;
              piVar2 = (int *)(param_3 * 0x13c + -0x77cc);
              *piVar2 = *piVar2 + ((int)uStack_96 >> 0xf) + (uint)CARRY2(uVar6,uStack_96);
            }
            goto LAB_OVL16_L0040__0034ca;
          }
          uVar17 = 0x1a0d;
          uVar10 = 0x181f;
        }
LAB_OVL16_L0040__003290:
        uVar11 = 0x1a1f;
        func_0x0001a878(uVar10,uVar17,iVar8);
      }
    }
  }
LAB_OVL16_L0040__0034ca:
  unaff_CS = uVar11;
  uStack_6c = CONCAT22(uStack_6c._2_2_,(undefined2)uStack_6c);
  if (iStack_b0 != 0) {
    pbVar3 = (byte *)(param_2 + param_3 * 0x13c + -0x77c4);
    *pbVar3 = *pbVar3 | 8;
    uStack_6c = CONCAT22(uStack_6c._2_2_,(undefined2)uStack_6c);
  }
LAB_OVL16_L0040__0034de:
  uVar6 = FUN_1000_8c28(unaff_CS,param_2,param_3);
  if ((uVar6 & 0x40) != 0) {
    uStack_96 = (*(byte *)0x53a6 - 6) * -2;
    iVar7 = FUN_1000_89a4(0x181f,param_2,0x13);
    if (iVar7 != 0) {
      uStack_96 = (int)uStack_96 >> 1;
    }
    *(undefined1 *)(param_2 + param_3 * 0x13c + -0x77b8) = (undefined1)uStack_96;
  }
  return uStack_8e;
}

```
