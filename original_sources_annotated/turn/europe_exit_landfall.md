# Europe-exit landfall / tax treasures (`FUN_48d3_06ba`)

| Item | Value |
|------|-------|
| Lines | **77943–78038** (~96 body) |
| Thunk | `FUN_291f_0a82` |
| Host | `FUN_3844_00f2` after Europe nation EOT |

Bridge: [`between_turns.md`](between_turns.md) ·
[`docs/ai_transcription.md`](../../docs/ai_transcription.md) ·
[`europe_finish_bridge.md`](europe_finish_bridge.md) (helpers / Europe UI).

## Phases

| # | Lines | Role |
|---|-------|------|
| 1 | 77965–66 | Two `2a1f_0246`→`48d3_03d0` landfall-delay ticks |
| 2 | 77967–78 | Walk units: ships type ∈ `(0x0c,0x13)`; remember human focus ship; note cargo |
| 3 | 77979–80 | More landfall-delay ticks |
| 4 | 77981–027 | **Treasure** type `0x0a`: value=`profession*100`; Crown tax=`min(tax_rate,50)`; credit nation gold; sound; dialog `0x148e`; **destroy** treasure |
| 5 | 78029–36 | If focus ship + human: set **`DS:0x14c=1`**, **`DS:0x14e=ship_index`** (open Europe focus) |
| 6 | 78037 | `2a1f_0238`→`48d3_064e` cleanup |

## Key DS

| Addr | Use |
|------|-----|
| `0x5394` / `0x5396` | Active / human nation |
| tax rate | `nation*0x13c` Europe block |
| gold words | `+0x8832` family |
| `0x14c` / `0x14e` | Open Europe + focus unit |

## Related (not this FUN)

Europe→map place: `48d3_048e` via `2a1f_0262` (Linux
`units_spiral_place_hs_near` / `ai_europe_exit_to_map`).

## Linux

| DOS | Linux | Fidelity |
|-----|-------|----------|
| Treasure tax cash-in | `europe_cash_treasure` (+ Cortes path) | **Done** — Crown cut `min(tax, 50)` |
| Arriving-ship Europe focus | `game_europe_deliver_bound_ships` | Reshape |
| Landfall delay ticks | Voyage timers / AI goto | Split |
| Map landfall coords | `map_gen_euro_landfall` | Mapgen |

## Placement chain — full audit 2026-09-17 (bugs.md #487, #489)

Every DOS appearance of a ship coming out of Europe resolves through **one**
tile source. There is no map-wide scan anywhere in the `48d3` module.

| Step | DOS | Detail |
|---|---|---|
| Nation landfall tile | nation record `-0x77c6` / `-0x77c5` | 2 bytes, x then y. Save field `return_from_europe_x/y` (`docs/save_format_map.md`). |
| Seed (scenario map) | raw 121035 | `@SCENARIO` row for the loaded `.MP` stem: `stem, x0,y0, x1,y1, x2,y2, x3,y3`, one pair per nation. AMER2 = (34,20) (39,10) (47,61) (50,33); all four are class `0x1a` High Seas. No RNG, no difficulty. |
| Seed (generated map) | `LAB_684c_1b4c` raw 107140-107154 | nation→slot shuffle seeded from the human nation, then per slot `y = (height/5)*(slot+1)` and `x` walked west from `width-2` while the tile is `0x1a`, `+1` (the HS band's western rim). Port: `map_gen.c:1615-1660`, already DOS-literal. |
| Restamp on departure | `FUN_48d3_007a` raw 77604-77607 | Writes the departing unit's own x,y into the nation tile, then copies it into `+0x314d/+0x314e` of every unit in the stack and sets `+0x315a` = voyage turns (`48d3_0002`). So "nation has no saved exit tile" only exists at new-game time. |
| New unit in Europe | raw 97805, 99622, 107547, 112448, 114391, 121627 | Purchases, REF/King arrivals, the damaged-hull Europe slot and the new-game fleet all copy the nation tile into the unit's `+0x314d/+0x314e`. No per-unit variation, no difficulty term. |
| Sail *to* Europe | `FUN_48d3_015e` raw 77636-77728 | Expanding ring around the unit's **current** tile for a `0x1a` tile that is empty or holds our own unit; stores it in `+0x314d/e`, bumps `DS:0x9456[nation]`, sets orders `3` (human) / `0xb` (AI) and `+0x314b = 0x45`. |
| Arrive *from* Europe | `FUN_48d3_048e` raw 77810-77896 (+ `FUN_48d3_0434` raw 77779) | Expanding ring around `+0x314d/+0x314e`, first tile passing `0434` (on map, class `0x1a`, occupant `< 0` or own nation nibble). Ring radius cap `max(0x853a, 0x853c)`; on total failure the unit is still placed on the saved tile. |
| Per-turn Atlantic tick | `FUN_48d3_03d0` | Decrements `+0x315a`; at 0 calls `281f_0880` + `08c6` → the `048e` placement. New-game fleets are created with `+0x315a = 0`, so they land on turn 1. |
| Lane cleanup | `FUN_48d3_064e` | Walks non-ship types out of the lane. |

**Linux**: `units_spiral_place_hs_near` = `048e` + `0434` and is now the only
placement path (`units_new_world_start`, `ai_spawn_euro_fleet`,
`game_europe_deliver_bound_ships`, `ai_king` MoW, `ai_euro` Europe exit).
`units_find_eastern_high_seas_tile` has **no DOS counterpart** and is kept only
as a *sail-target* helper (trade-route Europe stop, treasure sail target,
last-resort fallback) — DOS needs no such table because the human steers by hand
and `015e` accepts whatever High Seas tile the ship happens to reach.

## Lane chain (resolved 2026-09-18, bugs.md #489)

The sentinel diagonal is a **chain**, one hop per Atlantic tick, walked for the
**active nation only** (`FUN_48d3_03d0` places at `param_2 + DS:0x5394`):

| Lane | Role |
|---|---|
| `244+n` | eastbound entry (`FUN_48d3_007a` raw 77626, `nation-0x0c`) |
| `240+n` | eastbound, one hop out |
| `236+n` | **the Europe port** — no `03d0` call walks it; the dock lane |
| `232+n` | westbound entry (`FUN_48d3_0346` raw 77755, `nation-0x18`) |
| `228+n` | westbound, last hop — "arrives at the next tick" |
| `224+n` | `FUN_48d3_064e` → `048e` puts the hull on the map |

`FUN_48d3_06ba` head runs `0246(0xffe4,0xffe0)` then `0246(0xffe8,0xffe4)`
(westbound 228→224, 232→228) and, after the treasure block,
`0246(0xfff0,0xffec)` / `0246(0xfff4,0xfff0)` (eastbound 240→236, 244→240).

**`FUN_75c2_235c` (#489) — REFUTED as a player-visible divergence.** The
bootstrap does create all four fleets at `(228+n, 228+n)` with counter 0, but
228+n is the *last* westbound lane, and the human nation's own `06ba` pass runs
before the player ever sees the map. The genuine DOS turn-0 oracles
`original_saves/mapgen/SEED100.SAV` and `original_saves/COLONY00.SAV` both show
the **human fleet already on the landfall tile** (`goto` restamped to that same
tile) with only the three AI nations still at 229/230/231. That is exactly what
`units_new_world_start` produces, so nothing changed in the sim.

What *was* wrong was the save-lane mapping: the port wrote its harbor at
`228+n` and Bound at `232+n`. Now `236+n` / `228+n` / `244+n`
(`col1_bridge.c`), which also explains the old "unresolved fifth family" at
`240+n`.
