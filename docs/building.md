# Building OpenCol

## Requirements

- CMake ≥ 3.20
- A C11 compiler
- SDL2 (required for the game binary)
- FluidSynth (optional; MIDI / Pick Music — without it the bundled
  TinySoundFont backend plays MIDI instead, see [music.md](music.md))

## Build

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/opencol
```

`make build` does the same (configures `build/debug` on first use). Release
preset: `cmake --preset release` then `cmake --build --preset release`
(or `make release`).

The binary still needs the original game data to run; see
[Running the game](../README.md#running-the-game).

## Tests

`make test` builds and runs the ctest suite; `make test T=<target>` runs one
test binary. Layout and fixtures: [tests/README.md](../tests/README.md).

## Release packages

Standalone release archives (static SDL2 / FluidSynth, bundled soundfont) are
built with the scripts in `scripts/`:

- `scripts/build_release.sh` — Linux tarball
- `scripts/build_release_container.sh` — same, inside a manylinux2014
  container (glibc 2.17 baseline)
- `scripts/build_release_windows.sh` — Windows zip (mingw cross-build)

Output lands in `dist/`. The package READMEs are `scripts/release/README*.md`.
