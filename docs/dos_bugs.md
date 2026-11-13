# DOS bugs the port does not reproduce

STATUS: living record, not an audit dump. Authoritative.

VICEROY.EXE behaviour that is a bug in the original and that this port
**deliberately does not reproduce**. Fidelity to DOS is the default
([project_goals.md](project_goals.md)); an entry here is an explicit, user-approved
exception to it. Add a row only when the user has agreed the DOS behaviour is
undesirable — an agent must not decide this alone (every "stand-in" so far turned
out wrong; see [conventions.md](conventions.md)).

Not this file:

- Port diverges from DOS but DOS is *right* (data-model cost, capture still
  needed): [known_divergences.md](known_divergences.md).
- DOS bug that **is** ported literally and that nobody has judged unwanted:
  no entry anywhere; the `DOS-LITERAL` comment at the port site records it.
- Unwanted DOS bug fixed in opencol but still reproduced in test binaries
  (`dos_buggy_behavior()`, so the goldens hold): [tolerated_dos_bugs.md](tolerated_dos_bugs.md).
- Actionable port bug: [port_bugs.md](port_bugs.md).

Euro and native AI entries (Euro colony tick, ships, land units, AI diplomacy;
villages, Braves, raids) live in [dos_bugs_ai.md](dos_bugs_ai.md); screen, dialog,
popup, cheat and MAPEDIT entries live in [dos_bugs_ui.md](dos_bugs_ui.md). Ids are
shared across all three files.

Entries keep their `port_bugs.md #` id when they came from a bug row, so
citations still resolve; ids are permanent and never reused. Each entry names the
DOS function (`FUN_ssss_oooo` + raw offset), what DOS does, what the port does
instead, and the port site, so a later reader can tell "intentional" from
"drifted". Grep this file for a feature before "fixing" a port/DOS mismatch.

## Entries

### DB-1 — In-game load resumes the outgoing game's day on top of stale RAM
**DOS:** the in-game Load Game does not reset the running game. `FUN_130d_0290`
(year loop) finishes the *outgoing* game's day from the loaded slot's state
(extra production ticks before the first autosave), and tables the save never
carried keep whatever the previous game left in them (see entry DB-2). The result
depends on what was in memory before the load, so it is not reproducible and not
worth modelling. Evidence: [archive](archive/dos_bugs_evidence_2026-10-23.md#db-1).

**Port:** main-menu load and in-game load both run the same clean init
(`game_apply_col1_save`, `src/core/game_loop_saveload.c`): the world is rebuilt
from the file alone, nothing from the previous game survives.
**Status:** done (2026-10-18).

### DB-2 — Save files omit live state, so save+reload diverges from continuous play
**DOS:** the save (`FUN_75c2_0288`) does not write several tables the turn loop reads
(Euro AI stance `DS:0x9870`, the AI goal tables `DS:0x98b0..`, the Indian rank table
`DS:0x917c`, ...), so a game played straight through and the same game saved and
reloaded take different paths. [Evidence](archive/dos_bugs_evidence_2026-10-24.md#db-2).

**Port:** the `RTST` chunk of the port extension block (`COLNXEXT`; records listed in
[savegame.md](savegame.md) "Port extension block", `src/core/runtime_ext.c`) carries
every such table plus the port's LCG; `col1_bridge_apply` restores it. DOS loads such a
save (it ignores the tail) and drops the block on re-save.
**Status:** done (2026-10-19). Guarded by the `golden_idle_*_reload` CTests
([details](archive/dos_bugs_evidence_2026-10-24.md#db-2)).

### DB-3 — Independence rename overflows the country-name field
**DOS:** on @OTHERGRANTED (FUN_3844_0442, raw 58602) DOS `strcpy`s the republic name
into `player.country_name` (24 bytes, DS `nation*0x34 + 0x5426`) with no bound. "United
States of America" is 24 characters, so its NUL zeroes `player+0x30`
(`lcr_case5_bonus_used`, `named_new_world`). [Evidence](archive/dos_bugs_evidence_2026-10-24.md#db-3).

**Port:** `turn_year_end_rival_independence` (`src/core/turn_year_end.c`) fills the
field without a NUL at 24 characters and leaves `player+0x30` alone; readers go through
`col1_player_country_name` (`src/core/col1_save.c`).
**Status:** done (2026-10-21).

### DB-6 — Colony "out of ore" message tests net muskets against net tools
**DOS:** the Phase K colony messages in FUN_364b_0688 (raw 57696-57728) are seven
independent tests of the form `unmet[input] != 0 && 0b50(output) == 0` (0b50 = net stock
change). The @ORE arm (raw 57717-57724) instead tests
`unmet[ore] != 0 && 0b50(muskets) == 0b50(tools)`: a copy-paste slip from the @TOOLS arm
below it. So DOS stays silent when the blacksmiths ran the ore out and tools did not
grow while a gunsmith made muskets, and it nags whenever the two nets happen to be
equal, even when tools did grow.

**Port:** the @ORE arm in `turn_produce_crumbs` (`src/core/turn_production.c`,
Phase K table) tests `unmet[ore] != 0 && gross(tools) == 0`: the blacksmiths made no
tools. For the other six arms net output equals gross, because nothing consumes their
output. For tools it does not: an earlier `net(tools) == 0` test nagged every turn in
a colony whose gunsmith ate every tool the blacksmiths made (playtest, Fort Orange
2026-11-13), and stayed silent when the blacksmiths stood idle while the gunsmith
drew down the stock. Message chrome only: no stock, RNG or AI path reads it. Came from
port_bugs.md #911.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-6).
**Status:** done (2026-10-21; gross test 2026-11-13).

### DB-7 — Denounce Heresy tests Cotton Planter instead of Jesuit
**DOS:** Denounce Heresy (`thunk_FUN_1000_a594`, viceroy_overlays.asm 126507:
`IMUL BX,[BP+6],0x1c / CMP byte ptr [BX+0x315b],0x3`) gives the denouncer's side its
Jesuit bonus (weight ×2, alarm delta ×2) when the unit's profession byte is 3, the @JOB
row for Cotton Planter, not 0x18 (Jesuit Missionary) as the establish-mission body
(`thunk_FUN_1000_a5dc`) tests. A Missionary is never a Cotton Planter, so the bonus is
dead: a Jesuit denounces like a plain missionary, and a won mission is installed without
the Jesuit bit (0x10).

**Port:** `ai_contact_denounce_heresy` (`src/core/ai_contact_actions.c`) tests
`UNITS_JOB_MISSIONARY` (0x18), and a won mission takes the Jesuit bit on the establish
body's test (Jesuit or Brebeuf), so Brebeuf nations never get a plain mission from a
denounce. Came from port_bugs.md #558.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-7).
**Status:** done (2026-10-21).

### DB-9 — Jesuit "leave as Colonist" row keyed on the colony's record slot
**DOS:** FUN_15eb_3454 (raw 13561-13565) refuses a Jesuit Missionary's "leave as
Colonist" row when `*(int*)0x8dc6 < 4` and the control byte
`0x543f + *(int*)0x8dc6 * 0x34` is human, but `DS:0x8dc6` is the colony's record index,
not a nation: the refusal hits only colonies whose record slot equals a human nation's
number.

**Port:** `colonies_eject_row_offered` (`src/core/colony_workers.c`) drops the arm: any
Jesuit is offered the Colonist row (user decision 2026-10-21). Came from port_bugs.md
#559. [Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-9).
**Status:** done (2026-10-21).

### DB-11 — @TRAINFAIL names whatever colony the last message named
**DOS:** in the school pass of FUN_364b_0688, a teacher with no eligible student raises
@TRAINFAIL (`0xde7`, raw 57542), whose text names `%STRING0`. Token 0 is set to the
colony name only later, in the graduation arm (`FUN_281f_0416(0, colony+2)`, raw 57552),
and nowhere earlier in 0688. So the message shows whatever token 0 held from an earlier
message: another colony's name or an unrelated string.

**Port:** the @TRAINFAIL arm of the school pass in `src/core/turn_production.c` fills
`%STRING0` with the current colony's name. Human-only message chrome; no stock, RNG or
AI path reads it. Was the last bullet of known_divergences.md #909 (moved 2026-10-22).
**Status:** done (port behaviour predates the entry; recorded 2026-10-22).

### DB-12 — Europe Artillery price counter rises when the purchase is cancelled
**DOS:** FUN_38fd_4b50 (raw 64870-64875) computes the Artillery price as base +
`nation[0x1e]*100`, then increments `nation[0x1e]` before the @REALLYBUY popup is asked.
The payment and the unit spawn happen only on answer 1 (Yes). So a No or Esc still makes
the next Artillery 100 dearer, and the saved byte `nation+0x1e` drifts up on every
cancelled buy.

**Port:** `europe_purchase_commit` (`src/core/europe_dock.c`) bumps `artillery_bought`
(nation+0x1e) after the debit, so only a Yes raises the price.
`europe_purchase_open_confirm` no longer bumps it.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-12).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-1.

### DB-13 — Foreign-colony barter never debits the colony's offered goods
**DOS:** FUN_5f7a_020e (Jan de Witt barter at a foreign colony; raw 99019-99038). The
counter-offer is sized from the colony's own stock (`stock[c]`, capped at 100, then
walked down to the sold cargo's value). On "take the goods" the unit's hold is
overwritten with the offered cargo (FUN_281f_0cea / FUN_281f_0ca4) and the colony gets
`stock[sold] += qty`. The offered cargo is never subtracted from the colony, so every
barter creates goods and the AI colony keeps what it handed over.

**Port:** `colonies_foreign_trade_apply` (`src/core/colony_goods.c`) subtracts the
handed-over quantity from the colony's stock of the offered cargo on the "take goods"
arm, clamped at 0 (a War of Independence offer is forced up to 0x32 or 100 and can
exceed the stock). The gold arm is unchanged.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-13).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-2.

### DB-14 — Tea Party picks the cargo without using its random draw
**DOS:** FUN_38fd_3dc8 (raw 64176-64200) stores `rand(1, total)` in `local_84` and never
reads it; the weight walk starts from the total, so the pick is always the last stocked,
non-boycotted cargo with a nonzero weight instead of a weighted roulette.

**Port:** `ai_king_pick_dump_goods_cargo` (`src/core/ai_king.c`) walks from the draw, so
each eligible cargo is picked in proportion to its weight; with all weights zero the
draw picks uniformly among the eligible goods (user decision 2026-10-22).
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-14).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-3.

### DB-15 — @MORETHANTHREE cap skips the Rum Distiller
**DOS:** the work-assign validator thunk_FUN_1000_9808 (viceroy_overlays.c 60486-60496)
tallies every other colonist by occupation and refuses a fourth with
`if (2 < count[job] && 9 < job) return @MORETHANTHREE`. Indoor jobs start at 9 (Rum
Distiller), so `9 <` exempts it and the colony screen lets the player seat any number of
Distillers. Every other indoor building is capped at 3; `8 <` (or `>= 9`) was meant.

**Port:** `colonies_assign_workplace` (`src/core/colony_workers.c`) tests
`occupation > 8`, so the Distiller is capped at 3 like the other indoor jobs. The AI
placement has its own `< 3` check and is unaffected.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-15).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-4.

### DB-16 — withdrawn: King-gift ship loss tax cut is cut content
FUN_5fef_0352 (raw 99671-99690) reads the tax cut one table row too far, so
the King's gift Frigate never cuts the tax. The user judged it an intended
removal (2026-10-22, [why](archive/dos_bugs_evidence_2026-10-23.md#db-16)); the port keeps DOS
(`units_royal_loss_tax_cut`, `src/core/units_combat.c`). Was dos_bug_candidates_2026-10-22.md C-5.

### DB-17 — withdrawn: colony unit project at the unit cap, now tolerated

### DB-18 — withdrawn: leave-as gear at the unit cap, now tolerated

### DB-19 — withdrawn: colony birth at the unit cap, now tolerated

### DB-20 — withdrawn: crosses immigrant at the unit cap, now tolerated

### DB-21 — withdrawn: Brewster recruit at the unit cap, now tolerated
Each of these five 2026-10-22 fixes passed every golden but was reverted at the
user's call; the port reproduces DOS again: [tolerated_dos_bugs.md](tolerated_dos_bugs.md) TB-47.

### DB-24 — Founding join scores the forest penalty from stack residue
**DOS:** FUN_15eb_28c8 (raw 13119-13122) reads `local_32` for its forest penalty but
assigns it only in the food-emergency arm (raw 13037). In the founding join (after
FUN_364b_1ba8, raw 58088) the stack word is the colony x, not a terrain class, so the
founder never gets the forest -10. The tick's residue is tolerated_dos_bugs_ai.md TB-16.

**Port:** `colonies_found` (`src/core/colony.c`) sets no residue; the founding join
scores the tile's own terrain class, like the human auto-assign path.
[Evidence and tests](archive/dos_bugs_evidence_2026-10-24.md#db-24).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-15.

### DB-32 — Convert winning a fight draws the promotion roll for nothing
**DOS:** the combat-promotion body FUN_5fef_172c (raw 100064-100154) gates on
FUN_281f_0c9a, which returns 0 (eligible) for profession 0x1b, the Indian Convert. A
winning Convert therefore reaches the Washington test and, without Washington, draws
`FUN_281f_04d4(1, strength pool)` (raw 100100-100104). Then the ladder FUN_5fef_16ea
maps 0x1b to 0x1b (raw 100040-100059), the `iVar3 != iVar2` guard fails and nothing
changes. The roll can never have an effect: it only advances the shared RNG stream.

**Port:** `units_promote_on_win` (`src/core/units_combat.c`, the `UNITS_JOB_CONVERT`
arm) returns at once for a Convert, with no draw. Earlier tracked as port_bugs.md #598.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-32).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-39.

### DB-33 — Royal purchase adds the free-colonist count to the bells-this-turn word
**DOS:** FUN_43f7_1d42 (the King's purse-funded purchase, raw 74889-74890, asm
43f7:1e58) debits 1800 from `royal_money` and then adds the nation's free-colonist count
(`DS:0x9408 + nation`, a byte) to `nation + 0xe`. That word is "bells produced this
turn": it is zeroed at the start of each nation's turn and increased by the bell spend,
and nothing in the game reads it. The add is a stray write on the wrong field.

**Port:** `ai_king_1d42_royal_purse` (`src/core/ai_king.c`) stops after the
`royal_money` debit and leaves `liberty_bells_last_turn` alone.
[Evidence and tests](archive/dos_bugs_evidence_2026-10-23.md#db-33).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-46.

### DB-34 — Warehouse spoilage under 2 tons is never removed
**DOS:** the warehouse clamp in FUN_364b_0688 (raw 57845-57868) computes
`loss = min(stock, excess)`, then `if (loss < 2) loss = 0`, and only then does
`stock -= loss`. The "under 2" test is meant to hide a trivial @SPOIL message, but
because it zeroes the loss before the subtract, a stock one ton over the cap is never
reduced. It sits at cap+1 forever.

**Port:** `colonies_apply_warehouse_spoilage` (`src/core/colony_goods.c`) subtracts the
real loss and only withholds the spoilage count (and so the @SPOIL message) when the
loss is under 2. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-34).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-49.

### DB-35 — withdrawn: forest-clear lumber bonus follows DOS
FUN_479b_01a6 (raw ~76800) tests `FUN_281f_0754(colony) & 0x0a` on the
receiving colony's tile, so a Lumber Mill colony always gets the +1 step. The
user chose DOS over a 2026-10-22 trial fix ([details](archive/dos_bugs_evidence_2026-10-23.md#db-35));
the port (`units_pioneer_work_tick_w`, `src/core/units_pioneer.c`) keeps DOS.
Was dos_bug_candidates_2026-10-22.md C-52.

### DB-36 — Scored Go To fallback gives every due east/west step cost 1
**DOS:** the scored 8-neighbour fallback of `FUN_6662_0f74` (raw 104676-104690, asm
6662:13d1-1420) prices a step 1 when both tiles have a road or city, or when both have a
river and `cur_x == cand_x`, or when `cur_y == cand_y`. The last test is not inside the
river clause, so every due east or west step costs 1 whether or not a river is involved.
`FUN_465b_0000` (raw 75450-75465) and the flood tier of `FUN_6662_00f2` use the intended
grouping: river on both tiles and (same x or same y).

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) gives cost 1 on the river
clause only when both tiles have a river and the step is cardinal (same x or same y);
other steps cost `terr_cost * 3`.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-36).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-37 — Scored Go To fallback tests thirds of movement against tiles
**DOS:** the scored fallback of `FUN_6662_0f74` (raw 104700-104706) prices a step `3`
when `FUN_281f_090c(unit) < 2`, else `terr_cost * 3`. `090c` returns the unit's movement
in thirds (`DS:0x5234`, at least 3 for every type), so the test never holds and the
penalty is always `terr_cost * 3`. The flood tier of `FUN_6662_00f2` makes the same
choice correctly with `DS:0x5234 < 4` (raw 103896), i.e. only one-move units get the
flat 3.

**Port:** `units_greedy_next_step` (`src/core/units_move.c`) tests `units_max_mp() < 4`,
so a one-move unit pays 3 per step like in the flood tier.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-37).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-53.

### DB-38 — Arrival tile ring search lets the +e hit overwrite the -e hit
**DOS:** the ring search in `FUN_48d3_048e` (raw 77830-77875) walks each ring along its
horizontal rows (`y = start_y -/+ e`) and then its vertical columns, testing both sides
with an inner loop `local_12 = -1, +1`. The found flag `bVar1` is tested only by the
outer loops, not by the pair loop. When both the -e and the +e tile qualify, the second
hit overwrites the first, so the ship lands on the +e side although the -e tile was
found first and is equally near.

**Port:** `units_spiral_place_hs_near` (`src/core/units_cargo.c`) takes the first
qualifying tile of the pair (-e before +e) and stops. Callers: AI ship placement
(`ai_euro_ship.c`), end-of-turn arrival (`game_loop_eot.c`).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-38).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-55.

### DB-39 — Market pressure ratio truncated to 16 bits
**DOS:** the Europe price tick `FUN_38fd_0058` (raw ~58845 and ~58899) computes the
demand ratio `3 * Σledger / ledger[c]` with the 32-bit helper `FUN_1d1d_0ec6`, then
stores it in an `int` and subtracts it from the signed bid byte in 16 bits
(`local_6 - (int)lVar15`, and `local_52` in the cargo 1..4 pass). A ratio above 32767
wraps negative, so the sign of `bid - ratio` flips and the pressure word moves the price
the wrong way. The clamp-into-bid arm of the same loop compares the full 32-bit value
and is not affected.

**Port:** `europe_market_bid_sign` (`src/core/europe_harbor.c`), used by phases 2 and 3
of the market tick, takes the sign of `bid - ratio` without truncating. It only differs
once the ratio passes 32767 (late game, large `tons2` sums).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-39).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-57.

### DB-40 — Voyage roll burns an RNG draw for a west-edge penalty that does not exist
**DOS:** the voyage-length roll `FUN_48d3_0002` (raw 77563-77588) rolls RNG(1,100) and
settles the result (1 or 2 turns). Then `if (param_2 < 3)` it calls `FUN_281f_04d4(0,1)`
(an RNG(0,1) draw) and `FUN_281f_07b4(nation, 5)` (the Magellan test) and discards both.
The pedia describes a west-edge delay that Magellan removes, but the shipped code never
uses either result. The dead draw still advances the shared RNG stream whenever the
crossing starts or ends at a column below 3.

**Port:** `europe_voyage_turns_roll` (`src/core/europe_pool.c`) draws nothing for
`new_world_x < 3` (the argument is kept so callers stay unchanged). The input-replay
regression `europe_buy_ship_and_sail` was re-recorded from the port.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-40).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-58.

### DB-41 — Tory uprising size comes from the last colony scored
**DOS:** the Tory uprising FUN_43f7_06a6 (raw 73829-73932; the crown's fallback once its
land pools are spent) scores every eligible human colony into `local_18` (population x
(100 - SoL) x 2 / 100 + difficulty + 1, minus the attack strength on the colony tile;
0xffff when a crown unit is adjacent or no adjacent land is free) and keeps the best in
`local_16` (raw 73881-73882). The spawn loop then runs `while (0 < (int)local_18)` (raw
73891): it uses the score of the last colony scored, not the best one. The uprising
lands at the best colony with an unrelated number of Soldiers, and when the last colony
was disqualified (-1) nothing spawns and the uprising is silently cancelled.

**Port:** `ai_king_ref_tory_uprising` (`src/core/ai_king_ref.c`) spawns the best
colony's score. The port never reproduced the DOS read; recorded 2026-10-22 at the
user's call. [Tests](archive/dos_bugs_evidence_2026-10-23.md#db-41).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-61.

### DB-42 — Open-continents counter is never reset
**DOS:** the per-nation census FUN_4962_0018 (raw 78316-78319, asm 4962:05d7) adds to
`DS:0x9650` once for every continent with `continent_tally_a > 7` on which the nation
has no colony. Every sibling counter is zeroed at census entry; this one never is, so it
grows with every census call of every nation and stays large after the first turns. Its
readers, the Euro AI ship logic in FUN_521d_20e6 (raw 84955, 84982, 89498, 89527:
`!= 0`; raw 86353, 89663: `< 2`), therefore always see open land: ships keep founders
and soldiers aboard near home while the AI is small for the date, and the delivery pick
never gives under-defended colonies their bonus.

**Port:** `ai_euro_20e6_open_continents` (`src/core/ai_euro_land.c`), used by
`src/core/ai_euro_ship.c`, recomputes the nation's count on each call. The port never
reproduced the accumulation; recorded 2026-10-22 at the user's call.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-42).
**Status:** done (2026-10-22). Came from dos_bug_candidates_2026-10-22.md C-62.

### DB-43 — Coastal forts fire on ships of nations never met
**DOS:** the colony battery FUN_364b_03f6 (raw 57082-57083) fires on the first ship of
each adjacent water tile when `(FUN_281f_0a38(colony, ship) & 0x40) == 0` or the ship is
a Privateer. An unmet pair has relation byte 0, so the fort fires on it although the two
nations were never at war. First contact between Europeans needs land adjacency
(port_bugs.md #545), so a ship passing a colony never meets its owner; the Foreign
Affairs report does not list the nation at all. User-observed 2026-10-22 (campaign5,
[details](archive/dos_bugs_evidence_2026-10-23.md#db-43)).

**Port:** `units_fort_fire_is_hostile` (`src/core/units_combat_resolve.c`) also requires
the MET bit (0x20) for a European ship. Privateers are still fired on whatever the
relation. Recorded 2026-10-22 at the user's call.
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-43).
**Status:** done (2026-10-22).

### DB-44 — Disband gather budget is 0 for a ship on trade-route orders
**DOS:** Disband's refusal test runs the pickup gather `FUN_1427_10be` (raw 8606-8680)
first. For a ship with orders 2 (trade route) the hold budget stays 0 unless the ship
is off the Europe lane and off every colony tile (`FUN_137f_03e4` < 0) and
`FUN_1427_101c` returns >= 0, so a trade-route ship lying in port gathers nothing,
passes the `FUN_281f_08bc(ship,2) > 1` test and can be disbanded while sentried units
sit on its tile (they stay behind). Read statically at the #1168 fix; accepted by the user 2026-10-23
(not a user-observed bug).

**Port:** `units_ship_has_sentry_boarder` (`src/core/units_cargo.c`) uses cargo minus
goods holds whatever the ship's orders, so a sentried unit on the tile always blocks
the disband.
**Status:** accepted (2026-10-23, port_bugs.md #1168).

### DB-46 — Pioneer work counter carries over to a new job
**DOS:** the Pioneer progress counter +0x315a (raised by FUN_479b_01a6 / 0526) is zeroed
only on completion, Fortify and colony entry; the order entries (FUN_2b5a_123e / 1454)
write only the order byte. A Pioneer that chops 3 turns on one tile and starts Clear
Forest elsewhere finishes next turn.
A move (FUN_465b_0000) does not touch it either, so a Pioneer that keeps its
Clear/Plow or Road order while it walks finishes the job on the new tile at once.

**Port:** `units_pioneer_plow_w` / `units_pioneer_road_w` (`src/core/units_pioneer.c`)
and the AI 20e6 road arm zero the counter when the order changes to the job.
`units_move_commit_step` (`src/core/units_move.c`) zeroes a Pioneer's counter on
every step (2026-10-30, fandom wiki "pre-charge"). Two Pioneers on one forest are
not covered: the second keeps its own counter and plows at once once the first
clears the tile, as in DOS.
[Details and tests](archive/dos_bugs_evidence_2026-10-24.md#db-46).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-72 (port_bugs.md #620).

### DB-47 — Lost City Rumour counters are process-wide
**DOS:** FUN_65dd_0004 counts rumours explored in DS:0x1dc6 (raw 103459) and Cibola
finds in DS:0x1dc7 (raw 103546). Neither is in the save nor reset on New Game, so they
run for the life of the process: Fountain of Youth needs 4 rumours explored this process
(raw 103497), Cibola stops after 7 finds this process (raw 103524-103527), and a byte
wrap at 256 rumours briefly shuts both off again.

**Port:** `units_reset_state` (`src/core/units_move.c`) zeroes both on New Game and
Load; a port save's RTST chunk restores its own values after that (`runtime_ext.c`). The
rumour counter saturates at 255 (`units_combat.c`).
[Tests](archive/dos_bugs_evidence_2026-10-23.md#db-47).
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-73.

### DB-48 — Rumour trespass can lower the tribe's alarm
**DOS:** the @LOSTCITY8 arm of FUN_65dd_0004 (raw 103557-103559) computes the
alarm hit `rand(1,6) + (difficulty - skill + 1) * 5`, unclamped. A Seasoned
Scout (skill 2) at Discoverer gets -4..1, so the "the tribe is angered" rumour
lowers the alarm (FUN_281f_0d6c, raw 103650-103651; 0 skips it).

**Port:** `units_lcr_roll_outcome` (`src/core/units_combat.c`) clamps the hit at
0. All goldens and `golden_ai_joint` pass.
**Status:** done (2026-10-23). Came from dos_bug_candidates_2026-10-22.md C-74.

### DB-65 — Foreign units visible far outside the viewer's sight
**DOS:** the map draw (FUN_2f2b_6372) shows a foreign unit while the viewer's bit
is set in its vis byte (`col1_vis_mask`). A move resets that byte through
FUN_1427_0c9a, which ORs in the destination tile's owner nibble (465b_06dc, raw
75766). On open ground that nibble is the first nation to reveal the tile
(FUN_13f1_000a) or the last one to have a unit there. So any unit stepping onto
ground the human explored first shows up anywhere on the map. The bits also
stick: a unit is never hidden again until it moves, and FUN_465b_0673 ORs extra
bits into the units left behind on the tile a mover leaves.

**Port:** the sim and the save keep the DOS bits (AI and goldens use them). For
display only, `units_shown_to` (`src/core/units_map.c`) shows a foreign unit
when it stood on a tile in the viewer's live sight at some point this turn. That
sight is the viewed nation's units at `units_sight_radius` with the
`map_sight_each` domain gate, plus colonies at radius 1.
`game_render_map_composite` feeds the set every frame (`units_seen_note`). It
empties when the turn number or the viewed nation changes, and on load
(`units_seen_reset`). The main map, the minimap and the sidebar's tile stack all
use it, and so does the Go To destination check (`units_set_goto_w`), so a
refusal cannot reveal a hidden unit. Rival move slides and combat
lunges (`game_move_watch_w`, `game_combat_watch`, `game_phantom_lunge`) play only
when one end is in that live sight (`game_tile_live_seen`). Before the first note (unit tests) it
falls back to the DOS bit.
**Status:** done (2026-10-30, user-requested 2026-10-30).

### DB-66 — Captured colony's Custom House exports nothing
**DOS:** every Euro AI colony tick opens with memset(colony+0x8a, 0, 2)
(FUN_5952_035e raw 93943), so an AI colony carries no Custom House export bits.
The capture tail (FUN_5fef_1b0e) keeps the record's bits, so a colony taken from
the AI exports nothing once a Custom House stands there, built before or after
the capture. A founded colony gets the default set (FUN_364b_1ba8 raw 58058).

**Port:** when a human-controlled nation (`player[n].control == 0`) captures,
`colonies_capture_ex` (`src/core/colony_build.c`) re-arms
`COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK` when the captured colony's bits are 0. A
colony whose owner kept some exports keeps them; AI captors keep DOS's 0
(goldens). Test: `unit_colonies`
(`colonies_capture_ex` case).
**Status:** done (2026-10-31, user-requested 2026-10-31).

### DB-69 — Denounce Heresy weight compares two loop invariants
Was C-65.
**DOS:** `thunk_FUN_1000_a594`, overlays.c:75807 `if (uVar3 == param_2)` (asm
OVL13 ~0x1f3f-0x1f5e): both operands are constant inside the village loop, so
every mission's weight lands on one side; the per-village owner computed into
`[BP-4]` is never read. (DB-7 is a different slip in the same function.)

**Port:** `ai_contact_denounce_heresy` (`src/core/ai_contact_actions.c`)
compares each village's mission owner (`v->mission & NATION_MASK`) with `e`.
**Status:** done (user 2026-11-03; port already differed, all goldens pass).

### DB-74 — Europe purchase keeps the gold when the unit cannot spawn
**DOS:** `FUN_38fd_4b50` (raw 64877-64886): after @REALLYBUY answer 1 the purse
(`nation+0x2a`) is debited first, then `FUN_281f_095c` spawns the unit on the dock.
When the spawn fails (unit cap / full dock) the return is -1 and nothing refunds the
price: the player pays for nothing.

**Port:** `europe_purchase_commit` (`src/core/europe_dock.c`) takes the dock slot
first and refuses with nothing charged when the dock is full (same order as the
recruit and training paths, port_bugs.md #1416). Test: `unit_units_core`
(fountain full-dock case).
**Status:** done (user 2026-11-03, PR #1 from UncleTawnos).

### DB-75 — Custom House sells the tools the queued project needs
**DOS:** `FUN_364b_0688` runs the Custom House sale inside the Phase B cargo
loop (raw 57257-57330: stock > 99 sells down to 50) and only later, in Phase L
(raw 57748-57774), spends `colony+0xb6` tools on the finished project. A colony
with Tools checked ships the tools it was stocking for its build, and the
project then stalls on @NEEDTOOLS.

**Port:** `turn_produce_custom_house` (`src/core/turn_production.c`) hides the
current project's tools cost (`turn_project_tools_cost`) from the house for the
sale, so only the surplus above it sells (same > 99 / leave 50 rule on that
surplus). AI colonies never sell tools (FUN_364b_0636), so only the human arm
changes. Goldens and idle campaigns pass unchanged, so no `dos_buggy_behavior()`
gate. Test: `unit_turn_core` (`custom_house_autosell` case).
**Status:** done (user 2026-11-05).

### DB-76 — Any foreign warship nearby shuts the human's Custom House
**DOS:** `FUN_4962_0018` (raw 78259-78289) sets the colony blockade pair
`+0x1b` bits 0x01/0x02 for every foreign ship with a non-zero combat byte
within the 11x11 box and a sea route of cost 0..5, without reading diplomacy.
`FUN_364b_0688` then skips the whole Custom House sale of a human colony while
`+0x1b & 3` is set. A friendly or neutral frigate parked off the colony stops
all exports. Seen 2026-11-05: port_saves/discard COLONY09.SAV, Barbados
(45,38), four Dutch frigates at peace with France (relation 0xe2).

**Port:** `ai_euro_colony_ship_probe_4962` (`src/core/ai_euro.c`) counts a
ship for a human colony only when its nation is at war with the owner (MET
without PEACE in either relation row, the Foreign Affairs report's test; WAR
0x02 survives a treaty), it belongs to the Crown after the declaration, or it
is another nation's Privateer. The filter runs after the 0906 flood, so the
shared flood grid sees the same calls. AI colonies keep the DOS count (their
Custom House ignores the bits; the ship-pressure tallies feed the AI). The
@KINGFRIGATE offer (`ai_king_frigate_threat_counts`, FUN_3844_00f2 raw 58393)
keeps the DOS count: the probe also records the unfiltered pair
(`ai_euro_colony_dos_blockade_pair`), so a peaceful frigate still earns the
free-Frigate offer (port_bugs.md #1593). Goldens
and idle campaigns pass unchanged, so no `dos_buggy_behavior()` gate. Test:
`unit_ai_euro_expand_labor` (`unit_human_census_ship_pressure_refresh`).
**Status:** done (user 2026-11-05).

### DB-77 — A military unit on Go To attacks a native unit on its goal
**DOS:** FUN_479b_0972 (raw 77052-77122) takes each Go To step from FUN_6662_0f74 and
hands it to FUN_291f_044e → FUN_4720_049e → FUN_465b_0000. When the goal is adjacent,
0f74 returns the plain sign step FUN_6662_0086 (raw 104556-104565) with no occupancy
test, so a native unit on the goal is attacked: the @WHACKINDIANS confirm at peace,
none once the tribe is hostile. A non-zero 049e return (Cancel) makes 0972 clear
+0x314c (raw 77075-77080). Seen in DOS by the user: the Soldier attacked the Brave and
Cancel cleared the Go To. A native short of the goal is pathed around: the flood's
DS:0x1dd6 owner term (raw 103955-103957, 104044-104046) and the scored fallback's
FUN_281f_06d2 test (raw 104680-104681) reject a tile held by another nation.

**Port:** fixed behind the `mods.goto_native_wait` toggle (settings.md "Mods"; on by
default for new games, off for DOS saves and under `dos_buggy_behavior()`). On, a human
unit whose Go To goal holds a native unit (not a village) walks up to it and stops next
to it: no attack, no prompt, the Go To is kept, and the pacer parks the unit for the
turn (moves 0); it retries every turn. Off = DOS. One test for both paths:
`units_goto_waits_for_native` (`units_move.c`), called from `game_pacer_goto_step`
(`game_loop_update.c`) and `units_advance_goto_one_step_w`. AI units are unchanged.
Test: `unit_units_movement` `unit_goto_native_wait_mod`.
**Status:** done (user-approved toggle, default on).

### DB-78 — A tax of 128 or more reads as negative
**DOS:** the nation tax byte (Col1 nation +1) is read as a signed char. A tax of 128 or
more (the `]`/`[` tax cheat, an edited save) is then negative: the Royal loss cut in
FUN_5fef_0352 (raw 99680-99686, `min(tax, cut)` with `0 < cut`) cuts nothing, and
Thomas Paine's statesman bonus in FUN_2f2b_348c (raw 50647) becomes a penalty.

**Port:** every tax reader takes the byte unsigned. The last two signed reads,
`units_royal_loss_tax_cut` (`units_combat.c`) and the Paine bonus in
`ai_euro_colony_jobs.c`, read it unsigned too. Normal play keeps the tax below 128, so
goldens and replays are unchanged and there is no `dos_buggy_behavior()` gate. Tests:
`unit_units_combat` `unit_royal_loss_tax_cut`, `unit_ai_euro_5952_indoor`
`paine_bonus_reads_tax_unsigned`.
**Status:** done.

### DB-79 — A finished Warehouse Expansion left selected eats 20 tools a turn
**DOS:** FUN_364b_0688 Phase L (raw 57748-57771) checks and debits the project's 20 tools
before it calls FUN_364b_0114, and 0114 refuses a Warehouse Expansion once +0x95 > 1
(@NOMOREWAREHOUSE, raw 56906-56910) with the hammers and the selection kept. The
Expansion bit is never set, so the stale project spends 20 tools every turn it stays
selected (port_bugs.md #1521).

**Port:** `colonies_try_complete_building_ex` (`colony_build.c`) refuses before the tools
check, so the tools stay; @NOMOREWAREHOUSE still shows each turn the tools are there and
@NEEDTOOLS when they are not, as in DOS. Test: `unit_turn_core` `stale_warehouse_expansion`.
**Status:** done (2026-11-07, user call).

### DB-81 — Far-tier Go To ping-pongs between two coarse cells
**DOS:** `FUN_6662_0f74` far tier (`LAB_6662_10e1`, raw 104590-104600) snaps the
mover to its own 4x4 coarse cell (`FUN_6662_09ae`), takes the next cell of the
coarse BFS (`0b4e`) and floods toward that cell's probe tile. One step can cross
into a neighbouring cell whose coarse route leads back through the cell just left,
so the unit alternates between two tiles forever, spending all its moves. The
#1040 reversal guard covers only AI_SAIL units. Seen 2026-11-07 (campaign 21,
port_bugs.md #1576): a human Caravel, Go To (33,30) from (12,23), cell (3,5) has
no coarse link to (3,6), so it stepped to (11,22) in cell (2,5), whose waypoint
flooded back to (12,23), with open ocean to the goal.

**Port:** `units_goto_director` (`src/core/units_move.c`): a far-flood step that
undoes the unit's last step (`last_dir ^ 4`), for a Euro unit not on AI_SAIL,
takes the whole-map BFS step toward the goal instead (`units_bfs_next_step`, the
`UNITS_FAR_BFS` tier; its queue now holds the whole map). Ungated: every golden and
idle campaign still matches. Test: unit_units_movement `unit_db81_far_waypoint_pingpong`.
**Status:** done (2026-11-07, user call).

### DB-87 — A save with a zero map size loads when no map is live
**DOS:** `FUN_75c2_0840` (raw 120154-120157) compares the save's W*H only against a
live map (DS:0x180/0x182); with none it accepts any size, including 0.

**Port:** `col1_save_validate_head` (`src/core/col1_save.c`) returns NOT_A_SAVE
(LOADNOT) for W or H 0 with no live map; the parser and the bridge refuse an empty
map too. With a live map it gives LOADSIZE as DOS does.
**Status:** done (user 2026-11-08, port_bugs.md #1599 / #1608).

### DB-95 — Deleting a trade route rewrites the remembered route of idle transports (port_bugs.md #1720)
**DOS:** a wagon or ship keeps its last route and stop in +0x17 after it leaves the
route. FUN_647e_1486 (raw 103175-103189) walks every unit whose type has hold capacity
(type*0xe + 0x5237 != 0) with no orders test: route == slot gets route 0 and stop 0
(0862 / 08b2), with the orders cleared only if they are 2; route > slot gets route - 1.
The stale route also becomes the Begin-route picker's default row (FUN_291f_08ec).

**Port:** a unit holds a route only while its orders are 2 (`units_clear_orders` drops
it; the col1_bridge capture writes 0 for transports off a route), so
`game_do_trade_delete_slot` (`src/core/game_dialogs.c`) touches only units on routes,
and Begin preselects a route only for a unit already on one (port_bugs.md #1718 /
#1719). Saves differ in the +0x17 nibble of idle transports. Goldens pass without it.
**Status:** done (user 2026-11-10; port already differed).

### DB-100 — Map gen forest wander reads unset hill and mountain chances (port_bugs.md #1764)
**DOS:** FUN_684c_08c0 first writes [bp-0x20] / [bp-0x26] at 106c / 106f, inside the
base-type switch. If the first wander tile(s) already carry 0x80 or 0x20 (a mountain left
by the latitude paint), the epilogue at 11de (`cmp/jz`) reads stack residue and draws
rand(0, -5874), shifting the LCG for the rest of the map. DOSBox 2026-11-10 (NEW WORLD,
VR_SOUND, seeds 100 and 777): the residue was 0xe90e / 0x9b89 both times. None of the
four DOS map dumps hit it.

**Port:** `forest_wander_pass` (`src/core/map_gen.c`) starts both chances at 0 and draws
nothing.
**Status:** done (user 2026-11-10; port already differed).

### DB-101 — @INDIANCOME after first-contact peace reads an unrelated word
**DOS:** FUN_5bfb_0182 (raw 96551, asm 5bfb:01ee-0203) shows @INDIANCOME after
@INDIANPEACE when `FUN_281f_030c(euro - 4, tribe nation) < 0x19`. Every other 030c caller
passes (tribe slot, euro); here the arguments are swapped, so the word read is
DS:0x5b1c + ((euro - 4) * 0x27 + nation) * 2: a dwelling-record word (slots 71..80) for
euro 0..2, an Inca-record word for euro 3. Whether the line shows depends on an
unrelated village's population and mission bytes (port_bugs.md #1230).

**Port:** `ai_contact_apply_welcome_accept` (`src/core/ai_contact.c`) reads the tribe's
alarm toward the nation. FUN_5bfb_022e has just clamped it to 20 (raw 96620-96624), so
@INDIANCOME always follows @INDIANPEACE for a human nation. Message chrome only; no
AI, RNG or save path reads the gate. Test: `unit_ai_contact_core`
`welcome_accept_come_reads_alarm`.
**Status:** done (user 2026-11-11). Was dos_bug_candidates_2026-10-22.md C-66.

### DB-102 — The save writes 4 uninitialised stack bytes after the tallies
**DOS:** FUN_75c2_0288 (raw 120104, asm 75c2:07bd) writes the 4-byte stack local at
[BP-6] after `continent_tally_b`, at post_map offset 604 (`save_path_blob`). Nothing in
the function sets it (ENTER 6; its only other local is the version word at [BP-2]), so
every save carries whatever an earlier call left on the stack. The loader FUN_75c2_0940
reads the 4 bytes into `local_64` and never uses them (raw 120425).

**Port:** creates no junk of its own: `post_map.save_path_blob` is 0 on a new game and a
loaded save's 4 bytes are carried over unchanged to the next save. Nothing reads them in
DOS or the port, so the slip is invisible.
**Status:** recorded, not fixed (user 2026-11-11).

### DB-107 — A trade route keeps aiming at a stop colony another nation captured
**DOS:** FUN_479b_0bd0 binds a colony stop by record index only (`FUN_281f_09e6`, raw
77233-77236 / 77253: the record's x/y become the goto) and FUN_479b_0b84 (raw
77164-77174) tests arrival by tile only. Neither checks the owner, so after a capture
the route unit re-aims a colony tile it can never stand on and never moves on.

**Port:** `game_trade_stop_colony_ok` (`src/core/game_loop_eot.c`): a colony stop is
valid only while its colony is active and the route unit's nation owns it. An invalid
current stop counts as reached (`game_trade_route_at_stop`), so `game_trade_route_retarget`
services nothing and advances, stepping past further invalid stops; with no valid stop
the aim fails as it did for a missing colony. Ungated: every golden still matches.
Test: unit_stage_seams `test_trade_route_skips_captured_stop`.
**Status:** done (user 2026-11-11).

### DB-104 — A Trade Route ship boards no passengers in Europe or at a colony stop
**DOS:** `FUN_1427_10be` (raw 8630-8644) gives a hull with orders 2 (trade route) a
boarding budget of 0 when it stands on the Europe dock (`(nation - x) == 0x14`) or on a
settlement tile (`FUN_137f_03e4` >= 0). The route leg (`FUN_48d3_0346` raw 77741, and the
map move off a colony) therefore carries no sentried colonist waiting on the dock or in
the colony, and riders loaded by hand stay behind. Only the manual Sail click in Europe
(`FUN_38fd_2a92`, which first clears the orders and so the route) boards anyone.

**Port:** fixed behind the `mods.riders_keep_ship` toggle (DB-109, on by default; off =
DOS, and always DOS under `dos_buggy_behavior()`). `europe_set_sail_from_harbor_ex(...,
false)` (`src/core/europe_harbor.c`) boards sentried dockers up to free capacity and keeps
the riders and the route; `units_ship_departure_pickup` (`src/core/units_cargo.c`) no
longer zeroes the budget for a route hull on a colony tile. Toggle off or under the switch
(every test binary) both keep the DOS budget 0. Tests: `unit_europe`
`case_europe_trade_route_leg_boards_dockers`, `unit_units_core`
`case_trade_route_departure_no_pickup`.
**Status:** fixed, toggle-gated with DB-109 (user 2026-11-12: base game DOS, fix as mod).

### DB-105 — A ship beaten attacking out of its own Drydock port is sent elsewhere
**DOS:** when a ship attacker loses, `FUN_5fef_1b0e` (raw 100566-100567, 100759) hands
`FUN_5fef_0ec0` / `FUN_5fef_0352` the attacker's own tile (`+0x3144/+0x3145`) as the
distance origin. 0352's repair-port scan (raw 99584-99597) takes `dist <= best && dist
!= 0`, so a Drydock colony on that tile is always skipped: a damaged Privateer that
attacked from its own Drydock port limps to the nearest other Drydock colony, or to
Europe (and a WoI human with no other repair port loses the ship).

**Port:** fixed outside `dos_buggy_behavior()`. `units_apply_naval_loss_outcome`
(`src/core/units_combat.c`) takes the loser's own Drydock colony on its tile as the
repair port while `g_units_naval_attacker_lost` is set (only the attacker-loss limb of
`units_resolve_naval_combat_ff_w`). Captured and raided ports keep the distance-0 skip.
Test: `unit_units_ships` `unit_attacker_loss_stays_in_own_drydock`.
**Status:** fixed (user 2026-11-12).

### DB-106 — A Trade Route wagon never unloads a boycotted cargo at a colony stop
**DOS:** `FUN_479b_0bd0` (raw 77256-77267) wraps the whole unload loop in
`if (FUN_291f_0cd8(cargo) == 0)` (`FUN_38fd_05e8` raw 59010-59016, the nation `+0x20`
boycott word), above the split between the Europe stop (sell) and a colony stop
(unload). The load loop has no such gate. Once the Crown boycotts a cargo, a wagon or
ship on a colony-to-colony route keeps loading it and never unloads it, so its holds
fill up and it carries nothing else (seen in the discard campaign: sugar boycotted
autumn 1639, both wagons clogged with sugar at New Amsterdam).

**Port:** fixed unconditionally. `colonies_trade_route_service_stop`
(`src/core/colony_goods.c`) unloads the stop's unload list whatever the boycott;
the Europe stop keeps the gate (`europe_sell_commit` refuses a boycotted sale).
Test: `unit_colonies` `unit_trade_route_stop_load_order_and_boycott`.
**Status:** fixed (user 2026-11-12).

### DB-109 — Passengers switch ships when another own ship leaves the tile first
Was tolerated_dos_bugs.md TB-2.

**DOS:** a passenger is not "aboard" anything; it is a sentried land unit in
the tile's unit chain. When a ship leaves a tile, `FUN_1427_10be` (raw
8606-8680) sorts the tile with `FUN_1427_04d6` (hulls on top, then Treasure,
then @UNIT size 6..1, each bucket in chain order) and boards every same-nation
land unit that fits the hull's room, charging each its @UNIT size column
(`FUN_4720_00e0`, raw 74655-74661). Riders of other ships on the tile are in
that chain too, so whichever ship moves off a shared tile first takes them,
first come, first served, and may leave some of its own passengers behind for
the next hull. A rider of another hull boards when it is sentried or the tile
is water (asm 1427:11d4-1232, port_bugs.md #1780). DOSBox-confirmed
(independence seed-100 1->2,
[evidence](archive/tolerated_dos_bugs_evidence_2026-10-24.md#tb-2)).

Trade-route arm (raw 8630-8644): a hull with orders 2 gets budget 0 on a
settlement tile (`FUN_137f_03e4` >= 0) or in its Europe slot. Off both, its
record is parked at (-4,-4) and `FUN_1427_101c` (raw 8566-8602) runs: every
other hull on the tile, top of the chain first, runs its own 10be (nested, so
the `DAT_0200` guard, raw 8624-8626 / 8678, gives it the plain budget) and
leaves with its boarders for the (-2,-2) chain; `FUN_1427_040c` brings them
back. The route hull's riders stay in the tile chain, so the other hulls can
take them. The route hull then gets its budget only if a unit was left on the
tile, and walks the whole tile again; else it takes no one. So a route ship at
sea next to an empty own ship sails empty, its passengers left on the other.

The landfall pick re-runs the same pickup first (`units_first_landfall_cargo`:
FUN_4720_015c's FUN_281f_0916 = FUN_1427_12f6 -> 10be, raw 75967), so with two
own hulls at sea one hull's Make Landfall can put another hull's rider ashore
(port_bugs.md #1782).

**Effect:** with two or more own ships on one tile (colony or open sea), units
loaded on one ship can sail off on another, and a ship can depart without some
of the units the player loaded on it. Affects human players and the AI alike.

**Port:** `units_ship_departure_pickup` (`src/core/units_cargo.c`), called from
the ship move (`units_move.c`), the landfall pick, and the AI ship moves
(`ai_euro_ship.c`). Fixed behind the `mods.riders_keep_ship` toggle
(settings.md "Mods"; on by default for new games, off for DOS saves and saves
from before the toggle, and under `dos_buggy_behavior()`): the hull seats its
own riders first, then sentried land units on the tile, and never takes another
hull's rider; the trade-route at-sea arm is skipped, since with the fix no
other hull could take the route hull's riders. Off = DOS: the walk takes the
hull's own riders, tile sentries and other own hulls' riders in the 04d6 order,
and the at-sea arm runs the other hulls' pickups first (each takes its pick
off the tile until the route hull's budget is known). Not reproduced: the
nested pickups do not re-stamp Sentry on what they board (DOS +0x314c = 1,
raw 8676); at sea the water rule makes every rider eligible anyway. The
port runs the arm only on map tiles: on the off-map park (x or y >= 200, the
Europe dock and lanes) a route hull keeps its full budget, where DOS gives 0 in
its Europe slot and applies the 03e4 test elsewhere (not traced off-map). Tests:
`unit_units_core` `ship_switch_quirk` (DOS), `unit_units_ships`
`unit_tb2_departure_own_riders_only` (fix) and
`unit_trade_route_sea_departure_pickup` (both, the trade-route arm).
The same toggle also gates DB-104 (a Trade Route hull boarding no one at a colony
or in Europe), so toggle off is DOS for every departure.
**Status:** done (user-approved toggle, default on; 2026-11-12). Golden fixtures and
idle campaigns keep DOS through `dos_buggy_behavior()`.
