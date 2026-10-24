# tolerated_dos_bugs evidence archive (2026-10-24 batch)

STATUS: archived 2026-10-24, closed. Test history and DOSBox evidence moved verbatim
when [docs/tolerated_dos_bugs.md](../tolerated_dos_bugs.md) and
[docs/tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) were trimmed. Not a spec:
each `TB-N` entry stays authoritative and keeps its DOS citation, port site, guarding
tests and a one-line removal summary; this file holds the full "Blocks removal" record
(first differing save, byte offsets, every failing test) and the trace anecdotes it
used to carry. Look up one id: `grep -n '^## TB-NN$' docs/archive/tolerated_dos_bugs_evidence_2026-10-24.md`.

## TB-2

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Passengers switch ships when another own ship leaves the tile first):

Evidence: idle independence seed-100 1->2, at (1,53) Galleon 109
keeps four Dragoons, takes unit 58's two, and leaves its own Artillery and
Colonist (DOSBox BPM trace 2026-10-16).

**Blocks removal:** the idle independence campaign (CTest
`golden_idle_independence`, from save 1->2) depends on the DOS pickup order;
boarding only the hull's own riders changes which units sail and breaks the
DOS match.

## TB-5

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Colony job scoring reads Horses for the Fisherman):

**Blocks removal:** tried 2026-10-21: with the fix, CTest
`golden_idle_continuous`, `golden_idle_independence`, `golden_idle_america`
and the three `_reload` variants fail (seed-100 1492 campaign first differs at
1535, America at save 29), and `unit_ai_euro_28c8_job_score` fails.
`golden_ai_joint` passed. Reverted.

## TB-6

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Euro AI buying from natives scores the wrong goods):

**Blocks removal:** tried 2026-10-21: scoring the offered goods' price bytes
(same first-strict-max rule) makes CTest `golden_idle_independence` and
`golden_idle_independence_reload` fail from save 4 (an AI wagon trade at a
village, independence 4->5). All other tests and `golden_ai_joint` passed.
Reverted.

## TB-7

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Villages-per-continent table holds only the last tribe's villages):

**Blocks removal:** tried 2026-10-21: counting every settlement per continent
makes all six idle CTest gates fail from the first compared save (1493,
independence save 2, America save 2). The table is in the save, so it differs
byte-wise at once, and the AI's colony planning follows. `unit_ai_indian_census`
also fails; `golden_ai_joint` passed. Reverted.

## TB-8

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Indian horse breeding credits the last settlement's tribe):

**Blocks removal:** tried 2026-10-21: breeding the current tribe instead makes
all six idle CTest gates fail (1492 campaign from 1535, independence from
save 2, America from save 46) and `unit_ai_indian_census` fail;
`golden_ai_joint` passed. Reverted.

## TB-9

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI colony-site scan gives its own-tile bonus to tile (0,0)):

**Blocks removal:** tried 2026-10-22: `score += 0x10` when
`tx == u->x && ty == u->y`; `golden_ai_turns` and `golden_ai_joint` fail at
TURN3-to-4 (founded_colonies 1 vs 0 for nations 1 and 2, colony_count 3 vs 1);
`golden_idle_continuous` and `golden_idle_reload` first differ at save 1495
(byte 0x2c, got 2b, DOS 2d); `golden_idle_america` and
`golden_idle_america_reload` first differ at save 14 (byte 0x2c, got 42, DOS
43). No other test fails. Reverted.

## TB-10

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI plot scoring indexes tribe tables with the nation id):

In the America seed-100 DOSBox run (save 56 to 57), New
Amsterdam's claim byte 11 reads colony 1's own word (0), so the penalty clears
and a veteran goes fishing instead of logging the tribe's plot.

**Blocks removal:** tried 2026-10-22: index the three tables with the tribe
index (nation - 4); `golden_idle_america` and `golden_idle_america_reload`
fail (first divergence at save 12, first byte mismatch at 0x599: got 00, DOS
04; later saves differ at 0x608 and 0x612); `golden_idle_continuous`,
`golden_idle_independence`, `golden_ai_turns`, `golden_ai_joint` and the other
goldens pass; no other test fails. Reverted.

## TB-11

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI tile-improvement scoring reads the tribe alarm with the nation byte):

**Blocks removal:** tried 2026-10-22: pass the tribe index to the alarm read;
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign,
first byte mismatch at year_1502.sav offset 0x3a6: got 05, DOS 00; RNG state
also differs at save 1496); no other test fails and `golden_ai_joint` passes.
Reverted.

## TB-12

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Refused food beg bumps the wrong alarm word):

**Blocks removal:** tried 2026-10-22: passing the Euro nation
(`ai_contact_alarm_delta_00f2(ctx, nation_id, e, d)`) makes CTest
`golden_idle_independence` and `golden_idle_independence_reload` fail (first
at save 5, byte 0x30d2: got 05, DOS 04; later saves drift on the same
alarm-word bytes and the RNG stream). `golden_idle_continuous`,
`golden_idle_america` and `golden_ai_joint` passed. `unit_ai_contact_core`
fails ("BEGFOOD decline must leave the European's alarm"). Reverted.

## TB-13

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI colony tick loses its own nation after the improvement arm):

Seen in the DOSBox America seed-100 run (save 58: the Wagon Train arm reads
tribe 7's word, 0, and builds the wagon).

**Blocks removal:** tried 2026-10-22: never write the neighbour owner to
`improvement_owner`; `golden_idle_continuous` (first byte mismatch at
year_1557.sav offset 0x3ae: got 0c, DOS 2b), `golden_idle_independence`
(000004.SAV offset 0xcb7: got 06, DOS 0f), `golden_idle_america` (000058.SAV
offset 0x2e4: got 0c, DOS 2b) and the three `_reload` variants fail;
`golden_ai_joint` and all other tests pass. Reverted.

## TB-14

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI road connect leaves the "current colony" on another colony):

Seen in DOSBox: tr9 11 to 12 (Bahia's job passes land on St. Louis),
America2 resync 37 (a specialty counted twice let England's Galleon buy 100
muskets).

**Blocks removal:** tried 2026-10-22: the road-connect payer is the ticking
colony and the tick never rebinds; `golden_idle_america` and
`golden_idle_america_reload` fail (first byte mismatch at 000044.SAV offset
0x23c: got 28, DOS 14); the narrower variant (the tools charge still lands on
the rebound colony, only the later arms stay on the ticking colony) fails the
same two tests at the same save (offset 0x599: got 00, DOS 05). All other
tests pass, including `golden_ai_joint`. Reverted.

## TB-15

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI road-connect skips a colony only when it is far on both axes):

**Blocks removal:** tried 2026-10-22: the distance test uses OR, with and
without the walk-origin reset of DB-23; `golden_idle_america` and
`golden_idle_america_reload` fail (first byte mismatch at 000006.SAV offset
0x2dc: got 0a, DOS 00; 000007.SAV differs at the same offset). All other
tests pass, including `golden_ai_joint`. Reverted.

## TB-16

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI plot scoring reads stack residue for the forest penalty):

**Blocks removal:** tried 2026-10-22: the tail's forest test uses the
candidate tile's class in every caller; `golden_idle_continuous` (first byte
mismatch at year_1523.sav offset 0x1a7: got 00, DOS 04), `golden_idle_america`
(000011.SAV offset 0x5e9: got ff, DOS 00) and their `_reload` variants fail;
`golden_idle_independence`, `golden_ai_joint` and all other tests pass. The
failures come from the tick alone: applying the fix only to the tick fails
the same four tests at the same bytes, while applying it only to the founding
join (DB-24) passes everything. Reverted.

## TB-17

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI indoor job election clamps by a stale quantity):

**Blocks removal:** tried 2026-10-22: clamp only for jobs with an input cargo;
`golden_idle_continuous` (first byte mismatch at year_1566.sav offset 0x33c:
got 11, DOS 06), `golden_idle_america` (000059.SAV offset 0x274: got 11, DOS
08) and their `_reload` variants fail; `golden_idle_independence`,
`golden_ai_joint` and all other tests pass. Reverted.

## TB-18

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI schoolhouse arm elects the table row index as the profession):

**Blocks removal:** tried 2026-10-22: electing `k_5952_craft[r].job`;
`golden_idle_independence` and `golden_idle_independence_reload` fail (seed-100
independence campaign, first differs at save 15, byte 0x74c, got 0e, DOS 01).
All other tests and `golden_ai_joint` passed. Reverted.

## TB-19

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI expert purchase charges the price of the colonist slot):

**Blocks removal:** tried 2026-10-22: charging the trained job's price;
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign,
first differs at year_1585.sav, byte 0x22, got 05, DOS 04),
`golden_idle_america` and `golden_idle_america_reload` fail (first differs at
save 55, byte 0x7d6, got 25, DOS 24). All other tests and `golden_ai_joint`
passed. Reverted.

## TB-20

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI starter-horses arm reads the unit before the tile head):

**Blocks removal:** tried 2026-10-22: returning when the tile has no unit;
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign,
first differs at year_1533.sav, byte 0x48e, got 00, DOS 02), and
`unit_ai_euro_5952_build` fails (`case_starter_horses_purchase`, which pins the
empty-tile purchase). `golden_ai_joint` passed. Reverted.

## TB-21

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI Europe recruit loop upgrades to Dragoon without paying for the horses):

**Blocks removal:** tried 2026-10-22: moving the Dragoon upgrade and the bank
update inside the payment gate; `golden_idle_continuous` and
`golden_idle_reload` fail (seed-100 1492 campaign, first differs at
year_1546.sav, byte 0xb4c, got 01, DOS 04), `golden_idle_america` and
`golden_idle_america_reload` fail (first differs at save 20, byte 0xf9c, got 01,
DOS 04). All other tests and `golden_ai_joint` passed. Reverted.

## TB-22

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI emergency lumber is credited even when the 200 gold is not paid):

**Blocks removal:** tried 2026-10-22: crediting the lumber only when the 200
gold is paid; `golden_idle_continuous` and `golden_idle_reload` fail (seed-100
1492 campaign, first differs at year_1565.sav, byte 0x6e6, got 00, DOS 64),
and `unit_ai_euro_5952_lumber` fails (`purchase_gates`, which pins the free
lumber). `golden_ai_joint` passed. Reverted.

## TB-23

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI equip arm strips the expert profession of the wrong colonist):

**Blocks removal:** tried 2026-10-22: testing `colonists[pick].profession`;
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign,
first differs at year_1529.sav, byte 0x9cd, got 1c, DOS 10),
`golden_idle_america` and `golden_idle_america_reload` fail (first differs at
save 16, byte 0xe5b, got 1a, DOS 1c), `golden_idle_independence` and
`golden_idle_independence_reload` fail (first differs at save 9, byte 0x28ff,
got 16, DOS 1c). `golden_ai_joint` and all unit tests passed. Reverted.

## TB-24

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI pioneer-clear ring scan counts water twice):

**Blocks removal:** tried 2026-10-22: `else if` after the water block;
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign,
first differs at year_1511.sav, byte 0x335, got 90, DOS b0),
`golden_idle_america` and `golden_idle_america_reload` fail (first differs at
save 2, byte 0x26b, got 98, DOS b8), `golden_idle_independence` and
`golden_idle_independence_reload` fail (first differs at save 12, byte 0xcad,
got 90, DOS b0). `golden_ai_joint` and all unit tests passed. Reverted.

## TB-25

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI hull cargo buy checks gold against the last hold removal):

(DOSBox 1591: a French unload left 100, and the Dutch hull with a boycotted
hold bought nothing)

**Blocks removal:** tried 2026-10-22: gate on `price * 100`; CTest
`golden_idle_continuous` and `golden_idle_reload` fail (seed-100 1492 campaign
first differs at save 1501, offset 0x444: got 00, DOS 02),
`golden_idle_independence` and `_reload` fail (first differs at save 14, offset
0x2788: got 03, DOS 04), `golden_idle_america` and `_reload` fail (first
differs at save 4, offset 0x788: got 00, DOS 01). `golden_ai_joint` passed; no
other test failed. Reverted.

## TB-26

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Euro AI Scouts and Colonists enter villages whatever the tribe's anger):

**Blocks removal:** tried 2026-10-22: Scout arm requires raw
`alarm_by_player[tribe][nation] < 0x19` and Colonist arm requires
`<= 0x18`; CTest `golden_idle_continuous` and `golden_idle_reload` fail
(seed-100 1492 campaign first differs at save 1533, offset 0xa17: got 2e, DOS
2f), `golden_idle_independence` and `_reload` fail (first differs at save 11,
offset 0x2c: got 74, DOS 73). `golden_idle_america` and `golden_ai_joint`
passed; no other test failed. Reverted.

## TB-27

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Adjacent-foreign probe overwrites its reference medium):

**Blocks removal:** tried 2026-10-22: the centre medium stays fixed, so a
settlement neighbour is also compared with it (a ship at sea beside a coastal
village no longer counts); CTest `golden_ai_turns` fails (TURN2->3: the nation-1
type-13 unit at (51,39) ends with orders 0 and goal (51,39), DOS has orders 12
and goal (50,39); TURN4->5 misses a unit), `golden_idle_continuous` and `_reload`
fail (seed-100 1492 campaign first differs at save 1494, offset 0x1e2: got 00,
DOS 0c), `golden_idle_independence` and `_reload` fail (first differs at save 2,
offset 0xa24: got 01, DOS 0b), `golden_idle_america` and `_reload` fail (first
differs at save 7, offset 0x711: got 0c, DOS 0f). `golden_ai_joint` fails
(TURN2_to_3, TURN4_to_5). Non-golden: `unit_ai_euro_20e6`
(`unit_adjacent_foreign_domains`, a colony across a coast must be detected).
Reverted. A narrower fix (settlement neighbours still always pass, only the
carry-over of the overwritten medium to later tiles is dropped) passes every
test, but leaves the ship-beside-a-village case as DOS has it.

## TB-28

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Village "last bought" reset keyed on the unit slot, not the cargo):

**Blocks removal:** tried 2026-10-22: testing the cargo (`cargo == Muskets ||
cargo == Horses`) makes CTest `golden_idle_independence` and
`golden_idle_independence_reload` fail (saves 7 and 8, tribe last-bought byte:
got 0xff, DOS 0x08); `unit_ai_contact_colony` fails (#803 test). Other goldens
and `golden_ai_joint` passed. Reverted.

## TB-29

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Village colony-cover mask uses the wrong coordinate frame):

Evidence: DOSBox independence 4->5 (the crown
wagon's Ore / Furs bids 12 / 4).

**Blocks removal:** tried 2026-10-22: testing the cell's absolute position and
passing `(rel_x, rel_y)` to the ring lookup and the own-tile test makes CTest
`golden_idle_continuous` (first at year_1503.sav, byte 0x228: got 0x20, DOS
0x21), `golden_idle_independence` (save 7, byte 0x28a2: got 0xea, DOS 0xf2),
`golden_idle_america` (save 3, byte 0x614: got 0x11, DOS 0x16) and the three
`_reload` variants fail. `golden_ai_joint` passed. Reverted.

## TB-30

Moved from [tolerated_dos_bugs_ai.md](../tolerated_dos_bugs_ai.md) (Brave "stayed" facing 8 fed to the adjacent-direction helper):

**Blocks removal:** tried 2026-10-22: skipping the +3 adjacent-direction term
when `facing >= 8` makes CTest `golden_idle_independence` (save 5, byte 0x2c:
got 0x72, DOS 0x70), `golden_idle_america` (save 38, byte 0xd16: got 0x30, DOS
0x2f), `golden_idle_continuous` (year_1511.sav, byte 0x8d0: got 0x31, DOS 0x2f)
and the three `_reload` variants fail. `golden_ai_joint` passed. Reverted.

## TB-31

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Best-defender picker scores against "unit #nation", not the attacker):

**Blocks removal:** tried 2026-10-22: score against the real attacker unit;
CTest `golden_idle_independence` and `golden_idle_independence_reload` fail,
first differing at save 6 (independence 5->6, byte 0x21b2 got 09, DOS 07; save 7
0x2196 got 09 DOS 07; save 10 0x2c). All other goldens and `golden_ai_joint`
passed. Reverted.

## TB-32

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Colony capture toggles the WAR bit one-sidedly):

DOSBox BPM
(independence 3->4): the crown takes a Dutch colony and its 0x22 toward the
Dutch becomes 0x20.

**Blocks removal:** tried 2026-10-22: dropped the else-clear (captor's WAR bit
stays); CTest `golden_idle_independence` and `golden_idle_independence_reload`
fail from save 3 (byte 0x277b got 0x22, DOS 0x20; every later save differs the
same way). All other tests and `golden_ai_joint` passed. Reverted.

## TB-33

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Spanish tech-1 conquest treasure bound is computed but not used):

**Blocks removal:** tried 2026-10-22: roll with the computed bound; CTest
`golden_idle_continuous` and `golden_idle_reload` fail, first differing in
year_1552.sav (byte 0x2c, got 0x31, DOS 0x30; later saves differ too). All
other tests and `golden_ai_joint` passed. Reverted.

## TB-34

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Ship-sink census does not decrement the ship count):

**Blocks removal:** tried 2026-10-22: decrement `ship_counts[n]` with the other
three census fields; CTest `golden_idle_independence` and
`golden_idle_independence_reload` fail, first differing in save 13
(independence 12->13, byte 0x3496 got 0x07, DOS 0x08). All other tests and
`golden_ai_joint` passed. Reverted.

## TB-35

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (LCR "Vanishes" latch overrides the small-nation protection):

**Blocks removal:** tried 2026-10-22: test the latch only while the case is
still 5; CTest `golden_idle_continuous` and `golden_idle_reload` fail (seed-100
1492 campaign first differs in year_1501.sav, byte 0x2c got 0x2e, DOS 0x2c),
`golden_idle_america` and `golden_idle_america_reload` fail (first differs in
save 7, byte 0xce got 0x00, DOS 0x40, the latch bit DOS has set). Also
`unit_units_core` fails ("case5 latch must set lcr_case5_bonus_used" pins the
DOS behaviour). `golden_ai_joint` passed. Reverted.

## TB-36

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Declaration clears the seen plane with the crown index instead of its bit):

**Blocks removal:** tried 2026-10-22: `keep = ~(0x10 << crown)`;
`golden_idle_independence` and `golden_idle_independence_reload` fail from
save 2 (`independence-seed-100/000002.SAV`, first byte mismatch at 0x6558: port
0x60, DOS 0x70, the crown's 0x10 seen bit cleared on a tile the rebels never
saw). All other goldens and `golden_ai_joint` pass; no other test fails.
Reverted.

## TB-37

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (REF wave planner tests the owner of a stale unit index):

**Blocks removal:** tried 2026-10-22: read the neighbour tile's own head unit
and test its nation; `golden_idle_independence` and
`golden_idle_independence_reload` fail from save 3
(`independence-seed-100/000003.SAV`, first byte mismatch at 0x2c: port 0x68,
DOS 0x6b; the REF lands fewer units and every later save drifts). All other
goldens and `golden_ai_joint` pass; no other test fails. Reverted.

## TB-38

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (REF landing sweep tests "crown-held" through a stale owner nibble):

**Blocks removal:** tried 2026-10-22: test for an active crown unit on the tile
instead of the nibble; `golden_idle_independence` and
`golden_idle_independence_reload` fail from save 12
(`independence-seed-100/000012.SAV`, first byte mismatch at 0x2820: port 0x73,
DOS 0x74), with the saves after it diverging. All other goldens and
`golden_ai_joint` pass; no other test fails. Reverted.

## TB-39

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Invasion and intervention target lists are capped at ten colonies):

**Blocks removal:** tried 2026-10-22: both caps lifted (arrays of 256, all
coastal colonies scored); `golden_idle_independence` and
`golden_idle_independence_reload` fail from save 6
(`independence-seed-100/000006.SAV`, first byte mismatch at 0x6a: port 0x28,
DOS 0x29). The cap only matters past ten coastal human colonies, so that
campaign reaches it and DOS keeps it. All other goldens and `golden_ai_joint`
pass; no other test fails. Reverted.

## TB-40

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Tax-audience score reads the human's SoL for every nation):

**Blocks removal:** tried 2026-10-22: `nat->rebel_sentiment` in place of
`rebel_sentiment_report`; `golden_idle_continuous` and `golden_idle_reload`
fail from `1492-1600-seed-100/year_1529.sav` (first byte mismatch at 0x37:
port 0x02, DOS 0x00), `golden_idle_america` and `golden_idle_america_reload`
from `golden_idle_america_seed-100/000026.SAV` (0x37: port 0x01, DOS 0x02).
Other goldens and `golden_ai_joint` pass; unit test `unit_ai_king_core` also
fails (6 cases that pin the old score). Reverted.

## TB-41

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Fisherman yield cascade tests its thresholds in the wrong order):

**Blocks removal:** tried 2026-10-22: the fixed cascade; CTest
`golden_idle_independence` and `golden_idle_independence_reload` fail (seed-100
independence campaign, first differs at save 2, save byte 0x2ea: got 0x2b,
DOS 0x29), `golden_colony_prod01` fails (COLONY00->01 Dutch: New Amsterdam
Food 83 vs 81, Fort Nassau Food 170 vs 169, Bahia Food 149 vs 148) and
`golden_colony_prod02` fails (COLONY00->01 Dutch: New Amsterdam Horses 69 vs
68, New Holland Food 11 vs 10, Vlissingen Food 12 vs 10). `unit_colony_yield`,
`unit_turn_core` and `unit_ai_euro_28c8_job_score` also fail. `golden_ai_joint`
passed. Reverted.

## TB-42

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Major-river bonus only when the river is the sole contributor):

**Blocks removal:** tried 2026-10-22: major-river step added whenever the
river is major; CTest `golden_idle_continuous` and `golden_idle_reload` fail
(seed-100 1492 campaign, first differs at year_1547.sav byte 0x270: got 00, DOS 01), `golden_idle_independence`
and `_reload` fail (save 2, byte 0x622: got 6b, DOS 6a), `golden_idle_america`
and `_reload` fail (save 32, byte 0x7a6: got 38, DOS 37), `golden_colony_prod01`
fails (Fort Orange Food 163 vs 162) and `golden_colony_prod03` fails (farming
case1, expert Farmer on a major river: New Amsterdam Food 60 vs 59).
`unit_colony_yield` also fails (`fur_trapper_river_double_count`).
`golden_ai_joint` passed. Reverted.

## TB-43

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Stale claim table charges again for a plot a Pioneer already bought):

Seen in real DOS: America seed 100 save 12, France's
Pioneer buys (44,56), then the colony seats it: `lands_bought` 1 and tribe
alarm 7.

**Blocks removal:** tried 2026-10-22: skip the reaction when the snapshot
entry is used and the plot's purchased bit (layer2 `MAP_LAYER2_PURCHASED`) is
set; CTest `golden_idle_america` and `golden_idle_america_reload` fail (first
differs at save 12, byte 0x1960: got 00, DOS 07, the alarm 7 of the France
pair). No other test failed, `golden_ai_joint` passed. Reverted.

## TB-44

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (The destination flood cache is keyed on the goal only):

**Blocks removal:** tried 2026-10-22: store the mover's rules (domain,
low-move, uniform flag, native rumour rule, owner) with the grid and rebuild
when they differ (the RTST path chunk grows by 4 bytes). CTest
`golden_idle_america` and `golden_idle_america_reload` fail; the 63-save America
campaign first differs at save 51 (AI unit 58 ends at x=42, DOS x=44, orders 12
vs 0; the map path/mask planes follow). `regression_replay` also fails (not a
golden). A variant that never reuses the grid also fails
`golden_idle_continuous` (first at year 1502), `golden_idle_independence` (save
4), `golden_idle_america` (save 51), the three `_reload` variants and
`unit_units_movement`. `golden_ai_joint` passed. Reverted.

## TB-46

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Map generator latitude abs() draws the random number twice):

**Blocks removal:** tried 2026-10-22, two variants. (1) One draw per tile,
`abs()` of it: CTest `golden_mapgen_seed100` fails (first terrain mismatch at
(5,1): port 0x18, DOS 0x19; the shorter RNG stream shifts everything after the
latitude pass). (2) Both draws kept so the stream is unchanged, first draw used
for sign and value: `golden_mapgen_seed100` fails too (first mismatch at
(3,1): port 0x18, DOS 0x19, so later passes that read the base terrain drift)
and `smoke_play` fails (no colony founded within its budget on the new map).
Variant (1) fails only `golden_mapgen_seed100`. `golden_ai_joint` passed in
both. Reverted.

## TB-47

Moved from [tolerated_dos_bugs.md](../tolerated_dos_bugs.md) (Spawns refused at the unit cap still charge the cost):

**Blocks removal:** tried 2026-10-22: check `units_spawn_room` before
charging anything at all five sites (human and AI, the AI immigration tick in
`europe_harbor.c` included). Every golden and `golden_ai_joint` passed: no
enabled golden reaches the cap (instrumented, `units_spawn_room` never
refused). Kept at the user's call because the real DOS saves past the AI cap
show these charges (America seed-100 resync 290, Montreal at 292 units;
#1132: Boston muskets 50 -> 0, Barbados horses down to 212), as did the
removed america2 saves past 12. Was DB-17..DB-21. Reverted.
