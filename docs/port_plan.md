# Whole-Project Port Plan — Playability-First Work Queue

**Reframed 2026-08-24.** The project goal for this phase is **playability**: a
human can start a game, build an economy, trade with Europe, deal with the
natives, declare independence, fight the War of Independence and win — with
the UI, reports, popups and music good enough that none of it feels like a
placeholder. DOS-exact rival AI, 1:1 Indian AI, seed determinism, pixel-exact
art and full-fidelity music are **explicitly deferred** (see "Deferred
phases" below and the "Deferred AI track — detail" section, which absorbed
the old `ai_port_plan.md` / `ai_transcription.md` queues).

**Current posture (2026-09-08 reassessment):** playability tracks **P1–P11
are closed**, `bugs.md` has no open rows, and the deferred AI track is
**substantially complete** — D1 closed 2026-09-07g (crown runs the full Euro
nation turn, `golden_woi_ref01` green on original thresholds), D2 bodies
(`2820`/`2154`/`4528`) Done at logic level, D3 golden gates all live
(60/60). What remains AI-side is the short **residual punch list** in the
Deferred AI track section, not a track. The only remaining *phases* are
D4 (pixel chrome) and D5 (music timbre).

> **Doc merge note (2026-09-05):** `roadmap.md`, `ai_port_plan.md` and
> `ai_transcription.md` were merged into this file and removed. Their full
> changelog-style history (done T*/R* items, dated status notes) lives in git
> history of those paths. Citations like "ai_port_plan.md T1.18" elsewhere in
> the repo refer to that history; still-open T-items are carried below.

This file now also owns **phase order / whole-project "what's next"** (was
roadmap.md). North star: same rules, assets, saves, and inputs as DOS
Colonization (1994); acceptance order when goals conflict is in
[project_goals.md](project_goals.md#acceptance-order-when-goals-conflict).

Status detail still lives with its owners — see the Authority table in
[architecture.md](architecture.md#authority) (AI FUN inventory + deferred
queue is the exception: that stays in **this file**, "Deferred AI track —
detail").

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
  BGM scheduler. Details: assets_sound.md.
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
  (2026-09-16, bugs.md #466: the Spring-only hammers gate was REMOVED — the
  Spring→Autumn fixture that motivated it has no carpenter staffed in any
  colony, so its unchanged hammers prove nothing. Hammers bank every tick;
  the preview keeps the lumber debit.)

### P5 — War of Independence: declarable, fightable, winnable

Declare, REF waves (`0982` faithful), merc offer, Continental muster,
win/lose latches, intervention (P5.5), MP thirds all in. The crown unit-act
residue **closed with D1 2026-09-07g** (crown slot runs the full Euro
nation turn — see king_ref.md "D1 CLOSED"); remaining is king's-reply
chrome only (P11/D4).

- [x] P5.1 [auto] closed 2026-08-28 — REF prosecutes and wins
  (`golden_woi_ref01`, real Dutch fixture). Five defects found by the
  headless sim, none visible to synthetic fixtures: exact-match
  `units_find_type` vs NAMES plurals ("Regulars"); crown slot `control==2`
  never move-refreshed; Euro AI spent the crown's moves before `war_act`;
  own stack "blocked" the hunt (`units_id_at` → `units_foreign_unit_at`);
  civilians held a port forever (only armed units defend now).
  **Real-save bug:** king latch bytes lived in `head.market_demand_pool_raw[]`, which
  *is* DOS `market_demand_pool[16]` — moved to `game_options` bits +
  `unknown23_pad` (`ai_king_latch_get/set`). Later same day:
  `FUN_43f7_0982` ported in full (MoW pool, garrison need, weakest-colony
  scoring, three relaxing passes, `0512` seizure) replacing the fandom
  wave. **2026-09-07:** the MoW return-home stand-in is retired — the real
  beat is the `FUN_521d_20e6` ship-band tail (raw 89717-89720 →
  `FUN_48d3_015e` High-Seas spiral, orders `0x45`, `DS:0x9456` census
  counter, no `expeditionary_force` credit), ported as
  `ai_king_mow_sail_home_20e6` and run from the MoW's own `war_act` beat.
  The "`4d56` land scoring" row is **refuted**: overlay `4d56` is Indian AI
  end to end (`FUNCTION_CATALOG` 2602-2623). DOS `1a26` gives the crown
  `control = 1` (raw 74833), so the crown runs the full Euro nation turn
  `6d8e` (raw 6407) and its REF units are moved by the ordinary `20e6` land
  arms; the port substitutes `ai_king_war_act`'s hunt instead. Closing that
  is a WoI battle-path rewrite (golden re-baseline) — tracked in D1, not a
  thin spot. See king_ref.md.
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
  `golden_ai_turns` byte-identical to baseline. 465b_05ca shore force-to-max
  **ported 2026-09-04** (`units_move_crosses_shore`, `units.c:6484`; applied
  at `units.c:6339` land + `9250` ship). Partial-MP gamble **verified ported
  2026-09-08** (`units_try_move` `units.c:~6940`: range(1,cost) ≤ remaining,
  spent==0 free step, attack never denied; `units_can_afford_move_cost` is a
  pathfinder pre-filter with no DOS equivalent — its deny is deliberate, a
  roll there would burn extra LCG draws). The DOS "NULL-rng" is
  `FUN_281f_04ca` = Borland `randomize()` (srand from BIOS tick 0040:006C)
  fired before the foreign clause whenever the cheap clauses fail — a
  wall-clock reseed, non-reproducible even in DOS; intentionally NOT ported
  (would break seeded determinism). See
  `original_sources_annotated/ai/move_spent.c` Section 5 note.
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
`@ACTIONS` (`FUN_4d56_4528` human arm). (`2820` was later fully ported —
2026-08-29 verification rewrite; the old "do not open 2820" fence is
obsolete.) Full decode:
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
  Attack/Leave warn retired for met tribes. Heresy-roll nearby-threat term is
  **real** (stale "term is 0" corrected 2026-09-07f): `FUN_4cc6_03f8` ring-20
  attack-sum threat pass at `ai_contact.c:8827-8858`, folded into the colony
  score at `:8900` and consumed by `ai_contact_denounce_heresy` (`:8965`).
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
  port, `232+n` outbound, `244+n` inbound, `col1_counter16` = voyage turns).
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
| ~~D1~~ | ~~Rival Europeans behaving like DOS~~ | **CLOSED 2026-09-07g** — `5d04` fully live (DOS hire ladder, invented matrix deleted), `0a60`/`20e6` structurally done with cargo/boarding/census arms live, crown slot runs the full Euro turn; `golden_ai_joint`/`turns`/`mid01`/`late01`/`woi_ref01` all live gates | Residue = punch list below, not a phase |
| ~~D2~~ | ~~Indian behavior 1:1~~ | **CLOSED at logic level 2026-09-08** — `2820` (2026-08-29 rewrite), `2154`, `4528` all 9 human + 7 AI arms, `152e`/`1816`/`1b3a`/`021a` Done; VGA meet chrome stays D4 | Residue = punch list below |
| D3 | Known-seed determinism with DOS | **Gates live** (T1.23/T3.3 closed 2026-09-05, 60/60 green). Open residue = documented PORT DEBT: `k_mid_peels` rows (`ai.c` ~3930), brave-wander `home_dist` golden-fit term (emp picker only). Partial-MP gamble closed 2026-09-08 (already ported; DOS "NULL-rng" = wall-clock reseed, intentionally skipped) | None required for playability |
| D4 | Pixel-perfect graphics / VGA-identical chrome (dialogs, TRADE/FA editors, king letter). Congress F3 plates are **Done** to goldens; `DECLARAT.PIK` is unused leftover (signing uses DECOIND.PIK) | old W5.1–W5.3, T5.x | Content + layout correct (P2, P11); frames may stay port-drawn |
| D5 | Fully faithful music (SC-55 timbre parity, per-driver quirks) | [assets.md](assets.md) | P3 "passable" bar |
| ~~D6~~ | ~~Present-but-unused digital SFX (`COLDIG.BIN`)~~ | [assets.md](assets.md) | **Undeferred and closed 2026-08-29** — P3.2 / P3.7 both `[x]`. Playback + every reachable push site wired; ids `0x4c`/`0x50`/`0x51`/`0x55`/`0x5c` have no DOS push site. Retire coin-tier stays PARK (difficulty.md) |

Also parked with these: MAPEDIT catalog track (old W5.4).
(`unknown13_pad`/old W4.4 closed 2026-08-27 — static; `VR_B465X`/T4.6 closed
2026-09-08 — static, see T4.6 below.)

---
## Deferred AI track — detail

**Track status: SUBSTANTIALLY COMPLETE.** Full history (fidelity-tier
vocabulary, FUN_* inventory, dated write-ups) moved to
[archive/port_plan_deferred_ai_track.md](archive/port_plan_deferred_ai_track.md).
Genuinely open AI-side work: land-assault-vs-2+-defender-colony residue is
CLOSED (bugs.md #521 FIXED 2026-09-22); remaining items are D4 chrome —
T5.1 VGA-identical dialog chrome (meet/diplo/king wood frames, FA `3f41`
widget body) and T5.3 F3 Congress portrait grid polish (blocked on material
not in the repo) — see the Deferred phases table above.

## Updating this file

Check off in place with date + one-liner, keep history, promote items
between tracks with a note on what unblocked them, and keep the **[user]**
gate real: prepare, then ask. When a deferred item becomes a playability
blocker, add the minimum-thin bullet to the relevant P-track rather than
un-deferring the whole item.
