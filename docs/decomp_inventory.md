# Decompiled Surface Inventory

For a navigable index of decomp sources, `COLONIZE/` data files, and DOSBox memory
dumps, see [original_index.md](original_index.md). Manual feature coverage vs the
Linux port: [manual_gap.md](manual_gap.md). European / Indian AI FUN_* inventory and
1:1 transcription roadmap: [ai_transcription.md](ai_transcription.md).

This repository keeps Ghidra exports of `VICEROY.EXE` / `MAPEDIT.EXE` under
[`original_sources_decompiled/`](../original_sources_decompiled/) for reverse-engineering
reference. They are not buildable with a modern Linux compiler and retain DOS
memory-model / runtime artifacts.

| File | Source | Notes |
|------|--------|-------|
| `original_sources_decompiled/viceroy_unpacked.c` / `.asm` | Unpacked EXE | ~125k / ~305k lines; overlay-resident code is present |
| `original_sources_decompiled/mapedit.c` | `MAPEDIT.EXE` | Static map feature art |

Prefer **`viceroy_unpacked.*`** when chasing map-view or overlay call chains.

## High-Level Metrics

- Unpacked VICEROY export: large (overlay bodies + many segments)
- Common synthetic symbols:
  - Globals: `DAT_xxxx_xxxx`
  - Labels: `LAB_xxxx_xxxx`
  - Switch labels: `caseD_*`

## Function Clusters by Segment Prefix

The segment prefix in function names (`FUN_ssss_oooo`) provides a practical
first-pass clustering mechanism:

- `FUN_15eb_*`: high-density logic cluster
- `FUN_1d1d_*`: high-density logic + platform-adjacent routines
- `FUN_1427_*`: mid-size cluster
- `FUN_104b_*`, `FUN_1009_*`: smaller utility/control-flow clusters

This clustering should be preserved in initial source splitting to reduce risk.

## Known Ghidra Disassembly Faults (check before trusting a function)

`viceroy_unpacked.c` carries **386** Ghidra `WARNING:` comments (`grep -c
"WARNING: Instruction at\|WARNING: Removing unreachable\|WARNING: Unable to
track spacebase"`), each sitting immediately above the function it applies
to. These are Ghidra admitting its own **disassembly** — not just the
higher-level C reconstruction — went wrong near that function: typically an
instruction-boundary desync ("Instruction at (ram,X) overlaps instruction at
(ram,Y)", meaning decode started mid-instruction and re-synced by luck, so
everything downstream is suspect until it does), a stack-tracking failure
("Unable to track spacebase fully for stack" → the messy `unaff_SI/DI/ES/SS`
/ `in_stack_*` locals you'll see flooding the function's C body), or blocks
Ghidra pruned as unreachable (a symptom of the same confusion, not
necessarily truly dead code).

**Before attempting to port or deep-RE any FUN_\* body, check the ~5 lines of
C source right above its declaration for one of these comments.** If present,
treat the function as **not safely portable from the existing decomp output
as-is** — porting from a corrupted disassembly risks *confidently wrong*
first-draft logic, which is worse than leaving it thin/stub. Fixing it for
real needs manual re-disassembly from the correct byte offset, which is
out-of-band from reading Ghidra's existing C/ASM output (that output is the
thing that's wrong).

Confirmed hits during the 2026-08-13 AI-transcription push (all four
functions that were candidates for deep porting that day):

| Function | Warning |
|----------|---------|
| `FUN_4d56_417e` | ~~`Removing unreachable block (ram,0x0005180a)`~~ **fixed 2026-08-13** — clean 933-byte function recovered, self-contained in `OVL13_L0000`, ends right where `4528` begins. **Identified and ported same day (task #5, closed)** — "Incite Indians" (WARPATH), the Missionary `@ACTIONS` order that bribes a tribe to attack a rival Euro nation, confirmed against `GAME.TXT`'s `@INDIANWARPATH`/`@INDIANWARPATH2` text and two live DOSBox-X captures of a real player-driven Incite (exact params: unit 38, English, tribe/nation value 11). Ported to `ai_contact.c` as a 6th village-meet CHOICE (`AI_POPUP_TAG_CONTACT_INCITE`); 2 of 4 price terms and the exact caller stay approximated/unfound (documented in `indian_incite_417e.md`), everything else faithful. Closes a gap flagged "incite/WARPATH gold PARKED" in 5 other places in this project |
| `FUN_4d56_2820` | ~~`Instruction at (ram,0x00040af8) overlaps...`~~ **fixed 2026-08-13** — clean 3439-byte function recovered, self-contained in `OVL13_L0000`; see `indian_trade_2820.md` correction note (size/shape mismatch vs the doc's existing nested-call map flagged there, needs follow-up) |
| `FUN_4d56_4528` | ~~`Instruction at (ram,0x000586cb) overlaps...` + bad instruction data~~ **fixed 2026-08-13** — full clean re-disassembly recovered via the overlay-addressing project, root cause was the flattened file's false adjacency to the next RTLink segment; see `indian_settlement_4528.md` |
| `FUN_521d_5b66` | ~~`Instruction at (ram,0x00057701) overlaps...` + spacebase + 2× unreachable block~~ **fixed 2026-08-13** — true function is a tiny 198-byte dispatcher (not the 1815-line multi-phase body `euro_unit_act.md` describes — that content is almost certainly misattributed from the same desync pattern as `4528`; see correction note there) |
| `FUN_5fef_0f14` | ~~`Removing unreachable block (ram,0x0006125d)`~~ **fixed 2026-08-13** — clean 309-line recovery, closely matches `indian_raid_loot.md`'s existing ~298-line estimate (unlike `4528`, wasn't severely desynced); see doc for confirmation note |
| `FUN_4d56_1816` | ~~4× `Removing unreachable block`~~ **fixed 2026-08-13** — clean 799-byte recovery, confirms `indian_contact.md`'s existing phase checklist items 3-4 precisely; see doc for a flagged (not yet resolved) discrepancy on item 5's growth-loop call target |
| `FUN_521d_6d8e` | ~~`Removing unreachable block (ram,0x00059313)`~~ **fixed 2026-08-13** — clean 1333-byte recovery, closely matches `euro_dispatcher.c`'s existing line-range and thunk-wiring docs (unlike `4528`/`5b66`, wasn't severely desynced) |
| `FUN_521d_0a60` | ~~`Removing unreachable block (ram,0x00053911)`~~ **fixed 2026-08-13** — clean 5700-byte recovery, 853 lines vs. `euro_dispatcher.c`'s existing ~858-line estimate — confirmation, not correction |
| `FUN_5bfb_022e` | ~~`Removing unreachable block (ram,0x0005c64f)`~~ **fixed 2026-08-13** — clean 3565-byte recovery, self-contained, confirms `indian_contact.md`'s meet/trade checklist as trustworthy |
| `FUN_2f2b_5e44` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 386-byte recovery; see `euro_unit_act.md` §2d13 |
| `FUN_38fd_0058` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 1420-byte recovery; see `europe_nation_eot.md` |
| `FUN_5fef_1b0e` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 7270-byte recovery (main combat resolution function); see `docs/combat.md` |
| `FUN_521d_5c38` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — turned out to be a genuine trivial 4-byte `return 1;` stub, not corrupted content — nothing to correct |
| `FUN_5fef_0000` | **root-caused 2026-08-13** — not a disassembly-fault warning at all; Ghidra's decompiler hits `Offset must be between 0x0 and 0x10ffef, got 0xffffffff`, confirmed as a decompiler bug in `CALLF 0x1000:XXXX` far-call resolution (same class as `OVL12_L0000:0` below), not corruption — raw disassembly clean (362 bytes, self-contained, no bad pcode varnodes at the instruction level). Structural hand-read (candidate-unit search/scoring loop), not fully semantically ported; see `indian_raid_loot.md` |
| `FUN_521d_20e6` | ~~`Unable to decompile 'FUN_521d_20e6' — process: timeout`~~ **fixed 2026-08-13** — the central move-scoring formula (every `docs/seed100_brave.md` peel routes through this); never decompiled at all before, now clean in 27s, 2219 lines, zero warnings. Found a real missing branch while there — see `docs/seed100_brave.md` "Root cause candidate". **Re-verified 2026-08-14**: a fresh independent re-recovery (2215 lines, zero warnings) found the canonical export's *current* body has since drifted from that fix — a confusing self-referencing call in its commit-phase tail (past the explore-ring section) isn't present in the fresh recovery, so the 2026-08-13 fix likely wasn't patched back byte-for-byte past the section it was investigating. Full current-clean body: [`move_scoring_20e6_full.md`](../original_sources_annotated/ai/move_scoring_20e6_full.md) — treat that file, not the live canonical export, as authoritative for this function from here on |
| `FUN_OVL12_L0000_0` (task #2, real `a6e4` target) | **root-caused 2026-08-13** — same decompiler pcode bug as `FUN_5fef_0000` above, not corruption. Hand-transcribed clean from raw disassembly (145 bytes, tribe search + no-match dialog fallback); see `euro_unit_act.md`. Not ported to Linux — needs unlabeled DS globals/callees named first, same gate as `FUN_4d56_417e` (task #5) |
| `FUN_5952_035e` | ~~`Instruction at (ram,0x0005a676) overlaps instruction at (ram,0x0005a675)` + 2× `Removing unreachable block`~~ **fixed 2026-08-14** — the colony per-turn production/AI-hint tick, cited by `save_format_map.md`/`colony.h` as source of truth for 8 already-`mapped` fields (`garrison_quota`/`specialty_cargo`/`cargo_idle_turns`/`improve_timer`/`labor_shortage`/`warehouse_level`/`capitol_level`/`ai_flags` bits). Canonical export's param count (4) didn't even match the clean recovery (2) — real mismatch, not just noise. Clean 1577-line recovery spot-checks all 8 fields as **confirmed, not corrected**. Also caught and reverted a false lead from earlier the same session: a claimed `0x94e6` read site "inside `FUN_5952_035e`" was corrupted-tail content that doesn't belong to this function at all — see `colony_tick_5952_035e.md` and its correction in `move_scoring_land.md`. Full body: [`colony_tick_5952_035e.md`](../original_sources_annotated/ai/colony_tick_5952_035e.md) |

**Catalog-tail sweep, 2026-08-13** — the ~70 `FUNCTION_CATALOG.md`-only
names from the "systematic cross-reference" note below turned out to include
13 that are actually cited in real (non-catalog) docs too (a gap in the
original doc-count filter). Re-disassembled all 13 via the overlay project:

| Function | Result |
|----------|--------|
| `FUN_112b_01ba` | **clean** — 123 bytes, confirms `assets.md`/`unit_orders.md`/`manual_gap.md` unit-chrome citations |
| `FUN_2b5a_0070` | **clean** — 1670 bytes, confirms `unit_orders.md` |
| `FUN_2b5a_2464` | **clean** — 454 bytes (canonical export never gave it a function boundary here; created one) |
| `FUN_2b5a_3252` | **clean** — 343 bytes (same) |
| `FUN_2f2b_51ec` / `628a` / `6372` | **clean** — 656 / 206 / 2166 bytes (same "before-first-function" gap in the canonical export; all three are real, separate functions once given correct boundaries) |
| `FUN_6cb2_2322` | **own body clean** (346 bytes) — decompile-time inlining cited `FUN_0000_00c6` as corrupted; **corrected (task #14): false positive**, see below |
| `FUN_4720_049e` | **own body clean** (232 bytes, confirms `save_format_map.md`/`move_enter.md`) — inlining cited `FUN_0000_7e22`/`035c`/`fe5e`/`0d04`; **corrected (task #14): all false positives except `fe5e`**, see below |
| `FUN_479b_00ca` | **own body clean** (141 bytes) — inlining cited `FUN_0000_0512`; **corrected (task #14): false positive**, see below |
| `FUN_75c2_2d46` | **own body clean** (947 bytes, confirms `save_format_map.md` boot-timer citation) — inlining cited `FUN_0000_42cc`'s neighborhood; **corrected (task #14): false positive** — offset 0x4386 is a real instruction inside `FUN_0000_42cc`'s own already-clean body (confirmed decompiling perfectly earlier this session), see below |
| `FUN_684c_08c0` | **disassembly confirmed 100% clean** (6317 bytes, 3972 instructions, 0 gaps) — this is the **NEW WORLD map-generate entry** (`golden_mapgen_seed100`'s subject). Decompiler itself crashes (`Unable to resolve constructor` at 3 addresses, then a low-level RPC desync) — a real Ghidra bug, not corruption; existing Linux `map_generate` port already passes its golden so no urgent action, this just confirms it isn't standing on corrupted disassembly |
| `FUN_15eb_1d4c` | **corrected 2026-08-13 (task #13) — earlier same-day entry was wrong, see below** — the 497-byte boundary is actually **correct**: raw disasm confirms `FUN_0000_7bfc` runs 0x7bfc-0x7e21 straight-line, ends in a real `LEAVE; RETF`, immediately followed by a separate clean function at 0x7e22. The "~19KB corrupted function" reading came from the *decompiler* chasing an indirect switch jump-table (cases 9-17, dispatch table at `ram:0x1f44`) whose data only partially resolves — some entries (0x74c0/0x9ad4/0xc483) plausibly land on real resident function starts, but case 10's entry (`0xbe03`) points **mid-instruction** inside an unrelated `CALLF` elsewhere, and case 11's (`0x1`) isn't a valid code address at all. That's a real, narrower anomaly (bad/unresolved jump-table data, not a giant corrupted function) — root cause of *those* two entries not chased further this pass. `building_production.md`'s formula-domain content (percentage-scaling read, `unit+0x1a` class byte vs. `DS:0x543f` stride-0x34 table) sits entirely inside the clean 497-byte straight-line body and is safe to treat as legitimate |

**Resident pocket (task #14), resolved 2026-08-13 — mostly false positives,
same inlining-artifact lesson as `FUN_15eb_1d4c` (task #13).** Decompiled
all 6 candidates *directly* (as their own top-level target, not as an
inlined callee) to properly test them:

| Function | Result |
|----------|--------|
| `FUN_0000_00c6` | **clean** — decompiles with zero warnings as its own target; the "5 warnings" were entirely an artifact of `FUN_6cb2_2322`'s decompile inlining it |
| `FUN_0000_7e22` | **clean** — immediately follows `FUN_0000_7bfc` (`FUN_15eb_1d4c`)'s real end at 0x7e22, itself a normal 1149-byte function |
| `FUN_0000_035c` | **clean** |
| `FUN_0000_0512` | **clean** (tiny, 16 bytes) |
| `FUN_0000_0d04` | **clean** |
| `FUN_0000_42cc` | **clean** — already confirmed earlier this session (small function, calls `FUN_1000_8628` etc.); offset 0x4386 cited via `75c2_2d46`'s inlining is a real instruction inside this same already-clean body, not a separate issue |
| `FUN_0000_fe5e` | **real pcode error, own body likely clean** — `Could not follow disassembly flow into non-existing memory at 1000:ff05` (Ghidra's segmented display for flat resident offset `0x1ff05`). That address sits ~3KB past our resident extraction's captured end (`0x1e26f`, from `codeOffset` to the first overlay header) — most likely an edge case in `tools/rtlink_overlay_extract.py`'s resident-span boundary (worth widening/re-checking if anyone resumes this), not confirmed as corruption in the game's own code. Not chased further |

**Takeaway for future sweeps**: a decompile-time `WARNING:` or pcode error
attributed to a function via *another* function's inlining is not
evidence that function is corrupted — decompile it directly first. This
pattern (not `FUN_15eb_1d4c`'s jump-table case) accounted for 5 of this
pocket's 6 "corrupted" callees turning out clean.

**Final catalog-only sweep, 2026-08-13 (task #15) — lead exhausted.**
Pushed into the remaining 81 `FUNCTION_CATALOG.md`-only-cited names
(the low-value tail deprioritized since the first sweep) to test rather
than assume the lead was dry. Batch-decompiled all 81 directly
(`tools/CatalogSweep.java`), then for every one that showed any
`WARNING:` text, cross-checked whether the warning's cited address
actually falls inside that function's own body vs. a callee it inlines
(`tools/CatalogSweep2.java`, same fix as the resident-pocket lesson
above):

| Bucket | Count | Meaning |
|--------|------:|---------|
| `CLEAN` | 7 | zero warnings, own or inlined |
| `INLINED_ARTIFACT_ONLY` | 32 | warning present but every cited address belongs to a *different*, already-clean callee — false positive |
| `REAL_OWN_WARNING` | 17 | at least one warning genuinely inside the function's own body |
| `WARNING_NO_ADDR` | 22 | warning text without a `(space,addr)` citation (spacebase-tracking / jumptable-recovery-limit / stack-pointer-set style messages) |
| `DECOMPILE_FAILED` / `NO_FUNCTION` | 2 | infra edge cases, not corruption |

**None of the 17 `REAL_OWN_WARNING` hits (clustered mostly in segments
`210d`/`275d`, plus a few scattered `1b01`/`1d1d`) carry the severe
signature** (`Instruction ... overlaps`, `Control flow encountered bad
instruction data`, decompile timeout) **that every genuine corruption
fix this session actually had.** Sampled warning text directly: all are
`Removing unreachable block`, `Globals starting with '_' overlap smaller
symbols`, or `Read-only address ... is written` — the same mild/cosmetic
classes already established this session (`6d8e`/`0a60`/`5bfb_022e`/
`2f2b_5e44`/`38fd_0058`/`5fef_1b0e`/etc.) as decompiler noise, not
disassembly-fault corruption. Spot-checked the 22 `WARNING_NO_ADDR`
entries too: all `Unable to track spacebase fully for stack`, `Could not
recover jumptable ... too many branches`, or `Treating indirect jump as
call` — decompiler-limitation classes (same family as the `684c_08c0`
and `FUN_0000_fe5e` pcode issues), not corruption either.

**Conclusion: the corruption-hunting lead from this whole thread is now
genuinely exhausted.** Every function this session flagged by a real
disassembly-fault warning and backed by actual documentation has been
checked; every remaining catalog-only name has now been checked too;
nothing left shows the severe signature that meant real corruption
anywhere else. What's left in `viceroy_unpacked_2.c`'s ~2380 functions
beyond this ~159-name warning-adjacent set was never warning-flagged in
the first place — no further leads to chase without a new, different
starting signal.

Systematic cross-reference done 2026-08-13: extracted all ~78 function
names Ghidra's warnings sit immediately above in `viceroy_unpacked_2.c`,
matched against every doc under `original_sources_annotated/` +
`docs/` (excluding the generic all-functions `FUNCTION_CATALOG.md`).
Fixed the ones with rich existing documentation, above. The candidate list
originally named here (`FUN_521d_6d8e`, `FUN_521d_0a60`, `FUN_5bfb_022e`,
`FUN_2f2b_5e44`, `FUN_38fd_0058`, `FUN_5fef_0000`, `FUN_5fef_1b0e`,
`FUN_521d_5c38`) is **stale as of 2026-08-14** — every one of them was
fixed later the same day (2026-08-13) per the entries above; this section
just wasn't updated when that happened. Nothing left in this ~78-name
warning-adjacent set is outstanding. Full method if a new corrupted-decomp
lead ever needs the same treatment: search
`original_sources_decompiled/viceroy_unpacked_2.c` for a `WARNING:`
comment immediately above a `FUN_*` declaration, then check whether that
name appears in more than just `FUNCTION_CATALOG.md`.

Not yet checked against the full list: the remaining ~125K-line `.c`
export beyond the ~78 warning-adjacent names above (i.e. warnings that
don't sit immediately above a function declaration — mid-body warnings on
already-flagged functions, or ones this grep's "immediately above" rule
missed). Worth a systematic pass before the next deep-porting attempt on
any large body — a 30-second grep above a target function's declaration is
much cheaper than discovering the corruption mid-port.

**Root-cause dig on the flattening step itself** (does `rtlink_decode`'s V2 output
feed Ghidra bad bytes?): [rtlink_decode_v2_gap.md](rtlink_decode_v2_gap.md). Short
answer: found and verified a real V2 relocation gap in the tool (10 stale
data-segment pointers, safely fixable — patch included), but it's too small in
scope to be the primary driver of the 386 warnings; the doc lays out the
follow-up plan (dynamic ground-truth via DOSBox-X, Ghidra overlay-block import).

## DOS/Hardware-Coupled Surfaces

Observed direct I/O and hardware assumptions in the decomp exports:

- VGA DAC and retrace interaction:
  - `out(0x3c8, ...)`
  - `out(..., 0x3c9)`
  - `in(0x3da)`
- PIT timer programming:
  - `out(0x43, 0x36)`
  - `out(0x40, ...)`
- Conventional VGA memory segment assumptions:
  - references involving `0xa000`
- BIOS tick/global data style references:
  - patterns around `DAT_0000_046c` and low-memory globals

These routines belong behind a Linux platform API and must not remain as raw
port I/O in the native build.

## Proposed Boundary: Core vs Platform

Linux-side present layout and intended constraints (living):
[architecture.md](architecture.md).

### Core Candidate (kept behavior-first)

- Game state updates
- Turn progression and simulation
- Economic/unit/map logic
- Scenario/rules logic

### Platform Candidate (replace with SDL2/Linux services)

- Palette and framebuffer presentation
- Keyboard/mouse polling and event translation
- Time/tick services
- File path, save location, and case normalization
- Audio output (SDL callback + FluidSynth; see `src/core/sound.c`)

## Bring-Up Strategy Notes

- Do not refactor gameplay logic first.
- Introduce wrappers matching expected legacy behavior.
- Route every platform call through explicit interfaces so unresolved behavior
  can be logged and implemented incrementally.

## Current Linux Bring-Up Status

- SDL2 shell, diagnostics log, save/load: original `COLONY##.SAV` structs +
  byte-identical I/O in `src/core/col1_save.{h,c}`; runtime bridge in
  `src/core/col1_bridge.c` (see `docs/savegame.md`); verified against
  `original_saves/COLONY00.SAV` / `COLONY01.SAV`. **Codec ≠ complete field
  semantics** — opaque-byte RE track: [save_format_map.md](save_format_map.md)
  (P0–P4 done for proven peels: atlas, post_map/stuff split, community renames,
  connectivity rebuild + 00f2 cache parity, stuff census + DS-named late chunks,
  colony specialty/AI/timers/`tiles[20]`, head WoI bits + map_mode + zoom,
  indian `euro_diplo` / contact / accum, nation return-xy / diplo stand-ins.
  **P5 naming + P6 template interop done** — mask density, blank census,
  colony levels/specialty, `vis_mask`; late `unknown_ds_*` stay export-OK zero.
  Mid-campaign census = DOS-parity preserve (no freshen).)

- `GAME.TXT` / palette / MADSPACK+FAB / `.PIK` decode: done for menu background
- Decomp exports (`original_sources_decompiled/viceroy_unpacked.c`,
  `original_sources_decompiled/mapedit.c`) are not compiled into the binary; DOS
  typedef stubs live in `src/platform/dos_compat/dos_types.h` for incremental extraction
- Map compositor lookup tables from `VICEROY.EXE` are extracted to `src/data/viceroy_tables.{h,c}`
  (see `docs/viceroy_tables.md`); **static map feature art** follows `MAPEDIT.EXE` instead
- World map view (**fidelity OK vs MAPEDIT**): terrain, land transitions, forest/hill/mountain/river
  connectivity, coasts, estuaries, special resources, rumours — see below and `docs/assets.md`
- Coast / estuary: enabled (`MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` default 1)
- **Music playback: enabled** — GSOUND bytecode decode + FluidSynth (`COLONIZE_SOUND_PLAYBACK_ENABLED 1`; see `docs/assets.md`)
- Europe screen bring-up: `EUROPE.PIK` + market quotes / dock recruit from `NAMES.TXT`
  (press **E** from the map; phase 5 hold buy/sell + tax, goods persist on **H**/**S**;
  see `src/core/europe.c`)
- Colony screen bring-up: DOS six-view layout — settlement (sprite hit-rects; workers/outside
  on Note 1 selectable strips at building bottom-center / fence; unified colonist selection
  admit/eject via fence; profession sticks across gear/location; working sprites Hardy **#58** /
  Veteran **#59**; thin construction banner), area (1.5× 24px tiles, Note 1 yield strips; fisherman → fish **#57**),   people (SoL/Tory; colonists + fence units on one row; food/crosses/bells
  strips; fish before grain food), transport (class name, hold **#122** empties), multifunction (house **#67**,
  Production Note 1, Units bottom, Construction BUY/CHANGE + hammer rows); warehouse strip
  unchanged; preview via `colony_preview.c`; see `src/core/colony_screen.c`,
  `src/core/colony_yield.c`, `src/core/colony_craft.c`
- Units bring-up: `@UNIT` types from `NAMES.TXT`, map icons from `ICONS.SS`,
  starter Pioneer + Caravel, select/move (terrain/road/river MP costs), deploy dock
  immigrants (**D**), board/unload (**O**/**U**), ship landfall unload / colony dock
  disembark, tile stack popup (wake sentry cargo then select), sail ship to/from Europe
  with passengers (**H** on high seas / **S** in Europe); Pioneer plow/road (**P**/**R**,
  carried tools, WorldMap `improve` flags synced with Col1 mask; yield bonuses in
  `colony_yield.c`; see `src/core/units.c`, `src/core/map.c`, `src/core/europe.c`,
  `src/core/unit_stack.c`)
- Map menu bar: `MENU.TXT` pull-downs with mouse hit-testing (`src/core/map_menu.c`);
  left-click selects unit/tile/colony, right-click selects tile; pan-only while a unit is
  selected; blinking white tile outline in tile-select mode; `CURSOR.SS` #0 is the OS
  pointer over the 320×200 frame on all screens; Colonizopedia category lists from `PEDIA.TXT`
  (`src/core/pedia.c`); pull-down divider after Terrain Types; TRADE Create/Edit/Begin
  structural (VGA TRADE chrome PARKED)
- **New-game wizard** (`src/core/new_game.c`): `@BEGINMENU` → NEW WORLD / AMERICA /
  CUSTOMIZE → difficulty (`DIFFICUL.PIK`) → nation (`NATIONS.PIK`) → leader name /
  `@NATION{n}A/B` on `WOODPANL.PIK` → king audience → `LEVN0001`–`0010` sail → map.
  NEW WORLD / CUSTOMIZE use `map_generate` (`MapGenParams`); AMERICA loads `.MP`.
  Hall of Fame still a stub. Wizard captions use unbold green `FONTINTR` with
  black drop-shadow; nation pick remaps England fill onto `NATIONS.PIK` red.
- Shared wood **popup window** chrome (`src/core/popup.c`): black + mid brown + raised
  bevel from `@COLORS` border0/1/2; title `@BEGINMENU` is the first consumer (`OPENTILE.SS`);
  **GAME → Pick Music** uses the same chrome with `WOODTILE.SS` (`src/core/pick_music.c`)
- Main-map right panel + scrolling 1:1 minimap (`src/core/map_panel.c`): viewport **15×12**
  tiles (`x=0..239`); wood strip `x=240..319` (`WOODTILE.SS`) with black left rule and
  minimap-section separator; AMER2 minimap window **56×39** (click-to-center, dark-orange border);
  `@INFO` unit/date/gold (`NAMEPLAT.SS` / `FONTTINY.FF`); menu bar shares wood + tiny green
  / yellow hotkeys; nation box at `(315,197)` unchanged; not using `WOODPAN2` / `WOODFRAM`
- Report / adviser screens: F2–F10 + REPORTS menu (`src/core/reports.c`);
  F1 Terrain Information → Colonizopedia at cursor; F8=`REPORT8.PIK`; F10=`WOODPANL.PIK`;
  F2–F9 filled from Col1 save + runtime pools (crosses, FF, labor, trade, warehouses,
  ships, rivals, tribes); F10 Colonization Score from manual schedule
  (`reports_compute_score`)
- Colonizopedia: woodcut list screen (`WOODPANL.PIK`) with green entry links in up to
  3 columns, then cargo/unit/terrain/job/building/father/misc articles with
  ICONS / BUILDING / CC-NN / TERRAIN previews
- Turn progression (`src/core/turn.c`): `@TIMECHANGE` calendar, colony production,
  nation crosses/bells hooks, EN→FR→SP→DU Euro AI + Indian AI + King/REF,
  Wait-for-next-unit, End of Turn option, autosave hooks (slots 9 / 8),
  turn-owner indicator (`FUN_1984_00aa`: 5×3 at 315,197; shown only during AI/Indian
  EOT phases; `@COUNTRY` / `@TRIBES` colors)
- Music (`src/core/sound.c`): GSOUND.COL voice bytecode → MIDI events (~60 Hz ticks);
  FluidSynth with SC-55-preferring SoundFont search; ED chords, F3 volume envelope,
  BB pitch-bend RPN; BGM + event (`0x40..`) tables; Pick Music preview + title/map BGM
  via `COLONIZE_SOUND_PLAYBACK_ENABLED`; `COLDIG.BIN` SFX still deferred. Interpreter
  notes: `original_sources_annotated/sound/gsound_interpreter.md`.

## End-of-turn recovery checklist

Full orchestration map (Linux `TURN_PROC_*` ↔ DOS `FUN_130d_0290` /
`FUN_3844_*`, Layer D extracts): [turn_between_players.md](turn_between_players.md)
· [`original_sources_annotated/turn/between_turns.md`](../original_sources_annotated/turn/between_turns.md)
(callee depth: production / Europe EOT / census / landfall / mid-pass / ship-spawn /
fort fire / bells-FF / finish bridge).

Ordered pipeline recovered for the Linux port:

1. **Human ends turn** — Space / ORDERS → No Orders (`LABELS.TXT` “End of Turn”)
2. **Advance calendar** — `head.year` / `autumn` / `turn` (`@TIMECHANGE` in `GAME.TXT`):
   one turn/year until 1600; thereafter Spring then Autumn each year
3. **Colony production** — field harvest from map-ring `tiles[0..7]` (`NAMES.TXT` yields) − food
   consume 2/colonist; lumberjack → lumber (carpenter invents 1 lumber if none);
   settlement craft (`colony_craft.c`: raw→goods by workplace); hammers toward
   `building_in_production` (Colony Space = free production + UI deltas;
   `README.TXT` “free turn”)
4. **Nation ticks** — liberty bells + crosses; crosses ≥ needed → dock immigrant;
   founding-father election via `founding_fathers_tick` (manual-aligned effects;
   Sepulveda/Cortes/de Witt **Done**; KINGGALLEON2 leftover — see
   [ai_transcription.md](ai_transcription.md) unpark #3)
5. **European AI** — EN→FR→SP→DU via `player.control` (0 human / 1 AI / 2 withdrawn);
   `ai_euro_nation_turn` (`src/core/ai.c` → `ai_euro.c`): reseed from VR_SEED timer word, tick AI crosses,
   `6d8e`-shaped ship/land passes; **T2 early path** (seed-100 TURN1→7 via `golden_ai_turns`;
   landfall coastal staging + `ai_euro_06ae_first_colony_from_landfall`).
   Full-dispatch planner partial; deep land/ocean `20e6` still open — see [ai_transcription.md](ai_transcription.md).
6. **Indians** — village growth (`FUN_4d56_152e`-style), mid-turn Brave pulse + residual
   overlays (t1 empty; ~50 on t2–t6); named init burns `ai_native_post_first_brave_burns`.
   (`FUN_4d56_1816` / quiet `20e6`); meet/trade/raids via `ai_contact_*` (structural;
   deep `2820`/`4528` PARKED).
7. **King** — partial structural (`ai_king_nation_turn`: tax / declare / REF / war; R6;
   audience/confirm/merc via `ai_popup`)
8. **Refresh human MP** + select next unit with moves (“Continue turn.”)

**New-game AI actors** (`ai_init_new_game`): Col1 template (human control 0; AI
control 1 / gold 0; `nation_relation[]=-1`). Original starting gold is difficulty-scaled
(Discoverer 1000 / Explorer 300 / harder 0 — see [difficulty.md](difficulty.md)); port
still hardcodes human gold **1000**. Human and three rival fleets on eastern high seas
at turn 0 (Caravel / Dutch Merchantman with Pioneer+Soldier; skills from
difficulty/nation; landfall `goto`);
AMERICA villages from `TRIBE.TXT` + Brave per village; NEW WORLD / CUSTOMIZE procedural
villages (cap ~84). Human starter `nation_id` matches chosen power.

**Parked (later):** deep Euro `20e6` / T3 planner (**mapped** —
[`move_scoring_land.md`](../original_sources_annotated/ai/move_scoring_land.md) /
[`move_scoring_ship.md`](../original_sources_annotated/ai/move_scoring_ship.md));
deep Indian `2820`/`4528` + VGA meet chrome (**mapped** —
[`indian_trade_2820.md`](../original_sources_annotated/ai/indian_trade_2820.md) /
[`indian_settlement_4528.md`](../original_sources_annotated/ai/indian_settlement_4528.md));
deep King/REF (`10f0` economy, letter cinematic, exact `0x5382`). Early-AI T2 gate is green
(`test-saves-ai/TURN1`…`TURN7`). Roadmap: [ai_transcription.md](ai_transcription.md).
Year-end `0442` UI: [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md).

Evidence:

| Source | Finding |
|--------|---------|
| `GAME.TXT` `@TIMECHANGE` | Biannual seasons from 1600 |
| `original_saves/COLONY00/01.SAV` | turn 0→2 ≈ year 1492→1494 (1 year/turn); AI fleets leave Europe; AI crosses advance |
| `README.TXT` | Dutch turn ends European order; colony Space = free production |
| `LABELS.TXT` | “End of Turn” / “Continue turn.” |
| `NAMES.TXT` `@COUNTRY` / `@TRIBES` | Turn-owner box colors (DS:0x848 / 0x84c) |
| `COLONIZE/TRIBE.TXT` | AMERICA village seeds |
| `FUN_6a09_0006` / `FUN_4d56_152e` / `FUN_521d_6d8e` | Tribe place / growth / Euro AI dispatcher |
| `FUN_1984_00aa` / `FUN_281f_0590` | 5×3 fill at (0x13b, 0xc5) overlaid on screen |
| `original_sources_decompiled/viceroy_unpacked.asm` | `TIMECHANGE` / `MULTINEXT` / `SEASONS` string table only (no FUN_* XREF yet) |

AI colony production for non-human Europeans already runs through the shared
`turn_run_colony_production` loop (all active colonies). Older notes claiming a
nation skip were stale.

## Map generation (VICEROY)

Procedural NEW WORLD maps live in **VICEROY**, not MAPEDIT. Entry: `FUN_684c_08c0` (dispatched via `FUN_2a1f_083e`); land blobs `FUN_684c_02a8` / form thunks; continent labeling `FUN_67bf_0000`. Customize UI: `FUN_733a_0270` on `CUSTOMIZ.PIK` (4 columns × 3 rows; defaults all mid/`1`). Linux port: `src/core/map_gen.c` (`map_generate` / `MapGenParams`) + `NEW_GAME_PHASE_CUSTOMIZE` in `src/core/new_game.c`. See [assets.md](assets.md) “Map generation (NEW WORLD)”.

Continent flood-fill IDs (`FUN_67bf_0000`) are not written to layer2 in gen v1 (shipped AMER2 leaves layer2 zero); diagonal land cleanup (2×2 masks 6/9) is ported. RNG is exact DOS `FUN_1d1d_0e04` / `FUN_19ef_0032` (`src/core/dos_rng.c`): NEW WORLD draws customize axes (`range(0,3)`) then reseeds before `map_generate`. Tribe placement (`FUN_6a09`) **reseeds to `rng_seed`** at entry (matches DOS `6a09` / VR_SEED timer word) — it does **not** continue the post-mapgen stream or restore a post-axes LCG (stale “post-axes restore” docs were wrong for this path). Land mask, latitude/climate paint, forest wander, rivers, and arctic/HS tail bit-match seed 100 terrain. `FUN_6a09` capitals/satellites match SEED100; Braves spawn then take one post-`6a09` native pulse (`FUN_4d56_1816` path in `ai.c`) so coordinates/MP/`turns_worked` match the golden save (34 tribes / 46 units). Golden fidelity: `golden_mapgen_seed100` vs `test-saves-mapgen/SEED100.SAV` (no seed-special runtime path).

## Map compositor (MAPEDIT)

Authoritative static map compositor: `COLONIZE/MAPEDIT.EXE` /
`original_sources_decompiled/mapedit.c` (`FUN_1a47_0932`, land mask `FUN_1a47_01ae`).
No RTLink; no fog-of-war / animation. Coast and estuary are **on by default**
(`MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` in `src/core/map.h`);
the flags are compile-time debug toggles, not parked features.

**Recovered and matching MAPEDIT on AMER2:** coasts, estuaries, land–land transitions, forest/hill/mountain/river connectivity, procedural resources, rumours. Details and PHYS0 ranges: [assets.md](assets.md).

### Coast decoration

On ocean / high-seas tiles with at least one land neighbour:

1. Build 8-bit land mask (N→NW clockwise) and four 3-bit quadrant masks (cardinal → bits 0/2 on adjacent quads; diagonal → bit 1).
2. Special full-tile corners when mask matches (id 0..3 = land NW/NE/SW/SE):
   PHYS0 **`150 + id`**. MAPEDIT encodes `0x97+id` (1-based IDs 151–154);
   convert with −1 for 0-based sheet indices 150–153.
3. Else four 8×8 fragments: MAPEDIT ID **`0x6d + 4*quad_mask + q`** → index
   **`108 + 4*quad_mask + q`** at pixel offsets NW/NE/SE/SW (`0`/`8`).

Draw order vs MAPEDIT: land TERRAIN underlayer (last cardinal neighbour) → coast PHYS0 →
masked ocean into palette-0 holes (`FUN_1a47_0676`) → resources / estuary. Fog of war is not drawn
(MAPEDIT skips it too).

### River estuaries

Ocean tile with `terrain & 0xc0`: for each cardinal neighbour that is land with `terrain & 0x40`, blit MAPEDIT ID **`0x8d+q`** / **`0x91+q`** → indices **140–147**. Inland rivers unchanged.

Fixtures: `amer2_coast_fixtures` / `amer2_river_estuary` in `tests/unit/test_map.c`.

### Forest / hill / mountain / inland river connectivity

Same MAPEDIT cardinal mask as rivers (`FUN_1a47_030e` / `036e` / `0418`): **N=8, S=4, W=2, E=1**.

| Feature | Sprite (0-based) | Match |
|---------|------------------|-------|
| Forest (non-scrub) | `64 + mask` | any non-scrub forest neighbour |
| Mountain | `32 + mask` | `(n & 0xa0) == (self & 0xa0)` when `self & 0x20` |
| Hill | `48 + mask` | same bit test (hill when not also `& 0x80`) |
| Major / minor river | `0+mask` / `16+mask` (0 → 15) | `n & 0x40` |

Forest canopy via `map_phys0_forest_sprite_at`; hills/mountains/rivers/resources via overlay layers.

### Land transitions

`FUN_1a47_06da`: PHYS0 **104+q** colour-0 masks then neighbour TERRAIN fill (before forest). Ocean neighbours resolve through land cardinals (W/S/E/N).

### Resources / rumours

`FUN_12ab_0458` / `0540`: seed default **100**; type table DS **0x4de** / file **0x1794e** (MAPEDIT DS base **0x17470**); PHYS **89+type** / **103**. Ocean → fish (type 7). Mountain class **27** / hill **28**. Full type→terrain table in [assets.md](assets.md).

### Remaining gaps

- Fog-of-war: dedicated `map.seen` is Partial (reveal + black paint); MAPEDIT static view still has no fog by design
- Coast animation frames; per-tile texture-variation overlays from DOS RAM buffers
- Resource seed from live game RNG (static map view uses MAPEDIT default seed 100)

Prior VICEROY RAM-buffer / quadrant coast heuristics are **superseded** by MAPEDIT (see [viceroy_tables.md](viceroy_tables.md)).

