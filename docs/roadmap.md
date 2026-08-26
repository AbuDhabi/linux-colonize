# Whole-project roadmap

Living phase map for the Linux Colonization port: **what’s next for the whole
project**. This is a navigation layer — not a merged feature checklist.

**Detail lives elsewhere:** row-level Done/Partial/Missing in
[manual_gap.md](manual_gap.md); AI FUN inventory and unpark/R5 queues in
[ai_transcription.md](ai_transcription.md); save field atlas in
[save_format_map.md](save_format_map.md); light catalog peels in
[catalog_peel_ranking.md](catalog_peel_ranking.md). Goals and conflict order:
[project_goals.md](project_goals.md).

Update this file when a phase’s **status** or **exit criteria** change. Keep
per-feature and per-FUN status in the owning gap docs.

---

## North star

Same rules, assets, saves, and inputs as DOS Colonization (1994). When goals
conflict, prefer:

1. Save / data interoperability
2. Gameplay / determinism
3. UI parity
4. Visual polish last

Full bar: [project_goals.md](project_goals.md).

---

## Current posture

**2026-08-24 — goal reframed to playability.** [port_plan.md](port_plan.md)
now carries priority tracks P1–P11 (UI, reports, music, colony production,
War of Independence, Europe trade, rumours/treasure, basic Indian
interactions, FF effects, save interop, popups). Rival-AI parity, 1:1 Indian
AI, seed determinism, pixel-exact art, faithful music and `COLDIG.BIN` SFX are
**deferred** (port_plan.md D1–D6). The phase table below is kept for
archaeology; the P-tracks are the working order.

The port is strong on **shell, map art, navigation, reports / pedia, Col1
save/load, basic units / naval passengers, founding a colony, and Europe
buy/sell/recruit/hire**. Structural Indian contact (including player dialogs),
Euro/Indian diplomacy, king/REF (audience/confirm/merc), FF election, trade
routes (Create/Edit/Begin), and early Euro AI (seed-100 T2 + thin expand/war)
are in (Sepulveda/Cortes/de Witt effects **Done**). Next playability work is
leftover **FF** KINGGALLEON2, deep mid-planner `20e6`, production / combat depth,
and endgame polish — not waiting on missing combat/capture prerequisites. VGA
dialog chrome, Congress UI, COLDIG SFX, and full 1:1 AI bodies remain later.
Snapshot source: [manual_gap.md](manual_gap.md) takeaway.

**Maintained (not blocking):** Col1 save RE P0–P6 is **Done**
([save_format_map.md](save_format_map.md)); keep interop green. VICEROY light
catalog (Layers A–C) is **closed**; Layer D deep extracts only on demand;
MAPEDIT parked ([catalog_peel_ranking.md](catalog_peel_ranking.md)).

---

## Phases

| Phase | Status | Exit criteria (player-visible / save) |
|-------|--------|----------------------------------------|
| **0 Foundation** | Done | New game, map, menus, Col1 save/load round-trip, basic move/orders |
| **1 Colony–Europe loop** | Mostly done | Found/assign/produce/sail/buy-sell/pioneer/trade-routes structural playable |
| **2 Contact & conflict** | Partial | Land/naval combat + capture + Indian meet/trade/raid usable for a human game without deep PARK blockers |
| **3 Mid-game AI** | Partial / active | Euro mid-planner (`5d04` / land `20e6`) + Indian large bodies advance; `golden_ai_joint` **PARKED / DISABLED** 2026-08-19 — no chance of staying green before AI transcription is complete, see ai_transcription.md R0 |
| **4 Independence & endgame** | Partial | WoI bell→intervention + Europe closed post-declare **Done** (2026-08-22); FF weighted pick **Done**; REF/WoI win/lose latches **Done** thin; KINGGALLEON2 / 65dd weights / Magellan west-sail PARK |
| **5 Fidelity & polish** | Later | VGA dialog chrome, COLDIG SFX, VIEW modes, pixel layout, T3 AI goldens, remaining PARKED deep bodies |

```mermaid
flowchart LR
  p0[P0_Foundation]
  p1[P1_ColonyEurope]
  p2[P2_ContactConflict]
  p3[P3_MidgameAI]
  p4[P4_Independence]
  p5[P5_FidelityPolish]
  p0 --> p1 --> p2
  p2 --> p3
  p2 --> p4
  p3 --> p5
  p4 --> p5
```

Phases **3** and **4** can advance in parallel after contact/combat basics.
Polish is last per [project_goals.md](project_goals.md).

---

## Phase notes and next work

### 0 — Foundation (Done)

Shell, map compositor, fog plane, reports/pedia, music, Col1 save/load I/O,
new-game flow, basic unit move/orders. Maintain save interop; do not reopen as
a feature phase.

### 1 — Colony–Europe loop (Mostly done)

Colony screen, production preview, warehouse↔ship, Europe sail/market,
recruit/hire/train/purchase, pioneer clear/plow/road, trade-route Create/Edit/
Begin + stop nibbles are in. Remaining thin spots (boycotts / volume-price
chrome, docks pick-among-pool UI, SoL display stand-ins) fold into phases 2–4
or polish rather than blocking the loop.

**Leftover (non-blocking):**

- Market volume / pressure chrome — [manual_gap.md](manual_gap.md) (Economy)
- Colony / Europe UI depth beyond structural — [manual_gap.md](manual_gap.md)

### 2 — Contact & conflict (Partial) — active

Human games need reliable fight/capture and Indian contact without depending on
PARKED deep/VGA bodies.

**Now:**

- Combat depth beyond T0 (ship-slow, deeper `5fef`) — [combat.md](combat.md)
- Production / EOT formula fidelity (food, spoilage, bells/crosses) —
  [manual_gap.md](manual_gap.md), [turn_between_players.md](turn_between_players.md)
- Fog / exploration leftovers (Zoom + Hidden Terrain VIEW modes are **Done**
  per [manual_gap.md](manual_gap.md); remaining fog depth is polish)
- Indian meet/trade/raid: structural Done; deep `2820`/`4528` stay PARKED until
  playability needs them — [ai_transcription.md](ai_transcription.md)

### 3 — Mid-game AI (Partial / active)

Euro rivals and natives must stay coherent through mid-game. Shared surfaces
(`20e6`, Indian×Euro diplo/sticky, raids, FOUND) used to be required to keep
`golden_ai_joint` green; that gate is **PARKED / DISABLED** as of 2026-08-19
(see [ai_transcription.md](ai_transcription.md) R0) — it has no chance of
being useful before AI transcription is complete, since every remaining
unported/stubbed AI callee is a guaranteed future diff. Do not chase it back
to green piecemeal; re-enable it once the port is done and it's a real
regression gate again.

**Now:**

- Euro deep land `20e6` / remaining mid-planner (`5d04`) — unpark #4 / R5 Phase 3
  in [ai_transcription.md](ai_transcription.md)
- Indian large bodies (`2154` / `2820` / `4528`) toward R5 Phase 4 — same doc
- Joint mid/late goldens (`JOINT_MIDTURN`, `golden_ai_joint`) — R5 in
  [ai_transcription.md](ai_transcription.md)
- Seed-100 / early fidelity debt (R0) only as it blocks mid-planner claims —
  [seed100_brave.md](seed100_brave.md)
- **Found and fixed:** `units_display_name()` missing `Colonists`→`Free
  Colonist` branch + the ~18 dead `units_find_type("Free Colonist")`
  exact-match call sites in `ai_euro.c` (real `NAMES.TXT` only has
  `Colonists`; the Europe-dock hire ladder silently never resolved). Detail
  + regression test: [assets.md](assets.md) Units section and
  `test_ai_euro_expand.c` (`unit_dock_farmer_hire_real_names`).
- **Found and fixed (2026-08-24, confirmed via static decomp trace) —
  `colonies_can_found` minimum-distance gate.** The `dx<=1 && dy<=1`
  Chebyshev-adjacency rejection already shipped 2026-08-14 (`3abe4c4`) is
  now confirmed byte-faithful against DOS, not an invented threshold: the
  real Build Colony order handler (`FUN_2b5a_1662`/`16ce`, an undocumented
  gap in `FUNCTION_CATALOG.md`) calls `FUN_1000_8804` →
  `FUN_15eb_0142`/`FUN_0000_5ff2` (nearest colony, any nation/type) and
  rejects when the `FUN_0000_2500` distance metric equals 1 — which
  evaluates to exactly 1 for all 8 Chebyshev-adjacent tiles. Full trace +
  the related `unit_ai_euro_expand` first-failure-blocks-suite gap (now
  also resolved, same fix): [manual_gap.md](manual_gap.md) "Found colony"
  row and [port_plan.md](port_plan.md) W1.2.

### 4 — Independence & endgame (Partial)

Finish a campaign: FF leftovers, king/REF, win/lose/retire.

**Now (Phase 5, 2026-08-22):**

- **KINGGALLEON2** — still **PARK** (Phase 5: 38fd overlay + string search negative)
- **10f0** — landing scorer, per-call caps, Veteran `0x15` — **Done** Phase 5; foreign MoW ship **PARK**
- **65dd LCR** — decomp case table documented; WoI case-1→2 redirect + case5 latch **Done**; full weight reroll loops **PARK**
- **Year-end D** — `rebel_sentiment` when set, else `ai_king_sol_percent`; continent table at DOS `−0x6bf0` **PARK**
- **Lategame codec** — **Done** (W1.5, 2026-08-24): all Col1 `.SAV` fixtures (starters + lategame + `TURN`) byte-identical on re-encode; the "not yet" here was a stale doc note — fixed same-day it was written (`753662d`, 2026-08-22), just never updated. See [save_format_map.md](save_format_map.md) / [savegame.md](savegame.md) Phase 5
- Bell-pool + bridge load + F3 Congress structural — **Done** (Phase 4)
- WoI bell pool → intervention/REF (`4345_0a22` wartime branch) — **Done** (2026-08-22)
- Europe closed during WoI; `game_options.woi` authoritative — **Done** (2026-08-22)
- Century-weighted FF debate pick (`4345_06d2` / `015a`) — **Done** (2026-08-22)
- `rival_nation_slot_1/2` + slot_2 intervene — **Done** (Phase 4)
- Decile SoL notify — **Done** thin
- Congress VGA-identical chrome — **Done** (2026-08-25): both pages match golden screenshots pixel-for-pixel (title/OK chrome, bells + rebel/tory + expeditionary-force bars, 4-col FF list, page-2 group portrait). Religious (F2) done the same way earlier. See [report_screens.md](report_screens.md) for the workflow — Labor/Economic/Colony/Naval/Foreign/Indian (F4–F9) still need the same treatment.

### 5 — Fidelity & polish (Later)

Do not prioritize over gameplay/determinism.

**Parked until later:**

- VGA-identical dialog / TRADE / FA / king letter chrome
- Digital SFX (`COLDIG.BIN`) — investigated at length; no reachable DOS trigger
  found in either sound driver or `VICEROY.EXE` itself (see
  [assets.md](assets.md) Music/sound). Likely unused shared-driver capability,
  not a fidelity gap — do not revisit without new evidence
- Pixel-exact layout and style
- Blanket T3 AI goldens and remaining deep PARKED bodies (full `2820`/`4528`,
  letter cinematic, hang-dump RE)

---

## Doc ownership

| Concern | Owner |
|---------|-------|
| Phase order / whole-project “what’s next” | **This file** |
| Code architecture / layering (present + intended) | [architecture.md](architecture.md) |
| Manual feature Done/Partial/Missing rows | [manual_gap.md](manual_gap.md) |
| AI FUN_* inventory, unpark queue, R5 | [ai_transcription.md](ai_transcription.md) |
| AI porting — sequenced agent work queue | [ai_port_plan.md](ai_port_plan.md) |
| Whole-project sequenced agent work queue (non-AI + cross-cutting) | [port_plan.md](port_plan.md) |
| Col1 field atlas / save RE | [save_format_map.md](save_format_map.md) |
| Save codec / interop notes | [savegame.md](savegame.md) |
| Light catalog peel queue | [catalog_peel_ranking.md](catalog_peel_ranking.md) |
| Acceptance order / fidelity bar | [project_goals.md](project_goals.md) |
| Move-enter authority | [move_enter.md](move_enter.md) |
| Combat mechanics | [combat.md](combat.md) |
| Report screen (F2–F10 + HoF) DOS FUN map / layout | [reports.md](reports.md) (owner doc — pending creation, see port_plan.md P2.1), [report_screens.md](report_screens.md) (porting how-to) |
| EOT / between-player turns | [turn_between_players.md](turn_between_players.md) |
| Decomp / data navigation | [original_index.md](original_index.md) |

---

## How to update

1. Finish a track → update row status in the **owning** gap doc first.
2. If that closes or unblocks a **phase exit criterion**, bump the phase status
   (and “Now” bullets) here.
3. Do not duplicate AI FUN tables or manual feature matrices into this file.
4. Suggested feature order detail that is still useful for bring-up archaeology
   stays under [manual_gap.md](manual_gap.md) “Suggested implementation order”;
   **phase priority** is owned here.
