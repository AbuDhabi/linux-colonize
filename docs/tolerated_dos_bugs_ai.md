# DOS bugs tolerated for now: Euro and native AI

STATUS: living record, not an audit dump. Authoritative.

Second file of [tolerated_dos_bugs.md](tolerated_dos_bugs.md), split off to
keep each file under the doc-length limit. It holds TB-3, 4, 6, 7, 8, TB-9..TB-30, 37, 38, 50, 52 and 56 (re-split
2026-11-03), the entries
about the Euro AI (colony tick, ships, land units) and the native AI (villages,
Braves). The rules, the id scheme and the entry format are the ones in
tolerated_dos_bugs.md; ids are shared across both files and never reused.

## Entries

### TB-3 — Euro AI delivery sell books its trade ledger twice
**DOS:** when a Euro AI ship's delivery matrix in FUN_521d_20e6 picks no colony,
the sell tail (raw 89828-89851) dumps every hold for gold. Per hold it calls
`FUN_291f_0a2e` -> `FUN_38fd_1dfa`, which already credits the nation's
`trade.gold[g]` (+0x7c, tax-adjusted) and `trade.tons[g]` / `trade.tons2[g]`
(+0xbc / +0xfc) with the quantity. The tail then adds the untaxed
`DS:0x84bc price * qty` to `trade.gold[g]` and `qty` to `trade.tons[g]` a
second time. The treasury (+0x2a) is credited once, untaxed.

**Effect:** that AI nation's per-cargo trade ledger (gold and tons sold)
overstates these sales roughly twofold. The port reads `trade.gold` /
`trade.tons` only for the human nation (Trade report, King's tax-raise goods
pick FUN_38fd_3dc8), so for AI nations the inflated values mostly just live in
the save. `trade.tons2`, which the market price model sums, is booked once.

**Port (reproduces it):** `ai_euro_20e6_delivery_sell_tail`
(`src/core/ai_euro_ship.c`).

**Blocks removal:** the ledgers are saved, and the idle campaigns
(`golden_idle_*`) byte-compare saves with DOS; a single booking changes those
bytes from the first AI delivery sell on. Not test-run.
**Status:** tolerated (2026-10-21). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-4 — AI warehouse dump-sell adds the cargo index to tons sold
**DOS:** FUN_364b_0688 phase O (raw 57806-57848) sells a non-human Euro
colony's warehouse surplus. Per cargo it calls `FUN_291f_0a2e` ->
`FUN_38fd_1dfa` (ledgers + volume, as in TB-3), then adds to `trade.tons[c]`
the loop's cargo index `c` instead of the amount (raw 57829-57833, asm
364b:17bd `ADD [BX+0xbc],AX` with AX = cargo index), and adds the untaxed gross
`DS:0x84bc price * amount` to `trade.gold[c]` on top of 1dfa's taxed credit. The
treasury (+0x2a) is credited once, untaxed.

**Effect:** same ledger as TB-3: the AI nation's `trade.tons[c]` gains a
spurious `c` per dump-sell and `trade.gold[c]` is roughly doubled. Same readers,
so mostly invisible outside the save.

**Port (reproduces it):** `europe_ai_colony_dump_sell_w`
(`src/core/europe_market.c`).

**Blocks removal:** as TB-3 (saved ledgers, byte-compared by `golden_idle_*`).
Not test-run.
**Status:** tolerated (2026-10-21). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-6 — Euro AI buying from natives scores the wrong goods
**DOS:** in the native trade session (FUN_4d56_2820, LAB_002e92, asm
4d56:31b8-3204), after a sale the AI picks which of three offered goods to buy.
The offer list is the bid-sorted cargo order with Muskets, Food, Tools and
Trade Goods skipped. The pick does not score that list: it reads three raw
bytes of the sorted array at `BP-0x86 - i`, i.e. sorted slots 16, 15 and 14,
scores each with the `DS:0x84bc` price row and keeps the first strict maximum
(seed -9999), then uses the winning index 0..2 into the filtered list. Slot 16
is one past the 16-byte array and aliases the low byte of the haggle round
counter `iStack_88`, which is 0 whenever the AI gets here, so it scores Food.

**Effect:** which good a Euro AI unit buys from a village depends on the
prices of Food and the top two unfiltered cargos, not on the prices of the
goods actually offered; in practice an arbitrary pick, usually the first
offer. Human players choose through @BUYWHICH and are not affected.

**Port (reproduces it):** `ai_contact_2e92_ai_pick`
(`src/core/ai_contact_trade.c`). Came from port_bugs.md #796.

**Blocks removal:** tried 2026-10-21 (score the offered goods' price bytes);
fails `golden_idle_independence` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-6).
**Status:** tolerated (2026-10-21). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-7 — Villages-per-continent table holds only the last tribe's villages
**DOS:** the Indian census FUN_4962_06b6 (raw ~78392-78406) runs once per tribe
type (eight calls from FUN_4d56_1816). Each call zeroes the whole
`village_counts_by_continent[16]` table (`DS:0x947e`; it is not per tribe),
then counts only the current tribe's settlements into it. After the eight
calls it holds the villages of whichever tribe ran last, not all villages.

**Effect:** the reader is the Euro AI's continent balance check FUN_521d_0492
(raw 87098-87136, `ai_goals.c`), which compares
`continent land / 12` with villages + Euro colonies on that continent to
decide whether a continent is crowded. With most tribes' villages missing,
the AI treats continents as emptier than they are when planning colonies.

**Port (reproduces it):** `ai_contact_indian_census_4962_06b6`
(`src/core/ai_contact_demand.c`). Unit test `unit_ai_indian_census` pins the
last-tribe-only result.

**Blocks removal:** tried 2026-10-21 (count every settlement); fails all six
idle gates (the table is saved) and `unit_ai_indian_census`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-7).
**Status:** tolerated (2026-10-21). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-8 — Indian horse breeding credits the last settlement's tribe
**DOS:** in each tribe's turn, the census FUN_4962_06b6 (raw 78392-78406) walks
every settlement through `FUN_281f_0a4c` (= `FUN_15dc_0032`), leaving the bound
tribe pointer `DS:0x8d4e` on the nation of the last settlement in the array.
The horse-breeding step of FUN_4d56_1816 (raw 81617-81628) then reads
`horse_herds` (+8) and writes `horse_breeding` (+10) through that pointer, but
caps the result at `(pop + 25) * 2`, where `pop` is the *current* tribe's
population total (`DS:0x9622[tribe]`).

**Effect:** whichever tribe owns the last settlement record gets its horse
breeding advanced on every tribe's turn (up to eight times a turn), while the
other tribes never breed through this step. Each add is clamped to the cap of
the tribe whose turn it is, so a small tribe's turn can pull the last tribe's
breeding value down to that small tribe's cap.

**Port (reproduces it):** the §6c horse-breeding block after
`ai_contact_indian_census_4962_06b6` in `src/core/ai_contact_demand.c`.
Unit test `unit_ai_indian_census` pins it.

**Blocks removal:** tried 2026-10-21 (breed the current tribe); fails all six
idle gates and `unit_ai_indian_census`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-8).
**Status:** tolerated (2026-10-21). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-9 — Euro AI colony-site scan gives its own-tile bonus to tile (0,0)
**DOS:** FUN_521d_20e6, the explorer's colony-site scan (raw 89123-89125, asm
521d:2ac4-2ad5). After the site score `local_28 = (seen nibble & 0xf) * 4`, the
code adds 0x10 when `local_18 == 0 && local_1c == 0`. Those are the absolute
scan coordinates, not offsets from the unit, so the bonus belongs to map tile
(0,0), which is never a land site on the unit's continent. It looks like the
test was written for a loop over offsets (own tile = 0,0) and left behind when
the loop became absolute.

**Effect:** the bonus never fires. If it meant "the unit's own tile", the site
the unit stands on would score 0x10 higher, so the AI would found where it
stands more often. Affects every Euro AI explorer scan.

**Port (reproduces it):** `ai_euro_land_explore_scan_target`
(`src/core/ai_euro_land.c`, the `int score = nib * 4;` line) has no bonus.

**Blocks removal:** tried 2026-10-22 (`score += 0x10` on the unit's own tile);
fails `golden_ai_turns`, `golden_ai_joint`, `golden_idle_continuous`,
`golden_idle_reload`, `golden_idle_america` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-9).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-10 — Euro AI plot scoring indexes tribe tables with the nation id
**DOS:** FUN_15eb_28c8 (raw 13096-13111), in the claim term for a plot owned by
a village, takes the plot's claim byte (the village's nation id, 4..11) and
passes it unchanged to three tables that are indexed by tribe 0..7:
`FUN_15dc_00e0(claim, nation)` (the alarm word at DS:0x5b1c), the tribe
dwelling count `DS:0x91cc[claim*16+continent]` and the tribe data byte
`DS:0x9184[claim]`. FUN_4cc6_00f2 does it right (it subtracts 4). With ids 4..7
the reads land on the last four tribes' rows; with 8..11 they run past the
tables into the colony records and the unit-type counts.

**Effect:** the reluctance penalty for working tribe land is computed from
unrelated bytes (seen in DOSBox, America seed-100 56->57,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-10)). Hits AI job and
plot placement (and the founder's seating) every tick near tribe land.

**Port (reproduces it):** `ai_euro_28c8_score_full` in
`src/core/ai_euro_colony_jobs.c` (`const int n = claim_tribe + 4;`), with the
out-of-range reads emulated by `ai_euro_28c8_ds_byte_91cc`,
`ai_euro_28c8_ds_byte_9184` and `ai_euro_5952_alarm_word`. The fix is to index
with `claim_tribe` (nation - 4).

**Blocks removal:** tried 2026-10-22 (index with `claim_tribe`); fails
`golden_idle_america` and `_reload`; reverted.
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-11 — Euro AI tile-improvement scoring reads the tribe alarm with the nation byte
**DOS:** the tile-improvement arm of FUN_5952_035e (raw 94446-94449, overlay asm
5952:1318-1325) reads the plot's claim byte (the village's nation id, 4..11)
and pushes it unchanged to `FUN_281f_030c` (= `FUN_15dc_00e0`, the alarm word
at DS:0x5b1c + (tribe*0x27 + nation)*2), which is indexed by tribe 0..7. The
other 030c call in the same function subtracts 4. Tribes 4..7 read the last
four tribes' alarm, and ids 8..11 run past the Indian records into the colony
records.

**Effect:** the penalty that makes an AI colony avoid improving plots next to
a village is computed from another tribe's alarm or from colony bytes, so the
choice of plot the AI pioneers plow, clear or road changes every colony tick.
(The sign of the later `local_132 -= (4 - alarm)` term, where an angrier tribe
raises the plot score, is a separate question and is left alone.)

**Port (reproduces it):** `ai_euro_5952_improve_best_plot` in `src/core/ai_euro_land.c`
(`ai_euro_5952_alarm_word(ctx->col1, tribe[claim].nation_id, nation)`; the
out-of-range read is emulated in `ai_euro_5952_alarm_word`). The fix is to pass
`claim`, the tribe index (nation - 4).

**Blocks removal:** tried 2026-10-22 (pass the tribe index); fails
`golden_idle_continuous` and `golden_idle_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-11).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-12 — Refused food beg bumps the wrong alarm word
**DOS:** in FUN_5bfb_022e the accepted-beg arm lowers the Indian-to-Euro alarm
with `FUN_281f_0d6c(0x8d52, param_1, ...)` (raw 96796), where `param_1` is the
European. The refused-beg arm (raw 96819) raises it with
`FUN_281f_0d6c(0x8d52, param_2, local_8, 0)`, where `param_2` is the begging
Indian nation (4..11). The alarm array holds four words per tribe, so the write
lands past the tribe's own row, inside the next tribe's record (clamped 0..100).

**Effect:** the tribe's alarm toward the refuser never rises, and another
tribe's record bytes are edited instead. The village's own attitude word still
grows by half. Every refusal is affected, human and AI (AI Spain always
refuses).

**Port (reproduces it):** `ai_contact_apply_beg_food`
(`src/core/ai_contact_demand.c`), refuse arm, via
`ai_diplo_indian_alarm_word_raw`. Evidence: DOSBox independence 4->5.

**Blocks removal:** tried 2026-10-22 (pass the Euro nation); fails
`golden_idle_independence` and `_reload`, and `unit_ai_contact_core` ("BEGFOOD
decline must leave the European's alarm" pins DOS); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-12).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-13 — Euro AI colony tick loses its own nation after the improvement arm
**DOS:** FUN_5952_035e keeps the colony's nation (colony+0x1a) in the local
`local_1b0` (raw 93981). The tile-improvement arm's 8-neighbour veto loop
(raw 94481-94485) stores each neighbour's owner (FUN_281f_06d2: settlement
owner, else unit owner, else -1) into the same local and never restores it.
Everything that runs after the arm reads the clobbered value as the colony's
nation: the indoor-pass rank byte `DS:0x917c + local_1b0` (raw 94821), the
alarm word for the Wagon Train arm, the build cascade's Musket price
`DS:0x84cb + local_1b0*16`, armed-unit counts and ship counts
(raw 95759-95844). The earlier uses (threat scan at raw 94932/94967) run before
the arm and are not affected.

**Effect:** whenever the arm found a plot, the colony's job election and build
choices (muskets, armed units, ships, Wagon Trains) use the stats of the last
scanned neighbour's owner, a tribe, or index -1, instead of its own nation.
Seen in DOSBox (America seed-100 save 58,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-13)).

**Port (reproduces it):** `ai_euro_5952_improve_best_plot` in
`src/core/ai_euro_land.c` writes the last neighbour's owner to
`*out_improvement_owner`; `ai_euro_colony_tick_open` in
`src/core/ai_euro_colony_jobs.c` stores it as `t->improvement_owner`, which the
indoor pass, the rank byte and the build cascade read. Unit
`unit_ai_euro_5952_build` `case_blacksmith_gate_reads_local_1b0` pins the
consumer side. The fix is to leave `improvement_owner` at `col->nation_id`.

**Blocks removal:** tried 2026-10-22 (the fix); fails all six idle gates;
reverted.
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-14 — Euro AI road connect leaves the "current colony" on another colony
**DOS:** FUN_5952_035e raw 94389-94400, on every `turn % 7 == 0` tick where the
AI holds tools: FUN_5952_0000 builds a connecting road through a phantom
Pioneer (FUN_291f_0216 = FUN_479b_0526). The road completion calls
FUN_281f_0614 (raw 76908), whose FUN_15eb_0142 ends with FUN_15eb_002c and so
rebinds the current-colony globals `DS:0x8542` / `DS:0x8dc6` to the colony
nearest the gap tile. Nothing restores them. The `colony+0xb6 -= 20` charge
that follows and every later arm of the tick (improvement arm, idle sweep,
food and job passes, indoor pass, build cascade, the FUN_521d_6d8e prelude
read) run on that other colony.

**Effect:** the other colony is processed twice (and counted twice in the
6d8e needs tally), the colony whose turn it was is skipped for the rest of the
tick and keeps its old jobs, and the tools/timer charge lands on the wrong
colony. Seen in DOSBox (tr9 11->12, America2 resync 37,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-14)).

**Port (reproduces it):** `ai_euro_5952_tools_supply_and_connect`
(`src/core/ai_euro_land.c`) returns the colony nearest the road
(`g_units_pioneer_0614_colony`) as the payer; `ai_euro_colony_tick_open`
(`src/core/ai_euro_colony_jobs.c`) makes it the tick's colony and sets
`g_colonies_plot_cache_colony` (`colony_plots.c`) so the plot cache still
belongs to the colony being ticked; `ai_euro_colony_tick_run` reads that
colony in the 6d8e prelude. The fix is to charge and keep working on the
ticking colony.

**Blocks removal:** tried 2026-10-22 (the ticking colony pays and the tick never
rebinds); fails `golden_idle_america` and `_reload`. The narrower variant (the
tools charge still lands on the rebound colony, only the later arms stay on the
ticking colony) fails the same two. Reverted.
**Human colonies:** the rebind filter is -1 (raw 76902), so DOS also takes a human
colony; the port spares those since 2026-10-25 ([dos_bugs_ai.md](dos_bugs_ai.md) DB-64).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-15 — Euro AI road-connect skips a colony only when it is far on both axes
**DOS:** FUN_5952_0000 raw 93610-93621 skips a candidate colony only when
`|ox - x| > 6` and then also `|oy - y| > 6` (the y test is nested inside the x
test). The helper looks for nearby colonies to link by road; the natural test
is "far on either axis" (OR). With AND, a colony 20 tiles away in x but level
in y still gets the continent check, the random roll and a walk.

**Effect:** the roll `rng(0, own_colonies - 2)` is drawn for many more
candidates than intended, so the AI RNG stream shifts on every
`turn % 7 == 0` tick, and roads may be built between colonies far apart along
one axis.

**Port (reproduces it):** `ai_euro_5952_road_connect_0000` in
`src/core/ai_euro_land.c` (`abs(ox - cx) > 6 && abs(oy - cy) > 6`). The fix is
`||`.

**Blocks removal:** tried 2026-10-22 (`||`, with and without DB-23's walk-origin
reset); fails `golden_idle_america` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-15).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-16 — Euro AI plot scoring reads stack residue for the forest penalty
**DOS:** FUN_15eb_28c8 keeps the candidate tile's terrain class in `local_32`,
but assigns it only inside the food-emergency arm (raw 13037, asm
15eb:2ae7). The non-emergency tail's forest penalty (`local_32` in 8..0x17,
raw 13119-13122: score -10 when the colony is short of lumber) therefore reads
whatever the stack held. Inside the Euro AI colony tick that is 0x23ec after
a FUN_281f_0c04 ledger refresh and 0x12 after a FUN_281f_0c36 set-job (DOSBox
2026-10-16), so the penalty is off after a ledger refresh and on (0x12 is a
forest class) after a set-job, whatever the tile is.

**Effect:** in every AI colony tick the forest -10 follows the previous call
instead of the tile, so it hits non-forest plots and misses forest ones, which
shifts which plot or job a colonist gets.

**Port (reproduces it):** `ai_euro_28c8_score_full` in
`src/core/ai_euro_colony_jobs.c` (`l32_forest` from `s_28c8_l32`, updated by
`ai_euro_28c8_l32_mark` at each ledger refresh and set-job). Human
auto-assign already scores the candidate tile's class. The fix is to use the
candidate tile's class (`forest`) in the tick too.

**Blocks removal:** tried 2026-10-22 (the candidate tile's class in every
caller); fails `golden_idle_continuous`, `golden_idle_reload`,
`golden_idle_america` and `_reload`. The failures come from the tick alone: the
fix applied only to the founding join (DB-24) passes everything. Reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-16).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-17 — Euro AI indoor job election clamps by a stale quantity
**DOS:** the indoor-workplace election in FUN_5952_035e (raw 94784-94810)
computes `local_78` = stock - shortfall + gross of the job's input cargo
(`DS:0x2b6[job]`, with Gunsmith and Carpenter remapped) and clamps the job's
output with `if (local_78 < out) out = local_78` (raw 94806-94808). `local_78`
is assigned only inside `if (-1 < input cargo)`, but the clamp runs for every
job. Preacher and Statesman have no input cargo, so they are clamped by the
previous input job's value (usually Gunsmith or Carpenter), and the very
first job read in a tick sees an uninitialised stack word.

**Effect:** the AI's choice of indoor specialist for an idle colonist depends
on the last input job examined, not on what Preacher and Statesman can
actually produce, so crosses and bells jobs are over- or under-valued.

**Port (reproduces it):** `ai_euro_5952_indoor_pass`
(`src/core/ai_euro_colony_jobs.c`) keeps `avail` across jobs and slots and
clamps `qty` unconditionally; the uninitialised first read is modelled as
0x7fff (no clamp), a seed that still needs a live DOS capture (was
known_divergences.md #915(c)). The fix is to clamp only when the job has an
input cargo (`in >= 0`).

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`,
`golden_idle_reload`, `golden_idle_america` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-17).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-18 — Euro AI schoolhouse arm elects the table row index as the profession
**DOS:** FUN_5952_035e (raw 95874-95891), the improve_timer-gated expert
election. The loop over the six rows of the DS:0x864 craft table reads the
@JOB from `[row*4+0x865]` for the census test (`local_68[...] == 0`), but then
stores `local_8a = local_ac`, the row index, as the profession to elect. The
row index is read as a profession id, so the Gunsmith row elects Farmer (0),
the Blacksmith row Sugar Planter (1), and so on.

**Effect:** when a craft chain is owned, its input is produced and nobody works
that job, the AI colony sets a random eligible colonist to an unrelated
profession (0..5) instead of the craft expert. Saved profession bytes and the
AI's later job choices follow.

**Port (reproduces it):** `ai_euro_5952_specialist_arms` ARM 1
(`src/core/ai_euro_colony_jobs.c`, `target = r;`). The fix is
`target = k_5952_craft[r].job;`.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_independence`
and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-18).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-19 — Euro AI expert purchase charges the price of the colonist slot
**DOS:** FUN_5952_035e ARM 2 (raw 95919-95956). The gate compares the nation's
gold with the job-0 price (`DS:0x8ea8`, Farmer). After the expert is set, the
charge reads `*(long*)(local_30*8 - 0x7158)` (raw 95949), the price row of
`local_30`, the colonist slot, instead of the row of the trained job
(`local_ee`). The slot is 0..n-1 within the colony, so the AI pays the price of
Farmer, Sugar Planter, ... by seat number. A catalog row with price -1 makes
the charge a gain of 1 gold.

**Effect:** the AI treasury moves by the wrong amount on every expert purchase
(a colony's first seat is always charged the Farmer price). Later AI buying
and the nation's gold in saves follow.

**Port (reproduces it):** `ai_euro_5952_specialist_arms` ARM 2
(`src/core/ai_euro_colony_jobs.c`, `ai_euro_5952_job_price(last)`). The fix is
`ai_euro_5952_job_price(trained)`.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`,
`golden_idle_reload`, `golden_idle_america` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-19).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-20 — Euro AI starter-horses arm reads the unit before the tile head
**DOS:** FUN_5952_035e (raw 95959-95972, asm 2a03-2a63). The arm buys two
Horses for 10 gold when the colony holds under two and the turn is past 39. It
takes the tile head from FUN_281f_07e0 and indexes the unit table with it
without checking for -1 (no unit on the colony tile). Unit -1 is read at
`-1*0x1c + 0x3146` = DS:0x312a, a terrain-table byte (24). The cargo capacity
test then reads `DS:0x5237 + 24*0x0e` = DS:0x5387, the tutorial flag byte.

**Effect:** an AI colony with an empty tile gets the two Horses (and pays the
10 gold) only if the tutorial flag is set; otherwise it is skipped. A tile
with a unit works as intended. Observed in DOSBox (1592 Curacao).

**Port (reproduces it):** `ai_euro_5952_seed_horses`
(`src/core/ai_euro_colony_jobs.c`, the `else` branch of the head test). The fix
is to skip the arm when the tile has no unit.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`
and `golden_idle_reload`, and `unit_ai_euro_5952_build`
(`case_starter_horses_purchase` pins the empty-tile purchase); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-20).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-21 — Euro AI Europe recruit loop upgrades to Dragoon without paying for the horses
**DOS:** FUN_521d_5d04, the colony-demand recruit loop (raw 92950-92971). The
recruit's horse stock cost is paid only if the nation can afford it
(`gold >= local_1a`, raw 92956-92963). The Dragoon upgrade (`unit type = 4`,
raw 92964) and the horse-bank bookkeeping (the DS:0x84fc `+0x4a` counter,
lowered by 0x32 or reset through FUN_291f_0c14, raw 92965-92971) sit after the
closing brace, so they run even when the nation could not pay. The training
arm of the same function (raw 92721-92735) keeps both inside the gate.

**Effect:** a poor nation still gets a mounted recruit and uses up its horse
bank without paying. Unit types in saves, the unit-type counts that feed later
hire decisions, and the bank counter follow.

**Port (reproduces it):** `ai_euro_5d04_hire_tail_colony_demand`
(`src/core/ai_euro_europe.c`, after the `nat->gold >= horse_stock_cost` test).
The fix moves `ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 4)` and the
bank update inside that test.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`,
`golden_idle_reload`, `golden_idle_america` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-21).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-22 — Euro AI emergency lumber is credited even when the 200 gold is not paid
**DOS:** FUN_5952_035e (raw 94691-94699), the lumber emergency: no lumberjack
placed, colony lumber under 2, `turn & 7 == 0`. The code adds 100 to the
colony's lumber stock first, then debits 200 gold from the nation only if the
treasury holds at least 200. The two steps are not joined: the stock rise
sits outside the affordability test.

**Effect:** a nation with under 200 gold still gets 100 lumber for its colony
for free, one turn in eight, until the colony holds 2 or more.

**Port (reproduces it):** `ai_euro_5952_lumber_purchase`
(`src/core/ai_euro_colony_jobs.c`). The fix credits the lumber only inside the
`gold >= 200` branch.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`
and `golden_idle_reload`, and `unit_ai_euro_5952_lumber` (`purchase_gates` pins
the free lumber); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-22).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-23 — Euro AI equip arm strips the expert profession of the wrong colonist
**DOS:** FUN_5952_035e equip arm (raw 94346-94350, asm 5952:0f42-0f6d). The
scoring loop reads each colonist's profession into `local_18` and keeps the
best slot in `local_34`. After the loop the strip test does not use the picked
colonist: it pushes `local_18`, the profession of the last slot the loop
visited, to the expert test (FUN_281f_0c9a) and compares it with the target
(`local_1b4`). If that last colonist is an expert of another job, the picked
colonist (`local_34`) is set to Free Colonist (FUN_281f_0cae); otherwise it
keeps its profession.

**Effect:** an expert picked to become a Scout, Pioneer or Soldier is stripped
(or not) depending on the last colonist in the colony, not on itself. The
ejected unit carries the wrong profession byte, which shows in saves.

**Port (reproduces it):** `ai_euro_5952_equip_arm`
(`src/core/ai_euro_goals.c`, `last_prof` before the strip). The fix tests the
picked colonist's own profession.

**Blocks removal:** tried 2026-10-22 (test `colonists[pick].profession`); fails
all six idle gates; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-23).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-24 — Euro AI pioneer-clear ring scan counts water twice
**DOS:** FUN_5952_035e, the colony ring scan that decides the "wants Pioneer
clearing" flags (raw 94091-94115). A water plot (terrain class 0x19 or 0x1a)
adds 1 to the unproductive count (`local_144`) and to the good-food count
(`local_e`). The next test is a plain `if`, not an `else`: water is outside
the forest ranges, so it also runs the land branch. That branch reads the
terrain table food byte and adds another 1 to `local_144` when it is under 2,
or another 1 to `local_e` when it is 3 or more.

**Effect:** a coastal colony counts each water plot twice. The
"unproductive plots >= ring - 1" test (raw 94200) and the good-food test (raw
94203) then fire at the wrong times, so the AI colony sets or misses the
Pioneer-work/clear flag bit 0x20 in its saved AI flag byte, and the later
Pioneer and clearing choices follow.

**Port (reproduces it):** `ai_euro_refresh_colony_ai_flags`
(`src/core/ai_euro.c`, the land-branch `if` after the water block). The fix
makes it an `else if`.

**Blocks removal:** tried 2026-10-22 (`else if`); fails all six idle gates;
reverted. [Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-24).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-25 — Euro AI hull cargo buy checks gold against the last hold removal
**DOS:** the departing-hull cargo buy in FUN_521d_5d04 (raw 93041-93047) checks
affordability as `price * DS:0x8dc4`, then buys a literal 100 with
FUN_291f_0d8e. DS:0x8dc4 is the amount of the last hold slot removed anywhere
(FUN_15eb_317c), by any nation, so it has nothing to do with this purchase. The
check was meant to be `price * 100`.

**Effect:** whether a Euro AI hull in Europe buys a cargo lot depends on what
some earlier unload or sale happened to move, not on whether the nation can pay
for the 100 units. After a small removal the gate passes lots the nation
cannot pay for; after a large one it blocks lots the nation could pay for
(seen in DOSBox 1591, [evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-25)).

**Port (reproduces it):** the buy loop in the 5d04 departing-hull stage
(`src/core/ai_euro_europe.c`, `nat->gold >= price * units_last_hold_amount`,
bug #1029). The fix is `price * 100`.

**Blocks removal:** tried 2026-10-22 (`price * 100`); fails all six idle gates;
reverted.
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-26 — Euro AI Scouts and Colonists enter villages whatever the tribe's anger
**DOS:** FUN_521d_20e6 computes `local_54 = FUN_281f_0a60(alarm)`, where
FUN_15dc_00a2 turns the tribe's alarm toward the nation into a quartile 0..3
(raw 88443; asm `CMP [BP-0x52],0x19`). The village arms then compare that
quartile with alarm-sized constants: the Scout arm needs `local_54 < 0x19`
(raw 89065), the Colonist arm rejects on `0x18 < local_54` (raw 89368). The
quartile never exceeds 3, so the Scout test is always true and the Colonist
rejection never fires. The code meant the raw alarm (under 25 for a Scout, 25 or
more rejects a Colonist). The same variable is compared with 3 at raw 90189,
which is correct for a quartile.

**Effect:** AI Scouts and Colonists walk into villages to speak with the chief
or live among the natives however angry the tribe is with their nation.

**Port (reproduces it):** `ai_euro_20e6_village_arm` (`src/core/ai_euro_land.c`)
omits the alarm gate as "vacuously true".

**Blocks removal:** tried 2026-10-22 (gate both arms on the raw alarm); fails
`golden_idle_continuous`, `golden_idle_reload`, `golden_idle_independence` and
`_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-26).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-27 — Adjacent-foreign probe overwrites its reference medium
**DOS:** FUN_1427_09dc (raw 7927-7968) answers "is a foreign unit, colony or
village next to this tile". It keeps the centre tile's medium (land or water) in
`local_8` and compares each neighbour's medium with it. When a neighbour is a
settlement (FUN_137f_03e4 >= 0) it stores that neighbour's medium into `local_8`
(asm 1427:0a73). That makes the settlement neighbour's own medium test vacuous (a
Man-O-War at sea beside a village counts, DOSBox tr10 turn 276) and leaves
every following neighbour compared with the settlement's medium instead of the
centre's. The sibling FUN_1427_0b08 (raw 7997-8030) keeps the centre medium
fixed.

**Effect:** which neighbours count depends on the scan order of the 8 tiles
and on whether an earlier neighbour was a settlement. The probe feeds the Brave
destination score, the Euro 20e6 entry gate (`ai_euro_act.c`) and the Crown war
gate (`ai_king_war.c`).

**Port (reproduces it):** `ai_021a_adjacent_foreign` (`src/core/ai_native_021a.c`)
and `ai_euro_20e6_adjacent_foreign_09dc` (`src/core/ai_euro_land.c`). The fix is
to keep the centre medium fixed.

**Blocks removal:** tried 2026-10-22 (the centre medium stays fixed); fails
`golden_ai_turns`, `golden_ai_joint`, all six idle gates and `unit_ai_euro_20e6`
(`unit_adjacent_foreign_domains`); reverted. A narrower fix (settlement
neighbours still always pass, only the carry-over of the overwritten medium to
later tiles is dropped) passes every test, but leaves the ship-beside-a-village
case as DOS has it. [Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-27).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-28 — Village "last bought" reset keyed on the unit slot, not the cargo
**DOS:** in the native trade session, the sell settle (FUN_4d56_2bbc, raw
82704) and the gift settle (FUN_4d56_306c, raw 83052) store the tribe's `+8`
"last bought" byte as `0xff` when `[BP+6] == 0xf || [BP+6] == 8`, else as the
cargo (`[BP-0xc6]`). `[BP+6]` is FUN_4d56_2820's `param_1`, the acting unit's
index into the unit pool, not the cargo. The constants 0xf and 8 are Muskets and
Horses: the intent is that a tribe which just bought muskets or horses stays
willing to buy them again.

**Effect:** the tribe forgets its last purchase when the trading unit sits in
pool slot 8 or 15, whatever the cargo; and it keeps refusing (@BADCARGO) a
repeat sale of Muskets or Horses. Affects human and AI village trade.

**Port (reproduces it):** `ai_contact_2820_sell_settle` and
`ai_contact_2820_gift_settle` (`src/core/ai_contact_trade.c`), with the unit's
pool slot as the proxy (port_bugs.md #803). Pinned by
`test_803_last_bought_keyed_on_unit_slot` in `unit_ai_contact_colony`.

**Blocks removal:** tried 2026-10-22 (test the cargo); fails
`golden_idle_independence` and `_reload`, and `unit_ai_contact_colony` (#803
test); reverted. [Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-28).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.
With the fix, a Horses/Muskets sale leaves `last_bought = 0xff`, so the post-sale
@BRING (built from pre-sale asks) would name the cargo just sold. Bought horses
breed into the tribe's top sell good within a few turns, after which the village
refuses horses with @BADCARGO. So the fixed path also drops the sold cargo from the
@BRING wants (2026-11-03, `ai_contact_2820_buy_phase`), as DOS's
`last_bought = cargo` does for every other sale.

### TB-29 — Village colony-cover mask uses the wrong coordinate frame
**DOS:** FUN_4d56_2154 (raw 81816, asm 4d56:21b2-2227) builds a 5x5 mask of
village-local cells that a nearby colony already works. For each colony and each
village-local cell `(lx, ly)` it computes the cell's colony-relative ring index
`(local_6, local_5c) = (village - colony + l)`. It then passes the delta
`local_6 - 2, local_5c - 2` to FUN_281f_0302, the absolute map-interior test
(`1 <= x < width-1`), and passes the village-local `(lx, ly)` both to
FUN_281f_0ce0 (the colony ring-slot worker lookup, which wants colony ring
indices) and to the colony's own-tile test (`lx == 2 && ly == 2`). The intended
code uses the cell's absolute position for the first and `(local_6, local_5c)`
for the other two.

**Effect:** only cells at least one row and column south-east of the colony
can pass the bounds test, and the ring lookup reads the wrong slot, so most
colony-worked land is not excluded from the village's terrain scoring. This
feeds village bids and asks, gift and beg triggers and the Live Among skill
pick, for human and AI colonies.

**Port (reproduces it):** `ai_contact_meet_economics_2154`, Phase 1
(`src/core/ai_contact_demand.c`). Evidence: DOSBox independence 4->5.

**Blocks removal:** tried 2026-10-22 (the fix); fails all six idle gates;
reverted. [Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-29).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-30 — Brave "stayed" facing 8 fed to the adjacent-direction helper
**DOS:** the Brave step scorer FUN_4d56_021a (OVL13 0xc8f-0xcb9) adds +3 to a
direction that is one step either side of the unit's facing byte (`unit+0x314f`)
by calling FUN_124c_013c (raw 2833-2843, `(facing+1)&7 == d || (facing-1)&7 ==
d`). A Brave that stayed last time has facing 8 (written by 021a:11ae), and the
call has no `facing < 8` guard, so 8 reads as 0: it favours directions 1 (NE)
and 7 (NW) by +3. The Euro scorer at 521d:54f5 skips the facing term for any
byte >= 8.

**Effect:** a Brave that stayed a turn leans NE / NW on its next move instead
of scoring all directions equally. Affects every Brave step choice, so the
saved Brave positions and RNG stream.

**Port (reproduces it):** the facing / road / river block of the Brave step
scorer in `src/core/ai_native_021a.c` (`facing + 1` / `facing - 1` test, no
`facing < 8` guard).

**Blocks removal:** tried 2026-10-22 (skip the +3 term when `facing >= 8`);
fails all six idle gates; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-30).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-37 — REF wave planner tests the owner of a stale unit index
**DOS:** while scoring each human coastal colony's garrison, FUN_43f7_0982 (raw
74033-74041) walks the colony's eight neighbours and, for each land tile, tests
`unit[local_4c].owner == crown` before re-reading the tile's stack with 07e0.
`local_4c` is not the neighbour's top unit: it holds whatever the last
assignment left, first the head of the last colony tile scored, later where the
previous chain walk stopped. When it is -1 the read lands on DS:0x312b, which
held 0 in DOSBox. The test was meant to ask whether this neighbour tile's own
top unit belongs to the crown.

**Effect:** whether the crown's units next to a target count against its
garrison need depends on an unrelated stale index. The King's second and later
invasion waves are sized by it.

**Port (reproduces it):** `ai_king_0982_invasion` (`src/core/ai_king_ref.c`,
`last_tile_head_id`).

**Blocks removal:** tried 2026-10-22 (test the neighbour tile's own head unit);
fails `golden_idle_independence` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-37).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-38 — REF landing sweep tests "crown-held" through a stale owner nibble
**DOS:** the landing sweep of FUN_43f7_0982 (raw 74190) decides whether a
neighbouring land tile is already crown-held with `FUN_281f_06dc(x, y) ==
crown`, the layer3 owner nibble. The unit mover FUN_281f_023a sets the nibble
when a stack arrives but never clears it when the tile empties. The test was
meant to ask "is a crown unit standing here"; a stale crown nibble answers yes
for an empty tile.

**Effect:** the King's landing party treats tiles its units once stood on as
occupied. Cavalry and Artillery land alone on such an empty tile, and the
two-sweep Regular seizure logic (smallest enemy stack first) skips it.

**Port (reproduces it):** `ai_king_0982_land_troops` (`src/core/ai_king_ref.c`)
tests `units_tile_owner_nibble(...) == crown`.

**Blocks removal:** tried 2026-10-22 (test for an active crown unit on the
tile); fails `golden_idle_independence` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-38).
**Status:** tolerated (2026-10-22). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-50 — An AI hull sailing for Europe stays the current unit
**DOS:** the movers FUN_465b_0000 (raw 76484) and FUN_479b_0972 (raw 77099)
make the hull the current unit DS:0x5392 before FUN_291f_0208 sends it to
Europe. Nothing clears it until the human's Move Pieces entry, so the human's
autosave names the last AI hull that sailed (save header byte 0x22).

**Effect:** stale RAM in the save; nothing in play reads it before the human's
turn resets it.

**Port (reproduces it):** `ai_euro_ship_enter_europe`
(`src/core/ai_euro_ship.c`) sets `units.selected_id`.

**Blocks removal:** tried 2026-10-23: without the write every idle golden and
its reload twin fail (8 tests), on header byte 0x22 only (port 0xff, DOS the
hull's index); no other byte drifts. Removing it needs that byte on the idle
golden ignore list ([idle_campaign.md](idle_campaign.md)).
**Status:** tolerated (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-81. Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-52 — An AI that breaks a treaty by attacking never goes to war
**DOS:** FUN_465b_0000 raw 75558-75595. When a Euro attack meets the treaty bit
0x40 in either relation row, a human attacker gets @CANCELPEACE and the
target's row gains WAR (2). A computer attacker gets @DECLAREWAR (plus @SNEAK
for a human defender) but no WAR write on either side. Then 0x40 is cleared
both ways and the attacker's 0x01 dropped. Read from the decomp only; the
DOSBox evidence for "no WAR write" (independence 4->5) covers an attack with no
treaty. An AI attacks a treaty partner once its cooldown (nation +0x40+peer)
reaches 0; the path-step mover does it (FUN_479b_0972, campaign8 CN8-O17).

**Effect:** an AI unit attacks a treaty partner's colony or unit, the player is
told "now at war", and neither row carries WAR: the pair sits at neither peace
nor war. If the attack takes the colony, TB-32's capture tail sets WAR only in
the loser's row toward the captor, so the AI side still has no WAR. AI against
AI behaves the same way.

**Port (reproduces it):** `ai_diplo_465b_band_ctx` (`src/core/ai_diplo.c`, the
AI-attacker @DECLAREWAR arm), called from `units_move_combat_pre`
(`units_move.c`), `ai_euro_try_attack` (`ai_euro_land.c`) and `ai_contact.c`.
The fix is `ai_diplo_or_both(col1, attacker, defender, AI_DIPLO_WAR)` in that
arm.

**Blocks removal:** tried 2026-10-27 (that one line); fails
`golden_idle_america`, `golden_idle_america_reload` and `golden_ai_joint`
(`unit_ai_contact_core` too, which locks the DOS read); reverted.
**Status:** tolerated (user, 2026-10-27). Fixed in opencol 2026-10-27, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.

### TB-56 — 5d04 Pioneer training leaves the tools byte stale
Was dos_bug_candidates_2026-10-22.md C-71.
**DOS:** `FUN_521d_5d04` raw 92770-92783 turns a dock Colonist into a Pioneer
(dispatch byte 2) and books 100 tools on the market (`291f_0c14(0xe, 100)`),
but writes only the type: +0x3159 keeps whatever the record held (usually 0),
and `FUN_479b_0158` then wraps it to 236.. on the first improvement (#628, #636).

**Effect:** AI Pioneers bought this way start with 0 (or residue) tools although
the nation paid for 100.

**Port (reproduces it):** `ai_euro_5d04_cb_set_unit_dispatch_byte`
(`src/core/ai_euro_europe.c`), value 2 arm. The fix sets `tools = 100`.
Test: `unit_ai_euro_5d04_hire` `pioneer_training_tools`.

**Blocks removal:** tried ungated 2026-11-03: fails `golden_idle_continuous` and
`golden_idle_reload`.
**Status:** tolerated (user, 2026-11-03). Fixed in opencol 2026-11-03, gated on `!dos_buggy_behavior()`; test binaries still reproduce DOS.
