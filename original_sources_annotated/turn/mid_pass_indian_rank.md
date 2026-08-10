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

## Calendar string table (reconfirmed)

| String | Asm locus | FUN_* XREF |
|--------|-----------|------------|
| `MULTINEXT` | CODE_178 string table | **none** |
| `TIMECHANGE` | same | **none** |
| `SEASONS` | same | **none** |

Behavior recovered from `@TIMECHANGE` + `130d` year/autumn math — see
[`year_loop.c`](year_loop.c) / [`docs/turn_between_players.md`](../../docs/turn_between_players.md).
