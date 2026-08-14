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
still unmapped), halved when `unit+0x315b` (profession) `== 0x14` (not
independently identified — sibling profession codes `0x15/0x18/0x19/
0x1a/0x1b/0x1c` are named elsewhere for other skills, so `0x14` is
plausibly Hardy Pioneer, not confirmed). Once the counter hits the
threshold: orders clear, and — gated on live colony count
(`DS:0x539e`) and the unit's nation matching the bound colony's owner —
a reward lands: case 9 is a flat `+10` to that colony's
`hammers_purchased` (`col1_save.h`'s `+0x98` field, already named, matches
`§2d13` below); case 8 is a scaled reward from a paired terrain table at
`DS:0x2f80` (stride `0x10`, offset +8 from the case-9 table), gated by a
founding-father check (`FUN_281f_09fc(0x24)`, FF id `0x24` not yet named)
and a per-colony "last granted" turn stamp at colony-record `+0xa4` (not
in `col1_save.h` yet), credited via a 32-bit gold-add helper.

**Case 7** (`FUN_479b_076e`) is only the *top level* of Europe hire —
clears timer fields, calls `FUN_291f_09b2` for the actual hire pick, then
a UI-notify chain. `FUN_291f_09b2` (not traced this pass) is very likely
where the "deep case-7 economy OPEN" content the rest of this file
discusses actually lives.

**Case `0xb`/`0xc`** (`FUN_479b_0972`) is the entry into ship/land act —
short pre-check, calls `FUN_2a1f_0210`/`FUN_291f_044e`/`FUN_2a1f_0142`
(goto/move drivers, not traced this pass), terrain re-check on arrival,
then falls into the same `default` thunk before clearing state.

**Not yet mapped, flagged for a follow-up**: the full 16-byte-per-terrain-
class content of `DS:0x2f78`/`0x2f80` (only offset +2 named so far);
`FUN_281f_09fc(0x24)`'s founding-father identity; colony-record `+0xa4`;
`FUN_291f_09b2` (case 7's real hire-pick body); `FUN_2a1f_0210`/
`FUN_291f_044e`/`FUN_2a1f_0142` (case `0xb` move drivers).

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

### 2d. Linux thin — Pioneer tools delivery (case 7 economy stand-in)

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
- Explicit **OPEN** remainder for land combat / case 7 hire (thin tools-delivery today)
- Ocean naval `20e6` + full line-by-line still R5 / PARKED
- `SYMBOL_MAP` + catalog `links` updated
