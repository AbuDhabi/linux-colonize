#!/usr/bin/env bash
# Build standalone macOS (Apple Silicon) release zip (dist/opencol-<version>-macos-arm64.zip).
# Usage: bash scripts/build_release_macos.sh
# External requirements: Xcode command line tools and
#   brew install cmake pkgconf sdl2 fluid-synth dylibbundler
# Unlike the Linux kit, SDL2/FluidSynth are linked dynamically; the Homebrew
# dylibs are copied into lib/ and their install names rewritten to
# @executable_path/lib. Runs on bash 3.2 (the macOS system bash).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/.*COLONIZE_VERSION_STRING "\(.*\)".*/\1/p' "$ROOT/src/core/version.h")"
[ -n "$VERSION" ] || { echo "error: could not read version from src/core/version.h" >&2; exit 1; }

BREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
BUILD_DIR="$ROOT/build-release-macos"
DIST_ROOT="$ROOT/dist"
PKG_NAME="opencol-$VERSION-macos-arm64"
PKG_DIR="$DIST_ROOT/$PKG_NAME"
DEPS_LIST="$BUILD_DIR/macos-deps.txt"

echo "== Checking prerequisites =="
need() {
  echo "error: $1 is missing. Install the build dependencies with:" >&2
  echo "  brew install cmake pkgconf sdl2 fluid-synth dylibbundler" >&2
  exit 1
}
[ "$(uname -s)-$(uname -m)" = "Darwin-arm64" ] || { echo "error: this script builds for Apple Silicon (Darwin arm64) only" >&2; exit 1; }
for tool in cmake pkg-config dylibbundler otool vtool install_name_tool codesign ditto realpath; do
  command -v "$tool" >/dev/null 2>&1 || need "$tool"
done
pkg-config --exists sdl2 || need "sdl2 (pkg-config)"
pkg-config --exists fluidsynth || need "fluid-synth (pkg-config)"

echo "== Building $PKG_NAME =="
# Fresh configure: a stale cache could keep pkg-config results of another prefix.
# SKIP_RPATH: CMake would otherwise bake -L/opt/homebrew/lib into the binaries
# as an LC_RPATH, which the sanity step below rejects.
rm -f "$BUILD_DIR/CMakeCache.txt"
cmake -S "$ROOT" -B "$BUILD_DIR" \
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
# Transitive closure of Homebrew dylibs the binaries load. The list file is its
# own work queue: line N is scanned, newly found libs are appended.
printf '%s\n' "$PKG_DIR/opencol" "$PKG_DIR/opencol-mapedit" > "$DEPS_LIST"
i=1
while [ "$i" -le "$(wc -l < "$DEPS_LIST")" ]; do
  f="$(awk -v n="$i" 'NR==n' "$DEPS_LIST")"
  while IFS= read -r dep; do
    real="$(realpath "$dep")"
    grep -qxF "$real" "$DEPS_LIST" || echo "$real" >> "$DEPS_LIST"
  done < <(otool -L "$f" | awk -v p="$BREW_PREFIX/" 'NR>1 && index($1,p)==1 {print $1}')
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
  realpath "$SDL3_SRC" >> "$DEPS_LIST"
fi
tail -n +3 "$DEPS_LIST" > "$DEPS_LIST.libs"

echo "== Bundling dylibs =="
# </dev/null: an unresolvable @rpath dependency must fail, not wait for input.
dylibbundler -od -b \
  -x "$PKG_DIR/opencol" -x "$PKG_DIR/opencol-mapedit" \
  -d "$PKG_DIR/lib" -p @executable_path/lib/ \
  -s "$BREW_PREFIX/lib" </dev/null

if [ "$NEED_SDL3" = yes ]; then
  cp "$SDL3_SRC" "$PKG_DIR/lib/libSDL3.dylib"
  chmod u+w "$PKG_DIR/lib/libSDL3.dylib"
  install_name_tool -id @executable_path/lib/libSDL3.dylib "$PKG_DIR/lib/libSDL3.dylib"
fi

echo "== Third-party license texts =="
# One entry per Homebrew keg (formula + version) among the bundled dylibs; the
# texts are the license files Homebrew keeps at the top of each keg.
KEGS="$DEPS_LIST.kegs"
: > "$KEGS"
while IFS= read -r real; do
  rel="${real#"$BREW_PREFIX"/Cellar/}"
  [ "$rel" != "$real" ] || { echo "error: bundled lib outside the Homebrew Cellar: $real" >&2; exit 1; }
  formula="${rel%%/*}"
  ver="${rel#*/}"
  echo "$formula ${ver%%/*}" >> "$KEGS"
done < "$DEPS_LIST.libs"
sort -u -o "$KEGS" "$KEGS"

TPL="$PKG_DIR/THIRD_PARTY_LICENSES"
: > "$TPL"
while read -r formula ver; do
  keg="$BREW_PREFIX/Cellar/$formula/$ver"
  found=no
  while IFS= read -r file; do
    found=yes
    {
      echo "================================================================"
      echo "$formula $ver ($(basename "$file"))"
      echo "Homebrew formula: https://formulae.brew.sh/formula/$formula"
      echo "================================================================"
      echo
      cat "$file"
      echo
    } >> "$TPL"
  done < <(find "$keg" -maxdepth 1 -type f \( -iname 'LICEN[CS]E*' -o -iname 'COPYING*' -o -iname 'COPYRIGHT*' -o -iname 'UNLICENSE*' -o -iname 'NOTICE*' \) | sort)
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
  rpaths="$(otool -l "$f" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' | grep -E '/opt/homebrew|/usr/local' || true)"
  if [ -n "$bad" ] || [ -n "$rpaths" ]; then
    echo "error: ${f#"$PKG_DIR"/} still references non-bundled paths:" >&2
    echo "$bad" "$rpaths" >&2
    exit 1
  fi
  [ "$(lipo -archs "$f")" = "arm64" ] || { echo "error: ${f#"$PKG_DIR"/} is not arm64-only" >&2; exit 1; }
  vtool -show-build "$f" | awk '/minos/ {print $2}' >> "$MINOS_LIST"
done
if [ "$NEED_SDL3" = yes ]; then
  [ -f "$PKG_DIR/lib/libSDL3.dylib" ] || { echo "error: lib/libSDL3.dylib missing" >&2; exit 1; }
fi
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
