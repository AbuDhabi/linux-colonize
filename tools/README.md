# Tools & Scripts Reference

This document catalogs all tools and scripts in the project, organized by category and invocation method.

## Built Executables (via CMake)

These are compiled C tools built from `tools/*.c` sources. Run them from `build/debug` or `build/release`:

| Name | Purpose | Invocation | Needs |
|------|---------|-----------|-------|
| `sav_json` | COLONY##.SAV ↔ JSON converter | `sav_json IN [OUT]` | Built by CMake (output file ext drives direction) |
| `ai_replay` | Headless Euro-AI replay on a real save: loops `turn_end` and prints colonies / pop / units / hulls / Europe-park counts / gold per AI nation per turn | `ai_replay <save.SAV> [turns] [--units] [--seed N]` | Built by CMake; repo root as cwd (reads `COLONIZE/NAMES.TXT`) |
| `render_report` | Standalone report-screen renderer for golden comparison | `render_report <data_dir> <save.SAV> <out.ppm> [report_id] [params...]` | Built by CMake; output is 320×200 PPM; convert to PNG with ImageMagick |
| `render_map_panel` | Standalone main-map sidebar renderer for golden comparison | `render_map_panel <data_dir> <save.SAV> <x> <y> <select_unit> <out.ppm> [load=...] [buys=...] [sells=...]` | Built by CMake; output is 320×200 PPM; convert to PNG with ImageMagick |
| `render_colony` | Standalone colony-screen renderer for golden comparison | `render_colony <data_dir> <save.SAV> <colony_name> <multi_mode> <out.ppm>` | Built by CMake; output is 320×200 PPM; convert to PNG with ImageMagick |

## Python Tools (tools/)

These require Python 3.

| Name | Purpose | Invocation | Needs |
|------|---------|-----------|-------|
| `decomp_fn.py` | Print one Ghidra-decompiled `FUN_*` body (or its file:line range / callers) without reading the 3 MB exports; resolves overlay `FUN_0000_oooo` aliases via `address_mapping.csv` | `python3 tools/decomp_fn.py [--where\|--callers] FUN_521d_0a60` | Python 3 stdlib only; caches index in `build/decomp_fn_index.json` |
| `ai_logic_map.py` | Render and validate docs/ai_euro_logic_map.yaml | `python3 tools/ai_logic_map.py <check\|html\|mermaid\|dot\|outline> [MAP] [options]` | Python 3 stdlib only |
| `build_address_mapping.py` | Build FUN_<seg>_<off> ↔ overlay addressing lookup table | `python3 tools/build_address_mapping.py <canonical_csv> <overlay_csv> <layout_json>` | Python 3 stdlib only; requires Ghidra CSV dumps and rtlink layout |
| `dosbox_trace.py` | Drive real VICEROY.EXE under the DOSBox-X debugger: private game copy, nested Xephyr display, persistent debugger session, breakpoints/memory/screenshots, unattended save load | `python3 tools/dosbox_trace.py setup\|start\|send\|stop WORK ...` (guide: docs/dos_trace.md) | dosbox-x, Xephyr, ImageMagick, python3 pexpect + PIL; reference crops in `tools/dosbox_trace_ref/` |
| `trcmp.py` / `jdiff.py` | Port vs a draw-traced DOSBox run: runs `golden_idle_campaign independence 0 K` with the trace's music picks, compares the interval-K draw stream and prints per-save field diffs (DOS, port) from `sav_json` dumps | `/usr/bin/python3 tools/trcmp.py original_saves/independence-seed-100/tr9/tr9.txt K [show]`; `jdiff.py A.json B.json [limit]` | Writes port dumps to `tools/tc/` (ignored); docs/idle_independence.md |
| `rtlink_overlay_extract.py` | Extract RTLink v2 overlay segments from VICEROY.EXE | `python3 tools/rtlink_overlay_extract.py COLONIZE/VICEROY.EXE OUTDIR` | Python 3 stdlib only; reads VICEROY.EXE, writes segments.json + per-segment .bin files |
| `make_vr_sound.py` | Build `COLONIZE/VR_SOUND.EXE` from `VR_SEED.EXE`: the seed-locked build plus a no-op music pump, so a DOS reference run stays deterministic whatever the Sound options say (docs/dos_trace.md, docs/idle_independence.md) | `python3 tools/make_vr_sound.py` | Python 3 stdlib only; reads/writes under `COLONIZE/` |

Data files: `address_mapping.csv` (canonical `FUN_` name ↔ overlay address, consumed by `decomp_fn.py`; built by `build_address_mapping.py`), `viceroy_v2_output_layout.json` (per-segment file-offset layout of the RTLink v2 flattening, one of `build_address_mapping.py`'s inputs — docs/rtlink_decode_v2_gap.md), `dosbox_trace_ref/` (screen crops `dosbox_trace.py` matches against), `tc/` (gitignored `trcmp.py` dumps).

## Shell Scripts (scripts/)

| Name | Purpose | Invocation | Needs |
|------|---------|-----------|-------|
| `build_static_deps.sh` | Build static SDL2 and minimal FluidSynth for release binary | `bash scripts/build_static_deps.sh [PREFIX]` | meson, ninja, autotools (or system static glib); no docker required |
| `build_release.sh` | Build standalone Linux release tarball | `bash scripts/build_release.sh` | CMake, compiler; output: `dist/opencol-<version>-linux-x86_64.tar.gz` |
| `build_release_container.sh` | Build release tarball in manylinux2014 container (glibc 2.17 floor) | `bash scripts/build_release_container.sh` | docker or podman; output: `dist/opencol-<version>-linux-x86_64.tar.gz` |
| `build_release_windows.sh` | Cross-build Windows release zip in Fedora container | `bash scripts/build_release_windows.sh` | docker or podman; mingw-w64; output: `dist/opencol-<version>-windows-x86_64.zip` |
| `grid_overlay.sh` | Overlay pixel-coordinate grid on a screenshot | `bash scripts/grid_overlay.sh <image> [grid_step] [out]` | ImageMagick (convert); default grid_step=20, out=<image>.grid.png |
| `render_diff.sh` | Pixel-diff two screenshots, highlighting mismatches in red | `bash scripts/render_diff.sh <reference> <candidate> [out]` | ImageMagick (convert); prints absolute error (AE) pixel count |

## Python Scripts (scripts/)

These require Python 3.

| Name | Purpose | Invocation | Needs |
|------|---------|-----------|-------|
| `extract_viceroy_tables.py` | Extract static data tables from COLONIZE/VICEROY.EXE to src/data/viceroy_tables.c | `python3 scripts/extract_viceroy_tables.py` | Python 3 stdlib only; reads COLONIZE/VICEROY.EXE |
| `gen_fun_catalog.py` | Generate/merge original_sources_annotated/FUNCTION_CATALOG.md and MODULE_MAP.md | `python3 scripts/gen_fun_catalog.py [--seed JSON] [--output-dir DIR]` | Python 3 stdlib only; parses Ghidra C exports, merges human-filled fields |

## Shell Utilities (tools/)

| Name | Purpose | Invocation | Needs |
|------|---------|-----------|-------|
| `check_save_interop.sh` | Fast Col1 save-interop check: runs `unit_col1_save` unit tests only | `bash tools/check_save_interop.sh [build-dir]` | CMake build; build-dir defaults to build/debug, then build/release |

---

## Ghidra Headless Scripts (tools/)

These are Java postScripts for Ghidra's `analyzeHeadless` command. They require:
- **Ghidra** (with Java JRE ≥8)
- **Original VICEROY.EXE** (loaded as a Ghidra program or imported via raw-binary loader)
- **RTLink overlay segments** (generated by `rtlink_overlay_extract.py`) for projects that use overlay spaces

These scripts are **read-only** (do not modify or save the Ghidra program) unless otherwise noted.

| Name | Purpose | Invocation |
|------|---------|-----------|
| `DumpCanonicalFuncs.java` | Dump every function in canonical flattened-EXE project (decompiled-colonize) with name and byte offset | `analyzeHeadless <proj> <name> -process VICEROY_OUT_2.EXE -postScript DumpCanonicalFuncs.java <out.csv> -scriptPath tools` |
| `DumpOverlayFuncs.java` | Dump every function in OverlayTest project with address-space, offset, and body length | `analyzeHeadless <proj> <name> -process OverlayTest -postScript DumpOverlayFuncs.java <out.csv> -scriptPath tools -noanalysis` |
| `GhidraDecompileAt.java` | Force-create a function at an explicit address and decompile its C | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraDecompileAt.java <space:offset> [...] -scriptPath tools -noanalysis` |
| `GhidraDisasmExact.java` | Clear then disassemble starting EXACTLY at a given address (no fallback) | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraDisasmExact.java <space:offset> <len> [...] -scriptPath tools -noanalysis` |
| `GhidraDumpBytes.java` | Dump raw memory bytes at space:offset as hex, bypassing decompiler | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraDumpBytes.java <space:offset> <len> [...] -scriptPath tools -noanalysis` |
| `GhidraExportOverlaysFull.java` | Export full OverlayTest program (resident + all 31 RTLink overlays) to ASM + C | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraExportOverlaysFull.java <outDir> -scriptPath tools` |
| `GhidraForceRedecomp.java` | Clear Listing analysis, then disassemble + createFunction + decompile fresh | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraForceRedecomp.java <space>:<offsetHex> <lenHex> -scriptPath tools` |
| `GhidraImportOverlays.java` | Build OverlayTest program holding every RTLink v2 overlay at its true DOS load address | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraImportOverlays.java <segDir> -scriptPath tools` |
| `GhidraListInstrs.java` | List raw disassembled instructions in one or more [space:start, space:start+len) ranges | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraListInstrs.java <space:offset> <len> [...] -scriptPath tools -noanalysis` |
| `GhidraListXRefs.java` | List every reference (call/jump/data read) INTO a given address | `analyzeHeadless <proj> <name> -process <prog> -postScript GhidraListXRefs.java <space:offset> -scriptPath tools -noanalysis` |

---

## Python dependencies

Standard library only, except `dosbox_trace.py` (`pexpect`, `PIL`; plus
dosbox-x, Xephyr and ImageMagick on the system) and `trcmp.py` / `jdiff.py`,
which need the built `golden_idle_campaign` and `sav_json` binaries.
