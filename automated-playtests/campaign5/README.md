# Automated playtest, campaign 5

Goal: win the War of Independence on **Viceroy** (toughest, difficulty 4) as
the **Netherlands** (leader Michiel De Ruyter), playing the port headlessly.

Bugs and oddities: [bugs.md](bugs.md). Screenshots: `shots/` (2x PNG).
Saves: `saves/` (`trace_autosave_turn_NNN.sav` every turn, manual `tNNN*.sav`;
abandoned timelines in `saves/branchN/`).

## How it is played

Same harness as campaigns 1-4: `build/debug/play_driver` driven by
`tools/play.py start|send|stop automated-playtests/campaign5`, `settings.json`
with `debug.trace_autosaves` and `debug.logs`. New game with `--seed 1609`.

Scratch helpers (not in the repo, in the session scratchpad):
`p` (send commands), `t N` (end up to N turns, stop on decisions), `mv KEYS`
(move the selected unit one key at a time; refuses to step onto a foreign
unit and answers @WHACKINDIANS "No"), `go ID X Y` (activate + Go To),
`clr` (dismiss plain message popups only), `m R X Y` (one-char-per-tile map
with a two-line x header), `vil X Y R` (seen villages from the latest trace
autosave: scouted / capital flags).

Driving notes (new this campaign):

- The `field N CI TILE JOB` driver command goes straight to the field-assign
  writer, so it never shows @INDIANLAND (pay / take) and every seat on a
  native-owned plot charges the DOS land-work alarm (difficulty+5, doubled
  near a village, doubled again on a resource). Probing 32 tile/job combos on
  turn 15 took the Iroquois alarm from 12 to 88; that timeline was abandoned
  (`saves/branch0/`). In the colony screen, a drag onto native land shows
  @INDIANLAND with the price (422 gold on turn 24).
- Outside-colony units are drawn on the stockade/fence row (1x y=118), not on
  the bottom-left roster (that row is the colony's people). A drop that lands
  on a colonist icon inside a building is ignored: drop on the building's
  empty part.
- A plain click on an already selected fence unit opens the role menu; the
  next drag can then pick a role ("Equipped as Dragoon" by accident).
- `clr`-style blind Return presses are dangerous: one answered the Congress
  popup with its first row (Peter Stuyvesant instead of Jefferson). The
  Congress choice is made during the turn end, before that turn's trace
  autosave, so reloading the autosave does not undo it.
- The driver's nested-modal auto-Return also answers diplomacy questions
  (the French demarcation treaty was accepted that way, which was fine).
- Viceroy Tory limit: `colony_prod_sol_bonus` uses threshold 10 - difficulty
  = 6, i.e. -1 on every tile and building once round(pop * Tory%) >= 6. A
  colony of 7 at 9% SoL lost a point everywhere; keep pop <= 6 until SoL
  rises (pop 7 needs ~22% SoL, pop 8 ~38%).

## Journal

### 1492-1508: landfall on a small island

- Start (turn 0): Merchantman at (55,42) with a Pioneer (100 tools) and a
  Soldier (a free colonist with 50 muskets; on Viceroy he is not a veteran),
  **0 gold**. Land Ho at (48,42) on turn 1: the map's land reaches almost to
  the eastern high seas.
- The coast is a string of small forested islands. New Amsterdam's island
  (x 42-47, y 41-48) holds two native villages (Iroquois (42,47), Arawak
  (47,41)); a second island to the west (x 33-37) two more Iroquois villages;
  the land to the south (x 37-47, y 51-60) is Iroquois country with their
  capital at (39,58) and French Quebec at (42,59).
- Turn 8 (1500): the Iroquois (9 villages) offered peace and the land.
- Turn 9 (1501): the soldier founded **New Amsterdam** at (44,47), a
  forested grassland tile next to a fish resource (44,46) and a minerals
  swamp (45,47). The pioneer cleared (20 lumber) and plowed the centre.
- First Congress: **Peter Minuit**.

### The Cibola timeline that was thrown away

- In the first timeline the soldier walked onto the rumour next to the
  colony on turn 16 and found one of the **Seven Cities of Cibola** (3700,
  the Crown took 70% on Viceroy: 1110 gold).
- But on turn 15 I had probed every tile/job combination with the driver's
  `field` command (see the driving notes): the Iroquois alarm went 12 -> 88,
  and on turn 17 moving the soldier into a brave started a fight with no
  @WHACKINDIANS question (alarm >= 75 counts as hostile, DOS-literal). The
  soldier won and became a veteran.
- Reloaded turn 15 (`saves/t15.sav`) and replayed without the probing. The
  RNG stream had diverged (the native turns read alarm), so the same rumour
  now gave 140 gold. Kept the peaceful timeline.

### 1508-1550: cash, a scout, docks

- Rumours: 140 and 200 gold. The soldier took 50 horses as a scout (Seasoned
  after his first chief); chiefs gave 280, 80, 288, 96 in beads, one "tales
  of nearby lands", guides.
- Europe trips are short (the high seas are 8 tiles east). Sold silver
  (19), furs, tobacco (4-5), ore (5-6). Recruits cost 220 -> 340 by 1577.
  Bought an Elder Statesman (220), an Indentured Servant (240), an Expert
  Farmer (260), an Expert Lumberjack (280), a Master Carpenter (300), a Free
  Colonist (320), 100 horses, 100 tools. Free immigrants: three Petty
  Criminals, a Free Colonist, an Expert Farmer, a Hardy Pioneer, a Veteran
  Soldier.
- New Amsterdam: Docks (1527), Lumber Mill (1541), Printing Press (1547),
  Stockade (1559), Warehouse, Armory (1578), Schoolhouse (1584). Statesman in
  the Town Hall from 1527; SoL 10% (1532), 20% (1546), 30% (1557), 40%
  (1576). Pocahontas joined 1559, Peter Stuyvesant (by accident) 1570s.
- The scout visited eight Iroquois villages; on turn 61 (1553) an Iroquois
  brave **attacked and killed the Seasoned Scout** next to village (38,51)
  although the tribe was at peace with alarm 24 (C5-O1).
- 1551: founded **Fort Orange** (45,52) on the southern land with a Petty
  Criminal: two mountains (ore), a fish tile, one forest, an Iroquois village
  next door. It starved one colonist (Petty Criminal) in 1584 while its
  carpenters built the Docks; bought the last 7 hammers for 91 gold.
- 1574: a **Spanish Privateer** attacked the Merchantman off New Amsterdam:
  damaged, back to Amsterdam, and the **Master Carpenter and Expert Farmer
  aboard were lost**. A later privateer attack on the same ship failed.
- Taxes: 1% (1534), 2%, 4% (1562), 6% (1576).
