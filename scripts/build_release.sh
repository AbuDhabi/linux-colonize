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

echo "== Third-party license texts =="
# The statically linked libraries' licenses require shipping their license
# texts and keeping sources obtainable; exact versions/URLs come from
# build_static_deps.sh, texts from the extracted source trees in DEPS_PREFIX.
eval "$(grep -E '^(SDL2|FLUID|ZLIB|FFI|PCRE2|GLIB)_(VER|URL)=' "$ROOT/scripts/build_static_deps.sh")"
TPL="$PKG_DIR/THIRD_PARTY_LICENSES"
: > "$TPL"
add_license() {
  local name="$1" ver="$2" home="$3" url="$4" file="$5"
  {
    echo "================================================================"
    echo "$name $ver"
    echo "Homepage: $home"
    echo "Source:   $url"
    echo "================================================================"
    echo
    cat "$file"
    echo
  } >> "$TPL"
  echo "  $name $ver"
}
S="$DEPS_PREFIX/src"
add_license "SDL2"       "$SDL2_VER"  "https://www.libsdl.org/"                  "$SDL2_URL"  "$S/SDL2-$SDL2_VER/LICENSE.txt"
add_license "FluidSynth" "$FLUID_VER" "https://www.fluidsynth.org/"              "$FLUID_URL" "$S/fluidsynth-$FLUID_VER/LICENSE"
if [ -d "$S/glib-$GLIB_VER" ]; then
  # Present only when the glib stack was source-built (the release container
  # path); distro-static-glib builds are for local testing, not distribution.
  add_license "GLib"   "$GLIB_VER"  "https://gitlab.gnome.org/GNOME/glib"      "$GLIB_URL"  "$S/glib-$GLIB_VER/COPYING"
  add_license "PCRE2"  "$PCRE2_VER" "https://github.com/PCRE2Project/pcre2"    "$PCRE2_URL" "$S/pcre2-$PCRE2_VER/LICENCE"
  add_license "libffi" "$FFI_VER"   "https://github.com/libffi/libffi"         "$FFI_URL"   "$S/libffi-$FFI_VER/LICENSE"
  add_license "zlib"   "$ZLIB_VER"  "https://zlib.net/"                        "$ZLIB_URL"  "$S/zlib-$ZLIB_VER/LICENSE"
else
  echo "warning: glib stack not source-built here — THIRD_PARTY_LICENSES incomplete (test build only)" >&2
fi

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
