# King / REF / independence (`43f7`) — thin section-map

Layer D hygiene for tax → declare → REF → war. Linux:
[`src/core/ai_king.c`](../../src/core/ai_king.c) — **partial structural port**.
Odd deviations OK; not T3.

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
| `2564` / `1a26` | Declare gate / crown setup | `ai_king_try_declare` (auto when no `ai_popups`; else CHOICE Confirm/Not yet → `ai_king_apply_popup_result` / `ai_king_do_declare`; `unknown46[5]`) |
| `160a` | Independence rename cinematic | thin rename on declare (`country_name`); letter-anim PARKED |
| `060a` | Garrison score / landing pick | `ai_king_weakest_port` |
| `0982` | REF wave MoW + pools | `ai_king_ref_wave` (pools>0; thin MoW cargo unload) |
| `06a6` | Irregulars when REF empty | `ai_king_ref_wave` (else) |
| `1528` | REF arrival announce | thin status + `ai_popup` OK `KING_ARRIVAL` when queue attached |
| `10f0` | Foreign landing when REF empty + `backup_force` (≤2/call; third @diff≥2; prefer Regular+Dragoon) | `ai_king_foreign_intervene` (via `war_act`) |
| `2244` | Mercenary hire offer | auto-hire when no `ai_popups`; else CHOICE Hire/Decline → apply; cannot-afford OK; Hire/Decline follow-up OK |
| `2022` / `1eca` | War act + Continental/vet promote | `ai_king_war_act` (colony-SoL bias; deep type-id table PARKED) |
| `05ea` / `05f4` | Crown colors | `turn.c` (known) |

## DS / Col1 anchors

| DOS | Linux stand-in |
|-----|----------------|
| `0x5382` bit0 war | `head.unknown26` — **no**; use `head.unknown46[0]` WoI |
| `0x5382` bit1 REF present | `head.unknown46[1]` (thin) |
| Tax boycott / refuse | `head.unknown46[2]` (structural + `38fd_5be8` audience; CHOICE when `ai_popups`) |
| Merc hired/refused this war | `head.unknown46[3]` (`2244` hire/decline/cannot-afford; CHOICE when `ai_popups`) |
| Independence rename | `player[human].country_name` → `"United Colonies"` (+ `europe.nation_name` if present); `unknown46[4]` unused (name field exists) |
| Cargo boycott bits | `nation.boycott_bitmap` (EuropeScreen has none) |
| REF pools `0x53da…` | `head.expeditionary_force[4]` |
| Foreign pools `0x53e2…` | `head.backup_force[4]` — **10f0 stand-in** (seeded on declare) |
| Crown id `0x53d2` | Non-human Euro nation slot (0 or 1) |

Exact `0x5382` Col1 bit rename PARKED.

### Tax boycott / refuse audience (`1d42` + `38fd_5be8`)

When a spring tax year would hike:
- **Human + `ctx->ai_popups`:** enqueue `KING_AUDIENCE` CHOICE Accept/Refuse;
  defer effect to `ai_king_apply_popup_result` (Accept → hike; Refuse → boycott
  path). Status notes the demand.
- **Else (auto):** if `tax_rate >= 20` and (SoL ≥ 30 or liberty bells ≥ 80):
  **refuse** — do not raise tax; set `unknown46[2]`; OR Sugar boycott bit;
  grow REF once; status refuse line. Else Accept hike + status (+ OK popup
  when queue attached on apply path).

While `unknown46[2]` is set, further tax years skip hikes (hold-audience
status + OK when queue attached). Refuse apply/auto also enqueues a Sugar
boycott follow-up OK (`KING_TAX`) when queue attached. Restless SoL chrome
(40..49) must **not** clobber audience lines. Fugger/external
`boycott_bitmap==0` clears `unknown46[2]`. Dump-goods / `38fd_3dc8` RNG
second cargo: **Done** (OR bit via `ai_king_pick_dump_goods_cargo` when
`ctx->rng`; status/OK list **all** `boycott_bitmap` cargo names — refuse
Sugar + second, holds after partial clear). Europe bid eligibility +
price-weight (`local_7a` stand-in via `ctx->europe` cargo bids): **Done** —
when europe set, candidates require `bid > 0`, then roulette by bid; europe
NULL → uniform among all non-boycotted. Dump-goods modal CHOICE — **Done**
(`AI_POPUP_TAG_KING_DUMP_GOODS`; apply ORs chosen cargo then `KING_TAX` OK).
Auto / no-popups still RNG-picks via `ai_king_pick_dump_goods_cargo`.

### Thin `1528` REF arrival announce

When `0982` successfully spawns a ship or land unit, write a short arrival
line to `ctx->status` (if present), enqueue `KING_ARRIVAL` OK when
`ai_popups` attached, and keep `unknown46[1]` REF-present.
Same-turn `war_act` capture may overwrite with capture status.
VGA arrival chrome remains PARKED.
Man-O-War spawns on **water adjacent** to the target colony when any such tile
exists (smoke-asserted; fandom REF man-o-war → ports). At `difficulty ≥ 2`, if
`force[2]` still allows after the first Man-O-War, spawn a **second** MoW
stand-in same beat (same 0982 path; stack if only one water tile). Deep
multi-ship formation chrome remains PARKED.

### MoW cargo board (`0982` / `units_ship_capacity`)

When the wave drains `expeditionary_force[2]` and spawns a Man-O-War, board
REF land into the ship hold via `units_board_stacked` / `cargo_ids` up to
`units_ship_capacity` (Man-O-War type cargo = 6, capped by
`COLONIZE_UNIT_CARGO_MAX`): Regulars from `force[0]` first, then Dragoons from
`force[1]` while slots remain; drain those pools only (never invent beyond
`force[]`). Coastal multi-unload at human colony/coast in wartime `war_act`
dumps up to `min(moves_left, capacity)` passengers (Regular prefer, else
Dragoon; `units_unload_passenger`). If both land pools are empty, still
guarantee one land from another pool same beat (colony tile). Source: fandom
REF “Men-O-War, Regulars, Cavalry”; “man-o-war with 6 units”. **PARK:**
`160a` letter cinematic; full embark UI chrome.

### Thin `2244` Continental merc hire

During wartime `war_act`: if SoL > 50, `unknown46[3]` unset, human port exists:
- **gold < 300:** set `unknown46[3]`, status cannot-afford, OK popup when queue.
- **gold ≥ 300 + `ai_popups`:** CHOICE Hire/Decline; apply Hire → spend/spawn +
  success follow-up OK; Decline → set `unknown46[3]` + declined follow-up OK.
  Esc cancel leaves flag clear (re-offer).
- **gold ≥ 300, no queue:** auto-hire (spend/spawn/flag/status; success OK if
  queue attached on fall-through).

`unknown46[3]` gates once-per-war after hire/decline/cannot-afford.

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
- **colony SoL 40..50** (incl. exactly **50**): promote **Soldier** →
  `Veteran Soldier` only when that type exists (no Continental). Regular and
  Dragoon unchanged. Already Cont./Veteran/`Cont. Army`/`Cont. Cav` skipped.

Source: `FUN_43f7_1eca` / catalog “when colony SoL>50%”. King promote path
only — **not** FF Washington mass-promote / combat upgrade. Deep
veteran-profession / type-id table remains PARKED.

After promote, idle human **Cont. Army / Cont. Cav** (hunter name check includes
`Continental` / `Cont. Army` / `Cont. Cav`) `AI_MOVE` toward the **nearest**
human colony, then prefer the **founding capital** (lowest colony id) when
`cap_md ≤ nearest_colony_md + AI_KING_CAPITAL_MD_SLACK` (same helper as REF idle
hunters); fallback `weakest_port` when no human colony. Hold if already on a
human colony tile; on **founding capital** fortify up to two Cont. Army / Cont.
Cav when stack count < 2 (Defending a Colony cap 2; Army prefer over Cav).
Source: fandom Independence Cont. Army / Cont. Cavalry + REF main-port MD slack;
deep rebel AI PARKED.

### Thin `160a` independence rename + `2564` congress

Gate (SoL≥50 + bells≥100): human + `ai_popups` → `KING_CONGRESS` CHOICE
Confirm/Not yet (Confirm → `ai_king_do_declare`); else auto-declare.
On declare: status `"Congress declares independence!"`; rename
`country_name` → `"United Colonies"` (+ `europe.nation_name`); set
`unknown46[5]`. With `ai_popups`: enqueue rename OK (`United Colonies`) then
`"War of Independence begins!"` OK (`FUN_43f7_160a` / `1a26` chain). Same-turn
wave may overwrite status. Letter-anim cinematic **PARKED**. `unknown46[4]`
unused.

### Thin pre-declare SoL chrome

Peacetime before the declare gate: if SoL is **40..49**
(`AI_KING_RESTLESS_SOL_MIN` .. `AI_KING_DECLARE_SOL_MIN-1`) and `ctx->status`
is present, write `"Sons of Liberty grow restless (%d%%)."`. When `tax_rate`
is already in the refuse band (≥20), append `" Tax is at %u%%."` (reads
existing rate — no invented tax formula). With `ai_popups`, also enqueue INFO
OK (same body). Must **not** overwrite an existing 1d42 audience / tax-hike
status line. Does **not** set WoI/`unknown46[0]` or congress/`unknown46[5]`.
Auto-declare still requires SoL≥**50**
(`AI_KING_DECLARE_SOL_MIN` = FUN_43f7_2564 / fandom total SoL ≥ 50%; no new %)
+ bells≥100 (`AI_KING_DECLARE_BELLS_MIN`).

### Declare SoL gate (2564 — tighten/document only)

`ai_king_try_declare` fires only when `ai_king_sol_percent` is already
**≥ `AI_KING_DECLARE_SOL_MIN` (50)** and liberty bells ≥ 100. Threshold is the
existing 2564/fandom figure — do **not** invent a different SoL %. With
`ai_popups`, Confirm choice applies declare; without, auto-declare.

### Structural `10f0` foreign intervention (≤3 landings)

When WoI and REF pools empty and `backup_force` total > 0:
`ai_king_foreign_intervene` lands up to **two** units near the weakest human
port for a crown-hostile Euro nation (drain one pool entry per spawn);
**three** when `difficulty ≥ 2` (REF-pressure stand-in).
Intervene **nation pick**: prefer the non-human / non-crown Euro with the most
colonies; tie-break by on-map land unit count (`backup_force` is a shared pool).
If both Regular (`backup[0]`) and Dragoon (`backup[1]`) are > 0, prefer that
mix (one of each). Otherwise drain available pool types in order
(MoW pool still lands a Regular stand-in). When landings > 0: status +
`KING_ARRIVAL` OK **once** per beat (`Foreign troops have landed!`; 1528-shaped).
Deep economy / merc hire / VGA arrival chrome remain PARKED.

### REF land hunt + colony capture (war act)

During wartime `war_act`, idle crown **Regular/Dragoon** (and **Artillery** when
the Artillery/Cannon type exists in pool) with moves get `AI_MOVE` toward the
nearest human colony or human land unit (fandom REF AI; conceptual reuse of
Euro land hunt — implemented in `ai_king` only). **Artillery thin siege:**
prefer a fortified human colony (Stockade/Fort/Fortress) when hunting; wave
land spawn prefers `force[3]` Artillery when the target colony is fortified
and the type exists **even if Regular/Dragoon pools are also live** (else fall
back to Regular/Dragoon order; unfortified → Regular first). **Dragoon / Cont. Cav thin open-land bias:** when the Artillery type exists,
prefer human land units / unfortified colonies (leave fortified ports to
Artillery); else nearest. Cont. Army stays nearest (no open bias) — smoke
asserts Cont. Cav→open + Cont. Army→nearest fort colony.
**Capital MD bias:** founding capital = lowest active human colony id (Euro
colonies have no Col1 capital bit). Among colony targets, prefer capital when
`cap_md ≤ nearest_colony_md + 2` (`AI_KING_CAPITAL_MD_SLACK`); human land units
still win on equal/closer MD. **Artillery siege:** same slack among fortified
colony picks when the capital itself is fortified (smoke). Prefer an
**adjacent** uncaptured human colony over marching past (**Artillery
exception:** do not override a fortified hunt target with an unfortified
adjacent colony; adjacent fortified still wins). Adjacent
human unit → `units_resolve_land_combat`; attack win → occupy tile. Ending on
(or already standing on) a human colony tile → `colonies_capture` (conquest;
no gold fiction); thin human status
`"The King's forces have captured %s!"` (colony name; conquest chrome PARKED);
then **fortify up to two garrison** units on the tile via `units_order_fortify`:
prefer Regular (capturer if Regular, else another crown Regular on tile); if
none, one **Dragoon or Cont. Cav**; second slot when another idle garrison on
tile has moves_left > 0 (cap 2). Cite: Colonization.pdf Defending a Colony
("fortify soldiers, dragoons…"); king_ref thin multi-garrison. **Artillery after
capture (Euro pattern):** fortify crown Artillery on the captured tile
(capturer if Artillery, else each idle Artillery on tile); idle Artillery on
crown/captured colony with no adjacent human foe/colony also `FORTIFY` and hold
(Colonization.pdf fortify defense; euro_unit_act Artillery fortify after siege;
case 0x0b). Off-colony Artillery still hunts fortified ports. **REF stack:**
up to two Regular/Dragoon/Cont.Cav per colony tile FORTIFY/FORTIFIED; third+
hunt instead — **after-capture next colony:** extras when cap-2 stack is full
prefer the **nearest remaining human colony** by strict MD (no capital MD slack;
ignore closer human land units). Source: fandom REF AI uncaptured-colony /
weakest-port pressure. Multi-garrison chrome PARKED.
Deep multi-step siege / combat scoring remains PARKED.

### Wartime MoW sail + unload + idle coastal patrol

Crown **Man-O-War** (Galleon fallback) during `war_act`:
- `cargo_count > 0`: when adjacent to foundable/coastal land by a human colony
  (prefer the colony tile — unload+seize/attack path), unload up to
  `min(moves_left, capacity)` passengers via `units_unload_passenger`
  (prefer Regular; else Dragoon when cargo allows; **1 ship MP per pax**);
  same-beat seize/fortify for passengers skipped while aboard; else `AI_SAIL`
  toward water adjacent to a human colony. **After full unload** with moves
  left: `AI_SAIL` toward the **next** human coast (skip the port just served;
  fall back to nearest if no other coastal port). **After that sail step** (or
  when already on the next-coast water tile): if still carrying and now
  adjacent to a human colony, prefer unload same beat (then retarget next
  coast if the hold empties with moves left). Partial unload holds leftover
  cargo for the next beat.
- `cargo_count == 0` (idle empty): `AI_SAIL` coastal patrol toward water
  adjacent to the nearest human coastal colony (redirect existing ships only —
  do not invent new MoW).

Steps on water only; naval combat if a human ship blocks. 0982 boards up to
ship capacity into `cargo_ids` (Regular-then-Dragoon); multi-unload here.
**PARK:** embark UI chrome; `160a` letter cinematic (thin `@INDEPENDENCE` KING_LETTER Done).

### REF idle / post-capture garrison (one-stack)

Wartime **Regular**, or **Dragoon / Cont. Cav** when no Regular is available,
standing on an **own (crown)** colony (including a captured human capital) with
moves and **no** adjacent human land unit or human colony →
`units_order_fortify` **only if** fewer than two crown Regular/Dragoon/Cont.Cav
on that tile are already FORTIFY/FORTIFIED (cap 2; second slot needs
moves_left > 0). Prefer Regular over cavalry on the same tile. Already
FORTIFY/FORTIFIED garrison units stay put (do not wake to hunt). Third+
extras fall through to hunt. Adjacent uncaptured colony or human unit still
hunts. Cite: Colonization.pdf Defending a Colony ("fortify soldiers,
dragoons…"); king_ref thin multi-garrison (cap 2). Multi-garrison chrome
**PARKED**.

## Linux `ai_king_nation_turn` checklist

1. SoL (`0004`)
2. If !WoI: tax (`1d42`) → SoL 40–49 chrome (+ optional high-tax mention) → declare gate (`2564`/`1a26`; seeds REF + thin `backup_force` + thin `160a` rename + `unknown46[5]` congress)
3. If WoI: wave (`0982` MoW on water adjacent + second MoW @diff≥2 if pool allows + board up to ship capacity into `cargo_ids` / `06a6` + thin `1528` status; Artillery prefer if target fortified) → war act (`10f0` ≤2 landings, third @diff≥2 if REF empty + backup, nation pick by colonies, REF Regular/Dragoon/Artillery/Cont. land hunt + capital MD bias + after-capture extras → next nearest human colony + Dragoon/Cont. Cav open bias + Artillery adjacent-fort tighten + capture + capture status + fortify **up to two** Regular else Dragoon/Cont.Cav (Defending a Colony cap 2; third+ hunt) + **Artillery FORTIFY after capture / idle on crown colony** (Euro pattern), idle Regular/Dragoon/Cont.Cav on crown/captured capital → fortify up to two (third+ hunt; already FORTIFY/FORTIFIED stay), MoW+cargo multi-unload-at-coast ≤moves/capacity (1 MP/pax; prefer colony tile; same-beat seize/fortify) else AI_SAIL→human coast; **full unload + moves left → AI_SAIL next human coast**, idle empty MoW AI_SAIL coastal patrol, thin `2244` merc hire **or** cannot-afford once, `1eca` colony-SoL promote + Cont. Army/**Cont. Cav capital-rally** → nearest colony + founding-capital MD slack + **Cont. Army/Cav fortify on founding capital (cap 2)**)

## PORT DEBT

- **Done (ai_popup unpark):** `38fd_5be8` audience CHOICE Accept/Refuse (+ auto when no queue); `2564` congress CHOICE Confirm/Not yet; `2244` merc CHOICE Hire/Decline + cannot-afford OK + Hire success follow-up OK + Decline follow-up OK; `1528` REF arrival OK; `10f0` intervene landing ARRIVAL once; capture OK; tax hike OK on Accept apply
- **Done (structural REF / rebel — Marathon3):** **Dragoon garrison** (up to two Regular else Dragoon/Cont. Cav after capture / idle on crown; Defending a Colony cap 2; multi-garrison chrome still PARKED); **Cont. capital-rally** (nearest human colony + founding-capital MD slack; hold on colony tile; **Cont. Army/Cav fortify on founding capital cap 2**); **Artillery siege spawn** (`force[3]` prefer when target fortified even if Regular/Dragoon live; unfortified → Regular first); **SoL50 band** (`1eca`: SoL>50 Continental; exactly 50 mid-band Soldier→Veteran only, Dragoon unchanged). Smoke covers each.
- **Still PARKED (king modals / chrome):** VGA-identical wood chrome; `160a` rename **letter cinematic** (thin `country_name` + rename/WoI OK done); dump-goods `38fd_3dc8` **modal CHOICE** (Sugar + RNG second named in status/OK; Europe `bid>0` eligibility + weight Done — do **not** invent a fixed second refuse cargo); deep `10f0` economy / merc-hire dialog beyond thin OK; full MoW embark **UI**; REF deep siege scoring UI
- Deep `10f0` economy / merc hire / VGA arrival chrome — **PARKED** (≤2 + third @diff≥2 + Regular/Dragoon mix + nation-by-colonies pick + drain + thin ARRIVAL OK once Done)
- Deep `1eca` veteran-profession / type-id promote table — **PARKED** (colony-SoL tile bias + SoL50 mid-band + Cont. abbrev skip Done above; deep type-id table still PARKED)
- MoW hold fill + multi-unload — **Done** (`0982` boards Regular-then-Dragoon into `cargo_ids` up to `units_ship_capacity` / MoW×6; second MoW @diff≥2; wartime unload up to `min(moves_left, capacity)` at coast prefer colony tile (1 MP/pax) + same-beat seize/fortify + AI_SAIL→coast; **full unload + moves left → next human coast**; **after next-coast sail prefer unload if already adjacent**; idle empty MoW coastal patrol). Embark UI chrome — **PARKED**
- REF deep multi-step land combat / full siege scoring — **PARKED** (thin hunt/capture/garrison cap-2/Artillery/Cont. structural Done above; deeper combat scoring UI still PARKED). Multi-garrison chrome **PARKED**.
- Dump-goods refuse second cargo (`38fd_3dc8` RNG OR + all bitmap cargo names in status/OK) — **Done**; Europe `bid>0` eligibility + price-weight — **Done**; dump modal CHOICE (`KING_DUMP_GOODS`) — **Done**; refuse sync when `boycott_bitmap==0` (Fugger/external clear) Done
- `160a` letter cinematic — **PARKED** (thin rename + OK chain Done)
