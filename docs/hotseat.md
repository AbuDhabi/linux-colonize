# Hot-seat multiplayer

DOS Colonization ships a hot-seat mode that the game never advertises: no menu
row and no manual entry. The port carries it for save interop. A DOS
hot-seat save must load, play and save back in the port as hot-seat.

## DOS behaviour (static read)

- **Entry.** Env `COLONIZE=MULTI` (`FUN_7421_025a`, ND 0x70EDD-0x70EFB,
  case-folded whole-string compare) or an argv switch cluster with `K`
  (`FUN_7421_0054`, ND 0x70D0E: `-K`, `/k`, `-DK`) sets DS:0x201e. The nation
  picker (`FUN_733a_0c2a` raw 118727-118731) then hit-tests an undrawn 5th
  cell (x 112, y 195, 0x58 x 0x52), which returns 4. `FUN_75c2_10ae` resets
  DS:0x5398 to 0, so the leader name typed next goes into **England's**
  record, and after lore B opens the `@MULTI` checklist: four rows, the
  @NATIONALITY adjectives, all unticked; Space or a click toggles; Enter, Esc
  or a click away closes, and every exit keeps the ticks. Every ticked power
  gets control byte 0, DS:0x5398 = the **last** ticked power, and none ticked
  means England alone. Two or more ticked sets DS:0x5381 |= 0x80.
  `FUN_75c2_235c` gives every control-0 power the human start (gold, the
  veteran soldier at low difficulty).
- **Year loop** (`FUN_130d_0290` raw 6366-6420). For each slot 0..3, DS:0x5394
  = slot. A control-0 slot with bit 0x80 set does the following:
  1. blanks the screen (FUN_1b8d_0004 + FUN_1b70_003a(200, 0x140, 0));
  2. sets STRING0 to the power's name (FUN_15b3_024e);
  3. shows `@MULTINEXT` (FUN_281f_0652(0x137, 2));
  4. sets DS:0x5396 = slot, unless DS:0x53a4 is set;
  5. moves the cursor and camera to the power's colony nearest the map
     centre (FUN_15eb_0142(w>>1, h>>1, slot, -1));
  6. redraws.

  The slot's 00f2 EOT then runs, followed by the human arm: 0x5396 = slot,
  autosave, 0668 (mercenaries) and Move Pieces. **DS:0x5398 never moves.**
- **DS:0x5398 readers stay on the fixed power.** These are the King's per-nation
  beat (`FUN_43f7_2424`: tax, rebel-sentiment messages), the year-end
  win/loss checks (`FUN_3844_0442`) and the score (`FUN_41f2_0092`), among
  others. In hot-seat only that one power has a King. Paid peacetime
  mercenaries land for DS:0x5398 (`FUN_43f7_10f0` raw 74307) whoever paid.
- **Declaring** (`FUN_43f7_2564` raw 75217-75245) tests DS:0x53d0, the fixed
  power's cached SoL. With bit 0x80, `@MULTIREV` (0x138e) asks first. "Yes"
  clears the bit, sets DS:0x5398 = DS:0x5394, and goes on to `@DECLARE`. The
  other powers' control bytes stay 0 until the declaration itself:
  `FUN_43f7_1a26` withdraws every power except DS:0x5398 and the crown
  (`FUN_43f7_0108`: control 2, units deleted), the other humans included.
- **War of Succession** (`FUN_43f7_0218` raw 73622) does not run while the bit
  is set.
- **Cheat 0x68** (raw 56555) with the bit set: rebels go straight to 75
  (`ai_king_cheat_advance_revolution`).
- **Load** resumes at slot DS:0x5398 (`0x829 && 0x5398 <= slot`), not at the
  power that saved.

## Port model

| Port | DOS | Meaning |
|------|-----|---------|
| `human_nation` (game / ctx / world) | DS:0x5394 in a human slot, = DS:0x5396 | the power at the controls; during AI and Indian slots, the last one |
| `col1_fixed_human(col1, human_nation)` | DS:0x5398 | King, score, year-end |
| `col1_nation_is_human(col1, n, human_nation)` | control byte == 0 | is power n human |
| `col1_save_hotseat(col1)` | DS:0x5381 & 0x80 | hot-seat game |

Outside hot-seat both helpers reduce to `human_nation`. Single-player
behaviour, every golden and every fixture with a zeroed control table are
therefore unchanged by construction.

The port keeps one human's Europe state (purse, prices, dock, harbor, lanes,
crosses) in the `EuropeScreen` mirror. A handoff therefore rebinds the game the
way CHEAT Set Human Player does: capture into the live save, set DS:0x5394 /
0x5396, then reload in place. Every other power's Europe state lives in its
`col1->nation[]` record, as an AI power's does.

Where it lives:
- entry: `main.c` / `play_driver` and `new_game.c` (`NEW_GAME_PHASE_MULTI`);
- world setup: `ai.c` (`AiNewGameParams.human_mask`);
- the first mover: `game_commit_new_campaign`;
- the slot walk: `turn.c` (`turn_slot_is_other_human`, `turn_enter_slot`, `proc.handoff_*`);
- the rebind and `@MULTINEXT`: `game_hotseat_begin_turn` (`game_loop_saveload.c`), with the screen blank in `game_loop.c`;
- `@MULTIREV`: `ai_king_audience.c`.

Tests:
- `unit_turn_core` `hotseat_slot_order`;
- `unit_new_game` `wizard_hotseat_multi`;
- `unit_col1_save` (hot-seat capture);
- `unit_ai_king_core` `declare_latches_and_diplo` (through `@MULTIREV`);
- `regression_replay` `hotseat_handoff`, the England → France → Spain handoff on the real game loop;
- `unit_stage_seams` `test_hotseat_dos_save_round_trip` and the `unit_col1_save` fixture list,
  on the DOS-written save below.

**DOS interop golden** (DOSBox-X, 2026-10-26): `original_saves/hotseat/hotseat_1522.sav`
(readme and screenshots there). England (at the controls) and France (DS:0x5398) are
human. It was made by poking France's control byte, bit 0x80 and DS:0x5398 into a
1492-1600 autosave, letting DOS play one round with `@MULTINEXT`, and saving from DOS's
GAME menu. The port loads it at France with `@MULTINEXT`, pixel-identical to DOS's box.
Its re-save keeps the control table, DS:0x5398 and bit 0x80, and DOS loads that re-save
back as hot-seat at France. A DOS load shows `@MULTINEXT` for the resumed power too.

The port follows DOS on the load resume (DS:0x5398, whoever saved). It also
follows DOS on where paid mercenaries land and on whose SoL gates the
declaration.

## Divergences

- **`@MULTIREV` "Yes", then `@DECLARE` "Never".** DOS leaves the other humans
  at control 0 with the bit clear, so they go on moving without the handoff
  screen. The port hands them to the AI (control 1), as `@MULTIREV`'s text
  promises (`ai_king_multirev_accept`). On a confirmed declaration both end
  the same, with every other power withdrawn.
- **War of Succession inside that declaration.** DOS `FUN_43f7_0218` skips
  control-0 powers. The port's succession skips only the power at the
  controls, and the former humans are control 1 by then.
