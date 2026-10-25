# Campaign 5 bugs and oddities

Numbered C5-Bn (bugs) and C5-On (observations). Severity: high = loses the
game / blocks play, medium = wrong rules or misleading UI, low = cosmetic or
driver-only.

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
