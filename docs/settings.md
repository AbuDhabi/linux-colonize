# settings.json (port-only preference file)

DOS Colonization has no options file. `FUN_75c2_235c`
(`viceroy_unpacked_2.c:112401`) hard-codes the new-game words —
`DS:0x5382 = 0xc600`, `DS:0x5384` left at 0, `DS:0x5386 = 0x0e` — and the only
persistence is whatever ends up inside a `COLONY0n.SAV`. Start a fresh game and
every option snaps back to the shipped defaults.

The port keeps those bits and adds `settings.json`, written next to the
executable (`diag_exe_dir()`, the same anchor `savegame_default_dir()` uses;
falls back to `./settings.json`).

Owner: [`src/core/settings.c`](../src/core/settings.c) + `settings.h`.
JSON parsing reuses `src/core/json_min.c` (moved there from `tools/` so
`colonize_core` and `sav_json` share one reader).

## Who wins

Deliberately split, because "the save remembers" and "the program remembers"
answer different questions:

| Moment | Authority |
|--------|-----------|
| Startup | `settings.json`. `display` seeds the platform config unless `--windowed` / `--fullscreen` / `--scale` were passed (CLI wins). Sound options are pushed into the mixer after `sound_init`. |
| New Game | `ai_init_new_game` seeds the DOS words, then `settings_apply_to_head` overrides them — but **only if `settings_is_loaded()`**. |
| Load | **The save.** Options the player set during that game ride in its head and come back with it; `settings.json` is not applied. Only the audio mixer is re-pointed, since it lives outside the save. |
| Options dialog confirmed | Written to the live save head *and* flushed to `settings.json`, so the next new game and the next process start from it. |

`settings_is_loaded()` is false unless `settings_init` ran, and only `main.c`
calls it. Every test and golden harness therefore runs on untouched DOS
defaults no matter what file is sitting in the build directory.

## File shape

```json
{
  "version": 1,
  "game_options": {
    "show_indian_moves": true,
    "show_foreign_moves": true,
    "fast_piece_slide": false,
    "end_of_turn": false,
    "autosave": true,
    "combat_analysis": true,
    "water_color_cycling": true,
    "tutorial_hints": true
  },
  "colony_report_options": { "labels_on_buildings": true, "...": true },
  "sound_options": {
    "background_music": true,
    "event_music": true,
    "sound_effects": true
  },
  "display": { "windowed": true, "window_scale": 2 }
}
```

Written on first run so the options are discoverable and hand-editable without
opening a dialog first. Every key is optional: a missing file, section or key
falls back to the DOS default, so an older file keeps working when fields are
added. A file that fails to parse is reported (stderr + diagnostics log), left
on disk untouched — the player's edits are theirs to fix — and the session runs
on defaults. Writes go to `settings.json.tmp` and are `rename()`d into place.

## Polarity

Everything in `ColonizeSettings` is stored in the player-facing sense: `true`
means the thing happens. Several DOS bits are inverted suppress flags, and
`settings_apply_to_head` / `settings_capture_from_head` do the flipping:

- `water_color_cycling` — `DS:0x5383` bit 0 set = cycling *off*. Wired to the
  real effect: `game_water_cycle_tick` (game_loop.c) rotates map-palette
  entries 0x78–0x7F per `CYCLE.DAT` (~575 ms/step, DOS `FUN_1a0a_007a`) on
  the map screen. Those 8 blues appear in the sea-lane tile (TERRAIN 11),
  PHYS0 rivers + coast corners 150–153, and 2 px of swamp — open-ocean tiles
  (TERRAIN 10) are flat 0x3a–0x3c and never animate, matching DOS.
- All ten colony-report bits — `DS:0x5384`/`0x5385` set = report *suppressed*.
  `FUN_2b5a_223a` sets each bit when its checkbox is **clear**, and the EOT
  reporters fire on `bit == 0` (`turn_report_ok_*` in `turn.c`). A fresh save
  is all-zero, i.e. every report showing.

The colony options dialog had this backwards — checking "Report food
shortages" stored a 1 and thereby silenced it. `options_dialog_open_colony` /
`options_dialog_apply_colony` now invert, matching `FUN_2b5a_223a`.

## Tutorial hints

`0xc600` does **not** include bit 7. DOS ORs tutorial hints in afterwards only
when the chosen difficulty is 0 / Discoverer (`viceroy_unpacked_2.c:111468`);
`ai_init_new_game` now follows that rule. A preference file has no difficulty
to consult, so `settings.json` ships the Discoverer value and, once present,
is absolute — a stored preference outranking a difficulty default is the point
of the file.

## What is *not* in the file

The rest of `game_options` (`DS:0x5382` low bits: `woi`, `ref_present`,
`woi_crosses_event`, `independence_chrome`, `calendar_latch`,
`independence_force`, `ref_unit_threshold`) is game state, not preference, and
`cheats_enabled` is a per-game Alt-WIN unlock. `settings_apply_to_head` never
touches those bits — `unit_settings` asserts it.

## Adding an option

1. Add the field to `ColonizeSettings` and to `settings_defaults`.
2. Write it in `settings_save_file`, read it in `settings_load_file` (guarded,
   so absence keeps the default).
3. If it maps to a Col1 head bit, extend `settings_apply_to_head` and
   `settings_capture_from_head` — keep them mirror images, and check the DOS
   packer for polarity before assuming the bit means what its name says.
4. Extend `tests/unit/test_settings.c`.

Port-only options with no DOS bit (future: key bindings, autosave interval)
live in their own top-level section and skip the head bridge entirely.
