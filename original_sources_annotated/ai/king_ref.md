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
| Rebel | once `1eca` promote (colony-SoL bands; Soldier/Regular + Dragoon/Cavalry); else intervene hire → `10f0` (≤3 landings @diff≥2); thin `2244` merc |

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
| `10f0` | Foreign landing when REF empty + `backup_force` (≤2/call; third @diff≥2; prefer Regular+Dragoon) | `ai_king_foreign_intervene` (via `war_act`) |
| `2244` | Mercenary hire offer | thin auto-accept once/war via `ai_king_merc_offer` + hire status (real modal PARKED) |
| `2022` / `1eca` | War act + Continental/vet promote | `ai_king_war_act` (colony-SoL bias; deep type-id table PARKED) |
| `05ea` / `05f4` | Crown colors | `turn.c` (known) |

## DS / Col1 anchors

| DOS | Linux stand-in |
|-----|----------------|
| `0x5382` bit0 war | `head.unknown26` — **no**; use `head.unknown46[0]` WoI |
| `0x5382` bit1 REF present | `head.unknown46[1]` (thin) |
| Tax boycott / refuse | `head.unknown46[2]` (structural + thin `38fd_5be8` audience status; real modal PARKED) |
| Merc hired/refused this war | `head.unknown46[3]` (thin `2244` hire **or** cannot-afford status; real modal PARKED) |
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
status `"Audience: the colonies refuse the tax increase! Tax stays at %u%%."`.
While `unknown46[2]` is set, further tax years skip hikes and write
`"Audience: boycott holds — the King cannot raise taxes."`.
Restless SoL chrome (40..49) must **not** clobber these audience lines.
If `boycott_bitmap` is cleared externally (Jakob Fugger / diplo peace lift —
king does **not** write FF), king sync clears `unknown46[2]` when
`bitmap == 0` so tax may resume. Real accept/refuse modal and dump-goods
chrome remain **PARKED**.
Additional classic boycott cargo bits (wiki dump-goods / `38fd_3dc8` RNG) —
**PARKED** (only Sugar is named in-file; do not invent a second bit).

### Thin `1528` REF arrival announce

When `0982` successfully spawns a ship or land unit, write a short arrival
line to `ctx->status` (if present) and keep `unknown46[1]` REF-present.
Same-turn `war_act` capture may overwrite with capture status.
Full arrival chrome / dialog remains PARKED.
Man-O-War spawns on **water adjacent** to the target colony when any such tile
exists (smoke-asserted; fandom REF man-o-war → ports). At `difficulty ≥ 2`, if
`force[2]` still allows after the first Man-O-War, spawn a **second** MoW
stand-in same beat (same 0982 path; stack if only one water tile). Deep
multi-ship formation chrome remains PARKED.

### Thin MoW cargo unload (`0982` hold size 3 stand-in)

When the wave drains `expeditionary_force[2]` and spawns a Man-O-War, also
unload land near the target colony (same crown nation) as if from the ship
hold: fill up to **3** slots with Regulars from `force[0]` first, then
Dragoons from `force[1]` while slots remain; drain those pools.
If both land pools are empty, still guarantee one land from another pool same
beat. Source: fandom REF “Men-O-War, Regulars, Cavalry”; “man-o-war with 6
units”; full embark / `cargo_ids` / MoW×6 hold chrome remains **PARKED**.

### Thin `2244` Continental merc hire status

During wartime `war_act` (including the declare turn): if SoL > 50,
`unknown46[3]` unset, and a human port exists:
- **gold ≥ 300:** spend 300 gold (sync Europe if present), spawn one
  Soldier/Dragoon for the **human** near weakest port, set `unknown46[3]`,
  status `"Mercenaries join the Continental cause (−300 gold)."`.
- **gold < 300:** PARK UI refuse — set `unknown46[3]`, status
  `"Cannot afford mercenaries."` once (no spawn / no spend).

`unknown46[3]` gates once-per-war for hire **or** refuse (no status spam /
re-spend / re-spawn). Refuse then later gold still blocked (smoke). Real hire
modal remains **PARKED**.

### WoI flag (`unknown46[0]`)

`ai_king_try_declare` (SoL≥`AI_KING_DECLARE_SOL_MIN` 50 + bells≥100) sets
`head.unknown46[0]` via `ai_king_set_independence` (DOS `0x5382` bit0 stand-in;
exact Col1 bit PARKED). Idempotent if already set. Restless SoL chrome
(40..49) must **not** set this byte. Smoke: SoL 49 + bells≥100 leaves
`unknown46[0]` clear; declare at SoL≥50 sets it.

### `1eca` Continental / veteran promote (colony-SoL bias)

During wartime `war_act`, each human land unit uses **colony SoL** from Col1
`rebel_dividend`/`rebel_divisor` at the unit tile when a matching owned
colony exists; otherwise nation `0004` aggregate (fallback):

- **colony SoL > 50:** promote **Soldier** → `Continental Army` / `Cont. Army` /
  `Veteran Soldier`; **Dragoon**/**Cavalry** → `Continental Cavalry` /
  `Cont. Cav.` / `Veteran Dragoon`; type **Regular** → `Veteran Soldier` /
  `Continental Army` / `Cont. Army`. Armed Regulars often *display* as
  "Soldier" — Regular is classified by **type name**.
- **colony SoL 40..50:** promote **Soldier** → `Veteran Soldier` only when
  that type exists (no Continental). Regular types unchanged.

Source: `FUN_43f7_1eca` / catalog “when colony SoL>50%”. King promote path
only — **not** FF Washington mass-promote / combat upgrade. Deep
veteran-profession / type-id table remains PARKED.

After promote, idle human **Cont. Army / Cont. Cav** (hunter name check includes
`Continental` / `Cont. Army` / `Cont. Cav`) prefer `AI_MOVE` toward the human
**founding capital** (lowest colony id); fallback `weakest_port` when none.
Hold if already on a human colony tile. Source: fandom Independence Cont. Army /
Cont. Cavalry; deep rebel AI PARKED.

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

Peacetime before the declare gate: if SoL is **40..49**
(`AI_KING_RESTLESS_SOL_MIN` .. `AI_KING_DECLARE_SOL_MIN-1`) and `ctx->status`
is present, write `"Sons of Liberty grow restless (%d%%)."`. When `tax_rate`
is already in the refuse band (≥20), append `" Tax is at %u%%."` (reads
existing rate — no invented tax formula). Must **not** overwrite an existing
1d42 audience / tax-hike status line. Does **not** set WoI/`unknown46[0]`
or congress/`unknown46[5]`. Auto-declare still requires SoL≥**50**
(`AI_KING_DECLARE_SOL_MIN` = FUN_43f7_2564 / fandom total SoL ≥ 50%; no new %)
+ bells≥100 (`AI_KING_DECLARE_BELLS_MIN`).

### Declare SoL gate (2564 — tighten/document only)

`ai_king_try_declare` auto-fires only when `ai_king_sol_percent` is already
**≥ `AI_KING_DECLARE_SOL_MIN` (50)** and liberty bells ≥ 100. Threshold is the
existing 2564/fandom figure — do **not** invent a different SoL %. Real
congress confirm modal remains PARKED.

### Structural `10f0` foreign intervention (≤3 landings)

When WoI and REF pools empty and `backup_force` total > 0:
`ai_king_foreign_intervene` lands up to **two** units near the weakest human
port for a crown-hostile Euro nation (drain one pool entry per spawn);
**three** when `difficulty ≥ 2` (REF-pressure stand-in).
Intervene **nation pick**: prefer the non-human / non-crown Euro with the most
colonies; tie-break by on-map land unit count (`backup_force` is a shared pool).
If both Regular (`backup[0]`) and Dragoon (`backup[1]`) are > 0, prefer that
mix (one of each). Otherwise drain available pool types in order
(MoW pool still lands a Regular stand-in). Deep economy / merc hire /
arrival chrome remain PARKED.

### REF land hunt + colony capture (war act)

During wartime `war_act`, idle crown **Regular/Dragoon** (and **Artillery** when
the Artillery/Cannon type exists in pool) with moves get `AI_MOVE` toward the
nearest human colony or human land unit (fandom REF AI; conceptual reuse of
Euro land hunt — implemented in `ai_king` only). **Artillery thin siege:**
prefer a fortified human colony (Stockade/Fort/Fortress) when hunting; wave
land spawn prefers `force[3]` Artillery when the target colony is fortified
and the type exists (else fall back to Regular/Dragoon order). **Dragoon / Cont. Cav thin open-land bias:** when the Artillery type exists,
prefer human land units / unfortified colonies (leave fortified ports to
Artillery); else nearest. Cont. Army stays nearest (no open bias) — smoke
asserts Cont. Cav→open + Cont. Army→nearest fort colony.
**Capital MD bias:** founding capital = lowest active human colony id (Euro
colonies have no Col1 capital bit). Among colony targets, prefer capital when
`cap_md ≤ nearest_colony_md + 2` (`AI_KING_CAPITAL_MD_SLACK`); human land units
still win on equal/closer MD. Prefer an **adjacent** uncaptured human colony over marching past (**Artillery
exception:** do not override a fortified hunt target with an unfortified
adjacent colony; adjacent fortified still wins). Adjacent
human unit → `units_resolve_land_combat`; attack win → occupy tile. Ending on
(or already standing on) a human colony tile → `colonies_capture` (conquest;
no gold fiction); thin human status
`"The King's forces have captured %s!"` (colony name; conquest chrome PARKED);
then **fortify one Regular** on the tile via
`units_order_fortify` (capturer if Regular, else another crown Regular on
tile). **Artillery after capture (Euro pattern):** fortify crown Artillery on
the captured tile (capturer if Artillery, else each idle Artillery on tile);
idle Artillery on crown/captured colony with no adjacent human foe/colony also
`FORTIFY` and hold (Colonization.pdf fortify defense; euro_unit_act Artillery
fortify after siege; case 0x0b). Off-colony Artillery still hunts fortified
ports. **REF stack:** only one Regular per colony tile is FORTIFY/FORTIFIED;
extra Regulars on the same tile hunt instead — **after-capture next colony:**
extras not on the fortify-stack garrison slot prefer the **nearest remaining
human colony** by strict MD (no capital MD slack; ignore closer human land
units). Source: fandom REF AI uncaptured-colony / weakest-port pressure.
Deep multi-step siege / combat scoring remains PARKED.

### Wartime MoW sail + unload + idle coastal patrol

Crown **Man-O-War** (Galleon fallback) during `war_act`:
- `cargo_count > 0`: when adjacent to foundable/coastal land by a human colony
  (prefer the colony tile — unload+seize/attack path), unload **one**
  passenger via `units_unload_passenger` (prefer Regular; else Dragoon when
  cargo allows); else `AI_SAIL` toward water adjacent to a human colony.
- `cargo_count == 0` (idle empty): `AI_SAIL` coastal patrol toward water
  adjacent to the nearest human coastal colony (redirect existing ships only —
  do not invent new MoW).

Steps on water only; naval combat if a human ship blocks. Full multi-slot
embark / fandom MoW×6 hold chrome remains **PARKED** (0982 structural
hold-size-3 Regular-then-Dragoon unload on wave spawn still lands on the
colony tile same beat; third foreign landing @diff≥2 smoke covers pressure
without ×6 chrome).

### REF idle fortify on crown colony

Wartime Regular standing on an **own (crown)** colony (including a captured
human capital) with moves and **no** adjacent human land unit or human colony
→ `units_order_fortify` **only if** no other crown Regular on that tile is
already FORTIFY/FORTIFIED (one garrison; extras fall through to hunt).
Already FORTIFY/FORTIFIED Regulars stay put (do not wake to hunt). Adjacent
uncaptured colony or human unit still hunts.

## Linux `ai_king_nation_turn` checklist

1. SoL (`0004`)
2. If !WoI: tax (`1d42`) → SoL 40–49 chrome (+ optional high-tax mention) → declare gate (`2564`/`1a26`; seeds REF + thin `backup_force` + thin `160a` rename + `unknown46[5]` congress)
3. If WoI: wave (`0982` MoW on water adjacent + second MoW @diff≥2 if pool allows + thin cargo unload×3 / `06a6` + thin `1528` status; Artillery prefer if target fortified) → war act (`10f0` ≤2 landings, third @diff≥2 if REF empty + backup, nation pick by colonies, REF Regular/Dragoon/Artillery/Cont. land hunt + capital MD bias + after-capture extras → next nearest human colony + Dragoon/Cont. Cav open bias + Artillery adjacent-fort tighten + capture + capture status + fortify **one** Regular (stack extras hunt) + **Artillery FORTIFY after capture / idle on crown colony** (Euro pattern), idle Regular on crown/captured capital → fortify one (extras hunt; already FORTIFY/FORTIFIED stay), MoW+cargo unload-at-coast (prefer colony tile) else AI_SAIL→human coast, idle empty MoW AI_SAIL coastal patrol, thin `2244` merc hire **or** cannot-afford once, `1eca` colony-SoL promote + Cont. Army/**Cont. Cav capital-rally** → founding capital)

## PORT DEBT

- **Done (unpark #2 thin status chrome):** `38fd_5be8` tax audience status (structural refuse + `unknown46[2]` / `boycott_bitmap`); `2564` congress-confirm status + `unknown46[5]`; `2244` merc hire **or** cannot-afford status (`unknown46[3]`)
- **Still PARKED:** real modal widgets for audience / congress confirm / merc hire; `160a` rename **cinematic** (thin `country_name` done); `1528` arrival **chrome/dialog** (thin status announce done)
- Deep `10f0` economy / merc hire / arrival chrome — **PARKED** (≤2 + third @diff≥2 + Regular/Dragoon mix + nation-by-colonies pick + drain done)
- Deep `1eca` veteran-profession / type-id promote table — **PARKED** (colony-SoL tile bias Done: SoL>50 Soldier/Dragoon/Regular + SoL 40–50 Soldier→Veteran Soldier)
- Full MoW cargo-hold chrome / embark slots (fandom×6) — **PARKED** (structural hold-size-3 Regular-then-Dragoon unload on `0982` MoW spawn done; second MoW @diff≥2 when pool allows Done; wartime MoW+cargo unload-one-at-coast prefer colony tile, Regular else Dragoon + AI_SAIL→human coast Done; idle empty MoW AI_SAIL coastal patrol Done); multi-slot seize-landing polish
- REF deep multi-step land combat / full siege scoring — **PARKED** (Regular/Dragoon/Artillery/Cont. hunt + Artillery fortified-colony spawn/hunt bias + Artillery adjacent-unfortified must not override fortified hunt Done + Dragoon/Cont. Cav open-land bias when Artillery exists + **capital MD bias** (founding capital over distant colonies when MD within slack) Done + **after-capture next colony** (stack extras prefer nearest remaining human colony) Done + adjacent colony prefer + capture + human capture status + fortify one Regular / stack extras hunt + **Artillery after-capture / idle on crown colony FORTIFY** (Euro pattern) Done + idle fortify one on crown/captured capital + already-garrisoned stay Done; Cont. Army/Cav → human capital/colony after 1eca Done)
- Extra refuse boycott cargo bits beyond Sugar — **PARKED** (only Sugar named in-file; do not invent a second bit); refuse sync when `boycott_bitmap==0` (Fugger/external clear) Done
- Full MoW×6 embark / `cargo_ids` hold chrome — **PARKED** (structural hold-size-3 Regular+Dragoon + coastal unload-one + MoW spawn on water-adjacent smoke Done; smoke asserts unload ≤3 / not invent ×6)
