# linux-colonize

Linux port of **Sid Meier's Colonization** (MicroProse, 1994 DOS).

This is a standalone build. It bundles the SDL2 and FluidSynth libraries and
the Roland SC-55 soundfont. It does **not** include the original game data —
you must provide your own copy of the DOS game.

## Installation

1. Extract this archive anywhere, e.g. `~/games/linux-colonize`.
2. Copy the contents of the original game's directory (the DOS `COLONIZE`
   folder: `VICEROY.EXE`, `*.SS`, `*.COL`, `*.MP`, `*.TXT`, etc.) into the
   `COLONIZE/` folder next to the `linux-colonize` binary. The floppy/CD
   version 3.0 of the game is the reference; GOG's package contains the same
   files.
3. Run the game from this directory:

   ```
   ./linux-colonize
   ```

   The game looks for its data in `./COLONIZE` relative to the current
   directory, so start it from the folder you extracted to (or pass
   `--data-dir /path/to/COLONIZE`).

## Options

- `--data-dir PATH` — use a different game-data directory.
- `--windowed` / `--fullscreen`, `--scale N`, `--nosound` — display/audio options.
- `COLONIZE_SOUNDFONT=/path/to/font.sf2` — override the bundled soundfont.

Settings and saves are written next to the game data.

## Troubleshooting

- **No window / SDL error**: the bundled `lib/` directory provides SDL2 and
  FluidSynth; everything else (X11/Wayland, PulseAudio/ALSA, glib) comes from
  your distribution. Any mainstream desktop distro from ~2022 on should work.
- **No music**: check that `data/soundfonts/Roland_SC-55.sf2` is present, or
  set `COLONIZE_SOUNDFONT`.

## Licenses

- **linux-colonize** (the binary and this package's original files):
  PolyForm Noncommercial 1.0.0 — see `LICENSE`.
- **Roland_SC-55.sf2** soundfont: GPL v3 or later, Copyright (c) 2015
  deemster — see `data/soundfonts/COPYRIGHT.Roland_SC-55`.
- **SDL2** (`lib/libSDL2-2.0.so.0`): zlib license.
- **FluidSynth** (`lib/libfluidsynth.so.3`): LGPL v2.1.
- **libsndfile**, **libinstpatch** (bundled FluidSynth dependencies): LGPL.
- The original game content you copy into `COLONIZE/` remains the property of
  its rights holders and is not distributed here.
