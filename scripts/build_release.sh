#!/usr/bin/env bash
# Build a standalone release tarball:
#   dist/linux-colonize-<version>-linux-x86_64.tar.gz
# Layout inside:
#   linux-colonize                      single static-deps binary
#   data/soundfonts/Roland_SC-55.sf2    bundled soundfont + its COPYRIGHT
#   COLONIZE/put original game files here
#   README.md                           install instructions (scripts/release/README.md)
#   LICENSE                             repo LICENSE
#
# SDL2 and a minimal FluidSynth (+ glib) are linked statically into the binary
# (built by scripts/build_static_deps.sh), so no lib/ directory is shipped.
# The only runtime requirements are glibc and the host's X11/Wayland + audio
# stack, which static SDL2 dlopens as usual. The glibc floor is set by the
# build machine — build inside the container (build_release_container.sh) for
# releases.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/.*COLONIZE_VERSION_STRING "\(.*\)".*/\1/p' "$ROOT/src/core/version.h")"
[ -n "$VERSION" ] || { echo "error: could not read version from src/core/version.h" >&2; exit 1; }

# Overridable so the container build can use its own directories without
# clobbering the host's.
BUILD_DIR="$ROOT/${BUILD_DIR:-build-release}"
DEPS_PREFIX="$ROOT/${DEPS_PREFIX:-deps-static}"
DIST_ROOT="$ROOT/dist"
PKG_NAME="linux-colonize-$VERSION"
PKG_DIR="$DIST_ROOT/$PKG_NAME"

if ! ls "$DEPS_PREFIX"/lib*/libSDL2.a "$DEPS_PREFIX"/lib*/libfluidsynth.a >/dev/null 2>&1; then
  echo "== Building static deps (first run) =="
  "$ROOT/scripts/build_static_deps.sh" "$DEPS_PREFIX"
fi

echo "== Building $PKG_NAME =="
# Fresh configure: cached pkg-config results from a system-fluidsynth
# configure would poison the static link line.
rm -f "$BUILD_DIR/CMakeCache.txt"
PKG_CONFIG_PATH="$DEPS_PREFIX/lib/pkgconfig:$DEPS_PREFIX/lib64/pkgconfig" cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOLONIZE_DEBUG_MENU=OFF \
  -DCOLONIZE_STATIC_DEPS=ON \
  -DCMAKE_PREFIX_PATH="$DEPS_PREFIX"
cmake --build "$BUILD_DIR" --target colonize_linux -j"$(nproc)"

echo "== Assembling package =="
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/data/soundfonts" "$PKG_DIR/COLONIZE"

cp "$BUILD_DIR/colonize_linux" "$PKG_DIR/linux-colonize"
strip "$PKG_DIR/linux-colonize" || true

cp "$ROOT/data/soundfonts/Roland_SC-55.sf2" "$PKG_DIR/data/soundfonts/"
cp "$ROOT/data/soundfonts/COPYRIGHT.Roland_SC-55" "$PKG_DIR/data/soundfonts/"

touch "$PKG_DIR/COLONIZE/put original game files here"

cp "$ROOT/scripts/release/README.md" "$PKG_DIR/README.md"
cp "$ROOT/LICENSE" "$PKG_DIR/LICENSE"

echo "== Sanity: dynamic deps of the binary =="
# Must be base system only (libc/libm & friends). Any SDL/fluidsynth/glib/codec
# soname here means static linking silently regressed.
objdump -p "$PKG_DIR/linux-colonize" | awk '/NEEDED/{print "  " $2}'
if objdump -p "$PKG_DIR/linux-colonize" | grep -E "NEEDED.*(SDL|fluid|glib|sndfile|FLAC|vorbis|opus|ogg|mpg123|mp3lame|jack)"; then
  echo "error: binary still links bundled deps dynamically" >&2
  exit 1
fi

echo "== Creating tarball =="
TARBALL="$DIST_ROOT/$PKG_NAME-linux-x86_64.tar.gz"
tar -czf "$TARBALL" -C "$DIST_ROOT" "$PKG_NAME"

echo "== Done =="
echo "Package dir: $PKG_DIR"
echo "Tarball:     $TARBALL"
