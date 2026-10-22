# OpenCol

> **Note:** this project was written largely by AI (Claude, Cursor and GPT,
> among others), under human direction. Disclosed per the papal recommendation
> in *Magnifica humanitas*.

![Screenshot](./screenshots/2026-09-29_20-15.webp)

OpenCol, port of **Sid Meier's Colonization** (MicroProse, 1994 DOS). Goal: same
rules, assets, saves, and inputs as the original. Fidelity bar and conflict
order: [docs/project_goals.md](docs/project_goals.md).

Version: **0.8-beta-bugfix-1**.

## Running the game

OpenCol is an engine only. **It needs the files of the original DOS
Colonization** (graphics, sound, text catalogs, maps), which are not part of
this repository or of release packages. Use your own legally obtained copy;
the game is still sold on GOG and Steam. Version 3.0 of the DOS game is the
reference.

1. Get an OpenCol binary: a release package, or build it yourself
   ([docs/building.md](docs/building.md)).
2. Copy the contents of the original game folder (the one holding
   `VICEROY.EXE`, `*.SS`, `*.PIK`, `*.TXT`, `*.MP`, …) into a folder named
   `COLONIZE` next to the `opencol` binary (release packages ship an empty one).
   Keep the original upper-case file names.

![Correct folder arrangement](docs/images/original_data.png)

3. Run `./opencol` (`opencol.exe` on Windows).

The data folder is looked up in this order: `--data-dir DIR`, `COLONIZE/`
next to the executable, `./COLONIZE` in the current directory. If required
files are missing the game says so and exits.

Saves (`COLONY0n.SAV`, compatible with the DOS game) go to `COLONIZE/` next to
the executable unless `--save-dir` is given. Preferences live in
`settings.json` next to the executable ([docs/settings.md](docs/settings.md)).

Useful flags (CLI wins over `settings.json`, which wins over the default):

| Flag | Default | Meaning | `settings.json` |
|------|---------|---------|-----------------|
| `--data-dir DIR` | `./COLONIZE` | Original game data | `data_dir` |
| `--save-dir DIR` | platform default | Save games | `save_dir` (empty = default) |
| `--scale N` | `2` | Window scale, clamped to 1..8 | `display.window_scale` |
| `--windowed` / `--fullscreen` | windowed | Window / fullscreen | `display.windowed` |
| `--nosound` | — | Disable audio | `no_sound` |
| `--seed N` | — | Fixed RNG seed (debug; like VR_SEED; `0` is valid) | `seed` (`null` / omit = unset) |
| `--debug-menu` / `--no-debug-menu` | off | DEBUG pulldown on the map navbar | `debug.menu` |

Music needs a soundfont; one is bundled. Details and overrides:
[docs/music.md](docs/music.md).

## Building

See [docs/building.md](docs/building.md).

## Documentation

Status:

- [docs/architecture.md](docs/architecture.md) — present / intended code architecture
- [tests/README.md](tests/README.md) — smoke / unit / golden test layout
- [docs/port_plan.md](docs/port_plan.md) — whole-project work queue + deferred AI track (agents start here)
- [docs/manual_gap.md](docs/manual_gap.md) — feature Done / Partial / Missing
- [docs/original_index.md](docs/original_index.md) — decomp / data navigation
- [docs/port_bugs.md](docs/port_bugs.md) - user reported bugs

Docs map: for conventions, jargon, fixture traps and the verification loop
see [docs/conventions.md](docs/conventions.md); for debug env vars and
`debug.logs` categories see [docs/debug_env_vars.md](docs/debug_env_vars.md).
The port-only preference file `settings.json` lives next to the executable
(see [docs/settings.md](docs/settings.md)).

## License

OpenCol code is [PolyForm Noncommercial 1.0.0](LICENSE): noncommercial use
only. The original game data is not covered and not included. Full details,
including the bundled soundfont and third-party code:
[docs/licensing.md](docs/licensing.md).
