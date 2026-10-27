# Whole-Project Port Plan — Status and Remaining Work

**History.** Reframed 2026-08-24 around **playability** (a human can start a
game, build an economy, trade with Europe, deal with the natives, declare
independence, fight the War of Independence and win, with nothing feeling like
a placeholder); that bar was met 2026-09-03. Rival and Indian AI were then
ported to logic level (deferred track, done 2026-09-08). Seed determinism with
DOS was dropped as a goal 2026-10-23 ([project_goals.md](project_goals.md)
"Determinism"). Music was accepted as done by the user (D5 closed); pixel-exact
art stays deferred (D4 below).

**Current posture (2026-10-26): the port is feature-complete.** Every rule
system of the DOS game is ported from the decomp and plays end to end (the
few exceptions are under "Remaining work"):
playability tracks **P1–P11 are closed** (final user passes 2026-09-03), the
deferred AI track is **done** (D1 closed 2026-09-07g, D2 closed at logic level
2026-09-08, D3 dropped as a goal 2026-10-23). Evidence:

- Four idle campaigns matched real DOS saves exactly over their spans and stay
  as ctest gates: `golden_idle_continuous` (1492–1599),
  `golden_idle_independence`, `golden_idle_america`, `golden_idle_america2`
  ([idle_campaign.md](idle_campaign.md)), plus `golden_idle_reload`
  (save/reload determinism).
- Five headless playtest campaigns (`play_driver`,
  [automated-playtests/RULES.md](../automated-playtests/RULES.md)) each won the
  War of Independence, all four nations, one per difficulty from Discoverer
  to Viceroy; every bug they found is fixed or refuted (#1234–#1272).
- [port_bugs.md](port_bugs.md) had no OPEN rows on 2026-10-26 (the docs
  review that day filed and fixed #1273-#1286); the pending-verification archive was emptied
  into [archive/bugs_closed.md](archive/bugs_closed.md) the same day.

What is left is listed under "Remaining work" below, plus the deferred polish
phase **D4** (pixel chrome).

## Remaining work (2026-10-26)

Not bugs in shipped features. Each is a known, bounded gap. Closed
2026-10-26: the ally-pick prompt (#1287), DOS `HALLFAME.DAT` (#1289), the
dead Pillage arm (deleted) and the Euro AI deferred-band tail + stance
census operands (#1291), and hot-seat multiplayer, ported for save
interop (#1290, [hotseat.md](hotseat.md)). Closed 2026-10-27: the demo and
autoplay (DS:0x828 / 0x826, [demo.md](demo.md)) and the colony-screen keyboard
focus ([colony_screen.md](colony_screen.md) "Keyboard focus") and the map editor
([mapedit.md](mapedit.md)).

| Item | What is missing | Where |
|------|-----------------|-------|
| [port_bugs.md](port_bugs.md) | OPEN rows | — |

**Old citations.** `port_plan P<n>.<m>`, the old "How an agent should use this
file" section and the closed D1/D2/D3/D6 rows: [archive/port_plan_p1_p11_2026-10-24.md](archive/port_plan_p1_p11_2026-10-24.md).
`T*.*` items, the AI `FUN_*` inventory and the Euro-AI status history:
[archive/port_plan_deferred_ai_track.md](archive/port_plan_deferred_ai_track.md)
(items closed before 2026-09-05: git history of this file).
`W*.*`: [archive/port_plan_w_tier_archive.md](archive/port_plan_w_tier_archive.md).
`roadmap.md` / `ai_port_plan.md` / `ai_transcription.md` (merged here
2026-09-05): git history. Grep these by id; don't browse them.

This file owns **phase order / whole-project "what's next"**. North star: same
rules, assets, saves, and inputs as DOS Colonization (1994); acceptance order
when goals conflict is in
[project_goals.md](project_goals.md#acceptance-order-when-goals-conflict).
Status detail lives with its owners — see the Authority table in
[architecture.md](architecture.md#authority).

## Method notes

They apply to all RE/port work: read the raw decomp before trusting an
annotated summary; check `address_mapping.csv` / `viceroy_globals.h` /
`dosbox-x-dumps/*` before filing anything as live-capture-blocked; never invent
a constant; a fidelity fix usually invalidates the unit test that encoded the
old behaviour, so expect to rewrite the scenario; run the **full** `ctest`
before calling anything done; update the owning status doc when a slice lands.
Full list (RTLink placeholders, boundary-first second pass, DS globals in both
sign spellings, `func_0x…` lookup by flat address, far-thunk fake argument,
dropped `PUSH`/`AX` arguments): [archive/port_plan_deferred_ai_track.md](archive/port_plan_deferred_ai_track.md)
"Method notes". Gate tags (`[auto]` / `[user]` / `[live]`):
[conventions.md](conventions.md) §6.

## Fidelity bar

"Playable" ≠ "byte-exact". For any playability work, the bar is:
- Uses real `GAME.TXT` / `NAMES.TXT` / `LABELS.TXT` strings where DOS does
  (no invented dialog bodies — [popups.md](popups.md) legend).
- Formulas match DOS where the decomp is already read; where it is not,
  the manual's documented behavior is acceptable **for now** with a PARK
  comment naming the DOS function to revisit.
- Never regress Col1 save interop (P10) — every slice runs
  `unit_col1_save` and the `.SAV` fixture round-trips.

---

## Closed tracks P1–P11 — trap digest

Full entries: [archive/port_plan_p1_p11_2026-10-24.md](archive/port_plan_p1_p11_2026-10-24.md). Leftovers go
through [port_bugs.md](port_bugs.md). Retracted leads that stop repeated dead
ends:

- **P2.1** Only Labor (F4) has a DOS click-to-zoom (grid → its own detail
  page); no report jumps to colony or map.
- **P2.2** DOS spells "RELIGIOUS ADVISER REPORT" (greps for "Advisor" find
  nothing). `@MISC` has 223 lines: a 64-line section cap silently fell back for
  every index ≥ 64.
- **P3.2** BGM ids are passed in `AX`, which Ghidra drops — `ndisasm` the call
  sites.
- **P3.7** COLDIG ids `0x4c`/`0x50`/`0x51`/`0x55`/`0x5c` have no DOS push
  site — do not invent one.
- **P4.2** All real DOS `.SAV` fixtures have Town Hall outer slots `0xff`: DOS
  never writes them. Runtime stays 8 tiles; the 12 outer bytes are preserved
  opaquely.
- **P4.4** The `364b_0636` Custom House deny list is AI-only (Lumber genuinely
  auto-sells for humans). Wiring it for humans read cleanly from the decompile
  and failed both real-DOS goldens — check `address_mapping.csv` call-site
  resolution before trusting a thunk target (comment in `europe_market.c`).
- **P4.5** Horse breeding applies the *capped* figure (goldens); the static
  read alone suggested uncapped.
- **P4.8** No `@NOTHINGTOBUILD` section exists: DOS omits ineligible buildings
  from the Change list (`@BUILD1-10` is the intro scroll).
- **P4.10** Hammers bank every tick; the Spring-only gate was removed
  (port_bugs.md #466).
- **P5.1** `head.market_demand_pool_raw[]` *is* DOS `market_demand_pool[16]`;
  king latches moved to `ai_king_latch_get/set`. The "`4d56` land scoring" row
  is refuted: overlay `4d56` is Indian AI end to end.
- **P5.2** `@INDEPENDENCEWON` doesn't exist; the section is `@WINNING`.
- **P5.4** No fort damage in the `FUN_5fef_1b0e` capture tail — buildings carry
  over in DOS too.
- **P5.8** DOS "NULL-rng" is `FUN_281f_04ca` = `randomize()`, a BIOS-tick
  reseed: ported for unseeded play (`dos_rng_set_live_ticks`); a fixed seed
  pins the tick as VR_SEED does.
- **P6.1** The market rise/fall threshold sheds `±100` unconditionally; only
  the ±1 bid step is range-gated.
- **P6.2 / P6.3** No `@BOYCOTT*` and no `@NOGOLD*` section. Corrected
  2026-10-26: the market-to-ship drag (`FUN_38fd_1fa2` raw 60434-60455)
  does **not** clamp to `gold/ask`; it refuses a full load the treasury can't
  pay, shows the cargo and cost on the status line, and a plain drag adds
  `@TUTORIAL18` (no option gate, every time). Other buy entry points were not
  re-checked.
- **P6.4** The 4-row Europe dock equip menu was invented; DOS is
  `@ARMOPTIONS` (12 rows).
- **P6.5** DOS trade routes do auto-buy at Europe (`FUN_479b_0bd0`: sell the
  unload list, then buy the load list).
- **P7.1** LCR case 2 spawns Treasure, case 9 a Colonist (the dialog tag is
  literally "LOSTCITY"+case).
- **P7.3 / P7.4** No `@KINGGALLEON1`; the sections are `@KINGGALLEON2/3`, built
  at runtime as "KINGGALLEON"+"2"/"3" (why literal greps fail).
- **P8.1–P8.5** "Needs a trace" blockers were `PUSH imm16` suffixes Ghidra
  dropped — read the `.asm`. Invented names/rows retired: `@RAIDWIN*` (real
  `@INDIANWIN0/1/2` + `@INDIANWINCOLONY`), the player gold-gift village action.
  `@HELLO*` are Euro-rival first-contact greetings and `@TRIBUTE` / `@GIFTS` /
  `@WANTSTUFF*` Euro-rival diplomacy text, not Indian. F9 has no attitude-word
  column.
- **P8.8** The heresy-roll nearby-threat term is real (`FUN_4cc6_03f8` ring-20
  attack sum), not 0.
- **P9.2** The 2-east/4-west crossing was invented: DOS `FUN_48d3_0002` is 1
  turn, or 2 on `RNG>89 && ships>2 && !Magellan`.
- **P9.4** DOS ships no per-Father elect blurb; `@FREEDOM` is the only elect
  text.
- **P10.1** The DOS colony cap is `< 0x30` (48), not 32.
- **P11.3** Ghidra's `FUN_7b29_*` labels near calls from `6f74` are mislabeled,
  not a missing overlay; the compositor is `FUN_6f74_14c6` / `1198`.

---

## Deferred phases (not worked from this file)

| # | Deferred | Where it lives | Minimum-thin rule |
|---|----------|----------------|-------------------|
| D4 | Pixel-perfect graphics / VGA-identical chrome (dialogs, TRADE/FA editors, king letter). Congress F3 plates are **Done** to goldens; `DECLARAT.PIK` is unused leftover (signing uses DECOIND.PIK) | old W5.1–W5.3, T5.x | Content + layout correct (P2, P11); frames may stay port-drawn |

D1 (rival Europeans), D2 (Indian AI 1:1), D5 (music: the user judged it done;
confirmed again 2026-10-26) and D6 (COLDIG SFX) are closed and D3
(known-seed determinism) is dropped as a goal; their rows are in the archive
file above. The MAPEDIT catalog track (old W5.4) closed 2026-10-27: the map editor is
ported ([mapedit.md](mapedit.md)).

## Deferred AI track

Done. History, `FUN_*` inventory and the full method notes:
[archive/port_plan_deferred_ai_track.md](archive/port_plan_deferred_ai_track.md).
What remains AI-side is D4
chrome: T5.1 VGA-identical dialog chrome (meet/diplo/king wood frames, FA
`3f41` widget body) and T5.3 F3 Congress portrait-grid polish (blocked on
material not in the repo).

## Updating this file

Record a result + trap + citation in a few lines and leave chronology to git
history ([conventions.md](conventions.md) §6); keep retracted-lead traps. Keep
the **[user]** gate real: prepare, then ask. When a deferred item becomes a
playability blocker, file it in [port_bugs.md](port_bugs.md) with the
minimum-thin fix rather than un-deferring the whole item.
