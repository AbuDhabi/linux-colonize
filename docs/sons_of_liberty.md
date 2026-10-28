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
| Port [`colony_production.c`](../src/core/colony_production.c), [`ai_king.c`](../src/core/ai_king.c) | Ported (FUN_* cited at each site) |
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
[`founding_fathers_bolivar_sol_bonus`](../src/core/founding_fathers.c).

**No fallback.** `FUN_15eb_0274` has none: no pair means nothing has
accumulated, which is 0. (A `nation.liberty_bells_pool / 4` stand-in once made
a colony founded this turn read 100% until its first end of turn.)

**`FUN_364b_0688` Phase C, non-WoI arm (raw 57349-57355).** After the colony's
SoL% is read (`local_8e = FUN_281f_0c86()`), the colony's bells figure takes a
Tory penalty *unless* this is the rebel's own colony during the WoI:

```
if (!(0x5382 & 1) || colony.nation_id != DS:0x53d2) {   /* not the rebel's own colony */
    if (bells < colony.pop)  bells += sol% / -0x14;     /* i.e. bells -= sol% / 20 */
} else {
    bells = -(bells >> 1);                              /* WoI, rebel's own colony */
}
```

The guard is `bells < pop` — the penalty applies only while the colony's bell
output is below its population. `sol% / -0x14` is C truncation toward zero on
the *negative* divisor, so it subtracts `floor(sol%/20)` for the usual
non-negative SoL.

**Nation bell pool.** COL1 nation `+0xc` (`liberty_bells_pool`) is the live
Founding-Father bell pool, not a lifetime total: `FUN_4345_0a22` adds each
colony's bells to it (raw 73341) and zeroes it on a successful elect (raw
73370); `FUN_43f7_1a26` zeroes it at the declaration of independence (raw
74738). `+0xe` (`liberty_bells_last_turn`) is this turn's bells only. See
[`founding_fathers.md`](founding_fathers.md) and port_bugs.md #933.

### Instant population adjustment (colony screen join / leave)

DOS colony-screen colonist add (viceroy_unpacked.c:11301, the `FUN_15eb_1068`
caller) does `rebel_divisor += 100` and colonist remove (:10279)
`rebel_divisor -= 100`, so the displayed SoL% moves the moment population
changes rather than one EOT later. Founding seeds 100 (below), so an
n-colonist colony starts at `divisor ≈ 100·n` before the accumulator takes
over. **Port:** `colonies_col1_rebel_divisor_adjust` in
[`colony.c`](../src/core/colony.c), called from `colonies_admit_unit_w` (+100,
also the founder's seat in `colonies_found`) and `colonies_eject_colonist`
(−100) against the bound col1 context (`colonies_set_col1_context`); dividend
clamped ≤ divisor, divisor floored at 0. Starvation (`FUN_15eb_0d04`,
turn_production.c) and a colonist lost in a capture (units_capture.c) also take
100 off; births do not (the newborn is a map unit).

### EOT accumulator (`FUN_364b_0688` Phase C)

Each colony end-of-turn (DOS):

1. Shrink dividend and divisor pairs ≈ ÷64
2. `divisor += population * 2`
3. `dividend += liberty_bells` produced this turn — the **same word** the nation/congress tally gets, SoL bonus and all (`local_ba = FUN_281f_0b50(0x12,0)` at viceroy_unpacked.c:57230 is read once and passed both to `FUN_291f_09f8` and to the dividend add). Not a sol-free recompute: the port did that until smell #89, giving two different "bells this turn" for one colony. The feedback is one turn lagged, since the bells are composed from the *pre-tick* dividend/divisor ratio.
4. Clamp `dividend ≤ divisor`
5. Under War of Independence + crown-occupied colony: bells feed Tory
   (`bells = -(bells >> 1)` before the add)

**Port:** [`colony_prod_tick_rebel_accumulators`](../src/core/colony_production.c)
**Wired** — called from `turn_produce_one_colony` before sol flag refresh.
Jefferson/Paine FF **and the colony's own `colony_prod_sol_bonus`** applied to bells (step 3 above); WoI via `head.game_options.woi`; crown =
human peer (0↔1).

**Founding seed:** DOS `FUN_364b_1ba8` (colony founding) sets
`rebel_divisor = 100`, `rebel_dividend = 0` (viceroy_unpacked.c:58052-58055)
— *not* both-zero. The math is self-similar (dividend and divisor both decay
at the same ÷64 rate off the same per-turn additive terms), so a both-zero
start jumps straight to the steady-state `bells/(2×pop)` ratio on turn 1
instead of ramping. Port: `col1_bridge.c` seeds `rebel_divisor = 100` on a
first-appearance (non-preserved-by-xy) colony record.

### Nation / “overall” SoL (`FUN_43f7_0004`)

Pop-weighted average of **colony** SoL%:

```
nation_sol = Σ(pop × colony_sol%) / Σ(pop)
```

Used for declare, restless chrome, tax-refuse SoL gate, merc offer, score
rebel points. **Port:** [`ai_king_sol_percent`](../src/core/ai_king.c) —
per-colony `sol% = dividend×100/divisor` (truncated, Bolivar +20 applied and
clamped per colony, matching `FUN_281f_0c86`'s per-colony read inside the
`0004` loop) **then** pop-weighted: `Σ(pop×sol%) / Σ(pop)`; with **no
fallback** — no colonies (or a zero pop sum) returns the untouched
accumulator, i.e. 0 (`FUN_43f7_0004`, viceroy_unpacked.c raw 73477-73514;
port_bugs.md #424). Not `Σ(dividend×pop)×100/Σ(divisor×pop)`: the per-colony
percent comes first.

### Colony flag latches (+0x1c)

| Bit | Name | Set when | Clear hysteresis (DOS / port) |
|----:|------|----------|-------------------------------|
| `0x04` | `sol_50` | SoL ≥ 50% | Cleared when SoL &lt; 50 |
| `0x02` | `sol_100` | SoL ≥ 100% | Cleared when SoL &lt; **95** |
| `0x08` | inefficient government | Tory head count ≥ `10 − difficulty` | Cleared when it drops back under |

**Port:** [`colony_prod_refresh_sol_flags`](../src/core/colony_production.c)
wires `sol_50` / `sol_100` **one-step** per EOT (DOS nest: majority before
unanimous). Bit `0x08` is the DOS inefficient-gov latch
(`COLONIZE_COLONY_FLAG_INEFFICIENT_GOV`, see
[colony.md](colony.md#colonize_colony_flag_inefficient_gov)); its chrome is
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
| Tory count vs `10−diff` | `−⌊tories / thresh⌋` on production | **Wired** (same helper; AI Tory count zeroed, FUN_15eb_1d4c 15eb:1dbf) |
| Nation SoL ≥ 50% | May declare independence | **Structural Wired** (SoL only; `FUN_43f7_2564`) |
| Nation SoL 40..49 | Restless chrome | **Wired** thin |
| — (no SoL gate) | Mercenary offer `FUN_43f7_2022`: 1-in-3 per-turn roll | **Wired** (`ai_king_offers.c`) |
| Colony SoL at unit tile >50 / 40..50 | Cont. promote / Soldier→Veteran | **Wired** thin |
| Bolivar elected | +20 SoL (DOS display-time) | **Wired** (`founding_fathers_bolivar_sol_bonus`) |
| Rebel sentiment points | Score +1 per point | **Wired** |
| Year-end SoL C2 / D SoL chrome | Peace / pressure / rival dialogs | **Wired** thin (pop-weighted SoL via `ai_king_sol_percent`; C2 crown/human ratio; D rival pick via `rival_nation_slot_1/_2`, `(8−difficulty)×10` auto-declare, `rebellion_pct_last_notified` dedup; decile status via `sol_pct_last_notified`) |
| Combat popular-support % | Attacker side’s SoL/Tory share **on colony** | **Wired** (`combat_apply_1b0e_peels`: colony REF +50% + Tory/Rebel %) — [combat.md](combat.md) |
| Cont. Army muster by colony SoL | Declare-turn 1eca promote (Veteran Soldier/Dragoon on colony tile; SoL>49) | **Wired** (`ai_king_war_act`; popup Confirm chains ref_wave+war_act) |
| Map pop digit colors | White &lt;50 / green ≥50 / cyan 100 (+0x1c SoL latches) | **Wired** (`colony_chrome.c`, DOS `FUN_112b_0c64` badge) |
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
| Declare | Nation SoL ≥ **50%** (manual / `FUN_43f7_2564`) | `AI_KING_DECLARE_SOL_MIN`. DOS has no per-turn auto-check: declaring is menu-only (row below). The bit-`0x80`-gated first popup is a hotseat disambiguation step (set only by the new-game nation select, `FUN_75c2_10ae`) with no equivalent under the port's single `human_nation`, so the single @DECLARE Never/Yes is the complete behaviour (`ai_king_show_declare_choice` comment) |
| Declare (menu) | MENU.TXT `@GAME` "DECLARE INDEPENDENCE" (`FUN_43f7_2564`) | `ai_king_menu_declare_independence` / `MAP_MENU_ACTION_DECLARE_INDEPENDENCE` — below `AI_KING_DECLARE_SOL_MIN` shows `@TOOTORY` (`0x1386`); at/above the `@DECLARE` confirm; already-WoI is a status-only no-op. See `original_sources_annotated/ai/king_ref.md` |
| Restless chrome | SoL **40..49** | `AI_KING_RESTLESS_SOL_MIN` |
| Mercenary offer (`FUN_43f7_2022`) | No SoL gate: 1-in-3 per-turn roll while REF is not present or the Artillery backup pool is empty | `AI_KING_MERC_ROLL_CHANCE` |

Post-declare, liberty bells shift from FF election toward foreign intervention
(see [king_ref.md](../original_sources_annotated/ai/king_ref.md)); the
intervention pool seed, bell trigger and landings are ported
([difficulty.md](difficulty.md) "Foreign intervention").

---

## Continental promote (`FUN_43f7_1eca`)

Uses **colony** SoL at the unit’s tile ([`ai_king_colony_sol_at`](../src/core/ai_king.c)):

| Colony SoL | Effect |
|------------|--------|
| **> 49** | Colony-tile, Veteran-profession (`unit+0x315b==0x15`) Soldier → Continental Army; Dragoon → Continental Cavalry. Cap `max(1, min(pop/2, pop*(sol-50)/50))` per colony. Regular and already-Continental units never match the raw-type test and are left untouched; a colony-tile Soldier/Dragoon *without* Veteran profession is also skipped. **No fortify requirement**: the body tests only `unit+0x3146` (type) and `unit+0x315b` (profession), never `unit+0x08` (`orders`). |
| **40..49** | Restless status text only (`AI_KING_RESTLESS_SOL_MIN`..`AI_KING_DECLARE_SOL_MIN-1`) — **not** a promote band in `1eca` itself |
| **&lt; 40** | No effect |

This is the manual's “Continental Army Muster” (&lt;50% colony contributes 0),
run by `ai_king_war_act`; the declare Confirm popup chains ref_wave+war_act the
same turn.

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
Port: [`reports_compute_score`](../src/core/reports_score.c) — reads
`head.rebel_sentiment_report` (DS:0x53d0) directly (exact against
`dutch-reports.SAV`/score.png: 94); a pop-weighted recompute from each
colony's `rebel_dividend`/`rebel_divisor` gives 91 there — see
[reports_score.md](reports_score.md) F10 Rebel Sentiment. The difficulty-scaled
Colonization Rating after the score (`FUN_41f2_0b70`, not a gold rebate) is
ported as `reports_score_rating` — see [difficulty.md](difficulty.md).

---

## Year-end chrome

| Section | SoL use | Port |
|---------|---------|------|
| C2 (WoI) | Crown vs human SoL ratio; pressure if >79, peace if >89 | **Wired** thin (`ai_king_sol_percent`) |
| D (peace) | Rival SoL vs threshold `(8−difficulty)×10` | **Wired** — `rebel_sentiment × census_pop_proxy / 100` capped 100 (`turn_year_end_rival_rebels`; DS `−0x6bf0` = `census_pop_proxy`, not a continent table) |

Details: [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

---

## Conflicts resolved

| Topic | Rejected / weaker | Authoritative |
|-------|-------------------|---------------|
| Tory penalty size | Manual/fandom “−1 if ≥ thresh” | `−⌊tories / (10−diff)⌋` then +sol latches |
| Tory thresh 10…6 | — | Decomp + manual; also [difficulty.md](difficulty.md) |
| Bolivar | Port dividend bump alone as “the” rule | DOS/port: +20 on every SoL compute while FF held (human) |
| Declare gate | Manual/decomp: SoL≥50 only |
| +0x1c `0x08` | Port food starvation bit | DOS inefficient-gov latch — port agrees since 2026-09-04 (`COLONIZE_COLONY_FLAG_INEFFICIENT_GOV`; food latch moved to `food_shortfall_latch`) |
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
