# Whole-Project Port Plan — Playability-First Work Queue

**Reframed 2026-08-24.** The project goal for this phase is **playability**: a
human can start a game, build an economy, trade with Europe, deal with the
natives, declare independence, fight the War of Independence and win — with
the UI, reports, popups and music good enough that none of it feels like a
placeholder. DOS-exact rival AI, 1:1 Indian AI, seed determinism, pixel-exact
art and full-fidelity music are **explicitly deferred** (see "Deferred
phases" below and the "Deferred AI track — detail" section, which absorbed
the old `ai_port_plan.md` / `ai_transcription.md` queues).

**Current posture (2026-09-03):** playability tracks **P1–P11 are closed**.
Remaining work is `bugs.md` nits, production/combat depth on Partial rows in
[manual_gap.md](manual_gap.md), and the deferred D1–D5 tracks below.

> **Doc merge note (2026-09-05):** `roadmap.md`, `ai_port_plan.md` and
> `ai_transcription.md` were merged into this file and removed. Their full
> changelog-style history (done T*/R* items, dated status notes) lives in git
> history of those paths. Citations like "ai_port_plan.md T1.18" elsewhere in
> the repo refer to that history; still-open T-items are carried below.

This file now also owns **phase order / whole-project "what's next"** (was
roadmap.md). North star: same rules, assets, saves, and inputs as DOS
Colonization (1994); when goals conflict prefer (1) save/data interop,
(2) gameplay/determinism, (3) UI parity, (4) visual polish last —
[project_goals.md](project_goals.md).

Status detail still lives with its owners:

| Detail | Owner |
|--------|-------|
| Feature Done/Partial/Missing | [manual_gap.md](manual_gap.md) |
| Popup inventory / authenticity | [popups.md](popups.md), [popup_audit.md](popup_audit.md) |
| Combat mechanics | [combat.md](combat.md) |
| Report screen (F2–F10 + HoF) DOS FUN map / layout | [reports.md](reports.md), [report_screens.md](report_screens.md) |
| SoL / independence | [sons_of_liberty.md](sons_of_liberty.md) |
| Indians | [indians.md](indians.md) |
| Production formulas | [building_production.md](building_production.md), [terrain_yields.md](terrain_yields.md) |
| Music / sound | [assets.md](assets.md) "Music / sound" |
| AI FUN inventory + deferred queue | **this file**, "Deferred AI track — detail" |
| Architecture constraints | [architecture.md](architecture.md) |
| Fidelity bar / conflict order | [project_goals.md](project_goals.md) |
| Decomp / data navigation | [original_index.md](original_index.md) |

## How an agent should use this file

Method contract (see "Method notes" in the Deferred AI track section below —
they apply to all RE/port work, not just AI): read raw decomp before trusting
summaries; check `address_mapping.csv` / `viceroy_globals.h` /
`dosbox-x-dumps/*` before filing anything as live-capture-blocked; never
invent a constant; never `git commit` / `push`; run **full** `ctest` before
calling anything done; update the owning status doc when a slice lands.

1. Read this file and the owning doc of whatever you touch (CLAUDE.md
   rule 1).
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

**All tracks P1–P11 closed** (final user passes 2026-09-03). Entries below are
compressed to result + trap + citation; full dated write-ups are in git
history of this file (pre-2026-09-05).

### P1 — UI correctness (player-guided)

Closed 2026-09-03; leftover UI nits go through `bugs.md`, not a pre-seeded
checklist. This track stays **[user]**-driven if reopened.

- [x] P1.1/P1.3/P1.4/P1.5 [user] — map/panel, colony, Europe screens
  user-passed playable 2026-09-03.
- [x] P1.2 [auto] 2026-08-28 — `smoke_play` (`tests/smoke/test_play_smoke.c`):
  headless real-asset run new game → landfall → found colony → colony/Europe
  screens → save; skips when `./COLONIZE` absent. Re-run it (plus a by-hand
  `run` pass when UI changed) before each user session.

### P2 — Report screens (F1–F10 + Hall of Fame)

All F2–F10 reports match golden-derived DOS pixel layouts
(`original_saves/report-screen-goldens/`); user-passed 2026-09-03. Leftover
polish (a few hardcoded English headers, F9 headband always #113, HoF has no
golden) is not a gate. Pixel-exact chrome stays D4.

- [x] P2.1 [auto] 2026-08-26 — every report's DOS renderer located; table in
  [reports.md](reports.md). **Trap:** only Labor (F4) has a real DOS
  click-to-zoom (grid → its own detail page); no report jumps to colony/map —
  earlier "click-to-zoom plumbing" wording was aspirational.
- [x] P2.2 [auto] closed 2026-08-29 — all report display strings resolve live:
  titles from `LABELS.TXT @MISC` (**trap:** DOS spells "RELIGIOUS ADVISER
  REPORT" — British spelling + suffix; greps for "Advisor" find nothing), FF
  names `@FATHERS`, jobs/cargo/tribes/nation adjectives/tribe levels from
  `NAMES.TXT` via shared `g_reports_names` parse (per-index buffers where a
  `rows[]` array would alias the shared scratch buffer). **Real bug found:**
  `ColonizeMsgSection` capped sections at 64 lines while `@MISC` has 223, so
  every index ≥ 64 silently used the static fallback — sections now heap-grown
  (`assets.c`). Regressions in `test_reports.c`.
- [x] P2.3–P2.11 [auto] — F2 Religious, F3 Congress (2 pages), F4 Labor
  (+detail), F5 Economic, F6 Colony, F7 Naval, F8 Foreign, F9 Indian, F10
  Score all Done to goldens. F8 de Witt reveal grid added 2026-08-31 from a
  raw-`.asm` re-read of `FUN_3f41_2548` (Ghidra drops its pushed values):
  gate = FF #4 or `head.show_entire_map`; peer line never wraps
  (x = 2/80/160/240); "Withdrawn" keys off `head.crown_nation_id`; met =
  `euro_relation` bit 0x20, peace = bit 0x40; `nation_flags` bit 0x04 splices
  "Free" into the header. HoF stays Done thin — no DOS golden exists for it.
- [x] P2.12 [user] closed 2026-09-03.

### P3 — Passable music

`GSOUND.COL` emulated literally (`gsound_vm.c`) + DOS BGM scheduler; Pick
Music, COLDIG SFX, Military sting in. Listen-test passed 2026-09-03.
Remaining SFX misfires are polish. Details: [assets.md](assets.md).

- [x] P3.1 [auto] 2026-08-26 — cue audit already existed in assets.md
  "Sound-ID ranges"; confirmed the gap list P3.2 closed.
- [x] P3.2 [auto] closed 2026-08-29 — every BGM cue mapped via whole-EXE asm
  sweep of `281f_04c0`/`04b6`/`048e`/`0498` calls (**method:** id is passed
  in `AX`, which Ghidra drops — `ndisasm` the call sites). `3844` `0x3e` is
  **@KINGFRIGATE** (ported: `ai_king_frigate_offer`, +10% tax on Yes);
  `364b_0000` is the colony popup helper, sound arg 0 at all 9 sites (no
  colony-open id exists). Only the Retire tune stays PARKed with the coin
  tier (difficulty.md). Full table: assets.md "BGM cues pushed by gameplay
  code".
- [x] P3.3 [auto] 2026-08-26 — playback robustness verified by code read
  (real `FF nn` loop opcode emulation, exact PIT divisor `0x4DBF`, note-off
  at every transition, `--nosound` gates). Loop/restart description later
  obsoleted by P3.6 (all songs end, none loop).
- [x] P3.4 [auto] 2026-08-26 — SoundFont search order documented in README;
  `sound_find_soundfont` already complete.
- [x] P3.6 [auto] 2026-08-27 — "bad remix" root cause was the **id table**:
  Pick Music entry n ≠ `0x20+n` (DOS `2b5a:264c` maps 9–12 → `0x39,0x38,
  0x3a,0x3b`, sublists to `0x29..`/`0x2e..`/`0x32/33/35/36`). Replaced
  hand-written decoder with literal driver emulator (`gsound_vm.c`) + DOS
  BGM scheduler. Details: assets.md "Music / sound".
- [x] P3.7 [auto] closed 2026-08-29 — COLDIG event ids wired: typed combat
  ids `0x3b + attacker type` (`5fef:2271`), raid/tax/wagon/sunk ids, chords
  `0x8020`/`0x8024` (driver table `0x2AB6`). Ids `0x4c`/`0x50`/`0x51`/
  `0x55`/`0x5c` have **no DOS push site — do not invent one**.
- [x] P3.5 [user] closed 2026-09-03 — music fine vs DOS; SFX leftovers are
  polish (historical follow-ups stay as D5 / assets notes).

### P4 — Player colony production, complete

Economy loop, manufacturing rates, Town Hall outer-tile preserve, education,
Custom House, horse breeding, growth, warehouse, construction, preview
goldens all in (2026-09-03). Remaining mismatches go through
[bugs.md](../bugs.md).

- [x] P4.1 [auto] 2026-08-26 — manufacturing rates already DOS-confirmed in
  [building_production.md](building_production.md); "assets missing" blocker
  was stale, `golden_colony_prod01/02` pass.
- [x] P4.2 [auto] 2026-08-29 — Town Hall L2/L3 outer-ring: user decision,
  runtime stays 8 tiles; the 12 outer bytes preserved opaquely
  (`col1_outer_tiles`). Evidence: all ~195 real DOS `.SAV` fixtures have
  outer slots `0xff` — DOS never writes them; W3.3 "12/20 tiles" claim
  retracted.
- [x] P4.3 [auto] 2026-08-26 — education already fully wired incl. 3
  graduate-type sections (`@TRAINPROFESSION`/`@TRAINCRIMINAL`/
  `@TRAININDENTURED`), `@TRAINFAIL`, `@NOTEACHER`.
- [x] P4.4 [auto] closed 2026-08-28 — Custom House controller split:
  human colonies gate via `custom_house_bits` (`15eb_0302`); only AI
  colonies use the `364b_0636` type gate (Lumber deny is AI-only). Sell
  price `euro_price − 1`, buy `euro_price + burden` screen-wide
  (`38fd_0040`/`0016`) — Linux had both +1; `europe_sell_price`/
  `europe_buy_price` added. **Trap (2026-08-27 near-miss, reverted):**
  wiring the `0636` deny list for human colonies read cleanly from the
  decompile but immediately failed both real-DOS goldens (Lumber genuinely
  auto-sells in DOS saves) — check `address_mapping.csv` call-site
  resolution before trusting a thunk target; comment in `europe.c`.
- [x] P4.5 [auto] 2026-08-26 — horse breeding from `FUN_15eb_1f72` tail:
  `potential = ceil(horses/divisor)*2` (25 with Stable else 50), capped by
  food surplus and warehouse headroom; goldens proved the *capped* figure
  is applied (static read alone suggested uncapped). `colony_prod_horse_breed`.
- [x] P4.6/P4.7/P4.8 [auto] 2026-08-26 — growth at food 200, all 5
  `@FOOD*`/`@STARVE*` sections; warehouse `100*(1+level)` + `@SPOIL1..4`
  4-way pick; construction hammers/tools + `@CARGOREADY0/1/2`. **Trap:**
  no `@NOTHINGTOBUILD` section exists — DOS omits ineligible buildings from
  the Change list rather than refusing (`@BUILD1-10` is the intro scroll).
- [x] P4.9 [user] closed 2026-09-03 — player admit auto-assigns
  (`colonies_auto_assign_idle`); `FUN_15eb_28c8` stays the AI scorer.
- [x] P4.10 [auto] 2026-08-28 — `golden_colony_preview01`: preview vs tick
  delta equality over both real fixtures; found AI `food += difficulty>>1`
  and Spring-only hammers gates missing from `colony_preview.c` — mirrored.

### P5 — War of Independence: declarable, fightable, winnable

Declare, REF waves (`0982` faithful), merc offer, Continental muster,
win/lose latches, intervention (P5.5), MP thirds all in. Residual REF
behavior is `4d56` crown unit-act (D1) and king's-reply chrome (P11/D4).

- [x] P5.1 [auto] closed 2026-08-28 — REF prosecutes and wins
  (`golden_woi_ref01`, real Dutch fixture). Five defects found by the
  headless sim, none visible to synthetic fixtures: exact-match
  `units_find_type` vs NAMES plurals ("Regulars"); crown slot `control==2`
  never move-refreshed; Euro AI spent the crown's moves before `war_act`;
  own stack "blocked" the hunt (`units_id_at` → `units_foreign_unit_at`);
  civilians held a port forever (only armed units defend now).
  **Real-save bug:** king latch bytes lived in `head.unknown46[]`, which
  *is* DOS `price_group_state[16]` — moved to `game_options` bits +
  `unknown23_pad` (`ai_king_latch_get/set`). Later same day:
  `FUN_43f7_0982` ported in full (MoW pool, garrison need, weakest-colony
  scoring, three relaxing passes, `0512` seizure) replacing the fandom
  wave. Still thin: MoW return-home despawn stand-in; `4d56` land scoring
  stays D1. See king_ref.md.
- [x] P5.2 [auto] 2026-08-26 — win/lose already wired. **Trap:**
  `@INDEPENDENCEWON` doesn't exist; real section is `@WINNING`. Lose =
  `@LOSING1/2/3`, all Authentic.
- [x] P5.3 [auto] 2026-08-28 — combat modifiers cross-checked against
  [combat.md](combat.md) peel table; all Done and unit-tested. "Bombard" in
  Col1 = coastal fort fire (Done thin).
- [x] P5.4 [auto] 2026-08-28 — colony-capture tail from `FUN_5fef_1b0e`
  ~100905-101030: rebel dividend `+0xc2 = old × 2/3` (SoL drops a third,
  not reset); peacetime plunder = treasury share `gold × pop / (pop + Σ
  loser's remaining pop)`; relations zeroed, WAR bit, `@HOWTOWIN` latch.
  **No fort damage in the tail** — buildings carry over in DOS too.
  Ported `colonies_capture_ex`. Undefended-colony token militia (2026-08-26):
  DOS always fields a civilian stand-in defender unless Revere + >49
  muskets — `units_spawn_colony_temp_defender`, free-walk capture removed.
- [x] P5.5 [auto] 2026-08-28 — `FUN_43f7_10f0` static: intervention force
  is **player-controlled** (spawn nation = `DS:0x5398`, the human), not
  ally-tagged; MoW water-tile scorer static after all; troop pools Cont.
  Cav ≤2 / Artillery ≤2 / Cont. Army remainder (Linux caps were inverted).
  `ai_king_foreign_intervene` rewritten.
- [x] P5.6 [auto] 2026-08-26 — post-declare economy all wired: Europe
  closed, Custom House tax-free, `FUN_43f7_1eca` bell promotions (full
  port), SoL combat support.
- [x] P5.8 [auto] 2026-08-29 — MP in DOS thirds everywhere:
  `units_type_max_mp = movement × 3`, `units_move_cost` =
  `FUN_465b_0000` head (road/colony pair or minor-river pair + cardinal
  → 1; tribe destination caps 3), col1 bridge imports/exports spent
  thirds (exhausted land units export 0), map panel shows fractions.
  `golden_ai_turns` byte-identical to baseline. Not modelled: 465b
  ocean↔high-seas force-to-max, NULL-rng partial-MP gamble.
- [x] P5.7 [user] closed 2026-09-03 — declare-to-win playthrough passed.

### P6 — Player ↔ Europe trade, complete

Sail/harbor/market/recruit/train/purchase/equip Done; volume-price
(`FUN_38fd_0058`) byte-exact (`golden_market_prices01`); tax audience Done.

- [x] P6.1 [auto] closed 2026-08-28 — EOT market tick byte-exact vs two
  real-DOS turn pairs (method: `sav_json` both saves, python replica
  iterated until it reproduced the after-save, then C port matched).
  Keys: phase-1 ledger is `trade.tons2`; pool decay only in nation 0's
  pass; rise/fall threshold sheds `±100` **unconditionally** (only the ±1
  bid step is range-gated — Linux gated both, so capped cargos ran
  pressure away); Dutch attrition ×2 on odd turns; `@PRICEUP`/`@PRICEDOWN`
  real popups. `1dfa`/`1d80` sale-volume ledger also exact: difficulty
  term `(difficulty−2)·16·amt/100`, every sale lands on all four nation
  records (Dutch ×2/3). Formulas: `turn/europe_nation_eot.md`.
- [x] P6.2 [auto] 2026-08-26 — tax raises already real (`38fd_5be8`
  interval gate, favor ladder, 0..75 clamp), `@TEAPARTY`, Fugger +
  pay-arrears `@SOMEBOYCOTT` (**trap:** no `@BOYCOTT*` section exists).
- [x] P6.3 [auto] 2026-08-26 — buy/sell edge cases verified. **Trap:**
  `@NOGOLD*` doesn't exist; DOS has no "can't afford" modal — silent
  clamp to `gold/ask` is faithful.
- [x] P6.4 [auto] — 2026-08-26 pass concluded "no Europe dock equip UI";
  **corrected 2026-08-31 (bugs.md): that 4-row dock menu was invented —
  DOS `FUN_38fd_37xx` is GAME.TXT `@ARMOPTIONS` (12 rows), now ported.**
  Colony-fence bless remains a separate free action.
- [x] P6.5 [auto] — **corrected 2026-09-03 by full `FUN_479b_0bd0`
  decode:** DOS trade routes DO auto-buy at Europe (sell unload-list, then
  buy load-list); old sell-entire-hold reading was wrong. Wagon-assigned
  Europe stop is an inert no-op (land-only), not a crash.
- [x] P6.6 [user] closed 2026-09-03 (covered by P1.4).

### P7 — Rumours and treasure

`units_resolve_lcr_rumour` is a full `FUN_65dd_0004` port; treasure
spawn/tick/cash, Cortes, king's galleon, WoI full-value all wired.

- [x] P7.1 [auto] 2026-08-26/27 — real LCR state machine (skill tier,
  de Soto Scout-gated reroll, floor-ratchet roll, all gold formulas,
  session counters). **Trap fixed:** case 2/9 identity was swapped in the
  port (case 2 spawns Treasure, case 9 Colonist — dialog tag is literally
  "LOSTCITY"+case). See archive/mysteries_catalog.md 65dd entry.
- [x] P7.2 [auto] closed 2026-08-28 — all 9 outcomes wired with real
  bodies. Fountain of Youth is 8× the real Recruit picker
  (`FUN_38fd_4884` with passage forced 0) — player picks each from the
  live pool; ported as `AI_POPUP_TAG_FOUNTAIN_YOUTH` chain.
- [x] P7.3 [auto] closed 2026-08-28 — Treasure boards only a Galleon
  (`require_galleon` param); **trap:** `@KINGGALLEON1` doesn't exist —
  real sections `@KINGGALLEON2/3`. WoI: King gone, full value via
  `@CASHTREASURE`.
- [x] P7.4 [auto] 2026-08-28 — `@KINGGALLEON2` = `FUN_5fef_1908` (string
  built as "KINGGALLEON"+"2"/"3", why literal greps failed); wired with
  difficulty-scaled share.
- [x] P7.5 [auto] 2026-08-26 — rumour-cleared interop: Col1 has no
  explored-rumour bit; port reuses the `path` visitor nibble on import.
  Regression on `dutch-reports.SAV`.

### P8 — Basic Indian interactions (teach, alarm, gifts, raids)

Structural contact/meet/teach/gift/demand/convert/raid, alarm,
encroachment, missions, Pocahontas in. Human village meet is DOS
`@ACTIONS` (`FUN_4d56_4528` human arm). Deep `2820` haggle stays D2 —
**do not open `2820`**. Full decode:
[indian_actions_menu.md](../original_sources_annotated/ai/indian_actions_menu.md).

- [x] P8.1 [auto] 2026-08-28 — teach = "Live Among The Natives" menu
  action (`thunk_FUN_1000_a618`): `@LEARNSTAY` → `@LEARNDONE`/`@LEARNLATER`,
  `@LEARNSLOW` random refusal, `@LEARNMAD` at quartile ≥ 2, skill from the
  2154 bid table with tech trims. The "needs a 5bfb trace" blocker was an
  asm read of `PUSH imm16` suffixes Ghidra dropped.
- [x] P8.2 [auto] — alarm writer/reader confirmed. **Traps:** F9 has no
  attitude-word column at all (only the headband portrait); `@HELLO*`
  sections are **Euro-rival** first-contact greetings, not Indian — wired
  2026-09-03 in `ai_diplo_153e_encounter` (`@HELLOUSA` still not modeled).
- [x] P8.3 [auto] 2026-08-28 — **trap:** `@TRIBUTE`/`@GIFTS`/`@WANTSTUFF*`
  are Euro-rival diplomacy text; `@CHIEFGIFT`/`@CHIEFBORED` belong to
  Speak With Chief; DOS has **no** player gold-gift village action
  (invented Gift row removed). Demand Tribute = `thunk_FUN_1000_a5f4`
  (`ai_contact_demand_tribute`, continent strength roll, `@EXTORT*`).
- [x] P8.4 [auto] 2026-08-26/28 — raid outcome chrome uses real `@RAID*`
  bodies; walls gate ported (`rand(0,12)-1` vs `walls*3+1` →
  `@RAIDNOTHING`; Stockade 4/13, Fort 7/13, Fortress 10/13). **Trap:**
  `@RAIDWIN*` was an invented section name; real tags are
  `@INDIANWIN0/1/2` + `@INDIANWINCOLONY(2)`.
- [x] P8.5 [auto] 2026-08-28 — all three encroachment CHOICEs
  (`@INDIANLAND`/`@INDIANFOREST`/`@INDIANROAD`). "Take it" has no
  immediate consequence (friction is the 152e pass); dialog only at PEACE
  — outside it DOS acts **free** (silent auto-pay and hard block removed).
  Tribal-land radius = tech tier (Inca 3 / Aztec 2 / others 1).
- [x] P8.6 [auto] 2026-08-29 — tribe portrait sheets `IND{tribe}A{tier}.SS`,
  tier from alarm quartile; placement DOS-exact with P11.3.
- [x] P8.8 [auto] 2026-08-28 — meet menu = real `NAMES.TXT @ACTIONS` with
  per-unit gating (wagon/ship trade, Scout chief, Missionary
  mission/denounce, colonist live-among, armed tribute/attack); invented
  Attack/Leave warn retired for met tribes. Thin: heresy-roll nearby-threat
  term is 0.
- [x] P8.7 [user] closed 2026-09-03.

### P9 — Founding Fathers, complete

All 25 Fathers wired; no Father PARK left. Per-FF table with DOS FUN, port
symbol, test: [founding_fathers.md](founding_fathers.md).

- [x] P9.1 [auto] 2026-08-26 — table written from `PEDIA.TXT @FATHER0-24`
  (Tier 1) + code read; `@FATHERS` weights diffed byte-exact.
- [x] P9.2 [auto] closed 2026-08-28 — La Salle re-swept per tick + synchronous
  `founding_fathers_la_salle_check` on admit (DOS shows it the instant pop
  hits 3). Asm-backed close-out found three real gaps: **de Soto sight**
  radius (all non-ships 2, Scouts +1, own-domain outer ring —
  `units_sight_radius`); **voyage length** — the 2-east/4-west crossing was
  invented; DOS `FUN_48d3_0002` is 1 turn, or 2 on `RNG>89 && ships>2 &&
  !Magellan` (`europe_voyage_turns_roll`); **Brewster** = free
  `@RECRUITCHOOSE` pick (cancel keeps crosses, re-asks). Drake's two ×1.5
  sites are genuinely different formulas; Jones's unrestricted frigate is
  byte-faithful.
- [x] P9.3 [auto] 2026-08-26 — every Father has cited test coverage.
- [x] P9.4 [auto] 2026-08-26 — **trap:** DOS ships no per-Father
  elect-effect blurb; `@FREEDOM` is the only elect text.
- Deferred here: rival-AI-parity effects (D1).

### P10 — Mapgen + DOS save interop: keep green

Col1 save/load byte-identical on all 19 `.SAV` fixtures; mapgen matches
MAPEDIT; `.MP` load Done.

- [x] P10.1 [auto] 2026-08-29 — apply → capture → re-apply net added to
  `unit_col1_save`; caught the 32-colony cap (DOS gate is `< 0x30`, now 48)
  and dropped human Europe-lane ships (now written as DOS does: `228+n`
  port, `232+n` outbound, `244+n` inbound, `turns_worked` = voyage turns).
  Details: [savegame.md](savegame.md). 2026-09-03: Linux-written save
  loads in real DOS, ship arrives.
- [x] P10.2 [auto] 2026-08-26 — `tools/check_save_interop.sh` (fast
  `unit_col1_save`-only gate, ~0.1s).
- [x] P10.3 [auto] 2026-08-29 — legacy COLZ save path deleted; `savegame.h`
  is Col1-only.

### P11 — Popups: right text, options, layout

~80% of modals "Authentic" per [popup_audit.md](popup_audit.md); remaining
Partial rows are mostly contact/order-gate/FA-thin/save-load titles.

- [x] P11.1 [auto] 2026-08-26 — the only MissingWire row closed with P8.4.
- [x] P11.2 [auto/user] — `@PRICEUP`/`@PRICEDOWN` are real modals (DOS pops
  them itself: `FUN_281f_0652` inside `38fd_0058` phase 4, human only);
  user kept them 2026-09-03. HELLO greetings under P8.2.
- [x] P11.3 [auto] 2026-08-29 — real compositor is `FUN_6f74_14c6`/`1198`
  (Ghidra's `FUN_7b29_*` labels are mislabeled near calls from `6f74`, not
  a missing overlay). Rules in `ai_popup_render`: content width = `@width`
  (default 80), wrap in width−4, frame +3/side, pitch = glyph height + 1
  (6-px font counts as 5), outer = text+12, centred+clamped; portrait LEFT
  for tribes 0/3/5/7 + King, RIGHT otherwise. Per-section `@width=NNN`
  plumbed with no call-site surgery via `popup_msg_take_pending_width` →
  `AiPopupRequest.width`. Still open: exact DOS wrap
  (`FUN_6f74_36ca`/`3760`/`3848`) and enqueues that skip `popup_msg_fill`.
- [x] P11.4 [auto] 2026-08-26 — token audit `test_popup_msg.c`: all 181
  wired sections fill clean, no surviving `%STRING`/`%NUMBER` markers.
- [x] P11.5 [user] closed 2026-09-03.

---

## Deferred phases (not worked from this file)

| # | Deferred | Where it lives | Minimum-thin rule |
|---|----------|----------------|-------------------|
| D1 | Rival Europeans behaving like DOS (`5d04`/`20e6`/`−0x6790`, goldens) | "Deferred AI track — detail" below, `golden_ai_joint` | Rivals must not crash, must found/trade/fight *something*; that bar is already met |
| D2 | Indian behavior 1:1 (`2820` trade/haggle, deep `4528`, `2154`) | "Deferred AI track — detail" below, [indians.md](indians.md) | P8 thin outcome ports only |
| D3 | Known-seed determinism with DOS | T1.23/T3.3 below (seed-100 notes: git history of `docs/seed100_brave.md`) | None required for playability |
| D4 | Pixel-perfect graphics / VGA-identical chrome (dialogs, TRADE/FA editors, king letter). Congress F3 plates are **Done** to goldens; `DECLARAT.PIK` is unused leftover (signing uses DECOIND.PIK) | old W5.1–W5.3, T5.x | Content + layout correct (P2, P11); frames may stay port-drawn |
| D5 | Fully faithful music (SC-55 timbre parity, per-driver quirks) | [assets.md](assets.md) | P3 "passable" bar |
| ~~D6~~ | ~~Present-but-unused digital SFX (`COLDIG.BIN`)~~ | [assets.md](assets.md) | **Undeferred and closed 2026-08-29** — P3.2 / P3.7 both `[x]`. Playback + every reachable push site wired; ids `0x4c`/`0x50`/`0x51`/`0x55`/`0x5c` have no DOS push site. Retire coin-tier stays PARK (difficulty.md) |

Also parked with these: MAPEDIT catalog track (old W5.4), `VR_B465X` hang
dump (T4.6, by policy). (`unknown13_pad`/old W4.4 closed 2026-08-27 — static.)

---

## Deferred AI track — detail (merged 2026-09-05 from `ai_port_plan.md` + `ai_transcription.md`)

This section is the surviving, still-relevant core of the two removed AI
docs: the fidelity-tier vocabulary, the hard-won method notes, the FUN_*
inventory with honest per-module claims, and the **still-open** queue items.
Every completed T*/R* item's full dated write-up is in git history of
`docs/ai_port_plan.md` / `docs/ai_transcription.md`; per-function deep dives
live on in `original_sources_annotated/ai/*.md` (unaffected by the merge).

**Track status: DEFERRED (2026-08-24).** Work items here only when a P-track
needs a minimum-thin unblock or the user explicitly asks for AI work.

### Fidelity tiers

**Long-term goal:** every original AI control-flow path that affects game
state has a Linux counterpart with matching behavior, including DOS LCG call
order where the original burns RNG.

| Tier | Meaning |
|------|---------|
| **T0 — Behavioral slice** | Looks like the original at a high level; RNG / edge cases may differ |
| **T1 — Save-diff** | Matches observable fields in original saves after the same setup |
| **T2 — Golden / bit-faithful** | Matches a locked golden (e.g. seed-100) tile-for-tile / unit-for-unit |
| **T3 — 1:1 transcription** | Structured like the decomp (dispatcher → goals → scoring), all branches. **Not claimed** for any full planner |

**Port rule:** AI algorithms are baked into C from VICEROY decomp (not data
files — [data_vs_hardcoded.md](data_vs_hardcoded.md)). Use
[`dos_rng.c`](../src/core/dos_rng.c) for any path that must match seed-100
or save-diff. Planner modules are split (`ai_euro` / `ai_contact` /
`ai_diplo` / `ai_king` / `ai_goals` / `ai_popup`); `ai.c` keeps init, pulse,
and nation-turn entry.

**Golden alignment (for when the gates come back on):** alignment means
improving port fidelity to DOS, not scripting special cases. When Linux
output disagrees with a golden: diff the field → trace the DOS FUN_* /
annotated thin map that owns that mutation → fix or deepen the ported path.
No seed-/turn-/nation-only exception tables unless explicitly documented as
temporary PORT DEBT with a retire criterion. `--seed 100` overrides every
timer-word read for deterministic runs (docs/assets.md "Fixed seed");
`AI_EURO_EARLY_FIXTURE=1` re-enables the retired early-turn fixture for
regression bisect only.

### Method notes (don't relearn these)

- Read the **raw decompiled function** directly before trusting a secondary
  annotated-doc summary — those have drifted from the actual bytes before.
- A `CALLF <loader>; JMPF 0x0000:XXXX` in a decompile is an **unpatched
  RTLink placeholder**, not real content — resolve it via `rtlink_decode`'s
  jump-table parser (info mode), never by naive tail-following or reading
  the raw bytes as data.
- A `completed=true` decompile can still carry real corruption, in its own
  body (`WARNING:` lines, timeouts) or **inlined from a callee** — check
  whether a cited address actually falls inside the target function's own
  boundary before concluding "corrupted". Ghidra can also silently pull in
  *wrong but plausible* content via jump/call misresolution with no warning
  (`684c_08c0`, `15eb_1d4c` — both false alarms cleared by a boundary-first
  second pass). Prefer `tools/rtlink_overlay_extract.py` +
  `tools/GhidraImportOverlays.java` (re-disassembles each RTLink segment at
  its true DOS address) over the flattened `viceroy_unpacked.c` export for
  anything flagged suspicious.
- Cross-check any unnamed DS global / resident helper against
  `original_sources_annotated/include/viceroy_globals.h` and
  `tools/address_mapping.csv` before assuming it needs a live dump — many
  "unlabeled" globals are already named for a sibling function.
- **Before asking the user for a fresh live DOSBox-X capture,
  byte-pattern-search the existing `dosbox-x-dumps/*` saves** (throwaway
  script; static data is unchanged across saves). This closed 6 of 8
  original Tier-4 "needs a live session" items — the data was already
  sitting in an existing dump every time. Only file something as
  live-blocked after checking this and coming up empty.
- **Never invent a constant.** If a price/byte table has no captured value
  anywhere in the project, leave it stubbed with a comment.
- Structural confidence (params line up, globals named, formula shape fits)
  is **not** semantic confidence (what real-world mechanic this is). Keep
  the two separate in write-ups.
- When a fidelity fix changes behavior, the existing unit test usually
  encodes the *old* behavior — expect to rewrite test scenarios.
- The harness TaskList does **not** persist across sessions — this file +
  git log are the continuity mechanism.
- Ghidra's decompile drops immediate `PUSH`/`AX`-register arguments at some
  call sites — when an id/argument seems missing, `ndisasm` the raw overlay
  bytes at the call site (this unlocked the COLDIG event ids and the popup
  section tables).

### Open queue (all that remains of the old T-tiers)

- [x] **T1.23 — Brave residue in `golden_ai_turns`.** Closed 2026-09-05:
  all six TURN steps green (`AI_TURNS_ALL=1` clean run). The 3 residual
  diffs (TURN4→5 n=9 (36,52)→W, n=10 (48,41)→SE step2, TURN5→6 n=7
  (43,52)→S) were scoring holdouts of the same class as the existing
  mid-turn peel table — golden dir unambiguous from the TURN multiset +
  spent math (n=10's mv=7 = river-S cost 1 + SE cost 6 pins the path);
  downstream picks in the same nation streams stayed green, so no stream
  misalignment. 3 peel rows added (`k_mid_peels`). New debug hooks:
  `AI_SCORE_AT="n:x:y,..."` (pick_dir score dump at any coord),
  `DOS_RNG_TRACE=1` (indexed LCG draw log). `golden_ai_joint` target
  passes manually. Lower priority from the same pass:
  ship-band unload placement — the per-cargo `06ae` rule (decomp ~89587)
  shipped 2026-09-06 (`ai_euro_20e6_unload_mask`/`_unload_by_mask`; the
  first-colony beachhead branch and the empty-mask best-passenger fallback
  stay); `0a60`'s FOUND/CONTACT goal producers (*ocean* tiles next to
  villages / foreign colonies as ship goals, decomp ~87800–88060) were
  also ported 2026-09-06 as `ai_euro_0a60_settlement_goal_producers` — no
  longer a thin stand-in.
- [x] **T2.4 — Retire the Linux-only Euro alliance machinery (cleanup).**
  Closed 2026-09-06: deleted `ai_diplo_form/break_alliance[_ctx]`, ally
  treasury cost / treaty-min / trust penalty / Indian sticky raise /
  ally-aid / `fa_gift` / longevity helpers, the euro_balance ALLY arms
  (FA gift + imbalance-break CHOICE), the treaty-timer ALLY-expiry arm,
  the DIPLO_ALLIANCE/BREAK CHOICE applies, and the sticky "precludes new
  alliances" chrome. Kept: `AI_DIPLO_ALLY` define (self-pair virtual +
  ai_king 2244 byte-faithful read — never set on Euro pairs, so 2244
  eligibility reduces to self-only, as in DOS), tag 22 as a numbering gap,
  `DIPLO_BREAK` as the 13b0 treaty-cancel OK tag, the AI_TALK
  ALLY_PICK/ALLY_PAY paid-@SMITE stages (real DOS 153e), timer decrement +
  peace-tweak expiry (real 6d8e step 4). ai_diplo.c −422 lines,
  test_ai_diplo.c −876 (alliance tests removed; embargo-lift smoke moved
  to make_peace). ctest 57/57 + goldens green after.
- [x] **T3.3 — Re-enable `golden_ai_turns` / `golden_ai_joint`.** Closed
  2026-09-05 (user-confirmed): both DISABLED flips removed from
  CMakeLists.txt; full `ctest` now 57/57 with both gates green. Harness:
  `AI_TURNS_ALL=1` runs past a failing step, `AI_TURNS_ONLY=t` runs one
  step.
- [ ] **T4.6 — `VR_B465X` hang dump.** Parked **by policy** — a deliberate
  stop, not a stall. Do not resume without a new, stated reason.
- [ ] **T5.1 — VGA-identical dialog chrome** (meet/diplo/king wood frames,
  FA `3f41` full widget body, chief portrait `FUN_281f_04ac`). = D4.
- [ ] **T5.3 — F3 Congress portrait grid polish leftovers** (fatter-than-DOS
  bell glyph, F9 headband always #113, HoF has no golden — see
  [reports.md](reports.md); the plates themselves are Done to goldens).

### FUN_* inventory (status; deep dives in `original_sources_annotated/ai/`)

Symbols are Ghidra names in `original_sources_decompiled/viceroy_unpacked.c`.
Status: **ported** (full claim at stated tier) / **partial** (subset) /
**parked**.

**Tribe placement + Indian AI (`FUN_6a09_*`, `FUN_4d56_*`):**

| Symbol | Purpose | Linux | Status |
|--------|---------|-------|--------|
| `6a09_0006` | Capitals, satellites, Brave spawn loop | `ai_place_tribes_*`, `ai_spawn_brave_near` | ported (T2 seed-100) |
| `4d56_0038` | Settlement-record CREATE | covered by `ai_install_tribes` | partial (struct-equivalent) |
| `4d56_00e0`/`01e2`/`14fe` | Chain to growth/pulse dispatch | contact helpers, growth + pulse | partial (T0/T2 quiet) |
| `4d56_152e` | Village growth accumulator (capital-only gate!) + Euro-relation friction | `ai_grow_villages` | partial (T0) |
| `4d56_1816` | Indian nation turn entry: alarm prelude, unit loop, relation ticks | `ai_indian_nation_turn` + `ai_contact_*` | partial (structural; T2 quiet) |
| `4d56_1b3a` | Mid-turn: clear tables / ownership probes | — | partial (known; not raid) |
| `4d56_2154` | Meet economics (`0x9e58` ask / `0x9e78` bid tables) | `ai_contact_meet_economics_2154` + gift/demand | **Done** (scorer + `0ce0` work-slot gate) |
| `4d56_2820` | Village trade: sell/buy/haggle/gift (595 lines; "nest" was internal labels) | `ai_contact_*` trade paths | **Done** (2026-08-29 verification rewrite; see `indian_trade_2820.md`) |
| `4d56_3582` | Small helper after `2820` | friction floor | partial (thin Done) |
| `4d56_417e` | Incite Indians / WARPATH price + relation push | `ai_contact_incite_price` / `apply_incite` / `ai_contact_ai_incite_human` | **Done** both modes (2026-09-06 audit): price polarity fixed (multiplier is raw `0x5b1c` **alarm**+75, not 100−alarm), push resolved (`281f_0d6c` → `4cc6_00f2` = +100 alarm slam, French/Pocahontas-halved), `@NOCONTACT`/`@ALREADYSMITE`/`@UNFORTUNATE` gates + `@INDIANWARFARE` announce wired, Mode-1 target set = others minus crown (post-WoI: crown only — `0x5382`bit0=woi, not "AMERICA map"); Mode-2 wrong Euro↔Euro MET gate removed (DOS gates tribe↔human) |
| `4d56_4528` | Settlement enter/raid 9-way dispatch | `ai_contact_indian_raids`, `@ACTIONS` menu (all 9 human arms), `ai_contact_ai_incite_human`, `ai_euro_land_try_adjacent_village_seize` | **Done** logic (2026-09-06 audit): human 9-way byte-faithful (P8.8 + MP forfeit), AI arm cases 7/3/4 (Missionary) + 5 (teach pulse) + 9 (village seize, structural trigger) wired; AI scout/wagon arms (6/1) unreachable in Linux (AI movement never enters village tiles — no invented trigger); VGA meet chrome open (T5.1) |

**European AI (`FUN_521d_*`):**

| Symbol | Purpose | Linux | Status |
|--------|---------|-------|--------|
| `0000`…`0906` | Goal-table ops + founding helpers | `ai_goals.c` | partial (T0) |
| `0a60` (~5.5k lines) | Unit/colony goal writer + goal-consumption/orders engine | `ai_euro_colony_goals` (A–H writer) + `ai_euro_0a60_unit_housekeeping` (unit loop, live) + `ai_euro_0a60_settlement_goal_producers` (foreign-colony/village FOUND/CONTACT/MILITARY producers, live) + `ai_euro_0a60_goal_orders_structural` (consumption tail, live) | structurally done 2026-09-06 — per-unit `0x3148` housekeeping, garrison-quota distribution, ocean-tile ship-goal producers, G-table diplo gates + `−0x6168` max-tracker all live; the `FUN_1000_8aac` field-id wall RESOLVED (4fa8 cases 3/4/5/6/0xa/0xd carry no per-unit signal in the shipped binary; case 2 = chain splice — intended values substituted, documented per site). Remaining thin: haul work-queue gate/flags (deliberate, tested), DOS random weight seed. See `euro_goal_orders_0a60_full.md` |
| `20e6` (~2.2k) | Direction / move scoring, all unit kinds | quiet + `ai_euro_score_step` + 2026-08-27 structural land port + **2026-09-06 deepening**: LAB_52aa odds tail (crown==2 halving, Soldier/Dragoon colony mass gate via `8aac` case 0xb), scout/colonist `0x4c` village arms (→ `ai_contact` Speak-With-Chief / Live-Among outcomes; 8d4a attitude gate = session latch stand-in), colonist labor loop (fort-capacity wanted size, join/walk/Pioneer-convert, force-explore fall-through), LAB_3558 per-cargo unload mask (`ai_euro_20e6_unload_mask`, 0x40/0x20/0x10/0xffff bits; `0x1734`/`0x173c`/`0x173e`/`0x1740` substituted from live goal table + garrison flags), `−0x6168` rival strength live (`+0x3154` fatigue session-local), explore-plane low nibble = real seen-plane site score (fallback on generated maps) | partial — still thin: `8aac` cases 4/5/6 undecoded (flag-count reading), ship colony-sail matrix / HS spiral / haul tails, 8d4a attitude array. See `move_scoring_20e6_full.md` / `move_scoring_land.md` |
| `5b66` (44-line dispatcher → `479b_*` bodies) | Euro per-unit act | `ai_euro_unit_act` | partial (T0; case 7 FOUND + case 9 Pioneer-road ported full; case 8 thin) |
| `5c38`/`5c3c`/`5cf6` | Thin helpers before `5d04` | hire in planning | partial (T0) |
| `5d04` (~750) | Nation planning / hire / treasury | `ai_euro_nation_planning` (live; real treasury formula) + `ai_euro_5d04_nation_planning_structural` (full port, reference-only) | structurally complete; wiring the rest live is a deliberate future decision |
| `6d8e` | Euro AI dispatcher per nation | `ai_euro_dispatcher_turn` | shell **Done** (full control flow); "partial" inherited from callees |

Thunk wiring: `0554`→`5d04`, `0578`→`0342`, `050c`→`0a60`, `0488`→`5b66`
(→`20e6` via `04f4`). Goals ≈ `0a60`+`5d04`; scoring ≈ `20e6`; act ≈ `5b66`.

**Diplomacy (`FUN_15b3_*` / `FUN_5bfb_*`)** — thin map `euro_diplo.md`:

| Symbol | Purpose | Linux | Status |
|--------|---------|-------|--------|
| `15b3_0004`/`0032`/`0066`/`00d0` | Bilateral read/write/OR/clear | `ai_diplo_read/write/or_both/clear_both` | partial (structural) |
| `5bfb_10ec` / `13b0` | War eligibility / treaty sign-cancel | `ai_diplo_euro_balance`, form/break | partial |
| `5bfb_153e` | War-declare body; outcome jump table → 10 known targets | thin sting + `ai_diplo_153e_worthiness_score_structural` (reference-only) | partial — see `euro_diplo_153e_full.md` |
| `5bfb_3180` | Adjacent-unit encounter resolver | `ai_contact_encounter_scan` + naval ambush **Done**; diplo-dispatch branches parked | partial |
| `4cc6_00f2` | Indian relation delta | `ai_diplo_indian_relation_delta` | partial |

**King / REF (`FUN_43f7_*`)** — thin map `king_ref.md`, unit `unit_ai_king`:
`0004` SoL, `1d42` tax→REF, `2564`/`1a26` declare gate, `0108` eliminate
nation, `060a` landing score, `0982`/`06a6` REF wave, `2022`/`1eca` war act +
promote, `2424` nation dispatch, `10f0`/`1528`/`160a`/`2244` intervene /
announce / rename+cinematic / merc — all **partial structural** with the
listed sub-pieces Done (audience/merc formulas real, `160a` signing
cinematic Done, `38fd_5930` @KINGNEWWAR Done).

**Shared helpers:** `465b_0000` terrain MP → `ai_dos_move_spent`;
`281f_04ca`/`04d4` reseed/range → `dos_rng`; `124c_0040` generic distance
(not `20e6`-specific); `6662_0f74` land pathing → `units_next_goto_step` /
`units_greedy_next_step` (byte-exact toughness score); `4720_049e` is a
tension-notify handler (likely `@VIOLATE`), **not** a move driver, unwired.
`FUN_4d56_021a` is not a real symbol (decompiler gap after `01e2`).

### Per-module fidelity (honest — not blanket T3)

| Module | Claim |
|--------|-------|
| Early Euro TURN1→7 (`6d8e` path) | **T2** (joint fields) |
| Quiet Brave / tribes seed-100 | **T2** (T1.23 residue) |
| Indian×Euro `15b3` / sticky / meet floor 96 | **T2**-shaped partial |
| Ocean `3558` / first-colony `06ae` | Thin ports + soft-tip prior — not T3 |
| Mid `0a60` / `5d04` / `5b66` | Structural (2026-09-06) / partial / structural — not T3 |
| `2154` / `2820` bodies | **Done**; `4528` thin/partial — not T3 |
| Alarmed Indian unit-act | Escort peel + smoke — not T3 |
| King / REF | Partial structural (WoI battle path heavily hardened via bugs.md batches) |
| Mid / late joint goldens | `golden_ai_mid01`/`late01` green gates; `golden_ai_turns`/`joint` DISABLED (T3.3) |

### Evidence, gates and tests

| Artifact | Use |
|----------|-----|
| `original_saves/mapgen/SEED100.SAV` | Golden tribes/Braves; `golden_mapgen_seed100` |
| `test-saves-ai/TURN1.SAV`…`TURN7.SAV` | Early-AI T2 joint gate (`golden_ai_turns`, DISABLED) |
| `test-saves-ai/JOINT_MIDTURN.md` | Mid-game joint golden scaffold + field policy |
| `original_saves/COLONY00/01.SAV` | Rival fleets, sail, AI crosses save-diff |
| `COLONIZE/VR_SEED.EXE`, `VR_BRAVE*.EXE` | Seed-locked RE probes (not runtime) |
| `original_memory_dumps/`, `dosbox-x-dumps/` | RAM images for byte-pattern search (see method notes) |
| `tests/unit/test_ai*.c`, `test_founding_fathers.c` | Module units |
| `tests/golden/test_ai_turns.c` / `test_ai_mid01.c` / `test_ai_late01.c` | Joint field-diff gates |

```bash
cmake --build build --target golden_ai_joint
./build/golden_mapgen_seed100   # cwd = repo root
./build/golden_ai_turns         # TURN1→7 joint gate (currently DISABLED in ctest)
```

Size sense: Linux `ai.c` + `ai_*.c` ≈ 3.5k + modules; DOS Euro planner ≈
`6d8e` 500 + `0a60` 5.5k + `5d04` 750 + `20e6` 2.2k + `5b66`→`479b_*` 390;
Indian cluster ≈ `1816` 140 + `2154` 320 + `2820` 595 + `4528` 3k. The full
T0/T1 surface is in; remaining work is fidelity hardening, not missing
planner arms.

---

> The pre-2026-08-24 W-tier queue was moved to
> [archive/port_plan_w_tier_archive.md](archive/port_plan_w_tier_archive.md) (2026-09-05).

## Updating this file

Check off in place with date + one-liner, keep history, promote items
between tracks with a note on what unblocked them, and keep the **[user]**
gate real: prepare, then ask. When a deferred item becomes a playability
blocker, add the minimum-thin bullet to the relevant P-track rather than
un-deferring the whole item.
