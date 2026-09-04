# Between player turns — thin callgraph

Layer D hygiene for the year loop / nation EOT / year-end chrome. Full bridge
(Linux `TURN_PROC_*` ↔ DOS reshape):
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

Annotated extracts:

| Piece | File |
|-------|------|
| `FUN_130d_0290` (+ autosave/splash) | [`year_loop.c`](year_loop.c) |
| `FUN_3844_00f2` + `0004` | [`nation_eot.c`](nation_eot.c) |
| Ship-ready + immigrant spawn arms | [`nation_eot_ship_spawn.md`](nation_eot_ship_spawn.md) |
| `FUN_364b_0688` production | [`colony_eot_production.md`](colony_eot_production.md) |
| `FUN_364b_03f6` coastal fort fire | [`coastal_fort_fire.md`](coastal_fort_fire.md) |
| `FUN_38fd_5e52` / `0058` | [`europe_nation_eot.md`](europe_nation_eot.md) |
| `FUN_4962_0018` / `0606` | [`census_tally.md`](census_tally.md) |
| `FUN_48d3_06ba` | [`europe_exit_landfall.md`](europe_exit_landfall.md) |
| `48d3` helpers + `38fd_55b6` | [`europe_finish_bridge.md`](europe_finish_bridge.md) |
| `FUN_4345_0a22` bells / FF | [`nation_ticks_bells_ff.md`](nation_ticks_bells_ff.md) |
| Mid-pass `1b3a` + rank `5bfb_00f8` + `1816` XREF | [`mid_pass_indian_rank.md`](mid_pass_indian_rank.md) |
| `FUN_3844_0442` | [`year_end_chrome.c`](year_end_chrome.c) · [`year_end_chrome.md`](year_end_chrome.md) |

## DOS end-to-end

```
FUN_130d_0290 year_turn_loop          [thunk 281f_0546]
  ├─ boot: menu bar / turn-owner chrome / minimap
  └─ do {
       mid (if !0x829):
         clear all moves_spent (0x3149)
         rank euros          281f_0550 → 5bfb_00f8
         mid_turn_indian     281f_0676 → 4d56_1b3a
           ├─ clear 0x5b04 tables
           ├─ for slot 0..7: if !(tribe_flags[slot] & 0x80):
           │    4d56_1816(slot)   via stub 4d56:4c2c → record 281f:23b0
           └─ colony ring ownership stamps (281f_0704)
       for nation 0..3:
         DS:0x5394 = nation
         if control != withdrawn:
           nation_eot        281f_0644 → 3844_00f2
         if control == AI:
           euro_nation_turn  281f_0638 → 521d_6d8e
         if control == human:
           merc offer        281f_0668 → 43f7_2244
           Move Pieces       281f_062c → 2b5a_3b68
       calendar tick (year / autumn @TIMECHANGE shape)
       year_end_chrome       281f_061e → 3844_0442
     } while DS:0x53c2
```

### `FUN_3844_00f2` nation_eot

```
nation_eot (active = DS:0x5394)
  ├─ turn_owner_chrome       281f_0590 (@COUNTRY color)
  ├─ per-unit (this nation):
  │    reveal fog            281f_07a0 → 13f1_02f8
  │    treasure tick         291f_0a58 → 3844_0004
  │    ship-build ready chrome (types 0xd..0x12)
  ├─ tally professions       291f_0a9e → 4962_0606
  ├─ Europe nation EOT       291f_0a90 → 38fd_5e52
  ├─ Europe-exit / tax       291f_0a82 → 48d3_06ba
  ├─ optional Europe screen  281f_05fa → 38fd_55b6
  ├─ colony production       291f_0950 → 364b_0688
  ├─ census                  291f_0a74 → 4962_0018
  ├─ SoL / king              291f_0a66 → 43f7_2424
  └─ rare immigrant/ship spawn (turn&7==0, peacetime)
```

### `FUN_3844_0442` year_end_chrome

```
year_end_chrome
  ├─ census human (+ crown if war)
  ├─ no colonies / year≥1600 / peacetime → defeat (LAB_3844_04ec)
  ├─ wartime: victory / peace-offer thresholds (LAB_3844_0b4a)
  ├─ peacetime rivals: SoL pressure / auto-declare
  ├─ anniversary years (0x6fe/0x730, 0x708/0x73a)
  └─ epilogue: HoF / continue dialog / OR 0x5382 bit4
```

Full string / subst / threshold table: [`year_end_chrome.md`](year_end_chrome.md)
(port still PARKED).

## Linux column (batch-after-human)

```
game_do_end_turn
  └─ turn_processor_advance loop:
       TURN_PROC_SETUP
         turn_advance_calendar
         turn_run_colony_production          (~364b_0688, AI nations only)
         turn_run_coastal_fort_fire          (364b_03f6)
         turn_run_nation_ticks               (bells/crosses/FF)
       TURN_PROC_INDIAN (nations 4..11)     — DOS mid-pass order
         ai_indian_nation_turn               (~4d56_1816-shaped)
       TURN_PROC_EURO (one nation / frame)
         MP refresh + treasure tick          (~3844_0004)
         ai_euro_nation_turn                 (~521d_6d8e)
       TURN_PROC_FINISH
         turn_run_colony_production          (~364b_0688, human nation)
         ai_king_nation_turn                 (~43f7_2424; DOS was inside 00f2)
         europe_tick_market_prices           (~38fd_0058; sibling of 5e52)
         human MP + treasure + Cortes
         turn_select_next_unit
         autosave flags                      (~130d_0172)
  └─ game_finish_end_turn
       apply autosave / Europe arrivals / camera
```

## Reshape (do not “fix” docs to match)

| Piece | DOS | Linux |
|-------|-----|-------|
| Human Move Pieces | Inside `130d` nation loop | Already done; pipeline is post-human |
| Calendar | After nation pass | First in SETUP |
| `3844_00f2` | Per Euro before act | Split across SETUP / EURO / FINISH; the **human's** `364b_0688` half runs in FINISH (bugs.md 385) |
| `43f7_2424` | Inside `00f2` | FINISH `ai_king_nation_turn` |
| Indians | Mid-pass `4d56_1b3a` → `1816(slot)` ×8 **before** Euro loop | Full `1816`-shaped turns 4..11 in INDIAN phase, **before** EURO (reordered 2026-08-27) |
| `3844_0442` | Every year tick | B/C1(+REF pool)/C2/D/E status **Done** thin; HoF PARKED |

## Pointers (planner guts — do not duplicate)

| Topic | Map |
|-------|-----|
| Euro dispatcher | [`ai/euro_dispatcher.c`](../ai/euro_dispatcher.c) |
| Euro unit act / scoring | [`ai/euro_unit_act.md`](../ai/euro_unit_act.md), [`ai/move_scoring.md`](../ai/move_scoring.md) |
| Indian nation turn | [`ai/indian_nation_turn.c`](../ai/indian_nation_turn.c) |
| Brave spent | [`ai/brave_spent_callgraph.md`](../ai/brave_spent_callgraph.md) |
| King / REF | [`ai/king_ref.md`](../ai/king_ref.md) |
| AI fidelity tiers | [`docs/ai_transcription.md`](../../docs/ai_transcription.md) |

## Orchestration callees (mapped)

| DOS body | Map | Depth / port |
|----------|-----|--------------|
| `364b_0688` | [`colony_eot_production.md`](colony_eot_production.md) | **Deepened** F–H/K/O–P + B/C/D; birth/starve **Done**; AI dump-sell **Done** thin; education F–G **Done** thin |
| `364b_03f6` | [`coastal_fort_fire.md`](coastal_fort_fire.md) | Phase map; Linux SETUP reshape; pulse **Done** thin |
| `38fd_5e52` / `0058` | [`europe_nation_eot.md`](europe_nation_eot.md) | **Deepened** 5e52§4–6 + 0058§1–3; market half + phases 2–3 **Done** thin |
| `4962_0018` / `0606` | [`census_tally.md`](census_tally.md) | Phase map; blank census partial; live colony+unit tallies **Done** thin; profession tally **Done** thin |
| `48d3_06ba` | [`europe_exit_landfall.md`](europe_exit_landfall.md) | Phase map; treasure tax cap **Done** |
| `48d3_03d0`/`0002`/`064e` + `55b6` | [`europe_finish_bridge.md`](europe_finish_bridge.md) | Helpers + Europe UI gate; bound deliver reshape |
| `4345_0a22` | [`nation_ticks_bells_ff.md`](nation_ticks_bells_ff.md) | Accrue+elect; Linux ticks Partial |
| `5bfb_00f8` / `4d56_1b3a` | [`mid_pass_indian_rank.md`](mid_pass_indian_rank.md) | Rank **Done** thin (`turn_rank_euro_nations`); Indians reshape |
| Ship-ready / spawn | [`nation_eot_ship_spawn.md`](nation_eot_ship_spawn.md) | Ready + §C Merc **Done** thin |
| `3844_0442` | Every year tick | B/C1(+REF pool)/C2/D/E status **Done** thin; HoF PARKED |

Planner guts (not EOT orchestration, but contact siblings):

| Body | Map |
|------|-----|
| `4d56_2154` meet economics | [`../ai/indian_meet_scoring_2154.md`](../ai/indian_meet_scoring_2154.md) |
| `5fef_0f14` / `016c` raid loot | [`../ai/indian_raid_loot.md`](../ai/indian_raid_loot.md) |

## Open RE

- `MULTINEXT` / `TIMECHANGE` / `SEASONS`: string table only — **reconfirmed** no FUN_* XREF ([`mid_pass_indian_rank.md`](mid_pass_indian_rank.md))
- `FUN_4d56_1816`: **resolved 2026-08-27** — called from `4d56_1b3a` phase 2
  (`PUSH CS; CALL 4c2c` stub → record `281f:23b0`), i.e. `130d → 0676 → 1b3a
  → 1816(slot)` for each Indian slot with tribe flag bit7 clear. Ghidra's
  `FUN_41f2_0266` label on that call is a misresolve.
  See [`mid_pass_indian_rank.md`](mid_pass_indian_rank.md)
- Demo autoplay / independence splash (`130d_019e` / `0222`): PARKED; thin LAB in [`year_loop.c`](year_loop.c)
- `FUN_3844_0442` dialogs: **UI mapped** in [`year_end_chrome.md`](year_end_chrome.md); port PARKED
- Deep AI bodies (`20e6` land/ship, `2820`, `4528`, `2154`, loot): **mapped**; port PARKED — see
  [`move_scoring_land.md`](../ai/move_scoring_land.md),
  [`move_scoring_ship.md`](../ai/move_scoring_ship.md),
  [`indian_trade_2820.md`](../ai/indian_trade_2820.md),
  [`indian_settlement_4528.md`](../ai/indian_settlement_4528.md),
  [`indian_meet_scoring_2154.md`](../ai/indian_meet_scoring_2154.md),
  [`indian_raid_loot.md`](../ai/indian_raid_loot.md)
