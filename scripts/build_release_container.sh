#!/usr/bin/env bash
# Build the release tarball inside a manylinux2014 (CentOS 7) container so the
# binary links against glibc 2.17 (2013) — it then runs on essentially any
# x86_64 Linux from the last decade. The image ships a modern gcc on that old
# glibc; meson/ninja/cmake come from its bundled pythons.
#
# SDL2 is built X11-only there (CentOS 7 wayland is too old) — Wayland
# desktops run it fine via XWayland.
#
# Requires docker or podman. Output lands in dist/ as usual.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if command -v podman >/dev/null 2>&1; then
  RUNTIME=podman
elif command -v docker >/dev/null 2>&1; then
  RUNTIME=docker
else
  echo "error: need podman or docker (e.g. 'sudo apt install podman')" >&2
  exit 1
fi

IMAGE=quay.io/pypa/manylinux2014_x86_64

"$RUNTIME" run --rm \
  -v "$ROOT:/work" \
  -w /work \
  "$IMAGE" \
  bash -euo pipefail -c '
    # X11/audio dev headers for the static SDL2 build (dlopened from the host
    # at runtime, not linked). No wayland: CentOS 7 is pre-usable-wayland.
    yum install -y -q \
      libX11-devel libXext-devel libXrandr-devel libXcursor-devel \
      libXi-devel libXfixes-devel libXScrnSaver-devel \
      mesa-libGL-devel mesa-libEGL-devel \
      alsa-lib-devel pulseaudio-libs-devel dbus-devel > /dev/null
    # meson/ninja/cmake from the image python (manylinux wheels, glibc-2.17-safe)
    PYBIN="$(ls -d /opt/python/cp311*/bin | head -1)"
    # cmake<4: FluidSynth 2.3.x gentables subproject declares cmake_minimum 3.1,
    # which CMake 4 refuses to configure.
    "$PYBIN/pip" install --quiet meson ninja "cmake<4"
    export PATH="$PYBIN:$PATH"
    # Container-local build and deps dirs: the host ones hold artifacts built
    # against the host glibc and must not be mixed in.
    BUILD_DIR=build-container DEPS_PREFIX=deps-container scripts/build_release.sh
'"$([ "$RUNTIME" = docker ] && echo "    chown -R $(id -u):$(id -g) dist build-container deps-container")"'
  '
# (rootless podman: container root already maps to the host user, so no chown —
#  chowning there would shift files to a subuid instead)

echo "== Container build done =="
BIN="$ROOT/dist/$(ls "$ROOT/dist" | grep -v tar | head -1)/linux-colonize"
echo "glibc floor: $(objdump -T "$BIN" | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1)"
ls -l "$ROOT/dist/"
