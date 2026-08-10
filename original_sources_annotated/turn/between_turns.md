# Between player turns — thin callgraph

Layer D hygiene for the year loop / nation EOT / year-end chrome. Full bridge
(Linux `TURN_PROC_*` ↔ DOS reshape):
[`docs/turn_between_players.md`](../../docs/turn_between_players.md).

Annotated extracts:

| Piece | File |
|-------|------|
| `FUN_130d_0290` (+ autosave/splash) | [`year_loop.c`](year_loop.c) |
| `FUN_3844_00f2` + `0004` | [`nation_eot.c`](nation_eot.c) |
| `FUN_3844_0442` | [`year_end_chrome.c`](year_end_chrome.c) |

## DOS end-to-end

```
FUN_130d_0290 year_turn_loop          [thunk 281f_0546]
  ├─ boot: menu bar / turn-owner chrome / minimap
  └─ do {
       mid (if !0x829):
         clear all moves_spent (0x3149)
         rank euros          281f_0550 → 5bfb_00f8
         mid_turn_indian     281f_0676 → 4d56_1b3a   (NOT full 1816)
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
         turn_run_colony_production          (~364b_0688 all colonies)
         turn_run_coastal_fort_fire          (364b_03f6)
         turn_run_nation_ticks               (bells/crosses/FF)
       TURN_PROC_EURO (one nation / frame)
         MP refresh + treasure tick          (~3844_0004)
         ai_euro_nation_turn                 (~521d_6d8e)
       TURN_PROC_INDIAN (nations 4..11)
         ai_indian_nation_turn               (~4d56_1816-shaped)
       TURN_PROC_FINISH
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
| `3844_00f2` | Per Euro before act | Split across SETUP / EURO / FINISH |
| `43f7_2424` | Inside `00f2` | FINISH `ai_king_nation_turn` |
| Indians | Mid-pass `4d56_1b3a` only | Full `1816`-shaped turns 4..11 |
| `3844_0442` | Every year tick | Mostly PARKED (king/war thin elsewhere) |

## Pointers (planner guts — do not duplicate)

| Topic | Map |
|-------|-----|
| Euro dispatcher | [`ai/euro_dispatcher.c`](../ai/euro_dispatcher.c) |
| Euro unit act / scoring | [`ai/euro_unit_act.md`](../ai/euro_unit_act.md), [`ai/move_scoring.md`](../ai/move_scoring.md) |
| Indian nation turn | [`ai/indian_nation_turn.c`](../ai/indian_nation_turn.c) |
| Brave spent | [`ai/brave_spent_callgraph.md`](../ai/brave_spent_callgraph.md) |
| King / REF | [`ai/king_ref.md`](../ai/king_ref.md) |
| AI fidelity tiers | [`docs/ai_transcription.md`](../../docs/ai_transcription.md) |

## Open RE

- `MULTINEXT` / `TIMECHANGE` / `SEASONS` strings: table only, no FUN_* XREF yet
- `FUN_4d56_1816` call site unresolved in export (overlay/thunk gap); Linux still runs `1816`-shaped nation turns
- Demo autoplay tail in `130d` (`0x828`): PARKED
- `FUN_3844_0442` dialogs: **UI mapped** in [`year_end_chrome.md`](year_end_chrome.md); port PARKED
- Deep AI bodies (`20e6` land/ship, `2820`, `4528`): **mapped**; port PARKED — see
  [`move_scoring_land.md`](../ai/move_scoring_land.md),
  [`move_scoring_ship.md`](../ai/move_scoring_ship.md),
  [`indian_trade_2820.md`](../ai/indian_trade_2820.md),
  [`indian_settlement_4528.md`](../ai/indian_settlement_4528.md)
