# Music / MIDI soundfont

Songs play over a GM/GS soundfont through one of two synth backends:
FluidSynth (when built in), or the bundled single-header TinySoundFont
(`src/third_party/tsf.h`, MIT) as fallback. `settings.json`
`sound_options.midi_backend` (`"fluidsynth"` / `"tsf"`, empty = auto) forces
one; `-DCOLONIZE_ENABLE_TSF=OFF` compiles TSF out.
`sound_find_soundfont` (`src/core/sound.c`) picks a soundfont automatically,
in order:

1. `settings.json` `sound_options.soundfont`, if set and readable.
2. The bundled default, `data/soundfonts/Roland_SC-55.sf2` (GPL-3+, see
   `data/soundfonts/COPYRIGHT.Roland_SC-55`) — tried relative to the working
   directory, the executable's own directory, and `--data-dir`.
3. Common system soundfont locations (e.g.
   `/usr/share/sounds/sf2/FluidR3_GM.sf2`, ScummVM's bundled SC-55, etc.).

If none are found, the game logs a warning and falls back to a soft beep
path rather than failing — set `sound_options.soundfont` to point at any
GM-compatible `.sf2` file to override. `--nosound` skips audio entirely.
The bundled bank is optional and separately licensed (GPL-3+, see
[licensing.md](licensing.md)); it is not required to build or to run.
Without any soundfont, music uses the square-wave fallback (no MIDI).

Sound driver emulation and song tables: [assets_sound.md](assets_sound.md).
