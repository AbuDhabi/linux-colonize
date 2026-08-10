# Mid-pass Indian tables + Euro rank

DOS year-loop mid-pass (before EN..DU nation loop), not Linux's full
`TURN_PROC_INDIAN`. Bridge: [`between_turns.md`](between_turns.md).

---

## `FUN_5bfb_00f8` — rank Euro nations

| Item | Value |
|------|-------|
| Lines | **96506–96531** (~26) |
| Thunk | `FUN_281f_0550` (from `130d` mid) |

### Algorithm

1. Identity perm at `DS:0xa150`: `perm[i]=i`
2. Score[i] = `gold/100` + `2*colony_counts[i]` + pop proxy + land combat strength
3. Sort scores + perm (`291f_0ed0`→`1cf8_000a`)
4. Write inverse rank: `DS:0x917c[perm[i]] = i`

**Linux:** no full rank-table writer. Thin naval-weight comments in
`ai_diplo.c` only. Mid-pass rank **PARKED** vs batch-after-human pipeline.

---

## `FUN_4d56_1b3a` — mid-turn Indian tables

| Item | Value |
|------|-------|
| Lines | **81684–81738** (~55) |
| Thunk | `FUN_281f_0676` |
| Does **not** call `2154` / `2820` / `4528` / `1816` |

### Phases

| # | Role |
|---|------|
| 1 | Clear `DS:0x5b04` tables — 8 Indian × 4 Euro words |
| 2 | For Indian nations 0..7: if tribe flags clear bit7, `FUN_41f2_0266` (growth/message chrome probe) |
| 3 | For each colony: bind; for each worked ring tile with owner nibble: if tile owner is Indian (>3) and ≠ colony owner, and no tribe presence → `281f_0704` stamp ownership toward colony nation |

**Linux:** ownership/growth folded into `ai_indian_nation_turn` /
`ai_grow_villages` — not a separate mid-pass. Reshape.

Related: [`indian_contact.md`](../ai/indian_contact.md).

---

## `FUN_4d56_1816` call-site XREF (reconfirmed 2026-08-10)

| Probe | Result |
|-------|--------|
| `rg FUN_4d56_1816` in `.c` | **Definition only** @81543 — zero call sites |
| Same in `.asm` | Label `FUN_4d56_1816` only; **no CALLF/JMPF** to `4d56:1816` (false hits: `2f2b` locals, `5952:1816` jump) |
| Mid-pass sibling | `281f_0676` → **`1b3a`** (resolved) |
| Other `4d56` thunks | `2154`, `2820`, `4528`, … — **not** `1816` |

**Verdict:** call site still **unresolved** in this Ghidra export (overlay /
missing far-ptr). Body annotated in [`indian_nation_turn.c`](../ai/indian_nation_turn.c).
Linux runs `ai_indian_nation_turn` (`1816`-shaped) in `TURN_PROC_INDIAN`;
DOS resolved `130d` only shows mid-pass **`1b3a`**.

### Working assumption (not an XREF)

`1816` is a full nation turn (indian slot `0..7` → active `slot+4`, chrome,
alarm, growth, relation, Brave act loop) — the Indian twin of
`521d_6d8e`, **not** a child of `1b3a`. Every other year-loop heavyweight has a
`281f_*` far thunk; **none** targets `1816`. So the gap is likely a lost
thunk / overlay far-ptr, not dead code.

**Best guess for the missing caller:** `FUN_130d_0290`, with
`for indian_slot in 0..7: far_thunk(1816, slot)`, either:

1. Mid-pass, immediately after `1b3a` (manual “natives first”; adjacency of
   `1816`/`1b3a` in segment `4d56`), or
2. After the Euro 0..3 EOT+act loop, before calendar (Linux
   `TURN_PROC_INDIAN` order).

Treat as **hypothesis only** until a CALLF / thunk recovers. Do not invent a
call edge in extracts or the catalog. Linux’s post-`6a09` pulse is
`1816`-*shaped* for golden init; that does not prove DOS init called `1816`.

Brave edge cases: unknown parent / order can skew LCG and “when Indians act
vs Euros,” which may feed dir/peel mismatches. It does **not** explain the
remaining seed-100 **spent-only** holdouts (post-`465b` `0x3149` writer —
[`docs/seed100_brave.md`](../../docs/seed100_brave.md)).

## Calendar string table (reconfirmed)

| String | Asm locus | FUN_* XREF |
|--------|-----------|------------|
| `MULTINEXT` | CODE_178 string table | **none** |
| `TIMECHANGE` | same | **none** |
| `SEASONS` | same | **none** |

Behavior recovered from `@TIMECHANGE` + `130d` year/autumn math — see
[`year_loop.c`](year_loop.c) / [`docs/turn_between_players.md`](../../docs/turn_between_players.md).
