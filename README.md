# linux-colonize

Linux port of **Sid Meier's Colonization** (MicroProse, 1994 DOS). Goal: same
rules, assets, saves, and inputs as the original. Fidelity bar and conflict
order: [docs/project_goals.md](docs/project_goals.md).

Version: **0.1-alpha**.

## Current state

Strong on shell, map art, navigation, reports / pedia, Col1 save/load, units /
naval passengers, founding a colony, and Europe buy/sell/recruit/hire.
Structural Indian contact, diplomacy, king/REF, FF election, trade routes, and
early Euro AI are in. Next playability work is leftover FF KINGGALLEON2, deep
mid-planner `20e6`, production / combat depth, and endgame polish. VGA dialog
chrome, Congress UI, COLDIG SFX, and full 1:1 AI remain later.

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
`--data-dir`).

## Build / run

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/colonize_linux
```

Useful flags:

| Flag | Default | Meaning |
|------|---------|---------|
| `--data-dir DIR` | `./COLONIZE` | Original game data |
| `--save-dir DIR` | platform default | Save games |
| `--scale N` | `2` | Window scale |
| `--fullscreen` | — | Fullscreen |
| `--nosound` | — | Disable audio |

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
entirely.
