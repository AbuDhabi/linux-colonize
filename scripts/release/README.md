# linux-colonize

Linux port of **Sid Meier's Colonization** (MicroProse, 1994 DOS).

This is a standalone build: SDL2 and FluidSynth are linked into the binary,
and the Roland SC-55 soundfont is included. It does **not** include the
original game data — you must provide your own copy of the DOS game.

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

- **No window / SDL error**: everything except X11 and graphics/audio drivers
  is built into the binary. It needs glibc 2.17 or newer (any x86_64 distro
  from ~2014 on) and an X11 or Wayland desktop (Wayland works via XWayland).
- **No music**: check that `data/soundfonts/Roland_SC-55.sf2` is present, or
  set `COLONIZE_SOUNDFONT`.

## Licenses

- **linux-colonize** (the binary and this package's original files):
  PolyForm Noncommercial 1.0.0 — see `LICENSE`. Full source:
  <https://github.com/AbuDhabi/linux-colonize>
- **Roland_SC-55.sf2** soundfont: GPL v3 or later, Copyright (c) 2015
  deemster — see `data/soundfonts/COPYRIGHT.Roland_SC-55`.
- The original game content you copy into `COLONIZE/` remains the property of
  its rights holders and is not distributed here.

The following libraries are statically linked into the binary. Their license
texts are in `THIRD_PARTY_LICENSES`, and sources for the exact versions used
are obtainable at the URLs below (also listed per-library in that file):

- **SDL2** — zlib license — <https://www.libsdl.org/>
- **FluidSynth** — LGPL v2.1+ — <https://www.fluidsynth.org/>
- **GLib** — LGPL v2.1+ — <https://gitlab.gnome.org/GNOME/glib>
- **PCRE2** — BSD 3-clause — <https://github.com/PCRE2Project/pcre2>
- **libffi** — MIT — <https://github.com/libffi/libffi>
- **zlib** — zlib license — <https://zlib.net/>

Per LGPL, you can relink the program against modified versions of FluidSynth
or GLib: the program's full source is available at the repository above, and
the release build scripts (`scripts/build_release*.sh`) reproduce this binary.
