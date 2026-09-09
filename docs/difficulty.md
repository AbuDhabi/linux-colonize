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
| Human starting gold | Discoverer **1000**, Explorer **300**, Conquistador+ **0** | **Wired** 2026-08-28 (`ai_starting_gold`, `ai.c`) |
| Starter skills | Easy → Veteran Soldier; French Hardy always; Spanish Veteran always | Wired |
| FF liberty-bell thresholds | Harder → higher human need, lower AI need; WoI `diff*1500+2000` | Wired |
| King tax cadence | Audience interval `band − 2*(diff−2)`, band 18/15/12/9 by year; turn ≥ 30 | Wired |
| REF seed / growth | New-game pools `8*diff+15` / `5*(diff+1)` / `3*diff+2` / `6*diff+2`; purse `diff*8+10` per peacetime turn | Wired |
| Tory uprising | Fires on `rng(0,diff+1)!=0`; colony score `+diff+1` | Wired |
| Intervention pools | `1a26` census seed uses `−diff` / `(4−diff)/2`; landing size fixed | Wired |
| Euro AI gold / hire | Harder → less free gold, higher hire gate | Wired |
| AI immigration pressure | `((8-diff)*score)>>3` | Wired (AI only) |
| Indian alarm prelude | Harder → escalate more often / larger bumps | Wired |
| Raid kind demote | Discoverer only: harsh → STORES/NOTHING | Wired |
| Indian land purchase | Harder → costlier for human, cheaper for AI | Wired |
| Conquest treasure | Difficulty bands 0..3 (Gov+Vic share band 3) | Wired |
| Score villages burned | `-(diff+1)*burned` | Wired |
| Tory / inefficient gov | Thresh `10-diff`; full mod `−⌊tories/thresh⌋` + sol latches | **Wired** — see [sons_of_liberty.md](sons_of_liberty.md) |
| AI colony food | `+= difficulty>>1` | **Wired** |
| Town commons food | `+2` Discoverer / `+1` Explorer (all nations; `FUN_15eb_1f72` ~12519) | **Wired** 2026-08-28 (`colony_yield_town_commons`) |
| Town commons secondary | `+1` Discoverer only (`FUN_15eb_1f72` ~12568) | **Wired** 2026-08-28 |
| Ore/silver depletion | per unit `rng(0,diff+1)!=0` bumps `+0x97` (`FUN_364b_0688` ~57932); Discoverer 1/2 rate … Viceroy 5/6 | **Wired** 2026-08-28 (`turn_produce_one_colony` epilogue) |
| Rival SoL pressure | Threshold `(8-diff)*10` | thin / PARK |
| End-game Colonization Rating | `FUN_41f2_0b70` difficulty multiplier {4,5,6,8,10} | **Done** 2026-08-29 (`reports_score_rating`) |
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

**Port:** [`ai.c`](../src/core/ai.c) `ai_starting_gold` (new game, both the Col1 nation
record and the Europe screen). `europe_reset_campaign_nation` still seeds 1000 but
the new-game path overwrites it; loaded saves take gold from the save.

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

## King tax (royal audience)

The tax event has **no first year and no season**. It is a turn-counter gate,
ported from `FUN_38fd_5be8` (`viceroy_unpacked.c:68420`, caller `FUN_38fd_5e52`
at `:68539`) in [`ai_king_audience_roll`](../src/core/ai_king.c); the delta is
applied by `FUN_38fd_3dc8` (`ai_king_audience_apply_delta` /
`ai_king_tax_commit`). Called every peacetime turn from `ai_king_nation_turn`
(`FUN_43f7_2424` peacetime arm) for the human slot only; `1d42` is a full no-op
once the WoI is declared, and so is the audience with it.

Gate, in order:

```
turn counter (DS:0x538e) >= 30                      // no audience before turn 30
interval_base = 18 ; year>1600 → 15 ; year>1700 → −3 ; year>1750 → −3
interval      = interval_base − 2*(difficulty − 2)  // DOS "audience nation is human" arm
fire only when turn % interval == 0
skip when tax_rate > 85
```

Difficulty shortens the cadence by 2 turns per level above Conquistador and
lengthens it by 2 per level below — it never moves a "first year".

| Diff | ≤1600 | 1601–1700 | 1701–1750 | ≥1751 |
|-----:|------:|----------:|----------:|------:|
| 0 Discoverer | 22 | 19 | 16 | 13 |
| 1 Explorer | 20 | 17 | 14 | 11 |
| 2 Conquistador | 18 | 15 | 12 | 9 |
| 3 Governor | 16 | 13 | 10 | 7 |
| 4 Viceroy | 14 | 11 | 8 | 5 |

(The old doc's `22 − 2*diff` happens to be the ≤1600 column; it missed the year
bands, the turn-30 gate and the 85% skip, and the `1536 − diff` "first year" was
never in the code at all.)

Once the gate passes, difficulty drops out — the delta comes from a favor score
(`viceroy_unpacked.c:68460-68467`):

```
score = RNG(1,1000)
      + (rebel_sentiment_report*2 − tax_rate)*5
      + gold/100
      + census_pop_proxy[human]      // DS:nation−0x6bf0, the FUN_4962_0018 census
      + turn/30
```

Ladder → signed delta:

| Score | Delta |
|-------|-------|
| < 100 | cut `−min(RNG(2,5), tax_rate)`; **no event at all** if the cut would be 0 |
| 100–649 and `king_audience_streak < 30` | `+1`, streak++ |
| 650–949 (and the streak≥30 fallback out of the +1 band) | `+2` (plus a narrative-line reroll, no numeric effect) |
| 950–1099 | `RNG(3,4)` |
| ≥ 1100 | `RNG(5,8)` |

Apply is unconditional and clamped to **0..75%** (`ai_king_audience_apply_delta`;
excess is trimmed back out of the applied delta). Only a genuine positive
applied delta reaches the village-goods dialog, whose choice is keep-the-raise
vs. tea party (which *reverts* the raise and boycotts one roulette-picked
cargo) — it never gates whether the raise happens.

---

## REF seed, growth and intervention

### Expeditionary Force seed — at **new game**, not at declare

The REF exists from turn 1 and nothing drains it before the declaration
(`75c2:360b..3643`, `viceroy_unpacked_2.c:112436-112443`). Port:
[`ai.c`](../src/core/ai.c) `ai_new_game` (wizard path),
[`game_loop.c`](../src/core/game_loop.c) (save template + a load-time backfill to
this floor for pre-WoI saves from older builds). `ai_king_do_declare` re-seeds
**only** when all four pools are still zero — re-seeding at declare would shrink
an accumulated force.

| Pool | DS | Formula | 0 | 1 | 2 | 3 | 4 |
|------|----|---------|--:|--:|--:|--:|--:|
| Regulars | `0x53da` | `8*diff + 15` | 15 | 23 | 31 | 39 | 47 |
| Dragoons / Cavalry | `0x53dc` | `5*(diff + 1)` | 5 | 10 | 15 | 20 | 25 |
| Man-O-War | `0x53de` | `3*diff + 2` | 2 | 5 | 8 | 11 | 14 |
| Artillery | `0x53e0` | `6*diff + 2` | 2 | 8 | 14 | 20 | 26 |

(`head.expeditionary_force[0..3]` in save order regulars / dragoons / MoW /
artillery. The old `8+diff*4` … `2+diff` table matched nothing in the code.)

### Per-turn growth — the royal purse, not the tax event

`FUN_43f7_1d42` (`viceroy_unpacked.c:74846-74907`, OVL07 asm at file offset
`0x2c62`), ported as `ai_king_1d42_royal_purse`. Runs once per **peacetime** turn
for the human slot; the asm's first test (`TEST [0x5382],1 → RETF`) makes it a
full no-op after the declaration.

```
growth = difficulty*8 + 10 ;  doubled at year>=1600, >=1700, >=1750
nation.royal_money += growth
if royal_money >= 1800:
    k = 0                                                  // Regulars
    if (reg + 2)/3 > cavalry:              k = 1           // Cavalry
    if reg/4        > artillery:           k = 3           // Artillery
    if (reg+cav+art+5)/10 > man_o_war:     k = 2           // MoW (last write wins)
    expeditionary_force[k]++ ; @KINGBUY ; royal_money -= 1800
```

| Diff | growth ≤1599 | 1600–1699 | 1700–1749 | ≥1750 |
|-----:|-------------:|----------:|----------:|------:|
| 0 | 10 | 20 | 40 | 80 |
| 1 | 18 | 36 | 72 | 144 |
| 2 | 26 | 52 | 104 | 208 |
| 3 | 34 | 68 | 136 | 272 |
| 4 | 42 | 84 | 168 | 336 |

So Viceroy buys a REF unit roughly every 43 peacetime turns in the 1500s and
every 5–6 turns after 1750; Discoverer, 180 and 22. Growing pools on tax
audiences was an invented stand-in and is gone (2026-09-06).

### Wave and Tory-uprising difficulty terms

`ai_king_ref_wave` (`FUN_43f7_0982` / `FUN_43f7_2022` gate at
`viceroy_unpacked.c:74994`) runs the invasion while
`regulars + (cavalry>0) + (artillery>0) != 0` — the MoW pool alone does not
sustain it. When the land pools are empty the crown falls back to the
`FUN_43f7_06a6` Tory uprising (`:73829-73932`), which is the only
difficulty-sensitive part of the wave:

```
fire at all:  rng(0, difficulty+1) != 0        // (diff+1)/(diff+2): 1/2 … 5/6
colony score: pop*(100−SoL)*2/100 + difficulty + 1 − attack of units on the tile
```

Veteran and Dragoon promotion of the spawned irregulars roll on the same
`rng(0, difficulty+1) != 0`. `AI_KING_SECOND_MOW_DIFF` (the old "second MoW wave
at `diff ≥ 2`") is a leftover constant with no call site — that shape was a
stand-in.

### Foreign intervention

Two independent things, neither of which is a difficulty band:

1. **Pool seed at declare** — `FUN_43f7_1a26` (`viceroy_unpacked.c:74765-74795`,
   type lookup `:74424`) writes `backup_force[0..3]` (DS `0x53e2`/`e4`/`e6`/`e8` =
   Regulars / Dragoons / **Man-O-War** / Artillery) from the *ally's* live census
   (`ai_king_seed_backup_force_1a26`; the ally is the weaker of the two remaining
   Euro powers, `rival_nation_slot_1`). Difficulty enters twice, as `−diff` and as
   `iVar7 = (4−diff)/2`:

   ```
   iVar7 = (4 − diff) / 2
   pool0 = ((census_pop_proxy[ally]/10 − diff + 8) + 9) / 2         // Regulars
   pool1 = (((field_combat_totals[ally]+1)>>4) + iVar7 + 1 + 2) / 2 // Dragoons
   pool3 = ((iVar7 + ((land_combat_strength[ally]+1)>>5) + 3) + 4)/2// Artillery
   pool2 = ((iVar7 + privateers[ally] + frigates[ally] + 3) + 4)/2  // Man-O-War
   then: pool1, pool3 ≤ 2*pool2 ;  pool0 ≤ 6*pool2 − pool1 − pool3
   ```

   Harder levels shrink every pool by 1–2 (and the census term dominates), so the
   table of fixed `2+diff` / `1+(diff>0)` backup values in the old text is wrong
   in both shape and source.

2. **Trigger and landings** — the announce is bought by the WoI liberty-bell pool
   at `diff*1500 + 2000` bells (`founding_fathers_bells_needed`, see the FF table
   above), spent through `ai_king_spend_woi_bell_pool` (`FUN_4345_0a22` →
   `:74462` @INTERVENTION, which sets the `0x5382` bit2 **intervention-once**
   latch at `:74493`). After that latch, the per-turn free drain
   (`FUN_43f7_2022`, `:75007`) keeps landing forces every turn while
   `backup_force[2]` (the MoW pool) is nonzero. Each landing
   (`FUN_43f7_10f0`, `:74378-74449`, `ai_king_foreign_intervene_ex`) is spawned
   for the **human's** nation — the intervention force is player-controlled — and
   is fixed at six land units plus a hull, with no difficulty term:

   ```
   1 Man-O-War (type 0x12) on the best water tile beside the colony  (pool 2 −1)
   Cont. Cavalry  = min(pool1, 2)
   Artillery      = min(pool3, 2)
   Cont. Army     = 6 − (cavalry + artillery), capped by pool0
   all Veteran (profession 0x15), 5×5 reveal around the colony
   ```

   "Up to 2 landings, 3 at `diff ≥ 2`" was a stand-in; `AI_KING_INTERVENE_DIFF_THIRD`
   no longer exists.

See [`king_ref.md`](../original_sources_annotated/ai/king_ref.md).

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

Human English also `*2/3` (nation 0) independent of difficulty. `needed_crosses`
is this score each EOT (grows with pop/units). Idle **+2** into `current_crosses`
until the first dock immigrant; afterward only church production — not a fixed
9 with +1-per-immigrant bump.

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
Decomp `FUN_41f2_0b70` — resolved 2026-08-29: it is *not* a gold rebate. It
computes the **Colonization Rating** shown on the Retire exploits screen and
used as the Hall of Fame sort key: `rating = ((mult × total) / 100) >> 1` with
`mult = {4,5,6,8,10}` for Discoverer..Viceroy, and an exploits tier (`largest
n−1, n in 1..24, with n²/3 < (mult × total)/100`, cap 23) that picks how many
GAME.TXT `@SCORE` lines / which `SCORE<nn>.SS` picture the screen shows.
Ported: `reports_score_rating`, `reports_render_exploits`, `game_retire_after_score`.

---

## Production: easy-difficulty handouts (2026-08-28)

`FUN_15eb_1f72` (town-commons composer) reads `DS:0x53a6` twice: commons food
gets `+2` at Discoverer / `+1` at Explorer right after the terrain-class split,
and the secondary commodity gets `+1` at Discoverer. No nation gate. Depletion:
`FUN_364b_0688`'s epilogue rolls `rng(0, diff+1)` once per depletion unit
(units tallied in `FUN_15eb_18ec`: Minerals+Ore Miner 1, Minerals+Silver Miner
2, Silver deposit+Silver Miner 1) and bumps `+0x97` only on a nonzero roll.
None of this is exercised by the goldens (colony_prod01 = difficulty 2,
colony_prod02 = 4) — asm-read only. Side note from the same read: the decomp
adds the river bits to the *secondary* only and `layer2 & 0x40` to food, but
removing the port's river-on-food term breaks three real captures by −1
(Montreal, St. Louis, Fort Orange), so the port keeps river on food; the
`0x40` label is what's suspect, not the captures.

## Production: Tory / inefficient government

Manual + decomp (`viceroy_unpacked.c` ~57468): Tory pressure threshold

```
threshold = 10 - difficulty   // Discoverer 10 … Viceroy 6
```

Production penalty is **not** a flat −1 at the threshold. DOS uses
`−⌊tory_count / threshold⌋`, then adds +1/+2 from SoL latches — full formula and
port status in [sons_of_liberty.md](sons_of_liberty.md). DOS also latches colony
flag bit `0x08` on the inefficient path.
Port: full net mod in `colony_prod_sol_bonus` (Tory floor + sol latches / live
SoL stand-in). Inefficient-government EOT chrome (`@INEFFICIENT` /
`@EFFICIENT`) latches on that same Col1 `+0x1c` bit3
(`COLONIZE_COLONY_FLAG_INEFFICIENT_GOV`) since 2026-09-04, so a crossing
announced before a save is not announced again after the reload. The port's own
food-shortfall reading, which used to occupy bit3, moved to the runtime-only
`ColonizeColony.food_shortfall_latch`.

---

## PARK / thin original effects

| Effect | Decomp cite | Port |
|--------|-------------|------|
| AI colony food `+= difficulty>>1` | [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) | **Wired** (`turn_produce_one_colony`) |
| Rival SoL threshold `(8-diff)*10` | [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md) | Thin fixed thresholds |
| Score→Colonization Rating `FUN_41f2_0b70` | full static port 2026-08-29 (`viceroy_unpacked.c:72415-72548` + `41f2_0092`/`0f56`/`14a8`) | **Done**. The "gold rebate" label was wrong: the value is a percent rating (`((mult*score)/100)>>1`, mult `{4,5,6,8,10}`) fed to `@EXPLOITS` "COLONIZATION RATING: %NUMBER0%" and to the HALLFAME.DAT sort key; the "return-type mismatch" flagged before is just Ghidra typing the score composer's int return as `undefined1*`. See `docs/reports.md` F10 / Hall of Fame. |
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
| First tax year | Fandom **1534**; port doc's old `1536-diff` | Neither: `FUN_38fd_5be8` has no year gate at all — turn ≥ 30 + a turn-modulo interval |
| REF pool seed | Old doc `8+diff*4` / `4+diff*2` / `2+diff` at declare | `75c2:360b` at **new game**: `8*diff+15` / `5*(diff+1)` / `3*diff+2` / `6*diff+2` |
| REF pool growth | "grows on each tax event" | `FUN_43f7_1d42` royal purse, per peacetime turn |
| Easy starters | Both Hardy + Veteran for all ([assets.md](assets.md) old prose) | Hardy **French-only**; easy grants Veteran broadly |
| Tory caps 10…6 | — | Decomp `10-diff` + manual (fandom matched) |
| Starting gold | Port always **1000** (fixed 2026-08-28) | `FUN_38fd_6024`: **1000 / 300 / 0** |

---

## Implementation map

| Concern | Module |
|---------|--------|
| Wizard / names | [`new_game.c`](../src/core/new_game.c) |
| Save / runtime byte | [`col1_save.h`](../src/core/col1_save.h), [`game_loop.c`](../src/core/game_loop.c) |
| Starting gold | [`ai.c`](../src/core/ai.c) `ai_starting_gold` |
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
