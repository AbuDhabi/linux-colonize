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

State 2026-10-15 (continuous from START_SAVE): save 1 10 field diffs, save 2: 45, save 3: 98,
save 4: 204, save 5: 811, save 10: 1490, save 20: 1933. Save 3 rose from 60 when the dock
colonist was deleted: DOS's new Convert (below) now has no port counterpart, so indices shift.

## Open

- **START_SAVE -> 000001** (10 diffs). Roanoke (13) and St. Eustatius (19, French) 5952 job
  placement (28c8): DOS keeps St. Eustatius' slot 3 as Statesman (the port moves it to fishing,
  which also costs France 3 bells in save 2); Roanoke swaps trapper/lumberjack. `viewport_y`.
- **A Convert appears at (38,54) each turn from 000002 on.** Dutch, profession 27, origin 17,
  on the tile of the Dutch Veteran Soldier/Continental next to an Arawak Brave at (39,53).
  DOSBox (BPM on the unit count DS:0x539c, from 000003): created by FUN_1427_06b4 while
  DS:0x5394 = 0 (the crown's slot), just before the REF wave. Neither port creation site
  (022e mission visit, 1b0e subjugation join) fires; tribe 17 has no mission. Find the caller.
- **Continental Army muster picks different units** (types 7/4 swapped at indices 60/73/83 vs
  76/79/84 in save 3); the census `unit_type_counts` is also taken at a different time.
- **Second REF wave (000003 -> 000004)** is missing in the port.
- **REF unit vis bits.** The crown's landed units save vis 8 (target only) in DOS; the port adds
  the crown's own bit (and France's on one unit).
- **French galleon passengers on lane arrival (000001 -> 000002).** Lane 229 chain 75, 74, 147,
  151, 158, 159, 58, 90, 82, 109. DOS ends with galleon 109 carrying the six Dragoons and galleon
  58 carrying Artillery 147 + Colonist 151, so 58 colony-sails ('4') to (22,49). The port gives
  58 two Dragoons; it wanders. The 10be walk order/predicate needs a trace (BPM on the units' +8
  order byte).
- France and Spain each buy an extra Privateer in the port (Spain gold 0 vs 1908); recheck.
- Wartime bells still accrue up front (`turn_run_nation_ticks`), not per colony.
