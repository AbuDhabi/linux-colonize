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
traps. One-hit traces (first 5952 call per colony, first French act) are reliable.

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

State 2026-10-15 (continuous from START_SAVE): save 1 35 field diffs, all French units match;
the rest is Spain. Save 2: 77, save 5: 822, save 20: 2027. The human
side of save 1 matches.

## Open

- **START_SAVE -> 000001 Spain** (35 diffs left). Ship 92 wander, colony 2 build
  (`building_in_production` 255 vs 42), colony 13 / French colony 19 jobs, Spain bells 132 vs 138,
  `treaty_timer[3]` 1 vs 4. On the resumed slot DOS calls 20e6 for no Spanish unit (all spent).
- **Colony 8 (40,13) Scout.** DOS ends with a Seasoned Scout (profession 22) on the tile and
  slot 2 working job 13; the port absorbs the Scout and re-ejects the same Free Colonist
  (profession 28). DOSBox: at the first Spanish 5952 call for this colony the Scout is already a
  colonist (pop 12, slot 11 job 13, profession 28) and horses are 172, so something before 5952
  (Spain's resumed-slot EOT?) absorbed it. Next: trap the second tick (the trace above crashed).
- **French galleon unload (000001 -> 000002; seen before the fixes above, recheck).** In DOS a French hull drops its Dragoons at (3,55)
  (they save sentried, spent 12) and sails off with plan `'4'` (0x34 colony sail) toward (22,49).
  The port's hull, placed at (1,53) from lane 229, wanders on 4d2e without unloading.
- France and Spain each buy an extra Privateer in the port (Spain gold 0 vs 1908).
- Spain: Jamestown `building_in_production` 255 vs 42; Roanoke and St. Eustatius tile jobs differ.
- Wartime bells still accrue up front (`turn_run_nation_ticks`), not per colony.
