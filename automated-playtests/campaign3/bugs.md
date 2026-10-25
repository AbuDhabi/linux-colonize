# Campaign 3 bugs and oddities

Numbered C3-Bn (bugs) and C3-On (observations). Severity: high = loses the
game / blocks play, medium = wrong rules or misleading UI, low = cosmetic or
driver-only.

## C3-B1 (medium): AI dragoon captures a wagon train right after first-contact peace

- Spring 1601 (turn 109-110, `saves/t110.sav` is just after). An English
  Dragoon (unit 61) moved from (30,27) to (29,26), next to my Wagon Train at
  (29,25) near Guadeloupe. That step raised first contact: the English
  greeting, the demarcation treaty (answered Yes) and "We welcome the
  friendship of our brothers the French" (answered "Go in peace"). The same
  dragoon then attacked the wagon train on the same move:
  `COMBAT land attacker=Dragoons (nation 0, id 61) defender=Wagon Train
  (nation 1, id 58) at (29,25)`, "French wagon train CAPTURED by the English!"
  (`bin/opencol.log` around line 4950).
- After the turn both nations' `euro_relation` bytes for each other are 96
  (0x60 = met + peace), so the attack happened under a peace treaty.
- Expected: once the treaty is signed, the AI unit's attack decision should see
  peace (or the attack target should have been chosen before contact and
  dropped). Lost 50 horses and the only wagon train.

## C3-B2 (low): Europe colonist shows "tools 100" after save/load

- A Free Colonist bought/recruited in Europe and carried on the Caravel
  (unit 144 before the reload, unit 58 after) had no tools before
  `saves/t62.sav`; after reloading it it reports `tools 100`
  (`units` output: "Colonists ... prof 28 aboard 57 tools 100").
- The saved record has `cargo_hold = [0,0,255,0,0,100]`, the same bytes the
  Caravel carries: it looks like the DOS spawn-init pattern. The loader
  (`src/core/col1_bridge.c` ~1932, `else if (src->cargo_hold[5] <= 100 || kind
  == PIONEER) u->tools = cargo_hold[5]`) reads hold[5] as tools for every
  non-transport land unit, not only Pioneers.
- Joining a colony did not add the 100 tools to the warehouse (checked: Quebec
  stayed at 40), so the phantom tools seem harmless, but the unit state is
  wrong and may matter for code that reads `u->tools` on a non-pioneer.

## C3-B3 (low, driver): `colony N` opened an empty colony screen

- `play_driver`'s `colony N` passed the `colonies` print index to
  `game_enter_colony`, which takes the colony id. Once the ids and indices
  diverged (Montreal index 5) the screen came up blank ("Entered ;
  colony:?"). Fixed in tools/play_driver_main.c (index -> id).

## C3-O1 (observation): Lost City Rumours mostly "nothing"

- Of the first 8 rumours (1 hardy pioneer burial mound skipped, then a Scout,
  later a Seasoned Scout), 7 gave "You find nothing but rumors." and one gave
  Cibola (4800, mountain). The driver ran unseeded (`--load`) for all but the
  first, so the reseed comes from the wall clock. With the case table in
  `units_lcr_roll_outcome` roughly 5/9 of rolls end in "nothing" at skill 2,
  so this is probably bad luck (about 6%), noted only because campaign 2 saw
  the same pattern.

## C3-O2 (observation, possible AI bug): Dutch units camp on Quebec's tiles

- From about 1595 (turn ~100) to at least autumn 1615 (turn 139) Dutch units
  stand on all three land tiles next to Quebec (34,31): a Wagon Train at
  (34,30) that has not moved for 30+ turns (orders 0), and a rotating set of
  Dragoons / Soldiers / an Artillery at (33,30) and (35,30). The fourth
  neighbour is a Sioux camp, the rest is water.
- Effect: none of my land units can leave Quebec while all three tiles are
  occupied; `goto` from Quebec silently burns the unit's moves (log shows
  `ORDER ... goto (32,28)` each turn, no MOVE). France and the Netherlands
  are at peace (relation 0x60 both ways).
- In the contact dialog, "First you must withdraw your forces from our
  colonies!" gets "Our forces protect valid Dutch interests and shall not be
  moved."
- Traced (turn 167 save, `sav_json`): the Wagon Train at (34,30) is my own
  first wagon. The English captured it in 1601 (C3-B1), a Dutch dragoon took
  it from the English two turns later, and it still carries its Quebec origin
  binding (`goto (34,31)`, plan '5' = walk to origin colony; holds 50 horses).
  `ai_euro_20e6_wagon_origin_walk` keeps a captured wagon walking to its old
  origin colony (port_bugs.md #814, DOS-literal: "a captured Dutch wagon heads
  for New Amsterdam"), so it can never arrive and never leaves. The soldiers
  have plan 'F' = `ai_euro_20e6_border_park_arm`, "stay put next to the
  foreign border colony", also DOS-literal.
- So probably DOS behaviour, but the combination (a stuck captured wagon plus
  border-park soldiers on every free land neighbour) seals a coastal colony
  off by land for the rest of the game.
- Later (1750-1786) about 15 **Spanish** dragoons (orders 0) stood on every
  land tile around Guadeloupe (32,28) and Quebec, at peace, for decades. My
  wagon train with 100 tools + 100 ore never got out of Guadeloupe, and its 13
  guns could not walk to Quebec.

## C3-B4 (medium): ships cannot trade with a foreign colony (Jan de Witt)

- 1662 (turn 233), Jan de Witt in Congress, France and the Netherlands at
  peace. My Merchantman (furs + 120 rum) at (40,30) moved NW into Dutch New
  Amsterdam (39,29): nothing happens, no popup, no status text, the ship keeps
  its moves (log: just the key press). A move into open water from the same
  tile works.
- Code: `game_try_unit_move` runs `game_move_sea_unit` before
  `game_move_colony_prompts`. For a ship and a land destination that is not a
  friendly colony, `game_move_sea_unit` probes `units_enter_probe_w`; anything
  but LANDFALL / VILLAGE_SHIP goes to `game_report_enter_reason` and returns
  FALSE (game_loop_orders.c ~395-415). The FUN_5f7a_0662 foreign-trade arm
  ("every Wagon Train and every ship — bumping a foreign Euro colony runs
  FUN_5f7a_020e", game_loop_orders.c ~845) sits in the later stage, so a ship
  never reaches it; only wagon trains can.

## C3-O3 (medium, DOS bug TB-14?): Dutch AI rewrites my colonies' jobs and build queue

- Turn 273 (1682), during `TURN european nation 3` (Dutch), right after
  `Spawned unit id=1886 type=Pioneers at (35,29)`, the log shows every Quebec
  worker taken off its plot and re-seated: `COLONY Quebec: colonist #N off
  field tile ...` / `-> field tile ... as Farmer|Fisherman`, statesmen and
  preachers moved to Armory / Blacksmith / fields (bin/opencol.log ~51297).
  Result: the Expert Lumberjack fishing, farmers on the lumber tiles, bells
  72 -> 48, crosses 15 -> 2, lumber -32 a turn.
- Again in 1702 (turn 312) on Guadeloupe: same pattern right after
  `Spawned unit ... type=Pioneers at (34,29)` (log ~66294), and this time the
  build queue changed too (my Artillery -> Schoolhouse), statesmen moved into
  the Armory, bells 13 -> 1.
- Third time around 1771 (turn ~450), again Guadeloupe: five workers were made
  fur trappers on the forest tiles (furs +62, lumber 0, hammers 0) and the
  build was Schoolhouse again ("is set to produce a Schoolhouse, but it has
  already built one!").
- Cause: this is tolerated DOS bug TB-14 (docs/tolerated_dos_bugs_ai.md).
  On a `turn % 7 == 0` tick with tools, the Dutch AI builds a road through a
  phantom Pioneer (`ai_euro_5952_tools_supply_and_connect`); the road
  completion rebinds the "current colony" to the colony nearest the gap tile,
  and the rest of the AI colony tick (improvement arm, idle sweep, food and
  job passes, build cascade) runs on that colony. Here the nearest colony was
  a human one (Quebec, Guadeloupe), so the AI rewrote a human colony's jobs
  and production. TB-14 only describes the AI-on-AI effect; worth checking
  whether DOS's nearest-colony search (FUN_281f_0614 / FUN_15eb_002c) can
  really return another nation's colony, because for the human the effect
  is drastic.

## C3-O4 (observation): Custom House sells a boycotted good

- 1714 (turn 335): "Quebec sells 52 Rum for 572. 62% Tax: 354." from the
  Quebec Custom House, while Rum was under the 1689 boycott (the next Europe
  visit, turn 336, still demanded 6000 in back taxes to lift it). Worth
  checking whether DOS's custom house (FUN_... export pass) skips boycotted
  goods; the Col1 manual says boycotted goods cannot be traded in Europe.

## C3-O5 (observation, high impact, DOS TB-47 + FUN_1427_06b4): the 300-unit cap

- From about turn 430 (1765) the map held 300 units: Dutch 118, Spanish 93
  (mostly dragoons parked next to other nations' colonies), France 66, natives
  23. `units_spawn_room` (DOS FUN_1427_06b4) lets the human create units below
  300, but AI nations, natives and the Crown only below 292.
- Effect 1 (TB-47, tolerated DOS bug): every artillery Quebec completed after
  that charged 192 hammers and 40 tools and produced nothing, with a
  "Quebec colony produces Artillery" message plus @TOOMANYUNITS. Three guns
  (576 hammers, 120 tools) were lost before I noticed. Disbanding one idle unit
  (Shift+D) in the turn a gun completes makes the gun appear.
- Effect 2 turned out not to matter: I expected the REF landing to stall
  (`ai_king_0982_invasion` spawns through `units_spawn_room(crown)`, which needs
  fewer than 292 units), but the declaration itself (FUN_43f7_1a26 / 0108,
  `ai_king_audience.c` ~1020) withdraws the two other Euro powers and despawns
  every one of their units. The map went from 300 units to 90 in one turn and
  the REF landed at once (1787).
- Not a port bug (DOS does the same), but in a long game the cap is reached
  by AI armies alone, and the only signal is the @TOOMANYUNITS popup among the
  end-of-turn messages, while the colony still reports "produces Artillery". `divergences.unlimited_units` in settings.json lifts it
  (not used in this campaign).

## C3-B5 (low): Indian massacre popup names nobody ("{} dies fending off onslaught")

- Turn 482 (1787), the Inca killed a Montreal colonist: "{Inca} massacre
  {Rebel} colonists at {Montreal}! {} dies fending off onslaught."
  (`out.txt` ~line 94578).
- GAME.TXT @INDIANWINCOLONY fills `%STRING2` with the colonist who died. The
  port (`src/core/units_capture.c` ~139) passes
  `reports_job_short_name(UNITS_JOB_NONE)`, which is empty, instead of the
  removed colonist's occupation (`col->colonists[pick]` before it is shifted
  out).

## C3-O6 (observation): the Dutch won independence before me

- Between turns 300 and 350 (about 1700) the Dutch nation got bit 0x04
  (`turn_year_end.c` @OTHERGRANTED, raw 58605: AI rebellion granted). I never
  saw the popup (the driver's `auto` answers message popups), and the Dutch
  kept playing normally until my declaration withdrew them.
- Effect at the end: the score screen says "Independence Achieved (1 prior
  nations): +50%" and the total is 400 x 1.5 = 600. In the 2x screenshot the
  `%` glyph reads like a `2` ("+502"), which looks like a wrong score at first
  glance; the arithmetic is right.

## C3-O7 (observation, DOS-literal): one naval fight sank two men-of-war

- 1812 (turn 532): my two intervention men-of-war 703 and 728 shared tile
  (33,33). One Tory Man-O-War attack (59%) sank both, each with its troops.
  This is DOS FUN_5fef_0ec0: every unit on the losing tile takes its own
  loss, and during the war a beaten rebel ship with no drydock always sinks
  (`units_combat.c` ~2155). Worth knowing, because the land rule is different:
  a normal land attack only hurts the picked defender.
