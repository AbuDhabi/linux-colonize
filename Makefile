# Root convenience Makefile for linux-colonize.
#
# Canonical dev loop: `make test`. Everything else (build/release, cross
# builds, etc.) is a release/cross-build artefact only — build/debug (CMake
# preset "debug") is the one directory this Makefile and CI care about.

.PHONY: build test golden release clean

build:
	@if [ ! -d build/debug ]; then cmake --preset debug; fi
	cmake --build --preset debug

test: build
	ctest --preset debug

golden: build
	cmake --build --preset debug --target golden_ai_joint

release:
	@if [ ! -d build/release ]; then cmake --preset release; fi
	cmake --build --preset release

clean:
	rm -rf build/debug build/release
