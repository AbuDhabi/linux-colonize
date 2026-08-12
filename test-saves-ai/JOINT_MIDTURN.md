# Joint Euro↔Indian mid-turn goldens (T3 roadmap)

Early gate: `TURN1.SAV`…`TURN7.SAV` via `golden_ai_turns` (full dispatcher).
That smoke compares **both** Euro and Indian fields (units/Braves, tribes,
colonies, `euro_relation[]`, `relation_by_indian[]`, `indian_hostility_sticky`).

Aggregate CI gate: `cmake --build build --target golden_ai_joint`
(includes `golden_ai_mid01` + `golden_ai_late01`).

## Mid-game series

| Save | Role | Status |
|------|------|--------|
| `MID01.SAV` | Linux-derived mid-war stamp from TURN7 (sticky≥2, alarm, year≥1505) | **Done** via `golden_ai_mid01` (regenerates + self-checks) |
| `MID02.SAV` | One joint turn after MID01 (Euro dispatcher + Indian nation act) | **Done** via `golden_ai_mid01` (write + pair compare) |
| `LATE01.SAV` | Late-war stamp from MID02 + one joint turn; structural raid/hunt signals | **Done** via `golden_ai_late01` (not TURN XY field-diff; not blanket T3) |

`golden_ai_mid01` loads `TURN7.SAV`, stamps joint mid-war fields, writes
`MID01.SAV`, runs one `turn_end`, captures `MID02.SAV`, then pair-compares
calendar / Euro+Brave / tribes / colonies / sticky surfaces (same joint list
spirit as `golden_ai_turns`). Exact unit XY is Linux-derived each run.

`golden_ai_late01` loads `MID02.SAV`, stamps late-war (year≥1550, sticky≥2,
alarm/friction hot), writes `LATE01.SAV`, runs one `turn_end`, captures
`LATE01_POST.SAV` for debug, and asserts structural raid-side + hunt-side
mutation signals (not locked XY goldens).

Each pair should encode:

| Field group | Why joint |
|-------------|-----------|
| Euro units (xy/orders/goto) | Dispatcher / `5b66` / ocean `3558` |
| Indian Braves (xy/moves/spent) | Quiet `14fe` / alarmed escort / shared `20e6` |
| Tribes (pop / growth_accum) | Growth tick |
| Colonies (xy/pop/bip/hammers/stocks) | Found, raid loot, admit |
| `nation[].euro_relation[4]` | Euro×Euro war/ally (Privateer gate) |
| `nation[].relation_by_indian[8]` | Indian×Euro scalar matrix |
| `nation[].indian_hostility_sticky` | Hunt / scout / feeler / FA gift |
| FoW / `map.seen` (optional) | CONTACT rings / explore extras |

## Policy

PRs that touch `FUN_521d_20e6`, `15b3`/sticky, raids, or FOUND must run:

```bash
cmake --build build --target golden_ai_joint
```

Do not green Euro-only by ignoring Indian unit/tribe/diplo rows.
Chrome-only (VGA dialog strings, FA `3f41` widgets) stays out of golden
field lists — see `docs/ai_transcription.md` R5.
