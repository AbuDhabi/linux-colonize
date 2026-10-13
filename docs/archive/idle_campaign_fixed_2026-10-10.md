# Idle campaign gate: fixed in the 2026-10-10 pass

STATUS: archived 2026-10-13 from docs/idle_campaign.md (size split). History only;
docs/idle_campaign.md is the living status. Grep by FUN_ name or transition.

- Removed the invented Euro war-fatigue peace roll and its declare-war timer seed.
  `FUN_5bfb_10ec` only computes war worthiness; `FUN_5bfb_13b0` signs or cancels
  treaties on an encounter and returns when either relation is at war. The port now
  preserves both the 6d8e timer bytes and RNG stream during `ai_diplo_euro_balance`.
- 20e6's ship-band `local_a8` now snapshots `FUN_1427_0d38` mode 2 (tile-stack
  count minus the acting hull), including passengers and other units stacked at
  the same coordinates. The former `cargo_count` snapshot missed stackmates and
  could incorrectly take the dock-demand or `457e` empty-hull branch. A regression
  fixture with a second stacked hull covers that gate.

- Removed invented primary LABOR goals for construction, stock shortages and
  low population. DOS `521d_0a60` only produces LABOR in the coastal garrison-demand
  loop (raw 87684-87696), which remains live. The extra construction goal pulled
  Spanish Dragoons toward Isabella in 1555 but is absent from the fresh DOS goal
  table. Ten archived transitions improve by 58 JSON leaves: 1554, 1555, 1574,
  1583, 1592, 1594, 1595, 1596, 1597 and 1598; none regress.
- Spare transport marking now reads the earlier `4962_0018` census, like DOS
  `521d_0a60` raw 87542-87546. A hull bought during `5d04` does not count yet.
  This clears a false French Caravel spare mark in 1555 and another difference
  in 1563. Fresh 1555→1556 now matches completely. All fourteen exact archived
  transitions remain exact after both changes.

- Removed the unsupported `player.diplomacy` OR-summary from relation writes.
  DOS `15b3_0032` raw 9069-9080 stores only the selected relation byte.
  This clears 29 fields across 23 archived transitions; fresh 1558→1559
  confirms all four player bytes remain zero. Its remaining 16 fields belong
  to two unit decisions and their map effects.
- Removed duplicate land-adjacency CONTACT and unconditional wartime MILITARY
  producers. DOS uses act state 10 for land adjacency (raw 87566-87568), and
  the literal foreign-colony loop retains presence, defender-count and turn
  gates (raw 87776-87787). Settlement producers now read the earlier census
  tables, including population in the skilled count. These corrections leave
  all archived transition results unchanged.

- The Euro wander attack gate now tests treaty bit `0x40`, like DOS
  `521d_20e6` raw 88880-88885, rather than requiring formal war. Either
  Privateer still bypasses the treaty gate; the WoI control gate remains.
  This removes 71 archived fields across 1594 and 1595, with no regressions.
  Regression covers unmet, MET-only and treaty neighbours. It does not yet
  explain the separate fresh 1558 Spanish Dragoon mismatch.

- Ship colony-sail and `4393` now retain `3558`'s pre-unload passenger
  counts (`521d_20e6` raw 89412-89444). Unloading a Dragoon does not turn
  a mixed hull into a civilian-only colony pull in the same act. The work
  queue also requires the saved `local_a8 == 0` (raw 89877-89878).
  At an own berth, `0920` selects only the carrier and boarded members;
  an unboarded Scout beside it is excluded, confirmed by fresh 1544 DOS.
  The combined change removes 657 archived JSON leaves, retaining fourteen
  exact transitions. Nineteen transitions improve; 1592 and 1598 initially
  remain worse against the archive, but fresh DOS confirms both port carrier
  and passenger routes. The initial 1553 regression is resolved by the compact
  unit-index correction below. Debug and Release agree; full tests, golden
  tests and ship regression tests pass.

- The ship far-roam roll also uses the saved military/founder count and
  requires the saved unload mask to be zero (`20e6` raw 88632-88637).
  The former live recount and assumed zero mask could draw RNG after partial
  unloading. This clears three further archived fields in 1594 without
  regressions. A regression covers the empty live hull with a nonempty band
  snapshot, and proves a nonzero unload mask preserves RNG state.

- Empty-ship sailing cadence and Man-O-War parity now use the compact live
  unit index, matching DOS deletion compaction (`1427_0824`). In 1553 an
  earlier Brave deletion makes the Spanish hull index 34 rather than storage
  slot 35; the false Europe departure had changed subsequent RNG. This clears
  26 archived fields without regressions. Fresh 1553 now matches all unit and
  map fields, leaving only Quebec's Tobacco gift (30 versus DOS 36). Debug
  and Release sweeps agree; full tests, golden and the index regression pass.

- Fresh 1553→1554 is now byte-identical. A watch on Quebec Tobacco at
  `DS:5eae` caught two `022e:0b0a` writes, 24→30→36, from Braves 15 and 25.
  The port's once-per-native-nation visit latch discarded the second gift;
  DOS's `3180` encounter latch is local to each Brave scan. Gift selection
  now uses the current mood's Brave binding instead of an earlier adjacent
  visitor. This removes one archived leaf without regressions. A regression
  covers consecutive generous visits and the second Brave's binding. Full
  tests (96 enabled), golden, Release contact tests and both sweeps pass.

- Fresh 1592→1593 confirms the port's French carrier/passenger records after
  the queue gate; their archived route differs. Nineteen fresh fields remain,
  including three Isabella workers elected as Statesmen by the port versus
  Ore Miner, Lumberjack and Farmer in DOS. The crop and stock ledger matches;
  the job election is under a live `5952` score trace.

- `00f8` nation ranks now sort unsigned scores ascending, like
  `291f_0ed0`→`1cf8_000a` (raw 18654-18738). Turn setup had ranked strongest
  as zero while diplomacy already used DOS's ascending order. DOS's live
  `DS:917c` in fresh 1592 reads `00 02 03 01`, the reverse of the old port
  table. This corrects Isabella's three field elections and removes 68
  archived fields across twenty transitions without regressions. The sorter
  also reproduces descent reinsertion before ties; the rank regression
  covers distinct scores and that equal-score case. Full tests (96 enabled),
  golden, Release turn tests and both archive sweeps pass.

- Save capture preserves education nibbles beyond a colony's current
  population. Fresh 1592 Cayenne retains inactive slot 1 = 1, while the
  exporter zeroed all 32 slots before copying only the live roster. Retaining
  the loaded counters clears eight fields across 1591–1598 without
  regressions. The save round-trip regression seeds an inactive nibble and
  verifies capture preserves it. Full tests (96 enabled), golden, Release save
  tests and both sweeps pass.

- The `5952` closing Horse purchase is now ported (raw 95959-95972, asm
  2a03–2a63): after turn 40, stock below two and a nonzero stack-head
  capacity permit buying two Horses for 10 gold. Fresh 1592 Curacao watch
  `DS:650a` catches the debit and 0→2 write at `5952:2a63`. Its tile is empty:
  unchecked unit index −1 reads `DS:312a`, terrain row 27's labor byte 24,
  then capacity address `5237+24*e = 5387`, the tutorial byte. The port
  reproduces that alias using the existing terrain table and saved tutorial
  flags. Four archive transitions improve by eight fields, none regress;
  fresh 1592 falls to ten differing fields. Regression covers turn, purse,
  existing herd, live carrier and non-carrier, and the empty-tile tutorial gate.
  Full tests (96 enabled), golden, Release build tests and both sweeps pass.

- `5952` threat scoring now includes boarded land passengers in the whole
  neighbouring stack. Ships themselves still contribute zero. This aligns
  fresh 1592 Curacao's garrison quota 3 (formerly 1), leaving nine fresh
  fields. The archive improves by four fields overall; its Curacao quota
  remains the older value 1. Three transitions improve and 1592 worsens
  by one archived field. The threat regression now boards one Brave and
  verifies that its contribution survives while its carrier adds none.
  Full tests (96 enabled), golden and Release tests/sweep pass.

- `153e` now applies its shared treaty cooldown exit after AI encounters,
  and human talks write the other nation's timer for the speaker, as in
  raw 98414–98425. The cooldown remains `(6-difficulty)*2`, halved by the
  speaker's Benjamin Franklin. Fresh 1592 matches all three treaty timers
  and falls from nine differing fields to six. Archives improve by three
  fields overall across thirteen changed transitions; seven older French
  timer values differ after this correction. Fourteen exact transitions
  remain. Regression checks signing, skipped treaty cadence, timer direction
  and Franklin ownership. Full tests, golden and Release checks pass.

- Save import now restores a zero Pioneer tool count instead of retaining
  the spawn default of 100. Source 1592 already contains the zero-tool
  Pioneer; the DOS watch at `0c4b:08a2` confirms record compaction retains
  zero, and the fresh end-of-turn save does too. Ten archive transitions
  improve by one field each, none regress; fresh 1592 falls to five fields.
  The import regression includes a zero-tool land Pioneer alongside a
  loaded ship/passenger chain. Full tests (96 enabled), golden and Release
  tests/sweep pass; fourteen archive transitions remain exact.

- `0a60` garrison admission now walks the tile chain, latest arrival first,
  within each of DOS's five military preference passes (raw 87684–87760).
  Storage order previously admitted an older Dragoon at fresh 1592 Isabella,
  leaving the newly ejected Dragoon with `?` rather than DOS's `A`. Five
  archive fields improve across four transitions, none regress; fresh 1592
  falls to four fields. Regression pits two same-type arrivals against one
  remaining admission. Full tests, golden and Release tests/sweep pass.

- `457e` now retains `3558`'s delivery-cargo classification after a sell,
  alongside the already saved stack count (raw 89393–89408, 89935–89943).
  Recounting emptied holds prematurely sent fresh 1592's Spanish Caravel
  home from (53,52), whereas DOS wanders first and departs from (53,53).
  All three ship/return-tile fields now match, with no archived regressions.
  Regression checks that a sold delivery hold still blocks this cadence.
  Full tests, golden and Release tests/sweep pass.

- The `5952` indoor pass now uses `28c8` mode −2: scoring restores the
  original job and does not commit a plot after clearing DS:034d (raw
  13142–13144). Assignment scoring remains separate. This removes the
  stray Quebec owner stamp at (50,36), making fresh 1592→1593 byte-identical.
  Nine archived transitions improve by one field and 1546 gains one map
  field, net eight fewer differences. Fresh 1553 remains byte-identical;
  fresh 1535 retains its five tile-chain fields. Regression compares a
  probe and an assignment on the same idle worker and bought/native plots.
  Full tests (96 enabled), golden and Release tests/sweep pass; both builds
  report 4,627 archive fields with fourteen exact transitions.

- Ship movement now performs the destination's existing-stack sort before
  arrival, including boarded passengers in that physical chain (465b raw
  75709–75710). Previously only land movers sorted it, and the sort dropped
  passengers. This restores fresh 1535's `41→40→4` chain and makes that turn
  byte-identical. Five archived fields improve with no regressions. The
  movement regression sends a hull onto another hull/passenger tile and
  checks their resulting stack order. Full tests (96 enabled), golden and
  Release tests/sweep pass. Fresh 1553 and 1592 remain byte-identical.

- The wander scorer now retains DOS's shared stack word across unit acts.
  `521d_6d8e` asm `6fa4` leaves SI at `nation * 0x13c`; a subsequent
  `6662_00f2` flood saves SI at the same address as the next `20e6`'s
  uninitialized `local_ea`. The fresh 1558 watch caught Spanish unit 42's
  `PUSH SI` writing 632 before Dragoon 41's border park. Adjacent sign
  steps do not enter that frame. The mover reports actual flood entry,
  and the nation dispatcher resets the transient word before its unit waves.
  This removes 94 archived fields across nine transitions, with none worse.
  Fresh 1558 has one treaty timer difference; fresh 1598 has nine leaves,
  down from twenty. The regression walks an adjacent then distant goal
  before a different unit parks without an attack candidate. Full tests
  (96 enabled), golden, Release unit tests and both campaign sweeps pass.

- AI encounters now require movement, rather than any progress in an act.
  `465b` raw 75797 invokes `3180` from its movement tail. A border park
  changing a plan to `F` must not refresh the neighbouring nation's timer.
  This closes the last fresh 1558 difference: the complete save now matches
  byte for byte. Nine archived transitions improve by one timer field;
  1591 adds one archived timer difference. Fresh 1598 has eight leaves left.
  A regression preserves the peer timer when a unit parks without moving.

- `5952` absorption now enumerates the physical tile chain, latest arrival
  first, like `15eb_09c0` raw 10014-10034 and `0924` raw 9959-9979.
  Storage order had reversed St. Louis's two newly admitted workers in 1598.
  This clears six archived and fresh fields, leaving just Isabella's slot 6
  job and its plot assignment in the fresh save. The existing two-colonist
  absorption regression now uses distinct professions and checks their order.
  Full tests (96 enabled), golden, Release stage seams and both sweeps pass.

- Wealth ranks now follow DOS's load-resume lifecycle. `129f` raw 6354-6356
  skips `00f8` while `DS:0829` marks the loaded human turn; the unsaved
  rank table starts at zero. `153e` raw 97404 refreshes it on an audience,
  and the next normal turn refreshes it at setup. Import resets the table;
  an audience before ending the loaded turn is retained by the setup cache.
  Fresh 1598's Statesman score is 348 (weight 12), versus the former 145
  (weight 5); it beats the field score 342 and closes the last two fields.
  Fresh 1535, 1553, 1558, 1564, 1566, 1592 and 1598 all match byte for byte. The archived
  sweep adds two differences in each of 1564, 1566, 1567 and 1598, because
  its rank-dependent jobs differ from a cold replay. Fresh 1564 confirms
  both added archived differences are correct and is byte-identical now.
  Fresh 1566 is also byte-identical, clearing three differences from the
  former rank lifecycle despite its 202 archived differences. The rank regression covers
  cold load, a pre-end-turn audience and subsequent normal setup. Full tests
  (96 enabled), golden, Release turn and 20e6 tests and both sweeps pass.

- Fresh 1498→1499 now verified byte-identical as well. The archived
  Soldier's `W` recall, native friction and growth differences do not
  reproduce after loading the source in DOS; the port's `9` wander and
  native records match the fresh replay. No recall change was made.

- Expert purchases now apply DOS's explicit one-point tax increase through
  the existing tax clamp. `5952` assembly at 29f2-29f7 passes delta 1 to
  `291f_0ae0`; the decompiler omitted those arguments. This closes fresh
  1584's sole remaining difference. Eleven fresh replays checked this round
  match byte for byte. Full tests (96 enabled), golden, the Release purchase
  regression and both archived sweeps pass; the sweeps total 4,521 leaves.

- `0a60` stance now uses only local presence and strength, as in raw
  87995-88121. Removed the port's additional overrides for a war anywhere
  and the global Indian hostility latch. The local native-presence table now
  includes village population as well as Brave combat strength; omitting it
  was the source of the apparent 1554 regression. Fresh 1597 carries its
  dragoon to the colony instead of unloading on a beach, clearing all 12
  fresh differences, while fresh 1554 returns to its prior four physical
  ship-chain differences. All eleven earlier fresh exact replays remain
  byte-identical. A focused village-without-Brave regression covers the
  population table, and the defender-choice fixture supplies local reserves
  to enter the offensive tier. Full tests (96 enabled), golden, logic-map
  validation, Release 20e6 and war-land tests pass. Debug and Release archived
  sweeps both total 4,520 leaves with 14 exact transitions.

- The `20e6` own-colony berth block now runs before the `t`/`i` tasked-hull
  gate, matching raw 2996-3138's position above the first `local_6` checks.
  Fresh 1576 exposed the shared error: a tasked Dutch Merchantman retained
  four stale Europe cargoes and left its passenger ashore. DOS first dumps
  the hull, reserves and assembles the passenger, reloads three colony goods,
  then resumes the stored founding goal. The corrected replay is byte exact,
  clearing all 20 fresh differences. The berth regression now covers both
  untasked and tasked hulls. Full tests (96 enabled), golden, logic-map
  validation and the optimized 20e6 unit pass; Debug and Release archived
  sweeps both total 4,506 leaves with 14 exact transitions.
