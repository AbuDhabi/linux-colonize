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
13,423 → 12,739 → 8,798 → 8,122 → 7,023 → 5,666 → 5,597 → 5,591 → 5,531 → 5,502. Fourteen archived transitions
pass byte-for-byte: 1493→1494, 1494→1495, 1495→1496, 1505→1506, 1512→1513,
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

## Fixed in the 2026-10-07 second pass

- AI Europe dock is the per-nation sentinel 236+n (FUN_38fd_0718 `n - 0x14`), not (200,100):
  recruits, purchases, lane arrivals, damaged hulls.
- FUN_479b_0972 crosses at once when an 'E' goto ends on High Seas (raw 77091-77098); 007a also
  writes the nation's `return_from_europe` copy, sentries/stamps the passengers, and keeps plan 'E'.
- 015e's own ring (`units_015e_hs_course`) for the Europe course; 048e's spiral stays for placement.
- Patrol stay goes through LAB_589e (orders 5, facing 8); surplus recall commits via 20c6 'W'.
- Brave 021a writes facing and the orders latch from the pick (0x11b9-0x126e) before the tail; a
  tail stay only exhausts MP (0x14e6).
- Harness: `golden_turn` now passes `ctx.names` (every @RESOURCE site score read 0 before).
- 5d04 goods buy = FUN_38fd_1ebc: no gold gate, treasury clamps at 0 (FUN_15eb_0556); passengers
  and goods share the hull's holds when dock units board.
- 5952 improve arm: the tribal-claim alarm is `030c(village NATION byte, nation)` unadjusted, so
  nations 4..7 read indian[4..7] (DOS quirk, `ai_euro_5952_alarm_word`); the phantom pioneer
  stamps its visitor nibble (`units_set_nation`).
- Gift picker sort FUN_1cf8_000a is not stable (moved element lands before its equals, unsigned).
- Market tick FUN_38fd_0058 runs at the head of each nation's own 5e52 (after the reseed), not
  batched at the round's end.
- 4cc6_00f2 tier test caps only the new alarm at 99 (cooling 100→99 clamps village attitudes).
- 465b LAB_0bd1 tail: an unsettled AI move (attack, board, block) with no unit lost → orders 0,
  +0x315a++ (wrap 20: reset + exhaust); `units_465b_0bd1_tail` now runs for the Euro goal
  walker and Brave foreign-destination steps. The latter matches the 1534→1535 Fort Orange
  raider's counter 2.
- 015e ring skips the map border (078c reads class 0x19 outside FUN_137f_000a's interior).
- 5bfb_3180 turns each newly met neighbour stack head's facing to the scan direction.
- 4cc6_03f8 threat pressure halves only on a settlement tile (06be), not on unit presence.
- Nation +0x4b is NOT DOS-dead (lategame DOS saves carry 1..11): round-tripped as `unknown_4b`;
  the port's Indian-hostility sticky is now a session cache re-derived on apply.

## Fixed in the 2026-10-07 third pass (DOSBox-traced)

- Village tiles never take a Euro owner nibble: a port spawn ran the occupancy claim with the
  nation-0 placeholder (an empty-village temp defender stamped England), and 021a then saw a Euro
  "colony" next to the village (`units_claim_tile_owner_from_stack`).
- 48d3_03d0 lane tick counts down every unit on the lane tile, passengers included.
- 20e6 land far probe: +8 when the DS:0x9faa coarse cell is empty (was a per-nation seen[]
  stand-in; ships already used the plane).
- 6d8e prelude 0xa0cc[16] counters ported literally: specialty bumps its own good (muskets twice),
  empty muskets/goods-8/tools stocks, minus every own hull's held cargo and each Pioneer (tools).
- FUN_1427_06b4: a native unit's +0x12 word = turn at creation (021a's visit stamp).
- 584a crosses penalty counts colonists on the dock x == 236+n only, not lane passengers.
- 26e4 (via 5952_035e) restamps claimed, workable, empty, unbought plots with the tribe nibble
  (`colonies_26e4_claim_stamp`).
- AI ships berth ON the colony tile (DOS `iStack_2e == 0`); the "adjacent water" substitution
  made a hull one tile out dump, reload and sell its hold a turn early.
- A Brave attacking a colony with no field defender fights 1b0e's militia phantom
  (`units_revere_defend_colony_tile`); a win runs the native colony limb at the target tile.
- Harness: `units_reset_hooks` no longer clears the raid-repelled hook (sim wiring, set at load);
  every golden had lost 1b0e's colony-raid hand-off and its DS:0x54f6 clear.
- 0f14's early-game grace reads the turn on the 1b0e hand-off too (was skipped: no turn_number).
- 13b0 treaty tick runs only from an AI mover's 3180 land encounter with another AI Euro, not
  from the per-turn balance on any adjacency.
- 3180 marks a nation done only when 022e resolved something; a failed mood roll lets the next
  tile of that nation roll and take the scan facing.
- Ship price byte DS:0x84bc = euro_price − 1 (clamped 0) in the delivery score, load matrix and
  sell tail (`ai_euro_ship_price_84bc`; DOSBox 1505 load matrix scored ore 78 × 3).
- LAB_3fa6 is the only AI sail-home after the delivery band (holds full or 2+ occupied); the
  invented "≥ 50 export goods" Europe-export arm is deleted.
- Europe dock sell loop compacts holds after each sale (FUN_15eb_317c, `units_remove_goods_slot`),
  so every hold sells; it used to stop after hold 0.
- Native/phantom spawns get their nation before the tile claim (`units_spawn_allow_stack_nation`):
  a colony militia phantom stamped England on a French colony tile.
- The Indian post-pulse meet/trade stand-in (adjacency first contact, auto-trade, gift/demand) is
  unhooked; 3180 on the mover's step is the only encounter path, as in DOS.
- AI colony horses: 1f72 adds the UNCAPPED herd potential to the horse gross (DS:0x8dd8) and only
  the capped figure to food consumption; 0688 applies gross − consumption for AI colonies, so they
  gain the full potential (human colonies take FUN_281f_0b50's capped figure).
- 0688 phase O horse arm: the surplus goes to nation word +0x4a (`unknown26_pad` low, `unknown_4b`
  high — the "live +0x4b" mystery) with sale amount 0, and `trade.tons[c] += c` still runs. The
  musket arm's +0x49 lot counter is still on the Europe screen (+0x49 hosts the privateer stand-in).
- 3180's neighbour head is the tile CHAIN head (07e0 -> 1427_0002), not `units_id_at`.
- 20e6 Missionary arm (type 3) ported: alarm-weighted village pick, plan 'J'; no pick turns the
  unit into a Free Colonist. (An earlier note said the arm did not exist.)
- An AI hull docking at its own colony exhausts the passengers it puts ashore (FUN_281f_0934).
- The Missionary arm resolves the mission from the adjacent tile when its pick is next door (the
  walker's first step would enter the village: 465b -> 4528 case 3).
- Overnight FORTIFY -> FORTIFIED with moves 0 is human-only; DOS acts AI units on orders 0/5/6.
- FUN_1427_0d38 (08bc) stack query modes from the jump table (1427:0d78): 4 = military types
  1/4/6..9, 0xc = Artillery, 0xe = largest undamaged hull capacity. The 5d04 colony-demand hire
  read 4 as "land units" and bought a Dragoon DOS never bought.
- 20e6 ship band: local_a8 (stack count) is a band-entry snapshot, so a hull that just landed its
  passengers does not take the dock-demand sail-home in the same act.
- Save export keeps the runtime colonist order (no canonical occupation sort): DOS insert-sorts
  only at add time and never re-sorts on a job change.

## Fixed in the 2026-10-07 fourth pass (DOSBox-traced, 1515→1516)

- Nation +0x48/+0x49/+0x4a are the col1 record's own bytes, not port scratch: 5d04 (raw
  84230-84468 via DS:0x84fc) decrements `king_grace_counter`, spends/credits `musket_bank_lots`
  (+0x49, was misnamed `privateer_spawn_mask`) and normalizes against the +0x4a word
  (`col1_nation_bank_4a`). 0688 phase O banks AI colony musket surplus there per 50 and still runs
  0a2e plus the tons quirk on a 0 remainder (the EuropeScreen batch counter is gone). The
  wartime-Privateer stand-in's latch moved to session state.
- DS:0xa0cc is one table: 0xa0d4/0xa0da/0xa0db are its cells 8/14/15, so the prelude's
  "muskets specialty / empty stock" bumps feed the 5d04 tail's ship-buy demand too
  (Spain bought 100 muskets the port skipped).
- 00f2 order: 5e52 immigration tick, then the 0a82 lane tick, then the colony loop (raw
  58375-58384). The port ran the lane tick first, so a hull reaching the dock was counted by the
  584a dock penalty.
- 2424's SoL cache (nation +0x19) is written per slot at the 00f2 tail (0a66, raw 58392), before
  that nation's units move; the port wrote all four after the turn.
- FUN_15eb_317c shifts holds only below the count and clears nothing; the port now mirrors that
  on the raw hold bytes it round-trips, so stale slots match the save.
- Trace method: BP on the resident buy routine 2143:0d8e (291f_0d8e) reads the 5d04 caller's
  locals; one-shot `--patch-cc` traps miss later calls once RTLink reloads the overlay unpatched.

- A colony join never writes +0x8e / +0x1e (labor_shortage / garrison_quota); only the 0a60
  garrison admission (raw 87699-87753) and the 'G' marks do.
- 0688's starvation mercy roll (raw 57638-57645) runs only when DS:0x8e5a is non-zero (a
  non-human colony's shortfall below 3 is zeroed first) and is preceded by a reseed
  (FUN_281f_04ca(DS:0x83a6)). The port rolled it every turn from 1520 on Discoverer/Explorer.
- FUN_5fef_0f14 (raid picker) reseeds the same way before its walls roll (raw 99773).
  `dos_rng_reseed_83a6` is the shared helper; `ai_nation_reseed` records the word.
- Save export keeps a Brave's goto bytes; DOS saves them as-is (the port wrote 0,0 for every
  native unit without a goto order).
- The 5952 ledger's horse gross includes 1f72's uncapped herd potential (raw 12679), so a
  breeding colony does not ask for horses (0306 arm 3).
- 021a's attack-intent local `[bp-0x6a]` is zeroed once per call (0x225), not per direction, so
  the stay tile skips its tech roll after any attacking direction.

## Fixed in the 2026-10-08 pass (DOSBox-traced, 1514→1535)

- LCR: FUN_465b_0000 rolls the rumour inside the move (raw 75763/75788-75791) — the AI landing
  (`ai_euro_unload_pax_at`) now does too — and FUN_65dd_0004 reseeds from DS:0x83a6 before its
  roll loop (raw 103462).
- 5b66 walks a bound treasure with FUN_479b_0972 (arrival on a 0x0b goal exhausts MP); the port's
  thin goal advance let it re-act and cash the same turn.
- 20e6 delivery sell takes slot 0 each time through FUN_1000_8cdc (stale bytes past the count).
- 4528 AI arm (OVL13 0x463c-0x4764, tail 0x4bdb): a computer Soldier/Dragoon/Artillery entering a
  village takes code 9, OR-ing diplo bit 4 into both rows before the attack; a Scout speaks with
  the chief, a Missionary runs its arm, a 0x1c/0x19 colonist-class unit lives among them, and
  every non-attack code returns 1 (MP exhausted, step abandoned). The port attacked with all of them.
- The empty-village phantom (FUN_478c_002c) has home byte 0xff: its removal never flags the
  village's needs-colonist bit. 20e6's attack-term probe (FUN_5fef_1b0e probe mode) spawns that
  phantom too, so an empty village is no longer scored against defence 0.
- 022e: the beg-food block and the gift half need a colony on the encountered tile (local_4c,
  raw 96989); a Brave meeting a bare unit gives nothing.
- dos_tail carries the raw +0x0c..+0x15 block, so a unit created in a reused DOS slot inherits
  the bytes 06b4 leaves alone; the 5952 improve phantom (478c_002c) skips 06b4's pioneer 100.
- 5952 improve: plots are scored in DS:0xc8/0xde order (N, E, S, W, NW, NE, SE, SW), first tie
  wins (raw 94466).
- 20e6 pre-4d2e gate (raw 90210-90219): only a 0x0b unit on its goal is exempt; a 0x0c unit on its
  step tile still needs FUN_281f_0984, else 20e6 exits and the walker writes facing −1. 5b66 skips
  20e6 only for 0x0b with MP spent (raw 90551).
- 1b0e: an Indian attacker that beats a colony's defender (pop > 1 or a real defender) is
  destroyed (local_6, FUN_1427_0f30, raw 100390-100395/100753). The native mount/arm gear step
  applies only to a native ATTACKER (raw 100731) and exhausts it after the type change (raw 100736);
  the Brave step re-exhausts by the current type. The militia path draws DOS's colonist pick
  rand(0, pop−1) (raw 100419). A burned colony gives its tribe +1 horse herd / +1 musket lot when
  it held horses / muskets (raw 100700-100706).
- Removed `ai_diplo_euro_balance`'s "10ec war eligibility" arm: invented bands and a rand(1, 20)
  DOS never draws (Spain's plan stage, 1534→1535).
- Magellan: DOS reads the allotment live (FUN_1427_065a), so the elect turn already moves the
  nation's ships one tile further; the port adds the +1 to remaining MP at election.
- A native win over an undefended colony uses 1b0e's militia colonist pick (`local_b0`) in the
  0d04 removal, including the colonist shift and plot-seat renumbering. The loss runs before the
  winning Brave is removed when population remains above one (raw 100419/100680-100692).
- Save capture links sibling AI hulls on the same Europe sentinel tile after forming each hull's
  passenger chain (`FUN_1427_02ca` / `005c`). This restores the 42→4→41 chain in 1533→1534.
- 1516→1517 is now byte-identical. A DOS from-load watchpoint on `DS:0x360d` found the AI's
  throwaway Pioneer tool debit: `FUN_479b_0158` wraps +0x3159 from 0 to 236. The next Brave
  inherits that byte after the phantom is removed. The port now preserves the wrapped byte in
  both the plot-improvement and road-connection phantom paths.
- 1534→1535 Montreal's depletion counter now matches. `FUN_3844_00f2` walks colonies in
  descending save order and `FUN_364b_0688` checks each colony's Father election before its
  production. The port had elected for both French colonies first, consuming five RNG draws
  before Montreal. A DOS trace at `364b:1aa0` found both depletion rolls start from seed 100.
- New Amsterdam's Fisherman placement and worked-map bit now match. A trace at `15eb:2d14`
  found score 740; `FUN_15eb_1f72` writes the horse-breeding shortfall directly to
  `DS:0x8e5a[8]`, supplying the missing multiplier in `FUN_15eb_28c8`.
- Nation 6's 1534→1535 horse breeding now matches DOS (5): the Indian census leaves
  DS:0x8d4e bound to the last settlement's nation, so later nation turns breed that herd
  while using each current nation's population cap.
- 1535→1536 now matches the two French hulls' map positions, orders, plans and the
  nearby Soldier's Veteran promotion. A watchpoint on hull slot 40 showed DS:0x9456
  and DS:0x945a for nation 2 both stay zero through its move, even though 5d04 bought
  a dockside land unit. `FUN_4962_0018` takes the dock count before that purchase;
  the port now holds that count during the AI dispatcher. On the shared arrival lane,
  `064e` takes the chain-terminal hull (slot 40) first; its `10be` pickup takes the
  sentried passenger from slot 4's hull, matching the DOS watchpoint at `0c4b:02ec`.

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

## Open leads (most transitions first)

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
