#!/usr/bin/env bash
# Build standalone macOS (Apple Silicon) release zip (dist/opencol-<version>-macos-arm64.zip).
# Usage: bash scripts/build_release_macos.sh
# External requirements: Xcode command line tools, network access (FluidSynth
# source download) and
#   brew install cmake pkgconf sdl2 glib dylibbundler
# Unlike the Linux kit, SDL2 and FluidSynth are linked dynamically; the dylibs
# are copied into lib/ and their install names rewritten to @executable_path/lib.
# FluidSynth is built from source (same version and minimal feature set as
# build_static_deps.sh) so Homebrew's fluid-synth, with its readline (GPL),
# libsndfile and codec chain, is not bundled.
# Runs on bash 3.2 (the macOS system bash).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
VERSION="$(sed -n 's/.*COLONIZE_VERSION_STRING "\(.*\)".*/\1/p' "$ROOT/src/core/version.h")"
[ -n "$VERSION" ] || { echo "error: could not read version from src/core/version.h" >&2; exit 1; }

BREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
BUILD_DIR="$ROOT/build-release-macos"
DIST_ROOT="$ROOT/dist"
PKG_NAME="opencol-$VERSION-macos-arm64"
PKG_DIR="$DIST_ROOT/$PKG_NAME"
DEPS_LIST="$BUILD_DIR/macos-deps.txt"
DEPS_PREFIX="$ROOT/deps-macos"
FS_SRC="$DEPS_PREFIX/src"

# FLUID_VER / FLUID_URL: one pin for all platforms, read (not sourced) from
# build_static_deps.sh. The URL line refers to $FLUID_VER, expanded below.
read_pin() {
  sed -n "/^$1=/{s/^$1=//;s/\"//g;p;q;}" "$ROOT/scripts/build_static_deps.sh"
}
FLUID_VER="$(read_pin FLUID_VER)"
FLUID_URL_RAW="$(read_pin FLUID_URL)"
[ -n "$FLUID_VER" ] || { echo "error: FLUID_VER not found in scripts/build_static_deps.sh" >&2; exit 1; }
[ -n "$FLUID_URL_RAW" ] || { echo "error: FLUID_URL not found in scripts/build_static_deps.sh" >&2; exit 1; }
FLUID_URL="${FLUID_URL_RAW//\$FLUID_VER/$FLUID_VER}"
# SHA-256 of the tarball at $FLUID_URL for the version below (GitHub's v2.3.5
# tag archive, hashed on 2026-11-03). Bumping FLUID_VER in build_static_deps.sh
# requires updating both lines here.
FLUID_SHA256_VER=2.3.5
FLUID_SHA256=f89e8e983ecfb4a5b4f5d8c2b9157ed18d15ed2e36246fa782f18abaea550e0d
[ "$FLUID_VER" = "$FLUID_SHA256_VER" ] || {
  echo "error: build_static_deps.sh pins FluidSynth $FLUID_VER but this script only knows the SHA-256 of $FLUID_SHA256_VER." >&2
  echo "       Download $FLUID_URL, check it, and update FLUID_SHA256_VER and FLUID_SHA256 in scripts/build_release_macos.sh." >&2
  exit 1
}

# Homebrew keg / source origin of a library: "formula version" or "FluidSynth <ver>".
origin_of() {
  local real="$1" rel ver
  case "$real" in
    "$DEPS_PREFIX"/*) echo "FluidSynth $FLUID_VER" ;;
    "$BREW_PREFIX"/Cellar/*)
      rel="${real#"$BREW_PREFIX"/Cellar/}"
      ver="${rel#*/}"
      echo "${rel%%/*} ${ver%%/*}"
      ;;
    *) echo "error: bundled lib with unknown origin: $real" >&2; return 1 ;;
  esac
}

# License files Homebrew keeps at the top of a keg.
license_files() {
  find "$1" -maxdepth 1 -type f \( -iname 'LICEN[CS]E*' -o -iname 'COPYING*' -o -iname '*GPL*.txt' -o -iname 'COPYRIGHT*' -o -iname 'UNLICENSE*' -o -iname 'NOTICE*' \) | sort
}

echo "== Checking prerequisites =="
need() {
  echo "error: $1 is missing. Install the build dependencies with:" >&2
  echo "  brew install cmake pkgconf sdl2 glib dylibbundler" >&2
  exit 1
}
[ "$(uname -s)-$(uname -m)" = "Darwin-arm64" ] || { echo "error: this script builds for Apple Silicon (Darwin arm64) only" >&2; exit 1; }
for tool in cmake pkg-config dylibbundler otool vtool install_name_tool codesign ditto realpath; do
  command -v "$tool" >/dev/null 2>&1 || need "$tool"
done
pkg-config --exists sdl2 || need "sdl2 (pkg-config)"
# FluidSynth 2.3.5 requires glib-2.0 and gthread-2.0 (find_package(GLib2 REQUIRED)).
pkg-config --exists glib-2.0 gthread-2.0 || need "glib (pkg-config)"

echo "== FluidSynth $FLUID_VER (shared, minimal) =="
# Same switches as build_static_deps.sh, plus the ones that default ON on macOS
# or are not on Linux: portaudio is off by default, ladspa/midishare are
# disabled, enable-framework=OFF gives a plain dylib. CoreAudio and CoreMIDI
# stay on.
mkdir -p "$FS_SRC"
if [ ! -f "$FS_SRC/fluidsynth-$FLUID_VER.tar.gz" ]; then
  curl -fL --retry 3 -o "$FS_SRC/fluidsynth-$FLUID_VER.tar.gz.part" "$FLUID_URL"
  mv "$FS_SRC/fluidsynth-$FLUID_VER.tar.gz.part" "$FS_SRC/fluidsynth-$FLUID_VER.tar.gz"
fi
# Verified on every run, including a cached download, before anything is extracted.
echo "$FLUID_SHA256  $FS_SRC/fluidsynth-$FLUID_VER.tar.gz" | shasum -a 256 -c - >/dev/null || {
  echo "error: SHA-256 mismatch for $FS_SRC/fluidsynth-$FLUID_VER.tar.gz (expected $FLUID_SHA256)" >&2
  echo "       Delete it to re-download; if $FLUID_URL changed on purpose, update FLUID_SHA256." >&2
  exit 1
}
rm -rf "$FS_SRC/fluidsynth-$FLUID_VER" "$FS_SRC/fluidsynth-build"
tar -xzf "$FS_SRC/fluidsynth-$FLUID_VER.tar.gz" -C "$FS_SRC"
[ -f "$FS_SRC/fluidsynth-$FLUID_VER/LICENSE" ] || { echo "error: no LICENSE in the extracted FluidSynth $FLUID_VER source" >&2; exit 1; }
# FluidSynth's nested gentables sub-build also has an old cmake_minimum_required;
# only the environment variable (not -D) reaches it under CMake 4.
export CMAKE_POLICY_VERSION_MINIMUM=3.5
# CMAKE_INSTALL_NAME_DIR: absolute install name, so the binaries record
# $DEPS_PREFIX/lib/libfluidsynth.3.dylib (not @rpath/...) and the library scan
# below can follow it.
cmake -S "$FS_SRC/fluidsynth-$FLUID_VER" -B "$FS_SRC/fluidsynth-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$DEPS_PREFIX" \
  -DCMAKE_INSTALL_NAME_DIR="$DEPS_PREFIX/lib" \
  -DBUILD_SHARED_LIBS=ON \
  -Denable-framework=OFF \
  -Denable-coreaudio=ON \
  -Denable-coremidi=ON \
  -Denable-libsndfile=OFF \
  -Denable-jack=OFF \
  -Denable-pulseaudio=OFF \
  -Denable-pipewire=OFF \
  -Denable-oss=OFF \
  -Denable-dbus=OFF \
  -Denable-readline=OFF \
  -Denable-sdl2=OFF \
  -Denable-network=OFF \
  -Denable-aufile=OFF \
  -Denable-libinstpatch=OFF \
  -Denable-openmp=OFF \
  -Denable-portaudio=OFF \
  -Denable-ladspa=OFF \
  -Denable-midishare=OFF
cmake --build "$FS_SRC/fluidsynth-build" --parallel "$(sysctl -n hw.ncpu)"
cmake --install "$FS_SRC/fluidsynth-build"
unset CMAKE_POLICY_VERSION_MINIMUM

echo "== Building $PKG_NAME =="
# Fresh configure: a stale cache could keep pkg-config results of another prefix.
# PKG_CONFIG_PATH puts the FluidSynth built above ahead of any Homebrew one.
# SKIP_RPATH: CMake would otherwise bake -L/opt/homebrew/lib into the binaries
# as an LC_RPATH, which the sanity step below rejects.
rm -f "$BUILD_DIR/CMakeCache.txt"
PKG_CONFIG_PATH="$DEPS_PREFIX/lib/pkgconfig" cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOLONIZE_DEBUG_MENU=OFF \
  -DCMAKE_SKIP_RPATH=ON
cmake --build "$BUILD_DIR" --target opencol opencol-mapedit --parallel "$(sysctl -n hw.ncpu)"

echo "== Assembling package =="
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/data/soundfonts" "$PKG_DIR/COLONIZE"

for bin in opencol opencol-mapedit; do
  cp "$BUILD_DIR/$bin" "$PKG_DIR/$bin"
  strip -x "$PKG_DIR/$bin" || true
done

cp "$ROOT/data/soundfonts/Roland_SC-55.sf2" "$PKG_DIR/data/soundfonts/"
cp "$ROOT/data/soundfonts/COPYRIGHT.Roland_SC-55" "$PKG_DIR/data/soundfonts/"

touch "$PKG_DIR/COLONIZE/put original game files here"

cp "$ROOT/LICENSE" "$PKG_DIR/LICENSE"

# The game resolves ./COLONIZE, settings.json, opencol.log and saves relative to
# the current directory on macOS, so the launcher cd's to its own folder first.
cat > "$PKG_DIR/opencol.command" <<'EOF'
#!/bin/bash
cd "$(dirname "$0")" || exit 1
exec ./opencol "$@"
EOF
chmod +x "$PKG_DIR/opencol.command"

echo "== Resolving bundled libraries =="
# Transitive closure of the Homebrew and deps-macos dylibs the binaries load.
# The list file is its own work queue: line N is scanned, new libs are appended.
# LIBMAP records "<file name in lib/>|<origin>" per library;
# the license step and the final sanity check work from it.
LIBMAP="$DEPS_LIST.map"
: > "$LIBMAP"
printf '%s\n' "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit" > "$DEPS_LIST"
i=1
while [ "$i" -le "$(wc -l < "$DEPS_LIST")" ]; do
  f="$(awk -v n="$i" 'NR==n' "$DEPS_LIST")"
  # Captured first, so an otool failure stops the script (set -e).
  otool_out="$(otool -L "$f")"
  found="$(printf '%s\n' "$otool_out" | awk -v p="$BREW_PREFIX/" -v q="$DEPS_PREFIX/" 'NR>1 && (index($1,p)==1 || index($1,q)==1) {print $1}')"
  if [ -n "$found" ]; then
    while IFS= read -r dep; do
      real="$(realpath "$dep")"
      origin="$(origin_of "$real")"
      grep -qxF "$real" "$DEPS_LIST" || echo "$real" >> "$DEPS_LIST"
      # dylibbundler copies a symlinked lib under its real file name
      # (libfluidsynth.3.dylib -> libfluidsynth.3.2.3.dylib), so map both names.
      for n in "${dep##*/}" "${real##*/}"; do
        grep -qxF "$n|$origin" "$LIBMAP" || echo "$n|$origin" >> "$LIBMAP"
      done
    done <<EOF
$found
EOF
  fi
  i=$((i + 1))
done

# Homebrew's sdl2 is sdl2-compat: it links no SDL3, it dlopen()s libSDL3.dylib
# at runtime (@loader_path/libSDL3.dylib, @executable_path/libSDL3.dylib, ...),
# so dylibbundler cannot see it. Ship it next to the bundled libSDL2 in lib/.
NEED_SDL3=no
if grep -q "/Cellar/sdl2-compat/" "$DEPS_LIST"; then
  NEED_SDL3=yes
  SDL3_SRC="$BREW_PREFIX/opt/sdl3/lib/libSDL3.0.dylib"
  [ -f "$SDL3_SRC" ] || { echo "error: sdl2-compat is installed but $SDL3_SRC is missing (brew install sdl3)" >&2; exit 1; }
  sdl3_real="$(realpath "$SDL3_SRC")"
  sdl3_origin="$(origin_of "$sdl3_real")"
  echo "libSDL3.dylib|$sdl3_origin" >> "$LIBMAP"
fi

echo "== Bundling dylibs =="
# </dev/null: an unresolvable @rpath dependency must fail, not wait for input.
dylibbundler -od -b \
  -x "$PKG_DIR/opencol" -x "$PKG_DIR/opencol-mapedit" \
  -d "$PKG_DIR/lib" -p @executable_path/lib/ \
  -s "$DEPS_PREFIX/lib" -s "$BREW_PREFIX/lib" </dev/null

if [ "$NEED_SDL3" = yes ]; then
  cp "$SDL3_SRC" "$PKG_DIR/lib/libSDL3.dylib"
  chmod u+w "$PKG_DIR/lib/libSDL3.dylib"
  install_name_tool -id @executable_path/lib/libSDL3.dylib "$PKG_DIR/lib/libSDL3.dylib"
fi

# Drop every LC_RPATH. sdl2-compat carries @loader_path/../../../../opt/sdl3/lib,
# a Homebrew-relative path that points outside this package; SDL3 is found via
# its explicit @loader_path/libSDL3.dylib lookup in lib/ instead.
for f in "$PKG_DIR"/lib/*.dylib "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit"; do
  rpaths="$(otool -l "$f" | awk '/cmd LC_RPATH/ {getline; getline; print $2}')"
  if [ -n "$rpaths" ]; then
    while IFS= read -r rp; do
      install_name_tool -delete_rpath "$rp" "$f"
    done <<EOF
$rpaths
EOF
  fi
done

echo "== Third-party license texts =="
# One entry per Homebrew keg (formula + version) among the bundled dylibs; the
# texts are the license files Homebrew keeps at the top of each keg. FluidSynth
# (deps-macos) takes its text from the extracted source tree, as on Linux.
KEGS="$DEPS_LIST.kegs"
{ cut -d'|' -f2 "$LIBMAP" | grep -v '^FluidSynth ' || true; } | sort -u > "$KEGS"

TPL="$PKG_DIR/THIRD_PARTY_LICENSES"
: > "$TPL"
{
  echo "================================================================"
  echo "FluidSynth $FLUID_VER"
  echo "Homepage: https://www.fluidsynth.org/"
  echo "Source:   $FLUID_URL"
  echo "================================================================"
  echo
  cat "$FS_SRC/fluidsynth-$FLUID_VER/LICENSE"
  echo
  # Compiled into the binaries (src/third_party/tsf.h); the notice is quoted
  # from the file header.
  echo "================================================================"
  echo "TinySoundFont (src/third_party/tsf.h)"
  echo "Homepage: https://github.com/schellingb/TinySoundFont"
  echo "================================================================"
  echo
  awk '/LICENSE \(MIT\)/ {p=1} p && /^\*\// {exit} p' "$ROOT/src/third_party/tsf.h"
  echo
} >> "$TPL"
echo "  fluidsynth $FLUID_VER"
echo "  TinySoundFont"
while read -r formula ver; do
  keg="$BREW_PREFIX/Cellar/$formula/$ver"
  found=no
  if [ "$formula" = gettext ]; then
    # Only libintl (gettext-runtime, LGPL-2.1-or-later) is bundled. The keg's
    # COPYING is the GPL-3 text of the gettext tools, which do not ship here,
    # so the LGPL-2.1 text is taken from the glib keg instead.
    glib_ver="$(awk '$1=="glib" {print $2; exit}' "$KEGS")"
    lgpl=""
    if [ -n "$glib_ver" ]; then
      while IFS= read -r file; do
        [ -n "$file" ] || continue
        if grep -q 'GNU LESSER GENERAL PUBLIC LICENSE' "$file" && grep -q 'Version 2.1' "$file"; then
          lgpl="$file"
          break
        fi
      done <<EOF
$(license_files "$BREW_PREFIX/Cellar/glib/$glib_ver")
EOF
    fi
    [ -n "$lgpl" ] || { echo "error: no LGPL-2.1 text found in the glib keg for libintl" >&2; exit 1; }
    found=yes
    {
      echo "================================================================"
      echo "$formula $ver (libintl, gettext-runtime: LGPL-2.1-or-later)"
      echo "Homebrew formula: https://formulae.brew.sh/formula/$formula"
      echo "LGPL-2.1 text: ${lgpl##*/} from the glib $glib_ver keg"
      echo "================================================================"
      echo
      cat "$lgpl"
      echo
    } >> "$TPL"
  else
    while IFS= read -r file; do
      [ -n "$file" ] || continue
      found=yes
      {
        echo "================================================================"
        echo "$formula $ver (${file##*/})"
        echo "Homebrew formula: https://formulae.brew.sh/formula/$formula"
        echo "================================================================"
        echo
        cat "$file"
        echo
      } >> "$TPL"
    done <<EOF
$(license_files "$keg")
EOF
  fi
  [ "$found" = yes ] || { echo "error: no license file found in $keg" >&2; exit 1; }
  echo "  $formula $ver"
done < "$KEGS"

echo "== Signing (ad-hoc) =="
# install_name_tool invalidated the signatures; arm64 refuses unsigned or
# badly signed code. Libraries first, then the executables.
for f in "$PKG_DIR"/lib/*.dylib "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit"; do
  codesign --force -s - "$f"
done
for f in "$PKG_DIR"/lib/*.dylib "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit"; do
  codesign --verify --strict "$f"
done

echo "== Sanity: dynamic deps of the binaries and bundled libs =="
# Every dependency must be a macOS system lib or a lib bundled in lib/. Any
# /opt/homebrew, /usr/local, @rpath or @loader_path reference means a lib was
# missed or the rewrite failed.
MINOS_LIST="$DEPS_LIST.minos"
: > "$MINOS_LIST"
for f in "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit" "$PKG_DIR"/lib/*.dylib; do
  echo "  ${f#"$PKG_DIR"/}:"
  otool -L "$f" | awk 'NR>1 {print "    " $1}'
  bad="$(otool -L "$f" | awk 'NR>1 && $1 !~ /^(\/usr\/lib\/|\/System\/|@executable_path\/lib\/)/ {print $1}')"
  # The package must carry no LC_RPATH at all; everything is @executable_path/lib.
  rpaths="$(otool -l "$f" | awk '/cmd LC_RPATH/ {getline; getline; print "LC_RPATH " $2}')"
  if [ -n "$bad" ] || [ -n "$rpaths" ]; then
    echo "error: ${f#"$PKG_DIR"/} still references non-bundled paths or has an rpath:" >&2
    echo "$bad" "$rpaths" >&2
    exit 1
  fi
  [ "$(lipo -archs "$f")" = "arm64" ] || { echo "error: ${f#"$PKG_DIR"/} is not arm64-only" >&2; exit 1; }
  vtool -show-build "$f" | awk '/minos/ {print $2}' >> "$MINOS_LIST"
done
# Every library in lib/ must come from a known origin with its license text in
# THIRD_PARTY_LICENSES (the origin comes from the scan of the binaries' deps).
for f in "$PKG_DIR"/lib/*.dylib; do
  name="${f##*/}"
  origin="$(awk -F'|' -v n="$name" '$1==n {print $2; exit}' "$LIBMAP")"
  [ -n "$origin" ] || { echo "error: lib/$name has no known origin, so no license text was added" >&2; exit 1; }
  case "$origin" in
    FluidSynth*) pattern="^FluidSynth $FLUID_VER\$" ;;
    *) pattern="^$origin (" ;;
  esac
  grep -q -- "$pattern" "$TPL" || { echo "error: THIRD_PARTY_LICENSES has no entry for lib/$name ($origin)" >&2; exit 1; }
done
if [ "$NEED_SDL3" = yes ]; then
  [ -f "$PKG_DIR/lib/libSDL3.dylib" ] || { echo "error: lib/libSDL3.dylib missing" >&2; exit 1; }
fi
# The minimal FluidSynth must not drag in readline (GPL), libsndfile, PortAudio
# or the codec chain (a Homebrew fluid-synth was picked up if these show up).
for banned in readline sndfile portaudio FLAC vorbis opus mpg123 mp3lame ogg; do
  for f in "$PKG_DIR"/lib/lib*"$banned"*; do
    if [ -e "$f" ]; then
      echo "error: lib/ contains $banned (${f##*/}); FluidSynth was not the minimal source build" >&2
      exit 1
    fi
  done
done
[ -f "$PKG_DIR/lib/libfluidsynth.3.dylib" ] || { echo "error: lib/libfluidsynth.3.dylib missing" >&2; exit 1; }
# The oldest macOS the package loads on is the newest deployment target of any
# binary in it (Homebrew bottles target the macOS of their build machine).
MINOS="$(sort -V "$MINOS_LIST" | tail -n 1)"
echo "  minimum macOS: $MINOS"

# README last: it states the minimum macOS measured above.
README_TEXT="$(cat "$ROOT/scripts/release/README-macos.md")"
printf '%s\n' "${README_TEXT//@MINOS@/$MINOS}" > "$PKG_DIR/README.md"

echo "== Creating zip =="
ZIP="$DIST_ROOT/$PKG_NAME.zip"
rm -f "$ZIP"
# ditto keeps permissions and symlinks; the --no* flags keep ._ files and
# quarantine/provenance attributes out of the archive.
ditto -c -k --keepParent --norsrc --noextattr --noqtn "$PKG_DIR" "$ZIP"

echo "== Done =="
echo "Package dir: $PKG_DIR"
echo "Zip:         $ZIP"
