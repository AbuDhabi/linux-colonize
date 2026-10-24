# DOS bugs tolerated for now

STATUS: living record, not an audit dump. Authoritative.

VICEROY.EXE behaviour that is a bug in the original, that the user has judged
undesirable, but that the port **still reproduces for now** so development can
continue (typically because the golden fixtures and idle campaigns that guard
DOS fidelity depend on it). Each entry is due to move to
[dos_bugs.md](dos_bugs.md) (bugs the port does not reproduce) once the port
stops reproducing it. Add an entry only on the user's say-so; an agent must not
decide this alone.

Not this file:

- DOS bug the port already does not reproduce: [dos_bugs.md](dos_bugs.md).
- Port diverges from DOS but DOS is *right*: [known_divergences.md](known_divergences.md).
- Actionable port bug: [port_bugs.md](port_bugs.md).

TB-9..TB-30 (Euro and native AI) live in
[tolerated_dos_bugs_ai.md](tolerated_dos_bugs_ai.md); every other entry is here.
Ids are `TB-N`, permanent and never reused. An entry moving to `dos_bugs.md`
gets the next `DB-N` there and notes "was TB-N"; leave a one-line stub here
pointing at it so citations still resolve. Each entry names the DOS function
(`FUN_ssss_oooo` + raw offset), what DOS does, the port sites that reproduce it,
and what blocks removing it.

## Entries

### TB-1 — Coarse connectivity planes drop some NE/SW links
**DOS:** `FUN_67f4_0088` builds the land (`DS:0x85e8`) and sea (`DS:0x86f6`)
coarse connectivity planes: 15×18 cells of 4×4 tiles, one direction bit per
neighbouring cell that is reachable within the hop cap. For each cell it tests
directions 0..3 through `FUN_6662_0906` -> `FUN_6662_00f2` and accepts a link
only when `0 < cost < 8` (raw 105074-105075). `00f2` reuses its last
destination flood (`DS:0x2d1a/0x2d1c`, grid at `DS:0xa270`) when the
destination is unchanged and `grid[from] != 0`; it then leaves `DS:0xa370` at 0,
and `0906` returns 0. So a NE test whose destination is the one the previous
E/SE test flooded is rejected even when the cells are connected, and its
mirrored SW bit on the neighbour is never set. Bits are only ever missing, never
spurious. smcol (`supplemental-info.md`) records the same NE/SW mis-fill.

**Effect:** only the far tier of the Go To pathfinder (`FUN_6662_0f74` ->
`FUN_6662_09ae` snap / coarse BFS) reads the planes: long-range Go To for human
units, Euro AI and braves. Usually the coarse BFS detours around the missing
diagonal (N then E), giving a different waypoint and a slightly slower route.
Where two areas are joined only through that diagonal (a diagonal strait or
isthmus), the coarse BFS sees them as disconnected or far apart and the unit
falls back to the scored 8-neighbour step: odd detours, back-and-forth
wandering, or a Go To that never arrives. Frequency on real maps not measured.

**Port (reproduces it):**
- Saves: `col1_post_map_rebuild_connectivity` (`src/core/col1_post_map.c`,
  `PostCheapCache`) writes the planes byte-exact, quirk included (COLONY00
  planes match), so DOS loading a port save reads what it would have built.
- Live pathing: `units_coarse_build` (`src/core/units_move.c`) builds the
  port's own coarse grid through `units_flood_0906` on the shared `00f2` grid,
  which has the same reuse.

**Blocks removal:** dropping the reuse changes far-tier Go To routes, so the
idle campaigns (CTest `golden_idle_*`) and goldens stop matching DOS. Removing
it from live pathing only (keep the save planes DOS-exact) needs a way to keep
those gates meaningful, e.g. a switch the golden harnesses turn off.
**Status:** tolerated (2026-10-21).

### TB-2 — Passengers switch ships when another own ship leaves the tile first
**DOS:** a passenger is not "aboard" anything; it is a sentried land unit in
the tile's unit chain. When a ship leaves a tile, `FUN_1427_10be` (raw
8606-8680) sorts the tile with `FUN_1427_04d6` (hulls on top, then Treasure,
then @UNIT size 6..1, each bucket in chain order) and boards every same-nation
land unit that fits the hull's room, charging each its @UNIT size column
(`FUN_4720_00e0`, raw 74655-74661). Riders of other ships on the tile are in
that chain too, so whichever ship moves off a shared tile first takes them,
first come, first served, and may leave some of its own passengers behind for
the next hull. DOSBox-confirmed (independence seed-100 1->2,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-2)).

**Effect:** with two or more own ships on one tile (colony or open sea), units
loaded on one ship can sail off on another, and a ship can depart without some
of the units the player loaded on it. Affects human players and the AI alike.

**Port (reproduces it):** `units_ship_departure_pickup`
(`src/core/units_cargo.c`, declared in `units_cargo.h`) walks the hull's own
riders, sentried land units on the tile and riders of other own hulls there in
the 04d6 order; called from the ship move in `src/core/units_move.c` before the
group moves. The port keeps an explicit `aboard_ship_id`, so this is a
deliberate re-creation, not a side effect. Unit test: `unit_units_core`
`ship_switch_quirk`.

**Blocks removal:** boarding only the hull's own riders breaks
`golden_idle_independence` from save 2, which depends on the DOS pickup order.
**Status:** tolerated (2026-10-21).

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
**Status:** tolerated (2026-10-21).

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
**Status:** tolerated (2026-10-21).

### TB-5 — Colony job scoring reads Horses for the Fisherman
**DOS:** FUN_15eb_28c8 (the work-plot job scorer) calls
`FUN_15eb_18ec(x, y, &local_24, 0)` (raw 13000). 18ec writes the job index into
`local_24` and remaps a water job to Food only when `param_4 != 0` (raw 11983),
so for the Fisherman (job 8) `local_24` stays 8, and every later use reads cargo
slot 8 = Horses: the warehouse-room clamp (`colony+0x9a+8*2`, raw 13004-13011)
and the shortfall / unmet rows (`DS:-0x71ce` / `-0x71a6`). Jobs 0..7 map to
cargo 0..7 identically, so the Farmer correctly reads Food.

**Effect:** a Fisherman plot's yield is clamped by the room left for Horses,
not Food, so a colony with a nearly full Horses stock undervalues fishing; and
a food shortfall boosts Farmer plots but not Fisherman plots (the food
emergency branch does not read the cargo and still favours both). 28c8 places
AI colonists every tick and seats every new colonist joining a colony, human
or AI (`colonies_seat_new_colonist`).

**Port (reproduces it):** `ai_euro_28c8_score_full`
(`src/core/ai_euro_colony_jobs.c`, `const int cargo = job;`). Came from
port_bugs.md #582. The fix is `colony_yield_job_cargo(job)`.

**Blocks removal:** tried 2026-10-21 (the fix); fails all six idle gates
(`golden_idle_continuous`, `_independence`, `_america` and the `_reload`
variants) and `unit_ai_euro_28c8_job_score`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-5).
**Status:** tolerated (2026-10-21).

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
**Status:** tolerated (2026-10-21).

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
**Status:** tolerated (2026-10-21).

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
**Status:** tolerated (2026-10-21).

### TB-31 — Best-defender picker scores against "unit #nation", not the attacker
**DOS:** FUN_5fef_1b0e calls FUN_281f_07e0 with the attacker's nation nibble
and leaves that pushed word on the stack (`PUSH [BP-0x86]`, OVL17 asm
0x1baa-0x1bc1). It then pushes 07e0's unit and near-calls the defender picker
FUN_5fef_0000, so the picker's third argument, meant to be the attacker unit,
is the attacker's nation number (raw 99111-99205). The picker reads it as a
unit index: every candidate is scored against unit record #nation (FUN_281f_09dc
at raw 99157), and the Artillery-in-colony arm tests that record's nation
nibble.

**Effect:** which unit defends a tile or colony depends on whichever unit
record sits at index = attacker nation (usually a native Brave for nations
0..3), not on the real attacker. Every game, human and AI.

**Port (reproduces it):** `units_best_defender_at`
(`src/core/units_combat.c`, `pick_foe_id` / `pick_foe_nat`). Came from
dos_bug_candidates_2026-10-22.md C-35. The fix is `pick_foe_id = attacker_id`.

**Blocks removal:** tried 2026-10-22 (score against the real attacker); fails
`golden_idle_independence` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-31).
**Status:** tolerated (2026-10-22).

### TB-32 — Colony capture toggles the WAR bit one-sidedly
**DOS:** the capture tail of FUN_5fef_1b0e (raw 100999-101010, OVL17 asm
0x2f17-0x2f43) reads: if the captor's relation row toward the loser has no WAR
bit (2), set WAR in the loser's row toward the captor; else clear WAR in the
captor's row. The else arm is a toggle: capturing a colony while already at
war clears the captor's own war flag toward the loser. Intent is probably
"a capture means war", so the else-clear looks like a slip. DOSBox-confirmed
(independence 3->4, [evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-32)).

**Effect:** after a capture, the captor's WAR bit toward the loser is gone
whenever it was set before, so the pair is left at war on one side only. Every
capture, human and AI.

**Port (reproduces it):** `colonies_capture_col1_effects`
(`src/core/colony_build.c`, the `ai_diplo_read` / `ai_diplo_write` pair after
`ai_diplo_clear_both`). Came from dos_bug_candidates_2026-10-22.md C-36. The
fix is to drop the else arm.

**Blocks removal:** tried 2026-10-22 (drop the else arm); fails
`golden_idle_independence` and `_reload`; reverted.
**Status:** tolerated (2026-10-22).

### TB-33 — Spanish tech-1 conquest treasure bound is computed but not used
**DOS:** the treasure amount tail of FUN_5fef_31ea branches on the razed tribe's
tech. The tech-0 arm stores a Spanish-dependent bound in `-0x62` (raw 101414,
Spanish 3, others 6) and rolls `FUN_281f_04d4(0, bound)`. The tech-1 arm also
stores a Spanish-dependent bound in `-0x62` (raw 101433, Spanish 1, others 2)
but then rolls the literal `FUN_281f_04d4(0, 2)` (raw 101434-101436), so the
stored value is never read. The tech-0 arm shows the intent: the roll should
use the stored bound.

**Effect:** a Spanish conqueror of a tech-1 tribe gets the treasure roll
"0 of 0..2" (1 in 3) instead of "0 of 0..1" (1 in 2), and
the paid amount and RNG stream differ. Needs a tech-1 tribe (non-rich, no Cortes) to matter.

**Port (reproduces it):** `units_conquest_treasure_gold`
(`src/core/units_combat.c`, tech-1 arm). Came from
dos_bug_candidates_2026-10-22.md C-38. The fix is `dos_rng_range(rng, 0,
spanish ? 1 : 2)`.

**Blocks removal:** tried 2026-10-22 (roll with the computed bound); fails
`golden_idle_continuous` and `golden_idle_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-33).
**Status:** tolerated (2026-10-22).

### TB-34 — Ship-sink census does not decrement the ship count
**DOS:** when FUN_5fef_0352 sinks a hull (raw 99570-99579) it takes the hull
out of the Euro AI fleet census: it subtracts the ship's cargo from the cargo
total (`DS:0x9414`), decrements the per-type count (`DS:-0x6db4 + nation*0x13
+ type`) and, for an armed hull, the armed-ship count (`DS:0x9424`). It never
decrements the ship count `DS:0x9418`, which the unarmed-hull gate in the same
body reads as `0x9418 - 0x9424 < 9` (raw 99538-99540). The census is only
rebuilt by the next census pass (FUN_4962_0018), so until then a sunk ship still
counts.

**Effect:** the saved ship count (`DS:0x9418`, part of the census block in the
save) stays one too high for the rest of the turn after each sinking, and the
`< 9` fleet-size test in later sink decisions sees one ship too many. The
Euro AI Europe code that reads `ship_counts` (the "no ships" check) sees the
same stale count.

**Port (reproduces it):** `units_apply_naval_loss_outcome`
(`src/core/units_combat.c`, census block after `units_naval_damage_gate`).
Came from dos_bug_candidates_2026-10-22.md C-40. The fix is
`st->ship_counts[n]--`.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_independence`
and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-34).
**Status:** tolerated (2026-10-22).

### TB-35 — LCR "Vanishes" latch overrides the small-nation protection
**DOS:** in the Lost City Rumour body FUN_65dd_0004, case 5 ("Vanishes",
raw 103597-103612) first turns into 6 ("nothing") for a nation with census
pop < 5 and < 3 colonies, and half the time for a Pioneer with census pop < 9.
It then tests the one-shot per-nation latch (`nation + 0x543e` bit 0x40) without
checking that the case is still 5: if the latch is unset, DOS sets it and
changes the case to 4 (Burial Mounds). So the latch overrides the protections,
and a nation that was protected from Vanishes gets Burial Mounds instead of
nothing and spends its latch on it.

**Effect:** a small nation's first rolled case 5 pays out Burial Mounds even
when the protection said "nothing". Changes the LCR outcome, the RNG stream and
the saved latch bit. Every game, human and AI.

**Port (reproduces it):** `units_lcr_roll_outcome` (`src/core/units_combat.c`,
case-5 block; `lcr_case5_bonus_used`). Came from
dos_bug_candidates_2026-10-22.md C-41. The fix is to test the latch only while
`raw == 5`.

**Blocks removal:** tried 2026-10-22 (the fix); fails `golden_idle_continuous`,
`golden_idle_reload`, `golden_idle_america` and `_reload`, and `unit_units_core`
("case5 latch must set lcr_case5_bonus_used" pins the DOS behaviour); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-35).
**Status:** tolerated (2026-10-22).

### TB-36 — Declaration clears the seen plane with the crown index instead of its bit
**DOS:** FUN_43f7_1a26 (declaration of independence, raw 74818-74827) walks the
whole seen layer and ANDs every tile with `~*(int*)0x53d2`, the crown's slot
INDEX, then ORs `0x10 << crown` into tiles the human has seen. The mask meant
to be `~(0x10 << crown)` (the value `iStack_8` is computed one line above and
used for the OR). With the index the AND is a no-op for crown 0 and clears
low-nibble bits (the AI site-score nibble) for crowns 1..3, and the crown's own
seen bit is never cleared first.

**Effect:** tiles the crown had seen but the rebels had not keep the crown's
seen bit after the declaration; for crowns 1..3 the low-nibble bits are wiped
on every tile. The REF pathfinder reads this layer.

**Port (reproduces it):** the seen-plane sweep in the declaration step of
`ai_king_audience.c` (`keep = ~crown_fold`).

**Blocks removal:** tried 2026-10-22 (`keep = ~(0x10 << crown)`); fails
`golden_idle_independence` and `_reload`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-36).
**Status:** tolerated (2026-10-22).

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
**Status:** tolerated (2026-10-22).

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
**Status:** tolerated (2026-10-22).

### TB-39 — Invasion and intervention target lists are capped at ten colonies
**DOS:** the REF invasion wave (FUN_43f7_0982, raw 74001-74015) and the
intervention pick (FUN_43f7_10f0, raw 74312-74331) gather the human's coastal
colonies into a fixed ten-slot stack array. The gather loop takes a colony
only while `local_24 < 10`, so colonies past the tenth, in colony-pool order,
are never scored, weighted or picked.

**Effect:** once the human owns more than ten coastal colonies the King only
ever attacks or lands next to the oldest ten.

**Port (reproduces it):** `AI_KING_0982_MAX_TARGETS` and
`AI_KING_10F0_CANDIDATES` (`src/core/ai_king_ref.c`), both 10.

**Blocks removal:** tried 2026-10-22 (both caps lifted); fails
`golden_idle_independence` and `_reload`; reverted. The cap only matters past
ten coastal human colonies, so that campaign reaches it and DOS keeps it.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-39).
**Status:** tolerated (2026-10-22).

### TB-40 — Tax-audience score reads the human's SoL for every nation
**DOS:** the King's peacetime tax audience (FUN_38fd_5be8, raw 68460-68467) is
rolled for each nation at its end of turn, human or AI (the nation is the
bound index `DS:0x9e12`). Its score uses `*(int*)0x53d0` (the human's SoL
report) in the `(SoL * 2 - tax_rate) * 5` term, and the nation's own tax rate
and census population in the other terms. The per-nation SoL byte (`nation +
0x19`, same 0..100 scale) exists and is cached for every nation, so the term
was meant to read it.

**Effect:** an AI nation's tax audience is scored with the human's SoL, so AI
tax rates move as if they had the human's independence movement. Also, the
Simon Bolivar elect bump to `DS:0x53d0` counts in the human's score.

**Port (reproduces it):** `ai_king_audience_roll` (`src/core/ai_king_audience.c`,
`col1->head.rebel_sentiment_report`).

**Blocks removal:** tried 2026-10-22 (`nat->rebel_sentiment` in place of
`rebel_sentiment_report`); fails `golden_idle_continuous`, `golden_idle_reload`,
`golden_idle_america` and `_reload`, and `unit_ai_king_core` (6 cases pin the
old score); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-40).
**Status:** tolerated (2026-10-22).

### TB-41 — Fisherman yield cascade tests its thresholds in the wrong order
**DOS:** FUN_15eb_18ec (raw 11816-11835, asm 15eb:1979-19d7) adjusts a
Fisherman plot by the count `n` of Ocean/Sea Lane neighbours (FUN_15eb_173e).
The cascade is `n >= 8: -2; n >= 6: -1; n < 6: +1; n < 4: +2; n < 3: +3;
n < 1: +4`. The `n < 6` arm is tested before the smaller thresholds, so the
+2, +3 and +4 arms can never run. The intended order is clearly smallest
threshold first (sheltered water fishes better). The -2 arm is also dead in
play: a colony work plot always has the land colony tile as a neighbour, so
`n <= 7`.

**Effect:** every Fisherman, human and AI, gets only +1 (fewer than 6 ocean
neighbours) or -1 (6 or 7). A fishing plot in a sheltered bay yields the same
as one on an open coast.

**Port (reproduces it):** `colony_yield_fisherman_distance_mod`
(`src/core/colony_yield.c`). Came from dos_bug_candidates_2026-10-22.md C-47.
The fix is the cascade in the right order: `n >= 8: -2; n >= 6: -1; n < 1: +4;
n < 3: +3; n < 4: +2; else +1`.

**Blocks removal:** tried 2026-10-22 (the fixed cascade); fails
`golden_idle_independence` and `_reload`, `golden_colony_prod01`,
`golden_colony_prod02`, `unit_colony_yield`, `unit_turn_core` and
`unit_ai_euro_28c8_job_score`; reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-41).
**Status:** tolerated (2026-10-22).

### TB-42 — Major-river bonus only when the river is the sole contributor
**DOS:** FUN_15eb_18ec (raw 11961-11963) builds the road/plow/river bonus
`local_12` in steps of `u`. The major-river step is
`(local_28 & 0x40) && (local_12 += u, local_28 & 0x80) && local_12 == u`:
after the river's own `+u` the running sum is compared with `u`, so the major
bonus is added only when nothing else (Farmer `+u`, road, plow) came first.
A Farmer's `+u` is always there first, so a major river never gives a Farmer
the extra step. The condition looks like a slip for "river is major".

**Effect:** a major river adds its extra step only to a plot with no Farmer
step, no road or colony bit and no plow. A major-river Farmer, or a plowed or
roaded plot on a major river, yields the same as on a minor river.

**Port (reproduces it):** the improvement stack in `colony_yield_pipeline`
(`src/core/colony_yield.c`, `map_tile_has_major_river(...) && add ==
improvement_step`). Came from dos_bug_candidates_2026-10-22.md C-48. The fix is
dropping the `add == improvement_step` test.

**Blocks removal:** tried 2026-10-22 (the fix); fails all six idle gates,
`golden_colony_prod01`, `golden_colony_prod03` and `unit_colony_yield`
(`fur_trapper_river_double_count`); reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-42).
**Status:** tolerated (2026-10-22).

### TB-43 — Stale claim table charges again for a plot a Pioneer already bought
**DOS:** FUN_15eb_26e4 builds the 5x5 tribal-claim table (DS:-0x7262) at
colony-pass entry and already leaves out a plot whose purchased bit
(`FUN_15eb_0620`, mask `& 0x10`) is set. The seat (FUN_15eb_06d2 and the tail
of FUN_0000_6582) reads that table later and prices the plot with
FUN_4cc6_07c2, which never tests the purchased bit. If a Pioneer bought the
plot between the table build and the seat, the table still claims it and the
seat charges (AI colony with 1.5x the price in gold) or raises alarm (otherwise)
a second time. The seat never re-tests the bit the table builder used.

**Effect:** an AI colony can pay for, or anger the tribe over, a plot its own
Pioneer just bought. Seen in real DOS (America seed-100 save 12,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-43)).

**Port (reproduces it):** `colonies_assign_field_land_reaction`
(`src/core/colony_workers.c`) takes the entry from the claim snapshot
(`colonies_claim_snapshot_take`, `src/core/colony.c`) and prices it with
`colonies_indian_land_purchase_gold_stale_claim`, which skips the purchased
test on purpose. Came from dos_bug_candidates_2026-10-22.md C-50.

**Blocks removal:** tried 2026-10-22 (skip the reaction when the plot's
`MAP_LAYER2_PURCHASED` bit is set); fails `golden_idle_america` and `_reload`;
reverted.
**Status:** tolerated (2026-10-22).

### TB-44 — The destination flood cache is keyed on the goal only
**DOS:** `FUN_6662_00f2` (raw 103870-103885) reuses its last destination flood
(`DS:0x2d1a/0x2d1c`, grid at `DS:0xa270`) whenever the goal matches and
`grid[mover] != 0`. The key ignores everything else the grid depends on: the
mover's type (`DS:0x1dd2`: domain, low-move), the owner terms (`DS:0x1dd6`) and
the uniform-cost flag (`DS:0x1dd4`). Costs are bytes (they wrap past 255) and the
256-slot queue has no full test. Every 00f2 caller shares the grid: the Go To
tiers, `FUN_6662_0906` (snap probes, ship-pressure probe) and the road-connect
walk, so a flood made for one mover is reused for the next.

**Effect:** a Go To step can be chosen from a grid built for a different kind of
mover (a land probe's grid used by a ship, another nation's owner terms), so
the route can differ from the one the mover's own rules would give. TB-1 is a
special case (the coarse-plane populator).

**Port (reproduces it):** `units_flood_00f2` (`src/core/units_move.c`), shared
grid `s_flood`. The save-plane build (`col1_post_map_rebuild_connectivity`,
`src/core/col1_post_map.c`) has its own cache and is not affected by a change
here, so a live-only fix is possible. The live coarse build
(`units_coarse_build`) uses one rule set for all its floods, so a rules-aware
key also leaves it (and TB-1) unchanged.

**Blocks removal:** tried 2026-10-22 (store the mover's rules with the grid and
rebuild when they differ); fails `golden_idle_america` and `_reload` and
`regression_replay`. A variant that never reuses the grid also fails all six
idle gates and `unit_units_movement`. Reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-44).
**Status:** tolerated (2026-10-22).

### TB-45 — withdrawn: Man-O-War sight radius 1 is intended
FUN_13f1_02f8 gives the Man-O-War radius 1, not 2 like Galleon, Privateer and Frigate;
the user judged it intended (2026-10-22), so the port keeps it (`units_sight_radius`,
`src/core/units_map.c`). Was dos_bug_candidates_2026-10-22.md C-56.

### TB-46 — Map generator latitude abs() draws the random number twice
**DOS:** the base-terrain latitude band in `FUN_684c_08c0` (raw 105771-105787)
is `ABS(h/2 - rand(1,16) - y + 8)` written with a macro that evaluates its
argument twice. The sign test (`<= 0`, via `iVar12 + 8 < 0 || iVar12 == -8`)
uses one `rand(1,16)` and the value uses a second one, so every tile (water
included) consumes two draws and the value may be negated although its own
draw was positive.

**Effect:** the latitude band of a land tile (base terrain: Savannah ...
Plains, Tundra) is not the absolute value of one random offset: near the
sign flip the value from the second draw is negated by the first draw's test,
so some tiles get a band that no single draw would give. The map-generation RNG
stream also takes two draws per tile.

**Port (reproduces it):** `latitude_terrain` (`src/core/map_gen.c`), called for
every tile by `paint_terrain`.

**Blocks removal:** tried 2026-10-22, two variants: (1) one draw per tile,
`abs()` of it, fails only `golden_mapgen_seed100`; (2) both draws kept (stream
unchanged), the first used for sign and value, fails `golden_mapgen_seed100`
and `smoke_play`. Reverted.
[Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-46).
**Status:** tolerated (2026-10-22).

### TB-47 — Spawns refused at the unit cap still charge the cost
**DOS:** FUN_1427_06b4 refuses a new unit at the cap (292 units for an AI
nation or the natives, 300 in all, or a Euro census byte above 200). Five
callers ignore the refusal and charge anyway:
- colony unit project: FUN_364b_0688 debits the tools (raw 57771-57774), then
  FUN_364b_0114 (raw 56939-56950) bumps the unit-type count and the Artillery
  grace counter, zeroes the hammers and shows the "built" message;
- leave-as: FUN_15eb_1068 jumps to LAB_15eb_1236 (raw 11282-11283,
  11317-11331), which subtracts the target job's gear; the colonist stays;
- birth: FUN_364b_0688 (raw 57615-57622) debits 200 food, then shows
  @NEWCOLONIST;
- immigration: FUN_38fd_5e52 (raw 68579-68590) zeroes the crosses and draws
  and refills a recruit-pool slot before the spawn;
- Brewster pick: FUN_38fd_4884 (raw 64765-64768) zeroes the crosses before the
  spawn.

**Effect:** at the cap a colony or nation pays for a unit it never gets. A
human player loses stock or crosses visibly. AI nations reach their cap late
(America seed-100 around saves 181-199) and from then on keep losing food,
hammers, tools, gear and crosses.

**Port (reproduces it):** `colonies_try_complete_unit_construction`
(`src/core/colony_build.c`, `COLONIZE_UNIT_BUILD_REFUSED`),
`colonies_eject_colonist` (`src/core/colony_workers.c`, port_bugs.md #1132),
the birth block of `src/core/turn_production.c` (Phase I), the immigrant arm
of `turn_run_nation_ticks` (`src/core/turn_colony.c`) and
`units_brewster_apply_slot_w` (`src/core/units.c`).

**Blocks removal:** tried 2026-10-22 (check `units_spawn_room` before charging
at all five sites); every test passed, since no enabled golden reaches the cap.
Kept at the user's call because the real DOS saves past the AI cap show these
charges (America seed-100 resync 290, port_bugs.md #1132). Was DB-17..DB-21.
Reverted. [Details](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-47).
**Status:** tolerated (2026-10-22).

### TB-48 — Reversing a ship at sea keeps its crossing counter
**DOS:** the European Status drag handler 38fd:3228 (overlay segment 5,
ndisasm; dispatcher 38fd:4e8e, jump table 0x4f54, drag mode DS:0x9e3a == 9)
moves a ship between "Expected Soon" and "Bound for the New World" by
re-linking the hull and its passengers to the other lane tile and clearing its
orders byte +0x314c (38fd:326f). The crossing counter +0x315a is not touched.

**Effect:** a ship one turn from the New World, reversed, is one turn from
Europe (and the other way round): turning back costs the remaining turns of the
voyage, not the turns already sailed.

**Port (reproduces it):** `europe_reverse_transit` (`src/core/europe_harbor.c`),
port_bugs.md #1153; unit test `case_europe_reverse_transit_keeps_turns`,
regression `europe_reverse_sailing_ship`.

**Blocks removal:** nothing; an odd edge case, not worth fixing now (user,
2026-10-22).

### TB-49 — Fur Trapper gets the road and river bonus twice
**DOS:** FUN_15eb_18ec adds road-or-settlement (`FUN_137f_0142 & 0x0a`) +1 and
river +1 (+2 on a major river) for job 4 right after the base lookup (raw
11840-11848), before the SoL fold and the expert doubling, and the shared
improvement stack adds road and river again (raw 11945-11963).
The 2026-08-15 Hudson capture confirms DOS pays both.

**Effect:** a Fur Trapper on a road or river tile is paid for the road and
the river twice, once of it expert-doubled.

**Port (reproduces it):** `colony_yield_pipeline` (`src/core/colony_yield.c`), the
job-4 block before the zero-base gate.

**Blocks removal:** tried 2026-10-23: dropping the job-4 block fails every idle
golden and its reload twin (8 tests), `golden_colony_prod01` and
`unit_colony_yield`.
**Status:** tolerated (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-77.

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
**Status:** tolerated (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-81.
