# Root convenience Makefile for linux-colonize.
#
# Canonical dev loop: `make test`. Everything else (build/release, cross
# builds, etc.) is a release/cross-build artefact only — build/debug (CMake
# preset "debug") is the one directory this Makefile and CI care about.

.PHONY: build test golden release clean

# One test target: `make test T=unit_ff` (builds only that target, runs it from
# repo root; add CASE=<name> to run a single case via COLONIZE_TEST_ONLY).

build:
	@if [ ! -d build/debug ]; then cmake --preset debug; fi
	cmake --build --preset debug

test:
ifdef T
	@if [ ! -d build/debug ]; then cmake --preset debug; fi
	cmake --build --preset debug --target $(T)
	$(if $(CASE),COLONIZE_TEST_ONLY=$(CASE)) ./build/debug/$(T)
else
	$(MAKE) build
	ctest --preset debug
endif

golden: build
	cmake --build --preset debug --target golden_ai_joint

release:
	@if [ ! -d build/release ]; then cmake --preset release; fi
	cmake --build --preset release

clean:
	rm -rf build/debug build/release
