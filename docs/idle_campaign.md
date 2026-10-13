# Seed-100 idle campaign gate (`golden_idle_campaign`)

STATUS: working doc, opened 2026-10-06. Gate and harness: [tests/README.md](../tests/README.md).
Fixtures: `original_saves/1492-1600-seed-100/year_*.sav` (VR_SEED.EXE autosaves, human idle in
View mode, never moves).

## How to measure

Each transition is one `turn_end` from DOS save Y compared with DOS save Y+1, so years are
independent. Field-level diff (byte offsets are useless here):

```
GOLDEN_IDLE_DUMP=/tmp/x/sim.sav ./build/debug/golden_idle_campaign Y Y+1
./build/debug/sav_json /tmp/x/sim.sav /tmp/x/sim.json      # explicit output path!
./build/debug/sav_json original_saves/1492-1600-seed-100/year_<Y+1>.sav /tmp/x/dos.json
```

then a recursive JSON diff (show `*_hex` blobs as differing byte offsets). The target is
`EXCLUDE_FROM_ALL`: rebuild it explicitly (`cmake --build build/debug --target golden_idle_campaign`).

Progress (sum of differing JSON leaves over all 77 transitions): 18,869 → 16,546 → 16,178 →
13,423 → 12,739 → 8,798 → 8,122 → 7,023 → 5,666 → 5,597 → 5,591 → 5,531 → 5,502 → 5,431 → 4,774 → 4,771 → 4,745 → 4,744 → 4,676 → 4,668 → 4,660 → 4,656 → 4,653 → 4,643 → 4,638 → 4,635 → 4,627 → 4,622 → 4,528 → 4,520 → 4,514 → 4,522 → 4,521 → 4,536 → 4,520 → 4,506 → 4,497 → 4,467 → 4,461. Fifteen archived transitions
pass byte-for-byte: 1493→1494, 1494→1495, 1495→1496, 1496→1497, 1505→1506, 1512→1513,
1513→1514, 1514→1515, 1516→1517, 1527→1528, 1537→1538, 1541→1542,
1542→1543, 1543→1544 and 1547→1548. 1492→1493 is down to the
human's first-turn UI; 1497→1498 only to the stance-table artifact below.

**From-load target.** Because DOS keeps unsaved state across turns (stance table, below), the
archived Y+1 save is not always reachable from Y. Load year_Y in DOSBox (`setup --save`, `load_slot`,
then press space until `DS:0x538e` changes); the human-slot autosave lands in `$W/C/COLONY09.SAV`. Wait for its mtime to change (keep pressing
space): `setup` copies a stale COLONY09 from COLONIZE/, and a popup can delay the write.
`sav_json` it and diff against the sim: whatever remains is a port bug, whatever the archived save
adds on top is an artifact. 1498→1499, 1501→1502 and 1502→1503 through 1504→1505 match their from-load
autosaves exactly, and so do 1496→1497, 1497→1498, 1498→1499, 1508→1509, 1511→1512, 1515→1516,
1523→1524, 1526→1527, 1528→1529, 1536→1537, 1548→1549, 1551→1552 and 1555→1556 (the archive adds unsaved-state diffs there). The 1533→1534 Europe-lane chain link now matches DOS; the archived
save still differs in a Brave's movement and map bytes.

DOSBox method used here (docs/dos_trace.md): `setup --save year_Y.sav`, load, then `BPM` on the
record bytes that differ (unit chain +0x18, colony +0x8a / +0x70) or `BP` on a resident routine
(game DS 0x237e; resident CS = Ghidra segment − 0x1000 + 0x0824, e.g. 15eb → 0e0f, 1427 → 0c4b).
A hit in a high CS is an overlay: find the bytes in the EXE, map the file offset with
`tools/rtlink_overlay_extract.py`'s segments.json (segmentIndex N = OVLN).

## What the autosave is (DOS year loop, raw 6330-6470)

Day top: `FUN_281f_0550`, `+0x3149 = 0` for every unit (spent MP), `0676` Indian mid-pass.
Then slot by slot: `0644` (= `FUN_3844_00f2` EOT, ending in 06ba lanes + `4962_0018` census),
for the human `FUN_130d_0172` autosave **before** Move Pieces (`FUN_2b5a_3b68`, which sets
`DS:0x5392 = 0xffff`). The calendar step is at the loop **end**. So with human slot 0 the
autosave holds: AI slots of year Y (old date), day top, human EOT of Y+1, nothing of the
human's Move Pieces. Head UI words at that point: `map_modal_active 0`, `no_unit_selected 0`,
`map_mode` = whatever the player left, `active_unit` = last AI hull that sailed for Europe
(raw 76484/77099 set 0x5392 before `291f_0208`), else 0xffff.

## Fixed passes 2026-10-06 to 2026-10-08

Archived to [archive/idle_campaign_fixed_2026-10-06_08.md](archive/idle_campaign_fixed_2026-10-06_08.md)
(grep it by `FUN_` name or transition; do not read whole).

## Harness artifacts (not port bugs)

- 1505→1506: the archived save matches the port; the DOSBox from-load run differs (its Spanish
  caravel takes the random far-roam arm), so unsaved state diverges there. Prefer the archive when
  the port already passes it.

- The `-0x6790` stance table (DS:0x9870) is not in the save and is zero after a DOS load; the
  continuous run carried last turn's values. A DOSBox load of year_1497 reproduces the port's
  Isabella `short_defenders`/`specialty_cargo`, not the save's.
- A fresh DOSBox load of `year_1534.sav` followed by one idle turn produces a 24,057-byte
  autosave identical to the port's output. The archive's four differences are New Amsterdam's
  `short_defenders` bit and three stale goto/facing bytes on a Europe Scout. The Scout is born
  in slot 46, then compacted to 44; its slot 46 bytes are zero after a fresh DOS load, matching
  the port. The archived continuous run carried (53,52,7) in unsaved memory.
- 1526→1527 still has one archived byte at a new unit's stale +0x15 (DOS 236, port 0),
  but the previous from-load DOS replay matched the port; do not assign the archive's
  inherited byte to a port writer without another live trace.
- DOS's music picker (`FUN_129f_0008`) draws twice at the human's slot start; harmless because
  every slot's 5e52 reseeds.

## Fixed in the 2026-10-10 pass

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

## Fixed in the 2026-10-12 pass

- AI field-plot purchase (FUN_0000_6582 tail) no longer needs a Europe store in
  the world. `colonies_seat_new_colonist` builds its context without one, so
  on the founding seat (`colonies_found` -> 28c8) an AI nation that could
  afford the plot took the alarm charge instead of buying. Fresh 1586: French
  gold, `lands_bought` and `alarm_by_player` now match.
- `FUN_15eb_26e4` claim stamp (via `FUN_281f_0c72`) now runs where DOS runs it:
  at founding (`FUN_364b_1ba8` raw 58088, founder on occupation 0 and no plot
  yet), before the 3930 plot pass in the 5952 AI tick (raw 93958 before 93961;
  the port ran it after), before 3930 in EOT production (`FUN_364b_0688` raw
  57228), and on colony-screen open (`FUN_2f2b_6cd4` raw 56009). A DOSBox `BPM`
  on the layer3 byte found the writer: founding Cayenne (40,21) restamped
  (41,20) from France to tribe 6. Fresh 1586→1587 is now byte exact.
  Fresh 1576, 1577 and 1583-1588 are exact; 1578, 1581 and 1582 keep 17, 9
  and 5 chain leaves.
- Archived: 1496→1497 becomes exact (15 exact transitions); 1533, 1577, 1586
  and 1596 shrink. 1593, 1595 and 1597 gain one `HAS_UNIT` mask byte each,
  downstream of unit arrays that already differed in length. (Scratch-sweep
  leaf metric, which lumps unequal-length lists into one leaf: 1,825 → 1,712.
  It is not the metric of the progress line above.)
- Regressions: `test_land_work_alarm_6582` arm (4) (AI buy with no Europe
  store) and `test_found_claim_stamp_1ba8` (unit_ai_contact_colony).
- The 20e6 colony join (raw 89345 -> `FUN_291f_09a4` -> `FUN_364b_1b1a`) now
  runs DOS's order: 26e4 claim stamp, 3930 plot pass, then admit with
  `FUN_15eb_1068(idx, 0)` (occupation 0, no plot; asm 364b:1b3a pushes the
  0). The plot comes from the colony's next 3930 pass, not a 28c8 seat.
- Fresh 1596→1597 (new DOSBox target, 38 leaves) traced three 20e6 errors,
  now byte exact except one `vis_mask` bit:
  - Missionary strip (raw 90088): DOS keeps spent MP (+0x3149), so the new
    Colonist has 1 MP, not the Missionary's 2; the same 20e6 call then falls
    through to LAB_4d2e (`ai_euro_20e6_4d2e_tail`, extracted). The port's
    re-act had joined the unit to Vlissingen on the spot.
  - Labor walk (OVL14 0x3000): `MOV DX,0x33; JMP LAB_27f5` binds the colony
    as plan '3', act state 0x0b (arrival exhausts MP), not a 0x0c step.
  - The pre-4d2e act-state gate (raw 90210) now runs after the whole arm
    chain, as in DOS. A Dragoon on its 0x0c step tile ran the ring scan
    (+0x3154++) and the hop roll first; the port skipped both, shifting the
    French RNG stream by one draw and the Merchantman's far-roam roll.
    `unit_goal_walk_keeps_stored_goal` now spends a third of MP: 5b66 (raw
    90552) runs 20e6 for a fresh 0x0b unit, and its stance-0 Soldier would
    take the PATROL recall.
- Archived sweep (scratch metric) 1,712 → 1,580; 1596→1597 151 → 20. Tests:
  `unit_labor_arm_binds_colony_goal`, `unit_missionary_strip_keeps_spent_mp`,
  `unit_step_tile_still_runs_ring_scan`.
- DS:0x8dc4 is one global (`units_last_hold_amount`), rewritten by every
  0aec -> `FUN_15eb_317c` hold removal and never saved. The 5d04
  departing-ship buy gate (`price * 0x8dc4`, raw 93041) reads it stale across
  hulls and nations; the port had zeroed it per hull. DOSBox 1591: a French
  unload left 100, so the Dutch hull (boycotted hold, 61 gold) bought nothing
  where the port bought three cargos. The musket and horse branches now
  follow raw 93000-93010: their 0aec overwrites the lots value, and the horse
  bank adds the horse amount. Not yet modelled: the 32a0 / 35d0 writers.
- The save bridge boards passengers in transport-chain order, head first, so
  `cargo_ids[]` ends with the hull's `prev`; LAB_3558's unload walks it
  backwards. DOSBox 1591: chain 52 -> 47 -> hull, DOS put 47 ashore.
- The 20e6 prologue and explorer flag (raw ~88549, which bumps the
  -0x5ec4 per-continent cap) now run before the own-colony garrison block
  (raw 88584). DOSBox 1597: two garrisoned Spanish Soldiers filled the cap,
  so the second Dragoon at (45,52) wandered instead of taking its peer's ring
  goal (the 1577 lead's mirror case). Fresh 1597 is byte exact.
- New fresh targets: 1591 (138 → 8 leaves), 1593 (69 → 52), 1596 (1),
  1597 (0). Archived sweep 1,580 → 1,475. Tests:
  `stale_scratch_carries_across_hulls`, `test_import_passengers_in_chain_order`,
  `unit_garrison_soldier_counts_explorer_cap`.
- `FUN_5bfb_10ec` (war-worthiness) had its entry gate inverted: asm
  5bfb:10f4 `CMP [0xa153],AL; JNZ body` weighs a pair only while the human is
  NOT top-ranked. DOSBox 1593: France and the Dutch went to war on contact.
- `FUN_521d_20e6` LAB_4393 work-queue pick: distance is `FUN_281f_037a` =
  map_dos_dist, not Manhattan, and the OpenCol "+32 when the hull carries the
  colony's specialty" term is deleted (DOS raw 89889-89893 has none). DOSBox
  1593: two Dutch Merchantmen took each other's queue colonies.
  `unit_specialty_flag_a_haul_match` now pins that the specialty does not
  bias the pick.
- Fresh 1593 and 1597 are byte exact; archived sweep 1,475 → 1,438. Test:
  `unit_10ec_skips_while_human_tops_rank`.
- `FUN_479b_0972` -> `FUN_465b_0000` also runs for a spent unit (a hull the
  LAB_3558 unload exhausted): cost > left with spent != 0 reseeds (raw
  75649) and rolls (raw 75825), a draw that cannot pass at 0 left. The
  port's mover refuses 0 MP up front, so `ai_euro_goal_walk_479b` now burns
  that reseed + draw. DOSBox 1591: the French Merchantman's failed step drew
  365 before the next unit's 20e6. Two other AI move sites now pass the 465b
  reseed too. Fresh 1591 138 → 1 leaf; archived sweep 1,438 → 1,434. Test:
  `unit_spent_step_burns_465b_roll`.
- Europe lane chains (the old 1578/1581/1582 "mixed sentinel order" lead):
  - `col1_find_ship_root` walked `prev` first, so in a two-hull lane chain
    (59 -> Caravel -> 49 -> 56 -> 58 -> Merchantman, fresh 1582) the Caravel
    got all four Dragoons. A passenger belongs to the hull on its `next`
    side; `prev` is now the fallback.
  - Europe departure boarding (`FUN_291f_0ec2` -> `FUN_1427_10be`) picks the
    boarders walking the dock from the latest arrival until the hull is full,
    and the chosen group rides in arrival order. Fresh 1578: dock 53 -> 54 ->
    56, the Caravel took 56 and 54, chained 54 -> 56 -> hull. This also fixed
    fresh 1571 (Dutch Artillery + Missionary) and 1581.
  - A hull-at-head chain (hull -> p1 -> p2, the shape 10be leaves before
    the 040c move) now imports in chain order too, so the colony disembark
    lands p1 first. Fresh 1575 (Dutch Missionary + Artillery) is byte exact.
    Test: `test_import_hull_head_chain_order`.
  - Fresh 1571, 1578, 1581 and 1582 are byte exact. Tests:
    `test_import_two_hull_lane_chain`,
    `dock_boarding_takes_latest_arrivals_in_arrival_order`.
- Village visits (DOSBox 1594, a captured French Colonist):
  - The 20e6 colonist live-among case (raw 89363-89372) now sets plan 'L' and
    commits a step onto the village (LAB_589e), not an in-place visit.
  - The AI walk's village entry ends through 465b's 0bd1 tail: act state 0,
    and +0x315a is bumped.
  - The AI a618 visit draws its skill and roll from the village-seeded shared
    stream, then reseeds from the timer, as OVL13 0x36b8 / 0x381b / 0x392d do.
  - Fresh 1594 is down from 30 to 7 leaves. The archived sweep went from
    1,434 to 1,422.
  - No dedicated unit test yet.
- `units_ship_departure_pickup` (FUN_1427_10be at the old tile) now walks
  the tile chain from its `next` end. A hull comes first, then its passengers
  nearest-first. Each boarder goes to the front of `cargo_ids`, because 10be
  plus the 040c group move leave the first boarder nearest the hull.
  DOSBox 1574: a Caravel took the near Dragoon from another hull. Fresh
  1574 is byte exact. Test: extended `ship_switch_quirk`.
- `colonies_assign_field_w` now stamps the plot's owner nibble with the
  colony's nation, unless a unit or settlement stands there (FUN_15eb_06d2
  raw 9858-9864). DS:0x34d, the flag gating that tail, is FUN_15eb_28c8's
  trial-seat flag: it is 1 only around 28c8's candidate loop. The archive
  is neutral. Test: the 06d2 check in `test_land_work_alarm_6582`.
- Land combat relinks the attacker on its own tile before the roll
  (FUN_5fef_1b0e raw 100568-100569, `0916` then `089e`), so the attacker
  becomes the tile's latest arrival. DOSBox 1554: a Spanish Dragoon that
  attacked a village. Fresh 1554 is byte exact. Test:
  `case_1b0e_relinks_attacker`.
- Captures follow DOS's 0812/0894/0844 order: unlink, flip the flag, then
  link on the winner's tile. The old tile keeps its owner nibble (DOSBox
  1594/1595). Fresh 1595 is byte exact. Test:
  `case_capture_keeps_old_tile_owner`.
- The AI x AI encounter (3180 -> 153e -> 13b0) changed in two ways:
  - It no longer skips 13b0 when the port-only "deep Indian hostility"
    sticky is set. DOS sends an AI self straight to 13b0 (raw 97406-97411).
  - It now ORs MET (0x20) into both relation bytes, as 3180 does after 153e
    returns 1 (raw 98676-98678).
  - DOSBox 1557: Spain and France met and signed (0x60). Fresh 1557 is byte
    exact. The sticky test in `case_r3_r4_sugar_rum_cigars_boycott` now
    expects the signing.
- New fresh targets are byte exact: 1498, 1501, 1502, 1504, 1508, 1511, 1515,
  1528, 1557, 1561, 1562-1564, 1572,
  1566-1568, 1573, 1592 and
  1598. To make them,
  the replay script waits for the next break after a `run_until_break`
  timeout instead of sending `RUN` again.
- 5952 indoor-expert pass (OVL15 0x1cb2-0x1e65) is now ported as
  `ai_euro_5952_indoor_expert_pass`. It runs before the scored indoor pass.
  - Experts whose @JOB row is 9..0x11 get seated in their own workplace when
    one is free and fewer than 3 are seated there.
  - A Carpenter is seated when there is no lumber shortfall and there is
    lumber stock or gross output. If the colony wants construction and has
    more than one carpenter, it is demoted to Free Colonist instead.
  - A Preacher is demoted during the War of Independence; a Statesman is
    seated only outside it.
  - Crafts are seated when there is no input shortfall and there is input
    stock or gross output.
  - Gunsmith is skipped (ponytail note in the code).
  - Fresh 1552 is byte exact; archived sweep 1416 -> 1413. Test:
    `expert_pass_seats_master_carpenter`.
- AI unloads now run 3180's Euro halves. `ai_euro_unload_pax_at` lands
  through FUN_465b_0000 (LAB_3558 -> 465b_0c1e), whose move tail runs
  0984 -> 0192 -> 3180. The port ran only the Indian half there. The human
  153e limb and the AI x AI 13b0 limb now share `ai_euro_3180_euro_side` with
  the act drain. DOSBox 1591: a Dutch Dragoon unloaded at (24,6) beside French
  units sets `nations[1].treaty_timer[3]` = 12. Fresh 1591 is byte exact;
  archived sweep 1413 -> 1412. Test: `unit_unload_runs_3180_ai_encounter`.
- 1b0e attacker restore point. Raw 100568-100569 (`0916` + `089e` = 12f6 ->
  0362(unit, -2, -2)) park the attacker off map for the fight. 0948 puts a
  survivor back on its saved tile after the outcome: raw 100750 on a win,
  after the 0352 capture, and raw 100764-100768 on a loss. The port relinked
  it before the roll, so a captured unit landed ahead of its captor.
  - DOSBox 1594: French 45 captured Dutch colonist 44, giving the chain
    45 -> 44 -> 47 -> 48.
  - Fresh 1594 is down to `port_ext_hex` (port-only); 1554 stays exact.
  - Test: `case_1b0e_attacker_back_after_capture`.
  - FUN_1427_03a0 (via 08c6) sends a unit to the `prev` end. 1b0e calls it
    on the parked attacker, so the tile chain does not see it.
- 465b LAB_0673 pre-move vis OR (raw 75661-75664). A mover that is not the
  human ORs FUN_281f_0826(dest, own nation) into its vis: the dest owner bit
  plus every nation watching dest. It goes through 07fe = FUN_1427_0c72,
  which walks from the mover along `prev` (004a).
  - It runs before the attack (0a14) and before a granted move.
  - The mover itself is cleared again on arrival (08da), so the bits stick
    on an attacker and on the units behind it on the tile it leaves.
  - DOSBox 1596 (BPM on the unit's +3 byte, hit 0c4b:0c8a): a Dutch hull
    leaving Vlissingen left France's bit on the colonist behind it.
  - Fresh 1596 is byte exact. Test: `case_465b_0673_vis_or_behind_mover`.
  - Trace note: DOS deletes dead units at turn start and shifts the array,
    so a BPM on a slot from the loaded save can follow a different unit.
- 5952 placement stop (LAB_5952_178f, asm 5952:178f-17a6). The food pass
  and the general passes call `FUN_281f_0b6e` = a real FUN_15eb_28c8 seat,
  then test DS:0x8dbe. On a poor winner they jump to 178f:
  `0aa6(slot)` = 169c (clear the plot) and `0c36(slot, 0x12)` = 1068 with the
  idle job.
  - The seat's 06d2 has already stamped the plot: the owner nibble, the land
    reaction, and the purchased bit (0668).
  - The port skipped the seat. It now seats, then unseats, in
    `ai_euro_5952_178f_unseat`.
  - DOSBox 1556 (BPM on the layer2 byte; stack 137f_0184 <- 15eb_0668 <-
    06d2 <- 28c8:2e64 with args (slot 0, -1)): Guadeloupe's colonist stamped
    (34,28) purchased before the carpenter arm took him.
  - Fresh 1556 and fresh 1546 are byte exact; archived sweep 1412 -> 1410.
  - Test: `case_178f_stop_keeps_seat_stamp`.
  - `plot_was_worked_mask` is now reset at the start of each tick, because
    DOS rebuilds the 26e4 claim table every tick. This was neutral on every
    measure.
- Unported: the human-side join (`FUN_364b_1aec`) also stamps and runs 3930
  first. The port's colony-screen joins do neither.

## Open leads (most transitions first)

- Fresh 1565→1566 (14 leaves): Spanish Dragoon (save 38) steps (50,36) to
  (49,36), and DOS then border-parks it ('F'). Its 20e6 `local_ea`
  (SS:E7C4, BP-0xE8) reads 49 at the park. A DOSBox BPM traced that write to
  8c5a:042F, `OR [BP-0x28],AX` in `FUN_5fef_0352` (combat, via
  1b0e -> 2a1f_06e0). It happened between the Dragoon's two 20e6 calls, so it
  most likely came from another unit's combat in that act pass. The port's
  `ai_euro_wander_stack_word` models only the 00f2 flood spill. Modelling the
  combat spill needs the frame depth of each call path; it is not done yet.
  - 2026-10-13: that word is 0352's `local_2a` (raw 99354-99361). It is
    `FUN_13e4_0074` (0/1: is the tile class 0x19/0x1a) of the loser's and the
    winner's tiles, ORed in. On the `else` path it is never zeroed, so the
    49 that DOS read is older stack contents plus a 0/1 flag. A faithful port
    needs the word's value before the fight as well, i.e. the frame layout
    of whatever ran last at that depth.

- (Resolved 2026-10-12, see above.) Fresh 1586→1587 was down to one byte in
  `map.path` at `(41,20)`. Earlier fixes on this transition:
  `FUN_521d_0492` reads the frozen `FUN_4962_0018` nation×continent colony
  census at `DS:0x94e6`; the port instead recounted the live colony pool.
  France founds Cayenne before its Dragoon acts in this turn, so the live
  recount erased the Dragoon's frontier bonus, suppressed its explorer flag,
  and shifted the Dragoon, both Braves, and the complete French transport
  group. Reading the frozen table makes every unit and all three map planes
  byte exact. The archived sweep improves from 4,489 to 4,452 leaves with 14
  exact transitions. A DOS buy trace had already resolved both Europe
  errors: `local_46` reads the frozen pre-planning dock census, and generic
  hold sales leave their amount in global `DS:0x8dc4`, which the later buy
  affordability gate reuses. The Merchantman's last sale leaves 20, so its
  purchases stop after Muskets, Trade Goods and Horses; Spanish cargo, market
  state and gold now match. `FUN_48d3_0346`'s leading `FUN_1427_10be` relink
  also restores the exact Caravel→passenger→Merchantman lane chain. Replacing
  the ring-hop's obsolete per-nation seen stand-in with the already ported
  `DS:0x9faa` coarse plane makes the French native-land purchase, gold and
  alarm match. The plane now also preserves 0a60's exact unit stamp: `5` for
  profession-carrying types and `1` for other types, including the stamps of
  passengers at their carrier coordinates; ring hops test `& 6` while far
  probes require the entire byte to be zero. This static correction
  is neutral in the archived sweep and this fresh transition.
  Fresh 1587→1588 and 1588→1589 remain byte-identical.

- (Resolved 2026-10-12, see "Europe lane chains" above.) Fresh 1578 and 1581→1586 replays match DOS in all modeled gameplay state.
  Fresh 1584→1585 was already byte-identical. The save bridge now restores
  Europe sentinel physical order from the incoming chain and serializes
  sibling hulls by that order rather than record index: fresh 1583→1584 and
  1585→1586 become byte-identical, and the archived sweep falls by nine leaves
  to 4,497. A reversed-record-order hull chain with a passenger covers the
  import/export rule. Fresh 1578 still has an identical-record swap plus chain
  differences; fresh 1581 and 1582 retain 10 and 5 chain leaves, so DOS has an
  additional mixed sentinel-chain ordering rule to trace there.

- (Fresh 1577→1578 is byte exact as of 2026-10-12; kept for history.)
  Fresh 1577 reduces the archived 1577→1578 cluster to 15 JSON leaves. One
  Spanish Dragoon is the only behavioral split: DOS gives it the same
  explore-ring goal at (48,49) as a peer already at (45,49), while the port's
  ring scan declines and its 4d2e wander ends at (46,55). The remaining unit
  chain and map-mask leaves follow from that move. Port tracing identifies
  the actor as unit id 42; it is skipped by the 0a60 Soldier/Dragoon
  low-presence goal-binding gate and diverges later in the 20e6 explore scan.
  Resolve the live DOS ring inputs or act ordering before changing the scan.

- Cross-transition triage (2026-10-10): the largest repeated colony cluster is
  Montreal population 1 versus archived 2 in 40 transitions, with related
  worker slots, defender flags, musket stock and unit-array shifts. Fresh
  1548, 1551 and 1555 DOS replays reproduce the port's population, so this cluster
  needs a later fresh replay before any behavior change. Isabella population
  also differs in 23 transitions. Raw per-slot unit differences need identity
  matching first because creation/deletion shifts many following records.
  Scratch report: `/tmp/idle_common_diffs.json` (field frequency and years).

- A new 1535→1536 fresh DOS replay (2026-10-10) leaves five JSON fields,
  all final shared ship/passenger tile-chain links: DOS orders the members
  `41→40→4`, while the port writes `40→41→4`. The previously reported three
  new-slot goto/facing bytes and Montreal's `short_defenders` now match this
  fresh replay; do not fit the older target's unsaved bytes.
  A DOS watchpoint at `137f:029e` traced the former (30,50) owner mismatch
  to a provisional `15eb:06d2` work-plot seat from `15eb:28c8`: the trial is
  undone but its owner stamp remains. The port reproduces that stamp for the
  unseated-worker path; scoring an employed worker in 1527 did not stamp it.
- 1536→1537 now matches a fresh DOS load byte-for-byte. The French 20-tool
  colony purchase charges the sell-byte price but also calls `291f_0c14` to
  record 20 units at the Europe ask price in the buy-volume ledger. The six
  remaining native-unit leaves had one cause: 1816's `097a` reads each unit
  type's maximum MP. The port had hardcoded three thirds, so a Mounted Brave
  stopped after a six-third step instead of taking the second DOS attempt;
  its missing draw shifted the next Brave and Arawak-Spanish contact. The
  archive adds two stale goto bytes to the from-load target.
- 1537→1538 now matches both the archived and fresh DOS save byte-for-byte.
  The Dutch Merchantman at its own colony first loads 22 Furs, then uses the
  Europe dock-demand arm to collect a waiting land unit. Running that arm
  before the berth skipped the load; omitting it sent the hull on a different
  route. The port now checks dock demand immediately after its berth load.
- 1538→1539 now matches a fresh DOS load byte-for-byte. Once a Dutch Father
  election clears its bell pool, the nation-turn's later colony checks leave
  the next nominee unset despite the nonzero bells-made-this-turn counter.
  A French reveal of an English ship also stamps the two boarded passengers'
  visibility bits through the ship's tile chain. The archived save carries
  additional unsaved-state differences.
- 1541→1542 now matches both the archived and fresh DOS save byte-for-byte.
  A native Brave beside Quebec asks for food before the encounter's generous
  gift branch. DOS accepts the request for its AI owner and removes half the
  colony's post-production food (39 of 79); the extra roll also aligns a
  later Brave's third step. A live watchpoint on Quebec's food word at
  `DS:5eaa` caught the debit at overlay `8c5a:078f`. The port had attempted
  gifts first and skipped the beg when the generous branch returned true.
- 1542→1543 now matches byte-for-byte. A Brave's post-upgrade moves-spent byte
  and three passenger visibility bits match; the Fisherman's gross yield
  credits food in the AI colony ledger. A DOS scorer trace then showed New
  Amsterdam's fourth worker elects its eastern Fur plot with score 494 versus
  the port's old 456. `FUN_15eb_15c6` contributes 2 for a field good whose
  consumer's base building has a successor: DOS row 32 (Fur Trader's House)
  points to row 33 at `DS:0x8f86+32*0xc`. The port counted 1 for every
  consumer root. Correcting all five roots also preserves the 1505→1506
  byte-identical transition, which a Fur-only score adjustment had regressed.
- 1543→1544 is byte-identical too after the consumer-chain score correction.
- 1544→1545 matches a fresh DOS load byte-for-byte. The archived target has
  six stale bytes on two newly created unit records (goto 53,52 and cargo
  slot 5 = 236). Loading the 1544 save and advancing one turn in DOS leaves
  all six zero, as the port does; these bytes live beyond the save's unit
  count before the turn and depend on unsaved process history.
- 1545→1546 now matches a fresh DOS load byte-for-byte. The port serialized
  Spanish land units 43 and 46 at the shared Europe tile (238,238) as separate
  records; DOS links them 43→46 through the transport-chain words. The COL1
  save writer now links loose land units on the same Europe sentinel tile,
  leaving its existing passenger and ship chains in place. The archived
  target still has stale unit bytes from the earlier unsaved history.
- 1546→1547 now matches a fresh DOS load byte-for-byte. The fourth New Amsterdam
  worker stays on its eastern Fur plot: a DOS scorer breakpoint shows its
  indoor mode −2 plot probe yields Fur score 494; the port had deducted a
  forest penalty of 10 and elected Farmer score 486. In DOS, that terrain
  scratch word is uninitialized outside the food emergency arm. The native
  gift is 6 Tobacco. Its 2154 Horse bid divides by the live village-count
  table (`DS:0x962a[type]`), not game difficulty; using count 7 gives the
  same sorted keys and rank-3 Tobacco choice. Europe-lane ships also keep
  separate stack flags when a passenger chain is empty. A high-alarm colonist
  village visit consumes the DOS arm without teaching or adding the +3 alarm,
  and the 28c8 trial scorer leaves its owner stamp during the AI tick; these
  close the final alarm, learned-state, and (45,51) path residuals. The archived
  target still carries stale bytes from the earlier unsaved history.
- 1547→1548 now matches both the archived and fresh DOS save byte-for-byte.
  Three French units reach (45,49) in both runs. DOS sorts the destination's
  existing wagon-and-soldier stack before appending the next soldier, yielding
  soldier 41 → wagon 46 → soldier 38; the port had only appended arrivals.
  A watch on the three live unit x bytes showed the destination stack's
  existing links change during the third move.
- 1548→1549 now matches two fresh DOS autosaves byte-for-byte. The remaining
  nine bytes were the two Spanish Dragoons' route and resulting stack/map marks.
  Both runs first step (45,49) → (44,50), with far-tier waypoint (45,53).
  `FUN_6662_0906` raw 104185 overwrites `DS:0x1dd2` with Soldier for the
  coarse-snap probe and restores only the owner, so DOS's following flood
  uses one-tile movement costs. The port restored the Dragoon type instead.
  Preserving the probe type makes DOS's next south step to (44,51) win;
  the port formerly picked southeast then south to (45,52). The full 256-byte
  cost grid now matches the live DOS capture. A regression covers far-tier
  probe costs and near-tier retention of the mover's own terrain costs.
  The archived target still adds unsaved-state differences (11 JSON leaves).
  The arriving French caravel's earlier empty-ship fix remains in place:
  `457e` reads the passenger count snapped before `3558` disembarks the last
  passenger, leaving the hull idle at (52,44).
- The `457e` empty-ship cadence now uses DOS's compact unit-array index in
  its 1-in-32 turn check instead of the port's runtime ID. The existing
  cadence tests now align their beat to the slot; all earlier exact archived
  transitions still pass.
- 1551→1552 is down from 17 differing bytes against the fresh DOS autosave
  to an exact byte-for-byte match with the fresh DOS autosave.
  The Spanish Dragoon and downstream Tupi Brave now match, including visibility.
  DOSBox score and RNG breakpoints corrected the earlier route assumption:
  DOS moves the Dragoon (48,40) → (49,40) → (50,41), with first score vector
  N=7, E=13, SE=12, SW=12, W=-7. The earlier claim that both first steps
  went southeast was incorrect. Before those scores, DOS departs dock hull
  slot 4 from Europe inside 5d04 (291f_0ec2 → 48d3_0346 → 0002), reseeding
  to 100 and drawing `rng(1,100)`. The port postponed departure until the
  hull's own unit act, after the Dragoon had consumed the planning stream.
  Departure now runs at its DOS call site, and the unit act preserves the
  dock pass's damage gate. The archived output still has a different unit
  count and is not a suitable one-turn oracle for this input.
  The last byte was village deletion at (48,58): 4d56_00e0 clears only the
  settlement bit, leaving layer3 `0x7d`. The port additionally reset the
  owner to `0xf`, producing `0xfd`; that unsupported write is removed.

- Human end-of-slot draws: DOS 5, port 1 in 1497→1498 (human FF debate rolls / merc offer?).
- **AI colony tick vs DOS**: still the main source from 1499 on (worker/tile choice,
  `specialty_cargo`, `building_in_production` 255); trace the first diverging colony with the
  15eb:28c8 / 2d14 breakpoints. Downstream: alarm/friction, recruit pool / FF pick RNG phase.
- Remaining `transport_chain` diffs (~720 leaves) sit in transitions that already diverge
  elsewhere; recheck once those close.
- Remaining `vis_mask` diffs are mostly downstream of route/placement divergence; recheck after.
- DOSBox loops: press space every ~200 INT16 polls (popups and the idle human's turn both take it),
  and match units by content, not index — a colony founding compacts the array mid-turn.
- Nation +0x4b (`unknown_4b`): DOS writes it late-game (1..11 for nations 1/2); writer unknown.
- 465b LAB_0bd1 tail still needs other non-goal AI movers audited; the Brave foreign attack
  path is covered.
- 1492→1493 only: `tut2.nr1`, `rival_nation_slot_2`, `stuff.x/y`, `map_mode` come from the
  human's first Move Pieces (tutorial popup, View key); not reachable headless.
