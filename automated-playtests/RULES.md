# Rules for player agents

Read this before playing a campaign. It holds what campaigns 1-7 learned, so
you do not need to read game source to learn the rules. Read source only to
confirm a suspected bug.

## For small models (Haiku-class): read this first

Campaign 6 (Haiku 5.5) read this file once at turn 0 and did not apply it
later. It declared independence with 4 artillery and no Fortress, where the
War of Independence section asks for a Fortress first. It skipped the Docks
rule for a fishing colony. It sent `key Escape force`: the Exit to DOS popup
defaults to Yes, so the next Return quit the game. It answered Congress popups
with a blind `choose 0`, and it filed a rule it had read as a bug. Rules read
once are not enough. These steps make them checkable.

1. Gate each irreversible step. Before declaring independence, answering a
   Congress pick, or changing a colony's build away from a building a rule
   names, write a gate line in the journal: the rule (section name), each
   condition, met or not met. One condition not met means do not do the step.
   Keep playing and note the wait.
2. The declaration gate is the `advise` GATE lines (see "The advise loop").
   They scale with the REF, which `advise` prints. Do not use any other
   number. Campaigns 6 and 7 used the Viceroy "60+ guns" figure on
   Discoverer, judged it out of reach, stopped preparing and declared with
   2-7 guns. On Discoverer the REF was 23-30 land units: the real gate was
   about 20-24 guns and 9-11 dragoons.
3. A rule beats your plan. Stop and re-check when your reasoning says "not
   realistic", "too slow", "I'll skip it", or "the stock is big, so this does
   not apply". Campaign 6 skipped the Docks rule because Jamestown's food stock
   read 114.
4. Re-read the section you need before each gate, for example
   `grep -n -A20 "Before declaring" automated-playtests/RULES.md`. Re-read this
   whole file at the start of each era. Do not trust your memory of it.
5. Never send a `force` flag. To close a screen, send `status` first, then
   plain `key Escape` on that screen. The driver refuses Escape on the main
   map (it would ask Exit to DOS).
6. Never answer a popup blind. Loops stop on `CHOICES:`. Read the rows and
   send `choose N` for the row you mean. A `choose 0` loop picked Peter Minuit
   (1587) and Jakob Fugger (1717) by accident in campaign 6.
7. Before filing a bug, grep `docs/dos_bugs.md`, `docs/known_divergences.md`
   and this file for the key words, and write the results in the entry.
   Anything not confirmed is `CN-On`, not `CN-Bn`. Campaign 6's CN6-O8
   restated the "pick the next build at once" rule.
8. Declare as soon as `advise` prints `GATE ALL MET`. The REF grows while
   you wait (the King adds units every few years), so a later declaration
   only needs more guns. The 1800 cutoff (turn 507) is the last chance, not
   the plan.
9. Write the era plan in the journal before the era starts: target turn,
   gates, what must be built. Check it at each era start. Campaign 6 planned
   to "declare after the defence is in place" (3+ artillery in Jamestown,
   target 1740-1750), then declared in 1733 with 2 artillery there.
10. Unclear screen: stop. Send `status`, take one `shot` if you need it, then
    decide. Do not guess clicks or keys. Campaign 6 guessed row positions to
    save screenshot cost.

## The advise loop (every turn)

`advise` is a driver command that runs the checks this file asks for. Put it
in the turn helper after every `endturn auto`. Lines:

- `ADVISE WARN ...`: fix it before the next `endturn` (starvation, fishing
  with no Docks, finished building still selected, hammers +0, tools short
  at completion, Tory penalty, unit with no orders, 1800 cutoff).
- `ADVISE TODO ...`: plan work (second colony, gold idle, tools needed later).
- `ADVISE REF ...`: the Crown's force. Land = regulars + cavalry + artillery.
- `ADVISE colony ... coastal`: SoL, fortification, guns and the REF target
  score of each coastal colony.
- `ADVISE GATE ...`: one line per declaration condition, MET or NOT MET.
  `GATE ALL MET` = declare now.
- `ADVISE WAR ...` (after declaring): Crown land units near your colonies.
  `landed this turn, attack it now` = hit it with dragoons from the open.

A turn is: `endturn`, `auto`, `advise`; answer every `CHOICES:`; fix every
WARN; then the next turn. If a WARN cannot be fixed this turn, write why in
the journal.

## Playbook (Discoverer; campaign 1 won this way in 1652)

Campaign 1 won with two coastal colonies of 12-14 colonists. Aim for that,
not for a big empire. Era targets (check them with `advise` and `colonies`):

1. 1492-1510. Found colony 1 on the coast on a hill or by ore and forest,
   not next to a village. Docks first if anyone fishes. Scout the chiefs
   and rumours for gold. Every Europe trip brings colonists back: the ship
   never sails empty.
2. 1510-1560. Found colony 2 on the coast. Each colony: lumberjack +
   Carpenter's Shop worker (hammers), food first (2 per colonist), then
   Stockade, Docks, Lumber Mill. Sell furs, silver, ore, tools surplus.
3. 1560-1620. Town Hall statesmen in both colonies (bells: SoL, and
   Founding Fathers). Fort, then Fortress (pop 8; campaign 7 needed 200
   tools at completion).
   Pick George Washington when offered. Buy tools by the 100.
4. 1620-1650. Artillery from both colonies' builds and from Europe with all
   spare gold. Horses in a Stable colony (2+ horses and a food surplus).
   Turn veteran soldiers and colonists into dragoons. SoL 100% in both.
5. `GATE ALL MET`: `declare`, `choose 1`. Then the War of Independence
   section below.

## Setup

- Run everything from the repo root. Build with `make build`.
- Campaign dir `automated-playtests/campaignN/`: `README.md` (journal),
  `bugs.md`, `shots/`, `saves/`.
- New game: `python3 tools/play.py start DIR --seed N --live-ticks`.
  From the title menu `key Down` (Start a Game in AMERICA), `key Return`;
  then each step prints `STATUS new game: ...` with `CHOICES (new game)`:
  `choose N` for map / difficulty / nation, `key Return` for the rest.
  Plain `--seed` pins the timer word like VR_SEED.EXE, so every reseeded roll
  (chief's gift/tales, rumours, raids) repeats each time; `--live-ticks`
  moves it like the DOS clock and stays reproducible. A resume without
  `--seed` already reads the wall clock (varying, not reproducible).
- Resume: `python3 tools/play.py start DIR --load DIR/saves/tNNN.sav`
  (path from the repo root).
- Commands: `python3 tools/play.py send DIR "cmd" "cmd" ...`; stop with
  `python3 tools/play.py stop DIR`. Command list: header of
  `tools/play_driver_main.c`.
- One autosave per turn: `saves/trace_autosave_turn_NNN.sav`. Log:
  `DIR/bin/opencol.log`.
- After a rebuild: `save NAME`, `stop`, `start --load`.
- Loading renumbers every unit id. Run `units` again.

## Token budget

Cost = number of calls x context size. Output size matters much less.

- Put many commands in one `send`. Aim for one call per turn or less.
- Write helper scripts for routine turns: end the turn, stop on `CHOICES:`
  or `ERR`. Call them by a short path; do not `cd` into long paths each call.
- Filter output: pipe `units`, `colonies`, `near` through `grep`. Drop
  `STATUS gold` lines.
- Screenshots cost ~350 tokens. Take one only to read a menu layout or to
  record a moment for the journal.
- Split a long campaign into eras. At the end of an era: update the journal,
  save, and continue in a fresh session (journal + save + this file).

## Driving safely

- Keys go to the open popup. Arrow or keypad keys move its highlight; a
  blind Return picks the highlighted row. After each batch look for
  `CHOICES:` and answer with `choose N`.
- Send `wait 10` before keys to a popup that just opened.
- Move units one key per step and check the output between steps.
- Nested modals (Combat Analysis) get an automatic Return (`NESTED:` line),
  and a nested colony message takes "Continue turn.".
  A nested diplomacy question (AI demand, treaty, alliance) prints
  `NESTED CHOICES:` and waits: answer it with `choose N`; other commands are
  dropped until then, except `nestedpick TEXT`: it takes the first row
  containing TEXT (the rest of the line, `nestedpick Go in peace`, also
  `A|B`), now and for every later nested popup until the next `nestedpick`
  (bare: off). Row 0 of a demand is often the refusal ("We laugh at
  your puny threats" started a war in campaign 10). An independent European
  nation opens every audience with its "newly elected government" greeting
  (DOS swaps every HELLO* for HELLOUSA): answer "Go in peace" again.
- Cargo: `load SHIP CARGO [AMT]` / `unload SHIP [CARGO]` with the ship in a
  colony; `export N CARGO` toggles a Custom House row (it sells sugar and
  furs by default: untick inputs you process). CARGO is an id or a name
  prefix (`load 89 tobacco`, `buy 0 tools 100`). `load` takes from the colony
  the unit stands in: a wagon in Santo Domingo loads Santo Domingo's tools,
  not the docked ship's.
- `export` toggles: read the `EXPORT ... on|off` line. Campaign 11 meant to
  tick cloth and coats, which were already on, and switched them off.
- `field`, `work` and `admit` print `FOOD colony +-N stock S`; `STARVES NEXT
  TURN` means fix it now (with End of Turn off the turn can end in the same
  batch).
- Grep war lines as `ADVISE WAR ` (with the space): `ADVISE WAR` also matches
  every `ADVISE WARN`. The REF landing popup reads "lands near".
- The Congress pick (founding father) happens at turn end, before that
  turn's autosave. Reloading does not undo it. Answer it with `choose`.
- `key Escape` on the main map is refused (it would ask Exit to DOS).
- Check for `LOG:` after `activate`: a tutorial hint can open on selection
  and the next key only closes it (DOS: any key closes a message box).
- A warehouse-full drag on the colony screen asks first (`CHOICES: [0]
  Never mind. [1] Unload ... anyway`). The driver's `unload` does not ask:
  it prints `WARN ... over the warehouse cap`; `load` the excess back or it
  spoils at turn end (campaign 11 lost 100 tools).
- `field` refuses a plot on native land at peace (the game would ask
  pay/take there). Drag the colonist onto it on the colony screen instead.
  It also refuses a plot another colonist works (`ERR plot N is worked by
  colonist K`): move that one first. Before this check the old worker was
  left with no job at all (`field_job -1 slot -1`, eating, producing nothing).
- A popup can open between two clicks (a King's tax raise opened during a
  cargo drag, and the next click held a Tools Party: tools boycotted for
  the rest of the game). Check for `CHOICES:` after every click.
- Look up job numbers before `work` or `field` (`field_job 8` is fishing,
  not tobacco). `field` refuses a land job on a water plot, fishing on land,
  and fishing without Docks.
- `buybuild N yes` and then `build N X` in the same turn moves the bought
  hammers to X (DOS rule): the bought building is not built. Wait a turn
  before changing the build.
- `activate ID` that fails prints ERR and every following `key` of the same
  batch prints `SKIP key ...` (C12: keys meant for one unit fortified and
  moved the selected galleon). Re-send the batch after `status`.
- `villages [R [X Y]]` lists the seen villages (tribe, capital, mission,
  tension, alarm): no `sav_json` scouting needed.
- `fog [N]` counts the land tiles you have not seen yet and prints the first
  40 as `FOGTILE x,y` (campaign 13 explored the whole map with it; the last
  tiles were an arctic islet, the Galapagos and a Hudson-bay shore, reached
  only by ship).
- Trade routes have no driver command (menu only, campaign 13): `key t alt`
  opens TRADE; `click 195 22` = Create, `click 195 13` = Edit. Destinations
  come as `CHOICES (list ...)`, the name entry takes `key Return`. In the
  editor row N (from 0) is y = 71 + 20*N; Unload cells x 116-198, Load cells
  x 199+: click right of the existing icons to add (a list opens), click an
  icon to remove it. OK = `click 295 179`. Assign: `activate` the wagon,
  `key t`, `choose` the route, `choose` the first stop. Click a row's
  Destination cell (x 40) to add or change a stop (the list ends with
  "(Delete Destination)"). Editing stops renumbers them: a wagon on the
  route heads for the stop that now has its old index.
- A new Discoverer game has Tutorial Hints on; each hint eats the next key.
  Send `option hints off` once (campaign 14).
- `export N` (no cargo) lists the Custom House rows that are on. Tobacco,
  cotton, ore and silver start on: list before you toggle.
- `sell H CARGO AMT` sells AMT tons from the first CARGO hold (the partial
  sale). Each sale runs the market step once: see "Silver" below.
- ORDERS Automate (`key q`, settings `divergences.scout_autoexplore` /
  `ship_autoexplore`) explores on its own; `units` prints `automated` and
  advise does not count those units as idle. `goto` takes a unit off
  Automate. An automated Scout walks away from a treasure it found.
- `work N CI B` takes the same row numbers as `build` BIDs (Armory = 3,
  Town Hall 9, Carpenter's Shop 35, Lumber Mill 36, Blacksmith's House 39,
  Weaver's House 21).

## Driver quirks

- `goto ID X Y` activates the unit itself (DOS gives Go To only to the
  active unit) and takes the first step. The order then persists: the unit
  moves on when its turn in the rotation comes, also on later turns.
  Units waiting for orders come first, so after a `goto` give every waiting
  unit its orders, then `auto`. Before this fix a `goto` on a Fortified or
  Sentried unit left it stranded and `auto` ran to its 5000-frame limit.
- `activate ID` clears Go To.
- `goto` takes the first step itself. `ERR goto refused` = the target is a
  seen tile the unit cannot enter (a ship aimed at land: aim at the colony
  tile or the water next to it). `ERR goto found no path` = the game's
  pathfinder found no route (DOS rule: the order is dropped and the turn's
  moves are spent). A ship sent to land it cannot reach, such as an unseen
  inland tile, fails this way. Aim ships at water or a coastal colony.
- `activate` on a unit that fortified this turn leaves it 0 moves.
- `activate` clears Fortify and Sentry (a DOS click does). Do not activate
  fortified guards each turn: they never reach Fortified and wait for
  orders again. `activate` waits up to 60 frames for a queued popup to
  clear, then is refused while a popup or another screen is open.
- A fortified unit woken the next turn can still have 0 moves. Strikers
  that must attack next turn: end their turn with `key space`, not fortify.
- A sentried unit wakes whenever a foreign unit steps next to it (DOS rule).
- Units with 0 moves never block `endturn`. Its ERR line names the unit
  that waits.
- A unit waiting for orders blocks `endturn`. Fortify (`f`) also parks
  wagons and ships.
- `endturn` + `auto` plays one turn. A turn where no unit needs orders
  (Go To units only) stops at the End of Turn prompt again: send `endturn`.
- Game Options "End of Turn" is off in a new game: the turn then ends by
  itself as soon as no unit has moves left (DOS rule), but only on a turn
  where some unit waited for orders. A turn where nothing needed orders
  stops at the End of Turn prompt (`endturn`). Switch it on before war
  turns: `option eot on`.
- Declare independence with `declare`, then `choose 1` (Yes). Row 0 is No.
- Sentry in a colony = board the next ship that leaves. Any own ship or
  wagon entering the colony wakes every sentried unit there (DOS rule), so
  sentry them again after the last arrival, right before the ship sails.
  A ship crossing one of your colonies on Go To unloads its passengers there.
  Unloaded units keep their sentry and board the next ship out.
  So sentried units do NOT board a ship that enters their colony (it wakes
  them): sentry them after the ship has arrived.
- Colony screen: outside units stand on the fence row (y about 116-118).
  `hit X Y` names what is at a point. Drag a worker onto the fence (x=140)
  for the role menu; it opens on the current role, so click the row.
  A plain click on an already selected fence unit also opens it. A drop on
  a colonist icon inside a building is ignored: drop on its empty part.
- When the fence is full (50+ units outside), use `admit UID` (unit joins
  the colony) and `eject N CI ROLE` (colonist leaves). `eject` opens the
  colony screen (`key Escape` closes it) and asks the screen's questions:
  the last colonist gets the abandon prompt (`choose 1` keeps the colony).
  A refusal prints `ERR eject refused: <reason>`: a colony with a stockade
  (La Salle gives every pop-3 colony one) cannot drop below 3 colonists, so
  it can never be abandoned. Abandon drop-off colonies before pop 3.
- Go To port (`key g`): the port list prints as `CHOICES (list ...)`;
  answer with `choose N` (row 0 is Europe; a blind Return picks it).
- With many units on the colony tile, use `equip UID ROLE` (2 soldier,
  4 dragoon) instead of the screen.
- Build menus show only buildable items and are centred, so rows move. Use
  `build N BID` (it closes the colony screen it opened). `build N` alone,
  or with a BID not in the menu, prints the `BUILDABLE` rows (BID name).
- `buybuild N` prints the BUY price of colony N's project (`choose 1`, then
  `key Escape`); `buybuild N yes` buys and closes. The price falls as hammers
  pile up: San Salvador's Docks cost 1352 with 0 hammers, 481 with 15. A
  finished-hammers project still waits for its tools (Fortress: 200). `ERR build ... not in ... build menu` = the colony owns it
  (founding gives Town Hall, Carpenter's Shop and five craft houses) or the
  row cannot be built yet.
- A finished building stays selected and keeps taking hammers. Pick the next
  build at once.
- Europe: `europe` opens the screen and prints gold, the RECRUIT / TRAIN /
  PURCHASE rows (1-based), MARKET (cargo id, sell and buy price, BOYCOTT)
  and HARBOR ships (index H). Then `recruit N`, `train N`, `purchase N`,
  `sell H [CARGO]` (all holds of CARGO, or every hold), `buy H CARGO [AMT]`
  (default 100), `sail H` (answers its own Yes; a popup that opens instead,
  such as a mercenary offer, is printed as CHOICES and left for you).
  Never send `sail` twice for one ship. Read the rows before each recruit:
  the pool changes after every pick. `key Escape` leaves the screen. Never
  use `key e` to leave: on the main map E opens Europe. A ship sailing from
  Europe arrives at the tile it last left from. Cargo and passengers
  share the ship's holds: a caravel with one hold of goods carries one
  recruit.
- Go To often never steps toward a water tile next to a foreign colony or a
  village (campaign 13: ships kept orders 3 and full moves for turns). Move
  the last tiles by hand with keypad keys.
- A Go To that bumps into a native or foreign unit opens "Shall we attack?"
  with Yes preselected; answer `choose 1`.
- Land units cannot step from a ship into a village ("enemy occupied
  square"): land the scout on a tile next to it first.
- `admit` seats the newcomer by DOS's own plot scoring and can move other
  colonists (an Expert Farmer went from a farm to an ore plot twice in
  Barbados). Re-check `colonies` after every `admit`. Joiners that score no
  plot become Carpenters with no 3-worker cap (5 in Jamestown's mill).
- `field` refuses plots another colony works, colony centres, villages and
  foreign guards. A red no-entry circle on a plot means the job yields 0 there
  (a lumberjack on a tile without forest).

## Game rules that decide a campaign

Economy and natives

- Hammers without tools stall. Keep tools ahead of hammers.
- Horses breed only with a food surplus and at least 2 horses in stock.
- A colony with no food in stock and a negative food balance starves a
  colonist that turn (a new colony too).
- Fishing yields no food without a Dock in the colony. Build the Dock first.
- Custom House (needs Peter Stuyvesant) sells without ship trips, even
  boycotted goods. Refuse tax raises with a party on goods it sells and the
  rate stays put. Never party on goods you buy (tools, muskets, horses);
  only Jakob Fugger lifts boycotts.
- Buying a build costs less the more hammers are stored: with all hammers
  in, an artillery cost 130-260 gold (only the missing tools).
- Recruit prices drop once enough crosses pile up.
- A ship damaged at sea loses all cargo and passengers. Privateers hunt off
  your colonies.
- Native alarm 75 or more = hostile: moving into a brave attacks without
  asking. Working native land raises alarm (more near a village or on a
  resource).
- A village's tension toward you grows each turn near your colony. At the
  top its braves attack any unit of yours next to them, even at peace and
  at low alarm: a lone scout passing such a village can die.
- Site choice: a colony next to a native village is usually the wrong pick.
  It costs one tile to work, adds native land claims nearby, and tension
  rises fast. Choose it only to trade heavily with that village.
- Scouts visiting chiefs are good early money. On Viceroy the Crown takes
  70% of treasure. Under `--seed` without `--live-ticks` the chief's
  roll is the same every visit (campaign 12: 8 of 8 "tales" for a seasoned
  scout); with `--live-ticks` it varies as in DOS. Never send a scout into a village of a tribe you are at
  war with: "broken sacred taboos" kills it.
- The Crown's treasure offer comes only when the Treasure moves INTO a
  coastal colony (DOS FUN_465b_0000 tail). Founding a colony on the
  Treasure's tile does not trigger it: step out and back in.
- Spain with Hernan Cortes: burning a native city always yields treasure and
  the Crown ships it for the tax-rate share. Campaign 11's five Inca cities
  paid 6200 / 9300 / 16000 (capital) / 6200 / 9300. Artillery attacking a
  village is not "in the open": stack 4-6 guns on a mountain next to the
  city and attack with the healthy ones each turn; a city took 10-15 kills.
  Pay the Inca for a silver tile before the war (650) if you need it early.
- Silver crashes: 19 in 1550, 7 by 1630, 2 by 1660. Do not build the
  economy on it past the first century.
- Silver (campaign 14, sell 1 / buy 2 reached in 1698): the price word
  steps down one per market step while the silver volume (nr) is >= 100;
  each ton sold adds 3.68 (Discoverer), attrition takes 8 a turn, so after a
  century nr sits near -400..-800. One step runs per harbor sale and one per
  turn. Stockpile, then sell 600+ tons in one visit (19 -> 14, nr ~1250 left,
  then one step a turn); keep a few tons for 1-ton `sell H silver 1` sales,
  each one more step. Silver Deposits deplete (Barbados' two died after
  ~420 tons): plan a second and third deposit.
- A lone Treasure (no other unit on its tile) is a Brave target even at
  alarm 12 (1/8 roll per adjacent Brave): stack a unit on it while it walks.
- Converts come from braves of your mission villages visiting your
  colonies; missions next to colonies pay. 10 took until 1696 (6 missions,
  Brebeuf).
- A village whose only land neighbour holds a fortified Brave (Arawak
  (45,30), next to Fort Orange) cannot be entered from a ship.
- Tory penalty: -1 on every tile and building once
  round(pop x Tory%) >= 10 - difficulty (6 on Viceroy). On Viceroy keep pop
  at 6 or less until SoL rises (pop 7 needs ~22% SoL, pop 8 ~38%).
- Benjamin Franklin only stops the King's European wars from dragging you
  into New World wars, and makes AI Europeans offer peace when you
  negotiate. AI Europeans still declare war on their own: garrison every
  colony.
- The Custom House exports ore, silver and other raw goods from founding
  (stock above 99 sells down to 50). Untick a good to keep its stock.
- Islands have no forest: ship 20+ lumber in, let a carpenter work one turn,
  then `buybuild` (campaign 12: Docks 1352 -> ~630).
- Mercenary offers from the King or other powers are good value (campaign
  12: 3-4 dragoons for 3000-5600): accept when the gold allows.
- Making a pioneer takes the colony's tools in steps of 20, up to 100. Buy
  tools before you eject one when a build needs them.
- The King's frigate offer costs a +10% tax raise (the offer text does not
  say). Refusing the raise with a party keeps both the frigate and the rate.

Unit cap

- 300 units in the game. AI, natives and Crown stop at 292; you stop at 300.
- Past the cap a finished unit silently fails (@TOOMANYUNITS), and its
  hammers and tools are still spent. Disband an idle unit (`key d shift`)
  the turn a gun finishes.
- Declaring independence withdraws the two European powers that are neither
  you nor the Crown's slot (DOS `FUN_43f7_1a26` -> `FUN_43f7_0108`): every
  unit they own is deleted and they stop acting, but their colonies stay on
  the map, ownerless and inert (campaign 13: French and Dutch colonies still
  there after 1694). The intervention power is one of them; its force arrives
  as your own units.

Calendar

- One turn per year until 1600, then two per year.
- In 1800 the game ends unless you have declared independence. The 1790
  popup is the only warning. Declare by turn 507.

Combat

- Artillery in the open fights at -75%, attacking or defending, unless the
  defender is fortified.
- Ambush: if neither tile is a colony, the attacker gets the defender's
  terrain bonus (Cont. Cavalry 91% vs Regulars in forest). In a colony the
  defender keeps its terrain.
- Damaged artillery attacks badly (49-55%) and dies when it loses.
- Losing a sea fight sinks every ship on that tile. Never stack transports.
  In the war a beaten rebel ship with no drydock always sinks. Campaign 11
  moved three transports and a frigate (8 guns aboard) next to a Tory
  Man-O-War; all four sank. During the war move guns overland.
- No attacks from ships. Land next to the target, attack next turn.
- Never end a ship's turn next to an enemy or Tory colony with a fort: the
  fort shoots it (campaign 12 lost a caravel and a frigate this way).

War of Independence

- Before declaring: `advise` GATE lines. They need artillery >= 0.8 x REF
  land units (split over the coastal colonies), dragoons >= 0.35 x REF land,
  a Fortress, Fort or better and SoL 100% in every coastal colony, and 100
  spare horses and 50 muskets. Measured (guns vs REF land units at the
  declaration):

  | Campaign | Difficulty | Guns | REF land | Result |
  |---|---|---|---|---|
  | 1 | Discoverer | 19 + 9 dragoons | 23 | won 1652, no colony lost |
  | 2 | Explorer | 38 | 46 | won 1699 |
  | 3 | Conquistador | 56 | 89 | won 1816 |
  | 5 | Viceroy | 58-68 | ~130 | won 1802 with intervention |
  | 5 | Viceroy | 37 | ~128 | lost |
  | 7 | Discoverer | 7 | 30 | lost 1797 |
  | 11 | Conquistador | 61 (22 damaged) + 16 dragoons | 83 | lost (one colony stripped), then won 1772 |

  Also keep far more horses and muskets than seems needed: on Viceroy ~130
  horses and ~200 muskets ran out within ~10 turns.
- Then REF attacks win only ~40-55%. A losing Regular dies, Cavalry drops
  to Regulars, Artillery is damaged. Each REF win damages a gun, a second
  destroys it. A Fort, damaged guns or Tories push REF odds to 55-66%
  (Tory % = 100 - SoL is added to Crown attacks; `eject` spare workers to
  reach 100%).
- The REF lands only next to coastal colonies. Target = highest
  colonists x (125 - SoL) - 75 x armed units on the colony tile (at least
  100 - SoL); ties go to the later one in `colonies`. Work it out and put the
  guns there. The target is re-picked every wave: keep every coastal colony
  at score 0 (armed >= colonists x (125 - SoL) / 75). Campaign 11 shipped
  Isabella's guns to the target colony, Isabella's score went positive, a
  second force landed there and took it.
- A wave (at most 6 units) lands on the land tiles next to the colony that
  hold the fewest units and deletes your units there, and any ship on its
  landing water tile. Fill all but one tile with fortified stacks: the wave
  lands on the free tile, which you attack from every neighbour. Never end
  a ship's turn next to the target colony.
- Tory uprising: at most once per colony, when
  pop x (100 - SoL) x 2 / 100 + difficulty + 1 > the garrison's attack sum.
  Once the REF pool is empty the Crown uses it on any colony with nothing
  armed, even at 100% SoL.
- Hit landed stacks the turn they arrive (they cannot move yet), from open
  tiles (ambush bonus), with dragoons or Continental cavalry. Ambush odds
  vs Regulars: Continental Cavalry 75-79%, veteran dragoon 77%, dragoon 69%,
  soldier 55-60% (Explorer). On Conquistador campaign 11 measured ~50%,
  ambush or from inside the colony. Strike anyway: a loss only demotes the
  dragoon and costs 50 horses. Keep the dragoons INSIDE the colony next to
  the landing tile, so a demoted one re-equips there; stock 300+ horses
  before declaring (no Europe after it; the overflow can wait in a docked
  ship). Do not fortify strikers: a unit fortified this turn has 0 moves.
- A lost attack costs 50 horses or muskets, and re-equipping spends all the
  unit's moves. Colonies produce after the Crown moves, so a unit bought or
  finished now misses the Crown's next attacks.
- Campaign 13: the REF sat 7 units next to Boston and shelled its 6 guns away
  before taking it; a 100% SoL Plymouth with nothing armed fell to a Tory
  uprising. Retaking both with Continental cavalry ended the war (1701). Do
  not `fortify` strikers (0 moves next turn): park them with `key space`.
- Inland colonies count too: Tory uprisings took Veracruz and Santo Domingo
  in campaign 11 once their guns had left. A colony you retake needs armed
  units left in it: campaign 12 lost Fort Orange to a militia uprising again
  as soon as its garrison marched out. Leave each one an attack sum
  above pop x (100 - SoL) x 2 / 100 + difficulty + 1.
- Intervention: the bell count restarts at the declaration. At
  2000 + 1500 x difficulty bells (8000 on Viceroy) a foreign power lands
  Continental units, and your attacks on colonies get +50% Bombard
  (campaign 11, Conquistador: 40 turns after declaring; 4 Continentals, 2
  guns and Men-of-War; the bombard let 6 guns retake a fortress colony).
  Attacking a colony also adds its SoL%.
- Victory: the King gives up when Crown land units (Regulars, Cavalry,
  Artillery) fall below 1, or below 8 once the Crown holds one of your
  colonies. Campaign 11: with the REF gone the King still waited until
  every colony was retaken, including two taken by Tory uprisings
  (soldiers / colonists, easy kills for Continental cavalry). The check runs after the Crown moves, and every colony must be
  yours then: garrison a retaken colony before ending the turn. A small
  colony lost late can end the war: retake it while fewer than 8 remain.
  The win check also needs the Crown to hold NO colony (ai_king_war.c):
  campaign 14 had the REF at 6 land units and still had to march 13
  cavalry from Penobscot to Peru to retake Boston (1725).
- Before declaring, give every colony an armed unit, inland ones too: once
  the REF pool was empty the Crown raised 11 Tory soldiers around the
  unarmed silver colonies (campaign 14 branch A). Keep pop x (100 - SoL)
  x 2 / 100 + 1 below the garrison's attack sum (eject colonists if needed).

## Recording

- Journal in `README.md`, one section per era: events, decisions, numbers.
- `bugs.md`: `CN-Bn` = port bug, `CN-On` = oddity, unsure. Give turn, save
  file, steps, expected vs seen.
- Before filing, check `docs/dos_bugs.md` and `docs/known_divergences.md`.
  Many reported "bugs" were real DOS behaviour.
- Save before risky moves (`save tNNN`). Keep abandoned timelines in
  `saves/branchN/`.
- Screenshots: `shot NAME` makes `shots/NAME.png` (2x).
