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

## Current Linux surface (Phase 1 + early-AI T2 gate)

| Piece | Role |
|-------|------|
| [`src/core/ai.h`](../src/core/ai.h) / [`ai.c`](../src/core/ai.c) | All ported AI |
| `ai_init_new_game` | Col1 template, rival fleets, tribes/Braves, one post-spawn native pulse |
| `ai_euro_nation_turn` | Reseed, AI crosses, `6d8e`-style ship/land passes; **seed-100 early path** (`ai_euro_early_turn` sail/unload/found) + opportunistic settle for other seeds |
| `ai_indian_nation_turn` | Village growth + mid-turn Brave pulse + residual overlays |
| [`turn.c`](../src/core/turn.c) | `TURN_PROC_EURO` / `TURN_PROC_INDIAN`; nation ticks (immigrant / crosses FSM); King stub |
| [`tests/smoke/test_ai_turns.c`](../tests/smoke/test_ai_turns.c) | **T2 gate:** `TURN1`→`TURN7` field-diff (`smoke_ai_turns`) |

**Claims (T2 early AI):** with VR_SEED=100 and idle human, `smoke_ai_turns` matches
`test-saves-ai/TURN2`…`TURN7` on calendar, AI crosses, colonies (sites/names/pop/bip/hammers),
euro units (xy/orders/goto), Braves (xy/moves/turns_worked), and tribe pop/accumulators.
Seed-100 Euro path uses landfall-derived coastal staging + found-site helper
(`ai_euro_found_tile_from_landfall`); ship approach / mid-turn waypoints still
fixture. Mid-turn Braves: quiet `20e6` + residual overlays on remaining pulse
mismatches (R0 scoring debt; **t1 empty**, ~50 rows on t2–t6).

**Does not claim:** mid-game Euro economy/military planner, Indian raids/meet/missions,
King/REF, or bit-identical unknown blobs unrelated to AI moves.

```mermaid
flowchart TD
  eot[EOT pipeline] --> euro[FUN_521d_6d8e Euro dispatcher]
  eot --> indian[FUN_4d56_1816 Indian nation]
  euro --> goals[FUN_521d_0a60 / 5d04 / helpers]
  euro --> score[FUN_521d_20e6 move scoring]
  indian --> growth[FUN_4d56_152e growth]
  indian --> unitAct["per-unit act thunk ~0x42191"]
  unitAct --> score
  unitAct --> raids[FUN_4d56_2154 / 2820 / 4528]
```

Linux today: Euro early path via `6d8e`-shaped entry + landfall coastal staging;
Indian growth + quiet scoring / residual overlays (not full `1816` / raid bodies).

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
| `FUN_4d56_0038` | ~39 | Small helper; calls into `00e0` / map probes | — | **unknown** |
| `FUN_4d56_00e0` | ~60 | Chains to `01e2` / `14fe` | — | **unknown** |
| `FUN_4d56_01e2` | ~19 | Thin wrapper → `14fe` | — | **unknown** |
| `FUN_4d56_14fe` | ~16 | Dispatches growth `152e` | — | **partial** (via growth only) |
| `FUN_4d56_152e` | ~156 | Village growth accumulator → pop++ | `ai_grow_villages` | **partial** (T0) |
| `FUN_4d56_1816` | ~141 | Indian nation turn entry: alarm prelude, unit loop (`func_0x00042191`), relation ticks | `ai_indian_nation_turn` (growth + pulse / residual overlays) | **partial** (T2 early chain) |
| `FUN_4d56_1b3a` | ~59 | Calls `2154`; mid-turn Indian action | — | **parked** |
| `FUN_4d56_2154` | ~321 | Larger Indian action body (caller of raid-adjacent logic) | — | **parked** |
| `FUN_4d56_2820` | ~1396 | Heavy Indian decision / raid-scale logic | — | **parked** |
| `FUN_4d56_2aac`…`311e` | nested | Helpers inside `2820` body | — | **parked** |
| `FUN_4d56_3582` | ~51 | Small helper after `2820` | — | **unknown** |
| `FUN_4d56_417e` | ~188 | Mid-size helper | — | **unknown** |
| `FUN_4d56_4528` | ~3073 | Largest Indian cluster (combat/raid-adjacent; needs RE labels) | — | **parked** |

Nation entry `1816` does **not** call `2154`/`2820`/`4528` directly in the
decomp slice; those are reached from other turn / contact paths. Full Indian
AI = `1816` + unit-act thunk + those large bodies + `@RAID*` data.

### European AI — `FUN_521d_*` (25 far + nested)

| Symbol | ~Lines | Purpose (known / inferred) | Linux | Status |
|--------|-------:|----------------------------|-------|--------|
| `FUN_521d_0000`…`0906` | small | Planner helpers (scores, probes, bookkeeping) | — | **parked** |
| `FUN_521d_0a60` | ~858 | Unit / colony goal logic | — | **parked** |
| `FUN_521d_20c6` | nested | Near helper before scoring | — | **parked** |
| `FUN_521d_20e6` | ~3995 | Direction / move scoring (all unit kinds) | quiet+`54f5` **annotated**; Linux empirical — cutover blocked (phase 2–4) | **partial** |
| `FUN_521d_5b66` | ~1815 nested | Large helper **inside** the `20e6` span (not a separate far export); historically mis-cited as sole “unit goals” entry | — | **parked** |
| `FUN_521d_5c38` / `5c3c` / `5cf6` | small | Thin helpers before `5d04` | — | **parked** |
| `FUN_521d_5d04` | ~748 | Unit goals / planning (alongside `0a60`) | — | **parked** |
| `FUN_521d_6d8e` | ~516 | Euro AI **dispatcher** per nation | `ai_euro_nation_turn` (skeleton + `ai_euro_early_turn`) | **partial** (T2 early path) |

`6d8e` calls into `5b66`, `0a60`, `20e6`, `5d04`, and many small `521d_*`
helpers (plus platform `FUN_281f_*` / `FUN_2a1f_*`). Linux enters a structured
dispatcher shell; early-game goals are seed-100 slices until full `0a60`/`5d04` land.

**Naming note:** older docs listed `FUN_521d_5b66` as a top-level unit-goals
peer of `0a60`. Prefer: goals ≈ `0a60` + `5d04`; scoring ≈ `20e6` (with nested
`5b66`).

### Shared move / terrain helpers (AI-adjacent)

| Symbol | Role in AI | Linux |
|--------|------------|-------|
| `FUN_465b_0000` (terrain MP) | Brave step cost | `ai_dos_move_spent` |
| `FUN_124c_0040` | Home distance in quiet scoring | `ai_dos_dist` / home-dist in `ai_native_pick_dir` |
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
| Quiet NEW WORLD dir pick | `ai_native_pick_dir` | T2 empirical. ASM+`54f5`+fog annotated; Linux cutover blocked after phase 4 (LCG/pulse) |
| Apply step + MP | `ai_native_apply_step` / `ai_dos_move_spent` | Annotated `move_spent_add` / accessors |
| `FUN_4d56_152e` growth | `ai_grow_villages` | Threshold `AI_VILLAGE_GROWTH_THRESHOLD` (19); pop cap 15 |
| `FUN_4d56_1816` full body | `ai_indian_nation_turn` | Growth + pulse + residual overlays (~50 on t2–t6; t1 empty); alarm/raid parked; annotated entry in `indian_nation_turn.c` |
| Per-unit indian act | pulse / residual | Quiet path; residual only on pulse≠golden; DOS thunk `func_0x00042191` → annotated stub `indian_unit_act` |
| `FUN_521d_6d8e` | `ai_euro_nation_turn` | Skeleton + `ai_euro_early_turn` sail/unload/found (**T2** via `smoke_ai_turns`); annotated shell in `euro_dispatcher.c` |
| `FUN_521d_0a60` / `5d04` / `20e6` (non-quiet) | early slices | Approach sail + coastal goals; full planner parked — see `ai/move_scoring.md` |
| Col1 AI fleets + landfall `goto` | `ai_spawn_euro_fleet` / `ai_pick_landfall` / `ai_sail_ship` | T2 landings on VR_SEED=100 |
| Landfall unload + first colony | `ai_euro_early_turn` / `ai_try_ship_unload` | **T2** golden towns; opportunistic settle for other seeds |
| AI crosses tick | `ai_euro_nation_turn` | +2 / needed default 14 |
| `@RAID*` / meet / mission | — | **No** counterpart |
| King / REF AI | `turn_run_king_stub` | Stub only |

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
  spent=max emptied **t1** and keep cost fidelity. Residual overlays remain on
  **t2–t6 (~50 rows: 5/9/14/9/13)** — quiet-scoring holdouts plus Sioux/Apache
  spent-only rows (XY match; dump-side `465b` still open). Apache T2 XY fixed
  via tile-scoped quiet at `(45,52)` (skip facing / river home-base / roll-add);
  spent still open (`tools/brave_dump/midturn_465b.md`: next hang `VR_B465R`
  Sioux `BX==0x1F8` → AL=cost). ASM quiet + `LAB_521d_54f5` gate + fog
  annotated under `original_sources_annotated/ai/quiet_brave_scoring.c`;
  phase 2–4 Linux cutovers **regressed** (same Apache init XY); next RE is
  LCG burn shape on multi-Brave pulse. Mark/apply helpers stay until tables
  empty.
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

Still open for generic T1 (non-fixture):

1. Save-diff / hang-dump fidelity for first AI town vs original saves.
2. Broader multi-colony / second-wave settle (not first-colony-only).
3. Colony production already runs for all active colonies.

### R2 — Indian nation turn beyond quiet pulse

1. Port more of `FUN_4d56_1816` (alarm flags, relation ticks, unit loop structure).
2. Identify and name `func_0x00042191` (unit act); port quiet + alarmed branches.
3. Extend `20e6` / quiet scoring for mid-game (goods, missions, capital pull)
   without claiming full scoring yet.

### R3 — Contact and raids (after meet + combat)

1. Wire `@RAID*` from `GAME.TXT` once combat and meet UI exist.
2. RE-label then port slices of `FUN_4d56_2154` / `2820` / `4528`.
3. Teach / trade / missions / convert as separate contact features (manual
   natives chapter) — may live partly outside `ai.c`.

### R4 — Euro dispatcher skeleton

1. Port `FUN_521d_6d8e` structure: nation setup, colony/unit inventory, dispatch
   hooks (even if goal bodies are stubs).
2. Port goal slices from `FUN_521d_0a60` / `5d04` as evidence allows (colony
   build, military, trade ships).
3. Replace fixture/skeleton `ai_euro_nation_turn` (`ai_euro_early_turn` + opportunistic
   settle) with real dispatcher entry.

### R5 — Toward 1:1 (T2/T3)

1. Remaining `FUN_521d_20e6` branches (Euro combat, explore, colony tiles).
2. Nested `5b66` and small `521d_*` helpers.
3. Full `4d56` large bodies + nested `2820` helpers.
4. Golden / hang-dump coverage for mid-game turns (not only seed-100 turn 0).

---

## Prerequisites

Shared execution surfaces AI will call into (not the DOS planner itself).
Status reflects the AI-port prerequisite work:

| Subsystem | Status | Notes |
|-----------|--------|-------|
| `colonies_found(nation_id)` | **Done** | Owning nation set at found time |
| Unit orders (fortify, sentry, disband) | **Partial** | Map F / S / Shift+D + ORDERS menu; overnight fortify → fortified |
| Land combat | **Partial** | T0 attack/defense (+ fortified ×2); naval / colony defense later |
| Fog of war / `map.seen` | **Partial** | Dedicated plane; reveal on move; cheat Reveal; `.MP` fully seen |
| Alarm / contact hooks | **Partial** | Adjacent village bumps friction + status; no meet UI / `@RAID*` |
| AI colony economy + construction | **Ready** | `turn_run_colony_production` already ticks **all** active colonies (docs formerly claimed skip — stale) |
| Founding Fathers / liberty | Missing | Euro long-term goals |
| King / tax / REF | Stub | Independence AI only |

Suggested manual order still puts **full Euro/Indian AI** late (#10 in
manual_gap) after combat and Indian contact. **R1 Euro settle (T0)** and
**seed-100 early T2** (`smoke_ai_turns`) are in; R0 partial (t1 empty, ~50
Brave residuals, named init burns, landfall coastal staging + found-site
helper). Next: empty remaining `k_seed100_brave_t*` holdouts (Sioux spent +
wrong-dir scoring), then generic T1 Euro settle.

---

## Evidence and tools

| Artifact | Use |
|----------|-----|
| `test-saves-mapgen/SEED100.SAV` | Golden tribes/Braves; `smoke_mapgen_seed100` |
| `test-saves-ai/TURN1.SAV`…`TURN7.SAV` | Early-AI T2 gate; `smoke_ai_turns` |
| `original_saves/COLONY00.SAV` / `COLONY01.SAV` | Rival fleets, sail, AI crosses |
| `COLONIZE/VR_SEED.EXE`, `VR_BRAVE*.EXE` | Seed-locked RE probes (not runtime) |
| `original_memory_dumps/dosbox_save_state_brave/` | Live Brave pulse dumps |
| `tools/brave_dump/` | Hang-dump tooling |
| `tools/diff_turns.c` | Manual SAV↔SAV unit/tribe/crosses dump |
| [`.context/seed100-brave.md`](../.context/seed100-brave.md) | Durable Brave fidelity notes / open LCG burns |
| `COLONIZE/TRIBE.TXT`, `NAMES.TXT` `@TRIBES` / `@SCENARIO` | AMERICA villages / landfalls |
| `GAME.TXT` `@RAID*` | Raid tables (unused until R3) |
| `tests/smoke/test_ai.c` | Init + multi-turn smoke |
| `tests/smoke/test_mapgen_seed100.c` | T2 Brave/tribe fidelity |
| `tests/smoke/test_ai_turns.c` | T2 TURN1→7 field-diff |

Smoke:

```bash
cmake --build build --target smoke_mapgen_seed100 smoke_ai_turns
./build/smoke_mapgen_seed100   # cwd = repo root
./build/smoke_ai_turns         # TURN1→7 gate
cmake --build build --target smoke_ai && ./build/smoke_ai
```

---

## Size sense (unfinished work)

| Side | Rough scale |
|------|-------------|
| Linux `ai.c` | ~2370 lines |
| Euro planner alone | `6d8e` ~500 + `0a60` ~860 + `5d04` ~750 + `20e6` ~4k (+ nested `5b66` ~1.8k) |
| Indian cluster | `1816` ~140 + `2154` ~320 + `2820` ~1.4k + `4528` ~3k + helpers |

Current code is orchestration + new-game setup + seed-100 early Euro slices +
quiet Brave pulse / residual overlays — not a transcription of the full planners.

---

## See also

- [original_index.md](original_index.md) — FUN_* navigation
- [decomp_inventory.md](decomp_inventory.md) — EOT pipeline / bring-up
- [manual_gap.md](manual_gap.md) — feature checklist vs manual
- [data_vs_hardcoded.md](data_vs_hardcoded.md) — bake-into-C rule for AI
- [savegame.md](savegame.md) — Col1 nation / tribe / unit blobs
- [fandom_col1994.md](fandom_col1994.md) — unverified natives / combat wiki digest
