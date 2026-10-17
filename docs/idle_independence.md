# Independence idle campaign (`golden_idle_independence`)

STATUS: alignment in progress (started 2026-10-14). CTest `golden_idle_independence` is
DISABLED. Sister campaign (done): [idle_campaign.md](idle_campaign.md), whose method and
DOSBox recipe apply here too.

## Fixtures

`original_saves/independence-seed-100/` (readme.md there): VR_SEED.EXE, seed 100, Dutch
human (slot 3, `head.human_player = 3`). England (slot 0) is withdrawn (`control 2`).

| Save | Turn | Date | Note |
|---|---|---|---|
| `PRE_LOAD_FRENCH_1599.SAV` | | Spring 1599 | French game that was running when START_SAVE was loaded in-game (quirk below). Reference only. |
| `START_SAVE.SAV` | 268 | Spring 1680 | Manual save the player loaded in-game (= `COLONIZE/COLONY08.SAV`, Aug 15). Harness save 0. |
| `000001`..`000019` | 269..287 | Autumn 1680..Autumn 1689 | Every-turn autosaves (copies of COLONY09). |
| `000020` | 289 | Autumn 1690 | Turn 288 has no save; the harness plays through it. |

The player declares independence during turn 270 (between saves 2 and 3: GAME menu, DECLARE
INDEPENDENCE, second option). Otherwise the player presses Space: popups take the first option,
units are skipped.

Harness: `golden_idle_campaign independence FIRST LAST` (save numbers 0..20), see
[tests/README.md](../tests/README.md). Before simulating toward save 3 it calls
`ai_king_menu_declare_independence`, which confirms on its own when there is no popup queue.

## Quirks of this save set

- **START_SAVE was loaded from the in-game menu of a running French game.** The game was
  `PRE_LOAD_FRENCH_1599.SAV` (= the user's `COLONIZE/COLONY04.SAV`, French human, slot 1). The
  year loop FUN_130d_0290 clears its load-resume byte DS:0x829 at the end of each slot iteration
  but keeps its slot counter, so the loop finishes the French game's day from slot 2. It runs
  Spain's EOT and act at full strength, then the Dutch EOT and the human turn, with no day top
  and no Indian pass. Only then does the calendar step and the normal day follow. So Spain and the
  Dutch get two production ticks before 000001 and France gets one. DOSBox replay (load
  COLONY04 from the main menu, then GAME > Load Game > slot 8 at once, then Space) gives a
  000001 byte-identical to the archive (2026-10-14). A plain main-menu load of START_SAVE does
  not (one tick per nation). Harness: `IdleCampaign.resume_slot = 2` ->
  `golden_turn_from_slot` -> `turn_end_from_slot`.
  - On the resumed slot Spain's units still carry their day-268 spend (no day top), so they
    barely act.
  - Runtime tables the French game left in memory (rank table, stance table, AI goals) could
    leak into that partial day. Not seen so far.
- **From 000001 on, the archive is DOS-repeatable.** A DOSBox load of 000001 plus one idle turn is
  byte-identical to 000002 (2026-10-14). So from-load state is enough there, unlike the 1492
  campaign.
- **From 000004 on, load replays diverge.** 0a60's work-queue military arm reads the stance table
  DS:0x9870 (-0x6790, nation*16 + continent) left by the previous turn's 0a60; it is not in the
  save, so a fresh load reads zeros. Continuous DOSBox 000003 -> 000005 (load 000003, two idle
  turns) gives a save 5 equal to the archive except crown gold (761 vs 797); a load of 000004 plus
  one turn does not (its new Man-O-War takes Bahia's work slot and sails south instead of
  attacking the Dutch Galleon). Trace from save 3 (or earlier) and run through.
- **Brewster pick at the first turn.** The Dutch cross their crosses threshold (354 >= 316) in the
  START_SAVE turn. With Brewster, DOS shows @RECRUITCHOOSE (the player recalls Free Colonist,
  a specialist, Free Colonist). Both the archive and the DOSBox replay recruit a Free Colonist
  (first option).
- **Human slot 3 exposes AI spent MP.** In the 1492 campaign the human was slot 0, so every AI
  slot acted before the day-top MP clear, and AI spent bytes (+0x3149) were always zero at the
  autosave. Here France and Spain act after the clear, so their spent bytes are compared for the
  first time. Port MP bookkeeping that was invisible before shows up here.

## DOSBox replays

Replay the archive's own chain: `setup --save original_saves/independence-seed-100/
PRE_LOAD_FRENCH_1599.SAV` (COLONY08 is START_SAVE in the copied `COLONIZE/`), `load_slot(0)`,
then Alt-G (scan 0x22), Down x5, Enter (Load Game), Down x8, Enter, and Space every ~40 polls.
For overlay traps add `[dos] ems=false xms=false umb=false` to `dbx.conf`. Without it the trapped
overlay is re-cached and later calls are missed. OVL14 (20e6/0a60): patch 0x4e2d6 (20e6) and
0x4cc50 (0a60), original byte 0xc8. The 4d2e wander score is final at CS:5805 ([bp-0x4e] dir,
[bp-0x26] score). France's primary goal table is DS:0x98b0 + nation*0x100 (64 x {x, y, code,
prio}). Unit indices shift mid-turn (compaction), so identify units by xy and type.

Overlay-trap traps (2026-10-15): the patched overlay can be reloaded at a CS the script has already
seen. Handle every stop at `off+1` with `trap_overlay`, whatever the CS. A script that skipped
seen CSes let the INT 3 run on: the game silently corrupted (garbage gold, COLONY08 rewritten
in the scratch copy, no further traps). A run with OVL15 (5952, `--patch-cc 0x53b7e`) died on
the second reload with RTLink "Fatal error evm012 ... vectored call to page 0005h"; breakpoints
left on the old CS are the likely cause. Delete them (`BPDEL *`, then re-set) when a new CS
traps. One-hit traces (first 5952 call per colony, first French act) are reliable. The
in-game load's final Enter needs some `step()` polls to take effect, and the resumed Spanish slot
runs during them, so arm `BPINT 3` before that Enter or the patched INT 3 runs unhandled.

Prefer data breakpoints for this chain (no code patch, no overlay trouble): game DS is 0x237e,
colony record i is at DS:0x5d46 + i*0xca, unit i at DS:0x3144 + i*0x1c. Set `BPM` on the bytes of
interest (pop +0x1f, food +0x9a, horses +0xaa, flags +0x1b, build +0x94, rebel pair +0xc2/+0xc6)
before pressing the final Enter. Each change then stops with the writer's CS:IP, and `SS:BP`
locals can be read at that stop. Keyboard polls stop at F000 or 130B. This gave clean per-tick
timelines for Plymouth and Jamestown.

2->3 replays from load too: `load_slot(0)` on 000002, then Alt-G (`keys((0,0x22))`), Down x6
(DECLARE INDEPENDENCE), Enter, Down, Enter, then Space as usual. The autosave is byte-identical to
000003. RNG: `BP 1541:0E04` (rand) stops on every draw; the state dword is at 0x28ee in the
routine's DS, and the range wrapper's `(lo, hi)` sit at stack words 7 and 8. Arm it only for the
slot of interest (`BPM` on DS:0x5394) and compare with the port's `DOS_RNG_TRACE`.

Unit creation: `BPM` on the unit count DS:0x539c stops inside FUN_1427_06b4 (CS 0c4b) before the
new record is written, so its bytes are a deleted unit's leftovers. Read the record at the next
keyboard poll, and expect later deletes to shift indices. The caller's frame is `[BP]` of the
stop; overlay returns show as the RTLink stub 1931:1554, so identify the caller by its args and
locals (Ghidra `local_N` sits at BP-(N-2)). Walk BP with `d.word(..., signed=False)`.

`python3 tools/dosbox_trace.py` needs `/usr/bin/python3` (the conda `python3` on PATH has no
`pexpect`). `load_slot` works on this save list since the 2026-10-14 `list_row` fix (the list
header sits a few pixels off, and long save names cover the old highlight sample). Loop: Space
every 200 polls until `COLONY09.SAV` changes, then copy it.

## Port fixes so far (2026-10-14)

- Headless auto-declare removed (`ai_king_try_declare`). The port declared independence for the
  human at SoL >= 50 whenever no popup queue was attached, so every headless run declared on its
  first turn. DOS FUN_43f7_2564 is reached only from the menu.
- Headless Brewster: with no popup queue the 4884 pick resolves in place with pool slot 0
  (`units_brewster_apply_slot_w`). Before, the pick was silently dropped.
- Headless tax audience accepts (the first option). An invented SoL/bells tea-party heuristic was
  removed.
- The human's tax roll and @KINGNEWWAR (`ai_king_human_5e52_tail`) run in the 5e52 tail before the
  colony loop. They used to run in the KING step after production, so Custom House sales were
  priced at the old rate.
- The human's bells accrue and the FF election is tested per colony (`turn_run_human_colony_eot`,
  0a22 inside 364b_0688), like the AI. After an early-colony election, the later colonies refill
  the pool and the next nominee is picked in the same turn. Wartime still takes the old path.
- Headless human FF nomination takes the first category with a candidate (the debate's first row),
  not the AI's strongest category.
- Save bridge: removed the "station-keep tip" export (AI transport at its goto exported spent 0).
  DOS writes the real spent.

- The per-nation MP refresh (`turn_refresh_moves_for_nation_w`) no longer refills MP. Only the
  day top does (raw 6355-6357), which matters on the resumed slot. A new dock immigrant now
  starts with full MP (`europe_spawn_dock_mirror_unit`, spent byte 0). The refresh used to mask
  that (year_1496).
- `turn_end_from_slot` / `golden_turn_from_slot`: the rest of a DOS day from a given Euro slot.

- 0a60 foreign-ship CONTACT goal: gated on the foreign unit's own visibility nibble
  (`(0x10 << nation) & +0x3147`, port `col1_vis_mask`), not the map seen layer. Before this,
  France posted a CONTACT goal on the Dutch privateer at (53,56). Its arriving privateer bound
  to that goal, sailed next to the Dutch frigate, attacked and sank. DOSBox goal-table dump:
  France has no CONTACT goal there.
- Deleted the invented wartime free-Privateer spawn (`ai_diplo_war_privateer_spawn`, manual/
  fandom sourced) and its null-units 8-gold "prize" stand-in.
- Human Fortify (5) -> Fortified (6) happens when the unit comes up in the rotation
  (3ae6 -> FUN_479b_0b6c, `game_select_next_unit_awaiting_orders`), not overnight in the MP
  refresh. A View-mode player never cycles, so Fortify stays 5.
- Save bridge: goto bytes export as stored. The "idle transport goto == xy" rewrite is gone.
- 6d8e dispatcher: units with x >= 128 (Europe lanes) are never acted (FUN_1427_13b0 signed-x
  test). AI passengers on a map tile end the nation's act exhausted (13b0 has no aboard test;
  DOS spins them to the sticky clear). Units that make no progress are exhausted after the waves.
- 465b overspend: European runtime MP may go negative. The save exports spent = max - moves, so
  spent > max survives (Dragoons at 15/18).

- 6d8e wave scan acts AI passengers on a map tile in place (2026-10-15). 13b0 has no aboard
  test, so a sentried Dragoon in a hold takes a wave slot: its act only spends MP, but it ends
  the wave like any act. DOSBox 20e6 order for France: Artillery 137 (wave 0), passenger 144
  (wave 1), then Artillery 114 (wave 0), which takes Port Royal's one-shot surplus recall
  (bit 0x04, quota 3). Before this, Dragoon 142 got there first.
- LAB_5a78 tail end (OVL14 0x5b13): any unit whose act state is 0x0b at its own goal tile is
  exhausted (FUN_281f_0934), not only hulls. 5b66's 479b walk then clears the order to 0.
- The 20e6 land "re-bind own goal" path keeps the act state. A 0x0c unit on its step tile stays
  0x0c, and 479b clears it to 0. The port used to rewrite it to 0x0b.

- 20e6 'N' arm (OVL14 0x4a0f-0x4a4e): a Scout or Pioneer on a stance-0 continent whose nearest
  own colony is on the same continent walks home with plan 'N'. Fixed the Spanish Scouts and,
  through the RNG, galleon 76.

- 5952 "improve" election (ARM 1, asm 0x274b) walks `pop + DS:0x8d72`, so units on the colony
  tile are candidates (profession = unit byte, occupation = type default job). Plymouth's
  ejected Scout is elected and leaves as a Seasoned Scout.
- Idle human rotation in the harness (`turn_idle_human_rotation_w`): each turn the idle player's
  units come up; Fortify becomes Fortified (3ae6 -> 0b6c) and Space spends the rest. Save 1 still
  shows 5 because the autosave precedes that turn's rotation.
- Horse breeding reads the Phase A gross food (DS:0x8dc8), without the AI food subsidy
  (`difficulty>>1`, Phase B). Plymouth: 8dc8 = 46, food +47 - 24 - 11.
- AI colonies have no Tory production penalty: FUN_15eb_1d4c 15eb:1dbf-1dd7 zeroes the Tory count
  for an AI nation, like FUN_15eb_18ec does for fields. Plymouth (pop 12, 17% SoL) makes 19 bells
  with three Elder Statesmen in DOS (13 in the old port).
- 5952 build cascade: the on-tile Artillery count (asm 2632) walks the stack at BP-0xa4/-0xac,
  which the improvement arm leaves on its best plot (asm 13c3) or on the last scanned ring plot.
  It counts any owner's Artillery there. Jamestown's walk lands on (50,22), counts 0 and builds
  Artillery (project 42) instead of clearing the build slot.

- Colonist removal moves only the rebel divisor (raw 10279); the port's dividend clamp is gone
  (Jamestown saves 1479/1439).
- 153e audience: the closed-gate exit (LAB_34de) still re-arms the treaty cooldown at peace;
  headless, the idle player answers each audience popup with its first option; the 3180 AI
  limb fires only after a real step (465b), not for a unit parked beside the human.
- War of Independence: the headless declare runs the Congress Confirm path (declare + wave
  latch burn), so the REF lands at the crown slot right after the declaring turn. 0982's
  colony score uses 08bc mode 10 (1427:0e16: non-ship units with attack > 1), not a strength
  sum, and the disembark follows DOS's sweeps (seize least-stacked tiles with Regulars, then
  Cavalry/Artillery on crown-held tiles). Save 3's REF (Man-O-War, 4 Regulars, Cavalry,
  Artillery at Bahia) matches.

- The declaration deletes the human's off-map units (FUN_43f7_0188 over the unit pool and the
  Europe dock list, not only the harbor lanes), keeps `next_founding_father` (1a26 clears +0xc
  and +0x16 only), and latches the WoI hint one-shot (0x5382 bit 0x04) headless too.
- Harness: the port's WoI latches live in the human's DOS-dead `unknown23_pad`; `compare_save`
  copies DOS's bytes there before comparing.
- Harness: cursor and viewport coordinates are UI-only state.  The comparison now normalizes
  both pairs; the earlier first-save `viewport_y` mismatch was not simulation drift.
- Save bridge: crown-controlled map units retain the literal visibility mask written by 0982.
  Unlike ordinary Euro units, export does not synthesize their owner bit; the first landed REF
  wave therefore saves DOS's target-only mask (8 for this Dutch game).

- 022e rolls FUN_281f_04d4(0, 8) (arrival sound) when a Brave visits a human colony, right
  after the mood rolls and before the beg/gift/convert arms. The port skipped it, so the
  @INDIANSCONVERT roll (Jesuit mission: need 6) missed where DOS hit. DOSBox 3->4: Brave 21
  (village 27) visits Pernambuco (colony 17 at (38,54)) and 022e's frame holds local_6 = 6,
  local_4c = 17. The "Convert at (38,54)" in saves 3+ is that gift (the unit's +6 byte is the
  home colony, not a tribe). `golden_colony_prod01` drops the Arawak visitor at (41,57); its
  native stream is not reproduced and the extra draw turns it into a Tobacco gift.

- 0982's "crown troops already next to the target" discount tests the nation of a STALE
  `local_4c` (the 07e0 head of the last colony scored, then wherever the previous 02e4 walk
  stopped; unit -1 reads DS:0x312b = 0 = the crown here), not the neighbour's own head. The port
  had "fixed" that and discounted every crown unit, so wave 2 (3->4) landed 3 units instead of 6.
  It now matches DOS (pools 45/16/6/12, same six units on the same tiles).

- 022e's beg-food block (5bfb:0683, `rng(1,100)` when the village is short of food and the
  colony holds more than 0x4a food) has no `contact_state` latch; only the demand half
  (LAB_5bfb_0def) does. The port skipped it after an earlier gift that turn, so the next
  @INDIANSCONVERT roll took the wrong draw. With this and the arrival-sound draw, the Arawak
  turn of 2->3 makes the same 88 draws (values and ranges) as DOS, and Pernambuco gets its
  Convert.

- Continental mobilization (1eca) walks the colony tile chain from the latest arrival (07e0/02e4,
  new `units_tile_chain_next_id`), not slot order. 2022's non-crown branch (mobilization, 10f0
  drain, merc offer) runs from the human's own 00f2 right after its census (2424 is called for
  the human slot too, raw 58390/58392), not at the crown's beat; the census now counts the
  pre-mobilization types like DOS.

- At declaration (FUN_43f7_1a26) every seen byte drops the crown bit, then takes it wherever the
  human's bit is set. The port's declare now does the same.

- The 20e6 wagon village errand sets plan 0x34 and an AI goal (`ai_euro_set_goto` with the goal
  order), not a plain AI move. 5b66's head walks a deferred band unit that already holds a goal
  order (479b, one pathfinder step) without re-entering 20e6, and a wagon that just hauled walks
  its goal the same way.

- The 4393 work-queue gate has no hull prefilter: a Man-O-War passes on bVar7 (odd unit index, or
  the nation's only one), then takes only slots whose record +5 (military) is set.

- 022e's AI answer to @INDIANBEGFOOD (asm 0x5717e) is `nation == 2 ? refuse : give`. Spain always
  refuses; the port gave food for every AI nation (Roanoke lost 37 food in 4->5).

- FUN_1427_08ea (a unit becoming active) relinks it to the top of its tile. At the 2->3 turn
  start the Merchantman at (51,52) comes up above the Fortified Galleon, and the declaration
  then spends every unit, so save 3 keeps it there. The idle rotation now relinks each unit it
  brings up, and the harness activates the first unit before declaring.

- A naval loss is 0ec0: every unit on the loser tile, in chain order from the head, takes its
  own 0352 (the primary loser at its chain position). 0352 removes a sunk hull from the census
  at once (cargo total, type count, armed count; raw 99570-99579), so the Merchantman sinking
  first leaves the Dutch fleet short and forces the Galleon to survive damaged (4->5).

- 0982 creates landed REF units at (-2,-2) and slides them ashore with 0948 (relink only), so
  they carry only the target nation's vis bit. The port's slide re-stamped the whole tile stack
  with the move-vis reset.

- 2820 (village trade) reseeds the global stream (04ca, DS:0x83a6) and draws from it; the port
  used an invented per-nation stream.

- 2820's buy settle (OVL13 asm 0x343d-0x3462) passes `-rand(0, price/25+1)` to 0d6c: a
  purchase lowers the tribe's alarm by the roll. The decompile read it as `+price/25+1`
  (bugs.md #804's note is wrong). The buy price's bid term reads the 0x9e78 table after 0ed0
  sorted it in place, i.e. the bought cargo's own bid.

- A ship's attack is the 465b step into the foe's tile: the attacker is spent in full and a
  winning hull advances into the tile 0ec0 emptied. A damaged loser keeps its spent byte and
  takes the nation's return-from-Europe tile as its goto (raw 99618).

- 20e6's `local_a8` (stack_other) counts what 10be boards onto the hull (aboard units, then
  Sentry or water-tile units that fit), not every unit on the tile: two Man-O-Wars sharing a
  tile do not take the colony-sail pick.

- The wagon's village trade is the step onto the village: it faces the village and 465b's
  LAB_0bd1 exit clears its orders and bumps +0x315a.

With these, the crown's 4->5 slot makes the same 112 draws as a continuous DOSBox run.

- The music pick (FUN_129f_00f6) brackets its draws with FUN_19ef_002c = srand(BIOS tick &
  0x7fff) (asm 129f:0138 / 129f:0250) on the one game LCG; VR_SEED's tick reads DS:0x83a8 = 100.
  So a pick leaves the gameplay stream at srand(100) at a wall-clock moment (track end). Port:
  `dos_rng_tick_reseed` (called by the sound pump) queues that reseed for the next draw; an
  explicit reseed cancels it. Harness: `GOLDEN_IDLE_PICKS="5:334;6:12,90"` replays picks as draw
  indices from the start of that save's run (it prints `save N: D draws`). A brute-force search
  (one pick per position, saves 1-5, 2026-10-16) moved no save by more than 3 bytes, so the
  residues there are deterministic. Search again when a save's residue looks like a shifted
  draw stream.

- 28c8's forest -10 (raw 13119-13122) reads `local_32` ([bp-0x30]), written only by the
  food-emergency arm (asm 15eb:2ae7). Outside an emergency it is stack residue. DOSBox (BP at
  e0f:28c8 and e0f:2e9e, word SS:SP-0x32 at entry): inside 5952 it is 0x23ec after a 0c04 ledger
  refresh and 0x12 after a 0c36 set-job (178f unseat, indoor trial loop), so the -10 hits only
  then. Port: `s_28c8_l32`. This fixed Roanoke (Fur 6 beats Tobacco 4). The indoor arm's field
  commit is a fresh mode -1 call (not the -2 probe's result), and it stamps no owner (year_1567).
- The Euro rank table DS:0x917c is not saved. After the in-game load, DOS clears DS:0x829 at the
  end of the resumed slot, so the next day top refreshes it; `turn_end_from_slot` now clears the
  port's resume flag. France's St. Eustatius used zero ranks and lost its Statesman.

- 20e6's LAB_4d2e tail parks an armed hull (flag bit0) or land fighter (bit2) on its own goal
  tile when outside a colony and alone (plan 'B' / 'e', raw 88987-89009); the port had read
  these as pioneer codes. 6d8e drains a unit while its act changes its orders, so a step that
  only clears on arrival does not end the drain. 1b0e's probe on an undefended colony draws the
  militia pick and scores the phantom; beating a real defender standing in a colony never takes
  it (only beating the militia does). An AI wagon stepping onto a village trades (4528 code 1).

- 10be re-walks the tile (DOSBox BPM trace on the lane units, 2026-10-16): riders are not
  "aboard" in DOS, so the acting hull's 10be (both the departure and the 20e6 LAB_3558 call at
  asm 0x3609) boards land units in 04d6 order (hulls, then Treasure, then size 6..1, each in
  chain order, a hull followed by its riders nearest-first) into an empty hold, its own riders
  included. A shared open-water tile can therefore swap riders between hulls: at (1,53) French
  Galleon 109 kept four Dragoons, took 58's two and left its Artillery and Colonist to 58.
  `units_ship_departure_pickup` does this walk; the mover now calls it before the 040c relink.
- 20e6's explore-fatigue bump (+0x3154) tests iStack_14 (FUN_521d_0600), not iStack_6a; the
  labor-loop re-entry scans with iStack_6a forced on (`Ai20e6Unit.prio14`).
- FUN_3844_0442 (rival independence bands etc.) runs right after the calendar step, before the
  day top (raw 6459-6461), i.e. after the human's autosave; the port ran it in the KING step.

- The REF hull is created on the map (095c at x,y), and its 0948 -> 07a0 -> 13f1_02f8 reveal
  stamps the crown on the landing colony and its units (DOSBox BPM on Bahia's
  visible_to_euro[0]). The landed troops are created off-map and reveal nothing.
- After the declaration DOS sets the crown slot's `control` to 1 and withdraws the others, so the
  save export's "crown" test (skip the synthesized owner vis bit) now names the King's slot.
- 2154's silver bid divides by max(DS:0x962a[tribe], 1) (village count, as the horse bid),
  not the difficulty (DOSBox: San Juan's gift reads silver bid 20).

- FUN_5fef_1b0e's capture tail (OVL17 asm 0x2f17-0x2f43) toggles one row: if the captor's
  row toward the loser has WAR it is cleared, else the loser's row gains WAR (DOSBox BPM: the
  crown takes a Dutch colony and its 0x22 becomes 0x20). FUN_465b_0000 opens a war only out of
  a treaty bit (raw 75567-75593); an attack with no PEACE either way leaves the bytes alone.
- 3844_00f2's drydock tick (raw 58331-58344) counts +0x315a up the same day; the port's
  "skip the damage turn" grace tick was invented (bugs.md note) and is gone.

- 0a60's CONTACT lurk skip `(head + turn) % 4` adds the colony tile's head unit's record INDEX
  (`units_dos_index` of `units_tile_head_id_at`), not the port id of the first slot. This fixed
  the crown's goal table (DOSBox dumps of DS:0x98b0) and the Man-O-War binding.
- 2154's colony cover test FUN_281f_0302 = FUN_137f_000a, the 1-tile-inset interior test
  (crown wagon's Ore / Furs bids 12 / 4; purchase 335).
- 022e's refused beg halves its alarm add when the visit's calm roll bVar5 passed.
- During the war the human slot's 3844_00f2 `+0xe = 0` (raw 58382) lands on the crown's record
  (DS:0x84fc still points there): the crown's liberty_bells_last_turn reads 0 at every autosave.
- Wall-clock music picks in the archive: 5->6 needs reseeds before draws 256 and 353 (the crown
  wagon's 2820 and a Man-O-War's act), 6->7 one near 361 (searches with `pick2.py`-style
  brute force). The archive's 5 also carries +36 Tools gross (gold 797) that a DOSBox
  continuous replay does not (761): realisation-dependent, unexplained.

### DOSBox reference run (deterministic target past 000004)

The archive is one wall-clock realisation (music picks), and from 000005 on it carries effects a
DOSBox continuous replay does not reproduce (000005: crown +36 Tools gross; 000006: Bahia's
Blacksmith/Gunsmith pair). So the deterministic target is a recorded DOSBox run: load 000003,
idle with Space, and per day keep the autosave plus every music pick's draw index. BP
1541:0E04 counts draws; BP 0AC3:0138 / 0AC3:0250 (FUN_129f_00f6's two FUN_19ef_002c calls)
bracket a pick, whose own draws are not counted. The port with `GOLDEN_IDLE_PICKS` built from
those indices must equal each DOSBox autosave. Status 2026-10-16: equal through save 5
(picks 4:318,331; 5:121,255; both sides 368 draws in 4->5).

Status 2026-10-16 evening: equal through save 6 (381 draws both sides). A second realisation
(DOSBox continuous, picks 4:324,331; 5:137,254; 6:77,253,321) also matches through save 5 and
matches through save 6 too. A third (picks 4:330; 5:137,254; 6:101,269,314,341; 7:0,238,392)
matches DOS exactly through save 7. The first reference run still differs at save 7 (DOS 408
draws vs port 393; its recorded pick at 352 lands wrong), so a draw-traced run of a later
realisation is the way to find that event. Fixes that got here:

- Coastal battery FUN_364b_03f6 runs at the top of each colony's EOT (0688 raw 57227, thunk
  291f_09ce), in the owner's slot, not at turn setup: the Dutch fort fires (rand(1,386), stack
  word 0x0b7c = unit 105 * 0x1c) on the crown Man-O-War that parked beside it this turn.
- Ship-slow (FUN_5bfb_3180, viceroy_overlays.asm:151448/151576/151586) ADDs the drain to the
  spent byte: Fortress +0x32 keeps the overspend (the Man-O-War, max 15, saves spent 56).
- An AI stepping into an undefended foreign colony is 465b's bVar4 arm: the 07fe vis OR runs on
  the pre-combat chain, 1b0e advances by 0948 relink (no 13f1_02f8 reveal), then LAB_0bd1 clears
  orders and bumps +0x315a. A Brave's attack arm also takes the 07fe vis OR.
- 4962's ship probe walks the 11x11 box y-major and each tile chain (head not own nation); the
  order matters because 0906 reuses the shared flood grid when the goal key is unchanged.
- 20e6's attack pick held for want of MP (raw 89033-89039, `local_76 = 8`) keeps `local_ce`
  set, so the stay skips the `local_ce == 0` park arms (0x47 / 0x42 / 0x65 / 0x46) and writes
  '9'. This closed the second realisation's save-6 leftover.
- A beaten attacker keeps 1b0e's entry 0934 spent (its pre-combat allotment) through a
  demotion: a crown Cavalry beaten to Regulars saves spent 12. The 479b blocked-step "end the
  act" only clamps remaining MP to 0, so the overspend survives.
- Ship-slow branch A walks the neighbour tile's chain from the 07e0 head (newest first).
- The coastal battery is resolved by 1b0e on the temp attacker: x8 strengths, attack x3/2,
  human side minus (difficulty - 4) (Fortress + 1 Artillery vs Man-O-War: rand(1, 194 + 192)).
- FUN_364b_0636's Ore arm: an AI Custom House keeps Ore with an Armory or when the tick made
  tools / muskets (DS:0x8de4 / 0x8de6). Crown Bahia keeps its 100 Ore.
- The crown wagon's adjacent-village trade is 27f5's commit ('4', goal = village) then the 479b
  step, so plan '4' survives 0a60's '?' placeholder.
- Headless, the human's @INDIANBEGFOOD takes the first row (decline), like the idle DOS
  player's Space. The refusal's alarm word is FUN_4cc6_00f2(tribe, param_2 = the Indian
  nation) and spills into the next Indian record (tribe 2, index 6 = tribe 3's +4/+5; DOSBox
  BPM on DS:0x5bc4 caught the store at 4cc6:0170).
- Archive: 4->5 keeps a 2-byte crown-gold residue for every pick pair (not a pick effect; use
  GOLDEN_IDLE_RESYNC=5); 5->6 best picks stay 256,353 (12 bytes: Bahia's 14/15 jobs follow
  the resynced gold). Pick searches are cheap now (0.3 s a run; `pairsearch.py`, 8 threads).
- Loading an autosave in DOS does not reproduce the continuous run (save 5 loaded: 355 draws vs
  381), so traces of a given interval need a continuous run from 000003.

Status 2026-10-17: the tr8 realisation (picks in `dt16/out/tr8.txt`) matches DOS exactly
through save 10. Fixes for 9 -> 10 and 10 -> 11:

- FUN_5fef_0000's param_3 is the attacker's NATION, not the attacker: 1b0e (OVL17 asm
  0x1baa-0x1bc1) pushes the nation nibble for 07e0, keeps it on the stack and near-calls the
  picker with 07e0's unit. So 015e scores every candidate against unit RECORD #nation, and the
  artillery arm tests that record's nation nibble. Record 0 is a Brave here, so New Amsterdam's
  damaged Artillery (x2) outranks two fortified Cont. Cav. against an English attacker.
- The crown MoW sail-home despawned any crown hull standing on a High Seas tile; only an 'E'
  hull crosses (479b), and it crosses into the Europe lane (x = 244 + nation, voyage roll
  rand(1,100)) instead of leaving the unit array. The lane count (DS:0x9456) is 'E' hulls plus
  the 240/244 x-sentinels, not hulls that merely stand on High Seas. 007a writes no MP: the
  hull keeps its spent byte on the lane.
- A 20e6 ship attack still writes '9' and goes through 465b's LAB_0bd1 tail (orders 0,
  +0x315a + 1) when the fight removed no unit.
- 0352 captures a type-0 Colonist whatever its +0x3159 tools byte holds, and 15eb_1068 banks
  that byte only for the Pioneer role (a joining Colonist's residue stays out of the warehouse).
- 20e6's road term is `FUN_281f_0754(x, y) & 10`: a settlement tile (0x02) counts as a road
  (0x08) for the unit's tile and the destination. An English Scout beside Fort Nassau steps
  back onto the colony tile for the +1.
- The dispatcher's inner drain is the same 097a / 13b0 has-MP test, x >= 0 included: a hull
  that crossed into the Europe lane mid-drain is not acted again.
- A refused beg (`local_c == 1`) clears contact_state 2 for that European.
- Vis bits: 465b's `if (local_1c) FUN_281f_07d6(unit, DS:0x5396)` ORs the human's bit into
  the mover's whole origin chain when the human sees the move; 07d6 and 084e/0ce6 both go
  through FUN_1427_09ac, which walks the whole destination chain.
- An AI beating a colony's militia takes the town in the same step: 465b's LAB_01ce stop (AI
  with under 3 thirds at a foreign colony) runs BEFORE the bVar4 fight, and 1b0e's winner then
  advances whatever MP the fight left (tr9 6 -> 7: the crown Cavalry takes St. Louis).
- 022e is one (Brave, European) encounter: its beg and gift halves serve only that European
  (`ai_contact_set_visit_euro`). The port's beg loop also begged from a second European whose
  colony stood beside the same Brave (tr9 8 -> 9: an extra rand(1,100)).
- A crown hull already sailing home ('E', order 0x0b, MP spent) re-enters 20e6 only beside a
  foreigner, and 5b66 first takes it back out of DS:0x9456, so its own sail-home test passes
  again (re-course, no draws). The lane count never includes the acting hull.
- tr9 (dt19, a second continuous DOSBox run from 000003 that also logs the acting unit's record
  per draw) is the reference past tr8's save 11.
- DOSBox method for these: load the DOS autosave of interval K-1 (`setup --save`) and log
  each draw with the acting unit (DS:0x2d12) record. The RNG differs from the continuous run,
  but a unit's route and its facing / orders writes (BPM on the record) are usually the same.

State 2026-10-16 late (continuous from START_SAVE): 0 -> 1 -> 2 -> 3 -> 4 byte-identical; vs the
DOSBox continuous replay save 5 differs only in tribe 5's horse_herds (DOS 4cc6_00f2 called with
euro 8 spills into the next Indian record; tracing the caller). With picks 6:256,353 / 7:361:
saves 5-7: 3, 15, ~74 field diffs vs the archive.

## Open

- France and Spain each buy an extra Privateer in the port (Spain gold 0 vs 1908); recheck.
- Wartime bells still accrue up front (`turn_run_nation_ticks`), not per colony.
