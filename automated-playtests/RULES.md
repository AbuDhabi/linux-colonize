# Rules for player agents

Read this before playing a campaign. It holds what campaigns 1-5 learned, so
you do not need to read game source to learn the rules. Read source only to
confirm a suspected bug.

## Setup

- Run everything from the repo root. Build with `make build`.
- Campaign dir `automated-playtests/campaignN/`: `README.md` (journal),
  `bugs.md`, `shots/`, `saves/`.
- New game: `python3 tools/play.py start DIR --seed N`.
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
- Nested modals (Combat Analysis) get an automatic Return (`NESTED:` line).
  It can also answer a diplomacy question.
- The Congress pick (founding father) happens at turn end, before that
  turn's autosave. Reloading does not undo it. Answer it with `choose`.
- `key Escape` on the main map is refused (it would ask Exit to DOS).
- `s` or `f` with no selected unit opens the save dialog.
- Never test colony jobs with `field` on native land. It skips the pay/take
  prompt and every try raises native alarm (12 to 88 in one campaign).

## Driver quirks

- `activate ID` clears Go To. To move now: `activate ID`, `goto ID X Y`,
  `auto`. Go To is not resumed on later turns: give it again each turn.
- `activate` on a unit that fortified this turn leaves it 0 moves.
- A unit waiting for orders blocks `endturn`. Fortify (`f`) also parks
  wagons and ships.
- `endturn` + `auto` plays whole turns while no unit needs orders. Keep one
  unit with no orders as a turn stopper.
- Sentry in a colony = board the next ship that leaves.
- Colony screen: outside units stand on the fence row (y about 116-118).
  `hit X Y` names what is at a point. Drag a worker onto the fence (x=140)
  for the role menu; it opens on the current role, so click the row.
- With many units on the colony tile, use `equip UID ROLE` (2 soldier,
  4 dragoon) instead of the screen.
- Build menus show only buildable items and are centred, so rows move. Use
  `build N BID`.
- A finished building stays selected and keeps taking hammers. Pick the next
  build at once.
- Europe: keys (R/P/T) work better than clicks. A purchase asks Yes/No twice.

## Game rules that decide a campaign

Economy and natives

- Hammers without tools stall. Keep tools ahead of hammers.
- Horses breed only with a food surplus and at least 2 horses in stock.
- Native alarm 75 or more = hostile: moving into a brave attacks without
  asking. Working native land raises alarm (more near a village or on a
  resource).
- Scouts visiting chiefs are good early money. On Viceroy the Crown takes
  70% of treasure.
- Tory penalty: -1 on every tile and building once
  round(pop x Tory%) >= 10 - difficulty (6 on Viceroy). On Viceroy keep pop
  at 6 or less until SoL rises (pop 7 needs ~22% SoL, pop 8 ~38%).
- With Benjamin Franklin, AI Europeans never start a war with you. Without
  him, wars cost colonies.

Unit cap

- 300 units in the game. AI, natives and Crown stop at 292; you stop at 300.
- Past the cap a finished unit silently fails (@TOOMANYUNITS), and its
  hammers and tools are still spent. Disband an idle unit (`key d shift`)
  the turn a gun finishes.
- Declaring independence removes the other European powers and all their
  units.

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
  In the war a beaten rebel ship with no drydock always sinks.
- No attacks from ships. Land next to the target, attack next turn.

War of Independence

- Before declaring: 35+ artillery (50+ is safer), main colony with a
  Fortress at SoL 100%, one gun in every other colony, a stock of horses.
- Then REF attacks win only ~40%, and a REF unit that loses dies. A Fort,
  damaged guns or Tories push REF odds to 55-66% (Tory % is added to Crown
  attacks).
- The REF lands only next to coastal colonies, weakest first. A landing
  deletes your units on the empty landing tiles.
- Tory uprising: at most once per colony, when
  pop x (100 - SoL) x 2 / 100 + difficulty + 1 > the garrison's attack sum.
- Hit landed stacks the turn they arrive, from open tiles (ambush bonus),
  with dragoons or Continental cavalry. Washington promotes winners.
- Intervention: the bell count restarts at the declaration. At
  2000 + 1500 x difficulty bells (8000 on Viceroy) a foreign power lands
  Continental units, and your attacks on colonies get +50% Bombard.
  Attacking a colony also adds its SoL%.
- Victory: the King gives up when Crown land units (Regulars, Cavalry,
  Artillery) fall below 1, or below 8 once the Crown holds one of your
  colonies. The check runs after the Crown moves, and every colony must be
  yours then: garrison a retaken colony before ending the turn.

## Recording

- Journal in `README.md`, one section per era: events, decisions, numbers.
- `bugs.md`: `CN-Bn` = port bug, `CN-On` = oddity, unsure. Give turn, save
  file, steps, expected vs seen.
- Before filing, check `docs/dos_bugs.md` and `docs/known_divergences.md`.
  Many reported "bugs" were real DOS behaviour.
- Save before risky moves (`save tNNN`). Keep abandoned timelines in
  `saves/branchN/`.
- Screenshots: `shot NAME` makes `shots/NAME.png` (2x).
