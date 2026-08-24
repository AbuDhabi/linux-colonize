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
| Rebel | once `1eca` promote (colony-SoL bands; Soldier/Regular + Dragoon/Cavalry); each turn: REF-absent-or-artillery-pool-empty → self-funded troop-gift roll (own treasury) via `10f0` mode 1; else `10f0` mode 0 drains `backup_force` free (see "`2244`/`2022` — corrected" below, **not** a human-facing merc hire) |

## Key symbols → Linux

| Symbol | Role | Linux |
|--------|------|-------|
| `0004` | Pop-weighted SoL | `ai_king_sol_percent` |
| `1d42` | Tax→REF funding | `ai_king_tax_event` |
| `2564` / `1a26` | Declare gate / crown setup | `ai_king_try_declare` (auto when no `ai_popups`; else `@DECLARE` CHOICE Never/Yes → `ai_king_apply_popup_result` / `ai_king_do_declare`; `unknown46[5]`) |
| `160a` | Independence rename cinematic | thin rename on declare (`country_name`); letter-anim PARKED |
| `060a` | Garrison score / landing pick | `ai_king_weakest_port` |
| `0982` | REF wave MoW + pools | `ai_king_ref_wave` (pools>0; thin MoW cargo unload) |
| `06a6` | Irregulars when REF empty | `ai_king_ref_wave` (else) |
| `1528` | REF arrival announce | `@INVASION` status + `ai_popup` OK `KING_ARRIVAL` when queue attached |
| `10f0` | Foreign landing when REF empty + `backup_force` (≤2/call; third @diff≥2; prefer Regular+Dragoon) | `ai_king_foreign_intervene`. Slot mix **Done** Phase 4; coastal roulette + 8-neighbor scorer + per-call caps + Veteran 0x15 **Done** Phase 5. **PARK:** foreign MoW ship |
| `2244` | Peacetime AI-nation self-funded troop gift (**not** a human merc hire — see "`2244`/`2022` — corrected" below) | `ai_king_ai_peacetime_gift` **Done** |
| `2022` / `1eca` | War act + Continental/vet promote | `ai_king_war_act` (colony-SoL bias; Veteran-profession gate — see `1eca` note below) |
| `05ea` / `05f4` | Crown colors | `turn.c` (known) |

## DS / Col1 anchors

| DOS | Linux stand-in |
|-----|----------------|
| `0x5382` bit0 war | `head.game_options.woi` (mapped); `unknown46[0]` legacy sync only |
| `0x5382` bit1 REF present | `head.unknown46[1]` (thin) |
| Tax boycott flag | `head.unknown46[2]` — presentation/Fugger-sync only since 2026-08-19 (real `38fd_5be8`/`38fd_3dc8` audience no longer gated by it; see "Tax audience" section below) |
| `head.unknown46[3]` | unused — removed with the old invented once-per-war merc gate; `2022` rebel troop-gift is now a real recurring per-turn roll, no latch |
| Independence rename | `player[human].country_name` → `"United Colonies"` (+ `europe.nation_name` if present); `unknown46[4]` endgame latch (0 none / 1 won / 2 lost / peace-1800) |
| Mid-war `@WARN1` episode | `head.unknown46[6]` — set when one coastal port left + REF; clear when ports>1 |
| Mid-war `@WARN2` episode | `head.unknown46[7]` — set when one colony left + REF; clear when colonies>1 |
| Mid-war `@WARN3` episode | `head.unknown46[10]` — crown pop share 50–89%; clear when share <50% |
| `@SOONRETIRING0` once | `head.unknown46[8]` — peacetime Spring 1790 |
| `@SOONRETIRING1` once | `head.unknown46[9]` — wartime 1840 |
| Cargo boycott bits | `nation.boycott_bitmap` (EuropeScreen has none) |
| REF pools `0x53da…` | `head.expeditionary_force[4]` |
| Foreign pools `0x53e2…` | `head.backup_force[4]` — **10f0 stand-in** (seeded on declare) |
| Crown id `0x53d2` | Non-human Euro nation slot (0 or 1) |

Exact `0x5382` Col1 bit rename: **Done** — `game_options.woi` mapped; `unknown46[0]` kept in sync on declare for legacy Linux saves.

### `2424` tail — `rebel_sentiment_report` (2026-08-22)

DOS `FUN_43f7_2424` peacetime path stores `FUN_43f7_0004` result in
`DS:0x53d0` (`head.rebel_sentiment_report`) for tax-audience favor scoring
(`38fd_5be8`: `RNG(1,1000) + (report*2 − tax)*5`). Linux:
`ai_king_nation_turn` writes `rebel_sentiment_report = ai_king_sol_percent`
at **turn end** (audience on the same turn uses the prior cached value).
Decile congress notify (`sol_pct_last_notified`, status-only) **Done** thin;
full 0x1362/0x1358/0x136a popup chrome remains **PARK**.

### `0108` diplo-clear/set on nation elimination — done (2026-08-14)

`FUN_43f7_0108` (eliminate nation, called from `1a26`'s declare-independence
fold loop for every nation that is neither the declaring human `DS:0x5398`
nor the crown `DS:0x53d2`) does four diplo writes before the already-ported
unit-scrub + `control=2`: `FUN_281f_0a10`/`FUN_15b3_00d0` (clear-both,
bitmask `0xb`) and `switchD_2000:da9f::caseD_10`/`FUN_15b3_0066` (or-both,
bitmask `0x60`), each called once against the declaring human and once
against the crown. **Resolved this pass**: `caseD_10` is not a generic
event dispatcher (correcting the framing used in `euro_diplo_3180_full.md`
for a different call site) — it's a Ghidra-named single-case thunk
straight to `FUN_15b3_0066`, i.e. exactly the already-known/ported
`FUN_15b3_0066`/`00d0` pair (`ai_diplo_or_both`/`ai_diplo_clear_both`,
`FUNCTION_CATALOG.md` row already had this). `0xb` = `WAR(0x1)|PEACE(0x2)|
unmapped-bit3(0x8)`; `0x60` = `unmapped-bit5(0x20)|MET(0x40)`. Ported the
mapped bits (`WAR`/`PEACE` clear, `MET` set) via existing
`ai_diplo_clear_both`/`or_both` in `ai_king_do_declare`'s fold loop,
targeting both `human` and `ai_king_crown_nation(human)` — so "no
crown-nation-slot diplomacy model" (this doc's old framing) was also
stale: `ai_king_crown_nation` already reuses nation slot 0/1 as the crown
stand-in for exactly this kind of write. The two unmapped bits (`0x8`,
`0x20`) are intentionally not applied — no other site in the decompile
was found this pass that pins their meaning down. Guarded to skip
`n==crown_fold` (DOS's `0108` is never called with the crown as the
eliminated nation). Covered by `unit_ai_king`'s declare-path assertions
(pre-seeds nation-2 WAR vs human, asserts WAR/PEACE cleared + MET set vs
both human and crown fold post-declare).

### Tax audience (`38fd_5be8` + `38fd_3dc8`) — real formula ported 2026-08-19

Replaces the earlier invented "Accept/Refuse gates whether the hike
happens" design (that divergence is what `king_audience_tax_delta` in
`docs/mysteries_catalog.md` originally flagged). Real DOS shape, decoded
from `viceroy_unpacked.c:68420` (`5be8`) and `:64132` (`3dc8`):

- **Gate** (`ai_king_audience_roll`): fires on a **turn-counter** interval
  (`ctx->turn_number`, DOS `DS:0x538e`) — no spring-only restriction (the
  caller, `FUN_38fd_5e52`, has no season check either). Needs
  `turn_number ≥ 30`; interval = year-band base (18/15/12/9 for
  year ≤1600/≤1700/≤1750/>1750) narrowed by difficulty (`interval_base −
  2·(difficulty−2)`, the DOS "is-human" branch, always true here since this
  port only rolls the human's own audience); skip entirely if
  `tax_rate > 85`.
- **Score**: `RNG(1,1000) + (rebel_sentiment_report·2 − tax_rate)·5 +
  treasury/100 + this-nation SoL% + turn/30`. The SoL term substitutes
  `ai_king_sol_percent` for a DOS per-nation SoL% cache table
  (`DS:nation−0x6bf0`) this port doesn't maintain — same value, no stored
  cache, documented substitution not a guess.
- **Ladder**: `score<100` → cut `= −min(RNG(2,5), tax_rate)`, but **no
  audience at all** if that cut would be 0 (tax already 0%); `100≤score<650`
  and streak<30 → `+1` (streak++); `score>949` → `+3/+4` (score<1100) or
  `+5..+8`; else → `+2` (covers 650..949 and the streak≥30 fallback).
- **Apply** (`ai_king_audience_apply_delta`, `3dc8`): **unconditional** —
  `tax_rate += delta`, floored so it can't go below 0%, ceiled at 75%
  (excess trimmed back out of the "applied" amount used below). No
  Accept/Refuse gate on whether this happens at all.
- **Village-goods choice**: only reachable when the *applied* delta is
  positive (a real raise) and a Europe-bid-eligible, non-boycotted cargo
  candidate exists (`ai_king_pick_dump_goods_cargo` roulette by stock×bid,
  picked *before* the popup, exactly one cargo — no fixed Sugar-first, no
  separate "name a good" menu). Human + `ai_popups`: single `KING_AUDIENCE`
  CHOICE — Accept ("kiss the ring") keeps the raise; Refuse ("tea party",
  `ai_king_tax_teaparty`) **reverts** the applied delta and boycotts the one
  picked cargo, seizing stock via the existing `@TEAPARTY` OK (richest human
  colony, cap 100 — thin stand-in for DOS's own colony-array seize-into-
  royal-stock write, real field never resolved). No candidate / no popup
  queue → hike just stands, still surfaced as an OK popup for human UX.
  Auto/no-popups path: DOS's choice is inherently player-interactive with no
  documented AI answer, so the tea-party decision there is an invented
  stand-in heuristic (`tax_rate≥20` and (`SoL≥30` or `bells≥80`)), unrelated
  to the real delta formula above.

`unknown46[2]` (boycott-active) no longer gates the audience interval — that
was invented; it's presentation/Fugger-sync only now (`ai_king_sync_boycott_
refuse` still clears it when `boycott_bitmap==0`). Restless SoL chrome
(40..49) still must not clobber audience status lines. VGA `@TEAPARTY`
chrome remains PARKED.

**Known gap:** `tests/unit/test_ai_king.c` predates this port and assumes
the old deterministic/year-gated/RNG-free design throughout — its first tax
scenario (no `ctx.rng`, `turn=1`) now fails before the rest of the
6000+-line single-`main()` file runs. Rewriting the fixture for seeded RNG
+ turn-based setup is real follow-up work, out of scope for the formula
port; see `docs/ai_transcription.md` R6 for the fuller note.

### Thin `1528` REF arrival announce

When `0982` successfully spawns a ship or land unit, write GAME.TXT `@INVASION`
(`Royal Expeditionary Force lands near {%STRING0}!`, `STRING0` = weakest-port
colony name) to `ctx->status` (if present), enqueue `KING_ARRIVAL` OK when
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

### `2244`/`2022` — corrected 2026-08-14, this section was misidentified

**The "Thin `2244` Continental merc hire" write-up below described a
mechanic that doesn't exist in either `FUN_43f7_2244` or `FUN_43f7_2022`
— it conflated the two and invented a threshold neither uses.** Traced
both functions in full (canonical + a corrected-offset Ghidra recovery
for `2244`, which turned out NOT corrupted despite its `"gap"`
`address_mapping.csv` kind — the naive `OVL07_L0040:2244` overlay-offset
guess was wrong, the real offset is `0x3164`, a consistent `+0xf20` shift
from every `43f7:XXXX` citation in this segment; the *content* at the
canonical line citation was correct all along, three independent sources
agree byte-for-byte).

**What they really are:**
- **`FUN_43f7_2022(param_1)`** — the real wartime **per-nation** dispatcher
  (`viceroy_unpacked.c:74976-75070`), called once per Euro nation's turn
  during WoI. Crown branch: tax residual (`1d42`) then REF-pool-based
  invasion/irregulars dispatch (matches this doc's existing "`2022` war
  shell" table exactly). Rebel branch: a one-time setup flag, then **each
  turn**, if REF-present bit (`0x5382` bit **1**, not bit0/WoI) is clear OR
  the artillery backup-pool (`0x53e6`) is empty, rolls a self-funded
  troop-gift purchase (quantity roll into `0x9e46/0x9e48/0x9e4c`, price
  `(qty_terms)*((difficulty+3)*2+roll)*100`, paid from **that nation's
  own** 32-bit treasury at `*(int*)0x84fc+0x2a/0x2c` via a `0x1340` CHOICE
  popup) — else calls `10f0` with mode 0 to drain `backup_force` pools
  directly for free. Either way, ends by calling `thunk_FUN_2a1f_010a` →
  **`FUN_43f7_10f0`** with mode 1 (spend the just-rolled quantities) or 0
  (drain pools) — `10f0` is the actual spawn/landing executor for both
  paths, already documented in this file's own "Structural `10f0`"
  section.
- **`FUN_43f7_2244(void)`** (`viceroy_unpacked.c:75074-75152`, reached via
  `FUN_281f_0668`, called from the **generic per-AI-Euro-nation turn
  loop** — nothing to do with the King/REF/war dispatch chain at all,
  just living in the same code segment) — the **peacetime** twin of
  `2022`'s rebel-gift branch: fires only when WoI is **not yet declared**
  (`0x5382` bit0 clear), picks a random Euro nation and rolls the same
  quantity/price formula (price constant `+4` instead of `2022`'s `+3`,
  popup `0x134c` instead of `0x1340`), paid from **that AI power's own
  treasury**, landing troops via the same `10f0` mode-1 path. Reads as "a
  foreign AI power occasionally funds and gifts itself/an ally
  reinforcements before the war," not a human-facing hire offer at all.

**No SoL check and no literal `300` gold cost exist in either function.**

**`2022` rebel branch — implemented 2026-08-14** (`ai_king_merc_offer` /
`ai_king_do_merc_hire_at` in `ai_king.c`). Gate: WoI declared, REF-present
bit clear OR artillery `backup_force[3]` empty, 1-in-3 roll per turn
(`AI_KING_MERC_ROLL_CHANCE`). Quantity `dos_rng_range(2, ((4-difficulty)>>1)+2)`,
extra-unit coin flip (Artillery vs Dragoon), price
`(qty+2)*((difficulty+3)*2+roll(0,6))*100`, paid from the rebel (human)
nation's own gold — matches the real formula's `+3` price constant and
own-treasury payer. Landing tile picked via `ai_king_weakest_port` at
**offer time** and packed into the popup payload (`hx/hy/qty_a/extra_flag/
price`) so a same-turn colony-capture race can't invalidate it before
CHOICE apply (`ai_king_merc_payload`/`_parts`). Human popups on: CHOICE
Hire/Decline via `AI_POPUP_TAG_KING_MERC`; no popups: auto-accepts same as
the DOS AI-controlled case. No once-per-war gate — DOS has none; the roll
recurs every eligible turn. `AI_KING_MERC_COST`/`AI_KING_MERC_SOL_MIN` (the
old invented SoL/300-gold stand-in) removed.

**`2244` peacetime AI-nation-only branch — implemented 2026-08-14**
(`ai_king_ai_peacetime_gift` in `ai_king.c`, called from
`ai_euro_nation_turn` — the DOS caller, `FUN_281f_0668` at
`viceroy_unpacked.c:6409-6421`, sits in the SAME generic per-Euro-nation
turn loop gated on the identical human-controlled flag byte Linux's
`turn_run_european_ai_stubs` already uses to skip the human, confirming
this is genuinely unreachable for a human turn). Gate: WoI not declared,
1-in-21 roll. Picks a random Euro nation 0-3 as beneficiary; eligible only
if that's the acting nation itself or an ally (`AI_DIPLO_ALLY`).

**The quantity-roll shape is NOT identical to `2022`'s** — read both raw
bodies side by side (`viceroy_unpacked.c:75098-75113` vs `:75017-75028`)
rather than trust this doc's own earlier "same formula" summary, which
undersold a real difference: `2244` never touches the Dragoon slot at all
(regular = `range(1,3)`; a coin flip either sets artillery to 1-or-2, or
adds +1 more to regular — no Dragoon path exists here, unlike `2022`
which sometimes fills the Dragoon slot instead of Artillery). Price
constant is `+4` (not `2022`'s `+3`), matching this doc's original claim.
Paid from the acting AI nation's own gold; troops land for the
beneficiary (self or ally) at its own weakest port. No human popup is
reachable through this call chain — always auto-accepts when affordable.

**Approximated**: which global exactly represents "the acting nation" for
the self/ally eligibility check (DOS reads `DS:0x5398`, which this
specific call chain doesn't visibly reassign in the read window — the
more locally-scoped loop variable is `DS:0x5396`/`0x5394`) — used Linux's
own per-AI-nation-turn loop variable, matching every other established
convention in this codebase; not independently confirmed byte-exact for
this one call chain. Test: `test_ai_king.c`'s dedicated seeded block
(seed=13 deterministically hits both the gate and a self-gift roll on its
first two calls).

### WoI flag (`unknown46[0]`)

`ai_king_try_declare` (SoL≥`AI_KING_DECLARE_SOL_MIN` 50 only) sets
`head.unknown46[0]` via `ai_king_set_independence` (DOS `0x5382` bit0 stand-in;
exact Col1 bit PARKED). Idempotent if already set. Restless SoL chrome
(40..49) must **not** set this byte. Smoke: SoL 49 leaves
`unknown46[0]` clear; declare at SoL≥50 sets it.

### `1eca` Continental promote (colony-SoL bias) — full port

Direct read of the decompiled body (not the older secondary catalog
summary, which mis-described a Regular branch and an SoL 40..50 vet band
that the raw function does not have): per **colony** owned by the rebel
nation with **colony SoL > 49** (`0x31 < iVar1`, i.e. `sol>49`, so exactly
50 already qualifies):

```
cap = pop >> 1
alt = pop * (sol - 50) / 50
if alt < cap: cap = alt
if cap < 1: cap = 1
```

The decomp then walks *only the units stationed on that colony's own tile*
(its tile unit-stack, not every unit the nation owns anywhere), and for each
that is raw type **1** (Soldier) or **4** (Dragoon), **and**
profession `unit+0x315b == 0x15`, spends one slot of `cap` to promote:
Soldier → Continental Army (type 9), Dragoon → Continental Cavalry (type 7).
The budget is shared across both types in tile scan order — at the SoL==50
edge `cap` is always exactly 1 regardless of population (the `alt` term is
0), so only the first eligible unit found promotes that turn; the rest wait
for a later turn. Regular and already-Continental units never match the raw
type check and are never touched. A colony-count message pops when
`promoted>0` (singular/plural).

**Profession gate resolved 2026-08-14, corrects a wrong claim in the
paragraph above** ("Regular, Veteran... never match" was backwards). DOS
code `0x15` is `UNITS_JOB_SOLDIER` ("Veteran Soldiers") — confirmed via the
same offset (`unit+0x315b`) and adjacent code (`0x14`=Pioneer) already
established in the case-8/9 terrain-improve investigation
(`euro_unit_act.md`). So **only Veteran-status** Soldier/Dragoon promote —
an ordinary armed colonist (type Soldier/Dragoon, profession
`UNITS_JOB_NONE`) does **not**, even fortified on the colony's own tile.
Previously unported (Linux checked only the raw type, no profession gate)
— **fixed same day**: `ai_king_war_act`'s 1eca block now also requires
`u->profession == UNITS_JOB_SOLDIER`. Verified against `unit_ai_king`'s
existing 1eca test blocks (updated to grant Veteran profession to the
units expected to promote) plus a new negative case (fortified, own-tile,
right type, but `UNITS_JOB_NONE` — must not promote). Full `ctest` green,
including golden fixtures — the stricter gate never conflicted with any
already-verified seed-100 behavior.

**Fortify-gate correction (2026-08-24)**: the paragraph above previously
claimed a `FORTIFIED` requirement as part of "direct read of the decompiled
body" — that was wrong. A full end-to-end read of `FUN_43f7_1eca`
(`viceroy_unpacked.c:74910-74972`) shows only two per-unit tests: the type
byte at `unit+0x3146` and the profession byte at `unit+0x315b`. `orders`
(`ViceroyUnit.orders`, `original_sources_annotated/include/viceroy_types.h`)
lives at `unit+0x08`, an address this function never reads. The Linux port
had carried an extra `u->orders != UNITS_ORDER_FORTIFIED` gate (added
2026-08-14 alongside the real profession-gate fix, apparently by inference
rather than a direct byte-offset citation) that DOS does not have — removed
this pass, tests updated to prove a non-fortified on-tile Veteran Soldier
now promotes.

Source: `FUN_43f7_1eca`. King promote path only — **not** FF Washington
mass-promote / combat upgrade. Linux: `ai_king_war_act` in `ai_king.c`
(`unit_ai_king` 1eca block covers the own-tile-gate, Veteran-profession-gate,
shared-cap, and SoL==50-edge cases).

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

(SoL≥50 only): human + `ai_popups` → `KING_CONGRESS` CHOICE from
GAME.TXT `@DECLARE` (Never / Yes; Yes → `ai_king_do_declare`); else auto-declare.
On declare: status `"Congress declares independence!"`; rename
`country_name` → `"United Colonies"` (+ `europe.nation_name`); set
`unknown46[5]`. With `ai_popups`: enqueue rename OK (`United Colonies`) then
`"Road to Freedom"` / `@HOWTOWIN` OK (`FUN_43f7_160a` / `1a26` chain; invent
`"War of Independence begins!"` demoted). Same-turn
wave may overwrite status. Letter-anim cinematic **PARKED**. Endgame latch
`unknown46[4]` set by revolution win/lose / peace-1800 (not by declare).
Peacetime year≥1800: latch `PEACE_1800` + GAME.TXT `@SCORED` CHOICE
(`AI_POPUP_TAG_KING_SCORED`; That's all → `@RETIRING` + retire score via game_loop /
Keep playing).
Peacetime Spring 1790: `@SOONRETIRING0` once (`unknown46[8]`). Wartime 1840:
`@SOONRETIRING1` once (`unknown46[9]`).
Mid-war `@WARN1` (one coastal port + REF) uses `unknown46[6]` episode latch;
`@WARN2` (one colony + REF) uses `unknown46[7]`. Armed only when WoI+REF
already set at turn entry (peacetime tax may set REF early; declare beat keeps
`@INVASION`). Status write skipped if buffer already non-empty (same-turn
wave/merc).

### Thin pre-declare SoL chrome

Peacetime before the declare gate: if SoL is **40..49**
(`AI_KING_RESTLESS_SOL_MIN` .. `AI_KING_DECLARE_SOL_MIN-1`) and `ctx->status`
is present, write `"Sons of Liberty grow restless (%d%%)."`. When `tax_rate`
is already in the refuse band (≥20), append `" Tax is at %u%%."` (reads
existing rate — no invented tax formula). Status only (no invented wood OK).
Must **not** overwrite an existing 1d42 audience / tax-hike
status line. Does **not** set WoI/`unknown46[0]` or congress/`unknown46[5]`.
Auto-declare still requires SoL≥**50**
(`AI_KING_DECLARE_SOL_MIN` = FUN_43f7_2564 / fandom total SoL ≥ 50%; no new %)
SoL≥50 only (`FUN_43f7_2564`; port-only bells gate removed 2026-08-22).

### Declare SoL gate (2564 — tighten/document only)

`ai_king_try_declare` fires only when `ai_king_sol_percent` is already
**≥ `AI_KING_DECLARE_SOL_MIN` (50)** — SoL≥50 only; the port-only bells≥100
gate was removed 2026-08-22 (see above). Threshold is the
existing 2564/fandom figure — do **not** invent a different SoL %. With
`ai_popups`, Confirm (`AI_KING_CHOICE_CONFIRM` / `@DECLARE` Yes) applies declare;
without, auto-declare.

### Structural `10f0` foreign intervention (≤3 landings)

When WoI and REF pools empty and `backup_force` total > 0:
`ai_king_foreign_intervene` lands up to **two** units near the weakest human
port for a crown-hostile Euro nation (drain one pool entry per spawn);
**three** when `difficulty ≥ 2` (REF-pressure stand-in).
Intervene **nation pick**: prefer the non-human / non-crown Euro with the most
colonies; tie-break by on-map land unit count (`backup_force` is a shared pool).
If both Regular (`backup[0]`) and Dragoon (`backup[1]`) are > 0, prefer that
mix (one of each). Otherwise drain available pool types in order
(MoW pool still lands a Regular stand-in). When landings > 0: status + `@INTERVENTION` then `@INTERVENE`
`KING_ARRIVAL` OKs per beat (1528-shaped). Deep economy / merc hire / VGA
arrival chrome remain PARKED.

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
`"The King's forces have captured %s!"` status; human `@CAPTURED3` OK
(`popup_msg_fill`; conquest VGA PARKED);
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
3. If WoI: wave (`0982` …) → war act (…) → revolution end check (`@WARN1`/`@WARN2`/`@WARN3` / `@LOSING2`/`@LOSING1`/`@LOSING3` / `@WINNING` / `@RETIRING2`; `unknown46[4]`; warns use `unknown46[6]`/`[7]`/`[10]`)

## PORT DEBT

- **Done (ai_popup unpark):** `38fd_5be8` audience CHOICE Accept/Refuse (+ auto when no queue); `@TEAPARTY` refuse/dump follow-up OK (thin `3dc8` stock dump + tokens); `2564` congress `@DECLARE` CHOICE Never/Yes; merc CHOICE widget now ports real `2022` rebel-branch troop-gift (recurring per-turn roll, real price formula, own-treasury pay — see "corrected" note above; `2244` peacetime AI-only self/ally-funded twin now also ported, `ai_king_ai_peacetime_gift`); `1528` REF `@INVASION` arrival OK; `10f0` `@INTERVENTION`+`@INTERVENE` ARRIVAL; REF `@CAPTURED3` capture OK; tax hike OK on Accept apply; revolution end `@WINNING` / `@LOSING1`–`3` / `@RETIRING2` Done thin (`unknown46[4]` latch); mid-war `@WARN1`–`3` Done thin (`unknown46[6]`/`[7]`/`[10]`); peacetime 1800 `@SCORED` CHOICE + `@RETIRING` on That's all Done thin (`KING_SCORED` → retire score); `@SOONRETIRING0`/`1` Done thin (1790/1840; `unknown46[8]`/`[9]`); declare `@HOWTOWIN` Done thin (invent WoI-begins demoted); restless status-only (invent OK demoted)
- **Done (structural REF / rebel — Marathon3):** **Dragoon garrison** (up to two Regular else Dragoon/Cont. Cav after capture / idle on crown; Defending a Colony cap 2; multi-garrison chrome still PARKED); **Cont. capital-rally** (nearest human colony + founding-capital MD slack; hold on colony tile; **Cont. Army/Cav fortify on founding capital cap 2**); **Artillery siege spawn** (`force[3]` prefer when target fortified even if Regular/Dragoon live; unfortified → Regular first); **SoL50 band** (`1eca`: SoL>50 Continental; exactly 50 mid-band Soldier→Veteran only, Dragoon unchanged). Smoke covers each.
- **Still PARKED (king modals / chrome):** VGA-identical wood chrome; `160a` rename **letter cinematic** (thin `country_name` + rename/WoI OK done); dump-goods `38fd_3dc8` **CHOICE prompt** invent English (picker Done; `@TEAPARTY` after apply Done thin); deep `10f0` economy / merc-hire dialog beyond thin OK; full MoW embark **UI**; REF deep siege scoring UI
- Deep `10f0` economy / merc hire / VGA arrival chrome — **PARKED** (≤2 + third @diff≥2 + Regular/Dragoon mix + nation-by-colonies pick + drain + thin ARRIVAL OK once Done)
- ~~Deep `1eca` veteran-profession / type-id promote table — PARKED~~
  **stale, corrected 2026-08-14**: re-read `FUN_43f7_1eca` in full
  (`viceroy_unpacked.c:74910-74972`, 62 lines, clean, no corruption
  warnings) end to end — there is no type-id table anywhere in it, just
  two literal type checks (`unit+0x3146 == 1` Soldier / `== 4` Dragoon,
  gated on `unit+0x315b == 0x15` profession) and two literal promotions
  (`1→9` Continental Army, `4→7` Continental Cavalry). This matches the
  "full port" section above byte-for-byte — there was never a deep table
  to port; this bullet was describing fabricated content from the same
  era as the `caseD_10`/case-7/`−0x77c4` mischaracterizations already
  corrected elsewhere this session. `1eca` is **fully done**, nothing
  left here.
- MoW hold fill + multi-unload — **Done** (`0982` boards Regular-then-Dragoon into `cargo_ids` up to `units_ship_capacity` / MoW×6; second MoW @diff≥2; wartime unload up to `min(moves_left, capacity)` at coast prefer colony tile (1 MP/pax) + same-beat seize/fortify + AI_SAIL→coast; **full unload + moves left → next human coast**; **after next-coast sail prefer unload if already adjacent**; idle empty MoW coastal patrol). Embark UI chrome — **PARKED**
- REF deep multi-step land combat / full siege scoring — **PARKED** (thin hunt/capture/garrison cap-2/Artillery/Cont. structural Done above; deeper combat scoring UI still PARKED). Multi-garrison chrome **PARKED**.
- Dump-goods refuse second cargo (`38fd_3dc8` RNG OR + all bitmap cargo names in status) — **Done**; Europe `bid>0` eligibility + price-weight — **Done**; dump modal CHOICE (`KING_DUMP_GOODS`) — **Done**; `@TEAPARTY` follow-up OK + thin stock dump — **Done** thin; refuse sync when `boycott_bitmap==0` (Fugger/external clear) Done
- `160a` letter cinematic — **PARKED** (thin rename + OK chain Done)
- **Superseded 2026-08-19** (the `483`/`485`/`501` bullets above describe the
  *pre*-formula audience shell, now stale for the delta/apply mechanics
  specifically — the modal wiring, `@TEAPARTY`, dump-goods roulette, and
  Fugger sync they describe are still accurate): `38fd_5be8`'s delta ladder
  and `38fd_3dc8`'s clamp-apply are now the real formula, not a structural
  stand-in — see "Tax audience (`38fd_5be8` + `38fd_3dc8`) — real formula
  ported 2026-08-19" above for the full replacement. The old "Accept/Refuse
  CHOICE gates whether the hike happens" framing in bullet 483 is wrong for
  the delta itself (DOS applies unconditionally); Accept/Refuse now means
  keep-it / revert-it after the fact.
