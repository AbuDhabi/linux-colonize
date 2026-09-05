#!/usr/bin/env bash
# Build static SDL2 and a minimal static FluidSynth into a local prefix, for
# the single-binary release (CMake option COLONIZE_STATIC_DEPS=ON).
#
# FluidSynth is used purely as a synth (fluid_synth_write_s16 into SDL's audio
# callback), so every driver and file-format backend is disabled — no
# libsndfile, jack, pulse, etc. Its only remaining deps are glib/gthread.
# Where the distro ships static glib archives (Debian/Ubuntu libglib2.0-dev)
# those are used; otherwise (e.g. the manylinux2014 release container) glib
# and its deps (zlib, libffi, pcre2) are built static from source — that path
# needs meson+ninja on PATH.
#
# Usage: build_static_deps.sh [PREFIX]   (default: <repo>/deps-static)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${1:-$ROOT/deps-static}"
SRC="$PREFIX/src"
JOBS="$(nproc)"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"

SDL2_VER=2.30.11
FLUID_VER=2.3.5
ZLIB_VER=1.3.1
FFI_VER=3.4.6
PCRE2_VER=10.43
GLIB_VER=2.78.6
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-$SDL2_VER.tar.gz"
FLUID_URL="https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v$FLUID_VER.tar.gz"
ZLIB_URL="https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.gz"
FFI_URL="https://github.com/libffi/libffi/releases/download/v$FFI_VER/libffi-$FFI_VER.tar.gz"
PCRE2_URL="https://github.com/PCRE2Project/pcre2/releases/download/pcre2-$PCRE2_VER/pcre2-$PCRE2_VER.tar.gz"
GLIB_URL="https://download.gnome.org/sources/glib/${GLIB_VER%.*}/glib-$GLIB_VER.tar.xz"

mkdir -p "$SRC"

fetch() {
  local url="$1" out="$2"
  [ -f "$out" ] || curl -fL --retry 3 -o "$out" "$url"
}

have_static_glib() {
  pkg-config --exists 'glib-2.0 >= 2.6' gthread-2.0 || return 1
  local libdir
  libdir="$(pkg-config --variable=libdir glib-2.0)"
  [ -f "$libdir/libglib-2.0.a" ] && [ -f "$libdir/libgthread-2.0.a" ]
}

if have_static_glib; then
  echo "== Static glib found on system — skipping glib stack =="
else
  echo "== No static glib — building zlib/libffi/pcre2/glib from source =="
  command -v meson >/dev/null || { echo "error: meson required for glib build" >&2; exit 1; }
  command -v ninja >/dev/null || { echo "error: ninja required for glib build" >&2; exit 1; }

  echo "== zlib $ZLIB_VER (static) =="
  fetch "$ZLIB_URL" "$SRC/zlib-$ZLIB_VER.tar.gz"
  tar -xzf "$SRC/zlib-$ZLIB_VER.tar.gz" -C "$SRC"
  (cd "$SRC/zlib-$ZLIB_VER" && ./configure --prefix="$PREFIX" --static && make -j"$JOBS" && make install)

  echo "== libffi $FFI_VER (static) =="
  fetch "$FFI_URL" "$SRC/libffi-$FFI_VER.tar.gz"
  tar -xzf "$SRC/libffi-$FFI_VER.tar.gz" -C "$SRC"
  (cd "$SRC/libffi-$FFI_VER" && ./configure --prefix="$PREFIX" --disable-shared --enable-static --disable-docs && make -j"$JOBS" && make install)

  echo "== pcre2 $PCRE2_VER (static) =="
  fetch "$PCRE2_URL" "$SRC/pcre2-$PCRE2_VER.tar.gz"
  tar -xzf "$SRC/pcre2-$PCRE2_VER.tar.gz" -C "$SRC"
  (cd "$SRC/pcre2-$PCRE2_VER" && ./configure --prefix="$PREFIX" --disable-shared --enable-static && make -j"$JOBS" && make install)

  echo "== glib $GLIB_VER (static) =="
  fetch "$GLIB_URL" "$SRC/glib-$GLIB_VER.tar.xz"
  tar -xJf "$SRC/glib-$GLIB_VER.tar.xz" -C "$SRC"
  meson setup "$SRC/glib-build" "$SRC/glib-$GLIB_VER" \
    --prefix="$PREFIX" \
    --buildtype=release \
    --default-library=static \
    -Dlibmount=disabled \
    -Dselinux=disabled \
    -Dxattr=false \
    -Dman=false \
    -Dtests=false
  ninja -C "$SRC/glib-build" -j"$JOBS"
  ninja -C "$SRC/glib-build" install
fi

echo "== SDL2 $SDL2_VER (static) =="
fetch "$SDL2_URL" "$SRC/SDL2-$SDL2_VER.tar.gz"
tar -xzf "$SRC/SDL2-$SDL2_VER.tar.gz" -C "$SRC"
cmake -S "$SRC/SDL2-$SDL2_VER" -B "$SRC/SDL2-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON \
  -DSDL_TEST=OFF
cmake --build "$SRC/SDL2-build" -j"$JOBS"
cmake --install "$SRC/SDL2-build"

echo "== FluidSynth $FLUID_VER (static, minimal) =="
fetch "$FLUID_URL" "$SRC/fluidsynth-$FLUID_VER.tar.gz"
tar -xzf "$SRC/fluidsynth-$FLUID_VER.tar.gz" -C "$SRC"
cmake -S "$SRC/fluidsynth-$FLUID_VER" -B "$SRC/fluidsynth-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_SHARED_LIBS=OFF \
  -Denable-libsndfile=OFF \
  -Denable-jack=OFF \
  -Denable-pulseaudio=OFF \
  -Denable-pipewire=OFF \
  -Denable-alsa=OFF \
  -Denable-oss=OFF \
  -Denable-dbus=OFF \
  -Denable-readline=OFF \
  -Denable-sdl2=OFF \
  -Denable-network=OFF \
  -Denable-aufile=OFF \
  -Denable-libinstpatch=OFF \
  -Denable-lash=OFF \
  -Denable-openmp=OFF
cmake --build "$SRC/fluidsynth-build" -j"$JOBS"
cmake --install "$SRC/fluidsynth-build"

echo "== Done: static deps in $PREFIX =="
ls "$PREFIX"/lib*/libSDL2.a "$PREFIX"/lib*/libfluidsynth.a
