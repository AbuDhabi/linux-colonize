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
| Startup | Launch flags (`data_dir`, `save_dir`, `windowed` / `window_scale`, `no_sound`, `seed`, `debug.menu`) and sound options. Precedence, most to least: CLI (if that flag was given), `settings.json` (if that key is present and valid), hardcoded default. Sound mixer options are pushed after `sound_init`. `debug.mouse_coords`, `debug.building_rects`, and `debug.logs` are settings + DEBUG menu only (no CLI). |
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
    "sound_effects": true,
    "soundfont": "",
    "midi_backend": ""
  },
  "display": { "windowed": true, "window_scale": 2, "window_log_lines": 0 },
  "debug": {
    "menu": false, "mouse_coords": false, "building_rects": false, "logs": false,
    "trace_autosaves": false, "input_logger": false, "input_logger_directory": "./input_logs"
  },
  "divergences": { "unlimited_units": false, "modern_prng": false, "pioneer_plan": false, "current_vision": false, "scout_autoexplore": false, "ship_autoexplore": false, "hard_king": false },
  "mods": { "wagon_loot": false, "goto_native_wait": true },
  "data_dir": "./COLONIZE",
  "save_dir": "",
  "no_sound": false,
  "skip_intro": true,
  "seed": null
}
```

Launch keys map 1:1 onto the process flags (`--data-dir`, `--save-dir`,
`--windowed` / `--fullscreen`, `--scale`, `--nosound`, `--seed`,
`--debug-menu` / `--no-debug-menu`).
`sound_options.soundfont` names a GM-compatible `.sf2` file for MIDI; empty
means auto-detect (bundled SC-55, then system banks). Unreadable path falls
back to auto-detect with a logged warning. `sound_options.midi_backend` picks
the synth: `"fluidsynth"`, `"tsf"` (bundled TinySoundFont), or empty = auto
(FluidSynth if built in, else TSF); any other string is ignored at load.
`display.window_log_lines` (default 0, clamped 0..24) adds that many FONTTINY
rows of message log under the 320x200 screen: the window (and the framebuffer)
grows by `2 + lines * 7` px — a 1px black rule against the screen, a 1px
margin, then `WOODTILE.SS` fill with the same screen-aligned grain as the menu
bar and the map sidebar. Newest entry at the bottom, older ones pushed up; each entry starts with a
`*` and its wrapped rows line up after it.
Contents are the popup texts (title + flattened body) as each popup is
presented (`window_log_push` in `ai_popup.c`), `{}` emphasis markup kept: the
strip draws it in the popup's yellow (`popup_draw_text_markup`) and uses it to
decide what survives when a line is too long. `window_log_wrap` (game_loop.c)
flows one entry over as many rows as it needs, capped at half the strip so a
long popup cannot evict every older message; words are re-braced per row, so
emphasis crossing a break stays balanced. Whatever does not fit the last
allowed row goes through `window_log_fit_line`, which keeps the first two
words, the last two and every emphasised one, collapsing each dropped run into
`...`; if that still overflows it gives up the emphasised words
middle-outwards. The store is `src/core/window_log.c` and
the strip is painted by `game_render_window_log`, called by the host loop
*after* `game_render` — every screen renderer clears and centres by
`fb->height`, so the game itself only ever sees 320x200. Windowed mode only:
fullscreen ignores the key (`platform_create` zeroes the extra height), and a
pointer over the strip reads as the bottom screen row.

`divergences` holds deliberate departures from DOS behaviour, all off by
default. `divergences.unlimited_units` (read at startup, `main.c` ->
`units_set_unlimited`) skips the `FUN_1427_06b4` unit limits (292 for AI and
natives, 300 total, 200 per European nation, `@TOOMANYUNITS`); the physical
pool `COLONIZE_UNITS_MAX` (4096) is the only cap left. Records past 300 are
saved in the `UNXT` port extension chunk (docs/savegame.md), so DOS still
loads the first 300 and never sees the rest. Turning the flag off again does
not drop units: a save already above 300 loads whole, and new units wait
until the count falls back under the DOS limit.
`divergences.modern_prng` (read at startup, `main.c` -> `dos_rng_modern_seed`)
makes every `dos_rng_next` draw, on any `ColonizeDosRng`, the top 15 bits of one
process-wide xoshiro256** (`src/third_party/xoshiro256starstar.h`, authors' CC0
code). Seeded once per process via splitmix64 from the campaign seed (`--seed` /
`seed`) if set, else the wall clock; DOS reseeds (`dos_rng_seed`, DS:0x83a6, music
tick) then leave the stream alone, so nothing replays. Values re-derived on demand
from a stable seed stay on the LCG (`dos_rng_range_lcg`): the colony-screen building
layout (every frame) and a village's taught skill (@CHIEFHOWDY, then the visit). The state rides
in RTST record 13, written only while on; off = DOS draws and saves unchanged.
`divergences.pioneer_plan` adds VIEW Plan Pioneer and ORDERS Automate
(docs/pioneer_plan.md); off = the rows stay hidden and no `PPLN` chunk is written.
`divergences.scout_autoexplore` offers ORDERS Automate (`Q`) to Scouts, which then
explore on their own (docs/explore_auto.md); `divergences.ship_autoexplore` does the same
for ships (ocean fog only). Both off = no row for those units and no `SAUT` chunk.
`divergences.hard_king` makes the War of Independence REF tougher (`ai_king_ref.c`):
(1) `ai_king_ref_wave` runs up to difficulty + 1 FUN_43f7_0982 waves a turn
(Discoverer 1 .. Viceroy 5), each spending one Man-O-War from the pool; an empty hull
or land pool ends the turn's waves, and later waves skip colonies already invaded this
turn while another coastal colony is left; (2) the landing sweep
(`ai_king_0982_land_troops`) no longer counts the target's own units in a tile's stack,
so occupied tiles tie with empty ones and get seized (FUN_43f7_0512, units destroyed);
(3) target scoring adds the target's land units with attack > 1 within Chebyshev 2 of
the colony (off its tile) to the tile strength and, outside the Fort/Fortress
multiplier, to the garrison score. Off = DOS. Nothing saved.
`divergences.current_vision` dithers (1-pixel checkerboard: 1/4 fog blue, palette 60..62, over 3/4 pixel; palette-nearest LUT) every seen
map tile that no unit or colony of the viewed nation currently sees: unit tiles
from `units_sight_radius` + the `map_sight_each` domain gate, colonies radius 1.
The veil covers explored tiles plus a 1-tile ring into the unexplored. Layering,
top first: colony names, fog (pixels the terrain pass left as fog are re-stamped,
`s_fog_mask`), the veil (`game_render_current_vision`), everything else.
Render-only, nothing saved.

### Mods

`mods` holds per-feature toggles (`src/core/mods.h`, one `ModId` bit each; ids
are stable and append-only). DOS bug fixes default on, gameplay changes default
off. The file holds only the defaults for *new* games: `game_commit_new_campaign`
snapshots them into the campaign mask (`mods_campaign_set`), the mask rides the
save in RTST record 17, and code asks `mods_on(id)`. Changing the defaults never
touches a running campaign. A save without the record (DOS, older port saves)
loads with every mod off; leaving to the title clears the mask. With
`dos_buggy_behavior()` on (every test binary) a new game gets mask 0.

GAME > Mods for New Games (row 10, appended by `data/mods/MENU.TXT`) opens a
checklist (`data/mods/GAME.TXT` `@MODOPTIONS`) that edits these defaults and
flushes the file. `data/mods/*.TXT` is a text overlay that opencol loads from
`<exe>/data/mods` or `./data/mods`: it may add whole sections or append the next
row of a stock section, never replace a stock row (refused with a warning).

| Key | Default | Effect |
|-----|---------|--------|
| `wagon_loot` | off | Natives beating a loaded wagon train roll 1..10: 1-2 destroy it (DOS), 3-6 take every hold of one random good (`data/mods` `@MODWAGONLOOT1`), 7-10 take all goods (stock `@LOOTWAGONS`); the wagon survives. Off, or empty wagon: no extra RNG draw. |
| `goto_native_wait` | on | DOS bug fix (dos_bugs.md DB-75): a human Go To whose goal tile holds a native unit (not a village) stops next to it with the order kept instead of attacking. |
| `population_floor` | off | Every built building's NAMES.TXT min population (not just Stockade/Fort/Fortress at 3) floors voluntary ejects, player and AI (`colonies_eject_colonist`; the AI equip arm checks `colonies_eject_allowed` first, so a colony at its floor neither strips an expert nor ejects); the refusal reads `data/mods` `@MODKEEPSTOCKADE` with the floor as `%NUMBER0`. The Units tab gets an ABANDON button (label `data/mods` LABELS.TXT `@MODABANDON`; it covers the unit icons in that corner of a full pane): stock `@ABANDON`/`@ABANDON2` confirm, then every colonist walks out as a Colonist onto the tile and the colony is deleted; buildings and goods are lost, floor and `@SIEGE` do not apply. A refused spawn (`@TOOMANYUNITS`) stops the walk-out and keeps the colony with whoever is left. Abandoning (the button or moving the last colonist out) shows woodcut 12 the first time, captioned by `data/mods` WOODCUT.TXT `@MODWOODCUT12`. Starvation and capture ignore the floor. Off = DOS Stockade-at-3 refusal; nothing saved, no RNG. |
| `raid_scalps` | off | The colony-raid kind roll (`FUN_5fef_0f14`) becomes rand(1,5); kind 5 kills one victim and shows the dead stock `@RAIDSCALP` (AI victims get the usual `@RAIDWREAK` bulletin). No Fort: any colonist inside or any unarmed unit of the owner on the colony tile (no muskets, attack 0, not a wagon, treasure or ship); Fort: only those units outside; Fortress: the raid does nothing. The last colonist can die: the colony is then abandoned after the popup, and a human victim gets woodcut 11 the first time (DOS's own once-only bit). Abandoning a colony by moving its last colonist out shows woodcut 12 (see `population_floor`). Same early-game grace as burn/ship; alarm -16 like DOS's "unit killed" row; every European colony. Off = the DOS rand(1,4) draw, nothing saved. |
| `royal_mercy` | off | Losing a Royal-flagged unit (`FUN_5fef_0352`) cuts the tax by the matched DS:0x978d row's own byte instead of the next one (Caravel 2, Merchantman 4, Galleon 6, Privateer 3, Frigate 8, Artillery 2), capped at the current rate; a human loser sees the dead stock `@KINGMERCY` in the King's portrait. Off = DOS's one-row-late read (Frigate and Artillery cut nothing), no popup. Nothing saved, no RNG. |
| `late_fathers` | off | Father category and weights (`FUN_4345_0080` / `FUN_4345_06d2`) come from `data/mods` NAMES.TXT `@MODFATHERS` instead of `@FATHERS`. Its optional first line `YEARS, 1600, 1650, 1700` lists band start years (ascending, at most 15; N years give N+1 bands, band k from year k on, by year only); each row is then `name, category, w0, ..., wN`. Without a YEARS line the bands are DOS's (`FUN_4345_005a`: 1600, 1700) with three weights. A bad YEARS line (not ascending, a year outside 1..9999) or any row with the wrong weight count, a non-number, a weight outside 0..255 or a category outside 0..4 leaves the whole section unused: one `diag_warn` per NAMES.TXT load and `@FATHERS` as usual. The shipped section is `@FATHERS` with Stuyvesant, Washington, Jones, Jefferson and Franklin weighted 0 before 1650 and Adam Smith 0 before 1600, so neither Congress nor any AI nation nominates them earlier. A candidate already chosen (save or earlier debate, human or AI) whom `@MODFATHERS` weights 0 this year while `@FATHERS` weights him above 0 in this year's DOS band is dropped and re-rolled, and a stored debate slate holding one is discarded; a stock zero keeps DOS's rule (the candidate stays). Off = DOS `@FATHERS` and bands, same draws; on, the weights and any re-roll change the draws. Nothing saved. |
| `town_hall_tiers` | off | @BUILDING rows 10/11 (the Town Hall upgrades DOS's FUN_15eb_3650 zeroes) become buildable on the tier below (pop 4 / 8); player and AI may build them, the AI cascade never names them. Owning them grows the work radius to 12 / 20 plots through the existing DS:0x329 / FUN_137f_003c path, so production, AI plot choice and the Docks gate follow unchanged code. Colony screen: 5x5 area view, corners and plots outside the radius shaded and refusing colonists, BDARK.SS art per tier, hover label names the tier. Names and Colonizopedia articles come from `data/mods` `@MODBUILDING` (NAMES) and `@MODBUILDING10/11` (PEDIA), read only while on. |
| `building_upkeep` | off | Human colonies pay the NAMES.TXT upkeep column for the top owned tier of each building chain (the Town Hall tiers top their chain while `town_hall_tiers` is on), once per turn before colony production. Short treasury: no payment, `@UPKEEP` popup, every building worker makes half (floor) that turn and teachers do not advance; the colony screen shows the halved figures and a brown rebel line. F6 colony report shows TOTAL UPKEEP (`@MISC` row 92 of LABELS). AI nations never pay. |
| `tribe_mood` | off | Restores the cut @PISS0-5 lines. The alarm writers tally each delta per tribe/nation pair by DOS reason code (1 road, 2 forest clearing, 3 mission, 5 population pressure; 4 = an attack on a tribe below alarm 0x4b, wired in `units_move.c` since DOS passes 0). After a human Scout speaks with a chief and survives, the chief names the cause with the largest \|tally\| since the last report (@PISS<reason>, 0 = none) and his mood: alarm/25 picks NAMES @ATTITUDE, (alarm%25)/5 the @ATTITUDINAL modifier (reversed for Content). Two `data/mods` lines follow: @VILLAGEMOOD (this village's tier, the colour of its map marks) and, when a nation's colonies score in FUN_281f_0316, @VILLAGECROWD (that nation). The report clears the pair's tally. Tallies persist in RTST record 15, written only when non-zero. No RNG. |
| `scout_spying` | off | Computer Scouts infiltrate human colonies (`ai_euro_spy.c`). DOS ships the logic (FUN_5f7a_000e, @LOSTTHEIRSCOUTS) but gives a computer Scout "Nothing" (raw 98829). Every 10 turns per human colony (staggered by slot) a 20% roll sends one AI Scout on its continent there (plan byte 'S', a course the turn housekeeping keeps). Arrival uses the DOS threshold without the human-only difficulty term: (Stockade/Fort/Fortress count + 6) * 2, halved for a Seasoned Scout, against 1..36. Caught: @LOSTTHEIRSCOUTS to the human, scout lost, colony +100 horses. Success: that AI knows the colony's defence for 10 turns; otherwise the Soldier/Dragoon colony gate sees it within +-50%. A scouted colony weaker than the AI's land army on its continent becomes a prio-8 MILITARY goal. Rolls hash the turn, nation and slot and never draw from the DOS RNG. Intel persists in RTST record 16, written only when non-empty. |

Port-only keys, no DOS bit, no head bridge.
`save_dir` empty means the platform default (`<exe>/COLONIZE`). `seed` is
`null` in a first-run file (same as omitting the key): not set, so the campaign
RNG uses elapsed time. `"seed": 0` is a real seed and pins the LCG to 0.
Unseeded, every `FUN_281f_04ca` reseed (nation slots, Lost City Rumours, the
465b overspend move, raids, starvation) reads the wall clock, as VICEROY reads
the BIOS tick (`dos_rng_set_live_ticks`). A set seed pins that tick the way
VR_SEED.EXE does, so a seeded game replays every rumour roll and AI slot stream
(playtest C2-O1).
`debug.menu` shows the DEBUG pulldown on the map navbar (no-op if the binary
was built with `COLONIZE_DEBUG_MENU=OFF`). `debug.mouse_coords` and
`debug.building_rects` are pointer HUD / colony building outlines, toggled
from that pulldown (no CLI flags); the toggles write the keys back.
`debug.logs` (default false) gates `diag_info` lines in `opencol.log`;
WARN/ERROR still always write. `debug.trace_autosaves` (default false) writes
one extra save per turn, `trace_autosave_turn_XXX.sav` in the save directory,
alongside the normal COLONY08/09 autosaves and regardless of the DOS autosave
option bit; the files are never reused or pruned. `debug.input_logger`
(default false, read at startup) records raw keyboard and mouse input to
`input_YYYYMMDD_HHMMSS_mmm.jsonl` (local start time) in
`debug.input_logger_directory` (default `./input_logs`, cwd-relative like
`data_dir`; one directory level is created if missing) — see "Input log"
below. A wrong type, a negative `seed`, or an empty
path is not a valid value and the hardcoded default stays.

### Input log

`platform_poll_input` (`sdl_runtime.c`, "Input log" banner) writes every SDL
keyboard / mouse event before the game folds it into `ColonizeInputState`,
one JSON object per line, flushed once per frame. Every record has `type`,
`t_ms` (SDL's own event timestamp, ms since SDL init, monotonic), `unix_ms`
(the same instant on the wall clock: `t_ms` rebased on the clock read at
open) and `frame` (index of the `platform_poll_input` call that delivered it;
events with the same `frame` reached the game together).

| `type` | Extra fields |
|--------|--------------|
| `session` | first line: `version` (1), `window_scale`, `screen_w`, `screen_h` (logical, at open), `audio` (audio device open) |
| `mouse_move` | `x`,`y` window points; `lx`,`ly` logical screen pixel the game sees (clamped); `dx`,`dy` relative motion; `buttons` held mask |
| `mouse_down` / `mouse_up` | `x`,`y`,`lx`,`ly`; `button` (`left`/`middle`/`right`/`x1`/`x2`); `clicks` (2 = double click) |
| `mouse_wheel` | `dx`,`dy` |
| `key_down` / `key_up` | `key` (SDL key name), `keycode` (SDL_Keycode), `scancode`, `mod` (SDL_Keymod bits), `repeat` |
| `text` | `text` (UTF-8 as SDL delivered it; the game keeps ASCII only) |
| `focus_lost` / `focus_gained` | — (focus loss synthesizes the held-button releases) |
| `resize` | `w`,`h` window points |
| `quit` | — |

`buttons` uses SDL's mask bits: 1 left, 2 middle, 4 right, 8 x1, 16 x2. A
drag is `mouse_down`, then `mouse_move` records with that button's bit set,
then `mouse_up`. Touch-synthesized mouse events are skipped.

Trimmed logs replay as automated regressions (`tests-automated-regression/`,
tests/README.md "Input-replay regressions").

### What `debug.logs` records

Every line carries the screen it happened on (`[map]`, `[colony:Jamestown]`,
`[europe]`, `[report:Colony]`, `[unit stack]`, ...) — `diag_set_context`, kept
current by `game_track_screen` in [`game_loop.c`](../src/core/game_loop.c).
Lines are tagged by kind:

| Tag | Where | Content |
|-----|-------|---------|
| `VIEW` | `game_loop.c` | screen change, `old -> new` (modals included) |
| `COMMAND` | `game_loop.c` | any MENU/hotkey action, with the selected unit |
| `NEWGAME` | `game_loop.c` | leader, nation, difficulty, map, start year |
| `TURN` | `turn.c` | EOT setup line, then each European / native nation |
| `PROD` | `turn.c` | per colony per turn: pop, SoL, food, hammers, project, then net stock change per cargo |
| `COLONY` | `colony.c`, `game_loop.c` | screen open summary, worker assignments, construction picked / bought |
| `CARGO` | `colony.c` | colony ↔ unit goods transfers |
| `EUROPE` | `europe.c`, `turn.c`, `game_loop.c` | screen open (gold/tax/harbor/docks), buy, sell, train, purchase, recruit, sail, dock @ARMOPTIONS arm/equip rows, treasure cash-in, Custom House auto-sell (`EUROPE customs`, per cargo + colony total), AI warehouse dump-sell, crosses immigrant arriving on the docks |
| `ORDER` | `units.c` | orders given (fortify, sentry, goto, follow, plow/road, wake, disband) |
| `MOVE` | `units.c` | every completed unit step, with MP left |
| `COMBAT` | `combat_analysis.c` | both sides' names, baseline and final strength, every Combat Analysis modifier row, odds, roll, winner — logged for AI-vs-AI fights too, not only when the dialog shows |
| `LCR` | `units.c` | lost city rumour outcome, gold, explorer skill |
| `FF` | `founding_fathers.c` | founding father elected |
| `POPUP` | `ai_popup.c`, `europe.c`, `howmuch_dialog.c` | queued / shown / answered, with tag name, kind, the fully interpolated body, the choice list and the picked option. The Europe RECRUIT / TRAIN / PURCHASE / DOCK list dialogs log the same show/answered pair under `tag=EUROPE_<menu>`, and the @HOWMUCHn amount dialog under `tag=HOWMUCH_<kind>` |

`ai_popup_tag_name` and `map_menu_action_name` keep those lines readable —
add a case to both when a tag or menu action is added.

Written on first run so the options are discoverable and hand-editable without
opening a dialog first. Every key is optional: a missing file, section or key
falls back to the DOS / hardcoded default, so an older file keeps working when
fields are added. A file that fails to parse is reported (stderr + diagnostics
log), left on disk untouched — the player's edits are theirs to fix — and the
session runs on defaults. Writes go to `settings.json.tmp` and are `rename()`d
into place.

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
  is all-zero, i.e. every report showing. `options_dialog_open_colony` /
  `options_dialog_apply_colony` invert the same way.

## Tutorial hints

`0xc600` does **not** include bit 7. DOS ORs tutorial hints in afterwards only
when the chosen difficulty is 0 / Discoverer (`viceroy_unpacked_2.c:111468`);
`ai_init_new_game` now follows that rule. A preference file has no difficulty
to consult, so `settings.json` ships the Discoverer value and, once present,
is absolute — a stored preference outranking a difficulty default is the point
of the file.

## Intro cinematic (`skip_intro`)

`skip_intro` is a "not the first launch" marker, not a one-shot latch.

A missing `settings.json` is first launch: `OPENING.EXE` plays, and the
created file writes `"skip_intro": true` so the next start goes to the title
menu. After that the key is left alone — set it to `false` to watch the intro
every time. Tests never call `settings_init`, so harnesses never play it.

## What is *not* in the file

The rest of `game_options` (`DS:0x5382` low bits: `woi`, `intervention_announced`,
`woi_crosses_event`, `independence_chrome`, `calendar_latch`,
`independence_force`, `ref_unit_threshold`) is game state, not preference, and
`cheats_enabled` is a per-game Alt-WIN unlock (the map-screen Alt-W/I/N toggle writes it in-session and it is saved with the game). Two CHEAT entries also write the live head, not `settings.json`: Advance Revolution sets `independence_force` on its last stage (port_bugs.md #1171) and Reveal Map > Complete Map clears the live `show_indian_moves` bit (DS:0x5383 bit 7, port_bugs.md #1170); the next Options confirm flushes the latter to `settings.json` like any other change. `settings_apply_to_head` never
touches those bits — `unit_settings` asserts it.

## Adding an option

1. Add the field to `ColonizeSettings` and to `settings_defaults`.
2. Write it in `settings_save_file`, read it in `settings_load_file` (guarded,
   so absence keeps the default).
3. If it maps to a Col1 head bit, extend `settings_apply_to_head` and
   `settings_capture_from_head` — keep them mirror images, and check the DOS
   packer for polarity before assuming the bit means what its name says.
4. Extend `tests/unit/test_settings.c`.

Port-only options with no DOS bit (`data_dir`, `save_dir`, `no_sound`, `seed`,
`display`, `debug`, `skip_intro`, and future ones like key bindings) skip the head bridge
entirely.
