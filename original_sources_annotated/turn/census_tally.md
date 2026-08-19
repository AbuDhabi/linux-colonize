# Census + profession tally (`FUN_4962_*`)

Hosted by `FUN_3844_00f2` (`291f_0a74` / `291f_0a9e`). Also year-end chrome
census of human/crown. Bridge: [`between_turns.md`](between_turns.md).

---

## `FUN_4962_0018` — census

| Item | Value |
|------|-------|
| Lines | **78111–78327** (~217) |
| Thunk | `FUN_291f_0a74` |
| Arg | `param_1` = nation 0..3 |

### Phases

| # | Role |
|---|------|
| 1 | Zero nation counters + `unit_type_counts[19]` + cargo/continent scratch |
| 2 | **Unit loop**: type tallies, ships vs land, Europe-dock proxies, continent OR bits `0x95f2`, combat/`0x941c`, free colonist, field combat; foreign Euro near continent → OR `0x02`; also (2026-08-14) the nation×continent six-table block below |
| 3 | **Colony loop**: `0x9298++`, pop sum; clear `colony+0x1b` bits 0/1; 11×11 ship probe → **Frigate/other-armed bits (not "MoW/armed" — corrected 2026-08-19, see below)**; globals `0xa89a`/`0xa89b`; foreign colony continent OR `0x04`. Mechanism fully traced 2026-08-19 (raw `viceroy_unpacked.c:78243-78304`, cross-checked live via DOSBox-X): for each **own** colony, scan an 11×11 box (`-5..+5` both axes) around it; for each in-bounds tile holding a **foreign** unit, walk that tile's stack; a unit counts only if `+0x3146` (type) is `0x0d..0x12` (the ship roster, confirmed against `NAMES.TXT` `@UNIT` file order: Caravel/Merchantman/Galleon/Privateer/Frigate/Man-O-War) **and** `DS:0x5236+type*0xE` (`unit_type_combat_byte`) is nonzero, **and** a `FUN_2a1f_027e(seg,8,1,colony_y)` check lands in `0..5`. **Identity resolved 2026-08-19 by reading the source, not guessed from behavior**: `FUN_2a1f_027e` thunks to `FUN_6662_0906` (already catalogued in `FUNCTION_CATALOG.md` as "Short-range goto cost... when both axes <8"). It's a **movement/pathfinding cost check, not a diplomacy gate** — confirmed by reading its body (`viceroy_unpacked.c:104150-104198`): computes axis deltas against implicit-register coordinates, and if both are `<8` (always true here, the 11×11 probe's own deltas never exceed 5) calls a real pathfinder (`thunk_FUN_2a1f_05f0`) and returns its cost. The `0..5` check at the call site is "is there a short-enough navigable route between ship and colony" — filters ships that are geometrically close but land-blocked (opposite side of a peninsula), not a relation/hostility filter at all. This cleanly explains three separate live captures all passing regardless of diplomatic state (peace/alliance/war all confirmed to fire, see below) — diplomacy was never the variable, so those three data points, while individually solid, weren't actually testing what I thought they were. `thunk_FUN_2a1f_05f0`'s own cost formula is still unresolved. Then `colony+0x1b \|= 2` if `type==0x11` (**Frigate**) else `\|= 1`. **Privateer status: genuinely open, not excluded, not confirmed included either** — corrected twice same day. First read ("excluded") came from a "didn't fire" report with no debug capture attached — never independently verified `0xa89a`/`0x9e54` were even armed for that attempt, or that it was the same colony slot. Second read ("non-deterministic," comparing that report against a later confirmed hit) was also overreach — the comparison wasn't apples-to-apples since one side was never instrumented. What's actually confirmed: one instrumented, confirmed-armed capture (`0xa89a` 00→01, colony pointer `bx=65F4`, matching the earlier mixed-ship test's own colony pointer, at peace) shows a Privateer-only group **can** set bit0. No confirmed instrumented negative exists. Type-range (`0x0d..0x12`) and the `type==0x11`→bit1/else→bit0 split are solid (literal code); only "which types actually pass `FUN_2a1f_027e`" stays open — next attempt should arm all five breakpoints (`5D61`/`A89A`/`A89B`/`9E52`/`9E54`) and confirm the breakpoint list + colony pointer *before* concluding either way. Reset: unconditional at the top of every `FUN_4962_0018` call (once per nation, confirmed both by reading the raw zero-out at 78239-42 and live) |
| 4 | **Tribe loop**: continent OR `0x01` |
| 5 | Empty-colony continent bump `0x9650`; **mean pop** `0x944e = pop_sum / colonies` |

### Key DS (stuff window)

| Addr | Field |
|------|-------|
| `0x8cfc` | `all_unit_counts[4]` |
| `0x9298` | `colony_counts[4]` |
| `0x9408`…`0x942c` | free / pop / proxy / land / ship / armed / field |
| `0x941c` | `land_combat_strength[4]` |
| `0x924c` | `unit_type_counts[4][19]` |
| `0x944e` | mean colony pop |
| `0x95f2` | continent AI flag bytes (bitmask meaning fully resolved 2026-08-14, see below) |
| colony `+0x1b` | ship-pressure bits (Linux `ai_flags`) |

**Nation×continent block, resolved 2026-08-14** (this phase-2 sub-work was
previously undetailed here — full trace in
[`euro_g_table_0a60.md`](../ai/euro_g_table_0a60.md), which is what this
function turned out to feed): `0x94a6`
`land_unit_counts_by_continent`, `0x94e6` `colony_counts_by_continent`,
`0x9526` `skilled_unit_counts_by_continent`, `0x918c`
`unit_value_sum_by_continent`, `0x9572` `combat_value_sum_by_continent`,
`0x95b2` `field_combat_strength_by_continent` — all `[continent+nation*0x10]`
stride, confirmed via raw `.asm` register tracing of the `FUN_4962_0006`
call sites (the decompiler drops the implicit `AX`/`BX` args, but the
preceding `MOV AX,.../MOV BX,...` instructions are plain in the `.asm`).
`0x95f2`'s bitmask: bit1 any Indian tribe present, bit2 foreign Euro unit
present, bit4 foreign colony present, bit8 own exposed combat-capable land
force. These six tables plus `continent_tally_b` (`0x85c8`) are the full
data-table dependency list for the deep `−0x6790` G-table
(`FUN_521d_0a60`), now ported as `ai_euro_refresh_continent_stance` in
`ai_euro.c` (recomputes fresh each call rather than sharing this function's
own per-turn pass — see `euro_g_table_0a60.md` for why).

Helper: `FUN_4962_0006` — saturating +1 to 255.

### Linux

| DOS | Linux | Fidelity |
|-----|-------|----------|
| Full EOT census | `col1_stuff_census_refresh_colony_counts` in SETUP | Colony + unit/combat tallies **Done** thin |
| Blank-template fill | `col1_stuff_census_fill_blank` | **Partial** |
| Colony `+0x1b` ship bits | `ai_euro_refresh_colony_ai_flags` | Thin |
| Profession hist `0606` | `turn_tally_professions` → `ctx->profession_tally[4][32]` | **Done** thin (all Euro SETUP) |

---

## `FUN_4962_0606` — profession tally

| Item | Value |
|------|-------|
| Lines | **78332–78373** (~42) |
| Thunk | `FUN_291f_0a9e` |
| Arg | nation nibble |

### Phases

1. Zero `DS:0x9430[0x1d]` profession histogram  
2. Units of nation with specialty (`281f_0b78`): `hist[unit+0x315b]++`  
3. Colonies: each colonist job (`281f_0c54`) → `hist[job]++`

### Linux

Runtime **`turn_tally_professions`** fills `ctx->profession_tally[4][32]` in SETUP
for every Euro nation (colonist jobs + unit professions). DS:0x9430 RMW writer
still **PARKED**.

**Order in `3844_00f2`:** `0606` → Europe EOT `5e52` → … → production →
census `0018` → king `2424`.
