# Rules for player agents

Read this before playing a campaign. It holds what campaigns 1-6 learned, so
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
2. Use the numbers as written. The declaration numbers in "Before declaring"
   were measured on Viceroy (campaign 5). No Discoverer numbers are recorded.
   Do not invent a lower number. A missing Fortress fails the gate on any
   difficulty.
3. A rule beats your plan. Stop and re-check when your reasoning says "not
   realistic", "too slow", "I'll skip it", or "the stock is big, so this does
   not apply". Campaign 6 skipped the Docks rule because Jamestown's food stock
   read 114.
4. Re-read the section you need before each gate, for example
   `grep -n -A20 "Before declaring" automated-playtests/RULES.md`. Re-read this
   whole file at the start of each era. Do not trust your memory of it.
5. Never send a `force` flag. The driver's ERR line on the main map suggests
   `key Escape force`; ignore that suggestion. To close a screen, send
   `status` first, then plain `key Escape` on that screen (never on the main
   map).
6. Never answer a popup blind. Loops stop on `CHOICES:`. Read the rows and
   send `choose N` for the row you mean. A `choose 0` loop picked Peter Minuit
   (1587) and Jakob Fugger (1717) by accident in campaign 6.
7. Before filing a bug, grep `docs/dos_bugs.md`, `docs/known_divergences.md`
   and this file for the key words, and write the results in the entry.
   Anything not confirmed is `CN-On`, not `CN-Bn`. Campaign 6's CN6-O8
   restated the "pick the next build at once" rule.
8. The brief's deadline is a window, not a target. Declare as late as the
   gates allow, before the 1800 cutoff (see Calendar). Both campaign 6
   declarations lost within six years. If the gates cannot be met in time,
   end the run and report that.
9. Write the era plan in the journal before the era starts: target turn,
   gates, what must be built. Check it at each era start. Campaign 6 planned
   to "declare after the defence is in place" (3+ artillery in Jamestown,
   target 1740-1750), then declared in 1733 with 2 artillery there.
10. Unclear screen: stop. Send `status`, take one `shot` if you need it, then
    decide. Do not guess clicks or keys. Campaign 6 guessed row positions to
    save screenshot cost.

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
- `field` refuses a plot on native land at peace (the game would ask
  pay/take there). Drag the colonist onto it on the colony screen instead.
- A popup can open between two clicks (a King's tax raise opened during a
  cargo drag, and the next click held a Tools Party: tools boycotted for
  the rest of the game). Check for `CHOICES:` after every click.
- Look up job numbers before `work` or `field` (`field_job 8` is fishing,
  not tobacco).

## Driver quirks

- `activate ID` clears Go To. To move now: `activate ID`, `goto ID X Y`,
  `auto`. Go To is not resumed on later turns: give it again each turn.
- `activate` on a unit that fortified this turn leaves it 0 moves.
- A unit waiting for orders blocks `endturn`. Fortify (`f`) also parks
  wagons and ships.
- `endturn` + `auto` plays whole turns while no unit needs orders. Keep one
  unit with no orders as a turn stopper.
- Game Options "End of Turn" is off in a new game: the turn then ends by
  itself as soon as no unit has moves left (DOS rule). Switch it on before
  war turns: `key g alt`, `click 35 13` (Game Options), `click 105 100`
  (End of Turn row), `key Return`.
- Sentry in a colony = board the next ship that leaves. Any own ship or
  wagon entering the colony wakes every sentried unit there (DOS rule), so
  sentry them again after the last arrival, right before the ship sails.
  A ship crossing one of your colonies on Go To unloads its passengers there.
  Unloaded units keep their sentry and board the next ship out.
- Colony screen: outside units stand on the fence row (y about 116-118).
  `hit X Y` names what is at a point. Drag a worker onto the fence (x=140)
  for the role menu; it opens on the current role, so click the row.
  A plain click on an already selected fence unit also opens it. A drop on
  a colonist icon inside a building is ignored: drop on its empty part.
- When the fence is full (50+ units outside), use `admit UID` (unit joins
  the colony) and `eject N CI ROLE` (colonist leaves).
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

- Before declaring: a Fortress with 60+ artillery in the colony the REF
  will target (on Viceroy 37 guns lost, 58-68 won), SoL 100% there, one gun
  in every other colony, and far more horses and muskets than seems needed
  (~130 horses and ~200 muskets ran out within ~10 turns).
- Then REF attacks win only ~40-55%. A losing Regular dies, Cavalry drops
  to Regulars, Artillery is damaged. Each REF win damages a gun, a second
  destroys it. A Fort, damaged guns or Tories push REF odds to 55-66%
  (Tory % = 100 - SoL is added to Crown attacks; `eject` spare workers to
  reach 100%).
- The REF lands only next to coastal colonies. Target = highest
  colonists x (125 - SoL) - 75 x armed units on the colony tile (at least
  100 - SoL); ties go to the later one in `colonies`. Work it out and put the
  guns there.
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
  soldier 55-60%. Washington promotes winners.
- A lost attack costs 50 horses or muskets, and re-equipping spends all the
  unit's moves. Colonies produce after the Crown moves, so a unit bought or
  finished now misses the Crown's next attacks.
- Intervention: the bell count restarts at the declaration. At
  2000 + 1500 x difficulty bells (8000 on Viceroy) a foreign power lands
  Continental units, and your attacks on colonies get +50% Bombard.
  Attacking a colony also adds its SoL%.
- Victory: the King gives up when Crown land units (Regulars, Cavalry,
  Artillery) fall below 1, or below 8 once the Crown holds one of your
  colonies. The check runs after the Crown moves, and every colony must be
  yours then: garrison a retaken colony before ending the turn. A small
  colony lost late can end the war: retake it while fewer than 8 remain.

## Recording

- Journal in `README.md`, one section per era: events, decisions, numbers.
- `bugs.md`: `CN-Bn` = port bug, `CN-On` = oddity, unsure. Give turn, save
  file, steps, expected vs seen.
- Before filing, check `docs/dos_bugs.md` and `docs/known_divergences.md`.
  Many reported "bugs" were real DOS behaviour.
- Save before risky moves (`save tNNN`). Keep abandoned timelines in
  `saves/branchN/`.
- Screenshots: `shot NAME` makes `shots/NAME.png` (2x).
