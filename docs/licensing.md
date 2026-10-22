# Licensing

**opencol code** is [PolyForm Noncommercial 1.0.0](../LICENSE)
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
[`data/soundfonts/COPYRIGHT.Roland_SC-55`](../data/soundfonts/COPYRIGHT.Roland_SC-55).
The port runs without it.

**TinySoundFont** (`src/third_party/tsf.h`) by Bernhard Schelling is
**MIT** licensed; the license text is in the file header.

Release packages statically link further libraries (SDL2, FluidSynth, GLib,
…); their licenses are listed in the package README
(`scripts/release/README*.md`) and `THIRD_PARTY_LICENSES`.
