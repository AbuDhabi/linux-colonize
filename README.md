# linux-colonize

Linux port of **Sid Meier's Colonization** (MicroProse, 1994 DOS). Goal: same
rules, assets, saves, and inputs as the original. Fidelity bar and conflict
order: [docs/project_goals.md](docs/project_goals.md).

Version: **0.5-alpha**.

## Current state

Playability tracks P1–P11 closed 2026-09-03. Strong on shell, map art,
navigation, reports / pedia, Col1 save/load, units / naval passengers, founding
a colony, Europe buy/sell/recruit/hire/equip, rumours/treasure (incl.
KINGGALLEON2), structural Indian contact and diplomacy, king/REF, FF election
(all 25 Fathers), and trade routes. Euro AI is past the structural stage
(`20e6` land planner + `5d04` mid-planner, August 2026). Music is a literal
`GSOUND.COL` driver emulator; `COLDIG.BIN` SFX are wired (leftover misfires
are polish).

Remaining work is incoming [bugs.md](bugs.md) nits, production / combat depth
on the Partial rows in [manual_gap.md](docs/manual_gap.md), and the deferred
tracks (1:1 rival/Indian AI, seed determinism, pixel-exact dialog chrome,
SC-55 timbre).

Living status:

- [docs/architecture.md](docs/architecture.md) — present / intended code architecture
- [tests/README.md](tests/README.md) — smoke / unit / golden test layout
- [docs/roadmap.md](docs/roadmap.md) — whole-project phases / what’s next
- [docs/port_plan.md](docs/port_plan.md) — whole-project sequenced work queue (agents start here)
- [docs/ai_port_plan.md](docs/ai_port_plan.md) — AI porting work queue
- [docs/manual_gap.md](docs/manual_gap.md) — feature Done / Partial / Missing
- [docs/ai_transcription.md](docs/ai_transcription.md) — AI FUN inventory / unpark
- [docs/original_index.md](docs/original_index.md) — decomp / data navigation
- [bugs.md](bugs.md) - user reported bugs

## Requirements

- CMake ≥ 3.20
- SDL2 (required for the game binary)
- FluidSynth (optional; MIDI / Pick Music)

Original game data must be present under `COLONIZE/` (or passed with
`--data-dir`). This repo does not include those files; you provide a copy
you obtained yourself (see [License](#license)).

## Build / run

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/colonize_linux
```

Useful flags (CLI wins over `settings.json`, which wins over the default):

| Flag | Default | Meaning | `settings.json` |
|------|---------|---------|-----------------|
| `--data-dir DIR` | `./COLONIZE` | Original game data | `data_dir` |
| `--save-dir DIR` | platform default | Save games | `save_dir` (empty = default) |
| `--scale N` | `2` | Window scale | `display.window_scale` |
| `--windowed` / `--fullscreen` | windowed | Window / fullscreen | `display.windowed` |
| `--nosound` | — | Disable audio | `no_sound` |
| `--seed N` | — | Fixed RNG seed (debug; like VR_SEED; `0` is valid) | `seed` (`null` / omit = unset) |
| `--debug-menu` / `--no-debug-menu` | off | DEBUG pulldown on the map navbar | `debug.menu` |

Release preset: `cmake --preset release` then `cmake --build --preset release`.

## Music / MIDI soundfont

Songs play through FluidSynth over a GM/GS soundfont. `sound_find_soundfont`
(`src/core/sound.c`) picks one automatically, in order:

1. `$COLONIZE_SOUNDFONT` (env var), if set and readable.
2. The bundled default, `data/soundfonts/Roland_SC-55.sf2` (GPL-3+, see
   `data/soundfonts/COPYRIGHT.Roland_SC-55`) — tried relative to the working
   directory, the executable's own directory, and `--data-dir`.
3. Common system soundfont locations (e.g.
   `/usr/share/sounds/sf2/FluidR3_GM.sf2`, ScummVM's bundled SC-55, etc.).

If none are found, the game logs a warning and falls back to a soft
(non-FluidSynth) beep path rather than failing — set `COLONIZE_SOUNDFONT` to
point at any GM-compatible `.sf2` file to override. `--nosound` skips audio
entirely. The bundled bank is optional and separately licensed (GPL-3+);
it is not required to build or to run. Without any soundfont, music uses
the square-wave fallback (no MIDI).

## License

**linux-colonize code** is [PolyForm Noncommercial 1.0.0](LICENSE)
(`PolyForm-Noncommercial-1.0.0`). Noncommercial use, modification, and
redistribution are allowed if you keep the license and the
`Required Notice:` attribution line. **Commercial use needs explicit
written permission** from the copyright holder.

**Original Colonization data** (graphics, sound, text catalogs, and the
rest of a DOS/`COLONIZE/` install) is **not** relicensed here and is
**not** in this repository. The binary needs those files until someone
ships replacement assets. You must supply a legally obtained copy — the
1994 game is still sold on GOG and Steam, among other stores.

**Bundled SC-55 soundfont** (`data/soundfonts/Roland_SC-55.sf2`) is a
convenience for MIDI, licensed **GPL-3+** by deemster. See
[`data/soundfonts/COPYRIGHT.Roland_SC-55`](data/soundfonts/COPYRIGHT.Roland_SC-55).
The port runs without it.
