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

State 2026-10-14 (continuous from START_SAVE): save 1 105 field diffs, all French/Spanish AI unit
choices plus colony jobs. Save 2: 492, save 5: 860, save 20: 2036. The human side of save 1
matches.

## Open

- **START_SAVE -> 000001 AI tail.** French units around St. Eustatius (10,56) (units 114,
  140-142), Spanish galleon 76 (DOS goto (38,50), port (15,16)), Spanish scouts 162/164 (DOS plan
  'N' orders 0, port wander), Spanish colonies 2/8/13 jobs and Jamestown's build. Trace the
  first French unit to diverge (highest index acts first).
- **French galleon unload (000001 -> 000002; seen before the fixes above, recheck).** In DOS a French hull drops its Dragoons at (3,55)
  (they save sentried, spent 12) and sails off with plan `'4'` (0x34 colony sail) toward (22,49).
  The port's hull, placed at (1,53) from lane 229, wanders on 4d2e without unloading.
- France and Spain each buy an extra Privateer in the port (Spain gold 0 vs 1908).
- Spain: Jamestown `building_in_production` 255 vs 42; Roanoke and St. Eustatius tile jobs differ.
- Wartime bells still accrue up front (`turn_run_nation_ticks`), not per colony.
