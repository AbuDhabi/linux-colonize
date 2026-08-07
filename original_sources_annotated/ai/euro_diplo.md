# Euro diplomacy (`15b3` / `5bfb`) — thin section-map

Layer D hygiene for bilateral flags + war/ally policy. Quiet Brave
`diplomacy_flags` stub: [`accessors.c`](accessors.c). Indian meet/raid:
[`indian_contact.md`](indian_contact.md).

Linux: [`src/core/ai_diplo.c`](../../src/core/ai_diplo.c) — **partial structural
port**. Odd deviations OK; not T3.

## `15b3` bilateral bytes

| Symbol | Thunk | Role |
|--------|-------|------|
| `FUN_15b3_0004` | `281f_0a38` | Read peer byte |
| `FUN_15b3_0032` | — | Write peer byte |
| `FUN_15b3_0066` | `281f_0a10` sibling | OR both directions; assert symmetry |
| `FUN_15b3_00d0` | `281f_0a10` | Clear both directions |

Decomp addressing:

- **Euro** (`nation < 4`): `*(peer + nation * 0x13c − 0x77c4)`
- **Indian** (`nation ≥ 4`): `*(peer + nation * 0x4e + 23000)` — full matrix **PORT DEBT** on Linux

Linux Euro×Euro stand-in (316-byte / `0x13c` nation record):

| Slot | Use |
|------|-----|
| `nation[a].unknown26[0..3]` | Treaty timers toward peer (6d8e §4) |
| `nation[a].unknown26[4..7]` | Diplo flag byte toward peer (`15b3` mirror) |

Exact DS `−0x77c4` Col1 field rename PARKED.

### Bit constants (Linux)

| Bit | Name | Notes |
|-----|------|-------|
| `0x01` | WAR | |
| `0x02` | PEACE | |
| `0x04` | ALLY | DOS `13b0` often discusses bit `0x40` for alliance chrome — do not conflate with MET |
| `0x40` | MET | Meet / known |

## `6d8e` §4 vs opportunistic `5bfb`

```
euro_nation_turn (6d8e)
  §4 treaty timers: 0a38 read + decrement peer timers; peaceful Indian drift
  plan 5d04 / 0342 / 0a60
  [opportunistic] 10ec → 13b0 (ally −25g) → declare_war (thin 153e gold+tax+Indian −5);
                  at-war upkeep
  act 5b66 — combat may declare_war
```

| Symbol | Thunk | Role |
|--------|-------|------|
| `FUN_5bfb_10ec` | `2a1f_067a` | Euro A↔B war/ally eligibility by military balance |
| `FUN_5bfb_13b0` | `2a1f_065e` | Form or break alliance |
| `FUN_5bfb_153e` | `2a1f_05fc` | Large war-declare body (~1112) — thin gold+tax+upkeep |
| `FUN_5bfb_0000` / `00f8` / `312e` | census / rank / combat factor | Score stand-ins |
| `FUN_5bfb_102a` / `1092` / `0182` | dialogs | **PARKED** UI |
| `FUN_3f41_*` | FA advisor | **PARKED** |

### Thin `153e` war sting (Linux)

On first `ai_diplo_declare_war` (not already at war):

- Drain **100** gold from `nation[a].gold` and `nation[b].gold` (floor 0)
- Bump each side's `nation[].tax_rate` by **+1**, capped at **75** (same ceiling as king tax path)
- **−5** on each of `nation[].relation_by_indian[0..7]` for both warring Euros (Indians dislike Euro×Euro war; scalar via `ai_diplo_indian_relation_delta`, clamp 0..255)
- WAR / PEACE / ALLY / MET flag writes unchanged
- Relation summary still via mirror (`nation_relation` → −50 while at war)
- Re-declare does **not** re-sting gold, re-bump tax, or re-hit Indian relations

Ongoing (in `ai_diplo_euro_balance`, while already at war with a peer):

- If `nation[nation_id].gold > 0`, drain **5** gold (floor 0) once per war peer visited
- No new declare / ally logic for that peer that turn

Not ported: full `5bfb_153e` trade/military score body, dialogs (`102a`/`1092`), FA `3f41`, order clear `12d0`. Full `153e` / dialogs remain **PARKED**.

### Thin alliance treasury cost (Linux)

On `ai_diplo_form_alliance` (Euro×Euro):

- Each side pays **25** gold if able (floor 0)
- Flags still clear WAR and set ALLY|PEACE|MET as before
- Full `13b0` gold/score gates **PARKED**

### Thin peaceful Indian relation drift (Linux)

Called at end of `ai_diplo_treaty_timers` (6d8e §4 path):

- If the Euro nation is at war with **any** other Euro → no-op
- Else for each of 8 `relation_by_indian[i]`: if `< 160`, **+1** (cap **160**)
- Stand-in only; full Indian×Euro `15b3` bilateral matrix remains **PORT DEBT** / **PARKED**

## Linux checklist

1. `ai_diplo_read` / `write` / `or_both` / `clear_both` — peer-correct bytes
2. `ai_diplo_treaty_timers` — decrement; on expiry break ally or peace tweak; peaceful Indian drift
3. `ai_diplo_euro_balance` — `10ec`/`13b0`-shaped; declare → thin `153e` gold+tax+Indian hit; at-war → light upkeep
4. `ai_diplo_form_alliance` — ALLY flags + 25 gold each side
5. `ai_diplo_indian_relation_delta` — `4cc6_00f2` / `15dc_00e0` scalar (not full Indian `15b3`)

## PORT DEBT

- Full `153e` body, dialogs, FA UI, name tables `15b3_0144…`
- Indian×Euro bilateral `15b3` matrix (**PARKED**; thin drift / war-hit scalars only)
- Exact save-field rename for `−0x77c4`
- Quiet Brave `diplomacy_flags` −10 goldens
