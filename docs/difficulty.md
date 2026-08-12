# Difficulty level effects

Reference for what the new-game difficulty byte (`0` Discoverer … `4` Viceroy) changes
in original Colonization rules, and how the Linux port tracks that today.

Sentiment production math beyond the Tory **threshold** is owned by
[sons_of_liberty.md](sons_of_liberty.md) (building throughput without sentiment:
[building_production.md](building_production.md)). Authority order:
[project_goals.md](project_goals.md) (decomp / NAMES → manual → fandom).

## Sources

| Source | Role |
|--------|------|
| Decomp DS `0x53a6` (`VICEROY_DS_DIFFICULTY`) in [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c) | **Authoritative** formulas (starting gold, Tory band, tax, food, SoL pressure, …) |
| Port code that cites those FUN_* paths under [`src/core/`](../src/core/) | Wired stand-ins; PARK comments name intended gaps |
| [`COLONIZE/NAMES.TXT`](../COLONIZE/NAMES.TXT) `@DIFFICULTY` | Level **names** only (no multipliers) |
| [`COLONIZE/Colonization.pdf`](../COLONIZE/Colonization.pdf) | Prose for levels; Tory −1 thresholds; score village penalty; score×difficulty factor |
| [fandom_col1994.md](fandom_col1994.md) | Tier-3 cross-check — lose when decomp disagrees |

Save field: `ColonizeCol1Head.difficulty` (`uint8_t`, clamp 0..4). Runtime:
`game_difficulty()` / `ColonizeGameState.difficulty`.

## Levels

| Value | Name (`NAMES.TXT`) | UI gloss (`new_game.c`) |
|------:|--------------------|-------------------------|
| 0 | Discoverer | Easiest |
| 1 | Explorer | Easy |
| 2 | Conquistador | Moderate |
| 3 | Governor | Tough |
| 4 | Viceroy | Toughest |

---

## Effects summary

| System | Effect | Port |
|--------|--------|------|
| Human starting gold | Discoverer **1000**, Explorer **300**, Conquistador+ **0** | **Wrong** — hardcodes 1000 |
| Starter skills | Easy → Veteran Soldier; French Hardy always; Spanish Veteran always | Wired |
| FF liberty-bell thresholds | Harder → higher human need, lower AI need; WoI `diff*1500+2000` | Wired |
| King tax cadence | First year `1536-diff`; interval `22-2*diff` | Wired |
| REF seed / waves | Larger pools; 2nd MoW + 3rd intervene landing at `diff≥2` | Wired |
| Euro AI gold / hire | Harder → less free gold, higher hire gate | Wired |
| AI immigration pressure | `((8-diff)*score)>>3` | Wired (AI only) |
| Indian alarm prelude | Harder → escalate more often / larger bumps | Wired |
| Raid kind demote | Discoverer only: harsh → STORES/NOTHING | Wired |
| Indian land purchase | Harder → costlier for human, cheaper for AI | Wired |
| Conquest treasure | Difficulty bands 0..3 (Gov+Vic share band 3) | Wired |
| Score villages burned | `-(diff+1)*burned` | Wired |
| Tory / inefficient gov | Thresh `10-diff`; full mod `−⌊tories/thresh⌋` + sol latches | **Wired** — see [sons_of_liberty.md](sons_of_liberty.md) |
| AI colony food | `+= difficulty>>1` | **Wired** |
| Rival SoL pressure | Threshold `(8-diff)*10` | thin / PARK |
| End-game score→gold rebate | `FUN_41f2_0b70` difficulty multiplier | **PARK** |
| Combat strength (human Euro) | `str -= (diff-4)`; Discoverer −25% vs AI Euro | Wired — [combat.md](combat.md) |
| SoL declare gate (50%) | — | Unaffected |
| Victory / defeat calendar | Chrome names difficulty only | Chrome |

---

## Human starting gold

**Original** (`FUN_38fd_6024`, `viceroy_unpacked.c` ~68666–68677): for each Euro
nation Europe block (`0x84fc` → `nation*0x13c`), clear `nation.gold` (`+0x2a` /
`+0x2c`), then if that nation is human (`control==0`):

| Diff | Gold |
|-----:|-----:|
| 0 Discoverer | 1000 |
| 1 Explorer | 300 |
| 2–4 | 0 |

AI nations stay at **0**.

**Port today:** [`ai.c`](../src/core/ai.c) / [`europe.c`](../src/core/europe.c) always
set human gold to **1000** regardless of difficulty (divergence).

---

## Starter skills

[`units_starter_skills`](../src/core/units.c):

```
easy = difficulty <= 1
hardy  = (nation == French)          // always, all difficulties
veteran = easy || (nation == Spanish)
```

| Nation | Discoverer / Explorer | Conquistador+ |
|--------|-----------------------|---------------|
| English / Dutch | Veteran Soldier; plain Pioneer | Plain Pioneer + Soldier |
| French | Hardy Pioneer + Veteran Soldier | Hardy Pioneer; plain Soldier |
| Spanish | Veteran Soldier; plain Pioneer | Veteran Soldier; plain Pioneer |

Hardy Pioneer is **French-only** on every difficulty (matches DOS `COLONY00` /
[savegame.md](savegame.md); not “both experts on easy”).

---

## Founding Fathers (liberty bells)

[`founding_fathers_bells_needed`](../src/core/founding_fathers.c) (`FUN_4345_0982`):

```
human: base = (diff+3)*2 * 8
AI:    base = (14-diff) * 8
+50% per century after 1599 / 1649 / 1699 / 1749
need = (elected+1)*base + 1;  if elected==0: need >>= 1
WoI:   need = diff*1500 + 2000
```

| Diff | Human 1st FF | AI 1st FF | WoI need |
|-----:|-------------:|----------:|---------:|
| 0 | 24 | 56 | 2000 |
| 1 | 32 | 52 | 3500 |
| 2 | 40 | 48 | 5000 |
| 3 | 48 | 44 | 6500 |
| 4 | 56 | 40 | 8000 |

---

## King tax

[`ai_king_tax_event`](../src/core/ai_king.c):

```
first_year = 1536 - diff
interval   = 22 - diff*2   (min floor not hit in 0..4)
```

| Diff | First year | Interval |
|-----:|-----------:|---------:|
| 0 Discoverer | 1536 | 22 |
| 1 Explorer | 1535 | 20 |
| 2 Conquistador | 1534 | 18 |
| 3 Governor | 1533 | 16 |
| 4 Viceroy | **1532** | 14 |

Fandom “Viceroy 1534” is wrong; decomp/port use `1536-diff`.

---

## REF seed and intervention

On declare ([`ai_king_do_declare`](../src/core/ai_king.c)):

| Pool | Formula | Diff 0→4 |
|------|---------|----------|
| Regulars | `8+diff*4` | 8, 12, 16, 20, 24 |
| Dragoons | `4+diff*2` | 4, 6, 8, 10, 12 |
| Men-O-War | `2+diff` | 2, 3, 4, 5, 6 |
| Artillery | `2+diff` | 2, 3, 4, 5, 6 |
| backup Regulars | `2+diff` | 2..6 |
| backup Dragoons | `1+(diff>0)` | 1, 2, 2, 2, 2 |
| backup MoW | `diff>1 ? 1 : 0` | 0, 0, 1, 1, 1 |
| backup Artillery | `1` | always 1 |

Also when `diff ≥ 2`: second Man-O-War wave same beat; foreign intervention up to
**3** landings (else 2). See [ai_transcription.md](ai_transcription.md) /
[`king_ref.md`](../original_sources_annotated/ai/king_ref.md).

---

## Euro AI planning

[`ai_euro_nation_planning`](../src/core/ai_euro.c):

```
gold_bump = 10 + (4-diff)*5 + urgency   → 30, 25, 20, 15, 10 (+urgency)
hire_cost = 200 + diff*25               → 200, 225, 250, 275, 300
```

Higher difficulty → less free gold per tick, higher Europe-hire treasury gate.

---

## Immigration (AI)

[`europe_tick_immigration_pressure`](../src/core/europe.c) for non-human:

```
score = ((8-diff)*score) >> 3
pressure += 2   /* DOS 584a *param_2 default; treasure −2 PARKED */
```

| Diff | Scale |
|-----:|------:|
| 0 | 100% |
| 1 | 87.5% |
| 2 | 75% |
| 3 | 62.5% |
| 4 | 50% |

Human English also `*2/3` (nation 0) independent of difficulty. Human with no
colony population does not accrue pressure (port: crosses/churches own pre-colony
immigration; idle +2 crosses/turn was removed).

---

## Indians

### Alarm prelude

[`ai_contact_indian_prelude`](../src/core/ai_contact.c): roll `1..8 ≤ chance`, then bump.

| Diff | Chance (`2+(4-diff)`) | Alarm bump (`5+(4-diff)`) |
|-----:|----------------------:|--------------------------:|
| 0 | 6 | 9 |
| 1 | 5 | 8 |
| 2 | 4 | 7 |
| 3 | 3 | 6 |
| 4 | 2 | 5 |

Harder → escalate **more often** with **larger** bumps (Pocahontas / French may half).

### Raid demote

[`ai_contact_raid_kind_demote`](../src/core/ai_contact.c): only `difficulty <= 0`
(Discoverer) demotes SCALP / WREAK / GOLD → STORES or NOTHING.

### Land purchase

[`colonies_indian_land_purchase_gold`](../src/core/colony.c):

```
human: score = (diff+3)*2 + tech + bought - dist;  cost = (0x41 * score)/2  (+capital +50%)
AI:    score = tech + bought - diff - dist + 12;   cost = (0x32 * score)/2  (+capital +50%)
```

Harder → more expensive for the human, cheaper for AI.

### Conquest treasure

[`units_cortes_conquest_treasure_gold`](../src/core/units.c): difficulty clamped to
bands **0..3** (Governor and Viceroy share band 3). Per-band RNG ranges /
multipliers; final gold = `amount * 100`. Cortes / Spanish / rich-capital modifiers
layer on top (see function body).

---

## Score

### Villages burned

[`reports_compute_score`](../src/core/reports.c) / manual:

```
villages_penalty = -(difficulty + 1) * villages_burned
```

→ −1× … −5× burned villages.

### Final colonization score × difficulty

Manual: colonization score modified by a difficulty factor from the chosen level.
Decomp `FUN_41f2_0b70` applies a difficulty-scaled gold rebate / treasure dialog
after the score snapshot — **PARK** in the port (`reports_compute_score` has no
global score×difficulty multiplier).

---

## Production: Tory / inefficient government

Manual + decomp (`viceroy_unpacked.c` ~57468): Tory pressure threshold

```
threshold = 10 - difficulty   // Discoverer 10 … Viceroy 6
```

Production penalty is **not** a flat −1 at the threshold. DOS uses
`−⌊tory_count / threshold⌋`, then adds +1/+2 from SoL latches — full formula and
port status in [sons_of_liberty.md](sons_of_liberty.md). DOS also latches colony
flag bit `0x08` on the inefficient path (port repurposes `0x08` for starvation).
Port: full net mod in `colony_prod_sol_bonus` (Tory floor + sol latches / live
SoL stand-in). Inefficient-government EOT chrome (`@INEFFICIENT` /
`@EFFICIENT`) uses port-only `ColonizeColony.inefficient_gov` — Col1 `+0x1c`
bit3 remains food starvation.

---

## PARK / thin original effects

| Effect | Decomp cite | Port |
|--------|-------------|------|
| AI colony food `+= difficulty>>1` | [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) | **Wired** (`turn_produce_one_colony`) |
| Rival SoL threshold `(8-diff)*10` | [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md) | Thin fixed thresholds |
| Score→gold rebate `FUN_41f2_0b70` | FUNCTION_CATALOG | PARK |
| Tory floor production penalty | ~11880; [sons_of_liberty.md](sons_of_liberty.md) | **Wired** (`colony_prod_sol_bonus`) |

---

## Not affected

- SoL ≥ 50% independence declare gate
- Victory / defeat calendar years (chrome may **name** the difficulty)

Combat odds **are** difficulty-sensitive for human Euro sides
(`str -= difficulty-4` + Discoverer −25% vs AI Euro) — [combat.md](combat.md).

---

## Conflicts resolved

| Topic | Rejected claim | Authoritative |
|-------|----------------|---------------|
| Viceroy first tax year | Fandom **1534** | `1536-diff` → **1532** |
| Easy starters | Both Hardy + Veteran for all ([assets.md](assets.md) old prose) | Hardy **French-only**; easy grants Veteran broadly |
| Tory caps 10…6 | — | Decomp `10-diff` + manual (fandom matched) |
| Starting gold | Port always **1000** | `FUN_38fd_6024`: **1000 / 300 / 0** |

---

## Implementation map

| Concern | Module |
|---------|--------|
| Wizard / names | [`new_game.c`](../src/core/new_game.c) |
| Save / runtime byte | [`col1_save.h`](../src/core/col1_save.h), [`game_loop.c`](../src/core/game_loop.c) |
| Starting gold (should follow table) | [`ai.c`](../src/core/ai.c), [`europe.c`](../src/core/europe.c) — **flat 1000 today** |
| Starter skills | [`units.c`](../src/core/units.c) `units_starter_skills` |
| FF bells | [`founding_fathers.c`](../src/core/founding_fathers.c) |
| Tax / REF / intervene | [`ai_king.c`](../src/core/ai_king.c) |
| Euro AI economy | [`ai_euro.c`](../src/core/ai_euro.c) |
| Immigration | [`europe.c`](../src/core/europe.c) |
| Indian alarm / raids | [`ai_contact.c`](../src/core/ai_contact.c) |
| Land purchase | [`colony.c`](../src/core/colony.c) |
| Conquest treasure | [`units.c`](../src/core/units.c) |
| Score | [`reports.c`](../src/core/reports.c) |
| SoL / Tory production | [`colony_production.c`](../src/core/colony_production.c) (`colony_prod_sol_bonus`) |
