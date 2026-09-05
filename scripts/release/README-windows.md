# linux-colonize (Windows build)

Windows port of **Sid Meier's Colonization** (MicroProse, 1994 DOS) — the
Windows build of the linux-colonize project.

This is a standalone build: SDL2 and FluidSynth are linked into the
executable, and the Roland SC-55 soundfont is included. It does **not**
include the original game data — you must provide your own copy of the DOS
game.

## Installation

1. Extract this archive anywhere, e.g. `C:\Games\colonize`.
2. Copy the contents of the original game's directory (the DOS `COLONIZE`
   folder: `VICEROY.EXE`, `*.SS`, `*.COL`, `*.MP`, `*.TXT`, etc.) into the
   `COLONIZE\` folder next to `colonize.exe`. The floppy/CD version 3.0 of
   the game is the reference; GOG's package contains the same files.
3. Run `colonize.exe` from that folder. The game looks for its data in
   `.\COLONIZE` relative to the current directory (or pass
   `--data-dir C:\path\to\COLONIZE`).

A console window opens alongside the game window; it shows log output and is
harmless.

## Options

- `--data-dir PATH` — use a different game-data directory.
- `--windowed` / `--fullscreen`, `--scale N`, `--nosound` — display/audio options.
- `COLONIZE_SOUNDFONT=path\to\font.sf2` — override the bundled soundfont.

Settings and saves are written next to the game data.

## Troubleshooting

- **No music**: check that `data\soundfonts\Roland_SC-55.sf2` is present, or
  set `COLONIZE_SOUNDFONT`.
- Windows 7 or newer, 64-bit, is expected.

## Licenses

- **linux-colonize** (the executable and this package's original files):
  PolyForm Noncommercial 1.0.0 — see `LICENSE`. Full source:
  <https://github.com/AbuDhabi/linux-colonize>
- **Roland_SC-55.sf2** soundfont: GPL v3 or later, Copyright (c) 2015
  deemster — see `data\soundfonts\COPYRIGHT.Roland_SC-55`.
- The original game content you copy into `COLONIZE\` remains the property of
  its rights holders and is not distributed here.

The following libraries are statically linked into the executable. Their
license texts are in `THIRD_PARTY_LICENSES`, and sources for the versions
used are obtainable at the URLs below:

- **SDL2** — zlib license — <https://www.libsdl.org/>
- **FluidSynth** — LGPL v2.1+ — <https://www.fluidsynth.org/>
- **GLib** — LGPL v2.1+ — <https://gitlab.gnome.org/GNOME/glib>
- **PCRE2** — BSD 3-clause — <https://github.com/PCRE2Project/pcre2>
- **libffi** — MIT — <https://github.com/libffi/libffi>
- **zlib** — zlib license — <https://zlib.net/>
- **gettext (libintl)** — LGPL v2.1+ — <https://www.gnu.org/software/gettext/>
- **win-iconv** — public domain — <https://github.com/win-iconv/win-iconv>
- **winpthreads** (mingw-w64) — MIT/BSD — <https://www.mingw-w64.org/>

The mingw-w64 builds of these libraries come from Fedora's mingw packages
(<https://packages.fedoraproject.org/>).

Per LGPL, you can relink the program against modified versions of FluidSynth,
GLib, or gettext: the program's full source is available at the repository
above, and `scripts/build_release_windows.sh` reproduces this executable.
