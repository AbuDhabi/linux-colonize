# Campaign 6 bugs and oddities

## CN6-O1 (oddity, unsure): land-name popup appends to the pre-filled name
- Turn 1 (1493), Caravel landfall on uncharted land. The field starts with
  the seed "New England"; `type New Albion` produced "New AlbionNew England".
- Expected: unknown. Check the DOS land-name entry (does the first key
  replace the seed?). Code: src/core/name_entry_dialog.c (seed copied, no
  select-all).

## CN6-O2 (oddity): `s` with a selected unit that has 0 moves opens save/load
- 1557-1589, several times: `key s` on a selected caravel or colonist with
  0 moves opened the save dialog instead of sentry. Sentry works with
  moves > 0. Escape closes it. Check the DOS sentry key on 0-move units.

## CN6-O3 (oddity): `p` on a hill tile gives no feedback
- 1558, pioneer at (25,32) (hill): `key p` consumed nothing, no message.
  Expected a refusal line ("land cannot be plowed" or similar). Pioneer
  orders stayed 0. Check whether DOS refuses silently on hills.

## CN6-O4 (note): lumber shortage stalls the Carpenter's Shop
- Jamestown 1532-1589: "has run out of lumber ... carpenters cannot
  continue". Hammers come from lumber, so an island with no own forest
  needs a Lumberjack on a forest tile (which costs native compensation to
  clear). Not a bug.

## CN6-O5 (oddity): colonist sentry does not stick
- 1557: `key s` on a free colonist in the colony left orders 0; the same
  key on the caravel worked. Re-check on a clean turn.

## CN6-O6 (oddity, unverified): the "(+N)" on the stock food line
- The colonies line prints `stock food 0(+14)`. At Jamestown 1627-1634 the
  stock showed +14 while the header said `food -4` and the stock hit 0 with
  starvation. Reading of `(+N)` as the change since last turn is a guess
  (unverified); the header "food" value is the one that predicted starvation.

## CN6-O7 (oddity, unverified): cargo drag did nothing right after colony screen opened
- 1634: `drag 85 186 132 170` (furs to a caravel hold) did nothing; the same
  drag worked after `colony 0` plus `wait 10`. Check the colony screen is
  open before dragging (the first try ran before the screen finished opening).

## CN6-N1 (my error, not a bug): fishing needs Docks; I replaced Docks with Stable / Stockade / Warehouse
- docs/terrain_yields.md:120: fishing on ocean/sea lane needs a colony Docks
  ("18ec zeroes fish jobs without it"). I set Jamestown from Docks to Stable
  (1600s) and Plymouth to Stockade then Warehouse: no Docks ever finished, so
  every fisherman (most of the food) produced 0. Starvation 1627-1640 traces
  to this. Lesson: Docks first in any colony that lives on fish.

## CN6-B1 (driver friction, my error): `key Escape force` on the main map quit the game
- 1640: sent `key Escape force` on the main map (to close a stuck screen).
  That raised LOG "Exit to DOS?" (Yes/No); the following Return took Yes:
  "GAME QUIT". Recovered from trace_autosave_turn_189.sav (restart `--load`).
  Rule: never send `Escape force` on the map. Use `key Escape` on screens, and
  check the screen first (`status`). The driver's refusal on plain `key
  Escape` on the map is the guard; `force` removes it.

## CN6-O8 (note): finished builds keep taking hammers and log "already built one"
- 1636-1732, Plymouth and Jamestown: after Schoolhouse, Stockade, Armory,
  Warehouse, Docks completed, the colony stayed on that build and kept
  logging `set to produce X, but it has already built one!` each turn while
  hammers piled up (Armory 270 hammers, Schoolhouse 204). Matches the
  RULES note ("pick the next build at once"); the message is a fair warning,
  not a bug.

## CN6-O9 (note): the Crown's REF reached Jamestown 1737 and took the colony at once
- Two artillery in the colony were destroyed in the open by Regulars and a
  cavalry in two turns. The stack that landed next to the colony kept
  attacking. Expected per RULES (artillery in open is -75%). Playtest
  observation only.
