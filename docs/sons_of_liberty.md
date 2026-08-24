# Sons of Liberty / rebel sentiment

Reference for how colony and nation **Sons of Liberty** (SoL) membership is
computed, and every gameplay effect gated on SoL or **Tory** share.

Difficulty only supplies the Tory threshold (`10 − difficulty`); full production
mod math lives here. Building throughput without sentiment:
[building_production.md](building_production.md). Authority:
[project_goals.md](project_goals.md) (decomp → NAMES → manual → fandom).

## Sources

| Source | Role |
|--------|------|
| `FUN_15eb_0274` / `FUN_364b_0688` / `FUN_43f7_0004` in [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c) | **Authoritative** colony %, EOT accumulator, nation aggregate |
| Production mod ~11866–11888; +0x1c latches ~57415–57485 | Tory floor + SoL flag bonuses; inefficient-gov bit |
| Port [`colony_production.c`](../src/core/colony_production.c), [`ai_king.c`](../src/core/ai_king.c) | Wired stand-ins; PARK comments name gaps |
| [`Colonization.pdf`](../COLONIZE/Colonization.pdf) (~SoL / independence) | Prose: +1/+2, Tory −1 thresh, declare 50%, combat support, muster |
| [fandom_col1994.md](fandom_col1994.md) | Tier-3 — lose when decomp disagrees |
| Annotated notes | [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md), [`king_ref.md`](../original_sources_annotated/ai/king_ref.md), [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md) |

Col1 fields: colony `rebel_dividend` / `rebel_divisor`; colony flags +0x1c
(`sol_100=0x02`, `sol_50=0x04`; DOS inefficient latch `0x08`); nation
`rebel_sentiment` (+0x19); head `rebel_sentiment_report` (congress UI).

---

## Computation

### Colony SoL % (`FUN_15eb_0274`)

```
pct = (rebel_dividend * 100) / rebel_divisor   // 0 if divisor invalid
if Simon Bolivar elected and colony nation is human (control==0):
  pct = min(100, pct + 20)
```

Display / UI Tory share = `100 − sol`. People-band counts:

```
sol_count  = (pop * sol + 50) / 100
tory_count = pop - sol_count
```

**Port:** [`colony_prod_sol_percent`](../src/core/colony_production.c) reads Col1
pairs (clamp 0..100), then applies Bolivar via
[`founding_fathers_bolivar_sol_bonus`](../src/core/founding_fathers.c). If
missing rebel fields: stand-in `nation.liberty_bells_total / 4` then the same
Bolivar boost.

### EOT accumulator (`FUN_364b_0688` Phase C)

Each colony end-of-turn (DOS):

1. Shrink dividend and divisor pairs ≈ ÷64
2. `divisor += population * 2`
3. `dividend += liberty_bells` produced this turn
4. Clamp `dividend ≤ divisor`
5. Under War of Independence + crown-occupied colony: bells feed Tory
   (`bells = -(bells >> 1)` before the add)

**Port:** [`colony_prod_tick_rebel_accumulators`](../src/core/colony_production.c)
**Wired** — called from `turn_produce_one_colony` before sol flag refresh.
Jefferson/Paine FF applied to bells; WoI via `head.game_options.woi`; crown =
human peer (0↔1).

**Founding seed (fixed 2026-08-24):** DOS `FUN_364b_1ba8` (colony founding)
sets `rebel_divisor = 100`, `rebel_dividend = 0` (viceroy_unpacked.c:58052-58055)
— *not* both-zero. The math is self-similar (dividend and divisor both decay
at the same ÷64 rate off the same per-turn additive terms), so with a
both-zero start the colony SoL% jumps straight to its steady-state
`bells/(2×pop)` ratio on turn 1 instead of ramping — a size-2 colony with one
Town Hall statesman (bells≈7) hit 100% the very first turn. Port's
`col1_bridge.c` colony-export loop (which mints a fresh `ColonizeCol1Colony`
record the first time a newly founded colony appears) was leaving both
fields at the `memset` zero; it now seeds `rebel_divisor = 100` for a
first-appearance (non-preserved-by-xy) record, matching DOS.

### Nation / “overall” SoL (`FUN_43f7_0004`)

Pop-weighted average of **colony** SoL%:

```
nation_sol = Σ(pop × colony_sol%) / Σ(pop)
```

Used for declare, restless chrome, tax-refuse SoL gate, merc offer, score
rebel points. **Port:** [`ai_king_sol_percent`](../src/core/ai_king.c) —
per-colony `sol% = dividend×100/divisor` (truncated, Bolivar +20 applied and
clamped per colony, matching `FUN_281f_0c86`'s per-colony read inside the
`0004` loop) **then** pop-weighted: `Σ(pop×sol%) / Σ(pop)`; else
`liberty_bells_total/4`. (Corrected 2026-08-24: this line previously read
`Σ(dividend×pop)×100/Σ(divisor×pop)`, a different — and wrong — order of
operations that doesn't match either the decomp or the actual
`ai_king_sol_percent` body.)

### Colony flag latches (+0x1c)

| Bit | Name | Set when | Clear hysteresis (DOS / port) |
|----:|------|----------|-------------------------------|
| `0x04` | `sol_50` | SoL ≥ 50% | Cleared when SoL &lt; 50 |
| `0x02` | `sol_100` | SoL ≥ 100% | Cleared when SoL &lt; **95** |
| `0x08` | inefficient / Tory path (DOS) | Tory pressure ≥ band | DOS clears when below band |

**Port:** [`colony_prod_refresh_sol_flags`](../src/core/colony_production.c)
wires `sol_50` / `sol_100` **one-step** per EOT (DOS nest: majority before
unanimous). Bit `0x08` is **repurposed** as food-starvation in
the Linux Col1 mapping — not the DOS inefficient-gov latch. Inefficient-gov
chrome uses port-only `ColonizeColony.inefficient_gov` +
`turn_emit_inefficient_gov_chrome`. Human latch /
decade chrome: `turn_emit_sol_phase_d_chrome` (`@REBELMAJORITY` /
`@REBELUNANIMOUS` / `@TORY*` / `@SONSUP` / `@SONSDOWN`).

Do not confuse year-end “rebel colony” annotations that mention `0x1c & 0x40`
with SoL: the port maps `0x40` as **coastal**.

---

## Effects summary

| Gate | Effect | Port |
|------|--------|------|
| Colony SoL ≥ 50% / ≥ 100% | Production +1 / +2 (via sol latches in full DOS mod) | **Wired** (`colony_prod_sol_bonus`) |
| Tory count vs `10−diff` | `−⌊tories / thresh⌋` on production | **Wired** (same helper; AI thresh 10) |
| Nation SoL ≥ 50% | May declare independence | **Structural Wired** (SoL only; `FUN_43f7_2564`) |
| Nation SoL 40..49 | Restless chrome | **Wired** thin |
| Tax ≥20 and (SoL≥30 or bells≥80) | Tax refuse / boycott path | **Wired** structural |
| Nation SoL **>** 50 (WoI) | Continental merc offer | **Wired** thin |
| Colony SoL at unit tile >50 / 40..50 | Cont. promote / Soldier→Veteran | **Wired** thin |
| Bolivar elected | +20 SoL (DOS display-time) | **Wired** (`founding_fathers_bolivar_sol_bonus`) |
| Rebel sentiment points | Score +1 per point | **Wired** |
| Year-end SoL C2 / D SoL chrome | Peace / pressure / rival dialogs | **Wired** thin (pop-weighted SoL via `ai_king_sol_percent`; C2 crown/human ratio; D rival pick via `rival_nation_slot_1/_2`, `(8−difficulty)×10` auto-declare, `rebellion_pct_last_notified` dedup; decile status via `sol_pct_last_notified`; VGA PARK) |
| Combat popular-support % | Attacker side’s SoL/Tory share **on colony** | **Wired** (`combat_apply_1b0e_peels`: colony REF +50% + Tory/Rebel %) — [combat.md](combat.md) |
| Cont. Army muster by colony SoL | Declare-turn 1eca promote (Veteran fortified Soldier/Dragoon on colony tile; SoL>49) | **Wired** (`ai_king_war_act`; popup Confirm chains ref_wave+war_act) |
| Map pop digit colors | White &lt;50 / green ≥50 / blue 100 | **PARK** |
| Rebel accumulator EOT | Grow dividend/divisor from bells | **Wired** (`colony_prod_tick_rebel_accumulators`) |

---

## Production modifier

Full DOS net modifier on each production unit (field / craft / hammers / bells /
crosses) — decomp ~11866–11888:

```
sol%   = FUN_15eb_0274(...)
tories = (pop * (100 - sol%) + 50) / 100
thresh = human_colony ? (10 - difficulty) : 10
mod    = -floor(tories / thresh)
if sol_50 latch (0x04):  mod += 1
if sol_100 latch (0x02): mod += 1
```

| Piece | Meaning |
|-------|---------|
| Tory floor | Can be **−2, −3, …** when many Tories relative to thresh |
| SoL latches | Add back +1 at ≥50, +1 again at ≥100 (net +2 at full SoL with no Tory load) |
| Thresh | Discoverer **10** … Viceroy **6** — see [difficulty.md](difficulty.md) |

Manual / fandom “−1 when Tory count ≥ threshold” is a **simplification**. Prefer
the floor formula.

**Port today:** [`colony_prod_sol_bonus`](../src/core/colony_production.c)
implements the full net mod (Tory floor + sol latch / live SoL stand-in);
applied in EOT field/craft/hammers/bells/crosses and preview.

---

## Independence and king events

| Event | Gate | Port symbols |
|-------|------|--------------|
| Declare | Nation SoL ≥ **50%** (manual / `FUN_43f7_2564`) | `AI_KING_DECLARE_SOL_MIN` — two-stage-popup lead resolved 2026-08-24 (see `ai_king_try_declare` comment): `0x1386`=`@TOOTORY`, now reachable via the menu path below (not from the per-turn auto-check); the bit-`0x80`-gated first popup is a hotseat/multi-human-player disambiguation step (set only in the new-game nation-select screen, `FUN_75c2_10ae`), no reachable equivalent under the port's single-`human_nation` model — the auto-check's single @DECLARE Never/Yes is the complete per-turn behavior, not a gap |
| Declare (menu) | Reachable any time via MENU.TXT `@GAME` "DECLARE INDEPENDENCE" (`FUN_43f7_2564`, same as auto) | **Added 2026-08-24**: `ai_king_menu_declare_independence` / `MAP_MENU_ACTION_DECLARE_INDEPENDENCE` — below `AI_KING_DECLARE_SOL_MIN` shows `@TOOTORY`; at/above shows the same `@DECLARE` confirm; already-WoI is a status-only no-op. Lets a "Not yet" answer be revisited on demand instead of only via next turn's auto-repeat. See `original_sources_annotated/ai/king_ref.md` |
| Restless chrome | SoL **40..49** | `AI_KING_RESTLESS_SOL_MIN` |
| Tax refuse / boycott | tax ≥ **20** and (SoL ≥ **30** or bells ≥ **80**) | `AI_KING_BOYCOTT_*` |
| Continental merc | WoI and nation SoL **>** **50** | `AI_KING_MERC_SOL_MIN` |

Post-declare, liberty bells shift from FF election toward foreign intervention
(see [king_ref.md](../original_sources_annotated/ai/king_ref.md)); deep intervene
economy remains thin/PARK.

---

## Continental promote (`FUN_43f7_1eca`)

Uses **colony** SoL at the unit’s tile ([`ai_king_colony_sol_at`](../src/core/ai_king.c)):

| Colony SoL | Effect |
|------------|--------|
| **> 49** | Colony-tile, Veteran-profession (`unit+0x315b==0x15`) Soldier → Continental Army; Dragoon → Continental Cavalry. Cap `max(1, min(pop/2, pop*(sol-50)/50))` per colony. Regular and already-Continental units never match the raw-type test and are left untouched; a colony-tile Soldier/Dragoon *without* Veteran profession is also skipped. **No fortify requirement** — re-verified 2026-08-24 by reading the full decomp body end to end: it tests only `unit+0x3146` (type) and `unit+0x315b` (profession), never `unit+0x08` (`orders`). A prior fix (2026-08-14) had added a `UNITS_ORDER_FORTIFIED` gate here on top of the real profession gate; that extra gate was unsupported and has been removed. |
| **40..49** | Restless status text only (`AI_KING_RESTLESS_SOL_MIN`..`AI_KING_DECLARE_SOL_MIN-1`) — **not** a promote band in `1eca` itself |
| **&lt; 40** | No effect |

**Done** — full profession/type-id promote table ported (was previously filed
as deep-PARK; that filing was stale against the code, corrected 2026-08-24).
Manual “Continental Army Muster” (counts by colony SoL on declare; &lt;50%
colony contributes 0) via `ai_king_war_act` / `FUN_43f7_1eca` (Veteran
fortified Soldier/Dragoon on colony tile; cap from pop and SoL). Popup
declare Confirm chains ref_wave+war_act same turn.

---

## Simon Bolivar

| Source | Mechanism |
|--------|-----------|
| **DOS** (`FUN_15eb_0274`) | While Bolivar is elected and colony owner is human (`control==0`): every SoL **read** adds **+20** then caps at 100 |
| **Port** | Same: [`founding_fathers_bolivar_sol_bonus`](../src/core/founding_fathers.c) on colony / nation / combat SoL reads. Elect records FF only — no `rebel_dividend` mutation |

Gate is human control (decomp), not WoI / independence.

---

## Score

Manual: +1 Colonization Score per point of rebel sentiment (empire-wide).
Port: [`reports_rebel_sentiment_pct`](../src/core/reports.c) /
[`reports_compute_score`](../src/core/reports.c) — pop-weighted colony
dividend/divisor. End-game difficulty-scaled gold rebate after score
(`FUN_41f2_0b70`) is separate and **PARK** — see [difficulty.md](difficulty.md).

---

## Year-end chrome

| Section | SoL use | Port |
|---------|---------|------|
| C2 (WoI) | Crown vs human SoL ratio; pressure if >79, peace if >89 | **Wired** thin (`ai_king_sol_percent`) |
| D (peace) | Rival SoL vs threshold `(8−difficulty)×10` | **Wired** thin — `rebel_sentiment` when non-zero; else colony SoL stand-in; DOS continent table `−0x6bf0` **PARK** |

Details: [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

---

## PARK / not wired

| Effect | Source |
|--------|--------|
| DOS +0x1c bit `0x08` inefficient latch | decomp ~57468 — port chrome via `inefficient_gov` (bit3 stays starvation) |
| Combat popular-support attack % = side’s SoL/Tory share | Manual — **Done** in `combat_apply_1b0e_peels` (colony Tory/Rebel) — [combat.md](combat.md) |
| Continental Army muster by colony SoL | Manual — **Done** in `ai_king_war_act` (1eca) |
| Map population digit colors (white/green/blue) | Manual |
| Decade SoL chrome messages | `colony_eot_production.md` — **Done** thin (`@SONSUP`/`@SONSDOWN`) |

---

## Conflicts resolved

| Topic | Rejected / weaker | Authoritative |
|-------|-------------------|---------------|
| Tory penalty size | Manual/fandom “−1 if ≥ thresh” | `−⌊tories / (10−diff)⌋` then +sol latches |
| Tory thresh 10…6 | — | Decomp + manual; also [difficulty.md](difficulty.md) |
| Bolivar | Port dividend bump alone as “the” rule | DOS/port: +20 on every SoL compute while FF held (human) |
| Declare gate | Manual/decomp: SoL≥50 only |
| +0x1c `0x08` | Port food starvation bit | DOS inefficient-gov latch |
| `0x1c & 0x40` as “rebel” | Some year-end annotations | Port: coastal bit |

---

## Implementation map

| Concern | Module |
|---------|--------|
| Colony % / bonus / latches | [`colony_production.c`](../src/core/colony_production.c) |
| Craft / preview SoL add | [`colony_craft.c`](../src/core/colony_craft.c), [`colony_preview.c`](../src/core/colony_preview.c) |
| EOT apply | [`turn.c`](../src/core/turn.c) |
| People SoL/Tory meters | [`colony_screen.c`](../src/core/colony_screen.c) |
| Nation SoL, declare, restless, boycott, merc, promote | [`ai_king.c`](../src/core/ai_king.c) |
| Bolivar elect / display +20 | [`founding_fathers.c`](../src/core/founding_fathers.c) `founding_fathers_bolivar_sol_bonus` |
| Score rebel points | [`reports.c`](../src/core/reports.c) |
| Year-end chrome | [`turn.c`](../src/core/turn.c) `turn_run_year_end_chrome` |
| Col1 field layout | [`col1_save.h`](../src/core/col1_save.h), [save_format_map.md](save_format_map.md) |
