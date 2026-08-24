# Euro per-unit act (`FUN_521d_5b66`) — thin section-map

## Correction (2026-08-13) — true function is a tiny dispatcher, not 1815 lines

`FUN_521d_5b66` carried a Ghidra disassembly-fault warning in the canonical
export (`Instruction at (ram,0x00057701) overlaps instruction at
(ram,0x000576ff)` + `Unable to track spacebase fully for stack` + 2×
`Removing unreachable block` — `docs/decomp_inventory.md`), and line 17
below already flagged the symptom ("Decomp shows corrupted far prototype").
Re-disassembled directly via the overlay-addressing project
(`docs/rtlink_decode_v2_gap.md`, `tools/address_mapping.csv` →
`OVL14_L0000:5b66`): the real function is **198 bytes**, clean, no warnings:

```c
void FUN_521d_5b66(undefined2 param_1, int param_2)
{
  char *pcVar1;
  int iVar2;
  undefined2 unaff_CS;
  undefined2 unaff_DS;

  iVar2 = param_2 * 0x1c;
  if ((((undefined1 *)&LAB_003149)[iVar2] != '\0') &&
     (*(char *)(iVar2 + 0x314c) == '\v')) {
    if ((*(byte *)((uint)*(byte *)(iVar2 + 0x3146) * 0xe + 0x523d) & 1) == 0)
    goto LAB_005bda;
    unaff_CS = 0x181f;
    iVar2 = FUN_1000_8b74();
    if (iVar2 == 0) goto LAB_005bda;
    if (((undefined1 *)&LAB_00314b)[param_2 * 0x1c] == 'E') {
      pcVar1 = (char *)((*(byte *)(iVar2 + 0x3147) & 0xf) + 0x9456);
      *pcVar1 = *pcVar1 + -1;
    }
  }
  iVar2 = FUN_OVL14_L0000__007308(unaff_CS,param_2);
  if (iVar2 != 0) {
    return;
  }
LAB_005bda:
  switch(*(undefined1 *)(param_2 * 0x1c + 0x314c)) {
  case 7:
    FUN_1000_93ea(param_2);
    break;
  case 8:
    func_0x000193b2(0,param_2);
    break;
  case 9:
    FUN_1000_9406(param_2);
    break;
  default:
    FUN_1000_8b24(0,param_2);
    break;
  case 0xb:
  case 0xc:
    FUN_1000_96aa(param_2);
  }
  return;
}
```

## Case dispatch targets resolved (2026-08-14)

The `FUN_1000_93ea`/`func_0x000193b2`/`FUN_1000_9406`/`FUN_1000_8b24`/
`FUN_1000_96aa` calls above are RTLink overlay-loader thunks (segment
`0x1000` resident stub → `FUN_210d_0d91`/`FUN_210d_0dab` overlay-load →
tail-call), previously flagged "still-uninvestigated" (see the method
note further down). Re-ran the same overlay-addressing recovery used for
the switch itself and traced each thunk to its real handler — all five
sit clean and uncorrupted in the canonical export already, just never
linked from `5b66` by name:

| Case | Thunk | Real handler | Lines (`viceroy_unpacked.c`) | Size |
|---|---|---|---|---|
| 7 | `FUN_291f_01fa` | `FUN_479b_076e(int)` | 76961–77048 | 88 lines |
| 8 | `FUN_291f_01c2` | `FUN_479b_01a6(int)` | 76722–76858 | 137 lines |
| 9 | `FUN_291f_0216` | `FUN_479b_0526(int)` | 76862–76957 | 96 lines |
| default (incl. state `10`) | `FUN_281f_0934` | `FUN_1427_155e(int)` | 8880–8888 | 9 lines |
| `0xb`/`0xc` | `FUN_291f_04ba` | `FUN_479b_0972(undefined2,int)` | 77052–77122 | 71 lines |

**Case `10` has no dedicated branch** — the switch only lists 7/8/9/0xb/0xc;
any other state (including the literal value 10) falls through to
`default`, which is genuinely tiny: `FUN_1427_155e` just recomputes one
byte (`FUN_1427_065a`) into `unit+0x3149` (moves-spent) and returns. The
old "case 10 ~91195+, UI/chrome-ish" phase-table entry further down this
file describes corrupted-blob content, not this — see the warning above
the phase outline.

**Cases 8 and 9 read as Pioneer terrain-improvement completion**
(clear/plow one state, road-building the other — not fully confirmed
which is which). Shared skeleton: decode terrain class at the unit's
tile, increment a per-unit "turns worked" counter (`unit+0x315a`) against
a terrain-indexed threshold table at `DS:0x2f78` (stride `0x10` — this is
the *same* table `§2d8` below already names for the `improve_timer`
stand-in, just the "+2" byte of it; the other 15 bytes/terrain-class are
still unmapped), halved when `unit+0x315b` (profession) `== 0x14` — confirmed
`UNITS_JOB_PIONEER` (`units.h:963`, "Hardy Pioneers"), independently
cross-checked multiple times elsewhere this session (e.g. the `1eca`
Continental-promote Veteran gate). Once the counter hits the
threshold: orders clear, and — gated on live colony count
(`DS:0x539e`) and the unit's nation matching the bound colony's owner —
a reward lands: case 9 is a flat `+10` to that colony's
`hammers_purchased` (`col1_save.h`'s `+0x98` field, already named, matches
`§2d13` below); case 8 is a scaled reward from a paired terrain table at
`DS:0x2f80` (stride `0x10`, offset +8 from the case-9 table), gated by
**`FUN_281f_09fc(0x24)`, resolved 2026-08-14 — this is not a founding-
father check at all** (the doc's earlier guess was wrong; its own
catalog entry already said "test building bit," just never cross-checked
against a real building id). `FUN_15eb_038e`/`035e` test `has_building`
at a numeric index into the same array `NAMES.TXT`'s `@BUILDING` section
populates in file order (confirmed against `colony.c`'s own loader,
`pool->building_types[pool->building_type_count++]`) — index `0x24`
(36, 0-based) is **Lumber Mill**. Real mechanic: a Pioneer finishing a
terrain-clear/plow/road job earns a bonus (scaled by which terrain was
cleared) only when the colony has a Lumber Mill — sensible (Lumber Mill
processes the lumber a forest-clear yields).
**2026-08-20 correction, twice-revised same day — this is the lumber-
yield-into-warehouse credit for a completed forest clear, not a gold
grant or a turn-throttled gate.** First correction: the Lumber Mill
check is a *floor*, not an on/off switch, confirmed against the raw
`FUN_479b_01a6` body directly (`viceroy_unpacked.c:76722-76858`) —
without a Lumber Mill the table read (`local_14`) is forced down to `1`,
not `0`, so a colony with **no** Lumber Mill still nets a flat `20`
lumber (`40` for a Hardy Pioneer, `local_14*20<<hardy_pioneer` — Hardy
Pioneer *doubles* this reward, opposite direction from the work-turns-
needed halving); only a Lumber Mill lets the real, still-uncaptured `+8`
byte scale it above `20`. A small `+1` bump from a separate terrain/tile
flag (`FUN_281f_0754(...) & 0xa`) applies before the mill-or-1
substitution. Second correction (retracts the first pass's "turn-
elapsed throttle" misreading): `colony+0xa4` is not a separate stamp
field at all — it's `col1_save.h`'s existing `stock[COLONIZE_CARGO_LUMBER]`
slot (`hammers_purchased`@`+0x98` + `stock[]` starting `+0x9a` +
`LUMBER`=index 5 → exactly `+0xa4`), and the function misread as "current
turn" (`FUN_281f_0d3a`) is actually **warehouse capacity**
(`FUN_15eb_0a50`, `100×(1+expansion)`, per `FUNCTION_CATALOG.md`). So the
real formula is just an ordinary warehouse-capacity clamp: `add =
clamp(local_14*20<<hardy, 0, capacity - stock[LUMBER]); stock[LUMBER] +=
add` — no gold, no throttle. This is exactly the shape Linux's existing
placeholder in `units.c` (`units_pioneer_work_tick`'s clearing branch)
already implements, just with `local_14` hardcoded to `1` (flat `20`)
instead of the real mill/terrain-scaled `+8` value.

**Case 7 is FOUND COLONY, not "Europe hire" — correcting my own guess from
last pass (traced `FUN_291f_09b2` this pass, it's not a hire-pick body).**
`FUN_479b_076e` clears order/timer fields, calls
`thunk_FUN_2a1f_01f4(nation, name_buf[80])` (colony name generation), gates
on a human CHOICE dialog (`FUN_291f_0120(0x17)` — very likely "Build a
colony here?"), then calls `FUN_291f_09b2` → `FUN_364b_1ba8`
(`viceroy_unpacked.c:58015-58131`) with `(nation, x, y, unit_index)`.
That function is a straight-line **colony-record initializer**: owner
nation (`+0x1a`), x/y (`+0/+1`), and zeroing/defaulting a long list of
already-known Col1 colony fields (`ai_flags`/`colony_flags`/
`build_ai_flags`/`warehouse_level`/`capitol_level`/`depletion_counter`/
`cargo_produced_mask`/`specialty_cargo=0xff`/`improve_timer`/
`labor_shortage`/`cargo_idle_turns`/`hammers_purchased`/cargo stock (16
slots) all cleared, building-owned array reset). **Already correctly
cross-referenced in the Linux port**: `colonies_found` (`colony.c:518`)
cites this exact function by name at line 572 for the cargo-stock-clear
behavior, and its `specialty_cargo=0xff`/`building_in_production=-1`/
`tiles[]=-1` field-reset pattern already matches. Back on `FUN_479b_076e`:
after the record init succeeds, it writes the generated name into the
record (`FUN_1d1d_07e4(colony+2, name_buf)`), fires several UI-refresh
flags, and a founding narration/sting — all cosmetic, not state.

So the "deep case-7 economy OPEN" framing throughout the rest of this
file (§2d, §2e, the old Phase-outline table, `ai_transcription.md`) was
never about a hire economy at all — it was chasing corrupted-blob content
attributed to the wrong case. Founding itself is **already faithfully
ported**; nothing new to do here. (Case 8/9's terrain-improvement
formulas, mapped last pass, are unaffected by this correction — they're
genuinely separate cases.)

**Case `0xb`/`0xc`** (`FUN_479b_0972`) is the entry into ship/land act —
short pre-check, calls `FUN_2a1f_0210`/`FUN_291f_044e`/`FUN_2a1f_0142`
(goto/move drivers), terrain re-check on arrival, then falls into the
same `default` thunk before clearing state.

**Move drivers checked this same day, one resolved, two corrupted:**
`FUN_2a1f_0142` → `FUN_465b_0000` — **already known/ported**, this is the
terrain-MP-cost helper (`ai_transcription.md`'s "Shared move/terrain
helpers" table, Linux `ai_dos_move_spent`). `FUN_291f_044e` →
`FUN_4720_049e` carries a real Ghidra disassembly-fault warning
(`Instruction at (ram,0x0004c035) overlaps instruction at
(ram,0x0004c033)`, `viceroy_unpacked.c:76063`) — corrupted, needs the
overlay-recovery treatment before trusting it. `FUN_2a1f_0210` →
`FUN_6662_0f74` has no warning banner but shows the same corruption
*signature* seen elsewhere this session (`in_AX` — a register-passed
argument the decompiler failed to recover as a named parameter, despite a
`void` signature) — likely corrupted too, not confirmed by a banner.
Genuinely needs a fresh Ghidra recovery (comparable effort to the
`0a60`/`153e`/`3180`/`1816`/`5b66` recoveries already done this session)
to trust either — not attempted this pass; this is real "OPEN (unpark
#4)" territory, not a quick follow-up like case 9 turned out to be.

**Not yet mapped, flagged for a follow-up**: the full 16-byte-per-terrain-
class content of `DS:0x2f78`/`0x2f80` (only offset +2 named so far);
colony-record `+0xa4`; `FUN_4720_049e`/`FUN_6662_0f74` (case `0xb` move
drivers, corrupted, needs fresh recovery). (`FUN_281f_09fc(0x24)` itself
is resolved — Lumber Mill building check, not a founding father, see
above.)

**Update (2026-08-15) — both move drivers checked with the overlay
project; one resolved clean, one confirmed genuinely corrupted, and a
shared blocker found across three separate gaps.**

`FUN_6662_0f74` (`FUN_2a1f_0210`'s target): **clean, zero warnings**,
full 250-line multi-tier goto/pathing driver recovered. Real structure:
arrived-at-goto short-circuit; direct single-step for Chebyshev
distance ≤1; a smarter direct-route attempt for distance ≤6 (falls back
through an unresolved local waypoint helper for longer hauls); and, if
none of those commit, a full 8-neighbor scored fallback (walkability +
own-tile bias + distance-improvement + a toughness deduction) with an
up-to-8-try random "wiggle" retry if the chosen tile gets rejected for
ownership reasons on arrival. This is a materially richer algorithm than
Linux's current single-tier `ai_euro_score_move` (direct-only, no
detour/waypoint tier, no wiggle-retry) — real "OPEN (unpark #4)" territory,
now unblocked to attempt, but a genuinely large port (4 more local
helpers — `0015b7`/`0015bc`/`0015c1`/`000000` — still unnamed within this
same overlay) not a quick follow-up.

**The one piece of `0f74` that *was* immediately portable — the 8-neighbor
fallback's toughness-deduction term, `*(byte*)(terrain_class*0x10+0x2f76)*3`
— turns out to share the exact same unmapped table this doc already
flagged** (`DS:0x2f78`/`0x2f80`, stride `0x10`, only the `+2` byte named
so far). **`0x2f76` is table offset +0** — same family, one byte earlier.
No real values ever captured for any of it, so nothing here is safely
wireable without inventing constants (same "no invented numbers" rule as
`417e`'s price tables). **This one small unmapped table is now a shared
blocker across three separate gaps**: case 8's Pioneer-improvement reward
scale (still thin/parked), this pathing term, and (if the full pathing
tier structure is ever ported) its detour-route scoring too — a single
live memory-dump capture of `DS:0x2f76..0x2f88`ish would unblock all
three at once, the highest-leverage single capture identified so far this
session if anyone gets DOSBox-X debugger access again.

**Update (2026-08-15, later pass) — traced `0f74`'s own local helpers;
scope turns out much larger than the earlier "quick follow-up" framing.**
`FUN_OVL20_L0000__0015b7` (the short-distance direct-step helper): clean,
tiny — sign(dx)/sign(dy) → 8-direction-table lookup (`dos_dir_from_sign`
shape), genuinely small. But the other two are not: `0015bc` (the
"smarter direct-route" call) is itself a **~230-line 16×16-window
flood-fill/BFS route search** (FIFO work-queue relaxation over a scratch
cost-map, per-cell terrain/ownership/diplomacy gating, the same unmapped
`0x2f76` terrain-cost table, *and* an AI-debug visualization/keyboard-poll
loop at the end — confirmed cosmetic, gated by the same class of debug
flag `20e6` uses, safely skippable). `0015c1` (the longer-distance
fallback) is **another, separate flood-fill pathfinder** over an 18×18
window with its own scratch table, and calls a fifth still-unresolved
local helper (`FUN_OVL20_L0000__0009ae`) not traced this pass.
`FUN_OVL20_L0000__000000` hits the known pcode-error decompiler bug class
(`task #2`/`FUN_5fef_0000` precedent) — not yet hand-transcribed.

**Revised assessment: `FUN_6662_0f74` is not one move-driver function, it's
a small pathfinding *subsystem*** — direct step, two independently-sized
BFS flood-fill searches, and a scored single-step fallback, all clean
(no corruption) but genuinely large and interlocking. Porting it
faithfully is real, standalone, multi-pass-scale work, not a follow-up to
the small case-handler set this thread started from. **Not attempted this
pass** — flagging the honest scope instead of a partial/rushed port that
would risk getting the flood-fill relaxation order or cost formula subtly
wrong. If ever resumed, `0015b7` alone (the tiny direct-step helper) is
the one genuinely quick win left in this specific area.

**Doc-sync + `0015b7` decompiled, 2026-08-19: this whole tier structure
already has a real Linux counterpart, just never cross-referenced here.**
`units_next_goto_step` (`units.c`) independently implements the *same*
three-tier shape this section describes — adjacent sign-step (in-code
cite `FUN_6662_0086`), near-range destination cost-flood for both axes
≤6 (`FUN_6662_00f2`, also independently cited in `save_format_map.md`),
and a far-range uniform BFS + waypoint-flood fallback — with
`units_greedy_next_step` providing the same full-8-neighbor scored
fallback `0f74`'s own tail describes (walkability + distance-improvement
score). Not byte-exact (no own-tile bias, no toughness-deduction term —
still correctly blocked on the unmapped `0x2f76` table — no wiggle-retry
on rejected tiles), but this is a real, mature, structurally-faithful
implementation, not the "not attempted" gap this section's own framing
suggested; whoever wrote `units_next_goto_step` apparently worked from
the real `0086`/`00f2` addresses directly rather than through this file's
`0f74` investigation. Also decompiled `FUN_OVL20_L0000__0015b7` cleanly
this pass (Ghidra headless, `OverlayTest` project, `OVL20_L0000:15b7`):
confirms the "sign(dx)/sign(dy) → 8-direction-table lookup" reading
exactly (loop over the same 8-entry dx/dy table used throughout this
project, default 8 = no direction when both signs are 0) — but Linux's
existing `units_sign_i`-based direct step (used in the adjacent tier
above) already produces the identical result without needing a literal
port of the table-lookup shape, so there's no remaining gap to fill with
it. **Net: no further src/ changes from this specific thread** — it was
already done.

**2026-08-20 — `0f74`'s own tail (own-tile-adjacent scoring + wiggle-retry)
fully recovered and shipped, after a first attempt hit a real
field-ownership blocker (not a corruption or geometry problem).**
Force-decompiled
`OVL20_L0000:f74` fresh (`GhidraDecompileAt.java`, clean, matches the
2026-08-15 "250-line" citation). The full 8-neighbor scored fallback +
tail is now precisely transcribed, not just summarized:

- Scores all 8 neighbors (skip non-walkable / claim-rejected /
  diplomacy-blocked), tracking best dir index (sentinel `8` = none
  found). Per-candidate score = `(own-tile-occupant-toughness-or-3) +
  chebyshev*4 + |dx|+|dy|` — the "toughness" term is the same
  `*(byte*)(terrain_class*0x10+0x2f76)` table already blocked (Tier 4,
  `T4.1`), so this exact formula stays unportable regardless of the rest.
- **Tail (this pass's real find, fully clean this time):** if the best
  pick's direction equals the exact reverse of `unit+0x314f` (last-taken
  step direction, `dir^4`) **and** the unit is mid-goto — DOS doesn't
  take it; it rerolls up to 8 random directions (`dos_rng`-equivalent,
  0..7) instead, accepting the first legal/affordable one, giving up
  (total failure) if all 8 reroll attempts are also rejected. Prevents
  visible single-tile ping-pong. `unit+0x314f` gets written on every exit
  path (chosen dir, or `0xff`/-1 on failure) — this is the "own-tile
  bias" the 2026-08-15 note flagged as missing, now correctly identified
  as "last-direction anti-backtrack," not a literal same-tile score term.

**First attempt** (`units_greedy_next_step`/`units_next_goto_step` in
`units.c`, gated on `ColonizeDosRng*` so it's a no-op when `rng==NULL`)
wrote `unit+0x314f`'s obvious Linux home, `ColonizeUnit.last_dir`
(`col1_bridge.c` round-trips it as `facing`) — reverted before shipping
on discovering **that field already has a live, different owner**:
`ai.c`'s Indian native Brave-movement engine (`ai_native_pick_dir`,
`src/core/ai.c:3661/3713`) reads and writes it every Brave act for its
*own* anti-backtrack bias — and tellingly, `ai_euro.c` already
independently solved the exact same problem for Euro AI scoring via its
*own* file-local shadow array (`s_euro_last_dir[]`, `ai_euro.c:37`)
instead of touching `unit->last_dir` — a precedent this pass should have
checked *before* writing to the shared field, not after.
`units_advance_goto_one_step` handles goto-walking for any unit (not
gated to a single AI subsystem), so writing `unit->last_dir` there risked
silently corrupting the Indian native engine's own bookkeeping for a
Brave that happens to also run a goto step — a real regression with no
enabled test that would catch it (`golden_ai_turns`/`golden_ai_joint`,
the ones most likely to notice Indian movement drift, are parked per
`T3.3`). Full revert, confirmed clean (`git diff` empty), rebuilt, full
`ctest` green (40/40) again.

**Second attempt, shipped.** Re-ported the exact same algorithm using a
dedicated `units.c`-local shadow array instead
(`s_units_goto_last_dir[COLONIZE_UNITS_MAX]`, same zero-init/no-reset-hook
convention as `s_euro_last_dir` — "a unit's first goto step gets a
harmless, self-correcting bias instead of no history," not worth a
despawn/reuse reset hook). `ColonizeUnit.last_dir` is untouched by this
change (verified via `git diff`, no hits). `units_greedy_next_step`/
`units_next_goto_step` gained a `ColonizeDosRng*` parameter (NULL-safe —
wiggle is skipped, deterministic geometry still runs);
`units_advance_goto_one_step` writes the shadow array's entry for a unit
right after it commits a step, covering all three of
`units_next_goto_step`'s tiers (not just the fallback tier the wiggle
check itself lives in), matching DOS's own single write-on-every-exit
shape. Full `ctest` 40/40 green. The 8-neighbor scoring formula's own
toughness term stayed blocked on `0x2f76` (`T4.1`) at the time — only the
wiggle-retry shipped, not the full byte-exact score formula.

**2026-08-21 — full score formula wired, now that `T4.1`'s terrain-table
capture covers `0x2f76` offset +0.** Re-read `FUN_6662_0f74`'s tail
directly (`viceroy_unpacked.c:104652-104720`) rather than trusting this
doc's own prose summary above, which undersold the distance term — real
formula per candidate is `penalty + chebyshev(cand,goal)*4 +
manhattan(cand,goal)*5`, not `chebyshev*4 + |dx|+|dy|` (the raw code's
`iVar26*4 + uVar25+uVar24` folds a second manhattan term in beyond what
the prose captured). `penalty` is `3` if the unit's max MP (`FUN_281f_090c`,
already resolved as "max MP" per `move_scoring_land.md`) is `<2`, else
`map_dos_terr_cost_byte(candidate_terrain)*3`. Also ported the AI-only
non-worsening-move filter (`bVar27`, `DS:0x543f` per-nation human flag —
skip a candidate whose chebyshev+manhattan-to-goal is worse than staying,
unless the unit belongs to the human nation) via the existing
`g_units_combat_human_nation` module-static cache in `units.c` (same
pattern `units_can_show_combat_report` etc. already use), rather than
threading a new parameter through `units_next_goto_step`'s 16+ call
sites. Wired in `units_greedy_next_step`. Full `ctest` 41/41 green.

**2026-08-20, later same day (T1.8 re-check) — confirmed `units.c`'s
flood-fill substitute is real and already ships, with one caveat.**
`units_flood_next_step`/`units_bfs_next_step` (`units.c`) are wired into
`units_next_goto_step`'s near/far tiers. `units_flood_next_step` is a
16×16-window destination-outward cost flood — `UNITS_FLOOD_W==16`,
matching this section's own `0015bc` window size exactly.
`units_bfs_next_step` is a whole-map uniform BFS, a deliberately
*different* (arguably stronger) substitute for `0015c1`'s
18×18-window-plus-waypoint-retry scheme rather than a literal port.
Per this doc's own 2026-08-19 entry above, both were built citing a
*different* DOS routine pair (`FUN_6662_0086`/`FUN_6662_00f2`) — same
shape by independent convergent design, not a confirmed byte-level match
to `0015bc`/`0015c1`'s own formulas. A working, tested equivalent already
exists; literally porting `0015bc`/`0015c1` for byte-exactness is now
optional fidelity polish, not a functional gap. `0009ae`/`000000` remain
untraced, still not needed for anything currently unblocked. Full detail:
`ai_port_plan.md` T1.8.

**2026-08-21 — `0015bc` freshly force-decompiled (clean, zero warnings,
`OverlayTest`), structure and edge-cost formula now much more precisely
known; one real tooling bug found and fixed along the way.** Confirms the
2026-08-20 structural read: a destination-outward FIFO flood-fill over a
16×16 window (`origin = goal-8`), matching `units_flood_next_step`'s own
shape/window size exactly — real, not coincidental convergence. New
findings from reading the raw body directly:

- **Per-edge cost is not a generic move-cost — it's the exact same
  `penalty` term as `0f74`'s own already-ported scored-fallback tail**
  (`units_greedy_next_step`'s `penalty = max_mp<2 ? 3 :
  terrain_cost*3`), keyed off the same per-unit-type table this pass also
  named (below). DOS reuses one edge-cost formula across both the flood
  search and the scored fallback, not two independent ones — a real,
  concrete, low-risk target for closing the byte-exactness gap if this is
  resumed, since the formula and its Linux implementation already exist.
- Also folds in claim-ownership/diplomacy gating (`FUN_1000_88c2`/`88d6`/
  `88ae`/`88a4`) shaped like a `+8` cost penalty for entering another
  nation's claimed territory under certain relation states, gated by the
  same `DS:0x543f` per-nation human-flag table `0f74`'s own tail already
  uses (`g_units_combat_human_nation` in `units.c`) — plausibly already
  covered by `units_can_enter`'s existing claim checks, not independently
  verified this pass.
- **`FUN_281f_090c` ("max MP") re-resolved — its `address_mapping.csv` row
  is wrong.** The CSV lists `FUN_281f_090c -> ram:18afc (FUN_1000_8afc)`,
  but that address decompiles clean to a large, unrelated function
  (village/nation-throttle-table logic, nothing to do with movement).
  Reading the actual 2-call RTLink thunk body at `281f:090c` directly
  (`FUN_210d_0d91(); FUN_1427_065a(); return;`) and following the *second*
  call instead: `FUN_1427_065a -> ram:48ca (FUN_0000_48ca)` decompiles
  clean to a small, obviously-right max-MP accessor: `base =
  type_table[unit_type][DS:0x5234]`, `+3` if a per-nation capability bit
  (`FUN_0000_9810(nation_nibble, 5)`, table at `-0x77f1` stride `0x13c`,
  meaning of bit 5 itself not identified) is set **and** the unit type is
  in the ship range `0xd..0x12` — same range this project already knows
  from `0x5236`'s "combat-capable, ship range excluded" idiom
  (`euro_diplo_153e_full.md` etc). **Real formula is base-plus-conditional-
  ship-bonus, not the flat `type->movement` Linux currently reads** — but
  the bonus can only ever matter if base MP is already `<2`, which no real
  ship type is, so this is a confirmed-real but practically-inert gap; not
  wired (`units.c`'s comment at the fallback-tier formula updated with the
  full citation instead).
- **`DS:0x5234` named**: offset `+0` of the same stride-`0xe` unit-type
  table this project already has three other columns for (`+2`=`attack`
  `0x5236`, `+5`=`cost` `0x5239`, `+9`=flags `0x523d`) — this is
  `ColonizeUnitType.movement`, confirming the field `type->movement`
  already reads is the right one.
- `0015bc`'s own domain-match gate (ship-vs-land) uses the identical
  `0xd..0x12` type range and the real terrain (Ocean `0x19`/Sea Lane
  `0x1a`) via `func_0x0001897c` — same idiom throughout, no new unknowns.

**Not wired this pass** — the edge-cost-formula swap is real and
low-risk (reuses code that already exists and is tested), but several of
`0015bc`'s own scratch globals (`0x1dd2`/`0x1dd4`/`0x1dd6`, the flood's
own module-static goal-cache at `0x2d16`-`0x2d1c`) are still uncharacter-
ized well enough to be sure the *domain/ownership* gating (as opposed to
just the cost formula) matches `units_can_enter` exactly — a wrong guess
there risks a subtle pathing regression with no enabled golden to catch
it (`T3.3`). Real next step if resumed: confirm `units_can_enter`
already covers the ownership/diplomacy `+8` case before swapping
`units_flood_next_step`'s edge cost to the real formula.

**2026-08-22 — that open question answered: `units_can_enter` does NOT
cover it, so this is a real, previously-unflagged gap, not just an
unverified assumption.** Read `units_enter_probe` (`units.c`) end to end:
its only ownership-aware logic is (a) foreign-*unit* combat/bounce
resolution when an actual foe occupies the tile, and (b) ship-docking
rules at a foreign *colony* tile. For the case `0015bc`'s `+8` penalty
actually targets — a **land unit stepping onto empty, unoccupied land**
that happens to be tribe territory or near an enemy fort/colony — the
land branch falls straight through to `units_village_squat_illegal` and
then `COLONIZE_ENTER_OK`, with no ownership check of any kind. **So the
DOS `+8` claim-avoidance penalty has no Linux counterpart at all today**,
not "probably already covered." The four DOS accessors behind it are
already identified, not new unknowns: `FUN_1000_88a4`→`continent_id`,
`FUN_1000_88ae`→`tile_tribe_owner`, `FUN_1000_88c2`→
`tile_tribe_or_presence`, `FUN_1000_88d6`→"enemy Euro fort/colony owner
vs nation" (`FUNCTION_CATALOG.md` lines 1352-1357, thunks resolved to
`FUN_281f_06b4/06be/06d2/06e6`) — Linux equivalents of the first three
already exist (`map_continent_id_at` in `map.c`; `ai_owner_nibble`/
`ai_tile_tribe_or_presence`, currently `static` and private to `ai.c`'s
Indian quiet-scoring gate, `ai.c:2841-2861`), the fourth has no Linux
equivalent yet. **Still not wired**: the exact "certain relation states"
condition that gates the `+8` (which relation states, whose perspective)
wasn't traced this pass — that's the one remaining unknown, not the
accessor identities. Given a working, tested pathing substitute already
ships (`units_flood_next_step`/`units_bfs_next_step`) and no golden would
catch a wrong guess here (`T3.3`), this stays optional fidelity polish
per the note above, now with a precisely-scoped real next step instead of
an assumption to verify: (1) trace the exact relation-state condition
from `0015bc`'s own raw body, (2) expose a non-static tribe/fort-owner
accessor `units.c` can call, (3) add the `+8` term to
`units_flood_next_step`'s edge cost. Not attempted this pass beyond the
identification above (doc-only, `ctest` not run).

**2026-08-22, later same day — step (1) done: the exact condition, force-
decompiled fresh** (`analyzeHeadless` against `OverlayTest`,
`OVL20_L0000:15bc`, clean). It's meaningfully more than a flat `+8`; the
real shape, in the module's own scratch-global terms (`0x1dd2` = mover's
unit-type index, `0x1dd4` = a separate cost-override mode flag unrelated
to ownership — see below, `0x1dd6` = mover's nation id, `-1` if this
flood-search invocation has none):

```
if (mover_nation >= 0) {                              // *(int*)0x1dd6
  tribe = tile_tribe_or_presence(cand_x, cand_y);      // FUN_1000_88c2
  if (tribe >= 0 && tribe != mover_nation)
    reject_candidate();                                // hard skip, not a penalty
  else {
    fort_hit = enemy_fort_or_colony_owner(cand_x, cand_y, mover_nation); // FUN_1000_88d6
    if (fort_hit >= 0) {
      if (mover_nation > 3 || nation_is_ai[mover_nation])  // DS:0x543f, stride 0x34
        reject_candidate();                            // hard skip for AI nations
      else
        cost += 8;                                     // soft penalty, human nation only
    }
  }
}
```

So DOS's real behavior is **asymmetric by design**: an AI-controlled
nation's pathfinding treats another tribe's territory or an enemy
fort/colony's zone as a hard no-go (candidate rejected outright, same
bucket as off-map or non-walkable), while the *human* player's own
pathing request only gets discouraged (`+8`) — presumably so the human
isn't silently blocked from routing near hostile territory the way the
AI polices itself. This is a real behavioral asymmetry, not just a
cost-formula detail — worth knowing before anyone wires this, since a
naive "add `+8` for tribe/enemy tiles" port would be wrong for AI units
(should reject, not merely discourage) and wrong for the human (should
allow with a nudge, not block).

**`0x1dd4`'s own role, separately**: gates a *different* branch — whether
a candidate's terrain-cost term uses the real per-terrain-class formula
(`terrain_cost*3`, ship flat `3`) or falls back to a flat `+1` when a pair
of unrelated hazard-ish flags (`FUN_1000_8944`/`tile_fa_flags` bit `0xa`,
`FUN_1000_891c`/`tile_has_minor_river` bit `0x40`) are set and `0x1dd4`
itself is nonzero — reads as "already-favored/cached route tile is nearly
free," not an ownership concept at all; don't conflate it with the
tribe/fort gate above. Not traced further (not needed for the ownership
question this pass targeted).

**Still not wired** — `units.c` has no nation-scoped tribe/fort-owner
accessor exposed today (the three ready ingredients are private statics
in `ai.c`'s Indian-scoring file, and `enemy_fort_or_colony_owner` has no
Linux port at all), and the AI-vs-human reject/penalty asymmetry means
this can't be a single flat cost term — it needs a caller-nation check
inside `units_flood_next_step` itself. Real remaining steps if resumed:
(2) expose `tile_tribe_or_presence`/an `enemy_fort_or_colony_owner` port
from `units.c` (or thread the values in), (3) wire the reject-vs-penalize
branch keyed on whether the pathing nation is AI or human. Given the
existing flood substitute already ships and works, and there's still no
golden to catch a wrong wire (`T3.3`), leaving unwired — this is now a
precisely scoped, small implementation task rather than an open RE
question. Full trace: this session's `OVL20_L0000:15bc` force-decompile
(not separately archived — see the condition transcribed above). `ctest`
not run (doc-only).

**2026-08-22, later same day — step (2)/half of step (3) done: the tribe
hard-reject wired, the fort/colony `+8` half deliberately left unwired
(new, genuine unknown found, not the one this row expected).** Force-
decompiled `FUN_1000_88c2`/`88d6` themselves (their `1000:` addresses don't
exist as spaces in `OverlayTest`; resolved via the `281f_XXXX → ram:1XXXX`
identity already established for this accessor family — flat `ram:188c2`/
`ram:188d6` decompile clean):

```c
// FUN_1000_88c2 == accessors.c's tile_tribe_or_presence, confirmed exact:
// HAS_CITY-or-HAS_UNIT gate, then owner high nibble. No new unknowns.

// FUN_1000_88d6 (the fort/colony-zone term):
int FUN_1000_88d6(x, y, mover_nation) {
  if (!inset(x, y)) return -1;
  flags = tile_layer2_byte(x, y);
  if ((flags & 0x48) == 0) return -1;          // *** new unknown, see below ***
  owner = owner_nibble(x, y);
  if (owner < 0 || owner >= 4 || owner == mover_nation) return -1;
  if ((euro_relation[mover_nation][owner] & 0x40) == 0) return -1;  // MET bit
  return owner;
}
```

Good news: the nation/MET half is fully resolved with zero new unknowns —
`owner_nibble` range-gated to Euro `0..3` (excludes tribes, correctly
scoping this term to *Euro* forts/colonies only, matching the DOS name),
and `& 0x40` on `nation*0x13c-0x77c4` is exactly `euro_diplo.md`'s
already-named `euro_relation[a][b]` MET bit — this term only fires between
nations that have met, which makes sense (can't have a diplomatic-flavored
zone reaction to someone you've never seen).

**Bad news, genuinely new**: the tile-flags gate is `layer2_byte(x,y) &
0x48` (bits 3 and 6) — **not** `MAP_OCCUPANCY_HAS_CITY` (`0x02`) as a naive
reading of "fort/colony tile" would assume. Bit `0x08` is
`VICEROY_LAYER2_FA_MASK`'s known **road** component
(`viceroy_types.h:VICEROY_LAYER2_FA_ROAD`); bit `0x40` has **no
established real-DOS-mask meaning anywhere in this project** — it is
*not* the same thing as this project's own `MAP_LAYER2_FA_ROAD=0x40u`
(that's an explicitly-noted Linux-side synthetic stand-in bit, per
`map.h`'s own comment, unrelated to the original save format). Best
working guess (not verified, not wired on the strength of a guess per
project convention): a per-tile fortification-tier marker mirrored onto
the map plane for fast lookup (stockade/fort vs. fortress), since
`colony.h`'s own `fortification` field is colony-record-side and this is
a tile-plane check — but this is exactly the kind of "structural
confidence ≠ semantic confidence" trap the method notes warn about, so
treating it as **open, not resolved**. Real next step if resumed: find
what sets real DOS mask bit `0x40` (an XREF sweep on the `FUN_137f_015e`
OR/AND-clear helper's callers, the same tool `T1.11` used to find
`0x10`'s own write-trigger, would answer this directly).
**Tried this pass, inconclusive — not a dead end, just needs a different
angle.** `GhidraListXRefs` on the resident setter (`ram:394e`) found only
4 direct callers (`0000:4520/4595/654f`, `1000:8881`), but none show a
resolvable literal mask argument — three decompile with an empty
`FUN_0000_394e();` call (Ghidra couldn't recover the stack-passed
immediate) and the fourth (`8881`, a large, real, previously-unlinked
function worth its own look later) doesn't call it directly at all in the
decompiled body shown. **T1.11's own `0x10` search worked by grepping
literal-mask *call sites* into the wrapper switch, not by XREF-ing the
resident setter itself** — the real next step is that same literal-mask
grep (`switchD_2000:da9f::caseD_10`-style call sites passing `0x40`) as
applied to *this* setter's own overlay-side wrapper, not a repeat of the
XREF sweep just tried.

**Wired**: the tribe-owner hard-reject half only (fully resolved, zero
open questions) — `map_tile_tribe_or_presence` (`map.c`/`map.h`, new)
plus a nation-mismatch `continue` in `units_flood_next_step`'s candidate
loop (`units.c`). **Not wired**: the fort/colony `+8`-vs-reject term,
pending the `0x40` bit's real meaning — left as a precise code comment at
the wire site rather than guessed. Full `ctest` 41/41 green. (`FUN_291f_044e`'s target): **corruption is real but
narrow, and the function underneath is a genuinely major find — not a
move driver at all.**

**2026-08-22, later same day — the literal-mask grep this row itself
prescribed is now done, exhaustively, and it's a dead end: `0x40` isn't set
through this helper family at all.** `FUN_137f_015e` (== `ram:394e`) has
**exactly 4 real call sites in the whole binary** — confirmed two
independent ways (`GhidraListXRefs` fresh re-run, and grepping
`FUN_137f_015e(` directly in the flattened `viceroy_unpacked.c`, both give
the same 4). Read all 4 down to their literal args (raw-disasm push
sequence where the decompiler botched it, same method that unstuck
`T1.1`'s case dispatch): `viceroy_unpacked.c:7468` (mask `1`, clear —
unit-occupancy `VICEROY_LAYER2_PRESENCE` bit, cleared on disband),
`:7502` (mask `1`, set — occupancy bit, set on relocate), `:9816`
(`FUN_15eb_0668`, mask `0x10` — matches `map.h`'s already-known
`MAP_LAYER2_PURCHASED`, a nice bonus cross-confirmation: this is the
Indian land-purchase/gift-tile marker, not fort/colony-related at all),
and the `FUN_281f_068c` generic wrapper (resolves to the same target via
a tail-jump chain, `1000:887c`→`0000:394e`) — every one of *its* own
literally-resolvable callers across the whole flattened export
(`viceroy_unpacked.c:42499/42592/57001/58157/76688/81307/101569/101570`)
also only ever passes `{1, 2, 4, 0x10}`. **No call site anywhere passes
`0x40` or `0x48`.** So the fort/colony-zone bit isn't OR'd in through this
generic tile-flag setter family at all — it must be a direct inline
`OR byte,0x40` (or set as part of a wider literal byte write) somewhere in
colony/fort construction code, unreachable from this angle. Real next step
if resumed: a raw byte-pattern search of the whole `.asm` for an inline
`80 xx 40` (`OR byte ptr [...],0x40`)-shaped instruction near colony/fort
placement code, not another XREF/grep sweep of this helper family — that
avenue is now exhausted. Not attempted this pass (out of scope for a
single-session budget); the fort/colony `+8` term stays unwired, same as
before. `ctest` not run (doc-only).

**2026-08-24 — resumed per this row's own real next step (`0015bc` first,
smaller of the two flood-fills), plus the row's other three open items
(`0015c1`, `0009ae`, `000000`). Net: `0015bc`'s edge-cost formula wired
(real, low-risk fix landed); `0015c1` and `0009ae` decompile clean for the
first time (both previously either unread or force-decompile-avoided) and
are now structurally documented; `000000` (the pcode-error function) hand-
transcribed from raw disassembly, same method as `FUN_5fef_0000`/
`OVL12_L0000`. Byte-exact wiring of `0015c1`/`0009ae`/`000000` themselves
still not done — see "still open" at the end of this entry.**

- **`0015bc` edge-cost formula: wired.** Force-decompiled fresh again to
  re-verify byte-for-byte before writing any C (`OVL20_L0000:15bc`, clean,
  matches the 2026-08-21/22 passes' own citations exactly, no drift). One
  correction to a claim in this doc's own 2026-08-21 entry: that pass
  asserted `0015bc` "reuses the *same* `penalty` formula as `0f74`'s
  already-ported scored-fallback tail" (`max_mp<2 ? 3 :
  terrain_cost*3`). Re-reading the raw disassembly line-for-line, that's
  not quite right — `0015bc`'s own gate is `uStack_12 =
  type_table[type][DS:0x5234] < 4` (the raw `movement` column, no
  `FUN_281f_090c`/ship-bonus computation involved at all), not `<2` on a
  *computed* max-MP value. Same shape (flat `3` vs `terrain_cost*3`), a
  different, simpler threshold on a different (unadjusted) input — two
  real, independently-tuned formulas that happen to look alike, not one
  formula reused verbatim. Wired into `units_flood_next_step`
  (`units.c`) as the edge cost inside the flood's own BFS relaxation loop,
  replacing the previous generic `units_move_cost` edge — `flood_low_move
  = type->movement < 4`, edge `= flood_low_move ? 3 :
  map_dos_terr_cost_byte(map_dos_terr_class_at(...)) * 3`, both accessors
  already existing and already used by `0f74`'s own port. **Deliberately
  not ported**: the `DS:0x1dd4` "cached/favored route" flat-`+1` override
  (a hazard-flag-gated exception, unrelated to ownership, still untraced
  past what the 2026-08-22 entry above already noted) and re-deriving the
  domain/continent match term inline (already enforced upstream by
  `units_can_enter`). Full `ctest` 41/41 green (comment cites this
  entry).

- **`0009ae` — force-decompiled clean for the first time (was genuinely
  untraced before this pass).** 411 bytes, `OVL20_L0000:9ae`, no pcode
  error, no warnings. Structural read: takes window-local-scaled
  coordinates via registers (`in_AX`, `in_DX`) and a domain flag
  (`in_BX`), consults the same per-domain walkability grids `0015bc`/
  `0015c1` themselves read (`-0x790a`/`-0x7a18`, stride `0x12`); if the
  *current* cell is itself walkable, first tries a direct "stay" candidate
  (`local_10 = 8`, this project's usual "no offset" sentinel) validated
  via `FUN_OVL20_L0000_000000` + `FUN_OVL20_L0000_0015b2`; if that fails
  (or the current cell isn't walkable), falls back to scanning all 8
  neighbors, scoring each by a distance call (`FUN_1000_8560`, not
  independently identified this pass) and keeping the best that also
  passes both validators. On success, writes the winning cell into
  `DS:0xa14e`/`DS:0xa14c` (the same pair `0015bc`/`0015c1` themselves read
  as their own flood-origin/goal) and returns `1`; `0` on total failure.
  Reads as "pick one best immediate step toward the real goal, or bail" —
  structurally close to `0f74`'s own scored-fallback tail, but operating
  in `0015c1`'s coarser 18×18 window-local coordinate space rather than
  real map coordinates. Not the same function as the already-confirmed
  `0015b7` (direct-step helper matching `units_sign_i`) — a distinct,
  previously-unaccounted-for fifth helper, now identified rather than a
  mystery. `FUN_1000_8560`'s identity and the exact register-passing
  convention (which of `AX`/`DX` is x vs y, and whether they're real map
  coordinates or the `×4`-scaled quantities `0009ae`'s own body implies
  elsewhere) are **not** independently confirmed this pass — flagging as
  structural-not-semantic confidence per this file's own convention.

- **`0015c1` — force-decompiled clean for the first time (this row's own
  2026-08-20 note said it was never independently read raw; the shipped
  Linux `units_bfs_next_step` was built citing a *different* DOS pair,
  `0086`/`00f2`, not this one).** 5-byte stub function count aside, the
  real body is ~230 lines, clean, `OVL20_L0000:15c1`. Structure, read
  directly rather than inferred: an 18×18-window (`0x12`) flood-fill, same
  family shape as `0015bc`'s 16×16 but with one extra stage in front —
  it first calls `0009ae` (above) to try to pick a single best immediate
  step; if that fails outright, it falls through with the original
  in-args untouched (no flood attempted — a real "give up early" path
  `0015bc` doesn't have). If `0009ae` *succeeds*, `0015c1` runs its own
  destination-outward BFS flood over the 18×18 window — same per-cell
  8-neighbor relaxation shape as `0015bc`, same domain-vs-continent gate
  inlined (`FUN_1000_88a4`, `FUN_1000_8886`, `FUN_1000_88c2`/`88d6`/
  `88ae`/`89d0`, all already-identified accessors per the 2026-08-22
  entries above) — then, once the flood settles, scores the *unit's own*
  8 neighbors by flood-cost (tie-broken by `FUN_1000_856a`, an XY-distance
  helper, not independently confirmed this pass) and, on picking a
  winner, calls `FUN_OVL20_L0000_000000` **again** to validate/finalize it
  before committing. A popup/dialog tail (mirroring `0015bc`'s own —
  `FUN_0000_dd18`/`FUN_1000_83e0`/`83ea` message-box calls, catalog id
  `0x1df6`) fires only when a UI-visibility flag (`DS:0x1df4`) is set —
  matches `0015bc`'s analogous `DS:0x1df2` gate, almost certainly a
  "show the AI's pathing on-screen" debug/spectator toggle, not gameplay
  logic; out of scope for a port regardless. **Net: `units_bfs_next_step`
  remains a deliberately different substitute, not a literal port** —
  confirmed now from `0015c1`'s own raw body rather than inference. A
  real byte-exact port would need `0009ae`'s waypoint-pick semantics
  fully pinned down first (its own open questions above) plus threading
  the same edge-cost/ownership-gate work `0015bc` already has partway
  done — not attempted this pass, real next step if resumed.

- **`000000` — hand-transcribed from raw disassembly, same method as
  `FUN_5fef_0000`/`OVL12_L0000` (this doc's 2026-08-13 entry).** Confirmed
  still hits the known decompiler bug (`Offset must be between 0x0 and
  0x10ffef, got 0xffffffff`) even freshly force-cleared. 132 bytes
  (`OVL20_L0000:0000`, confirmed via `DumpOverlayFuncs.java`), small enough
  to transcribe whole. Disassembled exactly via `GhidraDisasmExact.java`
  and hand-decoded (`ENTER 0x6,0x0` frame; 3 register args saved to
  locals; 2 genuine stack args popped by the trailing `RET 0x4`):

  ```c
  // OVL20_L0000:0000 - hand-transcribed (pcode-error function, decompiler
  // can't resolve this one's CALLF targets, same bug class as
  // FUN_5fef_0000/OVL12_L0000). Entry registers AX/DX/BX are real inputs
  // (this compiler's near-call convention passes some args in registers,
  // not just the stack) - saved to locals at entry, never re-loaded from
  // fresh AX/DX/BX after. Two more args arrive via the stack proper
  // (BP+4, BP+6), consistent with the trailing `RET 4`.
  //
  // Confirmed accessor identities (already named project-wide, not new
  // RE): CALLF 1000:8958 == FUN_1000_8958 == FUN_13e4_0074 ==
  // "ocean_or_high_seas" (this project's `ai_is_ocean_hs`, ai.c:534).
  // CALLF 1000:88a4 == FUN_1000_88a4 == "continent_id" (per this doc's
  // own 2026-08-22 entry above, citing FUNCTION_CATALOG.md).
  //
  // NOT independently confirmed this pass: which of the 3 register args
  // is x vs y vs "wanted domain" bool, whether AX/DX arrive already
  // window-local-scaled (as 0009ae's own body implies) or as real map
  // coordinates, and what BP+4 vs the BX-register arg's respective roles
  // are (BP+4 reads like a stack-safe copy of a boolean domain want-flag,
  // not independently proven). Structural shape only - do not treat as
  // semantically confirmed.
  // Address-by-address control flow (0x0F JMP 0x67; 0x12/0x15 inner-loop
  // head; 0x64/0x67 outer-loop head) collapses cleanly to a standard
  // pre-tested nested for-loop — verified by re-deriving the outer bound
  // arithmetic from the raw jumps rather than assumed: `entry_ax` (saved
  // AX) and `row`'s own initial value are the SAME literal quantity (both
  // loaded from AX before anything clobbers it), so the outer test
  // `!(entry_ax+1 < row)` only holds for row in {entry_ax, entry_ax+1} —
  // a real, confirmed 2-row (not N-row) span; same arithmetic for the
  // inner `col` span against `entry_dx`. Reads as a 2x2-cell probe, not a
  // window-wide scan.
  int FUN_OVL20_L0000_000000(word bp4_domain_flag, word* bp6_out_pair) {
    word entry_bx = BX, entry_dx = DX, entry_ax = AX;  // pushed, unclobbered
    int found = 0;
    for (int row = entry_ax; !found && !(entry_ax + 1 < row); row++) {
      for (int col = entry_dx; !found && !(entry_dx + 1 < col); col++) {
        if (ocean_or_high_seas(col, row) == bp4_domain_flag) {
          /* real code re-issues the same call rather than reusing AX;
           * transcribed as one call here, behaviorally identical since
           * the accessor is a pure read. */
          if (bp4_domain_flag == 0 || continent_id(col, row) == 1) {
            *(word*)entry_bx = row;        // out param #1 (register ptr)
            bp6_out_pair[0]  = col;        // out param #2 (stack ptr)
            found = 1;
          }
        }
      }
    }
    return found;   // 0 or 1
  }
  ```

  Not ported to Linux this pass — called from `0009ae`/`0015c1` (above),
  both of which are themselves still unwired; porting `000000` alone
  without its two callers' own remaining open questions (register-arg
  roles, `FUN_1000_8560`/`FUN_1000_856a` identities) would be RE without
  a place to land it. Documented per project convention ("document even
  if untestable/unwired," `T1.9`'s own precedent) rather than left as a
  bare pcode-error citation.

**Still open, honestly**: `0015c1`/`0009ae`/`000000` are now fully
*documented* (clean decompiles or, for `000000`, a verified hand
transcription) but not byte-exact **ported** — `units_bfs_next_step`
keeps shipping as the tested, working, deliberately-different substitute
per the 2026-08-20 note above. Real next step if resumed: pin down
`FUN_1000_8560`/`FUN_1000_856a` (both distance/tie-break helpers, neither
independently identified) and the exact register-argument roles for
`000000`/`0009ae`, then decide whether `0015c1`'s two-stage
(waypoint-pick + windowed-flood) shape is worth porting literally given
`units_bfs_next_step` already covers the same functional need. `ctest`
41/41 green (one real `src/` change this pass: `0015bc`'s edge-cost
formula in `units_flood_next_step`).

**2026-08-24, later same day — resumed per this row's own real next step
(`FUN_1000_8560`/`FUN_1000_856a` first). Both resolved; a major structural
correction found along the way (`0015b7`/`0015b2`/`0015c1` are 5-byte JMPF
thunks into resident segment `1000`, not overlay-local bodies); two new,
genuine blockers surfaced (not restatements of old ones). No `src/`
change — forcing one would mean guessing behavior for an unresolved
load-bearing accessor and an unfound data-population step.**

- **`FUN_1000_8560`/`FUN_1000_856a` both resolved, and it's the same
  formula.** `address_mapping.csv` row 811 (`FUN_281f_0370,281f:0370,ram,
  18560,FUN_1000_8560,exact`) plus `FUNCTION_CATALOG.md`'s own entry for
  `FUN_281f_0370` ("Far thunk → `FUN_124c_0040` (DOS distance helper)")
  resolve `0009ae`'s neighbor-scoring call. `FUN_1000_856a` was already
  resolved to the same `FUN_124c_0040` in `euro_goal_orders_0a60_full.md`
  (octile distance, `max(dx,dy)+min(dx,dy)/2`). **Both of `0009ae`'s and
  `0015c1`'s "mystery" scoring/tie-break helpers are the identical
  already-live formula** — `units_octile` (`units.c:3927`) — reached via
  two different thunk chains, not two different formulas. Answers this
  row's own step-1 ask: the two functions "fit together" by sharing one
  distance primitive, no new logic needed for that piece specifically.

- **Structural correction: `0015b7`/`0015b2`/`0015c1`, as addressed
  within `OVL20_L0000`, are each plain 5-byte `JMPF` thunks into resident
  segment `1000`** — not overlay-local bodies as every prior pass
  (including this row's own 2026-08-24 earlier entry) implicitly assumed.
  Confirmed via `GhidraDisasmExact.java` with `len=1` at each address
  (`OVL20_L0000:15b7` → `JMPF 1000:a78c`; `:15b2` → `JMPF 1000:a46e`;
  `:15c1` → `JMPF 1000:a9c0`; each is exactly `EA <off_lo><off_hi>
  <seg_lo><seg_hi>`, 5 bytes). **Tooling gotcha**: a wider `len` at these
  same addresses throws `AddressRange`'s "different address space" error
  from `GhidraDisasmExact.java`'s own `clearCodeUnits` call — this
  overlay's block is short enough that `off+len` wraps out of
  `OVL20_L0000`'s space; `len=1` (or whatever fits) sidesteps it, worth
  trying first at a suspected thunk before assuming corruption. This
  doesn't invalidate any previously-banked finding — Ghidra's decompiler
  auto-follows a tail-call `JMPF` and renders the *target's* body under
  the thunk's own name, which is why `GhidraDecompileAt.java` at
  `OVL20_L0000:15b7`/`:15c1` etc. still produced real, useful bodies (the
  already-confirmed `0015b7`≈`units_sign_i` match is really about
  `1000:a78c`'s body) — just corrects the address attribution.

- **`1000:a46e` (`0015b2`'s real target) identified: a Chebyshev-8-gated
  shortcut that re-invokes the already-ported near-flood.** Body (from
  `GhidraDecompileAt.java` at `OVL20_L0000:15b2`, i.e. this target's
  code): if `|FUN_1000_1e7b() − in_BX| < 8` AND `|in_DX − param_4| < 8`
  (candidate goal within 8 tiles on both axes of whatever `FUN_1000_1e7b`
  returns — see caveat below), it sets `DS:0x1dd2` (next bullet), sets
  the flood-goal globals `DS:0xa14e`/`DS:0xa14c` to the real goal, blanks
  `DS:0x1dd6` temporarily, and calls **`FUN_OVL20_L0000_0015bc(0)` —
  `0015bc` itself, already ported as `units_flood_next_step`** — then
  returns its first-step result (`DS:0xa370`). Reads as "if the real goal
  is close enough, skip the windowed 18×18 dance and just reuse the
  16×16 near-flood directly." Structurally this mirrors Linux's own
  `units_next_goto_step` tiering (`adx<=6 && ady<=6` → `units_flood_next_step`)
  — the same ~8-tile-ish near/far split appears independently in two
  places in the DOS call graph, not just at `0f74`'s own top dispatch.

- **`DS:0x1dd2` resolved**: `1000:a46e` sets it to `1` when its domain
  arg is `0`, `0xd` (13) when nonzero; `0015c1`'s own body (see below)
  reads it back via a `13..18` inclusive range test to reconstruct the
  same domain flag as a 0/1 bool. One member of the previously-flagged
  `0x1dd2`/`0x1dd4`/`0x1dd6` trio (2026-08-21/22 notes) now resolved: it's
  a domain-flag relay between `1000:a46e`/`0015bc` and `0015c1`'s own
  body, not distinct new game state.

- **New genuine blocker: `FUN_1000_1e7b`'s `address_mapping.csv` "exact"
  identity (`FUN_210d_0dab`, the generic RTLink loader) does not hold for
  this call context.** Direct disassembly at its real flat address
  (`GhidraDisasmExact.java 0000:11e7b` — see tooling note below for why
  `1000:1e7b` and `0000:1e7b` both fail/mislead) shows `PUSHF; CLI; TEST
  CS:[0x39e1]... ; JNZ ...; TEST CS:[0x39de]...; JNZ ...; MOV
  CS:[0x39f1],0; MOV CS:[0x39e1],0xff; POPF; POP CS:[0x397d]` — a real
  critical-section/task-state primitive, not a loader stub and not a
  plain coordinate accessor. **Cross-confirms**
  `euro_goal_orders_0a60_full.md`'s own independent citation (via the
  unrelated `FUN_1000_96aa`→`FUN_1000_1e7b` chain) of "a computed
  function-pointer dispatch gated on globals `LAB_1000_39e1`/
  `LAB_1000_39dc_2`" — same address, same gating globals, found from two
  unrelated call chains now. Its actual return-value semantics (why
  `1000:a78c`/`1000:a46e` use it arithmetically as if it returned a
  coordinate) are **unresolved** — not guessed at. Likely explanation:
  the `address_mapping.csv` row was derived from a different caller
  context and doesn't generalize here — same class of stale-row error
  already caught once for `FUN_281f_090c` (T1.8's 2026-08-21 entry).
  **Tooling note**: this project's single `ram` memory block spans the
  *entire* real-mode range (`0000:0000`…`1000:e26f`, confirmed via a
  one-off memory-block dump), addressed as flat `segment*16+offset` — so
  a `FUN_1000_XXXX` symbol must be queried as `0000:<hex of
  0x10000+0xXXXX>` (e.g. `FUN_1000_1e7b` → `0000:11e7b`). Neither
  `1000:1e7b` (no block literally named `1000`, `GhidraDecompileAt.java`/
  `GhidraDisasmExact.java` error out) nor the naive `0000:1e7b` (silently
  resolves to a *different*, unrelated segment-0 address and prints
  plausible-looking garbage — confirmed by trying it first) work — a new
  instance of the "silently pull in wrong but plausible content" trap
  class, worth flagging explicitly since the mechanism (space-name
  aliasing, not jump/call misresolution) is different from the previously
  documented instances.

- **New genuine blocker: the window-local per-domain walkability grids
  are never populated by anything in this call chain.** `0009ae`/`0015c1`
  both read the two quad-map-resolution per-domain bitmaps (`-0x790a`/
  `-0x7a18`, stride `0x12`); `0015c1` additionally reads/writes its own
  flood-visited/parent-direction work arrays (`-0x5e9e`/`-0x5c8e`/
  `-0x5b8e`). None of `0009ae`, `0015c1`, `000000`, or `1000:a46e`'s own
  bodies populate the two walkability grids from scratch — whatever
  builds them ahead of a pathfind call has not been identified in this or
  any prior session. Without it, a literal byte-exact translation of
  `0009ae`'s 8-neighbor window scan or `0015c1`'s own 18×18 flood isn't
  implementable; a Linux port would need to substitute live
  `units_can_enter()` checks (an approximation, matching how
  `units_flood_next_step` already substitutes for `0015bc`'s own
  equivalent gating — not a new problem in principle, just not previously
  spelled out for this specific pair of functions).

- **`000000`'s existing hand-transcription re-confirmed unaffected** by
  either finding above — it calls neither `FUN_1000_1e7b` nor the
  unpopulated window grids, and remains the cleanest, most self-contained
  piece of this subsystem. Still has no safe wiring point until its two
  real callers (`0009ae`'s own "stay" validation, `0015c1`'s post-flood
  neighbor-scoring tail) are themselves portable.

**Net for this pass**: given the two blockers above are both new and
genuine (not restatements of the already-known "structural not semantic"
caveat), forcing a `0009ae`/`0015c1` port now would mean inventing
behavior for an unresolved load-bearing accessor (`FUN_1000_1e7b`) and an
unfound data-population step (the window grids) — against this project's
own "never invent" convention. **No `src/` change this pass.**
`units_bfs_next_step` re-checked as the far tier's sole caller (one call
site, `units.c:4832`, verified by grep) — stays exactly as-is, not
retired; nothing here makes it safe to replace. Real next step if
resumed: chase `FUN_1000_1e7b`'s actual return-value semantics (its two
`JNZ` targets and the `LAB_1000_39e1`/`39de`/`39f1`/`397d` state block),
and separately, find what populates the `-0x790a`/`-0x7a18` per-domain
walkability grids (likely a setup pass inside `0f74` itself, not chased
this session). `ctest`: rebuilt fresh in this pass's worktree (had to
symlink the gitignored `COLONIZE/` asset directory in from the main
checkout for the unit tests to load `NAMES.TXT` at all — a local,
non-committed fix, not a code change) — 40/41 green; the one failure
(`unit_ai_king`, a "multi-unload fortify count" assertion) is a
pre-existing baseline condition unrelated to this row (confirmed via
`git status`: zero `src/` files touched this pass), not a regression.

**2026-08-24, third pass — both blockers chased with force-clear/XREF/operand-
scan techniques instead of a live capture; both substantially resolved, one
new correction to the prior pass's own finding, no `src/` change (still not
safe to port `0009ae`/`0015c1` — new open sub-items, not old ones).**

- **Blocker 1 (`FUN_1000_1e7b` semantics) — resolved: it hands back nothing
  real, and the prior pass's "1000:a46e Chebyshev-8-gated shortcut" citation
  was a stale-analysis misattribution.** Force-redecompiled flat `1000:a46e`
  fresh (`clearCodeUnits` + `disassemble` + `createFunction`, the same
  technique this doc's 2026-08-15 "third pass" used on an unrelated
  function) instead of trusting the saved project's cached decompile: the
  real bytes there are a trivial 10-byte, 2-instruction relay —
  `CALLF 1000:1e7b; JMPF 0000:0906` — decompiling to
  `void FUN_1000_a46e(void) { FUN_1000_1e7b(); FUN_0000_084a(); return; }`.
  **Not** the Chebyshev-8 gate (`|x-goal|<8 && |y-goal|<8` → writes
  `DS:0x1dd2`/`0xa14e`/`0xa14c`/`0x1dd6`, calls `0015bc(0)`, reads `0xa370`)
  the 2026-08-24-earlier entry above attributed to it — that citation came
  from letting the decompiler chase `0015b2`'s `JMPF` through the *cached*
  project database instead of the real bytes; cross-confirmed by XREF:
  `0015bc`'s actual 4 callers (`GhidraListXRefs`, `OVL20_L0000` offsets
  `98f`/`10bb`/`1107`/`113e`) do not include `a46e` or anywhere in the
  resident space at all. **`FUN_1000_1e7b` itself, decompiled fresh and
  independently at its own address, is `void`**: a critical-section guard
  (`LAB_1000_39e1=='\0' && (LAB_1000_39dc_2&0xc)==0`) — guard-open path sets
  the busy flag, calls `FUN_1000_40a2()`, then branches on the *caller's
  own* pre-call CF/ZF (not anything `40a2` sets — confirmed via raw disasm:
  the `PUSHF` is 1e7b's own first instruction, popped right before the
  branch, so these are literally the flags the caller had at the `CALLF`)
  into one of 3 message-pointer setups ending in an indirect jump Ghidra
  can't resolve (`Could not recover jumptable... too many branches`);
  guard-blocked path calls `FUN_1000_1d4f()` (itself real but clearly
  unrelated to pathfinding — touches `unit+0x3146`/the `0x5232` stride-`0xe`
  unit-type table and an unrelated purchase/turn-limit counter pair
  `DAT_2000_92c2`/`92c4`) and returns *its* result. Chased one level
  further: `FUN_0000_0906` (`a46e`'s tail-jump target, independently
  force-created/decompiled) is itself another trivial thunk
  (`FUN_0000_084a(); return;`), and `FUN_0000_084a` is *also* `void`
  (2 params, calls `FUN_0000_c28a(0)` then
  `FUN_0000_c11c(0,param_1,param_2,DS:0x268a,DS:0x268c)`) — every link in
  this chain is `void` per Ghidra's own independent signature inference at
  each address, yet multiple *callers'* decompiles (`0015b2`'s Chebyshev-8
  body, and a genuinely separate population routine found below) show code
  using "the call's return value" in real comparisons — a decompiler
  rendering artifact, not real callee behavior: the value being compared is
  whatever the caller's own register already held before the `void` call,
  surviving through untouched. `084a`'s real shape (a screen/video-memory
  write gated behind a critical section, 2 caller coordinates plus 2 fixed
  `DS:` params) reads as a strong match for the *already-documented*
  "AI pathing on-screen debug/spectator visualization" toggle
  (`0015bc`/`0015c1`'s own `DS:0x1df2`/`0x1df4`-gated popup/dialog tails,
  2026-08-20/24 entries above) — i.e. likely debug instrumentation, out of
  scope for a gameplay port, not a second independent finding needing its
  own chase. **Net: `FUN_1000_1e7b`'s return value does not gate
  `0009ae`/`0015c1`'s logic, because there is no real returned value to gate
  with — the original blocker's premise doesn't hold.** **Still open**: the
  *real* Chebyshev-8-gated near-flood-shortcut logic the prior pass
  described is genuine, verified DOS code (confirmed via raw disasm, ending
  `RETF 0x6`) — it just lives somewhere else: a distinct, previously-
  uncatalogued function physically inside `OVL20_L0000` itself, roughly
  offset `0x8f0`-`0x9ab`, one of `0015bc`'s real 4 XREF callers (its own
  call to `0015bc` is the hit at `0x98f`). Its true entry point (`0x8f0`
  itself has zero XREFs and `createFunction` fails there — collides with
  something reachable via recursive-descent disassembly, so the real start
  is earlier, reached by fallthrough) and its own caller are not pinned
  down this pass — precise next step if resumed.

- **Blocker 2 (walkability-grid population) — resolved: found the
  populator, its own caller not yet found.** The "`-0x790a`/`-0x7a18`
  stack-relative" framing in the 2026-08-24-earlier entry above was itself
  a decompiler artifact — raw disasm of `0009ae`'s own reads
  (`GhidraDumpBody`, custom one-off script) shows the true addressing is
  `[BX+SI+0x86f6]` / `[BX+SI+0x85e8]`, i.e. two **fixed absolute `DS:`
  addresses** (`0x86f6`, `0x85e8`; `-0x790a`/`-0x7a18` are the same 16-bit
  bit pattern, just sign-reinterpreted by Ghidra's BP-relative heuristic —
  not a real multi-kilobyte stack frame). A custom `GhidraFindDisp.java`
  one-off (scans every already-disassembled instruction's operand scalars
  for a literal match) swept the resident block plus **all 31 overlay
  blocks** for any reference to these two literals: the *only* writer is
  **`FUN_OVL21_L0040__0007ef`** (DOS-canonical segment ≈`67f4`, a "gap"
  address in `address_mapping.csv` — not previously catalogued under any
  name). It `memset`s both tables to 0 via the already-known
  `FUN_0000_df7e` (confirmed project-wide `memset(ptr,val,len[,seg])`
  equivalent — same `0x10e`-byte size constant also appears in
  `euro_goal_orders_0a60_full.md`'s own unrelated `FUN_0000_df7e` call),
  then walks a nested row/col/4-direction loop, step-4 sampling across
  what resolves to an 18-wide window (matches the documented `0x12` stride
  exactly, and explains "`0009ae`'s own body implies... ×4-scaled
  quantities" from the 2026-08-24-earlier entry — that's literally this
  loop's own step size) calling `FUN_1000_84f2` plus the *same*
  `1000:a46e` relay chain (here invoked with 4 real args and an
  `0<result<8` octant-range gate — consistent with the debug-draw read
  above, or possibly a genuine octant/direction classifier reused by both;
  not disambiguated) to decide which bits to set per domain. The same
  function's tail (once both domains are done) also zeroes and recomputes
  two unrelated per-nation aggregate tables at `0x945e`/`0x85c8` — these
  are the *already-resolved* `-0x6ba2`/`-0x7a38` locals from this doc's own
  much earlier Treasure-Train-tension finding (`land_combat_strength`-style
  bookkeeping) — confirming `0007ef` is a broader **AI turn-start cache
  refresh**, not a narrow pathfinding-only helper. **Still open**: nothing
  in this project's current XREF index calls `FUN_OVL21_L0040__0007ef` at
  all (0 hits) — plausibly reached via a computed/indirect dispatch, or its
  true entry is earlier than `0x7ef` (same fallthrough pattern as blocker
  1's still-open shortcut function). Finding that caller is required to
  know *when* this population runs (once per AI turn vs. once per
  individual pathfind call) before it could be safely modeled in Linux —
  precise, well-scoped next step if resumed, not chased this pass.

**Net for this third pass**: both blockers are substantially further
resolved via static tooling alone (force-clear/XREF/custom operand-scan —
no live DOSBox-X session needed, and none is judged necessary yet: every
open sub-item above is still a static-analysis question, XREF/backward-scan
territory, not something only runtime behavior could answer). But neither
is closed all the way to "safe to port `0009ae`/`0015c1` without inventing
anything" — two *new* callers (the real Chebyshev-8-shortcut function's own
caller; `0007ef`'s own caller) are the concrete missing pieces, not
restatements of the walkability-grid/1e7b questions this row started with.
**No `src/` change this pass** — the one correction found (the `a46e`
misattribution) doesn't touch any currently-shipped code (`0015bc`'s own
edge-cost wiring in `units.c` never referenced `a46e`/`1e7b`). `ctest` not
re-run (doc/investigation-only pass, confirmed via `git status`: zero
`src/` files touched).

Re-checked with a small (0x100-byte) force-clear + fresh disassemble
(bypassing any stale analysis, same technique used elsewhere this
session): the corruption is **confined to one case (`case 4`) of a small
internal `switch(*(int*)0x9e4e - 1)`** near the function's entry (a
`f000:66b0` pcode target — classic wild-jump garbage — inside just that
one case). Cases 1/2/3/5/6/7/8 of that same switch decompile as small,
plausible (if not yet named) low-level operations. **Case 7 falls through
to the real body** (a C `break`, not a `goto` — genuine control flow, not
a merged-function artifact), so the switch is very likely part of this
function's own logic, not misattributed content — unlike the
`15eb_1d4c`/`684c_08c0` false-boundary class, this reads as a real,
mostly-intact function with one bad corner.

**The body after the switch is clean, coherent, and uses entirely real,
already-known fields**: `unit+0x3149` (moves spent), `+0x314c` (orders),
`+0x315a` (turns-worked, wraps at 20), the `0x2f76` terrain table (same
one `0f74`'s pathfinder reads), and — the actual find — repeated
diplomacy-flag reads (`FUN_1000_8c28`, `nation*0x13c-0x77c4`/`-0x77b8`,
matching `difficulty.md`'s already-known `euro_relation`/tension table)
gating **popup dialog calls** (`FUN_1000_8842` with catalog ids
`0x13ba`/`0x13ad`/`0x13cb`/`0x13d7`, name-formatting helpers
`FUN_1000_8c0a`/`func_0x00018b94`, `FUN_1000_8628` for string-arg slots).

**Correction (2026-08-15, third pass) — the gating bit is `MET`, not
`PEACE`.** Cross-checked `FUN_1000_8c28`'s `&0x40` bit against ground
truth instead of trusting the (inconsistent, across 3 different docs this
project already had) "0x40 = ..." guesses: Linux's `ai_diplo_read`
(`ai_diplo.c`) returns the DOS `euro_relation` byte **completely raw, no
remapping** (`return *f;`), and its own `#define AI_DIPLO_MET 0x40` is
that literal DOS bit — confirmed by `ai_diplo_write`'s callers only ever
OR/AND-ing these same DOS-numbered bits directly into the save-format
byte. So `FUN_1000_8c28`'s bit `0x40` is **"have these two nations met,"
not "are they at peace."** (`move_scoring_20e6_full.md`'s own separate
"`0x40` = at-war flag" citation is *also* wrong by this same evidence —
flagging there too, not fixed in this pass.) Re-reads below as "met",
correcting the previous "peace holds" framing throughout — this arguably
fits a territorial-notice mechanic *better*: you can't sensibly be told
"England violates your territory" about a nation you haven't met yet.

This is a richer "two different-nation units meet on the map" handler
than first read, with (at least) four distinct outcomes gated on pairwise
`FUN_1000_8c28` MET-flag (`&0x40`) checks between the acting unit's
nation (`uStack_44`) and an encountered nation (`uStack_e`, found via a
unit search at/near the destination):

1. **Correction (2026-08-15, fourth pass) — traced which unit's Treasure
   this checks; it's the acting unit's own, not the encountered one.**
   Chased the variable feeding the type check (`iVar12`) back to its
   assignment (line ~342: `iVar12 = FUN_1000_89d0(...)`, the found-unit
   search that also sets `uStack_e`) — `iVar12`'s own type is checked
   first (skip this whole arm if the *encountered* unit is a Treasure);
   the actual trigger, one level in, is `iStack_6` (**the acting unit
   itself**) being a Treasure (`0x3146==0x10`). So: **the AI's own
   Treasure Train, moving near a foreign unit, has a chance to flag
   tension against that nation** — thematically "hauling a fortune near
   a rival makes them suspicious/covetous," not "you noticed their
   treasure." Sets `nation[uStack_e].euro_relation[uStack_44] |= 0x80`
   (note the index order — the *other* nation's opinion of the acting
   nation), then an RNG roll (`<difficulty+1`) compares a table at
   `nation*2 + -0x6be4` between the two nations to decide `|=2` or `|=8`.
   No popup, pure state.

   **`-0x6be4` resolved — it's already fully live in Linux, no unknowns
   left**: `mod 0x10000` = `0x941c` = `save_format_map.md` row 247,
   `land_combat_strength[4]`, already computed every turn in
   `col1_stuff_census.c` (`ColonizeCol1Save.stuff.land_combat_strength`).
   So the RNG branch is: weaker `land_combat_strength` → bit `2`, stronger
   → bit `8`.

   **Still not ported — a real semantic puzzle, not a missing-data one,
   caught before shipping anything risky.** Bit `0x02` in this same byte
   is *already* `AI_DIPLO_PEACE` per the `ai_diplo_read` ground-truth this
   pass established — a passive Treasure-Train encounter conditionally
   setting the PEACE bit (on an already-default-peaceful unmet pair) reads
   as a near-no-op most of the time, which is a suspicious fit for a
   "tension" mechanic. Bit `0x08` is completely unmapped in Linux's
   4-bit `AI_DIPLO_*` enum (only `WAR`/`PEACE`/`ALLY`/`MET` — `0x02`,
   `0x01`, `0x04`, `0x40` — are modeled; `0x08`/`0x10`/`0x20`/`0x80` are
   real DOS bits this port has never needed before now). Given this
   touches the *same byte* the tested diplomacy system already reads/
   writes, wiring it on an unconfirmed bit-8 meaning risks a real
   regression, not just a wrong comment — **deliberately not shipped this
   pass**.

   **Update (2026-08-15, `153e` bit-semantics pass) — resolved, and
   shipped.** The "grudge/pressure" reading of bit `0x02` was a false
   alarm: `FUN_1000_8c28` (the accessor both `049e` and `153e` call)
   decompiled directly (`FUN_0000_5b34`) turns out to be a **pure raw-byte
   accessor with a nation-range branch** — param `<4` (Euro) reads exactly
   `euro_relation` (`-0x77c4`), param `>=4` (Indian tribe) reads a wholly
   *different* table (absolute `23000`, stride `0x4e`). The `153e` call
   sites that looked like "grudge, not peace" were all Indian-range calls
   into that other table — irrelevant to the Euro-Euro question. The
   *actual* Euro-Euro bit-2 sites in `153e` (direct `-0x77c4` reads, no
   accessor) are fully consistent with plain `AI_DIPLO_PEACE`: discounts
   negotiation "worthiness" when already peaceful, and gets set right
   alongside establishing contact — mirrors this port's own `ai_diplo_read`
   "unmet defaults to PEACE|MET" convention exactly. Bit `0x80` (set/
   cleared elsewhere in `153e`, a real transient flag) stands as found.
   Bit `0x08` still has no Euro-Euro-confirmed site in `153e` — genuinely
   open, not misattributed like bit 2 was.

   **Shipped**: `ai_euro_treasure_tension_bump` now writes the real DOS
   bit 2 (`AI_DIPLO_PEACE`) for the weaker-rival branch instead of a
   Linux-only stand-in, with one own-addition guard (skip if already at
   war, to avoid an internally contradictory WAR+PEACE byte — DOS itself
   doesn't guard this, but nothing here depends on matching that edge
   case exactly). Full `ctest` 42/43, same pre-existing baseline, no
   regression. Bit 8 keeps its Linux-only `AI_DIPLO_TREASURE_STRONGER`
   stand-in pending further tracing.
2. `A` has met `B` → format **only `uStack_e`'s** name into slot 0,
   popup **`0x13ba`**, 1-button — but the code then branches on the
   *return value* `!=2`, unusual for a plain 1-button "OK" dialog (the
   3rd `FUN_1000_8842` arg may not mean "button count" the way assumed
   last pass).
3. Reverse relation-flag check (`-0x77b8`) can skip the rest entirely.
4. `B` has met `A` → format **only `uStack_44`'s** name into slot 0,
   then `FUN_1000_869c(4)`/`FUN_1000_8b88()` — **no popup call at all**,
   looks like queuing a pending action/event rather than showing a dialog.
5. Either direction holds → format **both** nations' names into slots 0
   and 1 (`FUN_1000_8c0a` for Euro-non-crown → popup **`0x13cb`**; the
   shorter-form `func_0x00018b94` for crown/other → popup **`0x13d7`**),
   then actually calls `FUN_1000_8842` with 2 string args.

**Revised candidate: `0x13cb`/`0x13d7`, not `0x13ba`, now looks like the
better `@VIOLATE` match** — `@VIOLATE` needs two *nation* names
(`%STRING0`/`%STRING1`) plus a place (`%STRING2`); only the dual-nation
pair (5) names two nations at all, `0x13ba` names just one. `%STRING2`
(the place) isn't set by an explicit `FUN_1000_8628` call anywhere in
this function — plausibly auto-filled by the dialog engine from the
already-current unit/tile context rather than an explicit arg, which
would be consistent with not finding a third `8628` call. **Which nation
is `%STRING0` (violator) vs `%STRING1` (owner) is not established** —
slot order in the code is `uStack_e` then `uStack_44` for the Euro-
non-crown case, reversed sense not ruled out.

`popups.md`'s own entry for `@VIOLATE` explicitly says "Zero code refs;
trigger function not found" — this is still the first real lead this
project has had on it, just refined to a different (and better-fitting)
call site than initially guessed. `0x13ba`/`0x13ad` remain real, found,
but separately-unidentified single-nation notify popups (not necessarily
`@VIOLATE`) — worth naming on their own if this thread continues.

**Not confirmed to full certainty and not ported this pass** — mapping
raw hex catalog ids to `GAME.TXT` tag names still needs a numeric-catalog
decode this project doesn't have built; the violator/owner slot order and
the unusual `0x13ba` return-value check are open questions. But the
underlying mechanism (real diplomacy checks, real tension-flag writes, a
genuine two-nation notify dialog) is solid and well-evidenced regardless
of the exact tag match. Flagging as a high-value, close-to-resolved
target for a focused follow-up (ideally with a live capture to confirm
the id↔tag mapping directly) — smaller and more concrete now than the
`0f74`/`5d04` large-body items.

**2026-08-14, same day — checked cases 8/9 against Linux's existing
Pioneer plow/road port (`units_pioneer_work_tick` in `units.c`), found
it already faithful on the core timing and shipped one confirmed gap.**
`units_pioneer_work_needed` (`terr_cost + 2` for plow/clear, halved for
`profession == UNITS_JOB_PIONEER`) already matches case 8/9's
`table[terrain]+2, >>1 if unit+0x315b=='\x14'` formula exactly — this
independently **confirms profession code `0x14` = Pioneer** (previously
flagged unconfirmed above). `units_pioneer_tile_can_clear_or_plow`'s
`pedia 8..23` clearing range also matches case 8's forest-range check
(`[8,15]∪[16,23]`) exactly. Case 8's completion reward was already
ported with an explicit approximation (`units.c`, "FUN_479b_01a6 clear:
lumber → nearest same-nation colony... Thin add=20 (terrain×20/Hardy×2
PARKED)") — correctly still parked, the real formula needs the unmapped
`0x2f80` table values.

**Case 9's completion reward had no Linux port at all — implemented this
pass.** DOS is a flat, fully-resolved formula (no unmapped table, no FF
gate): nearest same-nation colony (no radius limit —
`FUN_281f_0614(x,y,nation,0xffff)`) gets `hammers_purchased += 10`.
Added to `units_pioneer_work_tick`'s road-completion branch, mirroring
the existing clear-forest "nearest own colony" search pattern. Real bug
caught before it shipped: `units_pioneer_road`'s public signature didn't
take a `colonies` parameter at all (unlike its `units_pioneer_plow`
sibling) — the new code would have been silently dead behind a `NULL`
check at every real call site, same class of mistake as this session's
naval-ambush placement bug. Added the parameter, threaded it through all
4 call sites (`ai_euro.c` ×2, `game_loop.c` ×2) and 2 existing tests, and
added a dedicated `unit_units` test that drives a real road completion
and asserts `hammers_purchased` actually moved (5→15), not just that
nothing crashed. Full `ctest` green (42/43, same pre-existing baseline
failure).

**⚠ Everything under "Phase outline" below cites `viceroy_unpacked.c` line
numbers in the ~90446–92260 range — that entire range is the corrupted
blob this correction replaces, not real content.** Section "0" and "1"
and "2. Case `0x0b` settle-adjacent notes" describe DOS behavior that does
not exist at those citations; treat their DOS-side claims as unverified.
The many "Linux thin — ..." subsections from `2b` onward describe actual
Linux port behavior and stay valid as behavioral documentation — they're
just not reliably tied to the specific DOS line numbers some of them cite
in passing. Not re-derived from the real `FUN_479b_*` handlers this pass
(out of scope — see the unmapped list above for what a full redo would
need).

This is a `switch(0x314c)` dispatcher with cases **7, 8, 9, 0xb/0xc, default**
— the same case numbers the phase table below documents — but each case is
a single call, not a multi-hundred-line inline body. **The elaborate
"Europe hire" / "ship-land act" phase content in this doc almost certainly
belongs to one of the callees, not to `5b66` itself.**

Followed the lead one hop further: `FUN_OVL14_L0000__007308` (the
unconditional call before the switch) turned out to be a bare
`JMPF 0x1000:a6e4` — one entry in this overlay's own local thunk table
(same mechanism as the `thunk_FUN_1000_*` stubs elsewhere, just not
auto-recognized as a named `Thunk` function by Ghidra's analyzer here).

**Correction (2026-08-13, later pass) — `a6e4` conclusion above was itself
wrong, same root cause as `4528`'s case-dispatch false lead.** The "data
table" read was from following an **unpatched RTLink call-thunk's raw
placeholder bytes** (`JMPF 0x0000:XXXX` — segment `0000` is a
build-time sentinel RTLink's loader patches at runtime; it is *not* the
real target, and reading through it naively decodes whatever static bytes
happen to sit at that literal file position — meaningless, and exactly
what produced the "12-byte repeating pattern" / "looks like a jump table"
read). Same failure mode `4528`'s case-dispatch chain hit (see
`indian_settlement_4528.md` "Case-dispatch tail" section) — solved there by
resolving the *real* target through `rtlink_decode VICEROY.EXE`'s own
jump-table parser (info mode) instead of trusting the placeholder bytes.
Applied the same fix here: `OVL14_L0000::7308`'s thunk chain → resident
`ram:0x1a6e4` → file offset `0x1cae4` → **`rtlink_decode`'s jump table
resolves this to segment index 12, offset 0** (`OVL12_L0000`'s entry
point) — a real, clean, ~145-byte self-contained function (tribe search:
matches a tribe by relative position + type nibble, sets a found flag and
sentinels the match; falls through to a `FUN_1000_8842`/`8628`/`8b94`
dialog call chain if no match). Full C decompile hits an unrelated pcode
error; the raw disassembly is coherent and legible, confirmed via
`docs/rtlink_decode_v2_gap.md`'s tooling.

**Pcode error root-caused (2026-08-13, task #2 close-out) - decompiler
bug, not corruption.** `Offset must be between 0x0 and 0x10ffef, got
0xffffffff` on this function (and independently on `FUN_5fef_0000`, see
`indian_raid_loot.md`) is **not** the disassembly-fault class every other
entry in `decomp_inventory.md` documents. Checked at the raw-pcode level
(`Instruction.getPcode()` per instruction, before the decompiler's
higher-level SSA pass): every instruction's pcode is clean - no
constant/unique varnode carries the `0xffffffff` sentinel. The bug lives
inside the decompiler's own call-target/segment resolution for
`CALLF 0x1000:XXXX`-style far calls specifically when they occur inside
these two overlay-space functions (the *same* literal calls, e.g.
`FUN_1000_8628`, decompile fine as named calls from other, already-working
functions elsewhere in the project - so segment `1000` itself is a real,
correctly-mapped address space; the failure is local to these two callers,
not the callee segment). Not worth chasing into Ghidra's decompiler
internals for two ~150-360-byte functions - hand-transcribed instead from
the confirmed-clean raw disassembly:

```c
// OVL12_L0000:0000 - tribe search + no-match dialog fallback.
// param_1 (BP+6), param_2 (BP+8): meaning not independently confirmed -
// read from context (a6e4's caller site, "tribe search" framing above).
// DS:0x54ee/0x54f1 table: stride 0x12 (18) bytes/record, byte @+0 and
// byte @+3 read here - record layout/count source (DS:0x539a) not named.
// DS:0x543f table: stride 0x34 (52) bytes/record, indexed by param_2.
int FUN_OVL12_L0000_0(int param_1, int param_2) {
  int found = 0;
  for (int i = 0; i < *(int16_t *)0x539a; i++) {
    uint8_t *rec = (uint8_t *)(0x54ee + i * 0x12);
    if ((uint8_t)(rec[0] - (uint8_t)param_1) == 4) {
      if ((rec[3] & 0xf) == param_2) {
        found = 1;
        rec[3] = 0xff; /* sentinel the match so it isn't picked twice */
      }
    }
  }
  if (found) {
    int a = FUN_1000_8c0a(param_1 + 4);
    FUN_1000_8628(a, 0);
    int b = FUN_1000_8b94(param_2);
    FUN_1000_8628(b, 1);
    if (param_2 >= 4 && *(uint8_t *)(0x543f + param_2 * 0x34) == 0) {
      FUN_1000_8842(0x14c8, 1); /* likely a message/dialog id + flag */
    }
  }
  return found;
}
```

Not ported to Linux - same "needs unlabeled DS globals named first"
blocker as `FUN_4d56_417e` (task #5); disassembly-level task #2 is closed,
semantic porting stays deferred pending that naming pass.

**2026-08-14: the DS globals ARE now named — the record layout, not the
caller, is what's still blocking this.** `DS:0x54ee`/stride `0x12` is 2
bytes into the settlement-record array fully mapped in
[`settlement_record_8d4a.md`](settlement_record_8d4a.md) (base `0x54ec`,
same stride) — `rec[0]` here is that doc's `+2` (`type`, index into the
8-entry type-profile table) and `rec[3]` is `+5` (`owner_flags`; this
function's `rec[3] = 0xff` write sets the owner nibble to "none" *and*
the sign-bit sentinel that other sites read as "record invalid/inactive"
— reads as **eliminate one settlement of a given type belonging to a
given nation**). `DS:0x543f` stride `0x34` is the per-nation
AI-difficulty/control table used pervasively elsewhere (`param_1*0x34+
0x543f`, e.g. `ai_king` control-status checks). So the callee's own body
is fully legible now.

**Resolved same day, later pass — the whole `a6e4`/`007308` thread was
chasing the wrong address.** `FUN_OVL12_L0000_0` is not reached through
`5b66`'s dispatcher at all; it doesn't need Ghidra hand-transcription
either. It's `FUN_4cc6_0000` (`viceroy_unpacked.c:80774-80802`), already
sitting fully clean/uncorrupted in the canonical export, real parameter
names and everything (`param_1`=type-4, `param_2`=owner nation — matches
this doc's transcription byte-for-byte). Its real caller is
`thunk_FUN_2a1f_0398`, fired from `FUN_4cc6_0092` (peer diplo helper),
`FUN_4cc6_00f2` (the already-known Indian relation-delta function, on a
low-relation branch), and directly from `FUN_4d56_1816` (Indian nation
turn) inside a previously-undocumented War-of-Independence tribe-defection
branch — see [`indian_woi_defect_1816.md`](indian_woi_defect_1816.md) for
the full mechanic this unblocked. `FUN_OVL14_L0000__007308` really is the
giant move-scoring gate as the phase-outline said; the old `a6e4` prose
above was simply investigating a different, wrong address from the start,
compounded by not checking the plain canonical export first (the
`FUN_4cc6_0000` copy was sitting there in the same file the whole time —
same lesson as the G-table/`153e` passes: check canonical before Ghidra).
Semantic porting of `FUN_4cc6_0000` itself now blocked only on deciding
whether it's worth porting standalone vs. as part of the WoI-defection
mechanic that calls it.

**The broader "12-byte pattern" region** (the run of `JMPF 0x0000:20e6`/
`5c3c`/`0a60`/`0072`/`00a8`/`02be`/`5cf6`/`052c` entries near `a6e4`) is,
by the same logic, **not data** — it's more unpatched RTLink call-thunks in
the same mechanism, each individually resolvable the same way (compute its
own file offset, look it up in `rtlink_decode`'s jump table). Not resolved
individually this pass; don't re-read them as a "data table to extract" —
that framing was the mistake.

**Method note for whoever continues this file:** when a `CALLF <loader>;
JMPF 0x0000:XXXX` stub's decompile looks implausible (turn-loop-sized
content from a 10-byte function, or a "data table" pattern from a function
Ghidra won't create), the `JMPF` target is very likely an unpatched RTLink
placeholder, not real control flow — resolve it via `rtlink_decode`'s jump
table (file offset → segment index + offset) before concluding anything
about what the code does. Naive tail-following or byte-pattern reading
through these placeholders has now produced two false leads in this file
alone (`a6e4` "data table", and — see `indian_settlement_4528.md` — an
"8 raid actions" reading that was actually one shared utility). The
case-7/8/9/0xb *dispatch targets* (`FUN_1000_93ea`, `func_0x000193b2`,
`FUN_1000_9406`, `FUN_1000_96aa`) are still-uninvestigated in their own
right — check whether each is itself a real function or another unpatched
thunk before trusting a decompile of it.

---

Layer D early-settle map only. `5b66` itself is the 44-line dispatcher at
the top of this file, not ~1815 lines — that estimate and the
`~90446–92260` range describe the corrupted blob, not real content (see
"Case dispatch targets resolved" above). The real per-state bodies are
`FUN_479b_076e`/`01a6`/`0526`/`0972` and `FUN_1427_155e` (76722–77122 and
8880–8888). **Mid-planner combat / deep case-7 economy / deep land
scoring slices are OPEN** (unpark #4) — now anchored to those real
functions, not the old fictional line range; many thin peels (dock hire
matrix, construction prefers, haul, fortify/wake, naval prey) are **Done**.

Linux: `ai_euro_unit_act` + expand/war thin — deepen vs peels (**OPEN** remainders).

## Entry / wiring

| Item | Detail |
|------|--------|
| Ghidra | `FUN_521d_5b66` |
| Thunk | `2a1f_0488` from `FUN_521d_6d8e` ship/land act loop |
| Args | Decomp shows corrupted far prototype; live arg = **unit index** |
| Annotated | `euro_unit_act` in [`euro_dispatcher.c`](euro_dispatcher.c) |

Not nested inside `20e6`. Goals are `0a60`/`5d04`; scoring is `20e6`; act is `5b66`.

## Phase outline

### 0. Early move-scoring gate (~90552–90580)

```
if moves_spent == 0 OR orders != 0x0B (goto):
    r = 2a1f_04f4 → FUN_521d_20e6 (move_scoring)   @90557
    if r != 0: return
else: path validate (281f_0984); order 'E' Europe-counter tweaks
if orders-7 > 5: clear orders (0934); return
switch (orders) cases 7..0x0b
```

### 1. `switch (314c)` arms (bodies; mid-planner **OPEN**)

| Lines | Case | Label |
|-------|------|-------|
| 90589–91142 | **7** | Europe hire (`0500`/`5c3c`), founding urgency, treasury buy — **partial** (Linux: dock expert matrix Done; deep economy/treasury OPEN; see §2d / §2e) |
| 91143–91158 | **8** | short |
| 91159–91194 | **9** | short |
| 91195–91362 | **10** | UI/chrome / dialog-ish (`281f_04ac` ≠ `06ae`) |
| 91363–92150 | **0x0b** | Ship/land act: ocean probe, naval band, dir8 score |

### 2. Case `0x0b` settle-adjacent notes (**OPEN** deepen)

| Lines | Concern |
|-------|---------|
| 91583–91591 | Unload / labor — `colony+0x8e--`, order `'G'` | **Done** thin (`labor_shortage` + admit) |
| 91603–91616 | Goal-priority → order `'B'` |
| 92151–92167 | Fortify? colony-check → order `'F'`, dir=8 |
| 92176–92212 | Apply orders 5/6/0xc; idle → `'0'` |
| 92243–92255 | Naval + order `'1'` → `'B'`; clear when goal tile reached |

Post-act primary upsert for exhausted ships lives in **`6d8e`**, not here.

### 2b. Linux thin — naval war hunt (act-level)

When nation is at war with a Euro peer, ships **not in Europe** that are idle /
station-keeping get `AI_SAIL` toward the nearest enemy sea unit or coastal water
beside a foreign colony at war. Adjacent enemy ships call `ai_euro_try_attack` /
`units_resolve_naval_combat`. Deep `20e6` naval combat scoring stays **PARKED** (ocean/T3).

**Coastal Fort/Fortress fire (Marathon8):** EOT `units_coastal_fort_fire_pulse`
(`FUN_364b_03f6`). AI: flee battery adjacency before hunt; war-hunt skips
fort-fire tiles; ocean score −800 into batteries. Cite: fandom Fort/Fortress;
ship-slow formula still **PARKED**.

**Privateer deepen:** display-name Privateer always re-aims hunt (even with a prior
sail goto) — commerce-raid stand-in; reuse `naval_war_hunt_target`. Cite: Europe
Privateer purchase; fandom Drake Privateer combat strength.

**Privateer cargo prey (adjacent):** when choosing naval `try_attack` target,
prefer Merchantman/Caravel cargo ships over warships (then lower defense). Cite:
euro_unit_act §2f; Europe Privateer commerce raid.

**Frigate warship hunt (adjacent):** Frigate prefers warships (Frigate /
Privateer / Galleon / Man-O-War) over cargo when adjacent — complement Privateer
cargo prey; then lower defense. Cite: euro_unit_act §2f; Europe Frigate purchase.

**War transport deepen (Galleon/Frigate/Man-O-War):** at war, idle Galleon /
Frigate / **Man-O-War** with passenger space (`cargo_count < ship_capacity`)
prefers `AI_SAIL` toward coastal water by a **threatened** own coastal colony
(war-peer unit within MD≤3); else falls back to naval war hunt (foe sea / enemy
coast). Cite: Colonization.pdf naval transport; Europe purchase Galleon/Frigate;
Jones Frigate/MoW fallback; king_ref MoW. Full ships without space keep plain hunt.

### 2c. Linux thin — land war hunt (act-level)

When at war with a Euro peer, **or** Indian hostility sticky with a tribe/Brave
on the map, idle land military (Soldier / Dragoon / **Regular** /
**Continental** / Scout — including formerly fortified/sentry) get `AI_MOVE`
toward the nearest enemy land unit, enemy colony, or **at-war native Brave /
tribe tile** (prefer `tribe.state.capital`). Idle `FORTIFY` / `FORTIFIED` /
`SENTRY` are woken via `units_wake` then hunted. Adjacent → `ai_euro_try_attack`,
preferring the foe with lower effective defense (fortified ×2); Indian adjacent
requires `ai_diplo_indian_at_war`. Does not steal founders on FOUND goals.
Act-level hunt / peace-border / scout explore share thin MP-drain goto advance
with FOUND/MILITARY/CONTACT (§2c3). Adjacent combat **chains** while
`moves_left` remain after enter (cap 8). Full multi-step `20e6` combat scoring
remains **PARKED**. Cite: `ai_diplo_indian_*`; Cortes capital treasure path;
Colonization.pdf war / Defending a Colony.

**Alarmed tribe MILITARY (planning F):** friction>50 → MILITARY; capital tribes
prio 5 vs 3.

### 2c2. Linux thin — CONTACT scout rings (0a60 E / act)

Peace + own colonies ≥ 1: idle Scout upserts `AI_GOAL_CONTACT` at a Manhattan
ring tile (MD 2–4) around the nearest beyond-adjacent tribe and `AI_MOVE`s
toward it. When `map.seen` exists, prefer tiles **not** seen by the nation
(`map_tile_seen_by` / Col1 FoW bit) — explore intent, not combat bonuses.
When `ai_diplo_indian_hostility_sticky` ≥ 2 (`unknown26[8]` very-low deepen),
prefer **closer** rings (higher MD weight) when fog is absent. **Sticky + FoW:**
when `map.seen` exists, prefer **deeper unseen** ring tiles (md=4) to push fog
outward; act re-aims even with a prior CONTACT goto. Cite: `euro_diplo.md` /
`ai_diplo.h`; manual fog / Col1 seen bit.

**Fog explore (no CONTACT):** when no beyond-adjacent tribe ring exists, peaceful
Scout `AI_MOVE`s toward an unseen land tile within MD ≤ 8 (`map_tile_seen_by`)
without upserting CONTACT. Prefer `map_tile_has_rumour` over plain unseen when
both exist (Lost City Rumours seek; LCR resolve still on stand only — no invented
gold/FoY table). Plain Scout → nearest within the preferred tier; **Seasoned
Scout** → deeper (max md) within that tier — AI explore preference for the skill
"Better at exploring rumors…" (Colonization.pdf OTHER). Scouts already see 2
squares (de Soto: all units → "as well as scouts"); do **not** invent extra
sight radius or MP. Cite: Colonization.pdf Lost City Rumours / Seasoned Scout;
Pass5 LCR scaffold; manual fog / Col1 seen bit.

### 2c5. Linux thin — Treasure train coast (act)

Idle land unit named Treasure → `AI_MOVE` toward nearest **own coastal colony**
(`map_tile_is_coastal`). If none, nearest coastal land tile (Europe sail path
stand-in). Already on target → hold (park for Galleon / king transport). Cite:
Colonization.pdf Treasure Trains (six holds / coastal colony / king galleon for
a price). No invented ransom/gold. Preserve goto vs FOUND/LABOR yank.

**Treasure → Europe sail deepen:** when Treasure is already on a coastal own
colony and an own ship with passenger space is adjacent/same-tile →
`units_board` / `units_board_stacked` + ship `AI_SAIL` toward eastern high seas
(`units_find_eastern_high_seas_tile`) or eastward water (Europe exit stand-in).
Treasure passengers are skipped by settle unload. Ships with Treasure aboard
skip naval war-hunt yank. **Treasure → Europe gold (unparked):** when Treasure
(or ship carrying Treasure) is at Europe (`x/y≥200`) or on high seas, AI calls
`europe_cash_treasure` with COL1 `cargo_hold[0..1]` LE16 mirrored in
`hold_goods_amount[0..1]`; Treasure despawned. Value 0/unset → PARK (no invented
default). AI may also tick due Expected→Harbor (`cargo_treasure_gold`). Cite:
Colonization.pdf Treasure Trains; GAME.TXT `@LOOTCASH`. **PARK:** KINGGALLEON2
non-Cortes royal-galleon extra share (see `europe_cash_treasure`).

**Cortes KINGGALLEON3 coastal cash (unparked):** with FF Cortes, Treasure on an
own coastal colony cashes via `europe_cash_treasure` (tax = Crown share) without
boarding a ship (`ai_euro_try_cortes_king_galleon_cash`). Cite: fandom Hernan
Cortes; GAME.TXT `@KINGGALLEON3`.

**2026-08-14 investigation note (KINGGALLEON2, still correctly PARKED):**
read the two GAME.TXT tags side by side — `@KINGGALLEON3` (Cortes) says
"for no extra charge... taken a percentage equal to the current tax rate";
`@KINGGALLEON2` (non-Cortes) drops the "no extra charge" line and just
says "once our assessors have computed the Crown's proper share," with an
explicit Yes/No choice ("let the Crown claim its rightful share" /
"kiss your royal pinky ring"). Traced the cited crown-cut function
(`FUN_48d3_06ba`, `viceroy_unpacked.c:77943-78039`) looking for a second,
higher percentage or a decline path — it's more tangled than the docs
implied: two separate scan loops (one over combat/ship-type units
`0xc-0x13` computing `local_4`/`bVar4`, one over Treasure-type (`0x0a`)
units applying the *same* tax-clamped-at-50% formula already in
`europe_cash_treasure` and despawning them), joined by a UI-notify flag
(`*(int*)0x14c`/`0x14e`) gated on the *first* loop's result and "current
nation is human." Couldn't confidently determine within this pass whether
`local_4`/`bVar4` selects "human has an eligible Galleon" (→ auto-cash,
KINGGALLEON3-shaped) or "human lacks one" (→ CHOICE popup,
KINGGALLEON2-shaped) — the condition reads as the former on a first pass
but that contradicts the narrative (@KINGGALLEON2 is the *no-Galleon*
case), so something in my reading is inverted or this function isn't the
right one for the interactive CHOICE at all (it never branches on a
Decline outcome anywhere in the ~95 lines read). Genuinely unresolved,
not a quick fix — needs the actual CHOICE-dispatch call site found first
(not yet located) before either the percentage or the Decline behavior
can be ported with confidence. Stays PARKED.

**2026-08-14, later same day — real domain-knowledge confirmation of the
mechanic's shape, plus a real (partial) function-identification
correction.** Asked the user directly (per the "ask the user" method,
`decomp_inventory.md`): confirmed there genuinely **are** two different
percentages — with Cortes, or using your own Galleon, the King takes the
standard tax rate; without Cortes **and** without a Galleon, the King's
Galleon offer still appears but the cut is "extreme, far in excess of the
normal tax rate" (exact figure not recalled). This resolves the shape
question this doc left open — the mechanic really is two-tier, not a
single rate, so `local_4`/`bVar4` (or something like them) really should
gate a percentage choice somewhere.

**But `FUN_48d3_06ba` is very likely NOT that function.** Re-read its
Treasure-cashing loop (`77985-78028`) with the confirmed two-tier shape
in mind, expecting to find the branch — it isn't there. Every Treasure
unit found gets the *same* single tax-clamped-≤50% formula applied
unconditionally (no branch on `local_4`/`bVar4`, no Decline path, no
second percentage anywhere in the loop) — this function is a flat-rate
auto-cash routine, structurally incapable of expressing the confirmed
two-tier mechanic. The "eligible ship found" flag it sets
(`DS:0x14c`/`0x14e`) isn't a King's-Galleon-choice trigger either — traced
its one real consumer (the caller at `viceroy_unpacked.c:58376-58381`,
already independently documented in `europe_finish_bridge.md`) and it
just **opens the Europe screen focused on the ship**, a UI convenience,
unrelated to any percentage decision.

**Net effect: this doc's earlier "traced the cited crown-cut function"
premise was itself likely a mis-attribution** (same class of mistake as
`euro_diplo_153e_full.md`'s retracted false lead this same session) —
`FUN_48d3_06ba` isn't "the KINGGALLEON2 function," just a same-segment
neighbor that happens to also touch Treasure units. The real two-tier
percentage/Choice function is still unfound; a few other `0x3146=='\n'`
(Treasure-type) sites exist nearby in the same file (`78908`, `79208`,
others) but the ones checked so far are UI/display code, not the cash
mechanic. **Genuinely still open** — narrowed (one wrong candidate ruled
out, the real shape now confirmed from a decisive external source) but
not located. Stays PARKED; worth a fresh, dedicated pass if picked up,
now with a concrete two-tier-percentage shape to search for instead of
an ambiguous "maybe inverted" reading.

**2026-08-20 re-attempt (T1.9) — the "Treasure-type unit scan" search
vector is now fully exhausted, still no match; one promising near-miss
ruled out with live tooling.** Grepped every `*(char*)(unit+0x3146) ==
'\n'` (Treasure-type) site in `viceroy_unpacked.c` — 9 total, all now
individually checked (3 fresh this pass: `FUN_465b_0000`/75800,
`FUN_521d_6d8e`/93229, `FUN_2f2b_51ec`/51779; combined with the
previously-checked `europe_cash_treasure`-adjacent `77985`/`78908`/
`79208` and 3 more unremarkable UI/combat sites this pass ruled out —
`7640` tile-icon redraw, `99345` combat defender-softness check,
`89997` inside the already-exhaustively-read `FUN_521d_20e6`). None is
the King's Galleon CHOICE:
- `93229`/`51779`: unrelated (sticky-wave unit-type grouping; unit-info
  panel display formatting a treasure gold value for the UI).
- `75800` (`FUN_465b_0000`, the already-known move-foreign/combat
  dispatcher) looked genuinely promising at first — Treasure-type gate,
  per-nation `0x543f`/`-0x6da5` (stride 0x13, FF-flag-shaped) checks, and
  a real thunk call, `FUN_2a1f_0186`. **Resolved via live Ghidra
  tooling** (canonical decompile check, same method that cracked T1.7):
  `FUN_2a1f_0186` → `FUN_5fef_1908`, which `combat.md` already documents
  as **Treasure ransom/loot gold** (the "native raid captures your
  Treasure, human gets an Accept/Refuse ransom CHOICE" mechanic) — a
  different, already-known mechanic, not the Galleon transport offer.
  Flagging explicitly so a future pass doesn't re-cite this site as a
  candidate.
- Real conclusion: the King's Galleon offer is **not** triggered by a
  per-unit Treasure-type scan the way `europe_cash_treasure`/ransom/UI
  code is — it must live somewhere else (most likely a Europe-screen
  periodic/harbor-tick check on *aggregate* pending treasure gold or
  cargo-hold state, not a per-unit record scan). **Real next step if
  resumed**: search the Europe-screen tick/harbor-arrival family
  (`europe_finish_bridge.md`'s neighborhood) or search for a CHOICE
  dialog call site with exactly 3 `STRING` args and **zero** `NUMBER`
  args (KINGGALLEON2's own GAME.TXT template has no `%NUMBER0`, unlike
  KINGGALLEON3's — a real, checkable signature) instead of repeating
  this unit-type grep. Stays PARKED.

**2026-08-22 — both suggested leads checked, neither panned out; one new,
unexplored lead surfaced instead.** The `europe_finish_bridge.md`
neighborhood (`48d3` cluster: `0002`/`03d0`/`064e`/`06ba`) is
docks/landfall/ship-walk bookkeeping, not a dialog site — already the
same overlay a 2026-08-14 pass ruled out (`06ba` itself), and its
siblings are equally unrelated. The "`FUN_1000_8842(dialog, id, 1)` show
CHOICE popup" call shape this doc's own reference table cites
(`indian_incite_417e.md`) **doesn't actually appear anywhere in either
flattened export under that name** — grepped both `viceroy_unpacked.c`
and `viceroy_unpacked_2.c` for `_8842(`, zero hits. That citation has no
traceable evidence trail in this project (no address, no call-site quote)
— worth flagging as unreliable rather than trusted at face value; the
real CHOICE-dispatch function `2820`/`4528` use has never actually been
pinned to a specific address, just assumed same-shaped.
**New lead, not chased before**: the King's Galleon offer is a
Crown-initiated proposal, structurally the same *kind* of mechanic as
audience/congress/mercenary-hire/tax-teaparty — all of which live in the
**`38fd`** overlay (`king_ref.md`'s whole catalog: `5be8`/`3dc8`/`2564`/
`2022`/`2244`/`5e52`, etc.), not `48d3`. Every prior KINGGALLEON2 pass
searched `48d3`'s neighborhood or did a resident-wide Treasure-unit-type
scan — nobody has searched `38fd` specifically for this. Not searched
this pass either (`38fd`'s own `address_mapping.csv` rows are mostly
`gap`-quality — 60+ unnamed functions, too large a space to blind-search
without a narrower address hint first). Stays PARKED; real next step if
resumed is narrowing `38fd`'s own gap functions (e.g. by proximity to
already-known Crown-cash functions like the audience/teaparty pair) before
attempting a blind sweep.

**2026-08-24 — the `38fd` sweep this note called for turned out to have
already been run a few hours after this note was written (same day,
2026-08-22), just never written back here.** Found via
`src/core/founding_fathers.c`'s `founding_fathers_cortes_free_king_galleon`
comment: "Phase 3: 38fd CHOICE 3 STRING / 0 NUMBER negative... Phase 4:
Europe harbor tick / treasure-sell / europe_cash_treasure path searched —
no Crown-initiated Galleon offer dispatch... Phase 5: 38fd overlay sweep
for CHOICE(3 STRING, 0 NUMBER) + callee chain from harbor/treasure —
no match." Cross-confirmed by matching "Phase 5: 38fd overlay + string
search negative" status lines added the same day to `docs/roadmap.md`,
`docs/manual_gap.md`, and `docs/ai_transcription.md`. Independently
re-verified this session rather than trusted outright (this item's own
history of unverifiable citations, e.g. the retracted `FUN_1000_8842`,
warrants that): mapped all 81 `38fd` functions
(`viceroy_unpacked.c:58695-68762`) and every CHOICE-adjacent call site
(`FUN_291f_0182` CHOICE-read, `FUN_1d1d_07e4` string-template format,
`thunk_FUN_291f_0ae0` notify) to its owning function, including the
overlay's two largest, `WARNING:`-flagged-corrupted outliers
(`FUN_38fd_3694`, 2886 lines; `FUN_38fd_4f6e`, 2205 lines — the likeliest
place a text-only scan could miss something). Neither is Galleon-shaped:
`3694` matches its `FUNCTION_CATALOG.md` "dock immigrant info / embark
bark dialog" label on inspection (unit-type checks span ship types
`0xd-0x12` plus a few land types, never Treasure's `'\n'`/`0x0a`); `4f6e`
reads as warehouse/goods-overflow accounting, not the catalog's guessed
"Europe keyboard/hotkey dispatcher," but not Galleon either way. `38fd`
is genuinely exhausted, independently confirmed, not just asserted.

**Also found and corrected: this note's own "all in the `38fd` overlay per
`king_ref.md`" framing was half wrong.** Re-reading `king_ref.md` (whose
own title is "King / REF / independence (`43f7`)"): only the tax-audience
pair (`38fd_5be8`/`38fd_3dc8`/`38fd_5e52`) is actually `38fd` — the
declare-gate/congress citation (`2564`/`1a26`) and the mercenary-hire pair
(`2022`/`2244`) this note also cited as "38fd" are really **`43f7`**, a
different, much smaller overlay (21 functions vs `38fd`'s 81). Since
KINGGALLEON2 is structurally closer to those Crown-proposal mechanics than
to audience/tax-teaparty, `43f7` — not `38fd` — is what `king_ref.md`
actually motivates as a candidate. Checked it in full this pass: 18/21
functions were already attributed in `king_ref.md`; the remaining 3
(`FUN_43f7_0082`, `_0108`, `_0188`) are now read and ruled out —
`0082` is a difficulty-scaled weight/constant table (no dialog at all),
`0108` is per-nation elimination/reset bookkeeping, `0188` disposes an
eliminated nation's ships with a single-string status message (one
`STRING` arg, no CHOICE). No corruption warnings anywhere in `43f7`, so no
live-Ghidra re-check is owed here. **`43f7` is now also fully exhausted
(21/21), not just `38fd`.** No new candidate overlay surfaced this pass.
Full write-up: `docs/ai_port_plan.md` T1.13's 2026-08-24 entry. Stays
PARKED — a future attempt needs either a genuinely new overlay hypothesis
or a live DOSBox-X capture, not a repeat of either sweep above.

### 2c6. Linux thin — Missionary CONTACT (act)

Peace + Missionary/Jesuit, **not fleeing** (adjacent tribe Alarm/friction ≥55 —
same band as `ai_contact` flee): upsert `AI_GOAL_CONTACT` (prio 3 > Scout ring
prio 2) at nearest tribe with `mission == 0xff` and `AI_MOVE` toward it. Idle
Jesuit prefers convert CONTACT over Scout explore / FOUND yank. Adjacent
convert lives in `ai_contact`. Cite: Colonization.pdf Establishing a Mission;
indian_contact.md.

### 2c3. Linux thin — multi-step land goto (FOUND / MILITARY / CONTACT / hunt)

Toward `AI_GOAL_FOUND`, `AI_GOAL_MILITARY`, or `AI_GOAL_CONTACT`, or when
act-level land war hunt / peace-border wake / scout explore set the goto,
scored advances **drain `moves_left`** in the same act (thin `20e6` MP
full-drain; was hard-cap 2). Full combat multi-step scoring stays **PARKED**.

### 2c4. Linux thin — multi-step naval sail (AI_SAIL)

Ships on `AI_SAIL` use scored ocean steps (same `ai_euro_score_move` /
`ai_euro_ocean_score_step` as land) and **drain `moves_left`** — mirror land
MP-drain. Replaces full `units_advance_goto` so HS west-explore bias applies
per step. Full ocean combat `20e6` stays **PARKED**.

### 2d. Linux thin — Pioneer tools delivery
(mislabeled "case 7 economy stand-in" in earlier passes — case 7 is Found
Colony, not a hire economy; this section's own DOS citations are `5d04`/
`5cf6`, unrelated to `5b66` case 7 — see "Case dispatch targets resolved")

Idle / arriving Pioneer or Hardy on an **own** colony tile when
`tools_short > 0` or colony `stock[TOOLS] < 20`: add **+10** tools
(cap 100) once per act; trim inventory `tools_short` and may decrement
`urgency`. Wired in `ai_euro_unit_act` just before LABOR/COLONY join.

**Ship/colony shortage cargo (hire side-effect):** after Europe hire board, when
`tools_short>20` deliver TOOLS (+20 ship / +15 colony); else LUMBER / ORE
(+20/+15); else MUSKETS / HORSES (+10/+10); else FOOD (+20/+15) when matching
short >20. Cite: mid-5d04 tools-cargo stand-in deepen; 5cf6 tallies.

**Wagon deepen (hire-once):** when a Wagon Train already exists and sits on a
tools-short colony with hold `TOOLS`, unload via `colonies_transfer_from_unit`
(structural cargo only). Pioneer delivery prefers this path when a wagon is on
the same tile before the +10 stand-in.

**Wagon haul (idle):** Wagon with free hold capacity or TOOLS / LUMBER / ORE /
MUSKETS / HORSES / FOOD cargo → `AI_MOVE` toward nearest matching short own
colony (`TOOLS`/`LUMBER`/`ORE`<20, `MUSKETS`/`HORSES`<10, food `<pop*2`). On a
surplus colony (tools/lumber/ore≥40 / muskets≥20 / horses≥20 / food≥pop*4) with
empty capacity, load that cargo via `colonies_transfer_to_unit` before hauling
(load order tools>lumber>ore>muskets>horses>food). Cite: manual Wagon Train
cargo; `COLONIZE_CARGO_*`; §2d unload delivery; 5cf6 lumber/ore_short.

### 2d2. Linux thin — Caravel/Merchantman/Galleon coastal haul (act)

Peace + idle Caravel/Merchantman/**Galleon** with goods-hold capacity or TOOLS /
LUMBER / ORE / MUSKETS / HORSES / FOOD cargo → `AI_SAIL` toward coastal water by
nearest own coastal colony short on that cargo (`TOOLS`/`LUMBER`/`ORE`<20,
`MUSKETS`/`HORSES`<10, food `stock < pop*2`). Adjacent short + matching hold →
`colonies_transfer_from_unit`; surplus (≥40 / ≥40 / ≥40 / ≥20 / ≥20 / ≥pop*4)
near ship → load same ladder as wagon (FOOD first when `food_short>20`). Cite:
Colonization.pdf naval transport /
colony supply; euro_unit_act §2d haul pattern; docs/assets.md Europe purchase
ladder (Galleon). War hunt owns idle ships at war; Treasure Europe sail skips
haul.

**Europe export sail (unparked):** when supply haul does not bind the ship, load
FUN_364b_0636-eligible surplus (`stock>99` → leave 50; prefer Silver) at coastal
own colony, then `AI_SAIL` Europe for existing dump-sell. Cite: FUN_364b_0688 /
`europe_cargo_export_eligible`; Colonization.pdf Europe buy/sell; Custom House
denylist (not Food/Lumber/Horses/Tools/Muskets). No invented sell rates.

**Privateer loot sail:** peace Privateer already holding export-eligible goods
→ `AI_SAIL` Europe dump-sell (no colony load). Cite: Privateer commerce raid /
Europe sell; complements cargo-ship export.

**Wagon inland→coast export feeder:** when supply haul does not bind the wagon,
same FUN_364b load (prefer Silver) then `AI_MOVE` nearest own coastal colony;
on coastal tile unload export holds into colony stock for ship pickup. Cite:
§2d Wagon Train; §2d2 Europe export.

### 2d4. Linux thin — Jan de Witt foreign-colony TRADE_GOODS (act)

With FF Jan de Witt + peace: Wagon on foreign Euro colony tile loads
`TRADE_GOODS` surplus (stock≥20 → 10; same muskets haul chunk) via
`colonies_de_witt_transfer_from_colony`, then `AI_MOVE` toward nearest own
colony and `colonies_transfer_from_unit` unload into warehouse (delivery loop).
Empty wagon may `AI_MOVE` toward nearest foreign surplus. Cargo ships: same load
on foreign dock (ships may enter foreign Euro docks when de Witt + peace via
`units_can_enter` + `g_units_ff_col1`); with TRADE_GOODS aboard → `AI_SAIL`
Europe (existing dump-sell). Stock transfer only — no gold/price.
Cite: docs/fandom_col1994.md Jan de Witt; `colonies_de_witt_transfer_*`.

### 2d3. Linux thin — peace colony garrison fortify (act)

**`garrison_quota` (+0x1e):** fortify consumes quota (DEC); planning thin-latches
`=1` when idle unfortified garrison sits on colony (full `threat>>3` seed PARKED).
Cite: save_format_map.md; FUN_5952_035e.


Peace + idle Soldier / Dragoon / **Regular** / **Continental** (Army/Cavalry) on
own colony tile → `units_order_fortify` if not already fortified (overrides
explore/FOUND yank while on-tile; keeps off-colony MILITARY/CONTACT). Cite: case
0x0b fortify arm (`'F'`); Colonization.pdf Defending a Colony ("fortify
soldiers, dragoons, army, cavalry…"). At war: wake+hunt (§2c).

**Peace Artillery fortify:** idle Artillery/Cannon on own colony (peace or war)
→ `units_order_fortify` (same case 0x0b `'F'` arm; PDF "…or artillery"). At war
off-colony: siege hunt toward fortified foreign Euro colonies (Stockade+).

**Peace colony-defense wake (MD≤2):** fortified/idle garrison above **or
Artillery/Cannon** on own colony wakes via `units_wake` when a foreign Euro land
unit is within Manhattan ≤2, then `AI_MOVE` toward that threat (adjacent
`try_attack` may declare war). Extends peace fortify border; war already has
global fortify-wake (§2c). Cite: Colonization.pdf Defending a Colony ("fortify
soldiers, dragoons… or artillery"); `units_wake`.

**Artillery fortify after siege:** covered by peace/war Artillery fortify above.

 **5d04 peace hire (thin, not full case-7 body):** `tools_short>30` or
 `lumber_short>30` or `ore_short>30` or `muskets_short>20` or `horses_short>20` +
 Wagon
 Train/Supply Train/Wagon type → hire wagon **once** (TOOLS preferred else LUMBER
 else ORE else MUSKETS else HORSES
 loaded on wagon
 before board); else `tools_short>20` prefer Pioneer/Hardy + ship/colony tools
 cargo. Case-7 deepen: prefer Hardy/Expert Pioneer or Master Carpenter already
 on Europe dock (consume dock slot; no free expert spawn). **`tools_short>20`**
 dock miss → prefer Master Blacksmith on Europe dock (Ore→Tools). **`food_short>20`:**
 prefer Expert Farmer on Europe dock (same consume pattern); if Farmer miss and
 nation has a coastal own colony, prefer Expert Fisherman on dock (coastal food
 fallback). **Construction LABOR:**
 when any colony has Stockade/Warehouse/Lumber Mill/Drydock/Shipyard incomplete
 (`ai_euro_colony_wants_construction_labor`), prefer Master Carpenter on Europe
 dock (same consume / `hire_cost`; not tools/food short). **`lumber_short>20`:**
 when any colony wants lumberjack LABOR or has construction in progress with low
 lumber stock, prefer Expert Lumberjack on Europe dock (same consume pattern).
 **`ore_short>20`:** Ore stock&lt;20 tallies → prefer Expert Ore/Silver Miner on
 Europe dock (same consume). **`muskets_short>20`:** Muskets stock&lt;10 tallies →
 prefer Master Gunsmith on Europe dock (same consume). **Unmissioned tribe:**
 prefer Jesuit/Missionary on Europe dock (convert CONTACT; before Seasoned Scout).
 **Peace + colonies≥1:**
 prefer Seasoned Scout on Europe dock (CONTACT / fog explore) when no higher
 shortage hire wins; else Elder Statesman (Town Hall liberty bells).
 **Church/Cathedral present:** prefer Firebrand Preacher on Europe dock (crosses).
 **Schoolhouse/College/University present:** prefer Expert Teacher on Europe dock
 (education / Skills Chart job 18).
 **Craft building + raw≥20:** prefer Master Distiller/Weaver/Tobacconist/Fur Trader
 on Europe dock (Sugar/Cotton/Tobacco/Furs → Rum/Cloth/Cigars/Coats).
 Cite: europe.c expert pools; building_production /
 terrain_yields; euro_unit_act §2e field-assign / §2c2 / §2c6. Treasury: skip hire /
tools-cargo when gold &lt; colonist `hire_cost`; Artillery uses Europe purchase
**500$** (fall back to Soldier when underfunded). **At war + tools_short:** still
prefer Soldier/Dragoon hire over Pioneer (profession_demand Pioneer is peace-only).
**At war + own colonies ≥ 3:** prefer Dragoon hire when type exists (same
`hire_cost`; fall back to Soldier if Dragoon missing). **At war + own colonies
≥ 2:** prefer Veteran Soldier when type exists and gold covers cost (`@UNIT`
cost, else NAMES `@JOB` Soldier→Veteran Soldiers **2000$**). Missing type/cost
→ plain Soldier (**PARK** comment). Cite: `COLONIZE/NAMES.TXT` `@JOB`.
**Ship board military:** at war, idle Soldier / Dragoon / **Regular** /
**Continental** (Army/Cavalry) / Artillery/Cannon on coastal own colony boards
an empty transport (`cargo_count==0`) with passenger space via `units_board` /
`units_board_stacked` before hunt yank / Artillery on-colony fortify — **except**
when the colony is threatened (stay to defend). Cite: Colonization.pdf naval
transport / Defending a Colony ("fortify soldiers, dragoons, army, cavalry, or
artillery").
**Ship unload military:** at war, ship with military cargo adjacent to own
threatened coastal colony (war-peer MD≤3) unloads one passenger onto the colony
tile — prefer Soldier, else Regular/Continental Army, else Dragoon/Continental
Cavalry, else Artillery (mirror king MoW unload ladder + board list) via
`units_unload_passenger` (before move-scoring gate + after sail). Cite:
Colonization.pdf naval transport / Defending a Colony; king_ref MoW unload;
complements board + war-transport sail-to-threatened-port.
**Done:** transport at Europe dump-sells all commodity holds with Europe bid via
`europe_sell_unit_hold` / `europe_sell_proceeds` (tax); nat↔europe gold sync
(Merchantman/Caravel/Galleon **and Privateer** — `units_is_transport` holds).
Skips holds whose cargo type bit is set in `nation.boycott_bitmap` (wiki Boycott /
king refuse — goods blocked in Europe; no invented prices). Cite: Colonization.pdf
Europe buy/sell + tax; fandom Boycott (Col).
**Pioneer plow/road (unparked):** idle Hardy/Expert Pioneer with tools picks a
nearby own-colony surround → `AI_MOVE` then on-tile `units_pioneer_plow`
(clear forest then plow in one API) / `units_pioneer_road`. Prefer plow over
road; among roads prefer tiles **already plowed** (Clear/Plow/Road sequence).
Hardy real power: "Clears forest, plows fields, and builds roads faster"
(Colonization.pdf) — prefer Hardy when both idle; no invented yields. Skip when
`tools_short` or on-colony construction LABOR stay. Cite: Colonization.pdf
Clear/Plow/Road. Remaining mid `5d04` deep economy / deep combat scoring stay
**OPEN** (unpark #4). Colonies≥6 planning hard-return removed (ship-buy + war/peace shortage hire **Done**; Free Colonist settle gated). Thin Europe ship buy ladder **Done**: Caravel (no ship / full), Merchantman
(cargo pressure), Galleon (at war), Frigate (at war, 5000$) — `smoke_5d04_buy_*`.
Wagon hire-once covers tools/lumber/ore (>30), muskets/horses (>20), and food
(>30). Surplus load prefers FOOD when `food_short>20` (else tools ladder).

### 2d5. Linux thin — Col1 `labor_shortage` (+0x8e)

Runtime `ColonizeColony.labor_shortage` bridged from Col1. Planning D upserts
`AI_GOAL_LABOR` when `>0` (and thin-latches `=1` when other LABOR needs fire;
full `FUN_5952_035e` seed PARKED). `colonies_admit_unit` decrements on join
(decomp ~91589 / order `'G'`). Cite: save_format_map.md +0x8e.

### 2d6. Linux thin — Col1 `specialty_cargo` (+0x8d)

Runtime `ColonizeColony.specialty_cargo` bridged from Col1 (`0xff` = none).
Inventory refreshes via `colonies_specialty_cargo_update` (FUN_5952_0306 shape:
warehouse-cap / boycott clear). Wagon/ship surplus load tries specialty first.
Smoke: `smoke_specialty_cargo_haul_prefer`. Cite: save_format_map.md +0x8d.

### 2d7. Linux thin — Col1 `cargo_idle_turns` (+0x8f)

Runtime `ColonizeColony.cargo_idle_turns` bridged from Col1. Euro inventory INC
cap `0x7f` (FUN_5952_035e); `colonies_transfer_from_unit` clears on goods unload
(~90249). Haul short-colony pick maximizes `idle*8 - MD` (~87677). Smoke:
`smoke_cargo_idle_turns_haul_prefer`. Cite: save_format_map.md +0x8f.

### 2d8. Linux thin — Col1 `improve_timer` (+0x8c)

Runtime `ColonizeColony.improve_timer` bridged from Col1. Inventory INC cap
`0x7f`. AI pioneer plow/road skips colony surround until timer ≥ 2 (thin stand-in
for terr@0x2f78+2; full table PARKED). Successful plow/road clears timer
(~94546). Smoke: `smoke_improve_timer_pioneer_gate`. Cite: save_format_map.md
+0x8c; FUN_5952 ~93663.

### 2d9. Linux thin — Col1 `build_ai_flags` (+0x1d bit7)

Runtime `ColonizeColony.build_ai_flags` bridged from Col1. Bit7
`COLONIZE_BUILD_AI_WANTS_CONSTRUCTION` latches construction LABOR (even without
`building_in_production`). Planning sets bit when named construction is live;
`colonies_clear_construction` / complete clears it (~95710). Smoke:
`smoke_build_ai_flags_wants_construction`. Cite: save_format_map.md +0x1d;
FUN_5952 ~94660 / ~95792.

### 2d10. Linux thin — Col1 `cargo_produced_mask` (+0x90)

Runtime `ColonizeColony.cargo_produced_mask` bridged from Col1. Cleared at
colony production start; OR bit per cargo with positive yield/craft
(`FUN_364b_0688`). Wagon/ship surplus load prefers produced cargos after
specialty. Smoke: `smoke_cargo_produced_mask_haul_prefer`. Cite:
save_format_map.md +0x90.

### 2d11. Linux thin — Col1 `ai_flags` (+0x1b)

Runtime `ColonizeColony.ai_flags` bridged from Col1. Planning refreshes ship
bits via MD≤5 foreign armed sea scan (MoW → 0x02, else attack>0 → 0x01;
`FUN_4962_0018`). Idle COLONY primary uses code/prio **8** when MoW bit set,
else **5** (euro_dispatcher). Thin latches needs_colonists / needs_garrison.
Smoke: `smoke_colony_ai_flags_mow_colony_alt`. Cite: save_format_map.md +0x1b.

### 2d12. Linux thin — Col1 `colony_flags` (+0x1c)

Runtime `ColonizeColony.colony_flags` bridged from Col1. Starvation (0x08)
latches when food < pop×2 (production + planning); forces LABOR. Thin wagon
(0x20) / coastal (0x40) / small-AI (0x10) latches. Smoke:
`smoke_colony_flags_starvation_labor`. Cite: save_format_map.md +0x1c;
FUN_364b_0688.

### 2d13. Linux thin — Col1 `hammers_purchased` (+0x98)

Runtime `ColonizeColony.hammers_purchased` bridged from Col1.
`colonies_buy_construction` adds BUY remainder (gold cost) to the counter
(`FUN_2f2b_5e44`) and sets `build_complete` (+0x1c bit7). Smoke:
`smoke_hammers_purchased_buy` / colony-screen BUY. Cite: save_format_map.md
+0x98.

`FUN_2f2b_5e44` disassembly verified clean (2026-08-13) — carried a Ghidra
`Removing unreachable block` warning in the canonical export
(`docs/decomp_inventory.md`); re-disassembled via `tools/address_mapping.csv`
→ `OVL03_L0000:5e44`: clean, self-contained, 386 bytes / 83 decompiled
lines, one unrelated minor warning left. Calls `thunk_FUN_1000_997c` —
target not resolved this pass. Confirms this mapping, not a correction.

### 2d14. Linux thin — Col1 SoL latches on `colony_flags` (+0x1c)

`colony_prod_refresh_sol_flags` sets sol_50 (0x04) / sol_100 (0x02) from
`colony_prod_sol_percent` (≥50 / ≥100); clears on drop. Called from colony
production and Euro planning. Cite: FUN_364b_0688 ~55373; unit_colonies SoL
flag checks.

### 2d15. Linux thin — Col1 `depletion_counter` (+0x97)

Runtime `ColonizeColony.depletion_counter` bridged from Col1. Each ore/silver
field yield INC; wrap at 50 subtracts 50 and sets `MAP_LAYER2_SUPPRESS` on the
worked tile (`FUN_364b_033a` feature 4). Smoke: turn `depletion_counter
wrap+suppress`. Cite: save_format_map.md +0x97.

### 2d16. Linux thin — Col1 `warehouse_level` / `capitol_level` (+0x95/+0x96)

Runtime fields bridged from Col1. Warehouse capacity uses `100*(1+level)`
(`FUN_15eb_0a50`); level also derived from Warehouse / Expansion buildings and
INC on complete. Capitol level INC on Capitol / Capitol Expansion complete
(`FUN_364b_0114`). Smoke: `smoke_warehouse_capitol_levels`. Cite:
save_format_map.md +0x95/+0x96.

### 2e. Linux thin — LABOR bind (food/tools short + construction)

Idle colonist-capable land unit (Pioneer/Hardy/Free Colonist/Colonist) within
MD≤1 of an own colony when inventory `tools_short` or `food_short` and the
colony is locally short → upsert `AI_GOAL_LABOR` and goto (overrides distant
FOUND). On-tile Pioneer/Hardy skip LABOR-join so tools-delivery stand-in is not
stacked with founder-loot dump — **except** when `building_in_production` is
**Stockade**, **Warehouse**, or **Lumber Mill** (carpenter hammers bind;
stay/LABOR rather than leave). Cite: `docs/building_production.md`. Colony
planning also upserts LABOR for those projects. No invented production numbers.

**Threatened Stockade LABOR:** when at war and a war-peer unit is within MD≤3
of an own colony with incomplete **Stockade**, idle Free Colonist within MD≤3
prefers that Stockade LABOR (prio bump) over distant FOUND. Cite:
`building_production.md` Stockade defense; Colonization.pdf fortify;
`ai_euro_colony_threatened_by_war`.

**Food emergency:** when inventory `food_short` ≥ 4, nearest food-capable
colonist/Pioneer within MD≤8 is bound to a hungry colony LABOR (planning + act).
Cite: manual 2 food/colonist; `5cf6` shortage tallies.

**Expert Farmer food LABOR:** idle Expert Farmer (display-name Farmer, or Free
Colonist/Colonist with `@JOB` Farmer profession 0) → food-short LABOR (MD≤8
when food_short). Cite: `docs/building_production.md` Farmer→Food; Skills Chart.
No invented food rates — LABOR join only.

**Free Colonist food LABOR (non-Expert Farmer):** idle Free Colonist / Colonist
(without Farmer profession) with `food_short` > 0 → MD≤8 toward a hungry own
colony LABOR join (same structural join as Expert Farmer path). Adjacent still
covers tools/construction; MD>1 is food-short only. Cite: manual 2 food/colonist;
5cf6 food_short; euro_unit_act §2e.

**Master Carpenter construction LABOR:** idle Master Carpenter → LABOR when
own colony has Stockade/Warehouse/Lumber Mill incomplete (`building_in_production` —
same Stockade pattern as Pioneer stay). Cite: `docs/building_production.md`
Carpenter→Hammers; Skills Chart Master Carpenter. Construction-only bind
(not tools/food). No invented hammer rates.

**Expert Lumberjack LABOR:** idle Expert Lumberjack → LABOR when own colony has
incomplete **Warehouse** or **Lumber Mill** and that building type exists in
the pool (lumber feeds carpenter hammers). Cite: `docs/building_production.md`
Lumberjack→Lumber; Colonization.pdf Skills Chart. Structural LABOR join only.

**Tools-short Pioneer deepen (peace):** when inventory `tools_short` > 0, idle
peace Pioneer/Hardy within MD≤8 is LABOR-bound toward a tools-short colony
(feeds on-tile §2d tools delivery). Cite: euro_unit_act §2d; 5cf6 tools tallies.

**PARK:** Custom House per-cargo UI chrome (`FUN_15eb_0326`). Drydock /
Shipyard prefer already wired via `colonies_list_buildable` +
`colonies_set_construction`.

**Stuyvesant Custom House construction prefer:** when nation owns Peter
Stuyvesant (`founding_fathers_nation_has` / `has_peter_stuyvesant`), idle
colony without Custom House queues it after Drydock→Shipyard prefer.
Cite: docs/fandom_col1994.md Stuyvesant; colony.c Custom House gate;
founding_fathers elect comment.

**Peace Church construction prefer:** idle colony with Stockade already owned,
no Church/Cathedral → queue Church when buildable (after defense/storage/naval/
Custom House prefers). Cite: building_production.md Church→Crosses;
Colonization.pdf Church / immigration.

**Wartime Armory construction prefer:** at war with a Euro peer, idle colony
with Stockade, no Armory/Magazine/Arsenal → queue Armory when buildable (after
Church prefer so wartime muskets beat crosses). Cite: building_production.md
Armory Tools→Muskets; Colonization.pdf Defending a Colony.

**Wartime Magazine construction prefer:** at war, Armory owned, no Magazine/
Arsenal → queue Magazine when buildable. Cite: building_production.md Magazine
doubles musket output.

**Peace Printing Press construction prefer:** idle colony with Stockade+Church
owned, no Printing Press/Newspaper → queue Printing Press when buildable.
Cite: building_production.md Printing Press +50% liberty bells.

**Peace Schoolhouse construction prefer:** idle colony with Stockade, pop≥4, no
Schoolhouse/College/University → queue Schoolhouse when buildable (after Press).
Cite: building_production.md Schoolhouse teach faculty 1.

**Peace Newspaper construction prefer:** Printing Press owned → Newspaper when
buildable. Cite: building_production.md Newspaper +100% liberty bells.

**Peace College construction prefer:** Schoolhouse owned, pop≥8 → College when
buildable. Cite: building_production.md College faculty 2.

**Peace University construction prefer:** College owned, pop≥10 → University
when buildable. Cite: building_production.md University faculty 3.

**Peace Cathedral construction prefer:** Church owned, pop≥8 → Cathedral when
buildable. Cite: building_production.md Cathedral crosses.

**Wartime Arsenal construction prefer:** at war, Adam Smith elected, Magazine
owned, no Arsenal → queue Arsenal when buildable. Cite: building_production.md
Arsenal factory muskets (Adam Smith); Colonization.pdf.

**Stable construction prefer:** fortified (Stockade/Fort/Fortress), no Stable →
queue Stable when buildable (peace or war). Cite: building_production.md Stable
horse breeding.

**Carpenter's Shop / Lumber Mill construction prefer:** idle → Shop when unmet;
Shop owned → Lumber Mill. Cite: building_production.md lumber chain.

**Blacksmith's House / Shop / Iron Works construction prefer:** ore≥20 →
House; House owned → Shop; Adam Smith + Shop → Iron Works. Cite:
building_production.md Ore→Tools / factory tools (Adam Smith).

**Craft shop/factory construction prefer:** Distiller/Weaver/Tobacconist/Fur
House→Shop→Factory when raw stock≥20; factories need Adam Smith. Cite:
building_production.md craft chains; dock craft hire stock≥20 gate.

**Capitol / Capitol Expansion construction prefer:** fortified → Capitol;
Capitol owned → Expansion. Cite: building_production.md Capitol liberty bells.

**Custom House auto-sell:** `europe_custom_house_autosell` from
`turn_produce` / `turn_run_colony_production` — `FUN_364b_0688` stock>99
leave 50; `FUN_364b_0636` denylist (not Food/Lumber/Horses/Tools/Muskets);
boycott bypass; WoI (`unknown46[0]`) untaxed.

**Expert Lumberjack forest field-assign (unparked):** idle Expert Lumberjack →
admit + `colonies_assign_field` on a free forest surround (pedia 8–23) with
`COLONIZE_JOB_LUMBERJACK`. Off-tile MD≤8 → LABOR goto. Warehouse/Lumber Mill
LABOR join remains the no-forest fallback. Cite: terrain_yields /
building_production Lumberjack→Lumber; Colonization.pdf Skills Chart. No
invented lumber rates.

**Expert Ore Miner / Silver Miner field-assign (unparked):** idle Expert Ore
Miner / Silver Miner → admit + `colonies_assign_field` on a free surround with
positive Ore/Silver yield (`COLONIZE_JOB_ORE_MINER` / `_SILVER_MINER`). Off-tile
MD≤8 → LABOR goto. Cite: terrain_yields Ore/Silver; Colonization.pdf Skills
Chart. Parallel to Lumberjack forest field-assign. No invented rates.

**Expert Farmer food field-assign (unparked):** idle Expert Farmer (display-name
Farmer, or Free Colonist/Colonist with `@JOB` Farmer profession 0) → admit +
`colonies_assign_field` on a free surround with positive Farmer food yield
(prefer higher `colony_yield_for_tile`). Off-tile MD≤8 → LABOR goto. Food-short
LABOR join remains the no-field fallback. Cite: terrain_yields / building_production
Farmer→Food; Colonization.pdf Skills Chart. Parallel to Lumberjack/Ore Miner.
No invented food rates.

**Expert Fisherman coastal field-assign (unparked):** idle Expert Fisherman
(display-name Fisherman, or Free Colonist/Colonist with `@JOB` Fisherman
profession 8) → admit + `colonies_assign_field` on a free ocean/sea-lane surround
(pedia 25–26) with positive Fisherman yield. Off-tile MD≤8 → LABOR goto. Cite:
terrain_yields Fisherman (Ocean/Sea Lane fish); building_production; Skills Chart.
Parallel to Farmer field-assign. No invented fish rates.

**Expert Sugar / Tobacco / Cotton Planter + Fur Trapper field-assign
(unparked):** idle Expert Sugar/Tobacco/Cotton Planter or Fur Trapper → admit +
`colonies_assign_field` on a free surround with positive matching yield
(`COLONIZE_JOB_SUGAR_PLANTER` / `_TOBACCO_PLANTER` / `_COTTON_PLANTER` /
`_FUR_TRAPPER`; prefer higher `colony_yield_for_tile`). Off-tile MD≤8 → LABOR
goto. Cite: terrain_yields Sugar/Tobacco/Cotton/Fur; Colonization.pdf Skills
Chart. Parallel to Farmer field-assign. No invented crop rates.

**Elder Statesman / Firebrand Preacher / Expert Teacher / Master Carpenter
workplace assign (unparked):** idle Elder Statesman → Town Hall; Firebrand
Preacher → Church else Cathedral; Expert Teacher → Schoolhouse else College else
University; Master Carpenter → Carpenter's Shop else Lumber Mill (highest owned;
construction LABOR join remains fallback without Shop/Mill). Off-tile MD≤8 →
LABOR goto. Cite: building_production.md Skills Chart jobs 13, 16–18;
Colonization.pdf. Parallel to craft workplace assign. No invented rates.

**FOUND on Indian homeland:** `colonies_found_with_indian_land` (FUN_4cc6_07c2
gold charge; Minuit FF 2 → free). Short gold → PARK (no despawn); thin human
`ctx->status` when cost>0 and gold short. Cite: Colonization.pdf Minuit /
indian land purchase; `colonies_indian_land_purchase_gold`.

**Pioneer plow/road** — see §2d (unparked).

### 2f. Linux thin — naval adjacent-foe pick

Like land adjacent-foe: when choosing naval `try_attack` target —
**Privateer** prefers Merchantman/Caravel cargo over warships; **Frigate**
prefers warships over cargo (complement); else lower type defense
(`ai_euro_naval_best_adjacent_foe`). **Done (thin FUN_157e_004a):** vet Soldier/
Dragoon profession `0x15` +50% land toughness; Drake Privateer +50% naval
toughness; Privateer + `ship_damaged` (0x3148 bit7) → −2; holds_occupied
(0x3150 / goods holds) subtracted — also in `units_resolve_naval_combat_ff`.

**PARK:** (retired) Wagon load FOOD hire — now Done for hire-once when
`food_short>30`. Surplus FOOD prefer when `food_short>20` **Done**.

**Seasoned + sticky fog deepen:** Seasoned Scout fog-explore with
`ai_diplo_indian_hostility_sticky` ≥ 2 and `map.seen` deepens a shallow prior
goto once at fresh MP (`pick_md > goto_md`) — mirror CONTACT sticky deepen
without max-md walk drift on dispatcher sticky waves. Cite: Colonization.pdf
Seasoned Scout; euro_unit_act §2c2.

**Done (thin Artillery siege / Dragoon open):** off-colony Artillery at war hunts
fortified foreign Euro colonies (Stockade+; MD slack ≤3 vs open); adjacent-foe
prefers higher fort %. Dragoon hunt prefers open colonies (leave forts to
Artillery). On own colony Artillery still FORTIFY. Cite: king_ref Artillery
siege / Dragoon open bias; Colonization.pdf Artillery.

**Done (thin Treasure adjacent prefer):** at equal land toughness, prefer Treasure
over other units (loot — Colonization.pdf Treasure Trains / @LOOTCASH). Land war
hunt also prefers Treasure within MD slack ≤3 vs nearer non-Treasure, then lower
`land_foe_toughness` within the same slack.

**PARK:** deep `FUN_521d_20e6` combat scoring (terrain/artillery tables,
multi-hex threat weights) — **section-mapped** in
[`move_scoring_land.md`](move_scoring_land.md) /
[`move_scoring_ship.md`](move_scoring_ship.md); port still OPEN unpark #4.
Thin adjacent-toughness pick includes fortified ×2,
colony Stockade/Fort/Fortress %, and FUN_157e_004a vet/Drake +50% peels +
2-step goto only.

**Done:** Treasure → Europe gold via `europe_cash_treasure` (LE16 hold value;
despawn; Expected→Harbor tick). **PARK:** value unset / KINGGALLEON2 extra share.

### 2g. Linux thin — ocean west-explore / east-Europe HS bias

When ship is on high seas and goto is westward, ocean `20e6` score prefers
westward HS steps and **leaving HS into ocean** (Atlantic first-leg). When goto
is eastward (Treasure/Europe exit / eastern HS), prefer eastward HS steps.
Europe-exit place uses `units_spiral_place_hs_near` (`FUN_48d3_048e` / `0434`).
Cite: Colonization.pdf Treasure Trains → Europe; `move_scoring.md` band table;
[`euro_ocean_scoring.c`](euro_ocean_scoring.c). Full `LAB_521d_3558` cargo/colony
sail matrix still **OPEN**; Atlantic approach / post-beachhead tips are
latitude-band geometry (TURN1→7 without XY peels).

### 3. Combat / diplomacy tails (**OPEN** mid-planner; Indian raid deep PARKED)

Land combat act tails deepen with unpark #4; Indian raid deep bodies stay PARKED.

## Naval type band note

Decomp often tests `type ∈ (0x0c, 0x13)` (open upper). Annotated
`SHIP_A..C = 0x0a..0x0c` is the dispatcher ship-wave set — **do not conflate**
with the wider naval cargo band inside `20e6` / `0a60`.

## Related symbols

| Symbol | Role |
|--------|------|
| `FUN_521d_20e6` | Direction / move scoring (`04f4` @90557) |
| `FUN_521d_06ae` | Best adjacent founding tile (from `20e6` @89587 only) |
| `FUN_521d_016a` | Upsert primary goal |
| `FUN_1427_*` / `281f_09xx` | MP chrome after steps |

## Exit criteria for a future deep extract

- Sectioned `.c` with provenance headers
- Ship unload + founding-order arms readable end-to-end
- Explicit **OPEN** remainder for land combat / case `0xb` move drivers
  (case 7 is Found Colony, already faithfully ported — see "Case dispatch
  targets resolved," not an open hire-economy item)
- Ocean naval `20e6` + full line-by-line still R5 / PARKED
- `SYMBOL_MAP` + catalog `links` updated
