# Automated playtest, campaign 4

Goal: win the War of Independence on **Governor** (tough, difficulty 3) as
**Spain** (leader Christopher Columbus), playing the port headlessly.

**Result: won.** Independence declared spring 1796 (turn 500), the King
conceded in autumn 1824 (turn 557): "Governor Christopher Columbus of the Free
Spain, President, Republic of Mexico to A.D. 1824", score 475, rating 19%
(`shots/win1.png` .. `win4.png`). It took five timelines (see the journal).

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
- `build N BID [queue]`: colony N's construction pick (see below).
- `equip UID ROLE`: a unit standing on a colony tile takes role ROLE
  (`COLONIZE_EJECT_*`: 2 soldier, 4 dragoon) through
  `game_colony_apply_outside_role`, the writer behind the colony screen's
  outside-unit role menu. Reason: with ~50 guns on the tile the colony screen
  only draws the first few outside units, so a Cont. Army that had lost its
  horses could not be dragged to the fence any more.

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
- `endturn` + `auto` runs through whole turns when no unit needs orders
  (it answers the next End-of-Turn prompt too); keep one unit with orders
  "none" as a turn stopper. A Go To set with `goto` moves the unit at once but
  is not resumed on later turns here, so standing orders were re-issued each
  turn.
- Save/load renumbers every unit id.
- A unit that is still *fortifying* (order F given last turn) and is then
  touched in the turn cycle completes to *fortified* with 0 moves left: only
  the first gun of a freshly fortified stack in the open could attack.

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

### 1662-1791 (third timeline): Franklin, the arms race

- Reloaded `saves/t232.sav`. Picked **Benjamin Franklin** at the 1668
  Congress. France still declared war in 1679 (four turns before Franklin
  joined) and took Havana (pop 1, no garrison), but with Franklin owned the
  next encounters were peace talks and the war fizzled; by 1706 the Dutch and
  by 1751 the French colonies had won their own independence (Republic of
  Surinam, Republic of Quebec) and made peace with everyone.
- Isabella built a Fort (1672), a Blacksmith's Shop (misclick, doubled tool
  output) and the **Fortress** (1711); Santo Domingo an Armory; both then
  built artillery nonstop: Isabella 36 hammers a turn (3 carpenters), Santo
  Domingo 30. Tools were the bottleneck: Isabella's two blacksmiths make 16
  a turn from ore shipped in from San Salvador and Veracruz, a wagon train /
  caravel carried 100 tools to Santo Domingo every ~12 turns, the Frigate
  bought 100 in Seville once.
- Added driver command `build N BID` because build-menu clicks kept landing
  one row off (two buildings built by accident: a second Wagon Train and a
  Blacksmith's Shop).
- More founding fathers: Jefferson (1697), Fugger, John Paul Jones (1724),
  Washington (1738), Drake, Magellan. Taxes climbed to 75% (Stamp Acts and
  "wars with Denmark"); several parties held on goods we no longer sold.
- Founded **Trinidad** (23,5), landlocked on purpose (the REF lands only next
  to coastal colonies).
- Autumn 1791 (turn 491): **declared independence** with 54 artillery (37 in
  the Isabella Fortress, 15 in the Santo Domingo Fort), 6 veteran dragoons,
  SoL 99%. `saves/t491_predeclare.sav` is the turn before. REF: 65 regulars,
  22 cavalry, 11 artillery, 20 men-of-war. Continental Army mobilised: 5
  units in Isabella, 1 in Santo Domingo. The Dutch republic offered to
  intervene at 6500 bells.

### 1791- : the War of Independence

- The REF landed next to Veracruz, then San Salvador (taken at once, 2 guns
  lost), then Santo Domingo: against the Fort its regulars and cavalry
  attacked at 47-52% ("Bombard +50%" on the REF side), knocked every gun
  down to damaged and took the colony by 1797.
- Isabella's Fortress sank a Man-O-War by shore fire.
- Continental Cavalry sorties from Isabella against REF units on the
  adjacent forest tiles run at ~50% (61 vs 60), but a win *destroys* the REF
  unit and a loss only costs 50 horses (the unit drops to Continental Army
  and re-equips in the colony), so the cavalry attacks every turn.
- By 1801 (turn 512) the Crown had also taken Trinidad (marched inland) and
  Isabella was down to 4 healthy and 36 damaged guns, with ~36 REF land units
  on the map and 25 more in the pool. Against damaged guns (base 3) the REF
  attacks at ~58%, so the Fortress would fall. The victory test
  (`ai_king_woi_end_win`, DOS FUN_3844_0442) needs the Crown to hold no colony,
  fewer than 8 Crown land units once it has captured one, and an almost empty
  pool, so every lost colony would also have to be retaken. Saved
  `saves/t512_lost.sav` and reloaded `saves/t491_predeclare.sav` to build
  longer before declaring (autosaves of this attempt: `saves/branch3/`).

### 1791-1800 (fourth timeline): out of time

- Reloaded turn 491, abandoned Veracruz, shipped ore and tools, kept building
  guns and did not declare. In 1800 (turn 508) the game ended: "Governor
  Christopher Columbus steps down after over 300 years of loyal service to the
  Crown". This is the DOS rule (turn_year_end.c, raw :58630): 1800 ends a game
  in which independence has *not* been declared (1850 ends any game). The 1790
  "Anniversary" popup is the only warning. So the last turn to declare is 507,
  and the 1791 save has a 16-turn window.

### 1791-1824 (fifth timeline): the war won

Reloaded turn 491 once more, this time with a plan built from the combat code
(`src/core/combat_strength.c`, `ai_king_ref.c`) and the attempt-3 combat log:

- **Ambush.** After the declaration, a Rebel unit that attacks a REF unit
  while *neither* tile is a colony gets the defender's terrain bonus itself
  (and the defender gets none): Cont. Cavalry 91 vs Regulars 40 on forest
  (69%), vs Artillery in the open 91 vs 10 (90%). From inside a colony the
  same attack is 61 vs 60 (50%).
- **Artillery in the open** fights at a quarter (attacking or defending)
  unless the *defender* is a fortifying/fortified European unit. Guns stay in
  colonies, or fortify before an REF turn.
- **Rebels attacking a colony** get +SoL% (and +50% once a foreign power
  intervenes), so a captured colony is cheap to retake. The Crown capturing a
  colony also lowers the victory bar from 0 to fewer than 8 Crown land units.
- **The REF lands seizing tiles**: units of mine standing on an empty landing
  tile next to the colony are removed, so the cavalry waits inside.

What happened:

- Turns 491-500: Santo Domingo's 15 guns walked to Isabella on the road (one
  more gun from SD's 1140 stored hammers, the rest of which are lost on
  completion), 400 horses were gathered from SD and Veracruz (Veracruz
  abandoned; its veteran soldier walked in). **Declared spring 1796** (turn
  500): 56 guns in the Isabella Fortress, 6 veteran dragoons mobilised to
  Continental Cavalry, SoL 100%. REF: 66 regulars, 23 cavalry, 20 artillery,
  10 men-of-war.
- The first waves landed at Santo Domingo (no guns: taken at once) and San
  Salvador; ambushes from the open tiles round San Salvador destroyed whole
  waves (turn 503: four cavalry attacks wiped out 3 Regulars and a Cavalry).
  Then the REF turned on
  Isabella: against the Fortress (SoL 100%, so no Tory bonus) its Regulars
  attack at 41%, Cavalry 46%, Artillery 50%, and a losing Regular dies. Most
  REF turns cost it 3-6 units for 1-3 damaged guns.
- Horses ran out by 1800 (Isabella has no food surplus, so no breeding), so
  the cavalry fought on as Cont. Army; two Armory workers turned the capped
  tool stock into muskets to re-arm the beaten. Washington promoted several
  winning soldiers and dragoons back to Continental units.
- San Salvador and Trinidad changed hands three times each and Santo Domingo
  once; each was retaken by walking guns or cavalry next to it (11 guns took
  back Santo Domingo, garrison 6, in two turns in 1810).
- **1815 (turn 542): the Dutch intervened** (6500 bells after the
  declaration): two landings of 2 Cont. Army, 2 Cont. Cavalry, 2 artillery and
  a Man-O-War, at Isabella and Santo Domingo.
- The REF pool drained to its last Regular by 1824; when the last REF wave
  died on the plains beside Santo Domingo, with all four colonies mine and
  fewer than 8 Crown land units left, the King gave up (turn 557, autumn
  1824).

War statistics (turns 500-557, from `bin/opencol*.log`): the REF attacked my
colonies 134 times and won 63 (expected 70); in the open it won 4 of 5. I
attacked 100 times and won 72 (26 attacks on colonies, 74 in the open).
Saves: `saves/t500_predeclare.sav`, `t508.sav`, `t547.sav`,
`t556_prewin.sav` (the turn before the concession); the attempt-4 autosaves
are in `saves/branch4/`.
