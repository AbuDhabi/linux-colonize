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
  `game_loop` (colony screen / center unit).
- [ ] **P2.3 [auto]** F1 Religious Advisor (crosses, immigration, recruit
  pool) to DOS layout.
- [ ] **P2.4 [auto]** F2 Continental Congress / F3 as DOS splits them
  (bells, rebels, FF list) — F3 portrait grid already structural; keep the
  portraits, fix the stats column.
- [ ] **P2.5 [auto]** F4 Labor Advisor: profession × colony matrix,
  click-to-zoom.
- [ ] **P2.6 [auto]** F5 Economic Advisor: treasury, tax, market bid/ask
  table, boycotts, trade ledger.
- [ ] **P2.7 [auto]** F6 Colony Advisor: per-colony warehouse/status rows
  incl. SoL %, buildings-in-progress, click-to-zoom.
- [ ] **P2.8 [auto]** F7 Naval Advisor: ship rows with cargo icons,
  location/destination.
- [ ] **P2.9 [auto]** F8 Foreign Affairs: rival strength table (de Witt
  gating), war/peace status.
- [ ] **P2.10 [auto]** F9 Indian Advisor: tribes, attitude, missions.
- [ ] **P2.11 [auto]** F10 Score: DOS score table layout; retire path.
- [ ] **P2.12 [user]** Review pass with the user on each report once
  P2.3–P2.11 land.

### P3 — Passable music

**Now:** `GSOUND.COL` General MIDI songs decoded and played through the
port's own sequencer + FluidSynth (`sound.c`, `pick_music.c`); Pick Music,
Sound Options, BGM/event ids, situational Military sting all in. "Passable"
here means: tracks play on the right cues, loop cleanly, don't glitch, and a
default SoundFont path works out of the box.

- [ ] **P3.1 [auto]** Audit cue coverage: every DOS BGM/event id push site
  (`FUN_12d8_000e` callers, tables `0x2A6E` / `0x2AC4`) vs port call sites.
  List missing cues (colony enter/leave, Europe enter, contact, combat
  win/lose, declare, king audience, year-end, endgame).
- [ ] **P3.2 [auto]** Wire the missing cues found in P3.1.
- [ ] **P3.3 [auto]** Playback robustness: loop points, tempo/timing
  drift, note-off leaks, track change without pops, `--nosound` path.
- [ ] **P3.4 [auto]** SoundFont discovery: sane default search order +
  clear error when none found; document in README.
- [ ] **P3.5 [user]** Listen test with the user on a handful of tracks vs
  DOSBox reference. Anything "sounds wrong but recognizable" is D5, not
  here.

### P4 — Player colony production, complete

**Now:** economy loop runs end to end; expert bonuses, SoL/Tory modifier,
spoilage, hidden-resource discovery, construction and manufacturing chain
are in. Formula fidelity is the gap: manufacturing tier rates, class scale,
Town Hall L2/L3 tile rings, school/college/university training, Custom
House, horse breeding, food→colonist growth details.

- [ ] **P4.1 [auto]** Manufacturing tier rates + class scale against decomp
  ([building_production.md](building_production.md) open items). Golden
  against `golden_colony_prod01/02` once `COLONIZE/` assets are present
  in the worktree (currently missing → both goldens fail at `NAMES.TXT`).
- [ ] **P4.2 [auto]** Town Hall L2/L3 outer-ring tiles (was W3.3): needs a
  `colony.h` layout change — save-bridge-adjacent, so confirm layout with
  the user **[user]** before touching, then port.
- [ ] **P4.3 [auto]** Training (schoolhouse/college/university): teacher
  assignment, turns-to-train, `@NOTEACHER`/`@TRAINFAIL*` popups full.
- [ ] **P4.4 [auto]** Custom House (Stuyvesant): auto-sell at EOT with
  boycott + WoI rules.
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
- [ ] **P4.6 [auto]** Food surplus → new colonist at 200, starvation
  warnings/deaths, `@STARVE*` full path.
- [ ] **P4.7 [auto]** Warehouse/Warehouse Expansion caps + `@WAREHOUSEFULL`
  / `@SPOIL*` exact thresholds.
- [ ] **P4.8 [auto]** Building construction: hammers/tools consumption,
  `@CARGOREADY*`, "nothing to build" and prerequisite refusals from
  `GAME.TXT`.
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
- [ ] **P5.2 [auto]** Win condition: exact DOS rule (REF land force
  destroyed / % of REF committed and beaten / turn cap) and the
  `@INDEPENDENCEWON`-class endgame popups + score hand-off. Lose
  condition: colonies captured threshold / all-lost.
- [ ] **P5.3 [auto]** Combat depth needed for a fair WoI: ambush bonus,
  artillery in the open / in colony, veteran status, Continental
  Army/Cavalry types, REF regulars/cavalry/artillery strengths and
  bonuses, Man-O-War vs Frigate/Privateer, bombard. Cross-check
  [combat.md](combat.md) status matrix; deep `−0x6790` AI scoring stays D1.
- [ ] **P5.4 [auto]** Colony capture/recapture mechanics during WoI
  (Tory/rebel population effects, `@CAPTURED*`, fort damage). **Undefended-
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
  attempt static first, **[live]** fallback).
- [ ] **P5.6 [auto]** Post-declare economy rules: no Europe, Custom House
  continues, tax removed, bells → Continental Army promotions, SoL
  combat support % (already wired — verify).
- [ ] **P5.7 [user]** Full playthrough test with the user: declare on a
  lategame fixture (`valid-lategame-saves/COLONY*`), fight to a win.
  Fixture-driven `unit_ai_king` scenarios stay the regression net.
- [ ] **P5.8 [auto]** `unit_ai_king` first-failure-blocks-suite: fix the
  "multi-unload fortify count" failure so the ~204 downstream WoI checks
  actually run (was W2.2 residue).

### P6 — Player ↔ Europe trade, complete

**Now:** sail/harbor/buy/sell/recruit/hire/train/purchase/equip Done;
volume-price T0 (`FUN_38fd_0058` ±1 bids) Done thin; boycotts enforced on
the Europe screen; tax audience Done; price change notices are status
lines.

- [ ] **P6.1 [auto]** Price model to DOS: `price_group_state`, EOT
  attrition, colony production feedback, buy/sell volume thresholds per
  commodity, `@PRICEUP`/`@PRICEDOWN` as real popups where DOS pops them.
- [ ] **P6.2 [auto]** Tax raise events: full `@KINGTAX` cadence formula
  (trigger, amount, cap), `@TEAPARTY` boycott of that good, boycott lift
  (Fugger / pay-arrears `@BOYCOTT*` flow).
- [ ] **P6.3 [auto]** Sell/buy edge cases: partial holds, selling into a
  boycott, buying with insufficient gold (`@NOGOLD*`), 100-unit lots,
  tax applied to sales only.
- [ ] **P6.4 [auto]** Equip/unequip in Europe (muskets/horses/tools
  pricing via market, missionary bless cost) and the dock-order menu
  completeness.
- [ ] **P6.5 [auto]** Trade routes with Europe as an endpoint (`TRADE`
  editor already Done structural — verify Europe stops, wagon/ship
  auto-buy/sell amounts).
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
- [ ] **P7.4 [auto]** KINGGALLEON2 re-attempt with the narrower `38fd`
  overlay hint from `ai_port_plan.md` T1.13 — if still negative, ship the
  manual's documented share and PARK the string.
- [ ] **P7.5 [auto]** Rumour tile clearing + Col1 `path`/`mask` bits so
  DOS-loaded saves and port-explored rumours agree (P10 tie-in).

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
- [ ] **P8.2 [auto]** Alarm model for the player: per-village + tribe
  alarm accrual from proximity/land use/missions/combat, decay, thresholds
  for attitude words in F9 and `@INDIANCOMMENT`/`HELLO*` bands, Pocahontas
  halving — from `FUN_4d56_152e` (already ported) + the reader side.
- [ ] **P8.3 [auto]** Gifts: Small/Large/Generous amounts and alarm effect,
  gift-of-goods from wagon/ship hold (already thin), tribute demand outcomes.
- [ ] **P8.4 [auto]** Raids on player colonies: trigger (alarm band +
  proximity), target pick, outcome table (`@RAID*`: burn building, steal
  goods, damage ship, kill colonist, plunder gold), stockade/soldier
  defense, `@RAIDWIN*` — thin port of the `4528` raid *outcome* path only,
  not its deep AI.
- [ ] **P8.5 [auto]** Land purchase / encroachment CHOICE (`@INDIANLAND*`
  bribe / take / leave, Minuit free) — currently thin OK/status.
- [ ] **P8.6 [auto]** Chief portraits on meet (`IND*.SS` shipped,
  unloaded) — cheap and visible; layout exactness is D4.
- [ ] **P8.7 [user]** Contact flow review with the user on a fresh game.

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
- [ ] **P9.3 [auto]** Verify each wired effect with a unit test if none
  exists (`test_founding_fathers.c` covers a subset).
- [ ] **P9.4 [auto]** FF election chrome: `@WHICHFREEDOM` / `@FREEDOM`
  bodies already authentic; add the elect-effect one-liners DOS shows
  (e.g. Coronado reveal, Jones frigate arrival) where `GAME.TXT` has them.
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
- [ ] **P10.2 [auto]** Add a CI-style script `tools/check_save_interop.sh`
  (or ctest label) that runs only the interop suite fast, for use before
  every user handoff.
- [ ] **P10.3 [auto]** Legacy COLZ save path quarantine/removal (was W3.4)
  — **[user]** confirm timing; reduces surface that can drift.

### P11 — Popups: right text, options, layout

**Now:** ~80% of player-facing modals are "Authentic" per
[popup_audit.md](popup_audit.md); remaining MissingWire/Partial rows are
mostly in contact (`@LEARN*`, `@RAID*`, `@CHIEF*`), Europe
(`@PRICEUP/DOWN`), order gates (`@NEEDTOOLS`…), FA `3f41` thin, boycott
`DIPLO_BOYCOTT`, and "Invented" title strings in save/load.

- [ ] **P11.1 [auto]** Close every **MissingWire** row in
  `popup_audit.md` (wire the real `@SECTION` body/choices).
- [ ] **P11.2 [auto]** Convert **Partial** rows that are status lines but
  DOS shows a modal (`@PRICEUP`/`@PRICEDOWN`, order gates, `@CARGOREADY`
  ship-finish, HELLO attitude) to real modals with correct choice sets.
- [ ] **P11.3 [auto]** Layout: popup width/height/wrap rules from the
  `6f74` compositor (`FUN_6f74_36ca`/`3760`/`3848`) so multi-line bodies and
  CHOICE lists size like DOS — content correctness only; wood-frame pixel
  chrome is D4.
- [ ] **P11.4 [auto]** Token substitution audit (`popup_msg_fill`): every
  `%s`/numeric token in used sections resolves; add a test that walks all
  wired sections and fills with a fixture.
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
dump (T4.6, by policy), `unknown13_pad` tick-handler live watch (old W4.4).

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
  tick-handler install site — Ghidra's own XREF index (not just grep)
  confirms zero absolute-address writers into `DS:0xa660`/`0xa664`, so the
  writer (if findable statically) needs indexed/computed-address tracing,
  same class of problem as the readers; stays **W4.4** for a live watch.
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
- [ ] **W4.4 — `unknown13_pad` tick-handler install (live watch).** Only
  after W1.6's static grep of `DS:0xa660`/`0xa664` writers comes up empty:
  live write-watch those cells to find what installs the colony-screen
  tick handler.

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
