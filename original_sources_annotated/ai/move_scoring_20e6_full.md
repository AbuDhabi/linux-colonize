# `FUN_521d_20e6` — full clean recovery (2026-08-14)

Complete, independently re-verified raw body of the Euro move-scoring
function (every AI unit direction/act decision routes through this) —
section-mapped already in [`move_scoring.md`](move_scoring.md) /
[`move_scoring_land.md`](move_scoring_land.md) /
[`move_scoring_ship.md`](move_scoring_ship.md); this file is the full
ground truth behind those maps, not a replacement for them.

Re-disassembled fresh via the overlay project (`OVL14_L0000:20e6`):
**2215 lines, zero warnings** — closely matches the 2219-line figure from
the 2026-08-13 phase-18 recovery (`docs/seed100_brave.md` "Root cause
candidate"), confirming that earlier fix is still good. Renamed
`FUN_OVL14_L0000__0020e6` → `FUN_521d_20e6` for readability; no other edits.

## One real finding: the canonical export's tail has drifted since 2026-08-13

Reading `viceroy_unpacked.c`'s current `FUN_521d_20e6` body (no `WARNING:`
at its declaration, so it looked trustworthy) turned up a confusing call
near its commit-phase logic: `thunk_FUN_2a1f_04f4()` — a thunk that
nominally targets `FUN_521d_20e6` **itself**, called with a stack-argument
setup that doesn't match a simple 1-param re-entry. **This fresh, fully
clean recovery has no such call anywhere** (`grep -c 2a1f_04f4` → 0). The
2026-08-13 fix evidently didn't get patched back into the canonical file's
full 2000+ line body byte-for-byte (or only the early explore-ring section
was cross-checked then) — the canonical file's later ~1/3 (past the explore
ring, i.e. past `move_scoring_land.md`'s own citation range) should be
treated as unverified until re-checked against this file, same "a function
without a warning at its own declaration can still have real desync deeper
inside" lesson `decomp_inventory.md` already documents for other functions.

**2026-08-14, verified this does NOT implicate the Linux port**: checked
whether `ai_native_pick_dir_asm` (`src/core/ai.c`, the actual port) has
any recursive/self-referencing call pattern matching the canonical
export's stray `thunk_FUN_2a1f_04f4` self-call — it doesn't (`grep` finds
no `2a1f_04f4` reference in `ai.c` at all). `move_scoring.md`'s own
citation of `2a1f_04f4` is the *normal, expected* outer entry thunk
(`5b66 → 2a1f_04f4 → 20e6`, the caller-side path INTO this function),
not anything inside `20e6`'s own body — matching this file's fresh, clean
recovery, not the canonical export's stray anomaly. **Conclusion: the
drift is confined to `viceroy_unpacked.c` (the tracking/ground-truth
file, not used directly for porting) — the shipped port was never built
from the corrupted spot and needs no changes.** Safe to close as
"canonical file is stale, port is fine" rather than an open question.

## SCOUT/PATROL (`0x314b == 0x56`) mystery resolved

`move_scoring_land.md`'s earlier note ("orders-byte consumer not found
anywhere, ties to the unfound-real-`5b66`-body mystery") is now closed:
this clean body **reads `unit+0x314b` at its own entry** (checks for `'t'`/
`'i'`/`'A'`/`'2'`/`'8'`/`'G'` — several other previously-written orders
values, not just `0x56`) as well as writing it throughout (`0x40`/`0x47`/
`0x56`/`0x4c`/`0x3d`/`0x55`/`0x52`/`0x42`/`0x65`/`0x46`/`0x39`/`0x43`/`0x30`
all appear). **`0x314b` is `20e6`'s own persistent per-unit decision cache,
read back on its next call — there is no separate downstream consumer to
find.** No misresolved call, no hidden body; the mystery was never a real
gap, just an unexplored self-reference.

## Deep-port pass (2026-08-15)

**0x46 attack gate re-confirmed already ported** (line ~2024-2044: combat-capable
+ not-ship + `in_stack_0000ff16` + 8-dir tile-nation scan via `FUN_1000_8886`) —
matches `ai_euro_land_try_adjacent_colony_seize`/`ai_euro_land_try_adjacent_attack`
in `ai_euro.c`. No new work needed there.

**Update (2026-08-15, later pass) — "corrupted" diagnosis retracted; real
root cause found, still not fully resolved.** User asked to "uncorrupt and
continue." Re-chased `FUN_1000_8aac` from scratch with the raw disassembly
(not the decompiler) as ground truth:

- The apparent byte mismatch that looked like project corruption was my own
  arithmetic error (used the wrong resident→file-offset delta twice in a
  row) — confirmed by hashing all 4 on-disk `VICEROY.EXE` copies (identical,
  `md5 0f5d5b00...`) and locating the real thunk bytes at file offset
  `0x1aeac`, not `0xaeac`. The Ghidra project's disassembly listing and its
  raw stored bytes agree perfectly once the arithmetic is right — **no
  disassembly/memory desync, no project corruption.**
- `FUN_1000_8aac`'s `JMPF 0x0000:4fa8` is *not* an unpatched RTLink
  placeholder at all (`rtlink_decode`'s own jump-table parser classifies it
  `segmentIndex=-1`, `segmentOffset=0x427` — its source comment explains
  this class: "method stubs for methods which reside in the static part of
  the executable and shouldn't need method stubs to begin with", i.e. an
  already-concrete, non-overlay far pointer, same real flat target `0x4fa8`
  either way).
- `FUN_0000_4fa8` genuinely starts clean at `0x4fa8` (verified with plain
  linear `ndisasm`, zero overlap, real `ENTER`/`LEAVE`/`RETF` framing) — a
  15-case dispatcher (`param_2` 0..0xe) selecting via `jmp [cs:bx+0xd78]`.
  **The "overlaps instruction"/BCD-arithmetic/`INT 21h` garbage was never a
  disassembly-fault in this function's own bytes — it's Ghidra's
  decompiler mis-chasing the indirect jump table**, the same
  misresolution-bug family as `684c_08c0`/`15eb_1d4c`
  (`decomp_inventory.md`), just manifesting on a *local* jump table instead
  of a far-call thunk chain. Confirmed by force-clearing and rebuilding the
  function from scratch (`tools/GhidraForceRedecomp.java`, new): Ghidra's
  own `createFunction`+decompile reachability walk grows the body to
  20KB+ and pulls in unrelated code, matching the earlier bad decompile
  exactly — the bug is reproducible and not an artifact of a stale cache.
- Read the real jump table directly out of memory (`tools/GhidraDumpBytes.java`,
  new) instead of trusting Ghidra's chase: 15 words at flat `0xd78`.
  Verified table[2] = `0x46c7` by hand-disassembling it: a clean, self-
  contained 0x50-byte routine that splices `unit+0x315e`/`unit+0x315c`
  (`next_unit_idx`/`prev_unit_idx` — literally `ColonizeCol1TransportChain`,
  already in `col1_save.h`) — **this is a transport-chain link-insert
  operation, not a cargo/count query.** Retracts `move_scoring_land.md`'s
  "cargo_query<2" nickname for this call shape — field 2 doesn't return a
  countable quantity at all (both branches return a unit id), so the `<2`
  comparison at the 20e6/0a60 call sites reads a *unit id or chain-splice
  result* against 2, not a cargo-slot count. What that comparison actually
  means is now a fresh open question, not resolved this pass — did not
  chase the other 14 cases or re-derive the `<2` semantics; flagging so a
  future pass doesn't reuse the retracted "cargo" framing.

**Update (2026-08-15, third pass) — all 15 cases disassembled; the
"per-unit field selector" framing was wrong.** User asked to name every
case. Dumped and hand-disassembled all 15 jump-table targets
(`0x2ce0`/`0xd47f`/`0x46c7`/`0xf2`/`0xa100`/`0x2ce0`/`0x6ef7`/`0x2bf2`/
`0xf446`/`0xd8f7`/`0xc00b`/`0x27d`/`0xc02b`/`0xc88b`/`0x1b8` for cases
0-0xe). **Only 2 of the 15 touch anything unit/colony-record-shaped**
(`*0x1c` stride, `0x31xx` unit-field offsets, or the known colony pointer
`0x8542`) — the other 13 are generic, low-level runtime-support routines
(setjmp/exception-frame install+reset sharing a global at `0x2d54`,
`atol`-shaped string→int parsing, DOS `INT 21h AX=0x4400` IOCTL + ASCII-
uppercase matching, far-heap alloc refcounting, a DS-segment-restore
trampoline back to the confirmed `0x1b5a` MS-Run-Time static segment, a
generic min/max-clamp loop, a nested sub-dispatch assigning small tier
codes by threshold). **Conclusion: `FUN_0000_4fa8` isn't a purpose-built
"AI unit-field query" dispatcher at all** — it's a shared, size-optimized
low-level utility multiplexer that many unrelated parts of the whole
compiled game (not just AI) reuse for miscellaneous small operations, each
case with its own ad hoc register/stack convention. There's no single
coherent "field enum" to name; each case's meaning is caller-dependent and
would need that specific caller's own setup code traced, not just the
table.

**What's actually confirmed, concretely:**
- **Case 2** (`0x46c7`, the one `20e6`'s `0x42`/`0x65` gate and several
  `0a60` sites use): real, game-specific — splices `unit+0x315e`/`0x315c`
  (`next_unit_idx`/`prev_unit_idx`, `ColonizeCol1TransportChain`), stride
  `*0x1c` confirmed. Two branches (already-linked fast path vs. general
  splice-insert via a small shared pointer-pair setter called twice).
  Returns a unit id in both branches (not a count) — so the `<2` comparison
  at the `20e6`/`0a60` call sites tests the *result of a chain-splice*
  against 2, not a cargo-slot count; what that specifically means (unit id
  0/1 being sentinel-shaped, most likely) is still open.
- **Case 6** (`0x6ef7`): real, game-specific — sets/clears a bit
  (`OR`/`AND` a byte) in a bitmap at `colony_ptr(0x8542)+0x84+(index>>3)`,
  bit selected by `index&7`; `index`/on-off aren't sourced from the same
  `[bp+6]`/`[bp+8]` slots case 2 uses (this case's calling convention is
  its own, register-based) — which caller register feeds `index` wasn't
  traced this pass.
- Cases 0/1/3/4/5/7/8/9/0xa/0xb/0xc/0xd/0xe: generic CRT/RTL-shaped, no
  unit or colony record touched — see the raw dump in this pass's
  `cases_listing.txt` if resumed (not checked into the repo; regenerate
  with the command below).

**Update (2026-08-15, fourth pass) — case 2's own helper traced; return
semantics resolved, one real hypothesis left open.** Checked `0a60`'s
other field-2 call site (`euro_goal_orders_0a60_full.md` line 599,
`iStack_10 = FUN_1000_8aac(0x181f,iStack_154,2)`) for a second data point —
its result feeds `iStack_10 + colony_ptr[0x1f]` compared against a
threshold, ruling out "returns a raw unit id" as too coincidental to be
useful there. Traced case 2's general-path helper (raw `CALLF 0x024c:0x2a`
→ flat `0x24ea`, clean): it's a generic `swap(word* a, word* b)` — swaps
the two words pointed to and returns the new `*a`. Re-reading case 2 with
that: it's a **doubly-linked-list node exchange**, not a plain insert —
it swaps *both* `next` (`+0x315e`) and `prev` (`+0x315c`) links between
`unit_A=[bp+6]` and `unit_B=[bp+8]` (calling the swap helper twice, once
per field), and returns the second swap's result = `unit_A`'s *old*
`next` value, now stored as `unit_B`'s new `next`. The fast path (`A`'s
`next` already equals `B`) is a no-op-shaped equivalent returning `B`
literally, for the same reason.

**For the AI's calls specifically** (`FUN_1000_8aac(unit_id, 2)`,
*only two args* — `[bp+8]` here is the literal constant `2`, not a second
unit): `unit_B` in this call is **unit-table slot 2 itself**, not a
variable. Reads as "splice this unit into slot 2's chain position,
return the resulting chain-next value" — consistent with slot 2 being a
reserved sentinel/head entry for some shared list (matches this game's
common pattern of low fixed unit-table indices as list anchors; not
independently confirmed). Both branches return a **chain-link value**
(a unit id, `-1` sentinel, or slot 2 itself), never a count — so `< 2` at
the call sites most plausibly reads as "the resulting chain link is empty/
terminal (0, 1, or none)", i.e. a **singleton/idle-list check** ("is this
unit not already part of an active chain?") gating the `0x42`/`0x65`
impulse — plausible, not confirmed; would need slot 2's own conventional
role traced to close out.

**Update (2026-08-15, fifth pass) — chased slot 2's identity, hit a real
wall, not a self-inflicted one this time.** Traced `0a60`'s
`FUN_1000_89d0(0x181f)` call (the source of its `iStack_154` first arg to
the field-2 call) to its real target (flat `0x42cc`, clean): it's a
coordinate-based unit search (`find unit at (x,y)`, `AX`/`DX` in = search
coords, scans `unit+0x3144`/`0x3145` against the whole unit table). But
that call site passes **no explicit x/y** — `AX`/`DX` are whatever the
surrounding code left in those registers, meaning `iStack_154` is a real,
variable, found-by-search unit id, not a "slot 2" reference — this
actually *rules out* the "unit-table slot 2 is a reserved sentinel"
framing for `0a60`'s call (only `20e6`'s literal-2-as-second-arg shape
suggested it). Checked Linux (`grep` `units.c`/`units.h`/`col1_bridge.c`)
for any already-known reserved/sentinel low unit index — nothing; this
port never inherited the DOS chain mechanism, so there's no shortcut via
existing Linux knowledge either. Confirming what registers hold at that
exact `0a60` call point needs reading a long stretch of that function's
own preceding code (which coordinates are live there), the same
diminishing-returns shape as `417e`'s caller hunt — **parking here, not a
corruption or tooling wall, a genuine "needs more context than is cheaply
available" stop.** The solid finding (case 2 = doubly-linked transport-
chain node exchange, helper `swap(word*,word*)` at flat `0x24ea`) stands
and is fully documented above regardless.

**Update (2026-08-15, sixth pass) — closed out, no code to ship.** User
asked to wire `0x42`/`0x65` anyway with the singleton-check best-guess,
flagged as approximate. Before writing it, checked whether the *outcome*
is already produced elsewhere in Linux — it is. `ai_euro_unit_act`'s
"H: light bind" block already binds idle, founding-capable land units
(Pioneer/Free Colonist/etc, not Soldiers, not aboard a ship, not already
on a goto) toward the best founding tile; `ai_euro_scout_contact_ring_target`
does the equivalent for Scout/contact. Both already tested and shipping.
DOS's field-2 "singleton chain" check is the same *kind* of safety gate as
Linux's already-present `aboard_ship_id >= 0` / `units_orders_follow_goto`
checks in that same block — just checking against DOS fields
(`unit+0x315e`/`0x315c`) this port never actively maintains for land units
(round-tripped through save/load only, never written by live logic).
Wiring a check against always-empty state would be dead code, not a real
approximation. **Not shipped** — the behavioral gap this whole
investigation chased turns out to already be closed, just via different,
already-shipped mechanics than DOS used. No source changes.

**Practical upshot for `0x42`/`0x65`:** still not safe to port — case 2's
own return-value semantics (what "id < 2" means) remain the real open
question, and it's now clear that's a self-contained puzzle about *this
one case*, not blocked on naming 14 unrelated CRT routines first (a false
scope this pass corrected). `0a60`'s other field-2 call sites (lines
599/796 in `euro_goal_orders_0a60_full.md`) are the next thing to check —
same case, different caller context, might disambiguate the `<2` meaning
faster than tracing case 2 in isolation further.

**Update (2026-08-21) — closed for practical purposes, per `ai_port_plan.md`
T1.2.** The "0a60's other field-2 call sites" lead above was already a dead
end by the time it was written: `euro_goal_orders_0a60_full.md`'s own
2026-08-18 "Third pass" resolved `0a60`'s `FUN_1000_89d0`-sourced field-2
calls independently (colony-tile unit search, no register ambiguity) and
explicitly flagged that this doc's "caller-context registers" open question
was conflated with a *different* call site — `20e6`'s own, right here —
not actually answered by chasing `0a60`. Separately, and more decisively:
this gate's own call site (`0x42`/`0x65`, lines ~2597/2609 in the raw dump
below) passes `param_2` — `20e6`'s own formal unit-id parameter — not a
register-sourced value at all, so there was never a "which registers are
live" ambiguity here in the first place, only the semantic "what does `<2`
mean" question, and that was already answered by the fourth pass above
(idle/singleton-chain check) with the sixth pass already concluding **no
code needs shipping regardless** (behavioral gap already covered by
`ai_euro_unit_act`'s H-block bind / `ai_euro_scout_contact_ring_target`).
Nothing left to do under `0x42`/`0x65`.

Regenerate the case dump: `analyzeHeadless <proj> <name> -process
seg_data_resident.bin -readOnly -noanalysis -postScript
GhidraListInstrs.java 0000:2ce0 90 0000:d47f 90 0000:f2 90 0000:a100 90
0000:6ef7 90 0000:2bf2 90 0000:f446 90 0000:d8f7 90 0000:c00b 90 0000:27d
90 0000:c02b 90 0000:c88b 90 0000:1b8 90 -scriptPath tools`.

**Net effect: `FUN_1000_8aac` is no longer a corruption dead-end**, and the
scope of what's left to resolve is now much narrower and precisely stated
(case 2's own return semantics) rather than "15 unnamed cases." New
reusable tooling: `GhidraListInstrs.java` (raw instruction + byte listing,
batches multiple address ranges in one headless launch — used for this
pass's full 15-case sweep), `GhidraDumpBytes.java` (raw hex dump, bypasses
the listing/decompiler entirely — the tool that broke this case open),
`GhidraForceRedecomp.java` (clear + fresh disassemble + decompile, for
reproducing/confirming decompiler jump-table bugs like this one).

**0x42/0x65 found/contact live-write gate (line ~2001-2023) — still open,
narrower blocker than before.** Both arms gate on `FUN_1000_8aac(nation, unit_id, field)` (field 2,
compared `<2`) — a generic per-unit query also used heavily by `0a60`
(`euro_goal_orders_0a60_full.md` lines 316-796, fields 2/3/4/5/6/0xd/0xc, never
itself interpreted there either — this is a shared unresolved helper, not a
20e6-only gap). Traced its resident target this pass: `FUN_281f_08bc` thunks to
`FUN_1000_8aac` (`ram:0x18aac`, confirmed via `address_mapping.csv`), which
decompiles as a two-call stub (`FUN_1000_1e61` — a real, RTLink-loader-shaped
function — then a bare tail call to `FUN_0000_4fa8`). **`FUN_0000_4fa8` is
corrupted**: its decompile carries a fresh `overlaps instruction` warning near
`ram:0xc00b` and the body mixes plainly unrelated case logic (BCD arithmetic,
DOS `INT 21h` string routines) under one switch — the same disassembly-fault
signature `decomp_inventory.md` already catalogs elsewhere, meaning this address
is very likely a **false collision**: an unpatched-thunk placeholder value that
happens to coincide with real resident code rather than a real resolved
call target. Per the project's own established method (`indian_settlement_4528.md`
"Case-dispatch tail", `euro_unit_act.md`'s `a6e4` correction), the fix is to
resolve the true target via `rtlink_decode`'s segment table (raw file offset →
segment index), not trust this address — **not done this pass** (would need
regenerating `seg_data_resident.bin` via `tools/rtlink_overlay_extract.py`, the
loose extraction from earlier sessions is gone from disk). Until then, `0x42`/
`0x65` stay unported; Linux's existing goal-driven found/contact impulses
(`0a60`/`5d04`) are a reasonable functional stand-in, just not this live path.

**Epilogue / commit block (line ~2213-2275) is fully resolved, zero unnamed
globals** — but has no Linux structural home yet. After the upstream arms
(whichever one fires) leave `local_76` = chosen dir (0..7) or 8 = stay:
1. Write `unit+0x314f` = `local_76` (last-commanded-dir byte).
2. If staying (8): set `unit+0x314c` (pending-order-state cache) to 5
   ("roam/re-evaluate next call"), or 6 if `unit+0x3148` bit1 set — both
   already-named fields (`move_scoring_20e6_full.md`'s own prologue table).
3. Else: step `local_88/local_94` one tile in `local_76`'s direction; if that
   tile is walkable (`FUN_1000_84f2`), stash it as a one-shot pending goto
   (`0x314c=0xc`, `0x314d/0x314e` = stashed x/y) instead of moving immediately
   this call.
4. Shared exit (`5a78`): if `0x314c` is 0 or 0xa (idle/none), reset orders to
   `0x30` (idle default) and re-arm `0x314c=5`. If `0x314c==5` (roaming), scan
   8 neighbors via `FUN_1000_8886`/`FUN_1000_8c28`(`&0x40` — **correction,
   2026-08-15**: this is the `MET` flag, not "at-war" as originally guessed
   here — cross-checked against Linux's `ai_diplo_read`, which returns the
   DOS `euro_relation` byte completely raw and defines `AI_DIPLO_MET 0x40`
   as that literal bit, no remapping; see `euro_unit_act.md`'s
   `FUN_4720_049e` writeup for the full cross-check) and clear the roam
   state (`0x314c=0`) the moment a **met** foreign unit is adjacent — i.e.
   **stop passively roaming next to any nation you've already encountered,
   force a re-decide next call** (broader than "enemy only" — the
   re-decide could lead to attack, diplomacy, or just noticing them,
   depending on actual relation, not gated to at-war alone). If a stashed
   one-shot goto (`0x314c==0xb`) has just been reached and the unit is a
   ship (type 0xd..0x12) whose orders were `'1'` (ASCII, some other arm's
   marker — not yet traced), flip to `0x42` (found impulse) on arrival.

**Correction, same pass: the "missing persistent cache" claim below was
wrong — checked `units.h` after writing it.** `unit+0x314c` (pending-order
cache), `+0x314d`/`+0x314e` (goto stash), and `+0x314f` (facing/last dir)
already have direct Linux homes: `ColonizeUnit.orders` (`UNITS_ORDER_NONE`/
`SENTRY`/`FORTIFY`/`FORTIFIED`/`BUILD_COLONY` = DOS `0`/`1`/`5`/`6`/`7`,
already numerically DOS-shaped), `.goto_x`/`.goto_y`
(`UNITS_GOTO_NONE`=0xFF), and `.last_dir` — and `unit+0x314b` (the orders/
act-opcode byte this whole file is about) is `.col1_ai_plan`
(`col1_bridge.c` comment: "DOS unit+0x07 / Col1 ai_plan"). **No struct or
save-format change needed.** What's actually missing is narrower: these
fields round-trip through save/load (`col1_bridge.c`) but nothing in
`ai_euro.c`/`ai.c` *reads* `col1_ai_plan` as a live decision cache the way
`20e6` reads `0x314b` at its own entry — the AI never consults its own
last act-opcode. Wiring the epilogue's roam-abort-if-hostile-adjacent
behavior isn't blocked on missing state either: `ai_euro_unit_act` already
runs `ai_euro_land_try_adjacent_attack` before movement, just gated to
designated "hunter" roles at war (`is_land_hunter`) rather than DOS's
broader "any unit currently idle-roaming" — narrower than DOS, but the
same shape, not a gap requiring new plumbing.

**Real remaining redesign, scoped correctly this time:** just the windowed
best-tile-in-box explore scan (`move_scoring_land.md`'s `2912`/`2a59`
section) replacing Linux's single-step 8-neighbor greedy walker — that one
genuinely doesn't fit the current architecture and still needs `−0x6b1a`/
`−0x6a8e` (structurally located, semantically unresolved) wired in. The
epilogue's state-machine *transitions* (write orders/goto/facing after a
dir is picked) could be wired against the existing fields today, independent
of that scan redesign — smaller, real, immediately portable slice.

**2026-08-20: roam-abort transition shipped.** Wired the one piece of the
"stop passively roaming next to any nation you've already encountered"
transition described above — `unit+0x314c==5` idle-roam state, cleared the
moment a MET foreign unit lands adjacent, forcing a re-decide. Linux has no
persistent `act_state` distinguishing *why* a given AI_MOVE goto was set, so
introduced a narrow file-local marker (`s_euro_roam_wander[]` in
`ai_euro.c`) that `ai_euro_move_scoring_gate` sets only on its two genuine
idle-wander arms (explore-scan target, fallback-west) and that
`ai_euro_set_goto` clears by default on every other goto write (found-tile
pursuit, war hunt, wagon delivery, ship staging) — so a stale roam flag from
an earlier turn can never leak onto a goal-directed goto set by a different
code path. Abort check runs at the top of `ai_euro_unit_act`, same MET-both-
directions gate `ai_euro_try_violate_notify` already uses. Full `ctest`
green (goldens still parked per T3.3, unaffected either way). Not done:
the `0x42` found-impulse "flip on arrival" sub-clause of the epilogue (ship-
only, orders `'1'` marker not yet traced) and the scan-redesign itself,
both still open per above.

**2026-08-20 — T1.1 attempt: `FUN_1000_8aac` fields 3/4/5/6/0xc, real
progress, no semantic closure yet.** Picked up `ai_port_plan.md`'s T1.1
(the highest-leverage queued item, gating T1.2/T1.3/T1.9). Confirmed
`FUN_1000_8aac` itself is a bare two-call stub (`FUN_1000_1e61();
FUN_0000_4fa8(); return;`, no per-field branch inside it at all) — so
whatever `field` (2/3/4/5/6/0xc) selects, the selection happens entirely
inside `FUN_0000_4fa8`'s jump table, exactly as the third-pass finding
above already concluded. Re-verified that jump table completely fresh via
`GhidraDumpBytes.java` at flat `0xd78` (raw bytes, not trusting the old
citation): cases 0-7 read back as `2ce0/d47f/46c7/f2/a100/2ce0/6ef7/2bf2` —
**matches the third-pass table exactly**, so that table is confirmed
correct, not just plausible.

Hand-disassembled cases 3 (`0xf2`), 4 (`0xa100`), and 0xc (`0xc02b`) fresh
via `GhidraListInstrs.java` (ground-truth bytes, decompiler bypassed):

- **Case 3** (`0xf2`): reads a word-pair at `0x2da4`/`0x2da6` (unnamed,
  not in `viceroy_globals.h` or `address_mapping.csv`), does a
  doubleword-shaped clamp/max against the incoming value, `CALLF 0xae72`
  conditionally, then a second comparison loop calling `0xc0c6` — shaped
  like a running max-tracker, not a flat per-unit field read. Checked
  whether `FUN_1000_8b10` (called immediately before the field batch at
  `move_scoring_20e6_full.md` line 1090) writes `0x2da4` first, since that
  would directly explain the read-back — **it doesn't**; ruled out, not
  confirmed.
- **Case 4** (`0xa100`): a small, fully self-contained ~26-byte routine —
  `[0x92c0]=3` unless `[0x92c2]>0x10` (then `[0x92c0]=0`), then
  unconditionally `[0x372] = word[BP+6]; return` with `AX` still holding
  that same echoed value. **Shaped like a per-turn call-budget/throttle
  gate with a side-effect write, not a data query** — if `[BP+6]` in this
  calling shape is the constant nation id (`0x181f`), the accessor's
  return value would be a nonzero constant every time, which would make
  every `iStack_48 != 0` branch downstream in `20e6` (lines 1201/1205/
  1225) an unconditional true — a real, checkable hypothesis, not
  confirmed (needs the exact `[BP+6]`/`[BP+8]` slot-to-argument mapping
  for this specific 3-arg call shape, which is the actual remaining
  blocker, not a new mystery).
- **Case 0xc** (`0xc02b`): a bounds-clamped accumulate-loop tail
  (`SI`/loop-counter shaped), immediately followed in memory by a small
  `SHR BX,4 / AND AX,0xf` nibble-split helper at `0xc054` and an
  ASCII-uppercase/flag-test routine at `0xc06c` — same low-level
  CRT/string-table neighborhood the third pass already catalogued nearby
  cases in (`0xc00b`/`0xc88b`). Reinforces "generic utility region," no
  unit-record touch found in the 60 bytes sampled.
- **Case 6** (`0x6ef7`) re-disassembled as a spot-check: bytes match the
  third pass's colony-bitmap set/clear finding exactly, no change.

**New, unrelated but real finding surfaced while chasing case 3's
`FUN_1000_8b10` lead**: `FUN_1000_8b10` (`ram:0x18b10`) is *not* part of
the `4fa8` family at all — it's the same `FUN_1000_1e61` loader-prologue
shape (confirming that prologue is a generic "resident library loaded"
guard reused by many unrelated thin stubs, not specific to `8aac`) tail-
calling a completely different real function, `FUN_0000_532e`. That
function is fully clean, decompiles with zero warnings, and is a genuine
**transport-chain movement-budget distributor**: computes
`local_8 = per-unit-type-max-moves[unit+0x3146] - unit+0x3150` (a
"moves remaining this turn" formula, stride-`0xe` table at `0x5237`),
then walks a unit chain via `FUN_0000_4272`/`FUN_0000_42ba` (next/prev —
very likely the same transport-chain link fields case 2 above splices,
`unit+0x315e`/`0x315c`), spending a per-unit-type move-cost
(stride-`0xe` table at `0x5238`) out of the shared budget and marking
`unit+0x314c=1` ("moved this turn") on each unit until the budget or
chain is exhausted. Checked Linux (`units.c`): `moves_left` is tracked
**independently per unit**, no shared/pooled chain budget exists — this
DOS mechanic (wagon-train-style shared movement pool) may be a real gap,
or may be functionally superseded by Linux's simpler per-unit model the
same way the `0x42`/`0x65` gate turned out to already be closed by
different means (sixth pass above). **Not chased further this pass** —
flagging as a fresh, undocumented lead for a future session rather than
scope-creeping off T1.1; not yet added to `ai_port_plan.md` since it
needs its own scoping pass first.

**2026-08-20, same day, second pass — the shared `4fa8` prologue itself
cracked, closing the calling-convention question.** Went one level up
from the case bodies to `FUN_0000_4fa8`'s own entry code (never
disassembled before this pass — every prior pass jumped straight to the
`0xd78` table's targets). Raw bytes, `ENTER 0x6,0`:

```
SI = [BP+6]                          ; first stack slot
DI = 0                                ; "not found" sentinel, held live
AX = SI; CALLF FUN_0000_4272(AX)      ; register-arg call, see below
SI = AX; if SI < 0, return DI (bail, 0x5177)
BX = SI * 0x1c                        ; unit-record stride confirmed
[BP-6] = BX
AX = byte[BX+0x3146]                  ; unit's type byte (zero-extended)
[BP-4] = [BP-2] = AX                  ; type byte cached twice
AX = [BP+8]                           ; second stack slot
if AX > 0xe, fall to a defensive 0x5168 branch (see below); else:
BX = AX * 2
XCHG AX,BX          ; AX := old BX (= SI*0x1c, the unit-record offset)
                     ; BX := AX*2 (case index)
JMP CS:[BX+0xd78]    ; dispatch
```

Decompiled `FUN_0000_4272`/`FUN_0000_42ba` (called via bare register `AX`,
no stack args — same "ad hoc per-callee convention" family) to see what
`[BP+6]` actually has to be:

```c
// FUN_0000_4272(int in_AX /* unit index */)
if (in_AX >= 0) {
  while (*(int*)(in_AX*0x1c + 0x315c) >= 0)   // walk PREV (0x315c) to the head
    in_AX = *(int*)(in_AX*0x1c + 0x315c);
}
return in_AX;

// FUN_0000_42ba(int in_AX /* unit index */)
if (in_AX >= 0) in_AX = *(int*)(in_AX*0x1c + 0x315e);   // one step via NEXT (0x315e)
return in_AX;
```

Both operate on `unit+0x315c`/`unit+0x315e` — the **exact same fields**
case 2 above splices, already named `prev_unit_idx`/`next_unit_idx` in
`col1_save.h`'s `ColonizeCol1TransportChain` (round-tripped through
save/load, never live). So `FUN_0000_4272` = "walk PREV links to the
convoy's head unit" and `FUN_0000_42ba` = "step one unit forward via
NEXT" — clean, reusable, fully game-specific findings on their own,
independent of the field-index puzzle.

This resolves the calling convention **by physical necessity, not
push-order guesswork** (push-order reasoning from the decompiler's
argument-list text turned out ambiguous/self-contradictory when checked
both ways): `[BP+6]` gets fed straight into `FUN_0000_4272` as a raw unit
index (`AX*0x1c` addressing) — it **has** to be a small, valid unit-table
index or the game reads garbage memory, so it cannot be the `0x181f`
nation constant (6175 decimal, wildly out of range as an index). It must
be the real unit id (`param_2` in `20e6`'s calls). `[BP+8]` is bounds-
checked against `0xe` (exactly the 15-entry table size) and shifted into
the jump — that's unambiguously `field`. This also matches, and now
properly explains, the already-established case 2 finding for the 2-arg
call shape (`FUN_1000_8aac(unit_id, 2)`): `[BP+6]=unit_id` lines up
exactly. **Net: `[BP+6]` = the unit being queried, resolved to its
convoy's head unit before any case runs; `[BP+8]` = the field/case
selector (0-0xe); any third argument (`0x181f` nation, present on the
`20e6`/`0a60` 3-arg calls) lands one slot further out (`[BP+0xA]`),
outside this shared prologue's 2-slot window — not read by the dispatch
itself, and not found reading `[BP+0xA]` in the 60-byte case-3/4/0xc
windows sampled last pass either (not exhaustively ruled out per-case,
but not seen).**

Concrete new implication for **fields 2-0xe generally**: every case
operates on the **convoy head's** unit record (`AX`/`BX` = head's
`unit_id*0x1c` at entry), not necessarily `param_2`'s own record — for an
unchained unit (the common case, `prev_unit_idx==-1`) these are the same
unit, so this doesn't overturn any already-shipped behavior, but it's the
right frame for reading case bodies going forward: a case that looks like
it's asking "does *this* unit have arms/cargo" is actually asking "does
*this convoy* have arms/cargo," coherent with `20e6`'s field-3/4/5/6/0xc
block being the **cargo/colony sail matrix** T1.3 needs (a convoy-level
resource query is exactly the right shape for that).

Bounds-check failure path (`[BP+8]>0xe`, `0x5168`) turned out to be a
defensive fallback — `AX(=SI)` steps forward one unit via `FUN_0000_42ba`
and retries the whole dispatch on the next chain member, bailing to 0 if
the chain runs out. Not reachable by any real call site found so far (all
confirmed literals are 2-0xd), so not chased further.

**Where T1.1 actually stands now**: the *mechanism* (unit resolution +
case dispatch) is fully closed with physical, not inferential, certainty.
Individual field semantics for 3/4/5/6/0xc are **still not named**.

**Same pass, immediate follow-up — re-read case 3's full body (`0xf2`-
`0x143`, 81 bytes, confirmed complete by its own `RETF`) against the
corrected frame, inconclusive.** `AX` at entry is the convoy-head record
offset as expected, used in a doubleword clamp against unnamed globals
`0x2da4`/`0x2da6`, then a small loop (`CALLF 0xae72`/`0xc0c6`/`0x26fa`)
converging on a return value. But partway through, the byte-exact
disassembly shows `OR AL,byte ptr [BP+DI+0x4c4]` — a **real** 8086
`[base+index+disp16]` addressing form (verified from the raw modrm byte,
not a rendering artifact), with `DI` confirmed `0` (set by `4fa8`'s
prologue, untouched since) and `BP` still `4fa8`'s own 6-byte frame. A
displacement of `0x4c4` (1220 bytes) from a 6-byte frame reads **far
outside any local variable** — either a legitimate DOS-era idiom this
project hasn't catalogued yet (some compilers/models use a frame-like
register to reach a fixed-offset global data block, not just locals) or
a case where trusting the raw disassembly's operand naming needs one
more level of care than usual. Genuinely unclear which without deeper
context (what's really stored 1220 bytes past this specific frame — a
question about the compiler's overall memory layout convention, not
about this one function). **Stopping here rather than guessing** —
consistent with the project's own "never invent a constant" rule. Cases
4/0xc not re-examined at this depth this pass (case 3 alone already hit
the same kind of wall).

Individual field semantics for 3/4/5/6/0xc remain open. `T1.2`/`T1.3`/
`T1.9` stay blocked until they're named or a live capture settles the
`0x4c4`-displacement question above (the concrete next step, and a
better use of a live DOSBox-X session than re-guessing more case bytes
statically).

**2026-08-20, later same day — T1.1 fields 4/5/6/0xc pass: 0xc closed clean,
4/5/6 hit a new, different live-capture wall; this pass's own case-4/case-6
citations above are wrong and retracted.** Picked up the plan's "fields
4/5/6/0xc are still genuinely static-tractable" framing. Built a new tool,
`tools/GhidraDisasmExact.java` (force-clears and disassembles starting
*exactly* at a given address, no fallback) — `GhidraListInstrs.java`
silently falls back to `Listing.getInstructionContaining(start)` when the
target isn't already an instruction boundary from a prior analysis pass,
and that's what happened here: it returned a **different, nearby**
already-analyzed instruction, 1-2 bytes off from the literal jump-table
value, and the mismatch went unnoticed in this file's own case-4/case-6
notes above. Re-verified from scratch against a freshly re-extracted
`seg_data_resident.bin` (`tools/rtlink_overlay_extract.py` against the
current `COLONIZE/VICEROY.EXE`, MD5 `0f5d5b00...` — matches the project's
own already-verified hash, so not a stale-binary issue) — byte-identical to
the existing `ghidra_overlay_scratch/OverlayTest` project at every address
checked, so this isn't an extraction/staleness artifact either.

- **Case 0xc (`0xc02b`) — confirmed closed, no change.** Force-disassembled
  exactly at `0xc02b`: decodes cleanly with no ambiguity (`MOV [BP-4],0`;
  loop comparing `SI`/`[BP-0xa]`; accumulate; bounds-check; `POP SI; LEAVE;
  RETF`). Backward jumps (`JL 0xbfd1`, `JGE`-fail-path `JMP 0xbfc0`) land
  well before this window, confirming `0xc02b` is a **mid-function landing
  inside some unrelated, larger routine** (no `ENTER`, a bare `POP SI` with
  no matching `PUSH` in view) — not its own dispatch target at all, just a
  coincidental collision the same way 12 of the other 13 non-2/non-6 cases
  already were. No unit-record (`*0x1c`) or colony-pointer (`0x8542`) touch
  anywhere in it. Re-confirms the third pass's "generic, no unit touch"
  verdict independently, this time with the corrected calling-convention
  frame in hand. **Field 0xc: closed, not game-specific, no further work.**

- **Case 5 = Case 4? No — case 5 = case 0, freshly re-confirmed.** Re-dumped
  the raw jump table at `0xd78` as 16-bit words (not the earlier session's
  byte-string transcription): `word[5] = 0x2ce0`, bit-identical to
  `word[0]`. Case 5 and case 0 are **the literal same target address** —
  whatever case 0 is, case 5 is too, by construction, no separate tracing
  needed. (Third pass already filed case 0 under "generic, no unit touch";
  this pass didn't overturn that filing, see below for what's actually
  unresolved about it.)

- **Cases 0/5 (`0x2ce0`) and case 4 (`0xa100`) and case 6 (`0x6ef7`) — real,
  reproducible finding: the literal jump-table addresses do NOT land on
  clean instruction boundaries.** Force-disassembling exactly at each cited
  address produces implausible instruction streams — `PUSH ES` immediately
  followed by an out-of-range shift-count `RCL byte[BP+SI+3],0x83` then a
  segment-prefixed near `RET 0x1092` (case 4, `0xa100`); `ADD AX,[BP+DI]`
  (reads the ENTER-pushed old-BP word at `[BP+0]`, since `DI=0` is held live
  from `4fa8`'s own prologue) then `PUSH CS` with no matching pop before a
  later `LEAVE`/`RETF` (case 6, `0x6ef7`); `ADD [BP+DI],CL` then a
  `SAR byte[SI+8],0xb8` with the same out-of-range-immediate shape (case
  0/5, `0x2ce0`). All three share one signature: a handful of bytes that
  don't cohere as a real instruction sequence, followed by a resync into
  something that *does* look like real, sensible code a few bytes later —
  and in two cases, that later code is demonstrably real: widening the
  disassembly window around case 6 found a **complete, coherent, `ENTER`-
  headed function at `0x6ee0`** (`CL = [BP+6]&7; AX = 1<<CL` — a bit-mask
  build — then falls into exactly the `SAR CX,3; ADD CX,[8542]; ADD CX,0x84`
  bit-offset computation, then the on/off-gated `OR`/`AND [BX],AL` set/clear,
  `LEAVE; RETF`) — a real, self-contained "set or clear bit `arg&7` of byte
  `colony_ptr+0x84+(arg>>3)`" routine, matching this doc's own third-pass
  description almost exactly. But its natural instruction boundary for the
  bit-offset step is **`0x6ef5`, two bytes before the jump table's cited
  `0x6ef7`** — `0x6ef7` is the *last byte* of that boundary's own 3-byte
  `SAR CX,0x3` encoding (`c1 f9 03`), not a valid entry point into this
  function at all. Case 0/5 shows the same shape in the other direction:
  widening around `0x2ce0` found two real conditional branches (`CMP[0xa0],0
  / JZ`, `CMP[0xa2],0 / JNZ`) both explicitly targeting `0x2ce1` — **one
  byte after**, not at, the table's cited `0x2ce0`. Two different cases,
  two different byte offsets (+2 late for case 6, -1 early for case 0/5) —
  ruled out a uniform table-parsing bug on this pass's own end (would
  produce a single consistent offset, not two different ones); the raw word
  values themselves were independently re-confirmed straight from `0xd78`
  bytes, not inferred.
  **Also retracts this same file's own "2026-08-20" case-4 finding above**
  (`[0x92c0]=3 unless [0x92c2]>0x10, then [0x372]=echoed word[BP+6]`,
  cross-matched to `FUNCTION_CATALOG.md`'s `FUN_1a0a_0004` palette-cycle
  init): that C does exist, verified real and byte-matching in
  `viceroy_unpacked_2.c`/`viceroy_unpacked.c` — but at overlay address
  `1a0a:0004`, a **completely different function in a different segment**,
  not reachable from the resident `0xa100` jump target at all. The two were
  conflated by pattern-matching decompiled C text without re-verifying the
  citing address landed inside the actual case body — same mistake class
  the project's own method notes already warn about (misattribution,
  coincidental collision), now caught on this file's own prior entry rather
  than an external one.
  **Conclusion: this is a genuine byte-level wall, not a guessable one.**
  Real, DOS-hardware execution at `CS:IP = 0000:a100` / `0000:6ef7` /
  `0000:2ce0` is deterministic and would run exactly the implausible bytes
  found — whether that's an intentional (if bizarre) byte-sharing trick, a
  latent bug in the shipped 1994 binary that these specific field values
  rarely trigger, or evidence that this project's addressing model has one
  more subtlety not yet understood for *these three* targets specifically
  (case 3 and 0xc don't show it) isn't resolvable from static bytes alone.
  **Same shape as `T4.7`'s case-3 `0x4c4` wall, now extending to cases
  0/4/5/6 as well** — a live DOSBox-X breakpoint on `FUN_0000_4fa8` entry,
  single-stepping through calls with `field∈{0,4,5,6}`, would show what the
  CPU actually fetches and executes. Not the same root cause as case 3's
  wall (that one's a plausible-but-unverifiable *far displacement*; this
  one's an implausible *local byte alignment*), so tracked as a new item
  (`T4.8`) rather than folded into `T4.7`.
  **Field 4: open, not closed (correction).** Field 5: same target as field
  0, whatever field 0 turns out to be. Field 6: the bit-set/clear mechanism
  and its `colony_ptr+0x84` bitmap are real and now more precisely mapped
  (mask-build sub-routine at `0x6ee0` identified, not just guessed at) but
  its actual entry alignment is unresolved, so still open, not closed.

**2026-08-21 — `T4.7`/`T4.8` both resolved, straight from an existing dump,
no debugger breakpoint needed.** Source: `dosbox-x-dumps/find_memory` (a
DOSBox-X savestate, zip-format, `Save_Remark`="memfind"). Same method as
`T4.1`'s terrain table: calibrated the `Memory` blob's header size
(`HDR=0x88` for this capture — cross-checked by locating the already-known
`-0x7b44` Indian-trade throttle-table bytes at their expected `DS:0x84bc`
and confirming byte-exact match) before trusting any offset math. Located
`FUN_0000_4fa8`'s real runtime code segment (**`CS=0x0823`**) not by timing
a breakpoint but by searching the whole 17MB memory image for the
5-byte `JMP CS:[BX+0xd78]` dispatch tail (`2E FF A7 78 0D`) — exactly one
hit, and the jump table read from `0x823:0xd78` matches this doc's own
cited values word-for-word (`2ce0/d47f/46c7/f2/a100/2ce0/6ef7/2bf2/f446/
d8f7/c00b/27d/c02b/c88b/1b8`), confirming the segment beyond doubt.

- **Case 3 (`T4.7`) — resolved, and it's a bigger correction than
  expected.** Hand-disassembled the real 90-byte body at `0x823:0xf2`
  clean, no ambiguity: **`[BP+DI+0x4c4]` does not appear anywhere in it.**
  That access was a static-tool artifact, not real. The actual body never
  touches `[BP-6]` (the unit-record pointer the shared prologue stashes
  before dispatch) **at all** — instead it validates a `DS:0x2DA4`/
  `0x2DA6` word pair (retry-on-out-of-range against bounds from a second
  far call) and ends by calling a "wait for keypress" helper. Traced its
  three `CALLF` targets (same live-dump method, same `CS=0x823` base):
  `130A:0002` is `INT 16h AH=1` (BIOS check-keystroke, returns 0/1,
  non-blocking poll) — a clean, textbook 12-byte routine (`PUSH BP; MOV
  BP,SP; MOV AH,1; INT 16h; JNZ ...`); `127B:038B` reads a cached word pair
  from `DS:0x92FC`/`0x92FE` gated on flag `DS:0x92F8`; `0A85:00DA` itself
  calls `130A:0002` again plus a sibling entry `130A:0016`, i.e. a genuine
  "block until a key is pressed" gate. **Conclusion: case 3 is not a
  per-unit field query at all** — it's an unrelated UI/cursor-validation
  utility that happens to share this dispatcher's index 3, not part of the
  "field accessor" family cases 0/2/4/5/6/0xc belong to. **This matters for
  T1.3**: its cargo/colony matrix reads `iStack_4a =
  FUN_1000_8aac(0x181f,param_2,3)` expecting a per-unit numeric field —
  re-verify that call site actually reaches this same case (same `[BP+8]`
  convention) before porting anything against it; a keyboard-wait
  mid-AI-scoring would be a strange thing to actually execute, so either
  the call site never really takes this path at runtime, or there's a
  second field-3 dispatch this doc hasn't found yet. **Don't assume the
  field-3 numeric semantic anyone previously guessed at — reopen that
  question instead of trusting old framing.**
- **Case 0/5 (`T4.8`) — probable resolution, one byte-offset fix.**
  Byte-exact at the table's cited `0x2ce0` decodes as garbage (`SAR
  byte[SI+8], 0xb8` — an absurd shift count, matching the "implausible"
  finding exactly). **+1 byte (`0x2ce1`)** gives a fully clean, self
  consistent read: `OR AX,AX` / `JL +8` / `MOV AX,1` / `CALLF 0AFB:000E`
  / `LEAVE` / `RETF`, with the `JL` branch landing exactly on the `LEAVE`
  — internally consistent, not just locally plausible. Less
  independently-confirmed than case 4 below (no external cross-reference),
  but the branch-target self-consistency is a real signal, not a guess.
- **Case 4 (`T4.8`) — resolved, strongly, and it vindicates the retracted
  finding.** Byte-exact at `0xa100` also decodes as garbage (`RCL
  byte[DX+3], 0x83` — same shift-immediate-garbage shape). **+2 bytes
  (`0xa102`)** gives a fully clean routine that reads/writes **`[0x92C0]`**,
  **`[0x92C2]`**, and **`[0x372]`** — the *exact* three addresses this same
  file's 2026-08-20 pass found and then retracted, believing they belonged
  to an unrelated function (`1a0a:0004`) reached by pattern-matching
  decompiled text without checking address alignment. That retraction was
  wrong about the address; the content was real all along, just
  mis-attributed — it's genuinely case 4's own body, two bytes past the
  jump table's literal entry.
- **Case 6 — correction, was never actually broken.** Byte-exact at the
  table's own `0x6ef7`, **no offset adjustment needed at all** — decodes
  clean and matches this doc's own already-known semantic description
  precisely: `OR [BX],AL` (set a bit) immediately followed by `AND [BX],AL`
  (clear a bit, using a pre-`NOT`'d mask), `LEAVE`/`RETF` between the two.
  The earlier "reproducible... lands 2 bytes into a `SAR CX,0x3`"
  finding was a real byte pattern, but at a different starting point
  (`0x6ef5`, reached by scanning forward from *before* the true entry) —
  not evidence against `0x6ef7` itself. Exactly the "check the citing
  address actually falls inside the target's own boundary" mistake this
  file's own method notes already warn about, now caught on this file's
  prior entry.

Net: `T4.7` fully closed. `T4.8` closed for cases 4 and 6 with strong
confidence, case 0/5 with moderate confidence (no live DOSBox-X session
used for any of it — all four resolved from the existing `find_memory`
dump). Field 6's `colony_ptr+0x84` bitmap semantics and mask-build routine
stand as previously mapped; only the entry-alignment question is new here.

**2026-08-21, same day — case 3's `0A85:00DA` fully decoded (110 bytes),
resolving the T1.3 reachability worry above.** It's a **keyboard-buffer
flush, not a blocking wait**: `check-key; if none queued, LEAVE/RETF
immediately; else consume it, check again, loop only while more are
queued`. Never waits for a *future* keystroke — in the normal AI-turn case
(nothing queued) it returns the same instant, so it's not a hang risk
called from AI scoring after all. Net semantic for the whole case-3 body:
read a cached `DS:0x2DA4`/`0x2DA6` last-cursor/click position, validate
against bounds (retry-refetch if out of range), flush any stray keyboard
input, return. Entirely unit-independent, confirmed not part of the
"field" family. **For T1.3**: `iStack_4a` (this matrix's own field-3 read)
ends up holding whatever stale UI coordinate happens to be cached there —
noise, not a real AI signal, but harmless (no crash/hang risk). If this
matrix is ever ported, treat that term as safely inert/no-op rather than
chasing byte-exact fidelity for it — there's no real semantic content to
preserve.

**2026-08-22 — resuming T1.3 now that `T4.7`/`T4.8` are closed; two new findings,
one closes cleanly, the other reopens a question about the whole matrix.**

First, cross-checked `T4.8`'s resident-code claim for cases 0/4/5/6 against
every other `dosbox-x-dumps/*` save (24 independent captures, spanning
2026-08-08 through 2026-08-22, different game states/turns), not just
`find_memory` alone — located the same `JMP CS:[BX+0xd78]` 5-byte dispatch
signature in each, then read the case 3/0-5/4/6 and jump-table bytes at the
same signature-relative offset in every one. **All 24 are byte-identical.**
This answers a real methodological worry before it got written down anywhere:
`CS=0x823` sits at a huge span (dispatch tail at `+0x4fe2`, farthest case
body at `+0x123b8`) that's larger than a typical small resident helper, which
raised the possibility this address range might actually be a shared
overlay-swap buffer whose content depends on whichever overlay happened to
be loaded at capture time — that would have meant `T4.8`'s single-dump
resolution wasn't trustworthy for reuse here. It's not: 24 saves across two
weeks of different play sessions agree byte-for-byte, so this is genuinely
stable resident content, not overlay-swap noise. `T4.8`'s findings stand.

Second — and this is the one that matters for the matrix — re-derived case
0/5's (`iStack_16`, field 5) *actual returned value* by tracing all the way
through its `CALLF 0xafb:0x000e` callee, not just confirming the call
target exists. Full body (disassembled directly from `find_memory`'s bytes,
`0afb:000e` onward): `CX = AX; DI = BX = DX = 0; if CX>=0x10: BX=1; if
CL&0x40: DI=1; if CL&0x20: DX=1;` then a chain of `OR`-and-branch tests on
`BX`/`DX`/`DI` against globals `DS:0xa0`/`0xa4` that either falls through to
`POP DI; RETF` (returning **AX unchanged**) or takes a side path through
`push cx; call 0x187c:0xa`. Case 0/5's own caller sets `AX=1` unconditionally
right before this call (only after passing a `sign(AX)` gate — negative
input skips the call and returns unchanged) — so `CX` is **always exactly
1** on entry, `1<0x10` and `1&0x40`/`1&0x20` are both false, all three of
`BX`/`DI`/`DX` end up 0, and the branch chain lands on `RETF` with `AX`
untouched. **Net: for any non-negative input (i.e. any real unit id — the
only kind this matrix ever passes), field 0/5 always returns exactly `1`,
full stop, regardless of what unit or context it's called for.** Not a
per-unit query at all — a hardcoded constant in disguise. Confirmed from
real bytes, not inferred from the case's general shape.

This closes field 5's real value (`iStack_16 = 1` always, for this matrix's
call pattern) but it also updates a standing worry: field 3 was already
known to read stale UI noise (2026-08-21), and now field 5 is a disguised
constant. Combined with `T1.2`'s own finding that field 2's chain fields
(`unit+0x315c/0x315e`) are "never actively written for land units in this
port" — meaning `iStack_a8` (`field 2 - 1`) is *also* realistically a fixed
value for every call this matrix ever makes on a land unit, not a variable
signal — **three of this formula's five terms (fields 2, 3, 5) are now
known to carry no real per-call variance**, leaving only fields 4 and 6
(both already flagged, independently of this session, as reaching
non-numeric side-effecting code — a palette-timer reset and a colony
bitmap bit toggle respectively, not plain reads) as the only terms that
could possibly vary. **Not confirmed to a certainty this session**, but
now a sharply scoped next step if this item is resumed: check whether
fields 4 and 6's *return values* (as opposed to their already-documented
side effects) carry any real per-call signal at all, the same way this
pass did for field 5 — if they turn out to be similarly input-independent,
the honest conclusion becomes "this whole `local_9c`/`iStack_82` gating
formula is functionally constant in the shipped binary," which would
finally explain why porting it byte-for-byte has stalled across so many
passes: there may not be a real design signal here to preserve. Don't
guess at fields 4/6's return values without doing the same full trace this
pass did for field 5 — a plausible-looking side effect (palette reset,
bitmap toggle) doesn't by itself tell you what ends up in `AX`.

**Same pass, continued — field 4 traced too, same shape as field 5.** Full
byte-accurate body at `0823:a102` (the already-established `+2`-corrected
entry): `XCHG AX,DX; ADD AX,[BX+SI]; CMP word[0x92c2],0x10; JG +6` — that
branch either falls through to `MOV word[0x92c0],0` or skips it — **but
both paths reconverge on the same three instructions: `MOV AX,[BP+6]; MOV
[0x372],AX; LEAVE; RETF`.** `[BP+6]` is the shared dispatcher's own first
argument slot — the resolved unit id (convoy head) — so **field 4 always
returns the caller's own input unit id, unchanged**, on top of its already-
documented palette-timer side effect. Not a per-unit *attribute* query
either: it's an identity echo. Plugging this into the formula
(`iStack_82 = (-iStack_48 - (iStack_16 - iStack_a8)) - iStack_4a`) with
`iStack_48 = unit_id`, `iStack_16 = 1` (this pass's field-5 finding),
`iStack_4a` = UI noise (already known), `iStack_a8` = near-fixed per `T1.2`:
the formula reduces to essentially `-unit_id + small_constant - noise` —
dominated by the acting unit's raw array-index value, which carries no
game-design meaning (units aren't ordered by anything relevant to cargo
scoring). **This is a real, evidence-based reason to suspect the whole
`iStack_82`/`local_9c` construction never worked as originally intended in
the shipped 1994 binary** — same shape as a jump-table-generation mismatch
scrambling which case bodies the compiler's switch statement actually
reaches, not a deliberately obscure design. Structural confidence high
(every step here is a direct byte read, not a guess); semantic confidence
about *why* DOS shipped it this way is not established — keeping the two
separate per this project's own standing caution.

Attempted field 6 too, but **stopping short of a claim there — found a
discrepancy with this doc's own 2026-08-21 case-6 write-up worth flagging,
not papering over.** That entry describes `0x6ef7` as decoding "clean" into
`OR [BX],AL` / `AND [BX],AL` with `LEAVE`/`RETF` between. Re-parsing the
same bytes instruction-by-instruction from the literal start (not just
locating that recognizable fragment further into the stream): the actual
first ~21 bytes at `0x6ef7` are `ADD AX,[BP+DI]; PUSH CS; INC DX; TEST
[BX+DI+0x84c1],AX; ADD [BP+DI+0x87e],AL; ADD [SI+6],DH; MOV BX,CX` *before*
reaching the `OR [BX],AL` the prior entry cites — a chain that includes two
far-offset memory touches (`[BP+DI+0x87e]`, `[BX+DI+0x84c1]`) in the same
suspicious shape as case 3's original (later-retracted) `[BP+DI+0x4c4]`
finding. Whether those 7 instructions are genuinely part of what executes
here (in which case the "clean" characterization undersold real, possibly
consequential side effects) or whether `0x6ef7` itself needs the same kind
of small entry-offset correction cases 0/4 got (in which case the true
entry is somewhere past this prefix, likely right at the `MOV BX,CX; OR
[BX],AL` pair) isn't resolved by this pass — didn't have time to trace
`0x6ee0`'s mask-build caller's own control flow to see which byte it
actually calls/jumps into. **Don't trust either the prior "clean, no
adjustment" claim or this pass's "suspicious prefix" observation as final
until someone traces the actual entry control flow into this address —
flagging the conflict is this pass's contribution, not resolving it.**

**Same pass, continued — field 6's discrepancy resolved, and it corrects a
previously-closed `T4.8` sub-finding.** Fetched a wide, properly-aligned
window (`0823:6ed0..6f30`) instead of starting exactly at the cited
`0x6ef7`: the real mask-build helper genuinely starts at **`0x6ee0`**
(`ENTER 6,0; CL=[BP+6]&7; AX=1<<CL; CX=[BP+6]; SAR CX,3; ADD CX,[0x8542];
ADD CX,0x84; CMP word[BP+8],0; JZ clear-path; ...OR/AND [BX],AL...`) — a
clean, self-consistent 3-argument function (bit index, colony pointer via
`DS:0x8542`, set/clear flag). But the jump table's word 6, re-read straight
from `0xd78` (`0x6ef7`, confirmed bit-exact, not a transcription slip),
lands exactly on the **third byte of that function's own `SAR CX,byte 0x3`
instruction** (`0x6ef5-0x6ef7` = `C1 F9 03`) — genuinely mid-instruction,
not a valid entry point at all. This directly **contradicts and corrects**
this doc's own 2026-08-21 "Case 6 — correction, was never actually broken"
entry, which claimed `0x6ef7` decoded clean with no adjustment — that
claim was checked by locating the recognizable `OR`/`AND` fragment further
into the byte stream without verifying the *literal* entry byte lines up
with the start of an instruction; it doesn't.

Traced what the CPU actually executes starting at the literal `0x6ef7`,
now with the case-dispatcher's own prologue fully in hand (recovered this
pass, `0823:4fb0` onward — was never disassembled before): at the moment
of `JMP CS:[BX+0xd78]`, **`BX` = `field_index*2`** (the jump-table index
register itself, not colony-related), **`DI = 0`** (confirmed, matches
prior convention), and — the key new fact — **`CX` is left over from
`FUN_0000_4272`'s own internal register use** (the convoy-head walker
called earlier in this same prologue), never set or documented as part of
the case-dispatch calling convention. Entering at `0x6ef7` with these
registers: `ADD AX,[BP+DI]`(=`[BP]`) → `PUSH CS` → `INC DX` → `TEST
[BX+DI+0x84c1],AX`(=`[0x84cd]`, flags-only) → `ADD [BP+DI+0x87e],AL`
(=`[BP+0x87e]`, a real write ~2KB past the frame) → `ADD [SI+6],DH`(SI
uncontrolled) → `MOV BX,CX` → `OR [BX],AL`. **Because `CX` is whatever
`FUN_0000_4272` left behind, not the intended `colony_ptr+0x84+bit_offset`
address, the final `OR [BX],AL` writes to an address this call site does
not control** — the "set/clear a colony flag bit" semantic the jump table
*appears* to offer for field 6 is not what actually executes; the real
colony-pointer/bit-offset arithmetic (the `SAR`/`ADD [0x8542]`/`ADD 0x84`
sequence) never runs at all, having been skipped over by landing mid-
instruction past it. **Field 6, like 0/3/4/5, does not deliver the field
this matrix's decompiled C names (`iStack_46`) as if it were a clean per-
unit read.** Whether this is a genuine (rare, likely harmless in practice
since `[BP+0x87e]`/`[SI+6]`/`[BX]` probably land on unused stack/data
space most of the time) shipped bug in the 1994 binary, or whether this
call site is simply never reached at runtime for some other reason not
visible in this static trace, isn't resolved by this pass — structural
confidence is high (every step is a direct byte read against a freshly-
recovered, complete prologue), semantic confidence about *why* is not
established, kept separate per this project's standing caution.

**Net for `T1.3`, all five terms now accounted for**: field 2 near-fixed
(`T1.2`), field 3 UI noise, field 4 a raw unit-id echo, field 5 a disguised
constant, field 6 a broken/uncontrolled-address dispatch that never reaches
its own apparent intent. **No term in the `iStack_82`/`local_9c` formula
carries the per-unit/per-cargo signal its variable names imply.** This
matches `T1.2`'s "no code to ship" shape closely enough to close on the
same basis: there's nothing here a faithful port could preserve, because
the shipped binary itself doesn't compute anything meaningful at this
call site. Checking `T1.3` off in `ai_port_plan.md` on this basis. `ctest`
not run (doc-only), no `src/` touched — nothing to port means nothing to
test.

**Correction to `T4.8`**: that item's case-6 finding ("was never actually
broken... no offset adjustment needed") is superseded by this update — the
byte-decode was correct as far as it went, but citing the `OR`/`AND`
fragment as reachable "clean" content without checking the literal `0x6ef7`
entry point against a real instruction boundary repeats the exact mistake
`T4.8`'s own case-4 entry had already caught and fixed for a different
case. Left `T4.8`'s checkbox as `[x]` (the item's real question — does
this need a live DOSBox-X session — is still answered "no, resolved from
existing dumps," just with a corrected answer for case 6 specifically) but
added a pointer there to this section.

## Raw recovered C

```c
undefined2 FUN_521d_20e6(undefined2 param_1,uint param_2)

{
  char *pcVar1;
  uint *puVar2;
  int *piVar3;
  byte *pbVar4;
  char *pcVar5;
  undefined1 *puVar6;
  bool bVar7;
  bool bVar8;
  char cVar9;
  byte bVar10;
  undefined2 uVar11;
  int iVar12;
  uint uVar13;
  uint uVar14;
  int iVar15;
  int iVar16;
  undefined2 unaff_CS;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar17;
  uint uVar18;
  uint uVar19;
  undefined2 uVar20;
  byte *pbVar21;
  int in_stack_0000ff16;
  uint uStack_e8;
  uint uStack_e6;
  int iStack_e2;
  sbyte sStack_de;
  int iStack_d4;
  int iStack_d2;
  int iStack_d0;
  int iStack_ce;
  int iStack_ca;
  char acStack_c8 [8];
  char cStack_c0;
  undefined2 uStack_b8;
  uint uStack_b6;
  int iStack_b4;
  int iStack_b2;
  int iStack_b0;
  int iStack_ae;
  uint uStack_ac;
  int iStack_aa;
  int iStack_a8;
  uint uStack_a6;
  int iStack_a4;
  int iStack_a2;
  int iStack_a0;
  undefined2 uStack_9e;
  uint uStack_9c;
  int iStack_9a;
  int iStack_98;
  int iStack_96;
  uint uStack_94;
  int iStack_92;
  int iStack_90;
  int iStack_8e;
  uint uStack_8c;
  undefined1 auStack_8a [2];
  uint uStack_88;
  undefined2 uStack_86;
  uint uStack_84;
  int iStack_82;
  int iStack_80;
  int iStack_7e;
  int iStack_7c;
  int iStack_7a;
  int iStack_78;
  uint uStack_76;
  int iStack_74;
  undefined2 uStack_72;
  int iStack_70;
  uint uStack_6e;
  int iStack_6c;
  int iStack_6a;
  int iStack_68;
  int iStack_66;
  uint uStack_64;
  uint uStack_62;
  int iStack_60;
  uint uStack_5e;
  uint uStack_5c;
  uint uStack_5a;
  uint uStack_58;
  undefined2 uStack_56;
  int iStack_54;
  undefined2 uStack_52;
  uint uStack_50;
  uint uStack_4e;
  uint uStack_4c;
  int iStack_4a;
  int iStack_48;
  int iStack_46;
  int iStack_44;
  int iStack_42;
  int iStack_40;
  int iStack_3e;
  undefined2 uStack_3c;
  int iStack_3a;
  int iStack_38;
  int iStack_36;
  int iStack_34;
  int iStack_32;
  int iStack_30;
  int iStack_2e;
  int iStack_2c;
  uint uStack_2a;
  uint uStack_28;
  int iStack_26;
  uint uStack_24;
  int iStack_22;
  int iStack_20;
  uint uStack_1e;
  uint uStack_1c;
  undefined2 uStack_1a;
  uint uStack_18;
  int iStack_16;
  int iStack_14;
  int iStack_12;
  uint uStack_10;
  uint uStack_e;
  uint uStack_c;
  int iStack_a;
  int iStack_8;
  int iStack_6;
  int iStack_4;
  
  uStack_b8 = 1;
  iStack_8e = 0;
  iStack_12 = 0;
  iStack_ae = 0;
  iVar15 = param_2 * 0x1c;
  uStack_e6 = *(byte *)(iVar15 + 0x3147) & 0xf;
  if ((((*(char *)(iVar15 + 0x314c) != '\0') && (*(char *)(iVar15 + 0x314c) != '\x05')) &&
      (*(char *)(iVar15 + 0x314c) != '\x06')) && (*(byte *)(iVar15 + 0x314c) < 10))
  goto LAB_OVL14_L0000__005a78;
  iVar16 = param_2 * 0x1c;
  uStack_88 = (uint)*(byte *)(iVar16 + 0x3144);
  uVar14 = (uint)*(byte *)(iVar16 + 0x3145);
  uStack_76 = 8;
  uStack_64 = (uint)*(byte *)(iVar16 + 0x3146);
  uStack_94 = uVar14;
  iVar15 = FUN_1000_84f2();
  if (iVar15 == 0) {
    *(undefined1 *)(iVar16 + 0x314b) = 0x40;
    unaff_CS = 0x181f;
    goto LAB_OVL14_L0000__005a78;
  }
  if ((*(char *)(param_2 * 0x1c + 0x314b) == 't') || (*(char *)(param_2 * 0x1c + 0x314b) == 'i')) {
    iStack_6 = 1;
  }
  else {
    iStack_6 = 0;
  }
  uStack_1a = FUN_1000_8b42(0x181f,uStack_88,uStack_94,uStack_e6,uVar14);
  uVar14 = FUN_1000_8804(0x181f,uStack_88,uStack_94,0xffff,0xffff);
  iStack_74 = *(int *)0x8db8;
  uStack_72 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                            ((undefined1 *)*(undefined2 *)0x8542)[1]);
  uStack_62 = FUN_1000_8804(0x181f,uStack_88,uStack_94,uStack_e6,0xffff);
  iStack_2e = *(int *)0x8db8;
  if ((int)uStack_62 < 0) {
    iStack_2c = -2;
  }
  else {
    iStack_2c = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                              ((undefined1 *)*(undefined2 *)0x8542)[1]);
  }
  uStack_ac = FUN_1000_8f74(uStack_88,uStack_94,0xffff,0xffff);
  iStack_a0 = *(int *)0x8db8;
  uStack_9e = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8d4a,
                            ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
  uVar11 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,uStack_e6);
  iStack_54 = FUN_1000_8c50(0x181f,uVar11);
  iStack_3e = *(int *)(*(int *)0x8d4a + uStack_e6 * 2 + 10);
  uStack_52 = FUN_1000_8804(0x181f,uStack_88,uStack_94,uStack_e6,0xfffe);
  uStack_3c = *(undefined2 *)0x8db8;
  if ((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) || (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) {
    iStack_34 = 0;
  }
  else {
    iStack_34 = 1;
  }
  iStack_aa = func_0x0001897c(0x181f,uStack_88,uStack_94);
  if ((iStack_aa == 0x19) || (iStack_aa == 0x1a)) {
    iStack_90 = 1;
  }
  else {
    iStack_90 = 0;
  }
  uStack_84 = FUN_1000_891c(0x181f,uStack_88,uStack_94);
  uStack_84 = uStack_84 & 0x40;
  uStack_5a = FUN_1000_8944(0x181f,uStack_88,uStack_94);
  uStack_5a = uStack_5a & 10;
  iStack_38 = FUN_1000_8912(0x181f,uStack_88,uStack_94);
  if (iStack_38 < 0) {
    uStack_2a = 5;
  }
  else {
    uStack_2a = (uint)(byte)((undefined1 *)&LAB_0000_9870)[uStack_e6 * 0x10 + iStack_38];
  }
  uStack_86 = FUN_1000_8908(0x181f,uStack_88,uStack_94);
  iStack_14 = FUN_OVL14_L0000__007349(0x181f,uStack_e6,param_2);
  if ((*(char *)(param_2 * 0x1c + 0x3146) == '\x02') || (*(char *)(param_2 * 0x1c + 0x3146) == '\0')
     ) {
    iStack_6a = 1;
  }
  else {
    iStack_6a = 0;
  }
  if (*(char *)(param_2 * 0x1c + 0x315b) == '\x1b') {
    iStack_6a = 0;
  }
  if ((*(char *)(param_2 * 0x1c + 0x3146) == '\x01') ||
     (*(char *)(param_2 * 0x1c + 0x3146) == '\x04')) {
    if (*(char *)(param_2 * 0x1c + 0x314c) == '\0') {
      iStack_6a = 1;
    }
    iVar15 = param_2 * 0x1c;
    if ((*(char *)(iVar15 + 0x314c) == '\v') &&
       (iVar15 = FUN_1000_856a(uStack_88,uStack_94,*(undefined1 *)(iVar15 + 0x314d),
                               *(undefined1 *)(iVar15 + 0x314e)), 0xc < iVar15)) {
      iStack_6a = 1;
    }
    iVar15 = FUN_OVL14_L0000__0072f9(0x181f,uStack_e6,iStack_38);
    if (2 < iVar15) {
      iStack_6a = 1;
    }
    iVar15 = FUN_OVL14_L0000__0072d6(0x181f,uStack_88,uStack_94,uStack_e6,0);
    if (-1 < iVar15) {
      iStack_6a = 0;
    }
    if (*(char *)(param_2 * 0x1c + 0x315b) == '\x15') {
      iStack_6a = 0;
    }
    iVar15 = uStack_e6 * 0x10 + iStack_38;
    if ((*(char *)(iVar15 + -0x6b1a) == '\0') && (*(byte *)(iVar15 + -0x6a8e) < 8)) {
      iStack_6a = 1;
    }
    if ((*(char *)(param_2 * 0x1c + 0x3146) == '\x04') &&
       ((*(byte *)(iStack_38 + -0x6a0e) & 4) != 0)) {
      iStack_6a = 0;
    }
  }
  if (*(char *)(param_2 * 0x1c + 0x3146) == '\x05') {
    if (*(char *)(param_2 * 0x1c + 0x314b) == '2') {
      iStack_6a = 1;
    }
    if (uStack_2a == 0) {
      iStack_6a = 0;
    }
    if ((*(char *)(iStack_38 + uStack_e6 * 0x10 + -0x6b1a) == '\0') && (*(int *)0x538e % 0xf == 0))
    {
      iStack_6a = 1;
    }
    if ((0xc < iStack_2e) && (2 < iStack_74)) {
      iStack_6a = 1;
    }
    iVar15 = FUN_OVL14_L0000__0072d6(0x181f,uStack_88,uStack_94,uStack_e6,0);
    if ((-1 < iVar15) || (0x672 < *(int *)0x538a)) {
      iStack_6a = 0;
    }
  }
  if ((((iStack_34 == 0) &&
       (iVar15 = param_2 * 0x1c, 1 < *(byte *)((uint)*(byte *)(iVar15 + 0x3146) * 0xe + 0x5236))) &&
      (*(char *)(iVar15 + 0x314a) < '\0')) && ((iStack_2e < 9 && (iStack_2c == iStack_38)))) {
    *(undefined1 *)(iVar15 + 0x314a) = (char)uStack_62;
  }
  if ((iStack_6a == 0) || (iStack_14 == 0)) {
    iStack_6a = 0;
  }
  else {
    iStack_6a = 1;
  }
  if (((*(char *)(param_2 * 0x1c + 0x3146) == '\0') && (iStack_2e == 0)) &&
     (FUN_1000_8bd6(0x181f,uStack_62), (*(byte *)(*(int *)0x8542 + 0x1b) & 0x10) != 0)) {
    iStack_6a = 0;
  }
  if (((iStack_6a != 0) && (-1 < iStack_38)) &&
     (cVar9 = *(char *)(param_2 * 0x1c + 0x3146),
     *(char *)(iStack_38 + -0x5ec4) = *(char *)(iStack_38 + -0x5ec4) + '\x01',
     (int)(3 - (uint)(cVar9 == '\0')) < (int)(uint)*(byte *)(iStack_38 + -0x5ec4))) {
    iStack_6a = 0;
  }
  if ((*(byte *)0x5382 & 1) != 0) {
    iStack_6a = 0;
  }
  bVar17 = *(char *)(param_2 * 0x1c + 0x3146) != '\x12';
  if (*(char *)(param_2 * 0x1c + 0x3146) == '\x11') {
    uStack_b6 = ((uint)*(byte *)(uStack_e6 + 0x9414) +
                (uint)*(byte *)(uStack_e6 * 0x13 + (uint)*(byte *)(param_2 * 0x1c + 0x3146) +
                               -0x6db4) * -3) - (uint)*(byte *)(uStack_e6 * 0x13 + -0x6da4);
    bVar17 = (int)uStack_b6 < 4 && bVar17;
    iVar15 = FUN_1000_8b74(0x181f,uStack_88,uStack_94,uStack_e6);
    if (iVar15 != 0) {
      bVar17 = false;
    }
  }
  if (*(char *)(param_2 * 0x1c + 0x3146) == '\x10') {
    if ((*(byte *)0xa89b < 2) && (*(int *)0x9e52 < 7)) {
      bVar17 = false;
    }
    else {
      bVar17 = true;
    }
  }
  bVar7 = bVar17;
  if ((*(char *)(param_2 * 0x1c + 0x3146) == '\x12') &&
     (((param_2 & 1) != 0 || (*(char *)(uStack_e6 * 0x13 + -0x6da2) == '\x01')))) {
    bVar7 = true;
  }
  if (iStack_ae != 0) {
    FUN_1000_896e(0x1742,0,0,0);
  }
  if (iStack_34 == 0) {
    bVar10 = *(byte *)(param_2 * 0x1c + 0x3146);
    if ((((*(byte *)((uint)bVar10 * 0xe + 0x5236) < 2) || (bVar10 == 4)) || (bVar10 == 8)) ||
       (iStack_2e != 0)) goto LAB_OVL14_L0000__00277a;
    uVar11 = 0x181f;
    uVar13 = uStack_62;
    FUN_1000_8bd6(0x181f,uStack_62);
    if ((*(char *)(*(int *)0x8542 + 0x8e) < '\x01') && (*(char *)(param_2 * 0x1c + 0x314b) != 'A'))
    goto LAB_OVL14_L0000__00277a;
    uStack_58 = 0;
    uStack_a6 = param_2;
    iVar15 = FUN_1000_84de(uVar11,uVar13,0x181f);
    while (unaff_CS = 0x181f, -1 < iVar15) {
      bVar10 = *(byte *)(iVar15 * 0x1c + 0x3146);
      if ((((1 < *(byte *)((uint)bVar10 * 0xe + 0x5236)) && ((bVar10 < 0xd || (0x12 < bVar10)))) &&
          (*(char *)(iVar15 * 0x1c + 0x3146) != '\x04')) &&
         (*(char *)(iVar15 * 0x1c + 0x3146) != '\b')) {
        uStack_58 = uStack_58 + 1;
      }
      iVar15 = FUN_1000_84d4(uVar13,0x181f);
    }
    param_2 = uStack_a6;
    if (*(char *)(uStack_a6 * 0x1c + 0x314b) == 'A') goto LAB_OVL14_L0000__005899;
    if (1 < (int)uStack_58) {
      iStack_8e = 1;
      goto LAB_OVL14_L0000__004d2e;
    }
LAB_OVL14_L0000__005888:
    unaff_CS = 0x181f;
    *(char *)(*(int *)0x8542 + 0x8e) = *(char *)(*(int *)0x8542 + 0x8e) + -1;
    *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x47;
    goto LAB_OVL14_L0000__005899;
  }
LAB_OVL14_L0000__00277a:
  if (iStack_ae != 0) {
    FUN_1000_896e((undefined1 *)&LAB_OVL14_L0000__001746,0,0,0);
  }
  unaff_CS = 0x181f;
  if (((((uStack_2a == 0) && (iStack_6 == 0)) && (iStack_34 == 0)) &&
      ((*(char *)(param_2 * 0x1c + 0x3146) == '\x02' ||
       (1 < *(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236))))) &&
     (iStack_2c == iStack_38)) {
    if (iStack_2e == 0) {
      *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x56;
      goto LAB_OVL14_L0000__005899;
    }
    uVar19 = 0x181f;
    uVar13 = uStack_62;
    FUN_1000_8bd6(0x181f,uStack_62);
    uVar18 = (uint)*(byte *)(*(int *)0x8542 + 1);
    goto LAB_OVL14_L0000__0027f5;
  }
  if (iStack_ae != 0) {
    FUN_1000_896e(0x174a,0,0,0);
  }
  if (((*(char *)(param_2 * 0x1c + 0x3146) == '\x05') && (iStack_a0 == 1)) &&
     (((*(byte *)(*(int *)0x8d4a + 3) & 8) == 0 && ((iStack_54 < 0x19 && (iStack_3e == 0)))))) {
    unaff_CS = 0x1a1f;
    uStack_76 = func_0x0001a78c(0x181f);
    *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x4c;
  }
  else {
    if (iStack_ae != 0) {
      FUN_1000_896e(0x174e,0,0,0);
    }
    uVar13 = param_2;
    iVar15 = FUN_1000_8d18(0x181f,param_2);
    if (iVar15 == 0) {
LAB_OVL14_L0000__002912:
      uVar18 = uStack_94;
      iStack_42 = FUN_OVL14_L0000__007344(0x181f,uStack_e6,uStack_88,uStack_94,6);
      if (iStack_6a != 0) {
        uVar18 = uStack_94;
        FUN_OVL14_L0000__0072ef(0x181f,uStack_e6,6,uStack_88,uStack_94,0);
      }
      do {
        if (iStack_ae != 0) {
          uVar18 = 0;
          FUN_1000_896e(0x1752,0,0,0);
        }
        if (iStack_6a == 0) {
LAB_OVL14_L0000__0029ad:
          if (iStack_6a != 0) {
            uStack_76 = 8;
            iStack_e2 = -999;
            uStack_c = 0;
            sStack_de = 3;
            if (*(int *)(iStack_38 * 2 + -0x6ba2) < 9) {
              sStack_de = 0;
            }
            else if (*(int *)(iStack_38 * 2 + -0x6ba2) < 0x19) {
              sStack_de = 1;
            }
            else if (*(int *)(iStack_38 * 2 + -0x6ba2) < 0x31) {
              sStack_de = 2;
            }
            iStack_ca = 3;
            if (0x1f < iStack_12) {
              iStack_ca = 2;
            }
            if (0x3f < iStack_12) {
              iStack_ca = 1;
            }
            for (uStack_1c = uStack_94 - iStack_ca; unaff_CS = 0x181f,
                (int)uStack_1c <= (int)(uStack_94 + iStack_ca); uStack_1c = uStack_1c + 1) {
              for (uStack_18 = uStack_88 - iStack_ca; (int)uStack_18 <= (int)(uStack_88 + iStack_ca)
                  ; uStack_18 = uStack_18 + 1) {
                uVar18 = uStack_18;
                iVar15 = FUN_1000_84f2(0x181f,uStack_18,uStack_1c);
                if ((iVar15 != 0) &&
                   (((uVar18 = uStack_1c, uStack_5c = FUN_1000_88c2(0x181f,uStack_18,uStack_1c),
                     (int)uStack_5c < 0 || (uStack_5c == uStack_e6)) &&
                    (iVar15 = FUN_1000_8912(0x181f,uStack_18,uStack_1c), iVar15 == iStack_38)))) {
                  uStack_4c = FUN_1000_893a(0x181f,uStack_18,uStack_1c);
                  uStack_4c = uStack_4c & 0xf;
                  uStack_28 = uStack_4c * 4;
                  if ((uStack_18 == 0) && (uStack_1c == 0)) {
                    uStack_28 = uStack_28 + 0x10;
                  }
                  iVar15 = func_0x0001897c(0x181f,uStack_18,uStack_1c);
                  if (iVar15 != 0x1b) {
                    uVar18 = 0x2af9;
                    uVar13 = uStack_18;
                    iStack_20 = FUN_1000_8f02(0x181f,uStack_18,uStack_1c);
                    if (iStack_20 != 0) {
                      uVar13 = *(uint *)0x8dbc;
                      uVar18 = 0x181f;
                      cVar9 = FUN_1000_88a4(0x181f,*(undefined2 *)0x8dba,uVar13);
                      if (cVar9 != '\x01') {
                        iStack_20 = 0;
                      }
                    }
                    if (iStack_20 == 0) {
                      uStack_4c = 0;
                      if (uStack_50 == 8) {
                        uStack_28 = 0;
                      }
                    }
                    else {
                      uVar18 = 0xffff;
                      FUN_1000_8804(0x181f,uStack_18,uStack_1c,0xffff,iStack_38);
                      if (-1 < *(int *)0x8dc6) {
                        if (*(int *)0x8db8 < 2) goto LAB_OVL14_L0000__002a59;
                        if (*(char *)(*(int *)0x8542 + 0x1a) == (sbyte)uStack_e6) {
                          if (*(int *)0x8db8 == 2) goto LAB_OVL14_L0000__002a59;
                          iStack_a2 = 9;
                          iStack_32 = *(int *)0x8db8;
                          if (iStack_32 < 9) {
                            iVar16 = iStack_32 + -9;
                            iVar15 = -(iStack_32 + -9);
                            goto LAB_OVL14_L0000__002baf;
                          }
                        }
                        else {
                          if (*(int *)0x8db8 == 2) {
                            uStack_28 = uStack_28 + -0x14;
                          }
                          iStack_a2 = 7;
                          if (*(char *)(iStack_38 + uStack_e6 * 0x10 + -0x6b1a) == '\0') {
                            iStack_a2 = 5;
                          }
                          iStack_32 = *(int *)0x8db8;
                          if (iStack_32 < iStack_a2) {
                            iVar16 = iStack_a2 - iStack_32;
                            iVar15 = iStack_32 - iStack_a2;
LAB_OVL14_L0000__002baf:
                            uStack_28 = uStack_28 + iVar15 * iVar16;
                          }
                        }
                      }
                      uVar18 = uVar14;
                      FUN_1000_8bd6(0x181f,uVar14);
                      iStack_a = 1;
                      uStack_a6 = param_2;
                      for (iStack_70 = 0; (uVar19 = uStack_a6, iStack_a != 0 && (iStack_70 < 9));
                          iStack_70 = iStack_70 + 1) {
                        uStack_1e = (int)*(char *)(iStack_70 + 0xbe) + uStack_1c;
                        uStack_e = (int)*(char *)(iStack_70 + 0xb4) + uStack_18;
                        uVar11 = 0x181f;
                        uVar18 = 0x2c03;
                        uVar19 = FUN_1000_89d0(0x181f);
                        while (-1 < (int)uVar19) {
                          if ((*(char *)(uVar19 * 0x1c + 0x314c) == '\a') && (uStack_a6 != uVar19))
                          {
                            iStack_a = 0;
                          }
                          uVar13 = 0x181f;
                          uVar18 = 0x2c24;
                          uVar19 = FUN_1000_84d4(uVar11,0x181f);
                        }
                      }
                      param_2 = uStack_a6;
                      if (iStack_a != 0) {
                        FUN_1000_8f74(uStack_18,uStack_1c,0xffff,0xffff);
                        if (-1 < *(int *)0x8d4c) {
                          iStack_60 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8d4a,
                                                    ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
                          iStack_32 = *(int *)0x8db8;
                          if (*(char *)(iStack_60 + uStack_e6 * 0x10 + -0x6b1a) == '\0') {
                            iStack_32 = iStack_32 + 1;
                          }
                          if (iStack_32 < 6) {
                            uVar11 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,uStack_e6);
                            iVar15 = FUN_1000_8c50(0x181f,uVar11);
                            iStack_6c = ((uint)*(byte *)(*(int *)0x8d4e + 2) + iVar15 + 3) * 2;
                            if (iStack_60 != iStack_38) {
                              iStack_6c = iStack_6c >> 1;
                            }
                            iStack_d4 = iStack_6c >> 1;
                            if (iStack_32 < 5) {
                              iStack_d4 = iStack_d4 + iStack_6c;
                            }
                            if (iStack_32 < 4) {
                              iStack_d4 = iStack_d4 + iStack_6c * 2;
                            }
                            if (iStack_32 < 3) {
                              iStack_d4 = iStack_d4 + iStack_6c * 4;
                            }
                            if (iStack_32 < 2) {
                              iStack_d4 = iStack_d4 + iStack_6c * 8;
                            }
                            if ((*(byte *)(*(int *)0x8d4a + 3) & 4) != 0) {
                              iStack_d4 = iStack_d4 << 1;
                            }
                            if (uStack_e6 == 1) {
                              iStack_d4 = iStack_d4 >> 1;
                            }
                            iVar15 = FUN_1000_89a4(0x181f,uStack_e6,0x10);
                            if (iVar15 != 0) {
                              iStack_d4 = iStack_d4 >> 1;
                            }
                            if (uStack_e6 == 2) {
                              iStack_d4 = iStack_d4 >> 2;
                            }
                            if (0x28 < iStack_12) {
                              iStack_d4 = iStack_d4 >> 1;
                            }
                            iStack_d4 = iStack_d4 -
                                        (uint)*(byte *)(iStack_60 + uStack_e6 * 0x10 + -0x6a8e);
                            if (iStack_d4 < 0) {
                              iStack_d4 = 0;
                            }
                            uStack_28 = uStack_28 - iStack_d4;
                          }
                        }
                        uVar18 = 0x2d7d;
                        uVar13 = uStack_ac;
                        FUN_1000_8c3c(0x181f,uStack_ac);
                        if ((iStack_6a != 0) && (3 < (int)uStack_4c)) {
                          if (*(char *)(iStack_38 + uStack_e6 * 0x10 + -0x6b1a) == '\0') {
                            uStack_28 = uStack_28 + ((int)uStack_28 >> 1);
                          }
                          if (*(char *)(uVar19 * 0x1c + 0x3146) == '\0') {
                            uStack_28 = uStack_28 << 1;
                          }
                          uStack_28 = uStack_28 + (iStack_12 >> sStack_de);
                          if (iStack_42 != 0) {
                            uStack_28 = uStack_28 + 0x10;
                          }
                        }
                        if (iStack_e2 <= (int)uStack_28) {
                          iStack_e2 = uStack_28;
                          uStack_c = uStack_4c;
                          uStack_4e = uStack_18;
                          uStack_5e = uStack_1c;
                        }
                      }
                    }
                  }
                }
LAB_OVL14_L0000__002a59:
              }
            }
            if (0 < (int)uStack_c) {
              if (iStack_6a != 0) {
                uVar19 = uStack_5e;
                if ((uStack_4e != uStack_88) || (uStack_5e != uStack_94))
                goto LAB_OVL14_L0000__0027f5;
                *(undefined1 *)(param_2 * 0x1c + 0x314c) = 7;
                goto LAB_OVL14_L0000__005a78;
              }
              uVar18 = 6;
              FUN_OVL14_L0000__0072f4
                        (0x181f,uStack_e6,(int)*(char *)(uStack_76 + 0xb4) + uStack_88,
                         (int)*(char *)(uStack_76 + 0xbe) + uStack_94,6,2);
            }
          }
        }
        else {
          if (*(char *)(param_2 * 0x1c + 0x3155) == '\0') {
            iVar15 = param_2 * 0x1c;
            *(undefined1 *)(iVar15 + 0x3156) = 0xff;
            if ((iStack_14 != 0) && (*(byte *)(iVar15 + 0x3154) < 0x7f)) {
              *(char *)(iVar15 + 0x3154) = *(char *)(iVar15 + 0x3154) + '\x01';
            }
            iStack_12 = (uint)*(byte *)(iStack_38 + -0x6168) * 8 +
                        (uint)*(byte *)(param_2 * 0x1c + 0x3154);
            goto LAB_OVL14_L0000__0029ad;
          }
          pcVar1 = (char *)(param_2 * 0x1c + 0x3155);
          *pcVar1 = *pcVar1 + -1;
        }
        if (iStack_ae != 0) {
          uVar18 = 0;
          FUN_1000_896e(0x1756,0,0,0);
        }
        if ((*(char *)(param_2 * 0x1c + 0x3146) != '\0') || (iStack_6a != 0))
        goto LAB_OVL14_L0000__00304c;
        uStack_24 = 0xffff;
        iStack_e2 = 9999;
        for (uStack_8c = 0; (int)uStack_8c < *(int *)0x539e; uStack_8c = uStack_8c + 1) {
          uVar18 = 0x181f;
          FUN_1000_8bd6(0x181f,uStack_8c);
          puVar6 = (undefined1 *)*(undefined2 *)0x8542;
          if (puVar6[0x1a] == (sbyte)uStack_e6) {
            uVar18 = (uint)(byte)puVar6[1];
            iVar15 = FUN_1000_8912(0x181f,*puVar6,uVar18);
            if ((iVar15 == iStack_38) &&
               (((*(byte *)(*(int *)0x8542 + 0x1b) & 0x10) != 0 ||
                (*(char *)(param_2 * 0x1c + 0x315b) == '\x1b')))) {
              iStack_a4 = FUN_1000_8e6c(0x181f);
              if (0x10 < iStack_a4) {
                iStack_a4 = 0x10;
              }
              iVar15 = FUN_1000_856a(uStack_88,uStack_94,*(undefined1 *)*(undefined2 *)0x8542,
                                     ((undefined1 *)*(undefined2 *)0x8542)[1]);
              uStack_28 = iVar15 >> 1;
              uStack_58 = -(*(char *)(*(int *)0x8542 + 0x1f) - iStack_a4);
              if (0 < (int)uStack_58) {
                uStack_28 = uStack_58 * uStack_28;
              }
              if ((char)iStack_a4 <= *(char *)(*(int *)0x8542 + 0x1f)) {
                uStack_28 = uStack_28 << 1;
              }
              uVar11 = FUN_1000_89d0(0x181f,2);
              iVar15 = FUN_1000_8aac(0x181f,uVar11);
              if ((iVar15 + *(char *)(*(int *)0x8542 + 0x1f) < iStack_a4 + 2) &&
                 ((int)uStack_28 < iStack_e2)) {
                iStack_e2 = uStack_28;
                uStack_24 = uStack_8c;
              }
            }
          }
        }
        if (-1 < (int)uStack_24) {
          uVar19 = uStack_24;
          FUN_1000_8bd6(0x181f,uStack_24);
          pcVar5 = (char *)*(undefined2 *)0x8542;
          if ((*(char *)(param_2 * 0x1c + 0x3144) == *pcVar5) &&
             (*(char *)(param_2 * 0x1c + 0x3145) == pcVar5[1])) {
            uVar14 = param_2;
            FUN_1000_8b24(0x181f,param_2);
            FUN_1000_9b94(uStack_24,param_2,uVar14,uVar19,uVar13);
            return 1;
          }
          uVar18 = (uint)(byte)pcVar5[1];
          goto LAB_OVL14_L0000__0027f5;
        }
        if (iStack_2e == 0) {
          iVar15 = param_2 * 0x1c;
          *(undefined1 *)(iVar15 + 0x314b) = 0x3d;
          *(undefined1 *)(iVar15 + 0x3146) = 2;
          *(undefined1 *)(iVar15 + 0x3159) = 0x14;
          unaff_CS = 0x181f;
          FUN_1000_8b24(0x181f,param_2);
          goto LAB_OVL14_L0000__005a78;
        }
        if ((*(byte *)0x5382 & 1) != 0) goto LAB_OVL14_L0000__00304c;
        iStack_6a = 1;
      } while( true );
    }
    iVar15 = param_2 * 0x1c;
    bVar10 = *(byte *)(iVar15 + 0x3146);
    if ((((1 < *(byte *)((uint)bVar10 * 0xe + 0x5236)) || (bVar10 == 5)) || (bVar10 == 3)) ||
       ((((*(char *)(iVar15 + 0x315b) != '\x1c' && (*(char *)(iVar15 + 0x315b) != '\x19')) ||
         ((iStack_a0 != 1 || (((*(byte *)(*(int *)0x8d4a + 3) & 2) != 0 || (0x18 < iStack_54))))))
        || (0x3f < iStack_3e)))) goto LAB_OVL14_L0000__002912;
    unaff_CS = 0x1a1f;
    uStack_76 = func_0x0001a78c(0x181f);
    *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x4c;
  }
  goto LAB_OVL14_L0000__00589e;
LAB_OVL14_L0000__00304c:
  if (iStack_ae != 0) {
    FUN_1000_896e(0x175a,0,0,0);
  }
  bVar10 = *(byte *)(param_2 * 0x1c + 0x3146);
  if (((*(char *)((uint)bVar10 * 0xe + 0x5237) == '\0') || (iStack_2e != 0)) ||
     (((bVar10 < 0xd || (0x12 < bVar10)) && (*(char *)(param_2 * 0x1c + 0x314a) != (char)uStack_62))
     )) {
LAB_OVL14_L0000__003558:
    if (iStack_ae != 0) {
      FUN_1000_896e(0x175e,0,0,0);
    }
    unaff_CS = 0x181f;
    if (iStack_34 != 0) {
      uStack_b6 = (uint)*(byte *)(param_2 * 0x1c + 0x3150);
      uVar11 = 0xd1d;
      FUN_0000_df7e(acStack_c8,0,0x10);
      iStack_44 = -1;
      for (uStack_8c = 0; (int)uStack_8c < (int)uStack_b6; uStack_8c = uStack_8c + 1) {
        iStack_7a = FUN_1000_8dd6(uVar11,param_2,uStack_8c);
        if ((0xc < iStack_7a) || (iStack_7a == 8)) {
          if (iStack_44 < iStack_7a) {
            iStack_44 = iStack_7a;
          }
          cVar9 = FUN_1000_8e58(0x181f,param_2,uStack_8c);
          acStack_c8[iStack_7a] = acStack_c8[iStack_7a] + cVar9;
        }
        uVar11 = 0x181f;
      }
      if (iStack_ae != 0) {
        uVar11 = 0x181f;
        FUN_1000_896e(0x1762,0,0,0);
      }
      uVar19 = param_2;
      FUN_1000_8b10(uVar11,param_2);
      iStack_a8 = FUN_1000_8aac(0x181f,param_2,2);
      iStack_a8 = iStack_a8 + -1;
      iStack_4a = FUN_1000_8aac(0x181f,param_2,3);
      iStack_48 = FUN_1000_8aac(0x181f,param_2,4);
      iStack_46 = FUN_1000_8aac(0x181f,param_2,6);
      iStack_16 = FUN_1000_8aac(0x181f,param_2,5);
      iStack_82 = (-iStack_48 - (iStack_16 - iStack_a8)) - iStack_4a;
      iStack_b4 = iStack_4a;
      if ((*(byte *)0x5382 & 1) != 0) {
        iVar15 = FUN_1000_8aac(0x181f,param_2,0xc);
        iStack_48 = iStack_48 + iVar15;
      }
      FUN_1000_8b38(0x181f,param_2,uStack_88,uStack_94);
      if (iStack_ae != 0) {
        FUN_1000_896e(0x1764,0,0,0);
      }
      iStack_66 = FUN_OVL14_L0000__007344(0x181f,uStack_e6,uStack_88,uStack_94,7);
      iVar15 = FUN_OVL14_L0000__007344(0x181f,uStack_e6,uStack_88,uStack_94,1);
      iStack_68 = thunk_FUN_1000_a654(param_2);
      if (-1 < iStack_68) {
        iVar16 = FUN_OVL14_L0000__007326(0x181f,uStack_e6,iStack_68,0xffff);
        iVar12 = FUN_OVL14_L0000__0072e0(0x181f,uStack_e6);
        uStack_58 = iVar16 + iVar12;
        if ((int)uStack_58 < 1) {
          if (iVar15 == 0) {
            iStack_82 = iStack_82 + iStack_4a;
            iStack_4a = 0;
          }
        }
        else {
          iStack_80 = iStack_82;
          iStack_4a = iStack_4a + iStack_82;
          iStack_82 = 0;
        }
      }
      if (iStack_ae != 0) {
        FUN_1000_896e(0x1766,iStack_4a,iStack_48,iStack_a8);
      }
      if (iStack_a8 != 0) {
        uStack_9c = 0;
        if (((iStack_4a != 0) || (iStack_48 != 0)) || (iStack_16 != 0)) {
          if (iStack_ae != 0) {
            FUN_1000_896e(0x1768,0,0,0);
          }
          for (uStack_50 = 0; (int)uStack_50 < 8; uStack_50 = uStack_50 + 1) {
            uStack_1c = (int)*(char *)(uStack_50 + 0xbe) + uStack_94;
            uStack_18 = (int)*(char *)(uStack_50 + 0xb4) + uStack_88;
            iVar16 = FUN_1000_84f2(0x181f,uStack_18,uStack_1c);
            if (((iVar16 != 0) && (iVar16 = FUN_1000_8958(0x181f,uStack_18,uStack_1c), iVar16 == 0))
               && ((uStack_5c = FUN_1000_88c2(0x181f,uStack_18,uStack_1c), (int)uStack_5c < 0 ||
                   (uStack_5c == uStack_e6)))) {
              uStack_9c = 0;
              iStack_60 = FUN_1000_8912(0x181f,uStack_18,uStack_1c);
              if (((((undefined1 *)&LAB_0000_9870)[uStack_e6 * 0x10 + iStack_60] != '\0') &&
                  (1 < (int)uStack_1c)) && ((int)uStack_1c <= *(int *)0x853c + -3)) {
                iVar16 = param_2 * 0x1c;
                if ((*(char *)(iVar16 + 0x314c) == '\v') &&
                   (iVar16 = FUN_1000_8912(0x181f,*(undefined1 *)(iVar16 + 0x314d),
                                           *(undefined1 *)(iVar16 + 0x314e)), iVar16 == iStack_60))
                {
                  uStack_9c = 0xffff;
                }
                if (((iStack_16 != 0) &&
                    (iVar16 = uStack_e6 * 0x10 + iStack_60,
                    ((undefined1 *)&LAB_0000_9870)[iVar16] != '\0')) &&
                   (((*(char *)(iVar16 + -0x6b1a) == '\0' &&
                     (10 < *(int *)(iStack_60 * 2 + -0x7a38))) ||
                    ((int)(uint)*(byte *)(iStack_60 + uStack_e6 * 0x10 + -0x6b5a) <
                     *(int *)(iStack_60 * 2 + -0x7a38) >> 3)))) {
                  uStack_9c = uStack_9c | 0x20;
                }
                if (iStack_4a != 0) {
                  iVar16 = FUN_OVL14_L0000__0072f9(0x181f,uStack_e6,iStack_60);
                  if (0 < iVar16) {
                    uStack_9c = uStack_9c | 0x40;
                  }
                  if (*(char *)(iStack_60 + uStack_e6 * 0x10 + -0x6b5a) == '\0') {
                    uStack_9c = uStack_9c | 0x40;
                  }
                  iStack_98 = 1;
                  if ((-1 < (int)uStack_62) && (iStack_2c == iStack_60)) {
                    iVar16 = *(byte *)(iStack_60 + uStack_e6 * 0x10 + -0x6b1a) - 8;
                    if (-iStack_2e != iVar16 && iStack_2e <= -iVar16) {
                      uStack_9c = uStack_9c & 0xffbf;
                    }
                    if (0xb < iStack_2e) {
                      iStack_98 = 0;
                    }
                  }
                  if ((((iStack_98 != 0) && (uStack_58 = *(int *)0x538e >> 4, *(int *)0x9650 != 0))
                      && (iVar16 = uStack_e6 * 0x10 + iStack_60,
                         (int)uStack_58 <
                         (int)((uint)*(byte *)(iVar16 + -0x6b1a) * 4 +
                              (uint)*(byte *)(iVar16 + -0x6b5a)))) &&
                     (*(int *)(uStack_e6 * 2 + 0x1734) < 0x14)) {
                    uStack_9c = uStack_9c & 0xffbf;
                  }
                  if ((iStack_80 != 0) && (1 < *(byte *)(iStack_60 + -0x5ec4))) {
                    uStack_9c = uStack_9c & 0xffbf;
                  }
                  if (iStack_66 != 0) {
                    uStack_9c = uStack_9c | 0x40;
                  }
                  if (iVar15 != 0) {
                    uStack_9c = uStack_9c | 0x40;
                  }
                  if ((*(uint *)0x173e & 1 << ((byte)iStack_60 & 0x1f)) != 0) {
                    uStack_9c = uStack_9c | 0x40;
                  }
                }
                if (((iStack_48 != 0) && ((*(byte *)0x5382 & 1) != 0)) &&
                   (*(char *)(iStack_60 + *(int *)0x5398 * 0x10 + -0x6b1a) != '\0')) {
                  uStack_9c = uStack_9c | 0x10;
                }
                if ((iStack_48 != 0) && ((*(byte *)0x5382 & 1) == 0)) {
                  if (((undefined1 *)&LAB_0000_9870)[iStack_60 + uStack_e6 * 0x10] == '\x04') {
                    uStack_9c = uStack_9c | 0x10;
                  }
                  if ((((iStack_46 == 0) && (uStack_58 = *(int *)0x538e >> 4, *(int *)0x9650 != 0))
                      && (iVar16 = uStack_e6 * 0x10 + iStack_60,
                         (int)uStack_58 <
                         (int)((uint)*(byte *)(iVar16 + -0x6b1a) * 4 +
                              (uint)*(byte *)(iVar16 + -0x6b5a)))) &&
                     (*(int *)(uStack_e6 * 2 + 0x1734) < 0x14)) {
                    uStack_9c = uStack_9c & 0xffef;
                  }
                  if (((*(char *)(iStack_60 + uStack_e6 * 0x10 + -0x6b1a) == '\0') &&
                      ((*(byte *)(iStack_60 + -0x6a0e) & 4) != 0)) && (iStack_74 < 7)) {
                    uStack_9c = uStack_9c | 0x10;
                  }
                  if ((*(byte *)(iStack_60 + -0x6a0e) & 8) != 0) {
                    uStack_9c = uStack_9c | 0x10;
                  }
                }
                if (iStack_48 != 0) {
                  iVar16 = FUN_OVL14_L0000__007344(0x181f,uStack_e6,uStack_88,uStack_94,7);
                  if (iVar16 != 0) {
                    uStack_9c = uStack_9c | 0x10;
                  }
                  iVar16 = FUN_OVL14_L0000__007344(0x181f,uStack_e6,uStack_88,uStack_94,1);
                  if (iVar16 != 0) {
                    uStack_9c = uStack_9c | 0x10;
                  }
                  if ((*(uint *)0x173c & 1 << ((byte)iStack_60 & 0x1f)) != 0) {
                    uStack_9c = uStack_9c | 0x10;
                  }
                }
              }
            }
          }
        }
        if (iStack_ae != 0) {
          FUN_1000_896e((undefined1 *)&LAB_OVL14_L0000__00176a,uStack_9c,*(undefined2 *)0x1740,0);
        }
        if (*(int *)0x1740 != 0) {
          uStack_9c = 0;
        }
        if (uStack_9c != 0) {
          if ((iVar15 != 0) || (iStack_66 != 0)) {
            iStack_6 = 0;
          }
          uStack_a6 = param_2;
          iStack_78 = 0;
          do {
            if (iStack_ae != 0) {
              FUN_1000_896e(0x176c,0,0,0);
            }
            iStack_26 = 0;
            uStack_76 = 0xffff;
            uVar18 = 0x181f;
            iVar15 = FUN_1000_84de(0x181f);
            while (((uStack_76 != 8 && (iStack_26 == 0)) && (-1 < iVar15))) {
              if ((*(byte *)(iVar15 * 0x1c + 0x3146) < 0xd) ||
                 (0x12 < *(byte *)(iVar15 * 0x1c + 0x3146))) {
                iVar16 = iVar15 * 0x1c;
                pbVar21 = (byte *)(iVar16 + 0x3146);
                uVar18 = (uint)(*(char *)(iVar16 + 0x3146) == '\v');
                uStack_76 = FUN_OVL14_L0000__0072ea
                                      (0x181f,uStack_e6,uStack_88,uStack_94,uStack_9c & 0x40,uVar18)
                ;
                if (((*(byte *)((uint)*pbVar21 * 0xe + 0x523d) & (byte)uStack_9c) != 0) &&
                   (uStack_76 != 8)) {
                  iStack_78 = 1;
                  *(undefined1 *)(iVar16 + 0x3149) = 0;
                  func_0x0001a340(0x181f,iVar15,uStack_76);
                  uVar18 = 0x1a1f;
                  FUN_1000_8b24(0x1a1f,iVar15);
                  if ((*(char *)(iVar16 + 0x3144) != (char)uStack_88) ||
                     (*(char *)(iVar16 + 0x3145) != (char)uStack_94)) {
                    iStack_26 = 1;
                  }
                }
              }
              uVar19 = 0x181f;
              iVar15 = FUN_1000_84d4(uVar18,0x181f);
            }
          } while (iStack_26 != 0);
          param_2 = uStack_a6;
          if (iStack_78 != 0) {
            FUN_1000_8b24(0x181f,uStack_a6);
          }
        }
        if (iStack_ae != 0) {
          FUN_1000_896e(0x176e,0,0,0);
        }
        if ((iStack_6 == 0) &&
           ((iStack_82 != 0 ||
            (((iStack_4a != iStack_a8 || (0x18 < *(int *)(uStack_e6 * 2 + 0x1734))) &&
             (uStack_9c == 0)))))) {
          uStack_4e = (uint)*(byte *)(param_2 * 0x1c + 0x3144);
          uStack_5e = (uint)*(byte *)(param_2 * 0x1c + 0x3145);
          iStack_e2 = -9999;
          for (uStack_8c = 0; (int)uStack_8c < *(int *)0x539e; uStack_8c = uStack_8c + 1) {
            FUN_1000_8bd6(0x181f,uStack_8c);
            pcVar5 = (char *)*(undefined2 *)0x8542;
            if (((pcVar5[0x1a] == (sbyte)uStack_e6) &&
                ((*(char *)(param_2 * 0x1c + 0x3144) != *pcVar5 ||
                 (*(char *)(param_2 * 0x1c + 0x3145) != pcVar5[1])))) &&
               ((iStack_60 = FUN_1000_8912(0x181f,*pcVar5,pcVar5[1]), iStack_b4 == 0 ||
                (((undefined1 *)&LAB_0000_9870)[iStack_60 + uStack_e6 * 0x10] != '\0')))) {
              iStack_a4 = FUN_1000_8e6c(0x181f);
              if (0xc < iStack_a4) {
                iStack_a4 = 0x10;
              }
              iStack_b2 = 1;
              if (iStack_48 == 0) {
                iVar15 = FUN_1000_86c4(0x181f,0,8);
                cVar9 = *(char *)(*(int *)0x8542 + 0x1f);
                if ('\x10' < cVar9) {
                  cVar9 = '\x10';
                }
                uStack_28 = iVar15 + ((0x11 - cVar9) * (0x11 - cVar9) + 2) * 4 +
                            (*(char *)(*(int *)0x8542 + 0x1f) - iStack_a4) * -2;
                if (*(char *)(iStack_60 + *(int *)0x5398 * 0x10 + -0x6b1a) != '\0') {
                  uStack_28 = uStack_28 + 0x14;
                }
                uStack_28 = uStack_28 +
                            (-(uint)((*(byte *)(*(int *)0x8542 + 0x1b) & 0x10) == 0) & 0xffce) +
                            0x19;
              }
              else {
                uStack_28 = 0;
                if (((undefined1 *)&LAB_0000_9870)[iStack_60 + uStack_e6 * 0x10] == '\0')
                goto LAB_OVL14_L0000__003e1f;
                iVar15 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                                       ((undefined1 *)*(undefined2 *)0x8542)[1]);
                uStack_58 = (*(byte *)(iVar15 + -0x6a0e) & 7) * 8;
                uStack_28 = uStack_28 + uStack_58;
                if ((*(char *)0xa89c != '\0') && (1 < iStack_48)) {
                  uStack_28 = uStack_28 + (uint)*(byte *)0xa89c * iStack_48 * -8;
                }
                if (*(char *)(iStack_60 + *(int *)0x5398 * 0x10 + -0x6b1a) != '\0') {
                  uStack_28 = uStack_28 + 0x32;
                }
                if ((*(byte *)(*(int *)0x8542 + 0x1b) & 0x40) == 0) {
                  if ((*(int *)0x9650 < 2) || (0x13 < *(int *)(uStack_e6 * 2 + 0x1734))) {
                    if ((*(byte *)(*(int *)0x8542 + 0x1b) & 8) == 0) {
                      uStack_28 = uStack_28 + -0xf;
                    }
                    else {
                      uStack_28 = uStack_28 + 0x2d;
                    }
                  }
                  else {
                    uStack_28 = uStack_28 + -0x2d;
                  }
                }
                else {
                  uStack_28 = uStack_28 + 0x3c;
                }
              }
              iVar15 = *(int *)0x8542;
              uStack_28 = uStack_28 + (int)*(char *)(iVar15 + 0x8f);
              if ((*(byte *)(iVar15 + 0x1b) & 2) == 0) {
                if (((*(byte *)(iVar15 + 0x1b) & 1) != 0) &&
                   (*(byte *)(param_2 * 0x1c + 0x3146) < 0x11)) {
                  uStack_28 = uStack_28 +
                              (*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5235) -
                              10) * 2;
                }
              }
              else if (*(char *)(param_2 * 0x1c + 0x3146) != '\x11') {
                uStack_28 = uStack_28 + -0x32;
              }
              if ((*(byte *)(*(int *)0x8dc6 * 0xca + 0x5d62) & 0x40) == 0) {
                FUN_1000_8804(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                              ((undefined1 *)*(undefined2 *)0x8542)[1],uStack_e6,0xfffe);
              }
              if (-1 < *(int *)0x8dc6) {
                uStack_1c = (uint)((byte *)*(undefined2 *)0x8542)[1];
                uStack_18 = (uint)*(byte *)*(undefined2 *)0x8542;
                iVar15 = FUN_1000_856a(uStack_88,uStack_94,uStack_18,uStack_1c);
                uStack_28 = uStack_28 - ((iVar15 * iStack_b2 >> 1) + 1);
                if (iStack_e2 <= (int)uStack_28) {
                  uStack_4e = uStack_18;
                  uStack_5e = uStack_1c;
                  iStack_e2 = uStack_28;
                }
              }
            }
LAB_OVL14_L0000__003e1f:
          }
          uVar18 = uStack_5e;
          if ((int)(-(uint)(iStack_48 == 0) & 0xfc19) < iStack_e2) goto LAB_OVL14_L0000__0027f5;
        }
      }
      if (iStack_ae != 0) {
        FUN_1000_896e(6000,0,0,0);
      }
      if (((((((*(byte *)0x5382 & 1) == 0) || (*(char *)(param_2 * 0x1c + 0x3146) != '\x12')) ||
            (iStack_6 != 0)) || ((iStack_a8 != 0 || (*(int *)0x53de != 0)))) ||
          (*(char *)(uStack_e6 + 0x9456) != '\0')) ||
         (*(int *)0x53da + *(int *)0x53dc + *(int *)0x53e0 == 0)) {
        if (iStack_ae != 0) {
          FUN_1000_896e(0x1772,0,0,0);
        }
        if (((iStack_a8 != 0) || (-1 < iStack_44)) ||
           ((!bVar7 ||
            ((iStack_6 != 0 || (*(byte *)(uStack_e6 + 0x945a) <= *(byte *)(uStack_e6 + 0x9456)))))))
        {
          if (iStack_ae != 0) {
            FUN_1000_896e(0x1774,0,0,0);
          }
          unaff_CS = 0x181f;
          if ((uStack_b6 != 0) && (iStack_6 == 0)) {
            if (-1 < iStack_44) {
              uStack_24 = 0xffff;
              iStack_e2 = -1;
              for (uStack_50 = 0; unaff_CS = 0x181f, (int)uStack_50 < *(int *)0x539e;
                  uStack_50 = uStack_50 + 1) {
                FUN_1000_8bd6(0x181f,uStack_50);
                if ((((*(char *)(*(int *)0x8542 + 0x1a) == (sbyte)uStack_e6) &&
                     (((uStack_50 != uStack_62 || (iStack_2e != 0)) &&
                      ((*(byte *)(uStack_50 * 0xca + 0x5d62) & 0x40) != 0)))) &&
                    ((int)*(char *)(param_2 * 0x1c + 0x314a) != uStack_50)) &&
                   ((iStack_44 != 8 ||
                    (iVar15 = FUN_1000_8f2a(0x181f),
                    (int)cStack_c0 + *(int *)(*(int *)0x8542 + 0xaa) <= iVar15)))) {
                  iStack_60 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8542,
                                            ((undefined1 *)*(undefined2 *)0x8542)[1]);
                  iStack_22 = FUN_1000_8f2a(0x181f);
                  iStack_a = 1;
                  uStack_28 = 0;
                  for (uStack_b6 = 0; (int)uStack_b6 < 0x10; uStack_b6 = uStack_b6 + 1) {
                    if (acStack_c8[uStack_b6] != '\0') {
                      iVar15 = *(int *)0x8542;
                      if (((*(uint *)(iVar15 + 0x90) & 1 << ((byte)uStack_b6 & 0x1f)) != 0) &&
                         (99 < *(int *)(iVar15 + uStack_b6 * 2 + 0x9a))) {
                        iStack_a = 0;
                        break;
                      }
                      if (iStack_22 <=
                          (int)acStack_c8[uStack_b6] + *(int *)(iVar15 + uStack_b6 * 2 + 0x9a)) {
                        uStack_28 = uStack_28 +
                                    ((iStack_22 - *(int *)(iVar15 + uStack_b6 * 2 + 0x9a)) -
                                    (int)acStack_c8[uStack_b6]) *
                                    (uint)*(byte *)(uStack_b6 + uStack_e6 * 0x10 + -0x7b44) * 4;
                      }
                      iVar15 = *(int *)0x8542;
                      if (*(byte *)(iVar15 + 0x8d) == (byte)uStack_b6) {
                        uStack_28 = uStack_28 + (*(char *)(iVar15 + 0x8f) + 8) * 4;
                      }
                      uStack_28 = uStack_28 +
                                  (iStack_22 - *(int *)(iVar15 + uStack_b6 * 2 + 0x9a)) + -1;
                    }
                  }
                  if (iStack_a != 0) {
                    if (iStack_44 == 0xf) {
                      if (*(char *)(iStack_60 + *(int *)0x5398 * 0x10 + -0x6b1a) != '\0') {
                        uStack_28 = uStack_28 + 0x10;
                      }
                      for (iStack_70 = 0; iStack_70 < 0x14; iStack_70 = iStack_70 + 1) {
                        uStack_1e = (int)*(char *)(iStack_70 + 0xde) +
                                    (uint)((byte *)*(undefined2 *)0x8542)[1];
                        uStack_e = (int)*(char *)(iStack_70 + 200) +
                                   (uint)*(byte *)*(undefined2 *)0x8542;
                        iVar15 = FUN_1000_84f2(0x181f,uStack_e,uStack_1e);
                        if (iVar15 != 0) {
                          uStack_10 = FUN_1000_88c2(0x181f,uStack_e,uStack_1e);
                          if ((int)uStack_10 < 4) {
                            uStack_28 = uStack_28 + 0x18;
                          }
                          else {
                            uVar11 = FUN_1000_84fc(0x181f,uStack_10 - 4,uStack_e6);
                            iVar15 = FUN_1000_8c50(0x181f,uVar11);
                            uStack_28 = uStack_28 + iVar15 * 0x10;
                          }
                        }
                      }
                    }
                    if ((*(byte *)(*(int *)0x8542 + 0x1b) & 2) == 0) {
                      if (((*(byte *)(*(int *)0x8542 + 0x1b) & 1) != 0) &&
                         (*(byte *)(param_2 * 0x1c + 0x3146) < 0x10)) {
                        iVar15 = (*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5235)
                                 - 10) * 2;
                        goto LAB_OVL14_L0000__0041e2;
                      }
                    }
                    else if (*(byte *)(param_2 * 0x1c + 0x3146) < 0x10) {
                      iVar15 = (*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5235) -
                               10) * 8;
LAB_OVL14_L0000__0041e2:
                      uStack_28 = uStack_28 + iVar15;
                    }
                    iVar15 = FUN_1000_856a(uStack_88,uStack_94,*(undefined1 *)*(undefined2 *)0x8542,
                                           ((undefined1 *)*(undefined2 *)0x8542)[1]);
                    uStack_28 = (int)uStack_28 / ((iVar15 >> 2) + 1);
                    if (iStack_e2 <= (int)uStack_28) {
                      uStack_24 = uStack_50;
                      iStack_e2 = uStack_28;
                    }
                  }
                }
              }
              if (-1 < (int)uStack_24) {
                FUN_1000_8bd6(0x181f,uStack_24);
                uVar18 = (uint)*(byte *)(*(int *)0x8542 + 1);
                goto LAB_OVL14_L0000__0027f5;
              }
              while (*(char *)(param_2 * 0x1c + 0x3150) != '\0') {
                uStack_b6 = FUN_1000_8cdc(unaff_CS,param_2,0);
                unaff_CS = 0x191f;
                func_0x00019c1e(0x181f,uStack_b6,*(undefined2 *)0x8dc4);
                uVar18 = (uint)*(byte *)(uStack_b6 + uStack_e6 * 0x10 + -0x7b44) * *(int *)0x8dc4;
                iVar15 = *(int *)0x84fc;
                puVar2 = (uint *)(iVar15 + 0x2a);
                uVar19 = *puVar2;
                *puVar2 = *puVar2 + uVar18;
                *(int *)(iVar15 + 0x2c) =
                     *(int *)(iVar15 + 0x2c) + ((int)uVar18 >> 0xf) + (uint)CARRY2(uVar19,uVar18);
                iVar15 = iVar15 + uStack_b6 * 4;
                puVar2 = (uint *)(iVar15 + 0x7c);
                uVar19 = *puVar2;
                *puVar2 = *puVar2 + uVar18;
                *(int *)(iVar15 + 0x7e) =
                     *(int *)(iVar15 + 0x7e) + ((int)uVar18 >> 0xf) + (uint)CARRY2(uVar19,uVar18);
                uVar18 = *(uint *)0x8dc4;
                puVar2 = (uint *)(iVar15 + 0xbc);
                uVar19 = *puVar2;
                *puVar2 = *puVar2 + uVar18;
                *(int *)(iVar15 + 0xbe) =
                     *(int *)(iVar15 + 0xbe) + ((int)uVar18 >> 0xf) + (uint)CARRY2(uVar19,uVar18);
              }
              *(undefined1 *)(param_2 * 0x1c + 0x3150) = 0;
            }
            bVar10 = *(byte *)(param_2 * 0x1c + 0x3150);
            if ((*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5237) == bVar10) ||
               (1 < bVar10)) goto LAB_OVL14_L0000__003fa6;
          }
          if (iStack_ae != 0) {
            unaff_CS = 0x181f;
            FUN_1000_896e(0x1776,0,0,0);
          }
          goto LAB_OVL14_L0000__004393;
        }
      }
LAB_OVL14_L0000__003fa6:
      unaff_CS = 0x191f;
      FUN_1000_94da(param_2);
      goto LAB_OVL14_L0000__005a78;
    }
LAB_OVL14_L0000__004393:
    if (iStack_ae != 0) {
      unaff_CS = 0x181f;
      FUN_1000_896e(0x1778,0,0,0);
    }
    if ((((0xc < *(byte *)(param_2 * 0x1c + 0x3146)) && (*(byte *)(param_2 * 0x1c + 0x3146) < 0x13))
        && ((bVar17 || (bVar7)))) && ((iStack_a8 == 0 && (iStack_6 == 0)))) {
      uStack_24 = 0xffff;
      iStack_e2 = -1;
      uStack_50 = 0;
      do {
        if ((-1 < *(int *)(uStack_50 * 6 + -0x5f24)) &&
           ('\0' < (char)((undefined1 *)&LAB_0000_a0e0)[uStack_50 * 6])) {
          iVar15 = uStack_50 * 6;
          FUN_1000_8bd6(unaff_CS,*(undefined2 *)(iVar15 + -0x5f24));
          uStack_28 = *(int *)(iVar15 + -0x5f22);
          unaff_CS = 0x181f;
          if (((int)*(char *)(param_2 * 0x1c + 0x314a) != *(int *)0x8dc6) &&
             (((*(byte *)(*(int *)0x8542 + 0x1b) & 2) == 0 ||
              (0xf < *(byte *)(param_2 * 0x1c + 0x3146))))) {
            unaff_CS = 0x181f;
            iVar15 = FUN_1000_856a(*(undefined1 *)(param_2 * 0x1c + 0x3144),
                                   *(undefined1 *)(param_2 * 0x1c + 0x3145),
                                   *(undefined1 *)*(undefined2 *)0x8542,
                                   ((undefined1 *)*(undefined2 *)0x8542)[1]);
            uStack_28 = (int)uStack_28 / ((iVar15 >> 2) + 1);
            if ((iStack_e2 <= (int)uStack_28) &&
               ((bVar17 || (*(char *)(uStack_50 * 6 + -0x5f1f) != '\0')))) {
              uStack_24 = uStack_50;
              iStack_e2 = uStack_28;
            }
          }
        }
        uStack_50 = uStack_50 + 1;
      } while ((int)uStack_50 < 0x10);
      if ((int)uStack_24 < 0) goto LAB_OVL14_L0000__00457e;
      iVar15 = uStack_24 * 6;
      iStack_9a = ((int)(char)((undefined1 *)&LAB_0000_a0e0)[iVar15] -
                  (uint)*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5237)) +
                  (uint)*(byte *)(param_2 * 0x1c + 0x3150);
      if (iStack_9a < 0) {
        iStack_9a = 0;
      }
      *(int *)(iVar15 + -0x5f22) =
           (iStack_9a * *(int *)(iVar15 + -0x5f22)) /
           (int)(char)((undefined1 *)&LAB_0000_a0e0)[iVar15];
      ((undefined1 *)&LAB_0000_a0e0)[iVar15] = (undefined1)iStack_9a;
      uVar19 = *(uint *)(iVar15 + -0x5f24);
      FUN_1000_8bd6(unaff_CS,uVar19);
      if (iStack_9a == 0) {
        *(undefined2 *)(iVar15 + -0x5f24) = 0xffff;
      }
      if (iStack_ae != 0) {
        puVar6 = (undefined1 *)*(undefined2 *)0x8542;
        FUN_1000_896e(puVar6 + 2,*puVar6,puVar6[1],0);
      }
LAB_OVL14_L0000__004567:
      uVar18 = (uint)*(byte *)(*(int *)0x8542 + 1);
      goto LAB_OVL14_L0000__0027f5;
    }
LAB_OVL14_L0000__00457e:
    if (iStack_6 != 0) goto LAB_OVL14_L0000__005a78;
    iVar15 = param_2 * 0x1c;
    if ((((0xc < *(byte *)(iVar15 + 0x3146)) && (*(byte *)(iVar15 + 0x3146) < 0x13)) &&
        (iStack_a8 == 0)) &&
       (((iStack_44 < 0 && (bVar7)) &&
        (((*(byte *)(iVar15 + 0x3148) & 0x20) != 0 ||
         (((char)param_2 + *(char *)0x538e & 0x1fU) == 0)))))) goto LAB_OVL14_L0000__003fa6;
    if (iStack_ae != 0) {
      unaff_CS = 0x181f;
      FUN_1000_896e(0x177c,0,0,0);
    }
    if (*(char *)(param_2 * 0x1c + 0x3146) == '\f') {
      if (*(char *)(param_2 * 0x1c + 0x3158) == '\0') {
        if (iStack_2e == 0) {
          if (*(char *)(param_2 * 0x1c + 0x314a) < '\0') {
            *(char *)(param_2 * 0x1c + 0x314a) = (char)uStack_62;
          }
          if (*(char *)(param_2 * 0x1c + 0x314a) == (char)uStack_62) {
            *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x55;
            goto LAB_OVL14_L0000__005899;
          }
        }
        if (*(char *)(param_2 * 0x1c + 0x314a) < '\0') {
          if (iStack_2c != iStack_38) goto LAB_OVL14_L0000__0047b9;
          *(char *)(param_2 * 0x1c + 0x314a) = (char)uStack_62;
          uVar19 = uStack_62;
        }
        else {
          uVar19 = (int)*(char *)(param_2 * 0x1c + 0x314a);
        }
LAB_OVL14_L0000__004701:
        FUN_1000_8bd6(unaff_CS,uVar19);
        goto LAB_OVL14_L0000__004567;
      }
      uStack_24 = 0xffff;
      iStack_e2 = 9999;
      for (uStack_50 = 0; (int)uStack_50 < *(int *)0x539a; uStack_50 = uStack_50 + 1) {
        FUN_1000_8c3c(unaff_CS,uStack_50);
        iVar15 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8d4a,
                               ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
        if (iVar15 == iStack_38) {
          iStack_32 = FUN_1000_856a(uStack_88,uStack_94,*(undefined1 *)*(undefined2 *)0x8d4a,
                                    ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
          if (((*(byte *)(*(int *)0x8d4a + 3) & 4) != 0) &&
             (iStack_32 = iStack_32 >> 1, iStack_32 < 1)) {
            iStack_32 = 1;
          }
          if (iStack_32 < iStack_e2) {
            uStack_24 = uStack_50;
            iStack_e2 = iStack_32;
          }
        }
        unaff_CS = 0x181f;
      }
      if ((int)uStack_24 < 0) goto LAB_OVL14_L0000__0047b9;
      uVar19 = uStack_24;
      FUN_1000_8c3c(unaff_CS,uStack_24);
      uVar18 = (uint)*(byte *)(*(int *)0x8d4a + 1);
    }
    else {
      if (iStack_ae != 0) {
        unaff_CS = 0x181f;
        FUN_1000_896e(0x1781,0,0,0);
      }
      if (*(char *)(param_2 * 0x1c + 0x3146) == '\n') {
        if (iStack_2e == 0) {
          uVar13 = (uint)*(byte *)(param_2 * 0x1c + 0x315b) * 100;
          iVar15 = *(int *)0x84fc;
          puVar2 = (uint *)(iVar15 + 0x2a);
          uVar14 = *puVar2;
          *puVar2 = *puVar2 + uVar13;
          piVar3 = (int *)(iVar15 + 0x2c);
          *piVar3 = *piVar3 + (uint)CARRY2(uVar14,uVar13);
          if ((*(byte *)0x5382 & 1) == 0) {
            uVar11 = func_0x00018b94(unaff_CS,uStack_e6);
            FUN_1000_8628(0,uVar11);
            FUN_1000_8628(1,*(undefined2 *)(uStack_e6 * 2 + -0x7c74));
            FUN_1000_8b9e(0,uVar13,0);
            unaff_CS = 0x181f;
            FUN_1000_8842(0x181f,0x1786,2);
          }
          goto LAB_OVL14_L0000__0047b9;
        }
        uVar19 = uStack_62;
        if (iStack_2c == iStack_38) goto LAB_OVL14_L0000__004701;
        uStack_a6 = param_2;
        iVar15 = FUN_1000_8a98(uStack_e6,param_2,uStack_88,uStack_94);
        if (-1 < iVar15) {
          iVar15 = iVar15 * 0x1c;
          uVar19 = (uint)*(byte *)(iVar15 + 0x3145);
          iVar16 = FUN_1000_8912(0x181f,*(undefined1 *)(iVar15 + 0x3144),uVar19);
          if (iVar16 == iStack_38) {
            uStack_e = (uint)*(byte *)(iVar15 + 0x3144);
            uStack_1e = (uint)*(byte *)(iVar15 + 0x3145);
            param_2 = uStack_a6;
            uVar18 = uStack_1e;
            goto LAB_OVL14_L0000__0027f5;
          }
        }
        unaff_CS = 0x181f;
        param_2 = uStack_a6;
        iVar15 = FUN_OVL14_L0000__0072d6(0x181f,uStack_88,uStack_94,uStack_e6,0);
        if (iVar15 == *(int *)0x5398) {
LAB_OVL14_L0000__0047b9:
          FUN_1000_89f8(unaff_CS,param_2);
          return uStack_b8;
        }
      }
      if (iStack_ae != 0) {
        unaff_CS = 0x181f;
        FUN_1000_896e(0x1792,0,0,0);
      }
      if (*(char *)(param_2 * 0x1c + 0x3146) == '\x03') {
        uStack_24 = 0xffff;
        iStack_e2 = -999;
        for (uStack_50 = 0; (int)uStack_50 < *(int *)0x539a; uStack_50 = uStack_50 + 1) {
          FUN_1000_8c3c(unaff_CS,uStack_50);
          iVar15 = FUN_1000_8912(0x181f,*(undefined1 *)*(undefined2 *)0x8d4a,
                                 ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
          if (iVar15 == iStack_38) {
            if ((*(byte *)(*(int *)0x8d4a + 5) & 0xf) == uStack_e6) {
              iVar15 = *(int *)(*(int *)0x84fc + 0x2c);
              if (((iVar15 < 1) && ((iVar15 < 0 || (*(uint *)(*(int *)0x84fc + 0x2a) < 0x9c4)))) ||
                 ((iVar15 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,*(undefined2 *)0x5398),
                  0x4a < iVar15 ||
                  ((uVar19 = FUN_1000_8c28(0x181f,*(undefined2 *)0x8d52,*(undefined2 *)0x5398),
                   (uVar19 & 0x20) == 0 ||
                   (*(byte *)(*(int *)0x5398 + -0x6e84) <= *(byte *)(uStack_e6 + 0x917c)))))))
              goto LAB_OVL14_L0000__0048ab;
            }
            bVar10 = *(byte *)(*(int *)0x8d4a + 3);
            uStack_56 = FUN_1000_8506(uStack_50,auStack_8a);
            iVar15 = FUN_1000_84fc(0x181f,*(undefined2 *)0x8d52,uStack_e6);
            iVar16 = FUN_1000_856a(uStack_88,uStack_94,*(undefined1 *)*(undefined2 *)0x8d4a,
                                   ((undefined1 *)*(undefined2 *)0x8d4a)[1]);
            iStack_d0 = (iVar15 << 3) / (iVar16 + 1);
            if ((bVar10 & 4) != 0) {
              iStack_d0 = iStack_d0 + (iStack_d0 >> 1);
            }
            if (iStack_e2 < iStack_d0) {
              uStack_24 = uStack_50;
              iStack_e2 = iStack_d0;
            }
          }
LAB_OVL14_L0000__0048ab:
          unaff_CS = 0x181f;
        }
        if (-1 < (int)uStack_24) {
          uVar19 = uStack_24;
          FUN_1000_8c3c(unaff_CS,uStack_24);
          uVar18 = (uint)*(byte *)(*(int *)0x8d4a + 1);
          goto LAB_OVL14_L0000__0027f5;
        }
        *(undefined1 *)(param_2 * 0x1c + 0x3146) = 0;
      }
      if (iStack_ae != 0) {
        unaff_CS = 0x181f;
        FUN_1000_896e(0x1797,0,0,0);
      }
      if ((((*(char *)(param_2 * 0x1c + 0x3146) != '\x05') &&
           (*(char *)(param_2 * 0x1c + 0x3146) != '\x02')) || (uStack_2a != 0)) ||
         (iStack_2c != iStack_38)) {
        if (iStack_ae != 0) {
          unaff_CS = 0x181f;
          FUN_1000_896e((undefined1 *)&LAB_OVL14_L0000__00179c,0,0,0);
        }
        if (iStack_6a != 0) {
          if (*(char *)(param_2 * 0x1c + 0x314c) == '\v') goto LAB_OVL14_L0000__005a78;
          uVar11 = unaff_CS;
          if (*(char *)(param_2 * 0x1c + 0x3156) < '\0') {
            uVar11 = 0x181f;
            cVar9 = FUN_1000_86c4(unaff_CS,1,0x14);
            *(char *)(param_2 * 0x1c + 0x3156) = cVar9 + -1;
          }
          iVar15 = param_2 * 0x1c;
          uStack_50 = (uint)*(byte *)(iVar15 + 0x3156);
          if ((*(char *)(iVar15 + 0x314b) == '8') && (*(char *)(iVar15 + 0x3155) != '\0')) {
            uStack_18 = (uint)*(byte *)(iVar15 + 0x314d);
            uStack_1c = (uint)*(byte *)(iVar15 + 0x314e);
          }
          else {
            uStack_18 = *(char *)(uStack_50 + 200) * 4 + uStack_88;
            uStack_1c = *(char *)(uStack_50 + 0xde) * 4 + uStack_94;
          }
          unaff_CS = 0x181f;
          iVar15 = FUN_1000_84f2(uVar11,uStack_18,uStack_1c);
          if ((iVar15 != 0) &&
             ((*(byte *)(((int)uStack_1c >> 2) + ((int)uStack_18 >> 2) * 0x12 + -0x6056) & 6) == 0))
          {
            unaff_CS = 0x181f;
            iVar15 = FUN_1000_8912(0x181f,uStack_18,uStack_1c);
            if (iVar15 == iStack_38) {
              uVar19 = 0x181f;
              unaff_CS = 0x181f;
              uVar18 = 0x4b2c;
              iVar15 = FUN_1000_88c2(0x181f,uStack_18,uStack_1c);
              if (iVar15 < 0) {
                iVar16 = *(char *)(uStack_50 + 0xde) * 4;
                iVar15 = *(char *)(uStack_50 + 200) * 4;
                if (iVar15 < iVar16) {
                  iVar15 = iVar16;
                }
                iVar16 = param_2 * 0x1c;
                *(undefined1 *)(iVar16 + 0x3155) = (char)iVar15;
                uVar13 = uStack_1c;
                if (8 < *(byte *)(iVar16 + 0x3154)) {
                  *(char *)(iVar16 + 0x3154) = *(char *)(iVar16 + 0x3154) + -8;
                }
                goto LAB_OVL14_L0000__0027f5;
              }
            }
          }
        }
        uVar11 = unaff_CS;
        if (iStack_ae != 0) {
          uVar11 = 0x181f;
          FUN_1000_896e(0x17a1,0,0,0);
        }
        if ((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) ||
           (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) {
          iVar15 = param_2 * 0x1c;
          if ((1 < *(byte *)((uint)*(byte *)(iVar15 + 0x3146) * 0xe + 0x5236)) &&
             (-1 < *(char *)(iVar15 + 0x314a))) {
            uVar13 = (uint)*(char *)(iVar15 + 0x314a);
            FUN_1000_8bd6(uVar11,uVar13);
            puVar6 = (undefined1 *)*(undefined2 *)0x8542;
            uVar11 = 0x181f;
            if (((puVar6[0x1b] & 4) != 0) &&
               ((puVar6[0x1e] != '\0' || (*(byte *)(iVar15 + 0x3146) != 4)))) {
              uVar19 = (uint)(byte)puVar6[1];
              uVar11 = 0x181f;
              iVar15 = FUN_1000_8912(0x181f,*puVar6,uVar19);
              if (iVar15 == iStack_38) {
                iVar15 = *(int *)0x8542;
                *(byte *)(iVar15 + 0x1b) = *(byte *)(iVar15 + 0x1b) & 0xfb;
                if (*(char *)(iVar15 + 0x1e) != '\0') {
                  *(char *)(iVar15 + 0x1e) = *(char *)(iVar15 + 0x1e) + -1;
                }
                uVar18 = (uint)*(byte *)(*(int *)0x8542 + 1);
                goto LAB_OVL14_L0000__0027f5;
              }
            }
          }
        }
        if (iStack_ae != 0) {
          uVar11 = 0x181f;
          FUN_1000_896e(0x17a6,0,0,0);
        }
        if ((*(char *)(param_2 * 0x1c + 0x3146) == '\x02') && (iStack_6a == 0)) {
          iStack_a = 1;
          if (-1 < (int)uStack_ac) {
            FUN_1000_8c3c(uVar11,uStack_ac);
            uVar11 = 0x181f;
            iStack_7c = FUN_1000_8c46(0x181f,*(undefined2 *)0x8d52);
            if ((iStack_a0 <= iStack_7c) && (iStack_54 < 3)) {
              iStack_a = 0;
            }
          }
          unaff_CS = uVar11;
          if (-1 < (int)uVar14) {
            unaff_CS = 0x181f;
            FUN_1000_8bd6(uVar11,uVar14);
            if ((*(char *)(*(int *)0x8542 + 0x1a) != (sbyte)uStack_e6) && (iStack_74 < 3)) {
              iStack_a = 0;
            }
          }
          uVar11 = unaff_CS;
          if (iStack_a != 0) {
            *(undefined1 *)(param_2 * 0x1c + 0x314c) = 9;
            *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x52;
            goto LAB_OVL14_L0000__005a78;
          }
        }
        iVar15 = param_2 * 0x1c;
        unaff_CS = uVar11;
        if (((((*(char *)(iVar15 + 0x314c) != '\0') && (*(char *)(iVar15 + 0x314c) != '\n')) &&
             (*(char *)(iVar15 + 0x314c) != '\x05')) && (*(char *)(iVar15 + 0x314c) != '\x06')) &&
           (((*(char *)(iVar15 + 0x314c) != '\v' || (*(char *)(iVar15 + 0x314d) != (char)uStack_88))
            || (*(char *)(iVar15 + 0x314e) != (char)uStack_94)))) {
          unaff_CS = 0x181f;
          iVar15 = FUN_1000_8b74(uVar11,uStack_88,uStack_94,uStack_e6);
          if (iVar15 == 0) goto LAB_OVL14_L0000__005a78;
        }
LAB_OVL14_L0000__004d2e:
        uVar11 = unaff_CS;
        if (iStack_ae != 0) {
          uVar11 = 0x181f;
          FUN_1000_896e(0x17ab,0,0,0);
        }
        uVar14 = 0;
        iVar15 = FUN_OVL14_L0000__0072d6(uVar11,uStack_88,uStack_94,uStack_e6,1);
        if ((iVar15 < 0) ||
           (((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd ||
             (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) &&
            (*(char *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236) == '\0')))) {
          uVar14 = 1;
        }
        if (((0xc < *(byte *)(param_2 * 0x1c + 0x3146)) &&
            (*(byte *)(param_2 * 0x1c + 0x3146) < 0x13)) && ((*(byte *)0x5382 & 1) != 0)) {
          uVar14 = 0;
        }
        iVar15 = param_2 * 0x1c;
        if (((0xc < *(byte *)(iVar15 + 0x3146)) && (*(byte *)(iVar15 + 0x3146) < 0x13)) &&
           (((uVar14 != 0 && ((iStack_48 + iStack_4a != 0 && (uStack_9c == 0)))) &&
            ((*(byte *)0x5382 & 1) == 0)))) {
          if ((*(byte *)(iVar15 + 0x3148) & 0x10) == 0) {
            uVar14 = 0x10;
            iVar16 = FUN_1000_86c4(uVar11,0);
            uVar11 = 0x181f;
            if (iVar16 == 0) {
              uStack_18 = FUN_1000_86c4(0x181f,2,*(int *)0x853a + -3);
              uStack_1c = FUN_1000_86c4(0x181f,2,*(int *)0x853c + -3);
              iVar16 = FUN_1000_8958(0x181f,uStack_18,uStack_1c);
              uVar11 = 0x181f;
              if (iVar16 != 0) {
                cVar9 = FUN_1000_88a4(0x181f,uStack_18,uStack_1c);
                uVar11 = 0x181f;
                if (cVar9 == '\x01') {
                  uVar18 = uStack_94;
                  uVar19 = uStack_18;
                  iVar16 = FUN_1000_856a(uStack_88,uStack_94,uStack_18,uStack_1c);
                  uVar11 = 0x181f;
                  if (7 < iVar16) {
                    *(byte *)(iVar15 + 0x3148) = *(byte *)(iVar15 + 0x3148) | 0x10;
                    uVar13 = uStack_1c;
                    goto LAB_OVL14_L0000__0027f5;
                  }
                }
              }
            }
          }
          else {
            uVar14 = 0x30;
            iVar15 = FUN_1000_86c4(uVar11,0);
            uVar11 = 0x181f;
            if (iVar15 == 0) {
              pbVar4 = (byte *)(param_2 * 0x1c + 0x3148);
              *pbVar4 = *pbVar4 & 0xef;
            }
          }
        }
        if (iStack_ae != 0) {
          uVar11 = 0x181f;
          FUN_1000_896e(0x17b0,0,0,0);
        }
        iStack_e2 = -999;
        uStack_76 = 8;
        for (uStack_50 = 0; (int)uStack_50 < 8; uStack_50 = uStack_50 + 1) {
          uStack_1c = (int)*(char *)(uStack_50 + 0xbe) + uStack_94;
          uStack_18 = (int)*(char *)(uStack_50 + 0xb4) + uStack_88;
          iVar15 = FUN_1000_84f2(uVar11,uStack_18,uStack_1c);
          if ((iVar15 != 0) &&
             (((iStack_3a = func_0x0001897c(0x181f,uStack_18,uStack_1c), iStack_3a != 0x19 &&
               (iStack_3a != 0x1a)) ||
              ((iStack_34 != 0 &&
               (cVar9 = FUN_1000_88a4(0x181f,uStack_18,uStack_1c), cVar9 == '\x01')))))) {
            uVar13 = uStack_18;
            cVar9 = FUN_1000_88cc(0x181f,uStack_18,uStack_1c);
            uStack_10 = (uint)cVar9;
            uStack_5c = FUN_1000_89d0();
            if ((((iStack_90 == 0) || (iStack_34 != 0)) || ((int)uStack_5c < 0)) ||
               (uStack_10 == uStack_e6)) {
              if (((iStack_34 == 0) || (iStack_3a == 0x19)) || (iStack_3a == 0x1a)) {
                if (*(char *)(param_2 * 0x1c + 0x3146) == '\x05') {
                  uVar19 = 8;
                  uStack_28 = FUN_1000_86c4(0x181f,1,8);
                  if (((uStack_84 == 0) ||
                      (uVar18 = FUN_1000_891c(0x181f,uStack_18,uStack_1c), (uVar18 & 0x40) == 0)) ||
                     ((uStack_50 & 1) != 0)) {
                    if ((uStack_5a == 0) ||
                       (uVar18 = FUN_1000_8944(0x181f,uStack_18,uStack_1c), (uVar18 & 10) == 0)) {
                      uVar18 = (uint)*(byte *)(iStack_3a * 0x10 + 0x2f76) * 3;
LAB_OVL14_L0000__004f41:
                      uStack_28 = uStack_28 - uVar18;
                    }
                    else {
LAB_OVL14_L0000__004ffa:
                      uStack_28 = uStack_28 + 1;
                    }
                  }
                  else {
                    uStack_28 = uStack_28 + 2;
                  }
                }
                else if (iStack_6a == 0) {
                  if (((*(byte *)(param_2 * 0x1c + 0x3147) & 0xf0) == 0) && (iStack_90 == 0)) {
                    iVar15 = (uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe;
                    if (((*(byte *)(iVar15 + 0x523d) & 0x20) == 0) &&
                       ((*(byte *)(iVar15 + 0x523d) & 0x10) == 0)) {
                      uVar19 = 3;
                      uStack_28 = FUN_1000_86c4(0x181f,1,3);
                      if ((*(byte *)0x5382 & 1) == 0) {
                        uVar18 = (uint)*(byte *)(iStack_3a * 0x10 + 0x2f77);
                        goto LAB_OVL14_L0000__00506d;
                      }
                      bVar10 = *(byte *)(iStack_3a * 0x10 + 0x2f77);
                    }
                    else {
                      uVar19 = 3;
                      uStack_28 = FUN_1000_86c4(0x181f,1,3);
                      if ((((uStack_84 != 0) &&
                           (uVar18 = FUN_1000_891c(0x181f,uStack_18,uStack_1c), (uVar18 & 0x40) != 0
                           )) && ((uStack_50 & 1) == 0)) ||
                         ((uStack_5a != 0 &&
                          (uVar18 = FUN_1000_8944(0x181f,uStack_18,uStack_1c), (uVar18 & 10) != 0)))
                         ) goto LAB_OVL14_L0000__004ffa;
                      bVar10 = *(byte *)(iStack_3a * 0x10 + 0x2f76);
                    }
                    uVar18 = (uint)bVar10;
                    goto LAB_OVL14_L0000__004f41;
                  }
                  uVar19 = 5;
                  uStack_28 = FUN_1000_86c4(0x181f,1,5);
                  if (((int)uStack_5c < 0) || (uStack_10 != uStack_e6)) {
                    uVar18 = (uint)*(byte *)(iStack_3a * 0x10 + 0x2f77) << 2;
LAB_OVL14_L0000__00506d:
                    uStack_28 = uStack_28 + uVar18;
                  }
                }
                else {
                  uVar19 = 4;
                  iVar15 = FUN_1000_86c4(0x181f,1,4);
                  uVar18 = FUN_1000_893a(0x181f,uStack_18,uStack_1c);
                  uStack_28 = (int)(iVar15 + (uVar18 & 0xf)) >> 1;
                }
                if (iStack_3a == 0x1a) {
                  uStack_28 = uStack_28 + -0x10;
                }
                if (((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd) ||
                    (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))) &&
                   ((1 < *(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236) &&
                    (iVar15 = FUN_1000_8886(0x181f,uStack_18,uStack_1c), -1 < iVar15)))) {
                  if (uStack_10 == uStack_e6) {
                    FUN_1000_8bd6(0x181f,uStack_62);
                    iVar15 = *(int *)0x8542;
                    if ((*(byte *)(iVar15 + 0x1b) & 0x40) == 0) {
                      if ((*(byte *)(iVar15 + 0x1b) & 4) == 0) {
                        if ((*(byte *)(iVar15 + 0x1b) & 0x10) != 0) {
                          uStack_28 = uStack_28 + 3;
                        }
                      }
                      else {
                        uStack_28 = uStack_28 + 6;
                      }
                    }
                    else {
                      uStack_28 = uStack_28 + 10;
                    }
                  }
                  else {
                    uStack_28 = uStack_28 + 0x10;
                  }
                }
                iStack_7e = 0;
                if ((((int)uStack_5c < 0) &&
                    (iVar15 = FUN_1000_88c2(0x181f,uStack_18,uStack_1c), iVar15 < 0)) ||
                   (uStack_10 == uStack_e6)) {
LAB_OVL14_L0000__0054f5:
                  iVar15 = param_2 * 0x1c;
                  if ((-1 < *(char *)(iVar15 + 0x314f)) && (*(char *)(iVar15 + 0x314f) < '\b')) {
                    uVar13 = (int)*(char *)(iVar15 + 0x314f) - uStack_50;
                    uStack_6e = uVar13;
                    if ((int)uVar13 < 1) {
                      uStack_6e = ~((int)*(char *)(param_2 * 0x1c + 0x314f) - uStack_50) + 1;
                    }
                    if (4 < (int)uStack_6e) {
                      uStack_6e = -(uStack_6e - 8);
                    }
                    uStack_28 = uStack_28 + uStack_6e * uStack_6e * -2;
                  }
                  for (iStack_70 = 0; iStack_70 < 8; iStack_70 = iStack_70 + 1) {
                    iStack_40 = (int)*(char *)(iStack_70 + 0xbe) + uStack_1c;
                    iStack_30 = (int)*(char *)(iStack_70 + 0xb4) + uStack_18;
                    uStack_10 = FUN_1000_8872(0x181f,iStack_30,iStack_40);
                    if ((((-1 < (int)uStack_10) && (uStack_10 != uStack_e6)) &&
                        (bVar10 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10), (bVar10 & 0x60) == 0x20
                        )) && (*(char *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236) ==
                               '\0')) {
                      uVar11 = 0x56ca;
                      iStack_96 = FUN_1000_89d0(0x181f);
                      while (-1 < iStack_96) {
                        if (*(char *)((uint)*(byte *)(iStack_96 * 0x1c + 0x3146) * 0xe + 0x5236) !=
                            '\0') {
                          uStack_28 = uStack_28 + -10;
                        }
                        iStack_96 = FUN_1000_84d4(uVar11,0x181f);
                      }
                    }
                    if (((0xc < *(byte *)(param_2 * 0x1c + 0x3146)) &&
                        (*(byte *)(param_2 * 0x1c + 0x3146) < 0x13)) &&
                       (iStack_92 = FUN_1000_89ae(0x181f,iStack_30,iStack_40), -1 < iStack_92)) {
                      uStack_10 = (uint)(byte)((undefined1 *)&LAB_OVL14_L0000__005d60)
                                              [iStack_92 * 0xca];
                      iStack_b0 = 0;
                      bVar10 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10);
                      if ((bVar10 & 0x60) == 0x20) {
                        iVar15 = FUN_1000_8512(0x181f,iStack_92,1);
                        if (iVar15 != 0) {
                          iStack_b0 = 0x14;
                        }
                        iVar15 = FUN_1000_8512(0x181f,iStack_92,2);
                        if (iVar15 != 0) {
                          iStack_b0 = 0x28;
                        }
                        uVar11 = 0x5617;
                        uStack_58 = FUN_1000_89d0(0x181f);
                        while (-1 < (int)uStack_58) {
                          if (*(char *)(uStack_58 * 0x1c + 0x3146) == '\v') {
                            iStack_b0 = iStack_b0 + 0x1e;
                          }
                          uStack_58 = FUN_1000_84d4(uVar11,0x181f);
                        }
                        iStack_b0 = (uint)*(byte *)(param_2 * 0x1c + 0x3150) * iStack_b0;
                        uStack_28 = uStack_28 - iStack_b0;
                      }
                    }
                  }
                  if (uVar14 != 0) {
                    uStack_1c = *(char *)(uStack_50 + 0xbe) * 4 + uStack_94;
                    uStack_18 = *(char *)(uStack_50 + 0xb4) * 4 + uStack_88;
                    if (((*(char *)(((int)uStack_1c >> 2) + ((int)uStack_18 >> 2) * 0x12 + -0x6056)
                          == '\0') &&
                        (iVar15 = FUN_1000_8958(0x181f,uStack_18,uStack_1c,uVar19,uVar13),
                        iVar15 == 0)) &&
                       (iVar15 = FUN_1000_84f2(0x181f,uStack_18,uStack_1c), iVar15 != 0)) {
                      uStack_28 = uStack_28 + 8;
                    }
                    if ((((iStack_34 != 0) && (4 < (int)uStack_50)) && ((int)uStack_50 < 8)) &&
                       (*(int *)0x853a >> 1 < (int)uStack_88)) {
                      uStack_28 = uStack_28 + 4;
                    }
                    iStack_70 = 0;
                    do {
                      iStack_40 = (int)*(char *)(iStack_70 + 0xbe) + uStack_1c;
                      iStack_30 = (int)*(char *)(iStack_70 + 0xb4) + uStack_18;
                      iVar15 = FUN_1000_84f2(0x181f,iStack_30,iStack_40);
                      if (iVar15 != 0) {
                        if (uStack_e6 < 4) {
                          uVar13 = FUN_1000_893a(0x181f,iStack_30,iStack_40);
                          if (((0x10 << (sbyte)uStack_e6 & uVar13 & 0xff) == 0) &&
                             ((iVar15 = FUN_1000_8958(0x181f,iStack_30,iStack_40), iVar15 == 0 ||
                              (iStack_34 != 0)))) {
                            uStack_28 = uStack_28 + 2;
                          }
                        }
                        iVar15 = FUN_1000_8872(0x181f,iStack_30,iStack_40);
                        if (-1 < iVar15) {
                          uStack_28 = uStack_28 + -2;
                        }
                        if (iStack_6a != 0) {
                          iVar15 = func_0x0001897c(0x181f,iStack_30,iStack_40);
                          uStack_28 = uStack_28 + *(byte *)(iVar15 * 0x10 + 0x2f79);
                        }
                      }
                      iStack_70 = iStack_70 + 1;
                    } while (iStack_70 < 8);
                  }
                  if (iStack_e2 < (int)uStack_28) {
                    iStack_e2 = uStack_28;
                    uStack_76 = uStack_50;
                    iStack_ce = iStack_7e;
                  }
                }
                else if ((int)uStack_10 < 4) {
                  uVar13 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10);
                  if (((((uVar13 & 0x40) == 0) || (*(char *)(param_2 * 0x1c + 0x3146) == '\x10')) ||
                      (*(char *)(uStack_5c * 0x1c + 0x3146) == '\x10')) &&
                     (((*(byte *)0x5382 & 1) == 0 ||
                      ((((int)uStack_10 < 4 && (*(char *)(uStack_10 * 0x34 + 0x543f) == '\0')) ||
                       (3 < (int)uStack_10)))))) {
LAB_OVL14_L0000__0052aa:
                    if (*(char *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5236) != '\0')
                    {
                      iStack_7e = 1;
                      func_0x00019c04(0x181f,param_2,uStack_18,uStack_1c,0,0);
                      uStack_e6 = 2;
                      uStack_e8 = uStack_5c;
                      iVar15 = FUN_1000_8aac(0x191f);
                      if (0 < iVar15) {
                        uStack_e8 = 2;
                        FUN_1000_8aac(0x181f,uStack_5c);
                      }
                      in_stack_0000ff16 = 0;
                      iVar16 = 0x531f;
                      iVar15 = FUN_1000_8aac(0x181f,uStack_5c);
                      cVar9 = *(char *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x5239);
                      uStack_e8 = (int)(((iVar15 + 1) / iVar16) * uStack_e8) /
                                  (int)(uint)(byte)((cVar9 - 1U & ~-(cVar9 == '\0')) + 1);
                      iStack_4 = 0;
                      uVar14 = uStack_1c;
                      iVar15 = FUN_1000_8886(0x181f,uStack_18);
                      if (-1 < iVar15) {
                        uStack_e8 = uStack_e8 * 3;
                        iStack_4 = 1;
                      }
                      uVar19 = 0x181f;
                      uVar13 = uStack_18;
                      iVar15 = FUN_1000_88e0(0x181f,uStack_18,uStack_1c);
                      if (-1 < iVar15) {
                        uStack_e8 = uStack_e8 << 1;
                        iStack_4 = 1;
                      }
                      if ((*(char *)(param_2 * 0x1c + 0x3146) == '\v') && (iStack_4 == 0)) {
                        uStack_e8 = 0;
                      }
                      if (((*(int *)0x53d2 == 2) && (iStack_4 == 0)) && (iStack_2e == 0)) {
                        uStack_e8 = (int)uStack_e8 >> 1;
                      }
                      if (((*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x523d) &
                           0x10) != 0) && (uStack_2a == 4)) {
                        uStack_e8 = uStack_e8 * 3;
                      }
                      if (((*(char *)(param_2 * 0x1c + 0x3146) == '\x01') ||
                          (*(char *)(param_2 * 0x1c + 0x3146) == '\x04')) &&
                         (uVar19 = uStack_18, uVar13 = uStack_1c,
                         iVar15 = FUN_1000_8886(0x181f,uStack_18,uStack_1c), -1 < iVar15)) {
                        uVar19 = 0xb;
                        iVar15 = FUN_1000_8aac(0x181f,uStack_5c,0xb);
                        if (iVar15 != 0) {
                          uStack_a6 = param_2;
                          iStack_8 = 0;
                          for (iStack_70 = 0; iStack_70 < 8; iStack_70 = iStack_70 + 1) {
                            iStack_40 = (int)*(char *)(iStack_70 + 0xbe) + uStack_1c;
                            iStack_30 = (int)*(char *)(iStack_70 + 0xb4) + uStack_18;
                            iVar16 = FUN_1000_89d0(0x181f);
                            if ((-1 < iVar16) && ((*(byte *)(iVar16 * 0x1c + 0x3147) & 0xf) == 2)) {
                              iVar16 = FUN_1000_8aac(0x181f,iVar16,0xb);
                              iStack_8 = iStack_8 + iVar16;
                            }
                          }
                          param_2 = uStack_a6;
                          if (iStack_8 <= iVar15) goto LAB_OVL14_L0000__005183;
                        }
                      }
                      if ((999 < (int)uStack_e8) || ((int)uStack_e8 < 0)) {
                        uStack_e8 = 1000;
                      }
                      if (((int)uStack_e8 < 0xc) &&
                         ((*(byte *)(param_2 * 0x1c + 0x3146) < 0xd ||
                          (0x12 < *(byte *)(param_2 * 0x1c + 0x3146))))) {
                        uStack_28 = uStack_28 + -999;
                      }
                      else {
                        if ((int)uStack_e8 < 1) {
                          uStack_e8 = 1;
                        }
                        uStack_28 = uStack_28 + uStack_e8 * 4;
                      }
                      goto LAB_OVL14_L0000__0054f5;
                    }
                  }
                }
                else {
                  iVar15 = FUN_1000_84fc(0x181f,uStack_10 - 4,uStack_e6);
                  if ((0x4a < iVar15) ||
                     (uVar13 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10), (uVar13 & 2) != 0)) {
                    uVar13 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10);
                    if ((uVar13 & 2) != 0) {
                      uStack_28 = uStack_28 << 1;
                    }
                    if (*(char *)(iStack_38 + uStack_e6 * 0x10 + -0x6b1a) != '\0')
                    goto LAB_OVL14_L0000__0052aa;
                  }
                }
              }
              else {
                FUN_1000_8886(0x181f,uStack_18,uStack_1c);
              }
            }
          }
LAB_OVL14_L0000__005183:
          uVar11 = 0x181f;
        }
        if (iStack_ae != 0) {
          uVar11 = 0x181f;
          FUN_1000_896e(0x17b5,0,0,0);
        }
        if (iStack_ce == 0) {
          if (iStack_8e != 0) {
            FUN_1000_8bd6(uVar11,uStack_62);
            goto LAB_OVL14_L0000__005888;
          }
          if ((((*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x523d) & 1) != 0) &&
              (iStack_2e != 0)) &&
             ((iStack_42 = FUN_OVL14_L0000__007344(uVar11,uStack_e6,uStack_88,uStack_94,0),
              iStack_42 != 0 && (iStack_42 < 5)))) {
            unaff_CS = 0x181f;
            iVar15 = FUN_1000_8aac(uVar11,param_2,2);
            uVar11 = unaff_CS;
            if (iVar15 < 2) {
              *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x42;
              goto LAB_OVL14_L0000__005899;
            }
          }
          unaff_CS = uVar11;
          if (((*(byte *)((uint)*(byte *)(param_2 * 0x1c + 0x3146) * 0xe + 0x523d) & 4) != 0) &&
             (iVar15 = FUN_OVL14_L0000__007344(uVar11,uStack_e6,uStack_88,uStack_94,2), iVar15 != 0)
             ) {
            unaff_CS = 0x181f;
            iVar15 = FUN_1000_8aac(uVar11,param_2,2);
            if (iVar15 < 2) {
              *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x65;
              goto LAB_OVL14_L0000__005899;
            }
          }
          bVar10 = *(byte *)(param_2 * 0x1c + 0x3146);
          if ((1 < *(byte *)((uint)bVar10 * 0xe + 0x5236)) &&
             (((bVar10 < 0xd || (0x12 < bVar10)) && (in_stack_0000ff16 != 0)))) {
            uVar20 = 0x59d1;
            uVar11 = unaff_CS;
            iVar15 = FUN_1000_8b7e(unaff_CS);
            unaff_CS = 0x181f;
            iVar16 = FUN_1000_84de(uVar20,uVar11,0x181f);
            if (iVar16 == iVar15) {
              for (uStack_50 = 0; unaff_CS = 0x181f, (int)uStack_50 < 8; uStack_50 = uStack_50 + 1)
              {
                uStack_1c = (int)*(char *)(uStack_50 + 0xbe) + uStack_94;
                uStack_18 = (int)*(char *)(uStack_50 + 0xb4) + uStack_88;
                unaff_CS = 0x181f;
                uStack_10 = FUN_1000_8886(0x181f,uStack_18,uStack_1c);
                if ((-1 < (int)uStack_10) && (uStack_10 != uStack_e6)) {
                  *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x46;
                  goto LAB_OVL14_L0000__005899;
                }
              }
            }
          }
        }
        else {
          unaff_CS = 0x181f;
          uVar14 = FUN_1000_8afc(uVar11,param_2);
          if ((int)((uVar14 & 0xff) - (uint)*(byte *)(param_2 * 0x1c + 0x3149)) < 3) {
            uStack_76 = 8;
          }
        }
        *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x39;
        goto LAB_OVL14_L0000__00589e;
      }
      uVar19 = uStack_62;
      FUN_1000_8bd6(unaff_CS,uStack_62);
      uVar18 = (uint)*(byte *)(*(int *)0x8542 + 1);
    }
LAB_OVL14_L0000__0027f5:
    FUN_OVL14_L0000__0020c6(uVar18,uVar19,uVar13);
    unaff_CS = 0x181f;
    goto LAB_OVL14_L0000__005a78;
  }
  uStack_a6 = param_2;
  iVar15 = FUN_1000_84de(0x181f);
  while (uVar19 = uStack_a6, -1 < iVar15) {
    if (*(char *)(iVar15 * 0x1c + 0x314c) == '\x01') {
      *(undefined1 *)(iVar15 * 0x1c + 0x314c) = 0;
    }
    iVar15 = FUN_1000_84d4();
  }
  param_2 = uStack_a6;
  FUN_1000_8bd6(0x181f,uStack_62);
  uVar13 = 0x181f;
  FUN_1000_8772(0x181f,uStack_e6);
  while (unaff_CS = 0x181f, *(char *)(uVar19 * 0x1c + 0x3150) != '\0') {
    uVar13 = 0;
    uStack_b6 = FUN_1000_8cdc(0x181f,uVar19,0);
    piVar3 = (int *)(*(int *)0x8542 + uStack_b6 * 2 + 0x9a);
    *piVar3 = *piVar3 + *(int *)0x8dc4;
  }
  if (iStack_34 != 0) {
    *(undefined1 *)(uVar19 * 0x1c + 0x314a) = 0xff;
    *(undefined1 *)(*(int *)0x8542 + 0x8f) = 0;
  }
  bVar10 = *(byte *)(uVar19 * 0x1c + 0x3146);
  iStack_d2 = (uint)*(byte *)((uint)bVar10 * 0xe + 0x5237) - (uint)*(byte *)(uVar19 * 0x1c + 0x3150)
  ;
  if ((bVar10 == 0xc) && (1 < iStack_d2)) {
    iStack_d2 = 1;
  }
  if ((((iStack_34 == 0) || ((*(byte *)(*(int *)0x8542 + 0x1b) & 2) == 0)) ||
      (iVar15 = uVar19 * 0x1c, 0xe < *(byte *)(iVar15 + 0x3146))) ||
     (*(char *)(iVar15 + 0x315a) = *(char *)(iVar15 + 0x315a) + '\x01',
     iVar16 = *(byte *)((uint)*(byte *)(iVar15 + 0x3146) * 0xe + 0x5237) - 10,
     -(uint)*(byte *)(iVar15 + 0x315a) == iVar16 || -iVar16 < (int)(uint)*(byte *)(iVar15 + 0x315a))
     ) {
    if (((iStack_34 != 0) && (bVar7)) && ((*(byte *)(uVar19 * 0x1c + 0x3148) & 0x20) == 0)) {
      uVar13 = 0x181f;
      uVar11 = 0x31fd;
      uStack_a6 = uVar19;
      iVar15 = FUN_1000_84de(0x181f);
      while ((-1 < iVar15 && (*(int *)(uStack_e6 * 2 + 0x1734) < 0x19))) {
        if (((*(byte *)(iVar15 * 0x1c + 0x3146) < 0xd) || (0x12 < *(byte *)(iVar15 * 0x1c + 0x3146))
            ) && (*(byte *)((uint)*(byte *)(iVar15 * 0x1c + 0x3146) * 0xe + 0x5238) <=
                  (byte)iStack_d2)) {
          bVar8 = false;
          if ((((*(byte *)(iVar15 * 0x1c + 0x3146) < 0xd) ||
               (0x12 < *(byte *)(iVar15 * 0x1c + 0x3146))) &&
              (iVar16 = iVar15 * 0x1c,
              1 < *(byte *)((uint)*(byte *)(iVar16 + 0x3146) * 0xe + 0x5236))) &&
             (((*(char *)(iVar16 + 0x314b) != 'G' && (*(char *)(iVar16 + 0x314b) != 'A')) &&
              (uStack_2a == 0)))) {
            bVar8 = true;
          }
          if (*(char *)(iVar15 * 0x1c + 0x3146) == '\x02') {
            if ((iStack_14 == 0) && (uStack_2a != 0)) goto LAB_OVL14_L0000__0032e3;
            bVar8 = true;
          }
          if (bVar8) {
            *(undefined1 *)(iVar15 * 0x1c + 0x314c) = 1;
            iStack_d2 = iStack_d2 -
                        (uint)*(byte *)((uint)*(byte *)(iVar15 * 0x1c + 0x3146) * 0xe + 0x5238);
          }
        }
LAB_OVL14_L0000__0032e3:
        uVar13 = 0x181f;
        iVar15 = FUN_1000_84d4(uVar11,0x181f);
      }
      param_2 = uStack_a6;
      *(undefined2 *)(uStack_e6 * 2 + 0x1734) = 0;
    }
    while ((iStack_d2 != 0 && (bVar17))) {
      uStack_24 = 0xffff;
      iStack_e2 = -1;
      iStack_a4 = FUN_1000_8f2a(0x181f);
      for (uStack_b6 = 0; (int)uStack_b6 < 0x10; uStack_b6 = uStack_b6 + 1) {
        iStack_36 = *(int *)(*(int *)0x8542 + uStack_b6 * 2 + 0x9a);
        if ((iStack_36 < iStack_a4) || (uStack_b6 == 0)) {
          if ((uStack_b6 == 8) && (iStack_36 = (iStack_36 - iStack_a4) + 0x17, iStack_36 < 0)) {
            iStack_36 = 0;
          }
        }
        else {
          iStack_36 = iStack_36 << 1;
        }
        if (uStack_b6 != 5) {
          if ((uStack_b6 == 0xe) || (uStack_b6 == 0xf)) {
            if ((iStack_34 == 0) ||
               ((*(uint *)(*(int *)0x8542 + 0x90) & 1 << ((byte)uStack_b6 & 0x1f)) == 0))
            goto LAB_OVL14_L0000__003356;
            iStack_36 = iStack_36 + -100;
          }
          if ((((iStack_34 == 0) || (uStack_b6 != 0xd)) && ((iStack_34 == 0 || (uStack_b6 != 0))))
             && (iStack_36 != 0)) {
            if (iStack_34 == 0) {
              uStack_28 = (uint)*(byte *)(uStack_b6 + uStack_e6 * 0x10 + -0x7b44);
              while ((1 < (int)uStack_28 && (iVar15 = FUN_1000_86c4(0x181f,0,3), iVar15 == 0))) {
                uStack_28 = uStack_28 - 1;
              }
              if (uStack_b6 == 0xd) {
                uStack_a6 = 8;
              }
              else {
                uStack_a6 = 4;
              }
              if ((int)uStack_28 < (int)uStack_a6) {
                uStack_58 = uStack_28;
                uStack_28 = iStack_36 * (uStack_a6 - uStack_28) + (1 - uStack_28) * 5;
              }
              else {
                uStack_28 = -1;
              }
              if (iStack_36 < 0x32) {
                uStack_28 = -1;
              }
            }
            else {
              uStack_28 = (uint)*(byte *)(uStack_b6 + uStack_e6 * 0x10 + -0x7b44) * iStack_36;
            }
            if (iStack_e2 < (int)uStack_28) {
              iStack_e2 = uStack_28;
              uStack_24 = uStack_b6;
            }
          }
        }
LAB_OVL14_L0000__003356:
      }
      if ((int)uStack_24 < 0) {
        iStack_d2 = 0;
      }
      else {
        iStack_36 = *(int *)(*(int *)0x8542 + uStack_24 * 2 + 0x9a);
        if (100 < iStack_36) {
          iStack_36 = 100;
        }
        piVar3 = (int *)(*(int *)0x8542 + uStack_24 * 2 + 0x9a);
        *piVar3 = *piVar3 - iStack_36;
        FUN_1000_8f48(0x181f,param_2,uStack_24,iStack_36);
        iStack_d2 = iStack_d2 + -1;
        if (iStack_34 == 0) {
          *(undefined1 *)(param_2 * 0x1c + 0x3158) = 1;
        }
        else {
          *(undefined1 *)(param_2 * 0x1c + 0x314a) = (char)uStack_62;
        }
      }
    }
    goto LAB_OVL14_L0000__003558;
  }
  *(undefined1 *)(iVar15 + 0x314b) = 0x43;
LAB_OVL14_L0000__005899:
  uStack_76 = 8;
LAB_OVL14_L0000__00589e:
  iVar15 = param_2 * 0x1c;
  *(undefined1 *)(iVar15 + 0x314f) = (undefined1)uStack_76;
  if (uStack_76 == 8) {
    if ((*(char *)(iVar15 + 0x314c) != '\x05') && (*(char *)(iVar15 + 0x314c) != '\x06')) {
      *(undefined1 *)(iVar15 + 0x314c) = 5;
    }
    if ((*(byte *)(param_2 * 0x1c + 0x3148) & 2) != 0) {
      *(undefined1 *)(param_2 * 0x1c + 0x314c) = 6;
    }
  }
  else {
    uStack_94 = uStack_94 + (int)*(char *)(uStack_76 + 0xbe);
    uStack_88 = uStack_88 + (int)*(char *)(uStack_76 + 0xb4);
    iVar15 = FUN_1000_84f2(unaff_CS,uStack_88,uStack_94);
    unaff_CS = 0x181f;
    if (iVar15 != 0) {
      iVar15 = param_2 * 0x1c;
      *(undefined1 *)(iVar15 + 0x314c) = 0xc;
      *(undefined1 *)(iVar15 + 0x314d) = (char)uStack_88;
      *(undefined1 *)(iVar15 + 0x314e) = (char)uStack_94;
    }
  }
LAB_OVL14_L0000__005a78:
  if ((*(char *)(param_2 * 0x1c + 0x314c) == '\n') || (*(char *)(param_2 * 0x1c + 0x314c) == '\0'))
  {
    *(undefined1 *)(param_2 * 0x1c + 0x314b) = 0x30;
    *(undefined1 *)(param_2 * 0x1c + 0x314c) = 5;
  }
  iVar15 = param_2 * 0x1c;
  if (*(char *)(iVar15 + 0x314c) == '\x05') {
    uStack_88 = (uint)*(byte *)(iVar15 + 0x3144);
    uStack_94 = (uint)*(byte *)(iVar15 + 0x3145);
    for (uStack_50 = 0; (int)uStack_50 < 8; uStack_50 = uStack_50 + 1) {
      uStack_1c = (int)*(char *)(uStack_50 + 0xbe) + uStack_94;
      uStack_18 = (int)*(char *)(uStack_50 + 0xb4) + uStack_88;
      uStack_10 = FUN_1000_8886(unaff_CS,uStack_18,uStack_1c);
      if ((-1 < (int)uStack_10) && (uStack_10 != uStack_e6)) {
        unaff_CS = 0x181f;
        uVar14 = FUN_1000_8c28(0x181f,uStack_e6,uStack_10);
        if ((uVar14 & 0x40) != 0) {
          *(undefined1 *)(param_2 * 0x1c + 0x314c) = 0;
          break;
        }
      }
      unaff_CS = 0x181f;
    }
  }
  iVar15 = param_2 * 0x1c;
  if (((*(char *)(iVar15 + 0x314c) == '\v') &&
      (*(char *)(iVar15 + 0x314d) == *(char *)(iVar15 + 0x3144))) &&
     (*(char *)(iVar15 + 0x314e) == *(char *)(iVar15 + 0x3145))) {
    if (((0xc < *(byte *)(iVar15 + 0x3146)) && (*(byte *)(iVar15 + 0x3146) < 0x13)) &&
       (*(char *)(iVar15 + 0x314b) == '1')) {
      *(undefined1 *)(iVar15 + 0x314b) = 0x42;
    }
    FUN_1000_8b24(unaff_CS,param_2);
  }
  return 0;
}

```
