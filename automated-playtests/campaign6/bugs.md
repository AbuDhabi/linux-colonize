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
