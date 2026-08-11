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
if Simon Bolivar elected and human pre-independence:
  pct = min(100, pct + 20)
```

Display / UI Tory share = `100 − sol`. People-band counts:

```
sol_count  = (pop * sol + 50) / 100
tory_count = pop - sol_count
```

**Port:** [`colony_prod_sol_percent`](../src/core/colony_production.c) reads Col1
pairs (clamp 0..100). If missing: stand-in `nation.liberty_bells_total / 4`
(not the DOS colony formula). Bolivar is **not** applied at display time (see
[Bolivar](#simon-bolivar) below).

### EOT accumulator (`FUN_364b_0688` Phase C)

Each colony end-of-turn (DOS):

1. Shrink dividend and divisor pairs ≈ ÷64
2. `divisor += population * 2`
3. `dividend += liberty_bells` produced this turn
4. Clamp `dividend ≤ divisor`
5. Under War of Independence + crown-occupied colony: bells feed Tory
   (`bells = -(bells >> 1)` before the add)

**Port:** accumulator tick **PARK** — SoL only moves when Col1 fields already
have values (loaded save) or Bolivar bumps dividend on elect.

### Nation / “overall” SoL (`FUN_43f7_0004`)

Pop-weighted average of **colony** SoL%:

```
nation_sol = Σ(pop × colony_sol%) / Σ(pop)
```

Used for declare, restless chrome, tax-refuse SoL gate, merc offer, score
rebel points. **Port:** [`ai_king_sol_percent`](../src/core/ai_king.c) —
`Σ(dividend×pop) × 100 / Σ(divisor×pop)`; else `liberty_bells_total/4`.

### Colony flag latches (+0x1c)

| Bit | Name | Set when | Clear hysteresis (DOS / port) |
|----:|------|----------|-------------------------------|
| `0x04` | `sol_50` | SoL ≥ 50% | Cleared when SoL &lt; 50 |
| `0x02` | `sol_100` | SoL ≥ 100% | Cleared when SoL &lt; **95** |
| `0x08` | inefficient / Tory path (DOS) | Tory pressure ≥ band | DOS clears when below band |

**Port:** [`colony_prod_refresh_sol_flags`](../src/core/colony_production.c)
wires `sol_50` / `sol_100`. Bit `0x08` is **repurposed** as food-starvation in
the Linux Col1 mapping — not the DOS inefficient-gov latch.

Do not confuse year-end “rebel colony” annotations that mention `0x1c & 0x40`
with SoL: the port maps `0x40` as **coastal**.

---

## Effects summary

| Gate | Effect | Port |
|------|--------|------|
| Colony SoL ≥ 50% / ≥ 100% | Production +1 / +2 (via sol latches in full DOS mod) | **Wired** as `colony_prod_sol_bonus` only |
| Tory count vs `10−diff` | `−⌊tories / thresh⌋` on production | **PARK** |
| Nation SoL ≥ 50% | May declare independence | **Structural Wired** (+ port bells≥100) |
| Nation SoL 40..49 | Restless chrome | **Wired** thin |
| Tax ≥20 and (SoL≥30 or bells≥80) | Tax refuse / boycott path | **Wired** structural |
| Nation SoL **>** 50 (WoI) | Continental merc offer | **Wired** thin |
| Colony SoL at unit tile >50 / 40..50 | Cont. promote / Soldier→Veteran | **Wired** thin |
| Bolivar elected | +20 SoL (DOS display-time) | **Wired** one-shot dividend bump |
| Rebel sentiment points | Score +1 per point | **Wired** |
| Year-end C2 / D SoL chrome | Peace / pressure / rival dialogs | **Thin / PARK** |
| Combat popular-support % | Attacker side’s SoL/Tory share | **Wired** thin (`combat_apply_1b0e_peels` WoI SoL + REF +50%) — [combat.md](combat.md) |
| Cont. Army muster by colony SoL | Declare-turn muster; &lt;50% = 0 | **PARK** |
| Map pop digit colors | White &lt;50 / green ≥50 / blue 100 | **PARK** |
| Rebel accumulator EOT | Grow dividend/divisor from bells | **PARK** |

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

**Port today:** [`colony_prod_sol_bonus`](../src/core/colony_production.c) returns
+1 if SoL≥50, +2 if SoL≥100; applied in EOT field/craft/hammers/bells/crosses
and preview. Tory floor **PARK**.

---

## Independence and king events

| Event | Gate | Port symbols |
|-------|------|--------------|
| Declare | Nation SoL ≥ **50%** (manual / `FUN_43f7_2564`) | `AI_KING_DECLARE_SOL_MIN`; also requires `liberty_bells_total ≥ 100` (**port extra gate**, not in manual) |
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
| **> 50** | Soldier/Regular → Continental Army; Dragoon/Cavalry → Continental Cavalry |
| **40..50** | Soldier → Veteran only; Dragoon unchanged |
| **&lt; 40** | No promote from this band |

Deep profession/type-id promote table still **PARK**. Manual “Continental Army
Muster” (counts by colony SoL on declare; &lt;50% colony contributes 0) is **PARK**.

---

## Simon Bolivar

| Source | Mechanism |
|--------|-----------|
| **DOS** (`FUN_15eb_0274`) | While Bolivar is elected and human pre-independence: every SoL **read** adds **+20** then caps at 100 |
| **Port** (`effect_bolivar_rebel`) | On elect: one-shot `rebel_dividend += 20% of divisor` (clamp ≤ divisor) on owned colonies |

Same player-visible intent (+20% membership); **different mechanism** — port does
not re-apply +20 on every later read.

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
| C2 (WoI) | Crown vs human SoL ratio; pressure if &gt;79, peace if &gt;89 | Thin (bells proxy) |
| D (peace) | Rival SoL vs threshold `(8−difficulty)×10` | Thin / PARK — fixed heuristics, not full decomp |

Details: [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

---

## PARK / not wired

| Effect | Source |
|--------|--------|
| Rebel dividend/divisor EOT tick | `FUN_364b_0688` Phase C |
| Tory floor `−⌊tories/thresh⌋` | decomp ~11880 |
| DOS +0x1c bit `0x08` inefficient latch | decomp ~57468 |
| Combat popular-support attack % = side’s SoL/Tory share | Manual — **thin Wired** in `combat_apply_1b0e_peels` — [combat.md](combat.md) |
| Continental Army muster by colony SoL | Manual |
| Map population digit colors (white/green/blue) | Manual |
| Decade SoL chrome messages | `colony_eot_production.md` |
| Bolivar display-time +20 every read | `FUN_15eb_0274` |

---

## Conflicts resolved

| Topic | Rejected / weaker | Authoritative |
|-------|-------------------|---------------|
| Tory penalty size | Manual/fandom “−1 if ≥ thresh” | `−⌊tories / (10−diff)⌋` then +sol latches |
| Tory thresh 10…6 | — | Decomp + manual; also [difficulty.md](difficulty.md) |
| Bolivar | Port dividend bump alone as “the” rule | DOS: +20 on every SoL compute while FF held |
| Declare gate | Port bells≥100 as original | Manual/decomp: SoL≥50 only — bells gate is port-extra |
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
| Bolivar elect bump | [`founding_fathers.c`](../src/core/founding_fathers.c) |
| Score rebel points | [`reports.c`](../src/core/reports.c) |
| Year-end chrome | [`turn.c`](../src/core/turn.c) `turn_run_year_end_chrome` |
| Col1 field layout | [`col1_save.h`](../src/core/col1_save.h), [save_format_map.md](save_format_map.md) |
