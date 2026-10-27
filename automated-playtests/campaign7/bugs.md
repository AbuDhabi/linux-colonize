# Campaign 7 bugs and oddities

## CN7-O1 (oddity): land-name popup pre-fills "New England" (repeat of CN6-O1)
- Turn 4 (1496), landfall on uncharted land. The field starts with the seed
  "New England". Kept it with `key Return` (no typing). Not retested with
  typing this campaign; CN6-O1 still open (typing appended to the seed).
- Assessed 2026-10-27: See CN6-O1: player error (stray Left keys), not a port bug.

## CN7-O2 (oddity, unverified): ship `goto` ignored on later turns
- Turns 1-3 (1493-1495): `activate 1`, `goto 1 30 36`, `auto` moved the
  caravel nothing; log shows `ORDER ... goto (30,36)` then no MOVE line.
  Turn 0 `goto 1 40 40` moved 3 tiles, then stopped. Arrow keys
  (`key Left`) worked the same turn. Probably the target is off the known
  map (unexplored) so the path fails. Check: does goto into unknown water
  work in DOS? Workaround: arrow keys.
- Resolved 2026-10-27: DOS rule, not a bug. The targets were inland land
  tiles a ship cannot reach; the pathfinder (FUN_6662_0f74) then drops the
  order and spends the turn. Turn 0's 3 tiles ended on (53,41), a coarse-grid
  sample tile, where every such goto fails at once. The driver now prints
  `ERR goto found no path`; RULES.md "Driver quirks" explains.

## CN7-O3 (oddity): `endturn` errors while a sentried ship still has moves
- Turn 4-5: `endturn` returned "no End of Turn prompt (selected unit 1)"
  while the caravel had 9-12 moves. Passed after `key s` (sentry) on the
  caravel with 12 moves (turn 5-6). Expected by the RULES (a unit waiting
  for orders blocks `endturn`), not a bug if sentry is the fix.
- Assessed 2026-10-27: Expected: a unit with moves and no orders waits (RULES). The driver ERR line now names the waiting unit.

## CN7-O4 (oddity): first `key b` (Build Colony) only showed the tutorial hint
- Turn 6 (1498), pioneer on the landing tile. `key b` printed "The
  abundance of Ore ... press B" (the hint); a second `key b` after a
  `key Return` opened the name entry. Check whether the hint consumed the
  first key (tutorial hint popup), or the first key was ignored.
- Assessed 2026-10-27: DOS behaviour: `activate` queued the ore hint; any key closes a DOS message box (FUN_6f74_2580 raw 116817), so that B closed it. RULES: check for LOG after activate.

## CN7-O5 (oddity): `build N BID` on an owned building fails silently
- Jamestown 1498: `build 3 35` (Carpenter's Shop) did nothing. The colony
  already had Carpenter's Shop, Weaver's House and Rum Distiller's House
  (founding starters). The driver prints `BUILD ... now <old>` with no reason.
  Expected a message, or the owned rows hidden from the menu. Check: the
  refusal is `colonies_dos_owns_row` (src/core/colony_build.c). Not a bug if
  DOS also gives starters; unverified.
- Assessed 2026-10-27: DOS gives Town Hall, Carpenter's Shop and five craft houses at founding (FUN_364b_1ba8, FUN_479b_076e); the menu never lists owned rows. Driver fixed: `build` now refuses anything not in the build menu with an ERR line.

## CN7-O6 (oddity): `drag` ore to the caravel hold works, but `key u` is the only sale
- Europe 1515: dragging hold cargo onto the London warehouse did nothing.
  `key u` sold the 70 ore (140 gold). Campaign 6 noted the same key.
- Assessed 2026-10-27: Player error: the drag went from hold row to hold row. Sell by dropping on the market strip (y >= 179) or with `sell H`.

## CN7-O7 (oddity): recruit list row changes between visits
- Europe 1500 list: None / Master Carpenters / Expert Farmers / Free
  Colonists. Europe 1545: row 3 was Indentured Servants. A blind `key Down
  x3` bought Indentured Servants (not the Free Colonist meant).
- Assessed 2026-10-27: DOS behaviour: a recruit refills the picked slot (FUN_38fd_4884), a crosses immigrant takes a random slot (FUN_38fd_5e52).

## CN7-O8 (unverified): Jamestown ore stock fell 300 -> ~76 with no logged sale
- Turn ~186-219 (1639-1656). Status showed `ore 300` at 1639 and `ore 76`
  at 1656; the log has no CARGO line from Jamestown in that span (the last
  CARGO was 1602, our load 100 to caravel 256). Could be a warehouse cap
  rule (storage 300 vs Warehouse Expansion "only one") or a log gap.
  Check the trace save turn 186 vs 220 ore field before filing.
- Assessed 2026-10-27: Not a bug: the Custom House completed at turn 203 and sold 252 ore (out.txt 'Jamestown sells 252 Ore'); ore export is on from founding.

## CN7-O9 (oddity): food stock goes 198 -> 0 on growth, pop display stays 8
- Turn ~219 (1656): `PROD Jamestown: food -198 (0)` and the event "Population
  increase in Jamestown. New colonist now available." The `colonies` line
  still prints pop 8 that turn. The new colonist waits outside; `units`
  lists it only after it is admitted. Expected: the pop count moves at once
  or the colonist is listed as outside.
- Assessed 2026-10-27: DOS behaviour: growth subtracts 200 (FUN_364b_0688 raw 57615-57618) and spawns a unit outside; pop moves when it joins.

## CN7-O10 (oddity): caravel cargo takes passenger room; dock units left in London
- 1671 (turn 250-254): caravel carried 100 tools + 86 ore. Only 1-2 of 3 dock
  recruits boarded ("passengers prof25" only). The rest stayed on the
  London dock ("boards next ship") and needed a second trip. Expected: all
  dock units board when the ship leaves if space allows; check capacity rule.
- Assessed 2026-10-27: DOS behaviour: the tools were sold first, so the caravel left with one hold of ore and one passenger (FUN_1427_10be; caravel 2 holds shared).

## CN7-O11 (oddity): arrow/keypad moves blocked by a queued colony popup
- Turn 254 (1673): ship `key KP_1`/`KP_4` did nothing. A COLONY_EVENT popup
  ("Fortress requires 200 tools") was queued; `auto` cleared it and the same
  keys moved the ship. Keys are swallowed while a popup waits; no ERR line.
- Assessed 2026-10-27: DOS behaviour: popups are modal and take the next key. Driver: `activate` now refuses behind a popup.

## CN7-O12 (oddity): `goto` for a ship ignored at Jamestown (1673, 1696)
- Caravel 1544 at (49,36): `goto 1544 48 40` produced no move, `auto` idle.
  Same as CN7-O2. Arrow keys work.
- Resolved 2026-10-27: (48,40) is seen land, so the order was refused
  (`ERR goto refused` in out.txt). Jamestown is (48,41).

## CN7-O13 (oddity, costly): English frigate offer raised the King's tax 10 points
- Turn 344 (1718): King asked "Shall we dispatch a frigate ... to assist you?"
  I answered Yes. Result: "Royal frigate dispatched" and the next popup raised
  tax by 10% (38 -> 48%) for the "recompense". Frigate arrival not checked yet.
  Expected: the offer's cost was shown before answering. Note in the journal:
  decline the offer on Discoverer unless the frigate is needed.
- Assessed 2026-10-27: DOS behaviour: FUN_3844_00f2 raw 58393-58424 spawns the frigate then raises tax 10 (KINGTAX); @KINGFRIGATE states no cost. RULES tip added.
