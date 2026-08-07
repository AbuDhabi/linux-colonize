# AI Transcription Gap

Source of truth for European / Indian AI: what DOS `VICEROY` implements, what
the Linux port has transcribed, and what remains for eventual **1:1
correspondence** with the original planners.

Navigation / bring-up status live elsewhere; this file owns the AI FUN_*
inventory and roadmap. See also [original_index.md](original_index.md),
[decomp_inventory.md](decomp_inventory.md), [manual_gap.md](manual_gap.md),
[data_vs_hardcoded.md](data_vs_hardcoded.md).

---

## Goal and fidelity tiers

**Long-term goal:** every original AI control-flow path that affects game state
has a Linux counterpart with matching behavior, including DOS LCG call order
where the original burns RNG.

| Tier | Meaning | Current use |
|------|---------|-------------|
| **T0 — Behavioral slice** | Looks like the original at a high level; RNG / edge cases may differ | Euro sail-to-goto, village growth |
| **T1 — Save-diff** | Matches observable fields in original saves after the same setup | Rival fleets / crosses vs `COLONY00`→`01` |
| **T2 — Golden / bit-faithful** | Matches a locked golden (e.g. seed-100) tile-for-tile / unit-for-unit | Tribe/Brave pulse + early AI TURN1→7 (`smoke_ai_turns`) |
| **T3 — 1:1 transcription** | Structured like the decomp (dispatcher → goals → scoring), all branches | **Not claimed** for any full planner |

“Limited fashion” in the roadmap means ship a **T0/T1** slice first (e.g. unload
and found), then harden toward **T2/T3** with dumps and goldens.

**Port rule:** AI algorithms are baked into C from VICEROY decomp (not data
files). Prefer golden saves and DOS hang dumps over wiki. Use
[`dos_rng.c`](../src/core/dos_rng.c) for any path that must match seed-100 or
save-diff. Split `ai.c` into `ai_euro.c` / `ai_indian.c` when size warrants.

---

## Current Linux surface (Full T0/T1)

| Piece | Role |
|-------|------|
| [`src/core/ai.h`](../src/core/ai.h) / [`ai.c`](../src/core/ai.c) | Init, seed-100 early Euro fixture, Indian pulse |
| [`ai_goals.c`](../src/core/ai_goals.c) | Primary/secondary/work goal tables (`521d_0000…0906`) |
| [`ai_euro.c`](../src/core/ai_euro.c) | Full dispatcher: plan/`5d04` hire, `0a60` goals, `5b66` act, Euro `20e6` step |
| [`ai_diplo.c`](../src/core/ai_diplo.c) | Bilateral `15b3` + `5bfb` war/ally (`euro_diplo.md` partial structural) |
| [`ai_contact.c`](../src/core/ai_contact.c) | Indian prelude/meet/trade/raids (`4d56`/`5bfb`/`@RAID*` partial structural) |
| [`ai_king.c`](../src/core/ai_king.c) | Tax / SoL declare / REF waves / war act (`43f7` partial structural; `king_ref.md`) |
| `ai_init_new_game` | Col1 template, rival fleets, tribes/Braves, post-spawn native pulse |
| `ai_euro_nation_turn` | Reseed, AI crosses; seed-100 fixture **or** `ai_euro_dispatcher_turn` (`AI_FULL_DISPATCH=1` / non-100) |
| `ai_indian_nation_turn` | `1816` phases: prelude → growth → relation → pulse → meet/raids |
| `ai_king_nation_turn` | Replaces king stub in EOT FINISH |
| [`turn.c`](../src/core/turn.c) | `TURN_PROC_EURO` / `INDIAN` / king → AI entries |
| [`tests/smoke/test_ai_turns.c`](../tests/smoke/test_ai_turns.c) | **T2 gate:** `TURN1`→`TURN7` field-diff (`smoke_ai_turns`) |

**Claims (T2 early AI):** with VR_SEED=100 and idle human, `smoke_ai_turns` matches
`test-saves-ai/TURN2`…`TURN7` on calendar, AI crosses, colonies (sites/names/pop/bip/hammers),
euro units (xy/orders/goto), Braves (xy/moves/turns_worked), and tribe pop/accumulators.
Seed-100 Euro path uses landfall-derived coastal staging + found-site helper
(`ai_euro_found_tile_from_landfall`); ship approach / mid-turn waypoints still
fixture unless `AI_FULL_DISPATCH=1`. Mid-turn Braves: quiet ASM + mid peels + residual overlays on
spent-only holdouts (R0; quiet residuals **2 rows** t2 Apache/Sioux;
empiricism keeps its larger overlay set via `AI_EMPIRICISM=1`).

**Claims (Full T0/T1):** Euro dispatcher (goals/hire/act/combat/capture), diplomacy
state, Indian meet/trade/missions/raids, king tax/REF/independence war loop —
behavioral, not bit-perfect. Shared: `colonies_capture`, `units_resolve_naval_combat`.

```mermaid
flowchart TD
  eot[EOT pipeline] --> euro[FUN_521d_6d8e Euro dispatcher]
  eot --> indian[FUN_4d56_1816 Indian nation]
  eot --> king[ai_king_nation_turn]
  euro --> goals[FUN_521d_0a60 / 5d04 / helpers]
  euro --> score[FUN_521d_20e6 move scoring]
  euro --> diplo[Euro diplo 5bfb / 15b3]
  indian --> growth[FUN_4d56_152e growth]
  indian --> unitAct["per-unit act thunk ~0x42191"]
  unitAct --> score
  unitAct --> raids[FUN_4d56_2154 / 2820 / 4528]
  king --> tax[tax / REF funding]
  king --> ref[REF land / war turn]
```

---

## Full T0/T1 surface (acceptance)

**Done means:** every in-scope AI path that mutates game state has a Linux
counterpart that can run end-to-end at **T0** (harden to T1 save-diff where cheap).
Not required: seed-100 mid/late T2 goldens, DOS LCG bit-identity, or polished
meet/king cinematic UI.

| Cluster | Linux entry | Fidelity bar |
|---------|-------------|--------------|
| Euro dispatcher + goals + hire | `ai_euro_dispatcher_turn` (`ai_euro.c`) | **Partial structural** 6d8e; seed-100 fixture unless `AI_FULL_DISPATCH=1` |
| Euro unit act + scoring | `ai_euro_unit_act` / ocean `20e6` branch | **Partial** 5b66 case 0x0b + naval score; land/combat PARKED |
| Diplomacy | `ai_diplo_*` (`ai_diplo.c`) | Bilateral peer bytes + war/ally; see R3.5 |
| Indian nation + contact | `ai_indian_nation_turn` + `ai_contact_*` | Alarm/relations/missions/meet/trade T0 |
| Raids | `ai_contact_indian_raids` | `@RAID*` kinds / friction-gated combat + colony loot |
| King / tax / REF | `ai_king_nation_turn` (`ai_king.c`) | **Partial structural** `2424` peace/war; see R6 |

### Shared surfaces (blocking work by phase)

| Surface | Phase that needs it | Linux |
|---------|---------------------|-------|
| Land combat (AI-initiated) | 2 | `units_resolve_land_combat` + act |
| Colony capture | 2 / 5 / 6 | `colonies_capture` |
| Naval combat T0 | 2 / 6 | `units_resolve_naval_combat` |
| Diplomacy bytes | 3 | `ai_diplo_read` / `ai_diplo_write` |
| Meet / alarm / `@RAID*` | 4–5 | `ai_contact_*` + Col1 tribe alarm |
| SoL / declare / tax events | 6 | `ai_king_*` + nation liberty/tax fields |

R0 Brave spent overlays (**2** quiet rows) stay until a dedicated hang pass — not a
blocker for Full T0/T1.

---

## Original inventory

Addresses are Ghidra symbols in
[`original_sources_decompiled/viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c).
Line spans are approximate (next function start − 1). Status:

| Status | Meaning |
|--------|---------|
| **ported** | Linux counterpart with T1/T2 claim |
| **partial** | Subset of behavior only |
| **parked** | Known cluster; not transcribed |
| **unknown** | Needs RE labeling before port |

### Tribe placement — `FUN_6a09_*`

| Symbol | ~Lines | Purpose | Linux | Status |
|--------|-------:|---------|-------|--------|
| `FUN_6a09_0006` | ~335 | Capitals, satellites, Brave spawn loop | `ai_place_tribes_*`, `ai_spawn_brave_near` | **ported** (T2 seed-100) |

### Indian AI — `FUN_4d56_*` (12 far + nested helpers)

| Symbol | ~Lines | Purpose (known / inferred) | Linux | Status |
|--------|-------:|----------------------------|-------|--------|
| `FUN_4d56_0038` | ~39 | Small helper; calls into `00e0` / map probes | contact helpers | **partial** (T0 via contact) |
| `FUN_4d56_00e0` | ~60 | Chains to `01e2` / `14fe` | contact helpers | **partial** (T0) |
| `FUN_4d56_01e2` | ~19 | Thin wrapper → `14fe` | — | **partial** (T0) |
| `FUN_4d56_14fe` | ~16 | Dispatches growth `152e` | growth + pulse | **partial** (T0/T2 quiet) |
| `FUN_4d56_152e` | ~156 | Village growth accumulator → pop++ | `ai_grow_villages` | **partial** (T0) |
| `FUN_4d56_1816` | ~141 | Indian nation turn entry: alarm prelude, unit loop, relation ticks | `ai_indian_nation_turn` + `ai_contact_*` | **partial** (structural; T2 quiet) |
| `FUN_4d56_1b3a` | ~59 | Mid-turn: clear tables / tribe + colony ownership probes (does **not** call `2154`) | — | **partial** (known; not raid) |
| `FUN_4d56_2154` | ~321 | Larger Indian action body; from `5bfb` via `2a1f_0434` | `ai_contact_indian_raids` (thin) | **partial** (structural) |
| `FUN_4d56_2820` | ~1396 | Heavy decision + nested trade `2aac…311e` | meet/trade auto-haggle | **partial** (T0; deep PARKED) |
| `FUN_4d56_2aac`…`311e` | nested | Trade buy/haggle/demand helpers | `ai_contact_indian_meet_trade` | **partial** (auto only) |
| `FUN_4d56_3582` | ~51 | Small helper after `2820` | — | **parked** |
| `FUN_4d56_417e` | ~188 | Mid-size helper | — | **parked** |
| `FUN_4d56_4528` | ~3073 | Settlement enter/raid | `ai_contact_indian_raids` + `@RAID*` kinds | **partial** (structural outcomes) |

Nation entry `1816` does **not** call `2154`/`2820`/`4528` directly in the
decomp slice; those are reached from other turn / contact paths. Full Indian
AI = `1816` + unit-act thunk + those large bodies + `@RAID*` data.

### European AI — `FUN_521d_*` (25 far + nested)

| Symbol | ~Lines | Purpose (known / inferred) | Linux | Status |
|--------|-------:|----------------------------|-------|--------|
| `FUN_521d_0000`…`0906` | small | Goal-table ops + founding helpers | `ai_goals.c` | **partial** (T0 ported) |
| `FUN_521d_0a60` | ~858 | Unit / colony goal writer | `ai_euro_colony_goals` | **partial** (T0 A–H condensed) |
| `FUN_521d_20c6` | nested | Near helper before scoring | scoring step | **partial** (T0) |
| `FUN_521d_20e6` | ~2180 | Direction / move scoring (all unit kinds) | quiet + `ai_euro_score_step` | **partial** (T0 Euro/ocean; T2 quiet) |
| `FUN_521d_5b66` | ~1815 | Euro **per-unit act** (separate far; often → `20e6`) | `ai_euro_unit_act` | **partial** (T0) |
| `FUN_521d_5c38` / `5c3c` / `5cf6` | small | Thin helpers before `5d04` | hire in planning | **partial** (T0) |
| `FUN_521d_5d04` | ~748 | Nation planning / hire / treasury (6d8e via `0554`) | `ai_euro_nation_planning` | **partial** (T0) |
| `FUN_521d_6d8e` | ~253 body | Euro AI **dispatcher** per nation | `ai_euro_dispatcher_turn` (+ seed-100 fixture) | **partial** (structural; T2 fixture) |

`6d8e` thunk wiring: `0554`→`5d04`, `0578`→`0342`, `050c`→`0a60`, `0488`→`5b66`
(→`20e6` via `04f4`). Linux still uses seed-100 peels until goal/act bodies port.

**Naming note:** `5b66` is **not** nested inside `20e6` and is not the unit-goals
entry — goals ≈ `0a60` + `5d04`; scoring ≈ `20e6`; act ≈ `5b66`.

### Diplomacy — `FUN_15b3_*` / `FUN_5bfb_*` (Euro war/ally)

| Symbol | ~Lines | Purpose | Linux | Status |
|--------|-------:|---------|-------|--------|
| `FUN_15b3_0004` / `0032` | small | Bilateral read/write (Euro×`0x13c`+peer) | `ai_diplo_read` / `write` | **partial** (structural) |
| `FUN_15b3_0066` / `00d0` | small | OR/clear both directions | `ai_diplo_or_both` / `clear_both` | **partial** |
| `FUN_5bfb_10ec` | ~63 | War/ally eligibility | `ai_diplo_euro_balance` | **partial** |
| `FUN_5bfb_13b0` | ~61 | Form/break alliance | `form_alliance` / `break_alliance` | **partial** |
| `FUN_5bfb_153e` | ~1112 | Large war-declare body | — | **parked** |
| `FUN_4cc6_00f2` | — | Indian relation delta | `ai_diplo_indian_relation_delta` | **partial** |

Thin map: [`euro_diplo.md`](../original_sources_annotated/ai/euro_diplo.md).

### King / REF — `FUN_43f7_*`

| Symbol | ~Lines | Purpose | Linux | Status |
|--------|-------:|---------|-------|--------|
| `FUN_43f7_0004` | ~42 | Pop-weighted SoL | `ai_king_sol_percent` | **partial** |
| `FUN_43f7_1d42` | ~64 | Tax→REF funding | `ai_king_tax_event` | **partial** |
| `FUN_43f7_2564` / `1a26` | ~200 / ~140 | Declare gate / crown setup | `ai_king_try_declare` (auto; UI PARKED) | **partial** |
| `FUN_43f7_060a` | ~37 | Landing / garrison score | `ai_king_weakest_port` | **partial** |
| `FUN_43f7_0982` / `06a6` | ~335 / ~106 | REF wave / empty irregulars | `ai_king_ref_wave` | **partial** |
| `FUN_43f7_2022` / `1eca` | ~98 / ~66 | War act + Continental promote | `ai_king_war_act` | **partial** |
| `FUN_43f7_2424` | ~61 | Nation SoL + peace/war dispatch | `ai_king_nation_turn` | **partial** (structural) |
| `FUN_43f7_10f0` / `1528` / `160a` / `2244` | — | Intervene / announce / rename / merc | — | **parked** |

Thin map: [`king_ref.md`](../original_sources_annotated/ai/king_ref.md). Smoke: `smoke_ai_king`.

### Shared move / terrain helpers (AI-adjacent)

| Symbol | Role in AI | Linux |
|--------|------------|-------|
| `FUN_465b_0000` (terrain MP) | Brave step cost | `ai_dos_move_spent` |
| `FUN_124c_0040` | DOS distance helper (not used in `FUN_521d_20e6`) | `ai_dos_dist` — empiricism-only home-dist in `ai_native_pick_dir` (not quiet ASM) |
| `FUN_281f_04ca` / `04d4` | Reseed / `range` | `dos_rng` |
| `func_0x00042191` | Per-unit Indian act from `1816` | No direct symbol; pulse approximates quiet path only |

Quiet dir-pick: [`ai.c`](../src/core/ai.c) labels the slice `FUN_4d56_021a`
(likely a label/offset in the native move path); seed-100 notes cite
`FUN_521d_20e6` quiet path. Same scoring behavior either way.

---

## Correspondence map

Labeled RE copies of several rows live under
[`original_sources_annotated/`](../original_sources_annotated/)
([`SYMBOL_MAP.md`](../original_sources_annotated/SYMBOL_MAP.md)). Prefer those
files when reading control flow; the raw export remains authoritative for
unannotated bodies.

| Original | Linux | Notes |
|----------|-------|-------|
| `FUN_6a09_0006` | `ai_place_tribes_procedural` / `ai_place_tribes_from_txt` / `ai_spawn_brave_near` | AMERICA via `TRIBE.TXT`; NEW WORLD procedural |
| Post-`6a09` native pulse | `ai_native_nation_pulse` at end of `ai_init_new_game` | One action per Brave per indian nation |
| Quiet NEW WORLD dir pick | `ai_native_pick_dir` | Quiet ASM default (stay LCG + init/mid peels). `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0` force emp |
| Apply step + MP | `ai_native_apply_step` / `ai_dos_move_spent` | Annotated `move_spent_add` / accessors |
| `FUN_4d56_152e` growth | `ai_grow_villages` | Threshold `AI_VILLAGE_GROWTH_THRESHOLD` (19); pop cap 15 |
| `FUN_4d56_1816` full body | `ai_indian_nation_turn` | Structural phases (prelude → growth → relation → pulse → meet/raid); quiet T2 overlays; thin maps `indian_contact.md` |
| Per-unit indian act | pulse / residual | Quiet path; residual only on pulse≠golden; DOS thunk `func_0x00042191` → annotated stub `indian_unit_act` |
| `@RAID*` / meet / mission | `ai_contact_*` | **Partial structural** — `@RAID*` loot kinds + `5bfb` meet; deep `2820`/`4528` PARKED |
| `FUN_521d_6d8e` | `ai_euro_dispatcher_turn` / fixture | **Partial structural** 6d8e; T2 seed-100 fixture |
| `FUN_521d_0000`…`0906` | `ai_goals_*` | T0 goal tables |
| `FUN_521d_0a60` | `ai_euro_colony_goals` | T0 condensed phases |
| `FUN_521d_5d04` | `ai_euro_nation_planning` | T0 treasury + Europe hire |
| `FUN_521d_5b66` | `ai_euro_unit_act` | T0 goto/unload/found/combat |
| `FUN_521d_20e6` (non-quiet) | `ai_euro_score_step` | T0 adjacent step toward goal |
| Col1 AI fleets + landfall `goto` | `ai_spawn_euro_fleet` / `ai_pick_landfall` / `ai_sail_ship` | T2 landings on VR_SEED=100 |
| Landfall unload + first colony | `ai_euro_early_turn` / dispatcher unload | **T2** golden towns; T0 dispatcher for other seeds |
| AI crosses tick | `ai_euro_nation_turn` | +2 / needed default 14 |
| King / REF AI | `ai_king_nation_turn` | **Partial structural** `43f7` peace/war; `smoke_ai_king` |
| Diplomacy | `ai_diplo_*` | **Partial structural** bilateral `15b3` + `10ec`/`13b0` balance |
| Colony capture | `colonies_capture` | military / REF / Indian raid |
| Naval combat | `units_resolve_naval_combat` | T0 ship vs ship |

---

## Remaining work roadmap

Ordered from limited playability toward full 1:1. Each row can be its own PR
series; do not skip prerequisite systems in [Prerequisites](#prerequisites).

### R0 — Fidelity debt and doc hygiene (**partial**)

- **Init LCG burns (named helper):** `ai_native_post_first_brave_burns` —
  Inca=6 / Tupi=1 after first Brave step when `seed100_init_burns=true`
  (`smoke_mapgen_seed100`). DOS CALL site still unlabeled (hang dumps B26/B27
  place the mover after `6a09` returns; exact inter-Brave burn not named). Not
  prelude-equivalent to mid-turn Inca=14 / Aztec=4.
- **Mid-turn pulse:** always runs; prelude Inca=14 / Aztec=4; MP loop allows
  spent past max (`097a`). Terrain river costing (`072c` &0x40), tribe-tile
  spend cap (`06be` / layer2&2), own-nation −0x28 (skip only river-into-tribe),
  mask fa-flags (road `layer2 0x40` → DOS `&0x0a`), and ocean-transition
  spent=max emptied **t1** and keep cost fidelity. **Phase 11:** seed-100
  **init and mid-turn** use quiet ASM by default (stay LCG; 13 init peels +
  104 mid-turn peels for matched-RNG scoring holdouts; coarse fog from phase
  9). **Phase 12:** `FUN_465b_0000` annotated in
  [`move_spent.c`](../original_sources_annotated/ai/move_spent.c); ocean/HS
  force-to-max uses `euro_settlement_owner` (`FUN_137f_0358`). **Phase 13:**
  multi-step / Inca tw residuals cleared — river/fa cost=1 peels let the
  existing `097a` pulse loop continue (`spent < 3`); mis-keyed t3/t6 overlays
  retired. **Phase 14–17:** spent-only static RE + dump-free predicates
  exhausted (cost head cannot distinguish T1 spent=9 vs T2 spent=3; ocean
  force-max ruled out via `dump_b465f3`; `dump_vrb465x2` shows spent=9 without
  XY ⇒ writer after ADD/`465b` return; ocean-adj / capital-dist clamps break
  T1). Quiet residuals remain **2 spent-only rows** in `k_quiet_brave_t2`.
  Hang **`VR_B465X` → `dump_b465x3`** is the last-resort localizer (see
  `tools/brave_dump/midturn_465b.md`). Empiricism mid-turn overlays retained
  under `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0`. Complete Map irrelevant.
- **Euro early path:** T2 coastal ship gotos from
  `ai_coastal_staging_from_landfall`; found tiles from
  `ai_euro_found_tile_from_landfall` (Quebec / New Amsterdam / Isabella; T3–T6
  SP pioneer + FR found peels). Dutch join uses first nation colony. Atlantic
  approach table + T3–T6 **ship** coastal waypoints still fixture (west-explore
  `spend_goto` misses golden coastal XY by 1–2 tiles until ocean `20e6`).
  T3 FR ship/unload peeled via found tile (`hold = found+(0,+2)`).
- Keep this file and [original_index.md](original_index.md) status rows aligned
  when slices land.

### R1 — Euro “AI plays” (limited, T0→T1)

**T0 landed:** rivals unload at landfall and found a first colony
(`ai_try_ship_unload` / settle helpers in `ai_euro_nation_turn`;
`units_pick_landfall_tile` / `units_landfall_unload_all`). Optional fortify on
leftover Soldier + Pioneer → Carpenter's Shop. Covered by `smoke_ai`.

**T2 (fixture path):** seed-100 New Amsterdam / Quebec / Isabella via
`ai_euro_early_turn` + `smoke_ai_turns` — not the generic planner.

**Second-wave settle (partial):** full-dispatch unload/found while
`colony_count < 6`, light H-bind founders→FOUND, founders prefer FOUND over
LABOR. Smoke: `smoke_ai_euro_expand`. **Mid-war:** Soldier/Dragoon Europe hire +
one idle military → foreign MILITARY goal (`smoke_ai_euro_war`). Thin **G** stance
(≥2 colonies: war MILITARY prio 6 / peace FOUND bump). Deep −0x6790 table PARKED.

Still open for generic T1 (non-fixture):

1. Save-diff / hang-dump fidelity for first AI town vs original saves.
2. Colony production already runs for all active colonies.

### R2 — Indian nation turn beyond quiet pulse (**partial structural**)

**Linux:** `ai_indian_nation_turn` mirrors annotated `1816` phase order
(prelude → growth → relation → quiet pulse → post-pulse meet/raids). Seed-100
LCG burns stay inside the pulse; prelude uses isolated contact RNG.

**Still open:** alarmed branches inside `14fe`; mid-game quiet scoring
(goods/missions/capital pull); retiring spent overlays.

### R3 — Contact and raids (**partial structural port**)

**Linux:** [`ai_contact.c`](../src/core/ai_contact.c) — `5bfb` meet/auto-trade,
`4528`/`5fef`-shaped raid arms with `@RAID*` loot kinds, `359c` Scout stub;
high-friction raid → `ai_diplo_indian_relation_delta`; peaceful meet friction decay;
Missionary adjacent convert pulse (`tribe.mission` + crosses); teach-skill sets
`tribe.state.learned` (+ optional Expert Farmer / Seasoned Scout).
Thin maps: [`indian_contact.md`](../original_sources_annotated/ai/indian_contact.md),
[`indian_raid_outcomes.md`](../original_sources_annotated/ai/indian_raid_outcomes.md).
Smoke: `smoke_ai_contact`.

**PORT DEBT:** full `2154`/`2820`/`4528` bodies; player meet/trade/raid dialog UI;
skill-from-`@TRIBES` mapping.

### R3.5 — Euro diplomacy (`15b3` / `5bfb`) (**partial structural port**)

**Linux:** [`ai_diplo.c`](../src/core/ai_diplo.c) — peer-correct bilateral flags in
`nation.unknown26[4+peer]` (timers in `[0..3]`); `treaty_timers` can
`break_alliance`; `euro_balance` is `10ec`/`13b0`-shaped; thin `153e` war sting
(100 gold + tax+1 on first declare; 5 gold/turn/peer upkeep). Thin map:
[`euro_diplo.md`](../original_sources_annotated/ai/euro_diplo.md). Smoke:
`smoke_ai_diplo`.

**PORT DEBT:** full `5bfb_153e`, dialogs, FA `3f41`, Indian×Euro `15b3` matrix,
exact DS `−0x77c4` field rename.

### R4 — Euro dispatcher skeleton (**partial structural port**)

**Linux:** `ai_euro_dispatcher_turn` in [`ai_euro.c`](../src/core/ai_euro.c)
mirrors annotated `euro_nation_turn` phases (inventory → treaty timers →
`5d04`/`0342`/`0a60` → `any_acted` waves → sticky → ship CONTACT). Goal upsert /
promote / 16-slot work queue in [`ai_goals.c`](../src/core/ai_goals.c). Seed-100
keeps `ai_euro_early_turn` unless `AI_FULL_DISPATCH=1`.

**Second-wave:** unload/found + light H-bind while `colony_count < 6`
(`smoke_ai_euro_expand`). **Mid-war hire/bind:** Soldier/Dragoon dock hire + one
MILITARY goto (`smoke_ai_euro_war`). Thin G stance (≥2 colonies). **PORT DEBT:**
mid-game `5d04` hire matrix; deep −0x6790 G table; deep E scout rings; full
land/combat `20e6`; `5b66` case 7 economy tails. Odd deviations OK; not T3 / LCG goldens.

### R5 — Toward 1:1 (T2/T3)

1. Remaining `FUN_521d_20e6` branches (Euro combat, explore, ocean/ship, colony tiles).
2. Full `5b66` order/combat arms (thin map done) and remaining `5d04`.
3. Full `4d56` large bodies + nested `2820` helpers.
4. Golden / hang-dump coverage for mid-game turns (not only seed-100 turn 0).

### R6 — King / REF (`43f7`) (**partial structural port**)

**Linux:** [`ai_king.c`](../src/core/ai_king.c) — `2424`-shaped peace (SoL → `1d42`
tax → `2564`/`1a26` auto-declare) vs war (`0982`/`06a6` wave → `2022` act +
`1eca` promote); thin `10f0` via `backup_force` foreign landing when REF empty;
structural tax boycott/refuse (`unknown46[2]` + Sugar boycott bit; UI PARKED).
WoI stand-in `head.unknown46[0]` (DOS `0x5382` bit0 PARKED); REF-present
`unknown46[1]`; crown/intervene use non-human Euro nation_ids. Thin map:
[`king_ref.md`](../original_sources_annotated/ai/king_ref.md). Smoke:
`smoke_ai_king`.

**PORT DEBT:** `38fd_5be8` boycott UI; player declare confirm; `160a` rename;
`2244` merc hire; `1528` arrival chrome; deep `10f0` economy; exact `0x5382`
Col1 bit rename / T3.

---

## Prerequisites

Shared execution surfaces AI will call into (not the DOS planner itself).
Status reflects the AI-port prerequisite work:

| Subsystem | Status | Notes |
|-----------|--------|-------|
| `colonies_found(nation_id)` | **Done** | Owning nation set at found time |
| Unit orders (fortify, sentry, disband) | **Partial** | Map F / S / Shift+D + ORDERS menu; overnight fortify → fortified |
| Land combat | **Partial** | T0 attack/defense (+ fortified ×2); AI-initiated via act/raids |
| Colony capture | **Done** (T0) | `colonies_capture` — Euro owner swap; Indian capture abandons |
| Naval combat | **Partial** (T0) | `units_resolve_naval_combat` |
| Fog of war / `map.seen` | **Partial** | Dedicated plane; reveal on move; cheat Reveal; `.MP` fully seen |
| AI coarse fog (`DS:0x9faa`) | **Partial** | Explore `>>2` + tribe `/5` dual index; Linux `s_ai_coarse_fog`; not player FoW |
| Alarm / contact hooks | **Partial** (T0) | `ai_contact_*` meet/trade/missions/raids + adjacent friction |
| AI colony economy + construction | **Ready** | `turn_run_colony_production` already ticks **all** active colonies |
| Founding Fathers / liberty | **Partial** | Bells threshold elect; ~12 tiny FF effects; full table PARKED |
| King / tax / REF | **Partial structural** | `ai_king_nation_turn` — R6; `smoke_ai_king` |

Suggested manual order still puts **full Euro/Indian AI** late (#10 in
manual_gap) after combat and Indian contact. **R1 Euro settle (T0)** and
**seed-100 early T2** (`smoke_ai_turns`) are in; R0 partial (quiet mid-turn
default, **2** Brave spent-only residuals — call graph annotated
(`brave_spent_callgraph.md`); post-ADD chrome does not write `0x3149`; overlays
kept; hang **VR_B465X** last resort). Next: generic T1 Euro settle (spent rows
stay overlaid until hang X).

---

## Evidence and tools

| Artifact | Use |
|----------|-----|
| `test-saves-mapgen/SEED100.SAV` | Golden tribes/Braves; `smoke_mapgen_seed100`; far-ocean probe |
| `tools/probe_far_ocean_4753.c` | Phase 8: far tiles Linux ↔ SAV ocean/land |
| `tools/probe_sioux_spent.c` | T1/T2 Brave cost-head + neighborhood oracle (spent residuals) |
| `test-saves-ai/TURN1.SAV`…`TURN7.SAV` | Early-AI T2 gate; `smoke_ai_turns` |
| `original_saves/COLONY00.SAV` / `COLONY01.SAV` | Rival fleets, sail, AI crosses |
| `COLONIZE/VR_SEED.EXE`, `VR_BRAVE*.EXE` | Seed-locked RE probes (not runtime) |
| `original_memory_dumps/dosbox_save_state_brave/` | Live Brave pulse dumps |
| `tools/brave_dump/` | Hang-dump tooling (**parked** / overlay-unsafe). Fog/dir notes: `init_20e6_4753.md`; spent: `midturn_465b.md`. Prefer coarse-fog port + goldens |
| `tools/diff_turns.c` | Manual SAV↔SAV unit/tribe/crosses dump |
| [`.context/seed100-brave.md`](../.context/seed100-brave.md) | Durable Brave fidelity notes / open LCG burns |
| `COLONIZE/TRIBE.TXT`, `NAMES.TXT` `@TRIBES` / `@SCENARIO` | AMERICA villages / landfalls |
| `GAME.TXT` `@RAID*` | Raid message tags → `AiRaidKind` loot picker in `ai_contact` |
| `tests/smoke/test_ai.c` | Init + multi-turn smoke |
| `tests/smoke/test_mapgen_seed100.c` | T2 Brave/tribe fidelity |
| `tests/smoke/test_ai_turns.c` | T2 TURN1→7 field-diff |
| `tests/smoke/test_ai_contact.c` | Meet + `@RAID*` loot + raid hostility + Missionary convert |
| `tests/smoke/test_ai_diplo.c` | Bilateral war/ally, war gold/tax sting, upkeep |
| `tests/smoke/test_ai_king.c` | SoL, tax→REF, declare, crown wave, `10f0` intervene |
| `tests/smoke/test_ai_euro_expand.c` | Second-wave settle on full dispatcher |
| `tests/smoke/test_ai_euro_war.c` | Mid-war Soldier hire + MILITARY goto |
| `tests/smoke/test_founding_fathers.c` | Liberty-bell FF election threshold |

Smoke:

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns smoke_ai_contact smoke_ai_diplo
./build/smoke_mapgen_seed100   # cwd = repo root
./build/smoke_ai_turns         # TURN1→7 gate
./build/smoke_ai_contact       # Indian meet/raid structural
./build/smoke_ai_diplo         # Euro bilateral diplo
cmake --build build --target smoke_ai && ./build/smoke_ai
```

---

## Size sense

| Side | Rough scale |
|------|-------------|
| Linux `ai.c` + `ai_*.c` | ~3.5k + goals/euro/diplo/contact/king modules |
| Euro planner (DOS) | `6d8e` ~500 + `0a60` ~860 + `5d04` ~750 + `20e6` ~4k (+ nested `5b66` ~1.8k) |
| Indian cluster (DOS) | `1816` ~140 + `2154` ~320 + `2820` ~1.4k + `4528` ~3k + helpers |

Full T0/T1 surface is in: dispatcher + contact + king modules. Remaining work is
fidelity hardening (T1/T2 goldens, LCG, deeper Layer D extracts) — not missing
planner arms.

---

## See also

- [original_index.md](original_index.md) — FUN_* navigation
- [decomp_inventory.md](decomp_inventory.md) — EOT pipeline / bring-up
- [manual_gap.md](manual_gap.md) — feature checklist vs manual
- [data_vs_hardcoded.md](data_vs_hardcoded.md) — bake-into-C rule for AI
- [savegame.md](savegame.md) — Col1 nation / tribe / unit blobs
- [fandom_col1994.md](fandom_col1994.md) — unverified natives / combat wiki digest
