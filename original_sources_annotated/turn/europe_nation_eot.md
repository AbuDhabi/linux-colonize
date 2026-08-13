# Europe nation EOT + market dynamics

Decomp: `original_sources_decompiled/viceroy_unpacked.c`.
Caller (DOS): `FUN_3844_00f2` via `291f_0a90` (`5e52`) and nested
`291f_0cbc` (`0058`). Linux reshape: `europe_tick_market_prices` /
`europe_apply_volume_price` in FINISH; full `5e52` immigrant/tax/FF arms
mostly **PARKED**.

Bridge: [`between_turns.md`](between_turns.md) ·
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

---

## `FUN_38fd_5e52` — Europe nation EOT

| Item | Value |
|------|-------|
| Lines | **68539–68623** (~85) |
| Thunk | `FUN_291f_0a90` |
| Arg | `param_1` = nation id |
| Early out | `DS:0x5382 & 1` (war) → return |

### Phases

| # | Lines | Role |
|---|-------|------|
| 0 | 68553 | Skip if wartime |
| 1 | 68554–55 | Page Europe `281f_0582`→`38fd_0000`; reseed `04ca` from `0x83a6` |
| 2 | 68556 | Clear Europe-block flag bit `0x20` at `*[0x84fc]` |
| 3 | 68557 | **Market EOT** `291f_0cbc`→`38fd_0058(0, 0xffff)` |
| 4 | 68558–67 | Immigration pressure `0b34`→`38fd_584a` → `+0x30` / accumulate `+0x2e` |
| 5 | 68568–615 | If score < pressure: pick dock slot; roll profession `0afc`→`46d4`; spawn harbor `0b26`→`0718`; dialogs; else Recruit UI `0d2c`→`4884`; may set `DS:0x14c` |
| 6 | 68617–20 | King tax-raise `0b7a`→`5be8`; on reject FF cargo gift `0c84`→`5930` |

### Key DS

| Addr | Use |
|------|-----|
| `0x5382` | War / chrome gates |
| `0x538e` | Turn (immigrant season `&3`) |
| `0x83a6` | RNG reseed |
| `0x84fc` | Nation Europe block (`nation*0x13c`) |
| `0x543f+n*0x34` | Player control |

### Linux

| DOS | Linux | Fidelity |
|-----|-------|----------|
| Phase 3 market | `europe_tick_market_prices` (FINISH) | **Partial** |
| Pressure / recruit / tax / FF | dock immigrants + `europe_tick_immigration_pressure` (584a +2/tick + phase5 pool→dock; `@UNREST` not `open_on_dock`) **Done** thin; king tax elsewhere | Recruit UI / atomic `5e52` chrome **PARKED** |

---

## `FUN_38fd_0058` — market dynamics

**Disassembly verified clean (2026-08-13).** Carried a Ghidra
`Removing unreachable block` disassembly-fault warning in the canonical
export (`docs/decomp_inventory.md`). Re-disassembled via the
overlay-addressing project (`tools/address_mapping.csv` →
`OVL05_L0040:458`): clean, self-contained, 1420 bytes / 275 decompiled
lines, 3 unrelated minor unreachable-block warnings left (ordinary
decompiler noise). Calls `thunk_FUN_1000_9bda` — target not resolved this
pass. Confirms the market-dynamics mapping below is working from
trustworthy source.

| Item | Value |
|------|-------|
| Lines | **58741–59005** (~265) |
| Thunk | `FUN_291f_0cbc` |
| Args | `param_1==0` full EOT peel; `param_2` cargo or `0xffff` = all |

### Phases

| # | Role |
|---|------|
| 1 | Price groups `0..0xf`: copy `DS:0x53ea[g]`; sum nation ledgers; human decays `0x53ea` |
| 2 | Cargos **9..12**: ratio vs sum; nudge pressure `+0x5c` or clamp bid `+0x4c` |
| 3 | `param_1==0`: cargos **1..4**; year bonuses; pressure ± |
| 4 | All cargos **0..15**: attrition; rise/fall ±1 bid + dialogs `0xfa8`/`0xfb0`; write ask preview `0x84bc`; if `param_2≥0` undo attrition |

### Key DS

| Addr | Use |
|------|-----|
| `0x53ea[16]` | `price_group_state` |
| `0x84fc+0x4c` / `+0x5c` | Bid / pressure per cargo |
| `0x96fe..` | `@CARGO` low/rise/fall |
| `0x538a` / `0x53a6` | Year / difficulty |

### Linux

| DOS | Linux |
|-----|-------|
| EOT all-cargo | `europe_tick_market_prices` | phases 2–3 pressure **Done** thin + attrition |
| Post buy/sell | `europe_apply_volume_price` |
| Colony → `0x53ea` half | **Done** thin — `europe_tick_market_prices(eu, col1, colonies)` decays `head.price_group_state[c]` by colony stock sum `>> 7` |
| Phase 2 cargos 9..12 | **Done** thin — `trade_nr += sign * mid * 100` |
| Phase 3 cargos 1..4 | **Done** thin — `trade_nr += mid * sign`; fur year &lt;1700/&lt;1600 |
| Phase 4 rise/fall status | **Done** thin — `"Market price rose/fell."`; dialogs PARKED |

---

## Deep — `5e52` phases 4–6

Cite: **68558–68620**.

### Phase 4 — immigration pressure (`584a`)

| Item | Detail |
|------|--------|
| Lines | **68558–67** |
| Call | `0b34`→`38fd_584a(DS:0x9e12, &local_c)` |
| Write | `Europe+0x30 = score`; `Europe+0x2e += delta`; clamp `+0x2e ≥ 0` |

`584a` (**68248–68300**): sum colony pops + unit count; `<<1` if &lt;4000; `+8`;
cap 4000; non-human `((8−diff)*score)>>3`; nation0 `*2/3`. Out-delta `*param_2`
defaults to **+2** (treasure can force −2 — PARKED).

**Port:** score → `needed_crosses`; idle **+2/tick** → `current_crosses` until
first dock immigrant (TURN5–7 stay 0 without churches); church crosses add to
`current` before the tick; spawn when `current > needed`. Separate
`immigration_pressure` fields are mirrors only.

### Phase 5 — dock immigrant vs Recruit

Gate: **`+0x30 < +0x2e`** (**68568**).

| Branch | Steps |
|--------|-------|
| Spawn path | Clear `+0x2e`. Slot `04d4(0,2)`. **`46d4((0x538e&3)==0)`** season quad. **`0718(old_prof)`** harbor spawn; human msgs `0x1190` / tip `0x1197`; Europe flags `\|0x40` |
| Else | **`4884(0,1)`** Recruit UI |
| Human follow | May set **`DS:0x14c=1`** (open Europe — see [`europe_finish_bridge.md`](europe_finish_bridge.md)) |

Callee args: `46d4(int season_force)`; `0718(int profession)`; `4884(0,1)`.

### Phase 6 — tax then FF gift

| Lines | Call | Meaning |
|-------|------|---------|
| 68617 | `0b7a`→`5be8()` | King tax audience; **1** = dialog ran |
| 68618–19 | if **0**: `0c84`→`5930()` | FF cargo/gold grant path |

Linux: dock immigrants / crosses in `turn_run_nation_ticks`; tax in `ai_king`;
atomic `5e52` **PARKED**.

---

## Deep — `0058` phases 1–3

Cite: **58787–58929** (phase 4 attrition already in table above).

### Phase 1 — price_group + ledger half

For `g=0..0xf`: seed from `DS:0x53ea[g]`; add nation ledgers at
`(n*0x4f+g)*4 − 0x76fc`. If `param_1==0` and `0x9e12==0`:
`0x53ea[g] −= (sum >> 7)`. Linux approximates with colony stock `>>7`.

### Phase 2 — cargos **9..12**

`sum = au[9]+…+au[12]`; per cargo ratio vs `0ec6(sum×3, cargo)`;
`sign = sgn(bid[+0x4c] − ratio)`. EOT (`param_1==0`):
`pressure[+0x5c] += sign * ((rise+fall)/2) * **100**`. Else clamp bid into
`[low,high]`.

### Phase 3 — cargos **1..4** (`param_1==0` only)

Denom from half cargo-0 + au[1..3]; cargo **4** halves ledger before ratio;
year &lt;`0x6a4` / &lt;`0x640` → +1/+2 to target. Nudge:
`pressure[+0x5c] += ((rise+fall)/2) * sign` — **no ×100**.

Cross-links: [`nation_ticks_bells_ff.md`](nation_ticks_bells_ff.md) ·
[`ai/king_ref.md`](../ai/king_ref.md).
