# dos_bugs.md evidence archive (2026-10-23 batch)

STATUS: archived 2026-10-23, closed. Evidence, trace and test-history text moved
verbatim (links re-rooted, short lead-ins added) out of
[docs/dos_bugs.md](../dos_bugs.md) entries to keep that file under the doc-length
limit. Not a spec: each entry in `dos_bugs.md` stays authoritative and keeps its
id, DOS citation, port site and approval note. Look up one id:
`grep -n '^## DB-NN$' docs/archive/dos_bugs_evidence_2026-10-23.md`.

## DB-1

Evidence: the first two idle-independence recordings
([idle_independence.md](../idle_independence.md) "Earlier recordings") started
from such a load and drifted from a clean main-menu load of the same slot.

## DB-2

A DOSBox load of a mid-campaign autosave does not reproduce the continuous run
(save 5: 355 RNG draws vs 381).

Port, nine records (2026-10-19): RNG state + the `FUN_281f_04ca` reseed word,
stance `DS:0x9870` + rival strength `DS:0x9e98`, unit-slot compaction residue
(`ColonizeUnitPool.dos_tail`), the AI goal / work-queue / inventory / plan tables,
the rank cache `DS:0x917c` + profession census, the coarse fog plane `DS:0x9faa`,
Euro AI scratch (0a60 masks, explorers, village-visited bits, ship pressure,
sticky anti-spin), per-colony `plot_was_worked_mask`, and the pathfinder word
`DS:0x1dd2` + flood cache. The current list is the `RTST` row of
[savegame.md](../savegame.md) "Port extension block".

## DB-4

Some writers are deterministic: FUN_6662_00f2's `PUSH SI` (nation*0x13c, only when
00f2 is entered at SP e852 through 6d8e -> 0972 -> 0f74), the FUN_5bfb_022e met
arm (the mover's x), and the previous 20e6 scan's own saw-foe flag. The others are
RTLink overlay-loader residue (an overlay size or relocation word, e.g. 0x2fe0,
0x4232). Which overlays reload, and so the residue value, depends on the overlay
area's history since process start.

Evidence: america seed-100 save 63 -> 64 (England's Dragoon u61 beside Quebec
parks in the archive; a from-load parks all three Dragoons), and dozens of resync
pairs past it
([idle_america_fixed_2026-10-19_21.md](idle_america_fixed_2026-10-19_21.md), "e7c4
class").

2026-10-23 (port_bugs.md #1135), DOSBox VR_SOUND from-loads with `BPM` on SS:e7c4 and
the 20e6 entry kept as INT 3 (ENTER emulated, see dos_trace.md):

- Founding (america 105, England founds Boston, turn 128): the last write before the
  next 20e6 is `0e0f:0b53` (FUN_15eb_0b52's `push bp`) storing 0xe7d6, the frame pointer
  of FUN_15eb_0b96 in the chain 076e -> FUN_364b_1ba8 (second FUN_281f_0c22, raw 58089)
  -> 15eb_3956 -> 394c -> 1f72 -> 0bd4(1,9) (raw 12688) -> 0b96 -> 0b52. Identical in two
  from-loads; every later English 20e6 sees it until the next flood. Ported: with it
  america resync 105 -> 106 is byte-identical when DB-25 and DB-28 are reproduced (the
  port's non-reproductions of those two leave 127 diffs there).
- Hull step (america 156 and 137): each step's flood `9d71:00fa` writes nation*0x13c,
  then `0ba3:01ac` stores 0x020e (FUN_137f_0200's near call to 01ac at 137f:020b) via
  465b_0000 raw 75698 -> FUN_281f_0916 -> 1427_12f6 -> 10be -> 04d6 -> 0362 -> 02ca ->
  005c -> 137f_0314 -> 0200, but only on a step off a tile the hull shares (04d6 lifts
  the stack and 023a clears the presence bit, so a lone hull's re-place skips 0200).
  Ported; full resync 1..465 goes from 148 to 166 byte-identical pairs, none lost.
- Not modelled: a visible move's redraw (america2 14: FUN_112b_01ba unit-icon draw ->
  FUN_4cc6_03f8 zeroes its local array, `[bp-0x1c]` = e7c4 at BP e7e0) and the loader
  writes (`f000:d105` BIOS read, 0x2fe0; 13c2/1272 frames, 0x4232). In from-loads the
  loader runs after many hull steps and overwrites 0x020e.

## DB-6

Every golden and idle campaign stays byte-identical. Unit test: `unit_turn_core`
`phase_k_build_advisory` ("ORE net-tools-zero" fails on the DOS test).

## DB-7

Unit test: `unit_ai_contact_core` `actions_menu_missionary` / `actions_menu_wagon`
(heresy block). All goldens, idle campaigns and `golden_ai_joint` stay byte-identical
(2026-10-21).

## DB-8

Unit test: `unit_ai_diplo` `case_153e_wantstuff_demand` (Muskets move, Furs do not). All
goldens, idle campaigns and `golden_ai_joint` stay byte-identical (2026-10-21).

## DB-9

Unit test: `unit_colonies` #559b (fails on the DOS arm). All goldens, idle campaigns and
`golden_ai_joint` stay byte-identical (2026-10-21).

## DB-12

Unit test: `unit_europe` (@REALLYBUY block: open and No leave the counter alone, Yes
adds one). All goldens and `golden_ai_joint` stay byte-identical.

## DB-13

Unit test: `unit_colonies` foreign-colony trade ("colony hands over the offered cargo").
All goldens and `golden_ai_joint` stay byte-identical.

## DB-14

Unit test cases of `unit_ai_king_core` `dump_goods_pick_api`: weighted 7:9 over
200 seeds, zero-weight pick over 50 seeds, exactly one draw burned in both,
campaign4 weights.

All goldens and `golden_ai_joint` stay byte-identical.

## DB-15

Unit test: `unit_colonies` school faculty + occupation cap (three distillers fit, the
fourth is refused). All goldens and `golden_ai_joint` stay byte-identical.

## DB-16

The same block fills every token of @KINGMERCY but never shows it, and VICEROY.EXE
has no KINGMERCY tag string (dead text), so the feature was removed on purpose.

## DB-22

Unit test: `unit_ai_contact_core` (capital-destroy surrender block, friction expectation
changed from 0x20 to 0). All goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-23

The DOS evidence for the carry-over (america resync 129: Penobscot's walk to
Barbados ended at (44,39), so Providence was skipped, one `rng(0,2)` fewer than a
per-candidate reset draws; america2 resync 37) lies past the enabled golden saves
of both Americas archives.

All goldens, idle campaigns and `golden_ai_joint` stay byte-identical (2026-10-22); no
unit test pinned the carry-over.

## DB-24

The founding join's stack word: 35 at Curacao (35,59), DOSBox seed-100 265 to 266.

All goldens, idle campaigns and `golden_ai_joint` stay byte-identical (2026-10-22); no
unit test pinned the founder residue.

## DB-25

(The DOS evidence for the wrap, america resyncs 105-119 and 151+, lies past the
enabled golden saves of both Americas archives and would need the old behaviour.)

No test pins the wrap; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-26

Unit test: `unit_regress_ai_tables` `case_530_unload_mask_scan_and_order` ("a later
rejected shore must keep the earlier mask" fails on the DOS behaviour). All goldens,
idle campaigns and `golden_ai_joint` stay byte-identical.

## DB-27

No test pins the slip; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-28

The enabled goldens never complete a ring hop, so they do not exercise this (the
hop evidence in the code comment, america resync 92->93, is past the enabled
golden saves).

No test pins the signed store; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-29

All goldens, idle campaigns and `golden_ai_joint` stay byte-identical, so no test in the
suite reaches a Muskets roll with a later draw that depends on it.

## DB-30

All goldens, idle campaigns and `golden_ai_joint` stay byte-identical.

## DB-31

All goldens, idle campaigns and `golden_ai_joint` stay byte-identical (no idle campaign
puts that much military next to a village).

## DB-32

Unit test: `unit_units_types` `unit_promote_convert_rng_598` (now expects the same RNG
state with and without Washington). All goldens, idle campaigns and `golden_ai_joint`
stay byte-identical (2026-10-22).

## DB-33

Only one golden buy happens (`golden_idle_america`, save 31, free-colonist count
0), so the goldens do not exercise a non-zero add.

Unit test `unit_ai_king_core` (audience cut branch, 1d42 buy beat) checks that the word
keeps its seeded value. All goldens and `golden_ai_joint` pass.

## DB-34

Unit test: `unit_colonies` ("1-ton spoil not reported", "1-ton overflow removed"). All
goldens, idle campaigns and `golden_ai_joint` stay byte-identical.

## DB-35

A 2026-10-22 trial moved the test to the cleared tile (goldens passed), but the
user chose the DOS behaviour; the live DOS capture (+60 at Bahia) matches it.

## DB-36

No unit test calls it; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-37

The goldens reach this fallback only about once per campaign, with a ship, so they
barely exercise the change.

No unit test calls it; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-38

`unit_units_ships` still passes; all goldens, idle campaigns and `golden_ai_joint` stay
byte-identical.

## DB-39

No test reaches that range; `golden_market_prices01`, all idle campaigns and
`golden_ai_joint` stay byte-identical.

## DB-40

The input-replay regression `europe_buy_ship_and_sail` pinned the extra draw: its
`output.sav` was re-recorded from the port (only the 4 LCG-state bytes of the port
extension block differ).

Every golden, idle campaign and `golden_ai_joint` stay byte-identical, so no golden
crossing starts or ends west of column 3.

## DB-41

Not exercised by any golden.

## DB-42

The idle goldens (63 saves at most) end before the counts fall below 2, so they do not
show the difference.

## DB-43

User-observed 2026-10-22 (campaign5): Jamestown's fortress fired on a French
Caravel; England and France had relation byte 0 through turn 123.

Unit test `coastal_fort_naval_fire` (unit_units_core) covers the unmet case. No golden
changed.

## DB-46

The 5952 phantom Pioneer's order latch: DOS calls the work bodies directly there;
zeroing it broke every idle golden.

Unit case `unit_pioneer_dos_gates_2026_09_22` and the replay `map_unit_orders`
(its fixture preloaded the counters for an instant finish; re-blessed: the two
jobs now start at progress 1) were updated.

All goldens and `golden_ai_joint` pass.

## DB-47

All goldens, the reload goldens and `golden_ai_joint` pass.

## DB-49

All goldens and `golden_ai_joint` pass.

## DB-50

All goldens and `golden_ai_joint` pass; unit case in `unit_ai_contact_core`
(port_bugs.md #924) updated.

## DB-54

DOSBox-confirmed 2026-10-23 on dutch-reports.SAV Curacao: (43,41) mask 00 -> 10, owner
Arawak -> Dutch, nibble 0xf -> 0, before the menu draws.

Test `unit_stage_seams test_colony_jobs_menu_open_has_no_side_effects`; replay
colony_tile_assignment.

## DB-55

DOSBox-confirmed 2026-10-23 (french-campaign COLONY04.SAV, patched missionary next to
the Iroquois at 23,67).

## DB-56

DOSBox-confirmed 2026-10-23 (french-campaign COLONY02.SAV, virtual mouse at DS:0x92f8).
