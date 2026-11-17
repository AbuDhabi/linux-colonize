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
