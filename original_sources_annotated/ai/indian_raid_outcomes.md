# Indian raid outcomes — thin section-map

Maps settlement-raid / loot clusters for a **reasonable** Linux port in
`ai_contact_indian_raids`. Full `FUN_4d56_4528` (~3073 lines) stays PARKED.

Related: [`indian_contact.md`](indian_contact.md). Peels:
`.context/peel_shards/layer_c_4d56_4528.json`, `layer_b_combat_raid.json`.

## Entry / wiring

| Symbol | Role |
|--------|------|
| `FUN_4d56_4528` | Settlement enter / raid contact (thunk `2a1f_016c`) |
| `FUN_5fef_0f14` | Colony raid loot + tension (goods/building/unit/gold) |
| `FUN_5fef_016c` / `0352` / `0ec0` | Plunder pick / apply outcome / sweep — PARKED deep |
| `FUN_4d56_359c` | Relation-gated kill / warn / displace by RNG — thin Scout despawn only |
| `FUN_4d56_2154` | Larger action body from `5bfb` — raid-adjacent; not `1b3a` |

## Linux phase arms (`ai_contact_indian_raids`)

1. **Gate** — pick target Euro by max(`alarm_by_player`, tribe friction); band ≥ ~40
2. **Adjacent combat** — `units_resolve_land_combat` vs target-nation land unit
3. **Colony approach** — Chebyshev walk toward colony ≤6
4. **Loot outcome** — `@RAID*` kind picker (below); mutates stock / pop / gold
5. **Capture** — high band + tiny pop → `colonies_capture` (Indian → abandon)
6. **Scout hostility** — alarm ≥90 + Scout name → despawn (`359c`-shaped stub)
7. **PARKED** — full `2820` decision matrix, player haggle, dialog subst, ship harbor deep

## `@RAID*` message tags (`COLONIZE/GAME.TXT`)

UI strings, not numeric tables. Linux uses the **kind enum** to pick loot:

| Tag | Kind | Linux loot stand-in |
|-----|------|---------------------|
| `@RAIDNOTHING` | NOTHING | No stock change (raid party “wiped” flavor unused) |
| `@RAIDWREAK` | WREAK | Multi: food + tools + friction bump |
| `@RAIDSTORES` | STORES | Decrement a cargo stock (food/goods preferred) |
| `@RAIDBURN` | BURN | Building-in-production clear / damage stub |
| `@RAIDSCALP` | SCALP | Population −1 if pop > 1 |
| `@RAIDSHIP` | SHIP | Coastal harbor: damage nearby Euro ship MP/HP stub |
| `@RAIDGOLD` | GOLD | Nation gold −N (treasury raid) |

## Exit criteria for deeper extract

- Sectioned `4528` with threat / combat / loot / dialog clusters named
- `5fef_0f14` line-faithful goods picker
- Dialog subst still PARKED
