# Campaign 8 bugs and oddities

Status: unverified unless marked. Port bug = `CN8-Bn`, oddity = `CN8-On`.
Check docs/dos_bugs.md and docs/known_divergences.md before filing.

## CN8-O1 (oddity): `sail H` + `choose 0` in one batch did not confirm the set-sail popup
- 1499 and 1513 London. The batch `sail 0` `wait 5` `choose 0` left the
  CHOICES popup open ("Shall we set sail..."). A second `choose 0` sailed.
  Probably the popup opens a frame after `sail`. RULES says `sail H` + `choose 0`.

## CN8-O2 (oddity): `goto` for a unit silently idle while a tutorial popup waits
- 1516 pioneer at (37,30): `goto 159 40 30` logged the ORDER and no MOVE for
  many turns. The first `key KP_6` (manual step) opened the road tutorial
  ("press R to build roads"); the popup took the key. Once the popup closed,
  the same goto moved. Expected: the tutorial should not eat an order, or the
  driver should print a line for a queued popup. Only `auto` cleared it.

## CN8-O3 (oddity): sentried scout wakes every turn and blocks endturn
- Scout 111 (Seasoned Scout) set to sentry (`key s`) with moves left: orders
  1 the same turn. Next turn "has no orders (blocks endturn)" again. Log
  shows "wake (was sentry)" when a native/foreign unit comes near. Workaround:
  give it a move each turn. Probably DOS (sentried units wake on foreign units).
  Not verified against DOS.

## CN8-O4 (oddity): `s` key with no unit selected opens save dialog
- 1516: `s` after a failed `activate 127` (ship in port) opened Save/Load.
  Known (RULES "Driving safely"). Escape closes it.

## CN8-O5 (oddity): caravel passenger limit is 2, third dock unit waits
- 1513 and 1516: Expert Farmer + Master Gunsmith + Free Colonist on the London
  dock: only 2 boarded; the Free Colonist (unit 120, prof 28) still waits in
  London at 1542. Caravel passengers = 2 in this port (cargo takes no room in
  the trip with furs). Check the DOS hold count before filing.

## CN8-O6 (oddity): colony growth empties the food stock
- Jamestown 1540: food stock 184 (+18) -> 0 the turn a colonist was born
  (new colonist outside). Same as CN7-O9. Known: DOS population growth takes
  the food bank. Not verified in DOS.

## CN8-O7 (oddity): `build N BID` on an owned building is silent
- 1502: `build 1 35` (Carpenter's Shop, starter-owned) printed "BUILD ... now
  6" with no reason (6 = Docks, the prior production). Same as CN7-O5.

## CN8-O8 (oddity): village chiefs give gold only sometimes
- Arawak villages: 928 (1510), 928 (1514), 832 (1541). Second visit to the
  same village (1514, chief at 38,36): no gold, "always pleased to welcome".
- Sioux (33,28), (28,27), (33,31 camp) and Tupi (28,33): "tales of nearby
  lands", no gold. Possibly tribe/treasure based. Check the DOS rule (chief
  gift table) before filing.

## CN8-O9 (oddity): eject pioneer takes all colony tools
- 1536 `eject 1 0 1` gave the new pioneer 100 tools from Jamestown's stock
  (tools 100 -> 0). Custom House then "needs 50 tools, stock 0" (advise WARN).
  Expected by DOS? The Europe-bought tools fix it. Note for RULES: buy tools
  before ejecting a pioneer.

## CN8-O10 (oddity): Spanish demarcation treaty popup at a camp
- 1532: scout entered a Spanish "New Spain" settlement at (33,31): demarcation
  treaty, then a diplomacy popup (go in peace / alliance / ...). Not a bug.

## CN8-O11 (oddity): natives attack our scout at peace (1600)
- Scout 310 at (30,30), turn ~113: "COMBAT land attacker=Braves (nation 10)
  defender=Scouts (id 310)". Scout won ("English Scouts defeat Sioux near
  Wilderness!"). Sioux had accepted peace in 1520. Check the native tension
  rule (a scout standing near a camp) before filing.

## CN8-O12 (oddity): King's tax raise: Furs party refused the tax, 100 furs thrown in the sea
- 1603 tax 8% -> 10% offered; choice 1 "Hold Plymouth Furs Party" kept the
  tax at 8% ("Tax stays at 8%"), cost 100 furs at Plymouth, boycotted furs in
  Europe. Matches RULES (party refuses tax). Custom House sale at Jamestown
  still went on (verify the boycott does not stop the Custom House).

## CN8-O13 (oddity): drag hold -> warehouse refused silently at the storage cap
- Jamestown 1636-1640: merchant hold of 100 tools refused (warehouse 150 + 100 >
  200 cap, Warehouse only). No message; the hold stays aboard. Warehouse
  Expansion (16) raised the cap to 300 and the drag worked.

## CN8-O14 (oddity): ejecting a pioneer takes 100 tools from colony stock
- Jamestown 1616: `eject 1 9 1` gave Pioneer 600 (tools 100) and Jamestown tools
  fell 150 -> 50, starving the Fortress of tools. Buy tools first.

## CN8-O15 (oddity): colonist ordered to a colony tile is refused, then stuck
- 1664 (Plymouth 40,30 -> Roanoke 38,29): `goto ... 38 29` refused ("tile not
  enterable for this unit"). Keys moved nothing. Soldier 3 at (37,30) also
  refused a `goto` into Jamestown (36,30) and fortify raised "Break Treaty"
  (native treaty). Disband with `key d shift` + `choose 0` worked.

## CN8-O16 (RULES error): Franklin does NOT stop AI European wars
- RULES.md says "With Benjamin Franklin, AI Europeans never start a war with
  you". docs/founding_fathers.md line 102 says that effect was a fandom
  invention and was deleted (port_bugs #472/#476). Consistent with it: French
  attacked 1666 (dragoons, soldiers, galleon, caravel); French took Roanoke
  (38,29) (347 gold plundered). Fix RULES.md.

## CN8-O17 (oddity): French invasion took Roanoke (1666) while a "peace treaty with the French" popup was shown
- FORTIFY_TREATY popup "We have signed a peace treaty with the French" appeared
  on fortify (1659). Two turns later French dragoons and soldiers attacked
  Plymouth and Roanoke. Need to check whether a King war (Denmark) set the
  French at war. Not yet checked.

## CN8-O18 (oddity, costly): Tools party threw 59 tons of tools into the sea
- 1669: Stamp Act (tax 22% -> 30% offered). "Hold Jamestown Tools Party" kept
  the tax at 22% but destroyed 59 tools in Jamestown's warehouse and boycotted
  tools in Europe. RULES: never party on goods you buy. Only Fugger lifts it.
  Cheap for the tax, expensive for the stock.

## CN8-O19 (oddity): stale production "already built" loop
- Jamestown 1671-1679: production "Town Hall" repeated "is set to produce a Town
  Hall, but it has already built one!" every turn while the Town Hall 3 row
  (11) showed "build needs 100 tools, stock 65-90". Changing the build (build
  1 19) cleared it; the Printing Press then completed in ~2 turns. Unclear
  whether row 11 was owned (has_building[11]) or just stuck. Check the flag.

## CN8-B1 (candidate port bug, unverified): end-of-turn deadlock with unfortified units at 0 moves
- 1742-1744: a dozen artillery/dragoons in Jamestown had `moves 0 orders 0`.
  `endturn` refused ("no End of Turn prompt (selected unit ...)"), so no turn
  start, so no move refresh, so `fortify` refused (needs moves). `sentry` (`key s`)
  left orders 0. Only `goto` set an order (orders 3) and unblocked.
  Also: a fortified unit woke at once ("wake (was fortify)") when another unit
  of the same stack was activated in the same batch.
  Workaround in helpers/cycle.sh: fortify each guard, admit colonists, endturn.
  Check in DOS: does a unit with 0 moves and no orders block the turn?

## CN8-O20 (oddity, cost me many turns): `key e` on the main map opens the Europe screen
- Cycle helper used `key e` to leave Europe. On the main map `e` opens Europe,
  so every cycle reopened it and the turn stalled. Use `key Escape` (driver
  refuses it on the main map, harmless) or `europe`-screen only commands.
## CN8-O21 (oddity): artillery goto to Plymouth (40,30) refused; (39,30) accepted (1791)
