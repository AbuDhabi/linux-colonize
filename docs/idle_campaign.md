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

Archived to [archive/idle_campaign_fixed_2026-10-10.md](archive/idle_campaign_fixed_2026-10-10.md)
(grep it by `FUN_` name or transition; do not read whole).

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
- 20e6 `local_ea` after an Indian encounter (fresh 1565). A Spanish Dragoon
  border-parks ('F') on its second 20e6 because `local_ea` (SS:E7C4) reads 49.
  A DOSBox CPU log (`LOG` from the first 20e6) shows the writer:
  `FUN_5bfb_022e` at 5bfb:042c, `mov [bp-2c],ax` = the mover's x, at the start
  of the already-met arm (0a38 & 0x20). 022e runs from 3180 on the mover's
  step (frame e7f0 under 3180 at e84a), at the depth where the next 20e6's
  locals sit. Here it then exits on unit +4 bit 0x08. The earlier BPM pointed
  at 0352: memory breakpoints report late. The drain now stores the mover's x
  in `ai_euro_wander_stack_word` when `ai_contact_met_tribe_adjacent` holds.
  The 04da store (Brave x after the swap) is not modelled; 20e6 tests only
  zero versus nonzero. Fresh 1565: 14 leaves to 0; archived sweep 1,410 to
  1,406.
  - Test: `test_encounter_scan_ignores_stale_owner_stamp` (met-adjacent checks).
- 06ae landing pick: DOS skips only settlement tiles (06d2 is the settlement
  owner on a layer2 & 2 tile, raw 87267-87273). The port also ran
  `colonies_can_found` there (no colony within one tile). The sole caller is
  the LAB_3558 unload, so an AI ship next to a foreign colony kept its
  passenger. Fresh 1591 two-turn: the French Merchantman lands a Dragoon at
  (19,9) beside Dutch Curacao, 20e6 then exhausts the hull (`local_78`, raw
  89591-89608). Test: `case_06ae_lands_beside_colony`.

- 5952 `local_1b0` (fresh 1566 two-turn). The job election's rank tests
  (raw 94821; statesman asm 5952:2047-206d) read `byte[local_1b0 + 0x917c]`.
  `local_1b0` starts as the colony's nation (raw 93981), but the improvement
  arm's 8-neighbour scan stores each 06d2 result in it (raw 94482). After a
  scan it holds the last neighbour's owner, so the index runs past the table:
  -1 reads DS:0x917b (0, the high byte of the startup word DS:0x917a), and
  4..11 read DS:0x9180 `land_combat_totals` / DS:0x9184 `tribe_data_9184`.
  Isabella's scan ended on an unowned tile, so the statesman weight doubled
  (21, not 5) and tools lost their doubling; two colonists became Statesmen.
  `ai_euro_5952_improve_best_plot` now hands the value out and
  `ai_euro_5952_rank_byte` reads it. Test: `rank_byte_unchecked_index`.
- `turn_rank_euro_nations` (FUN_5bfb_00f8) reads the FUN_4962_0018 census
  (`colony_counts`, `census_pop_proxy`) instead of recounting live pools,
  as raw 96513-96515 does. Neutral on every target.

- `units_clear_slot` now drops the record's raw DOS bytes. A port slot is
  reused by later spawns; when the DOS tail it spawns over has no raw copy,
  the new unit kept the previous occupant's bytes. Fresh 1571 two-turn: a
  phantom Pioneer's +0x15 = 0xec leaked into Isabella's re-equipped Soldier.
  Test: `case_despawn_drops_raw_record`.

- 5952 `local_7c` (tories) is computed at raw 93994, before the prelude
  absorbs units into the colony (raw 94097+). The port computed it at the
  job election, after a Soldier had joined, so Isabella's statesman weight
  read the new population (fresh 1572 three-turn: 5 tories, not 4, so the
  Statesman beat the Blacksmith instead of tying it).

- FUN_465b_0000 raw 75577-75578: when the destination is foreign (bVar4,
  which an undefended foreign colony sets through its settlement owner), an
  AI or native mover with under 3 thirds left exhausts (LAB_465b_01ce,
  FUN_281f_0934) and stops. The port rolled the partial-MP gamble (with a
  reseed) and could walk into the colony; its combat-path copy of the gate
  also left the MP unspent. Fresh 1592 three-turn: the draw streams now
  match 50 draws instead of 19. Test: `case_465b_low_mp_foreign_colony_exhausts`.

## Two-turn from-load targets (2026-10-13)

A fresh target covers one turn after a load. Two turns in one DOSBox session
also exercise the state DOS keeps in memory across the turn boundary. Load
year Y, keep playing, and take the second autosave; compare it with
`GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=out.sav golden_idle_campaign Y Y+2`.
Copy the autosave as soon as its mtime changes (DOS stamps it on close); a
copy taken later can already hold the next save.
- 1497, 1501, 1504, 1513, 1533, 1541, 1545, 1551, 1555, 1561, 1571, 1575,
  1581, 1585, 1587, 1591 and 1595 are byte exact. 1566 differs only in Brave
  27, which differs between two DOS runs too.
- Three-turn targets (`mkfresh3`): 1511, 1534, 1542, 1572 and 1582 are byte
  exact for all three turns. 1526, 1552 and 1562 diverge first on native
  Braves (DOS-unrepeatable). 1592's third turn is not repeatable in DOS
  either: two runs disagree on the draw stream right after the French
  attacks near (24,6).
- DOS is not fully repeatable here: two identical 1591 runs differed in one
  colony's `garrison_quota`. Treat a single-field mismatch as noise until a
  second run reproduces it.
  - `plot_was_worked_mask` is now reset at the start of each tick, because
    DOS rebuilds the 26e4 claim table every tick. This was neutral on every
    measure.
- Unported: the human-side join (`FUN_364b_1aec`) also stamps and runs 3930
  first. The port's colony-screen joins do neither.

## Continuous run from year_1492 (2026-10-13)

The archive was made in one sitting, so the strongest check is one port run
from `year_1492.sav` through every later year, carrying the port's own
session state the way DOS carries its unsaved memory:

```
GOLDEN_IDLE_KEEP_GOING=1 GOLDEN_IDLE_DUMP=/tmp/x/sim_%d.sav golden_idle_campaign 1492 1599
```

`%d` in `GOLDEN_IDLE_DUMP` takes the year; years missing from the archive
(DOS made no autosave there) are played and dumped but not compared. Then
JSON-diff each `sim_Y` against `year_Y`. Four leaves are always the human's
first-turn UI (`map_mode`, `tut2.nr1`, `stuff.x/y`).

Status: every year through 1589 matches except small residues (below:
1505/1506);
1591 is the first real split.

Fixed by this run:
- AI moves roll Lost City Rumours in the move (FUN_465b_0000 raw
  75763/75788-75791: `075e` = FUN_137f_0598 read before the step, then
  `2a1f_0178` -> FUN_65dd_0004 for a mover with nation < 4). The port only
  did this on ship unloads. 1499 -> 1501: a French Soldier walked onto
  (45,55) and found burial mounds (`lcr_case5_bonus_used`, tribe alarm 97).
  Test: `lcr_ai_step_resolves`.
- Every spawned unit has a raw +0x0c..+0x15 image (zero, or the compaction
  tail), and goods unloads shift it the FUN_15eb_317c way. 1506: a Spanish
  Merchantman kept Cloth 100 in hold 1 after selling both holds and loading
  78 Sugar. Test: `case_unload_keeps_stale_hold_bytes`.
- Colony phantom defender: DOS deletes the phantom (00d0, raw 100637) before
  a losing attacker, so the vacated slots keep phantom, then the last real
  record. The port deleted the attacker first. DOSBox 1499 -> 1504 (from
  load): the Brave born in slot 44 carries +0x15 = 0, not 196. Port
  from-load 1499 now equals DOS from-load for four turns.
- Village convert-join (FUN_5fef_1b0e raw 101144-101184) runs after every
  European win over a village's phantom defender, razed or not, and the
  Convert appears on the attacker's tile (uVar21/22, raw 100566). The port
  rolled it only when razing, on the village tile. DOSBox 1519 -> 1521 (a
  06b4 breakpoint, then a CPU log): a French Dragoon at (45,53) beat village
  (45,54), population 7 -> 6, Convert at (45,53). Test:
  `unit_village_win_convert_on_attacker_tile`.
- Colony delete (raw 58135-58200) compacts the colony array and unbinds the
  units bound to the dead colony (+0x314a = 0xff); later colonies shift one
  index down. The port kept a hole, so Guadeloupe (1542) took Fort Orange's
  old slot ahead of Fort Nassau, and the units bound to Fort Nassau saved
  origin 5 instead of 4. Now: new colonies go after the last live slot,
  `colonies_abandon` unbinds (`colonies_set_units_context`), the save writer
  maps `col1_origin` (a colony id) to the DOS index, and the surplus recall
  looks the colony up by id. Test: `colony_delete_appends_next`.
- Europe departure boarding restamps the boarders' tile order in boarding
  order, so the arrival relink lands hull, then passengers in that order
  (1558: Dutch hull, Dragoon, Soldier at (50,4)).
- AI Scout village visit (FUN_521d_20e6 raw 89064-89068): DOS writes plan
  'L' and commits a step onto the village; 465b -> 4528 speaks with the
  chief and the 0bd1 tail clears the act state and bumps +0x315a. The port
  resolved the visit in place. DOSBox 1531 -> 1532: Spanish Scout at
  (47,48). Test: `unit_scout_visits_village_at_war` (Scout now DOS type 5).
- Colonist removal leaves the education nibble: FUN_15eb_1068 zeroes the
  leaving colonist's +0x60 counter (raw 11256-11258), then FUN_15eb_0d04
  shifts the rest down and leaves the last slot's value past population.
  The pool keeps it in `col1_tail_turns`, like the occupation/profession
  tails. 1529 Isabella: slot 2 = 1.
- AI immigrants go to the bottom of the dock chain: FUN_38fd_5e52 raw 68588
  calls FUN_281f_08c6 = FUN_1427_03a0 after the 0b26 spawn, which relinks
  the new unit at the oldest end (+0x315e = -1). FUN_38fd_4884 (Brewster,
  free passage) does not. 1571-1576: the Dutch Missionary 50 sits under
  Artillery 48. Port: `units_tile_stack_sink`. Test:
  `tile_stack_sink_oldest_end`.
- FUN_15eb_28c8's `local_14` (raw 12960-12966, subtracted from the Indian
  land penalty) counts units on the colony tile: asm 0000:87c4-8804 passes
  the colony x/y in AX/DX to FUN_1427_005c and walks the chain with 004a.
  The port counted the whole nation. Neutral in the sweep and the
  continuous run.
- FUN_15eb_039e(b) walks `b` and then the `-0x707a` next-row (upgrade)
  byte, so the 28c8 Ore bonus `039e(0x28) + 2*039e(3)` counts Blacksmith's
  Shop + Iron Works and Armory + Magazine + Arsenal. The port counted the
  House + Shop and the Armory alone. 1578 Isabella (House only): Ore lost
  its +1, so the Convert in slot 1 takes Fur (yield 5) in the general pass
  and the forced-lumberjack arm picks slot 4, as DOS does. 1578 and 1579
  now match.
- AI hulls landing on the Europe dock lane put their passengers ashore:
  FUN_48d3_03d0 moves every lane unit on its own (0880 + 03a0), so at the
  236+n hop the hull and then each passenger, nearest first, sink to the
  bottom of the dock chain, keeping orders and goto. FUN_1427_10be then
  re-boards dock units only while hold room lasts. The port kept them
  aboard through the Europe stop. 1580: the Dutch Merchantman 5 bought 4
  holds of goods, so the Soldier it brought stays on the dock (and the
  Dutch crosses count it). `units_lane_dock_arrival`. 1582-1586 now match.
- 0a60 FOUND/MIL bits on an empty hull: only the dock tile is excluded; on
  a sailing lane the riders of another hull count (1581: Merchantman 5
  takes 0x0c from the Dragoons riding Merchantman 39 on 231).
- FUN_4962_0018 census runs per nation in that nation's own EOT (and for
  the human in FUN_3844_0442). The port also refreshed all four nations at
  round start, so mid-round the Dutch rows were fresher than DOS's. 1590:
  France meets the Dutch; with DOS's older Dutch rows 10ec finds the pair
  not war-worthy and 13b0 signs a treaty
  (0x60, timers 1/12). FUN_5bfb_10ec also reads the frozen census tables
  (-0x6b1a/-0x6b5a/-0x6a8e) rather than live recounts.
- AI Pioneers bought in Europe keep a stale tools byte (bugs.md #636, the
  old deliberate substitution is reverted): FUN_521d_5d04 raw 92770-92783
  writes only the type, FUN_2b5a_0b08 lets any type-2 unit work, and
  FUN_479b_0158's byte `-= 0x14` wraps 0 to 236. `units_is_pioneer` is now
  type-only. 1587: the Spanish Pioneer saves tools 0. Test: `usedup #636`.
- AI unload (`ai_euro_20e6_unload_by_mask`) walks passengers in tile-chain
  order (latest arrival first), not boarding order. 1590: Dutch hull 39
  chained 39 -> 46 -> 44 lands Dragoon 44.

Open:
- 1591: Dutch hull 39 sails to (28,6) instead of (24,8) after landing its
  Dragoon; Braves 31 and 34 differ. Reproduced from load (DOSBox 1589 ->
  1591 twice: only Brave 22 varies between DOS runs). A DOS RNG-draw trace
  (scratch `dt_rng.py`, BP 1541:0e04) matches the port draw for draw
  through the hull's whole 1590 wander, except 152e/reseed draws the BP
  misses; DOS then draws 6 more in the Dutch slot. So the 4d2e direction
  scores differ on non-random terms (facing, far-probe fog/unseen, colony
  terms) with identical random draws. Next: BP at the 20e6 commit (OVL14
  58a9, [bp-0x74] dir / [bp-0xe0] best) for hull 39 in 1590.
- 1505/1506: ship 4's facing, Brave 26's step and unit 5's goto differ.
  Port from-load 1504 equals DOS from-load, and port continuous differs from
  the archive the same way, so DOS carries something unsaved here that the
  port does not model.
  A DOS replay from `year_1492` (scratch `mkfreshn.sh 1492 12`, about 1.3
  minutes a turn) equals the archive through 1496 and differs from 1497
  (unit 34 stays at (49,..) with full moves), where the port continuous run
  matches the archive. So a DOS replay is not an oracle for the archive's
  later years either.

## Open leads (most transitions first)

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
