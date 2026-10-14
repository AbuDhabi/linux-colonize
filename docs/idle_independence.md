# Independence idle campaign (`golden_idle_independence`)

STATUS: alignment in progress (started 2026-10-14). CTest `golden_idle_independence` is
DISABLED. Sister campaign (done): [idle_campaign.md](idle_campaign.md), whose method and
DOSBox recipe apply here too.

## Fixtures

`original_saves/independence-seed-100/` (readme.md there): VR_SEED.EXE, seed 100, Dutch
human (slot 3, `head.human_player = 3`). England (slot 0) is withdrawn (`control 2`).

| Save | Turn | Date | Note |
|---|---|---|---|
| `START_SAVE.SAV` | 268 | Spring 1680 | Manual save the player loaded (= `COLONIZE/COLONY08.SAV`, Aug 15). Harness save 0. |
| `000001`..`000019` | 269..287 | Autumn 1680..Autumn 1689 | Every-turn autosaves (copies of COLONY09). |
| `000020` | 289 | Autumn 1690 | Turn 288 has no save; the harness plays through it. |

The player declares independence during turn 270 (between saves 2 and 3: GAME menu, DECLARE
INDEPENDENCE, second option). Otherwise the player presses Space: popups take the first option,
units are skipped.

Harness: `golden_idle_campaign independence FIRST LAST` (save numbers 0..20), see
[tests/README.md](../tests/README.md). Before simulating toward save 3 it calls
`ai_king_menu_declare_independence`, which confirms on its own when there is no popup queue.

## Quirks of this save set

- **START_SAVE to 000001 is not what a load of START_SAVE gives.** A DOSBox load of START_SAVE,
  pressing Space until the autosave, runs one colony-production tick per nation, the same as the
  port. The archived 000001 has two ticks for Spain and the Dutch and one for France (Quebec
  hammers 44 -> 50 in DOSBox, 56 in the archive). The player's session did something between the
  load and 000001 that a plain load doesn't. Unknown so far.
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

State after these: from 000001, the human side of 000002 matches DOS exactly.

## Open

- **French galleon unload (000001 -> 000002).** In DOS a French hull drops its Dragoons at (3,55)
  (they save sentried, spent 12) and sails off with plan `'4'` (0x34 colony sail) toward (22,49).
  The port's hull, placed at (1,53) from lane 229, wanders on 4d2e without unloading.
- France and Spain each buy an extra Privateer in the port (Spain gold 0 vs 1908).
- Spain: Jamestown `building_in_production` 255 vs 42; Roanoke and St. Eustatius tile jobs differ.
- The port-only spin guard in `ai_euro_dispatcher_turn_unit_waves` leaves a no-progress unit's MP
  unspent. DOS spins it to the 0x14 sticky clear (0934 exhausts it). After the waves, stalled units
  are now exhausted. Exhausting them inside the loop broke the gates.
- Wartime bells still accrue up front (`turn_run_nation_ticks`), not per colony.
