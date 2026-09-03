# Whole-Project Port Plan — Playability-First Work Queue

**Reframed 2026-08-24.** The project goal for this phase is **playability**: a
human can start a game, build an economy, trade with Europe, deal with the
natives, declare independence, fight the War of Independence and win — with
the UI, reports, popups and music good enough that none of it feels like a
placeholder. DOS-exact rival AI, 1:1 Indian AI, seed determinism, pixel-exact
art and full-fidelity music are **explicitly deferred** (see "Deferred
phases" below). The AI queue in [ai_port_plan.md](ai_port_plan.md) stays the
authoritative queue for AI transcription, but it is now a **deferred track**,
not a peer of this file's Tier 1.

Status detail still lives with its owners:

| Detail | Owner |
|--------|-------|
| Phase order / exit criteria | [roadmap.md](roadmap.md) |
| Feature Done/Partial/Missing | [manual_gap.md](manual_gap.md) |
| Popup inventory / authenticity | [popups.md](popups.md), [popup_audit.md](popup_audit.md) |
| Combat mechanics | [combat.md](combat.md) |
| Report screen (F2–F10 + HoF) DOS FUN map / layout | [reports.md](reports.md), [report_screens.md](report_screens.md) |
| SoL / independence | [sons_of_liberty.md](sons_of_liberty.md) |
| Indians | [indians.md](indians.md) |
| Production formulas | [building_production.md](building_production.md), [terrain_yields.md](terrain_yields.md) |
| Music / sound | [assets.md](assets.md) "Music / sound" |
| AI FUN inventory + sequenced queue | [ai_transcription.md](ai_transcription.md), [ai_port_plan.md](ai_port_plan.md) |
| Architecture constraints | [architecture.md](architecture.md) |
| Fidelity bar / conflict order | [project_goals.md](project_goals.md) |

## How an agent should use this file

Same method contract as `ai_port_plan.md`'s "How an agent should use this
file" + "Method notes" (read raw decomp before trusting summaries; check
`address_mapping.csv` / `viceroy_globals.h` / `dosbox-x-dumps/*` before filing
anything as live-capture-blocked; never invent a constant; never `git commit`
/ `push`; run **full** `ctest` before calling anything done; update the owning
status doc when a slice lands).

1. Read this file, [roadmap.md](roadmap.md), and the owning doc of whatever
   you touch (CLAUDE.md rule 1).
2. Work the **playability tracks P1–P11** in the order below unless the user
   points at a specific one. Within a track, the "Open" bullets are ordered.
3. Each bullet carries a gate tag:
   - **[auto]** — agent-autonomous (static RE / port / tests).
   - **[user]** — needs the user's eyes or decision (UI look, behavior
     choice, anything that changes default player-facing behavior). Prepare
     the work, then stop and ask.
   - **[live]** — needs the user's live DOSBox-X session. File it, do not
     block on it; try `dosbox-x-dumps/*` byte-pattern search first.
4. **Deferred** items are not worked from this file. If a playability track
   is blocked by a deferred item, do the *minimum* thin port that unblocks
   playability, mark it thin in the owning doc, and move on.
5. Check items off in place with a one-line dated result; keep history.

### Fidelity bar for this phase

"Playable" ≠ "byte-exact". For every track below, the bar is:
- Uses real `GAME.TXT` / `NAMES.TXT` / `LABELS.TXT` strings where DOS does
  (no invented dialog bodies — [popup_audit.md](popup_audit.md) rules apply).
- Formulas match DOS where the decomp is already read; where it is not,
  the manual's documented behavior is acceptable **for now** with a PARK
  comment naming the DOS function to revisit.
- Never regress Col1 save interop (P10) — every slice runs
  `unit_col1_save` and the `.SAV` fixture round-trips.

---

## Playability tracks (priority order)

### P1 — UI correctness (player-guided)

**Now (2026-09-03):** user-passed as playable. Remaining UI nits go through
`bugs.md` as they come in; this track is not a gate.

**How to work:** this track is **[user]**-driven by design. The user plays,
reports what is wrong or missing, and the agent fixes. Agents should not
self-select UI polish here; they should keep a running checklist in this
section from the user's feedback.

- [x] **P1.1 [user] — closed 2026-09-03.** Playable; leftover issues file
  as `bugs.md` rows rather than a pre-seeded checklist.
- [x] **P1.2 [auto] — 2026-08-28: automated as `smoke_play`
  (`tests/smoke/test_play_smoke.c`).** Drives the real `COLONIZE` assets
  headlessly through the key script new game (Enter through the wizard,
  click to skip the sail) → sail west with KP4/KP7/KP1 → `@LANDFALL`
  "Make Landfall" → `B` found colony → Enter colony screen → Esc → `E`
  Europe → Esc → `S` save slot 0, rendering each screen once and
  asserting `COLONY00.SAV` lands in `test-saves-play/`. Needed nine
  read-only probes on `game_loop.h` (`game_in_colony_screen`,
  `game_modal_open`, `game_ai_popup_tag`, `game_save_dialog_open`,
  `game_turn_busy`, ...). First run passed clean — colony founded on
  turn 3, no crash or render fault on any of the five screens. Full
  `ctest`: 47/47. Skips (exit 0) when `./COLONIZE` is absent. Still
  re-run it (and a by-hand `run` pass when UI changed) before each user
  session; this row stays the hook for that habit.
- [x] **P1.3 [user] — closed 2026-09-03.** Colony screen playable at a
  glance (drag/assign, buildings, warehouse↔ship, production, People strip).
  Remaining nits are not a P1 gate.
- [x] **P1.4 [user] — closed 2026-09-03.** Europe screen playable
  (dock/harbor/market, recruit/train/purchase, tax/boycott). Remaining nits
  are not a P1 gate.
- [x] **P1.5 [user] — closed 2026-09-03.** Map/panel playable (chrome,
  orders, minimap, status line, F-keys, end-turn, stack picker). Remaining
  nits are not a P1 gate.

### P2 — Report screens (F1–F10 + Hall of Fame)

**Now (corrected 2026-08-26 by P2.1's RE pass — see [docs/reports.md](reports.md)):**
this paragraph was stale.
[`reports.c`](../src/core/reports.c) already renders every F2–F10 report to
golden-screenshot-derived DOS pixel layout (columns, icons, sprite chrome,
paging) — a matching golden PNG exists for all nine in
`original_saves/report-screen-goldens/`. Only Labor (F4) has a click-to-zoom
in DOS at all, and it's wired (grid cell → that profession's detail page);
no report jumps to the colony/map screen on click in DOS itself. The real
remaining gap (narrowed 2026-08-26 by P2.2): report titles and every
name table (FF/job/cargo/tribe/nation/level) now resolve live from
`LABELS.TXT`/`NAMES.TXT`; column headers and body strings are still
hardcoded English typed from the goldens (non-blocking after P2.12
user-pass 2026-09-03).

**Target:** each report matches the DOS screen in content, column layout
and interaction (scroll, click-to-zoom to colony / unit where DOS does),
using `LABELS.TXT` strings and existing sprite sheets. Pixel-exact chrome
stays deferred (D4).

- [x] **P2.1 [auto] — RE complete 2026-08-26; `docs/reports.md` written.**
  Located every F2–F10
  report's DOS renderer in `FUNCTION_CATALOG.md`/`viceroy_unpacked.c`
  (Religious `FUN_3f41_06d0`, Congress `FUN_3f41_0618`+`FUN_4345_06d2`,
  Labor `FUN_3f41_10d8`/`0d3e`, Economic `FUN_3f41_1710`/`1550`, Colony
  `FUN_3f41_1bec`, Naval `FUN_3f41_1ed8`, Foreign `FUN_3f41_2548`, Indian
  `FUN_3f41_010a`, Score `FUN_41f2_0092`/family) plus Hall of Fame
  (`FUN_41f2_0f56`), and recorded data source/columns/ordering/scroll/click/
  strings for each against `src/core/reports.c`. **Real finding: this
  track's own "Now" framing above is stale** — every F2–F10 report already
  has golden-derived pixel layouts (`original_saves/report-screen-goldens/`
  has a matching PNG for all nine), not "flat text lines," and only Labor's
  click-to-zoom exists in DOS at all (grid→its own detail page; no report
  jumps to the colony/map screen) — so P2.2's "click-to-zoom plumbing into
  game_loop (colony screen / center unit)" describes behavior DOS's own
  renderers don't have.
- [x] **P2.2 [auto] — closed 2026-08-29** (all sub-items landed 2026-08-26/28; P2.12 user-passed 2026-09-03). Shared report scaffolding: heading from
  `LABELS.TXT`, column helper, scroll, click-to-zoom plumbing into
  `game_loop` (colony screen / center unit). **Screen titles: earlier
  "not achievable" note (2026-08-26) was itself wrong, corrected same
  day.** That pass grepped `COLONIZE/*.TXT` for "Religious Advisor"/
  "Labor Advisor"/etc. (American spelling, no suffix) and got zero hits —
  but the real asset spells it "RELIGIOUS ADVISER REPORT" (British
  spelling, "REPORT" suffix), in `LABELS.TXT` `@MISC` (all 9 titles
  present, byte-checked). Re-checked and **fixed for real**:
  `reports_title` (`reports.c`) now resolves live from `LABELS.TXT`
  `@MISC` via a new `reports_labels_field` helper (mirrors
  `reports_names_field`'s shape but no comma columns — LABELS.TXT is
  one-string-per-line) and a `k_report_title_labels_index[]` table (the
  9 titles' 0-based `@MISC` line positions, hand-counted against the raw
  file), falling back to the existing `k_report_titles` static array
  (which already had byte-correct text, just never resolved live) when
  assets aren't loaded. New `test_reports.c` regression: all 9 ids
  checked against the real asset text, plus the out-of-range fallback.
  `ctest`: 43/43 (was 42, +1 assertion block in `unit_reports`), no
  regressions — visually identical output, since the live and static
  values already matched exactly. **Founding Father names fixed 2026-08-26**
  (a different,
  real instance of this row's "hardcoded, not live" problem):
  `reports_ff_name` (`reports.c`) now resolves from `NAMES.TXT @FATHERS`
  live (loaded once in `reports_load`, mirrors the `assets_msg_find`
  pattern already used in `ai_contact.c`), falling back to the existing
  `k_ff_names` static table when assets aren't loaded — was a hand-typed
  copy that would've silently drifted from a modded `NAMES.TXT`. New
  public `reports_ff_display_name` wrapper + `test_reports.c` regression
  (checks idx 0/24 against real `NAMES.TXT` text, and the post-`reports_free`
  fallback path). **Job names + cargo names also fixed same day
  (follow-up on the identical shape):** `reports_job_name`/
  `reports_cargo_name` now resolve from `NAMES.TXT @JOB` column 2 /
  `@CARGO` column 0 the same way, sharing one `NAMES.TXT` parse
  (`g_reports_names`, generalized from the FF-only cache via a new
  `reports_names_field(section,row,col)` helper — `k_job_names`/
  `k_cargo_names` stay as the no-assets fallback). Public
  `reports_job_display_name`/`reports_cargo_display_name` wrappers +
  `test_reports.c` regressions (job 0/27, cargo 0/15, plus the
  post-`reports_free` fallback path for both). `ctest`: 42/42, no
  regressions — live-parsed values match the static tables exactly for
  the shipped asset, as expected. `k_report_titles` itself now also
  resolves live (see the corrected note above — the "no live source
  exists" claim was wrong).
  **Tribe names also fixed same day:** `k_tribe_names` (Indian Adviser
  F9 rows) resolves live from `NAMES.TXT @TRIBES` column 0 too — but
  *not* via the shared `reports_names_field` scratch buffer directly,
  since F9 builds a whole `rows[]` array of these before drawing (one
  shared buffer would alias every row to the last tribe parsed); a
  dedicated `reports_tribe_name` gives each of the 8 tribe indices its
  own small buffer instead. Public `reports_tribe_display_name` +
  regressions (tribe 0/7, an aliasing check reading both back at once,
  post-`reports_free` fallback). **Nation adjectives + tribe tech levels,
  same pass:** `reports_nation_adjective` (`NAMES.TXT @NATIONALITY`) and
  `reports_tribe_level` (`@LEVELS` column 0) had the identical
  rows[]-array-then-draw shape (Foreign Affairs `r->leader`/`r->adjective`,
  Indian Adviser `r->level` next to `r->name`) — same per-index-buffer fix,
  same aliasing-regression-test shape. Public
  `reports_nation_adjective_display_name`/`reports_tribe_level_display_name`.
  All four (FF/job/cargo already done, now +tribe/nation/level) share one
  `NAMES.TXT` parse via `g_reports_names`. `ctest`: 42/42 across every
  step, no regressions — this closes P2.2's "hardcoded, not live" gap for
  every report-display string table in `reports.c`, screen titles included
  (see the corrected note above). **Residue closed 2026-08-28:** every
  remaining header/body word with a `LABELS.TXT` `@MISC` line now resolves
  live via a new `reports_misc_word` helper (Congress header/sentiment/
  Expeditionary Force/Founding Fathers, Labor zoom hint + Off/On Mapboard/
  In Colonies, Foreign "Rebels"/"Tories" #86/#87 — the earlier "only
  singular forms exist" claim was wrong — Score Gold/Citizens/Congress/
  Rebel Sentiment/Total Score, shared OK button; Indian "Muskets" via
  `@CARGO`). Only strings with no shipped text stay literal ("Villages",
  HoF "Nation"/Esc hint, port-only empty states). **Real bug found on the
  way:** `ColonizeMsgSection` hard-capped sections at 64 lines while
  `@MISC` has 223, so every `@MISC` index ≥ 64 already cited as "live"
  (War/Peace, HoF headers, Bid/Ask Price, Economic/Colony subtitles) was
  silently on the static fallback; lines are now heap-grown per section
  (`assets.c`), which also drops ~10 MB of fixed-size section storage for
  GAME.TXT's 1045 sections. `unit_reports` regression asserts #86/#87/#56/
  #112/#121 against the asset + fallback path. `ctest`: 47/47.
  P2.12 user-passed 2026-09-03; hardcoded English column headers remain a
  non-blocking residue.
- [x] **P2.3 [auto]** F2 Religious Adviser (crosses, immigration, recruit
  pool) to DOS layout. **Done** — golden `religious.png`,
  `reports_render_religious` (was mislabeled "F1" here; DOS F1 is the
  Colonizopedia terrain article, not a report — see
  [reports.md](reports.md)).
- [x] **P2.4 [auto]** F3 Continental Congress as DOS splits it (bells,
  rebels, FF list, page-2 portrait grid) — **Done**, golden
  `continental_p1.png`/`continental_p2.png` (was mislabeled "F2/F3" here;
  it's F3 only, Religious is F2).
- [x] **P2.5 [auto]** F4 Labor Adviser: profession × colony matrix,
  click-to-zoom. **Done** — golden `labor.png`/`labor_detail.png`, the
  only report with a real DOS click-to-zoom (grid → per-profession detail
  page).
- [x] **P2.6 [auto]** F5 Economic Adviser: treasury, tax, market bid/ask
  table, boycotts, trade ledger. **Done** — golden
  `economic_p1.png`/`economic_p2.png`.
- [x] **P2.7 [auto]** F6 Colony Adviser: per-colony warehouse/status rows
  incl. SoL %, buildings-in-progress. **Done** — golden
  `colony_p1.png`/`colony_p2.png`. (No DOS click-to-zoom exists on any of
  F6/F7/F8/F9 — confirmed in the decompile, see
  [reports.md](reports.md); the "click-to-zoom" wording here and below was
  aspirational, not a DOS behavior.)
- [x] **P2.8 [auto]** F7 Naval Adviser: ship rows with cargo icons,
  location/destination. **Done** — golden `naval.png`.
- [x] **P2.9 [auto]** F8 Foreign Affairs: rival strength table (de Witt
  gating), war/peace status. **Done** — golden `foreign.png`.
  **2026-08-31 — the de Witt half was actually missing; now ported, plus
  three corrections found in the same read** (`FUN_3f41_2548` re-read from
  raw `.asm` 3f41:2548..2aca, because Ghidra drops this function's pushed
  values and mis-pairs every label with its number):
  - **Jan de Witt reveal grid** — six cells per block, two rows of three at
    x=2/80/160, filling the empty space the user reported: Colonies /
    Average Colony / Population (row A), Military Power / Naval Power /
    Merchant Marine (row B). Gate = viewer owns FF #4 **or**
    `head.show_entire_map`. Formulas + @MISC ids in
    [reports.md](reports.md) F8. Resolved `unknown_ds_944e` →
    `avg_colony_pop` (mean colony size, `FUN_4962_0018` divides in place)
    as a side effect.
  - **Peer line never wraps** — DOS's x ladder is 2/80/160/240 on one line;
    the port's 2-column wrap was an invention that would have overflowed
    the 45px block once the de Witt rows were added.
  - **Real gates** — the centered "(Withdrawn from New World)" keys off
    `head.crown_nation_id`, not `player.control`; peers are filtered on
    `euro_relation` bit 0x20 (met); War/Peace is bit 0x40 (set = peace) in
    one direction, superseding the old `(ab|ba) & 0x02` empirical fit;
    `nation_flags` bit 0x04 splices "Free" into the header and drops that
    block's Rebels/Tories line.
  - **Bug fix** — a War pair printed "Peace" in the War colour
    (`reports_labels_field`'s single static buffer aliased between @MISC
    101 and 102). Golden diff 3580 → 3372 px; the no-de-Witt render is
    otherwise byte-identical to the pre-change baseline. ctest 52/52.
- [x] **P2.10 [auto]** F9 Indian Adviser: tribes, attitude, missions.
  **Done** — golden `indian.png`. Headband-portrait variant selection
  (`ICONS.SS` #113-117) still unidentified, always renders #113 — cosmetic,
  not blocking.
- [x] **P2.11 [auto]** F10 Score: DOS score table layout; retire path.
  **Done** — golden `score.png`. Hall of Fame (shares F10's chrome) stays
  **Done thin**: no DOS golden exists for it at all, so its column
  widths/chrome are unconfirmed (see [reports.md](reports.md)).
- [x] **P2.12 [user] — closed 2026-09-03.** User: stale. P2.3–P2.11 already
  match goldens; leftover hardcoded English headers / incomplete Congress
  page-2 FF slot table are polish, not a P2 gate.

### P3 — Passable music

**Now (updated 2026-08-27):** `GSOUND.COL` is emulated literally
(`gsound_vm.c`) and driven in real time by `sound.c` + FluidSynth; Pick Music,
Sound Options, the corrected BGM id table, the DOS BGM scheduler, the
situational Military sting and `COLDIG.BIN` digital SFX playback are all in.
"Passable" here means: tracks play on the right cues, end/advance as DOS does,
don't glitch, and a default SoundFont path works out of the box. Music
listen-test **Done** 2026-09-03 (P3.5). Cue/SFX wiring (P3.2, P3.7) already
closed; leftover is some SFX still sounding off or missing — not a P3 gate.

- [x] **P3.1 [auto] — closed 2026-08-26, already done, just uncross-
  referenced.** Audit cue coverage: every DOS BGM/event id push site
  (`FUN_12d8_000e` callers, tables `0x2A6E` / `0x2AC4`) vs port call sites.
  List missing cues (colony enter/leave, Europe enter, contact, combat
  win/lose, declare, king audience, year-end, endgame). This audit
  already exists — [assets.md](assets.md) "Sound-ID ranges beyond the 12
  BGM tracks" (its own dated RE work, not new this pass): the `0x20..0x3f`
  BGM range's only **confirmed, precisely-traced** trigger is combat
  engagement start (`0x32`, ported as `units_combat_music_sting`); six
  other segments (`65dd` LCR, `75c2` save/load, `48d3` Europe exit, `364b`
  colony, `38fd`/`3844` trade) are "confirmed-real-but-unmapped-to-a-
  precise-trigger" — real push sites exist in the decompile but the exact
  call site within each wasn't pinned down; the `0x40..0x5c` event-music
  range and the `≥0x8020` chord-sting range are engine-ready with **no
  confirmed DOS trigger found at all** despite an exhaustive search (do
  not invent one). Port call sites are just the 2 `sound_set_bgm(1)` calls
  (new-game/load-save start) plus the one combat sting — confirms the gap
  P3.2 needs to close is real and matches this list exactly.
- [x] **P3.2 [auto] — closed 2026-08-29 (pass 4).** Last open cues resolved: `364b_0000` is the colony-screen *popup* helper, its sound arg is 0 at all 9 sites (no colony-open id exists); `3844_00f2`'s `0x3e` precedes **@KINGFRIGATE** (Crown frigate offer, now ported — `ai_king_frigate_offer`, +10% tax on Yes, AI auto-accept); LCR `0x24`/`0x32`/pool 1–2 arms mapped and wired in `units_resolve_lcr_rumour`; raid pool 2 / `0x32` were already wired. Only the Retire tune stays PARKed with the coin tier (difficulty.md). Earlier passes: **2026-08-29: the "unmapped" segments are mapped** —
  a whole-EXE asm sweep of every `281f_04c0`/`04b6` call (id in `AX`,
  which Ghidra drops) pins each: `75c2` = new-game init → `0x39` Hornpipe
  (ported), `38fd` = King's audience `3dc8` → `0x3e` (ported), `43f7` =
  intervention → `0x3f` (ported), `364b` = colony-screen open with a
  caller-supplied id (callers' pushed value still unread) + pool switches
  `04b6(1/2)` (already `sound_set_bgm`), `3844` = nation EOT `0x54`
  (already), `48d3` Europe exit and `65dd` LCR have **no** `04c0`/`04b6`
  call at all — they must go through another helper (`281f_048e`?), so
  P3.1's list was partly wrong. Table in assets.md "BGM cues pushed by
  gameplay code". Second sweep (same day) covered `281f_048e`/`0498` too and wired: treasure
  cash-in `0x24`, load `0x3e`, Europe screen pool 3, immigrant pool 2, LCR
  Fountain/Cibola/Burial tunes, village-visit tribe pools 5/6/7. Left:
  Third pass: naval pool 1/4, raid-repelled pool 2, chord stings
  `0x8020`/`0x8024` (driver table `0x2AB6` added to `gsound_vm.c`). Left:
  Retire tune (needs the PARKed coin tier), colony open id, `3844`
  nation-EOT `0x3e` popup identity, raid pool/`0x32`. Full table in
  assets.md. **Not
  attempted 2026-08-26** — same blocker as P8.3/P8.5 above: the 6
  "confirmed-real-but-unmapped" segments need their *exact* call site
  traced (which `FUN_364b_xxxx` sub-path fires on colony-enter vs.
  colony-exit, etc.) before a cue can be wired to the right moment without
  guessing; the audit above already did the coarse pass, this needs a
  focused per-segment RE follow-up, not a blind port.
- [x] **P3.3 [auto] — closed 2026-08-26, static-review pass.** Playback
  robustness: loop points, tempo/timing drift, note-off leaks, track
  change without pops, `--nosound` path. Code-read confirms all 4 checked
  items are already handled carefully, not just approximated: **loop
  points** emulate the real DOS `FF nn` in-song loop opcode (`sound.c`
  ~706-729, count/nesting/stuck-loop guard), not a crude "restart at end"
  hack — the end-of-song restart (`sound_service`) is only a fallback for
  songs that never hit an explicit loop-back. **Tempo** derives from the
  exact hardware PIT divisor DOS uses (`SOUND_PIT_DIVISOR 0x4DBF` →
  `1193182/0x4DBF ≈ 59.95 Hz`, not a rounded "60 Hz" stand-in). **Note-off
  leaks**: `sound_all_notes_off_unlocked` is called at every song
  start/stop/preview-transition (7 call sites), so no stuck notes survive
  a track change. **`--nosound`**: `enable_audio` gates every playback
  call site (`sound_playback_enabled`/`sound_ok`) while `inited` stays
  true so decode-only paths (tests, offline render) keep working; covered
  by `smoke_sound`. Nothing found needing a fix — the remaining item this
  row lists ("without pops") is perceptual and needs an actual listen
  test against DOSBox, which is P3.5's job, not something a static read
  can confirm.
- [x] **P3.4 [auto] — closed 2026-08-26.** SoundFont discovery: sane
  default search order + clear error when none found; document in README.
  `sound_find_soundfont` (`sound.c`) was already fully done code-side (env
  override → bundled `Roland_SC-55.sf2` via 6 relative paths → a dozen
  common system locations → `diag_warn` + soft-beep fallback, never a
  hard failure) — only the "document in README" half was missing (README
  didn't mention soundfonts at all). Added a "Music / MIDI soundfont"
  README section describing the search order and the override env var.
- [x] **P3.6 [auto] — 2026-08-27 music rewrite.** Root cause of "bad
  remix": the id table. Pick Music entry *n* is **not** `0x20+n` — the DOS
  handler (`2b5a:264c`) maps 9–12 to `0x39,0x38,0x3a,0x3b` and the
  sublists to `0x29..`, `0x2e..`, `0x32/33/35/36`; every A/B reference was
  compared against the wrong song. Replaced the hand-written decoder with
  a literal driver emulator (`gsound_vm.c`: 9 voice blocks, channel 9 =
  drums, single-slot `FA/F9`, `CD..D4` are jumps, mini-x86 for song
  handlers incl. random/warm-restart variants) and mirrored the DOS BGM
  scheduler (pool categories, fade-then-pump, auto-advance at song end —
  all songs end, none loop). `compare_music_ab.py` now reads dtw 0.04
  (Jine, vs 0.07 for OST-vs-DOSBox) / 0.09 / 0.15 / 0.17. P3.3's "FF loop
  emulation / restart fallback" description is obsolete. Details:
  [assets.md](assets.md) "Music / sound".
- [x] **P3.7 [auto] — closed 2026-08-29 (pass 3).** `0x44`/`0x45` tail decoded: gated on `local_6` = Indian attacker beat a colony defender (pop > 1); wired as `0x45` in `units_try_move` (the `0x44` arm needs a ship attacker — dead). `0x4d` naval arm: both combatants ships + visible, pushed at the 07db junction *before* the damaged/sunk split — wired at `units_apply_naval_loss_outcome` entry. Remaining ids `0x4c`/`0x50`/`0x51`/`0x55`/`0x5c` have no push site (typed-rule dead ends) — nothing left to wire. **2026-08-29 pass
  (2):** the `5fef_1b0e` "unit-class variants" resolved from the asm
  (`5fef:2271`): a human attacker on an Indian pushes `0x3b + attacker unit
  type`, so `0x41..0x51` are just type indices (Regulars 0x41 … Braves
  0x4e); ported in `units.c` engagement (typed id when the defender's
  nation ≥ 4, generic `0x40` otherwise). `0x44`/`0x45` hit/glance tail
  (`5fef:28b0`) still unmapped to a Linux outcome. **2026-08-29 pass (1):**
  wired `0x57` ship sunk (`units.c` @SHIPSUNK, combat sound hook), `0x4d`/
  `0x4e`/`0x4f`/`0x5b` raid outcomes (@RAIDGOLD/@RAIDSCALP/@RAIDSTORES/
  @RAIDNOTHING in `ai_contact.c`), `0x52` wagon-train move (human move
  success), `0x56` tax raise / tea party (`ai_king.c`). Still open: the
  `5fef_1b0e` unit-class variants (the `mov ax,N; callf 281f_04c0` pattern
  does not appear inside 1b0e's asm range — the pushes must go through a
  local helper; needs a fresh asm pass), `0x4d` naval *capture* (no Linux
  capture path). Chords `0x8020`/`0x8024` wired later the same day (driver table `0x2AB6`). Original text: Wire the
  remaining `COLDIG.BIN` event-sound push sites. The "no reachable DOS
  trigger" verdict is **retracted**: ids `0x40..0x5c` are pushed in `AX`
  (`mov ax,N; callf FUN_281f_04c0`), which Ghidra drops — see
  [assets.md](assets.md) "COLDIG.BIN" for the full id → COLDIG sample →
  DOS push site table. Playback (sample table at driver image `0x1C7B`,
  35 entries, 11025/19050 Hz unsigned 8-bit, 16-slot queue, mixed after
  the synth) is **done**. **Wired so far:** `0x40` attack fire, `0x4a`/`0x4b`
  combat won (`0x4b` when natives involved), `0x53` colony burned, `0x54`
  found colony + colony enter, `0x58` fortify/sentry, `0x5a` King's Galleon
  credit. **Still to wire:** `0x4d`/`0x4e`/`0x4f`/`0x5b` raid loot outcomes
  (`5fef_0f14`), `0x4d`/`0x57` naval capture / ship sunk (`5fef_0352`),
  `0x52` wagon-train move (`465b_0000`), `0x56` tax raise / tea party
  (`38fd_3dc8`), the remaining `5fef_1b0e` unit-class variants
  (`0x41`/`0x42`/`0x43`/`0x44`/`0x45`/`0x48`/`0x49`), and the `0x8020` /
  `0x8024` chord stings (war declaration `5bfb_153e`, assign colonist
  `2f2b_2f3e`). Ids `0x4c`, `0x50`, `0x51`, `0x55`, `0x5c` have no located
  DOS push site yet — do not invent one.
- [x] **P3.5 [user] — closed 2026-09-03.** Music is fine vs DOS. SFX are
  not all appropriate and some may still be missing, but the biggest
  offenders are gone. Remaining SFX misfires are polish, not a P3 gate.
  Historical follow-ups (colony/Europe `sound_set_bgm(2/3)`, title `0x33`,
  `0x34` never-ends) stay as D5 / assets notes if they still bite.

### P4 — Player colony production, complete

**Now:** economy loop runs end to end; expert bonuses, SoL/Tory modifier,
spoilage, hidden-resource discovery, construction and manufacturing chain
are in. Formula fidelity is the gap: manufacturing tier rates, class scale,
Town Hall L2/L3 tile rings, school/college/university training, Custom
House, horse breeding, food→colonist growth details.

- [x] **P4.1 [auto] — closed 2026-08-26, blocking premise stale + work
  already done.** Manufacturing tier rates + class scale against decomp
  ([building_production.md](building_production.md) open items). Golden
  against `golden_colony_prod01/02` once `COLONIZE/` assets are present
  in the worktree (currently missing → both goldens fail at `NAMES.TXT`).
  **The "assets missing" blocker doesn't hold in this worktree** —
  `COLONIZE/` has 332 files, `golden_colony_prod01`/`02` both pass
  (`ctest -R golden_colony_prod0`), confirmed directly. And the
  underlying work this row asks for is already extensively done:
  `building_production.md`'s own "Port status (manufacturing)" table has
  Tier rates 3/6/9, Factory input 6→9, and Class /3·×2/3 all marked
  **DOS-confirmed**, plus ~20 further dated fixes (2026-08-15 through
  2026-08-26, several player-confirmed against real DOS saves) covering
  SoL/Tory folding, Jefferson/Paine/Penn ordering, Press vs. Newspaper
  exclusivity, horse breeding (P4.5, closed separately), settlement-badge
  capacity-vs-actual, and more. Nothing further found needing a fix; this
  row's own doc note just never got updated after the asset situation
  (and the work) resolved.
- [x] **P4.2 [auto] — closed 2026-08-29.** Town Hall L2/L3 outer-ring
  tiles (was W3.3). User decision: DOS's 20-slot colony tile table is a
  legacy layout leftover — the game only ever uses the 8-tile ring, so the
  runtime stays at 8. Bridge now preserves the 12 outer bytes opaquely
  (`ColonizeColony.col1_outer_tiles`, filled on `col1_bridge_apply`, written
  back by `col1_bridge_capture`; new colonies init to `0xff`). Evidence:
  every one of ~195 real DOS `.SAV` fixtures has all 12 outer slots `0xff`,
  so DOS never writes them; the W3.3 "12/20 tiles for L2/L3" claim is
  retracted. `unit_col1_save` 48/48 green.
- [x] **P4.3 [auto] — closed 2026-08-26, already fully wired.** Training
  (schoolhouse/college/university): teacher assignment, turns-to-train,
  `@NOTEACHER`/`@TRAINFAIL*` popups full. `turn.c`'s teacher/student
  per-turn tick (~1080-1160) is real: tier-gated (Schoolhouse/College/
  University), `turns_in_job` countdown vs a tier `need`, and — a genuine
  find beyond what this row asked for — 3 real `GAME.TXT` sections
  dispatched by graduate type (`@TRAINPROFESSION`/`@TRAINCRIMINAL`/
  `@TRAININDENTURED`, not just one generic body), plus `@TRAINFAIL` when a
  ready teacher has no students. `@NOTEACHER` wired separately
  (`colonies_emit_noteacher_chrome`, `colony.c`) for the teacher-
  assignment-attempt gate itself (only an expert can teach). No gap
  found.
- [x] **P4.4 [auto] — closed 2026-08-28, static.** The 2026-08-27 puzzle
  resolved: `FUN_364b_0688` picks the type gate by controller — human
  colonies use `281f_0cfe` → `15eb_0302` (colony `+0x8a` bit per cargo ==
  `custom_house_bits`, which Linux already honoured); only AI colonies go
  through `thunk_291f_09c0` → `364b_0636` (checked in `address_mapping.csv`
  this time), so the Lumber deny term is real but never applied to the
  human. Ported the whole arm: AI gate now denies Lumber too (Ore extra
  arm still not modelled); human Custom House shut while `colony+0x1b & 3`
  (enemy armed ship / MoW adjacent); sale price is `euro_price − 1`
  (`38fd_0040`) — confirmed on the dutch2 pair (54 lumber @1, 35% → +36
  treasury, +35 `trade.gold`); tax rounding `gross − gross·tax/100`; tax
  → `royal_money`, net → nation `+0x26`; the `1dfa` ledger without the
  `0058` step (see P6.1); human sale is a real OK popup (DOS assembles it
  from load-time word pointers `DS:0x2e18/0x2e1a` not yet resolved, so the
  body is the port's summary line). Spill-over fix: DOS sells at
  `euro_price − 1` and buys at `euro_price + burden` everywhere
  (`38fd_0040`/`0016`; 1494 screenshot Food 0/8, Lumber 1/6, Silver
  19/20) — Linux had both +1. `europe_sell_price`/`europe_buy_price` added,
  `ask = bid + burden`, harbor sell / dump-sell / F5 / market strip /
  sale popups all moved over; `unit_europe`/`unit_turn`/`unit_ai_euro_expand`
  expectations updated. `ctest` 50/50. Per-cargo Custom House toggle UI
  unchanged (already `colonies_toggle_custom_house_cargo`).
  **2026-08-27 history: real near-miss, reverted — worth
  flagging for whoever next touches this.** `europe_custom_house_
  cargo_eligible`'s type-gate function (`FUN_364b_0636`) reads cleanly as
  denying Food(0)/Lumber(5)/Horses(8)/Tools(0xe)/Muskets(0xf), and
  `FUN_364b_0688`'s own body genuinely gates its whole sell block on this
  same check — both facts checked out from the raw decompile. Wired both
  (added Lumber to the deny list, called the eligibility gate from
  `europe_custom_house_autosell`'s loop) — and both `golden_colony_prod01`/
  `02` (real DOS `.SAV` ground truth) immediately failed: Lumber genuinely
  gets auto-sold to 50 in real DOS saves. Reverted both changes (comment
  left in `europe.c` explaining why). Real unresolved question for a
  future pass: either the `param_1==5` deny term doesn't mean "cargo index
  5" in this call's context (`local_b6` may be pre-transformed, not a raw
  cargo index), or `0688` actually calls a *different* type-gate than the
  one this reads as calling — the call-site resolution (`thunk_FUN_291f_
  09c0`) wasn't independently double-checked against `address_mapping.csv`
  before this revert. The original, already-passing behavior (deny Food/
  Horses/Tools/Muskets, allow everything else incl. Ore/Lumber) is
  unchanged and still what ships.
- [x] **P4.5 [auto] — closed 2026-08-26.** Found `FUN_15eb_1f72`'s horse-
  breeding tail (viceroy_unpacked.c ~12649-12690, raw asm 15eb:2300-2392):
  `potential = ceil(horses/divisor)*2` (divisor 25 with a Stable else 50,
  `FUN_15eb_038e(0x11)`), capped by this turn's food surplus
  (`ceil(max(0,food_gross-pop*2)/2)`) and by warehouse headroom — the
  applied amount adds to horses stock and debits food 1:1. The static asm
  read alone pointed at applying the *uncapped* potential to horses
  (a pipeline detail not fully resolved statically); real DOS ground truth
  (`golden_colony_prod01`/`02`, 13/13 Dutch colonies) proved the *capped*
  figure is what DOS actually applies, so the port uses that — see
  `colony_prod_horse_breed` in `colony_production.h`/`.c` for the full
  derivation. Replaces the old "manual/fandom" flat-cap-6-or-8
  approximation in `turn.c`/`colony_preview.c`. New direct unit test
  (`test_turn.c`, `colony_prod_horse_breed direct`) plus both goldens now
  pass. `ctest --test-dir build_p45`: 42/42 active tests green (4 golden
  AI suites intentionally disabled) — `golden_colony_prod01`/`02` flipped
  from failing to passing by this fix; no other regressions.
- [x] **P4.6 [auto] — closed 2026-08-26, already fully wired.** Food
  surplus → new colonist at 200, starvation warnings/deaths, `@STARVE*`
  full path. Direct read of `turn.c`: birth at `food>=200` (subtracts 200,
  spawns Free Colonist, `turn_produce_one_colony` ~765); all 5 real
  `GAME.TXT` food/starve sections are wired — `@FOOD1`/`@FOOD2` (first
  latch, stock<need not yet killing), `@STARVE1`/`@STARVE2` (colonist
  actually starves, season-picked like the other pair), `@FOODLOW`
  (production<consumption eating into stores, DOS `0xe5e` gate). No gap
  found; row was already done, just unchecked.
- [x] **P4.7 [auto] — closed 2026-08-26, already fully wired.**
  Warehouse/Warehouse Expansion caps + `@WAREHOUSEFULL`/`@SPOIL*` exact
  thresholds. `colonies_warehouse_capacity` (`colony.c` ~2027) is a
  direct `FUN_15eb_0a50` cite: `100*(1+warehouse_level)`. All 5 real
  `GAME.TXT` sections wired: `@WAREHOUSEFULL` (`colony.c` ~2100, ship
  hold→warehouse overflow) and `@SPOIL1..4` (`turn.c` ~1560, picked by
  `expanded = warehouse_level>1` × single/multi-cargo-type spoiled this
  tick — matches the section table's own 4-way split). No gap found.
- [x] **P4.8 [auto] — closed 2026-08-26, already wired; one premise was
  speculative.** Building construction: hammers/tools consumption,
  `@CARGOREADY*`, "nothing to build" and prerequisite refusals from
  `GAME.TXT`. Hammers/tools consumption: `turn.c`'s "TURN5→6" block
  (building_production.md's 2026-08-15 fix entry). `@CARGOREADY0/1/2` all
  3 wired (`turn.c` ~1611/2862, warehouse-level-gated pick). **Checked:
  no `@NOTHINGTOBUILD`/prerequisite-refusal section exists in
  `GAME.TXT`** — grepped every build/construct-shaped `@SECTION`; the only
  hit besides `@CARGOREADY*` and unrelated `@TOONEARBUILD` (founding
  distance) is `@BUILD1`-`10`, which is the opening-cinematic scroll text
  ("In the Year of Our Lord One Thousand Four Hundred Ninety-Two..."), not
  construction chrome at all. DOS's real behavior for an ineligible
  building is to omit it from the Change list, not refuse it with a
  popup — already how `colony.c`'s Change list works (min-pop/upgrade/FF
  gates filter the list itself, per `manual_gap.md`'s "Construction
  queue" row). Nothing left to port.
- [x] **P4.9 [user] — closed 2026-09-03.** User: stale / already done.
  Player admit path auto-assigns (`colonies_auto_assign_idle` on every
  `colonies_admit_unit`, plus `game_auto_assign_new_colonist` on Join
  Colony). `FUN_15eb_28c8` stays the AI/reference scorer (T1.17), not a
  player-join blocker.
- [x] **P4.10 [auto] — closed 2026-08-28.** New `golden_colony_preview01`
  (`tests/golden/test_colony_preview01.c`): both real-DOS colony-prod
  fixtures, every active colony (human + AI), `colony_preview_compute`
  vs one `turn_run_colony_production` tick — `goods[]`/`food_net`/
  `hammers` must equal the observed warehouse/hammer deltas (warehouse-
  cap clamps, pop-change ticks, building completions excluded; Horses
  ±2). Found one real drift: every AI colony's food preview was exactly
  1 short — `FUN_364b_0688` Phase B's AI `food += difficulty>>1` lived in
  `turn.c` only — and, on the Autumn fixture, every Carpenter colony's
  preview promised hammers the tick never banks (turn.c's real-DOS
  Spring-only hammers gate had no mirror). Both mirrored into
  `colony_preview.c`; 32 + 17 colonies now match exactly. Open [user]
  question: does DOS's own Production tab hide Carpenter hammers in
  Autumn, or show them regardless? `ctest`: 48/48. See
  [building_production.md](building_production.md) fix row.

### P5 — War of Independence: declarable, fightable, winnable

**Now:** declare (menu + auto) with SoL ≥ 50 gate, `@INDEPENDENCE` letter,
Europe closed post-declare, bell pool → intervention, REF wave/landing
scorer, merc offer, Continental Army muster, win/lose latches — all
"structural" or "thin". Combat: land/naval engage, best defender, fort
tiers, promote/demote/capture, plunder, coastal fort fire, Combat Analysis
— playable bar Done. Gaps are depth and the REF's own campaign behavior.

- [x] **P5.1 [auto] — closed 2026-08-28 (0982 faithful; 4d56 residue → D1).** REF campaign loop: turn-by-turn REF behavior after
  landing (target choice, siege, re-embark, reinforcement waves from
  `backup_force`), king's replies. Port from `43f7`/`4345` bodies to the
  point where a REF actually prosecutes a war against the player, not
  just lands once. **2026-08-28 — REF now prosecutes and wins the war
  against real assets** (new `golden_woi_ref01`: real Dutch fixture,
  SoL forced, menu declare, passive human — Regulars land t1, first
  capture t2, all 7 colonies fall by t24, `@LOSING` endgame latch set).
  A headless 40-turn run (scratch `woi_sim`) had shown **zero attacks**
  before; five independent defects, none visible to the existing
  synthetic-fixture `unit_ai_king`: (1) `units_find_type` is exact-match
  and the REF asked for "Regular"/"Dragoon"/"Soldier"/"Scout" while
  NAMES.TXT ships "Regulars"/"Dragoons"/... — no REF land unit ever
  spawned (now tolerant of a trailing s/.); (2) the crown slot is
  `control==2` after the declare fold, so the Euro loop never refreshed
  its moves; (3) once refreshed, `ai_euro_nation_turn` ran on the crown
  before `war_act` and spent every Regular's moves (DOS `2424` dispatches
  the crown to `2022` instead — `turn_euro_nation_is_ref` now skips the
  Euro AI for it); (4) the hunt step used `units_id_at`, so an own stack
  on the next tile (REF column, a crown wagon train visiting the port)
  "blocked" the column for good — now `units_foreign_unit_at` + a greedy
  detour over the non-losing neighbours; (5) capture required *no* human
  unit on the tile, so a demoted (unarmed) defender or civilians held a
  port forever — now only armed/mounted/attack>0 units defend, same-tile
  combat from the colony tile, civilians change hands on capture.
  **Also found on the way (real-save bug):** the king's latch bytes
  lived in `head.unknown46[]`, which *is* DOS `price_group_state[16]`
  (market pool words rewritten every EOT) — on any real DOS save the
  endgame byte read as nonzero and every WoI end-check bailed; moved to
  `game_options.woi/ref_present` + the human nation's DOS-dead
  `unknown23_pad` via `ai_king_latch_get/set` (`ai_king.h`), all
  callers/tests converted. `ctest`: 49/49. Still open in this row:
  `backup_force` reinforcement waves / king's replies / re-embark, and
  the fandom-shaped hunt vs DOS's real `4d56` crown unit-act scoring.
  **2026-08-28 (later) — `FUN_43f7_0982` ported in full** (user chose the
  faithful port over layering): MoW-pool gate/regrow, exhaust rule,
  `060a` garrison need, weakest-colony scoring, three relaxing passes with
  the `DS:0x5333=31` cap, landing-tile pick, `0512` seizure, direct land
  placement `max(3, need)` with Dragoon/Artillery caps — replacing the
  fandom MoW-cargo / second-MoW / Artillery-bias wave (those
  `unit_ai_king` blocks rewritten to DOS semantics). Also fixed: REF column
  froze when a *third* nation's units sat inside the target port (step loop
  now fights the human defender or enters an undefended port). See
  king_ref.md "`0982` REF wave — ported". `golden_woi_ref01`: capture t3,
  all lost t10. Still thin: MoW return-home (`4d56` crown ship act) is a
  despawn stand-in; `4d56` land scoring stays D1; king's reply chrome P11.
- [x] **P5.2 [auto] — closed 2026-08-26, already fully wired; one
  section name was invented.** Win condition: exact DOS rule (REF land
  force destroyed / % of REF committed and beaten / turn cap) and the
  `@INDEPENDENCEWON`-class endgame popups + score hand-off. Lose
  condition: colonies captured threshold / all-lost. **No `@INDEPENDENCEWON`
  section exists** (checked) — real name is `@WINNING`, already
  `Authentic` per `popup_audit.md`. Win rule (`ai_king.c` ~4616):
  `year >= AI_KING_YEAR_CAP && ai_king_crown_units_alive(...) <= 0` —
  REF/crown force destroyed, gated by the same year cap (1850) the
  Authentic-verdict popup row already documents. Lose: `@LOSING1` (all
  coastal ports lost) / `@LOSING2` (all colonies lost) / `@LOSING3`
  (crown pop share ≥90%) — all three already Authentic, covering "colonies
  captured threshold / all-lost" completely. **Score hand-off confirmed
  wired**, not missing: the win/lose write `col1->head.unknown46[
  AI_KING_ENDGAME_BYTE]` (`ai_king.c`), and `reports.c`'s F10 score
  (`reports_load` ~3056) reads that same byte index (`unknown46[4]==1`)
  for `independence_achieved`, feeding the score bonus — connected via a
  raw index match rather than a shared named constant across the two
  files (a code-hygiene nit, not a functional gap). No gap found.
- [x] **P5.3 [auto] — closed 2026-08-28, already covered.** Cross-checked
  every item against [combat.md](combat.md)'s peel table + status matrix:
  ambush (terrain stash → attacker, `015e` C), artillery open-field ÷4 and
  arty-in-colony ×2 vs natives (`1b0e` peels), Veteran +50% (`004a`),
  Cont. Army/Cavalry + REF Regulars/Cavalry/Artillery strengths straight
  from `NAMES.TXT @UNIT` (attack/defense), WoI crown open-field +
  REF +50% on colony + Tory/Rebel support %, Man-O-War vs Frigate/
  Privateer via the naval `004a` pipeline (holds penalty, Drake) — all
  Done and unit-tested. "Bombard" in Col1 is the coastal fort/fortress
  fire at adjacent ships (`turn_run_coastal_fort_fire`, Done thin). Only
  the deep `−0x6790` AI scoring remains, and that stays D1. No code
  change; `golden_woi_ref01` (P5.1) exercises the REF strengths live.
- [x] **P5.4 [auto] — closed 2026-08-28, DOS trace found + ported.** The
  colony-capture tail lives inside `FUN_5fef_1b0e` (viceroy_unpacked.c
  ~100905-101030, the block that picks `@CAPTURED`/`2`/`3` = DS tags
  `0x1c52`/`0x1c5b`/`0x1c48`): crown capture during WoI sets `0x5382|0x40`
  (`ref_unit_threshold`); `colony_counts`/`colony_pop_totals` move with
  the colony; colony `+0x1a` nation swaps; **rebel dividend (`+0xc2`) =
  old × 2/3** — that is the Tory/rebel population effect (SoL drops by a
  third, not reset); peacetime plunder is a **treasury share** `gold ×
  pop / (pop + Σ pop of the loser's remaining colonies)` moved to the
  captor and shown as `@CAPTURED %NUMBER0` (the port used a warehouse
  stock sum); both `nation_relation` words zeroed; WAR bit set between
  the two if not already; `@HOWTOWIN` once-latch on `0x5386` bit0.
  **No fort damage anywhere in the tail** — buildings carry over
  untouched in DOS too, so that sub-item was never a gap. Ported as
  `colonies_capture_ex` + `colonies_set_col1_context` (`colony.c`; the
  Col1 pointer is set beside `units_set_ff_col1` in turn.c/game_loop.c so
  every capture site — human move-enter, REF, Euro seize, raids — gets
  the same effects); `units_try_capture_foreign_colony` now reports the
  DOS share as the plunder token when a save is loaded. Regression:
  `test_colonies.c` `unit_capture_col1_effects` (400 = 1000×4/10, 90→60
  dividend, tallies, WAR, WoI/crown threshold bit). `golden_woi_ref01`
  now ends at t15 (the dividend cut compounds). `ctest`: 49/49.
  Earlier note kept: **Checked
  2026-08-26: 2 of 3 remaining sub-items confirmed real gaps, not
  implemented (need a decompile trace, not a guess).**
  `colonies_capture` (`colony.c` ~1431) is a bare reassignment — just
  flips `nation_id`, nothing else. Real gaps confirmed: **no Tory/rebel
  population effect on capture** (SoL% carries over untouched — DOS may
  reset or shift it, unconfirmed) and **no fort damage on capture** (fort
  level carries over untouched too). Neither number is in any doc found
  this pass; porting either without a trace risks inventing game balance,
  same risk class as P8.3/P8.5 above — flagged, not guessed. `@CAPTURED*`
  itself is **not** a gap — confirmed wired via `units_combat_notify_
  colony_captured` (`units.c`, picks `@CAPTURED`/`@CAPTURED2`/`@CAPTURED3`
  by human-involvement + plunder). **Undefended-
  colony token militia fixed 2026-08-26 (was W1.8):** `units_try_move`
  used to walk straight into any Euro colony with zero live defenders
  (colonists but no soldier) and let `units_try_capture_foreign_colony`
  capture it for free — no roll, no chance to lose. Traced DOS
  `FUN_5fef_1b0e`'s "no live defender, `colony_at_xy>=0`" branch
  (viceroy_unpacked.c ~100417-100432): it always fields a defender — a
  weak civilian stand-in from a random colonist's profession
  (`FUN_281f_04d4` RNG over `+0x1f` colonist_count, `FUN_281f_0c54`
  job read, `FUN_281f_02c6` profession→ICONS.SS index), *unless* the
  nation owns Paul Revere and has >49 muskets (`FUN_281f_07b4` FF-bit
  test + `+0xb8` muskets stock), in which case it's the real armed
  Soldier override instead — confirming Revere's already-ported
  `founding_fathers_revere_auto_arm` mechanic **is** this same DOS
  branch's special case, not a separate one. Added
  `units_spawn_colony_temp_defender` (`units.c`, mirrors the already-
  ported village empty-dwelling Brave arm) as the fallback inside
  `units_revere_defend_colony_tile` when Revere doesn't apply: phantom
  Free-Colonist-type defender, fights via the normal
  `units_resolve_land_combat_ff` path, always despawned after regardless
  of outcome (win or lose), never touches the colony's real
  `colonist_count`. New regression: `test_units.c` "undefended colony
  token militia". Full `ctest`: 42/42 active, no regressions (this path
  is gated on `g_units_ff_col1` being set, same precondition Revere
  already required, so the pre-existing free-capture test scenario —
  which never wires that global — is unaffected).
- [x] **P5.5 [auto] — closed 2026-08-28, static.** `FUN_43f7_10f0` re-read
  end to end: every unit is spawned via `281f_095c(type, DS:0x5398, …)`
  and `0x5398` is the focus (human) nation — the force is **player-
  controlled**, not ally-tagged (the old port gave it to the ally slot).
  Man-O-War placement (old W4.2 "[live]") is static after all: the 8-neighbour
  scorer wants a WATER tile (`281f_0768` → `13e4_0074`, terrain 0x19/0x1a)
  with no foreign unit (`0682`), −999 for a REF Man-O-War, score = 1 + its
  land neighbours on the colony's continent without a colony (`0722`/`06be`);
  the ship (type 0x12, pool `0x53e6` −1) lands there, then the troops:
  Cont. Cav. ≤2 (`0x53e4`), Artillery ≤2 (`0x53e8`), Cont. Army = 6 − those
  (`0x53e2`), each pool-capped, `+0x15` Veteran, unloaded at the colony
  (`0948`), 5×5 reveal. Unit types from `43f7_0082`. The Linux caps were
  also inverted (Regular ≤2 / Artillery remainder). `0x53d4` still names the
  intervening nation for `@INTERVENE`. `ai_king_foreign_intervene` rewritten,
  `unit_ai_king` 10f0 blocks replaced (7 human units incl. MoW at sea; pools
  2/1/0/0; small-pool case). The `@MERCS` arm (`param_1 != 0`, counts from
  `-0x61ba`, no pool drain) stays with `ai_king_do_merc_hire_at`. Thin spot:
  `281f_06b4 == 1` (open-ocean layer3 region) preferred, not required.
  **History — checked 2026-08-26 —
  arrival is real and DOS-cited, "control" is confirmed still genuinely
  open, not fixed this pass.** `ai_king_foreign_intervene` (`ai_king.c`
  ~3131, `FUN_43f7_10f0`-shaped) is a real port: triggers when REF is
  empty and the backup-force pool has units, up to 2 landings per call
  (3 at higher difficulty), Regular+Dragoon preferred. But every spawned
  unit is nation-tagged to the ally's own slot id
  (`ai_king_spawn_landing(ctx, ally, ...)`) — grepped for any
  human-control handoff/reassignment anywhere near the intervene path,
  found none. If DOS really does give the player direct command of these
  units (this row's own parenthetical), that's unimplemented; if DOS
  actually keeps them AI-controlled-but-friendly (a live-only fact to
  confirm), current behavior may already be correct — genuinely can't
  tell from static reading alone, matches this row's own pre-existing
  `[live]` fallback note. Man-O-War spawn placement unchanged, same
  status.
- [x] **P5.6 [auto] — closed 2026-08-26, verified all 5 sub-items already
  wired.** Post-declare economy rules: no Europe, Custom House continues,
  tax removed, bells → Continental Army promotions, SoL combat support %
  (already wired — verify). **No Europe** + **Custom House continues**:
  `game_options.woi` authoritative, closed post-declare — Done
  2026-08-22 per [roadmap.md](roadmap.md); `europe_custom_house_autosell`
  is `woi`-aware and **tax removed** there (`tax=0` when `woi` true,
  `europe.c` ~1954-1958). **Bells → Continental Army/Cavalry promotion**:
  `FUN_43f7_1eca` full port (`ai_king.c` ~3662-3806) — per rebel-owned
  colony with SoL>49%, promotes up to `max(1,min(pop>>1,...))` eligible
  units, cited against the raw decompile body
  (`viceroy_unpacked.c:74910-74972`), not a stand-in. **SoL combat
  support %**: `combat_colony_sol_at` (`combat_strength.c` ~401, used at
  ~551) folds colony SoL into the defender's combat strength. All 5
  confirmed real and DOS-cited, no gap found.
- [x] **P5.8 [auto] — MP in thirds (closed 2026-08-29, same day).** Every
  unit's `moves_left` is now DOS thirds: `units_type_max_mp` = `@UNIT
  movement × 3` (the DS:0x5234 byte — NAMES `Braves` = 1 but the table byte
  is 3, Magellan ships +3), `units_max_mp` adds the Magellan bonus,
  `units_move_cost` / `map_move_spent_thirds` is the `FUN_465b_0000` cost
  head (`terr_cost × 3`; road/colony `layer2 & 0x0a` on both tiles or minor
  river on both + cardinal → 1; tribe-settlement destination caps at 3; sea
  tile 3). Touched: spawn/`units_wake`/turn refresh/`units_refresh_all`
  writers, `units_can_afford_move_cost`/`units_try_move` (unchanged logic,
  thirds in), shore-step charge in `units_unload_passenger`, Magellan elect
  bump (+3), coastal landfall order (−3), REF MoW multi-unload budget
  (`moves_left / 3` pax), AI ship/land allotments (`ai.c`, `ai_euro.c` incl.
  the TURN4→5 pioneer hack now = a plain 3-third allotment), map panel
  "Moves:" shows `1`, `2/3`, `1 1/3` (`units_format_mp`), col1 bridge: Euro
  units import `moves` as spent thirds (`max − spent`) and export
  `max − moves_left` (exhausted land units export 0 — DOS clears spent at
  the end of a nation's day; ships on a goto keep theirs), natives keep the
  literal byte (Brave engine already tracks DOS spent, max 3). The
  pathfinder's `movement < 4` / `max_mp < 2` gates now read the thirds value
  as DOS does (Wagon Train is no longer "low move"; the `< 2` arm never
  fires). Tests: 7 unit binaries retuned (`N` tiles → `N * UNITS_MP_PER_TILE`),
  `test_ai_contact` Brave type movement 3 → 1 (NAMES). `ctest` 48/48;
  `golden_ai_turns` output byte-identical to the pre-pathfinding baseline —
  the TURN4→5 pioneer now lands on (48,39) via DOS's own three river steps.
  Not modelled: 465b's ocean↔high-seas force-to-max (Linux handles the
  Europe sail elsewhere), the partial-MP gamble for units the AI moves with
  a NULL rng (pathing still pre-filters on `units_can_afford_move_cost`).
- [x] **P5.7 [user] — closed 2026-09-03.** User playthrough: declare on a
  lategame fixture, fight to a win. Fixture-driven `unit_ai_king` stays the
  regression net.
- [x] **P5.8 [auto] — closed 2026-08-26, already fixed, checkbox stale.**
  `unit_ai_king` first-failure-blocks-suite: fix the "multi-unload fortify
  count" failure so the ~204 downstream WoI checks actually run (was W2.2
  residue). Ran `./build/unit_ai_king` directly (from repo root, needs
  `COLONIZE/` assets in cwd): exits 0, no `FAIL` lines, and every
  downstream scenario after the multi-unload/fortify stage (REF stack
  fortify caps, after-capture hunt, revolution win/lose/warn ×6,
  peacetime/wartime `@SCORED`/`@RETIRING`/`@SOONRETIRING`, WoI bell-pool
  intervention) prints `ok` — confirms the suite is no longer blocked this
  early. No code change needed; this row's own blocker was already fixed
  by an earlier, unlogged pass.

### P6 — Player ↔ Europe trade, complete

**Now:** sail/harbor/buy/sell/recruit/hire/train/purchase/equip Done;
volume-price T0 (`FUN_38fd_0058` ±1 bids) Done thin; boycotts enforced on
the Europe screen; tax audience Done; `@PRICEUP`/`@PRICEDOWN` are real
OK popups (P6.1 / P11.2).

- [x] **P6.1 [auto] — closed 2026-08-28** (every listed sub-item now
  DOS-exact or explained; see the two dated notes below). Price model to DOS: `price_group_state`, EOT
  attrition, colony production feedback, buy/sell volume thresholds per
  commodity, `@PRICEUP`/`@PRICEDOWN` as real popups where DOS pops them.
  **2026-08-28 — EOT tick now byte-exact vs two real-DOS turn pairs**
  (new `golden_market_prices01`: `COLONY00→01_no-transports` and
  `dutch2 t0→t1`; method: `sav_json` both saves, python replica of
  `FUN_38fd_0058` iterated until it reproduced the "after" save, then the
  C port made to match). Findings, all fixed in `europe_tick_market_prices`:
  (1) the phase-1 ledger is nation `+0xfc` = `trade.tons2`, not `tons`,
  and the pool decay is `price_group −= (pool + Σ max(0,tons2)) >> 7`
  computed **in nation 0's pass only** (`DS:0x9e12==0`) — so it never
  happens while nation 0 is withdrawn (the no-transports pair's pool is
  byte-identical across the turn; the dutch2 pair decays on all 16
  slots exactly); the old colony-stock`>>7` approximation is gone;
  (2) phases 2/3 confirmed real for the human pass (either removed
  breaks the match); (3) the rise/fall threshold sheds `rise*100` /
  `fall*100` from the pressure word **unconditionally** — only the ±1
  bid step is gated by `[low,high]` (Linux gated both, so a capped cargo
  like Rum at 20 ran its pressure away; same fix in
  `europe_apply_volume_price`); (4) Dutch attrition ×2 on odd
  post-increment turns (`0x9e12==3 && turn&1`); (5) `@PRICEUP`/
  `@PRICEDOWN` now real OK popups (`turn.c` FINISH, tokens cargo /
  port / new bid) instead of a status line. Also learned: the Custom
  House sells at `bid − 1` (the per-nation `DS:-0x7b44` table phase 4
  writes) — 54 lumber @ bid 2, 35% tax → +35 gold in the dutch2 pair;
  relevant to P4.4. **Still open here:** per-cargo residuals on goods
  the player's colonies produced/sold that turn (dutch2: furs −4,
  lumber +7 beyond the sale, horses +34, muskets −33; no-transports:
  sugar −4, cotton, furs −6, lumber, silver −15, tools +14, muskets −1)
  — the `1dfa` sale-volume term and/or a `364b_0688` colony-production
  feedback into `nr`, not yet traced; the golden skips exactly those
  slots. AI nations' own price records are not ticked (only their bids
  feed `ai_euro`). `ctest`: 50/50.
  **2026-08-28 (with P4.4) — `1dfa`/`1d80` traced and ported exact**
  (`europe_apply_trade_volume`; formula in
  `turn/europe_nation_eot.md`): the lumber "+7 beyond the sale" is the
  `1d44` difficulty term (`(difficulty−2)·16·amt/100` for the human,
  `−0.32·amt` for AI sellers), and every seller's sale lands on **all
  four** nation records (Dutch ×2/3) — the dutch2 pair's lumber (+93 on
  nations 0–2, +61 Dutch) is reproduced exactly from 54 human + 12 + 18
  AI tons (`unit_europe` 1dfa block). Residue that remains on the golden
  skip list is therefore the AI nations' own Europe sales/purchases in
  those turns (AI sim isn't replayed by the golden), not an untraced
  formula; the furs/horses/muskets/tools slots follow the same rule
  (`tons2` deltas in the pair match). Also: the sell price is
  `euro_price − 1` and the buy price `euro_price + burden` (fixed
  screen-wide, see P4.4).
- [x] **P6.2 [auto] — closed 2026-08-26, already fully wired.** Tax raise
  events: full `@KINGTAX` cadence formula (trigger, amount, cap),
  `@TEAPARTY` boycott of that good, boycott lift (Fugger / pay-arrears
  `@BOYCOTT*` flow). All three already real, DOS-cited (`ai_king.c`
  header comment, "ported 2026-08-19, real formula"): **trigger** =
  `FUN_38fd_5be8`'s turn-interval gate (`ai_king_audience_roll`);
  **amount** = the same function's favor-score-ladder signed delta
  (cut/+1/+2/+3-4/+5-8); **cap** = `FUN_38fd_3dc8`'s unconditional
  apply, clamped 0..75%. `@TEAPARTY` follow-up (accept keeps the hike /
  refuse reverts it + boycotts a roulette-picked cargo) wired the same
  pass. Boycott lift: Fugger clears `boycott_bitmap` on election
  (`founding_fathers.c`, confirmed earlier this session); pay-arrears is
  `europe_buyback_boycott` (`FUN_38fd_2dfe`, `@SOMEBOYCOTT` trigger,
  `ask_price×500` cost) — **Done** 2026-08-24 per `manual_gap.md`. No
  literal `@BOYCOTT*` section exists (checked — real name is
  `@SOMEBOYCOTT`); the full `@KISSUP`/`@KISSSORRY` Pay/Cancel CHOICE
  *dialog chrome* stays thin (immediate action + status line instead of a
  VGA confirm box), consistent with this project's existing D4 VGA-chrome
  deferral, not a content gap.
- [x] **P6.3 [auto] — closed 2026-08-26, all 5 sub-cases already covered.**
  Sell/buy edge cases: partial holds, selling into a boycott, buying with
  insufficient gold, 100-unit lots, tax applied to sales only. Direct read
  of `europe_buy_cargo`/`europe_sell_hold`/`europe_sell_proceeds`
  (`europe.c`): partial holds fill/drain existing partial slots before
  opening a new one (both directions); boycott gates both buy and sell
  with a status line, same as `europe_cargo_boycotted` already documented
  in [manual_gap.md](manual_gap.md); buying clamps to `gold/ask` (no
  overspend possible, so nothing to "handle" beyond the clamp — see the
  `@NOGOLD*` note below); each hold slot caps at 100 units both ways
  (buy's `buy>100→100` clamp, sell empties a slot in one call), matching
  "100-unit lots"; tax is applied only in `europe_sell_proceeds`
  (`bid*amount*(100-tax)/100`) — `europe_buy_cargo` charges flat
  `bought*ask` with no tax term. Nothing here needed a code change, only
  confirming the row against the actual functions. **Checked 2026-08-26:
  `@NOGOLD*` doesn't exist** — grepped
  `COLONIZE/GAME.TXT` for every gold/afford/treasury-shaped `@SECTION`;
  the only real candidate, `@NOTENOUGH` ("your treasury is not large
  enough to back your promise"), is already correctly attributed
  elsewhere to deep village-haggle `2820` (PARKED per
  [popups.md](popups.md)), same for `@BUYWHICH`/`@BUY0`/`@BUY1` (Indian
  trade CHOICE bodies, not the Europe market screen). DOS has no modal
  for "can't afford this Europe purchase" at all — `europe_buy_cargo`'s
  existing behavior (silently clamp `buy` to `gold/ask`, plain status
  line) is therefore not a gap to close, just needed the stale invented
  section name removed from this row.
- [x] **P6.4 [auto] — closed 2026-08-26, premise didn't hold.** Equip/
  unequip in Europe (muskets/horses/tools pricing via market, missionary
  bless cost) and the dock-order menu completeness. **DOS has no
  equip/unequip UI on the Europe dock at all** — checked `MENU.TXT` for
  every equip/musket/horse/bless-shaped menu string, zero hits; DOS
  equips a unit via colony fence icons (already Done, `colony.c`), not a
  Europe submenu. Muskets/horses/tools "pricing via market" is just the
  existing Europe buy-cargo flow (already Done) — nothing separate to
  price at equip time, the cost was already paid buying the cargo.
  **"Missionary bless cost" doesn't exist either** — direct code comment,
  Colonization.pdf-cited: "Church bless: leave as Missionary (no cargo
  cost)" (`colony.c` ~1245), a free colonist-conversion action, not a
  priced one. Dock-order menu (None/Don't board/Board next/Move to
  front) already matches DOS's real 4-option set (confirmed earlier this
  session, `manual_gap.md`). Nothing found needing a fix.
- [x] **P6.5 [auto] — closed 2026-08-26, verified already correct.**
  Trade routes with Europe as an endpoint (`TRADE` editor already Done
  structural — verify Europe stops, wagon/ship auto-buy/sell amounts).
  `game_trade_route_service_stop` (`game_loop.c` ~6605): a sea unit at
  the Europe stop (`colony_index==999`, eastern high-seas tile) sells its
  *entire* hold via `europe_sell_unit_hold` in a loop until empty — real
  auto-sell, all cargo types, no invented cap. **No auto-buy at Europe,
  and that's confirmed correct, not a gap**: the TRADE editor itself
  already disables load-list configuration for a Europe stop
  (`trade_edit_need_load = (stop_idx != 999)`, `game_loop.c` ~956/2255) —
  a deliberate, pre-existing design signal that Europe stops are
  sell-only by intent, matching DOS trade routes (buying at Europe is a
  manual Europe-screen action, not something a route automates). Wagon
  trains can never physically reach the Europe stop tile (land-only,
  high-seas is water) so the `units_is_sea` gate correctly makes a
  wagon-assigned Europe stop a inert no-op rather than a crash — a minor,
  likely-unreachable-in-practice UX edge (no status feedback if a player
  somehow assigns one), not fixed this pass, low priority.
- [ ] **P6.6 [user]** Europe screen behavior review with the user.

### P7 — Rumours and treasure

**Now:** `units_resolve_lcr_rumour` thin transcription of `FUN_65dd_0004`
(case table documented, WoI case-1→2 redirect + case-5 latch Done, weight
reroll loops PARK); treasure train spawn/tick/cash, Cortes conquest
treasure, king's galleon transport with Cortes free, ransom on capture.
KINGGALLEON2 (non-Cortes galleon share string) PARK.

- [x] **P7.1 [auto]** (2026-08-26) LCR outcome weights + reroll loops from
  `65dd` (difficulty, de Soto, unit type Scout vs other, already-explored
  latch), so outcome frequencies match DOS. Real state machine ported
  (`units_lcr_roll_outcome` in `src/core/units.c`, replacing the flat
  percentage table): skill tier from unit type (`Scouts`)/profession
  (Seasoned Scout), de Soto gated on Scout-type per decomp (not universal —
  a real correction), base `RNG(1,9)`/floor-ratchet roll + `RNG(1,100)+
  skill*10` gate, case-5/8 skill-scaled "kicker", de-Soto reroll-until-not-
  Nothing loop, case-8 hidden trespass now difficulty+skill-scaled and
  proximity-gated (kept visible as `TRESPASS_ANGER` for continuity — P7.2
  call). Also revised case 1 from `TRESPASS_ANGER`→`FOUNTAIN_OF_YOUTH`
  (decomp's own 8x immigrant loop is literally on case 1, matching this
  file's existing FoY citation; WoI redirect is now FoY→Survivors, not
  Trespass→Survivors) — flagged in code comments for a second look. Terrain
  qualify test (`FUN_281f_078c`) stays unresolved; PARKed as an unbiased
  coin flip (previously "always fails", which made case 1 unreachable
  outside de Soto and broke an existing test — fixed). ctest: 42/42 active
  (4 golden suites disabled), same as baseline; added a de Soto Scout-type
  gate regression test + reworked the case-5-latch seed search for the new
  RNG call shape.
  **2026-08-27 follow-up — every P7.1 PARK closed statically:** terrain
  qualify = `terrain_class_at` pedia class (case 1 wet classes, case 2
  desert/scrub/hill/mountain); `0x1dc6/0x1dc7` session counters (FoY needs
  ≥4 rumours explored, Cibola capped at 7 per process) as statics +
  `units_lcr_reset_session_counters`; case-5 colony gates use
  `stuff.census_pop_proxy`/`colony_counts` (+ Pioneer 50% mercy); case 3 =
  3d8×10×(skill+2)/2, case 7 = 4d10×2, Cibola = ((skill+2)×10+RNG(1,20))
  hundred, Burial3 = (RNG(1,8)+(skill+5)×2)×2 hundred, burial variant by
  the loop's `gate` (<25/<50|<65/else) and tribe claim `RNG(1,(dist+5)<<
  skill)<4` → SCREWED +100 relation (no kill). **Case 2/9 identity was
  swapped in the port** (dialog tag is literally "LOSTCITY"+case; case 2
  spawns unit type 10 Treasure, case 9 type 0 Colonist) — fixed. See
  mysteries_catalog.md 65dd entry. ctest 44/44.
- [x] **P7.2 [auto]** Each LCR outcome fully applied. **Status
  2026-08-26 (checked, this "Now" framing was stale):** all 9
  `units_lcr_roll_outcome` cases in `units.c` (`units_apply_lcr_outcome`
  area, ~2658-2790) are wired with real `@LOSTCITY*`/`@BURIAL*`/`@SCREWED`
  bodies — Cibola/small-treasure/chief's-gift/burial-mounds gold amounts
  (Cibola includes `+difficulty`), survivors-join colonist spawn,
  trespass/burial-mounds native-anger relation delta, unit-vanishes
  despawn are all real, not stubs. **One genuine remaining thin spot:**
  Fountain of Youth's 8 free immigrants call `europe_immigrant_from_pool`
  with `rng=NULL` (deterministic first-filled slot) instead of a player
  pick-among-pool popup — PEDIA doesn't require a picker for FoY
  specifically (that's a Brewster thing), so this may not even be a real
  gap; flagging rather than closing outright. **Closed 2026-08-28 — it
  was a real gap.** `FUN_65dd_0004` case 1 (viceroy_unpacked.c 103727-
  103731) is literally `for (8) FUN_291f_0d2c(1, 0)` = eight calls of the
  Recruit picker `FUN_38fd_4884` with `param_1=1`: passage forced to 0
  (64695-64697), no `+6` recruit-count bump and no `+0x2e` crosses clear
  (both gated on `param_1==0`, 64778/64766) — i.e. the player chooses each
  of the eight from the live 3-slot pool. Ported: `units_fountain_youth_
  enqueue_pick` / `_apply_popup` (`units.c`, new `AI_POPUP_TAG_FOUNTAIN_
  YOUTH`, `@RECRUIT` body with `%NUMBER0`=0 since 4884 draws the same list
  with a zeroed passage), chained pick→refill→next pick until 8 have
  landed; `europe_recruit_free_from_pool` (`europe.c`) is the no-charge
  4884 tail; `game_loop.c` applies the result. No-UI callers (AI, tests
  without a popup queue) keep the old first-filled fallback. Regression:
  `test_units.c` "fountain of youth 8x free recruit pick". Row done.
- [x] **P7.3 [auto]** Treasure train: move rules (1 MP, no boarding
  except Galleon), cash-in at coastal colony w/ Galleon absent →
  king's offer (`@KINGGALLEON1`, share % by difficulty), Cortes free,
  transport by own Galleon → Europe cash at full value; WoI behavior.
  **2026-08-26: "1 MP" already correct** (data-driven from `NAMES.TXT`
  `@UNIT`'s Treasure row, movement=1 — not a port constant, nothing to
  fix). **"No boarding except Galleon" was a real gap, fixed**:
  `units_find_boardable_ship` (`units.c`/`units.h`) let a Treasure Train
  board *any* ship with room; added a `require_galleon` param (both call
  sites now pass `strstr(mover_type->name,"Treasure")!=NULL`), so only a
  type named "Galleon" qualifies when the boarding unit is a treasure.
  New regression: `test_units.c` "P7.3... Treasure Trains may only board
  a Galleon" (Caravel present → rejected; Galleon added at the same
  stacked tile → accepted). `ctest`: 42/42, no regressions. **`@KINGGALLEON1`
  doesn't exist** — checked; real sections are `@KINGGALLEON2` (non-Cortes
  share offer, still PARK per P7.4/T1.13) and `@KINGGALLEON3` (Cortes free
  transport, tax-share only — already wired,
  `units_cortes_cash_coastal_treasures`). Cortes-free path, Europe
  cash-at-full-value, and Cortes-conquest-treasure spawn were all already
  wired (`units_spawn_treasure_train`, `units_cortes_cash_coastal_treasures`,
  `europe_cash_treasure`). **Closed 2026-08-28:** the non-Cortes king's-
  offer flow is the ported `FUN_5fef_1908` (see P7.4) and its WoI arm is
  explicit — during the war the King is gone, so a coastal Treasure is
  credited at full value (`@CASHTREASURE`) with no offer. Row done.
- [x] **P7.4 [auto] — closed 2026-08-28, already resolved + ported.**
  `@KINGGALLEON2` was found 2026-08-27 as `FUN_5fef_1908` (the string is
  built as "KINGGALLEON"+"2"/"3", which is why every grep for the literal
  tag failed) and is fully wired in `units_king_galleon_offer_coastal_
  treasures` (`units.c`): non-Cortes coastal Treasure without a Galleon →
  CHOICE popup with the difficulty-scaled share (`units_king_galleon_share_
  pct`), Accept → `units_king_galleon_credit`; Cortes → `@KINGGALLEON3`
  free transport; WoI → `1908`'s else-branch, no King, full value via
  `@CASHTREASURE`. Nothing left to re-attempt.
- [x] **P7.5 [auto] — closed 2026-08-26, already fully wired.** Rumour
  tile clearing + Col1 `path`/`mask` bits so DOS-loaded saves and
  port-explored rumours agree (P10 tie-in). `col1_bridge.c` (~682-706)
  already does exactly this: DOS's Col1 format has no dedicated "rumour
  already explored" bit, so the port reuses the `path` field's
  visitor-history nibble (`0xf` = never occupied) as the signal — any
  tile a unit has ever stood on gets `MAP_LAYER2_RUMOUR_CLEARED` on
  import, since resolving an LCR always means standing on it. Real
  player-reported bug fix (`dutch-reports.SAV` — every already-explored
  mound showed as fresh on load), with a dedicated regression
  (`test_col1_save.c` ~1758-1835) loading that exact fixture and
  asserting no visited tile still reports a live rumour. No gap found.

### P8 — Basic Indian interactions (teach, alarm, gifts, raids)

**Now:** structural contact/meet/teach/gift/demand/convert/raid
(`ai_contact.c`), alarm bookkeeping, encroachment, missions, Pocahontas.
Deep `2820` (village trade/haggle) and `4528` (deep settlement battle) PARK.
**Village trade is deferred (D2)** — do not open `2820`.

- [x] **P8.1 [auto] — done 2026-08-28 (static).** Teach is now the DOS
  "Live Among The Natives" menu action (`thunk_FUN_1000_a618`, ported as
  `ai_contact_live_among_natives`): `@LEARNSTAY` Yes/No CHOICE →
  `@LEARNDONE` / `@LEARNLATER`; `@LEARNSLOW` random refusal in the 25..49
  alarm quartile (`rand(1,1000) < 200*difficulty+100`); `@TEACHCONVERT` for
  Indian Converts; `@LEARNMAD` at quartile ≥ 2 (+3 alarm, silent when
  met-but-no-peace); `@LEARNALREADY` only when taught && !capital;
  `@LEARNMASTER` / `@LEARNCRIMINAL` kept. Taught skill = the 2154 bid
  table with DOS tech trims, Fur-Trapper→Seasoned-Scout on `(x+y)%3==0`,
  Farmer→Fisherman ocean roll (was: last-sold cargo / nation default). The
  earlier "needs a decompile trace of the 5bfb teach dispatch" blocker was
  an asm read of the `PUSH imm16` suffixes Ghidra dropped. Full decode:
  [indian_actions_menu.md](../original_sources_annotated/ai/indian_actions_menu.md).
  Human teach is menu-only now; the per-turn auto-teach pulse stays for AI
  nations. Tests: `test_ai_contact.c` "@ACTIONS village menu".
- [x] **P8.2 [auto] — closed 2026-08-26, substance already done; "F9
  attitude words"/"HELLO*" premise was wrong.** Alarm model for the
  player: per-village + tribe alarm accrual from proximity/land use/
  missions/combat, decay, thresholds for attitude words in F9 and
  `@INDIANCOMMENT`/`HELLO*` bands, Pocahontas halving — from
  `FUN_4d56_152e` (already ported) + the reader side. Writer side
  (`ai_contact_152e_village_growth`, alarm/friction accrual from
  proximity/encroachment/missions/combat) and Pocahontas ongoing
  half-rate (`ai_contact_alarm_bump_amount`) were already confirmed live
  in earlier sessions. Reader side checked this pass: `@INDIANCOMMENT`
  (encroachment alarm-band comment, threshold-crossing at friction≥40) is
  wired (`ai_contact.c` ~3603-3648). **F9's real DOS layout has no
  attitude-word text column at all** — confirmed via
  [report_screens.md](report_screens.md)'s own golden-pixel RE
  (`FUN_3f41_010a`): the row is name/level/villages/missions/muskets/
  horse-herds; the only alarm-tied visual is a 5-variant headband
  portrait index (`ICONS.SS` #113-117, still hardcoded to #113 — a real,
  already-tracked cosmetic gap, not a text-attitude gap). **`HELLO*`
  doesn't map to Indian tribes at all** — `@HELLOFIRST`/`USA`/`AHOY`/
  `MEEK`/`MANLY` are Euro-rival first-contact greetings ("claimed all of
  this land in the name of {King}... here to {mission}"), unrelated to
  this row's Indian-alarm scope; genuinely unwired (`ai_diplo.c` has no
  first-contact greeting chrome at all, hand-typed or real) — noting it
  here as a real, separate finding for whoever next touches Euro-rival
  first contact, not fixed in this pass (out of P8's Indian scope, and a
  new UI moment — first-contact modal — needing `[user]` sign-off on
  when/how it interrupts play, not a silent auto-port).
  **2026-09-03:** HELLO is wired (`ai_diplo_153e_encounter`:
  `@HELLOFIRST`/`AHOY`/`MEEK`/`MANLY`; `@HELLOUSA` still not modeled).
  User: P8.2 looks fine now.
- [x] **P8.3 [auto] — closed 2026-08-28 (static).** The suspected
  mis-mapping was real and is now resolved: `@TRIBUTE` / `@TRIBUTEUSA` /
  `@GIFTS` / `@WANTSTUFF*` are **Euro-rival** diplomacy text (resident
  `FUN_1d1d_07e4(…,0x1916)` builds `TRIBUTE`+`USA`), not Indian gift/demand;
  `@CHIEFGIFT` / `@CHIEFBORED` belong to "Ask to Speak With Chief". DOS has
  **no** player-initiated gold-gift village action at all (goods gifts =
  the 2820 trade gift arm, already ported) — the port's invented Gift row
  is removed from the menu (handler kept, unreachable). "Demand Tribute" is
  `thunk_FUN_1000_a5f4`, ported as `ai_contact_demand_tribute`: continent
  strength roll (exposed Euro combat vs Brave combat, Spanish/Cortes ×1.5),
  `@EXTORTSTUFF` (10 of the village's top bid good into the nearest colony,
  once per village — `tribe.state.tribute_paid`, DOS +3 bit 0x10) /
  `@EXTORTPOOR` / `@EXTORTNO` / `@EXTORTLAUGH`, alarm bump `difficulty+1`
  (×2 on success, 0 on POOR). Decode in
  [indian_actions_menu.md](../original_sources_annotated/ai/indian_actions_menu.md).
- [x] **P8.4 [auto]** Raids on player colonies: trigger (alarm band +
  proximity), target pick, outcome table (`@RAID*`: burn building, steal
  goods, damage ship, kill colonist, plunder gold), stockade/soldier
  defense, `@RAIDWIN*` — thin port of the `4528` raid *outcome* path only,
  not its deep AI. **Status chrome upgraded 2026-08-26** (was the doc's own
  flagged debt — `indian_raid_outcomes.md` "Status-line chrome for other
  raid/warn kinds is thinned"): the 6 human-facing outcome kinds
  (`NOTHING`/`STORES`/`BURN`-named/`SCALP`/`SHIP`/`GOLD`) now call
  `popup_msg_fill` with the real `GAME.TXT` `@RAID*` body instead of a
  hand-typed paraphrase, with the same old text kept only as the
  no-catalog fallback. Needed 3 new module-static "what got hit" values
  (`ai_contact.c`: stolen cargo name, damaged ship's `units_display_name`,
  gold drained) threaded from `ai_contact_apply_raid_loot`'s existing loot
  logic out to the status block, same pattern as the already-shipped
  `s_last_burn_building`. `@RAIDWREAK` intentionally left as its existing
  thin paraphrase — its real DOS text is a third-party "Spies report... in
  the {nation-adjective} colony of..." frame, wrong register for the
  victim's own status line (per this row's own `indian_raid_outcomes.md`
  note). New regression: `test_ai_contact.c`, real-`GAME.TXT` STORES
  scenario asserting the actual body text incl. substituted cargo name.
  **Defence half closed 2026-08-28 (static):** `FUN_5fef_0f14`'s head is
  the walls gate — `FUN_281f_0ab0(0)` counts the Stockade→Fort→Fortress
  chain, `rand(0,12)-1 (+difficulty-2 vs a human)` `< walls*3+1` →
  `@RAIDNOTHING` "raiding party wiped out" (bare 1/13, Stockade 4/13, Fort
  7/13, Fortress 10/13) — ported into `ai_contact_pick_raid_kind`, with the
  early-game building/unit-kind demotion (turn < (2-difficulty)*40 on
  Discoverer/Explorer). The soldier fight was already the real combat
  engine + P5.4 militia. See indian_actions_menu.md "Thin spots". **Checked
  2026-08-26: `@RAIDWIN*` doesn't exist** — grepped every win/defend-shaped
  `@SECTION`; the real tags are `@INDIANWIN0/1/2` (field-unit ambush, tribe
  beats a unit in the open — already wired, `ai_contact.c` ~5237-5261) and
  `@INDIANWINCOLONY`/`@INDIANWINCOLONY2` (colony massacre framing, human vs
  spy-report variant — unused, candidate for the abandoned-colony "%s
  overrun %s!" hand text above, not attempted this pass, needs its own
  check against `@BURNED` which already covers the SCALP/BURN abandon
  case). This row's own `@RAIDWIN*` wording was an invented section name,
  same class of error as the `@NOGOLD*`/`@NOTENOUGH` ones found earlier —
  removing it rather than porting to nothing.
- [x] **P8.5 [auto] — done 2026-08-28 (static).** All three DOS
  encroachment CHOICEs found and ported: `@INDIANLAND` (found colony,
  `game_do_found_colony_at_unit`), `@INDIANFOREST` (clear order,
  `thunk_FUN_1000_91fc`) and `@INDIANROAD` (road order,
  `thunk_FUN_1000_9304`) via `game_request_indian_land_choice` +
  `AI_POPUP_TAG_INDIAN_LAND`. Findings that dissolved the old blocker:
  "Take it" has **no** immediate consequence in any site (friction is the
  already-ported 152e pass — nothing to invent); "offer gold" is greyed
  when unaffordable (`func_0x000193a6(dlg,2,1)`; port drops the row);
  the dialog only appears at PEACE (`8c28 & 0x40`) with a real price, and
  outside it DOS founds/clears/builds **free** — the port's silent auto-pay
  and "need N gold" hard block are gone. "Offer gold" →
  `colonies_indian_land_pay` (debit, `lands_bought++`, purchased bit) +
  `@INDIANBRIBE`. Also fixed while there: tribal-land radius is the tech
  tier (`FUN_15dc_006a`: Inca 3 / Aztec 2 / others 1) with a same-continent
  filter, not "capital ? 2 : 1" (`colonies_tile_indian_homeland`).
- [x] **P8.6 [auto] — done 2026-08-29, static.** `FUN_6f74_0042`: when
  `DS:0x1f5c` (the contact tribe, set from `0x8d52` at every Indian contact
  dialog) is < 8 the compositor loads `IND{tribe}A{tier}.SS`, tier =
  `15dc_00a2(alarm_by_player[tribe][nation])` (<25/50/75 → 0/1/2, else 3).
  Port: `AiPopupRequest.portrait_tribe/tier`, lazy per-sheet cache remapped
  onto the game palette (`ai_popup.c`), figure drawn full-height left of the
  dialog with the pair centred; attached in `ai_contact_human_chrome` for
  every tribe-addressed popup. Placement made DOS-exact with P11.3 (same day): side by tribe, frame spans sprite + dialog.
- [x] **P8.7 [user] — closed 2026-09-03.** Contact flow on a fresh game:
  user-passed.
- [x] **P8.8 [auto] — done 2026-08-28 (static).** The meet menu is now
  DOS's real `NAMES.TXT` `@ACTIONS` list with the real per-unit gating
  (`FUN_4d56_4528` human arm, overlay 13 `0x478a..0x4bdb`): wagon/ship →
  Trade / Enter Hostile Village (`a5e8`: rand(0,500) vs alarm →
  `@KILLWAGONS` / `@MADATWAGONS` / `@GRUDGEWAGONS`); Scouts → Ask to Speak
  With Chief (`a60c`: `@CHIEFHOWDY` + guides/tales/gift/bored, Arawak kill
  roll, `@CHIEFKILL` unless Coronado); Missionaries → Establish Mission
  (`a5dc`, `@MISSION0..3`, Sepulveda/Las Casas/Pocahontas/French count
  terms) or Denounce Heresy of {rival}'s Mission (`a594`, weighted roll →
  `@HERESY0/1`, replaces the invented 50/50 for human units); colonists →
  Live Among The Natives (P8.1); armed → Demand Tribute (P8.3) / Attack
  Village (commits the move in `game_loop`). Body = `@VILLAGEHAPPY/SAVAGE/
  MEDIUM/BAD/WAR` by alarm band with the `@LEVELS` noun. The invented
  Attack/Leave "raid warn" CHOICE is retired for met tribes (kept only as
  the unmet fallback). `@ORDERS` "Live In Village" is this same action
  (immediate, not a persistent order). Thin: `FUN_4cc6_03f8`'s nearby-threat
  term in the heresy roll is 0; see the doc's "Thin spots".
- [x] **P9.1 [auto]** Write a per-FF status table into a new
  `docs/founding_fathers.md` (effect, DOS FUN, port symbol, test) — it
  does not exist today; `fandom_col1994.md` is Tier-3 evidence only.
  **Done 2026-08-26.** All 25 Fathers tabulated, sourced from
  `COLONIZE/PEDIA.TXT` `@FATHER0`–`24` (Tier 1, not fandom) with DOS FUN
  addresses, port symbols and test coverage from a direct code read;
  `NAMES.TXT` `@FATHERS` weight table diffed byte-exact against
  `founding_fathers.c`. Confirmed accurate: the wired-17 list and the
  Fugger/Coronado/La Salle/Jones "core-files-only" claim (all 4 are
  correctly elect-only, no ongoing gate needed). Corrected: Magellan is
  wired in 3 places, not "turn.c only" (also `col1_bridge.c`). Found and
  filed as open items (not fixed — P9.2 territory): La Salle only
  sweeps at elect, not per-turn, for "future" colonies reaching pop 3;
  Drake has 2 call sites computing the same bonus (dedup candidate);
  Jones's frigate isn't code-restricted to the human nation. Confirmed
  Adam Smith's 1.5× factory throughput **is** wired at the production
  math, not just the build gate — closes that P9.2 uncertainty early.
  See [founding_fathers.md](founding_fathers.md).
- [x] **P9.2 [auto] — closed 2026-08-28.** Port missing/thin player-facing effects. **Status
  2026-08-26 (see [founding_fathers.md](founding_fathers.md) for the full
  per-FF table):** Fugger, Coronado, Magellan (+1 naval MP; west-edge sail
  time PARK, no decomp evidence), John Paul Jones, Adam Smith (factory
  1.5× confirmed wired in production, not just build gate), Stuyvesant
  (build gate + autosell; per-cargo UI stays P4.4), Minuit — all already
  **Done**, no further work found needed here. **La Salle fixed this
  pass:** was elect-time-only (future colonies / colonies growing into
  pop 3 later never got the free Stockade); now re-swept every
  `founding_fathers_tick` while owned, matching the Las Casas re-tick
  shape — closes the "existing + future" PEDIA text gap. **Follow-up same
  day (user-reported):** the tick-only fix still granted it "next turn"
  from the player's seat; DOS shows it the instant a colony hits pop 3.
  Added `founding_fathers_la_salle_check`, called from `colonies_admit_unit`
  (`colony.c`) right when a join crosses the threshold — synchronous, no
  turn wait. Threaded a `col1` param through all 10 `colonies_admit_unit`
  call sites (`ai.c`, `ai_euro.c`, `game_loop.c`) to make this possible.
  EOT-driven growth (food-surplus birth) needed no change — it and
  `founding_fathers_tick` already run in the same `turn_processor_start`
  pass, before the player sees the next turn. Checked and
  closed as non-issues: Drake's two ×1.5 call sites are confirmed to be
  two genuinely different combat formulas (general naval engine vs
  coastal-fort-fire), not a dedup candidate; Jones's frigate not being
  human-restricted is confirmed byte-faithful to DOS (`FUN_4345_0342`
  `param_2==0xe` branch has no nation-0 guard — same shared dispatch
  every FF case uses). No further P9.2 work found needed this pass.
  **2026-08-28 close-out (static, asm-backed):** three real gaps found by
  re-reading the FF-bit call sites instead of the doc table. (1) **de Soto
  sight** was elect-only: `FUN_13f1_02f8` (Ghidra dropped the `15eb_3960`
  result; the `CODE_17` listing has it) = radius 1, Galleon/Privateer/
  Frigate 2, de Soto → every non-ship 2, Scouts +1; `FUN_13f1_0158` reveals
  the outer ring only for the unit's own domain (water / same continent).
  Ported as `units_sight_radius` + `map_reveal_sight`, wired at all unit
  reveal sites. (2) **Magellan / voyages:** the port's 2-turn-east /
  4-turn-west crossing was invented; DOS `FUN_48d3_0002` (both directions
  via `291f_0aee`) is 1 turn, or 2 on `RNG>89 && ships>2 && !Magellan` —
  `europe_voyage_turns_roll`; the immigrant-Merchantman landfall had the
  polarity inverted (Magellan *caused* the delay) and gated on docks. The
  `x<3` west-edge branch discards its RNG/FF results (asm), so the "west
  edge shortcut" PARK is closed as the same roll. (3) **Brewster pick**:
  `5e52` FF-0x14 branch → `FUN_38fd_4884(0,1)` = `@RECRUITCHOOSE` free
  pick; `europe_tick_immigration_pressure`→2, `AI_POPUP_TAG_BREWSTER_PICK`,
  `units_brewster_apply_popup`; cancel keeps crosses (re-asks next turn).
  Also refreshed [founding_fathers.md](founding_fathers.md) rows 5/7/10/20
  (stale `65dd` + `KINGGALLEON2` PARKs). `ctest`: 50/50 active. **No
  Father has a PARK left**; only P4.4's Custom-House per-cargo UI and
  Franklin's FA chrome (P11) remain, both outside this track.
- [x] **P9.3 [auto] — closed 2026-08-26, already satisfied.** Verify each
  wired effect with a unit test if none exists (`test_founding_fathers.c`
  covers a subset). [founding_fathers.md](founding_fathers.md)'s P9.1
  per-Father table (all 25 rows) already cites a real test for every one
  — not just `test_founding_fathers.c`, several route through the AI
  suites that actually exercise the effect in context (`test_ai_euro_*`,
  `test_ai_contact.c`, `test_ai_diplo.c`, `test_colonies.c`, `test_turn.c`,
  `test_units.c`). This row's "covers a subset" framing was accurate when
  written but stale by the time P9.1 finished the full table — no
  Father is missing coverage.
- [x] **P9.4 [auto] — closed 2026-08-26, premise didn't hold.** FF
  election chrome: `@WHICHFREEDOM`/`@FREEDOM` bodies already authentic;
  add the elect-effect one-liners DOS shows (e.g. Coronado reveal, Jones
  frigate arrival) where `GAME.TXT` has them. Grepped `GAME.TXT` for every
  Father's name/effect keyword (frigate, reveal, etc.) — the only hit,
  `@KINGFRIGATE`, is the King's unrelated merc-frigate-offer dialog, not a
  Jones announcement. `@FREEDOM` itself is the *only* elect-time text DOS
  ships ("{Father} has joined the Continental Congress!", generic, no
  per-Father effect blurb) — both `@WHICHFREEDOM` (`founding_fathers.c`
  ~731) and `@FREEDOM` (~1289) are already wired. There is nothing further
  to port here; DOS has no per-effect one-liner asset for any Father.
- **Deferred** in this track: effects that only matter for rival AI
  behavior parity (D1) and KINGGALLEON2 string (P7.4 handles the
  gameplay).

### P10 — Mapgen + DOS save interop: keep green

**Now:** Col1 save/load byte-identical on all 19 `.SAV` fixtures (W1.5
closed); mapgen matches MAPEDIT for terrain/resources/rumours; `.MP` load
Done.

- [x] **P10.1 [auto] — closed 2026-08-29.** Strict codec round-trip was
  green (19/19) but nobody had automated the *port-written* half, so
  `unit_col1_save` now also does apply → capture → re-apply per fixture and
  asserts nothing is lost. That net caught two real losses on first run:
  (1) `COLONIZE_COLONIES_MAX` 32 truncated the five 33-colony lategame
  fixtures — DOS gate is `colony_count < 0x30`, cap now 48; (2) human ships
  in the Europe harbor / Expected / Bound lanes live only in `EuropeScreen`
  after `units_despawn_ship_with_cargo`, so Save dropped them (COLONY04
  −1 unit, COLONY06 −2). Capture now writes them as DOS does (`228+n` port,
  `232+n` outbound, `244+n` inbound, `turns_worked` = voyage turns,
  passengers chained) and apply reads the two transit lanes back with
  passengers aboard instead of dumping them on the docks. Details in
  [savegame.md](savegame.md) "Human Europe ships". ctest 48/48.
  **2026-09-03 [user]:** Linux-written save with a ship at sea loads in DOS
  and arrives.
- [x] **P10.2 [auto] — done 2026-08-26.** Added `tools/check_save_interop.sh`:
  builds just the `unit_col1_save` target then runs it alone via
  `ctest -R '^unit_col1_save$'` (the strict byte-identical round-trip over
  all 19 Col1 `.SAV` fixtures — W1.5's regression net) instead of the full
  42-test suite. ~0.1s. Optional arg overrides the build dir (default
  `build`). No CMake changes — no existing test carried labels, so a
  ctest label wasn't worth the churn; a thin wrapper script matches this
  row's own "(or ctest label)" either/or.
- [x] **P10.3 [auto] — done 2026-08-29.** Legacy COLZ save path removed
  (was W3.4): `savegame_write`/`savegame_read`, `ColonizeSaveHeader`/
  `ColonizeSavePayload` and the two `smoke_savegame*` tests deleted; no
  gameplay code called them. `savegame.h` is Col1-only now. ctest 48/48.

### P11 — Popups: right text, options, layout

**Now:** ~80% of player-facing modals are "Authentic" per
[popup_audit.md](popup_audit.md); remaining MissingWire/Partial rows are
mostly in contact (`@LEARN*`, `@RAID*`, `@CHIEF*`), order gates
(`@NEEDTOOLS`…), FA `3f41` thin, boycott `DIPLO_BOYCOTT`, and "Invented"
title strings in save/load. `@PRICEUP`/`@PRICEDOWN` and HELLO greetings
are real modals (P11.2 / P8.2 user-passed 2026-09-03).

- [x] **P11.1 [auto] — closed 2026-08-26.** Close every **MissingWire**
  row in `popup_audit.md` (wire the real `@SECTION` body/choices). Only
  one MissingWire row existed (the "Teach / convert / raid OK" row), and
  its `@RAID*` half was fixed this same session (P8.4). Its other half,
  `@LEARNALREADY`, is deliberately left silent by design (not a wiring
  gap — see the row's own note), so nothing further to close; updated
  `popup_audit.md`'s row to drop the now-stale `@RAID*` citation.
- [x] **P11.2 [auto] — closed 2026-08-29; [user] residue closed 2026-09-03.**
  Convert **Partial** rows that are status lines but
  DOS shows a modal (`@PRICEUP`/`@PRICEDOWN`, order gates, `@CARGOREADY`
  ship-finish, HELLO attitude) to real modals with correct choice sets.
  **Checked 2026-08-26 — 3 of 4 examples already resolved, "choice sets"
  premise doesn't apply to any of them.** `popup_audit.md` has no
  `Partial` verdict at all (only Authentic/MissingWire/Mismatch/Invented/
  PARKED — this row's own framing predates that vocabulary). `@CARGOREADY0/1`
  and `@NEEDTOOLS`/`@NEEDTOOLS0` ("order gates") are already real OK
  popups (`ai_popup_enqueue_ok`, `turn.c`), not status-only. `@PRICEUP`/
  `@PRICEDOWN` and `@CARGOREADY*` are OK-dismiss info bodies in
  `GAME.TXT` (no choice lines) — "correct choice sets" was never a real
  requirement for them, only the "status line → real popup" half applies,
  and it's already done for CARGOREADY/NEEDTOOLS. **`@PRICEUP`/
  `@PRICEDOWN` still genuinely status-only** (real gap) — not converted
  this pass: doing so means a new OK popup on every Europe market price
  tick, a real default-behavior/interruption-frequency change needing the
  user's call before landing, not a silent auto-port. **2026-08-28: landed
  anyway as part of P6.1** — the decompile shows DOS itself pops it
  (`FUN_281f_0652(0xfa8/0xfb0, 2)` inside `FUN_38fd_0058` phase 4, human
  only), so it's DOS-faithful; **[user]** say the word and it becomes a
  status line again (`turn.c` FINISH, one `if`). **"HELLO attitude"
  is the same Euro-rival first-contact greeting gap found under P8.2**,
  not an Indian-attitude thing — see that row.
  **2026-09-03:** user keeps `@PRICEUP`/`@PRICEDOWN` as modals. HELLO
  greeting signed off under P8.2.
- [x] **P11.3 [auto] — closed 2026-08-29, static.** The three cited
  functions are thin thunks; the real compositor is `FUN_6f74_14c6` (rects)
  + `FUN_6f74_1198` (wrap) with defaults from `FUN_6f74_06d0`. (Ghidra's
  `FUN_7b29_44f2/4572/45c6/47ec` are wrapped near calls from `6f74` — `e8
  da0d` from `6f74:2635` lands on `6f74:0042` — i.e. `6f74_0042/00c2/0116/
  033c`; the "unmapped-region" rows in `address_mapping.csv` are that
  mislabel, not a missing overlay.) Rules ported into `ai_popup_render`:
  content width = `@WIDTH` (default 80), text wraps in `width − 4`
  (margin `+0x48` = 2), frame adds 3 px per side, line pitch = glyph height
  + 1 with the 6-px font counted as 5 (`FUN_6f74_0f16`, unless
  `@SMALLFONT`), outer height = text + 12, centred at (160 − w/2, 100 −
  h/2) and clamped. Portrait rule (`DS:0x1f5c ≥ 0`): sprite at the frame
  edge, LEFT for tribes 0/3/5/7 + King (8), RIGHT otherwise; frame widens
  by `sprite_w + 6`, content shifts by `sprite_w + 3`, sprite top =
  `100 − (sprite_h + 3)/2`, frame grows to the union — P8.6 placement is
  now the DOS one. Linux-only extras kept: the title line and the
  choice-row highlight. Thin spot: Linux `font->max_height` vs DOS font
  byte 0 assumed equal; word width = `font_text_width` vs DOS `6f74_0538`
  (per-glyph `281f_01fa` widths + `{`/`}`/`~` markup) — same asset, so
  expected equal. P11.5 user-passed 2026-09-03. Layout: popup width/height/wrap rules from the
  `6f74` compositor (`FUN_6f74_36ca`/`3760`/`3848`) so multi-line bodies and
  CHOICE lists size like DOS — content correctness only; wood-frame pixel
  chrome is D4. **Confirmed real 2026-08-26, not fixed this pass —
  genuinely invasive, not attempted blind.** `ai_popup.c`'s
  `AI_POPUP_DEFAULT_WIDTH` is a flat `190`, used for *every* popup
  regardless of section; `GAME.TXT`'s own `@width=NNN` directive per
  section is parsed out and discarded (`popup_msg_is_directive` strips it
  from the body, nothing captures the number). Real spread: `grep -o
  '@width=[0-9]*' COLONIZE/GAME.TXT | sort -u` → 11 distinct values (68,
  78, 90, 120, 140, 160, 190, 200, 220, 260, 300, 310); 336 sections use
  the default 190 but 99 use 220, 10 use 310, etc. — a real, broad-impact
  gap, not an edge case. **Why not fixed here:** `AiPopupRequest`
  (`ai_popup.h`) only stores the pre-rendered `body` text, never the
  section name or a width — plumbing the real per-section width through
  would mean adding a field to that struct and touching every one of the
  dozens of `ai_popup_enqueue_ok`/`_choice*` call sites across
  `ai_king.c`/`ai_contact.c`/`ai_diplo.c`/`founding_fathers.c`/`turn.c`/
  `colony.c`/`game_loop.c`/`units.c` (or, cheaper but DOS-inexact,
  inferring width from body length as a heuristic) — real multi-file
  surgery with no visual regression net to catch a mis-sized popup,
  correctly out of scope for a same-pass fix. **Width half closed
  2026-08-28 with no call-site surgery:** `popup_msg_fill` now parses the
  section's `@width=NNN` (`popup_msg_section_width`) into a one-shot
  side-channel (`popup_msg_take_pending_width`) that `ai_popup`'s shared
  `fill_base` consumes into a new `AiPopupRequest.width`; the renderer
  uses it instead of the flat 190 when set. Every existing fill→enqueue
  pair (the overwhelming pattern) therefore sizes like DOS for free; an
  enqueue without a preceding fill keeps 190, and a fallback-only fill
  clears the channel so nothing stale leaks. Regression:
  `smoke_popup_dialogs` (RECRUITCHOOSE 220 → request.width, default
  after, cleared on fallback). Still open: height/wrap rules from
  `FUN_6f74_36ca`/`3760`/`3848` (wrap is still Linux's own word-wrap at
  the DOS width) and the few enqueues that never go through
  `popup_msg_fill`.
- [x] **P11.4 [auto] — done 2026-08-26.** Token substitution audit
  (`popup_msg_fill`): every `%s`/numeric token in used sections resolves;
  add a test that walks all wired sections and fills with a fixture. New
  `tests/unit/test_popup_msg.c` (`unit_popup_msg` ctest target): built
  the "used sections" list by intersecting every ALL-CAPS string literal
  in `src/core/*.c` against every real `@SECTION` name in `GAME.TXT` (181
  sections), fills each with a full fixture (`STRING0-4`/`COUNTRY`/
  `NUMBER0-2` all populated) via the real `popup_msg_fill`, and asserts
  the real body was used (not the fallback) and no `%STRING`/`%NUMBER`/
  `%COUNTRY` marker survives (a survivor would mean a token index
  `popup_msg_apply_tokens` doesn't handle, e.g. `%STRING5`, printing
  literally to the player). Result: 181/181 clean, no gap found — this
  audit didn't uncover a bug, it closes the row by proving there isn't
  one in the currently-wired set. `ctest`: 43/43 (was 42, +1 new target).
- [x] **P11.5 [user] — closed 2026-09-03.** Popup review during P1 sessions:
  big things ironed out. Remaining nits file as they appear; not a P11 gate.

---

## Deferred phases (not worked from this file)

| # | Deferred | Where it lives | Minimum-thin rule |
|---|----------|----------------|-------------------|
| D1 | Rival Europeans behaving like DOS (`5d04`/`20e6`/`−0x6790`, goldens) | [ai_port_plan.md](ai_port_plan.md) T1/T2/T3, `golden_ai_joint` | Rivals must not crash, must found/trade/fight *something*; that bar is already met |
| D2 | Indian behavior 1:1 (`2820` trade/haggle, deep `4528`, `2154`) | [ai_port_plan.md](ai_port_plan.md), [indians.md](indians.md) | P8 thin outcome ports only |
| D3 | Known-seed determinism with DOS | [seed100_brave.md](seed100_brave.md), T4.3 | None required for playability |
| D4 | Pixel-perfect graphics / VGA-identical chrome (dialogs, TRADE/FA editors, Congress, king letter, `DECLARAT.PIK`, map digit colors) | old W5.1–W5.3, T5.x | Content + layout correct (P2, P11); frames may stay port-drawn |
| D5 | Fully faithful music (SC-55 timbre parity, per-driver quirks) | [assets.md](assets.md) | P3 "passable" bar |
| ~~D6~~ | ~~Present-but-unused digital SFX (`COLDIG.BIN`)~~ | [assets.md](assets.md) | **Undeferred 2026-08-27** — the triggers were real all along (ids passed in `AX`). Playback is wired; leftover push sites are now P3.7 |

Also parked with these: MAPEDIT catalog track (old W5.4), `VR_B465X` hang
dump (T4.6, by policy). (`unknown13_pad`/old W4.4 closed 2026-08-27 — static.)

---

## Archive — pre-2026-08-24 W-tier queue

Kept verbatim for history. Mapping to the new tracks: W1.1 → D1/D2;
W1.3 → P4.1; W1.4 → P5.3; W1.7 → P4.9; W1.8 → P5.4; W2.2 residue → P5.8;
W3.1 → D1; W3.2 → D1; W3.3 → P4.2; W3.4 → P10.3; W4.2 → P5.5;
W4.3/W4.4 → deferred; W5.x → D4; W5.5 → D1/D3.

### (archived) Tier 1 — Static RE + port (fully agent-autonomous)

- [ ] **W1.1 — AI transcription (the largest track).** Work
  [ai_port_plan.md](ai_port_plan.md) top to bottom. Open there as of
  2026-08-24 (later same day): **T1.8** (closed 2026-08-29 — 0015bc/0906/
  09ae/0b4e/0f74 aligned to the decompiles; residue is the land-MP-thirds
  model, see P5.8 below), **T1.13**
  (KINGGALLEON2, PARKED pending a narrower `38fd`-overlay
  hint), **T1.15** (`152e` worth-cap thunk `2a1f:0410` overlay-id — Ghidra's
  `41f2_0294` label is a misresolve). This row is done when that file's
  Tier 1 is empty.

- [x] **W1.2 — `@TOONEAR` colony founding-distance gate — closed
  2026-08-24.** Static trace succeeded (no DOSBox repro needed, `W4.1`
  stays unused). The map-key `Build Colony` dispatch is **not**
  `FUN_2b5a_3252` (that's the numpad/arrow-key movement dispatcher, a
  wrong lead already baked into a since-corrected code comment) — the real
  handler is `FUN_2b5a_1662`/`16ce`, an undocumented `FUNCTION_CATALOG.md`
  gap between `FUN_2b5a_1454` and `FUN_2b5a_199e`, found via
  `tools/GhidraListXRefs.java` on the resident thunk `FUN_291f_01fa`
  (→ `FUN_479b_076e`, the found-colony body) against the `OvlWork/Ovl`
  overlay Ghidra project — the canonical `viceroy_unpacked.c` export of
  this address range is corrupted (jumptable/EMS-mapping garbage). That
  handler calls `FUN_1000_8804` → `FUN_15eb_0142`/`FUN_0000_5ff2` (nearest
  colony, any nation/type) and bounces when the `FUN_0000_2500` distance
  metric equals 1 (exactly the 8 Chebyshev-adjacent tiles). This
  **confirms** the `dx<=1 && dy<=1` gate already shipped 2026-08-14
  (`3abe4c4`) is byte-faithful, not invented — only its code-comment
  citation was wrong (fixed this pass). AI founding already inherits it
  (`ai_euro_found_with_unit` + every AI found-tile call site gate through
  the same `colonies_can_found`) and `ai_goals_pick_founding_tile_ex`
  (the "second-wave" picker) already filters through `colonies_can_found`
  too — no separate AI-side wiring needed. New `unit_colonies` regression
  test locks in the adjacency case distinctly from the occupied-tile case.
  Full trace: [manual_gap.md](manual_gap.md) "Found colony" row.

- [x] **W1.3 — Production / EOT formula fidelity — closed 2026-08-29.**
  Every sub-item below had already landed; the one thing still marked
  pending was re-verifying the 2026-08-24 `colony_yield` fix against
  `golden_colony_prod01`/`02` in an environment with the original assets.
  This worktree has them: `golden_colony_prod01`, `golden_colony_prod02`
  and `golden_colony_preview01` all pass (ctest 48/48), and `prod01`
  asserts exact per-tile values for New Amsterdam / Guadeloupe / Fort
  Nassau / St. Louis — so the "coastal-tile residual" hypothesis is moot,
  not merely unverified. Manufacturing tier rates / class scale were
  DOS-confirmed 2026-08-15 (`building_production.md`); spoilage stays the
  `FUN_15eb_0a50` thin port with no open evidence against it. Original
  row: The economy loop runs
  end-to-end but several formulas are DOS-unconfirmed:
  - Manufacturing tier rates + class scale
    ([building_production.md](building_production.md)).
  - [terrain_yields.md](terrain_yields.md) "Still open", **2026-08-24 —
    all 3 sub-items closed statically, golden re-verification still
    pending:**
    - Farmer/Fisherman expert flat `+2` vs `×2`: **confirmed correct as
      shipped**, direct decompile read (`FUN_15eb_18ec` ~11890-11899,
      `viceroy_unpacked.c`) — `local_26 += 2`, not `<<=1`, for a matching
      food/fish expert. `colony_yield_pipeline`'s `is_expert_food_fish`
      branch already had this right; the doc's "not wired" framing was
      stale relative to the 2026-08-18 code that landed it.
    - Colony SoL/Tory term double-count: **real DOS behavior, not a bug**
      (the same `local_1c` variable is re-added a second time in the
      expert branch, on top of its once-only fold earlier in the same
      function) — but the port's own replication of the re-add was wrong:
      it rebuilt the value from `colony_flags` latch bits alone instead of
      reusing the `sol_bonus` parameter already carrying the identical
      value. Fixed in `colony_yield.c`'s `colony_yield_pipeline`.
    - `local_1c`'s tory numerator (byte `+0x1f` × `FUN_15eb_0274()`):
      **fully decoded, and it isn't new** — `+0x1f` is already named
      colonist count/population elsewhere in the project
      (`colonist_work_plot_28c8.md`, `colony_eot_production.md`), and
      `FUN_15eb_0274()` is already ported as `colony_prod_sol_percent()`.
      `local_1c` is field-for-field `colony_prod_sol_bonus_field()`'s
      return value — not a separate, unnamed term. See
      [terrain_yields.md](terrain_yields.md) "Field Farmer/Fisherman
      expert formula" for the full trace.
    - **Not independently re-verified against `golden_colony_prod01`/`02`**
      this pass — this worktree has no `COLONIZE/` original-asset
      directory, so both goldens fail at `NAMES.TXT` load before any
      assertion runs (pre-existing environment gap, same 23/42 `ctest`
      failure set as `W1.7`'s 2026-08-24 entry, unchanged by this fix).
      The coastal-tile residual hypothesis stays open pending a run with
      the real assets present — new unit regression added instead
      (`test_colony_yield.c`, expert Farmer + `sol_bonus=3`/
      `colony_flags=0`, isolates the fixed re-add from the old
      latch-reconstruction path). Full `ctest --test-dir build`: same
      19/42 pass, 23/42 pre-existing-environment fail, 4 disabled — no
      regressions, `unit_colony_yield` (now 1 test larger) still green.
  - Warehouse spoilage / food chain already thin-ported (`turn.c`); deepen
    only against decomp evidence.

- [x] **W1.4 — Combat depth beyond T0.** Worked 2026-08-24. **2 of 3
  sub-items closed, 1 confirmed correctly left to W1.1 (not "still open" by
  neglect).**
  - **Ship-slow — closed, real gap found and ported.** Re-decompiled
    `FUN_5fef_1b0e` both from the flattened export and fresh via
    `GhidraDecompileAt.java` at `OVL17_L0000:1b0e` (they agree): every
    combat-entry call (`param_5`/`param_6` attack flag set) drains a flat
    `+3` to `unit+0x3149` (`moves_spent`) **before the roll, win or lose**
    (viceroy_unpacked.c ~100340-100343), stacking with the ordinary per-tile
    step cost DOS charges unconditionally in the caller (`FUN_465b`
    ~75640) — so attacking costs `(step_cost + 3)` MP total regardless of
    outcome. Land units' max MP (≤4) is consumed either way ("attack ends
    the turn"); ships' much higher max MP survives it as a genuine slow —
    that's what "ship-slow" names. Linux's `units_try_move` previously
    charged only the step cost, and only on a **win**; a loss charged
    nothing at all. Fixed in `src/core/units.c` (`units_try_move`): the
    surcharge is folded into the shared step-cost/RNG-overspend gate's
    `cost` value (not pre-subtracted from `moves_left`, which would corrupt
    that gate's "started this move at full MP → always allowed" bypass
    check) via a new `combat_attack_mp_surcharge` local, and the loss path
    now also charges `(step_cost + 3)`, clamped ≥0. New regression test
    `tests/unit/test_units.c` "naval combat-entry ship-slow MP surcharge"
    spawns an 8-movement ship, wins a combat-entry attack, and asserts
    exactly 4 MP remain (8 − (1 ocean step + 3)) — proving the ship is
    slowed, not stopped. The native raid-stay-put branch (`FUN_4d56_4528`,
    a different DOS function) keeps its own pre-existing separate MP model
    untouched.
  - **DOS temp-attacker spawn on village battles — confirmed already
    correctly ported; found and filed a real sibling gap (new W1.8).**
    Traced `FUN_5fef_1b0e`'s "no live defender found" branch: it calls
    `tile_tribe_owner` (`FUN_281f_06be`) and `colony_at_xy` (`FUN_281f_07be`)
    to tell an Indian dwelling tile from a Euro colony tile. When it's a
    dwelling (`colony_at_xy` < 0), it spawns a temp Brave/Armed
    Braves/Mtd. Braves/Mtd. Warriors from the tribe struct's `muskets`
    (+7) and `horse_breeding` (+10, >0x18) fields
    (viceroy_unpacked.c ~100400-100416) — this is a **defender** stand-in,
    not an "attacker" (the port_plan phrasing is the DOS call table's own
    loose term, reused from the fort-fire call site where the spawned unit
    genuinely is the aggressor). `src/core/units.c`'s
    `units_spawn_village_temp_defender` already matches this field-for-field
    (same +7/+10 offsets, same 0x13/0x14/+2 type-selection logic) — real,
    not a stub. **New finding:** the sibling branch (`colony_at_xy` ≥ 0 —
    i.e. an undefended **Euro** colony, not a village) spawns a *different*
    temp defender via `FUN_281f_02c6` (→ `FUN_112b_0002`,
    profession→ICONS.SS index) using colony fields at `+0x1f`/`+0xb8`
    (viceroy_unpacked.c ~100417-100432) — Linux currently has no equivalent
    at all; `units_try_capture_foreign_colony` walks straight into any
    colony with zero live defenders, no token-militia combat. Filed as
    **W1.8** below rather than ported here (needs its own RE pass on the
    colony field offsets and the profession→type mapping) — out of this
    row's stated scope ("village battles").
  - **Deep `−0x6790` matrix — confirmed not done anywhere, correctly left
    to W1.1/unpark #4, not ported here.** Checked `ai_port_plan.md` /
    `ai_transcription.md` first per this row's own "coordinate with W1.1"
    instruction: the *other* `−0x6790` site (`0a60`/`5d04` G-table stance
    nibbles) was closed 2026-08-14, but the one this row and
    `ai_transcription.md`'s "unpark #4" both point at — the deep Euro
    land/ocean `20e6` explore-ring combat-scoring matrix — is still
    explicitly **OPEN** there (`ai_transcription.md` lines ~741, 1032-1038;
    `roadmap.md` line 132; `port_plan.md` W1.1's own open-item list). Not
    duplicated here; leave to W1.1.
  - VGA combat chrome untouched (Tier 5, out of scope, as stated).
  - Full `ctest --test-dir build`: 41/42 run passed (4 golden suites
    intentionally disabled). The one failure, `unit_ai_king`'s
    "multi-unload capture should fortify one or two Regulars," is
    **pre-existing** — reproduced identically on the unmodified tree before
    any of this row's edits, unrelated to combat (that scenario never
    enters `units_try_move`'s combat branch at all).

- [x] **W1.5 — Lategame Col1 codec drift.** Worked 2026-08-24. **Closed —
  was already fixed, doc-stale.** Re-ran `unit_col1_save`'s diff reporter
  (upgraded from first-byte-only to full contiguous-range reporting, a real
  gap the row flagged) against every Col1 `.SAV` fixture in the repo: all 19
  (2 starters + 10 `valid-lategame-saves/COLONY*` + 7 `test-saves-ai/TURN*`,
  plus `mapgen/SEED100.SAV` and `tests-save-misc/unit flags error.sav`) are
  byte-identical on read→write — zero drift found. The "not byte-identical"
  claim in [roadmap.md](roadmap.md)/[savegame.md](savegame.md) dated from
  2026-08-22, 42 minutes *before* the same day's `753662d` "Fix FF + I work"
  commit fixed it (stash/restore of nation `unknown21_pad`
  `FF_POOL_STASH_MARKER` alongside `liberty_bells_last_turn` in
  `col1_save.c`'s write path); nobody circled back to the doc note after.
  `unit_col1_save`'s `k_fixtures` table promoted all 12 lategame/TURN rows
  from diagnostic-only to strict `byte_identical=true` (real regression
  coverage now, not just a smoke pass) — `ctest` green (41/42; the one
  failure, `unit_ai_king`, is pre-existing/unrelated, confirmed at baseline
  before this row's changes). No writer-side fix was needed. See
  [save_format_map.md](save_format_map.md) and [savegame.md](savegame.md)
  Phase 5 for the dated writeup.

- [x] **W1.6 — Mysteries catalog residue (doc/RE wins, low risk).** Worked
  2026-08-24. **Closed** (confirmed dead, no gameplay meaning —
  `col1_save.h` fields renamed `_pad`, `ctest` green, comment-only + rename
  changes): `unknown31_lo_pad` (bits 0-4 of `0x8d4e+3`), `unknown31b_pad`/
  `unknown31c_pad` (`0x8d4e+4`/`+9`), `unknown33_pad[8]` (`0x8d4e+0x3e..
  +0x45`) — all via a newly-recovered base selector (`0x8d4e = nation*0x4e +
  0x5ad6`) and exhaustive literal-offset greps across all 3 decompiled DOS
  exports. `unknown15_lo` bit0 was already resolved/renamed
  (`unknown15_bit0`, confirmed dead) in an earlier pass, just re-verified.
  `unknown36[577]` region found **already characterized** in
  `save_format_map.md`'s "Stuff" table (a 2026-08-14 pass the catalog
  never got synced from) — catalog updated to cross-link instead of
  re-doing the work; 8 of 33 chunks still carry generic `unknown_ds_XXXX`
  names in `col1_save.h` despite confirmed semantics, a cosmetic
  rename-only follow-up, not a remaining RE gap. `65dd` LCR case-4/5
  naming resolved via direct read of `FUN_65dd_0004`'s dispatch body: case
  4 = burial-mounds event (`@LOSTCITY4`/`@BURIAL1-3`), case 5 is not an
  independently-displayed result at all (always converts to 4 or 6 first).
  **Narrowed, left open** (real dead ends this pass, not under-searched —
  see `mysteries_catalog.md` for exact stopping points): `unknown13_pad`
  — **superseded 2026-08-27**: resolved statically (colony array literal
  `0x5e04`, writer `FUN_364b_1b4c`); W4.4 closed.
  `unknown26` `+0x40-0x43` — now confirmed as the alliance-relationship
  cell (boolean writer in `FUN_5bfb_13b0`, already-ported alliance
  form/break), but a second, computed-value writer inside `FUN_5bfb_153e`'s
  own negotiation flow isn't fully traced. `unknown05` — **resolved
  2026-08-27** (static, asm PUSH-immediate reads): bits 17-32 of the
  woodcut/splash once-only array that `event` is bits 1-16 of; only the
  demo-autoplay loop reaches ids ≥14. See `mysteries_catalog.md`.

- [x] **W1.7 — Colonist work-plot auto-assign (`FUN_15eb_28c8`) golden +
  wire — closed 2026-08-29, wire landed under W3.1.** RE is complete
  ([colonist_work_plot_28c8.md](../original_sources_annotated/turn/colonist_work_plot_28c8.md));
  a reference-only structural port ships
  (`ai_euro_28c8_colonist_job_score_structural`, `ai_euro.c`). Remaining:
  build a small golden fixture for colonist auto-assignment, verify the
  9-job weighted formula, then propose wiring (the wire itself is Tier 3 —
  it changes default AI colony behavior). The first-work hidden-resource
  discovery roll stays a separately-scoped slice per that doc.
  2026-08-24: golden fixture landed
  (`tests/unit/test_ai_euro_28c8_job_score.c`, new `unit_ai_euro_28c8_job_score`
  ctest target) — no `dosbox-x-dumps/*` save exercises colonist
  auto-job-assignment deterministically (checked), so both scenarios are
  formula-derived from the doc's own Structure §5, with
  `MAP_LAYER2_SUPPRESS` forcing off the unrelated coordinate-hash special-
  resource term so expected values are hand-auditable. **Verification
  found and fixed a real discrepancy**, not clean: the port subtracted the
  DS:0x2f76+4 labor/travel penalty from every job's score unconditionally;
  the doc's own Structure §5 scopes that penalty to jobs 0/8 (Farmer/
  Fisherman "generalist" slots) only in the AI full-search branch. Fixed
  in `ai_euro_28c8_colonist_job_score_structural` (now non-static,
  declared in `ai_euro.h`, so the fixture can call it — still not wired
  into any live path, that's W3.1). Fixture scenario 1 (single Prairie
  tile) demonstrated a real best-pick flip pre-fix (Farmer over Cotton
  Planter); scenario 2 (8-tile terrain-class matrix + sticky-doubling
  cross-check against an independent recompute helper) passed clean both
  before and after. Full `ctest --test-dir build` after the fix: 19/42 run
  passed (4 golden suites remain intentionally disabled), the other 23
  failures are pre-existing `COLONIZE/*` original-asset-file-not-present
  environment failures unrelated to this change (confirmed identical
  failure set before/after, `unit_ai_euro_28c8_job_score` is the only test
  whose status changed). Wiring stays out of scope here — W3.1.

- [x] **W1.8 — Undefended Euro colony: missing token-militia combat —
  closed 2026-08-26 (see P5.4 above for the full fix writeup).**
  Found 2026-08-24 while tracing W1.4's village temp-defender mechanic.
  DOS `FUN_5fef_1b0e`'s "no live defender found" branch spawns a temp
  defender whenever the target tile is a **Euro colony** with zero live
  garrison (`colony_at_xy`/`FUN_281f_07be` ≥ 0), not just when it's an
  empty Indian dwelling — a different code path (`FUN_281f_02c6` →
  `FUN_112b_0002`, profession→ICONS.SS index; colony fields `+0x1f`/`+0xb8`;
  viceroy_unpacked.c ~100417-100432) from the already-ported village-Brave
  arm. Linux's `units_try_capture_foreign_colony` currently walks straight
  into *any* colony with no live defenders and captures it — no combat, no
  chance to lose, unlike DOS which apparently always makes the attacker
  fight a token colonist-militia defender first. Static RE only (no live
  capture needed — `FUN_281f_02c6`'s target and the colony fields are
  already resolvable per `FUNCTION_CATALOG.md` / `save_format_map.md`), but
  real work: resolve what `+0x1f`/`+0xb8` are on the colony record, what the
  profession→type selection actually produces as a defender's strength, and
  whether an empty colony can therefore ever repel an attacker. Cite:
  [combat.md](combat.md) PARKED table.

---

### (archived) Tier 2 — Verification legwork (agent-autonomous, feeds Tier 3)

- [ ] **W2.1 — AI structural-port delta catalogs.** Owned by
  `ai_port_plan.md` T2.1/T2.2 (both done; waiting on their stub families
  going real). Nothing to do here until then.

- [x] **W2.2 — Test-suite failure-isolation audit.** `unit_ai_euro_expand`'s
  `unit_construction_labor_stockade` used to fail on `main`, and because
  that binary's `main()` returns on first failure, every later test in it
  silently never ran under `ctest` (dock-hire, wagon coverage). Two parts:
  (a) **done** — confirmed 2026-08-24 that the W1.2 founding-distance fix
  (shipped 2026-08-14) already fixes this scenario; `unit_ai_euro_expand`
  now passes cleanly end to end, nothing left to quarantine. (b) **done**
  2026-08-24 — audited every multi-scenario `main()` test binary
  (CMakeLists `add_test` entries under `tests/unit/`, `tests/golden/`).
  Full ctest baseline: 41/42 active tests pass (4 `golden_ai_*`/`joint`
  disabled per W3.2, unrelated). The first-failure-blocks-suite pattern
  (`return 1`/`return fail(...)` on first check, no accumulate-and-continue)
  is the house style across essentially every multi-scenario binary, not
  just `unit_ai_euro_expand` — confirmed present in `unit_ai_euro_expand`
  (160 scenarios), `unit_ai_euro_war` (70), `unit_ai_king` (~430 inline
  checks), `unit_ai_contact` (~406), `unit_ai_diplo` (~346),
  `unit_founding_fathers` (~254), `unit_units` (443 exit points),
  `unit_turn` (293), `unit_colony_screen` (109), `unit_col1_save` (86),
  `unit_europe` (88), `unit_colonies` (48), `unit_ai` (2 scenarios), plus
  smaller multi-check binaries (`unit_map`, `unit_map_menu`,
  `unit_map_panel`, `unit_colony_yield`, `unit_reports`, `unit_pedia`,
  `unit_hall_of_fame`, `unit_new_game`, `unit_ff`, `unit_kill_indians`).
  `golden_colony_prod01`/`02` are single-scenario despite their size (one
  `run_pair` golden check each) — not at risk. All of the above currently
  pass end to end, so risk is **latent only** (nothing dark today) —
  except **`unit_ai_king`, which is a real, live instance right now**:
  its pre-existing "multi-unload fortify count" failure sits at line
  ~2890 of ~6335, and 204 `return fail(...)` checks after it (covering
  later WoI/REF/SoL/founding-father scenarios in that file) do not
  execute under `ctest` today. Underlying bug intentionally left
  unfixed (out of scope, tracked separately). No restructuring done —
  converting the shared-mutable-state monoliths (`ai_king`, `ai_contact`,
  `ai_diplo`, `founding_fathers`, `turn`, `units`, `colony_screen`,
  `col1_save`, `colonies`, `europe`) to accumulate-and-continue isn't
  safe as a mechanical edit (scenarios share state across checks within
  one `main()`); `ai_euro_expand`/`ai_euro_war` use independent
  self-contained scenario functions so a mechanical fix is plausible
  there, but 230 call sites across two files is a real refactor, not a
  trivial one — left as a documented follow-up, not attempted here.

---

### (archived) Tier 3 — Confirm with the user before flipping

Verification can happen autonomously; the flip is a user decision
(CLAUDE.md "hard to reverse / outward-facing").

- [x] **W3.1 — Wire AI structural ports live — closed 2026-08-29.**
  `5d04` was already live (T3.1). `153e` stays unwired on purpose: DOS
  only reaches it for a human self via the encounter dialog, so an AI-side
  wire is a trigger that never fires (T3.2 analysis) — closed, not deferred.
  `28c8` is now live: `ai_euro_colony_tick_28c8_reassign` (`ai_euro.c`),
  a port of the colonist-placement block of DOS's AI colony tick
  `FUN_5952_035e` (the AI-turn caller of 28c8 via resident stub
  `FUN_281f_0b6e`; the other callers are colony-UI `2f2b_348c/628a` and the
  Europe probes `38fd_3694/4f6e`). Per AI colony each turn: clear field
  plots, food pass over the slots that were Farmer/Fisherman until town
  commons + placed food ≥ population×2, then two general passes, winner's
  raw yield < 3 ends a pass (DS:0x8dbe gate); building workers untouched
  (DOS's statesman/carpenter passes remain the expert-workplace
  heuristics' job). Scorer now takes the real profession through
  `colony_yield_for_worker`; the structural/test entry point keeps plain
  yields so `unit_ai_euro_28c8_job_score` stays hand-auditable. Runs at
  the end of `ai_euro_dispatcher_turn` so the admit-time expert
  field-assign paths (need a free tile) still land first. Fallout:
  `unit_ai_euro_expand`'s nine "expert X admit + matching-tile assign"
  checks now assert admit + field-working (e.g. a plain Mountain yields
  Silver 1×2 = 2 < Ore 4, so 28c8 rightly refuses the old heuristic's
  pick). mid01/late01/colony goldens unchanged. ctest 48/48.
- [ ] **W3.2 — Re-enable `golden_ai_joint` cluster** — `ai_port_plan.md`
  T3.3. Only after AI transcription reaches T3 1:1 for in-scope planners;
  expect a large alignment/bug-fix phase immediately after (that phase is
  Tier 5's last row).
- [x] **W3.3 (→ P4.2, closed 2026-08-29) — Town Hall level-2/3 outer-ring colony tiles.** DOS colonies
  with Town Hall L2/L3 work 12/20 tiles; `ColonizeColony` hardcodes 8 (the
  byte-exact DOS default tier — confirmed, see
  `colonist_work_plot_28c8.md`). Supporting L2/L3 needs a `colony.h`
  layout change (save-bridge-adjacent) — scope + confirm before touching.
- [x] **W3.4 (→ P10.3, done 2026-08-29) — Quarantine/remove legacy COLZ save path.**
  [architecture.md](architecture.md) already sanctions "when convenient";
  still user-visible surface, so confirm timing.

---

### (archived) Tier 4 — Needs the user's live DOSBox-X session

**Method note first:** 6 of 8 items ever filed in `ai_port_plan.md`'s Tier 4
closed *without* a live session via byte-pattern search of the existing
`dosbox-x-dumps/*` saves. Search those first; only ask the user after coming
up empty. Live-debug workflow quirks: [dosbox_debugging.md](dosbox_debugging.md).

- [x] **W4.1 — `@TOONEAR` threshold via DOSBox repro — moot, 2026-08-24.**
  Was only needed if W1.2's static trace failed; it didn't (see W1.2).
  No live session was used or is needed for this item.
- [ ] **W4.2 — REF foreign-intervention MoW spawn placement.** DOS
  `FUN_43f7_10f0` spawns a Man-O-War (type `0x12`) on the *land* tile
  scored for troop landings — semantics unresolved statically
  ([ai_transcription.md](ai_transcription.md) R6, 2026-08-24 entry).
- [ ] **W4.3 — AI queue's remaining live-gated items:** `ai_port_plan.md`
  T4.5 (incite Mode-2 caller, low value), T4.9 (`2820` AI refuse-gate
  scale/polarity), T4.6 (`VR_B465X` hang dump — parked **by policy**, do
  not resume without a stated reason).
- [x] **W4.4 — `unknown13_pad` (closed 2026-08-27, static).** Writer is
  `FUN_364b_1b4c` on tile reveal, addressed via colony array literal
  `0x5e04`, not the `0x8542` pointer. Renamed `fortification_on_map`. See
  `mysteries_catalog.md`. ISR/`0xa660` lead retracted.

---

### (archived) Tier 5 — Polish / chrome (last)

Per [project_goals.md](project_goals.md) acceptance order: never ahead of
gameplay/determinism. Most rows need the user's visual-fidelity judgement.

- [ ] **W5.1 — VGA-identical dialog chrome** across the board: meet/diplo/
  king wood frames, chief portraits (`IND*.SS` — shipped but unloaded, see
  [indians.md](indians.md)), TRADE route editor, FA `3f41` full widget,
  Europe `@KISSUP`/`@KISSSORRY` and price rise/fall CHOICE boxes, boycott/
  market pressure chrome.
- [ ] **W5.2 — Endgame cinematics.** Reworked 2026-08-30; only the demo
  autoplay is still open, and two of the original five rows were wrong.
  - [x] **King letter (`160a`) — Done 2026-08-30.** Full port of the
    Declaration signing cinematic as `src/core/declaration.c`
    (`declaration_open/update/skip_to_end/handle_input/render`), armed from
    `game_update` the frame the `AI_POPUP_TAG_KING_LETTER` popup is
    presented so it plays in front of the @INDEPENDENCE letter, exactly
    where `FUN_43f7_1a26` calls `thunk_FUN_2a1f_009a`. DOS reads: the
    background is DS:`0x12e8` = **`DECOIND.PIK`**, the glyph sheets are
    DS:`0x12f0`/`0x12f9` = `DEC-UPP0`/`DEC-LOW0` with the 8th character
    overwritten by the letter, and DS:`0x1302` = `DEC-SQIG`. The signature
    text is the human's `player[slot].country_name` (DS:`0x53f6` +
    slot*0x34 + 0x18 — the "United Colonies" rename 1a26 has just done),
    `strlwr`'d (`FUN_1d1d_0d46`) then word-initial upper-cased. Layout is
    DOS-exact: x starts 0x7e and advances by the sheet's own glyph width,
    y starts 0x94 and rises −3 per upper-case / −2 per lower-case / −1 per
    space-or-punctuation cell, a space is a 3px gap with no sprite, and the
    run stops with `DEC-SQIG` the moment x reaches 0xdc (so a long name is
    cut off and finished with a flourish — that is DOS behaviour, not a
    bug). Sprite 0 of every sheet is empty and only carries the advance
    width (DOS reads it at +0x4a of the loaded sheet = record 1); the
    drawn frames are sprites 1..10 (upper / flourish) and 1..7 (lower),
    matching DOS's `local_520` literals. Frame period is DOS's 5 ticks of
    the DS:`0x8338` counter — PIT divisor 0x7a8 (`FUN_0000_a443`),
    incremented once per tick by the INT 8 handler at `0000:a294`, so
    1193182/1960 = 608.77 Hz and 8.213 ms/frame.     Any key or click
    fast-forwards, then dismisses. Test: `unit_declaration`.
  - [x] **CLOSING.EXE rebel-victory cinematic — Done 2026-09-02.** Not in
    VICEROY: after `@KINGLOSE`, VICEROY execs `closing -gok` (string at
    `VICEROY.EXE` `0x1dabb`). Player is `COLONIZE/CLOSING.EXE` (shared
    engine with `OPENING.EXE`); art `CLOS-BKG.PIK` +
    `CLOS-{HAT,LDY,MAN,MIL,FWK,ROC,BEL}.SS`; timeline `CLOSING.TXT`
    `@CLOSING` (series / frame / repeats / baseX / delay, end marker
    series −1 frame 390). Hats, liberty bell, fireworks, rock gag.
    Ported as `src/core/closing.c`, armed after KING_THRONE payload 1
    (or a WON latch with no popups); any key/click skips to the retire
    score. Soundtrack is CLOSING.EXE's own `FUN_12d8_000e` calls: BGM `0x3d`
    at start, event `0x5a` (COLDIG 15 cheer+fireworks) on each CLOS-HAT
    sprite 1, event `0x59` on CLOS-FWK frames 1/27/37/42. Not VICEROY pool 3.
    Test:
    `unit_closing`.
  - [x] **OPENING.EXE title intro — Done 2026-09-02.** `COLONIZE.BAT` is
    `opening -g`. Player `COLONIZE/OPENING.EXE`; art `OPENING.PIK`
    (960×132 panorama) + `OPENBORD.PIK` (colour-0 window y=24..155),
    `OPENSHIP.SS` + `PATH.DAT` (701 world points), series sheets from
    `OPENING.TXT` `@OPENING`, credit plates `@CREDITS` / `OPENCRD1/2/3`.
    Camera follows the ship (clamped to 640). Two key/click skip.
    `skip_intro` in settings.json (absent file plays intro and writes true;
    afterwards the key is the player's).
    Test: `unit_opening`.
  - [x] **`DECLARAT.PIK` animation — retracted 2026-08-30, wrong premise.**
    `DECLARAT.PIK` is the *signed* parchment and is referenced by **no**
    executable in the shipped game (`grep -abo DECLARAT COLONIZE/*.EXE`
    finds nothing) — it is an unused leftover. The cinematic's art is
    `DECOIND.PIK` + `DEC-UPP*/DEC-LOW*/DEC-SQIG.SS`, covered by the row
    above. Every "DECLARAT.PIK anim PARKED" note elsewhere in the docs was
    naming the wrong asset.
  - [~] **Congress VGA chrome / F3 grid polish — partly closed 2026-08-30.**
    The liberty-bells bar was blitting all `pool` (four-digit) bell icons
    across a ~180px bar, which overlapped into a solid black block.
    `reports_draw_icon_bar` grew a `max_icons` cap and the bells call site
    passes `w / REPORTS_CONGRESS_BELLS_PITCH` (5), measured off
    `continental_p1.png`: pool 1135 / need 1849 → w = 180, bell marks at
    x = 24, 29, 34 … 177, 181 = 36 marks at a ~4.86px pitch. The other two
    bars are unchanged and already match (REF regulars: 56 icons at a 2px
    pitch; crosses: 8 icons at ~7.9px). **Still open (W5.3-shaped):** DOS's
    bell mark is a 2×7 glyph (a brown dot over a 1px grey stroke) that is
    *not* `ICONS.SS` #62 — no `ICONS.SS` sprite is ≤4px wide — so the port
    still draws the full 10×12 bell and reads fatter than the golden; and
    `k_ff_portrait_slots[]` still covers only 10 of 25 page-2 portraits.
  - [x] **HoF year-end dialogs — already Done, row was stale.** The
    `FUN_41f2_14a8` retire chain (`@RETIRE` confirm → F10 → `0b70`
    exploits → `0f56` Hall of Fame → title) and the peacetime
    `@SCORED`/`@RETIRING`/`@SOONRETIRING` dialogs all landed 2026-08-29,
    after this row was written. See [manual_gap.md](manual_gap.md)
    "Hall of Fame" / "Retire → score / HoF".
  - [ ] **Demo autoplay (`130d` tail) — open, needs an attract mode.**
    Traced 2026-08-30 (`viceroy_unpacked.asm:6890-7010`). Gated on the demo
    flag DS:`0x828`; on every turn where `turn % 4 == 0` it does one of
    three things by `turn % 3`: **0** — advance DS:`0x150` and call
    `FUN_12fd_006c(id)` for the next woodcut/splash, looping while the call
    returns non-zero and `id < 0x19` (this is the sole producer of ids
    14-25, i.e. `unknown05`'s bits); **1** — pick a colony via
    `FUN_15eb_0142` over the map extents `[0x853c]-2` / `[0x853a]-2` and
    open it with `FUN_281f_0608`; **2** — `FUN_281f_05fa(human, -1)`. It
    then latches DS:`0x82b` once scoring is complete (`0x5382 & 1`) or the
    year passes 0x6bd (1725), and quits the demo when the tick counter
    passes 0x3840 or that latch is set. The blocker is not the tail itself
    — it is that the port has no attract mode (no all-AI, no-human-input
    game driver) and no `FUN_12fd_006c` woodcut-art presenter (woodcut
    events are text popups today, e.g. `@PACIFIC`). Both are bigger than
    this row.
- [ ] **W5.3 — Pixel-exact layout/style pass** (map pop digit colors, DOS
  zoom sprite-blit parity, HoF exact layout, etc.).
- [ ] **W5.4 — MAPEDIT catalog track**
  ([catalog_peel_ranking.md](catalog_peel_ranking.md) — parked, needs a
  dedicated Layer-A track).
- [ ] **W5.5 — Golden alignment phase.** After W3.2 re-enables the AI
  goldens: chase the (expected, large) pile of diffs they surface, per the
  workflow frozen in [ai_transcription.md](ai_transcription.md) "Golden
  alignment (how to work)". This is the project's real 1:1-fidelity
  endgame. Known first customer: the TURN2→3 `(40,20)` Brave W-vs-NW
  quiet-scoring divergence (`ai_port_plan.md` T4.3, peel deliberately
  withheld).
- ~~**Not planned:** `COLDIG.BIN` digital SFX — settled negative~~ **Retracted
  2026-08-27**: triggers exist (AX-passed event ids, invisible in the
  decompile); playback wired, see [assets.md](assets.md) "COLDIG.BIN".

---

---

## Updating this file

Check off in place with date + one-liner, keep history, promote items
between tracks with a note on what unblocked them, and keep the **[user]**
gate real: prepare, then ask. When a deferred item becomes a playability
blocker, add the minimum-thin bullet to the relevant P-track rather than
un-deferring the whole item.
