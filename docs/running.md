# Running OpenCol

Basic setup (binary + original data files): [README](../README.md#running-the-game).

## Data folder

The original game data folder is looked up in this order: `--data-dir DIR`,
`COLONIZE/` next to the executable, `./COLONIZE` in the current directory.
Version 3.0 of the DOS game is the reference. Keep the original upper-case
file names. If required files are missing the game says so and exits.

## Saves and settings

Saves (`COLONY0n.SAV`, compatible with the DOS game) go to `COLONIZE/` next to
the executable unless `--save-dir` is given. Preferences live in
`settings.json` next to the executable, written on first run
([settings.md](settings.md)).

## Command-line flags

CLI wins over `settings.json`, which wins over the default:

| Flag | Default | Meaning | `settings.json` |
|------|---------|---------|-----------------|
| `--data-dir DIR` | `./COLONIZE` | Original game data | `data_dir` |
| `--save-dir DIR` | platform default | Save games | `save_dir` (empty = default) |
| `--scale N` | `2` | Window scale, clamped to 1..8 | `display.window_scale` |
| `--windowed` / `--fullscreen` | windowed | Window / fullscreen | `display.windowed` |
| `--nosound` | — | Disable audio | `no_sound` |
| `--seed N` | — | Fixed RNG seed (debug; like VR_SEED; `0` is valid) | `seed` (`null` / omit = unset) |
| `--debug-menu` / `--no-debug-menu` | off | DEBUG pulldown on the map navbar | `debug.menu` |
| `-K` | — | Hot-seat: the nation picker's hidden fifth cell ([hotseat.md](hotseat.md)); also env `COLONIZE=MULTI` | — |
| `-D` (or `-d:0`) | — | Self-running demo, as `COLDEMO.BAT` ([demo.md](demo.md)). A key quits and saves slot 5; `J` restarts the cycle | — |

## Map editor

`opencol-mapedit [--data-dir DIR] [--scale N] [-c | -m:FILE.MP]` is MAPEDIT.EXE: it edits
the `.MP` scenario maps in the data directory ([mapedit.md](mapedit.md)).

## Music

Music needs a soundfont; one is bundled. Details and overrides:
[music.md](music.md).
