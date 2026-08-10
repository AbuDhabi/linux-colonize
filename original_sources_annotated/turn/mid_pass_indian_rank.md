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

**Linux:** `turn_rank_euro_nations` **Done** thin — fills
`ctx->euro_power_rank[]` in `TURN_PROC_SETUP` (0 = strongest). Live colony/pop
+ gold/100; live `land_combat_strength` via census refresh. Diplo military score
uses place. Not a save DS:0x917c writer (RMW layout PARKED).

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

## `FUN_4d56_1816` — Indian nation turn (live; overlay-dispatched)

Body annotated in [`indian_nation_turn.c`](../ai/indian_nation_turn.c).
Linux: `ai_indian_nation_turn` in `TURN_PROC_INDIAN`.

### Mapped (hang dumps 2026-08-10)

| Item | Value |
|------|-------|
| Role | Full Indian nation turn (slot `0..7` → active `slot+4`; chrome, alarm, growth, relation, Brave act loop) — twin of `521d_6d8e`, **not** a child of `1b3a` |
| Live | **Yes** — [`vr_1816.md`](../../tools/brave_dump/vr_1816.md) hang in body (`param_1=0` on Brave seed) |
| Entry | Resident thunk file **`0x1C9A0`** / mem `~0x22858`: `CALLF` overlay loader (`1930:0E52`) → `JMPF 4d56:1816` |
| Far return | Always **`1930:1554`** (overlay **Return Vector**) — same inside body and after loader returns ([`vr_0e52.md`](../../tools/brave_dump/vr_0e52.md)) |
| Static Ghidra | Definition-only @81543 — no `CALLF`/`JMPF` to `4d56:1816`; no `281f_*` thunk to `1816` |
| Mid-pass | `281f_0676` → **`1b3a` only** (tables); does not call `1816` |

### Still open — dispatcher XREF

Who **invokes** the thunk is **not** yet a recovered year-loop `FUN_*`.
Static map ([`vr_1554.md`](../../tools/brave_dump/vr_1554.md)):

| Fact | Detail |
|------|--------|
| `1930:1554` | Only `JMP 1446` (overlay epilogue) — not a call target |
| Forge | **`1930:2A02`** XCHG’s real far ret on `[SS:SI]` → `1930:1554`; queues real ret at `[CS:3952]` |
| Thunk | File `0x1C9A0` trailer overlay id **`0x0C`** |
| Dead path | `15CF` `CALLF *:1816` skipped by `EB 3D` at `15CD` |

No static `CALLF *:2430` in dumps; thunk-entry hang stack was alias noise
([`vr_1930.md`](../../tools/brave_dump/vr_1930.md)).

Do **not** invent a `130d → 1816` edge. Order vs Euro EOT remains
**hypothesis**. Unknown parent/order can skew LCG timing; it does **not**
explain seed-100 **spent-only** holdouts (post-`465b` `0x3149` —
[`docs/seed100_brave.md`](../../docs/seed100_brave.md)).

**Narrowed edge:** RETF peels abandoned (v7–v12). v13 = forge `2A4D` skip
`1930:238B` + `CCx81` — [`vr_1554.md`](../../tools/brave_dump/vr_1554.md).

## Calendar string table (reconfirmed)

| String | Asm locus | FUN_* XREF |
|--------|-----------|------------|
| `MULTINEXT` | CODE_178 string table | **none** |
| `TIMECHANGE` | same | **none** |
| `SEASONS` | same | **none** |

Behavior recovered from `@TIMECHANGE` + `130d` year/autumn math — see
[`year_loop.c`](year_loop.c) / [`docs/turn_between_players.md`](../../docs/turn_between_players.md).
