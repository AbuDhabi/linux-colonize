# Coastal Fort / Fortress fire (`FUN_364b_03f6`)

Per-colony battery pulse vs adjacent ocean ships. Decomp:
`viceroy_unpacked.c` **57016–57118** (~107 lines). Far thunk
`FUN_291f_09ce` / `thunk_FUN_291f_09ce` (EMS `210d_0dab` → body).

Orchestration: [`between_turns.md`](between_turns.md) ·
[`colony_eot_production.md`](colony_eot_production.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

**Port:** **Done** thin — `units_coastal_fort_fire_pulse` /
`turn_run_coastal_fort_fire`. Ship-slow on miss: `moves_left=0` **Done** thin;
human sank/slowed status **Done** thin; damaged bit7 / deep DOS formula
**PARKED** (bit7 vs ship-build).

## Call sites / reshape

| | DOS | Linux |
|--|-----|-------|
| When | Nested in `364b_0688` phase A (`57227`: `thunk_291f_09ce`) after bind `09e6` / nation page | `TURN_PROC_SETUP` **after** `turn_run_colony_production` (all colonies) |
| Scope | Active colony `DS:0x8542` only | Every active Fort/Fortress colony |
| Combat | Spawn temp attacker `291f_0a20`→`478c_002c`, engage `0a14`→`5fef_1b0e`, undo `0a06`→`478c_00d0` | Direct strength vs ship defense; sink on fort win (no temp unit / dialog) |

Do not “fix” docs to nest fort fire inside Linux production.

## Phase table

| Phase | Lines | What |
|-------|-------|------|
| **0 — Battery power** | 57040–63 | `09fc(1)` Fort → `local_c++`, icon `DS:0x8f8e`; `09fc(2)` Fortress → `local_c++`, icon `0x8f9a`. Colony `x/y`/`+0x1a` owner from `0x8542`. Count type **`0x0b`** (Artillery) on colony stack (`07e0`/`02e4`); `local_12 = 1 + arty`. Power `atk = local_12 * local_c * 4`. Early out if 0 |
| **1 — 8 ocean dirs** | 57066–71 | `dir = 0..7`; tile `x+DS:0xb4[d]`, `y+DS:0xbe[d]`; skip unless `0768` ocean/HS |
| **2 — Hostile ship** | 57072–79 | Stack walk; keep type **`0x0d..0x12`**. Owner nibble `3147&0xf` ≠ colony. Attack if `(0a38 diplo & 0x40)==0` **or** type `0x10` (Privateer) |
| **3 — Hit / chrome gate** | 57083–92 | `local_8`: human 8-adj presence `0970(…,0x5396)` **or** ship flag `(0x10<<human) & 3147` |
| **4 — Resolve** | 57093–111 | Spawn with `atk`; scratch `0x537d=atk`, `0x5372=icon`, `0x5376=8`; clear `3149`. If `local_8`: camera/msg (`0438`/`0416`/`09a4`/`0652` id `0xd7f`). `0a14(ship_xy, local_8, 1)`; `0a06` cleanup |

## DS / colony / unit offsets

| Addr / field | Role |
|--------------|------|
| `DS:0x8542` | Bound colony: `+0` x, `+1` y, `+0x1a` owner |
| `DS:0xb4` / `0xbe` | Dir8 dx / dy |
| `09fc` → `15eb_038e` | Building-bit test (1=Fort, 2=Fortress) |
| `0x8f8e` / `0x8f9a` | Fort / Fortress combat-icon words |
| `unit*0x1c+0x3146` | Type (`0x0b` arty; `0xd..0x12` ships; `0x10` Privateer) |
| `+0x3147` | Nation nibble + visibility bits |
| `+0x3149` | `moves_spent` (cleared on temp attacker) |
| `0x5396` | Human nation |
| `0x52cc` | Spawn-type crumb into `0a20` |
| `0x5372..0x537d` | Spawn/combat scratch |
| `0x5230+type*0xe` | Type table (name subst `0438`) |

## Linux formula

`units_coastal_fort_attack_strength`: Fortress → tier 2 else Fort → 1;
`atk = 4 * tier * (1 + arty_on_colony_tile)`. Pulse: water neighbors; hostile =
Euro/Indian at war **or** Privateer; `units_fort_vs_ship` roll; win → despawn
ship; lose → **ship-slow thin** (`moves_left=0`; damaged bit7 PARKED — conflicts
with ship-build latch). Deep DOS combat chrome still **PARKED**. AI flee/skip:
`ai_euro.c` (Marathon8).
