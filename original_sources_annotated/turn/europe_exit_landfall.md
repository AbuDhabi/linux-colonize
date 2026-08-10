# Europe-exit landfall / tax treasures (`FUN_48d3_06ba`)

| Item | Value |
|------|-------|
| Lines | **77943–78038** (~96 body) |
| Thunk | `FUN_291f_0a82` |
| Host | `FUN_3844_00f2` after Europe nation EOT |

Bridge: [`between_turns.md`](between_turns.md) ·
[`docs/ai_transcription.md`](../../docs/ai_transcription.md).

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
| Treasure tax cash-in | `europe_cash_treasure` (+ Cortes path) | **Partial** |
| Arriving-ship Europe focus | `game_europe_deliver_bound_ships` | Reshape |
| Landfall delay ticks | Voyage timers / AI goto | Split |
| Map landfall coords | `map_gen_euro_landfall` | Mapgen |
