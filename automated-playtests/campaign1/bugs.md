# Bugs found in campaign 1

Each entry: what happened, where, evidence. "Suspected cause" lines are from a
quick source read, not a verified diagnosis.

## B1. Build Colony consumes a unit on the cursor tile, not the selected unit

- 1497 (turn 5), (32,57). Pioneer (id 2) and Veteran Soldier (id 3) on the
  same tile. Cycled with W until the soldier was selected (sidebar and log:
  `COMMAND build_colony (unit Soldiers id=3 ...)`), pressed B, named the
  colony. The colony was founded with the **pioneer**: log line
  `Founded colony 'New Amsterdam' at (32,57) pop=1 tools=100 muskets=0`,
  and the soldier stayed outside.
- Expected (DOS): the active unit founds the colony.
- Suspected cause: `game_try_found_colony_at_cursor` (src/core/game_dialogs.c)
  picks the founder with `units_id_at(&game->units, cx, cy)` (first unit on
  the tile) instead of the selected unit.
- Log: `bin/opencol.3.log` (the run that started from `saves/t3.sav`).

## B2 (minor). Goto-port list logs as "cheat list"

- 1497, G on the Merchantman opens "Select a port to sail to"; the debug log
  says `VIEW map -> cheat list` (the screen name for the shared list dialog).
  Cosmetic, debug.logs only.

## B3. After loading a save, Go-To units sit still and the turn stalls

- Load `saves/trace_autosave_turn_027.sav` (1519): the Merchantman has
  orders Go To New Amsterdam (`orders 3`) and full moves. Nothing moves, no
  unit is selected and the sidebar shows no "End of Turn" prompt; the game
  idles forever (3000+ frames). Pressing Return runs the go-to (the ship
  sails 5 tiles) and ends the turn.
- Expected: the unit cycle resumes the go-to after a load, as it does at the
  start of a normal turn.
- Repro: `play_driver DIR --load saves/trace_autosave_turn_027.sav`, then
  `auto 3000`, `units`.

## B4. A selected fence unit in the colony screen cannot be dragged

- Colony screen: pressing on the fence (outside-colony) unit that is already
  selected opens the role popup (`Select a Profession for ...`) on the press,
  so it can never be dragged onto a building or tile; the first press on an
  unselected fence unit selects it and does start a drag. Several of my drags
  turned into role popups this way (1528 New Amsterdam, 1550 Fort Orange).
  Workaround: click a worker in a building first, then drag the fence unit.
- Code: `game_loop_update.c` COLONY_HIT_OUTSIDE_UNIT branch
  (`csv->selected_outside_unit == ouid` -> `game_colony_fence_drop`). Not
  checked against DOS; DOS may open the popup only on a release without a
  move.

## B5. Pioneer's tools reset to 100 after a round trip through Europe transit

- 1531: Pioneer with 60 tools (id 68) boarded the Merchantman at New
  Amsterdam; the ship was sent to Amsterdam by mistake and I dragged it from
  "Expected soon" back to "Bound for New Netherlands" in the Europe screen.
  When it arrived the pioneer (now id 78) had **100** tools. Free 40 tools.
- Suspected cause: `EuropeHarborShip` keeps only `cargo_types` /
  `cargo_professions` for passengers, so the unit is rebuilt with the
  profession's default equipment.

## B6. Units left without orders and no End of Turn prompt after leaving a colony screen

- 1544 and 1555: a ship with full moves and no orders, plus colonists that had
  just disembarked, are on the map; after closing the colony screen no unit
  is selected and the sidebar shows no "End of Turn". Pressing G does nothing,
  Return opens the colony under the cursor. Clicking the ship's tile (stack
  list) or its sidebar row selects it again.
- Same family as B3: the unit cycle does not pick up units that still need
  orders once the map regains focus.

## B7 (minor). `selected_id` points at a foreign unit during AI turns

- During end-of-turn popups the selected unit is reported as e.g.
  `unit 82 Caravel at (245,245)` (another nation's ship in Europe). Seen in
  the driver's `status`; no visible effect found.

## B8. Activate (A) can only ever pick the first unit of a colony stack

- War of Independence, New Amsterdam with 6 artillery, 4 dragoons and a
  Continental unit, all fortified. Pressing A (ORDERS > Activate unit) always
  selects the same unit: `MAP_MENU_ACTION_ACTIVATE_UNIT` uses
  `units_id_at(cursor)`, the first unit on the tile. Clicking the colony tile
  opens the colony screen, so the stack chooser (`unit_stack_try_open`)
  is never offered on a colony tile, and the colony screen's fence popup only
  re-equips colonists. Net effect: from a colony you can attack with exactly
  one chosen unit per turn; as soon as it has moved, no unit is pending and
  the turn ends by itself. (1615, `saves/attempt1/`)
- Expected (DOS): some way to pick any unit of the stack (the docs say
  "Stack popup wake then select").

## B9 (observation). Sidebar unit rows are not clickable

- Clicking a unit row in the map sidebar does not select that unit:
  `game_update_map_menu_bar` treats any sidebar click as "wait / next unit"
  (or End of Turn while the prompt is up). It looked like row selection
  because the next unit in the queue happened to be the one I clicked.
  Not checked against DOS.

## B10. A unit dropped on an ocean tile can get a land job

- 1601 Fort Orange: dragging a pioneer from the fence onto an ocean tile made
  him a "Sugar Planter" producing 0 sugar (job menu: `Select a Profession
  for Free Colonists (Sugar Planter)`). Other ocean drops gave Fisherman.

## B11 (observation). Go To routes ships past enemy coastal forts

- 1613: after the REF took Fort Orange, a Merchantman ordered "Go to New
  Amsterdam" from the south-west of the island routed straight past the
  captured fort; `Fort at Fort Orange opens fire on Rebel Merchantman`,
  sunk. Probably DOS-faithful pathing; worth knowing.

## B12 (observation). The REF uses the old Spanish slot and gets the Spanish bonus

- After Spain was absorbed by England (1546) the Crown's REF played as
  nation 2. Its attacks on Inca cities near Fort Orange list
  `Spain Bonus +50%` in the combat log. Whether DOS gives the Crown the
  Spanish ambush bonus in this case is unverified.

## B13 (minor). Victory end screens are labelled "Retired"

- 1652, after "Royal Expeditionary Force annihilated!": the status text on
  the score and exploits screens reads `Retired — Colonization Score: ...`
  and `Retired — Exploits: ...` although the game was won, not retired
  (`game_status_text`). The screens themselves look right.

## B14 (observation). Hall of Fame names the republic "Surinam"

- The victory Hall of Fame entry reads `President, Republic of Surinam`
  while the land was named New Netherlands at landfall. Probably the DOS
  per-nation republic name; unverified.

## Notes that are not port bugs

- The King's concession text "far more importatnt matters" is a typo in the
  shipped COLONIZE/GAME.TXT.
- The play driver (not the game) deadlocked once: Combat Analysis runs a
  nested input loop inside `game_update`, which could not see later stdin
  commands. Fixed in the driver.
