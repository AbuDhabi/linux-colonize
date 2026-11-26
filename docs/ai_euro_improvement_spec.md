# Euro AI improvement spec

STATUS: implemented 2026-11-15 behind `mods.ai_improvements` (section 7). Tuning open.

Goal: the computer Europeans play under the same mechanics the player has.
Goods travel by wagon or ship, tiles are improved by Pioneer units, colonists
learn from teachers or are bought in Europe, and colonies feed themselves under
the player's production rules. The source list of differences is
[ai_euro_advantages.md](ai_euro_advantages.md); ids below (G2, P1, ...) refer to
its rows.

## 1. Ground rules

1. **One mod: `ai_improvements`.** Every change sits behind
   `mods_on(MOD_AI_IMPROVEMENTS)` (`src/core/mods.h`, appended after the last
   id; docs/settings.md "Mods"). With the mod off the Euro AI is DOS byte for
   byte: same RNG draws, same goldens, same Col1 save interop.
2. **Off by default.** A gameplay change ("New Rules and Features" page), so
   the `settings.json` default is off, as for the other gameplay mods.
3. **Colonial nations only.** The mod applies to the four colonial Euro
   nations. The King AI (REF waves, the crown slot's Euro turn during the WoI,
   offers, tax audiences) is out of scope and stays DOS with the mod on.
4. **Replace the shortcut with the player's mechanic, not with nothing.**
   Removing a handout without giving the AI the matching capability (haul,
   pioneer, teach) only weakens it. Each item therefore names the capability it
   needs (section 4).
5. **Same layering as today.** All code stays in `colonize_sim`; nothing in
   these changes may call `colonize_ui`.
6. **Measured.** An item is done when an all-AI demo run (`-D`,
   docs/demo.md) with the mod on shows the AI still founding, growing and
   defending colonies (section 5).

## 2. In scope: substantive deviations

Differences where the AI skips a mechanic the player must use (goods move
without transport, land is improved without a unit, colonists change skill
without a teacher or a trip to Europe, gold appears to buy a ship), where
a production rule the player lives under does not apply to the AI at all, or
where the AI gets a handout or handicap the player does not.

| Id | Difference | Kind |
|----|-----------|------|
| G1 | Free gold every turn, scaled by difficulty | Handout |
| G2 | Gold raised to Caravel/Privateer/Frigate price | Conjured gold for a purchase |
| G3 | Colony surplus sold from the colony, untaxed | Trade without transport or tax |
| G5 | Europe hold sales pay no tax (#1900) | Trade without tax |
| G6 | Hull cargo with no colony taker sold where the hull stands (#1901) | Trade without transport |
| C1 | 20 tools placed in the colony | Goods without transport |
| C2 | 100 lumber, free when broke | Goods without transport or payment |
| C3 | 2 horses for 10 gold | Goods without transport |
| C4 | Colony muskets banked into Europe recruits | Goods without transport |
| P1 | Phantom Pioneer improves a plot in one tick | Improvement without a unit or time |
| P2 | Phantom Pioneer builds inter-colony roads | Improvement without a unit or time |
| P3 | Pioneer units absorbed for their tools | Consequence of P1/P2 |
| S1 | Expert Farmer/Fisherman bought in the colony | Training without Europe or school |
| S2 | Free expert promotion with a schoolhouse, no teacher | Training without a teacher |
| S3 | Random Master Carpenter grant | Free expertise |
| S4 | Servant/Criminal rewritten to Free Colonist | Free promotion |
| F1 | Colony food `+= diff >> 1` | Handout |
| F2 | Food shortfall < 3 forgiven | Rule exemption |
| F3 | Uncapped horse breeding (overflow then sold, G3) | Rule exemption |
| F4 | Tory production penalty never applied (fix: mirrored threshold, 4.4) | Rule exemption |
| F5 | Bolivar bell subsidy `(pop+3)/5` | Handout |
| F8 | Craft output without inputs (#898) | Rule exemption (removed #1915) |
| R2 | No starter Veteran Soldier for the AI at any level (human gets one on Discoverer/Explorer) | Handicap |
| R3 | AI tax +1 on each in-colony expert purchase, no audience | Tax outside the King's path |

Decision quality: places where the DOS AI uses the same mechanics as the
player but chooses badly. Not in ai_euro_advantages.md; listed here as found.

| Id | Problem | Section |
|----|---------|---------|
| Q1 | Clears its colonies down to no workable forest | 4.2 forest reserve |
| Q2 | Founds colonies next to native villages | 4.9 |

## 3. Out of scope

Price and threshold terms scaled by difficulty; difficulty stays the dial for
them: G4 war-goods price cap, F6 cheaper Founding Fathers, E1 immigration
scale, E2 native land price, K1-K3 combat terms, R1 fair-play attack gates.
Same mechanic as the player: S5 automatic re-equip (the player can do the
same by hand). AI policy, not a shortcut: F7 Custom House cargo list.
King AI: everything in ai_king_* (ground rule 3).

## 4. Items

Each item: what the mod-on AI does instead, and what capability it needs.

### 4.1 Logistics: goods move by wagon or ship (C1, C2, C3, C4, G3, F3)

- **Instead:** a colony that wants tools, lumber or horses raises a demand
  that is met by a wagon from another colony or by a ship from Europe. Muskets
  for recruits are bought in Europe at the dock price. Warehouse overflow is
  hauled to a port and sold through the normal tax path, or it spoils. Horse
  breeding is capped as for the player.
- **Needs:** a demand→haul planner for the AI. Existing pieces: the colony
  demand tallies (`ai_euro_5d04_cb_colony_needs`, DS:0xa0cc demand cells), the
  departing-ship buy-100 loop (5d04 `depart`), the wagon haul / ship load matrix
  (`ai_euro_try_wagon_haul`). Gap: these respond to demand opportunistically;
  nothing guarantees delivery to an inland colony.
- **Proposed:**
  - C1 tools: skip the 20-tool arm; the colony's tool demand stays and is met
    by the departing-ship buy-100 loop (coastal) and the wagon/ship load
    matrix (inland). Wagon Trains are already in the build cascade.
  - C2 lumber: skip the 100-lumber arm. The forced-lumberjack arm already runs
    before it; a colony with no workable forest (see the 4.2 forest reserve)
    turns lumber into a haul demand.
  - C3 horses: skip the 2-horse arm; horses arrive like any other cargo.
  - C4 musket bank: skip the bank arm; the hire arm buys the recruit's
    muskets at the Europe dock price.
  - G3 surplus: skip the colony dump-sell. Surplus is hauled to a coastal
    colony and sold from the ship at the dock, taxed (the departing-ship loop
    already sells the hold). What is not hauled spoils.
  - F3 horses: the AI takes the capped breeding figure, as the human does
    (one test in `turn_production.c`).
  - Risk: AI income falls (G3 is free gold today). Demo runs must show this
    step working before G1 is removed (4.6).

### 4.2 Pioneering by units (P1, P2, P3)

- **Instead:** the colony tick's improve and road arms post a job (tile, kind)
  instead of completing it. A Pioneer unit walks there and works the normal
  number of turns, spending its tools as a player Pioneer does. Pioneers are no
  longer absorbed just to bank their tools.
- **Needs:** AI Pioneer unit behaviour: claim a job, walk, work, return for
  tools, train or buy Pioneers when there is a backlog. Candidate reuse: the
  `MOD_PIONEER_PLAN` plan grid and Automate (docs/pioneer_plan.md), which already
  drive human Pioneers through a job list.
- **Proposed:** the picker (`ai_euro_5952_improve_best_plot`,
  `ai_euro_5952_road_connect_0000`) writes (tile, kind) into a per-nation plan
  grid instead of running the phantom; AI Pioneers with tools take the nearest
  planned tile on their continent through the Pioneer Plan Automate logic and
  re-equip at home when out of tools; the absorption arm skips a Pioneer while
  its continent has planned work. Existing Pioneer sources stay (equip arm,
  5d04 Europe training). Plan grid saved in RTST.
- **Choice of improvements (forest reserve).** Observed in play: an AI colony
  can end up with no forest it can work, so it cannot lumberjack. Mod on, the
  picker needs a better policy than DOS's. Code read 2026-11-15 (not yet
  confirmed by a trace) shows three ways to get there:
  1. The clear flag (`ai_euro_refresh_colony_ai_flags`, WANTS_PIONEER_CLEAR,
     raw 94200-94206) keeps clearing while the ring has `forests > 1`, so it
     aims for one forest left. That count is raw terrain over the whole ring:
     it does not drop forests the colony cannot work (native claim, another
     colony's plot, occupied, `colonies_plot_blocked_mask`). The one left can
     be unusable.
  2. Without the clear flag, a forest plot worked with a field job < 4 (farmer
     and the plantation jobs) and not plowed scores as "improvement missing"
     and gets the plow body, which on forest is a clear
     (`ai_euro_5952_improve_best_plot`, `do_plow`). The food pass seats such
     workers on forest when the colony is short of food, so the last forest
     can go this way too.
  3. Nothing values the forest's own yield: a Prime Timber or fur resource
     forest is cleared like any other.
  Proposed policy, mod on: count only forests the colony can actually work;
  never clear (by either path) below a reserve of workable forests, sized to
  the lumberjacks the colony needs (at least 2, more for a colony building a
  long project list); never clear a forest with a lumber or fur resource;
  on a forest worked for food, prefer a road or moving the worker (4.8) over
  clearing. Confirm 1-2 with an `AI_5952_BUILD_TRACE=1` demo run before
  implementing.

### 4.3 Training (S1, S2, S3, S4)

- **Instead:** experts come from the player's sources only: bought or trained
  in Europe, taught in a schoolhouse/college/university by a teacher, learned
  from a native village, or promoted in combat. Servants and Criminals rise by
  schooling.
- **Needs:** AI teacher placement in school buildings; an Europe "train expert"
  purchase arm in 5d04 planning; native-village learning already exists for
  Free Colonists (village arm in 20e6).
- **Proposed:**
  - S1: skip the in-colony purchase. The food-short trigger stays and becomes
    a training request to 5d04 planning, which already trains Pioneers,
    Missionaries and soldiers in Europe. It pays the correct @JOB price (not
    the DOS slot-indexed row) and the next ship carries the expert home. R3
    goes with it.
  - S2: skip the free promotion. Let the AI seat teachers: the DOS indoor pass
    excludes the Teacher job (0x12), so the DOS AI never teaches. The shared
    education tick then does the teaching.
  - S3: skip the Master Carpenter roll; carpenters train like other experts.
  - S4: skip the rewrite; Servants and Criminals rise by schooling.

### 4.4 Production rules (F2, F4)

- **Instead:** AI colonies starve on any shortfall, as player colonies do.
- **F4 instead:** an inverse difficulty gate, like R2. The AI's Tory count is
  no longer zeroed; the human keeps the DOS threshold `10 − diff` (Discoverer
  10 … Viceroy 6) and the AI uses the mirror `10 − (4 − diff)` = `6 + diff`
  (Discoverer 6 … Viceroy 10), so the AI is hit hardest on the easy levels.
  The same threshold drives the inefficient-government latch, so that latch
  uses the mirrored value for the AI too. Code: `colony_prod_sol_bonus`
  (`colony_production.c`). Decided 2026-11-15.
- **Needs:** the colony tick's food pass already seats food workers until food
  covers need; F2 removal needs it to aim for a margin. F4 needs AI bell
  production (statesmen) to keep SoL up, or the AI loses production; that is
  the same governor-style colony automation as F5 (4.8).
- **Proposed:** F2 is one test (the DS:0x8e5a forgiveness in
  `turn_production.c`); F4 is the mirrored threshold in
  `colony_prod_sol_bonus` and the inefficient-government latch. Both land
  together with 4.8, since without it AI colonies would starve and lose
  production.

### 4.5 Ship purchase gold (G2)

- **Instead:** the AI buys a ship only with gold it has. A shipless nation
  saves toward a Caravel.
- **Needs:** nothing new in planning; risk is an AI that never gets a ship
  back. Measure (section 5) before deciding whether it needs a savings rule.
- **Proposed:** skip `ai_euro_5d04_apply_naval_gold_floors`. The ship-buy
  ladder already buys only what is affordable. G1 is still on at this step,
  so measure before adding a savings rule.

### 4.6 Handouts (G1, F1, F5)

- **Instead:** no per-turn treasury bump; AI colonies get the player's food
  and bell figures. The AI's gold comes from selling goods, treasure, trade
  and native dealings, as the player's does.
- **Needs:** G1 pays today for ship buys, Europe hires and the C1-C3 purchases.
  Without it the AI must earn through trade: the 4.1 haul/sell loop has to
  produce income, not only move goods. Do G1 after 4.1 works.
- **F5:** removed outright with the mod on; no replacement term. AI bell
  production comes from seating statesmen, done by colony automation of the
  kind the player has in the colony governor (4.8, Politics focus). Decided
  2026-11-15.
- **F1:** removed outright with the mod on; no replacement term. AI colonies
  feed themselves through the same governor-style colony automation (4.8,
  Farming focus). Decided 2026-11-15.
- **Proposed:** G1 skips `ai_euro_5d04_treasury_bump`, last in the order,
  once demo runs show trade income after 4.1. F1 (`turn_production.c` Phase B
  food) and F5 (bells composer in `colony_production.c`) are one test each,
  landing with 4.8.

### 4.7 Parity (R2, R3)

- **R2 instead:** an inverse difficulty gate. The human keeps the DOS starter
  Veteran Soldier on the easy levels (`diff < 2`, Discoverer/Explorer); the AI
  gets one on the mirrored hard levels (`4 − diff < 2`, i.e. `diff > 2`,
  Governor/Viceroy). Gate in `units_cargo.c` (FUN_75c2_235c human-only test).
  Spanish keep their nation Veteran regardless, as in DOS. Decided 2026-11-15.
- **R3 instead:** the AI's tax changes only through the King's normal
  audience path. With S1 removed (4.3) the +1 call goes with it; the item is
  done when no AI tax write remains outside the King's audience.
- **Proposed:** R2 adds `mods_on(MOD_AI_IMPROVEMENTS) && AI && diff > 2` to
  the existing human/Spain test, with a unit test per difficulty. R3: before
  4.3 lands, grep the other `ai_king_tax_delta_ai` callers and gate any that
  are outside the audience path.

### 4.8 AI colony automation (capability for F1, F2, F4, F5)

- **What:** AI colonies seated by governor-style automation, the same kind the
  player has (`colony_governor.c`, docs/colony_governor.md: Farming, Cash,
  Industry, Politics, Military focus), so that food (F1, F2) and bells (F4,
  F5) come from colonist placement instead of subsidies and exemptions.
- **Proposed:**
  - Mod on, `colony_governor_run_w` replaces the tick's seating passes (food
    pass, pass 2, indoor, forced lumberjack, carpenter). The rest of the tick
    stays DOS: prelude, absorption/equip, pioneer trio (as changed in 4.2),
    build cascade. Running it after the DOS passes would only overwrite them.
  - The governor seats only. The DOS build cascade keeps choosing the project
    and the governor seats carpenters for it; the governor's own project pick
    is not used for AI colonies.
  - Focus chosen each tick from flags the colony already has: food short →
    Farming; Tories near the threshold or SoL falling → Politics; garrison
    deficit → Military; else Industry, or Cash for an export-heavy colony.
  - Removing the DOS passes removes their RNG draws; that only matters with
    the mod off, where nothing changes.

### 4.9 Colony placement near natives (Q2)

- **Observed:** AI colonies are often founded right next to native villages.
  That suits Spain, which means to destroy them, but costs everyone else
  needless native conflict, France most of all.
- **Causes (code read 2026-11-15, not yet traced):**
  1. The first colony on a continent is staged at a village. The 0a60
     beachhead producer (`ai_euro_goals.c`, DOS decomp 88049) puts the ship's
     FOUND goal on the open-sea tile next to a village, on every continent
     where the nation has no colony. The colonists land beside the village
     (`ai_goals_pick_founding_tile_ex_w`, FUN_521d_06ae, no village term) and
     then pick a site within radius 1-3 of where they landed.
  2. The site scan (`ai_euro_land_explore_scan_target`, FUN_521d_20e6) has a
     village penalty, but a weak one:
     - it looks only at the nearest village, not every village in range;
     - the base is `(tech + alarm quarter + 3)·2`, cut in half across water;
     - France's penalty is halved (`nation_id == 1`), Spain's quartered,
       Pocahontas halves it again, high rival pressure halves it again;
     - the nation's combat value on that continent is subtracted, so an
       armed nation ignores villages altogether;
     - distance 1 (adjacent) is only a penalty, never a hard skip.
- **Proposed, mod on:**
  - Spain keeps the DOS terms.
  - Everyone else:
    - the beachhead goal goes next to the best coastal site on the continent
      that is outside village range, not next to a village;
    - the site scan counts every village within range, not only the nearest;
    - no France halving;
    - the combat-value discount stays for England and the Netherlands (and
      Spain, under DOS terms); France loses it;
    - a hard skip for a site adjacent to a village (distance 1); distance 2
      and beyond is allowed and only takes the normal penalty;
    - a penalty per ring tile the natives claim
      (`colonies_indian_claim_tribe_from_w`), since claimed land is what
      raises alarm and land demands.
- **Decided 2026-11-15:** the Pocahontas halving stays. England, France and
  the Netherlands take the same adjacency rule (hard skip only when adjacent, distance 1;
  every village counted, claim penalty); only Spain keeps the DOS terms. The
  military-strength discount stays for Spain, England and the Netherlands and
  is dropped for France. The hard skip holds even when the discount zeroes
  the penalty.

## 5. Measuring

- All-AI demo runs (`opencol -D`, docs/demo.md) with `ai_improvements` on and off, fixed
  seeds, comparing per nation by 1650/1700: colonies, population, gold, improved
  tiles, experts, ships.
- Automated playtests (automated-playtests/RULES.md) for the human-facing feel.
- Goldens and replay tests must stay green with the mods off; they need no
  re-bless.

## 6. Decisions

Made 2026-11-15:

- One mod, `ai_improvements` (ground rule 1).
- In scope added: G1, F1, F5, R2, R3 (section 2).
- King AI out of scope (ground rule 3).
- R2 is an inverse difficulty gate, not parity: exact mirror, AI Veteran on
  Governor and Viceroy (section 4.7).
- F4 is an inverse difficulty gate too: AI Tory threshold `6 + diff`
  (section 4.4).
- Q2 placement: Spain keeps DOS; England, France, Netherlands avoid village
  adjacency under one rule; Pocahontas halving stays; military-strength
  discount kept for Spain, England, Netherlands, dropped for France
  (section 4.9).
- F1 and F5 removed outright; AI food and bells come from governor-style
  colony automation (sections 4.6, 4.8).
- Order of work (may change later): 4.7 R2 (one gate), 4.9 colony placement,
  4.2 pioneering (most visible, reuse available), 4.1 logistics (largest),
  4.3 training (takes R3 with S1), 4.8 colony automation with 4.4 rules and
  F1/F5, 4.5 ship gold, 4.6 G1 last (needs 4.1 income).

## 7. Implementation (2026-11-15)

Gate: `ai_euro_improved_nation(col1, human, nation)` (`ai_euro_improved_gate.c`,
linked into slim targets too): mod on, nation 0..3, not the human, AI
controlled, not the crown slot during the WoI. Mod off, every path below is the
DOS path unchanged (ctest and `make golden` green, no re-bless).

| Item | Where | What runs mod on |
|------|-------|------------------|
| C1 | `ai_euro_5952_tools_supply_and_connect` | no 20-tool buy; road connect still runs on `turn % 7 == 0` |
| C2, C3 | `ai_euro_colony_jobs.c` | lumber and starter-horse arms skipped |
| C4 | `ai_euro_goals.c` | no bank deposit; the hire arm with no lot buys muskets at the dock price (DOS path) |
| G3, F3 | `turn_production.c` | no dump-sell; capped horse breeding |
| P1, P2 | `ai_euro_land.c` | improve / road arms post a job (CLEAR on forest, PLOW, ROAD) to the per-nation list in `ai_euro_improved.c` (32 per nation, RTST record 19); no phantom, no tools charge |
| P3 | `ai_euro_goals.c`, `ai_euro.c` | a Pioneer with 20+ tools is not absorbed while its continent has open jobs; `ai_euro_pioneer_act` (before 20e6) walks it to the nearest live unclaimed job and runs the player Pioneer body; dead jobs drop |
| Pioneer source | `ai_euro_goals.c` equip arm | open jobs and no Pioneer on the continent: equip one from the colony's 20+ tools; the 20e6 labor band charges 20 tools from the home colony |
| Forest reserve | `ai_euro_improved.c` | workable ring forests (inset, unblocked, not native-claimed, not posted for clearing); reserve `2 + (pop >= 8)`; resource forests and forests at the reserve get a road instead of a clear; the clear flag needs workable > reserve |
| S1, R3 | `ai_euro_5952_specialist_arms` ARM 2 | pays the trained job's @JOB price and spawns the expert on the Europe dock (sentried, `ai_euro_europe_train_expert`); no in-place rewrite, no tax rise |
| S2-S4 | `ai_euro_colony_tick_run` | ARM 1 off; seating passes (unseat, food, pass2, lumberjack, carpenter, restore, indoor) replaced by `colony_governor_seat_w` with `ai_euro_colony_focus`: Military when the garrison flags are set, Politics at `tories + 1 >= 6 + diff` or SoL < 50 with pop 4+, Farming below pop 4, else Industry. Teachers come from the governor |
| F1, F2, F4, F5 | `turn_production.c`, `colony_production.c` | no food subsidy, no shortfall forgiveness, Tory threshold `6 + diff` with Tories counted, no Bolivar bells; inefficient-government latch at `6 + diff` |
| G1, G2 | `ai_euro_5d04_nation_planning_structural` | no treasury bump, no naval gold floors |
| R2 | `units_starter_skills` | AI Veteran on Governor and Viceroy (`starter_ai_improvements_veteran`) |
| Q2 | `ai_euro_land_explore_scan_target`, 0a60 village producer | non-Spain: skip a site at distance 1 from a village; per-village DOS penalty over every village within 6 (capital x2, Pocahontas and fatigue halving kept, combat discount not for France) plus 3 per native-claimed ring tile; the beachhead goal picks the open-sea tile within 12 with the most free same-continent land not next to a village |

Choices made here, not in the spec: claim penalty 3, beachhead radius 12,
forest reserve `2 + (pop >= 8)`, the focus rules above (Cash never chosen).
`AI_IMPROVED_TRACE=1` prints job posts, Pioneer picks and the equip-arm tally.
G3 surplus hauling relies on the existing departing-ship loop; nothing new
hauls inland overflow.

Measured with `ai_replay test-saves-ai/TURN1.SAV 100 --seed 1..12 [--mods
0x800000]` (TURN1.SAV's human holds no colony, so it loses in 1600 and the
processor stops at the day top after about turn 107; measure below that).
Mean per run, three AI nations summed, mod off vs on: colonies 7.9 / 8.1,
population 23.9 / 23.8, gold 3686 / 1915, units 32 / 23, improved ring plots
15.8 / 10.6. The mod-on AI keeps pace on colonies and population without the
handouts; it has less gold, fewer units and fewer improvements (real Pioneers
walk and take turns; small colonies post few jobs). Demo runs to 1650/1700
(section 5) not done yet.

Seating fix (2026-11-15, after the scratchpad playtest): AI colonies stalled
a few hammers short of a project (Docks at 48 of 52) and rarely seated a
lumberjack, because the governor priced the last hammers of a project, and
any hammer with no project, below a field worker's food. `colony_governor_seat_w`
now counts every hammer as project hammers (the DOS cascade re-picks each
turn) at twice the human governor's hammer weight (`GOV_AI_HAMMERS`). The
focus rules are unchanged; dropping Military, the SoL arm or the Farming arm
all measured worse. `ai_replay` (with the `buildings` column: owned rows
Stockade..Iron Works, about 7 at founding), sums over three AI nations, mean
per run, mod off / mod on before / mod on after:

| Run | colonies | population | buildings |
|-----|----------|------------|-----------|
| TURN1.SAV, 100 turns, seeds 1-8 | 7.9 / 8.2 / 7.8 | 24.9 / 22.8 / 23.5 | 66.9 / 59.9 / 62.4 |
| 1646 playtest autosave, 120 turns, seeds 1-4 | 22.5 / 14.5 / 15.2 | 135.0 / 72.8 / 83.8 | 253.5 / 121.8 / 202.0 |

Buildings per colony on the 1646 run now pass DOS (13.3 vs 11.3); the
remaining gap is colony count (expansion), not seating. Seed noise is about
+-15 buildings on the 1646 run.

Second playtest fixes (2026-11-15; Netherlands, Viceroy, New World, seed 5151,
divergences on):

- **Pioneer restart loop.** 0a60's contact-claim probe overwrites a working
  Pioneer's order 8/9 with 10 at the nation-turn top. `ai_euro_pioneer_act`
  then re-picked the same job, and the plow/road body restarted it at 0 (the
  DB-46 reset): a forest clear that never finished in 80 turns. On its own job
  tile with `col1_counter16 > 0`, the act now restores the job's order first.
  Test: `unit_ai_euro_improved` `pjob_resume`.
- **Pioneer-carrying hull deadlock.** Without the G1 bump, a nation whose only
  hull roams with a Pioneer aboard earns nothing. The fold demotes the Pioneer
  to a civilian, and `ai_euro_20e6_colony_sail_pick` skips every colony at
  continent stance 0 for Pioneer cargo. DOS lands the Pioneer only when the
  roam happens onto a FOUND goal tile, and pays for the wait with G1. With the
  mod on, the stance skip does not apply to a colony whose continent has open
  Pioneer jobs. England in the playtest went from 1 colony, pop 1, gold 0 at
  1546 to pop 6 and trading. TURN1.SAV (100 turns, seeds 1-8) is unchanged
  within noise.
- **Dead plow jobs.** The improve arm posted Plow on Hills (and Mountains),
  which `units_pioneer_tile_can_clear_or_plow` refuses. The job dropped as
  dead at the next act, the colony re-posted it the next turn and never chose
  another plot, so its two Pioneers idled for 150 turns. The post now takes
  the clear/plow kind from that test. The scan skips a roaded plot that
  cannot be cleared or plowed.
- **Pioneer waits for a taken job.** A Pioneer whose continent has only
  claimed jobs is kept out of the colony (P3) but had nothing to do; it
  wandered off with DOS 20e6. It now waits in place for the next post.

Third playtest fix (2026-11-16; France, Conquistador, New World, seed 6363):

- **Carpenters at the bank cap.** With no project (the 5952 cascade builds
  nothing more at pop 4-7 once the small band is done), the seating still
  priced hammers as project hammers with no limit, so New Amsterdam and San
  Salvador kept 2-3 carpenters seated at the TB-57 512-hammer cap for 40-80
  turns. With no project, `gov_seat_colonists` now counts hammers only up to
  the cap (`COLONIZE_HAMMER_BANK_MAX - hammers`). Test:
  `unit_colony_governor` `ai_seat_bank_full`.

Expansion gap traced (2026-11-16, pt5 playtest; Netherlands, Explorer,
alternate map). With the mod on, the AI held 1-2 colonies per nation until
about turn 125. `ai_replay`, 8 seeds, AI colonies summed at the last turn,
mod off / on: pt5 turn-10 save to 150, 88 / 62; TURN1.SAV to 100, 59 / 52.
Q2, G1-G3, F1-F3, C2-C4, S2 and the governor seating each move it by noise
only. Founder breakdown (4 seeds, `ai_euro_found_with_unit`): in DOS,
explorer-scan founders (plan '?') are colonists, Pioneers, Soldiers and
Dragoons alike.
- pt5: Pioneer founders fall from 12 to 3. `ai_euro_pioneer_act` holds every
  tooled Pioneer on posted jobs, so it never reaches the 20e6 ring scan that
  founds. With the act off, it is 45 (the equip arm re-mints a Pioneer each
  time one founds), or 130 colonies.
- TURN1: Dragoon founders fall from 8 to 1. Colonies eject 36 Dragoons
  against DOS's 185; not from horses (C3 and F3 restored move nothing).
  Traced 2026-11-17: the 5952 equip arm needs muskets > 49 in the colony,
  and colony muskets come from hulls leaving Europe (raw 93041-93050, 100 of
  each demanded good). That purchase loop stops on bVar7 `cargo_short`
  (fleet holds <= (pop/2 + colonies*2)/2). Mod-on nations stay cargo-short
  because they buy fewer hulls: seeds 1-8 at turn 100, 42 hulls against
  DOS's 63. Seed 1 hull departures: 36 against 65, and 27 of the 36 are
  cargo-short. G1 and G2 are 0 on TURN1 (Discoverer), so the gap is income.
  Hull sales over seeds 1-4: cotton 151 against 2375, tobacco 123 against
  1743, ore 817 against 2388, furs 4105 against 7316. The cause is the
  governor focus `ai_euro_colony_focus`, which picks Farming below pop 4,
  Politics while SoL < 50 and never Cash.
  Variants tried (seeds 1-8, colonies / pop / hulls). With the
  Pioneer rule, TURN1 is 56 / 176 / 42 and pt5 89 / 477 / 68.
  - Cash below pop 4: TURN1 59 / 150 / 50, pt5 92 / 424 / 87.
  - Cash at pop >= 4 only, or with Politics off: TURN1 unchanged (its colonies
    stay under pop 4).
  - DOS job passes instead of the governor: TURN1 55 / 141 / 54.

  Income costs growth either way. DOS gets 59 / 208 / 63 only with the
  F1/F2 food handouts.
- **Landed (2026-11-17): Cash focus below pop 4.** The focus picker sets
  Cash for a colony under 4 colonists. Before, it set Farming. Test:
  `unit_ai_euro_improved` `colony_focus_small_cash`.
  - TURN1 Dragoon founders: 4 to 7 (seeds 1-8; Soldiers 26 to 28).
  - TURN1, 24 seeds, colonies / pop / hulls at turn 100: 176 / 552 / 122
    before, 175 / 487 / 144 now, mod off 180 / 578 / 175.
  - pt5, turn 140: 262 / 1366 / 213 before, 276 / 1327 / 254 now, mod off
    245 / 1375 / 279.
  - Tuning tried, 24 seeds, TURN1 / pt5 colonies / pop:
    - Cash at pop 2-3, Farming at pop 1: 176 / 530 and 271 / 1257.
    - Cash once the colony holds 50 food: 182 / 570 and 267 / 1229.
    - Cash once the colony holds 25 food: 171 / 513 and 266 / 1214.

    No variant recovers pop on both saves. The total pop of each is within 1%
    of plain Cash, so the simplest rule landed.
- A trial rule, a Pioneer the ring scan already marked as an explorer (plan
  '?') keeps the DOS path unless it is on a job, gave pt5 115 and TURN1 45:
  it helps one map and hurts the other, so it is not landed.
- **Landed: Pioneer expansion release** (`ai_euro_pioneer_released`,
  ai_euro_improved.c). While a nation has fewer than 3 colonies on the
  Pioneer's continent (`AI_EURO_PIONEER_EXPANSION_COLONIES`), a Pioneer
  between jobs skips `ai_euro_pioneer_act` and takes the DOS 20e6 path, so it
  can found. It stays on the job list when it is walking to a listed job,
  clearing, plowing, building a road, mid-job on a listed tile, or standing
  on a colony tile (releasing there too gives TURN1 seeds 9-24 108 / 317
  against 120 / 376).
  `ai_replay`, 24 seeds, AI colonies / pop summed at the last turn:
  pt5 (turn 10 to 150) mod on 188 / 1056, rule 266 / 1390, mod off
  260 / 1473; TURN1 (to 100) mod on 164 / 591, rule 176 / 552. The pop cost on
  TURN1 is from Pioneers founding instead of plowing; mod off on seeds 9-24
  is 121 / 370 against the rule's 120 / 376. Tried and dropped: thresholds 2
  (pt5 72) and 4 (no gain on TURN1), letting
  P3 absorb a released Pioneer (no change), gating the PEQ equip arm while
  expanding (pt5 73) and gating the hull Pioneer-cargo exception (TURN1 pop
  171). Test: `unit_ai_euro_improved` `pioneer_released`. The TURN1 Dragoon
  founder gap above comes from income (focus picker); the Cash focus
  entry above lands the fix.

France expansion gap (2026-11-17, pt6 playtest; England human, Conquistador,
New World, alternate map). From the pt6 turn-200 save, 150 turns, France held
77 colonies against DOS's 101 (seeds 1-6). Restoring one mod item at a time,
only turning `ai_euro_pioneer_act` off closed it (98). In DOS France founds
with Pioneers only (9 of 9 founders, seeds 1-4); with the mod, 1 of 4. The
expansion-phase threshold does not reach it: France holds 3+ colonies per
continent. Raising the threshold or releasing on few open jobs cut improved
plots by half or did nothing.
- **Landed: release on a DOS site.** `ai_euro_pioneer_released` also releases
  a Pioneer between jobs when the 20e6 explorer ring scan finds a site
  (`ai_euro_20e6_site_probe`, ai_euro_land.c: the scan run as a probe, its
  fatigue bump undone). So the Pioneer founds where DOS would, and works the
  job list otherwise. 12 seeds, colonies / pop (improved plots):
  - pt6 turn 200 to 350: France 152 / 952 before, 173 / 1115 now (DOS
    195 / 1083). Spain 36 / 204 to 46 / 267, Dutch 271 / 1869 to
    248 / 1798. All AI 459 / 3025 (973) to 467 / 3180 (1094).
  - pt5 turn 10 to 150: 138 / 643 (227) to 166 / 674 (235).
  - TURN1: unchanged (no continent passes 2 colonies in 100 turns).
  Goal note (user, 2026-11-17): the mod aims at sensible play under the
  normal rules, not DOS parity; the DOS columns are context.

All-AI playtests (2026-11-17, pt7-pt7d; CHEAT Set Human None from the turn-0
save, Conquistador, New World, alternate map generator + modern PRNG, about
430 turns each). With no human, the four improved nations ran into:
- England never advanced (tax 0, no Founding Father in 431 turns): the
  bridge copied the idle Europe mirror over DS:0x5398's record every turn
  (port_bugs.md #1857, not a mod bug).
- The Dutch hull roamed with a tool-less Pioneer: the colony-sail pick's stance
  gate is now skipped for improved nations (#1858).
- The Dutch founded on a one-tile islet off the coast and starved every
  colonist they bought: the site scan and the ship unload mask now reject
  land masses under `AI_EURO_MIN_SITE_LAND` (5) tiles (#1859).
- A Dutch hull came back from Europe into a bay with no water path to its
  colonies and stayed there: the sail pick only takes colonies on the hull's
  sea, a loaded hull with no own colony on its sea sails home, and a hull
  leaving Europe re-aims its landfall at a sea with an own colony (#1860).

pt7f (all fixes, #1857-#1861), colonies / pop at turns 100 / 200 / 300 / 486:
England 3/8, 7/32, 4/16, 0/0 (conquered by t380, still landing Pioneers);
France 6/11, 10/30, 18/54, 30/123 (12 Founding Fathers); Spain 4/9, 4/11,
7/30, 14/84; Dutch 4/5, 7/16, 3/11, 1/4. All four expand until about t200,
then France and Spain take English and Dutch colonies. No stall in 486 turns.

pt7d (all fixes but #1860), turn 435, colonies / pop / Founding Fathers:
England 11 / 46 / 7, France 16 / 65 / 7, Dutch 5 / 21 / 3 (stalled until about
t240), Spain 1 / 6 / 2. Spain (DOS site terms, 4.9) peaked at 4 colonies by
t60 and lost them to French and English wars. With #1860 the Dutch hull
then ferried the same four holds between two own colonies every turn: the
berth dump and the DOS load matrix reload what was just unloaded, and the
origin latch sends it on. Improved hulls no longer reload a cargo type they
dumped at that berth (#1861).

pt7f's navies were bought, never built: the DOS naval band needs a
factory-tier building that AI colonies never reach. An improved colony that
makes 6+ hammers and 3+ tools (or owns a Blacksmith's House with ore) now
builds a Shipyard and hulls, also below pop 8 (#1862). pt7g (same start, to
t528): 9 Drydocks and 1 Shipyard by t450, 6 Shipyards by t528 (France 5),
France building 3 Frigates; hull counts still close to pt7f, since the
Shipyards come late.

The DOS completion step hands an AI colony the tools its unit project lacks
(C5). Improved colonies now make or stock them first: the cascade starts no
unit without a tools source, and the governor stops gunsmiths draining a
project's tools and seats the blacksmith once only tools are missing (#1863).

pt7g's end state (t528) showed four more gaps, fixed 2026-11-17: colonies the
DOS cascade leaves on WANTS build from a short useful list instead of banking
hammers (#1864, building tools gift dropped too); Europe buys on demand only,
ships carry surplus tools to colonies short of them, horses stay home, and the
Custom House comes at pop 4 (#1867); small colonies farm once tax passes 50%
(#1868). AI tax raises lose the difficulty level and come more often on
easy levels (#1865); warship threats weigh every other Euro (#1866).
Horses that no longer ride ships mount garrison Soldiers as Dragoons, and the
Custom House sells what breeding pushes past capacity − 50 (#1869).

pt7h (to t543) left France growing and the rest flat (England 3-5 colonies,
Spain 2-5, Dutch 1-3), fixed 2026-11-17. A nation keeps one Scout per
continent (#1870); Pioneers come one per four open jobs, the job list holds
128 tiles, and jobs next to enemies or angry braves wait (#1871, #1876);
Church colonies bless Missionaries for nearby villages without a mission
(#1872). DOS AI pairs never leave a war, so all four were at war from t340:
an improved nation now fights one Euro at a time and signs peace once neither
side finds the war worthy (#1873). The colonist recruit ignores the fleet-size
proxy (#1874), and Europe Artillery stops at one per colony (#1875; France had
bought 77). Reverted 2026-11-25 (#1922).

The user's discard campaign (Viceroy, t127) showed French colonies stuck at
one or two colonists: the governor planned on the AI food subsidy F1 had
removed, so absorbed colonists starved (#1877), and colonies kept asking for
colonists their land could not feed (#1878).

At t188 the same campaign showed three Spanish faults. A colony that cannot
feed another colonist no longer takes one (#1878), and DOS never ships a
Colonist out of a colony, so surplus colonists on Santo Domingo's 9-tile
island wandered there. A spare Colonist with no site and no colony wanting it
on its land mass now waits in the nearest colony, and a ship berthing there
takes it while a colony elsewhere wants colonists (#1880). The first landing
had put Spain's Veteran Soldier on a tile whose only land neighbour is a
village, where it stood for 180 turns; the unload never picks such a tile
(#1879). Spain's small fleet tripped `cargo_short` on every departure, so it
bought no cargo at all and held 1 tool across seven colonies, which meant no
Pioneers; an improved hull now buys tools anyway, but still no muskets or
horses, which colonies spend arming their colonists (#1881).

At t214 Isabella still banked about 500 hammers with nothing in production (#1882). With no tools and no way to make them, the tools skip dropped every pick, and the delivery picker only sent tools to a colony whose project needed them. An improved colony left with no project now builds the cheapest building its tools stock covers, and a colony with no project and 100+ banked hammers counts as 100 tools short when a ship picks where to unload tools.

At t254 (#1883-#1886) Isabella was idle again, because ships dropping founders unloaded all their tools at the new colony; a ship now keeps its tools when another colony is short of them. France had turned most of its people into Dragoons: Europe recruits came armed, berths loaded soldiers onto Galleons that then roamed with nowhere to go, and Dragoons founded 1-pop colonies. An improved nation stops arming recruits once it has a Soldier or Dragoon per colony, boards soldiers only for a real war destination, lands a soldier cargo where colonists are wanted, and lets an idle Soldier or Dragoon join a colony that wants colonists. The recruit cap, the war-destination boarding and the soldier join were reverted 2026-11-25 (#1922); the ship tools fix stays.

At t286 (#1887-#1888) Isabella and Santo Domingo again had nothing in production and no tools. Spain's tools had become Pioneers, eight of which sat on the two-colony northern island with two jobs while a Galleon shuttled them between the island's colonies. A berth now boards a Pioneer only when its land mass has more Pioneers than open jobs and a colony the hull can reach has fewer, and a hull carrying one sails there first. Pioneers that did reach a job turned back the next turn: standing on a job tile before work began did not count as mid-job, so on a land mass with fewer than three colonies the DOS path walked them home. Over 30 replayed turns Spain went from 1 improved plot to 10.

All-AI playtests 2026-11-18 (`ai_replay --all-ai --save-every 25`, the turn-1 saves of the discard game and campaigns 7-9, all mods, 300 turns): the four nations reached 5-12 colonies and 30-75 colonists each by t301. Fixed: tool-starved idle colonies (#1889) and swarming colonist stacks (#1890); with both, the four games ended at 130 colonies / 832 pop against 120 / 710. Seen and left open: warehouses full of furs, horses, trade goods, tools or muskets for 50+ turns (nothing ships them out); armed units at half the colonists or more; colonies building something with no lumber coming in, hammers flat for 25+ turns.

Idle builds, checked 2026-11-18: the late idle colonies have built everything that needs no tools, so every remaining project needs 20-200 tools, and the governor stops seating carpenters once the bank reaches the cap, so the cost is the missing buildings, not lost labour. Tool deliveries already go to the short colonies first. The limit is supply: the Europe buyer takes one 100-tool lot per ship visit. Buying one lot per short colony halved idle hammers but cost colonists and buildings (the four games at t301 went from 832 to 688 colonists; buying extra lots only above 1000 gold still lost, 832 to 729 on seed 7 and 890 to 857 on seed 11), because the gold no longer recruited. Left as is: more tools would have to come from colony blacksmiths, not from Europe.

Full warehouses, checked 2026-11-18: most sightings are imports, not lost production. A 100-lot of tools, muskets or trade goods from Europe fills a colony without a Warehouse, and horses at the cap only stop breeding. Goods a colony makes and cannot ship (fur, sugar, coats, ore) were at the cap in about 2% of colony snapshots. On a turn with no colonist on the Europe dock the purchase gate `bar <= demand[cargo]` has a bar of 0, so a cargo no colony asked for can be bought (30 of 706 lots in four 300-turn games were sugar, tobacco, cloth, coats and the like). Blocking every zero-demand lot for improved nations, or only the zero-demand lots of goods the nation sells, left 4 of 5 seed runs with fewer colonists (seed 7: 832 to 853 and 832 to 693; seed 11: 890 to 760 and 890 to 847; seed 13: 770 to 728), so the gate stays as DOS has it.

Frozen builds, checked 2026-11-18: 3.7% of colony snapshots (25-50 turns apart) show the same project with unchanged hammers. These colonies seat no lumberjack and no carpenter, and that is the governor's choice, not a seating bug. In a small colony (Havana, pop 7, three farmers) any move off food starves it. In a big one (campaign 9 New Amsterdam, pop 11, Military focus, 44 hammers for 125 turns) a lumberjack and carpenter pair scores 182 against 196 for the distillers, weavers and fur traders it would replace, whose rum, cloth and coats ship out for sale. The three-turn horizon cannot see a building's payoff. Left as is.

Governor SoL value for the AI, checked 2026-11-18: the governor commits dd7599af..3a8fc7b3 reach the AI only through the bell value of SoL (`GOV_SOL_SHARE`); with that value zeroed for AI colonies the four games replay exactly as before the commits. With it, the twelve 300-turn runs (four games, seeds 7, 11 and 13) end with 2617 colonists against 2418, 415 colonies against 392, colony SoL 42% against 20%, 443 Founding Fathers against 261 and more gold. Statesmen rise from about 5% to 20% of colonists, carpenters and lumberjacks fall by a third to a half, and builds frozen for 50 turns rise from 31 to 105 (mostly Docks and Stockade). Halving the bell value for AI colonies cut frozen builds to 41 but lost on every outcome (2396 colonists, 372 colonies, 346 Fathers), so the AI keeps the full value and the frozen builds.

Early scouting, fixed 2026-11-18 (#1891): improved nations had scouted 15 of some 880 villages by t51 in the twelve replays, against 131 once a nation with no Scout mounts a dock colonist in Europe and its Scout walks to the nearest seen unscouted village or rumour. The economy gain is small and inside the replay noise (three runs: 2601-2729 colonists at t301 against 2617, buildings +3-9%, Fathers +3-6%); villages taught a skill fell from 242 to 188-233. A few Scouts still stood idle late, once nothing was left on their land.

Spare Scouts, fixed 2026-11-18 (#1892): a Scout keeps its last target while that is still a target, because the AI goto lasts one turn. With no village or rumour left, it walks to the nearest unseen land. It checks each target with the real goto pathfinder first. The land walk stops at villages, foreign colonies and foreign units, and the 0f74 flood has no step to some land that the walk reaches. A Scout with nothing it can reach joins a colony that wants colonists, or it explores and founds one. In twelve replays at t301 this gave 18 Scouts instead of 38, with 49% of the land seen instead of 46%. Colonists went from 2601 to 2744 and buildings from 6371 to 6716.

Cargo fleet, fixed 2026-11-19 (#1893): the DOS ship-buy ladder needs a Galleon's 3000 gold, which DOS tops up from the treasury bump and gold floors. The improved nation gets no free gold and spends its income on recruits and tools, so at t301 it had 2 cargo holds per colony (old AI 3.9) and 13% of its colonies went 50+ turns without a ship while furs overflowed. A nation with fewer than 3 holds per colony and a sale good at the warehouse limit (Custom House colonies excepted) now buys a Galleon or a Merchantman, and until then keeps a Merchantman's price back from recruits and from Europe cargo buys other than tools a build waits on. In twelve replays at t301 Europe sales rose from 1.35M to 1.44M gold, colonists from 2744 to 2796 and buildings from 6716 to 6844. 11% of colonies still go 50+ turns without a ship: the hulls exist but the haul pick does not send them there.

Founding Fathers, fixed 2026-11-19 (#1894): DOS keeps the weighted roll of the category with the most Fathers left, whatever the Father does, so most nations elected Henry Hudson first with no Fur Trapper to double. An improved nation now sees the slate a human's Congress shows (one weighted roll per category) and takes the row worth most to it: Jefferson, Smith, Bolivar, Penn, Franklin and Stuyvesant score high; Fugger, Hudson, de Brebeuf and Las Casas only count a live boycott, Fur Trappers, own missions or Converts; de Witt's trade is human-only and scores 0. Spain values Cortes and Sepulveda higher, on top of its DOS bonuses against and for natives, and Cortes also rises with every village the nation has burned. Bolivar's +20 SoL, human-only in DOS, applies to improved nations too. In twelve replays at t301 Fathers rose from 477 to 504, buildings from 6844 to 7603 and Europe sales from 1.44M to 1.60M gold; colonists held (2796 to 2741). A pick from all 25 Fathers without the slate was tried first: it elected more Fathers but never de Brebeuf or Pocahontas, and Converts fell from 324 to 141 and colonists by 14%.

Building projects, fixed 2026-11-19 (#1895): the AI seating weighted every hammer alike, whatever it built, so a colony with fishermen and no Docks kept its statesmen and never seated a carpenter (discard campaign, t151: 8 of 12 AI colonies made no hammers; Roanoke 0 hammers for 51 turns, Fort Nassau 51 turns at 36 of 52). `colony_governor_seat_w` now seats once as if the project stood and prices each hammer at the per-turn score gain times 20 turns over the project's cost, when that beats the flat weight. In twelve replays against HEAD at t301: colonists 2685 to 2868, buildings 7412 to 8089, Custom Houses 151 to 204, Docks 280 to 331, Docks projects stalled at 0 hammers 54 to 29; colonies 424 to 416 and Fathers 502 to 501 held. A second climb from Industry's seating, which reached seatings the single moves miss, lost 9% of colonists and 7% of colonies and was dropped. The extra seating makes a 300-turn all-AI replay about 1.7x slower (100 s to 168 s). Colonies with no workable forest (Barbados) still cannot build Docks.

Standing armies, fixed 2026-11-19 (#1896): DOS garrisons about half of every colony (wanted defenders `(n - 1) / 2` on the tile, more homed off it, up to three Artillery a tile), so the all-AI replays on alternate maps carried 103 Soldiers, Dragoons and Artillery to 171 colonists at t300 (sums over four nations, 25 games, one per map and difficulty). An improved colony now keeps a peacetime garrison of one defender for 2-11 people, one more per ten (`ai_euro_improved_peace_garrison`), wants no field force, stands the surplus on its tile down into its warehouse (Veteran Soldiers last; a colony that cannot feed him keeps his muskets and horses and sends him out as a Colonist), recalls its Dragoons too, keeps its horses in store instead of mounting Soldiers (#1869), and its military and upgrade bands pick Artillery only while it has none on its tile and the nation fields fewer than one per colony; with nothing else worth building, Artillery stays the hammer sink, as for a human player (DOS's three a tile). It goes back to the DOS footing, arming colonists from the banked muskets and horses through the 5952 equip arm, when it is threatened (`ai_euro_improved_threatened`): at war with a European power that holds a colony or with a living tribe, in the War of Independence, or with another power's armed units in the field within two tiles. DOS's own threat scan (`garrison_quota`) was tried first and dropped: it counts any foreign unit within five tiles, colonists included, and kept 60-90% of one nation's colonies armed. Same 25 starts against HEAD at t300: Soldiers 18 to 8, Dragoons 41 to 19, Artillery 44 to 44, colonists 171 to 187, buildings 489 to 581, Fathers 34 to 39, muskets in store 1517 to 2145, horses 2927 to 3604; colonies and native villages unchanged; hammers banked in colonies 2020 to 2420. Capping the hammer-sink Artillery too cut Artillery to 34 but banked 2700 hammers. Reverted 2026-11-25 (#1922, user): playtests found the improved AI too weak; colonies keep DOS's garrison and Artillery footing again, and #1869 mounts Soldiers in peace too. Since nearly every AI nation counts as short of cargo room, its ships then bought almost no muskets; a ship now buys a musket lot for each two colonies that want defenders and hold under 50 (#1923).

Full holds and spread muskets, 2026-11-25 (#1924): the DOS dock-demand arm sails any hull home the moment a colonist waits on the Europe dock, so a nation with steady crosses shipped 5-60 tons a trip and left its furs and coats in the warehouses (discard game Spain: never over 400 gold). An improved hull with at most one hold in use first collects a full 100 lot from an own colony on its sea within 16 tiles. Muskets bought for colonies short of defenders (#1923) no longer land all at one berth: a hull keeps them, as #1883 keeps tools, until it reaches a coastal colony short of them, so a warehouse cap no longer eats them.

Armies at home, 2026-11-25 (#1925): DOS walks every armed unit its colony does not hold through the LAB_4d2e wander tail, a random step or a park beside any rival colony, so in the second discard game half of France's and England's armies stood in the open at peace while colonies had no defender, and England's field force never came back when the human took Roanoke and Barbados. The #1911 Artillery rule now covers improved Soldiers and Dragoons too, away from a war enemy (none within 6 tiles; near one they keep DOS's wander and attack scan): they park beside a rival colony only with war declared or intended, walk to the nearest own colony short of defenders that no other unit is walking to, else home, and a unit sharing its colony with another armed unit leaves for a short colony. All-AI 25 starts x two seeds at t300: units in the field 8 to 3.4 a nation, colonies of pop 3+ with no armed unit 1.25 to 0.55, armed units 23 to 22, population and colonies within seed noise.

Colonists where they are useful, 2026-11-26 (#1926): DOS sends a new colonist to join a colony before it ever looks for a site, picks the fullest colony that still wants people, and joins the colony it stands on, so colonies fill to 8-12 before anyone founds and small colonies get nobody. An improved colonist joins the smallest colony in reach (distance + 2 x population), and when its best colony already has 4 people and the ring scan has a site nearby, it founds a new colony instead. All-AI 25 starts x two seeds at t300: colonies 7.7 to 8.5 a nation, population 40 to 45, armed units 22 to 21.

No free upgrades for carpenters, 2026-11-26 (#1927): DOS's carpenter arm turns the Indentured Servant or Petty Criminal it picks into a Free Colonist, and in a colony of 7+ people with no Master Carpenter makes the colonist one on a 1-in-(17 - difficulty) roll. An improved nation seats the colonist as he is; it gets Free Colonists and experts the way a player does.

Tools for blocked builds, fixed 2026-11-19 (#1897): colonies kept banking hammers with no project because every project left to them needs tools they lack (#1864 skips those), and the tools came too slowly. Three leaks: the equip arm made a Pioneer from the 100 tools a colony's build was waiting on (Havana's first delivery in 50 turns left as a Pioneer the same turn); a hull in Europe bought one 100-lot a visit however many colonies were short, and a short colony took the whole hull; and a hull keeping its tools for a short colony (#1883) never sailed there, because the delivery pick divides by distance and kept choosing near colonies that were not short, where it kept them again (Spain parked three Merchantmen with 100 tools each on its west coast for 20+ turns while New Holland on the east coast waited). Now Pioneers come only from tools past the build's (`ai_euro_colony_build_tools`, in the DOS Pioneer target, the 4.2 open-jobs arm and the labor-band Pioneer), a hull buys a lot for each short colony (less one per 100 tools already aboard the nation's hulls), a short colony unloads lots only until its build is covered, and a hull with only tools aboard skips colonies not short of them. Same 25 starts against #1896 at t300: buildings 581 to 592, colonists 187 to 190, Fathers 39 to 40, hammers banked 2416 to 2207, colonies with 200+ hammers and no project 29 to 28. The rest are mostly broke nations and hulls still in transit.

Buying small tools gaps, 2026-11-19 (#1898): with no tools gift (#1863/#1864) an improved colony also waited on a ship for the last few tools of a small project, and a human would pay the Buy surcharge for those instead. Now the cascade counts a project in reach when its tools gap is 50 or less and the nation can pay the human Buy price for it (gap x (Europe tools price + 4), FUN_2f2b_5e44), and once the hammers are paid the colony buys the gap through `colonies_buy_construction_ex`; bigger gaps are still imported. Restricting it to buildings needing at most 50 tools left 12 stalls. Two seeds x 25 starts at t300 against #1897: colonies with 200+ hammers and no project 27.5 to 1.5, hammers banked 2272 to 1831, gold 5236 to 4022 (about 300 a nation spent), buildings 576 to 554 and colonists 187 to 183 (seed-to-seed swing about 30 buildings).

Mirrored market pressure, 2026-11-19 (#1899, user): DOS gives every AI seller 1d44's fixed −2 arm, so an AI moved Europe prices like a Discoverer human on every level. An improved AI now takes the human arm at the mirrored level 4 − difficulty: a Viceroy AI keeps the old −2 (Discoverer stability), a Discoverer AI gets +2 (Viceroy stability), per ton (2 − difficulty)·0.16 on top of the volatility shift (`europe_apply_trade_volume`). Two seeds x 25 starts at t300 against #1898: Viceroy runs are identical (same term); on easier levels sugar and furs end a step lower (Discoverer furs 3.5 to 3.2), finished goods stay at 11-13, and colonies, population, experts, army, warships and hulls move within seed noise. The term is small next to the volatility shift (1.68 against 2.32 per ton at volatility 1), so it is a mild dial.

Taxed sales and no sale at sea, 2026-11-19 (#1900, #1901, user): DOS pays an AI the untaxed gross for every hull sale (FUN_38fd_1f0c at the dock, the FUN_521d_20e6 sell tail anywhere), so its tax rate only touched Custom House sales and the #1865 spread barely reached its treasury. An improved AI now pays its tax rate on both, booked to royal_money like the player's (`ai_euro_improved_sale_net`). And a hull whose supplies no colony takes no longer sells its hold where it stands: it sails home and sells at the dock (`ai_euro_20e6_3fa6_sail_home`). Two seeds x 25 starts at t300 against #1899: Crown take from AI sales about 2.5-4x (Discoverer 4081 to 11822, Viceroy 1251 to 5356), gold a nation 1075 to 631; Viceroy population 68 to 70 and experts 43 to 47, other columns within seed noise; late New World exports after tax 44 a turn on Discoverer to 204 on Viceroy.

Tools price and import-only buys, 2026-11-19 (#1902, #1903, user): DOS caps an AI's tools and muskets rise at `high + (difficulty - 4)*2 + (turn - 600)/100` (FUN_38fd_0058), which sits below the floor on easy levels, so a Discoverer AI paid 2 for tools all game while the hard ceiling `((4 - difficulty)*3)/2 + 3` meant to make easy AIs pay more (DB-114). An improved AI skips the rise cap and keeps the ceiling; its t300 tools price now runs 6.8 on Discoverer down to 2.7 on Viceroy. And a departing improved hull buys only Europe imports (horses, trade goods, tools, muskets), not the New World goods DOS's demand cells also asked for. Two seeds x 25 starts at t300 against #1901: Discoverer population 56.5 to 51.3 and army 131 to 103, Viceroy population 70 flat and warships 30 to 37; gold a nation 408-783 to 549-2404, so the money saved on goods is partly banked.

Mirrored colony tile, 2026-11-20 (#1904, user): DOS gives every colony tile +2 food and +1 secondary on Discoverer and +1 food on Explorer, AI colonies included. An improved AI takes that handout at the mirrored level 4 − difficulty (`colony_yield_commons_difficulty`), so a Viceroy AI gets the Discoverer boost and a Discoverer AI none; the DOS AI food `+difficulty>>1` was already off for it (4.6 F1). Two seeds x 25 starts at t300 against #1903: Conquistador identical; Discoverer population 51 to 49 and experts 28 to 25; Governor population 63 to 67 and experts 37 to 45; Viceroy experts 47 to 51 and gold 2404 to 3391. Population now runs 49 (Discoverer) to 71 (Viceroy).

Inland and island sites, 2026-11-20 (#1905, user): DOS never founds inland (FUN_521d_20e6 raw 89134 skips a tile without open sea before the best-site compare), and #1859 kept improved nations off land masses under 5 tiles. Once an improved nation holds 3+ colonies and 20+ colonists, a small island with a forest in its ring is a site (and a landing tile for its hulls), and an inland tile is scored like a coastal one when the continent holds 2+ of its coastal colonies and one of its colonies, coastal or not, is within 5 steps. Two seeds x 25 starts at t300 against #1904: colonies a nation 7.9-9.8 to 10.2-13.5, about 40% inland and 23 on islets in all; population Discoverer 49 to 56, Viceroy 71 to 95; SoL down about 4 points. Inland colonies stay smaller (mean pop 4.9 against 6.7) and 30% of them sit at pop 1.

Inland hauling, 2026-11-20 (#1906, user): DOS builds AI Wagon Trains only before 1600 and sends a wagon to sell one hold at a village or parks it, so inland colonies piled up exports. An improved nation keeps building wagons past 1600 while it has fewer than one per two inland colonies on the continent; its wagons carry 100-lots of crops, furs, silver and manufactures from inland colonies to the nearest own coastal colony and return for more; their DOS village errand takes no food or horses; and an inland colony starts its Custom House before the cascade's pop-4 stop. Two seeds x 25 starts at t300 against #1905: inland cargos at warehouse cap 0.48 to 0.11 a colony, wagons 0.3 to 3 a nation; population within noise on every level (Viceroy over six seeds x 5 starts 84 to 91 a nation, standard error about 3.5). Before food was barred from errands, Viceroy fell from 95 to 81 on both seeds.

Roads where they pay, 2026-11-20 (#1907, user): the 4.2 improve arm posted a road on any ring plot it could not plow, so improved nations paved plowed farmland and open plains, where a road adds nothing, and the turn-7 connector laid a second road beside a chain that already linked the pair. Now a ring road is posted only on forest, Hills or Mountains or under a Fur Trapper, Lumberjack or Ore/Silver Miner, a plot with nothing worth doing never wins the scan, and the connector skips a pair that roads already join within twice their distance (`ai_euro_road_linked`, ai_euro_land.c). Two seeds x 25 starts at t300 against #1906: roads off settlements 129 to 102 a game, roads on open flat land 48 to 15; colonies and colonists within noise or slightly up on every level (Viceroy 85 to 91 a nation).

Surplus gold, 2026-11-20 (#1908, user): improved nations ended games with 30k-180k gold. Gold above 10,000 left after the 5d04 ladder now trains Europe experts each turn: first the @JOB with the most colonists working it without the skill, less experts of it already outside a colony (only rows with a dock price, so no Sugar/Tobacco/Cotton Planter or Fur Trapper); with no such need, founders (Expert Farmer, Fisherman, Lumberjack, Master Carpenter, fewest first) while the nation has under 4 colonists outside colonies. Up to 4 a turn, one of a kind waiting on the dock. Limit: the AI unit cap (DOS 0x124, shared by all AI nations) is full by about t400 in all-AI Viceroy games, so the hoards that build after that cannot be spent this way; at t500 artillery, mostly built in colonies, holds about half the slots. Two seeds x 25 starts at t300 against #1907: Viceroy gold 1918 to 1393 a nation, colonists 90.9 to 91.8; other levels within noise. Three Viceroy starts to t500: colonists 1207 to 1228, gold still 9k-180k a nation.

Lumber at the site, 2026-11-20 (#1910, user): on a 1799 all-AI map France had 30 colonies, and three desert colonies had no forest in their ring at all: the DOS site score (FUN_682a_000c nibble) only weighs terrain, and any score above 0 is a site, so once the good tiles near its colonies were taken a nation filled in the desert between them. An improved nation now skips a site with no lumber plot around it; the forest reserve (2 workable forests, 3 from pop 8) keeps a lone forest uncleared. Two seeds x 25 starts at t300 against #1909: colonies without forest 0.5-1.8% to 0-0.3%, colonists 56.3 to 54.7 a nation (within noise per level; a two-plot gate gave byte-identical games). Seed 211 unlimited to 1799: 82 to 89 colonies, colonists 148 to 164 a nation, none without forest. Also tried and dropped: a two-plot gate (65 colonies, 150 colonists on seed 211), a site-score floor of 6 for wide nations (colonists 53.3 at t300, colonies -9%), and raising the forest reserve to a flat 3 (colonists 51.1, from lost farmland).

Artillery stays home, 2026-11-20 (#1911, user): at peace the seed 211 1799 map had 160 of 401 AI Artillery in the field. DOS sends a combat unit its colony does not need as garrison through the LAB_4d2e wander: a random step ('9'), or the 0x46 park beside a rival colony; the improved build cascade then replaces each one that leaves (its idle fill caps Artillery on the colony tile only), and parked Artillery made the rival colony "threatened" (#1896), so it kept DOS's three-Artillery cap. Artillery in the open dies to ambushes and field attacks, so an improved nation's Artillery now stays on its own colony, waits beside a rival colony only with war declared or intended on that rival, and otherwise walks to the nearest own colony. Two seeds x 25 starts at t300 against #1910: Artillery in the field 556 to 61 of about 2000-2200, colonists 54.7 to 55.6 a nation (noise); seed 211 at 1799: 160 to 2 in the field.

Wars stick, 2026-11-21 (#1913, user): improved wars ended at the first 13b0 peace check (seed 211: 1-14 turns), since peace came once neither side found the war worthy and an all-AI game's checks mostly answered no (independence, outnumbered, the anchor gates of #1912). Peace now needs both sides' momentum at or below 0, where momentum = 4 when `ai_euro_10ec_war_worthy`, plus a freshness term (+3 at the start, one less every 4 turns, down to -4 after 28), plus net fights won against that rival in this war / 2 (cap ±4), plus net colonies taken from it × 2 (cap ±6). A pair's ledger (first turn, fights, colonies; `ai_diplo.c` "War ledger") starts with its WAR bit and clears with peace, kept only with the mod on and saved in RTST record 20; fights come from the unit-attack and colony-militia combats, colonies from `colonies_capture`. Peace is checked on an encounter, every third turn, and (#1914, user) by the 6d8e treaty-timer pass when the pair has had no peace test for 30 turns, so a war without contact ends once both sides tire (seed 211 before #1914: one pair 178 turns; after: 8-135 turns, median 20; 50 games at t300: warring pairs over 50-turn saves 234 to 179, colonies 9.3 and colonists 54.8 a nation). Two seeds x 25 starts at t300 against #1912: games with a war at some 50-turn save 40 to 48, warring pairs summed over saves 173 to 234, colonies flat, colonists 56.4 to 54.7 a nation, no nation eliminated. Seed 211 unlimited to 1799: 17 wars, ended ones 12-178 turns (median 22), two still on in 1799; Spain lost three early wars and ends with 4 colonies.

Fighting a war, 2026-11-24 (#1921, user): in the discard game Spain (Republic of Mexico, improved) was at war with the human French from t276 and lost Isabella, Santo Domingo, Trinidad and Asuncion in t345-t355 without one land attack. Four causes, four changes. (1) A colony short of defenders with nothing to make or buy tools with started no Artillery (#1863), so Veracruz and Trinidad banked 509 hammers with no project and no defender for 50 turns; now a colony short of defenders within 6 tiles of a European war enemy's armed unit or colony queues the Artillery anyway, which reserves its tools from Pioneers, makes hulls buy them and lets the #1898 gap buy run. (2) DOS keeps every lone garrison where it stands, so Spain's five Soldiers held quiet western colonies while four eastern colonies of 12 stood empty; now, at war, the lone Soldier of a colony with no war enemy within 6 tiles walks to the nearest empty own colony on its continent with a war enemy within 4 (plan 'd', kept across the 0a60 order clear, 'A' on arrival). (3) Spain founded three colonies on (45,39), 3 tiles from French Martinique, each lost the next turn; now no site within 3 tiles of a war enemy's armed unit or colony, in the scan and at the founding act. (4) DOS zeroes Artillery's odds on an open tile, and an admitted ('A') Artillery never scores; now a surplus Artillery (another armed unit stays) with a war enemy adjacent scores the sally, still behind the odds < 12 gate, and since 1b0e quarters Artillery attacking in the open the gate passes only weak targets (DOS's own odds core scored the siege stacks beside Isabella and San Juan at 1-2). Replays (`ai_replay`, 6 seeds) from the t340 save, France idle and the war on, 40 turns: Spain's empty colonies 8.0 to 6.8, Soldiers 2.5 to 3.3, colonies 16.8 to 15.5 (fewer sites beside French units), population 150 to 149. All-AI from t276 (France makes peace within ten turns), 60 turns: within seed noise. Goldens and ctest unchanged (mod off).
