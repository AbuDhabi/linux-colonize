# Indian contact / nation-turn contact path — thin section-map

Layer D hygiene for meet/trade/raid attachment. Quiet Brave pulse remains
[`indian_nation_turn.c`](indian_nation_turn.c) (`1816` → `14fe`). Raid **outcome**
arms: [`indian_raid_outcomes.md`](indian_raid_outcomes.md).

Linux: [`src/core/ai_contact.c`](../../src/core/ai_contact.c) +
`ai_indian_nation_turn` in [`ai.c`](../../src/core/ai.c). **Partial structural
port** — odd deviations OK; not T3.

## `FUN_4d56_1816` phase checklist

Annotated shell (quiet path only for act):

| # | DOS section | Linux |
|---|-------------|-------|
| 1 | Reseed LCG (`04ca`); set active nation = indian+4 | `ai_nation_reseed` |
| 2 | Select indian context + chrome | (no-op / turn cursor) |
| 3 | Alarm prelude (NEW WORLD) | `ai_contact_indian_prelude` — flag body thin; LCG burns stay in pulse |
| 4 | Clamp alarm byte ≥ 0 | prelude clamp |
| 5 | Tribe growth loop (`41f2_0280` / `152e`) | `ai_grow_villages` |
| 6 | Relation / goods tick (`2a1f_0270` → `4962_06b6`) | `ai_contact_indian_relation_tick` |
| 7 | Clear act_counter | pulse clears `turns_worked` |
| 8 | Act loop → quiet `14fe` only | `ai_native_nation_pulse` (+ seed-100 overlays) |
| 9 | Meet / trade / raid (other paths; not inside `14fe`) | post-pulse `ai_contact_indian_meet_trade` / `…_raids` |

Alarmed / mission branches inside unit act: **PARKED** (`2154` / `2820` / `4528`).

## Call-graph (authoritative vs catalog myths)

| Symbol | Thunk | Real callers (decomp) | Not |
|--------|-------|----------------------|-----|
| `FUN_4d56_1b3a` | `281f_0676` | Mid-turn helper: clear `0x5b04` tables, tribe `41f2_0266`, colony ownership probes | Does **not** call `2154` |
| `FUN_4d56_2154` | `2a1f_0434` | From **`5bfb` neighborhood** (~96088) after meet/diplo scoring | Not from `1b3a` |
| `FUN_4d56_2820` | `2a1f_044c` | Heavy decision + nested trade `2aac…311e`; also ~86766 | Full body PARKED |
| `FUN_4d56_4528` | `2a1f_016c` | Settlement enter/raid; from **move foreign** / contact (`move_spent` §3) | Not quiet `14fe` |
| `FUN_5bfb_022e` | `2a1f_066c` | Indian unit meet/contact (~96565); also ~98793 | Dialog UI PARKED |
| `FUN_4cc6_00f2` / `0000` | `0d6c` / `0398` | Relation delta / mission clear | — |

Peels: `.context/peel_shards/layer_c_4d56.json`, `layer_b_ai_diplo.json`,
`layer_c_spent_465b.json` (`4528`), `layer_c_colony_sim_ticks.json` (`2154` thunk).

## Meet / trade `5bfb_022e` checklist (Linux)

1. Adjacent Euro land unit → set `met_by_player`, relation bump
2. Peaceful meet (alarm/friction < 40): slight tribe `alarm[].friction` decay (−1)
3. Optional mission assign if friction low (teach/convert **UI PARKED**)
4. **Missionary convert pulse** (structural deepen): Euro unit whose display name
   contains `"Mission"` adjacent to a tribe of this nation, and relations not
   hostile (`alarm_by_player` / tribe friction both < 50) → set
   `tribe.mission = euro nation id`, decay alarm/friction by 1 if > 0, bump
   `nation[euro].current_crosses` by 1. One pulse per tribe per call.
5. **Teach-skill pulse** (structural deepen): Free Colonist or Scout (display-name
   match) adjacent to tribe, peaceful band (`alarm_by_player` / tribe friction
   both < 40), and `tribe.state.learned` clear → set **`tribe.state.learned = 1`**
   (real Col1 bit). If unit `profession == UNITS_JOB_NONE`: Scout →
   `UNITS_JOB_SCOUT` (Seasoned); else → tribe-appropriate outdoor `@JOB`
   (see mapping below). One pulse per tribe per call. Teach dialog UI **PARKED**.
6. Peaceful trade: colony trade-goods → lower alarm/friction (auto-haggle stand-in for `2aac…311e`)
7. Gift / demand dialogs **PARKED**

### Teach-skill profession map (Linux)

Prefer `tribe.last_sold` when it is raw cargo **1..7** (sugar..silver) → matching
field job. Food(0) is **not** treated as an override so zeroed Col1 tribes still
hit the nation table. Else static map by `tribe.nation_id` (4..11 = `@TRIBES`
order). Unmapped → Expert Farmer. Full `@TRIBES` flavor-good string parse
(**Jewelled Relics**, …) remains **PARKED**.

| Driver | Value | Profession (`@JOB`) |
|--------|-------|---------------------|
| `last_sold` | Sugar (1) | Sugar Planter (1) |
| `last_sold` | Tobacco (2) | Tobacco Planter (2) |
| `last_sold` | Cotton (3) | Cotton Planter (3) |
| `last_sold` | Furs (4) | Fur Trapper (4) |
| `last_sold` | Lumber (5) | Lumberjack (5) |
| `last_sold` | Ore (6) | Ore Miner (6) |
| `last_sold` | Silver (7) | Silver Miner (7) |
| nation 4 | Inca | Silver Miner (7) |
| nation 5 | Aztec | Ore Miner (6) |
| nation 6 | Arawak | Fisherman (8) |
| nation 7 | Iroquois | Fur Trapper (4) |
| nation 8 | Cherokee | Tobacco Planter (2) |
| nation 9 | Apache | Cotton Planter (3) |
| nation 10 | Sioux | Fur Trapper (4) |
| nation 11 | Tupi | Sugar Planter (1) |
| fallback | — | Farmer (0) / Scout→Seasoned |

Raid hostility deepen (loot success + high friction → `ai_diplo_indian_relation_delta`):
see [`indian_raid_outcomes.md`](indian_raid_outcomes.md). Full `2820`/`4528` + player
meet/trade dialog UI remain **PARKED**.

## PORT DEBT

- Full `2154` (~321), `2820` (~1.4k), `4528` (~3k)
- Player meet/trade/raid dialog subst
- Teach dialog UI + full skill-from-`@TRIBES` flavor-good string parse
- Folding alarmed act into quiet `14fe` (would fight seed-100 T2)
