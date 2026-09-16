# Conventions, jargon, traps

**Read this first, before any task in this repo.** It is the durable layer:
the vocabulary, the code rules, the fixture traps, the evidence hierarchy and
the verification loop. It does **not** own feature status — that stays in the
[Authority table](architecture.md#authority). Session history stays in git.

---

## 1. Jargon glossary

### Reverse-engineering vocabulary

| Term | Meaning |
|---|---|
| `FUN_<seg>_<off>` | Ghidra synthetic name for a 16-bit function at *segment:offset*, e.g. `FUN_521d_20e6`. Calling convention in the export is usually `__cdecl16far`. |
| `DAT_<seg>_<off>`, `LAB_*`, `caseD_*` | Global/static data, labels, switch-case stubs. Switch bodies often survive only as `caseD_*` thunks — see the `caseD_10` trick below. |
| `DS:0xNNNN` | A DOS data-segment address. Globals are catalogued in `original_sources_annotated/include/viceroy_globals.h` and `docs/viceroy_tables.md`. |
| **DS→EXE rule** | A *static* DS string or table lives at **`COLONIZE/VICEROY.EXE` file offset `121248 + addr`**. Works only for static strings; runtime pointer slots (`0x2dxx`, `0x9xxx`) hold load-time pointers and read as garbage — identify those by table stride instead. (memory: `mysteries-sweep-2026-08-27`, `sidebar-49dd-0424-port`) |
| **`raw NNNNN`** | A line number in `original_sources_decompiled/viceroy_unpacked.c`. The normal citation unit in code comments (~470 uses in `src/core`). |
| **`FUN_1000_X` rule** | `FUN_1000_XXXX` resident stubs are thunks for `FUN_281f_(XXXX − 0x81f0)`; `FUNCTION_CATALOG.md` then names the real body. (memory: `ai-20e6-structural-land-port`) |
| `tools/address_mapping.csv` | canonical_name → canonical_address → overlay/offset map. Check it (plus `viceroy_globals.h`, `dosbox-x-dumps/*`) before filing anything as "needs a live capture". |
| **Overlays / RTLink** | VICEROY is RTLink-overlaid. A `CALLF <loader>; JMPF 0x0000:XXXX` is an **unpatched placeholder**, not content — resolve via `tools/rtlink_overlay_extract.py` + `tools/GhidraImportOverlays.java`, never by tail-following. (port_plan "Method notes") |
| Overlay offset examples | `38fd:X` = OVL05 offset `X+0x400`; `2f2b:X` = OVL03 offset `X`; `41f2:X` = OVL06 offset `0x2f13 + (X − 0x0003)`. (memory: `bugs-review-2026-08-31`, `bugs-batch-2026-09-05`) |
| **VICEROY ≠ MAPEDIT** | Separate programs, separate address spaces. Never equate `FUN_1a47_*` in MAPEDIT with a VICEROY `FUN_*` of the same digits. (`docs/original_index.md`) |

### Game / port vocabulary

| Term | Meaning |
|---|---|
| **golden** | A locked fidelity test (`tests/golden/`, `golden_*`). `smoke_ai_mid01`/`late01` were renamed from `golden_ai_*` because they generate their own fixture. |
| **Col1 save** | DOS `COLONY##.SAV`. Layout owners: `docs/savegame.md`, `docs/save_format_map.md`. |
| **head** | The save's header record (`ColonizeCol1Save.head`): turn, `human_player` (DS:0x5398), `nation_turn` (0x5394), `curr_nation_map_view` (0x5396), `founding_father[25]`, `active_unit` (0xffff = none). |
| **census / stuff** | The derived per-nation counters block (`col1_stuff_census.c`). Census counters are `FUN_281f_09c8` value sums **×8**; several saturate at 255. (memory: `smell-sweep2-hmh-2026-09-09`) |
| **profession bytes 25/26** | `@JOB` bytes `0x19`/`0x1a` = Indentured Servant / Criminal. The port also uses 19 as a free-colonist alias and 28 = `UNITS_JOB_NONE`. (memory: `bugs-batch-2026-09-03`) |
| **layer2 / layer3** | Map planes. layer2 carries presence/feature bits (bit `0x40` = plowed, `0x08` = road). layer3 **high nibble = owner (0..14, 0xf = none)**, **low nibble = continent id**; layer3 is never fog. |
| **COLONYFLAG** | DOS `@COLONYFLAG` "Colony Flags Error." — a village tile's layer3 **high nibble must equal the tribe's `nation_id` (4..11)**. Stamping a Euro nibble onto a village tile makes DOS pop the error on every lookup. (memory: `colonyflag-village-owner-nibble`) |
| **Lake** | terrain class `0x19` (Ocean) **and** layer3 low nibble != 1. `map_continent_id_at` returns −1 on water, so read `map_get_layer3() & 0x0f` directly. (memory: `bugs-batch-2026-09-04d`) |
| **REF / WoI** | Royal Expeditionary Force / War of Independence. Crown slots are `player[n].control == 2`. |
| **FF** | Founding Father. **Not globally exclusive**: eligibility is the nation's own bitfield (`nation.founding_fathers[]`); `head.founding_father[]` is a write-once *first-claimer* record DOS never reads as a gate. |
| **SoL** | Sons of Liberty. Latch bits on colony `+0x1c`: `0x04` = 50%, `0x02` = 100%. Colony-screen join/leave adjusts `rebel_divisor` by **±100**. |
| **LCR** | Lost City Rumour (`FUN_65dd_0004`). |
| **Popup tag ids = DS tag addresses** | A "popup id" is the DS address of the GAME.TXT tag string (`0x1866` = `INDIANCITY`). Table: `docs/popup_tag_ids.md`. Tags can be assembled at runtime, so grep the base-name address, not the full tag. |
| **GAME.TXT `^` / `^^`** | A leading caret is a **layout directive, not text**: `^` = draw this line verbatim on its own row, `^^` = same but centred. DOS eats exactly two carets (no loop). `_` stands in for a leading space. Shared helper: `popup_msg_caret_flags()`. |
| **`{}` emphasis** | Popup markup: `{` turns on hilite, `}` off, `~` escapes, `|` terminates. `popup_msg_apply_tokens` **keeps** the braces — renderers use `popup_draw_text_markup`, plain sinks call `popup_msg_strip_markup`. |
| **Two tick clocks** | `DS:0x8338` = **608.77 Hz** (every IRQ0); `DS:0x92e8` = **60.877 Hz** (the clock most game timers read, via `FUN_1c0c_0006`). Conflating them makes any ported timing 10× fast. |
| **MP thirds** | `moves_left` is in **thirds** (`UNITS_MP_PER_TILE`); a plain tile costs 3. |

---

## 2. Code conventions

### Naming

- **`_id` = stable identifier, `_index` = 0-based slot.** Verified by grep over
  `src/core/*.h`: `nation_id`/`unit_id`/`colony_id`/`ship_id` are ids;
  `type_index`/`colonist_index`/`hold_index`/`tile_index` are slots.
  Known deliberate stragglers: `ColonizeUnit.type_index` **is** the DOS type for
  NAMES-loaded pools but not for hand-built test type tables; `tribe_index` and
  `harbor_index` are slots into fixed tables.
- **Unit ids start at 1 and are not slot indices.** `units_spawn` returns an
  **id**; always go through `units_get(pool, id)` / `units_get_const`. A loop
  `for (i...) units_get(pool, i)` is the recurring **id-walk bug** — it silently
  drops ids ≥ pool size and can mutate the wrong unit. ~50 of these have been
  fixed; assume more exist in `ai_euro.c`.
- 1-based vs 0-based known spots: **ICONS.SS sprite ids are 1-based**;
  PHYS0 blit indices are 1-based; LABELS/NAMES section line indices are 0-based
  over non-blank, non-`;` lines.

### Porting rules

- **Never invent a constant.** Read the decomp. Invented numbers have been the
  single largest defect class (Discoverer damper, `intervention_bells`, the −2
  Tory clamp, `sol_bonus`, the "ship-slow" attack MP model — all deleted).
- **`DOS-LITERAL` means verbatim.** The tag is spelled exactly `DOS-LITERAL`
  in comments (normalized 2026-09-16; grep for it). Where a comment carries it
  (or cites a `FUN_`/raw line), it is a transcription — do **not** "improve" it,
  clamp it, or add defensive bounds. DOS oddities that are deliberate include
  `best_score = 99`, the tons ledger adding the cargo *index*, and the mode-6
  double-count of vet-professioned types.
- **Comment citation style**: name the DOS function and the raw line, e.g.
  `/* FUN_465b_0000 (~75692): ... */` or `raw 100381-100383`. ~2900 `FUN_*`
  citations and ~470 `raw NNNNN` citations exist in `src/core` — match them.
- **Per-call-site parameters are not global rules.** `FUN_4cc6_0356`'s continent
  filter is read at the colony tile for one caller and at the unit tile for
  another; porting one call site's choice as the function's semantics is what
  put totems on water. (memory: `bugs-batch-2026-09-04c`)
- **The status line is a third UI channel.** `DS:0x2d54` + the `281f_0056/006a/
  0074/007e` append API paints the map's top strip. When a DOS site builds a
  sentence through that API rather than the `6f74` compositor, the port must
  **not** raise a popup.
- **Big-function rule: extract stages into static functions with a status enum**
  (e.g. `AiEuroActStatus { CONTINUE, RETURN }`); no function > ~400 lines. Split
  by **gameplay concern**, not arbitrary line count. All stage functions take a
  shared context struct (`ai_euro_act_ctx`, `game_update_ctx`, etc.). Pattern:
  `FUN_` with complex control flow → dispatcher over `ai_euro_act_*` stages with
  `ai_euro_reset()` lifecycle hook for stateful iterators. (2026-09-16)

### Structural invariants

| Rule | Where |
|---|---|
| **Popup blocking**: every player-facing modal blocks all state processing. A new modal must be listed in `game_modal_open` (`game_loop.c`) — no per-site gating. The sim runs only on the overland map with nothing over it. | memory `popup-blocking-invariant` |
| **Single treasury accessor**: one purse per nation behind `europe_nation_gold` / `europe_nation_gold_add` (`europe.h`). Human = live `eu->gold`, others = the col1 record. `europe_gold_stamp_record` is the absolute copy; never assign. | `sweep3-third10-fix-wave` |
| **Every AI-side borrow of the shared `EuropeScreen` needs save/restore** gated on `nation_id != ctx->human_nation` — otherwise AI gold/tax leaks into the human's record at the next capture. | `crown-europe-batch-2026-09-04` |
| **Popup art palettes: merge, never remap.** A sheet that owns a DAC block the host screen leaves black (`SCORE<nn>.SS` over `WOODPAN2.PIK` is the only such pair) gets its entries merged via `ai_popup_art_palette_merge`. Sheets that paint where hosts paint (PARCH/WOODTILE/BUILDING/ICONS) are **remapped**, one copy per destination palette. | `crown-europe-batch-2026-09-04`, `sweep3-sixth-wave-batch20` |
| **No weak fallbacks in `colonize_core` files.** `__attribute__((weak))` definitions satisfy the reference, so the linker never pulls the real archive member — `unit_ai_diplo` ran stubbed AI for months. Cross-module stubs live in `src/core/ai_contact_link_stubs.c`, compiled **only** by the four slim targets (see its file header + `COLONIZE_SLIM_SOURCES` in `CMakeLists.txt`). | `sweep3-fifth-wave-batch20` |
| **Slim targets**: `unit_units`, `unit_combat_strength`, `unit_colonies`, `unit_reports` do not link `colonize_core`. Adding a call to a symbol outside `COLONIZE_SLIM_SOURCES` breaks all four at link time. They are declared with `colonize_add_test(... SLIM)` in `CMakeLists.txt`, which appends `src/core/ai_contact_link_stubs.c` and defines `COLONIZE_SLIM_TEST=1`; the stubs file `#error`s in any other target. | `duplication-audit-2026-09-14` |
| **MP on native units** routes through `units_mp_charge` / `units_mp_exhaust` (`units.c`): Euro units store thirds remaining, Braves store thirds **spent** (max 3). | `smell-batch1-2026-09-08` |
| **No per-turn "drip" effects without a DOS trace.** Indian alarm grows only through `FUN_4d56_152e` → `ai_contact_alarm_delta_00f2`. Discrete-event bumps are safe; recurring drips compound into order-of-magnitude divergence. | `alarm-fandom-drips-retired` |
| **Don't re-merge "kept split on DOS grounds"** rows from `docs/duplication_audit_2026-09-14.md`; use the shared helpers listed in its Resolution table. | `duplication-round2-2026-09-15` |
| **Never store port-only state in a `head.unknown*` field** without checking `save_format_map.md` for its real DOS meaning (`unknown46` is the market pool and is rewritten every EOT). | `woi-headless-sim-and-unknown46` |
| **Expert Teacher (@JOB 18) was cut before release** — never port DOS code that creates or hires one. Existing arms are save tolerance only. | `expert-teacher-cut-type` |

---

## 3. Fixture traps

These have each cost a session. Check them before blaming the code.

- **Uninitialised fixtures.** `units_reset()` deliberately does not zero
  `pool->types`. A stack-declared `ColonizeUnitPool` runs on garbage
  (`guns`/`hull`/`cargo`) — benign at `-O0`, wrong at `-O3`. Always
  `memset(&pool, 0, sizeof pool)`. Same for `ColonizeReportsView`.
  Also `*_set_occupancy_map(NULL)` after every `units_reset`/`colonies_init`,
  or a dead stack map stays bound. **Reset functions** (`units_reset_hooks()`,
  `ai_euro_reset()`, `ai_native_reset()`, `turn_reset()`) must be called in all
  test fixtures and from `ai_init_new_game` + `game_apply_col1_save`. Known
  exclusion: `s_euro_last_dir` is not reset (intentional — it tracks the human's
  last ship direction across save/load).
- **Zeroed `founding_father[]`.** Granting an FF by writing `head.founding_father[i]`
  does nothing — grant via `col1.nation[n].founding_fathers[idx/8] |= 1<<(idx%8)`
  and clear the bit on reset, or grants leak across sub-tests.
- **Blank census.** A zeroed census/`stuff` block makes DOS-shaped scorers read
  0 and take branches DOS never takes. Seed real counters.
- **Tiny-seed RNG.** `dos_rng_seed` with small consecutive seeds (1..64) gives
  tiny first outputs, so the first `dos_rng_range(1, 0x148)` is 1–3 for all of
  them. "Loop seeds until the roll passes" fixtures must spread seeds (`*12345`).
- **Latch-before-check.** Tests that read a bonus straight from live SoL% without
  first calling `colony_prod_refresh_sol_flags` encode the bug, not the rule.
- **Native fixture flip.** ~40 native fixtures were written against the inverted
  MP gates (`moves_left = 3` meaning "ready"). Any old branch touching native MP
  fixtures carries that inversion.
- **`layer3 = 0xf0` / calloc'd layer3.** A calloc'd layer3 reads as all-lake
  (low nibble 0 != 1); stamp `layer3 = 1` for ocean tiles.
- **Europe-mirror purse.** Human gold is the Europe mirror — a test that sets
  only the nation record must also set `europe.gold`.
- **FF head-only.** See above; `head.founding_father` is not a gate.
- **id-vs-slot.** `pool.units[id]` in a fixture edits the wrong slot. Use
  `units_get`.
- **Stale build tree.** Live trees are `build/debug` and `build/release`
  (CMakePresets `binaryDir`). `ctest --test-dir build` on the bare `build/` dir
  gives phantom pass/fail. Always `--test-dir build/debug`.
- **Worktree isolation.** Fresh worktrees lack the gitignored `COLONIZE/`,
  `test-assets/`, `test-saves-*` — 20–40 tests fail on missing `NAMES.TXT` until
  symlinked. Agent worktree isolation has failed under rate-limit restarts;
  verify where edits actually landed.
- **`git apply` is atomic.** Per-file "applied cleanly" lines mean nothing if
  `rc != 0`.
- **`sav_json` writes next to its input.** `sav_json in.SAV` with no output
  argument writes `in.SAV.json` **beside the input** — even when it later
  crashes (buffered partial file). It has overwritten tracked fixtures. Always
  pass an explicit scratchpad output path.
- **`port_saves/**` are the user's live player saves.** The user plays while work
  is in progress; unexplained diffs there are his autosaves. **Never**
  `git checkout`-restore them without asking.
- **Regenerated fixtures.** `test-saves-ai/MID02.SAV`, `LATE01.SAV`,
  `LATE01_POST.SAV` are rewritten in-tree by `smoke_ai_mid01`/`late01` on every
  run. Binary diffs there after a behaviour change are normal, not corruption.
- **Line endings.** Historically several `src/core` files were CRLF and Python
  text-mode round-trips silently reflowed them. `.gitattributes` now enforces
  `* text=auto eol=lf` and the previously-listed files check out as LF — but a
  scripted rewrite should still `file <path> | grep CRLF` first.
- **Parallel `ctest` collisions.** A `golden_ai_joint` or golden failure sharing
  a run with a genuinely red unit test is usually the unit test failing *inside*
  the joint target. Rerun on a settled tree before root-causing.
- **Two concurrent `cmake --build` on one build dir race** — phantom failures.
  Give parallel agents their own build directories.

---

## 4. Evidence hierarchy

Strongest to weakest. When they conflict, the higher rank wins.

1. **User-observed DOS behaviour.** The user plays the real game. His field
   observations have overturned static readings repeatedly (Prairie base yield,
   pioneers finishing work at 0 MP, peace-time capture).
2. **Raw disassembly / decompile**, read at the byte level — `.asm` for XREFs and
   exact PUSH sequences, `.c` for control flow. Read the **raw** function before
   trusting any annotated summary; annotated docs have drifted from the bytes.
3. **DOS save-file statistics** — 47 original saves are ground truth for which
   bits DOS actually sets.
4. **Golden fixtures** — locked outputs, but **synthetic tile setups inside a
   golden are curve-fit re-picks**, not DOS evidence (`golden_colony_prod01`'s
   Guadeloupe/New Holland/Bahia/St. Louis tiles were calibrated to whatever
   formula was live at re-pick time). Real captured totals are ground truth;
   the tile choices are not.
5. **These memory notes and the docs** — a summary of the above, and the first
   thing to distrust when it disagrees with 1–4.

**Fandom wiki is tier 3** (`docs/fandom_col1994.md`) and has produced several
invented mechanics. Never port from it without a DOS trace.

### Standing rules

- **Dead-text rule.** A GAME.TXT section with **no DS string in VICEROY.EXE**
  (absent from `docs/popup_tag_ids.md`; confirm with a `strings`/byte search of
  the EXE) can never be displayed by DOS. Do not port it. ~25 tags are proven
  dead this way.
- **Trace before strip.** Before deleting a field, arm or table as "dead",
  trace its writers *and* readers. Several "noise" fields (`nation+0x26`,
  `unknown13`, unit `+0x314a`) turned out to be real.
- **Byte-offset tally.** To prove a DOS byte unused, tally every access to that
  offset across all three decompiled exports. This is what retired the invented
  "prelude escalate" (indian record `+6` is never accessed anywhere).
- **Approximation-fitting is over.** When single-term toggles fix disjoint
  subsets of a miss table, you are overfitting. The next step is DOS-side
  evidence (a DOSBox-X trace), not another term.

### Static methods worth knowing

| Method | What it solves | Explained in |
|---|---|---|
| **ndisasm-literal** | Ghidra drops far-call arguments (every `FUN_281f_*` thunk). Find a prologue literal in `viceroy_ndisasm.asm` / `viceroy_unpacked.asm` and read the real `PUSH`es (cdecl: last arg pushed first). | memory `sidebar-49dd-0424-port`, `bugs-batch-2026-09-05` |
| **Label-ordinal** | DOS DS word tables hold string **ids**, not pointers. LABELS `@MISC[n]` = `DS:0x2dba + 2n`; recover any bare DS word by locating its pointer-table base and matching a LABELS section. | `pedia-article-dos-recreation`, `bugs-batch-2026-09-04c/d` |
| **PUSH-pair scan** | Enumerate call sites whose args Ghidra dropped by scanning the `.asm` for `PUSH` pairs before `CALLF <target>` (found all ~119 `FUN_281f_0652` sites). | `mss-myr-popup-graphics` |
| **`caseD_10` grep** | Ghidra names some thunks `switchD_2000:da9f::caseD_10`. Grepping the `FUN_` name misses every call site — grep the `caseD_*` name. | `ai-residues-closeout-2026-09-08` |
| **Extract-and-ndisasm switch bodies** | Ghidra leaves switch bodies as `??` bytes (e.g. `FUN_4d56_021a`, 4836 bytes). Extract the range and `ndisasm -b16` it at the right origin. | `popup-ids-are-ds-tag-addresses` |
| **Boundary-first second pass** | A `completed=true` decompile can carry corruption inlined from a callee, and Ghidra can silently pull in *wrong but plausible* content. Check the cited address falls inside the function's own boundary before declaring corruption. | `docs/port_plan.md` "Method notes" |
| **Offline render + PPM cmp** | `tools/render_report`, `render_colony`, `render_map_panel` link `libcolonize_core.a` and dump PPMs; compare against a `git archive HEAD` build. No PPM goldens exist in tests. | `duplication-audit-2026-09-14` |
| **Headless driver on a real save** | For "does X work end to end", write a scratch driver (gcc against `build/debug/libcolonize_core.a`, `-I src`) that loads a real `.SAV`, loops `turn_end`, and prints counters — then promote it to a `golden_*` test. Attach a popup queue (`ctx.ai_popups`) or the King auto-declares independence on turn 1. | `woi-headless-sim-and-unknown46`, `ai-ship-wiggle-fix` |

---

## 5. Verification loop

```bash
cmake --preset debug && cmake --build build/debug -j       # live tree = build/debug
ctest --test-dir build/debug -R unit_units --output-on-failure
ctest --test-dir build/debug                               # FULL run before calling anything done
cmake --build build/debug --target golden_ai_joint         # when AI or turn order was touched
```

- `golden_ai_joint` is a **build-only convenience target**, not a ctest test: it
  re-runs `golden_mapgen_seed100`, `golden_ai_turns`, `unit_ai_contact`,
  `unit_ai_diplo`, `smoke_ai_mid01`, `smoke_ai_late01`.
- Most executables expect **repo root as cwd** (`golden_ai_turns` loads
  `test-saves-ai/TURN*.SAV` relatively).
- Anything touching Col1 interop must run `unit_col1_save` and round-trip a
  `.SAV` fixture (`docs/port_plan.md` fidelity bar).
- Run **Release too** when the change is AI/fixture shaped — the `-O0`/`-O3`
  split has hidden a real bug before. valgrind the `-O0` binary first when a
  test flips under optimisation.
- Headless drivers for end-to-end questions: the WoI sim harness
  (`tests/golden/test_woi_ref01.c`) and the ship driver pattern in
  memory `ai-ship-wiggle-fix`.
- Debug switches are catalogued in **[debug_env_vars.md](debug_env_vars.md)**
  (note which ones *change behaviour*). Quick picks: `DOS_RNG_TRACE`,
  `AI_SCORE_AT="n:x:y"`, `AI_PEEL_AUDIT=1`, `AI_TURNS_ONLY=t`,
  `AI_TURNS_ALL=1`, `AI_SHIP_TRACE=1`, `AI_SET_GOTO_TRACE=1`. `debug.logs` is a
  `settings.json` / DEBUG-menu toggle, not an env var (categories table in
  `docs/settings.md`; context stamped by `diag_set_context` / `game_track_screen`).
- Finally: annotate the `bugs.md` row. Status meanings there —
  **OPEN** (no resolution), **FIXED** (agent claims a fix, awaiting the user's
  verification), **CLOSED** / **REFUTED** (user-verified, moved to
  `docs/archive/bugs_closed.md`). Agents write FIXED; only the user writes CLOSED.
  IDs are permanent and never reused. Cite rows as `bugs.md #NNN` (the `#`
  column). Before 2026-09-16 citations were file line numbers; old line N = id
  N−6, and every in-tree citation was rewritten.

---

## 6. Process rules

- **Never `git commit`, `git push`, `git stash`, or change branches.** The user
  handles git through GitHub Desktop. Land edited files, not commits — inside a
  worktree too. (CLAUDE.md rule 2; memory `no-commit-push`)
- **Surgical edits.** Prefer a targeted edit over rewriting a file whenever the
  end result is the same (CLAUDE.md rule 5).
- **Load the docs at the start of work** and don't let compaction erase them
  (CLAUDE.md rule 1). Start from the **Authority table** in
  [architecture.md](architecture.md#authority) — it says which doc owns what.
  `docs/original_index.md` is the navigation layer for the decomp.
- **Docs are authoritative over memory.** Memory notes are session summaries;
  when a memory and a doc disagree, the doc wins and the memory is stale.
  When a memory names a file or function, grep it before citing.
- **Update the owning status doc when a slice lands** — result + trap + citation
  in a few lines. Leave chronology to git history; keep retracted-lead traps,
  they stop repeated dead ends. (memory `docs-compression-2026-09-05`)
- **Doc-length policy**: past roughly 800 lines a doc gets split or its closed
  history archived under `docs/archive/` with a status header. Audit dumps
  (`smell_audit_*`, `duplication_audit_*`) live in `docs/` while open and move
  to `docs/archive/` when closed.
- **Gate tags in `port_plan.md`**: `[auto]` = agent-autonomous, `[user]` =
  prepare then stop and ask, `[live]` = needs the user's DOSBox-X session (file
  it, don't block; try `dosbox-x-dumps/*` byte search first).
- **Anything player-visible that changes default behaviour is `[user]`** —
  prepare it, then ask.
- When delegating to subagents: give file paths, method traps, and explicit
  fences ("no builds", "no stash", "no port_plan edits"); assign disjoint file
  ownership and separate build directories; a rate-limited agent resumes cleanly
  via `SendMessage` with its context intact.
