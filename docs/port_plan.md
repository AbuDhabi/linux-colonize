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

**Now:** map/colony/Europe screens, menus, panel, keyboard model all
structurally in; many details are "Done thin" against screenshots rather
than against the user's play experience.

**How to work:** this track is **[user]**-driven by design. The user plays,
reports what is wrong or missing, and the agent fixes. Agents should not
self-select UI polish here; they should keep a running checklist in this
section from the user's feedback.

- [ ] **P1.1 [user]** Establish a UI feedback checklist (screen → issue →
  fix). Seed it from the user's first play session. Append below as items
  arrive.
- [ ] **P1.2 [auto]** Before each user session: build, run a
  new-game→colony→Europe→save smoke by hand (`run` skill), fix anything
  that crashes or obviously misrenders, so the user's time goes to
  judgement calls not crashes.
- [ ] **P1.3 [user]** Colony screen: confirm drag/assign, building
  click, warehouse↔ship, production preview numbers, and the "People"
  strip all read as DOS does at a glance.
- [ ] **P1.4 [user]** Europe screen: confirm dock/harbor/market interactions,
  recruit/train/purchase menus, tax/boycott chrome.
- [ ] **P1.5 [user]** Map/panel: unit chrome, orders letters, minimap,
  status line, F-key model, end-turn flow, stack picker.

### P2 — Report screens (F1–F10 + Hall of Fame)

**Now (corrected 2026-08-26 by P2.1's RE pass — see [docs/reports.md](reports.md)):**
this paragraph was stale.
[`reports.c`](../src/core/reports.c) already renders every F2–F10 report to
golden-screenshot-derived DOS pixel layout (columns, icons, sprite chrome,
paging) — a matching golden PNG exists for all nine in
`original_saves/report-screen-goldens/`. Only Labor (F4) has a click-to-zoom
in DOS at all, and it's wired (grid cell → that profession's detail page);
no report jumps to the colony/map screen on click in DOS itself. The real
remaining gap: report titles/column headers/body strings are hardcoded
English typed from the goldens, not resolved live from `LABELS.TXT` at
runtime (P2.2's "heading from LABELS.TXT" is therefore still open).

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
- [ ] **P2.2 [auto]** Shared report scaffolding: heading from
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
  every report-display string table in `reports.c` except the screen
  titles (confirmed not live-loadable, see note above).
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
- [x] **P2.10 [auto]** F9 Indian Adviser: tribes, attitude, missions.
  **Done** — golden `indian.png`. Headband-portrait variant selection
  (`ICONS.SS` #113-117) still unidentified, always renders #113 — cosmetic,
  not blocking.
- [x] **P2.11 [auto]** F10 Score: DOS score table layout; retire path.
  **Done** — golden `score.png`. Hall of Fame (shares F10's chrome) stays
  **Done thin**: no DOS golden exists for it at all, so its column
  widths/chrome are unconfirmed (see [reports.md](reports.md)).
- [ ] **P2.12 [user]** Review pass with the user on each report — P2.3–
  P2.11 are now content/layout-complete against goldens, but titles/column
  headers are still hardcoded English rather than `LABELS.TXT`-driven
  (P2.2 residue) and Congress page 2's FF portrait slot table only has
  10/25 positions confirmed; worth the user's eyes before calling P2 fully
  closed.

### P3 — Passable music

**Now:** `GSOUND.COL` General MIDI songs decoded and played through the
port's own sequencer + FluidSynth (`sound.c`, `pick_music.c`); Pick Music,
Sound Options, BGM/event ids, situational Military sting all in. "Passable"
here means: tracks play on the right cues, loop cleanly, don't glitch, and a
default SoundFont path works out of the box.

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
- [ ] **P3.2 [auto]** Wire the missing cues found in P3.1. **Not
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
- [ ] **P3.5 [user]** Listen test with the user on a handful of tracks vs
  DOSBox reference. Anything "sounds wrong but recognizable" is D5, not
  here.

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
- [ ] **P4.2 [auto]** Town Hall L2/L3 outer-ring tiles (was W3.3): needs a
  `colony.h` layout change — save-bridge-adjacent, so confirm layout with
  the user **[user]** before touching, then port.
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
- [ ] **P4.4 [auto]** Custom House (Stuyvesant): auto-sell at EOT with
  boycott + WoI rules. **2026-08-27: real near-miss, reverted — worth
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
- [ ] **P4.9 [user]** Colonist auto-assign on join (`FUN_15eb_28c8`, W1.7
  structural port exists + golden): wire for the **player** colony join
  path — changes default behavior, confirm with user.
- [ ] **P4.10 [auto]** Colony production preview matches actual EOT result
  (regression test: preview == turn delta for a fixture colony).

### P5 — War of Independence: declarable, fightable, winnable

**Now:** declare (menu + auto) with SoL ≥ 50 gate, `@INDEPENDENCE` letter,
Europe closed post-declare, bell pool → intervention, REF wave/landing
scorer, merc offer, Continental Army muster, win/lose latches — all
"structural" or "thin". Combat: land/naval engage, best defender, fort
tiers, promote/demote/capture, plunder, coastal fort fire, Combat Analysis
— playable bar Done. Gaps are depth and the REF's own campaign behavior.

- [ ] **P5.1 [auto]** REF campaign loop: turn-by-turn REF behavior after
  landing (target choice, siege, re-embark, reinforcement waves from
  `backup_force`), king's replies. Port from `43f7`/`4345` bodies to the
  point where a REF actually prosecutes a war against the player, not
  just lands once.
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
- [ ] **P5.3 [auto]** Combat depth needed for a fair WoI: ambush bonus,
  artillery in the open / in colony, veteran status, Continental
  Army/Cavalry types, REF regulars/cavalry/artillery strengths and
  bonuses, Man-O-War vs Frigate/Privateer, bombard. Cross-check
  [combat.md](combat.md) status matrix; deep `−0x6790` AI scoring stays D1.
- [ ] **P5.4 [auto]** Colony capture/recapture mechanics during WoI
  (Tory/rebel population effects, `@CAPTURED*`, fort damage). **Checked
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
- [ ] **P5.5 [auto]** Foreign intervention force: arrival, control
  (player-controlled per DOS), Man-O-War spawn placement (was W4.2 —
  attempt static first, **[live]** fallback). **Checked 2026-08-26 —
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
- [ ] **P5.7 [user]** Full playthrough test with the user: declare on a
  lategame fixture (`valid-lategame-saves/COLONY*`), fight to a win.
  Fixture-driven `unit_ai_king` scenarios stay the regression net.
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
the Europe screen; tax audience Done; price change notices are status
lines.

- [ ] **P6.1 [auto]** Price model to DOS: `price_group_state`, EOT
  attrition, colony production feedback, buy/sell volume thresholds per
  commodity, `@PRICEUP`/`@PRICEDOWN` as real popups where DOS pops them.
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
- [ ] **P7.2 [auto]** Each LCR outcome fully applied. **Status
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
  gap; flagging rather than closing outright.
- [ ] **P7.3 [auto]** Treasure train: move rules (1 MP, no boarding
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
  `europe_cash_treasure`). Still open in this row: the non-Cortes
  cash-in-without-a-Galleon king's-offer flow itself (blocked on
  `@KINGGALLEON2`'s own PARK, tracked at P7.4) and explicit WoI behavior
  for treasure trains (not checked this pass).
- [ ] **P7.4 [auto]** KINGGALLEON2 re-attempt with the narrower `38fd`
  overlay hint from `ai_port_plan.md` T1.13 — if still negative, ship the
  manual's documented share and PARK the string.
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

- [ ] **P8.1 [auto]** Teach: one-shot per village, skill by village type
  (`@LEARN*` full set), Scout → Seasoned, expert refuses, alarm-band
  refusals — finish the "MissingWire" rows in
  [popup_audit.md](popup_audit.md). **`@LEARNCRIMINAL` wired 2026-08-26:**
  a Petty Criminal adjacent to a village is now refused outright
  (`ai_contact_is_petty_criminal` gate in `ai_contact_teach_skill`,
  before the existing Free-Colonist/Scout learner check), one-shot not
  consumed — was previously silently ignored (fell through
  `ai_contact_is_teachable_learner`'s name filter with no popup at all).
  New regression: `test_ai_contact.c` "LEARNCRIMINAL". **`@LEARNALREADY`
  checked, deliberately left alone:** the already-taught-village silent
  skip is an existing, documented design choice (preserves gift/trade
  chrome the same turn — see `indian_contact.md`), not an oversight; a
  popup here risks changing that intent, so not touched without the
  user's call. **`@LEARNSTAY`/`@LEARNLATER`/`@LEARNDONE`** (DOS's actual
  accept/decline CHOICE around teaching, vs. this port's instant-apply)
  and **`@LEARNSLOW`** (Indentured Servant learner, not currently
  recognized as teachable at all) stay open — real behavior-shape
  questions needing a decompile trace of the `5bfb` teach dispatch
  before porting, not safe to guess from GAME.TXT text alone.
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
- [ ] **P8.3 [auto]** Gifts: Small/Large/Generous amounts and alarm effect,
  gift-of-goods from wagon/ship hold (already thin), tribute demand outcomes.
  **Checked 2026-08-26, not implemented this pass — mapping is ambiguous,
  same status-text gap shape as P8.4's raid fix but riskier to guess.**
  `ai_contact.c`'s Gift/Demand CHOICE flow (`ai_contact_apply_gift_gold`,
  `ai_contact_apply_demand_tools`, `AI_POPUP_TAG_CONTACT_GIFT`/`_DEMAND`)
  is entirely hand-typed English, never `popup_msg_fill`. Candidate
  `GAME.TXT` sections exist (`@CHIEFGIFT`, `@TRIBUTE`, `@TRIBUTEUSA`,
  `@GIFTS`, `@CHIEFBORED`) and none are wired anywhere in `src/`, but
  their exact trigger mapping isn't obvious from text alone:
  `@TRIBUTEUSA`'s "{tribe}... willing to overlook this... for an indemnity
  of {N}" reads like a strong match for the Demand-gold CHOICE body, but
  `@TRIBUTE`'s near-identical "donation to the 'Church'" framing and
  `@CHIEFGIFT`'s "welcome the emissaries... beads worth {N}" (a
  *tribe-initiated* gift *to* the player, not the player-initiated Gift
  CHOICE this code implements) suggest at least 2 of these 5 sections
  belong to a different trigger than the one currently coded — possibly
  the Incite-Indians rival-payoff flow (`FUN_4d56_417e`, already ported)
  rather than plain Demand. Wiring the wrong section to the wrong trigger
  would be a real correctness bug (wrong text in the wrong context), worse
  than the current honest paraphrase — needs a decompile trace of the
  `5bfb`/`4d56` dispatch that actually calls each section (same class of
  RE work P8.5 above also needs) before porting, not attempted blind.
- [ ] **P8.4 [auto]** Raids on player colonies: trigger (alarm band +
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
  Still open in this row: stockade/soldier defense odds (the defender-wins
  path, real `4528`/`5fef` combat, not covered by this pass — that's raid
  *combat*, not the raid *loot outcome* text this pass touched). **Checked
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
- [ ] **P8.5 [auto]** Land purchase / encroachment CHOICE (`@INDIANLAND*`
  bribe / take / leave, Minuit free) — currently thin OK/status.
  **Scoped 2026-08-26, not implemented this pass — real trace needed
  first.** Confirmed the gap is real (zero code hits for `INDIANLAND`) and
  found the exact shape: `colonies_indian_land_purchase_gold`/
  `colonies_found_with_indian_land` (`colony.c`) compute a real cost and
  Minuit-free already works, but the call site
  (`game_do_found_colony_at_unit`, `game_loop.c` ~626-641) silently
  auto-pays when affordable and hard-blocks ("need N gold", founding
  refused outright) when not — never shows DOS's real 3-choice
  `@INDIANLAND` CHOICE ("Very well, we shall respect your wishes." /
  "We offer you {N}$ for this land." / "You are mistaken; this is OUR
  land now!"). There are actually **3 separate** encroachment CHOICEs in
  `GAME.TXT`, not one: `@INDIANLAND` (found colony, this call site),
  `@INDIANROAD` (pioneer road build), `@INDIANFOREST`/`@INDIANFOREST2`
  (pioneer clear-forest) — none of the three are wired; `@INDIANBRIBE`
  (accept-bribe follow-up) exists too and is presumably chained after
  picking "offer gold". **Why not just port it now:** the "Take it" free
  option's actual DOS consequence (alarm/relation delta amount, and
  whether "offer gold" is even selectable when the treasury can't cover
  it, or what DOS does then) isn't in any existing RE doc — the found-
  colony dispatch chain already traced for W1.2
  (`FUN_2b5a_1662`/`16ce`→`FUN_1000_8804`→...) stops before reaching this
  CHOICE's own handler, and `FUN_4cc6_07c2` (this cost formula's own
  citation) turns out to be misattributed — `FUNCTION_CATALOG.md` has it
  as "Indian contact/alarm distance score," a different function, so the
  real CHOICE dispatch address is still unknown. Porting the "Take it"
  consequence without that trace would mean inventing a game-balance
  number, against this project's own no-invent rule. **Needs:** a fresh
  decompile trace of the CHOICE dispatch (search near the cost formula's
  real caller, or `FUN_1000_8804`'s siblings) before this can be ported
  safely — flagging for a dedicated RE pass, not attempting blind.
- [ ] **P8.6 [auto]** Chief portraits on meet (`IND*.SS` shipped,
  unloaded) — cheap and visible; layout exactness is D4.
- [ ] **P8.7 [user]** Contact flow review with the user on a fresh game.
- [ ] **P8.8 [auto] — new, found 2026-08-26, not attempted.** Meet-village
  action menu is missing 3 of DOS's real 10 `NAMES.TXT` `@ACTIONS`
  choices. Cross-checked the port's current 6-item meet menu
  (`ai_contact.c` ~677: `{"Trade","Gift","Demand","Teach","Incite",
  "Leave"}`) against the real `@ACTIONS` table (`NAMES.TXT`, verbatim:
  "Trade With Village" / "Enter Hostile Village" / "Establish Mission" /
  "Denounce Heresy of %Fs Mission" / **"Live Among The Natives"** /
  **"Ask to Speak With Chief"** / "Incite Indians" / "Demand Tribute" /
  "Attack Village" / "Cancel Action"). Trade/Incite/Demand/Cancel map
  cleanly to the port's existing 4; "Enter Hostile"/"Establish Mission"/
  "Attack Village" are handled elsewhere as separate flows (raid-warn
  CHOICE, adjacent-Missionary auto-mission), not menu choices needing a
  slot here. **3 real gaps, genuinely unimplemented, not a doc-staleness
  case**: "Denounce Heresy of {tribe}'s Mission" (a rival-mission
  heresy-denounce action — the port's existing "foreign-mission heresy
  50/50" per `manual_gap.md` may be the automatic-outcome half of this
  same mechanic; unclear if a menu trigger is separately needed),
  **"Live Among The Natives"** — this is also the origin of
  `unit_orders.md`'s standalone "`@ORDERS` index 4 Live In Village —
  Missing" row (`NAMES.TXT` `@ORDERS`: "Live In Village, L") — choosing
  it from the meet menu is almost certainly what puts a unit into that
  persistent order state, unifying two previously-separate "Missing"
  notes into one real feature, and **"Ask to Speak With Chief"** (unclear
  mechanical effect — possibly the `@CHIEFGIFT`/`@CHIEFBORED` unprompted-
  gift sections found under P8.3 above belong here as its response body,
  not to a spontaneous event as first guessed there). **Not implemented
  this pass**: no PEDIA/GAME.TXT text found describing what "living
  among the natives" mechanically does once chosen (disappear from the
  map? passive teach? tribute discount?) — needs a decompile trace of the
  `@ACTIONS` dispatch (`5bfb`/`2820` territory, already PARKED for its
  deep body) before porting, not a guess. Flagging as a concrete, well-
  scoped lead — narrower and more specific than the old vague "Missing"
  notes it replaces.

### P9 — Founding Father effects (non-deferred ones)

**Now:** election/debate/pool Done; effects wired for Bolivar, Brebeuf,
Brewster, Cortes, de Soto, de Witt, Drake, Franklin, Hudson, Jefferson,
Paine, Penn, Pocahontas, Revere, Sepulveda, Washington, Las Casas
(assimilate), Minuit/Smith/Stuyvesant referenced from `colony.c`/`ai_euro.c`
(verify depth). Not found outside `founding_fathers.c`/`reports.c`:
**Fugger, Coronado, La Salle, Magellan (turn.c only), Jones (ai_ only)**.

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
- [ ] **P9.2 [auto]** Port missing/thin player-facing effects. **Status
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

- [ ] **P10.1 [auto]** Every slice in P1–P9 that touches `col1_save.h`,
  `col1_bridge.c`, or colony/unit layout runs `unit_col1_save` strict
  round-trip + a load-in-DOS spot check of at least one port-written save
  **[user]** when the change is bridge-adjacent (P4.2, P7.5).
- [x] **P10.2 [auto] — done 2026-08-26.** Added `tools/check_save_interop.sh`:
  builds just the `unit_col1_save` target then runs it alone via
  `ctest -R '^unit_col1_save$'` (the strict byte-identical round-trip over
  all 19 Col1 `.SAV` fixtures — W1.5's regression net) instead of the full
  42-test suite. ~0.1s. Optional arg overrides the build dir (default
  `build`). No CMake changes — no existing test carried labels, so a
  ctest label wasn't worth the churn; a thin wrapper script matches this
  row's own "(or ctest label)" either/or.
- [ ] **P10.3 [auto]** Legacy COLZ save path quarantine/removal (was W3.4)
  — **[user]** confirm timing; reduces surface that can drift.

### P11 — Popups: right text, options, layout

**Now:** ~80% of player-facing modals are "Authentic" per
[popup_audit.md](popup_audit.md); remaining MissingWire/Partial rows are
mostly in contact (`@LEARN*`, `@RAID*`, `@CHIEF*`), Europe
(`@PRICEUP/DOWN`), order gates (`@NEEDTOOLS`…), FA `3f41` thin, boycott
`DIPLO_BOYCOTT`, and "Invented" title strings in save/load.

- [x] **P11.1 [auto] — closed 2026-08-26.** Close every **MissingWire**
  row in `popup_audit.md` (wire the real `@SECTION` body/choices). Only
  one MissingWire row existed (the "Teach / convert / raid OK" row), and
  its `@RAID*` half was fixed this same session (P8.4). Its other half,
  `@LEARNALREADY`, is deliberately left silent by design (not a wiring
  gap — see the row's own note), so nothing further to close; updated
  `popup_audit.md`'s row to drop the now-stale `@RAID*` citation.
- [ ] **P11.2 [auto]** Convert **Partial** rows that are status lines but
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
  user's call before landing, not a silent auto-port. **"HELLO attitude"
  is the same Euro-rival first-contact greeting gap found under P8.2**,
  not an Indian-attitude thing — see that row.
- [ ] **P11.3 [auto]** Layout: popup width/height/wrap rules from the
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
  correctly out of scope for a same-pass fix.
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
- [ ] **P11.5 [user]** Popup review with the user during P1 sessions;
  file per-popup fixes here.

---

## Deferred phases (not worked from this file)

| # | Deferred | Where it lives | Minimum-thin rule |
|---|----------|----------------|-------------------|
| D1 | Rival Europeans behaving like DOS (`5d04`/`20e6`/`−0x6790`, goldens) | [ai_port_plan.md](ai_port_plan.md) T1/T2/T3, `golden_ai_joint` | Rivals must not crash, must found/trade/fight *something*; that bar is already met |
| D2 | Indian behavior 1:1 (`2820` trade/haggle, deep `4528`, `2154`) | [ai_port_plan.md](ai_port_plan.md), [indians.md](indians.md) | P8 thin outcome ports only |
| D3 | Known-seed determinism with DOS | [seed100_brave.md](seed100_brave.md), T4.3 | None required for playability |
| D4 | Pixel-perfect graphics / VGA-identical chrome (dialogs, TRADE/FA editors, Congress, king letter, `DECLARAT.PIK`, map digit colors) | old W5.1–W5.3, T5.x | Content + layout correct (P2, P11); frames may stay port-drawn |
| D5 | Fully faithful music (SC-55 timbre parity, per-driver quirks) | [assets.md](assets.md) | P3 "passable" bar |
| D6 | Present-but-unused digital SFX (`COLDIG.BIN`) | [assets.md](assets.md) | Not planned; user notes it is used in some versions — revisit only with a version that triggers it |

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
  2026-08-24 (later same day): **T1.8** (`0015bc`'s edge-cost formula now
  wired; `0015c1`/`0009ae` decompile clean and `000000` hand-transcribed
  but none byte-exact-ported yet, plus the still-unwired fort/colony `+8`
  mask-bit `0x40` — deprioritized, working substitute ships), **T1.13**
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

- [ ] **W1.3 — Production / EOT formula fidelity.** The economy loop runs
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
  own negotiation flow isn't fully traced. `unknown05` — confirmed to sit
  inside a real bit-array accessor's addressable range, but the only
  caller found is `WARNING`-flagged/corrupted with unrecovered literal
  args, a genuine dead end not a "grep harder" gap.

- [ ] **W1.7 — Colonist work-plot auto-assign (`FUN_15eb_28c8`) golden +
  wire.** RE is complete
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

- [ ] **W3.1 — Wire AI structural ports live** (`5d04`, `153e`, `28c8`) —
  `ai_port_plan.md` T3.1/T3.2 + W1.7's wire. Changes default AI behavior.
- [ ] **W3.2 — Re-enable `golden_ai_joint` cluster** — `ai_port_plan.md`
  T3.3. Only after AI transcription reaches T3 1:1 for in-scope planners;
  expect a large alignment/bug-fix phase immediately after (that phase is
  Tier 5's last row).
- [ ] **W3.3 — Town Hall level-2/3 outer-ring colony tiles.** DOS colonies
  with Town Hall L2/L3 work 12/20 tiles; `ColonizeColony` hardcodes 8 (the
  byte-exact DOS default tier — confirmed, see
  `colonist_work_plot_28c8.md`). Supporting L2/L3 needs a `colony.h`
  layout change (save-bridge-adjacent) — scope + confirm before touching.
- [ ] **W3.4 — Quarantine/remove legacy COLZ save path.**
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
- [ ] **W5.2 — Endgame cinematics:** king letter (`160a`), `DECLARAT.PIK`
  animation, Congress VGA chrome / F3 grid polish, HoF year-end dialogs,
  demo autoplay (`130d` tail).
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
- **Not planned:** `COLDIG.BIN` digital SFX — settled negative, do not
  revisit without new evidence ([roadmap.md](roadmap.md) Phase 5).

---

---

## Updating this file

Check off in place with date + one-liner, keep history, promote items
between tracks with a note on what unblocked them, and keep the **[user]**
gate real: prepare, then ask. When a deferred item becomes a playability
blocker, add the minimum-thin bullet to the relevant P-track rather than
un-deferring the whole item.
