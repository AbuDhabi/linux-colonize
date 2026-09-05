#!/usr/bin/env bash
# Build a standalone release tarball:
#   dist/linux-colonize-<version>-linux-x86_64.tar.gz
# Layout inside:
#   linux-colonize                      release binary (rpath $ORIGIN/lib)
#   lib/                                bundled SDL2 + FluidSynth (+ deps)
#   data/soundfonts/Roland_SC-55.sf2    bundled soundfont + its COPYRIGHT
#   COLONIZE/put original game files here
#   README.md                           install instructions (scripts/release/README.md)
#   LICENSE                             repo LICENSE
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/.*COLONIZE_VERSION_STRING "\(.*\)".*/\1/p' "$ROOT/src/core/version.h")"
[ -n "$VERSION" ] || { echo "error: could not read version from src/core/version.h" >&2; exit 1; }

BUILD_DIR="$ROOT/build-release"
DIST_ROOT="$ROOT/dist"
PKG_NAME="linux-colonize-$VERSION"
PKG_DIR="$DIST_ROOT/$PKG_NAME"

echo "== Building $PKG_NAME =="

# Release build: debug menu off, rpath so the binary finds ./lib first.
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOLONIZE_DEBUG_MENU=OFF \
  -DCMAKE_EXE_LINKER_FLAGS='-Wl,-rpath,$ORIGIN/lib,--disable-new-dtags'
cmake --build "$BUILD_DIR" --target colonize_linux -j"$(nproc)"

echo "== Assembling package =="
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/lib" "$PKG_DIR/data/soundfonts" "$PKG_DIR/COLONIZE"

cp "$BUILD_DIR/colonize_linux" "$PKG_DIR/linux-colonize"
strip "$PKG_DIR/linux-colonize" || true

# Bundle shared libraries that are not part of every base system.
# Everything else (X11/Wayland, ALSA/Pulse, glib, ...) is expected from the distro.
resolve_lib() {
  # Prefer the copy the binary actually resolved to; fall back to ldconfig.
  local name="$1" path
  path="$(ldd "$PKG_DIR/linux-colonize" | awk -v n="$name" '$1==n {print $3; exit}')"
  if [ -z "${path:-}" ] || [ ! -e "$path" ]; then
    path="$(ldconfig -p | awk -v n="$name" '$1==n {print $NF; exit}')"
  fi
  echo "$path"
}

# libsndfile's codec deps and fluidsynth's libjack are bundled too: their
# sonames differ across distros (e.g. libFLAC.so.12 vs .so.8) or they are not
# installed by default, so the bundled libsndfile/fluidsynth would fail to load.
BUNDLE_LIBS="libSDL2-2.0.so.0 libfluidsynth.so.3 libinstpatch-1.0.so.2 libsndfile.so.1
  libFLAC.so.12 libvorbis.so.0 libvorbisenc.so.2 libopus.so.0 libogg.so.0
  libmpg123.so.0 libmp3lame.so.0 libjack.so.0 libsamplerate.so.0"
for lib in $BUNDLE_LIBS; do
  src="$(resolve_lib "$lib")"
  if [ -n "$src" ] && [ -e "$src" ]; then
    cp -L "$src" "$PKG_DIR/lib/$lib"
    echo "  bundled $lib  ($src)"
  else
    echo "warning: $lib not found on this system; not bundled" >&2
  fi
done

cp "$ROOT/data/soundfonts/Roland_SC-55.sf2" "$PKG_DIR/data/soundfonts/"
cp "$ROOT/data/soundfonts/COPYRIGHT.Roland_SC-55" "$PKG_DIR/data/soundfonts/"

touch "$PKG_DIR/COLONIZE/put original game files here"

cp "$ROOT/scripts/release/README.md" "$PKG_DIR/README.md"
cp "$ROOT/LICENSE" "$PKG_DIR/LICENSE"

echo "== Checking bundled link closure =="
# Everything the binary or a bundled lib needs must resolve from bundle + base
# system libs (X11/Wayland, ALSA/Pulse, glib, ... are expected from the distro).
missing="$(LD_LIBRARY_PATH="$PKG_DIR/lib" ldd "$PKG_DIR/linux-colonize" "$PKG_DIR"/lib/*.so* 2>/dev/null | grep 'not found' | sort -u || true)"
if [ -n "$missing" ]; then
  echo "warning: unresolved libraries remain:" >&2
  echo "$missing" >&2
fi

echo "== Creating tarball =="
TARBALL="$DIST_ROOT/$PKG_NAME-linux-x86_64.tar.gz"
tar -czf "$TARBALL" -C "$DIST_ROOT" "$PKG_NAME"

echo "== Done =="
echo "Package dir: $PKG_DIR"
echo "Tarball:     $TARBALL"
