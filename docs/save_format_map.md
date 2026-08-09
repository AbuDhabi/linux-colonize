# Col1 save format map (roadmap + field atlas)

Living inventory of every `COLONY##.SAV` region and the path to a
**decomp-backed** field map. Codec layout, byte-identical RMW, P0–P5 naming,
and P6 template export rebuild (density / blank census / colony levels) are
done; unread late `unknown_ds_*` stay zero-on-template by cite.

Companion: [savegame.md](savegame.md) (interop / bridge / gaps summary).
Structs: [`src/core/col1_save.h`](../src/core/col1_save.h).

---

## Goal / non-goals

**Goal:** name every byte (or bit) with evidence — DOS reader/writer cite and
allowed value ranges — so Linux→DOS export can rebuild templates without
inventing blobs.

**Non-goals:**

- Changing packed section sizes or breaking byte-identical fixture RMW
- Inventing post_map / stuff contents without decomp proof
- Treating community names (smcol / Format.md) as proven until matched to
  VICEROY DS / `FUN_*` (cite them as `community`, then promote)

**Standing project bar:** 100% DOS↔port save interop
([project_goals.md](project_goals.md)). This document is the RE track for that.

---

## Status legend

| Status | Meaning |
|--------|---------|
| `mapped` | Named in port; used or clearly documented; values understood |
| `partial` | Named or partly decoded in port; holes or stand-ins remain |
| `community` | Named in smcol / Format.md / viceroy; **not yet** proven here |
| `opaque` | Preserved as bytes; no solid semantics in-repo |
| `misaligned` | Port comment/split conflicts with decomp or community layout |

Rough fixture ratio (COLONY00-sized): map planes dominate structured bytes.
Post-P5, remaining `opaque`/`community` are mostly **closed** save-only pads
(`other`, head pads, `unknown_ds_*`) rather than unnamed mystery regions.

---

## DOS anchors (start here)

| Role | Symbol | Notes |
|------|--------|-------|
| Write save | `FUN_75c2_0288` | Canonical section dump; ~33 discrete stuff writes; post-map DS blobs |
| Load save | `FUN_75c2_0940` | Mirror of 0288 |
| Header probe | `FUN_75c2_0840` | Sig / version **73** / map product — ported as `col1_save_validate_head` |
| Slot UI / paths | `FUN_7562_*` | COLONY## paths, autosave 8/9 |
| Connectivity fill | `FUN_67f4_0088` | Builds 2×`0x10e` planes → saved after map layers |
| Live units | DS:`0x3144` stride `0x1c` | Save dumps the array wholesale |

Catalog: [FUNCTION_CATALOG.md](../original_sources_annotated/FUNCTION_CATALOG.md)
(seg `75c2` / `7562`). Bodies: `original_sources_decompiled/viceroy_unpacked.c`.

**Community oracles** (cite → prove):
[smcol_sav_struct.json](https://github.com/pavelbel/smcol_saves_utility),
[Format.md](https://github.com/hegemogy/Colonization-SAV-files),
[viceroy savegame.h](https://github.com/hegemogy/viceroy).

**Fixtures:** `original_saves/COLONY00.SAV`, `COLONY01.SAV`,
`original_saves/valid-lategame-saves/COLONY{00–08,10}.SAV`,
`test-saves-ai/TURN*`, `test-saves-mapgen/SEED100.SAV`,
`tests-save-misc/unit flags error.sav`.

---

## File order (standard 58×72)

| Section | Size | Port type |
|---------|------|-----------|
| Prefix (`head` + `player[4]` + `other`) | 390 | — |
| `colony[]` | 202 × C | `ColonizeCol1Colony` |
| `unit[]` | 28 × U | `ColonizeCol1Unit` |
| `nation[4]` | 1264 | `ColonizeCol1Nation` |
| `tribe[]` | 18 × T | `ColonizeCol1Tribe` |
| `indian[8]` | 624 | `ColonizeCol1Indian` |
| `stuff` | 727 | `ColonizeCol1Stuff` (33 DOS DS writes) |
| Map ×4 (tile/mask/path/seen) | 4 × W × H | bitfield structs |
| `post_map` | **614** | `ColonizeCol1PostMap` (was unknown_e+f) |
| `trade_route[12]` | 888 | `ColonizeCol1TradeRoute` |

---

## Field atlas

Offsets are **within the named struct** unless noted. Update status in place
as peels land.

### Head (158)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `sig_colonize` / `sig_eof` / `save_version` | 9+1+2 | `mapped` | `COLONIZE\0` + `0x1A`; ver **73** |
| `map_size_x` / `map_size_y` | 4 | `mapped` | Typically 58×72 |
| `tut1.*` known bits | — | `mapped` | Tutorial flags |
| `tut1.unknown01` / `unknown02` | 2 bits | `opaque` | No distinct cite — closed as tut pad |
| `unknown03` | 1 | `opaque` | Save R/W; no gameplay cite |
| `game_options.woi`…`ref_unit_threshold` | 7 bits | `mapped` | DS:`0x5382` WoI/REF latches (was `unused01`) |
| `game_options` (hints…moves) | — | `mapped` | |
| `game_options.cheats_enabled` | 1 bit | `mapped` | DS:`0x5383` bit5; Alt-WIN |
| `colony_report_options` | — | `mapped` | `unused03` pad |
| `tut2` / `tut3` | — | `mapped` | `unused04` pad |
| `unknown39` | 2 | `opaque` | Save R/W; no gameplay cite |
| `year` / `autumn` / `turn` | 6 | `mapped` | |
| `map_mode` | 2 | `mapped` | DS:`0x5390`; 0=Move / 1=View |
| `active_unit` | 2 | `mapped` | |
| `nation_turn` / `curr_nation_map_view` / `human_player` | 6 | `mapped` | DS:`0x5394`..`0x5398` |
| `tribe_count` / `unit_count` / `colony_count` | 6 | `mapped` | |
| `trade_route_count` / `show_entire_map` / `fixed_nation_map_view` | 6 | `mapped` | DS:`0x53a0`.. |
| `difficulty` | 1 | `mapped` | 0..4 |
| `unknown43` | 2 | `opaque` | Save R/W; no gameplay cite |
| `founding_father[25]` | 25 | `mapped` | −1 = unrecruited |
| `turn_loop_running` / `map_modal_active` / `no_unit_selected` | 6 | `mapped` | DS:`0x53c2`/`c4`/`c6` |
| `nation_relation[4]` | 8 | `mapped` | |
| `rebel_sentiment_report` + `unknown45_pad[8]` | 10 | `mapped` | DS:`0x53d0` |
| `expeditionary_force` / `backup_force` | 16 | `mapped` | |
| `unknown46` / `price_group_state[16]` | 32 | `partial` | DOS prices @`0x53ea`; Linux king bytes 0–5 overlay words 0–2 |
| `event` | 2 | `mapped` | Woodcut / discovery flags |
| `unknown05` | 2 | `opaque` | Save R/W; no gameplay cite |

### Players (52 × 4)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `name` / `country_name` | 48 | `mapped` | |
| `unknown06_lo` / `named_new_world` | 1 | `mapped` | bit7 discovery one-shot (`FUN_4720_049e`) |
| `control` / `founded_colonies` / `diplomacy` | 3 | `mapped` | control 0/1/2 |

### Other (24) — prefix trailer

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `other[]` | **24** | `community` | DS:`0x948e`; save R/W only in unpacked; smcol unexplored + click xy |

### Colony (202 × C)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `x` / `y` / `name` / `nation_id` / `population` | — | `mapped` | |
| `ai_flags` (`ColonizeCol1ColonyAiFlags`) | 1 | `mapped` | +0x1b; ship/AI planner bits (`FUN_4962_0018` / `5952_035e`) |
| `flags` (`ColonizeCol1ColonyFlags`) | 1 | `mapped` | +0x1c; SoL/starvation/build-busy/… (`FUN_364b_0688`) |
| `build_ai_flags` | 1 | `partial` | +0x1d; bit7 `wants_construction`; other bits reserved |
| `garrison_quota` | 1 | `mapped` | +0x1e; `threat>>3` (`FUN_5952_035e`) |
| `occupation` / `profession` | 64 | `mapped` | |
| `specialty[16]` | 16 | `mapped` | +0x60; colonist specialty nibbles (`FUN_15eb_0c7a`; was `duration`) |
| `tiles[20]` | 20 | `mapped` | +0x70; ring `[0..7]`; `[8..19]` empty `0xff` in fixtures |
| `buildings` / `custom_house` | — | `mapped` | `unused05` pad |
| `improve_timer` | 1 | `mapped` | +0x8c; INC cap `0x7f`; gates pioneer |
| `specialty_cargo` | 1 | `mapped` | +0x8d; `0xff` none (`FUN_5952_0306`) |
| `labor_shortage` | 1 | `mapped` | +0x8e |
| `cargo_idle_turns` | 1 | `mapped` | +0x8f; cleared on unload; AI score `*8` |
| `cargo_produced_mask` | 2 | `mapped` | +0x90; bit per cargo (`FUN_364b_0688`) |
| `hammers` / `building_in_production` | — | `mapped` | |
| `warehouse_level` | 1 | `mapped` | +0x95; cap `100*(1+level)` (`FUN_15eb_0a50`) |
| `capitol_level` | 1 | `mapped` | +0x96; Capitol / Expansion (`0x1e`/`0x1f`) |
| `depletion_counter` | 1 | `mapped` | +0x97; wrap at 50 |
| `hammers_purchased` | 2 | `mapped` | +0x98; BUY remainder (`FUN_2f2b_5e44`) |
| `stock[16]` | 32 | `mapped` | |
| `visible_to_euro[4]` | 4 | `mapped` | +0xba; fog `0x10<<euro` |
| `unknown13_pad[4]` | 4 | `opaque` | +0xbe; found-zero; no reader |
| `rebel_dividend` / `rebel_divisor` | 8 | `mapped` | SoL display |

Export often **zeros** unnamed colony bytes on rebuild ([savegame.md](savegame.md)).

### Unit (28 × U) — live DS:`0x3144`

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `x` / `y` / `type` / `nation_id` | — | `mapped` | Europe sentinels ≥200 |
| `vis_mask` | 4 bits | `mapped` | euro owner `1<<n` (`FUN_1427_0992`); natives 0 on spawn/capture |
| `unknown15_lo` / `ship_damaged` | 1 | `partial` | bit7 damaged (`FUN_1427_13b0`); lo bits AI latches |
| `moves` / `orders` / `goto_*` | — | `mapped` | |
| `origin` | 1 | `mapped` | Brave home tribe |
| `ai_plan` | 1 | `mapped` | Default `0x58` / `'X'` |
| `facing` / `facing_pad` | 1 | `mapped` | Low 3 = last dir (`FUN_1427_0968`) |
| Cargo / profession / `turns_worked` / chain | — | `mapped` | |

### Nation (316 × 4)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `tax_rate` / `recruit*` / FF / bells / gold / crosses | — | `mapped` | |
| `nation_flags` | 1 | `partial` | Was `unknown19`; bits `0x04`/`0x08`/`0x40` live |
| `tax_hike_count` | 1 | `mapped` | Was `unused07`; `FUN_38fd_44a4` |
| `unknown21` | 1 | `opaque` | No reader cite |
| `unknown22` | 2 | `partial` | `int16`; `FUN_38fd_5be8` write; role thin |
| `ff_count_end_prob` | 2 | `community` | smcol; cleared on independence; no FF-prob reader |
| `rebel_sentiment` + `unknown23_pad[4]` | 5 | `mapped` | nation+0x19 |
| `artillery_count` / `boycott_bitmap` | — | `mapped` | |
| `royal_money` + `unknown24_pad[4]` | 8 | `mapped` | `int32` @ +0x22 REF budget |
| `return_from_europe_x/y` | 2 | `mapped` | `FUN_48d3_007a` |
| `euro_relation[4]` | 4 | `mapped` | −0x77c4 / `FUN_15b3_*` |
| `relation_by_indian[8]` | 8 | `mapped` | |
| `treaty_timer` / `diplo_flag` / sticky / privateer | 12 | `partial` | Linux stand-ins (`ai_diplo.c`); union w/ `unknown26[12]` |
| `trade` (240) | 240 | `mapped` | euro_price / nr / gold / tons |

### Tribe (18 × T)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `x` / `y` / `nation_id` / `state` / `population` / `mission` | — | `mapped` | `unused09` pad |
| `growth_accum` | 1 | `mapped` | +=pop; clear when >19 (`FUN_4d56_152e`) |
| `unknown28_pad` | 1 | `opaque` | No cite |
| `last_bought` / `last_sold` / `alarm[4]` | — | `mapped` | |

### Indian (78 × 8)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `capitol_*` / `tech` / `tons` / `alarm_by_player` | — | `mapped` | |
| `extinct` | 1 bit | `mapped` | bit7 of first unknown31 byte |
| `lands_bought` | 1 | `mapped` | `FUN_479b_00ca` INC |
| `unknown31_flags` | 1 | `partial` | Linux contact prelude bit `0x20` |
| `muskets` / `horse_herds` | 2 | `mapped` | |
| `horse_breeding` | 2 | `mapped` | ±0x32 acquire/tick (`FUN_5bfb_*` / `4d56`) |
| `unknown31*` pads | 3+ | `opaque` | Closed as no-reader pads |
| `contact_state[4]` | 8 | `mapped` | +0x2e; FSM 0/1/2 |
| `euro_relation_accum[4]` | 4 | `mapped` | +0x36; spill → `FUN_281f_0d6c` |
| `euro_diplo[4]` | 4 | `mapped` | +0x3a; met `0x20` / peace `0x40` (was `met_by_player`) |
| `unknown33[8]` | 8 | `opaque` | +0x3e; unused by DOS contact |

### Stuff (727)

DOS `FUN_75c2_0288` writes **33** chunks (`FUN_1d1d_060c`); sizes sum to **727**.
RAM is scattered; the port stores one packed `ColonizeCol1Stuff` for RMW.

| File off | Size | DS | Notes |
|----------|------|-----|-------|
| 0 | 12 | `0x9566` | `unknown34` — save R/W only |
| 12 | 4 | `0x8cfc` | `all_unit_counts[4]` — `FUN_4962_0018` |
| 16 | 4 | `0x9298` | `colony_counts[4]` — `FUN_4962_0018` |
| 20 | 4 | `0x9408` | `free_colonist_counts[4]` — type==0 units |
| 24 | 4 | `0x940c` | `colony_pop_totals[4]` — Σ colony pop |
| 28 | 4 | `0x9410` | `census_pop_proxy[4]` — +1 skilled unit + Σ pop |
| 32 | 4 | `0x9180` | `land_combat_totals[4]` — Σ land combat mode 0 |
| 36 | 4 | `0x9414` | `ship_cargo_totals[4]` — Σ ship cargo capacity |
| 40 | 4 | `0x9418` | `ship_counts[4]` |
| 44 | 8 | `0x941c` | `land_combat_strength[4]` — Σ combat mode 1 (u16) |
| 52 | 4 | `0x9424` | `armed_ship_counts[4]` |
| 56 | 4 | `0x9428` | `veteran_teach_threshold[4]` — reader-only / vestigial writer |
| 60 | 4 | `0x942c` | `field_combat_totals[4]` — land not in colony / not A\|G |
| 64 | 76 | `0x924c` | `unit_type_counts[4][19]` — `FUN_4962_0018` |
| 140 | 16 | `0x947e` | `unknown_ds_947e` — save I/O |
| 156 | 16 | `0x95f2` | `unknown_ds_95f2` — AI readers (`4d56`/`5952`) |
| 172 | 64 | `0x94a6` | `unknown_ds_94a6` — save I/O |
| 236 | 64 | `0x94e6` | `unknown_ds_94e6` — `FUN_5952_035e` |
| 300 | 64 | `0x95b2` | `unknown_ds_95b2` — save I/O |
| 364 | 64 | `0x9526` | `unknown_ds_9526` — save I/O |
| 428 | 64 | `0x918c` | `unknown_ds_918c` — save I/O |
| 492 | 64 | `0x9572` | `unknown_ds_9572` — save I/O |
| 556 | 8 | `0x944e` | `unknown_ds_944e` — pop word totals |
| 564 | 1 | `0x336` | `ui_toggle_336` — `FUN_2f2b_*` |
| 565 | 8 | `0x9184` | `tribe_data_9184` |
| 573 | 8 | `0x9622` | `unknown_ds_9622` — save I/O |
| 581 | 8 | `0x962a` | `unknown_ds_962a` — save I/O |
| 589 | 128 | `0x91cc` | `tribe_dwellings_91cc` |
| 717 | 2 | `0x8540` | `stuff.x` focus tile |
| 719 | 2 | `0x853e` | `stuff.y` |
| 721 | 2 | `0x184` | `zoom_level` + `zoom_pad` (`FUN_2b5a_0f92`) |
| 723 | 2 | `0x17c` | `viewport_x` camera center |
| 725 | 2 | `0x17e` | `viewport_y` |

Port packing (727): `unknown34` + census + DS-named late chunks (was
`unknown36[577]`) + cursor/viewport. **Not connectivity** (that is `post_map`).

**Census policy (DOS parity):** RMW/export **preserves** census bytes when the
window is non-zero. Do not freshen mid-turn lag. **Blank templates only:**
`col1_stuff_census_fill_blank` mirrors `FUN_4962_0018` counters from live pools.

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `unknown34` | 12 | `opaque` | DS:`0x9566` save R/W vestigial |
| census + mid-window | 128 | `mapped` | See chunk table |
| late DS chunks (577) | 577 | `partial` | Named `unknown_ds_*` / tribe_* / `ui_toggle_336` |
| `x` / `y` / zoom / viewport | 10 | `mapped` | Focus + camera |

### Map layers (W×H each)

| Plane | Status | Notes |
|-------|--------|-------|
| `tile` | `mapped` | Terrain bitfield |
| `mask` | `mapped` | Occupancy + density rebuilt on export (`col1_bridge_sync_map_*`; `FUN_684c_08c0` / `137f_015e`); purchased sticky / layer2 |
| `path` | `mapped` | Region + visitor |
| `seen` | `mapped` | Fog / score nibbles |

### Post-map (`ColonizeCol1PostMap`, 614)

Replaces legacy `unknown_e[504]` + `unknown_f[110]` (same bytes). Proven from
`FUN_75c2_0288` asm (`AX` length) + `FUN_67f4_0088` fill:

| File off | Size | DS | Field | Status | Notes |
|----------|------|-----|-------|--------|-------|
| 0 | 270 | `0x86f6` | `sea_connectivity[270]` | `mapped` | 15×18 neighbor bits; fill `local_24==1`; rebuilt on blank export |
| 270 | 270 | `0x85e8` | `land_connectivity[270]` | `mapped` | fill `local_24==0`; rebuilt on blank export |
| 540 | 32 | `0x945e` | `continent_tally_a[16]` | `mapped` | land terrain-class filter; rebuilt |
| 572 | 32 | `0x85c8` | `continent_tally_b[16]` | `mapped` | land tile counts; rebuilt |
| 604 | 4 | SS:`local_8` | `save_path_blob` | `mapped` | Save-path blob; not filled by 67f4 |
| 608 | 4 | `0x8d80` | `boot_timer` | `mapped` | `FUN_75c2_2d46`; **not** seed |
| 612 | 2 | `0x190` | `prime_resource_seed` | `mapped` | full u16; `FUN_684c_08c0` mapgen |

Smcol’s post-connectivity carve does **not** match these DOS writes — prefer DOS.
`veteran_teach_threshold` (`0x9428`) is reader-only (no writer in unpacked image).

**Export rebuild (P3+P4):** `col1_post_map_rebuild_connectivity` (`FUN_67f4_0088`)
runs from `col1_bridge_capture` when `post_map` is all-zero. Planes + tallies
from live terrain + layer3; models `FUN_6662_00f2` dest cost-grid cache (COLONY00
planes byte-exact). Tail preserved; blank templates may stamp
`map.prime_resource_seed`. Nonzero DOS `post_map` left intact on RMW.

### Trade routes (74 × 12)

| Field | Size | Status | Notes |
|-------|------|--------|-------|
| `name` / `sea` / `dest_count` | 34 | `mapped` | |
| `stop[4]` (`ColonizeCol1TradeStop`, 10 B) | 40 | `mapped` | DOS unload@+3 then load@+6 (`FUN_647e`); smcol names those blobs swapped |

---

## Phased work queue

| Phase | Scope | Exit criteria | Status |
|-------|--------|---------------|--------|
| **P0 — Atlas** | Inventory every opaque hole | This document exists; all `unknown*` listed | **Done** |
| **P1 — Correct the big mis-split** | Reconcile stuff vs post-map vs `FUN_75c2_0288` / `FUN_67f4_0088`; fix wrong comments | Connectivity planes named; stuff chunk table; `ColonizeCol1PostMap` | **Done** |
| **P2 — Absorb proven community names** | Rename head/nation/indian/unit/trade fields where smcol + decomp agree | Struct names match evidence; sizes unchanged; `smoke_col1_save` byte-identical | **Done** |
| **P3 — Export rebuild** | Template/new-game rebuilds connectivity (+ required defaults) so DOS survives past UNITFLAG | Linux→DOS smoke; remaining holes documented | **Done** |
| **P4 — Deep leftovers** | Colony opaques, indian contact, stuff FA/counts, pathfinder | Cite + value ranges | **Done** |
| **P5 — Remaining holes** | Ready peels + nation/head pads + `unknown36` chunk split + `other`/head vestigial close | Every byte named or closed save-only/vestigial + DS | **Done** |
| **P6 — Linux→DOS interop** | Mask density, blank census, colony capture fill, vis_mask, AI blob discipline | Template export smoke; fixture RMW identical | **Done** |

```mermaid
flowchart TB
  codec[Codec byte-identical RMW done]
  atlas[P0 Field atlas]
  absorb[P2 Absorb smcol names with DS proof]
  connect[P1 Realign post-map connectivity vs stuff]
  export[P3 Bridge rebuild for DOS-safe new-game]
  deep[P4 Colony nation indian head leftovers]
  p5[P5 Map remaining holes]
  p6[P6 Full Linux DOS interop]
  codec --> atlas
  atlas --> absorb
  atlas --> connect
  absorb --> export
  connect --> export
  export --> deep
  deep --> p5
  p5 --> p6
```

### Remaining HOLD

**P5 naming + P6 template interop complete.** Former export gaps closed:

- Mask `suppress` / `purchased` / `pacific` synthesized on capture
- Blank-template census fill; mid-campaign census still preserved
- Colony specialty / warehouse / capitol / visibility on capture
- Late `unknown_ds_*` / `tribe_dwellings` / `other` / boot path blob: **export-OK
  zero** (save I/O only in unpacked VICEROY — no invented FA rebuild)

Standing rules: keep RMW sizes; do not invent blobs without decomp evidence;
**census** mid-campaign = DOS-parity preserve only (see Stuff §).

---

## Related docs

- [savegame.md](savegame.md) — interop layers, bridge, Linux→DOS gaps
- [project_goals.md](project_goals.md) — 100% save interop acceptance
- [decomp_inventory.md](decomp_inventory.md) — what is shipped vs open RE
- [manual_gap.md](manual_gap.md) — Col1 I/O checklist (playable ≠ fully mapped)
- [ai_transcription.md](ai_transcription.md) — uses Col1 blobs; not the field map epic
- [original_index.md](original_index.md) — index entry for SAV layout
