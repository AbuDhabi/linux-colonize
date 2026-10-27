# Campaign 6 bugs and oddities

## CN6-O1 (oddity, unsure): land-name popup appends to the pre-filled name
- Turn 1 (1493), Caravel landfall on uncharted land. The field starts with
  the seed "New England"; `type New Albion` produced "New AlbionNew England".
- Expected: unknown. Check the DOS land-name entry (does the first key
  replace the seed?). Code: src/core/name_entry_dialog.c (seed copied, no
  select-all).
- Assessed 2026-10-27: Player error: five queued Left keys reached the name field after Land Ho and moved the caret to 0 (port arrows move the caret, a documented divergence, docs/assets.md). With no stray keys the first typed key replaces the seed, as in DOS FUN_6f74_2580 raw 116950-116993.

## CN6-O2 (oddity): `s` with a selected unit that has 0 moves opens save/load
- 1557-1589, several times: `key s` on a selected caravel or colonist with
  0 moves opened the save dialog instead of sentry. Sentry works with
  moves > 0. Escape closes it. Check the DOS sentry key on 0-move units.
- Assessed 2026-10-27: Port bug, fixed as port_bugs.md #1298: plain S/L opened Save/Load; DOS has no such hotkey (S with no unit does nothing).

## CN6-O3 (oddity): `p` on a hill tile gives no feedback
- 1558, pioneer at (25,32) (hill): `key p` consumed nothing, no message.
  Expected a refusal line ("land cannot be plowed" or similar). Pioneer
  orders stayed 0. Check whether DOS refuses silently on hills.
- Assessed 2026-10-27: DOS behaviour: FUN_2b5a_0b34 raw 42225-42227 hides Clear/Plow on Hills and Mountains, so P matches nothing and DOS is silent too.

## CN6-O4 (note): lumber shortage stalls the Carpenter's Shop
- Jamestown 1532-1589: "has run out of lumber ... carpenters cannot
  continue". Hammers come from lumber, so an island with no own forest
  needs a Lumberjack on a forest tile (which costs native compensation to
  clear). Not a bug.
- Assessed 2026-10-27: Note, not a bug.

## CN6-O5 (oddity): colonist sentry does not stick
- 1557: `key s` on a free colonist in the colony left orders 0; the same
  key on the caravel worked. Re-check on a clean turn.
- Assessed 2026-10-27: Player error: the sentry stuck (opencol.1.log turn 60); the later activate woke the colonist, it stepped off the caravel, and the next S went to the selected caravel.

## CN6-O6 (oddity, unverified): the "(+N)" on the stock food line
- The colonies line prints `stock food 0(+14)`. At Jamestown 1627-1634 the
  stock showed +14 while the header said `food -4` and the stock hit 0 with
  starvation. Reading of `(+N)` as the change since last turn is a guess
  (unverified); the header "food" value is the one that predicted starvation.
- Assessed 2026-10-27: Driver bug, fixed: food `(+N)` was gross production; it now prints the net (food after eating), the header's number. Other goods: net change before warehouse waste (driver header).

## CN6-O7 (oddity, unverified): cargo drag did nothing right after colony screen opened
- 1634: `drag 85 186 132 170` (furs to a caravel hold) did nothing; the same
  drag worked after `colony 0` plus `wait 10`. Check the colony screen is
  open before dragging (the first try ran before the screen finished opening).
- Assessed 2026-10-27: Not reproducible: `colony` already waits 10 frames. The original batch sent Return/Escape after the open; the drag ran only after the reopen. Possibly an unanswered warehouse-full prompt (CN8-O13).

## CN6-N1 (my error, not a bug): fishing needs Docks; I replaced Docks with Stable / Stockade / Warehouse
- docs/terrain_yields.md:120: fishing on ocean/sea lane needs a colony Docks
  ("18ec zeroes fish jobs without it"). I set Jamestown from Docks to Stable
  (1600s) and Plymouth to Stockade then Warehouse: no Docks ever finished, so
  every fisherman (most of the food) produced 0. Starvation 1627-1640 traces
  to this. Lesson: Docks first in any colony that lives on fish.
- Assessed 2026-10-27: Player error (agent's own).

## CN6-B1 (driver friction, my error): `key Escape force` on the main map quit the game
- 1640: sent `key Escape force` on the main map (to close a stuck screen).
  That raised LOG "Exit to DOS?" (Yes/No); the following Return took Yes:
  "GAME QUIT". Recovered from trace_autosave_turn_189.sav (restart `--load`).
  Rule: never send `Escape force` on the map. Use `key Escape` on screens, and
  check the screen first (`status`). The driver's refusal on plain `key
  Escape` on the map is the guard; `force` removes it.
- Assessed 2026-10-27: Driver hardened: `key Escape force` is gone; Escape on the bare map is always refused (an open map pull-down still takes it).

## CN6-O8 (note): finished builds keep taking hammers and log "already built one"
- 1636-1732, Plymouth and Jamestown: after Schoolhouse, Stockade, Armory,
  Warehouse, Docks completed, the colony stayed on that build and kept
  logging `set to produce X, but it has already built one!` each turn while
  hammers piled up (Armory 270 hammers, Schoolhouse 204). Matches the
  RULES note ("pick the next build at once"); the message is a fair warning,
  not a bug.
- Assessed 2026-10-27: DOS behaviour: FUN_364b_0688 keeps the finished project selected and posts @ALREADYHAVE each turn (raw 57739/57783).

## CN6-O9 (note): the Crown's REF reached Jamestown 1737 and took the colony at once
- Two artillery in the colony were destroyed in the open by Regulars and a
  cavalry in two turns. The stack that landed next to the colony kept
  attacking. Expected per RULES (artillery in open is -75%). Playtest
  observation only.
- Assessed 2026-10-27: Note, DOS combat rules.

## CN6-B2 (driver note): Colonization building rows (driver `work N CI B`)
- Plymouth/Jamestown 1733 (branch 2): work rows as probed through
  `work`: 9 Town Hall, 10 Armory, 21-23 Weaver's House, 24-26 Tobacconist's,
  27-31 Rum Distiller's, 32-34 Fur Trader's, 35-36 Carpenter's Shop,
  37-38 Church, 39-52 Blacksmith's House. A refused `work` (building full)
  keeps the old building: read the building name after each probe.
- Assessed 2026-10-27: Driver note.
