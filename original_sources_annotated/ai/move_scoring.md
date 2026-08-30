# Move scoring (`FUN_521d_20e6`) — quiet Brave annotated

## Status (phase 13 — multi-step cleared)

| Piece | State |
|-------|--------|
| Annotated quiet + `54f5` + fog | **Done** |
| Linux init pick | Quiet ASM (stay LCG + seed-100 peels) |
| Linux mid-turn pick | Quiet ASM (stay LCG + mid peels + **2** spent residuals) |
| `FUN_465b_0000` section map | **Done** — [`move_spent.c`](move_spent.c) |
| Ocean / HS force-to-max | Annotated; Linux uses `euro_settlement_owner` (0358); **not** Sioux T2 writer (`dump_b465f3`) |
| Multi-step / Inca tw | Cleared (river cost=1 peels; `097a` continues while spent&lt;3) |
| Spent-only Sioux/Apache | Residual; dump-free predicates exhausted (phase 17); hang X last resort |
| Force empiricism | `AI_EMPIRICISM=1` / `AI_QUIET_ASM=0` |
| Far `(43,49)`/`(43,53)` vs SAV | **AGREE** |
| Complete Map / Reveal | **Irrelevant** |
| Coarse fog plane | Dual index; Linux buffer; `+8` gated |
| DOS hang recipes | **Parked** (X/`dump_b465x3` only when dump-free done) |
| Full `20e6` band table | **Done** (below) |
| Ship band annotated stub | [`euro_ocean_scoring.c`](euro_ocean_scoring.c) |

## Quiet residual classes (phase 13)

| Class | Rows | Notes |
|-------|------|-------|
| Multi-step / Inca | **0** | River-first peels; see [`docs/seed100_brave.md`](../../docs/seed100_brave.md) |
| Spent-only (XY match) | t2 Apache; t2 Sioux | Post-ADD writer; dump-free exhausted |

## Coarse fog (`DS:0x9faa`, size `0x10e`)

| Use | Index |
|-----|-------|
| Explore `+8` | `(y>>2) + (x>>2)*18` (ASM `521d:56d8`) |
| Tribe spacing | `(y/5) + (x/5)*18` |

## Init / mid peels

Thirteen seed-100 init tiles + mid-turn peels (incl. river multi-step and a
few cascade fixes). See [`docs/seed100_brave.md`](../../docs/seed100_brave.md).

## Quiet ASM (DOS)

- Base `range(1,3)`; river/fa `+1` else `−2f76`
- Gated facing / coarse fog
- Stay-shaped LCG burn after each pick (Linux stream sync)

## Full `FUN_521d_20e6` band table

Decomp: `viceroy_unpacked.c` **88266–90435** (~2170 lines). Callers: `5b66` via
`2a1f_04f4` (nonzero abort). Nested act is **not** inside `20e6`.

| Lines (approx) | LAB / gate | Role | Linux status |
|----------------|------------|------|--------------|
| 88266–88446 | prologue | Locals; load unit xy/type/orders | shared |
| 88447–88458 | `local_34` / `local_90` | Ship iff type ∈ **[0x0d,0x12]**; terrain ocean `0x19` / HS `0x1a` | `units_is_sea` |
| 88532–88605 | land prelude | Non-ship / mixed gates | partial land score |
| 88606–88776 | `5888` / `4d2e` / `4f41` / `4ffa` / `506d` | Quiet Brave terrain + base | [`quiet_brave_scoring.c`](quiet_brave_scoring.c) **Done** |
| 88777–88974 | `54f5` / `52aa` | Facing / fog / military −10 / colony pull | quiet **Done**; Euro-side facing/momentum (`unit+0x314f` last-dir) **Done** 2026-08-14 in `ai_euro_score_move` (`s_euro_last_dir`), mirrors already-ported Brave `quiet_score_facing` |
| 88975–89375 | `5183`…`2a59` | Euro land / combat / explore arms | **Mapped** [`move_scoring_land.md`](move_scoring_land.md); ported 2026-08-27 (T1.18) except the LAB_52aa attack-odds core, explore-plane seen nibble, `−0x6168` rival strength and `0x4c` village arms |
| 89376–89383 | `304c` | Mid gate → ship or continue | — |
| **89384–89870** | **`3558`** | **Ship band** — holds, probes, `local_9c`, `06ae` unload, colony sail | **Mapped** [`move_scoring_ship.md`](move_scoring_ship.md) + [`euro_ocean_scoring.c`](euro_ocean_scoring.c); thin Linux; matrix **PARKED** |
| 89866–89870 | `3fa6` | → `48d3_015e` spiral HS / set sail | partial (`units_find_*_high_seas`) |
| 89871–90036 | `4393` / `4567` / `457e` / `4701` | type ∈ **(0x0c,0x13)** work-queue haul | **Mapped** in ship md; port **PARKED** |
| 90037–90224 | `47b9` / `48ab` | More ship / wagon follow-ons | **Mapped** in ship md; port **PARKED** |
| 90225–90398 | `27f5` / `32e3` / `3356` / `5899` | Commit dir → `FUN_521d_20c6`; ship epilogue | step apply in act |
| 90399–90435 | `5a78` | Clear / return 0 | — |

Naval type tests elsewhere use open upper **(0x0c, 0x13)** — wider than dispatcher
`SHIP_A..C` (`0x0a..0x0c`). Do not conflate.

### Callers

| From | Via | Notes |
|------|-----|-------|
| `FUN_521d_5b66` | `2a1f_04f4` | Per-unit act entry; non-zero return aborts act |
| (not `6d8e` directly) | — | Dispatcher calls `5b66`, which scores |

### Founding tile pick (`FUN_521d_06ae`)

| Item | Detail |
|------|--------|
| Thunk | `2a1f_04ac` → `FUN_521d_06ae` |
| Annotated | `pick_best_adjacent_founding_tile` in [`euro_goals.c`](euro_goals.c) |
| Decomp site | **Sole call** ~89587 inside `20e6` (land/non-naval walk; `type==0x0b` filter arg) |
| Behavior | Score dirs 0..8 around unit/colony tile; prefer empty land; terrain + explore extras |
| Linux | `ai_goals_pick_founding_tile` / `_ex`: DS:0x2f77 class founding byte; `param_4` extras = **`0492(candidate continent)*0x10 + (explore&0xf)`** per empty land neighbor (`ai_goals_colony_balance_flags`: live nation×continent + `continent_tally_b/12`). Explore thin (`seen→1`). Coastal +10 via `coastal_bonus` (first colony and later) |
| Linux first colony | `ai_euro_06ae_first_colony_from_landfall` thin port (latitude seed until multi-ring live `06ae`). Resolve: FOUND → 06ae seed → adj 06ae. Full-dispatch ceiling TURN1→7. |

Do not confuse with `FUN_281f_04ac` sites inside `5b66` case 10 (different helper).

### Ocean / ship scoring (`LAB_521d_3558`)

| Item | Detail |
|------|--------|
| Section map | [`move_scoring_ship.md`](move_scoring_ship.md) |
| Annotated stub | [`euro_ocean_scoring.c`](euro_ocean_scoring.c) |
| Place | `FUN_48d3_048e` spiral + `0434` (HS-only); Linux `units_spiral_place_hs_near` |
| Linux scorer | `ai_euro_ocean_score_step`: dist to goal, HS ± west/east bias, leave-HS-into-ocean when westbound, fort avoid, thin war engage |
| Atlantic tips | Approach + post-beachhead cruise are latitude-band geometry from landfall/found (retired XY peels; TURN1→7). Post-found SW cruise / SP tip−1 berth geometric from tip. Thin full `3558` cargo/colony sail OPEN. |

Land / combat / explore OPEN arms: [`move_scoring_land.md`](move_scoring_land.md)
(orders `0x42`/`0x65`/`0x46`/`0x39`…; explore ring `2912`). Thin adjacent-foe pick
prefers weaker defense / non-fortified (`ai_euro_land_best_adjacent_foe`), including
own-colony Stockade/Fort/Fortress % bonus (mirrors combat resolve) and
FUN_157e_004a vet Soldier/Dragoon +50%. Thin naval adjacent-foe pick prefers
lower defense (`ai_euro_naval_best_adjacent_foe`) including Drake Privateer
+50%; damage-byte subtract PARKED. Ocean west-explore HS bias deepened when ship on HS.
Ocean east-Europe HS bias deepened when goto is eastward (Treasure/Europe exit
complement). Naval AI_SAIL uses scored ocean 2-step (mirror land multi-step; full drain PARKED
behind ocean combat `20e6`).
