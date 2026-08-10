# Colony EOT production (`FUN_364b_0688`)

Layer D section map for the per-colony end-of-turn production / SoL /
construction tick. Active colony via `DS:0x8542`. Caller: nation EOT
`FUN_3844_00f2` → thunk `291f_0950`.

Decomp: `viceroy_unpacked.c` **57152–57951** (~800 lines). Next sibling:
`FUN_364b_1aec` @57955.

Orchestration: [`between_turns.md`](between_turns.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md) ·
[`docs/building_production.md`](../../docs/building_production.md).

**Port status:** Linux **Partial** — spine in `turn_run_colony_production` /
`turn_produce_one_colony` (`src/core/turn.c`); shared rules in
`colony_production.c` / `colony_craft.c`. Education, birth, starve-kill,
building advisories, and most dialog chrome **PARKED** or thin.

## Sibling note — `FUN_364b_03f6` (coastal fort fire)

@57016–57118 (~107 lines); thunk `291f_09ce`. Counts coastal batteries,
walks 8 ocean adjacencies, finds hostile ships (type 0xd..0x12), rolls hit
vs fort power. **Linux:** `units_coastal_fort_fire_pulse` /
`turn_run_coastal_fort_fire`. DOS nests this inside `0688` prologue; Linux
runs it as a separate SETUP pulse after all colony production (reshape).

## Colony offsets touched

| Offset | Role |
|--------|------|
| `+0x1a` | Owner nation |
| `+0x1c` | `colony_flags`: sol_100=`0x02`, sol_50=`0x04`, starvation=`0x08`, build_complete=`0x80` |
| `+0x1f` | Population |
| `+0x90` | `cargo_produced_mask` |
| `+0x92` | Hammers bank |
| `+0x94` | Build-menu project index |
| `+0x95` | Warehouse expansion level |
| `+0x97` | Ore/silver depletion counter |
| `+0x9a..` | `stock[16]` (food @ `+0x9a`; tools @ `+0xb6`) |
| `+0xc2/+0xc4`, `+0xc6/+0xc8` | Rebel SoL accumulators |

Scratch: `DS:−0x7238` (gross), `−0x71f6` (reserve). Net: `281f_0b50` → `15eb_0b0c`.

## Phase / LAB map

| Phase | Lines (approx) | What |
|-------|----------------|------|
| **A — Bind + side pulses** | 57223–57236 | Clear `0xa898`; bind `09e6`; nation `0582`; **fort fire** `09ce`→`03f6`; compose `0c22`; bells `0b50(0x12)` → `09f8`→`4345_0a22`; warehouse `0d3a`; clear `+0x90` |
| **B — Apply cargo 0..15** | 57238–57348 | Net yield → stock; food difficulty bonus; Custom House sell>99 leave 50; produced-mask; surplus vs cap |
| **C — SoL accumulators** | 57349–57414 | SoL % `0c86`; rebel pairs `+0xc6`/`+0xc2` |
| **D — SoL / Tory / starve flags** | 57415–57485 | Latch bits 2/4/8; msgs `0xd8a`…`0xddd` |
| **E — Surplus re-clamp** | 57486–57501 | Re-min surplus |
| **F — Education scan** | 57502–57539 | Teachers / schoolhouse students |
| **G — Education graduate** | 57540–57589 | Pair students; set job; msgs |
| **H — Random skill** | 57590–57614 | LCG specialty discover |
| **I — Birth** | 57615–57622 | Food≥200 → spawn Free Colonist |
| **J — Starvation kill** | 57623–57695 | Kill colonists / abandon empty |
| **K — Build advisories** | 57696–57728 | Missing building msgs |
| **L — Hammers / construction** | 57730–57787 | Add hammers; spend tools; complete `097a`→`0114` |
| **M — Crosses** | 57788–57789 | Scratch → nation crosses |
| **N — Farmer pressure** | 57790–57805 | Count farmers vs food |
| **O — AI dump-sell + trim** | 57806–57873 | Non-human sell; spoilage clamp |
| **P — Spoilage msgs** | 57874–57931 | Multi-cargo / century dialogs |
| **Q — Depletion** | 57932–57944 | `+0x97` wrap → map feature 4 |
| **R — Epilogue** | 57945–57950 | Flush colony screen; `DS:0x34a=−1` |

## Major callees (thunk → real)

| Role | Thunk | Real |
|------|-------|------|
| Bind colony | `281f_09e6` | `15eb_002c` |
| Fort fire | `291f_09ce` | `364b_03f6` |
| Compose yields | `281f_0c22` | `15eb_3956` |
| Net yield | `281f_0b50` | `15eb_0b0c` |
| Bells + FF | `291f_09f8` | `4345_0a22` |
| Warehouse cap | `281f_0d3a` | `15eb_0a50` |
| Custom House gate | `291f_09c0` | `364b_0636` |
| Sell + tax | `291f_0a2e` | `38fd_1dfa` |
| SoL % | `281f_0c86` | `15eb_0274` |
| Spawn / remove | `095c` / `0a9c` | `1427_06b4` / `15eb_0d04` |
| Complete build | `291f_097a` | `364b_0114` |
| Depletion tile | `291f_0988` | `364b_033a` |

## Linux correspondence

| DOS | Linux |
|-----|-------|
| A fort fire (nested) | Separate SETUP `turn_run_coastal_fort_fire` |
| B cargo apply | `turn_produce_one_colony` + craft/yield — **Partial** |
| B Custom House | `europe_custom_house_autosell` |
| C/D SoL flags | `colony_prod_refresh_sol_flags` |
| L hammers | `colony_prod_colony_hammers` + complete |
| O spoilage | `colonies_apply_warehouse_spoilage` |
| F–J education/birth/kill | **PARKED** / thin (starvation flag only) |
