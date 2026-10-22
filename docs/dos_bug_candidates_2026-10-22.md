# DOS bug candidates, sweep 2026-10-22

STATUS: open review list for the user. Not authoritative, not a spec. Nothing
here is filed in [dos_bugs.md](dos_bugs.md) or
[tolerated_dos_bugs.md](tolerated_dos_bugs.md) yet; an agent must not do that
alone. Archive this file once every item is decided.

Source: nine read-only reviewers, one per source slice (Euro AI x2, native
contact, diplomacy/Indian/Brave, King/turn/FF, colony, combat,
units/map/Europe, UI/glue plus docs and archive rows). Each item was checked
against the decomp by its reviewer; items marked **(spot-checked)** were
re-read by the coordinator as well. Duplicates across slices are merged.
Already filed (DB-1..9, TB-1..8) and known_divergences entries are excluded,
except where section F says an entry looks misfiled.

`raw NNNNN` = line in `original_sources_decompiled/viceroy_unpacked.c`.
Confidence is that the DOS code is a programmer error, not design.

Sections:

- A. Port reproduces it; no golden impact expected (dos_bugs candidates)
- B. Port reproduces it; AI / RNG / saved bytes, so idle goldens depend on it (tolerated candidates)
- C. Port already does not reproduce it, undocumented (needs a dos_bugs entry, or a literal port)
- D. Unsure: possibly design
- E. Port-vs-DOS leads found in passing (DOS is not buggy; port_bugs candidates)
- F. Housekeeping in the existing docs

## Outcome of sections A and B (2026-10-22)

Each fix was simulated alone (user rule: goldens pass -> dos_bugs.md, any
golden fails -> tolerated_dos_bugs.md). Entries carry the full evidence.

- Not reproduced any more, [dos_bugs.md](dos_bugs.md): C-1 DB-12, C-2 DB-13,
  C-3 DB-14, C-4 DB-15, C-11 DB-22, C-14 walk-origin half DB-23, C-15
  founding half DB-24, C-22 DB-25, C-24 DB-26, C-26 DB-27, C-27 DB-28, C-31
  DB-29, C-33 DB-30, C-34 DB-31, C-39 DB-32, C-46 DB-33, C-49 DB-34, C-53 DB-36 + DB-37, C-55 DB-38, C-57 DB-39, C-58 DB-40.
- Tolerated, [tolerated_dos_bugs.md](tolerated_dos_bugs.md) and
  [tolerated_dos_bugs_ai.md](tolerated_dos_bugs_ai.md) (TB-9..30): C-8b TB-9,
  C-9 TB-10, C-10 TB-11, C-12 TB-12, C-13 TB-13, C-14 road-connect payer TB-14,
  C-14 AND test TB-15, C-15 tick TB-16, C-15 local_78 TB-17, C-16 TB-18, C-17
  TB-19, C-18 TB-20, C-19 TB-21 + TB-22, C-20 TB-23, C-21 TB-24, C-23 TB-25,
  C-25 TB-26, C-28 TB-27, C-29 TB-28, C-30 TB-29, C-32 TB-30, C-35 TB-31, C-36
  TB-32, C-38 TB-33, C-40 ship count TB-34, C-41 TB-35, C-42 TB-36, C-43 TB-37
  + TB-38, C-44 TB-39, C-45 TB-40, C-47 TB-41, C-48 TB-42, C-50 TB-43, C-54
  TB-44, C-59 TB-46, C-6 TB-47 (all five unit-cap sites; fix passed the goldens
  but the user kept DOS for the late-game AI saves; DB-17..DB-21 withdrawn).
- No fix without inventing a rule (left as they are): C-8a, C-8c, C-8d, C-37,
  C-40 capacity term (20e6 raw 88557-88566 has the same shape: a heuristic),
  C-51, C-60 (byte save fields).
- Judged intended by the user, entry withdrawn: C-5 (DB-16, @KINGMERCY cut
  content), C-52 (DB-35, follow DOS), C-56 (TB-45, Man-O-War radius 1).
- Section C, port already differs, recorded as not reproduced: C-61 DB-41,
  C-62 DB-42.
- C-7 refuted: DOS closes the bracket; the port's missing ")" was a port bug,
  fixed as port_bugs.md #1147.

---

## A. Port reproduces; no golden impact expected (dos_bugs candidates)

### C-1. Europe Artillery price counter rises even when the purchase is cancelled (spot-checked)
- DOS: FUN_38fd_4b50 raw 64870-64875 does `price += nation[0x1e]*100; nation[0x1e]++` before @REALLYBUY; payment happens only on answer 1.
- Why bug: the counter belongs inside the "yes" arm. A No/Esc still makes the next Artillery 100 dearer. Confidence: high.
- Effect: human only; saved byte nation+0x1e drifts up on every cancelled buy.
- Port: `europe_dock.c:344-353` (`europe_purchase_open_confirm`); pinned by `test_europe.c:774-788`.

### C-2. Foreign-colony barter (Jan de Witt) never debits the colony's goods (spot-checked)
- DOS: FUN_5f7a_020e raw 99024-99038. On "take the goods" the unit's hold is overwritten with the offered cargo (`0cea`/`0ca4`) and the colony gets `stock[sold] += qty`; the offered cargo is never subtracted from the colony.
- Why bug: the offer is sized from the colony's own stock, which it then keeps. Every barter creates goods. Confidence: high.
- Effect: human only (the AI never barters this way); also inflates the AI colony's stock afterwards.
- Port: `colony_goods.c:743-760` (`colonies_foreign_trade_apply`), comment "transcribed, not fixed"; pinned by `test_colonies.c:2433`.

### C-3. Tea Party picks the cargo without using its random draw
- DOS: FUN_38fd_3dc8 raw 64176-64200 (asm 38fd:3f63-3fbc). `rand(1,total)` is stored and never read; the walk subtracts weights from the original total, so the pick is always the last stocked, non-boycotted cargo with nonzero weight.
- Why bug: intended `r = rand(); r -= w[c]` roulette. Confidence: high.
- Effect: human tea party only; the seized/boycotted good is predictable (highest-index traded cargo).
- Port: `ai_king.c:93-150` (`ai_king_pick_dump_goods_cargo`); pinned by `test_ai_king_core.c` `case_dump_goods_pick_api`. See also E-8 (port weights and RNG draws differ from DOS here).

### C-4. @MORETHANTHREE cap skips the Rum Distiller
- DOS: thunk_FUN_1000_9808 (viceroy_overlays.c:60501): `if (2 < count[job] && 9 < job) return @MORETHANTHREE`. Indoor jobs start at 9 (Distiller).
- Why bug: looks like `8 <` / `>= 9` was meant; every other indoor building is capped at 3. Confidence: medium.
- Effect: human can seat 4+ Distillers through the colony screen (AI placement has its own `< 3` check).
- Port: `colony_workers.c:319-321`, "oddity included"; pinned by `test_colonies.c:2862` (#590).

### C-5. King-gift Frigate loss tax cut reads the next table row (spot-checked)
- DOS: FUN_5fef_0352 raw 99671-99690. The search stores the found row in `local_10`, but the cut is read at the post-loop counter `local_30` (= found + 1); row 0 is excluded by `0 < local_10`.
- Why bug: wrong variable after the loop. Only the King's gift Frigate carries flag 0x40, and its "next row" byte is 0, so the tax cut never happens. Confidence: high.
- Effect: dead mechanic; human only. A fix makes the cut live (a behaviour change, not a no-op).
- Port: `units_combat.c:3988-4030` (`units_royal_loss_tax_cut`), "off-by-one included"; pinned by `test_units_combat.c` ~1740 (#659).

### C-6. Refusals at the unit cap still charge the cost
One family; the spawn result is ignored at several sites:
- Colony construction, FUN_364b_0114 raw ~56940-56947: hammers zeroed, type count and Artillery grace bumped, tools charged, no unit. Port `colony_build.c:806-812` (`COLONIZE_UNIT_BUILD_REFUSED`).
- Leave-as (eject), FUN_15eb_1068 raw 11292-11300/11333: colonist stays, but the target job's gear (muskets/horses/tools) is debited. Port `colony_workers.c:1241-1250` (#1132); pinned by `test_colonies.c:1373-1394`.
- Birth, FUN_364b_0688 raw 57615-57622: 200 food debited and @NEWCOLONIST shown, no unit. Port `turn_production.c:1428-1452`.
- Immigrant, FUN_38fd_5e52 raw 68566-68590, and Brewster recruit, FUN_38fd_4884 raw 64765-64770: crosses zeroed before the spawn is attempted. Port `turn_colony.c` ~600-640, `units.c:1594-1600`.
- Confidence: medium (an "unreachable edge" in design terms, but America seed-100 hit it at 292 units, resync 290).
- Bucket split: the human-only sites (leave-as, immigrant, Brewster) are dos_bugs candidates; construction and birth also hit AI colonies late game, so those are tolerated unless the America resync gate is retired.

### C-7. Colopedia Privateer stat line has an unclosed "("
- DOS: FUN_6cb2_07e6 raw 110572-110590 opens "(" for the type-11 stats row and never appends the closing builders.
- Confidence: medium (rests on the port's ndisasm reading; Ghidra drops the args).
- Effect: display only. Port `pedia.c:1441-1451`.

### C-8. No-effect dead code (record only, if at all)
- @KINGNEWWAR strength compare, FUN_38fd_5930 raw 68352-68362: sums are taken only over peers whose count must then be 0, so the comparison is always `0 <= 0`. Port `ai_king_war.c:70-84`.
- Explore-scan own-tile `+0x10` tests absolute tile (0,0), raw 89123-89125; port omits it, no effect.
- 5d04 tail reads `unit[-1].flags & 0x20` (raw 92589, DS:0x312c static byte); port uses constant 0 (`ai_euro_europe.c:1735`).
- Colony name list exhausted: FUN_479b_0000 returns row 0 forever (every later colony gets the same name). Port `colony.c` ~608-640. Confidence low; rare.

---

## B. Port reproduces; AI / RNG / saved bytes (tolerated candidates)

Fixing any of these is expected to break at least one idle CTest gate; none was test-run.

### Indian nation id (4..11) used where a tribe index (0..7) is expected
A recurring slip. C-63, C-66 and C-67 in section C belong to the same family.

#### C-9. 28c8 Indian-claim term indexes tribe tables with the nation id
- DOS: FUN_15eb_28c8 raw 13096-13111. The 26e4 claim byte (village nation 4..11) indexes `15dc_00e0`, `DS:0x91cc[n*16+cont]` and `DS:0x9184[n]`; ids 8..11 run past the tables into colony records.
- Evidence: DOSBox America seed-100 56->57 (claim 11 reads colony 1's +0x6c). Confidence: high.
- Effect: AI job/plot placement near tribe land. Port `ai_euro_colony_jobs.c:517-551` (OOB emulation 272-285); same slip in the 5952 improve arm (C-10).

#### C-10. 5952 tile-improvement arm reads tribe alarm with the nation byte
- DOS: FUN_5952_035e raw 94446-94449 (overlays asm 144230-144236) pushes the nation byte to 84fc without `-4`; the same function's other 84fc call subtracts 4. Also `local_132 -= (4 - alarm)` makes angrier tribes raise the plot score (sign slip, low confidence).
- Effect: which plot AI pioneers improve near villages. Port `ai_euro_land.c:322-325`.

#### C-11. Razing a capital clears alarm in the wrong tribe's dwellings
- DOS: FUN_5fef_1b0e raw 101293-101298 compares settlement `+2` (nation 4..11) with `DS:0x8d52` (tribe 0..7). FUN_4cc6_00f2 raw 80877 does it right (`+ 4`).
- Effect: the razed tribe keeps its alarm; tribe idx 4..7 clears tribe idx-4's dwellings. Evidence: America resync 236. Confidence: high.
- Port: `ai_diplo.c` ~258-275 (`ai_diplo_indian_capital_surrender`).

#### C-12. Refused food beg bumps the wrong alarm word (spot-checked)
- DOS: FUN_5bfb_022e raw 96819: refuse arm calls `0d6c(0x8d52, param_2, ...)` with the Indian nation; the accept arm (raw 96796) passes `param_1` (the Euro). The word lands past the tribe's 4-word alarm array, inside the next tribe's record (clamped 0..100).
- Effect: the tribe-level alarm toward the refuser never rises (the village's own alarm still grows by half); another tribe's record bytes get edited. Every refusal, human and AI (AI Spain always refuses). Evidence: DOSBox independence 4->5. Confidence: high.
- Port: `ai_contact_demand.c:1639-1644` (`ai_contact_apply_beg_food`).

### Euro AI colony tick (FUN_5952_035e / FUN_15eb_28c8)

#### C-13. Colony-nation local `local_1b0` overwritten by a neighbour loop
- DOS: raw 94481-94485: the improve arm's 8-neighbour veto loop stores each plot owner into `local_1b0` (the colony nation) and never restores it. Later reads expecting the colony nation: indoor rank weight (raw 94821), stack-owner compare (94931), attitude index (94967), build cascade Musket price / armed counts / Wagon alarm (95759-95844).
- Effect: AI build and job choices use another nation's or a tribe's stats whenever the arm ran. Frequent. Confidence: high.
- Port: `ai_euro_land.c:378` (`out_improvement_owner`) consumed at `ai_euro_colony_jobs.c:1332, 2270, 2506, 2610-2616, 2670, 3221-3226`; pinned by `unit_ai_euro_5952_build` `case_blacksmith_gate_reads_local_1b0`.

#### C-14. Road connect leaves the "current colony" global on another colony
- DOS: raw 94389-94400. FUN_479b_0526 rebinds `DS:0x8542` to the colony nearest the road gap; the rest of the tick (improve arm, idle sweep, job passes, indoor pass, build cascade, 6d8e prelude) runs on that colony. One colony is processed twice, another skipped. Every `turn % 7 == 0` with a road built.
- Same helper, FUN_5952_0000 raw 93610-93636: a colony is skipped only if far on both axes (AND, not OR), and the walk origin is never reset between candidates.
- Evidence: tr9 11->12, America resync 129, America2 resync 37. Confidence: high.
- Port: `ai_euro_colony_jobs.c:3215-3220, 3587-3610`; `ai_euro_land.c` ~564-760.

#### C-15. 28c8 forest -10 test reads an uninitialised terrain local; 5952 clamp reads stale `local_78`
- DOS: FUN_15eb_28c8 raw 13119-13122 reads `local_32`, written only in the food-emergency arm (raw 13037). DOS sees stack residue (0x23ec after a ledger refresh, 0x12 after a set-job, the colony x at founding).
- DOS: FUN_5952_035e ~raw 94806 min() clamp reads `local_78`, written only for jobs with an input cargo (Preacher/Statesman get the previous job's value; first read is uninitialised).
- Effect: AI plot/job scores every tick; also the founder's first plot and human auto-assign (where the port substitutes a "candidate terrain" value and can differ). Confidence: high.
- Port: residue tracker `ai_euro_colony_jobs.c:243-268, 553-565`, `colony.c:1465`; `local_78` seed 0x7fff at `:1295-1310`. DB-4/DB-5 class.

#### C-16. Schoolhouse arm elects the table row index as the profession
- DOS: raw 95874-95891: `local_8a = local_ac` (row of the DS:0x864 table) where the same loop reads the @JOB from `[local_ac*4+0x865]`. Gunsmith row elects Farmer, Blacksmith row Sugar Planter, etc. Confidence: high (port comment already calls it a DOS bug).
- Port: `ai_euro_colony_jobs.c:2980-2987`. Saved profession bytes.

#### C-17. ARM 2 training charge indexes the price table by colonist slot
- DOS: raw 95949 `*(uint*)(local_30*8 - 0x7158)`, `local_30` = colonist slot; the gate (raw 95919) uses the job-0 price. Rows with price -1 pay +1 gold. Confidence: high.
- Port: `ai_euro_colony_jobs.c:3051-3068`. AI treasury.

#### C-18. Starter-horses arm reads unit[-1] and the tutorial byte
- DOS: raw 95961-95963: the tile head (-1 if no unit) is used unguarded; `unit[-1]+0x3146` = DS:0x312a (terrain row 27 byte, 24), whose cargo capacity read hits `DS:0x5387` = tutorial flag nr5. AI colonies with an empty tile get 2 horses for 10 gold only if the human's tutorial flag is set. Evidence: DOSBox watch (1592 Curacao). Confidence: high.
- Port: `ai_euro_colony_jobs.c:3076-3108`; pinned by `case_starter_horses_purchase`.

#### C-19. Payment gated, benefit not
- 5d04 recruit loop raw 92958-92964: horses are paid only if affordable, but the Dragoon upgrade and horse bookkeeping run anyway (the training arm at raw 92728 keeps both inside the gate). Port `ai_euro_europe.c:1511-1527`.
- 5952 emergency lumber raw 94692-94695: `lumber += 100`, then pay 200 only if gold >= 200. Port `ai_euro_colony_jobs.c:1528-1540`; pinned by `unit_ai_euro_5952_lumber`.
- Confidence: medium.

#### C-20. Equip arm strips the expert profession of the wrong colonist
- DOS: raw 94347-94350: after the scoring loop, the strip test uses the loop's last slot, not the picked colonist `local_34`. Confidence: high.
- Port: `ai_euro_goals.c:1825-1829`. Saved profession bytes.

#### C-21. Pioneer-clear ring scan counts water twice
- DOS: raw 94092-94117: water does `local_144++, local_e++` and falls into the next non-else `if` (+1 unproductive). Confidence: medium-low.
- Port: `ai_euro.c:905-925`.

### Euro AI ships and Europe

#### C-22. Delivery tally is a signed byte
- DOS: FUN_521d_20e6 (overlays asm 136271-136273 `ADD byte [BP+SI-0xc6],AL`, read with CBW). Two 100-lots of one cargo read -56; 256 reads 0. Colony-pick scoring and the Horses gate then misjudge. Evidence: America resyncs 105-119, 151+. Confidence: high.
- Port: `ai_euro_ship.c:131-134` (`int8_t`).

#### C-23. Departing-hull cargo buy gated on a stale global quantity
- DOS: FUN_521d_5d04 raw 93041-93047: affordability is `price * DS:0x8dc4` (last hold removal anywhere) but the buy is a literal 100. Evidence: DOSBox 1591. Confidence: high.
- Port: `ai_euro_europe.c:1648-1655` (#1029).

#### C-24. Unload mask reset inside the neighbour loop
- DOS: FUN_521d_20e6 raw 89463: `local_9c = 0` inside the 8-neighbour loop, consumed once after it, so only the last land neighbour counts. Confidence: medium.
- Port: `ai_euro_ship.c:2199`; pinned by `test_regress_ai_tables.c:275-326` (#530).

### Euro AI land units

#### C-25. Scout/Colonist village arms compare the alarm quartile with 25 (spot-checked)
- DOS: FUN_521d_20e6 raw 89065 and 89368 (asm `CMP [BP-0x52],0x19`); `local_54` is `FUN_281f_0a60(alarm)` = quartile 0..3 (overlays asm 134386-134388). Raw 90189 compares the same variable with 3.
- Effect: the Scout gate is always true and the Colonist rejection never fires, so AI Scouts and Colonists enter villages regardless of tribe anger. Confidence: high.
- Port: `ai_euro_land.c:1448` omits the gate as "vacuously true".

#### C-26. War-worthy check: second clause reads nation a's byte for nation b
- DOS: FUN_5bfb_10ec raw 97166-97173: copy-paste; the word compare switched to `param_2`, the byte read did not. Confidence: high.
- Port: `ai_euro_land.c:1062` ("verbatim"). AI-vs-AI war / treaty cancel.

#### C-27. Explorer countdown stores a signed max(dx*4, dy*4)
- DOS: raw 90126-90138 (dup 85300). NW ring slots store -4, decremented as a signed char (raw 89294), so ~252 steps without site scanning. Confidence: medium-high.
- Port: `ai_euro_land.c:2756`, countdown 3946.

#### C-28. Adjacent-foreign probe overwrites its reference medium inside the loop
- DOS: FUN_1427_09dc raw 7927-7968: a settlement neighbour sets `local_8 = medium`, so later neighbours compare against it; sibling FUN_1427_0b08 (raw 7997-8030) keeps the centre medium fixed. Evidence: tr10 turn 276 (Man-O-War at sea counts as next to a village). Confidence: medium-high.
- Effect: Brave dest scores, Euro 20e6 entry gate (`ai_euro_act.c:845/1100`), `ai_king_war.c:294`.
- Port: `ai_native_021a.c:156`, `ai_euro_land.c:3290`.

### Natives

#### C-29. Village "last bought" reset keyed on the unit slot, not the cargo
- DOS: FUN_4d56_2bbc raw 82704 and FUN_4d56_306c raw 83052: `if ([BP+6]==0xf || [BP+6]==8)` tests param_1 (unit index); 0xf/8 are Muskets/Horses. Intended: muskets and horses stay resellable. Confidence: high.
- Effect: human and AI village trade (@BADCARGO). Port `ai_contact_trade.c:1394, 1435` (pool-slot proxy, #803); pinned by `test_ai_contact_colony.c:748`.

#### C-30. Village colony-cover mask uses the wrong coordinate frame
- DOS: FUN_4d56_2154 raw 81816 (asm 4d56:21b2-2227): colony-relative deltas go to the absolute bounds test 0302, and village-local indices go to 0ce0, which wants colony-ring indices. Most colony-worked land is not excluded from the village's terrain scoring. Confidence: medium-high.
- Effect: village bids/asks, gift/beg triggers, Live Among skill pick. Port `ai_contact_demand.c` ~855-905.

#### C-31. Raid goods pick burns a dummy rand(0,200) for Muskets
- DOS: FUN_5fef_0f14 raw 99826-99829; result unused, one extra LCG draw. Confidence: medium.
- Port: `ai_contact_raid.c:287`.

#### C-32. Brave "stayed" facing 8 fed to the adjacent-direction helper
- DOS: FUN_4d56_021a (OVL13 0xc8f-0xcb9) passes facing 8 to FUN_124c_013c (raw 2833-2843), which then favours NE/NW by +3; the Euro scorer (521d:54f5) skips facing >= 8. Confidence: medium-high.
- Port: `ai_native_021a.c:634-640`.

#### C-33. Brave `grudge` latch not reset on other-tribe tiles
- DOS: FUN_4d56_021a OVL13 0x66f-0x6ba: the owner >= 4 arm skips the store, so the previous direction's value carries over. Confidence: medium.
- Port: `ai_native_021a.c` ~1071.

#### C-34. Tribe threat accumulators are signed bytes
- DOS: FUN_4d56_152e raw 81387-81534 (OVL13 0x1756/0x171d): threat >= 128 wraps negative and cools instead of heating. Needs ~22 Dragoons or ~13 Artillery at a capital. Confidence: medium.
- Port: `ai_indian.c:686/704`.

### Combat

#### C-35. Best-defender picker gets the attacker's nation as a unit id
- DOS: FUN_5fef_1b0e leaves `PUSH [BP-0x86]` (attacker nation) on the stack before the 07e0 call; it becomes the second argument of FUN_5fef_0000 (raw 99111-99205), which reads it as a unit record (`IMUL BX,[BP+8],0x1c`). Defender ranking is scored against "unit #nation". Confidence: high.
- Effect: which unit defends a tile or colony; every game, human and AI.
- Port: `units_combat.c:128-170` (`units_best_defender_at`, `pick_foe_id`); evidence independence 9->10.

#### C-36. Colony capture toggles the WAR bit one-sidedly
- DOS: FUN_5fef_1b0e raw ~100999-101010: `if (!(rel[captor][def] & 2)) rel[def][captor] |= 2; else rel[captor][def] &= ~2;`. Capturing while at war clears the captor's war flag. Evidence: independence 3->4 BPM. Confidence: medium-high.
- Port: `colony_build.c` ~190-200 (`colonies_capture_col1_effects`); `test_colonies.c:899/956` pin only one branch.

#### C-37. Attacker Artillery-in-open penalty keyed to the defender's orders and nation
- DOS: FUN_5fef_1b0e raw 100472-100493 (asm 0x1f50-0x1fa6): the attacker clause tests the defender's fortify orders and Euro-ness (copy of the defender clause). Confidence: medium (could be a deliberate "bombard fortified targets" rule).
- Port: `combat_strength.c` ~755-790 (`def_shielded`).

#### C-38. Spanish tech-1 treasure bonus computed, not used
- DOS: FUN_5fef_31ea raw 101414 vs 101433-101436: Spanish bound stored, roll is literal `rand(0,2)`. Confidence: medium.
- Port: `units_combat.c:2804-2811`.

#### C-39. Convert winning a fight draws the promotion roll for nothing
- DOS: FUN_5fef_172c raw 100085-100111: Convert passes the eligibility gate, rolls, ladder maps 0x1b to 0x1b. RNG stream only. Confidence: high.
- Port: `units_combat.c` ~870-910 (#598).

#### C-40. Ship-sink census bookkeeping and the unarmed-hull capacity test
- DOS: FUN_5fef_0352 raw 99570-99579 decrements cargo/type/armed counts but not the ship count `0x9418`; the unarmed gate (raw 99538-99560) multiplies Frigates by capacity but not Privateers, and omits Man-O-War. Confidence: low-medium.
- Port: `units_combat.c:1540-1590`, ~2160.

#### C-41. LCR "Vanishes" latch overrides the small-nation protection
- DOS: FUN_65dd_0004 raw 103597-103612: the one-shot Burial Mounds latch is tested even after the protections turned case 5 into "nothing". Confidence: low-medium (benign).
- Port: `units_combat.c:3460-3470`.

### King / War of Independence

#### C-42. Declaration clears the seen plane with ~crown instead of ~(0x10 << crown)
- DOS: FUN_43f7_1a26 raw 74818-74827 (asm ~117623 `NOT AX` on `[0x53d2]`). For crown 1..3 the AND clears low-nibble bits (the AI site-score nibble) on every tile and leaves the crown's stale seen bit. Confidence: high.
- Port: `ai_king_audience.c:1104-1124`.

#### C-43. REF wave planner reads a stale unit and a never-cleared owner nibble
- DOS: FUN_43f7_0982 raw 74033-74041 tests the owner of a stale `local_4c` (last colony's head, or -1 = DS:0x312b); raw 74190 tests "crown-held" via the layer3 owner nibble, which 023a never clears. Confidence: high.
- Port: `ai_king_ref.c` ~665-666, ~795-830; evidence independence 3->4.

#### C-44. Invasion and intervention target lists capped at ten colonies
- DOS: FUN_43f7_0982 raw 74005, FUN_43f7_10f0 raw 74316. Only the oldest ten coastal human colonies can be targets. Confidence: low-medium.
- Port: `ai_king_ref.c:45, 413, 759`. Could be a dos_bugs candidate if no idle gate reaches 10 ports.

#### C-45. Tax-audience score for AI nations uses the human's SoL
- DOS: FUN_38fd_5be8 raw 68460-68467 reads `*(int*)0x53d0` (human/crown SoL) for every nation (#1053); the per-nation byte +0x19 exists. Confidence: low.
- Port: `ai_king_audience.c:164-169`.

#### C-46. Royal purchase adds the free-colonist count to the bells word
- DOS: FUN_43f7_1d42 raw 74889-74890 adds to nation+0xe; nothing reads it. Confidence: low. Port `ai_king.c:768-770`.

### Colony production

#### C-47. Fisherman yield cascade tests its thresholds in the wrong order (spot-checked)
- DOS: FUN_15eb_18ec raw 11816-11835: `if (n < 6) +1; else if (n < 4) +2; else if (n < 3) +3; else if (n < 1) +4;` inside `n < 6`. The +2/+3/+4 arms are unreachable; only +1 (fewer than 6 ocean neighbours) and -1 (6-7) apply. The `-2` (8) arm is dead in play too.
- Why bug: the intended order is clearly smallest threshold first (sheltered water fishes better). Confidence: high that the arms are dead; the intended values are a guess.
- Effect: every Fisherman, human and AI.
- Port: `colony_yield.c:220-225` keeps only the live arms.

#### C-48. Major-river bonus only when the river is the sole contributor
- DOS: FUN_15eb_18ec raw 11961-11963: `... (local_12 += u), major && local_12 == u` — the running sum includes farmer, plow and road bonuses. Confidence: low-medium (player-confirmed values agree with DOS).
- Port: `colony_yield.c:572`.

#### C-49. Warehouse spoilage under 2 tons is never removed
- DOS: FUN_364b_0688 raw ~57845-57868: `loss = min(stock, excess); if (loss < 2) loss = 0; stock -= loss;` Stock can sit at cap+1 forever. Confidence: medium (one reviewer reads it as deliberate rounding).
- Port: `colony_goods.c:358`.

#### C-50. Stale claim table charges again for a plot a Pioneer already bought
- DOS: FUN_15eb_26e4 builds the table; the seat (FUN_15eb_06d2, FUN_0000_6582 tail) prices it via FUN_4cc6_07c2, which never tests the purchased bit. Evidence: America save 12 (France). Confidence: medium.
- Port: `colony.c` ~911, ~1005-1050; `colony_workers.c:461`.

#### C-51. Mine depletion suppresses every worked mine
- DOS: FUN_364b_033a raw 56977-57012: on each counter wrap all worked Minerals/Silver plots with a miner are suppressed, not just the one that depleted. Confidence: low-medium.
- Port: `turn_production.c:457-512` (see also E-10).

#### C-52. Forest-clear lumber bonus tests the colony tile, not the cleared tile
- DOS: FUN_479b_01a6 raw ~76800: `0754(colony.x, colony.y) & 0x0a` is always true (city bit), so Lumber Mill colonies always get +20 per clear. The real-DOS "+60" capture matches. Confidence: low-medium.
- Port: `units_pioneer.c:466-468`.

### Movement and pathing

#### C-53. Scored Go To fallback: E/W steps all cost 1, and a dead one-move arm
- DOS: FUN_6662_0f74 raw 104676-104690 (asm 6662:13d1-1420): `&& cur_y != cand_y` is grouped over the whole river clause, so any due east/west step costs 1. FUN_465b_0000 (raw 75450-75465) and the flood tier use the intended grouping. Raw 104700-104706: `090c(unit) < 2` compares thirds (>= 3) with tiles, so the arm never fires (the flood tier tests `DS:0x5234 < 4` correctly). Confidence: high / medium-high.
- Port: `units_move.c:2566-2567, 2602-2625` (#699, #1038).

#### C-54. 00f2 flood cache keyed on the goal only
- DOS: FUN_6662_00f2 raw 103870-103885 reuses the grid when the goal matches, ignoring mover type, owner terms, domain and low-move; costs are bytes (wrap > 255) and the 256-slot queue has no full test. TB-1 is one consequence; live Go To shares it. Confidence: medium.
- Port: `units_move.c:1930-2150` (#706, #1045).

#### C-55. Arrival tile ring search lets the +e hit overwrite the -e hit
- DOS: FUN_48d3_048e raw 77830-77875: the found flag is tested only in the outer loop. Confidence: medium; low impact.
- Port: `units_cargo.c:1850-1875` (`units_spiral_place_hs_near`).

#### C-56. Man-O-War missing from the radius-2 sight list
- DOS: FUN_13f1_02f8 asm 13f1:030e-031e lists Galleon, Privateer, Frigate. Confidence: low.
- Port: `units_map.c:384-397`.

### Europe, market, map generation, census

#### C-57. Market pressure ratio truncated to int16
- DOS: FUN_38fd_0058 raw ~58845, ~58899: `3*sum/ledger` above 32767 goes negative and moves the price the wrong way (late game, large tons2 sums). Confidence: medium.
- Port: `europe_harbor.c:880-886` (#1051).

#### C-58. Magellan west-edge delay is dead code that still draws RNG
- DOS: FUN_48d3_0002 raw 77583-77586: `rand(0,1)` and a Magellan test, both discarded. Confidence: medium-high.
- Port: `europe_pool.c:851-876` (#1019).

#### C-59. Map generator latitude abs() draws rand twice
- DOS: FUN_684c_08c0 raw ~105790-105810: double-evaluated ABS macro; the sign test and the value use different draws. Confidence: high. A fix must still burn both draws; `golden_mapgen_seed100` would need re-recording.
- Port: `map_gen.c:357-370`.

#### C-60. Census counters: some saturate at 255, some wrap
- DOS: FUN_4962_0018 raw 78111-78324 with FUN_4962_0006. Confidence: medium. Inherent to the u8 save fields.
- Port: `col1_stuff_census.c:227-340`.

---

## C. Port already does not reproduce; undocumented

Each needs a decision: add a dos_bugs entry (keep the port as is), or port the DOS behaviour literally.

### C-61. Tory uprising size comes from the last colony's score, not the best
- DOS: FUN_43f7_06a6 raw 73829-73931 (asm 43f7:0788-0795, 0930): best kept in `[BP-0x14]`, spawn loop reads `[BP-0x16]` (last scored colony; -1 if disqualified, which cancels the uprising). Confidence: high.
- Port: `ai_king_ref.c:447-575` uses `best_score` (~516).

### C-62. Open-continents counter DS:0x9650 is never reset (spot-checked)
- DOS: FUN_4962_0018 raw 78316-78319 is the only write (`+= 1`); census entry zeroes every sibling counter but not this one. It grows across every census call, so the 20e6 gates (`!= 0`, `< 2`; raw 84955-89663) degenerate after the first turns. Confidence: high.
- Port: `ai_euro_land.c:1138` recomputes per call. Late-game Euro AI ship behaviour differs from DOS; idle gates (<= 63 turns) do not see it.

### C-63. Military Assistance passes the nation id to the tribe selector
- DOS: FUN_5bfb_153e raw 98271, 98335 (OVL16 0x2da4, 0x2ec6): FUN_281f_0a42 gets 4..11; all 17 other callers subtract 4. Extinct test and hire price read the wrong tribe. Confidence: high.
- Port: `ai_diplo.c:1883, 1951` use `p - 4` and have no extinct filter.

### C-64. Euro diplomacy score and withdraw cost overflow int16
- DOS: FUN_5bfb_153e raw 97530-97577, 98150-98156 (OVL16 0x1990-0x1a02): `(diff+8)*delta*10` wraps past 32767 (delta > 273 at Viceroy), making the score 0 or random; withdraw cost wraps and is raised to 100. Confidence: high on the mechanism.
- Port: `ai_diplo.c:1223, 1243, 2183` use 32-bit int.

### C-65. Denounce Heresy weight compares two loop invariants
- DOS: thunk_FUN_1000_a594, overlays.c:75807 `if (uVar3 == param_2)` (asm OVL13 ~0x1f3f-0x1f5e): both are constant in the loop, so every village's mission weight goes to one side; the per-village owner is computed into `[BP-4]` and never read. Confidence: high.
- Port: `ai_contact_actions.c:1457` uses the per-village owner. (DB-7 is a different slip in the same function.)

### C-66. @INDIANCOME follow-up gate calls the alarm reader with swapped arguments
- DOS: FUN_5bfb_0182 raw 96551 (asm 5bfb:01e0-01f5): `030c(euro - 4, indian)` reads before the Indian records. Confidence: high.
- Port: `ai_contact.c:478/525` (`rel_before < 25u` on 100-alarm), opposite polarity to DOS's intended test.

### C-67. Missionary revisit gate passes a tribe index as a nation id, and the human
- DOS: FUN_521d_20e6 raw 90057-90063 (overlays asm 138096-138101): `8c28([0x5398], [0x8d52])`, missing `+ 4`. Confidence: medium.
- Port: `ai_euro_land.c:2953-2960` uses the AI nation and the right record.

### C-68. Wander scorer's Euro-foe arm with owner nibble -1
- DOS: FUN_521d_20e6 raw 88877-88880 (asm 138877 signed `CMP [BP-0xe],4`): owner -1 passes and reads nation+0x33 (`return_from_europe_y`) as a relation byte, so the PEACE test is effectively ignored on unclaimed land. Confidence: high.
- Port: `ai_euro_land.c:2342` stand-in uses the occupant's nation.

### C-69. Homeless Brave despawn returns -1 and the caller steps the freed slot
- DOS: FUN_4d56_021a asm 0x337-0x365 returns -1 after destroying the unit; FUN_4d56_14fe 0x1502-0x1521 then steps it with dir -1. Use-after-free; corrupt/edited saves only.
- Port: `ai_brave.c` ~407 stops cleanly (#826).

### C-70. 5d04 Artillery price unset before turn 40
- DOS: FUN_521d_5d04 raw 92834: `local_38` assigned only when turn >= 40, otherwise stale or uninitialised. Port uses 0 (`ai_euro_europe.c:1427-1431`). DB-5 class.

### C-71. 5d04 Pioneer conversion leaves tools stale
- DOS: the unconditional Colonist->Pioneer conversion never sets +0x3159, and FUN_479b_0158 then wraps it to 236.. (#628, #636). Port substitutes tools = 100.

---

## D. Unsure: possibly design

- **C-72. Pioneer work counter not reset when the work changes.** Unit +0x315a is incremented by FUN_479b_01a6 (raw 76753) and 0526 (raw 76886) and zeroed only on completion, fortify (raw 42418) or colony entry (raw 76733); order entry (raw 42511/42602) writes only +0x314c. Chop 3 turns on A, walk away, Clear on B: done next turn. Port `units_pioneer.c:328`, `game_loop_eot.c` ~817 (#620). Human and AI.
- **C-73. LCR counters DS:0x1dc6/0x1dc7 are process-global.** FUN_65dd_0004 raw ~103461, 103497, 103527: never reset on New Game, not in the save, byte wrap. Port carries them in RTST (`units_combat.c:3210-3225`).
- **C-74. LCR trespass alarm can go negative.** FUN_65dd_0004 raw ~103560 unclamped `rand(1,6) + (diff - skill + 1)*5`. Port `units_combat.c` ~3405.
- **C-75. Raid horse swap tests the rolled cargo's pile.** FUN_5fef_0f14 raw 99822-99825 `stock[local_20] > 0x34`. Port `ai_contact_raid.c:282`.
- **C-76. Raid burn demotes the lowest owned building tier** (Church burns, Cathedral stays). FUN_5fef_0f14 raw 99832-99863 (#924). Original saves show lone upper tiers.
- **C-77. Fur Trapper gets road and river twice.** FUN_15eb_18ec raw ~11830-11846 plus the generic stack 11949-11963; the Hudson capture confirms DOS does it. Port `colony_yield.c:335-345`.
- **C-78. @GIVECASH overwrites the tone buffer, so a later refusal always reads "MEEK".** FUN_5bfb_153e raw 98005-98041; port `ai_diplo.c:2158-2172`. Reviewers disagree on intent.
- **C-79. CANCELTREATY section missing from GAME.TXT.** FUN_5bfb_13b0 raw 97260-97316 uses tag DS:0x1898; no such section, so an AI treaty break is silent (user-confirmed). Data bug; the port cannot add MicroProse text, only a port-authored notice.
- **C-80. Explorer goal 6 never posted.** raw 89270-89280 else arm unreachable; the +0x10 explore-goal bonus is inert.
- **C-81. AI hull entering Europe leaves the current-unit global on it**, so the human autosave names an AI unit (`ai_euro_ship.c:1563`). Stale-RAM leak into the save.
- Low-confidence leftovers: 20e6 `milvet == 0` clear dead at peace (raw 89531, `ai_euro_ship.c:2278`); 5d04 `|| type == 0x0b` contradicted by `0xc < type` (raw 92996, damaged hulls never leave); passenger-mark loop bound DS:0x1734 never changes (raw 90266); two AI recruit price formulas disagree (raw ~92603 vs ~92864) and differ from the human's; `rival_navy_strength` sums 3 nations then `>> 2` (`ai_euro_europe.c:258`); cower counter shares +0x315a with the voyage counter.

---

## E. Port-vs-DOS leads (not DOS bugs; port_bugs candidates)

DOS behaviour looks intended here and the port differs. Simulated 2026-10-22:
each fix was verified against the decomp, applied alone in a scratch copy of
the tree, and run through full ctest (103 tests) plus `golden_ai_joint`, then
discarded. All 11 confirmed. No fix changes any golden or idle campaign; only
E-8a breaks unit tests (fixtures pin the old weights).

Applied 2026-10-22 as port_bugs.md #1136-#1146 (E-1..E-11 in order; E-8a and
E-8b together are #1143), rows in docs/archive/bugs_fixed_pending.md. Two
differ from the simulated patch: E-7 stops at DOS record 0 via
`units_dos_index` (port unit ids start at 1), and E-8b fixes the empty-span
draw in `dos_rng_range` itself.

- E-1. `ai_diplo.c:1114, 1124` (FUN_5bfb_153e): shift counts inverted. OVL16 0x16bd / 0x1786 (`cmp byte,1; sbb; and 1; add 1`; decomp raw 97442/97458) shift by 2 when difficulty (DS:0x53a6) / target_colonies is 0, else 1. Fix `== 0 ? 2 : 1`: all pass; branch not exercised.
- E-2. `ai_euro_465b_privateer_sighting` (`ai_diplo.c:3178`) ORs 0x40 (PEACE); DOS asm (viceroy_unpacked.asm 119153-119159) ORs 0x02 (WAR) into nation[occupant].euro_relation[mover] when the occupant is weaker, else 0x08. Fix PEACE -> WAR: all pass; not exercised. The comments in `ai_diplo.h` ~74-85 and `ai_diplo.c` ~3095-3110 also call bit 0x02 PEACE and need rewriting.
- E-3. `ai_diplo.c:1687` tribute gate `gold >= score`; DOS raw 97894-97897 (OVL16 0x240b-0x241e) is strictly `gold > score`. Fix `>`: all pass; equality not exercised.
- E-4. Demand Tribute @EXTORTSTUFF cap, `ai_contact_actions.c` ~1185-1199: DOS (thunk_FUN_1000_a5f4, overlays.c 77428) sorts DS:0x9e78 in place with FUN_1cf8_000a (unsigned, unstable) and caps at `min(100, bid[top]*3 + 10, room)`, floor 10. The port read the unsorted Muskets slot, so the cap was always 10; its local insertion sort is also stable and signed (tie order differs). Fix: reuse `ai_contact_2820_sort` (made non-static), read the top bid. All pass; human-only path.
- E-5. `units_combat.c` ~1383 capture gate: FUN_5fef_0352 param_1 = loser, param_2 = winner; raw 99378-99383 tests the winner's type for a hull, so the port's `!loser_is_hull` is dead. Fix `!winner_is_hull`: all pass; not exercised.
- E-6. `units_nearest_own_drydock_colony` (`units_combat.c:1763-1781`): DOS raw 99584-99600 seeds 9999, uses FUN_281f_037a = FUN_124c_007c octile `max + (min >> 1)` (`map_dos_dist`), takes `d != 0 && d <= best` (last tie wins, own tile excluded). Fix: all pass; not exercised.
- E-7. WoI capture re-home, `units_capture.c:248-266`: DOS raw 100960-100975 walks the 8 neighbours, requires the chain head's nation to be the captor, then re-homes the whole chain `while (0 < id)` (id 0 ends the walk; non-crown units in that chain are re-homed too). The port's winner exclusion has no DOS counterpart (no practical effect). Fix: all pass; WoI goldens unchanged.
- E-8a. Tea party weights, `ai_king_audience.c:378`: DOS asm 38fd:3e4c-3e7f is `labs(tons) / 100` via 1d1d:0ec6 (signed 32-bit divide), not `* 100`. Fix: idle goldens and `golden_ai_joint` pass (idle saves have all-zero `trade.tons`); `unit_ai_king_core` fails 10 cases (`popup_audience_accept` first: payload 400, want 403) because its fixtures use tons 9 / 7, which divide to weight 0; bumped to 900 / 700 they pass.
- E-8b. Tea party RNG draws, `ai_king.c:117, 147`, `ai_king_audience.c:595`: DOS 3dc8 draws once for every human audience with a nonzero change, including cuts and an empty roulette (FUN_19ef_0032 always draws); the port drew nothing there. Fix: all pass. `golden_idle_america` hits the empty roulette 3 times (saves 31, 44, 56) and stays byte-identical, because the LCG is reseeded at every nation slot. Making `dos_rng_range` burn a draw when hi < lo also passes everything.
- E-9. Succession FUN_43f7_0218, `ai_king_audience.c` ~840: DOS deletes only units off the map interior (FUN_281f_0302 = FUN_137f_000a) and transfers the rest; the port kept only units inside colonies. Fix `map_coords_inset`: all pass; no test has merged-nation units. Still not literal after the fix: DOS writes unit +3 = heir as a whole byte (port sets `1 << heir`) and runs a seen-nibble merge and tile-owner remap the port skips.
- E-10. Mine depletion, `turn_production.c:487-488`: FUN_281f_0c0e = FUN_15eb_0e18 returns the occupation (colony +0x20), not the specialty (+0x40, FUN_15eb_0e52). Fix: key on `field_job`; all pass; not exercised. The function's header comment says SPECIALTY and is wrong.
- E-11. Tribe population total, `ai_contact_demand.c:2426`: FUN_4962_06b6 asm ~124745 `ADD byte [SI+0x9622],AL` wraps; the port saturates at 255. Fix wrap: all pass; no total ever exceeds 255 in any test. Village and per-continent counts are also plain byte INCs in DOS (port caps them; irrelevant in practice).


## F. Housekeeping in the existing docs

Done 2026-10-22:

- dos_bugs.md header: the three stale "ported literally" examples are removed
  (they were DB-9, the TB-8 site and DB-8).
- TB-8: raw cite corrected to 81617-81628 (also in `ai_contact_demand.c`), and
  the `(pop + 25) * 2` current-tribe cap detail added.
- known_divergences.md #780 moved to dos_bugs.md DB-10; #909(d) (@TRAINFAIL
  stale %STRING0) moved to DB-11. Stubs left in known_divergences.md.
- #909(b) (University-without-College teacher seating) stays in
  known_divergences.md: DOS testing the College bit only is not clearly a bug.
  The state is reachable through C-76 (raid burn leaves an upper tier).
