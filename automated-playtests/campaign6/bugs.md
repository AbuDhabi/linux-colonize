# Campaign 6 bugs and oddities

## CN6-O1 (oddity, unsure): land-name popup appends to the pre-filled name
- Turn 1 (1493), Caravel landfall on uncharted land. The field starts with
  the seed "New England"; `type New Albion` produced "New AlbionNew England".
- Expected: unknown. Check the DOS land-name entry (does the first key
  replace the seed?). Code: src/core/name_entry_dialog.c (seed copied, no
  select-all).
