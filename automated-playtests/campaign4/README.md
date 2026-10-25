# Automated playtest, campaign 4

Goal: win the War of Independence on **Governor** (tough, difficulty 3) as
**Spain** (leader Christopher Columbus), playing the port headlessly.

Bugs and oddities: [bugs.md](bugs.md). Screenshots: `shots/` (2x PNG).
Saves: `saves/` (`trace_autosave_turn_NNN.sav` every turn, manual `tNNN*.sav`).

## How it is played

Same harness as campaigns 1-3: `build/debug/play_driver` driven by
`tools/play.py start|send|stop automated-playtests/campaign4`, `settings.json`
with `debug.trace_autosaves` and `debug.logs`. New game with `--seed 1519`;
restarts use `--load automated-playtests/campaign4/saves/tNNN.sav` (the path
is relative to the repo root, not to the save dir).

Driver addition this campaign (tools/play_driver_main.c):

- `field N CI TILE JOB` / `work N CI B`: put colonist CI of colony N on ring
  tile TILE (0 N .. 7 NW) as field job JOB (0 farmer .. 8 fisherman), or into
  @BUILDING row B (as `hit` prints it). Both call the shared assign writers the
  colony screen ends in (`colonies_assign_field_w` / `colonies_assign_workplace`).
  Reason: the jobs menu row offsets differ per unit (experts get a
  "(CLEAR SPECIALTY)" row and a two-line title), and the build/job menus are
  vertically centred, so fixed click coordinates kept picking the wrong row.

Scratch helpers (not in the repo): `safe.py` (send command groups, stop on a
choice), `turn.py N [EXPLORER]` (fortify new guns, move an explorer, end turn,
stop on decisions), `explore.py` (nearest rumour / frontier walker),
`step.py UNIT KEY..` (one key at a time, checks the unit is still selected),
`atk.sh` / `war.sh` (attack with a unit each turn), `m.py` (map with explicit
coordinates).

Driving notes (new this campaign):

- Keys sent while a popup is open move its highlight: a stray keypad press
  turned a "Continue turn" popup to "Zoom to colony" and nearly answered an
  "attack the Arawak?" prompt. Send movement keys one at a time and check for
  popups in between.
- `activate ID` on a unit that fortified this same turn wakes it with 0 moves.
- A unit with a leftover goto spends its moves at the start of the next turn.
- The outside-unit role menu (Soldier / Dragoon / Pioneer ...) opens by
  dragging the unit from the fence row onto the fence (1x x=140).
- Building menus list only buildable items and are centred, so row positions
  move whenever a building is completed or burned: take a screenshot first.
- A completed building stays selected in the build box and keeps collecting
  hammers ("set to produce a Church, but it has already built one!").

## Journal

### 1492-1518: an island in the north-west

- Start (turn 0): Caravel at (29,14) with a Pioneer (100 tools) and a Veteran
  Soldier, **0 gold**. The high seas reach unusually far west here.
- Land Ho at (17,13) on turn 2. It is a forested island (x 11-26, y 3-13)
  with **8 Iroquois villages**; the other Europeans are far to the south-east
  (x 37-52).
- Spring 1499 (turn 7): founded **Isabella** (22,8) on a prairie hill with an
  ore resource (6 ore from the centre tile) and an ocean bay.
- Peter Minuit (1533), William Penn (1557), Brewster (1575), Adam Smith,
  Stuyvesant (1604), Jan de Witt, Paul Revere (1625), John Paul Jones (1640),
  Jakob Fugger picked.
- 1518: the veteran soldier founded **Santo Domingo** (17,5); it turned out to
  be on a mixed-forest tile (map letter `P` = forested plains), so the centre
  gave 2 food until the Hardy Pioneer cleared and ploughed it.

### 1518-1600: scouting gifts, recruits, a third colony

- A Master Tobacconist (free immigrant) + 50 horses became a scout: the
  Iroquois chiefs gave 320, 320, 320 and the Sioux 160 in beads; one rumour,
  "nothing but rumours".
- Recruits cost 200-390; bought a Master Carpenter, Hardy Pioneer, Expert
  Lumberjack, Elder Statesman, free colonists, a Veteran Soldier, and a second
  Caravel (1000). Sold ore (4), furs (5), cotton, cloth.
- Isabella: Docks, Lumber Mill, Printing Press, Warehouse, Stockade, Armory,
  Church, Newspaper; SoL 100% by 1611. First Artillery in 1600, then one
  every ~8 turns (a Blacksmith on the hill's ore supplies tools).
- 1592: founded **San Salvador** (24,10) on a marsh hill with fish.
- 1600: the Seasoned Scout reached the mainland at (37,25) (Arawak, 9
  villages) and later met the Dutch.
- Taxes: Stamp Acts took the rate 0 -> 14% (1604) -> 22% -> 30% (1623);
  held an Ore Party (1618), Cloth Party, Tools Party.

### 1606-1644: the Iroquois war and the Dutch invasion

- The Iroquois villages next to my colonies reached friction ~250 (max).
  Ships could no longer trade ("do not trust the men in your ships"); a
  colonist landed next to their capital and a Wagon Train with 100 horses were
  ambushed and killed.
- 1620: went to war (Spain gets +50% attacking villages). A Veteran Dragoon
  attacked at 93% and burned (26,8), (22,3), (21,7) (600 treasure, 210 after
  the Crown's 65%), (17,6), (13,8), (13,11). Iroquois raids in return burned
  Isabella's Newspaper and Lumber Mill, Santo Domingo's Warehouse, and killed
  a colonist at San Salvador. Fortified artillery in a colony defends at
  ~90-95% against braves ("Artillery Vs. Raid +100%").
- Autumn 1620: **the Dutch declared war** (C4-O2) because my scout had been
  roaming New Netherlands; they killed it. From 1640 a Dutch force of ~7
  dragoons, a frigate and a merchantman besieged Isabella (their attacks: 25%
  against fortified artillery behind a stockade), crippled a caravel, routed
  and captured my veteran, destroyed Santo Domingo's artillery and soldier and
  **took Santo Domingo** (pop 9) in 1644.
- Decision: reload `saves/t148_prewar.sav` (turn 148, 1620, before the scout
  provoked the Dutch) and keep all units out of Dutch land. The 1620-1644
  branch above is the lost timeline; its autosaves were moved to
  `saves/branch1/`.
- The reload did not help: the Dutch declared war again at the end of turn
  148 (the decision was already made). Continued from there, this time
  spreading artillery to every colony before the Dutch army arrives.

### 1620-1695 (second timeline): Iroquois surrender, French invasion

- Continued from turn 148. Artillery turned out to be the best native
  killer: base 7, Attack +50%, Spain +50%, no open-field penalty against
  natives, 95% against a village brave. Burned (26,8), (22,3), (17,6),
  (21,7) (600 treasure) and the capital (17,13) (1200 treasure; founded
  **Veracruz** on the site so the Crown would ship it: +420). "The Iroquois
  tribe bows before the might of the Spanish": surrender, later "everlasting
  peace" gifts. Pocahontas joined in 1649.
- The Dutch frigate that crippled a caravel parked off San Salvador for ~40
  turns and blockaded the east side. Twice the Dutch offered a demarcation
  treaty and twice the King cancelled the peace (@KINGNEWWAR, DOS-literal
  FUN_38fd_5930: needs peace with everyone, ~10%/turn on Governor) and paid
  1000 gold + 3 Veteran Soldiers each time.
- Founding fathers: Bolivar (1637), Pocahontas (1649), Paine (1668), Fugger
  (1684; lifted the Ore/Cloth/Furs/Tools boycotts).
- Founded **Havana** (26,8). Santo Domingo got a Fort, San Salvador a Custom
  House (sells boycotted ore).
- 1669: **France declared war** at an encounter (aggressive greeting); French
  dragoons that had been parked next to my colonies attacked. From 1673 a
  French army of ~30 land units (dragoons, soldiers, 7 artillery) landed from
  galleons. They took Veracruz (pop 1). Sorties from Isabella captured ~8
  French colonists (artillery captures colonists), but my artillery attacked
  unfortified units at 46% ("Artillery In Open -75%" applies unless the
  defender is fortified) and lost; by 1695 Isabella had 12 damaged guns and no
  healthy one.
- Found the rule that matters (src/core/ai_diplo.c,
  `ai_diplo_153e_worthiness_score(ctx, human, target, ...)`): if the human
  owns **Benjamin Franklin**, the encounter is never "worthy" of war and turns
  into a peace offer. Franklin was 13 turns away. Saved `saves/t297.sav` and
  reloaded `saves/t232.sav` (French still at peace); this timeline's autosaves
  were moved to `saves/branch2/`.
