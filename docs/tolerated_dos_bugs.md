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
the next hull. Evidence: idle independence seed-100 1->2, at (1,53) Galleon 109
keeps four Dragoons, takes unit 58's two, and leaves its own Artillery and
Colonist (DOSBox BPM trace 2026-10-16).

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

**Blocks removal:** the idle independence campaign (CTest
`golden_idle_independence`, from save 1->2) depends on the DOS pickup order;
boarding only the hull's own riders changes which units sail and breaks the
DOS match.
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

**Blocks removal:** tried 2026-10-21: with the fix, CTest
`golden_idle_continuous`, `golden_idle_independence`, `golden_idle_america`
and the three `_reload` variants fail (seed-100 1492 campaign first differs at
1535, America at save 29), and `unit_ai_euro_28c8_job_score` fails.
`golden_ai_joint` passed. Reverted.
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

**Blocks removal:** tried 2026-10-21: scoring the offered goods' price bytes
(same first-strict-max rule) makes CTest `golden_idle_independence` and
`golden_idle_independence_reload` fail from save 4 (an AI wagon trade at a
village, independence 4->5). All other tests and `golden_ai_joint` passed.
Reverted.
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

**Blocks removal:** tried 2026-10-21: counting every settlement per continent
makes all six idle CTest gates fail from the first compared save (1493,
independence save 2, America save 2). The table is in the save, so it differs
byte-wise at once, and the AI's colony planning follows. `unit_ai_indian_census`
also fails; `golden_ai_joint` passed. Reverted.
**Status:** tolerated (2026-10-21).
