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
| Rebel | once thin `1eca` promote (Soldier/Regular + Dragoon/Cavalry; SoL bands); else intervene hire → `10f0` (dual landing structural); thin `2244` merc |

## Key symbols → Linux

| Symbol | Role | Linux |
|--------|------|-------|
| `0004` | Pop-weighted SoL | `ai_king_sol_percent` |
| `1d42` | Tax→REF funding | `ai_king_tax_event` |
| `2564` / `1a26` | Declare gate / crown setup | `ai_king_try_declare` (auto; thin congress status + `unknown46[5]`; real modal PARKED) |
| `160a` | Independence rename cinematic | thin rename on declare (`country_name`); letter-anim PARKED |
| `060a` | Garrison score / landing pick | `ai_king_weakest_port` |
| `0982` | REF wave MoW + pools | `ai_king_ref_wave` (pools>0; thin MoW cargo unload) |
| `06a6` | Irregulars when REF empty | `ai_king_ref_wave` (else) |
| `1528` | REF arrival announce | thin status line after successful `0982` spawn (chrome UI PARKED) |
| `10f0` | Foreign landing when REF empty + `backup_force` (≤2/call; prefer Regular+Dragoon) | `ai_king_foreign_intervene` (via `war_act`) |
| `2244` | Mercenary hire offer | thin auto-accept once/war via `ai_king_merc_offer` + hire status (real modal PARKED) |
| `2022` / `1eca` | War act + Continental/vet promote | `ai_king_war_act` (thin widen; deep table PARKED) |
| `05ea` / `05f4` | Crown colors | `turn.c` (known) |

## DS / Col1 anchors

| DOS | Linux stand-in |
|-----|----------------|
| `0x5382` bit0 war | `head.unknown26` — **no**; use `head.unknown46[0]` WoI |
| `0x5382` bit1 REF present | `head.unknown46[1]` (thin) |
| Tax boycott / refuse | `head.unknown46[2]` (structural + thin `38fd_5be8` audience status; real modal PARKED) |
| Merc hired this war | `head.unknown46[3]` (thin `2244` + hire status; real modal PARKED) |
| Independence rename | `player[human].country_name` → `"United Colonies"` (+ `europe.nation_name` if present); `unknown46[4]` unused (name field exists) |
| Cargo boycott bits | `nation.boycott_bitmap` (EuropeScreen has none) |
| REF pools `0x53da…` | `head.expeditionary_force[4]` |
| Foreign pools `0x53e2…` | `head.backup_force[4]` — **10f0 stand-in** (seeded on declare) |
| Crown id `0x53d2` | Non-human Euro nation slot (0 or 1) |

Exact `0x5382` Col1 bit rename PARKED.

### Tax boycott / refuse audience (`1d42` + thin `38fd_5be8` status)

When a spring tax year would hike and `tax_rate >= 20` and (SoL ≥ 30 or
liberty bells ≥ 80): **refuse** — do not raise tax; set `unknown46[2]`; OR in
`nation.boycott_bitmap` bit1 (Sugar); grow REF pools once without a hike;
status `"The colonies refuse the tax increase! Tax stays at %u%%."`.
While `unknown46[2]` is set, further tax years skip hikes and write
`"Audience: boycott holds — the King cannot raise taxes."`.
Real accept/refuse modal and dump-goods chrome remain **PARKED**.

### Thin `1528` REF arrival announce

When `0982` successfully spawns a ship or land unit, write a short arrival
line to `ctx->status` (if present) and keep `unknown46[1]` REF-present.
Full arrival chrome / dialog remains PARKED.

### Thin MoW cargo unload (`0982` hold size 2 stand-in)

When the wave drains `expeditionary_force[2]` and spawns a Man-O-War, also
unload Regulars near the target colony (same crown nation) as if from the
ship hold: spawn **min(2, force[0])** land Regulars and drain `force[0]`.
If `force[0]` is empty, still guarantee one land from another pool same beat.
Full embark / `cargo_ids` hold chrome remains PARKED.

### Thin `2244` Continental merc hire status

During wartime `war_act` (including the declare turn): if human gold ≥ 300,
SoL > 50, and `unknown46[3]` unset — spend 300 gold (sync Europe if present),
spawn one Soldier/Dragoon for the **human** near weakest port, set
`unknown46[3]`, status
`"Mercenaries join the Continental cause (−300 gold)."`.
Real hire modal remains **PARKED**.

### Thin `1eca` Continental / veteran promote

During wartime `war_act`:

- **SoL > 50:** promote human land units whose type/display name contains
  **Soldier** (not already Veteran/Continental; not type **Regular**) to
  `Continental Army` / `Cont. Army` / `Veteran Soldier` (first type that exists);
  likewise **Dragoon** or **Cavalry** → `Continental Cavalry` / `Cont. Cav.` /
  `Veteran Dragoon`; type name containing **Regular** → `Veteran Soldier` /
  `Continental Army` / `Cont. Army` (fallback). Armed Regulars often *display*
  as "Soldier" — Regular is classified by **type name**.
- **SoL 40..50:** promote **Soldier** → `Veteran Soldier` only when that type
  exists (no Continental rename in this band). Regular types are unchanged.

Deep DOS colony-SoL fraction / veteran-profession / type-id table
(`43f7_1eca`) remains PARKED.

### Thin `160a` independence rename + `2564` congress status

On declare (`ai_king_try_declare`): write congress-confirm status
`"Congress declares independence!"` (if `ctx->status` present);
set `player[human].country_name` to `"United Colonies"` and sync
`europe.nation_name` when Europe is attached; set `head.unknown46[5]`.
Same-turn `0982`/`1528` wave may overwrite `ctx->status` when it spawns
(wave leaves congress status if empty). Letter-by-letter rename cinematic
remains **PARKED**. `head.unknown46[4]` is **not** used as a renamed flag —
writable Col1 `country_name` exists. Real `2564` confirm modal remains
**PARKED**.
### Thin pre-declare SoL chrome

Peacetime before the declare gate: if SoL is **40..49** and `ctx->status`
is present, write `"Sons of Liberty grow restless (%d%%)."`. Auto-declare
still requires SoL≥50 + bells≥100.

### Structural `10f0` foreign intervention (dual landing)

When WoI and REF pools empty and `backup_force` total > 0:
`ai_king_foreign_intervene` lands up to **two** units near the weakest human
port for a crown-hostile Euro nation, draining one pool entry per spawn.
If both Regular (`backup[0]`) and Dragoon (`backup[1]`) are > 0, prefer that
mix (one of each). Otherwise drain up to two available pool types in order
(MoW pool still lands a Regular stand-in). Deep economy / merc hire /
arrival chrome remain PARKED.

## Linux `ai_king_nation_turn` checklist

1. SoL (`0004`)
2. If !WoI: tax (`1d42`) → SoL 40–49 chrome → declare gate (`2564`/`1a26`; seeds REF + thin `backup_force` + thin `160a` rename + `unknown46[5]` congress)
3. If WoI: wave (`0982` MoW + thin cargo unload / `06a6` + thin `1528` status) → war act (`10f0` ≤2 landings if REF empty + backup, thin `2244` merc, thin `1eca` SoL-band promote)

## PORT DEBT

- **Done (unpark #2 thin status chrome):** `38fd_5be8` tax audience status (structural refuse + `unknown46[2]` / `boycott_bitmap`); `2564` congress-confirm status + `unknown46[5]`; `2244` merc hire status (auto-accept + `unknown46[3]`)
- **Still PARKED:** real modal widgets for audience / congress confirm / merc hire; `160a` rename **cinematic** (thin `country_name` done); `1528` arrival **chrome/dialog** (thin status announce done)
- Deep `10f0` economy / merc hire / arrival chrome — **PARKED** (dual landing + Regular/Dragoon mix + drain done); third landing @ difficulty≥2 still **PARKED**
- Deep `1eca` colony-SoL / veteran-profession / type-id promote table — **PARKED** (thin widen Done: SoL>50 Soldier/Dragoon/Regular + SoL 40–50 Soldier→Veteran Soldier)
- Full MoW cargo-hold chrome / embark slots — **PARKED** (thin hold-size-2 Regular unload on `0982` MoW spawn done); seize-landing polish
