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
13,423. 1493→1494, 1494→1495 and 1495→1496 pass byte-for-byte; 1492→1493 is down to the
human's first-turn UI; 1497→1498 only to the stance-table artifact below.

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

## Fixed in the 2026-10-06 pass

- Europe arrivals: `FUN_48d3_06ba` tail (064e→048e) places hulls in the nation's own lane tick,
  before 0a60; the fitted "first leg + west-explore (4,13)" exit course is deleted (the (4,13)
  goto is the 20e6 far roam, which now also writes plan `'D'`, 521d:4e86). 048e's ±e pair loop:
  the +e hit wins.
- Day-top spent clear moved to the Indian mid-pass entry and now restores runtime MP; the
  calendar advance moved there too (slots above the human act under the old turn; market
  ticks use each slot's own turn).
- AI landers reveal on landing (465b_0000 07a0); first contact stamps `contact_state = 2`
  (raw 96669); the invented WELCOME land grant (purchased bit) is deleted; AI moves no longer
  set `named_new_world` (bugs.md #1057a reversed: 049e is the human UI mover).
- `FUN_4962_0018` ported per nation (`col1_stuff_census_4962_w`), called at each nation's 00f2;
  `FUN_1427_0824` / `FUN_364b_1ba8` / colony delete keep `all_unit_counts` / `colony_counts`
  live in between.
- Colony founding mints its COL1 record (1ba8): custom-house export bits, `visible_to_euro`
  all 1, founder's +100 `rebel_divisor`; `last_colony_founded_turn` is the nation record field.
- Capture keeps DOS's stale hold bytes past `holds_occupied` (DOS remover `FUN_15eb_317c`
  never clears); autosaves stamp the 130d_0172 head state; Move Pieces entry resets 0x5392.

- Ship tile-chain order (DOSBox BPM trace of unit+0x18/+0x1a, 1492→1494). Every ship relink is
  `FUN_1427_10be` (04d6 the tile, hull to the -2 chain, passengers on it top-down) then 040c
  (relink top-first, so reversed). The 465b mover adds `FUN_281f_08e4` = `FUN_1427_0644`
  (04d6 on the -2 chain) in between, which leaves the hull *under* its passengers; the 20e6
  LAB_3558 band (overlay 0x3609 10be, 0x3693 040c on the own tile) and the 064e Europe
  placement do not, which leaves it on top. Port: `units_tile_stack_ship_relink`.
- AI explorer fatigue / hop countdown / hop slot are the save's `cargo_hold[0..2]`
  (+0x3154..56) on the unit (`ai_euro_20e6_hold_scratch`), not session arrays; spawn resets
  them 0/0/0xff (06b4). The land wander tail writes plan `'9'` (raw 89040) like the ship one.
- Every plot seat (`FUN_0000_6582`) ends in `6518(dx,dy,1)`: mask bit 0x10 on the worked tile,
  claimed or not (all 869 worked plots in the saves carry it) — `colonies_stamp_worked_plot`.
- `FUN_5952_035e` opens with `memset(colony+0x8a, 0, 2)` (OVL15 0x39f, BPM-confirmed): AI
  colonies lose the Custom House bits on their first tick. The invented "default bits when the
  Custom House completes" in colony_build.c is gone (0d26's only callers: 1ba8, human dialog).
- DS:0x35e is 0 during the tick's food pass and pass 2 and set at LAB_17a9 (raw 94628), so
  28c8's food weight `local_4` is 0 there (BP trace of 15eb:2d14 scores).
- The tick's absorption arm joins through `FUN_15eb_1068(outside slot)`: idle, no 2ea0 seat
  (`colonies_admit_unit_idle_w`).
- Move vis (465b raw 75764-75772): `07d6(unit, 06dc(dest))` ORs the destination's owner-nibble
  bit as read before the relink, for every mover including braves (BPM-traced); the port read it
  after its own arrival stamp and only for Euro movers. `units_vis_mask_after_move(..., dest_owner)`.
- Side fix (golden_woi_ref01): the crown MoW sail-home gate's `iStack_a8` is the hull's own
  -2 group (taken at 0x3609 after 10be), not the tile; DS:0x9456 is counted by the 0x45 plan
  stamp, not order 11 (shared with every AI goto) — the REF fleet no longer parks for good.

## Fixed in the 2026-10-07 pass

- Per-slot EOT and RNG phase (DOSBox BP on 1541:0e04, per-slot draw counts now equal DOS's for
  every AI and Indian slot): each AI nation's colony EOT (5e52 immigration, Phase A bells + 0a22
  elect, production, crosses) runs at the head of its own slot (`turn_run_ai_nation_eot`), not
  batched in SETUP; 5e52 opens with the timer-word reseed `FUN_281f_04ca` (raw 68542), like
  6d8e. The human's nation tick moved to FINISH (its own 00f2, after the day top) with the same
  reseed. `FUN_4345_06d2` rolls every category before `015a` picks one; `015a` ties go to the
  later category.
- Unit array order: `FUN_1427_0824` compacts and `06b4` appends, so new units take `slot_end`,
  not the first hole; the vacated DOS record keeps its goto/facing and the next spawn inherits
  them (`ColonizeUnitPool.dos_tail`). `06b4` binds `origin` to the colony on the spawn tile.
- Colony colonist bytes past `population` keep DOS's stale tail (`FUN_15eb_0d04` never clears);
  idle colonists are job 0x12 (DOS's own idle join).
- Recruit pool stores DOS's 0x1c for Free Colonists (46d4 free tier, Brewster, 3-experts bail);
  the human's 0x40 crosses-immigrant latch is a save bit, not a unit heuristic; @TUTORIAL5 latch.
- Plans: 20c6 walk-home `'V'`, LAB_4567 `'5'` + orders 0x0b for wagons/treasure/hauls; 20e6
  origin bind (OVL14 0x24ec, `[BP-0x32]` is the is-ship flag, not stance); colony tick binds
  unbound tile units (`DS:0x8dc6`); colony eject reveals while the colonist still counts.

## Harness artifacts (not port bugs)

- The `-0x6790` stance table (DS:0x9870) is not in the save and is zero after a DOS load; the
  continuous run carried last turn's values. A DOSBox load of year_1497 reproduces the port's
  Isabella `short_defenders`/`specialty_cargo`, not the save's.
- DOS's music picker (`FUN_129f_0008`) draws twice at the human's slot start; harmless because
  every slot's 5e52 reseeds.

## Open leads (most transitions first)

- Human end-of-slot draws: DOS 5, port 1 in 1497→1498 (human FF debate rolls / merc offer?).
- Multi-colony nations: Phase A runs for all a nation's colonies before any production; DOS
  interleaves colony by colony (`turn_run_ai_nation_eot` ponytail note).
- **AI colony tick vs DOS**: still the main source from 1499 on (worker/tile choice,
  `specialty_cargo`, `building_in_production` 255); trace the first diverging colony with the
  15eb:28c8 / 2d14 breakpoints. Downstream: alarm/friction, recruit pool / FF pick RNG phase.
- Remaining `transport_chain` diffs (~720 leaves) sit in transitions that already diverge
  elsewhere; recheck once those close.
- **Brave routes next to new colonies** (1496→1497): a nation-7 brave steps past French Isabella
  in DOS (owner stamps 7 on (44,52)/(44,53), tribe friction 9, relation accum) but not in the port —
  the 021a scorer's inputs around a just-founded colony. Remaining `vis_mask` diffs (~590) are
  mostly downstream of route/placement divergence; recheck after.
- DOSBox loops: press space every ~200 INT16 polls (popups and the idle human's turn both take it),
  and match units by content, not index — a colony founding compacts the array mid-turn.
- `nations[].indian_hostility_sticky` is a port stand-in stored in DOS-dead byte +0x4b;
  DOS saves always carry 0 there. Needs a port-only home (COLNXEXT) before the byte can be 0.
- 1492→1493 only: `tut2.nr1`, `rival_nation_slot_2`, `stuff.x/y`, `map_mode` come from the
  human's first Move Pieces (tutorial popup, View key); not reachable headless.
