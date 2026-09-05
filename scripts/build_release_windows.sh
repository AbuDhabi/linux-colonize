#!/usr/bin/env bash
# Cross-build the Windows release zip in a Fedora container (mingw-w64):
#   dist/linux-colonize-<version>-windows-x86_64.zip
# Fedora ships prebuilt static mingw SDL2/glib, so only FluidSynth is
# cross-built here (same minimal config as the Linux release). Everything is
# linked statically (-static) into one console-subsystem colonize.exe.
#
# Requires docker or podman. Output lands in dist/ as usual.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if command -v podman >/dev/null 2>&1; then
  RUNTIME=podman
elif command -v docker >/dev/null 2>&1; then
  RUNTIME=docker
else
  echo "error: need podman or docker" >&2
  exit 1
fi

IMAGE=registry.fedoraproject.org/fedora:40

"$RUNTIME" run --rm \
  -v "$ROOT:/work" \
  -w /work \
  "$IMAGE" \
  bash -euo pipefail -c '
    # mingw64-cmake/-pkg-config wrappers ship in mingw64-filesystem (pulled in
    # by mingw64-gcc); there are no standalone packages of those names.
    dnf install -y -q \
      mingw64-gcc mingw64-gcc-c++ \
      mingw64-SDL2-static mingw64-glib2-static \
      mingw64-winpthreads-static mingw64-pcre2-static mingw64-zlib-static \
      mingw64-gettext-static mingw64-win-iconv-static \
      cmake make curl zip gcc gcc-c++ > /dev/null

    VERSION="$(sed -n '"'"'s/.*COLONIZE_VERSION_STRING "\(.*\)".*/\1/p'"'"' src/core/version.h)"
    eval "$(grep -E "^FLUID_(VER|URL)=" scripts/build_static_deps.sh)"
    DEPS=/work/deps-mingw
    SRC="$DEPS/src"
    mkdir -p "$SRC"

    echo "== FluidSynth $FLUID_VER (mingw static, minimal) =="
    [ -f "$SRC/fluidsynth.tar.gz" ] || curl -fL --retry 3 -o "$SRC/fluidsynth.tar.gz" "$FLUID_URL"
    tar -xzf "$SRC/fluidsynth.tar.gz" -C "$SRC"
    # The mingw64-cmake wrapper supplies its own source-dir argument, which
    # overrides -S — configure from inside the source directory instead.
    # Install into the mingw sysroot (the wrapper default prefix): the cross
    # pkg-config prepends PKG_CONFIG_SYSROOT_DIR to paths, so a prefix outside
    # the sysroot comes back mangled and headers are never found.
    ( cd "$SRC/fluidsynth-$FLUID_VER" && mingw64-cmake -B "$SRC/fluidsynth-build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_C_FLAGS="-DGLIB_STATIC_COMPILATION" \
      -Denable-libsndfile=OFF -Denable-jack=OFF -Denable-pulseaudio=OFF \
      -Denable-pipewire=OFF -Denable-alsa=OFF -Denable-oss=OFF \
      -Denable-dbus=OFF -Denable-readline=OFF -Denable-sdl2=OFF \
      -Denable-network=OFF -Denable-aufile=OFF -Denable-libinstpatch=OFF \
      -Denable-lash=OFF -Denable-openmp=OFF \
      -Denable-dsound=OFF -Denable-wasapi=OFF -Denable-waveout=OFF \
      -Denable-winmidi=OFF )
    cmake --build "$SRC/fluidsynth-build" -j"$(nproc)"
    cmake --install "$SRC/fluidsynth-build"

    echo "== Game (mingw, static) =="
    mingw64-cmake -B build-windows \
      -DCMAKE_BUILD_TYPE=Release \
      -DCOLONIZE_DEBUG_MENU=OFF \
      -DCOLONIZE_STATIC_DEPS=ON \
      -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc"
    cmake --build build-windows --target colonize_linux -j"$(nproc)"

    echo "== Packaging =="
    PKG="dist/linux-colonize-$VERSION-windows"
    rm -rf "$PKG"
    mkdir -p "$PKG/data/soundfonts" "$PKG/COLONIZE"
    cp build-windows/colonize_linux.exe "$PKG/colonize.exe"
    x86_64-w64-mingw32-strip "$PKG/colonize.exe" || true
    cp data/soundfonts/Roland_SC-55.sf2 data/soundfonts/COPYRIGHT.Roland_SC-55 "$PKG/data/soundfonts/"
    touch "$PKG/COLONIZE/put original game files here"
    cp scripts/release/README-windows.md "$PKG/README.md"
    cp LICENSE "$PKG/LICENSE"

    # Third-party license texts: FluidSynth from its source tree; the Fedora
    # mingw static packages install theirs under /usr/share/licenses.
    TPL="$PKG/THIRD_PARTY_LICENSES"
    : > "$TPL"
    add() { { echo "================================================================"
              echo "$1"
              echo "Source: $2"
              echo "================================================================"
              echo; cat "$3"; echo; } >> "$TPL"; }
    add "FluidSynth $FLUID_VER (https://www.fluidsynth.org/)" "$FLUID_URL" \
        "$SRC/fluidsynth-$FLUID_VER/LICENSE"
    for p in mingw64-SDL2 mingw64-glib2 mingw64-pcre2 mingw64-gettext mingw64-winpthreads mingw64-libffi; do
      d="/usr/share/licenses/$p"
      if [ -d "$d" ]; then
        while IFS= read -r f; do
          add "$p ($(basename "$f"))" \
              "https://packages.fedoraproject.org/ + upstream homepage (see README)" "$f"
        done < <(find "$d" -type f | sort)
      else
        echo "warning: no license dir for $p" >&2
      fi
    done
    # zlib and win-iconv ship no license dir in their Fedora mingw packages:
    # zlib text lives verbatim in its header; win-iconv is public domain.
    sed -n "/Copyright/,/madler@alumni.caltech.edu/p" \
      /usr/x86_64-w64-mingw32/sys-root/mingw/include/zlib.h > /tmp/zlib-license.txt
    add "zlib (from zlib.h)" "https://zlib.net/" /tmp/zlib-license.txt
    echo "win-iconv is placed in the public domain (https://github.com/win-iconv/win-iconv)." > /tmp/win-iconv-license.txt
    add "win-iconv" "https://github.com/win-iconv/win-iconv" /tmp/win-iconv-license.txt

    echo "== Sanity =="
    file "$PKG/colonize.exe"
    if x86_64-w64-mingw32-objdump -p "$PKG/colonize.exe" | grep -iE "DLL Name.*(SDL2|fluid|glib|libwinpthread)"; then
      echo "error: exe still imports bundled deps as DLLs" >&2
      exit 1
    fi
    x86_64-w64-mingw32-objdump -p "$PKG/colonize.exe" | grep -i "DLL Name" | sed "s/^/  /"

    ( cd dist && rm -f "linux-colonize-$VERSION-windows-x86_64.zip" \
      && zip -qr "linux-colonize-$VERSION-windows-x86_64.zip" "linux-colonize-$VERSION-windows" )
'"$([ "$RUNTIME" = docker ] && echo "    chown -R $(id -u):$(id -g) dist build-windows deps-mingw")"'
  '

echo "== Windows container build done =="
ls -l "$ROOT/dist/" | grep -i windows
