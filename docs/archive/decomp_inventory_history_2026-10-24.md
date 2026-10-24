# decomp_inventory.md history (archived 2026-10-24)

STATUS: archived 2026-10-24 from docs/decomp_inventory.md — history, not a spec. The live rule (check the ~5 lines above a `FUN_*` for a Ghidra `WARNING:` before porting; decompile an inlined callee directly before calling it corrupt) stays in [decomp_inventory.md](../decomp_inventory.md). Lookup keys: `FUN_` names, task numbers.

## Ghidra disassembly-fault fixes, catalog-tail sweep and resident pocket (2026-08-13 / -14)

Confirmed hits during the 2026-08-13 AI-transcription push (all four
functions that were candidates for deep porting that day):

| Function | Warning |
|----------|---------|
| `FUN_4d56_417e` | ~~`Removing unreachable block (ram,0x0005180a)`~~ **fixed 2026-08-13** — clean 933-byte function recovered, self-contained in `OVL13_L0000`, ends right where `4528` begins. **Identified and ported same day (task #5, closed)** — "Incite Indians" (WARPATH), the Missionary `@ACTIONS` order that bribes a tribe to attack a rival Euro nation, confirmed against `GAME.TXT`'s `@INDIANWARPATH`/`@INDIANWARPATH2` text and two live DOSBox-X captures of a real player-driven Incite (exact params: unit 38, English, tribe/nation value 11). Ported to `ai_contact.c` as a 6th village-meet CHOICE (`AI_POPUP_TAG_CONTACT_INCITE`); 2 of 4 price terms and the exact caller stay approximated/unfound (documented in `indian_incite_417e.md`), everything else faithful. Closes a gap flagged "incite/WARPATH gold PARKED" in 5 other places in this project |
| `FUN_4d56_2820` | ~~`Instruction at (ram,0x00040af8) overlaps...`~~ **fixed 2026-08-13** — clean 3439-byte function recovered, self-contained in `OVL13_L0000`; see `indian_trade_2820.md` correction note (size/shape mismatch vs the doc's existing nested-call map flagged there, needs follow-up) |
| `FUN_4d56_4528` | ~~`Instruction at (ram,0x000586cb) overlaps...` + bad instruction data~~ **fixed 2026-08-13** — full clean re-disassembly recovered via the overlay-addressing project, root cause was the flattened file's false adjacency to the next RTLink segment; see `indian_settlement_4528.md` |
| `FUN_521d_5b66` | ~~`Instruction at (ram,0x00057701) overlaps...` + spacebase + 2× unreachable block~~ **fixed 2026-08-13** — true function is a tiny 198-byte dispatcher (not the 1815-line multi-phase body `euro_unit_act.md` describes — that content is almost certainly misattributed from the same desync pattern as `4528`; see correction note there) |
| `FUN_5fef_0f14` | ~~`Removing unreachable block (ram,0x0006125d)`~~ **fixed 2026-08-13** — clean 309-line recovery, closely matches `indian_raid_loot.md`'s existing ~298-line estimate (unlike `4528`, wasn't severely desynced); see doc for confirmation note |
| `FUN_4d56_1816` | ~~4× `Removing unreachable block`~~ **fixed 2026-08-13** — clean 799-byte recovery, confirms `indian_contact.md`'s existing phase checklist items 3-4 precisely; item 5 growth-loop discrepancy resolved 2026-08-27 (stub `4c40` → `152e`); caller of `1816` itself resolved same day (`1b3a` phase 2 via stub `4c2c`) — `turn/mid_pass_indian_rank.md` |
| `FUN_521d_6d8e` | ~~`Removing unreachable block (ram,0x00059313)`~~ **fixed 2026-08-13** — clean 1333-byte recovery, closely matches [`euro_dispatcher.c`](../../original_sources_annotated/ai/euro_dispatcher.c)'s existing line-range and thunk-wiring docs (unlike `4528`/`5b66`, wasn't severely desynced) |
| `FUN_521d_0a60` | ~~`Removing unreachable block (ram,0x00053911)`~~ **fixed 2026-08-13** — clean 5700-byte recovery, 853 lines vs. [`euro_dispatcher.c`](../../original_sources_annotated/ai/euro_dispatcher.c)'s existing ~858-line estimate — confirmation, not correction |
| `FUN_5bfb_022e` | ~~`Removing unreachable block (ram,0x0005c64f)`~~ **fixed 2026-08-13** — clean 3565-byte recovery, self-contained, confirms `indian_contact.md`'s meet/trade checklist as trustworthy |
| `FUN_2f2b_5e44` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 386-byte recovery; see `euro_unit_act.md` §2d13 |
| `FUN_38fd_0058` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 1420-byte recovery; see `europe_nation_eot.md` |
| `FUN_5fef_1b0e` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — clean 7270-byte recovery (main combat resolution function); see `docs/combat.md` |
| `FUN_521d_5c38` | ~~`Removing unreachable block`~~ **fixed 2026-08-13** — turned out to be a genuine trivial 4-byte `return 1;` stub, not corrupted content — nothing to correct |
| `FUN_5fef_0000` | **root-caused 2026-08-13** — not a disassembly-fault warning at all; Ghidra's decompiler hits `Offset must be between 0x0 and 0x10ffef, got 0xffffffff`, confirmed as a decompiler bug in `CALLF 0x1000:XXXX` far-call resolution (same class as `OVL12_L0000:0` below), not corruption — raw disassembly clean (362 bytes, self-contained, no bad pcode varnodes at the instruction level). Structural hand-read (candidate-unit search/scoring loop), not fully semantically ported; see `indian_raid_loot.md` |
| `FUN_521d_20e6` | ~~`Unable to decompile 'FUN_521d_20e6' — process: timeout`~~ **fixed 2026-08-13** — the central move-scoring formula (every seed-100 peel routes through this); never decompiled at all before, now clean in 27s, 2219 lines, zero warnings. Found a real missing branch while there — see the seed-100 notes "Root cause candidate". **Re-verified 2026-08-14**: a fresh independent re-recovery (2215 lines, zero warnings) found the canonical export's *current* body has since drifted from that fix — a confusing self-referencing call in its commit-phase tail (past the explore-ring section) isn't present in the fresh recovery, so the 2026-08-13 fix likely wasn't patched back byte-for-byte past the section it was investigating. Full current-clean body: [`move_scoring_20e6_full.md`](../../original_sources_annotated/ai/move_scoring_20e6_full.md) — treat that file, not the live canonical export, as authoritative for this function from here on. **Checked 2026-08-14 whether the shipped port was affected — it isn't**: `ai_native_pick_dir` (`src/core/ai_brave.c`, split out of `ai.c` 2026-09-23) has no reference to the stray self-call at all, and the cited `2a1f_04f4` symbol only ever appears as the normal caller-side entry thunk in the docs used for porting, never inside `20e6`'s own body. Drift confined to the tracking file; no port-side action needed |
| `FUN_OVL12_L0000_0` (task #2, real `a6e4` target) | **root-caused 2026-08-13** — same decompiler pcode bug as `FUN_5fef_0000` above, not corruption. Hand-transcribed clean from raw disassembly (145 bytes, tribe search + no-match dialog fallback); see `euro_unit_act.md`. Not ported to OpenCol — needs unlabeled DS globals/callees named first, same gate as `FUN_4d56_417e` (task #5) |
| `FUN_5952_035e` | ~~`Instruction at (ram,0x0005a676) overlaps instruction at (ram,0x0005a675)` + 2× `Removing unreachable block`~~ **fixed 2026-08-14** — the colony per-turn production/AI-hint tick, cited by `save_format_map.md`/`colony.h` as source of truth for 8 already-`mapped` fields (`garrison_quota`/`specialty_cargo`/`cargo_idle_turns`/`improve_timer`/`labor_shortage`/`warehouse_level`/`capitol_level`/`ai_flags` bits). Canonical export's param count (4) didn't even match the clean recovery (2) — real mismatch, not just noise. Clean 1577-line recovery spot-checks all 8 fields as **confirmed, not corrected**. Also caught and reverted a false lead from earlier the same session: a claimed `0x94e6` read site "inside `FUN_5952_035e`" was corrupted-tail content that doesn't belong to this function at all — see `colony_tick_5952_035e.md` and its correction in `move_scoring_land.md`. Full body: [`colony_tick_5952_035e.md`](../../original_sources_annotated/ai/colony_tick_5952_035e.md) |

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
| `FUN_684c_08c0` | **disassembly confirmed 100% clean** (6317 bytes, 3972 instructions, 0 gaps) — this is the **NEW WORLD map-generate entry** (`golden_mapgen_seed100`'s subject). Decompiler itself crashes (`Unable to resolve constructor` at 3 addresses, then a low-level RPC desync) — a real Ghidra bug, not corruption; existing OpenCol `map_generate` port already passes its golden so no urgent action, this just confirms it isn't standing on corrupted disassembly |
| `FUN_15eb_1d4c` | **corrected 2026-08-15 (this pass) — the 2026-08-13 table-content claim below was wrong, verified against the actual table bytes.** The address boundary is correct: `FUN_0000_7bfc` runs `0x7bfc`-`0x7e21` (549 bytes, not 497 — that number was a subtraction slip, not an address problem), ends in a real `LEAVE; RETF`, immediately followed by the already-confirmed-clean `FUN_0000_7e22`. But the jump-table content this row previously described (`0x74c0`/`0x9ad4`/`0xc483` as case targets, case 10 → `0xbe03` "mid-instruction", case 11 → `0x1` "not a valid code address") **does not match the table's actual bytes** — read directly off `viceroy_unpacked_2.asm` (`15eb:1f44`-`15eb:1f55`, the same table under its other-file alias) and cross-verified via `ndisasm`, all 9 entries are ordinary same-segment code addresses (`0x1ed8` ×6, `0x1e50`, `0x1e82`, `0x1f18`) landing on sensible, decodable code — no partial resolution, no mid-instruction landings, no invalid entries. Full peel: [`manufacturing_worker_calc_1d4c.md`](../../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md). The real corruption is confined to the **`.c` pseudocode export** (both `viceroy_unpacked_2.c` and `viceroy_overlays.c` independently synthesize unrelated sound/driver code for several switch cases) — not the jump table, not the `.asm`. Where the earlier `0x74c0` etc. numbers actually came from wasn't chased down this pass. `building_production.md`'s formula-domain content (percentage-scaling read, `unit+0x1a` class byte vs. `DS:0x543f` stride-0x34 table) is confirmed byte-exact and safe to treat as legitimate |

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
| `FUN_0000_fe5e` | **real pcode error, own body likely clean** — `Could not follow disassembly flow into non-existing memory at 1000:ff05` (Ghidra's segmented display for flat resident offset `0x1ff05`). **2026-08-14, re-checked the "just widen the boundary" theory — weaker than it looked.** Re-ran `rtlink_overlay_extract.py` fresh: resident `codeSize` is `0x1e270` (matches the `0x1e26f` citation, off-by-one rounding), and the target address (`0x1ff05`) overshoots it by `0x1c95` (~7.2KB), not the "~3KB" this row previously said — a bigger gap than a simple header/relocation-table alignment slop would explain, and the boundary itself (`first_header_offset - code_offset`) is architecturally sound (RTLink headers reliably follow resident code with no gap in every other segment checked). Leaning toward this being a genuinely bad/computed far-call target in the game's own code rather than an extraction bug, but not conclusively resolved either way — still not chased further, now with a documented reason not to reach for "widen the boundary" as a quick fix |

**Takeaway for future sweeps**: a decompile-time `WARNING:` or pcode error
attributed to a function via *another* function's inlining is not
evidence that function is corrupted — decompile it directly first. This
pattern (not `FUN_15eb_1d4c`'s jump-table case) accounted for 5 of this
pocket's 6 "corrupted" callees turning out clean.

## User testimony as an RE tool: worked examples (2026-08-14 era)

Examples that backed the "ask the user" method in decomp_inventory.md.

- `indian_incite_417e.md` — the whole function's real-world identity
  ("Incite Indians"/WARPATH) was settled when the user recognized a
  plain-language walkthrough of the code as a known game feature, not by
  any disassembly.
- `settlement_record_8d4a.md` — a real accept/decline sign-convention
  blocker (parked for a full pass, `2026-08-14`) was resolved in minutes
  once the user recognized live gameplay text (`@INDIANBEGFOOD`, "the
  tribe has fallen upon hard times...") and reported, from direct
  experience, "give food → relations improve, refuse → worsens." No
  disassembly could have produced that answer — the raw code's own
  "gives away half the colony's stock" wording actively pointed the
  wrong way until the user's testimony reframed it correctly.
- `docs/archive/popup_string_resolver.md` — a live DOSBox-X capture session (a
  `BPM` memory breakpoint on the generic dialog-flush entry point, not a
  scenario-specific trap) let the user relay ids and on-screen text turn
  by turn, cheaper and more reliable than continuing to trace a resident-
  space resolver chain statically.

## Final catalog-only sweep and warning cross-reference (2026-08-13, task #15)

**Final catalog-only sweep, 2026-08-13 (task #15) — lead exhausted.**
Pushed into the remaining 81 `FUNCTION_CATALOG.md`-only-cited names
(the low-value tail deprioritized since the first sweep) to test rather
than assume the lead was dry. Batch-decompiled all 81 directly
(`tools/CatalogSweep.java`), then for every one that showed any
`WARNING:` text, cross-checked whether the warning's cited address
actually falls inside that function's own body vs. a callee it inlines
(`tools/CatalogSweep2.java`; both were one-off Ghidra scripts, since
removed from `tools/` — same fix as the resident-pocket lesson
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
