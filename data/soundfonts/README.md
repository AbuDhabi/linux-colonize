Roland_SC-55.sf2
=================

This directory is **not** covered by the repository's PolyForm
Noncommercial license. The SoundFont stays GPL-3+ as declared below.
The game binary does not require this file; it is shipped as a MIDI
playback convenience.

Source
------
Bundled copy of the SoundFont shipped with ScummVM as
`dists/soundfonts/Roland_SC-55.sf2` (also packaged as
`/usr/share/scummvm/Roland_SC-55.sf2` on Debian/Ubuntu via `scummvm-data`).

License (as declared by ScummVM / Debian)
-----------------------------------------
Copyright (c) 2015 deemster.
Licensed under the GNU General Public License v3 or later.

See ScummVM's `dists/soundfonts/COPYRIGHT.Roland_SC-55` and the Debian
`scummvm-data` copyright file entry for `dists/soundfonts/Roland_SC-55.sf2`.

Note on embedded metadata
-------------------------
The SF2 INFO chunk names the bank "Sound Canvas Pure.sf2" and contains the
string "Copyright 1996 Roland Corporation U.S.". That string is part of the
file's internal metadata; redistribution here follows ScummVM/Debian's
attribution of the file to deemster under GPL-3+.

Override
--------
Set `COLONIZE_SOUNDFONT` to another `.sf2` (for example Trevor0402's SC-55
SoundFont) if you want a different bank.
