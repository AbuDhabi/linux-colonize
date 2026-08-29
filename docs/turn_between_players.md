# Between player turns

End-to-end map of what runs after the human finishes orders and before the next
human Move Pieces. Owns the **orchestration** bridge (calendar, production,
nation EOT pieces, Euro/Indian/King order, market, chrome). AI planner
internals stay in [ai_transcription.md](ai_transcription.md).

Layer D extracts + thin callgraph:
[`original_sources_annotated/turn/`](../original_sources_annotated/turn/)
([`between_turns.md`](../original_sources_annotated/turn/between_turns.md)).
Callee depth maps: production (deepened), Europe EOT/market (deepened), census,
landfall, mid-pass rank/`1b3a`, ship-ready/spawn, fort fire, bells/FF, finish
bridge (same folder).

Bring-up checklist (shorter): [decomp_inventory.md](decomp_inventory.md)
“End-of-turn recovery checklist”.

---

## Linux path (authoritative for the port)

Entry: Space / Wait / End-of-Turn option →
[`game_do_end_turn`](../src/core/game_loop.c) →
[`turn_processor_start`](../src/core/turn.c) + frame-stepped
`turn_processor_advance` until idle → `game_finish_end_turn`.

Synchronous full run (tests): `turn_end`.

```mermaid
flowchart TD
  humanEOT[Human ends turn]
  setup[TURN_PROC_SETUP]
  euro[TURN_PROC_EURO]
  indian[TURN_PROC_INDIAN]
  finish[TURN_PROC_FINISH]
  after[game_finish_end_turn]
  nextHuman[Continue turn / next unit]
  humanEOT --> setup --> euro --> indian --> finish --> after --> nextHuman
```

### `TURN_PROC_SETUP` (no turn-owner indicator)

| Step | Linux | DOS provenance |
|------|-------|----------------|
| Calendar | `turn_advance_calendar` (`@TIMECHANGE`) | `FUN_130d_0290` year/autumn tick (runs **after** nation pass in DOS) |
| Colony production | `turn_run_colony_production` | `FUN_364b_0688` via `291f_0950` inside `3844_00f2` (per nation in DOS) |
| Coastal fort fire | `turn_run_coastal_fort_fire` | `FUN_364b_03f6` |
| Nation ticks | `turn_run_nation_ticks` (bells / crosses / FF) | Europe EOT / census pieces of `3844_00f2` + FF helpers |

### `TURN_PROC_EURO` (indicator on; one AI Euro nation per frame)

Order EN→FR→SP→DU; skip `human_nation` and withdrawn (`player.control==2`).

| Step | Linux | DOS |
|------|-------|-----|
| Set active + MP refresh | `turn_set_active_nation` / `turn_refresh_moves_for_nation` | spent clear is mid-pass in `130d`; MP refresh at act entry |
| Treasure tick | `units_tick_treasure_outside_colony` | `FUN_3844_0004` |
| Planner | `ai_euro_nation_turn` → `ai_euro_dispatcher_turn` | `FUN_521d_6d8e` after `3844_00f2` |

### `TURN_PROC_INDIAN` (indicator on; one nation 4..11 per frame)

| Step | Linux | DOS |
|------|-------|-----|
| Nation turn | `ai_indian_nation_turn` (`1816`-shaped) | Mid-pass `1b3a` in `130d` calls `1816(slot)` for each Indian slot (tribe flag bit7 clear) **before** the Euro loop ([`mid_pass_indian_rank.md`](../original_sources_annotated/turn/mid_pass_indian_rank.md)) |

### `TURN_PROC_FINISH` (no indicator)

| Step | Linux | DOS |
|------|-------|-----|
| King / REF | `ai_king_nation_turn` | `FUN_43f7_2424` via `291f_0a66` **inside** `3844_00f2` |
| Europe market | `europe_tick_market_prices` | `FUN_38fd_0058` (sibling of nation EOT `38fd_5e52`) |
| Human MP + treasure + Cortes | refresh + `units_tick_treasure_*` + `units_cortes_*` | Human treasure inside that nation’s `00f2`; Cortes elsewhere |
| Select next unit | `turn_select_next_unit` | Return to Move Pieces / focus |
| Autosave flags | decade Spring → slot 8 else 9 | `FUN_130d_0172` |

### After processor idle — `game_finish_end_turn`

1. Apply autosave requests  
2. `game_europe_deliver_bound_ships` (dock arrivals; may open Europe)  
3. Recenter camera on selected unit  

Status line: “Continue turn.” when a unit with moves is selected.

Turn-owner box (`FUN_1984_00aa`, 5×3 at 315,197) only while EURO/INDIAN steps run
(`turn_processor_show_indicator`).

---

## DOS spine

Main loop: **`FUN_130d_0290`** (thunk `FUN_281f_0546`). Annotated:
[`year_loop.c`](../original_sources_annotated/turn/year_loop.c).

Per Euro nation EOT (before AI or human act): **`FUN_3844_00f2`**
([`nation_eot.c`](../original_sources_annotated/turn/nation_eot.c)).

After calendar tick: **`FUN_3844_0442`**
([`year_end_chrome.c`](../original_sources_annotated/turn/year_end_chrome.c)).

```mermaid
flowchart TD
  mid[spent clear / rank euros / 4d56_1b3a]
  n0[nation 0: 3844_00f2 then AI or human]
  n1[nation 1..3 same]
  cal[year or season tick]
  ye[3844_0442 year-end chrome]
  mid --> n0 --> n1 --> cal --> ye
```

Major thunks (catalog):

| Thunk | Real body | Role | Layer D |
|-------|-----------|------|---------|
| `281f_0644` | `3844_00f2` | Nation EOT | [`nation_eot.c`](../original_sources_annotated/turn/nation_eot.c) |
| `281f_0638` | `521d_6d8e` | Euro AI dispatcher | ai/ |
| `281f_062c` | `2b5a_3b68` | Human Move/View Pieces | — |
| `281f_0676` | `4d56_1b3a` | Mid-turn Indian tables | [`mid_pass_indian_rank.md`](../original_sources_annotated/turn/mid_pass_indian_rank.md) |
| `281f_0550` | `5bfb_00f8` | Rank Euro nations | same |
| `281f_061e` | `3844_0442` | Year-end chrome | [`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md) |
| `281f_0668` | `43f7_2244` | Human merc offer | — |
| `291f_0a66` | `43f7_2424` | SoL / king (inside `00f2`) | ai/king_ref |
| `291f_0950` | `364b_0688` | Colony production | [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) |
| `291f_0a90` | `38fd_5e52` | Europe nation EOT | [`europe_nation_eot.md`](../original_sources_annotated/turn/europe_nation_eot.md) |
| `291f_0cbc` | `38fd_0058` | Market dynamics | same |
| `291f_0a74` / `0a9e` | `4962_0018` / `0606` | Census / profession tally | [`census_tally.md`](../original_sources_annotated/turn/census_tally.md) |
| `291f_0a82` | `48d3_06ba` | Europe-exit / tax treasures | [`europe_exit_landfall.md`](../original_sources_annotated/turn/europe_exit_landfall.md) |
| `291f_0a58` | `3844_0004` | Treasure tick | nation_eot.c |

---

## Phase correspondence

| Linux step | DOS site | Linux symbol | Fidelity |
|------------|----------|--------------|----------|
| Human already done | `2b5a_3b68` inside `130d` | (prior frame) | Reshape — intentional |
| SETUP calendar | `130d` post-nation tick | `turn_advance_calendar` | **Done** (`@TIMECHANGE`) |
| SETUP production | `364b_0688` in `00f2` | `turn_run_colony_production` | **Partial** — birth/starve **Done**; AI dump-sell **Done** thin; education F–G+no-students **Done** thin; K craft+raw+`5384` gates **Done** thin; map [`colony_eot_production.md`](../original_sources_annotated/turn/colony_eot_production.md) |
| SETUP fort fire | `364b_03f6` | `turn_run_coastal_fort_fire` | **Done** |
| SETUP nation ticks | Europe/census crumbs | `turn_run_nation_ticks` | **Partial** — human+AI bells/crosses accrue; [`europe_nation_eot.md`](../original_sources_annotated/turn/europe_nation_eot.md), [`census_tally.md`](../original_sources_annotated/turn/census_tally.md), [`nation_ticks_bells_ff.md`](../original_sources_annotated/turn/nation_ticks_bells_ff.md) |
| EURO treasure | `3844_0004` | `units_tick_treasure_outside_colony` | **Partial** |
| EURO ship-build ready | `00f2` unit walk | `units_tick_ship_build_ready` | **Partial** — progress/clear construction bit7 + threshold=`defense`/`0x5235` **Done** thin; dialog chrome PARKED |
| EURO Drydock repair | colony EOT | `units_tick_drydock_repair` | **Done** — clears combat-damage bit7 on finished ships at own Drydock; `@REFIT` chrome Done thin; after ship-build tick |
| EURO fog reveal | `281f_07a0` in `00f2` | `turn_reveal_fog_for_nation` | **Done** — every Euro nation's unit walk (human at FINISH, AI at its EURO step), full `13f1_0158` side effects via `units_reveal_sight`; colony radius-2 invention removed (DOS reveals ±5 only at founding) |
| EURO AI | `521d_6d8e` after `00f2` | `ai_euro_nation_turn` | **Partial structural** (T2 early; deep `20e6` mapped/PARKED) |
| INDIAN | `4d56_1b3a` mid-pass → `1816(slot)` ×8 | `ai_indian_nation_turn` | **Partial structural** — [`mid_pass_indian_rank.md`](../original_sources_annotated/turn/mid_pass_indian_rank.md); Euro rank `5bfb_00f8` **Done** thin |
| FINISH king | `43f7_2424` in `00f2` | `ai_king_nation_turn` | **Partial structural** |
| FINISH market | `38fd_0058` / `5e52` family | `europe_tick_market_prices` | **Partial** — attrition + colony→`price_group` half + phases 2–3 pressure **Done** thin |
| FINISH human refresh | return to Move Pieces | MP + select next | **Done** |
| FINISH autosave | `130d_0172` | autosave flags | **Done** |
| Year-end chrome | `3844_0442` | `turn_run_year_end_chrome` | **Partial** — B/C1(+fleet+REF+map+latch)/C2/D(+auto-declare)/E(+richest) **Done** thin; HoF **PARKED** |
| Demo autoplay | `130d` `0x828` tail | — | **PARKED** |

### Known reshape (do not paper over)

| Concern | DOS | Linux |
|---------|-----|-------|
| Human slot | Inside nation loop | Pipeline is **post-human only** |
| Calendar | After nations | **First** in SETUP |
| King | Per-nation inside `00f2` | Once in **FINISH** |
| Indians | Mid-pass `1b3a` → `1816` ×8, **before** the EN..DU loop — but the human's Move Pieces sits *inside* that loop | Pipeline starts after the human: **EURO** slots above the human → **INDIAN** → EURO slots below (2026-08-28; no human slot → Indians first) |
| `00f2` | Atomic per Euro | Split across SETUP / EURO / FINISH |

Manual “natives first” order is **not** what either DOS `130d` (as resolved) or
Linux runs. Relative to the human's end of turn the DOS order is: Euro slots
above the human, then the mid-pass Indian turns, then the slots below the
human (seed-100, human = England: FR → SP → DU → Indians → EN). Linux
`turn_processor_advance` splits `TURN_PROC_EURO` around `TURN_PROC_INDIAN`
the same way (2026-08-28; the 2026-08-27 "Indians before every Euro" order
broke the Dutch TURN2→3 first contact).

---

## Planner pointers

| Cluster | Doc / extract |
|---------|----------------|
| Euro goals / act / scoring | [ai_transcription.md](ai_transcription.md); [`euro_dispatcher.c`](../original_sources_annotated/ai/euro_dispatcher.c); [`euro_unit_act.md`](../original_sources_annotated/ai/euro_unit_act.md); [`move_scoring.md`](../original_sources_annotated/ai/move_scoring.md) |
| Indian pulse / contact | [`indian_nation_turn.c`](../original_sources_annotated/ai/indian_nation_turn.c); [`indian_contact.md`](../original_sources_annotated/ai/indian_contact.md); [`brave_spent_callgraph.md`](../original_sources_annotated/ai/brave_spent_callgraph.md) |
| King / REF | [`king_ref.md`](../original_sources_annotated/ai/king_ref.md) |
| Diplomacy | [`euro_diplo.md`](../original_sources_annotated/ai/euro_diplo.md) |

---

## Open RE

- **`MULTINEXT` / `TIMECHANGE` / `SEASONS`**: string table only — **reconfirmed**
  no FUN_* XREF ([`mid_pass_indian_rank.md`](../original_sources_annotated/turn/mid_pass_indian_rank.md)).
  Calendar from `@TIMECHANGE` + `130d` year/autumn math.
- **`FUN_4d56_1816`**: **caller resolved 2026-08-27** (static) — `4d56_1b3a`
  phase 2 via overlay-local stub `4d56:4c2c` → record `281f:23b0`. DOS runs
  all Indian turns in the mid-pass before Euro 0..3; Linux INDIAN phase
  reordered before EURO to match (2026-08-27). Map:
  [`mid_pass_indian_rank.md`](../original_sources_annotated/turn/mid_pass_indian_rank.md).
- **`FUN_3844_0442`**: **UI mapped**
  ([`year_end_chrome.md`](../original_sources_annotated/turn/year_end_chrome.md));
  B/C1(+fleet+REF pool+map+latch)/C2/D(+auto-declare)/E(+richest) status
  **Done** thin; HoF dialogs still PARKED.
- **Demo / independence splash** (`130d_019e` / `0222`): PARKED; thin LAB in
  [`year_loop.c`](../original_sources_annotated/turn/year_loop.c).
- **Deep AI** (`20e6` / `2820` / `4528` / `2154` / `5fef` loot): section-mapped
  under `original_sources_annotated/ai/`; Linux port still PARKED (unpark #4).
- **Nation EOT arms**: ship-build ready (`defense`/`0x5235` combat) + §C Merc
  (+`48d3_0002` duration) **Done** thin; live census colony+unit tallies
  **Done** thin; AI fog reveal **Done** (2026-08-29).
- **Deep-map pass (docs):** `0688` F–H/K/O–P, `5e52`/`0058` interiors,
  [`coastal_fort_fire.md`](../original_sources_annotated/turn/coastal_fort_fire.md)
  ([combat.md](combat.md) player-facing summary),
  [`nation_ticks_bells_ff.md`](../original_sources_annotated/turn/nation_ticks_bells_ff.md),
  [`europe_finish_bridge.md`](../original_sources_annotated/turn/europe_finish_bridge.md).
- **Unparked this session (port):** C1 REF pool + calendar_latch + `53c2`;
  century tip `tut3.nr6`; ship-build `0x5235`=combat; live census tallies;
  `5384` report gates; education no-students + tier name; fort-fire status;
  immigrant open_on_dock; spoilage cargo name; §C landfall duration.

---

## Tests / evidence

| Gate | Path |
|------|------|
| Calendar / production unit | `tests/unit/test_turn.c` |
| Early AI T2 | `golden_ai_turns` (`TURN1`…`TURN7`) |
| King thin | `unit_ai_king` |
| Saves / autosave fields | [savegame.md](savegame.md) |
