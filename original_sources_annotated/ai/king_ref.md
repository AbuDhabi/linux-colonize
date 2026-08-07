# King / REF / independence (`43f7`) — thin section-map

Layer D hygiene for tax → declare → REF → war. Linux:
[`src/core/ai_king.c`](../../src/core/ai_king.c) — **partial structural port**.
Odd deviations OK; not T3.

Peels: `.context/peel_shards/layer_b_nation_colors_43f7.json`,
`layer_c_turn_eot.json`.

## `FUN_43f7_2424` peacetime vs wartime

```
EOT → 291f_0a66 → 43f7_2424  (SoL refresh + dispatch)
  peacetime:
    1d42  tax→REF pool growth (+ notify)
    0004  SoL aggregate
    maybe 0218 crown bootstrap
    chrome thresholds
    UI 2564 (SoL≥50) → 1a26 declare
  wartime (0x5382 bit0):
    2022 independence-war nation turn
```

## `FUN_43f7_2022` war shell

| Branch | Bodies |
|--------|--------|
| Crown | tax residual `1d42`?; pools>0 → `0982` invasion; else `06a6` irregulars |
| Rebel | once `1eca` promote; else intervene hire → `10f0` (partial structural) |

## Key symbols → Linux

| Symbol | Role | Linux |
|--------|------|-------|
| `0004` | Pop-weighted SoL | `ai_king_sol_percent` |
| `1d42` | Tax→REF funding | `ai_king_tax_event` |
| `2564` / `1a26` | Declare gate / crown setup | `ai_king_try_declare` (auto; UI PARKED) |
| `060a` | Garrison score / landing pick | `ai_king_weakest_port` |
| `0982` | REF wave MoW + pools | `ai_king_ref_wave` (pools>0) |
| `06a6` | Irregulars when REF empty | `ai_king_ref_wave` (else) |
| `10f0` | Foreign landing when REF empty + `backup_force` | `ai_king_foreign_intervene` (via `war_act`) |
| `2022` / `1eca` | War act + promote | `ai_king_war_act` |
| `05ea` / `05f4` | Crown colors | `turn.c` (known) |

## DS / Col1 anchors

| DOS | Linux stand-in |
|-----|----------------|
| `0x5382` bit0 war | `head.unknown26` — **no**; use `head.unknown46[0]` WoI |
| `0x5382` bit1 REF present | `head.unknown46[1]` (thin) |
| Tax boycott / refuse | `head.unknown46[2]` (structural; `38fd_5be8` UI PARKED) |
| Cargo boycott bits | `nation.boycott_bitmap` (EuropeScreen has none) |
| REF pools `0x53da…` | `head.expeditionary_force[4]` |
| Foreign pools `0x53e2…` | `head.backup_force[4]` — **10f0 stand-in** (seeded on declare) |
| Crown id `0x53d2` | Non-human Euro nation slot (0 or 1) |

Exact `0x5382` Col1 bit rename PARKED.

### Tax boycott / refuse stand-in (`1d42` + parked `38fd_5be8`)

When a spring tax year would hike and `tax_rate >= 20` and (SoL ≥ 30 or
liberty bells ≥ 80): **refuse** — do not raise tax; set `unknown46[2]`; OR in
`nation.boycott_bitmap` bit1 (Sugar); grow REF pools once without a hike.
While `unknown46[2]` is set, further tax years skip hikes entirely.
Accept/refuse dialog and dump-goods chrome remain PARKED.

## Linux `ai_king_nation_turn` checklist

1. SoL (`0004`)
2. If !WoI: tax (`1d42`) → declare gate (`2564`/`1a26`; seeds REF + thin `backup_force`)
3. If WoI: wave (`0982`/`06a6`) → war act (`10f0` first if REF empty + backup, then `2022`/`1eca`)

## PORT DEBT

- `38fd_5be8` tax audience / boycott **UI** (structural refuse + `unknown46[2]` / `boycott_bitmap` done)
- Player `2564` confirm dialog; `160a` rename cinematic
- `1528` arrival announce; `2244` merc hire
- Deep `10f0` foreign intervention / `backup_force` — **partial structural** (landing + drain; merc/chrome PARKED)
- Multi-unit MoW cargo holds; seize-landing polish
