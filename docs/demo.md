# Autoplay and the self-running demo

STATUS: ported 2026-10-27. Code: `src/core/game_loop_demo.c` (game side),
`turn.c` `turn_step_all_ai_anchor` (pipeline), `ai_euro.c` `ai_euro_set_watch`
(6d8e hook), `col1_save.h` `col1_all_ai_fixed_nation`.

Two DOS flags share one engine:

| DS | Name | Set by |
|----|------|--------|
| `0x826` | autoplay | argv `D` (ND 0x70D05); CHEAT Set Human → **None** → `@SETAUTO` Yes (ND 0x23D7D) |
| `0x828` | demo | argv `D` only (ND 0x70D00). `COLDEMO.BAT` runs `opening -d:0`; OPENING chains `viceroy -od` |
| `0x82b` | end of demo cycle | `J` at a 6d8e pass (asm 53254), 1725 or the WoI (raw 6488-6490) |

The other writer, `FUN_1262_02fe` raw 3087, sets 0x828 on Alt-X / Alt-Q and then
exits at once, so it is no entry point. The port takes `-D` (and COLDEMO's
`-d:0`, and OPENING's `-od`) as a switch cluster, like `-K` (`main.c`). In a
cluster, `C`, `M` and `P` eat the rest of the argument; `O` does not.

## No power at the controls

CHEAT "None" and the demo's New World set every Euro control byte to 1 and leave
DS:0x5398 alone (`game_set_all_ai`). In the port:

- `col1_all_ai_fixed_nation(save)` returns DS:0x5398 when no control byte is 0
  (outside hot-seat), else -1. `col1_fixed_human(save, -1)` returns it, so the
  King, the score and the year-end checks still follow that power.
- The game hands the sim `human_nation = -1` (`game_fill_turn_context`).
  `turn.c` keeps DS:0x5398's place in the pipeline. Its slot runs from FINISH
  as an AI slot (00f2 then 6d8e, raw 6400-6408), with the 2424 King arms, which
  test `param == 0x5398` and not the control byte (raw 75160). There is no merc
  offer and no Move Pieces.
- The Europe mirror stays bound to DS:0x5398 for display, but holds no units.
  `game_set_all_ai` flushes it and reloads in place, and the bridge leaves every
  Europe unit in the pool for the AI (`col1_bridge_mirror_nation`). Capture keeps
  the all-AI control table.
- No unit is selectable (`game_unit_selectable`,
  `game_select_next_unit_awaiting_orders`) and the goto pacer stays off, so each
  day ends in a view-only Move Pieces when autoplay is off (spectator,
  `@SETAUTO` No). A resumed save's selected unit would otherwise run its goto
  outside the AI.
- The combat layer's human (`units_set_combat_human_nation`) is -1, so the
  human-only combat dialogs stay quiet. `turn_refresh_moves_for_nation_w` and the
  in-place reload no longer fall back to DS:0x5398 for it.
- Nested blocking pumps (the combat popup pump, Combat Analysis) give up after
  the same 0x78 ticks under autoplay, and a window close in them quits.

## Autoplay (DS:0x826)

- Turns run back to back; no human input reaches the game (Move Pieces returns
  at once, raw 46879).
- For the view nation (DS:0x5396) or with the whole map shown (DS:0x53a2), 6d8e
  moves the cursor to an off-screen unit before its act and pauses 0x1e ticks
  after its last act (raw 93258-93311). This is `game_autoplay_watch`.
- A key wait or dialog gives up after 0x78 ticks (`FUN_1262_0060` raw
  2917-2920, `FUN_6f74_2580` raw 117068-117074). The port answers with Enter
  (Escape for a screen) after the same time.
- Outside the demo, a click stops autoplay at the end of the day (raw
  6427-6432).

## Demo (DS:0x828)

- Start (`FUN_75c2_2778` raw 121917-121929): `COLONY05.SAV` resumes if it reads
  and is older than 1700. Otherwise a New World starts with difficulty 2, nation
  `FUN_281f_04d4(0, 3)`, the default leader, and no King speech
  (`new_game_begin_demo`), and then every control byte turns 1.
- A key or click on the sail pages exits the program with nothing saved
  (`FUN_1262_02fe` raw 3085-3098).
- At the top of each 6d8e pass a waiting key is read: `J` ends the cycle, any
  other key (a click reads as Esc) saves slot 5 and exits (asm 53215-53266).
- After each day (raw 6467-6500), on every fourth turn:
  - turn % 12 == 0: splash tour. DS:0x150 counts up to 25 until
    `FUN_12fd_006c` shows one.
  - turn % 3 == 1: the DS:0x5398 colony nearest a random point (y drawn
    first, both `FUN_19ef_0032(2, size - 2)`).
  - turn % 3 == 2: DS:0x5398's Europe.

  Each view stays up 600 ticks; a key or click there saves slot 5 and exits.
- The cycle ends on the WoI, after 1725, or once 0x3840 ticks have run (about
  236 s). It saves slot 5 and chains `opening -od` (`FUN_130d_019e`). The port
  replays its OPENING and starts again.
- The tune pool's default arm is tunes 1-24 (`sound_set_demo`).

## Divergences

- The AI arm autosaves at DS:0x5398's slot before its 6d8e (raw 6403-6404). The
  port saves once the day's pipeline is idle.
- A 6d8e key that saves slot 5 does so mid-nation in DOS; the port saves after
  that nation's slice.
- DOS's 6d8e pacing spins the clock word. The port presents frames for the same
  time and counts the cycle clock from the demo start, not from the program
  start.
- The demo's Europe view draws the mirror, which holds no units under all-AI, so
  the docks and harbor show empty.
- Not ported: the OPENING side of `-d:0`, since OPENING.EXE is not decompiled.
  The port replays its OPENING, where a key skips the cinematic.
