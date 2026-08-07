# Euro per-unit act (`FUN_521d_5b66`) — thin section-map

Layer D early-settle map only. Full body ~1815 lines at
`viceroy_unpacked.c` ~90446–92260. **PARKED** for line-by-line extract.

Linux: `ai_unit_spend_goto` / `ai_euro_early_turn` peels — **PORT DEBT**.

## Entry / wiring

| Item | Detail |
|------|--------|
| Ghidra | `FUN_521d_5b66` |
| Thunk | `2a1f_0488` from `FUN_521d_6d8e` ship/land act loop |
| Args | Decomp shows corrupted far prototype; live arg = **unit index** |
| Annotated | `euro_unit_act` in [`euro_dispatcher.c`](euro_dispatcher.c) |

Not nested inside `20e6`. Goals are `0a60`/`5d04`; scoring is `20e6`; act is `5b66`.

## Phase outline

### 0. Early move-scoring gate (~90552–90580)

```
if moves_spent == 0 OR orders != 0x0B (goto):
    r = 2a1f_04f4 → FUN_521d_20e6 (move_scoring)   @90557
    if r != 0: return
else: path validate (281f_0984); order 'E' Europe-counter tweaks
if orders-7 > 5: clear orders (0934); return
switch (orders) cases 7..0x0b
```

### 1. `switch (314c)` arms (all PARKED bodies)

| Lines | Case | Label |
|-------|------|-------|
| 90589–91142 | **7** | Europe hire (`0500`/`5c3c`), founding urgency, treasury buy — **PARKED** economy (Linux: thin Pioneer tools-delivery only; see §2d) |
| 91143–91158 | **8** | short |
| 91159–91194 | **9** | short |
| 91195–91362 | **10** | UI/chrome / dialog-ish (`281f_04ac` ≠ `06ae`) |
| 91363–92150 | **0x0b** | Ship/land act: ocean probe, naval band, dir8 score |

### 2. Case `0x0b` settle-adjacent notes (still PARKED)

| Lines | Concern |
|-------|---------|
| 91583–91591 | Unload / labor — `colony+0x8e--`, order `'G'` |
| 91603–91616 | Goal-priority → order `'B'` |
| 92151–92167 | Fortify? colony-check → order `'F'`, dir=8 |
| 92176–92212 | Apply orders 5/6/0xc; idle → `'0'` |
| 92243–92255 | Naval + order `'1'` → `'B'`; clear when goal tile reached |

Post-act primary upsert for exhausted ships lives in **`6d8e`**, not here.

### 2b. Linux thin — naval war hunt (act-level)

When nation is at war with a Euro peer, ships **not in Europe** that are idle /
station-keeping get `AI_SAIL` toward the nearest enemy sea unit or coastal water
beside a foreign colony at war. Adjacent enemy ships call `ai_euro_try_attack` /
`units_resolve_naval_combat`. Deep `20e6` naval combat scoring stays **PARKED**.

### 2c. Linux thin — land war hunt (act-level)

When at war with a Euro peer, idle land military (Soldier / Dragoon / Scout —
not fortified, no useful goto) get `AI_MOVE` toward the nearest enemy land unit
or enemy colony tile. Adjacent → `ai_euro_try_attack`. Does not steal founders on
FOUND goals. Deep `20e6` land combat scoring stays **PARKED**.

### 2d. Linux thin — Pioneer tools delivery (case 7 economy stand-in)

Idle / arriving Pioneer or Hardy on an **own** colony tile when
`tools_short > 0` or colony `stock[TOOLS] < 20`: add **+10** tools
(cap 100) once per act; trim inventory `tools_short` and may decrement
`urgency`. Wired in `ai_euro_unit_act` just before LABOR/COLONY join.
Full case-7 hire / wagon / treasury matrix stays **PARKED**.

### 3. Combat / diplomacy tails (PARKED)

Defer with Indian raid clusters / land combat port.

## Naval type band note

Decomp often tests `type ∈ (0x0c, 0x13)` (open upper). Annotated
`SHIP_A..C = 0x0a..0x0c` is the dispatcher ship-wave set — **do not conflate**
with the wider naval cargo band inside `20e6` / `0a60`.

## Related symbols

| Symbol | Role |
|--------|------|
| `FUN_521d_20e6` | Direction / move scoring (`04f4` @90557) |
| `FUN_521d_06ae` | Best adjacent founding tile (from `20e6` @89587 only) |
| `FUN_521d_016a` | Upsert primary goal |
| `FUN_1427_*` / `281f_09xx` | MP chrome after steps |

## Exit criteria for a future deep extract

- Sectioned `.c` with provenance headers
- Ship unload + founding-order arms readable end-to-end
- Explicit PARKED remainder for combat / case 7 hire (thin tools-delivery only)
- `SYMBOL_MAP` + catalog `links` updated
