# Campaign 4 bugs and oddities

Numbered C4-Bn (bugs) and C4-On (observations). Severity: high = loses the
game / blocks play, medium = wrong rules or misleading UI, low = cosmetic or
driver-only.

## C4-B1 (low): Spain's nation-bonus page shows "50%%"

- New game, Spain, the second lore page (GAME.TXT `@NATION2B`) reads
  "receives a 50%% combat bonus" (`shots/d8.png`). The text file escapes the
  percent sign as `%%`; in-game popups built by `popup_msg_fill` print it as
  "50%" (e.g. the rebel-sentiment message from `@...` line 2546 shows "{50%}").
- `src/core/new_game.c` `new_game_enter_lore` loads the section with
  `new_game_load_choice_section`, which copies the raw lines; nothing turns
  `%%` into `%` on the lore / king pages. DOS runs these through its text
  formatter, so it shows a single "%".

## C4-B2 (medium): colony building layout changes after save/load

- Santo Domingo before the reload (turn 110, `shots/sd4.png`): Carpenter's
  Shop in the middle of the building area (around 1x (142,62)); after
  `--load saves/t110.sav` (`shots/sd8.png`) the Carpenter's Shop is at the top
  left (1x (27,28)) and the other buildings moved too. Isabella changed the
  same way (Lumber Mill moved from (145,65) to (40,78); Docks took its old
  place).
- The layout is a pure display matter, but it means the colony screen of a
  reloaded game does not match the screen before saving (and presumably not
  the DOS screen of the same save). Looks like a per-colony layout/seed that
  is not saved or is re-derived from something that changes on load.

## C4-O1 (observation): keyboard step onto the west high seas never asks "sail to Europe?"

- Caravel moved by keypad from (2,5) onto the western high seas (1,5), then
  (1,6): no @SAILHOME prompt. A Go To onto a west high-seas tile does sail to
  Europe (and the ship comes back on the east side).
- `game_move_sea_unit` cites DOS 4720 reason 4/5 (east/west rim push and an
  *eastward* step deeper into the lane). If DOS really asks only for eastward
  steps this is DOS behaviour; noted because the west lane looks like a dead
  end to a keyboard player.

## C4-O2 (observation): Dutch declared war over a scout in their land

- Turn 149 (autumn 1620): after my Seasoned Scout had explored around New
  Amsterdam for ~30 turns, the Dutch sent "your presence continues to befoul
  New Netherlands" and in the same message pump "Prepare for WAR!", then
  killed the scout. The messages are the DOS ones; the war then cost
  Santo Domingo (see README). Kept as an observation (AI behaviour).

## C4-B3 (low): customs-sale log line prints the human's purse for any nation

- `debug.logs`: a Crown-held San Salvador's Custom House sales log
  `EUROPE customs San Salvador total 2250$ (nation=0, gold=937)`; 937 is
  Spain's (my) treasury, constant across sales. `src/core/europe_market.c`
  ~487 prints `eu->gold` (the live human purse) whatever `nation` is. The
  credit itself goes to the right record (`europe_nation_gold_add`), so this
  only misleads log readers.

## C4-O3 (observation): more than 50 units on a colony tile

- Isabella held ~66 units (56 guns, cavalry, colonists). The colony screen's
  outside row lists only the first 0x32 = 50 (DOS FUN_15eb_09c0 clamp,
  port_bugs.md #1078), so a Cont. Army that had lost its horses could not be
  dragged to the fence to re-horse. DOS-faithful; worked around with the new
  driver command `equip UID ROLE`.

## C4-O4 (observation): a fortifying stack in the open gets one attack

- Turn 546 at (23,9): 8 guns given F the turn before (orders 5). Gun 84 woke
  with 3 moves and attacked San Salvador; every other gun of the stack then
  read `wake (was fortified)` with mp=0 (`bin/opencol.log`). The fortify
  completed and took their moves when the turn cycle reached them. Guns
  fortified inside a colony attacked one after another normally. Not checked
  against DOS.

## C4-O5 (observation): Crown-held colonies build Pioneers that clear the ambush terrain

- While the Crown held San Salvador it repeatedly spawned Pioneers at (24,9)
  (`Spawned unit ... type=Pioneers at (24,9)` right after the colony's PROD
  line), and the forest there became grassland. The Rebel ambush bonus on
  that tile went from +50% to 0. AI colony behaviour, noted for the record.
