# Campaign 5 bugs and oddities

Numbered C5-Bn (bugs) and C5-On (observations). Severity: high = loses the
game / blocks play, medium = wrong rules or misleading UI, low = cosmetic or
driver-only.

Filed 2026-10-26 as port_bugs.md #1270 (O1), #1271 (O4) and #1272 (O5), all
REFUTED (DOS behaviour). Driver-only: O2 fixed (`field` now refuses native
land at peace), O3 refuted (the End of Turn option was off; with it on the
turn waits), B1 fixed.

## C5-O1 (medium?): an Iroquois brave at peace (alarm 24) killed a lone Scout

- Turn 61 (1553), `bin/opencol.log`: `COMBAT land attacker=Braves (nation 7,
  id 34) defender=Scouts (nation 3, id 53) at (39,51)`, "Attack Bonus +50%
  Ambush +50%", 69%, attacker wins; popup "{Iroquois} ambush {Dutch Scouts}
  near Wilderness!". The Seasoned Scout (50 horses) was destroyed.
- State (trace autosave turn 62): Iroquois `alarm_by_player[Dutch]` = 24,
  `euro_diplo` = 0x60 (met + peace); village (38,51) `alarm[Dutch]` friction
  0, attacks 0. The scout had just stepped next to that village on its way
  to speak with the chief; no Dutch soldiers were near it.
- Code read (src/core/ai_native_021a.c): the brave's direction picker does
  not read the peace bit at all. With alarm <= 0x19 a target is skipped
  unless it is "loot" or the tile owner has a grudge (`att >= 0x80`, alarm
  >= 0x4b, or the owner is the village's threat nation); a Scout is never
  loot. So the attack needed a grudge latch that the save does not show,
  unless the "tile owner" read (ai_native_021a.c:443, alarm read from the
  tile owner, not the unit's nation) resolved to something else. Not traced
  further; `AI_021A_TRACE=1` on `saves/trace_autosave_turn_061.sav` would
  show the score.

## C5-O2 (low, driver): `field` bypasses @INDIANLAND

- `field N CI TILE JOB` calls the field-assign writer directly. On a native
  plot the colony screen asks @INDIANLAND (respect / pay N / take); the driver
  silently takes and charges the land-work alarm every time. 32 probes on
  turn 15 raised the Iroquois alarm from 12 to 88. Driver-only; the game's
  own drag path asks correctly.

## C5-B1 (low, driver, fixed): `colonies` / `colony N` lose colonies after a colony is destroyed

- Turn 130: `colonies` stopped listing Fort Nassau, `colony 14` / `work 14`
  answered "ERR no colony 14". The colony was fine (the save written then
  still had it, and turn processing kept producing it).
- Cause: `colonies_abandon` (src/core/colony_build.c) clears the slot in
  place and sets `colony_count` to the number of live colonies, so live
  colonies can sit past `colony_count` (colony.c already says so). The
  driver looped `i < colony_count`. Aztecs had burned French Cayenne (slot
  12), so Fort Nassau in slot 14 fell outside the loop.
- Fixed in tools/play_driver_main.c: walk all `COLONIZE_COLONIES_MAX`
  slots, refuse inactive ones. Game code already walks the whole pool.

## C5-O3 (low, driver): the nested-modal auto-Return also ends the turn

- With every other unit fortified or sentried, the game shows the End of
  Turn prompt as soon as the active unit spends its moves, and the driver's
  nested-modal auto-Return (300 idle polls) answers it. The first war turn
  ended after one attack (turn 468, `saves/branch3/`). Game Options "End of
  Turn" was off in this save; switching it on did not change the driver's
  behaviour. Workaround: wake all units that should still act first.

## C5-O4 (observation): a Jesuit Missionary soldier demotes to Missionaries

- Turn 473 (`saves/branch3/`): Soldier 167 (profession Jesuit Missionary)
  lost an attack and became "Missionaries" with 3 moves left, not
  Colonists. `units_combat_demote_type_index` cites it as the FUN_5fef_0352
  demote table, so probably DOS behaviour; noted because it surprises.

## C5-O5 (observation): ship logistics

- Sentried land units on a colony tile with a ship in port wake up at the
  start of the next turn (seen repeatedly in New Amsterdam, turns 446-457),
  so they must be re-sentried right before the ship sails, or they stay.
- A ship on Go To whose path crosses one of my colonies drops its
  passengers there (turn 479: a Frigate carrying 4 guns from (45,46) to
  Fort Orange unloaded them in New Amsterdam on the way). Consistent with
  "arriving passengers are put ashore the moment the ship docks", but easy
  to trip over.
- Units unloaded in a colony keep their sentry order, so the next ship
  leaving that colony takes them again (turn 478).
