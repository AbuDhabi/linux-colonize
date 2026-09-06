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
| Does **not** call `2154` / `2820` / `4528`; **does** call `1816` (phase 2, see below) |

### Phases

| # | Role |
|---|------|
| 1 | Clear `DS:0x5b04` tables — 8 Indian × 4 Euro words |
| 2 | For Indian slots 0..7: if `DS:0x5ad9 + 0x4e*slot` (tribe flags) bit7 clear → **`FUN_4d56_1816(slot)`** — the full Indian nation turn. Ghidra labels the call `FUN_41f2_0266`; that is a misresolve (see "Dispatcher — resolved") |
| 3 | For each colony: bind; for each worked ring tile with owner nibble: if tile owner is Indian (>3) and ≠ colony owner, and no tribe presence → `281f_0704` stamp ownership toward colony nation |

**Linux (2026-09-06d — phases 1 and 3 now real, was "Reshape"):**
`ai_indian_midpass_clear_tables` and `ai_indian_midpass_claim_worked_tiles`
(`ai.c`), called from `turn.c` immediately before native slot 4 and
immediately after slot 11 — the same bracket DOS uses. Phase 2 is the
`TURN_PROC_INDIAN` cursor loop itself.

- Phase 1's `DS:0x5b04` decodes as `0x5ad6 + 0x2e` = `indian.contact_state[4]`
  (Indian record base, stride `0x4e` = `0x27` words). **This makes
  `contact_state` a per-YEAR latch, not the permanent one `ai_contact.c`'s
  gift/beg arm assumed** — DOS wipes all 32 words every year, which is why
  villages keep visiting instead of going quiet forever.
- Phase 3 callees: `281f_06dc` = `137f_0200` (owner nibble), `281f_06d2` =
  `137f_0428` (settlement `layer2 0x02` **or** unit `0x01` present),
  `281f_0704` = `137f_0228` (owner stamp), `281f_0c5e` = `15eb_0470`
  (`min(FUN_15eb_039e(10), 2) + 2` → `DS:0x329` ring tier). Net effect: a
  colonist actually *working* a native-owned plot silently transfers that
  plot. `0704`'s @SEIZURE popup arm cannot fire here — it needs a native
  settlement on the tile, which the `06d2` gate already excluded.
- Trap: the DS ring tables are `DS:0xc8` = **dx**, `DS:0xde` = **dy** — the
  reverse of `colonist_work_plot_28c8.md`'s labels (corrected there). Note the DOS order:
all eight Indian nation turns run **inside the mid-pass, before** the Euro
0..3 loop; Linux `TURN_PROC_INDIAN` now runs before `TURN_PROC_EURO` too (2026-08-27).
**2026-08-28 correction for the Linux pipeline:** "before the Euro loop" is
per year tick; the human's Move Pieces is *inside* that loop, so seen from
the human's end of turn the order is Euro slots above the human → Indians →
slots below. Linux now splits `TURN_PROC_EURO` around `TURN_PROC_INDIAN`
accordingly (`turn.c` `turn_human_slot`). The `DS:5394 = 3` dump evidence
is consistent with either reading.

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

### Dispatcher — resolved 2026-08-27 (static)

```
FUN_130d_0290 mid-pass
  → CALLF 281f:0676            (resident thunk)
  → FUN_4d56_1b3a phase 2
      for slot 0..7:
        if !(byte[DS:0x5ad9 + 0x4e*slot] & 0x80):
          PUSH slot; PUSH CS; CALL 4d56:4c2c      ; emulated far call
  → 4d56:4c2c  JMPF 1a1f:03b0   (overlay-local export stub, raw seg 1a1f = Ghidra 2a1f)
  → bank record 281f:23b0 (file 0x1C9A0): CALLF loader; JMPF 0000:1816 ; ovl 0x0C
  → FUN_4d56_1816(slot)
```

Why it was invisible: overlay `0x0C` reaches its own public routines through a
15-entry `JMPF` stub table at `4d56:4c22..4c6c` (5-byte stride). The stubs'
`JMPF` targets are reloc-`0000` records, so Ghidra resolved the near call at
`4d56:1b84` to an unrelated label inside `FUN_41f2_0266` and never created a
function at `2a1f:03b0` (`address_mapping.csv` therefore lists no record).
Raw bytes at `4d56:1b76`: `ff 76 f0 0e e8 a5 30 83 c4 02` =
`PUSH [BP-10]; PUSH CS; CALL 4c2c; ADD SP,2`.

The live hang `dump_1930_2` already agreed: pre-loader `[SS:SP]` =
`CC81:1B87` = `4d56:1b84 + 3`, `[SS:SP+4]` = slot `0`, `DS:5394` = 3 (still
the last Euro slot — the mid-pass runs before the Euro loop). Earlier notes
dismissed it as alias noise by mapping `CC81` onto a `CC89` load from a
different dump.

Full stub-table map (all `PUSH CS; CALL` sites in `4d56`, Ghidra targets
for these are wrong):

| Stub | Record | Real target | Callers in `4d56` |
|------|--------|-------------|-------------------|
| `4c22` | `281f:1248` | `4d56:00e0` | `0209` |
| `4c27` | `281f:23a4` | `4d56:1ec4` | `4b70` |
| `4c2c` | `281f:23b0` | **`4d56:1816`** | **`1b84` (in `1b3a`)** |
| `4c31` | `281f:23bc` | `4d56:14fe` | `1ac4` (in `1816` act loop) |
| `4c36` | `281f:23c8` | `4d56:417e` | `4b80` (in `4528`) |
| `4c3b` | `281f:23d4` | `4d56:021a` | `1506` |
| `4c40` | `281f:23e0` | `4d56:152e` | `19b5` (in `1816` growth loop) |
| `4c45` | `281f:23ec` | `4d56:1c5a` | `4b5d` |
| `4c4a` | `281f:23f8` | `4d56:359c` | `4b39` |
| `4c4f` | `281f:2404` | `4d56:3e20` | `4bb4` |
| `4c54` | `281f:2410` | `4d56:0000` | `0086` |
| `4c59` | `281f:241c` | `4d56:39ea` | `4b90` |
| `4c5e` | `281f:2428` | `4d56:3646` | `3aae`, `4ba4` |
| `4c63` | `281f:2434` | `4d56:2154` | `28df`, `36ac`, `4060` |
| `4c68` | `281f:244c` | `4d56:2820` | `363e`, `3ac4`, `4b23` |

Historical static map that led to the forge-edge probes
([`vr_1554.md`](../../tools/brave_dump/vr_1554.md)):

| Fact | Detail |
|------|--------|
| `1930:1554` | Only `JMP 1446` (overlay epilogue) — not a call target |
| Forge | **`1930:2A02`** XCHG’s real far ret on `[SS:SI]` → `1930:1554`; queues real ret at `[CS:3952]` |
| Thunk | File `0x1C9A0` trailer overlay id **`0x0C`** |
| Dead path | `15CF` `CALLF *:1816` skipped by `EB 3D` at `15CD` |

No static `CALLF *:2430` in dumps; thunk-entry hang stack was alias noise
([`vr_1930.md`](../../tools/brave_dump/vr_1930.md)).

Edge is now proven above: `130d → 0676 → 1b3a → 1816(slot)`, Indians
**before** the Euro 0..3 loop each year tick. This fixes the parent/order
question; it still does **not** explain seed-100 **spent-only** holdouts
(post-`465b` `0x3149` — [`docs/seed100_brave.md`](../../docs/seed100_brave.md)).

The `VR_2A02` forge-edge probe series (v1–v16, [`vr_1554.md`](../../tools/brave_dump/vr_1554.md))
is closed — no further live work needed for this XREF.

## Calendar string table (reconfirmed)

| String | Asm locus | FUN_* XREF |
|--------|-----------|------------|
| `MULTINEXT` | CODE_178 string table | **none** |
| `TIMECHANGE` | same | **none** |
| `SEASONS` | same | **none** |

Behavior recovered from `@TIMECHANGE` + `130d` year/autumn math — see
[`year_loop.c`](year_loop.c) / [`docs/turn_between_players.md`](../../docs/turn_between_players.md).
