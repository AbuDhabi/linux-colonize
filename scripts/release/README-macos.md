# OpenCol

OpenCol, port of **Sid Meier's Colonization** (MicroProse, 1994 DOS).

This is the macOS build for **Apple Silicon** (M1 or newer). It needs
macOS @MINOS@ or newer. SDL2 and FluidSynth ship in the `lib/` folder, and the
Roland SC-55 soundfont is included. The package does **not** include the
original game data: you must provide your own copy of the DOS game.

## Installation

1. Unzip the archive anywhere, e.g. in `~/Games`.
2. Copy the contents of the original game's directory (the DOS `COLONIZE`
   folder: `VICEROY.EXE`, `*.SS`, `*.COL`, `*.MP`, `*.TXT`, etc.) into the
   `COLONIZE/` folder inside the unzipped folder. The floppy/CD version 3.0 of
   the game is the reference; GOG's package contains the same files.
3. Allow macOS to open it (first launch only, see below).
4. Double-click `opencol.command`, or run it from Terminal in this folder:

   ```
   ./opencol
   ```

   The game looks for its data in `./COLONIZE` relative to the current
   directory. `opencol.command` changes into its own folder first; when you run
   `./opencol` yourself, start it from the unzipped folder (or pass
   `--data-dir /path/to/COLONIZE`).

## First launch: Gatekeeper

The build is not signed with an Apple Developer ID or notarized (only the
ad-hoc signature that Apple Silicon requires is present), so macOS blocks
files downloaded from the internet. Do one of these:

- Remove the download mark from the whole folder, once, in Terminal (adjust
  the folder name to the version you downloaded):

  ```
  xattr -dr com.apple.quarantine opencol-*-macos-arm64
  ```

  After that `opencol.command` and `./opencol` start without a prompt.
- Or try to open `opencol.command` (double-click), dismiss the warning, then
  open System Settings > Privacy & Security and press **Open Anyway** next to
  the message about `opencol.command`. On macOS 14 and older, right-click >
  **Open** on `opencol.command` also works. macOS may ask once more for the
  game itself and for some of the libraries in `lib/`; allow each.

## Options

- `--data-dir PATH` — use a different game-data directory.
- `--windowed` / `--fullscreen`, `--scale N`, `--nosound` — display/audio options.
- `-K` — hot-seat multiplayer (pick the hidden fifth cell of the nation
  picker, then tick the human powers).
- `-D` — self-running demo; a key quits.
- `settings.json` `sound_options.soundfont` — override the bundled soundfont.
- `settings.json` `mods` — per-feature toggles (`pioneer_plan`,
  `scout_autoexplore`, `ship_autoexplore`, `current_vision`, `hard_king`, ...), also
  on the title menu (MODS for New Games); each campaign keeps the set it started
  with. Gameplay mods are off by default, DOS bug fixes on.

`opencol.command` passes its arguments on to `opencol`.

## Where files go

Everything is written inside the unzipped folder, nothing elsewhere:

- Saves (`COLONY0n.SAV`, compatible with the DOS game): `COLONIZE/`
- Preferences: `settings.json`, created on first run
- Log: `opencol.log`, rewritten on each start. Attach it when reporting a problem.

To move or back up your games, copy the folder.

## Map editor

`./opencol-mapedit` is the original map editor (MAPEDIT.EXE). It reads and
writes `.MP` maps in the data directory and needs `MAPEDIT.EXE`,
`MAPEDIT.TXT` and `MAPMENU.TXT` from the original game in `COLONIZE/`.
Options: `--data-dir PATH`, `--scale N`, `-c` (new map), `-m:FILE.MP` (edit
that map), `?` (usage). Run it from this folder too.

## Troubleshooting

- **"cannot be opened because the developer cannot be verified" / "is
  damaged"**: see "First launch: Gatekeeper" above.
- **Intel Macs**: not supported by this build.
- **No music**: check that `data/soundfonts/Roland_SC-55.sf2` is present, or
  set `sound_options.soundfont` in `settings.json`.
- **Crash or no window at start**: read `opencol.log` in this folder.

## Licenses

- **opencol** (the binaries and this package's original files):
  PolyForm Noncommercial 1.0.0 — see `LICENSE`. Full source:
  <https://github.com/AbuDhabi/linux-colonize>
- **Roland_SC-55.sf2** soundfont: GPL v3 or later, Copyright (c) 2015
  deemster — see `data/soundfonts/COPYRIGHT.Roland_SC-55`.
- The original game content you copy into `COLONIZE/` remains the property of
  its rights holders and is not distributed here.

The dynamic libraries in `lib/` are FluidSynth (built from source, minimal
configuration, source URL in `THIRD_PARTY_LICENSES`) and, from Homebrew, SDL2
(via sdl2-compat) together with SDL3, plus GLib and the libraries it needs.
Their license texts are in `THIRD_PARTY_LICENSES`, one entry per Homebrew
formula and version; each entry names the formula page on
<https://formulae.brew.sh/>, which links the upstream homepage and source.
`THIRD_PARTY_LICENSES` also carries the notice of TinySoundFont, which is
compiled into the program.

The libraries are separate files, so you can relink the program against
modified versions of the LGPL ones by replacing the file in `lib/` (re-sign it
afterwards with `codesign --force -s - lib/<file>`). The program's full source
is available at the repository above, and the release build script
(`scripts/build_release_macos.sh`) reproduces this package.
