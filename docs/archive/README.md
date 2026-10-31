# Archive

Superseded docs kept for history, not as living references. Consult only
when tracing why something is the way it is; don't cite as current fact.

**Read discipline (agents):** never open a file here to "get context", to
browse for work, or to check whether something was already done — the living
docs and `port_bugs.md` answer that, and these files are large enough to eat a
context window on their own. Open one only with a concrete lookup key (a bug
`#` id, a `FUN_ssss_oooo` name, an audit item number, a symbol) and read only
the matching lines: `grep -n '| NNN |' docs/archive/bugs_closed.md`,
`grep -n FUN_5952_035e docs/archive/<file>.md`, then `sed -n 'A,Bp'`. If the
grep misses, the answer is not here — stop, don't scan the file. Nothing in
this directory is a spec; a conflict with a living doc is resolved in the
living doc's favour.

| File | What it is | Archived | Consult when |
|------|-----------|----------|--------------|
| [bugs_fixed_pending.md](bugs_fixed_pending.md) | FIXED / REFUTED bug rows awaiting user verification (full resolution text) | rolling | Agents move rows here from `port_bugs.md` on FIXED; user closes from here |
| [bugs_closed.md](bugs_closed.md) | User-verified CLOSED / REFUTED bug rows with full resolution text (#1-#950) | rolling | Looking up one bug id you already have: `grep -n '| NNN |'`. Never read whole |
| [dos_bugs_evidence_2026-10-23.md](dos_bugs_evidence_2026-10-23.md) | Evidence / trace / test-history text trimmed out of `docs/dos_bugs.md` entries (DB-1, 2, 4, 6, 7, 8, 9, 12, 13, 14, 15, 16, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 46, 47, 49, 50, 54, 55, 56); each entry links its section | 2026-10-23 | Following an entry's evidence link: `grep -n '^## DB-NN$'` |
| [dos_bugs_evidence_2026-10-24.md](dos_bugs_evidence_2026-10-24.md) | Full pre-trim text of `docs/dos_bugs.md` entries DB-2, 3, 4, 9, 14, 23, 24, 28, 31, 46, 56, 60, 63 (evidence, port detail, test history); each entry links its section | 2026-10-24 | Following an entry's details link: `grep -n '^## DB-NN$'` |
| [bugs_resolutions_full_2026-09-05.md](bugs_resolutions_full_2026-09-05.md) | Full user bug list with agent resolutions, pre-2026-09-05 | 2026-09-05 | Looking up how an old bug row was diagnosed/fixed; current bugs live in `port_bugs.md` |
| [mysteries_catalog.md](mysteries_catalog.md) | Catalog of DS globals/flags whose meaning was unresolved as of 2026-08-19 | ~2026-08-27 (closed statically per MEMORY) | Investigating an "unknownNN" field name still lingering in code/saves |
| [port_plan_w_tier_archive.md](port_plan_w_tier_archive.md) | Pre-2026-08-24 W-tier work queue, verbatim from old `port_plan.md` | 2026-09-05 | Mapping an old `W*.*` citation elsewhere in the repo to its current track in `port_plan.md` |
| [smell_audit_2026-09-09.md](smell_audit_2026-09-09.md) | Second mechanics-smell sweep dump (106 findings; outcomes mostly in commits, not inline) | 2026-10-18 (closed 2026-09-10) | Citations like "smell_audit_2026-09-09 #N" in `src/core` comments |
| [smell_audit_2026-09-10.md](smell_audit_2026-09-10.md) | Third mechanics-smell sweep dump (106 findings) | 2026-09-23 (findings closed 2026-09-10 through 2026-09-15) | Tracing why a fix exists; citations like "smell_audit_2026-09-10 item N" |
| [smell_audit_2026-10-31.md](smell_audit_2026-10-31.md) | Whole-src smell audit + cppcheck dump (~180 findings) | 2026-10-31 (closed same day as port_bugs.md #1333-#1359) | Tracing a #1333-#1359 resolution back to its finding |
| [duplication_audit_2026-09-14.md](duplication_audit_2026-09-14.md) | Code duplication audit + Resolution/Round-2 fix tables | 2026-09-23 (findings closed 2026-09-14/15) | Checking whether a duplicate-looking pair was "kept split on DOS grounds" |
| [port_plan_deferred_ai_track.md](port_plan_deferred_ai_track.md) | "Deferred AI track — detail" section moved out of `port_plan.md` | 2026-09-23 | Reading full history of the AI porting track; current open items are summarized in `port_plan.md` |
| [ai_first_colony_trace.md](ai_first_colony_trace.md) | First-colony DOS trace and AI_SHIP_DOS retirement log (#530, closed), incl. the "Brave 021a trace" DOSBox-X worked example | 2026-10-24 | Tracing why the fitted first-colony layer was deleted; dos_trace.md cites its worked example |
| [popup_audit.md](popup_audit.md) | Popup authenticity audit dump (invented vs wired; 2026-09-28 sweep #984-990), nothing open | 2026-10-24 | Citations like "popup_audit MissingWire"; live per-popup status is `docs/popups_catalog.md` |
| [dos_bug_candidates_2026-10-22_decided.md](dos_bug_candidates_2026-10-22_decided.md) | Decided sections (A, B, C-61/C-62, D, E, F) of the 2026-10-22 DOS-bug sweep, verbatim; the undecided C-63..C-71 stay in `docs/dos_bug_candidates_2026-10-22.md` | 2026-10-24 | A "Came from / Was dos_bug_candidates_2026-10-22.md C-N" or E-N cite: grep the id |
| [tolerated_dos_bugs_evidence_2026-10-24.md](tolerated_dos_bugs_evidence_2026-10-24.md) | Full pre-trim "Blocks removal" test history and DOSBox trace anecdotes of `docs/tolerated_dos_bugs.md` / `tolerated_dos_bugs_ai.md` entries TB-2, 5-44, 46, 47; each entry links its section | 2026-10-24 | Following an entry's details link: `grep -n '^## TB-NN$'` |
| [port_plan_p1_p11_2026-10-24.md](port_plan_p1_p11_2026-10-24.md) | Closed playability tracks P1-P11, old "How an agent should use this file", closed D1/D2/D3/D6 rows and the 2026-09-26 AI-track status paragraph, verbatim from `port_plan.md` | 2026-10-24 | Resolving a `port_plan P<n>.<m>` citation; `port_plan.md` keeps a trap digest |
| [decomp_inventory_history_2026-10-24.md](decomp_inventory_history_2026-10-24.md) | 2026-08-13/14 Ghidra-fault fix tables, catalog-tail sweep, resident pocket, final catalog-only sweep, user-testimony examples, from `decomp_inventory.md` | 2026-10-24 | Looking up a `FUN_` name's old WARNING / re-disassembly result |
| [rtlink_decode_v2_investigation_2026-10-24.md](rtlink_decode_v2_investigation_2026-10-24.md) | rtlink_decode V2 relocation dig: code reading, patch 1/2 experiments, measurements, original ranked plan, overlay-extractor resident-base bug | 2026-10-24 | Re-deriving the rtlink patches or the resident block base |
| [assets_sound_trigger_ledger_2026-10-24.md](assets_sound_trigger_ledger_2026-10-24.md) | Per-ID DOS vs port non-music sound launch counts (2026-09-24 trigger parity audit) | 2026-10-24 | Re-auditing a sound event id's launch sites |
| [founding_fathers_history_2026-10-24.md](founding_fathers_history_2026-10-24.md) | `founding_fathers.md` resolved open items, P9 cross-check, 2026-09-07 Congress-debate / 6f74 row-pitch slice narrative | 2026-10-24 | Tracing why a Father effect or the Congress dialog metrics changed |
| [savegame_history_2026-10-24.md](savegame_history_2026-10-24.md) | `savegame.md` Phase 5 codec-drift story (W1.5, retired FF stash / FF_POOL_STASH_MARKER) | 2026-10-24 | Chasing W1.5 or the old unknown21_pad stash |
| [save_format_map_history_2026-10-24.md](save_format_map_history_2026-10-24.md) | `save_format_map.md` P0-P6 RE phase table + mermaid (all done) | 2026-10-24 | A doc cites a save-RE phase id (P0-P6) |
